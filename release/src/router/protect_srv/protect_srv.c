 /*
 * Copyright 2017, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */
 
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>
/*--*/
#include <libptcsrv.h>
#include <linklist.h>

#ifdef ASUSWRT_SDK /* ASUSWRT SDK */
#else /* DSL_ASUSWRT SDK */
#include <shared.h>
#include "libtcapi.h"
#endif

#ifdef RTCONFIG_NOTIFICATION_CENTER
#include <libnt.h>
#endif

#define MyDBG(fmt,args...) \
	if(isFileExist(PROTECT_SRV_DEBUG) > 0) { \
		Debug2Console("[ProtectionSrv][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args); \
	}
#define ErrorMsg(fmt,args...) \
	Debug2Console("[ProtectionSrv][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args);

/*  ### [RETRY_EXPIRED_MODE] ###       *
 *  Switch expired mode/unlimit mode   */
#define ENABLE_RETRY_EXPIRED_MODE 1

/*  ### For user retry */
#define RETRY_INTERVAL_TIME 60
#define RETRY 5

/*  ### DROP_EXPIRED_MODE] ###  *
 *  For drop src ip time        */
#define ENABLE_DROP_EXPIRED_MODE 1
#define PROTECTION_VALIDITY_TIME 5 /* minutes */

#define MUTEX pthread_mutex_t
#define MUTEXINIT(m) pthread_mutex_init(m, NULL)
#define MUTEXLOCK(m) pthread_mutex_lock(m)
#define MUTEXTRYLOCK(m) pthread_mutex_trylock(m)
#define MUTEXUNLOCK(m) pthread_mutex_unlock(m)
#define MUTEXDESTROY(m) pthread_mutex_destroy(m)


enum {
	OFF=0,
	ON
};

static MUTEX report_list_lock;
static MUTEX drop_list_lock;
struct list *report_list=NULL;
struct list *drop_list=NULL;
static int terminated = 1;

/* ------- Internal Function Define -------- */
static int insert_drop_rules();
static void start_local_socket(void);
static void local_socket_thread(void);
/* ------- Internal Function Define End ----- */


void handlesignal(int signum)
{
	if (signum == SIGUSR1) {
		insert_drop_rules();
	} else if (signum == SIGTERM) {
		terminated = 0;
	} else
		MyDBG("Unknown SIGNAL\n");
	
}

static void signal_register(void)
{
	struct sigaction sa;
	
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler =  &handlesignal;
	/* Handle Inser Firewall Rules via SIGUSR1 */
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);    
}

static PTCSRV_STATE_REPORT_T *stateReport_listcreate(PTCSRV_STATE_REPORT_T input)
{
	PTCSRV_STATE_REPORT_T *new=malloc(sizeof(*new));
	
	memcpy(new,&input,sizeof(*new));
	return new;
}

static DROP_REPORT_T *drop_listcreate(DROP_REPORT_T input)
{
	DROP_REPORT_T *new=malloc(sizeof(*new));
	
	memcpy(new,&input,sizeof(*new));
	return new;
}

static int insert_drop_rules()
{
	FILE *fp;
	struct listnode *ln;
	DROP_REPORT_T *sreport;
	
	if ((fp = fopen(PROTECT_SRV_RULE_FILE, "w")) == NULL) 
		return -1;
	
	fprintf(fp, "*filter\n");
	
	LIST_LOOP(drop_list,sreport,ln)
	{
		fprintf(fp, "-A %s -s %s -j DROP\n", PROTECT_SRV_RULE_CHAIN, sreport->addr);
	}
	
	fprintf(fp, "COMMIT\n");
	fclose(fp);
	system("iptables -F " PROTECT_SRV_RULE_CHAIN);
	system("iptables-restore --noflush " PROTECT_SRV_RULE_FILE);
	MyDBG("Finish inser ruls to %s chain.\n", PROTECT_SRV_RULE_CHAIN)
	return 1;
}

static void insert_to_drop_list(char *addr)
{
	DROP_REPORT_T *sreport;
	DROP_REPORT_T report;
	struct listnode *ln;
	char   log[256];
	time_t now;
	
	LIST_LOOP(drop_list,sreport,ln)
	{
		if (!strcmp(sreport->addr, addr)) {
			MyDBG("Already in drop_list return.\n");
			return;
		}
	}
	strcpy(report.addr, addr);
#ifdef ENABLE_DROP_EXPIRED_MODE
	report.tstamp = time(&now);
#else
	report.tstamp = 0;
#endif
	sreport=drop_listcreate(report);
	
	if (sreport)
		listnode_add(drop_list,(void*)sreport);
	
#ifdef RTCONFIG_NOTIFICATION_CENTER
	char msg[100];
	snprintf(msg, sizeof(msg), "{\"IP\":\"%s\",\"msg\":\"\"}", report.addr);
	SEND_NT_EVENT(LOGIN_FAIL_SSH_EVENT, msg);
#endif
	MyDBG("New [%s] dropTime:[%d minutes] add to drop list\n",report.addr, PROTECTION_VALIDITY_TIME);
	snprintf(log, sizeof(log), "Detect [%s] abnormal logins many times, system will block this IP %d minutes.\n",
		report.addr, PROTECTION_VALIDITY_TIME);
	syslog(LOG_WARNING, log);
	
	insert_drop_rules();
}

/*
static int query_addr_from_drop_list(char *addr)
{
	DROP_REPORT_T *sreport;
	struct listnode *ln;
	
	LIST_LOOP(drop_list,sreport,ln)
	{
		if (!strcmp(sreport->addr, addr))
			return 1;
	}
	return 0;
}
*/

#ifdef ENABLE_DROP_EXPIRED_MODE
static void check_drop_list()
{
	DROP_REPORT_T *sreport;
	struct listnode *ln;
	int check;
	time_t now;
	
	check = 0;
	MUTEXLOCK(&drop_list_lock);
	LIST_LOOP(drop_list,sreport,ln)
	{
		if (time(&now) - sreport->tstamp >= PROTECTION_VALIDITY_TIME*60) {
			MyDBG("[%s] VALIDITY_TIME:[%d] has been timeout.\n", sreport->addr, PROTECTION_VALIDITY_TIME*60);
			check = 1;
			break;
		}
	}
	if (check) {
		MyDBG("Delete [%s] from drop_list\n", sreport->addr);
		listnode_delete(drop_list, sreport);
		MUTEXUNLOCK(&drop_list_lock);
		insert_drop_rules();
	} else 
		MUTEXUNLOCK(&drop_list_lock);
}
#endif

void check_report_list()
{
	PTCSRV_STATE_REPORT_T *sreport;
	struct listnode *ln;
	int check;
	
	check = 0;
	MUTEXLOCK(&report_list_lock);
	LIST_LOOP(report_list,sreport,ln)
	{
		if (sreport->frequency >= RETRY) {
			MyDBG("[%s] already over %d retry time, add to droplist\n", sreport->addr, RETRY);
			insert_to_drop_list(sreport->addr);
			check = 1;
			break;
		} 
#if ENABLE_RETRY_EXPIRED_MODE
		else if (time((time_t *) NULL) - sreport->tstamp >= RETRY_INTERVAL_TIME) {
			MyDBG("[%s] retry timeout remove from report_list\n", sreport->addr);
			check = 1;
			break;
		}
#endif
	}
	if (check) {
		MyDBG("Delete [%s] from report_list\n", sreport->addr);
		listnode_delete(report_list, sreport);
	}
	MUTEXUNLOCK(&report_list_lock);
}

void receive_s(int newsockfd)
{
	PTCSRV_STATE_REPORT_T report;
	PTCSRV_STATE_REPORT_T *sreport;
	int n;
	int checkup;
	time_t now;
	struct listnode *ln;
	
	bzero(&report,sizeof(PTCSRV_STATE_REPORT_T));
	
	n = read( newsockfd, &report, sizeof(PTCSRV_STATE_REPORT_T));
	if( n < 0 )
	{
	        MyDBG("ERROR reading from socket.\n");
	        return;
	}
	
	MyDBG("[receive report] addr:[%s] s_type:[%d] msg:[%s]\n",report.addr, report.s_type, report.msg);
	
	if (GetDebugValue(PROTECT_SRV_DEBUG)) {
		char info[200];
		sprintf(info, "echo \"[ProtectionSrv][receive report] addr:[%s] s_type:[%d] msg:[%s]\" >> %s",report.addr, report.s_type, report.msg, PROTECT_SRV_LOG_FILE);
		system(info);
	}
	
	checkup = 0;
	MUTEXLOCK(&report_list_lock);
	if (report_list) {
		LIST_LOOP(report_list,sreport,ln) {
			if (!strcmp(sreport->addr, report.addr)) {
				if (time(&now) - sreport->tstamp <= RETRY_INTERVAL_TIME) {
					sreport->frequency +=1;
					checkup = 1;
					MyDBG("[%s] frequency chage to  %d \n",report.addr, sreport->frequency);
				} 
#if ENABLE_RETRY_EXPIRED_MODE
				else {
					//Update info
					sreport->frequency = 1;
					report.tstamp = time(&now);
					MyDBG("[RETRY INTERVAL TIME OUT][Update] [%s] info to report_list.\n", sreport->addr);
				}
#endif
			}
		}
		if (!checkup) {  //The addr is new, then add in list
			report.frequency = 1;
#if ENABLE_RETRY_EXPIRED_MODE
			report.tstamp = time(&now);
#else
			report.tstamp = 0;
#endif
			MyDBG("New [%s] tstamp:[%d] add to list\n",report.addr, report.tstamp);
			sreport=NULL;
			sreport=stateReport_listcreate(report);
			if (sreport)
				listnode_add(report_list,(void*)sreport);
		}
	} else { //The report_list is NULL
		report.frequency = 1;
#if ENABLE_RETRY_EXPIRED_MODE
			report.tstamp = time(&now);
#else
			report.tstamp = 0;
#endif
		MyDBG("New [%s] tstamp:[%d] add to list\n",report.addr, report.tstamp);
		sreport=NULL;
		sreport=stateReport_listcreate(report);
		if (sreport)
			listnode_add(report_list,(void*)sreport);
	
	}
	MUTEXUNLOCK(&report_list_lock);
	
}

static void start_local_socket(void)
{
	struct sockaddr_un addr;
	int sockfd, newsockfd;
	
	if ( (sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		MyDBG("socket error\n");
		perror("socket error");
		exit(-1);
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, PROTECT_SRV_SOCKET_PATH, sizeof(addr.sun_path)-1);
	
	unlink(PROTECT_SRV_SOCKET_PATH);
	
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		MyDBG("socket bind error\n");
		perror("socket bind error");
		exit(-1);
	}
	
	if (listen(sockfd, 3) == -1) {
		MyDBG("listen error\n");
		perror("listen error");
		exit(-1);
	}
	
	while (1) {
		if ( (newsockfd = accept(sockfd, NULL, NULL)) == -1) {
			MyDBG("accept error\n");
			perror("accept error");
			continue;
		}
		
		receive_s(newsockfd);
		close(newsockfd);
	}
}

static void local_socket_thread(void)
{
	pthread_t thread;
	pthread_attr_t attr;
	
	MyDBG("Start local socket thread.\n");
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread,NULL,(void *)&start_local_socket,NULL);
	pthread_attr_destroy(&attr);
}

int main(void)
{
	char cmd[20];
	int pid;
	
	MyDBG("[Start ProtectionSrv]\n");
	
	pid = getpid();
	sprintf(cmd,"echo %d > %s",pid, PROTECT_SRV_PID_PATH);
	system(cmd);
	
	/* Signal */
	signal_register();
	
	/* start unix socket */
	local_socket_thread();
	
	/* record failed login info */
	report_list=list_new();
	
	/* record drop addr */
	drop_list=list_new();
	
	/* init mutex lock switch */
	MUTEXINIT(&report_list_lock);
	MUTEXINIT(&drop_list_lock);
	
	
	while (terminated) {
		check_report_list();
#ifdef ENABLE_DROP_EXPIRED_MODE
		check_drop_list();
#endif
		sleep(1);
	}	
	
	MyDBG("ProtectionSrv Terminated\n");
	
	MUTEXDESTROY(&report_list_lock);
	MUTEXDESTROY(&drop_list_lock);

	return 0;
}
