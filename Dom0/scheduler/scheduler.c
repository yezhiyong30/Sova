#include <stdio.h>
#include <stdlib.h>
#include <libxl.h>

#include "queue.h"
#include "sensor.h"
#include "scheduler.h"
#include "migration.h"

/* Global variables, the head pointer of the list */
static dom_stats * list1, * list2, * list3;

/* Global variables, the status information of the list */
static list_stats list_info;

/* Global variables, VF status information */
static vf_stats vf_info;

Queue Q;

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
    	xtl_logger_destroy((xentoollog_logger *)logger);
		libxl_device_pci_dispose(&pcidevs[i]);
    }
    free(pcidevs);
	
    return err;
}

/* Assign VF to the VM */
static int add_dom_vf(libxl_ctx * ctx, int domid)
{
	int nr_num_vf, i = 0;
	libxl_device_pci *pcidevs = NULL;
	static xentoollog_logger *logger;
	
	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);

	// nr_num_vf is the total number of allocable VF
	pcidevs = libxl_device_pci_assignable_list(ctx, &nr_num_vf);   
	if (nr_num_vf == 0) {
		printf("No Enough VF!\n");
		return -1;
	}

	// Assign VF to this VM
	libxl_device_pci_add(ctx, domid, &pcidevs[0], 0); 

	libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger *)logger);
	
	for(i = 0; i < nr_num_vf; i++) {
		libxl_device_pci_dispose(&pcidevs[i]);
	}
    free(pcidevs);
	
	return 1;
}

/* Assign VF to the VM on the list plist */
static void vf_assign_list(libxl_ctx * ctx, dom_stats * plist)
{
	int nr_num_vf, i = 0;
	dom_stats * p = NULL;
	libxl_device_pci *pcidevs = NULL;
	static xentoollog_logger *logger;
	
	if (plist == NULL || plist->next == NULL) {
		return;
	}

	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);
	
	// nr_num_vf is the total number of allocable VF
	pcidevs = libxl_device_pci_assignable_list(ctx, &nr_num_vf);   
	if (nr_num_vf == 0) {
		printf("No Enough VF!\n");
		return;
	}
	
	p = plist->next;
	while (p != NULL) {
		if (i < nr_num_vf && p->up == 0) {
			// Assign VF to this VM
			libxl_device_pci_add(ctx, p->domid, &pcidevs[0], 0);  
			p->up = 1;
			++vf_info.used;
			++i;
		} 
		p = p->next;
	}

	libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger *)logger);
	
	for(i = 0; i < nr_num_vf; i++) {
		libxl_device_pci_dispose(&pcidevs[i]);
	}
    free(pcidevs);
	
	return;
}

/* Assign VF to the VM */
static void vf_assign_dom(libxl_ctx *ctx, dom_stats *pNode)
{
	int num_vf, i = 0;
	libxl_device_pci * pcidevs = NULL;
	static xentoollog_logger *logger;

	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);
	
	// nr_num_vf is the total number of allocable VF
	pcidevs = libxl_device_pci_assignable_list(ctx, &num_vf);   
	if (num_vf == 0) {
		printf("No Enough VF!\n");
		return;
	}

	if (pNode->up == 0) {
		// Assign VF to this VM
		libxl_device_pci_add(ctx, pNode->domid, &pcidevs[0], 0);  
		pNode->up = 1;
		++vf_info.used;
	}

	libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger *)logger);

	for(i = 0; i < num_vf; i++) {
		libxl_device_pci_dispose(&pcidevs[i]);
	}
    free(pcidevs);

	return;
}

