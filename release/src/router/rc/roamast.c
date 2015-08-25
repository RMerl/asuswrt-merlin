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
 * Copyright 2012, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <shared.h>
#include <rc.h>
#if defined(RTCONFIG_RALINK)
#include <bcmnvram.h>
#include <ralink.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#ifdef RTCONFIG_WIRELESSREPEATER
#include <ap_priv.h>
#endif
#elif defined(RTCONFIG_QCA)
#include <bcmnvram.h>
#include <net/ethernet.h>
#include <netinet/ether.h>
#else
#include <wlioctl.h>
#endif
#include <wlutils.h>

#define RAST_COUNT_IDLE 20
#define RAST_COUNT_RSSI 6
#define RAST_POLL_INTV_NORMAL 5
#if defined(RTCONFIG_RALINK)  /* Remove dead STA from assoclist */
#define RAST_TIMEOUT_STA 10	
#define RAST_MTK_DATARATE 128 /* 1024=1k, 8bit=1Byte */
#else
#define RAST_TIMEOUT_STA 10   
#endif
#define RAST_DFT_IDLE_RATE 	2 	/* Kbps */
#define	MAX_IF_NUM 3
#define MAX_SUBIF_NUM 4
#define MAX_STA_COUNT 128
#define ETHER_ADDR_STR_LEN 18

