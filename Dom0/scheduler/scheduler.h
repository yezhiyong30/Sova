
#ifndef SCHEDULER_H
#define SCHEDULER_H

/* ratio threshold */
#define Threshold_Ratio 2/3

/* speed threshold */
#define Threshold_Speed 2/3 

/* idle domain's ID */
#define IDLE_DOMAIN -1  

/* VF information */
typedef struct {
	int total;
	int used;
} vf_stats;

/* Linked list status information */
typedef struct {
	int total;
	int list1_num;
	int list2_num;
} list_stats;

/* VM status information */
typedef struct dom_stats {
	int domid;
	int up;
	float ratio;
	double net_speed;
	struct dom_stats * next; 
} dom_stats;

#endif /* SCHEDULER_H */