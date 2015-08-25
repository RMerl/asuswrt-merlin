/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Copyright 2014, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <bcmnvram.h>
#include <stdlib.h>
#include <rc.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sysinfo.h>
#include <wlutils.h>
#include "../networkmap/networkmap.h"
#include <fcntl.h>
#include <resolv.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>

#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#else
#include <wlioctl.h>
#endif

#define POLL_INTERVAL_SEC 30
#define POLL_SHORT_INTERVAL_SEC 10
#define MAC_STR_LEN 18
#define IP_STR_LEN 15
#define WIFI_INF_MAX 3
#define MAX_STA_COUNT   128
#define DHCP_LEASE "var/lib/misc/dnsmasq.leases"
#define PACKETSIZE  64

#define KGINFO(fmt, arg...) \
        do { \
                _dprintf("KeyGuard %lu: "fmt, uptime(), ##arg); \
        }while (0)

#define KGDBG(fmt, arg...) \
	do { \
		if(kg_dbg) \
		_dprintf("KeyGuard %lu: "fmt, uptime(), ##arg); \
	}while (0)

static struct itimerval itv;
int KG_WAN = 0;
int KG_STREAM = 0;
int KG_WIFI_RADIO = 0;
int KG_RADIO_2G = 0;
int KG_RADIO_5G = 0;
char router_orig_wan_enable[WAN_UNIT_MAX][32];
char router_orig_txchain[WIFI_INF_MAX][32];
char router_orig_rxchain[WIFI_INF_MAX][32];
char router_orig_wl_radio[WIFI_INF_MAX][32];
int kdevice_status;
int count = 0;
int kg_dbg = 0;

typedef struct sta_info {
	char mac[MAC_STR_LEN];
	char ip[IP_STR_LEN];
	int enable;
	struct sta_info *next;
}kdevice_t;

kdevice_t *KeyDevice = NULL;

struct packet
{
    struct icmphdr hdr;
    char msg[PACKETSIZE-sizeof(struct icmphdr)];
};

int pid=-1;
struct protoent *proto=NULL;
int cnt=1;

/*--------------------------------------------------------------------*/
/*--- checksum - standard 1s complement checksum                   ---*/
/*--------------------------------------------------------------------*/
unsigned short checksum(void *b, int len)
{
	unsigned short *buf = b;
	unsigned int sum=0;
	unsigned short result;

	for ( sum = 0; len > 1; len -= 2 )
		sum += *buf++;
	if ( len == 1 )
		sum += *(unsigned char*)buf;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	result = ~sum;
	
	return result;
}


/*--------------------------------------------------------------------*/
/*--- ping - Create message and send it.                           ---*/
/*--------------------------------------------------------------------*/
int ping(char *address)
{
	const int val=255;
	int i, sd;
	struct packet pckt;
	struct sockaddr_in r_addr;
	int retry;
	struct hostent *hname;
	struct sockaddr_in addr_ping,*addr;
	int len;
	char r_ip[INET_ADDRSTRLEN];

	pid = getpid();
	proto = getprotobyname("ICMP");
	hname = gethostbyname(address);
	bzero(&addr_ping, sizeof(addr_ping));
	addr_ping.sin_family = hname->h_addrtype;
	addr_ping.sin_port = 0;
	addr_ping.sin_addr.s_addr = *(long*)hname->h_addr;

	addr = &addr_ping;

	sd = socket(PF_INET, SOCK_RAW, proto->p_proto);
	if ( sd < 0 )
	{
		perror("socket");
		return 1;
	}

	if ( setsockopt(sd, SOL_IP, IP_TTL, &val, sizeof(val)) != 0)
    	{
        	perror("Set TTL option");
        	return 1;
	}
	if ( fcntl(sd, F_SETFL, O_NONBLOCK) != 0 )
    	{
		perror("Request nonblocking I/O");
		return 1;
	}

	for (retry=0; retry < 3; retry++)
	{
		len=sizeof(r_addr);

		if ( recvfrom(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)&r_addr, &len) > 0 )
		{
			inet_ntop(r_addr.sin_family, &(r_addr.sin_addr), r_ip, sizeof(r_ip));
			if(strcmp(address, r_ip) == 0) {
				close(sd);
				return 0;
			}
		}

		bzero(&pckt, sizeof(pckt));
		pckt.hdr.type = ICMP_ECHO;
		pckt.hdr.un.echo.id = pid;
		
		for ( i = 0; i < sizeof(pckt.msg)-1; i++ )
			pckt.msg[i] = i+'0';
		
		pckt.msg[i] = 0;
		pckt.hdr.un.echo.sequence = cnt++;
		pckt.hdr.checksum = checksum(&pckt, sizeof(pckt));
		
		if ( sendto(sd, &pckt, sizeof(pckt), 0, (struct sockaddr*)addr, sizeof(*addr)) <= 0 )
			perror("sendto");

		usleep(300000);
		KGDBG("Ping %s ...(%d)\n", address, retry);

	}

	close(sd);	
	return 1;
}



static void alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

int check_auth_device_wifi() {

	char ifname[32], vifname[32];
	char prefix[32];
	char tmp[128];
	char ea[ETHER_ADDR_LEN];
	char *next;
	int ret, unit;
	int vidx;
	int mcnt;
	struct maclist *mac_list;
	int mac_list_size;
	kdevice_t *ptr;
	int kdevice_found;

#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#elif defined(RTCONFIG_QTN)
#else /* BCM */
	kdevice_found = 0;
	mac_list_size = sizeof(mac_list->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
	mac_list = malloc(mac_list_size);
        if(!mac_list)
                goto exit;

	foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
		memset(mac_list, 0, mac_list_size);
		strcpy((char*)mac_list, "authe_sta_list");
		if(wl_ioctl(ifname, WLC_GET_VAR, mac_list, mac_list_size))
			continue;
		for(mcnt=0; mcnt < mac_list->count; mcnt++) {
			ether_etoa((void *)&mac_list->ea[mcnt], ea);
			ptr = KeyDevice;
			while(ptr != NULL) {
				if(strcmp(ea, ptr->mac) == 0 && ptr->enable) {
					KGDBG("detect key device [wifi authe - %s]: %s (%s)\n", ifname, ptr->mac, ptr->ip);
                                        kdevice_found = 1;
                                        break;
                                }
                                ptr = ptr->next;
                        }
			if(kdevice_found) goto exit;
		}

		//sub-unit
		for(vidx = 1; vidx < 4; vidx++) {
			snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, vidx);
			if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
			{
				sprintf(vifname, "wl%d.%d", unit, vidx);
				memset(mac_list, 0, mac_list_size);
				strcpy((char*) mac_list, "authe_sta_list");
				if (wl_ioctl(vifname, WLC_GET_VAR, mac_list, mac_list_size))
					continue;
				for(mcnt=0; mcnt < mac_list->count; mcnt++) {
                                        ether_etoa((void *)&mac_list->ea[mcnt], ea);
					ptr = KeyDevice;
                                	while(ptr != NULL) {
                                        	if(strcmp(ea, ptr->mac) == 0 && ptr->enable) {
                                                	KGDBG("detect key device [wifi authe - %s]: %s (%s)\n", ifname, ptr->mac, ptr->ip);
                                                	kdevice_found = 1;
                                                	break;
                                        	}
                                        	ptr = ptr->next;
                                	}
                                	if(kdevice_found) goto exit;
				}
			}

		}
        }

exit:
	if(mac_list) free(mac_list);
#endif	
	return kdevice_found;
}

