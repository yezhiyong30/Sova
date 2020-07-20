#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <xenstore.h>
#include <libxl.h>
#include <sys/time.h>
#include <time.h>

#include "queue.h"
#include "detector.h"
#include "migration.h"
#include "scheduler.h"
#include "sensor.h"

/* Remove the VF of the VM */
static int remove_dom_vf(libxl_ctx *ctx, int domid)
{
    int nr_dom_vf, err, i = 0;
    libxl_device_pci *pcidevs;
    static xentoollog_logger *logger;

    logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);

	// nr_dom_vf is the number of VFs in the VM
    pcidevs = libxl_device_pci_list(ctx, domid, &nr_dom_vf);

    libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger*)logger);

    for(i = 0; i < nr_dom_vf; i++) {
    	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    	libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);

		// Remove the i-th VF of the VM
		if(libxl_device_pci_remove(ctx, domid, &pcidevs[i], 0) < 0) {
			printf("VF remove error;\n");
			fflush(stdout);
			err = 0;
		}
		libxl_ctx_free(ctx);
    	xtl_logger_destroy((xentoollog_logger*)logger);
		libxl_device_pci_dispose(&pcidevs[i]);
    }
    free(pcidevs);
	
    return err;
}

/* Read VM network traffic information and memory from xenstore */
static migrat_stat *get_migrat_stat(libxl_ctx *ctx, int *num)
{
	char mem_path[100], net_path[100], *memory, *net;
	int dom_num, len, domid, i = 0, j = 0;
	libxl_dominfo *dom_info = NULL;
	xs_transaction_t xt;
	long long tatol_cpu_time, tatol_memoy, tatol_net;
	time_t now;
	static xentoollog_logger *logger;
	
	struct xs_handle *xh = xs_daemon_open();
	if ( !xh ) {
		printf("xs_daemon open error!\n");
		return;
	}

	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);

    dom_info = libxl_list_domain(ctx, &dom_num);
	*num = dom_num;

	libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger *)logger);
	
	migrat_stat *migrat_stat_data = (migrat_stat *)malloc(sizeof(migrat_stat) * dom_num);
	if (migrat_stat_data == NULL) {
		printf("malloc error!\n");
		return NULL;
	}
	memset(migrat_stat_data, 0, sizeof(migrat_stat)*dom_num);

	io_stat *io_stats_data = get_io_stats();
	
	printf("get cpu, memory and net data...\n");
	tatol_cpu_time = tatol_memoy = tatol_net = 0;
	for (i = 0; i < dom_num; ++i) {
		domid = dom_info[i].domid;

		migrat_stat_data[i].domid = domid;
		if (domid == 0) {
			migrat_stat_data[i].factor = 0;
			printf("domain%d 's factor: 0.0\n", domid);
			continue;
		}

		sprintf(mem_path, "/local/domain/%d/memory/target", domid);
		sprintf(net_path, "/local/domain/%d/network/value", domid);
		
		xt = xs_transaction_start(xh);
		if(!xt) {
			printf("xs_transaction start error!\n");
			return NULL;
		}
		
		// Read the size of memory and net from xenstore
		memory = xs_read(xh, xt, mem_path, &len);
		net = xs_read(xh, xt, net_path, &len);

		xs_transaction_end(xh, xt, false);

		time(&now); 		
		printf("SystemTime: \t%s domain%d 's memory: %s and net value: %s\n",
							ctime(&now), domid, memory, net);
		migrat_stat_data[i].net_speed = atoi(net);
		tatol_net += migrat_stat_data[i].net_speed;
		migrat_stat_data[i].memory = atoi(memory);
		tatol_memoy += migrat_stat_data[i].memory;

		for (j = 0; j < dom_num; ++j) {
			if (io_stats_data->domid == domid) {
				migrat_stat_data[i].cpu_time = io_stats_data->cpu_time;
				tatol_cpu_time += migrat_stat_data[i].cpu_time;
				break;
			}
		}
	}
	printf("\n");

	for (int i = 0; i < dom_num; ++i) {
		long long net_t, memory_t, cpu_time_t;

		net_t = pow(migrat_stat_data[i].net_speed / tatol_net / dom_num - 1, 2);
		memory_t = pow(migrat_stat_data[i].memory / tatol_memoy / dom_num - 1, 2);
		cpu_time_t = pow(migrat_stat_data[i].cpu_time / tatol_cpu_time / dom_num - 1, 2);

		migrat_stat_data[i].factor = sqrt(net_t + memory_t + cpu_time_t);
	}
	
	free(dom_info);
	xs_close(xh);	
	
	return migrat_stat_data;
}

