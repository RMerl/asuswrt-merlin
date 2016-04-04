#include "base.h"
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/if_ether.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>
#include "array.h"
#include "buffer.h"
#include <string.h>
#include <stdio.h>
//#include "log.h"

#include <stdlib.h>
#include <limits.h>

#include <errno.h>
#include <assert.h>

#define HAVE_LIBSMBCLIENT_H

#include "sys-socket.h"
#include <libsmbclient.h>
#include <dlinklist.h>
//#include <time.h>

#if EMBEDDED_EANBLE
#include "nvram_control.h"
#ifndef APP_IPKG
#include <utils.h>
#include <shared.h>
#endif
#endif

//#include <sys/shm.h>
//void* shared_memory = (void*)0;

#define _MAIN_FUNC
//#define _USEMYTIMER
//#define _USEMYTIMER2
#define _USEARP
#define MAC_BCAST_ADDR  "\xff\xff\xff\xff\xff\xff"
#define TT_SIGUSR2 (SIGUSR2)
#define ARP_TIME_INTERVAL 5
//#define ARP_TIME_INTERVAL 3600
//#define ARP_TIME_INTERVAL 120

#define RE_SCAN_TIME_INTERVAL 120

#define LIGHTTPD_ARPPING_PID_FILE_PATH	"/tmp/lighttpd/lighttpd-arpping.pid"

#define THREAD_BEGIN_INDEX 1

#define DBE 0

#define NUM_THREADS 15
#define MAX_THREADS 253
int g_threadIndex = THREAD_BEGIN_INDEX;
int g_current_time = 0;
int g_rescancount=0;
int g_useSystemTimer=0;
int g_bInitialize=0;
time_t g_begin_time=0;
timer_t g_arp_timer;
char g_scan_interface[10]="eth0";
char* g_temp_file = "/tmp/arpping_list";
int g_kill_list = 0;


int is_shutdown = 0;

struct arpMsg {
    struct ethhdr ethhdr;                   /* Ethernet header */
    u_short htype;                          /* hardware type (must be ARPHRD_ETHER) */
    u_short ptype;                          /* protocol type (must be ETH_P_IP) */
    u_char  hlen;                           /* hardware address length (must be 6) */
    u_char  plen;                           /* protocol address length (must be 4) */
    u_short operation;                      /* ARP opcode */
    u_char  sHaddr[6];                      /* sender's hardware address */
    u_char  sInaddr[4];                     /* sender's IP address */
    u_char  tHaddr[6];                      /* target's hardware address */
    u_char  tInaddr[4];                     /* target's IP address */
    u_char  pad[18];                        /* pad for min. Ethernet payload (60 bytes) */
};

struct ifinfo {
    char ifname[IFNAMSIZ];
    u_long addr;                /* network byte order */
    u_long mask;                /* network byte order */
    u_long bcast;               /* network byte order */
    u_char haddr[6];
    short flags;
};

typedef struct _SrvInfo{
	int id;
	pthread_t t;
	char iface[10];
	struct in_addr ipSrv;
	u_int8_t macSrv[6];	
	in_addr_t arp_ip;
	buffer* arp_cip;
	int found;
}SrvInfo;
SrvInfo a_srvInfo[MAX_THREADS];

int init_a_srvInfo(void);

void save_arpping_list(void)
{

#if EMBEDDED_EANBLE

	smb_srv_info_t* c;
	
	buffer* smbdav_list = buffer_init();
	buffer_copy_string(smbdav_list, "");

	for (c = smb_srv_info_list; c; c = c->next) {
		if(c->name->used == 0 || strcmp(c->name->ptr,"")==0)
			continue;

		char temp[50]="\0";
		sprintf(temp, "%s<%s<%s<%d>", c->name->ptr, c->ip->ptr, c->mac->ptr, c->online);
		buffer_append_string(smbdav_list, temp);
	}
	
	//Cdbg(DBE, "nvram_set_smbdav_str %s", smbdav_list->ptr);
	nvram_set_smbdav_str(smbdav_list->ptr);

	buffer_free(smbdav_list);
#else
	unlink(g_temp_file);

	smb_srv_info_t* c;
	
	char mybuffer[100];
	FILE* fp = fopen(g_temp_file, "w");

	//Cdbg(DBE, "save_arpping_list %s", g_temp_file);
	
	if(fp!=NULL){
		smb_srv_info_t* c;
	
		for (c = smb_srv_info_list; c; c = c->next) {
			if(c->name->used == 0 || strcmp(c->name->ptr,"")==0)
				continue;
			
			fprintf(fp, "%s<%s<%s<%d\n", c->name->ptr, c->ip->ptr, c->mac->ptr, c->online);
		}
		
		fclose(fp);
	}
#endif
}