int detect_by_echo_reply() {

	int kdevice_found = 0;
	kdevice_t *ptr;
	char cmd[128];

	ptr = KeyDevice;
	while(ptr != NULL) {

		if(strcmp(ptr->ip, "0.0.0.0") == 0) {
			kg_update_ip_from_dhcp_lease(ptr);
			if(strcmp(ptr->ip, "0.0.0.0") == 0) //static ip? lookup network map.
				kg_update_ip_from_nmap();
		}

		if(strcmp(ptr->ip, "0.0.0.0") != 0 && ptr->enable) {
			if(!ping(ptr->ip)) {
				kdevice_found = 1;
				KGDBG("detect key device [echo reply]: %s (%s)\n", ptr->mac, ptr->ip);
                		break;
        		}
		}
		
		ptr = ptr->next;
	}

	return kdevice_found;
}


static void kg_watchdog(int sig) {
	
	int i, shm_client_info_id;
        void *shared_client_info=(void *) 0;
        char mac_buf[32];
        P_CLIENT_DETAIL_INFO_TABLE p_client_info_tab;
        int lock;
	int timeout;
	int kdevice;
	kdevice_t *ptr;

	if ( !nvram_get_int("wlready") ||
	     nvram_get_int("sw_mode") != SW_MODE_ROUTER ||
	     (!KG_WAN && !KG_STREAM && !KG_WIFI_RADIO) ||
	     KeyDevice == NULL)
		return;

	/* detect key device state:
		1. wireless authe list
		2. icmp echo
		3. arp
	*/

	// Check wireless sta first
	kdevice = check_auth_device_wifi();

	// Check by ping
	if(!kdevice) {
		alarmtimer(0, 0);
		kdevice = detect_by_echo_reply();
		alarmtimer(POLL_INTERVAL_SEC, 0);
	}

	if(!kdevice && kdevice_status) {	// All key devices were left? send arp to confirm.
		
		if(count == 0) {
			timeout = 20;
			alarmtimer(0, 0);
			KGDBG("Send arp...\n");
			killall("networkmap", SIGUSR1);
			while(1)
			{
				sleep(1);
				timeout--;
				if(!nvram_get_int("networkmap_fullscan") || !timeout) {
					break;
				}
			};
			sleep(10);
			kg_update_ip_from_nmap();
			alarmtimer(POLL_SHORT_INTERVAL_SEC, 0);
		}

		lock = file_lock("networkmap");
		shm_client_info_id = shmget((key_t)1001, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
		if (shm_client_info_id == -1){
			fprintf(stderr,"shmget failed\n");
			file_unlock(lock);
			return;
		}

		shared_client_info = shmat(shm_client_info_id,(void *) 0,0);
		if (shared_client_info == (void *)-1){
			fprintf(stderr,"shmat failed\n");
			file_unlock(lock);
			return;
		}

		kdevice = 0;
		p_client_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shared_client_info;
		for(i=0; i<p_client_info_tab->ip_mac_num; i++) {
			memset(mac_buf, 0, 32);

			if(p_client_info_tab->exist[i]==1) {
				sprintf(mac_buf, "%02X:%02X:%02X:%02X:%02X:%02X",
				p_client_info_tab->mac_addr[i][0],p_client_info_tab->mac_addr[i][1],
				p_client_info_tab->mac_addr[i][2],p_client_info_tab->mac_addr[i][3],
				p_client_info_tab->mac_addr[i][4],p_client_info_tab->mac_addr[i][5]
				);

				ptr = KeyDevice;
				while(ptr != NULL) {
					if(strcmp(mac_buf, ptr->mac) == 0 && ptr->enable) {
						KGDBG("detect key device [arp]: %s (%s)\n", ptr->mac, ptr->ip);
						kdevice = 1;
						break;
					}
					ptr = ptr->next;
				}
			}
			if(kdevice)	break;
        	}


		ptr = NULL;
        	shmdt(shared_client_info);
        	file_unlock(lock);
	}


	if(kdevice_status != kdevice) {
		if(kdevice)
			alarmtimer(POLL_SHORT_INTERVAL_SEC, 0);
		if(count > 0) {
			count = 0;
			kg_trigger(kdevice);
		}
		else
			count++;
	}

        return 0;
}

void kg_exit(int signo) {

	kdevice_t *next;	
	alarmtimer(0, 0);
	signal(SIGTERM, SIG_IGN);
	kg_restore_original();
	KGINFO("Exit...\n");
	while(KeyDevice) {
		next = KeyDevice->next;
		free(KeyDevice);
		KeyDevice = next;
	}
	remove("/var/run/keyguard.pid");
	exit(0);
}


/*--------------------------------------------------------------------*/
/*--- Condition changed, update rule and key device                ---*/
/*--------------------------------------------------------------------*/
void kg_update_condition(int signo) {

	KGDBG("updade rule and key devices list\n");
	alarmtimer(0,0);
	kg_init_setting();
	alarmtimer(POLL_INTERVAL_SEC, 0);
}

/*--------------------------------------------------------------------*/
/*--- Retrieve key device ip from dhcp lease                       ---*/
/*--------------------------------------------------------------------*/
void kg_update_ip_from_dhcp_lease(kdevice_t *ptr) {
	
	FILE *fp;
	char buf[256];
	char word[256], *next_word;
	char mac[MAC_STR_LEN];
        char ip[IP_STR_LEN];
	int item;

	fp = fopen(DHCP_LEASE, "r");
	if(fp==NULL)
		return;
	while(fgets(buf, sizeof(buf), fp) != NULL) {
		item = 0;
		foreach(word, buf, next_word) {
			if(item == 1)	// mac
				strncpy(mac, word, MAC_STR_LEN);
			if(item == 2) {	// ip
				strncpy(ip, word, IP_STR_LEN);
				break;
			}
			item++;
		}
		if(!strcasecmp(ptr->mac, mac)) {
			strcpy(ptr->ip, ip);
			break;
		}
	}

	fclose(fp);
}

/*--------------------------------------------------------------------*/
/*--- Retrieve key device ip by arp                                ---*/
/*--------------------------------------------------------------------*/
void kg_update_ip_from_nmap() {
	
	int i, shm_client_info_id;
	void *shared_client_info=(void *) 0;
	P_CLIENT_DETAIL_INFO_TABLE p_client_info_tab;
	int lock;
	char mac[MAC_STR_LEN];
	char ip[IP_STR_LEN];
	kdevice_t *ptr;

	lock = file_lock("networkmap");
	shm_client_info_id = shmget((key_t)1001, sizeof(CLIENT_DETAIL_INFO_TABLE), 0666|IPC_CREAT);
	if (shm_client_info_id == -1){
		fprintf(stderr,"shmget failed\n");
		file_unlock(lock);
		return 0;
	}

	shared_client_info = shmat(shm_client_info_id,(void *) 0,0);
	if (shared_client_info == (void *)-1){
		fprintf(stderr,"shmat failed\n");
		file_unlock(lock);
		return 0;
	}

	p_client_info_tab = (P_CLIENT_DETAIL_INFO_TABLE)shared_client_info;
	for(i=0; i<p_client_info_tab->ip_mac_num; i++) {
		sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                                p_client_info_tab->mac_addr[i][0],p_client_info_tab->mac_addr[i][1],
                                p_client_info_tab->mac_addr[i][2],p_client_info_tab->mac_addr[i][3],
                                p_client_info_tab->mac_addr[i][4],p_client_info_tab->mac_addr[i][5]
                                );
		sprintf(ip, "%d.%d.%d.%d",
                                p_client_info_tab->ip_addr[i][0],p_client_info_tab->ip_addr[i][1],
                		p_client_info_tab->ip_addr[i][2],p_client_info_tab->ip_addr[i][3]);

		ptr = KeyDevice;
		while(ptr != NULL) {
			if(strcmp(mac, ptr->mac) == 0) {
				if(strcmp(ip, ptr->ip) != 0)
					strncpy(ptr->ip, ip, IP_STR_LEN);
				break;
			}
			ptr = ptr->next;
		}
        }
        shmdt(shared_client_info);
        file_unlock(lock);
}