/* Get the VM with the largest migration factor */
static int get_max_domid(migrat_stat *migrat_stat_data, int num)
{
	int max_id, i;
	float max_factor;

	max_id = migrat_stat_data[0].domid;
	max_factor = migrat_stat_data[0].factor;
	
	for (i = 1; i < num; ++i) {
		if (migrat_stat_data[i].factor > max_factor) {
			max_id = migrat_stat_data[i].domid;
			max_factor = migrat_stat_data[i].factor;
		}
	}

	return max_id;
}

void migrat_sched(libxl_ctx * ctx)
{
	int sock_fd, strLen, domid ,dom_num, addr_len;
	struct sockaddr_in serv_addr, loc_addr;
	char buffer[BUF_SIZE], cmd[100], flag[8];
	migrat_stat *migrat_stat_data;
	float net_usage;
	time_t now;
	
	sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ( sock_fd == -1 ) {
		fprintf(stderr, "Error: Client Socket can not be created !! \n");
        fprintf(stderr, "errno : %d\n", errno);
		return;
	}

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(HOST_IP);
    serv_addr.sin_port = htons(PORT);
    if (connect(sock_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
		fprintf(stderr, "Connect Server Failed!\n");
		return;
	}
	
	net_usage = get_net_usage(DEV_NAME);
	sprintf(buffer, "%f", net_usage); 
	
	if (send(sock_fd, buffer, BUF_SIZE, 0) == -1) {
		fprintf(stderr, "Send Data Failed!\n");
		return;
	}
	time(&now);
	printf("SystemTime: %s net_usage sending to server is: %s\n", ctime(&now), buffer);
	
	strcpy(flag, "false");
	if (migration_detect(DEV_NAME, &Q) == true) {
		memset(flag, 0, 8);
		strcpy(flag, "true");
		if (send(sock_fd, flag, 8, 0) == -1 ) {
			fprintf(stderr, "Send Data Failed!\n");
			return;
		}
		
		memset(buffer, 0, BUF_SIZE);
		strLen = recv(sock_fd, buffer, BUF_SIZE, 0);
		if ( strLen == -1) {
			fprintf(stderr, "Recieve Data Failed!\n");
			return;
		}
		printf("migration host is: %s\n", buffer);
		
		addr_len = sizeof(loc_addr);
		memset(&loc_addr, 0, sizeof(loc_addr));
		getsockname(sock_fd, (struct sockaddr *)&loc_addr, &addr_len);
		
		if (strcmp(buffer, inet_ntoa(loc_addr.sin_addr)) != 0) {

			migrat_stat_data = get_migrat_stat(ctx, &dom_num);
			domid = get_max_domid(migrat_stat_data, dom_num);
			
			remove_dom_vf(ctx, domid);
			
			if (domid != 0) {
				sprintf(cmd, "xl migrate %d %s", domid, buffer); 
				time(&now); 
				printf("SystemTime: \t%s \tcmd=%s\n", ctime(&now), cmd);
				system(cmd);
			}
		}
	} else {
		strcpy(flag, "false");
		if (send(sock_fd, flag, 8, 0) == -1 ) {
			fprintf(stderr, "Send Data Failed!\n");
			return;
		}
	}
   
    close(sock_fd);

    return;
}