void read_arpping_list(void)
{
#if EMBEDDED_EANBLE
	char* aa = nvram_get_smbdav_str();

	if(aa==NULL){
		return;
	}
	
	char* str_smbdav_list = (char*)malloc(strlen(aa)+1);
	strcpy(str_smbdav_list, aa);
	//Cdbg(DBE, "read_arpping_list................str_smbdav_list=%s", str_smbdav_list);
#ifdef APP_IPKG
	free(aa);
#endif
	if(str_smbdav_list!=NULL){
		char * pch;
		pch = strtok(str_smbdav_list, "<>");
		
		while(pch!=NULL){
			
			smb_srv_info_t *smb_srv_info;
			smb_srv_info = (smb_srv_info_t *)calloc(1, sizeof(smb_srv_info_t));
			
			//- PC Name
			//Cdbg(DBE, "name=%s", pch);
			smb_srv_info->name = buffer_init();
			buffer_copy_string_len(smb_srv_info->name, pch, strlen(pch));
						
			//- IP Address
			pch = strtok(NULL,"<>");
			if(pch==NULL){
				fprintf(stderr, "pch=NULL\n");
				nvram_set_smbdav_str("");
				return;
			}
			//Cdbg(DBE, "ip=%s", pch);
			smb_srv_info->ip = buffer_init();
			buffer_copy_string_len(smb_srv_info->ip, pch, strlen(pch));
			
			//- MAC Address
			pch = strtok(NULL,"<>");
			if(pch){
				//Cdbg(DBE, "mac=%s", pch);
				smb_srv_info->mac = buffer_init();
				buffer_copy_string_len(smb_srv_info->mac, pch, strlen(pch));
			}
			
			//- PC Online?
			pch = strtok(NULL,"<>");
			if(pch){
				//Cdbg(DBE, "Online=%s", pch);
				smb_srv_info->online= atoi(pch);
			}
			
			DLIST_ADD(smb_srv_info_list, smb_srv_info);

			//- Next
			pch = strtok(NULL,"<>");
		}

		free(str_smbdav_list);
	}

#else
	size_t j;
	int length, filesize;
	FILE* fp = fopen(g_temp_file, "r");
	if(fp!=NULL){		
		
		char str[1024];

		while(fgets(str,sizeof(str),fp) != NULL)
		{
			char * pch;
			int name_len, ip_len, mac_len, online;
			
      			// strip trailing '\n' if it exists
      			int len = strlen(str)-1;
      			if(str[len] == '\n') 
         			str[len] = 0;

			smb_srv_info_t *smb_srv_info;
			smb_srv_info = (smb_srv_info_t *)calloc(1, sizeof(smb_srv_info_t));

			//smb_srv_info->id = thread_id;

			//- PC Name
			smb_srv_info->name = buffer_init();
			pch = strtok(str,"<");
			buffer_copy_string_len(smb_srv_info->name, pch, strlen(pch));
			//Cdbg(DBE, "from arpping-list %s", smb_srv_info->name->ptr);
			
			//- IP Address
			smb_srv_info->ip = buffer_init();
			pch = strtok(NULL,"<");
			buffer_copy_string_len(smb_srv_info->ip, pch, strlen(pch));

			//- MAC Address
			smb_srv_info->mac = buffer_init();
			pch = strtok(NULL,"<");
			buffer_copy_string_len(smb_srv_info->mac, pch, strlen(pch));

			//- PC Online?
			pch = strtok(NULL,"<");
			smb_srv_info->online = atoi(pch);
						
			DLIST_ADD(smb_srv_info_list, smb_srv_info);
		}
			
		fclose(fp);
	}
#endif
}

int openRawSocket (int *s, u_short type) {
	int optval = 1;

	if((*s = socket (AF_INET, SOCK_PACKET, htons (type))) == -1) {
		//Cdbg(DBE, "Could not open raw socket\n");
		return -1;
	}

	if(setsockopt (*s, SOL_SOCKET, SO_BROADCAST, &optval, sizeof (optval)) == -1) {
		//Cdbg(DBE, "Could not setsocketopt on raw socket\n");
		return -1;
	}
	return 0;
}