/*--------------------------------------------------------------------*/
/*--- init keyguard                                                ---*/
/*--------------------------------------------------------------------*/
void kg_init_setting() {
	
	kdevice_t *sta, *ptr;
	char enable[512], *next_enable;
	char word[4096], *next_word;
	int time;

	// Rule
	KG_WAN = nvram_get_int("kg_wan_enable");
	KG_STREAM = nvram_get_int("kg_powersaving_enable");
	KG_WIFI_RADIO = nvram_get_int("kg_wl_radio_enable");
	if(KG_WIFI_RADIO) {
		if(nvram_get_int("kg_wl_radio")) {
			KG_RADIO_2G = 0;
			KG_RADIO_5G = 1;
		}
		else {
			KG_RADIO_2G = 1;
			KG_RADIO_5G = 0;
		}
	}
	else {
		KG_RADIO_2G = KG_RADIO_5G = 0;
	}

	// Key Devices
	while(KeyDevice != NULL) {
		ptr = KeyDevice->next;
		free(KeyDevice);
		KeyDevice = ptr;
	}

	foreach_62(word, nvram_safe_get("kg_mac"), next_word) {

		sta = malloc(sizeof(kdevice_t));
		memset(sta, 0, sizeof(kdevice_t));
		strncpy(sta->mac, word, MAC_STR_LEN);
		strcpy(sta->ip, "0.0.0.0");
		sta->enable = 0;
		sta->next = NULL;

		if(KeyDevice == NULL) {
			KeyDevice = sta;
			ptr = KeyDevice;
		}
		else {
			ptr->next = sta;
			ptr = ptr->next;
		}
	}

	ptr = KeyDevice;
	foreach_62(enable, nvram_safe_get("kg_device_enable"), next_enable) {
		if(ptr == NULL) break;
		if(strlen(enable) > 0)
			ptr->enable = atoi(enable);
		ptr = ptr->next;
	}

	time = 20;
	killall("networkmap", SIGUSR1);
	while(1)
	{
		sleep(1);
		time--;
		if(!nvram_get_int("networkmap_fullscan") || !time)
		break;
	};
	sleep(10);

	kg_update_ip_from_nmap();

	kdevice_status = 1;
	
	ptr = KeyDevice;
	while(ptr != NULL) {
		if(strcmp(ptr->ip, "0.0.0.0") == 0)
			kg_update_ip_from_dhcp_lease(ptr);	
		KGDBG("key device: %s (%s) [%d]\n", ptr->mac, ptr->ip, ptr->enable);
		ptr = ptr->next;
	};
	
}

