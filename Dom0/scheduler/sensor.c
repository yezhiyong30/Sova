#include <xenstore.h>
#include <libxl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sensor.h"


/* Initialize the xenstore of each DomU and grant read and write permissions */
void xenstore_init(libxl_ctx *ctx)
{
	char path[100], cmd[100], pathn[100];
	int dom_num, domid, i = 0;
	libxl_dominfo *dom_info = NULL;
	xs_transaction_t xt;
	static xentoollog_logger *logger;

	struct xs_handle *xh = xs_daemon_open();
	if (!xh) {
		printf("xs_daemon open error!\n");
		return;
	}

	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);

    dom_info = libxl_list_domain(ctx, &dom_num);

    libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger*)logger);
	
	printf("initialization xenstore...\n");
	for (i = 0; i < dom_num; ++i) {
		domid = dom_info[i].domid;

		if (domid == 0) {
			continue;
		}

		sprintf(path, "/local/domain/%d", domid);
		strcpy(pathn, path);
		strcat(path, "/network/value");
		strcat(pathn, "/network/freq");
		printf("%s\n", path);

		xt = xs_transaction_start(xh);
		if(!xt) {
			printf("xs_transaction start error!\n");
			return;
		}

		xs_write(xh, xt, path, "0", strlen("0"));
		xs_write(xh, xt, pathn, "0", strlen("0"));
		xs_transaction_end(xh, xt, false);
		
		// Sets the permissions of the Xenstore KEY. b：read and write (both)
		sprintf(cmd, "xenstore-chmod %s b", path);  
		system(cmd);
		sprintf(cmd, "xenstore-chmod %s b", pathn);  
		system(cmd);
		
		printf("initialization of domain%d 's xenstore\n", domid);
	} 
	printf("\n");
	
	free(dom_info);
	xs_close(xh);
}

/* Initialize a single DomU and grant read and write permissions */
void xenstore_initdom(int domid)
{
	char path[100], cmd[100], pathn[100];
	xs_transaction_t xt;

	struct xs_handle *xh = xs_daemon_open();
	if (!xh) {
		printf("xs_daemon open error!\n");
		return;
	}

	sprintf(path, "/local/domain/%d", domid);
	strcpy(pathn, path);
	strcat(path, "/network/value");
	strcat(pathn, "/network/freq");
	printf("%s\n", path);

	xt = xs_transaction_start(xh);
	if(!xt) {
		printf("xs_transaction start error!\n");
		return;
	}

	xs_write(xh, xt, path, "0", strlen("0")); 
	xs_write(xh, xt, pathn, "0", strlen("0"));
	xs_transaction_end(xh, xt, false);
	
	sprintf(cmd, "xenstore-chmod %s b", path);  
	system(cmd);
	sprintf(cmd, "xenstore-chmod %s b", pathn);  
	system(cmd);
	
	printf("initialization of domain%d 's xenstore\n", domid);

	xs_close(xh);
}

/* Get VM network traffic information from xenstore */
net_stat * get_net_speed(libxl_ctx *ctx, int *num)
{
	char path[100], pathn[100], *result, *freq;
	int dom_num, len, domid, i = 0;
	libxl_dominfo *dom_info = NULL;
	xs_transaction_t xt;
	static xentoollog_logger *logger;

	struct xs_handle *xh = xs_daemon_open();
	if (!xh) {
		printf("xs_daemon open error!\n");
		return;
	}

	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);

    dom_info = libxl_list_domain(ctx, &dom_num);
	*num = dom_num;

	libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger *)logger);

	net_stat *net_stat_data = (net_stat *)malloc(sizeof(net_stat) * dom_num);
	if (net_stat_data == NULL) {
		printf("malloc error!\n");
		return NULL;
	}
	memset(net_stat_data, 0, sizeof(net_stat) * dom_num);

	printf("get net_speed...\n");
	for (i = 0; i < dom_num; ++i) {
		domid = dom_info[i].domid;
	
		net_stat_data[i].domid = domid;
		if (domid == 0) {
			net_stat_data[i].net_speed = 0;
			printf("domain%d 's net_speed: 0\n", domid);
			continue;
		}
	
		sprintf(path, "/local/domain/%d/network/value", domid);
		sprintf(pathn, "/local/domain/%d/network/freq", domid);
		
		xt = xs_transaction_start(xh);
		if(!xt) {
			printf("xs_transaction start error!\n");
			return NULL;
		}
		
		// Read the value of net from xenstore
		result = xs_read(xh, xt, path, &len);
		freq = xs_read(xh, xt, pathn, &len);
		xs_transaction_end(xh, xt, false);

		printf("domain%d 's net_speed: %s, freq: %s\n", domid, result, freq);
		net_stat_data[i].net_speed = atoi(result)/10000 + freq;
		
	}
	printf("\n");

	free(dom_info);
	xs_close(xh);	

	return net_stat_data;
}

/* Read VM IO information from the file IO_STATS_FILE */
io_stat_p get_io_stats(void)
{
	unsigned int i = 0;
	int IO_STATES_SIZE = sizeof(io_stat);
	
	FILE *stat_fd = fopen(IO_STATS_FILE, "r");
	if (!stat_fd) {
		fprintf(stderr, "Can't open file IO_STATS_FILE!\n");
		return NULL;
	}
	
	io_stat_p io_stats_data = (io_stat *)malloc(sizeof(io_stat) * NDOMAINS);
	memset(io_stats_data, 0, sizeof(io_stat)*NDOMAINS);

	printf("get io_stat...\n");
	for (i = 0; i < NDOMAINS; ++i) {
		fread((io_stats_data + i), IO_STATES_SIZE, 1, stat_fd);
		if (io_stats_data[i].dom_in_use == 1 ) {
			printf("domain%d 's ratio: %f \n", io_stats_data[i].domid, io_stats_data[i].ratio);
		}
	}
	printf("\n");

	fclose(stat_fd);

	return io_stats_data;
}

/* Find the VM, and return its ratio value */
float find_io_stats(io_stat *io_stats_data, int domid)
{
	int i = 0;
	float ratio;

	for (i = 0; i < NDOMAINS; ++i) {
		if (io_stats_data[i].domid == domid && io_stats_data[i].dom_in_use == 1) {
			ratio = io_stats_data[i].ratio;
			printf("find_io_stats: domain%d 's ratio: %f\n", domid, ratio);
			return ratio;
		}
	}

	return 0.0;
}