void mkArpMsg(int opcode, u_long tInaddr, u_char *tHaddr,
		 u_long sInaddr, u_char *sHaddr, struct arpMsg *msg) {
	bzero(msg, sizeof(*msg));
	bcopy(MAC_BCAST_ADDR, msg->ethhdr.h_dest, 6); /* MAC DA */
	bcopy(sHaddr, msg->ethhdr.h_source, 6); /* MAC SA */
	msg->ethhdr.h_proto = htons(ETH_P_ARP); /* protocol type (Ethernet) */
	msg->htype = htons(ARPHRD_ETHER);	       /* hardware type */
	msg->ptype = htons(ETH_P_IP);		   /* protocol type (ARP message) */
	msg->hlen = 6;						  /* hardware address length */
	msg->plen = 4;						  /* protocol address length */
	msg->operation = htons(opcode);		 /* ARP op code */
	*((u_int *)msg->sInaddr) = sInaddr;	     /* source IP address */
	bcopy(sHaddr, msg->sHaddr, 6);		  /* source hardware address */
	*((u_int *)msg->tInaddr) = tInaddr;	     /* target IP address */
	if ( opcode == ARPOP_REPLY )
		bcopy(tHaddr, msg->tHaddr, 6);	  /* target hardware address */
}

int arpCheck(int thread_id,u_long inaddr, struct ifinfo *ifbuf, long timeout, buffer* arp_ip)  {
	int			     s;		      /* socket */
	int			     rv;		     /* return value */
	struct sockaddr addr;	   /* for interface name */
	struct arpMsg   arp;
	fd_set		  fdset;
	struct timeval  tm;
	time_t		  prevTime;
	char checkip[100];
	char checkmac[100];

	strcpy(checkip, arp_ip->ptr);

	rv = 1;
	if( openRawSocket(&s, ETH_P_ARP) == -1 ){
		return -1;
	}
		
	/* send arp request */
	mkArpMsg(ARPOP_REQUEST, inaddr, NULL, ifbuf->addr, ifbuf->haddr, &arp);
	bzero(&addr, sizeof(addr));
	strcpy(addr.sa_data, ifbuf->ifname);
	if ( sendto(s, &arp, sizeof(arp), 0, &addr, sizeof(addr)) < 0 ) rv = 0;
	

	/* wait arp reply, and check it */
	tm.tv_usec = 0;
	time(&prevTime);
	while ( timeout > 0 ) {
		FD_ZERO(&fdset);
		FD_SET(s, &fdset);
		tm.tv_sec  = timeout;
		if ( select(s+1, &fdset, (fd_set *)NULL, (fd_set *)NULL, &tm) < 0 ) {
			//Cdbg(DBE, "Error on ARPING request: %s\n", strerror(errno));
			if (errno != EINTR) rv = 0;
		} else if ( FD_ISSET(s, &fdset) ) {
			if (recv(s, &arp, sizeof(arp), 0) < 0) rv = 0;
			if( arp.operation == htons(ARPOP_REPLY) && 
			     bcmp(arp.tHaddr, ifbuf->haddr, 6) == 0 && 
			     *((u_int *)arp.sInaddr) == inaddr ) {
				
				//inet_ntop(AF_INET, arp.sInaddr, checkip, sizeof(checkip)) ;

				//fprintf(stderr,"enter %d\n", thread_id);
				//char* hostname = smbc_nmblookup(checkip);				
				//fprintf(stderr,"leave %d, %s\n", thread_id, hostname);
				//hostname = "TSET";

				#if 1
				//if(hostname!=NULL)
				{
					
					sprintf(checkmac, "%02X:%02X:%02X:%02X:%02X:%02X", 
						    arp.sHaddr[0]&0xff, arp.sHaddr[1]&0xff, arp.sHaddr[2]&0xff,
						    arp.sHaddr[3]&0xff, arp.sHaddr[4]&0xff, arp.sHaddr[5]&0xff);

					#if 0
					fprintf(stderr, "Valid arp reply receved for this address, thread_id=[%d]\n", thread_id);
					fprintf(stderr, "IP is %s, MAC is %s\n\n", 
						    checkip,
						    checkmac);
					#endif

					int bFound = 0;
					smb_srv_info_t *p;

					//- Check this pc is existed!
					for (p = smb_srv_info_list; p; p = p->next) {						
						if( strcmp(p->ip->ptr, checkip)==0 &&
						    strcmp(p->mac->ptr, checkmac)==0 ){
							//fprintf(stderr,"\t\tFound ip=[%s] in list!\n", checkip);
							bFound = 1;
							
							if(p->online==0){
								p->online = 1;
								//Cdbg(DBE ,"set ip=[%s] online!\n", checkip);
								save_arpping_list();
							}
							break;
						}
					}

					//- If same mac but not same ip, remove it!
					for (p = smb_srv_info_list; p; p = p->next) {						
						if( strcmp(p->ip->ptr, checkip)!=0 &&
						    strcmp(p->mac->ptr, checkmac)==0 ){
							buffer_free(p->ip);
							buffer_free(p->mac);
							buffer_free(p->name);
							DLIST_REMOVE(smb_srv_info_list, p);
							free(p);
							//fprintf(stderr,"\t\tRemove ip=[%s] in list!\n", checkip);
							break;
						}
					}
					
					if(bFound==0){
						
						smb_srv_info_t *smb_srv_info;
						smb_srv_info = (smb_srv_info_t *)calloc(1, sizeof(smb_srv_info_t));

						smb_srv_info->id = thread_id;
						
						smb_srv_info->ip = buffer_init();
						buffer_copy_string(smb_srv_info->ip, checkip);
						
						smb_srv_info->mac = buffer_init();
						buffer_copy_string(smb_srv_info->mac, checkmac);

						//- Query host name later.
						smb_srv_info->name = buffer_init();
						buffer_copy_string(smb_srv_info->name, "");
						
						smb_srv_info->online = 1;
						
						DLIST_ADD(smb_srv_info_list, smb_srv_info);

						SrvInfo *pSrvInfo = &a_srvInfo[thread_id];
						pSrvInfo->found = 1;
						//Cdbg(DBE ,"add ip=[%s], thread_id=[%d], pSrvInfo->found=[%d] to list!\n", checkip, thread_id, pSrvInfo->found);

						save_arpping_list();
					}
				}

				rv = 0;
				#else
				rv = 1;
				#endif
				
				break;
			}
		}
		timeout -= time(NULL) - prevTime;
		time(&prevTime);
	}


	close(s);
	
	if(rv==1){
		smb_srv_info_t *p;
		for (p = smb_srv_info_list; p; p = p->next) {			
			if(strcmp(p->ip->ptr, checkip)==0){
				p->online = 0;
				SrvInfo *pSrvInfo = &a_srvInfo[thread_id];
				pSrvInfo->found = 0;
				//Cdbg(DBE ,"set ip=[%s] offline!\n", checkip);						
				/*
				buffer_free(p->ip);
				buffer_free(p->mac);
				buffer_free(p->name);
				DLIST_REMOVE(smb_srv_info_list, p);
				free(p);
				*/				
				save_arpping_list();
				break;
			}
		}
	}
	
	return rv;
}


