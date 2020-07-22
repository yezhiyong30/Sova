#include <xenstore.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>

#define MAX_BUFF_SIZE 256

static int frequency;
static pthread_mutex_t freq_lock;

struct net_stat {
    const char *dev;
    int up;
    long long last_read_recv, last_read_trans; //last read total num
    long long recv, trans; //real total num
    double recv_speed, trans_speed;
};

/* network interface stuff */
static struct net_stat netstats[16] = {0};

static struct net_stat *get_net_stat(const char *dev)
{
    unsigned int i = 0;

    if (!dev)
        return NULL;

    /* find interface stat */
    for (i = 0; i < 16; i++) {
        if (netstats[i].dev && strcmp(netstats[i].dev, dev) == 0) {
            return &netstats[i];
        }
    }

    /* wasn't found? add it */
    if (i == 16) {
        for (i = 0; i < 16; i++) {
            if (netstats[i].dev == NULL) {
                netstats[i].dev = strndup(dev, MAX_BUFF_SIZE);
                return &netstats[i];
            }
        }
    }
    fprintf(stderr, "too many interfaces used (limit is 16).");

    return NULL;
}

static void clear_net_stats(void)
{
    memset(netstats, 0, sizeof(netstats));
}

static void update_net_stats(const char *dev, double delta)
{
    int i = 0;
    char buf[MAX_BUFF_SIZE] = {0};
    FILE *net_dev_fp;

    /* open file and ignore first two lines */
    if (!(net_dev_fp = fopen("/proc/net/dev", "r"))) {
        fprintf(stderr, "fopen failed.\n");
        //clear_net_stats();
        return;
    }
    fgets(buf, 255, net_dev_fp);    /* garbage */
    fgets(buf, 255, net_dev_fp);    /* garbage (field names) */

    /* read each interface */
    for (i = 0; i < 16; i++) {
        struct net_stat *ns = NULL;
        unsigned char *s = NULL, *p = NULL;
        unsigned long long r = 0, t = 0;
        unsigned long long last_recv = 0, last_trans = 0;

        if (fgets(buf, 255, net_dev_fp) == NULL) {
            //File EOF
            break;
        }

        //Skip Space
        p = buf;
        while (isspace((int) *p)) {
            p++;
        }
        /*s: network interface name*/
        s = p;

        //Skip Network Interface Name
        while (*p && *p != ':') {
            p++;
        }
        if (*p == '\0') {
            continue;
        }
        *p = '\0';
        /*p: reveive bytes*/
        p++;

        //Judge Network Interface or Not?
        if (strcmp(s, dev) != 0)
            continue;

        //Get struct net_stat
        ns = get_net_stat(s);
        ns->up = 1;
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

        /* calculate speeds */
        if (last_recv == 0)
            ns->recv_speed = 0;
        else
            ns->recv_speed = (ns->recv - last_recv) / (1024 * delta);
        if (last_trans == 0)
            ns->trans_speed = 0;
        else
            ns->trans_speed = (ns->trans - last_trans) / (1024 * delta);
        /*
        //First Time Run, Or Time Overflow
        if(current_time == 0)
        {
            ns->recv_speed = 0;
            ns->trans_speed = 0;
        }
        */
        //Find Network Interface, And Work Over
        break;
    }

    fclose(net_dev_fp);
}

static int init_daemon(void) 
{ 
    int pid = 0; 
    int i = 0; 
 
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    
    pid = fork();
    if (pid > 0) {
        exit(0);
    } else if (pid < 0) { 
        return -1;
    }
 
    setsid();
 
    pid = fork();
    if (pid > 0) {
        exit(0);
    } else if (pid < 0) {
        return -1;
    }
 
    for(i = 0; i < NOFILE; close(i++));
 
    chdir("/");
    umask(0);
    signal(SIGCHLD, SIG_IGN); 

    return 0;
}

