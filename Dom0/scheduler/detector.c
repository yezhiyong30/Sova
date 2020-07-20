#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>
#include <time.h>

#include "queue.h"
#include "predict.h"
#include "detector.h"

static struct net_stat netstats[16] = {0};

static void clear_net_stats(void)
{
    memset(netstats, 0, sizeof(netstats));
}

static struct net_stat *get_net_stat(const char * dev)
{
    unsigned int i = 0;
	
    if (!dev) {
        return NULL;
    }
	
    /* find interface stat */
    for (i = 0; i < 16; i++) {
        if (netstats[i].dev && (strcmp(netstats[i].dev, dev) == 0)) {  
            return &netstats[i];
        }
    }

    /* wasn't found? add it */
    if (i == 16) {
        for (i = 0; i < 16; i++) {
            if (netstats[i].dev == 0) {
                netstats[i].dev = strndup(dev, MAX_BUFF_SIZE);
                return &netstats[i];
            }
        }
    }

    fprintf(stderr, "too many interfaces used (limit is 16)");
	
    return NULL;
}

/* Update the network status information of the network card device */
static void update_net_stats(const char *dev, int period)
{
    FILE *net_dev_fp;
    unsigned int i = 0;
    char buf[256] = {0};
	
    /* open file and ignore first two lines */
    if (!(net_dev_fp = fopen("/proc/net/dev", "r"))) {
        fprintf(stderr, "fopen failed.\n");
        clear_net_stats();
        return;
    }
    fgets(buf, 255, net_dev_fp);    /* garbage */
    fgets(buf, 255, net_dev_fp);    /* garbage (field names) */
	
    /* read each interface */
    for (i = 0; i < 64; i++) {
        struct net_stat *ns = NULL;
        unsigned char *s = NULL, *p = NULL;
        unsigned long long r = 0, t = 0;
        unsigned long long last_recv = 0, last_trans = 0;	
		struct timeval now_time, last_time; 
		float interval, cur_rate = 0;

        if (fgets(buf, 255, net_dev_fp) == NULL) {
            break;
        }
		
        // Skip Space
        p = buf;
        while (isspace((int) *p)) {
            p++;
        }
        s = p; 
		
        //Skip Network Interface Name
        while (*p && *p != ':') {
            p++;
        }
        if (*p == '\0') {
            continue;
        }
        *p = '\0';
        p++;

        // Judge Network Interface or Not?
        if(strcmp(s, dev) != 0)
            continue;

        // Get struct net_stat
        ns = get_net_stat(s);

        last_recv = ns->recv;
        last_trans = ns->trans;

        /* bytes packets errs drop fifo frame compressed multicast|bytes ... */
        sscanf(p, "%lld %*d %*d %*d %*d %*d %*d %*d %lld", &r, &t);

        /* if recv or trans is less than last time, an overflow happened */
        if (r < ns->last_read_recv) {
            last_recv = 0;
        } else {
            ns->recv += (r - ns->last_read_recv);
        }
        ns->last_read_recv = r;

        if (t < ns->last_read_trans) {
            last_trans = 0;
        } else {
            ns->trans += (t - ns->last_read_trans);
        }
        ns->last_read_trans = t;
		
		 /* calculate interval(ms) */
		gettimeofday(&now_time, NULL); 
		last_time = ns->last_time;
		ns->last_time = now_time;
		if (last_time.tv_sec == 0) {
			interval = period * 1000;
		} else {
			interval = 1000 * ( now_time.tv_sec - last_time.tv_sec ) +
                    (now_time.tv_usec - last_time.tv_usec)/1000;
		}

        /* calculate bandwidth utilization */
        if(last_recv == 0 && last_trans == 0) {
			cur_rate = 0;
		} else {
			cur_rate = (float)(ns->recv - last_recv + ns->trans - last_trans) * 8 / (1000 interval);
		}
        ns->net_usage = cur_rate/BANDWIDTH;

        break;
    }
    fclose(net_dev_fp);
	
	return;
}

/* Migration trigger detection */
bool migration_detect(const char *dev, pQueue pQ)
{
	int i, degree, method, queue_length = 0, out_number=0;
	float pred, *data;
	struct net_stat *ns = NULL;
	time_t now;
	
	//update_net_stats(dev,10);
    ns = get_net_stat(dev);	
	printf("%s 's bandwidth utilization: %f\n", dev, ns->net_usage);
	
	net_stats_Queue(pQ, ns->net_usage);

	if ((data = (float *)malloc(sizeof(float) * MAXQSIZE)) == NULL) {
		fprintf(stderr, "Failed to allocate space for data\n");
		return false;
	}

	i = pQ->front;
	while(pQ->rear != i) {
		if (pQ->pBase[i] >= Thresh_bandwidth) {
			out_number++;
		}
		data[queue_length] = pQ->pBase[i];
		queue_length++;

		i = (i+1) % MAXQSIZE;
	}

	if (queue_length == Record_Number && out_number >= Thresh_Out_Number) {
		degree = 4;
		method = MAXENTROPY;
		//method = LEASTSQUARES;

		pred = predict(data, queue_length, degree, method);
		printf("predict=%f\n", pred);
		
		if (pred >= Thresh_bandwidth) {
			time(&now); 
			printf("SystemTime: \t%s hotspot!\n", ctime(&now));
			
			if (data != NULL) {
				free(data);
			}
			
			return true;
		}
	}

	if (data != NULL) {
		free(data);
	}
	
	return false;
}

/* Get the bandwidth usage of the network card */
float get_net_usage(const char *dev)
{
	struct net_stat *ns = NULL;

	update_net_stats(dev, 10);
    ns = get_net_stat(dev);	
	
	return ns->net_usage;
}