int arpping(SrvInfo *psrvInfo) {
	struct ifinfo ifbuf;

	strcpy(ifbuf.ifname, psrvInfo->iface);
	ifbuf.addr = psrvInfo->ipSrv.s_addr;
	ifbuf.mask = 0x0;
	ifbuf.bcast = 0x0;

	memcpy(ifbuf.haddr, psrvInfo->macSrv, 6);
	ifbuf.flags = 0;
	
	return arpCheck(psrvInfo->id, psrvInfo->arp_ip, &ifbuf, 2, psrvInfo->arp_cip);
}

static void *thread_do_arp_check_function(void *srvInfo)
{	
	SrvInfo *psrvInfo = srvInfo;
	
	if(psrvInfo){
		arpping(psrvInfo);
	}

	pthread_exit(NULL);
}

static int thread_arpping(char* iface)
{
	int res;
	int lots_of_threads;

	//- Complete all samba scan
	if(g_threadIndex>=MAX_THREADS){		
		return 0;	
	}
		
	for( lots_of_threads=g_threadIndex; 
	     lots_of_threads<g_threadIndex+NUM_THREADS; 
	     lots_of_threads++){

		if(is_shutdown)
			return 0;
		
		if(lots_of_threads>=MAX_THREADS){
			g_threadIndex = MAX_THREADS;
			return 0;	
		}

		SrvInfo *pSrvInfo = &a_srvInfo[lots_of_threads];

		//Cdbg(DBE, "Create arpping thread: %d", lots_of_threads);
		
		pSrvInfo->id = lots_of_threads;
					
		pthread_attr_t thread_attr;
		res = pthread_attr_init(&thread_attr);
		
		if(res!=0){
			//Cdbg(DBE, "Attribute creation failed!\n");
			continue;
		}

		res = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
		if(res!=0){
			//Cdbg(DBE, "Setting detached attribute failed!\n");
			continue;
		}

		res = pthread_create( &pSrvInfo->t, 
						    &thread_attr,
						    thread_do_arp_check_function, 
						    (void*)pSrvInfo );
		
		if(res!=0){
			//Cdbg(DBE, "Thread create fail! %s, lots_of_threads= %d, errno=[%d]\n", pSrvInfo->arp_cip, lots_of_threads, res);
			continue;
		}
		
	}

	g_threadIndex=lots_of_threads;

	if(g_threadIndex>=MAX_THREADS)
		return 0;	

	return 1;
}

