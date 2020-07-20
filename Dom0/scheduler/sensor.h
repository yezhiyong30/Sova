
#ifndef SENSOR_H
#define SENSOR_H

/* The file containing the I/O status information to be read */
#define IO_STATS_FILE "/var/run/sova/io-stats"

/* Number of domains we can keep track of in memory */
#define NDOMAINS 64  
 
/* I/O status information for VMs */
typedef struct io_stat {
	int dom_in_use;
	int domid;
	float ratio;
    long long cpu_time;
} io_stat, *io_stat_p;

/* Network status information for VMs*/
typedef struct {
	int domid;
	double net_speed;
} net_stat;

void xenstore_init(libxl_ctx *ctx);
void xenstore_initdom(int domid);
net_stat * get_net_speed(libxl_ctx * ctx, int *dom_num);
io_stat_p get_io_stats(void);
float find_io_stats(io_stat *io_stats_data, int domid);

#endif /* SENSOR_H */