/*--------------------------------------------------------------------*/
/*--- keyguard condition trigger                                   ---*/
/*--------------------------------------------------------------------*/
void kg_trigger(int kdevice) {

	char ifname[32];
        char tmp[32];
        char *next;
        int unit;

	KGDBG("*** %s ***\n", kdevice ? "key device is back" : "all key devices were left");

	if(!kdevice) {	// apply the setting for all key devices leave

		if(KG_WAN) {
			KGINFO("--- WAN DISABLE ---\n");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
                		stop_wan_if(unit);
		}

		if(KG_STREAM) {
			KGINFO("--- ENTER WIFI POWER SAVING MODE ---\n");
			foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#else /* BCM */

#if defined(RTCONFIG_QTN)
#else
				eval("wl", "-i", ifname, "txchain", "1");
				eval("wl", "-i", ifname, "rxchain", "1");
				eval("wl", "-i", ifname, "down");
				eval("wl", "-i", ifname, "up");
#endif
#endif     
	           	}
        	}

		if(KG_WIFI_RADIO) {
			KGINFO("--- DISABLE WIFI - %s ---\n", KG_RADIO_2G ? "2GHz" : "5GHz");
			unit = 0;
                        foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
                                sprintf(tmp, "wl%d_nband", unit);
				if((nvram_get_int(tmp) == 2 && KG_RADIO_2G) ||
				   (nvram_get_int(tmp) == 1 && KG_RADIO_5G)) {
#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#else /* BCM */

#if defined(RTCONFIG_QTN)
#else
					eval("wl", "-i", ifname, "radio", "off");
#endif
#endif
				}
				else {
#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)     
#else /* BCM */

#if defined(RTCONFIG_QTN)
#else
					eval("wl", "-i", ifname, "radio", "on");
#endif
#endif
				}
                                unit++;
                        }
		}
	}
	else {	// key device is back. Roll back to normal setting
		if(KG_WAN) {
			KGINFO("--- WAN ENABLE ---\n");
			for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit)
                		start_wan_if(unit);
		}

		if(KG_STREAM) {
			KGINFO("--- LEAVE WIFI POWER SAVING MODE ---\n");
			unit = 0;
			foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#else /* BCM */

#if defined(RTCONG_QTN)
#else
				eval("wl", "-i", ifname, "txchain", router_orig_txchain[unit]);
				eval("wl", "-i", ifname, "rxchain", router_orig_rxchain[unit]);
				eval("wl", "-i", ifname, "down");
				eval("wl", "-i", ifname, "up");
#endif
#endif
				unit++;
			}
		}

		if(KG_WIFI_RADIO) {
			KGINFO("--- ENABLE WIFI - %s ---\n", KG_RADIO_2G ? "2GHz" : "5GHz");
			unit = 0;
			foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
				sprintf(tmp, "wl%d_nband", unit);
				if((nvram_get_int(tmp) == 2 && KG_RADIO_2G) ||
                                   (nvram_get_int(tmp) == 1 && KG_RADIO_5G)) {
#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#else /* BCM */

#if defined(RTCONG_QTN)
#else
					eval("wl", "-i", ifname, "radio", "on");
#endif
#endif
				}
				unit++;
			}
		}
	}


	kdevice_status = kdevice;

}

