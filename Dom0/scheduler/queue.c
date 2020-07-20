#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "queue.h"

/* Initialize the queue */
void init_Queue(pQueue pQ)
{
	// Construct an empty queue
	pQ->pBase = (float *)malloc(sizeof(float) * MAXQSIZE); 
	if (NULL == pQ->pBase) {
		printf("malloc error!\n");
		return;
	}
	pQ->front = pQ->rear = 0;  

	return;
}

/* Determine whether the queue is full */
static bool is_full(pQueue pQ)
{
	if ((pQ->rear + 1) % MAXQSIZE == pQ->front)
		return true;
	else 
		return false;
}

/* Determine whether the queue is empty */
static bool is_empty(pQueue pQ)
{
	if (pQ->rear == pQ->front)
		return true;
	else 
		return false;
}

/* Traverse the queue */
static void traverse_Queue(pQueue pQ)
{
	int i = pQ->front;

	printf("traverse_Queue:\n");
	while (pQ->rear != i) {
		printf("%f   ", pQ->pBase[i]);
		i = (i + 1) % MAXQSIZE;
	}
	printf("\n");
	
	return;
}

/* Enqueue operation */
static bool en_Queue(pQueue pQ, float val)
{
	if (is_full(pQ)) {
		return false;
	} else {
		pQ->pBase[pQ->rear] = val;
		pQ->rear = (pQ->rear + 1) % MAXQSIZE;

		return true;
	}
}

/* Dequeue operation */
static bool out_Queue(pQueue pQ, float *pVal)
{
	if (is_empty(pQ)) {
		return false;
	} else {
		*pVal = pQ->pBase[pQ->front];
		pQ->front = (pQ->front + 1) % MAXQSIZE;

		return true;
	}
}

/* Put network status data in the queue */
void net_stats_Queue(pQueue pQ, float val)
{
	float dump;
	
	if (is_full(pQ)) {
		// If the queue is full, dequeue the head element first, then enqueue
		out_Queue(pQ, &dump);
		en_Queue(pQ, val);
	} else {
		// If it is not full, enqueue directly
		en_Queue(pQ, val);
	}

	traverse_Queue(pQ);
}
