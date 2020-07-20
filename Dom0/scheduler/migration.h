
#ifndef MIGRATION_H
#define MIGRATION_H

#define PORT 1234

#define HOST_IP "172.16.101.136"

#define DEV_NAME "p5p1"

#define BUF_SIZE 1024

/* The value of the migration factor of DomU */
typedef struct {
	int domid;
    int net_speed;
    long long memory;
    long long cpu_time;
	float factor;
} migrat_stat;


void migrat_sched(libxl_ctx * ctx);

#endif /* MIGRATION_H */