/*--------------------------------------------------------------------*/
/*--- keep original configuration while start keyguard             ---*/
/*--------------------------------------------------------------------*/
void kg_keep_original() {

	char ifname[32];
        char prefix[32];
        char tmp[32];
	char *next;
	int unit;

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
		snprintf(prefix, sizeof(prefix), "wan%d_", unit);
		strcpy(router_orig_wan_enable[unit], nvram_safe_get(strcat_r(prefix, "enable", tmp)));
	}

	unit = 0;
	foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "%d:", unit);
		strcpy(router_orig_txchain[unit], nvram_safe_get(strcat_r(prefix, "txchain", tmp)));
		strcpy(router_orig_rxchain[unit], nvram_safe_get(strcat_r(prefix, "rxchain", tmp)));
		unit++;
	}

	unit = 0;
	foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		strcpy(router_orig_wl_radio[unit], nvram_safe_get(strcat_r(prefix, "radio", tmp)));
		unit++;
	}
}


/*--------------------------------------------------------------------*/
/*--- stop keyguard and roll back to original configuration        ---*/
/*--------------------------------------------------------------------*/
void kg_restore_original() {
	char ifname[32];
	char *next;
	int unit;

	for(unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
		
		if(atoi(router_orig_wan_enable[unit]))
			start_wan_if(unit);
	}

	foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#else

#if defined(RTCONFIG_QTN)
#else
		eval("wlconf", ifname, "down");
		eval("wlconf", ifname, "up");
		eval("wlconf", ifname, "start");
#endif
#endif
	}
}

int keyguard_main(int argc, char *argv[])
{
	sigset_t sigs_to_catch;
        sigemptyset(&sigs_to_catch);
        sigaddset(&sigs_to_catch, SIGALRM);
        sigaddset(&sigs_to_catch, SIGTERM);
        sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

        signal(SIGALRM, kg_watchdog);
        signal(SIGTERM, kg_exit);
	signal(SIGUSR1, kg_update_condition);

	kg_dbg = nvram_get_int("kg_debug");
	KGINFO("KeyGuard Start...\n");

	FILE *fp = fopen("/var/run/keyguard.pid", "w");
        if(fp != NULL){
                fprintf(fp, "%d", getpid());
                fclose(fp);
        }

	kg_keep_original();
	kg_init_setting();

	alarmtimer(POLL_INTERVAL_SEC, 0);

	while(1) {
		pause();
	};

	return 0;
}