/* Remove the VF of the VM on the list plist */
static void vf_remove_list(libxl_ctx * ctx, dom_stats * plist)
{
	int num, i = 0;
	dom_stats * p = NULL;
	libxl_device_pci * pcidevs = NULL;
	static xentoollog_logger *logger;
	
	if (plist == NULL || plist->next == NULL) {
		return;
	}

	p = plist->next;
	while (p != NULL) {
		if (p->up == 1) {
			logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    		libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);
			
			// num is the total number of allocable VF
			pcidevs = libxl_device_pci_list(ctx, p->domid, &num);
			if (num != 0) {
				for (i = 0; i < num; ++i) {
					if(libxl_device_pci_remove(ctx, p->domid, &pcidevs[i], 0) < 0) {
						printf("VF remove error!\n");
					}	
					libxl_device_pci_dispose(&pcidevs[i]);
				}
				p->up = 0;
				vf_info.used--;
			}

			libxl_ctx_free(ctx);
    		xtl_logger_destroy((xentoollog_logger *)logger);

			free(pcidevs);
		}
		p = p->next;
	}
	
	return;
}

/* Remove the VF of all VMs and initialize the global variable vf_info */
static void vf_init(libxl_ctx *ctx)
{
	int dom_num, vf_num, i = 0;
    libxl_dominfo *dom_info;
	libxl_device_pci *pcidevs;
	static xentoollog_logger *logger;

	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);

    dom_info = libxl_list_domain(ctx, &dom_num);

    libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger*)logger); 
	
    for(i = 0; i < dom_num; ++i) {
		remove_dom_vf(ctx, dom_info[i].domid);   
    }
	
	pcidevs = libxl_device_pci_assignable_list(ctx, &vf_num); 
	
    vf_info.total = vf_num; 
    vf_info.used = 0;
	
    for(i = 0; i < vf_num; ++i) {
		libxl_device_pci_dispose(&pcidevs[i]);
	}
	
    free(pcidevs);
    free(dom_info);
}

/* Get the network data and I/O data of the VM */
static dom_stats *get_dom_stats(libxl_ctx *ctx, int *num)
{
	int dom_num, domid, i = 0;
	libxl_dominfo *dom_info = NULL;
	
	io_stat *io_stats_data = get_io_stats();
	net_stat *net_stat_data = get_net_speed(ctx, &dom_num);
	*num = dom_num;
	
	dom_stats *dom_stat_data = (dom_stats *)malloc(sizeof(dom_stats) * dom_num);
	if (dom_stat_data == NULL) {
		printf("malloc error!\n");
		return NULL;
	}
	memset(dom_stat_data, 0, sizeof(dom_stats)*dom_num);
	
	printf("get dom_stat...\n");
	for (i = 0; i < dom_num; ++i) {
		domid = net_stat_data[i].domid;
		
		dom_stat_data[i].domid = domid;
		dom_stat_data[i].net_speed = net_stat_data[i].net_speed;
		
		dom_stat_data[i].ratio = find_io_stats(io_stats_data, domid);
		printf("domain%d 's net_speed: %lf and ratio: %f \n", domid, dom_stat_data[i].net_speed, dom_stat_data[i].ratio);
	}
	printf("\n");
	
	free(io_stats_data);
	free(net_stat_data);

	return dom_stat_data;
}

/* Initialize the linked list */
static void list_init(void)
{
	// Create a head node for each linked list
	list1 = (dom_stats *)malloc(sizeof(dom_stats));    
    list1->next = NULL; 
	
	list2 = (dom_stats *)malloc(sizeof(dom_stats));    
    list2->next = NULL; 
	
	list3 = (dom_stats *)malloc(sizeof(dom_stats));    
    list3->next = NULL; 
	
	list_info.total = 0;
	list_info.list1_num = 0;
	list_info.list2_num = 0;

	return;
}

/* Traverse linked list */
static void traverse_list(dom_stats * plist)
{
	if (plist == list1) {
		printf("traverse list1...\n");
	} else if (plist == list2) {
		printf("traverse list2...\n");
	} else {
		printf("traverse list3...\n");
	}

	dom_stats * p = plist->next;
	if (p == NULL ) {
		printf("This list is NULL!\n");
	}

	while (p != NULL ) {
		printf(" domain%d 's ratio: %f and net_speed: %lf\n", p->domid, p->ratio, p->net_speed);
		p = p->next;
	}
	printf("\n");

	return;
}

