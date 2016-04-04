 /*
 * Copyright 2015, ASUSTeK Inc.
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
#include <sys/un.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <syslog.h>
/* -- */
#include <nt_center.h>
#include <nt_eInfo.h>
#include <libnt.h>

#define MyDBG(fmt,args...) \
	if(isFileExist(NOTIFY_CENTER_DEBUG) > 0) { \
		Debug2Console("[Notification_Center][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args); \
	}
#define ErrorMsg(fmt,args...) \
	Debug2Console("[Notification_Center][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args);


#define PRINT_DEBUG_TO_FILE 1

#define MUTEX pthread_mutex_t
#define MUTEXINIT(m) pthread_mutex_init(m, NULL)
#define MUTEXLOCK(m) pthread_mutex_lock(m)
#define MUTEXTRYLOCK(m) pthread_mutex_trylock(m)
#define MUTEXUNLOCK(m) pthread_mutex_unlock(m)
#define MUTEXDESTROY(m) pthread_mutex_destroy(m)

/* Globals */
static NT_CHECK_SWITCH_T *pEsw = NULL;

enum {
	OFF=0,
	ON
};

static MUTEX event_list_lock;
struct list *event_list=NULL;
static int terminated = 1;

static int send_cnt = 0;
static int rec_cnt = 0;

NOTIFY_EVENT_T *event_listcreate(NOTIFY_EVENT_T input)
{
	NOTIFY_EVENT_T *new=malloc(sizeof(*new));
	
	memcpy(new,&input,sizeof(*new));
	return new;
}

void receive_s(int newsockfd)
{
	int    n;
	time_t now;
	char   date[30];
	NOTIFY_EVENT_T event_t;
	NOTIFY_EVENT_T *sevent_t;
	
	memset(&event_t, 0, sizeof(NOTIFY_EVENT_T));

	n = read( newsockfd, &event_t, sizeof(NOTIFY_EVENT_T));
	if( n < 0 )
	{
		ErrorMsg("ERROR reading from socket.\n");
		return;
	}


	/* Check Event */
	if(eInfo_get_idx_by_evalue(event_t.event) < 0) {
		
		MyDBG("Warning!!! receive undefined event, drop [%x] event.\n", event_t.event );
		return;
	}
	
	event_t.tstamp = time(&now);
	StampToDate(event_t.tstamp, date);
	rec_cnt++;
	
	MyDBG("[%s] event:[%s (%x)] msg:[%s] Num:[%d]\n", date, eInfo_get_eName(event_t.event), event_t.event, event_t.msg, rec_cnt);
	
#if PRINT_DEBUG_TO_FILE
	if(GetDebugValue(NOTIFY_CENTER_DEBUG)) {
		char info[200];
		sprintf(info, "echo \"[Notification_Center][receive event] [%s] event:[%s (%x)] msg:[%s] Num:[%d]\" >> %s", 
			date, eInfo_get_eName(event_t.event), event_t.event, event_t.msg, rec_cnt, NOTIFY_CENTER_LOG_FILE);
		system(info);
	}
#endif
	
	MUTEXLOCK(&event_list_lock);
	if(event_list) {
		sevent_t=NULL;
		sevent_t=event_listcreate(event_t);
		if(sevent_t)
			listnode_add(event_list,(void*)sevent_t);
	
	}
	MUTEXUNLOCK(&event_list_lock);
	
}
static int start_local_socket(void)
{
	struct sockaddr_un addr;
	int sockfd, newsockfd;
	
	if ( (sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		ErrorMsg("socket error\n");
		perror("socket error");
		exit(-1);
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, NOTIFY_CENTER_SOCKET_PATH, sizeof(addr.sun_path)-1);
	
	unlink(NOTIFY_CENTER_SOCKET_PATH);
	
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		ErrorMsg("socket bind error\n");
		perror("socket bind error");
		exit(-1);
	}
	
	if (listen(sockfd, MAX_NOTIFY_SOCKET_CLIENT) == -1) {
		ErrorMsg("listen error\n");
		perror("listen error");
		exit(-1);
	}
	
	while (1) {
		if ( (newsockfd = accept(sockfd, NULL, NULL)) == -1) {
			ErrorMsg("accept error\n");
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
	
	MyDBG("Start unix socket thread.\n");
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread,NULL,&start_local_socket,NULL);
	pthread_attr_destroy(&attr);
}

static int get_event_action(int e)
{
	int i;
	for(i=0; i < MAX_NOTIFY_EVENT_NUM; i++) {
		if(pEsw[i].event == e) {
			return pEsw[i].notify_sw;
		}
	}
	return -1;
}
static void action_handler(int action, NOTIFY_EVENT_T e)
{
	//Send Action Notify Mail & Write App/WebUI event into NT_DB
	if(action & (ACTION_NOTIFY_WEBUI | ACTION_NOTIFY_APP)) {
		MyDBG("[IMPORT WEBUI/APP INTO DATABASE]\n");
		NOTIFY_DATABASE_T *input_t = initial_db_input();
		input_t->tstamp = e.tstamp;
		input_t->event = e.event;
		input_t->flag = action;
		strcpy(input_t->msg, e.msg);
		NT_DBCommand("write", input_t);
		db_input_free(input_t);
	}
	if(action & ACTION_NOTIFY_EMAIL) {
		MyDBG("[SEND_EMAIL_NOTIFY]\n");
		send_notify_event(&e, NOTIFY_MAIL_SERVICE_SOCKET_PATH);
	}
	if(action & ACTION_NOTIFY_WEEKLY) {
		MyDBG("[SEND_WEEKLY_NOTIFY]\n");
	}
}

static void event_handler()
{
	NOTIFY_EVENT_T *listevent;
	NOTIFY_EVENT_T  actevent;
	struct listnode *ln;
	time_t now;
	char date[30];
	
	StampToDate(time(&now), date);
	MUTEXLOCK(&event_list_lock);
	LIST_LOOP(event_list,listevent,ln)
	{
		send_cnt++;
		MyDBG("[%s]NOTIFY_EVENT[%s (%x)] Action:[%x] MSG:[%s] Num:[%d]\n", date, eInfo_get_eName(listevent->event), listevent->event,
			get_event_action(listevent->event), listevent->msg, send_cnt);
#if PRINT_DEBUG_TO_FILE
		if(GetDebugValue(NOTIFY_CENTER_DEBUG)) {
			char info[200];
			sprintf(info, "echo \"[Notification_Center][  send  event] [%s] event:[%s (%x)] Action:[%x] MSG:[%s] Num:[%d]\" >> %s",
				date, eInfo_get_eName(listevent->event), listevent->event,
				get_event_action(listevent->event), listevent->msg, send_cnt, NOTIFY_CENTER_LOG_FILE);
			system(info);
		}
#endif
		actevent.event  = listevent->event;
		actevent.tstamp = listevent->tstamp;
		strcpy(actevent.msg, listevent->msg);
		action_handler(get_event_action(listevent->event), actevent);
		break;
	}
	
	listnode_delete(event_list, listevent);
	MUTEXUNLOCK(&event_list_lock);
}

static void handlesignal(int signum)
{
	if (signum == SIGUSR1) {
		//TODO: update event switch structure
		;
	} else if (signum == SIGTERM) {
		terminated = 0;
	} else
		MyDBG("Unknown SIGNAL\n");
	
}

static void signal_register(void) {
	
	struct sigaction sa;
	
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler =  &handlesignal;
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);    
}

static int esw_init(void)
{
	pEsw = (NT_CHECK_SWITCH_T *) malloc(sizeof(NT_CHECK_SWITCH_T)* MAX_NOTIFY_EVENT_NUM);
	if(pEsw == NULL) {
		MyDBG("pEsw malloc error\n");
		return -1;
	}
	memset(pEsw, 0, sizeof(NT_CHECK_SWITCH_T));
	return 0;
}
static void free_esw()
{
	if(pEsw) {
		free(pEsw);
	}
}

static void load_esw_info()
{
	int i;
	//TODO: fake data
	for(i=0; i < MAX_NOTIFY_EVENT_NUM; i++) {
		pEsw[i].event = WAN_DISCONN_EVENT;
		pEsw[i].notify_sw = ACTION_NOTIFY_WEBUI | ACTION_NOTIFY_EMAIL | ACTION_NOTIFY_APP;
	}
}

int  main(void)
{
	char cmd[20];
	int pid;
	
	MyDBG("[Start Notification_Center]\n");
	
	pid = getpid();
	sprintf(cmd,"echo %d > %s",pid, NOTIFY_CENTER_PID_PATH);
	system(cmd);
	
	/* Signal */
	signal_register();
	
	/* start unix socket */
	local_socket_thread();
	
	/* record event info*/
	event_list=list_new();
	
	/* init mutex lock switch */
	MUTEXINIT(&event_list_lock);
	
	/* malloc pEsw */
	if(esw_init() < 0) return -1;
	
	/* Get event notify action value */
	//TODO: fake data
	load_esw_info();
	
	while(terminated) {
		event_handler();
		usleep(1000);
	}
	
	/* free memory */
	free_esw();
	
	MUTEXDESTROY(&event_list_lock);
	
	MyDBG("Notification_Center Terminated\n");
	
	return 0;
}
