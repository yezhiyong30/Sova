
#ifndef DETECTOR_H
#define DETECTOR_H

#include <sys/time.h>
#include "queue.h"

/* Network card bandwidth (Mb/s) */
#define BANDWIDTH 10000

/* String maximum buff size */
#define MAX_BUFF_SIZE 256

/* Record the number of network status data (note that it must match the length of the queue) */
#define Record_Number 5

/* 80% of the data exceeds the threshold and the migration is triggered */
#define Thresh_Out_Number 3

/* Threshold of network bandwidth utilization */
#define Thresh_bandwidth 0.75

/* Used to save the network information of the network card */
struct net_stat {
    const char *dev;                 
	struct timeval last_time;
    long long last_read_recv, last_read_trans;
    long long recv, trans;
	float net_usage;
};

bool migration_detect(const char *dev, pQueue pQ);
float get_net_usage(const char *dev);

#endif /* DETECTOR_H */