static void *freq_handle(void *arg)
{
    int i = 0;
    FILE *net_dev_fp;
    char buf[MAX_BUFF_SIZE] = {0};
    static long long last_read_recv, last_read_trans;

    while(1) {
        /* open file and ignore first two lines */
        if (!(net_dev_fp = fopen("/proc/net/dev", "r"))) {
            fprintf(stderr, "fopen failed.\n");
            //clear_net_stats();
            return;
        }
        fgets(buf, 255, net_dev_fp);    /* garbage */
        fgets(buf, 255, net_dev_fp);    /* garbage (field names) */

        /* read each interface */
        for (i = 0; i < 16; i++) {
            unsigned char *s = NULL, *p = NULL;
            unsigned long long r = 0, t = 0;

            if (fgets(buf, 255, net_dev_fp) == NULL) {
                //File EOF
                break;
            }

            //Skip Space
            p = buf;
            while (isspace((int) *p)) {
                p++;
            }
            /*s: network interface name*/
            s = p;

            //Skip Network Interface Name
            while (*p && *p != ':') {
                p++;
            }
            if (*p == '\0') {
                continue;
            }
            *p = '\0';
            /*p: reveive bytes*/
            p++;

            //Judge Network Interface or Not?
            if (strcmp(s, dev) != 0)
                continue;

            /* bytes packets errs drop fifo frame compressed multicast|bytes ... */
            sscanf(p, "%lld %*d %*d %*d %*d %*d %*d %*d %lld", &r, &t);

            if (last_read_recv != 0 && r - last_read_recv > 128 &&
                last_read_trans ！= 0 && t - last_read_trans > 128) {
                pthread_mutex_lock(&freq_lock);
                frequency++;
                pthread_mutex_unlock(&freq_lock);
            }
            last_read_recv = r;
            last_read_trans = t;

            break;
        }
        fclose(net_dev_fp);

        sleep(1);
    }
}

int main(void)
{
    struct net_stat *ns = NULL;
    xs_transaction_t xt, th;
    char pathn[50] = {0}, path_freq[50] = {0};
    char *bo = NULL, *buf = NULL, *path = NULL;;
    char result[100] = {0};
    int domid, len, err;
    pthread_t thread_id;
    struct xs_handle *xh = xs_domain_open();

    init_daemon();

    if (!xh) {
        printf("error.\n");
        return -1;
    }

    xt = xs_transaction_start(xh);  
    if (!xt) {
        printf("error.\n");
        return -1;
    }

    buf = xs_read(xh, XBT_NULL, "domid", &len);
    if (!buf) {   
        printf("Could not read domid.\n");   
        return -1;   
    }
    domid = atoi(buf);

    path = xs_get_domain_path(xh, domid);
    if (path == NULL) {  
        printf("Dom Path in Xenstore not found.\n" );   
        return -1;   
    }
    xs_transaction_end(xh, xt, false);

    path = realloc(path, strlen(path) + strlen("/network/bool") + 1);
    strcpy(pathn, path);
    strcpy(path_freq, path);
    strcat(path, "/network/bool");
    strcat(pathn, "/network/value");
    strcat(path_freq, "/network/freq");

    update_net_stats("bond0", 1);
    ns = get_net_stat("bond0");

    if (pthread_mutex_init(&freq_lock, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init error!\n");
        return -1;
    }
    
    if (pthread_create(&thread_id, NULL, (void *)(&freq_handle), NULL) == -1) {
        fprintf(stderr, "pthread_create error!\n");
        return -1;
    }

    while(1) {
        update_net_stats("bond0", 1);
        ns = get_net_stat("bond0");

        xt = xs_transaction_start(xh);
        sprintf(result, "%d", (int)(ns->recv_speed + ns->trans_speed));
        if(!xs_write(xh, xt, pathn, result, strlen(result)))
            printf("error.\n");

        pthread_mutex_lock(&freq_lock);
        sprintf(result, "%d", (int)(frequency));
        if(!xs_write(xh, xt, path_freq, result, strlen(result)))
            printf("error.\n");
        frequency = 0;
        pthread_mutex_unlock(&freq_lock);

        xs_transaction_end(xh, xt, false);

        if(!strcmp(bo, "1")) {
            clear_net_stats();

            xt = xs_transaction_start(xh);
            printf("change find.");
            if(!xs_write(xh, xt, path, "0", strlen("0")))
                printf("error.\n");
            xs_transaction_end(xh, xt, false);
        }
        sleep(10);
    }

    xs_transaction_end(xh, xt, false);
    xs_close(xh);  
    free(buf);
}