#define RAST_INFO(fmt, arg...) \
	do {	\
		_dprintf("RAST %lu: "fmt, uptime(), ##arg); \
	} while (0)
#define RAST_DBG(fmt, arg...) \
        do {    \
		if(rast_dbg) \
                _dprintf("RAST %lu: "fmt, uptime(), ##arg); \
        } while (0)

#if defined(RTCONFIG_RALINK)
#elif defined(RTCONFIG_QCA)
#define MACF	"%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERP_TO_MACF(ea)	((struct ether_addr *) (ea))->ether_addr_octet[0], \
				((struct ether_addr *) (ea))->ether_addr_octet[1], \
				((struct ether_addr *) (ea))->ether_addr_octet[2], \
				((struct ether_addr *) (ea))->ether_addr_octet[3], \
				((struct ether_addr *) (ea))->ether_addr_octet[4], \
				((struct ether_addr *) (ea))->ether_addr_octet[5]
#define ETHER_TO_MACF(ea)	(ea).ether_addr_octet[0], \
				(ea).ether_addr_octet[1], \
				(ea).ether_addr_octet[2], \
				(ea).ether_addr_octet[3], \
				(ea).ether_addr_octet[4], \
				(ea).ether_addr_octet[5]
#else
#define MACF	"%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERP_TO_MACF(ea)	((struct ether_addr *) (ea))->octet[0], \
				((struct ether_addr *) (ea))->octet[1], \
				((struct ether_addr *) (ea))->octet[2], \
				((struct ether_addr *) (ea))->octet[3], \
				((struct ether_addr *) (ea))->octet[4], \
				((struct ether_addr *) (ea))->octet[5]
#define ETHER_TO_MACF(ea)	(ea).octet[0], \
				(ea).octet[1], \
				(ea).octet[2], \
				(ea).octet[3], \
				(ea).octet[4], \
				(ea).octet[5]
#endif

typedef struct rast_sta_info {
	time_t timestamp;
	time_t active;
	struct ether_addr addr;
	struct rast_sta_info *prev, *next;
	float datarate;	/* Kbps */
	time_t idle_start;
	uint8 idle_state;
	int32 rssi_hit_count;
#if defined(RTCONFIG_RALINK)
	int32 rssi[3];
	float curDatarate;  // Kbps
	char mac_addr[18];
#elif defined(RTCONFIG_QCA)
	int32 last_txrx_bytes; /* bytes */
	int32 rssi;
#else
	int32 rssi;
#endif
}rast_sta_info_t;

typedef struct rast_bss_info {
	char wlif_name[32];
	char prefix[32];
	int user_low_rssi;
	int32 idle_rate;
	rast_sta_info_t *assoclist[MAX_SUBIF_NUM];
}rast_bss_info_t;

#if defined(RTCONFIG_RALINK)
#define xR_MAX  3
typedef struct _roaming_info {
       	char mac[19];
       	char rssi[xR_MAX][7];
	char curRate[33];
       	char dump;
} roam;
typedef struct _roam_sta {
       	roam sta[128];
} roam_sta;
#elif defined(RTCONFIG_QCA)
typedef struct _WLANCONFIG_LIST {
         char addr[18];
         unsigned int aid;
         unsigned int chan;
         char txrate[6];
         char rxrate[6];
         unsigned int rssi;
         unsigned int idle;
         unsigned int txseq;
         unsigned int rcseq;
         char caps[12];
         char acaps[10];
         char erp[7];
         char state_maxrate[20];
         char wps[4];
         char rsn[4];
         char wme[4];
         char mode[31];
} WLANCONFIG_LIST;
#endif

rast_bss_info_t bssinfo[MAX_IF_NUM];
uint8 init = 1;
uint8 wlif_count = 0;
uint32 ticks = 0;
static struct itimerval itv;
int rast_dbg = 0;

static void
alarmtimer(unsigned long sec, unsigned long usec)
{
        itv.it_value.tv_sec  = sec;
        itv.it_value.tv_usec = usec;
        itv.it_interval = itv.it_value;
        setitimer(ITIMER_REAL, &itv, NULL);
}

void rast_init_bssinfo(void)
{
	char ifname[128], *next;
	char usr_rssi[128];
	int idx = 0, idxList, rate;
	memset(bssinfo, 0, sizeof(rast_bss_info_t));
	
	rate = nvram_get_int("rast_idlrt");
	if(rate <= 0)	rate = RAST_DFT_IDLE_RATE;

	foreach(ifname, nvram_safe_get("wl_ifnames"), next) {
		strncpy(bssinfo[idx].wlif_name, ifname, 32);
		bssinfo[idx].user_low_rssi = 0;
		bssinfo[idx].idle_rate = rate;
		strncpy(bssinfo[idx].prefix, "", 32);
		for(idxList=0; idxList < MAX_SUBIF_NUM; idxList++) bssinfo[idx].assoclist[idxList] = NULL;

#if (defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA))
		if (idx >= MAX_NR_WL_IF) { 
			break;
		}
		else {
			snprintf(bssinfo[idx].prefix, sizeof(bssinfo[idx].prefix), "wl%d_", idx);
			bssinfo[idx].user_low_rssi = nvram_get_int(strcat_r(bssinfo[idx].prefix, "user_rssi", usr_rssi));
		}
		RAST_DBG("[%s]: WI[%s], idx[%d] \n", __FUNCTION__, bssinfo[idx].wlif_name, idx);
#else
		int ret, unit;

		ret = wl_ioctl(bssinfo[idx].wlif_name, WLC_GET_INSTANCE, &unit, sizeof(unit));
		if(ret < 0) {
			RAST_INFO("[WARNING] get instance %s error!!!\n", bssinfo[idx].wlif_name);
		}
		else {
			snprintf(bssinfo[idx].prefix, sizeof(bssinfo[idx].prefix), "wl%d_", unit);
			bssinfo[idx].user_low_rssi = nvram_get_int(strcat_r(bssinfo[idx].prefix, "user_rssi", usr_rssi));
		}
#endif
		idx++;
	}
	wlif_count = idx;

	RAST_DBG("[%s]: TotalWI[%d] \n\n", __FUNCTION__, wlif_count);
}

rast_sta_info_t *rast_add_to_assoclist(int bssidx, int vifidx, struct ether_addr *addr)
{
	rast_sta_info_t *sta, *head;

	sta = bssinfo[bssidx].assoclist[vifidx];
	while(sta) {
		/* find sta in assoclist */
#if (defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA))
		if(memcmp(&(sta->addr), addr, ETHER_ADDR_STR_LEN/3) == 0) {
#else
		if(eacmp(&(sta->addr), addr) == 0) {
#endif
		
			break;
		}
		sta = sta->next;
	}

	if(!sta) {
		sta = malloc(sizeof(rast_sta_info_t));
		if(!sta) {
			RAST_INFO("[%s]: Malloc failure!\n", __FUNCTION__);
			return NULL;
		}

		memset(sta, 0, sizeof(rast_sta_info_t));
		memcpy(&sta->addr, addr, sizeof(struct ether_addr));
		sta->timestamp = uptime();
		sta->active = uptime();
#if defined(RTCONFIG_RALINK)
		int idx;
		for( idx=0; idx<xR_MAX; idx++ ) sta->rssi[idx] = 0;
		sta->curDatarate = 0;
#elif defined(RTCONFIG_QCA)
		sta->last_txrx_bytes = 0;
		sta->rssi = 0;
#else
		sta->rssi = 0;
#endif
		sta->rssi_hit_count = 0;
		sta->datarate = 0;
		sta->idle_state = 0;

		/* insert sta in the front of assoclist */
		head = bssinfo[bssidx].assoclist[vifidx];
		if(head)
			head->prev = sta;

		sta->next = head;
		sta->prev = (struct rast_sta_info *)&(bssinfo[bssidx].assoclist[vifidx]);
		bssinfo[bssidx].assoclist[vifidx] = sta;

#if defined(RTCONFIG_RALINK)
		RAST_DBG("[%s]: Add [%s] to StaList\n", __FUNCTION__,  ether_ntoa(addr));
#else
		RAST_DBG("add sta ["MACF"] to assoclist\n", ETHERP_TO_MACF(addr));
#endif
	}

	return sta;
}

rast_sta_info_t *rast_remove_from_assoclist(int bssidx, int vifidx, struct ether_addr *addr)
{
	bool found = FALSE;
	rast_sta_info_t *assoclist = NULL, *prev, *head, *next_sta = NULL;

	assoclist = bssinfo[bssidx].assoclist[vifidx];
	if(assoclist == NULL) {
		return next_sta;
	}

	/* found at 1st element, update pointer */
#if (defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA))
      	if(memcmp(&(assoclist->addr), addr, ETHER_ADDR_STR_LEN/3) == 0) {   
#else
	if(eacmp(&(assoclist->addr), addr) == 0) {   
#endif
		head = assoclist->next;
		bssinfo[bssidx].assoclist[vifidx] = head;
		if(head) {
			head->prev = (struct rast_sta_info *)&(bssinfo[bssidx].assoclist[vifidx]);
			next_sta = head;
		}
		found = TRUE;
	}
	else {
		prev = assoclist;
		assoclist = prev->next;

		while(assoclist) {
#if (defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA))
		       	if(memcmp(&(assoclist->addr), addr, ETHER_ADDR_STR_LEN/3) == 0) {    
#else
			if(eacmp(&(assoclist->addr), addr) == 0) {   
#endif
				head = assoclist->next;
				prev->next = head;
				if(head) {
					head->prev = prev;
					next_sta = head;
				}
				
				found = TRUE;
				break;
			}

			prev = assoclist;
			assoclist = prev->next;
		}
	}

	if(found) {
		free(assoclist);
	}

	return next_sta;
}

void rast_deauth_sta(int bssidx, int vifidx, rast_sta_info_t *sta)
{
#if (defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA))
	char wlif_name[32];
	char cmd[128];

	if (vifidx > 0) {
		sprintf(cmd, "wl%d.%d_ifname", bssidx, vifidx);
		strcpy(wlif_name, nvram_safe_get(cmd));
	}
	else
		strcpy(wlif_name, bssinfo[bssidx].wlif_name);

#if defined(RTCONFIG_RALINK)
	RAST_INFO("[%s]: [DISCONNECT] WI[%s], Sta[%s] \n", __FUNCTION__, wlif_name,  sta->mac_addr);
	sprintf(cmd,"iwpriv %s set DisConnectSta=%s", wlif_name, sta->mac_addr);
#elif defined(RTCONFIG_QCA)
	RAST_INFO("DEAUTHENTICATE ["MACF"] from %s\n", ETHER_TO_MACF(sta->addr), wlif_name);
	sprintf(cmd, "iwpriv %s kickmac "MACF, wlif_name, ETHER_TO_MACF(sta->addr));
#endif
	system(cmd);
#else /* BCM */
	int ret;
	scb_val_t scb_val;
	char wlif_name[32];

	if(vifidx > 0)	sprintf(wlif_name, "wl%d.%d", bssidx, vifidx);
	else		strcpy(wlif_name, bssinfo[bssidx].wlif_name);

	RAST_INFO("DEAUTHENTICATE ["MACF"] from %s\n", ETHER_TO_MACF(sta->addr), wlif_name);
	memcpy(&scb_val.ea, &sta->addr, ETHER_ADDR_LEN);
	scb_val.val = 8; /* reason code: Disassociated because sending STA is leaving BSS */

	ret = wl_ioctl(wlif_name, WLC_SCB_DEAUTHENTICATE_FOR_REASON, &scb_val, sizeof(scb_val));
	if(ret < 0) {
		RAST_INFO("[WARNING] error to deauthticate ["MACF"] !!!\n", ETHER_TO_MACF(sta->addr));
	}
#endif
}

void rast_check_criteria(int bssidx, int vifidx)
{
	int idx=0;
	int flag_rssi, flag_idle;
	time_t now = uptime();
	rast_sta_info_t *sta = bssinfo[bssidx].assoclist[vifidx];

	while(sta) {
		flag_rssi = 0;
		flag_idle = 0;
		idx++;

#if defined(RTCONFIG_RALINK)
		if( (sta->rssi[0] < bssinfo[bssidx].user_low_rssi) &&
			(sta->rssi[1] < bssinfo[bssidx].user_low_rssi) && 
			(sta->rssi[2] < bssinfo[bssidx].user_low_rssi)	) {
#else
		if(sta->rssi < bssinfo[bssidx].user_low_rssi) {
#endif
			sta->rssi_hit_count++;
			if(sta->rssi_hit_count >= RAST_COUNT_RSSI)
				flag_rssi = 1;
		}
#if defined(RTCONFIG_RALINK)
		else     sta->rssi_hit_count = 0;
#endif

		if(sta->datarate < bssinfo[bssidx].idle_rate) {
			if(!sta->idle_state) {
				sta->idle_state = 1;
				sta->idle_start = now;
			}
			if((now - sta->idle_start) >= RAST_COUNT_IDLE)
				flag_idle = 1;
#if defined(RTCONFIG_RALINK)
			else	flag_idle = 0;
#endif
		}
		else {
			sta->idle_state = 0;
		}		

#if defined(RTCONFIG_RALINK)
		RAST_DBG("[%d][%s]: RSSI[(%d)1:%d/2:%d/3:%d], RSSI[C:%d/F:%d], DATA[R:%f/F:%d] %s \n", 
				idx, sta->mac_addr, 
				bssinfo[bssidx].user_low_rssi, sta->rssi[0], sta->rssi[1], sta->rssi[2],
				sta->rssi_hit_count, flag_rssi, sta->datarate, flag_idle,
				(flag_rssi & flag_idle) ? " *** Deauthenticate *** " : "");
#else
		RAST_DBG("[%d]sta ["MACF"]: rssi = %d dBm [Hit=%d],datarate = %.1f Kbps, active = %lu [idle start = %lu, period = %lu] %s\n",
			idx,
			ETHER_TO_MACF(sta->addr),
			sta->rssi,
			sta->rssi_hit_count,
			sta->datarate,
			sta->active,
			sta->idle_state ? sta->idle_start : 0,
			sta->idle_state ? now - sta->idle_start : 0,
			(flag_rssi & flag_idle) ? " *** Deauthenticate *** " : "");
#endif
		if(flag_rssi & flag_idle) {
			rast_deauth_sta(bssidx, vifidx, sta);
			sta = rast_remove_from_assoclist(bssidx, vifidx, &sta->addr);
			continue;	
		}
		
		sta = sta->next;
	}

	return;
}

#if !(defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA))
void rast_retrieve_bs_data(int bssidx, int vifidx)
{
	int ret;
	int argn;
	char ioctl_buf[4096];
	iov_bs_data_struct_t *data = (iov_bs_data_struct_t *)ioctl_buf;
	iov_bs_data_record_t *rec;
	iov_bs_data_counters_t *ctr;
	float datarate;
	rast_sta_info_t *sta = NULL;
	char wlif_name[32];

	if(vifidx > 0)  sprintf(wlif_name, "wl%d.%d", bssidx, vifidx);
        else            strcpy(wlif_name, bssinfo[bssidx].wlif_name);
	
	memset(ioctl_buf, 0, sizeof(ioctl_buf));
	strcpy(ioctl_buf, "bs_data");
	ret = wl_ioctl(wlif_name, WLC_GET_VAR, ioctl_buf, sizeof(ioctl_buf));
	if(ret < 0) {
		return;
	}

	for(argn = 0; argn < data->structure_count; argn++) {
		rec = &data->structure_record[argn];
		ctr = &rec->station_counters;

		if(ctr->acked == 0) continue;

		datarate = (ctr->time_delta) ?
			(float)ctr->throughput * 8000.0 / (float)ctr->time_delta : 0.0;

		sta = rast_add_to_assoclist(bssidx, vifidx, &(rec->station_address));
		if(sta) {
			sta->datarate = datarate;
		}
	}
	return;
}
#endif

void rast_timeout_sta(int bssidx, int vifidx)
{
	time_t now = uptime();
	rast_sta_info_t *sta, *prev, *next, *head;
	sta = bssinfo[bssidx].assoclist[vifidx];
	head = NULL;
	prev = NULL;

	while(sta) {
		if(now - sta->active > RAST_TIMEOUT_STA) {
#if defined(RTCONFIG_RALINK)
			RAST_DBG("[%s]: Free Mac[%s], TIME[N:%lu/A:%lu]\n", __FUNCTION__, sta->mac_addr , now , sta->active);
#else
			RAST_DBG("free ["MACF"] from assoclist [now = %lu][active = %lu]\n",
					ETHER_TO_MACF(sta->addr),
					now,
					sta->active);
#endif			
			next = sta->next;
			free(sta);
			sta = next;
			if(prev)
				prev->next = sta;
			continue;
		}
		
		if(head == NULL)
			head = sta;

		prev = sta;
		sta = sta->next;
	}
	bssinfo[bssidx].assoclist[vifidx] = head;
}

void rast_update_sta_info(int bssidx, int vifidx)
{
#if defined(RTCONFIG_RALINK)
	char *sp, *op;
	char wlif_name[32], header[128], data[2048];
	int hdrLen, staCount=0, getLen;
	struct iwreq wrq;
	roam_sta *ssap;
	rast_sta_info_t *staInfo = NULL;

	if (vifidx > 0) {
		sprintf(data, "wl%d.%d_ifname", bssidx, vifidx);
		strcpy(wlif_name, nvram_safe_get(data));
	}
	else
		strcpy(wlif_name, bssinfo[bssidx].wlif_name);

	memset(data, 0x00, sizeof(data));
       	wrq.u.data.length = sizeof(data);
       	wrq.u.data.pointer = (caddr_t) data;
       	wrq.u.data.flags = ASUS_SUBCMD_GROAM; 
	
	if (wl_ioctl(wlif_name, RTPRIV_IOCTL_ASUSCMD, &wrq) < 0) {
		RAST_INFO("[%s]: WI[%s] Access to StaInfo failure\n", __FUNCTION__, wlif_name);
		return;
	}
	
	memset(header, 0, sizeof(header));
	hdrLen = sprintf(header, "%-19s%-7s%-7s%-7s%-33s\n", "MAC", "RSSI0", "RSSI1","RSSI2","CURRATE");
	if (wrq.u.data.length > 0 && data[0] != 0) {
		getLen = strlen(wrq.u.data.pointer + hdrLen);
		ssap = (roam_sta *)(wrq.u.data.pointer + hdrLen);
	       	op = sp = wrq.u.data.pointer + hdrLen;
	       	while (*sp && ((getLen - (sp-op)) >= 0)) {
			ssap->sta[staCount].mac[18]='\0';
		       	ssap->sta[staCount].rssi[0][6]='\0';
		       	ssap->sta[staCount].rssi[1][6]='\0';
		       	ssap->sta[staCount].rssi[2][6]='\0';
			ssap->sta[staCount].curRate[32]='\0';			
		       	sp += hdrLen;
			staCount++;
		}

#ifdef RTCONFIG_WIRELESSREPEATER
		char *aif;
		unsigned char pap_bssid[18];
		struct iwreq wrq1;
	       	if(nvram_get_int("sw_mode") == SW_MODE_REPEATER && nvram_get_int("wlc_band") == bssidx) {
			memset(header, 0, sizeof(header));
		       	aif = nvram_get( strcat_r(bssinfo[bssidx].prefix, "vifs", header) );
		       	if(wl_ioctl(aif, SIOCGIWAP, &wrq1)>=0); {
			       	wrq1.u.ap_addr.sa_family = ARPHRD_ETHER;
			       	sprintf(pap_bssid, "%02X:%02X:%02X:%02X:%02X:%02X",
					       	(unsigned char)wrq1.u.ap_addr.sa_data[0], (unsigned char)wrq1.u.ap_addr.sa_data[1],
					       	(unsigned char)wrq1.u.ap_addr.sa_data[2], (unsigned char)wrq1.u.ap_addr.sa_data[3],
					       	(unsigned char)wrq1.u.ap_addr.sa_data[4], (unsigned char)wrq1.u.ap_addr.sa_data[5]  );
		       	}
	       	}
#endif

		if( !staCount ) return;
		// add to assoclist //
		for(hdrLen=0; hdrLen< staCount; hdrLen++) {
#ifdef RTCONFIG_WIRELESSREPEATER
			// pap bssid,skip //
			if( !strncmp(pap_bssid, ssap->sta[hdrLen].mac, sizeof(pap_bssid))) {
				RAST_DBG("[%s]: P-AP BSSID[%s]\n", __FUNCTION__, pap_bssid);
				continue;
			}
#endif
		       	staInfo = rast_add_to_assoclist(bssidx, vifidx, ether_aton(ssap->sta[hdrLen].mac) );
			
			for( getLen=0; getLen<xR_MAX; getLen++ )  
				staInfo->rssi[getLen] = !atoi(ssap->sta[hdrLen].rssi[getLen]) ? -100 : atoi(ssap->sta[hdrLen].rssi[getLen]);

			memcpy(staInfo->mac_addr, ssap->sta[hdrLen].mac, sizeof(staInfo->mac_addr) );

			if( staInfo->curDatarate != 0 ) {
				if( staInfo->curDatarate > (float)atoi(ssap->sta[hdrLen].curRate)/RAST_MTK_DATARATE/RAST_POLL_INTV_NORMAL ) staInfo->curDatarate = 0;
				staInfo->datarate = (float)(atoi(ssap->sta[hdrLen].curRate)/RAST_MTK_DATARATE/RAST_POLL_INTV_NORMAL ) - staInfo->curDatarate ; //kbits
				staInfo->curDatarate = ( atoi(ssap->sta[hdrLen].curRate)/RAST_MTK_DATARATE/RAST_POLL_INTV_NORMAL );
			}
			else {
				staInfo->curDatarate = ( atoi(ssap->sta[hdrLen].curRate)/RAST_MTK_DATARATE/RAST_POLL_INTV_NORMAL );
				staInfo->datarate = 0;
			}
			
			staInfo->active = uptime();			

			RAST_DBG("[%s]: [%s][%d][%s], RSSI[1:%d/2:%d/3:%d], RATE[%f] \n", __FUNCTION__, wlif_name, hdrLen,
				       	staInfo->mac_addr, staInfo->rssi[0], staInfo->rssi[1], staInfo->rssi[2], staInfo->datarate, staInfo->active );
		}
	}
       	rast_timeout_sta(bssidx, vifidx);
        rast_check_criteria(bssidx, vifidx);

#elif defined(RTCONFIG_QCA)
	#define STA_LOW_RSSI_PATH "/tmp/low_rssi"
	#define STA_TXRX_BYTES_PATH "/tmp/txrx_bytes"
	FILE *fp1, *fp2;
	char wlif_name[32], line_buf[300];
	char tmp[64], *ptr;
	int cur_txrx_bytes;
	WLANCONFIG_LIST *result;
	rast_sta_info_t *sta = NULL;

	if (vifidx > 0) {
		sprintf(line_buf, "wl%d.%d_ifname", bssidx, vifidx);
		strcpy(wlif_name, nvram_safe_get(line_buf));
	}
	else
		strcpy(wlif_name, bssinfo[bssidx].wlif_name);

	result = malloc(sizeof(WLANCONFIG_LIST));
	memset(result, 0, sizeof(WLANCONFIG_LIST));

	/* get MAC and RSSI of station */
	doSystem("wlanconfig %s list > %s", wlif_name, STA_LOW_RSSI_PATH);
	fp1 = fopen(STA_LOW_RSSI_PATH, "r");
	if (fp1) {
		fgets(line_buf, sizeof(line_buf), fp1); // ignore header
		while (fgets(line_buf, sizeof(line_buf), fp1)) {
			sscanf(line_buf, "%s%u%u%s%s%u%u%u%u%s%s%s%s%s%s%s%s", 
						result->addr, 
						&result->aid, 
						&result->chan, 
						result->txrate, 
						result->rxrate, 
						&result->rssi, 
						&result->idle, 
						&result->txseq, 
						&result->rcseq, 
						result->caps, 
						result->acaps, 
						result->erp, 
						result->state_maxrate, 
						result->wps, 
						result->rsn, 
						result->wme,
						result->mode);
			RAST_DBG("[%s][%u][%u][%s][%s][%u][%u][%u][%u][%s][%s][%s][%s][%s][%s][%s]\n", 
						result->addr, 
						result->aid, 
						result->chan, 
						result->txrate, 
						result->rxrate, 
						result->rssi, 
						result->idle, 
						result->txseq, 
						result->rcseq, 
						result->caps, 
						result->acaps, 
						result->erp, 
						result->state_maxrate, 
						result->wps, 
						result->rsn, 
						result->wme);
			/* add to assoclist */
			sta = rast_add_to_assoclist(bssidx, vifidx, ether_aton(result->addr));
			sta->rssi = -result->rssi;
			sta->active = uptime();
			/* get Tx/Rx bytes of station */
			doSystem("80211stats -i %s %s > %s", wlif_name, result->addr, STA_TXRX_BYTES_PATH);
			fp2 = fopen(STA_TXRX_BYTES_PATH, "r");
			if (fp2) {
				cur_txrx_bytes = 0;
				while (fgets(tmp, sizeof(tmp), fp2)) {
					if ((ptr = strstr(tmp, "rx_bytes ")) 
							|| (ptr = strstr(tmp, "tx_bytes "))) {
						ptr += strlen("rx_bytes ");
						cur_txrx_bytes += atoi(ptr);
					}
				}
				sta->datarate = (float)((cur_txrx_bytes - sta->last_txrx_bytes) >> 7/* bytes to Kbits*/) / RAST_POLL_INTV_NORMAL/* Kbps */;
				sta->last_txrx_bytes = cur_txrx_bytes;
				fclose(fp2);
				unlink(STA_TXRX_BYTES_PATH);
			}
		}
		free(result);
		fclose(fp1);
		unlink(STA_LOW_RSSI_PATH);
	}
	rast_timeout_sta(bssidx, vifidx);
	rast_check_criteria(bssidx, vifidx);

#else /* BCM */
	struct maclist *mac_list;
	int mac_list_size;
	scb_val_t scb_val;
	int mcnt;
	char wlif_name[32];
	int32 rssi;
	rast_sta_info_t *sta = NULL;

	if(vifidx > 0)  sprintf(wlif_name, "wl%d.%d", bssidx, vifidx);

        else            strcpy(wlif_name, bssinfo[bssidx].wlif_name);

	mac_list_size = sizeof(mac_list->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
	mac_list = malloc(mac_list_size);
	
	if(!mac_list)
		goto exit;

	memset(mac_list, 0, mac_list_size);

	/* query authentication sta list */
	strcpy((char*) mac_list, "authe_sta_list");
	if(wl_ioctl(wlif_name, WLC_GET_VAR, mac_list, mac_list_size))
		goto exit;

	for(mcnt=0; mcnt < mac_list->count; mcnt++) {
		memcpy(&scb_val.ea, &mac_list->ea[mcnt], ETHER_ADDR_LEN);


		if(wl_ioctl(wlif_name, WLC_GET_RSSI, &scb_val, sizeof(scb_val_t)))
			continue;
		
		rssi = scb_val.val;

		/* add to assoclist */
		sta = rast_add_to_assoclist(bssidx, vifidx, &(mac_list->ea[mcnt]));
		sta->rssi = rssi;
		sta->active = uptime();
	}

	rast_retrieve_bs_data(bssidx, vifidx);
	rast_timeout_sta(bssidx, vifidx);
	rast_check_criteria(bssidx, vifidx);

exit:
	if(mac_list) free(mac_list);
#endif
	return;
}

static void rast_watchdog(int sig)
{
	int idx,vidx;
	char prefix[32];
	char tmp[32];

	if(!nvram_get_int("wlready"))
		return;

	if(init) {
		rast_init_bssinfo();
		init = 0;
	}

	for(idx=0; idx < wlif_count; idx++) {
#if (defined(RTCONFIG_RALINK) || defined(RTCONFIG_QCA))
		if(!bssinfo[idx].user_low_rssi) continue;

		for(vidx = 0; vidx < MAX_SUBIF_NUM; vidx++) { 
			if(vidx > 0) {
#ifdef RTCONFIG_WIRELESSREPEATER
				if (nvram_get_int("sw_mode") == SW_MODE_REPEATER) continue;
#endif

				sprintf(prefix, "wl%d.%d", idx, vidx);
				if( !nvram_match(strcat_r(prefix, "_bss_enabled", tmp), "1") ) continue;
			}
			RAST_DBG("[%s]: StaInfo[%s], RssiCriteria[%d]\n", __FUNCTION__,
					vidx > 0 ? nvram_safe_get(strcat_r(prefix, "_ifname", tmp)) : bssinfo[idx].wlif_name, 
					bssinfo[idx].user_low_rssi );
			rast_update_sta_info(idx,vidx);
	       	}
#else /* BCM */
		int val;

		if(!bssinfo[idx].user_low_rssi) continue;
#ifdef RTCONFIG_PROXYSTA
		if(psta_exist_except(idx) || psr_exist_except(idx)) continue;
		else if(is_psta(idx) || is_psr(idx))	continue;
#endif

		wl_ioctl(bssinfo[idx].wlif_name, WLC_GET_RADIO, &val, sizeof(val));
		val &= WL_RADIO_SW_DISABLE | WL_RADIO_HW_DISABLE;
		if(val) {
			RAST_DBG("%s radio is disabled!\n", bssinfo[idx].wlif_name);
			continue;
		}

#ifdef RTCONFIG_WIRELESS_REPEATER
		if((nvram_get_int("sw_mode") == SW_MODE_REPEATER) && 
			(nvram_get_int("wlc_band")) == idx)
		{
			RAST_DBG("### Check stainfo [wl%d.%d][rssi criteria = %d] ###\n",
			idx,
			1,
			bssinfo[idx].user_low_rssi);
				
			rast_update_sta_info(idx,1);
			continue;
		}
#endif

		for(vidx = 0; vidx < MAX_SUBIF_NUM; vidx++) {

			if(vidx > 0) {
				sprintf(prefix, "wl%d.%d", idx, vidx);
				if(!nvram_match(strcat_r(prefix, "_bss_enabled", tmp), "1"))
					continue;
			}

			RAST_DBG("### Check stainfo [%s][rssi criteria = %d] ###\n",
				vidx > 0 ? prefix : bssinfo[idx].wlif_name,
				bssinfo[idx].user_low_rssi);

			rast_update_sta_info(idx,vidx);
		}
#endif
	}
		
	RAST_DBG("\n");
	
}

static void rast_exit(int sig)
{
	/* free assoclist */
	int i, vi;
	rast_sta_info_t *assoclist, *next;

	alarmtimer(0, 0);

	for(i = 0; i < wlif_count; i++) {
		for(vi = 0; vi < MAX_SUBIF_NUM; vi++) {
			assoclist = bssinfo[i].assoclist[vi];
			while(assoclist) {
				next = assoclist->next;
				free(assoclist);
				assoclist = next;
			}
		}
	}

	RAST_INFO("ROAMAST Exit...\n");
	remove("/var/run/roamast.pid");
	exit(0);
}

int roam_assistant_main(int argc, char *argv[]) 
{
	FILE *fp;
        sigset_t sigs_to_catch;

        /* write pid */
        if ((fp = fopen("/var/run/roamast.pid", "w")) != NULL)
        {
                fprintf(fp, "%d", getpid());
                fclose(fp);
        }

#if 1
	rast_dbg = nvram_get_int("rast_dbg");
#else	// Open DBG_INFO msg
	rast_dbg = 1; 
#endif
	RAST_INFO("ROAMAST Start...\n");

        /* set the signal handler */
        sigemptyset(&sigs_to_catch);
        sigaddset(&sigs_to_catch, SIGALRM);
        sigaddset(&sigs_to_catch, SIGTERM);
        sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

        signal(SIGALRM, rast_watchdog);
        signal(SIGTERM, rast_exit);

	alarmtimer(RAST_POLL_INTV_NORMAL, 0);	
	RAST_INFO("%s \n", __FUNCTION__);
        /* Most of time it goes to sleep */
        while (1)
        {
                pause();
        }

        return 0;
}