/* Insert node p into the linked list in order of ratio value from large to small */
static void insert_list_ratio(dom_stats * plist, dom_stats * pNode)
{
	dom_stats * p = plist;

	while ((p->next != NULL) && (pNode->ratio < p->next->ratio)) {
		p = p->next;
	}

	pNode->next = p->next;  
	p->next = pNode;

	return;
}

/* Insert node p into the list in order of net_speed value from largest to smallest */
static void insert_list_speed(dom_stats * plist, dom_stats * pNode)
{
	dom_stats * p = plist;

	while ((p->next != NULL) && (pNode->net_speed < p->next->net_speed)) {
		p = p->next;
	}

	pNode->next = p->next;  
	p->next = pNode;

	return;
}

/* Sort according to the value of the VM ratio from large to small */
static void insertionSort_ratio(dom_stats * plist)
{
	if (plist == NULL || plist->next == NULL) {
		return;
	}

	dom_stats *p = plist->next;
	plist->next = NULL;
	
	dom_stats *temp = p;
	while (p != NULL) {
		temp = p;
		p = p->next;
		temp->next = NULL;
		insert_list_ratio(plist, temp);	
	}

	return;
}

/* Sort by the value of the VM speed from largest to smallest */
static void insertionSort_speed(dom_stats * plist)
{
	if (plist == NULL || plist->next == NULL) {
		return;
	}

	dom_stats *p = plist->next;
	plist->next = NULL;

	dom_stats *temp = p;
	while (p != NULL) {
		temp = p;
		p = p->next;
		temp->next = NULL;
		insert_list_speed(plist, temp);	
	}

	return;
}

/* Scheduling initialization */
static void scheduler_init(libxl_ctx * ctx)
{
	int dom_num, i = 0;
	dom_stats *p = NULL;
	
	dom_stats *dom_stat_data = get_dom_stats(ctx, &dom_num);
	list_info.total = dom_num - 1;

	// First put all VMs into list3 according to the value of ratio, from large to small
	for (i = 0; i < dom_num; ++i) {
		if (dom_stat_data[i].domid == 0) {
			continue;
		}

		p = (dom_stats *)malloc(sizeof(dom_stats));
		p->domid = dom_stat_data[i].domid;
		p->ratio = dom_stat_data[i].ratio;
		p->net_speed = dom_stat_data[i].net_speed;
		p->up = 0;
		p->next = NULL;
		
		/* Insert node p into list3 in descending order of ratio value */
		insert_list_ratio(list3, p);
	}
	traverse_list(list3);
	
	// Move VMs with ratio values ​​above the set threshold in list3 to list2
	if (list_info.total != 0 ) {
		list2->next = list3->next;
		p = list3->next;
		for (; list_info.list2_num < list_info.total * Threshold_Ratio; ++list_info.list2_num) {
			p = p->next;
		}
		list3->next = p->next;
		p->next = NULL;
	}
	
	insertionSort_speed(list2);
	traverse_list(list2);
	
	// Move the VM with the speed value above the set threshold in list2 to list1
	if (list_info.list2_num != 0) {
		list1->next = list2->next;
		p = list2->next;
		for (; list_info.list1_num < list_info.list2_num * Threshold_Speed; ++list_info.list1_num) {
			p = p->next;
		}
		list2->next = p->next;
		p->next = NULL;
		list_info.list2_num -= list_info.list1_num;
	}

	traverse_list(list1);
	traverse_list(list2);
	traverse_list(list3);
	
	free(dom_stat_data);
}

static void initialization(libxl_ctx *ctx)
{
	/* Initialize the xenstore of each DomU and grant read and write permissions */
	xenstore_init(ctx);
	
	/* Remove the VF of all VMs and initialize the global variable vf_info */
	vf_init(ctx);
	
	/* Initialize the linked list */
	list_init();
	
	/* Initialize the queue */
	init_Queue(&Q);

	return;
}

/* Find if there is a VM in the linked list */
static bool find_dom_list(dom_stats * plist, int domid)
{
	dom_stats * p = NULL;
	
	p = plist->next;
	while (p != NULL) {
		if (p->domid == domid) {
			return true;
		}
		p = p->next;
	}
	
	return false;
}