void query_one_hostname(void)
{
	smb_srv_info_t *p;
	int bchange = 0;
	
	for (p = smb_srv_info_list; p; p = p->next) {
		if(is_shutdown || g_kill_list)
			break;

		//Cdbg(DBE, "Query samba server name, ip=[%s]", p->ip->ptr);
		const char *hostname = smbc_nmblookup(p->ip->ptr);
		
		if( strcmp(p->name->ptr, "")!=0&&
		    hostname!=NULL && 
		    strcmp(p->name->ptr, hostname)==0 &&
		    p->online ==1 ){	

			if(hostname){
				free((char*) hostname);
				hostname=NULL;
			}
			
			continue;
		}
		
		if(hostname==NULL){
			if(p->online==1){
				p->online = 0;
				bchange = 1;
				//Cdbg(DBE, "Can't query samba server name, set ip=[%s] offline", p->ip->ptr);
			}
		}
		else{			
			p->online = 1;
			buffer_reset(p->name);
			buffer_copy_string(p->name, hostname);
			bchange = 1;
			//Cdbg(DBE, "query samba server name[%s], set ip=[%s] online", hostname, p->ip->ptr );			
		}	

		if(bchange==1)
			save_arpping_list();

		if(hostname){
			free((char*) hostname);
			hostname=NULL;
		}
	}

	//if(bchange==1)
	//	save_arpping_list();
	
}

void on_arpping_timer_handler(server *srv)
{
	
	if(g_bInitialize==0)
		return;
	
	time_t cur_time = srv->cur_ts;
	thread_arpping(srv->srvconf.arpping_interface->ptr);
		
	query_one_hostname();
	
	//- Rescan samba server
	if( g_current_time > RE_SCAN_TIME_INTERVAL &&g_threadIndex == MAX_THREADS){

		g_rescancount++;
		g_threadIndex = THREAD_BEGIN_INDEX;;

		char strTime[9] = {0};
		strftime(strTime, sizeof(strTime), "%T", localtime(&cur_time));	
		//Cdbg(DBE, "start to rescan samba server count=[%d], time_offset=[%d], time=[%s]\n", g_rescancount, g_current_time, strTime);
		
		g_current_time = 0;
		g_begin_time = 0;
		init_a_srvInfo();
	}

	if(g_useSystemTimer==1){
		if(g_begin_time==0)
			g_begin_time = cur_time;
		else
			g_current_time = cur_time - g_begin_time;
	}
#ifdef _USEMYTIMER
	else
		g_current_time += ARP_TIME_INTERVAL;
#endif
	
}

//#define MAX_MAC_NUM 16
//static int mac_num;
//static char mac_clone[MAX_MAC_NUM][18];

