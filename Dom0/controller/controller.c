#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h> 
#include <errno.h>
#include<pthread.h>     
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

#define PORT 1234

#define BUF_SIZE 1024

#define CLIENT_NUMBER 3

#define MIN_USAGE 0.6

/* Client's IP and bandwidth usage information */
typedef struct {
	unsigned int client_ip;
	float net_usage;
} client_stat;

static client_stat *client_stats;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void clear_clients_stats(void)
{
    memset(client_stats, 0, sizeof(client_stats));
}

static client_stat *get_client_stat(const unsigned int ip_addr)
{
    unsigned int i = 0;
	
    if (ip_addr == 0) {
        return NULL;
    }

    /* find client ip */
    for (i = 0; i < CLIENT_NUMBER; i++) {
        if ( (client_stats[i].client_ip != 0 ) && (client_stats[i].client_ip == ip_addr)) {  
            return &client_stats[i];
        }
    }
	
    /* wasn't found? add it */
    if (i == CLIENT_NUMBER) {
        for (i = 0; i < CLIENT_NUMBER; i++) {
            if (client_stats[i].client_ip == 0) {
                client_stats[i].client_ip = ip_addr;
                return &client_stats[i];
            }
        }
    }
	
    fprintf(stderr, "client number is not right!\n");
	
    return NULL;
}

/* Get the host with the least bandwidth usage */
static unsigned int get_min_host(void)
{
	unsigned int min_ip, i;
	float min_usage;

	if (client_stats[0].client_ip != 0) {
		min_ip = client_stats[0].client_ip;
		min_usage = client_stats[0].net_usage;
	} else {
		return 1;
	}

	for (i = 1; i < CLIENT_NUMBER && client_stats[i].client_ip != 0; ++i) {
		if (client_stats[i].net_usage < min_usage) {
			min_ip = client_stats[i].client_ip;
			min_usage = client_stats[i].net_usage;
		}
	}

	if (min_usage >= MIN_USAGE) {
		return 1;
	}

	return min_ip;
}

static void data_handle(void * sock_fd)
{
    int fd = *((int *)sock_fd);
	struct sockaddr_in addr;
	struct in_addr addrs;
    char recv_data[BUF_SIZE], * data_send;
	int strLen, addr_len;
	unsigned int ip_addr, ip_addr_min;
	client_stat * cs = NULL;
	
	// lock
    if (pthread_mutex_lock(&mutex) != 0) {
		fprintf(stderr, "pthread_mutex_lock error\n");
		return;
	}
	
	memset(recv_data, 0, BUF_SIZE);
	strLen = recv(fd, recv_data, BUF_SIZE, 0);
	if (strLen == -1) {
		fprintf(stderr, "read error!\n");
		return;
	}
	printf("read net_usage from client : %s\n", recv_data);
	
	addr_len = sizeof(addr);
	memset(&addr, 0, addr_len);
	getpeername(fd, (struct sockaddr *)&addr, &addr_len);
	ip_addr = addr.sin_addr.s_addr;
	printf("client ip is: %d or %s\n", addr.sin_addr.s_addr, inet_ntoa(addr.sin_addr));
	
	cs = get_client_stat(ip_addr);
	cs->net_usage = (float)atof(recv_data);
	
	strLen = recv(fd, recv_data, BUF_SIZE, 0);
	if(strLen == -1) {
		fprintf(stderr, "read error!\n");
		return;
	}
	printf("read migration flag from client : %s\n", recv_data);
	
	if (strcmp(recv_data, "true") == 0) {
		ip_addr_min = get_min_host();
		if (ip_addr_min == 1) {
			ip_addr_min = ip_addr;
		}
		addrs.s_addr = ip_addr_min;
		data_send = inet_ntoa(addrs);
		
		if(send(fd, data_send, strlen(data_send), 0) == -1) {
			fprintf(stderr, "Server Send Data Failed!\n");
			return;
		}
	}

	// unlock
    if( pthread_mutex_unlock(&mutex) != 0 ) {
		fprintf(stderr, "pthread_mutex_unlock error\n");
		return;
	}

    printf("terminating current client_connection, tid is: %lu\n", pthread_self());
    close(fd);
    pthread_exit(NULL);
}

int main(void)
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr, clnt_addr;
    int strLen, ret;

	client_stats = (client_stat *)malloc(sizeof(client_stat) * CLIENT_NUMBER);
	
	memset(client_stats, 0, sizeof(client_stats));

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
		printf("Error : Could not create socket\n");
        printf("Errno %d\n", errno);
        exit(-1);
	}
    
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);
	
    if (bind(listenfd,(struct scokaddr *)(&serv_addr), sizeof(serv_addr)) == -1) {
		printf("Error:Bindint with port # %d failed\n", PORT);
        printf("Errno %d\n", errno);
        if (errno == EADDRINUSE) {
			printf("Another socket is already listening on the same port\n");
		}
        exit(-1);
	}

    if( listen(listenfd, 10) == -1) {
        printf("Error:Failed to listen\n");
        printf("Errno %d\n", errno);
        if (errno == EADDRINUSE) {
			printf("Another socket is already listening on the same port\n");
		}
        exit(-1);
    }

    while(1) {
        pthread_t thread_id;
        strLen = sizeof(clnt_addr);

        connfd = accept(listenfd, (struct sockaddr_*)(&clnt_addr), (socklen_t *)(&strLen));
        if ( connfd == -1 ) {
            fprintf(stderr, " Accept error!\n");
            continue;                       
        }
		
        if (pthread_create(&thread_id, NULL, (void *)(&data_handle), (void *)(&connfd)) == -1) {
            fprintf(stderr, "pthread_create error!\n");
            break;
        }
		printf("A new connection occurs, tid is: %lu\n", pthread_self());
    }

    ret = shutdown(listenfd, SHUT_WR);
    if (ret != -1) {
		printf("Server shuts down!\n");
	}

    return 0;
}