static dom_stats *get_dom_list(dom_stats * plist, int domid)
{
	dom_stats * p = NULL;
	
	p = plist->next;
	while (p != NULL) {
		if (p->domid == domid) {
			return p;
		}
		p = p->next;
	}
	
	return NULL;
}

static bool find_dom_info(libxl_dominfo *dom_info, int dom_num, int domid)
{
	int i = 0;
	for (i = 0; i < dom_num; ++i) {
		if (dom_info[i].domid == 0 ) {
			continue;
		}

		if (dom_info[i].domid == domid) {
			return true;
		}
	}

	return false;
}

static void find_missed_dom(libxl_dominfo *dom_info, int dom_num, dom_stats *plist)
{
	dom_stats *p, *pre;

	p = plist->next;
	pre = plist;
	while (p != NULL) {
		if(!find_dom_info(dom_info, dom_num, p->domid)) {
			pre->next = p->next;
			printf("missed VM is domain%d\n", p->domid);
			free(p);

			p = pre->next;
			continue;
		}
		pre = p;
		p = p->next;
	}

	return;
}

static void free_list(dom_stats * plist)
{
	dom_stats * p;
	
    if(plist == NULL) {
		return;
	}

    p = plist->next;
    while(p != NULL) {
        free(plist);
        plist = p;
        p = p->next;
    }
    free( plist );

    return;
}

/* Capture the changing state of VMs, and update the number of VMs in the list */
static void update_dom_number(libxl_ctx * ctx)
{
	int dom_num, domid, i = 0;
    libxl_dominfo *dom_info;
	dom_stats * p = NULL;
	static xentoollog_logger *logger;

	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);

    dom_info = libxl_list_domain(ctx, &dom_num);

    libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger*)logger);
	
	// If there are VMs increase or decrease
    if (list_info.total != dom_num - 1) {
		printf("update dom...\n");

		if (list_info.total < dom_num - 1) {
			//If the number of VMs increases
			for (i = 0; i < dom_num; ++i) {
				domid = dom_info[i].domid;
				if (domid == 0) {
					continue;
				}

				// Determine whether it is a newly added VM
				if (!(find_dom_list(list1, domid) ||
					find_dom_list(list2, domid) || find_dom_list(list3, domid)) ) {
					// Initialize the xenstore of the VM
					xenstore_initdom(domid);

					p = (dom_stats *)malloc(sizeof(dom_stats));
					p->domid = domid;
					p->ratio = 0;
					p->net_speed = 0;
					p->up = 0;
					p->next = NULL;

					printf("new VM is domain%d\n", domid);

					insert_list_ratio(list1, p);

					// Assign VF to the VM
					vf_assign_dom(ctx, p);
				} 
			}	
		} else {
			// If the number of VMs decreases
			find_missed_dom(dom_info, dom_num, list1);
			find_missed_dom(dom_info, dom_num, list2);
			find_missed_dom(dom_info, dom_num, list3);
		}
	}
    free(dom_info);

    return;
}

static void update_list(dom_stats *list1_t, dom_stats *list2_t,
						dom_stats *list3_t)
{
	dom_stats * p = NULL, * q = NULL;

	// Swap list1 and list1_t
	p = list1_t;
	list1_t = list1;
	list1 = p;

	// Update the VF status of list1
	p = list1->next;
	while (p != NULL) {
		q = get_dom_list(list1_t, p->domid);
		if (q != NULL) {
			p->up = q->up;
		}

		p = p->next;
	}
	free_list(list1_t);
		
	// Swap list2 and list2_t
	p = list2_t;
	list2_t = list2;
	list2 = p;

	// Update the VF status of list2
	p = list2->next;
	while (p != NULL) {
		q = get_dom_list(list2_t, p->domid);
		if (q != NULL) {
			p->up = q->up;
		}
		p = p->next;
	}
	free_list(list2_t);
	
	// Swap list1 and list1_t
	p = list3_t;
	list3_t = list3;
	list3 = p;	

	// Update the VF status of list1
	p = list3->next;
	while (p != NULL) {
		q = get_dom_list(list3_t, p->domid);
		if (q != NULL) {
			p->up = q->up;
		}
		p = p->next;
	}
	free_list(list3_t);
	
	traverse_list(list1);
	traverse_list(list2);
	traverse_list(list3);
	
	return;
}