void dumparptable(void)
{
	char buf[256];
	char ip_entry[32], hw_type[8], flags[8], hw_address[32], mask[32], device[8];
	//char macbuf[32];
	
	FILE *fp = fopen("/proc/net/arp", "r");	
	if (!fp) {
		fprintf(stderr, "no proc fs mounted!\n");
		return;
	}
	
	//mac_num = 0;
		
	//while (fgets(buf, 256, fp) && (mac_num < MAX_MAC_NUM - 2)) {
	while (fgets(buf, 256, fp) ) {
		sscanf(buf, "%s %s %s %s %s %s", ip_entry, hw_type, flags, hw_address, mask, device);
		
	    if (!strcmp(device, g_scan_interface) && strlen(hw_address)!=0)
	    {
	    	//Cdbg(DBE, "%s, %s", ip_entry, hw_address);
	        
			int bFound = 0;
			smb_srv_info_t *p;

			//- Check this pc is existed!
			for (p = smb_srv_info_list; p; p = p->next) {						
				if( strcmp(p->ip->ptr, ip_entry)==0/* &&
				    strcmp(p->mac->ptr, hw_address)==0*/ ){
					bFound = 1;
					break;
				}
			}

			//- If same mac but not same ip or same ip, remove it!
			for (p = smb_srv_info_list; p; p = p->next) {						
				if( ( strcmp(p->ip->ptr, ip_entry)!=0 && strcmp(p->mac->ptr, hw_address)==0 ) ||
					 strcmp(p->mac->ptr, "00:00:00:00:00:00")==0 ){

					//Cdbg(DBE, "Remove ip=[%s]", ip_entry);
					
					buffer_free(p->ip);
					buffer_free(p->mac);
					buffer_free(p->name);
					DLIST_REMOVE(smb_srv_info_list, p);
					free(p);
					
					break;
				}
			}
					
			if(bFound==0){
						
				smb_srv_info_t *smb_srv_info;
				smb_srv_info = (smb_srv_info_t *)calloc(1, sizeof(smb_srv_info_t));

				//smb_srv_info->id = thread_id;
						
				smb_srv_info->ip = buffer_init();
				buffer_copy_string(smb_srv_info->ip, ip_entry);
						
				smb_srv_info->mac = buffer_init();
				buffer_copy_string(smb_srv_info->mac, hw_address);

				//- Query host name later.
				smb_srv_info->name = buffer_init();
				buffer_copy_string(smb_srv_info->name, "");
						
				smb_srv_info->online = 0;
				
				DLIST_ADD(smb_srv_info_list, smb_srv_info);

				//Cdbg(DBE, "Add ip=[%s] to list!\n", ip_entry);

				//save_arpping_list();
			}
		}
	}
	fclose(fp);
}

void on_arpping_timer_handler2(time_t cur_time)
{

	if(g_bInitialize==0)
		return;
	
	thread_arpping(g_scan_interface);	
	
	query_one_hostname();
	
	//- Rescan samba server
	if( g_current_time > RE_SCAN_TIME_INTERVAL && g_threadIndex == MAX_THREADS ){

		g_rescancount++;
		g_threadIndex = THREAD_BEGIN_INDEX;;

		char strTime[9] = {0};
		strftime(strTime, sizeof(strTime), "%T", localtime(&cur_time));	
		//Cdbg(DBE, "start to rescan samba server count=[%d], time_offset=[%d], time=[%s]\n", g_rescancount, g_current_time, strTime);
		
		g_current_time = 0;
		g_begin_time = 0;
		init_a_srvInfo();
	}

	if(g_useSystemTimer==1){
		if(g_begin_time==0)
			g_begin_time = cur_time;
		else
			g_current_time = cur_time - g_begin_time;
	}
	else{
		g_current_time += ARP_TIME_INTERVAL;
	}
}

#ifdef _USEMYTIMER
timer_t create_timer(int signo)
{
	timer_t timerid;
    struct sigevent se;
    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_signo = signo;

    if (timer_create(CLOCK_REALTIME, &se, &timerid) == -1) {
		return -1;
   	}
    return timerid;
}

void install_sighandler(int signo, void(*handler)(int))
{
    sigset_t set;
    struct sigaction act;

    /* Setup the handler */
    act.sa_handler = handler;
    act.sa_flags = SA_RESTART;
    sigaction(signo, &act, 0);

	/* Unblock the signal */
	sigemptyset(&set);
	sigaddset(&set, signo);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
}

void set_timer(timer_t timerid, int seconds)
{
	struct itimerspec timervals;
	timervals.it_value.tv_sec = seconds;
	timervals.it_value.tv_nsec = 0;
	timervals.it_interval.tv_sec = seconds;
	timervals.it_interval.tv_nsec = 0;

	if (timer_settime(timerid, 0, &timervals, NULL) == -1) {
	    
	}
}
#endif

