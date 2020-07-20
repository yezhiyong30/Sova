
#ifndef QUEUE_H
#define QUEUE_H

/* Maximum queue length (the actual usable length is: MAXQSIZE-1) */
#define MAXQSIZE 6   

typedef struct Queue {
	float * pBase;  /* The first address of the array */
	int front;      /* queue head */
	int rear;       /* queue tail */
} Queue, *pQueue;

void init_Queue(pQueue pQ);
void net_stats_Queue(pQueue pQ, float val);

#endif /* QUEUE_H */