/* Recalculate and update the VMs in the list */
static void update_dom_stats(libxl_ctx * ctx)
{
	int dom_num, i = 0;
	dom_stats * p = NULL;
	
	// Create three temporary lists
	dom_stats * ist1_t, *list2_t, *list3_t;

	list1_t = (dom_stats *)malloc(sizeof(dom_stats));    
    list1_t->next = NULL; 

	list2_t = (dom_stats *)malloc(sizeof(dom_stats));    
    list2_t->next = NULL; 

	list3_t = (dom_stats *)malloc(sizeof(dom_stats));    
    list3_t->next = NULL; 
	
	dom_stats * dom_stat_data = get_dom_stats(ctx, &dom_num);
	
	list_info.total = dom_num - 1;
	for (i = 0; i < dom_num; ++i) {
		if (dom_stat_data[i].domid == 0) {
			continue;
		}

		p = (dom_stats *)malloc(sizeof(dom_stats));
		p->domid = dom_stat_data[i].domid;
		p->ratio = dom_stat_data[i].ratio;
		p->net_speed = dom_stat_data[i].net_speed;
		p->up = 0;
		p->next = NULL;

		insert_list_ratio(list3_t, p);
	}
	//traverse_list(list3_t);
	
	list_info.list2_num = 0;
	if (list_info.total != 0 ) {
		list2_t->next = list3_t->next;
		p = list3_t->next;
		for (; list_info.list2_num<list_info.total * Threshold_Ratio;
			++list_info.list2_num) {
			p = p->next;
		}
		list3_t->next = p->next;
		p->next = NULL;
	}

	insertionSort_speed(list2_t);
	//traverse_list(list2_t);
	
	list_info.list1_num = 0;
	if (list_info.list2_num != 0) {
		list1_t->next = list2_t->next;
		p = list2_t->next;
		for (; list_info.list1_num<list_info.list2_num * Threshold_Speed;
			++list_info.list1_num) {
			p = p->next;
		}
		list2_t->next = p->next;
		p->next = NULL;
		list_info.list2_num -= list_info.list1_num;
	}

	update_list(list1_t, list2_t, list3_t);
	
	free(dom_stat_data);
	
	return;
}

static void scheduler(libxl_ctx *ctx)
{
	migrat_sched(ctx);

	update_dom_number(ctx);
	
	update_dom_stats(ctx);
	
	if (list_info.total < 4) {
		vf_assign_list(ctx, list1);
		vf_assign_list(ctx, list2);
		vf_assign_list(ctx, list3);
	} else {
		vf_remove_list(ctx, list2);
		vf_remove_list(ctx, list3);

		vf_assign_list(ctx, list1);
	}

	return;
}

int main(int argc, char *argv[])
{
	libxl_ctx *ctx;
	static xentoollog_logger *logger;

	logger = (xentoollog_logger *)xtl_createlogger_stdiostream(stderr, 4, 0);
    libxl_ctx_alloc(&ctx, 0, 0, (xentoollog_logger *)logger);

	initialization(ctx);

	scheduler_init(ctx);

	// If the number of VMs is less than 4, assign VF to all VMs
	if (list_info.total < 4) {
		vf_assign_list(ctx, list1);
		vf_assign_list(ctx, list2);
		vf_assign_list(ctx, list3);
	} else {
		// Assign VF to the VM of list1
		vf_assign_list(ctx, list1);
	}
	
	while (1) {
		scheduler(ctx);		

		sleep(10);
	}

	libxl_ctx_free(ctx);
    xtl_logger_destroy((xentoollog_logger*)logger);

	return 0;
}