int init_a_srvInfo(void)
{	
	u_int8_t macSrv[6];
	struct in_addr ipSrv;
	int fd = -1;
	struct ifreq ifr;
	struct sockaddr_in *sin;
	
	if((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) >= 0) {
		ifr.ifr_addr.sa_family = AF_INET;
		strcpy(ifr.ifr_name, g_scan_interface);
			
		if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
			sin = (struct sockaddr_in *) &ifr.ifr_addr;
			ipSrv = sin->sin_addr;
		} else {
			//Cdbg(DBE, "%s SIOCGIFADDR failed!\n", g_scan_interface);
			close(fd);
			return 0;	
		}

		if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
			memcpy(macSrv, ifr.ifr_hwaddr.sa_data, 6);
		} else {
			//Cdbg(DBE, "%s SIOCGIFHWADDR failed!\n", g_scan_interface);
			close(fd);
			return 0;	
		}
	
		close(fd);
	} 
	else {
		//Cdbg(DBE, "cannot init socket.\n");
		return 0;	
	}
		

	int i;
	for(i=0; i<MAX_THREADS; i++){
		SrvInfo *pSrvInfo = &a_srvInfo[i];

		pSrvInfo->id = -1;
		pSrvInfo->t = 0;
		pSrvInfo->found = -1;
		pSrvInfo->arp_cip = buffer_init();

		//- Scan IP
		register char *p = (char *)&ipSrv;		
		#define	UC(b)	(((int)b)&0xff)
		pSrvInfo->ipSrv = ipSrv;
		memcpy(pSrvInfo->macSrv, macSrv, 6);
				
		char ipAddr[18] = "";
		sprintf(ipAddr,"%u.%u.%u.%u", UC(p[0]), UC(p[1]), UC(p[2]), i+1);
		pSrvInfo->arp_ip = inet_addr(ipAddr);
		buffer_copy_string(pSrvInfo->arp_cip, ipAddr);

		//Cdbg(DBE, "pSrvInfo->arp_cip = %s", pSrvInfo->arp_cip->ptr);
		
		strcpy( pSrvInfo->iface, g_scan_interface );		
	}

	return 1;
}

void start_scan_sambaserver(int use_sys_timer)
{
	//Cdbg(DBE, "start start_scan_sambaserver...");
	
	//- Remove all
	smb_srv_info_t *c;
	for (c = smb_srv_info_list; c; c = c->next) {
		//Cdbg(DBE, "remove , ip=[%s]\n", c->ip->ptr);
		DLIST_REMOVE(smb_srv_info_list,c);
		free(c);
		c = smb_srv_info_list;
	}
	free(smb_srv_info_list);

	read_arpping_list();
	
	if( init_a_srvInfo() == 0 )
		return;
	
	g_useSystemTimer = use_sys_timer;
	g_bInitialize = 1;
	
#ifdef _USEMYTIMER
	if(g_useSystemTimer==0){
		g_arp_timer = create_timer(TT_SIGUSR2);
    	install_sighandler(TT_SIGUSR2, on_arpping_timer_handler2);
    	set_timer(g_arp_timer, ARP_TIME_INTERVAL);
	}
#endif

#ifdef _USEMYTIMER2
	struct itimerval tout_val;
  
  	tout_val.it_interval.tv_sec = ARP_TIME_INTERVAL;
  	tout_val.it_interval.tv_usec = 0;
  	tout_val.it_value.tv_sec = ARP_TIME_INTERVAL; /* set timer for "INTERVAL (10) seconds */
  	tout_val.it_value.tv_usec = 0;
  	setitimer(ITIMER_REAL, &tout_val,0);
  	signal(SIGALRM,on_arpping_timer_handler2); /* set the Alarm signal capture */

#endif

}

#ifdef _MAIN_FUNC
static void get_auth_data_fn(const char * pServer,
                 const char * pShare,
                 char * pWorkgroup,
                 int maxLenWorkgroup,
                 char * pUsername,
                 int maxLenUsername,
                 char * pPassword,
                 int maxLenPassword)
{
	UNUSED(pServer);
	UNUSED(pShare);
	UNUSED(maxLenWorkgroup);
	UNUSED(maxLenUsername);
	UNUSED(maxLenPassword);
	
	UNUSED(pWorkgroup);
	UNUSED(pUsername);
	UNUSED(pPassword);
}

static void sigaction_handler(int sig, siginfo_t *si, void *context)
{
	static siginfo_t empty_siginfo;
	UNUSED(context);
	
	if (!si) si = &empty_siginfo;

	switch (sig) {
	case SIGTERM:{
		#if XXEMBEDDED_EANBLE
		int i, act;
		for (i = 30; i > 0; --i) {
        	if (((act = check_action()) == ACT_IDLE) || (act == ACT_REBOOT)) break;
        	fprintf(stderr, "Busy with %d. Waiting before shutdown... %d", act, i);
        	sleep(1);
    	}
		
		nvram_do_commit();
		#endif
		//Cdbg(DBE, "Shut down arpping....SIGTERM");
		is_shutdown = 1;
		break;
	}
	case SIGINT:
		break;
	case SIGALRM:
		break;
	case SIGHUP:
		break;
	case SIGCHLD:
		break;
	case SIGUSR1:
		//- Remove all
		g_kill_list = 1;
		break;
	}	
}

int main(int argc, char *argv[]) 
{
	int o;
	while(-1 != (o = getopt(argc, argv, "f:m:hvVDpt"))) {
		switch(o) {
		case 'f':
			strcpy(g_scan_interface, optarg);	
			break;
		}
	}

	//- Check if same process is running.
	FILE *fp = fopen(LIGHTTPD_ARPPING_PID_FILE_PATH, "r");
	if (fp) {
		fclose(fp);
		exit(EXIT_FAILURE);
		return 0;
	}
	
	//- Write PID file
	pid_t pid = getpid();
	fp = fopen(LIGHTTPD_ARPPING_PID_FILE_PATH, "w");
	if (!fp) {
		exit(EXIT_FAILURE);
	}
	fprintf(fp, "%d\n", pid);
	fclose(fp);

#if EMBEDDED_EANBLE
	sigset_t sigs_to_catch;

	//- set the signal handler
    sigemptyset(&sigs_to_catch);
    sigaddset(&sigs_to_catch, SIGTERM);
	sigaddset(&sigs_to_catch, SIGUSR1);
    sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

    signal(SIGTERM, sigaction_handler);  
	signal(SIGUSR1, sigaction_handler);
#else
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &act, NULL);
	sigaction(SIGUSR1, &act, NULL);

	act.sa_sigaction = sigaction_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;

	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGHUP,  &act, NULL);
	sigaction(SIGALRM, &act, NULL);
	sigaction(SIGCHLD, &act, NULL);	
	sigaction(SIGUSR1, &act, NULL);	
#endif

	//int          dbglv = -1;//9;
	int          dbglv = 0;
	smbc_init(get_auth_data_fn, dbglv);
	
	start_scan_sambaserver(0);
	
	time_t prv_ts = 0;
	while (!is_shutdown) {

		sleep(10);
		
		time_t cur_ts = time(NULL);
		
#if EMBEDDED_EANBLE
		if(prv_ts == 0 || cur_ts - prv_ts >= ARP_TIME_INTERVAL){
			dumparptable();	
			query_one_hostname();
			prv_ts = cur_ts;
		}
#else		
		if( prv_ts == 0 || cur_ts - prv_ts >= ARP_TIME_INTERVAL ){	
			on_arpping_timer_handler2(0);	
			prv_ts = cur_ts;			
		}		
#endif
		
		if (g_kill_list) {
			smb_srv_info_t *c;
			smb_srv_info_t *tmp = NULL;

			g_kill_list = 0;
			c = smb_srv_info_list;
			if (c != NULL) {
				while (1) {
					int end = (c->next == NULL) ? 1 : 0;

					if (end == 0) {
						tmp = c->next;
						//Cdbg(DBE, "next, ip=%s", tmp->ip->ptr);
					} else
						tmp = NULL;

					if (c) {
						//Cdbg(DBE, "remove , ip=[%s]", c->ip->ptr);
						DLIST_REMOVE(smb_srv_info_list, c);
						free(c);
					} else
						break;

					if (end == 1)
						break;

					c = tmp;
				}
			}
			g_threadIndex = 0;
		}
  	}

	//Cdbg(DBE, "Success to terminate lighttpd-arpping.....");
	
	exit(EXIT_SUCCESS);

	return 0;
}
#endif
