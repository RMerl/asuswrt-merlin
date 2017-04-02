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
 */
/*
 * ASUS Home Gateway Reference Design
 * Web Page Configuration Support Routines
 *
 * Copyright 2004, ASUSTeK Inc.
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 */

#ifdef WEBS
#include <webs.h>
#include <uemf.h>
#include <ej.h>
#else /* !WEBS */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <httpd.h>
#endif /* WEBS */
#include <typedefs.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <shutils.h>
#include <qca.h>
#include <iwlib.h>
//#include <stapriv.h>
#include <ethutils.h>
#include <shared.h>
#include <sys/mman.h>
#ifndef O_BINARY
#define O_BINARY 	0
#endif
#ifndef MAP_FAILED
#define MAP_FAILED (-1)
#endif

#define wan_prefix(unit, prefix)	snprintf(prefix, sizeof(prefix), "wan%d_", unit)
//static char * rfctime(const time_t *timep);
//static char * reltime(unsigned int seconds);
void reltime(unsigned int seconds, char *buf);
static int wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit);

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/klog.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <net/if_arp.h>

#include <dirent.h>

const char *get_wifname(int band)
{
	if (band)
		return WIF_5G;
	else
		return WIF_2G;
}

typedef struct _WPS_CONFIGURED_VALUE {
	unsigned short 	Configured;	// 1:un-configured/2:configured
	char		BSSID[18];
	char 		SSID[32 + 1];
	char		AuthMode[16];	// Open System/Shared Key/WPA-Personal/WPA2-Personal/WPA-Enterprise/WPA2-Enterprise
	char 		Encryp[8];	// None/WEP/TKIP/AES
	char 		DefaultKeyIdx;
	char 		WPAKey[64 + 1];
} WPS_CONFIGURED_VALUE;

/* shared/sysdeps/api-qca.c */
extern u_int ieee80211_mhz2ieee(u_int freq);
extern int get_channel_list_via_driver(int unit, char *buffer, int len);
extern int get_channel_list_via_country(int unit, const char *country_code, char *buffer, int len);

#define WL_A		(1U << 0)
#define WL_B		(1U << 1)
#define WL_G		(1U << 2)
#define WL_N		(1U << 3)
#define WL_AC		(1U << 4)

static const struct mode_s {
	unsigned int mask;
	char *mode;
} mode_tbl[] = {
	{ WL_A,	"a" },
	{ WL_B,	"b" },
	{ WL_G, "g" },
	{ WL_N, "n" },
	{ WL_AC, "ac" },
	{ 0, NULL },
};

/**
 * Run "iwpriv XXX get_XXX" and return string behind colon.
 * Expected result is shown below:
 * ath1      get_mode:11ACVHT40
 *                    ^^^^^^^^^
 * @iface:	interface name
 * @cmd:	get cmd
 * @buf:	pointer to memory area which is used to keep result.
 * 		it is guarantee as valid/empty string, if it is valid pointer
 * @buf_len:	length of @buf
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *     -2:	run iwpriv command fail
 *     -3:	read result strin fail
 */
static int __iwpriv_get(char *iface, char *cmd, char *buf, unsigned int buf_len)
{
	int len;
	FILE *fp;
	char *p = NULL, iwpriv_cmd[64], tmp[128];

	if (!iface || *iface == '\0' || !cmd || *cmd == '\0' || !buf || buf_len <= 1)
		return -1;

	if (strncmp(cmd, "get_", 4) && strncmp(cmd, "g_", 2)) {
		dbg("%s: iface [%s] cmd [%s] may not be supported!\n",
			__func__, iface, cmd);
	}

	*buf = '\0';
	sprintf(iwpriv_cmd, "iwpriv %s %s", iface, cmd);
	if (!(fp = popen(iwpriv_cmd, "r")))
		return -2;

	len = fread(tmp, 1, sizeof(tmp), fp);
	pclose(fp);
	if (len < 1)
		return -3;

	tmp[len-1] = '\0';
	if (!(p = strchr(tmp, ':'))) {
		dbg("%s: parsing [%s] of cmd [%s] error!", __func__, tmp, cmd);
		return -4;
	}
	p++;
	chomp(p);
	strlcpy(buf, p, buf_len);

	return 0;
}

/**
 * Run "iwpriv XXX get_XXX" and return string behind colon.
 * Expected result is shown below:
 * ath1      get_mode:11ACVHT40
 *                    ^^^^^^^^^ result
 * @iface:	interface name
 * @cmd:	get cmd
 * @return:
 * 	NULL	invalid parameter or error.
 *  otherwise:	success
 */
static char *iwpriv_get(char *iface, char *cmd)
{
	static char result[256];

	if (__iwpriv_get(iface, cmd, result, sizeof(result)))
		return NULL;

	return result;
}

static void getWPSConfig(int unit, WPS_CONFIGURED_VALUE *result)
{
	char buf[128];
	FILE *fp;

	memset(result, 0, sizeof(result));

	sprintf(buf, "hostapd_cli -i%s get_config", get_wifname(unit));
	fp = popen(buf, "r");
	if (fp) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			char *pt1, *pt2;

			chomp(buf);
			//BSSID
			if ((pt1 = strstr(buf, "bssid="))) {
				pt2 = pt1 + strlen("bssid=");
				strcpy(result->BSSID, pt2);
			}
			//SSID
			if ((pt1 = strstr(buf, "ssid="))) {
				pt2 = pt1 + strlen("ssid=");
				strcpy(result->SSID, pt2);
			}
			//Configured
			else if ((pt1 = strstr(buf, "wps_state="))) {
				pt2 = pt1 + strlen("wps_state=");
				if (!strcmp(pt2, "configured") ||
				    (!strcmp(pt2, "disabled") && nvram_get_int("w_Setting"))
				   )
					result->Configured = 2;
				else
					result->Configured = 1;
			}
			//WPAKey
			else if ((pt1 = strstr(buf, "passphrase="))) {
				pt2 = pt1 + strlen("passphrase=");
				strcpy(result->WPAKey, pt2);
			}
			//AuthMode
			else if ((pt1 = strstr(buf, "key_mgmt="))) {
				pt2 = pt1 + strlen("key_mgmt=");
				strcpy(result->AuthMode, pt2);/* FIXME: NEED TRANSFORM CONTENT */
			}
			//Encryp
			else if ((pt1 = strstr(buf, "rsn_pairwise_cipher="))) {
				pt2 = pt1 + strlen("rsn_pairwise_cipher=");
				if (!strcmp(pt2, "NONE"))
					strcpy(result->Encryp, "None");
				else if (!strncmp(pt2, "WEP", 3))
					strcpy(result->Encryp, "WEP");
				else if (!strcmp(pt2, "TKIP"))
					strcpy(result->Encryp, "TKIP");
				else if (!strncmp(pt2, "CCMP", 4))
					strcpy(result->Encryp, "AES");
			}
		}
		pclose(fp);
	}
	//dbg("%s: SSID[%s], Configured[%d], WPAKey[%s], AuthMode[%s], Encryp[%s]\n", __FUNCTION__, result->SSID, result->Configured, result->WPAKey, result->AuthMode, result->Encryp);
}

static char *__get_wlifname(int band, int subunit, char *buf)
{
	if (!buf)
		return buf;

	if (!subunit)
		strcpy(buf, (!band)? WIF_2G:WIF_5G);
	else
		sprintf(buf, "%s0%d", (!band)? WIF_2G:WIF_5G, subunit);

	return buf;
}

#define MAX_NO_MSSID	4
static int get_wlsubnet(int band, const char *ifname)
{
	int subnet, sidx;
	char buf[32];

	for (subnet = 0, sidx = 0; subnet < MAX_NO_MSSID; subnet++)
	{
		if(!nvram_match(wl_nvname("bss_enabled", band, subnet), "1")) {
			if (!subnet)
				sidx++;
			continue;
		}

		if(strcmp(ifname, __get_wlifname(band, sidx, buf)) == 0)
			return subnet;

		sidx++;
	}
	return -1;
}

char *getAPPhyMode(int unit)
{
	static char result[sizeof("11b/g/nXXXXXX")] = "";
	const struct mode_s *q;
	char *mode, *puren, *p, *sep, *iface;
	unsigned int m = 0;

	iface = (char*) get_wifname(unit);
	mode = iwpriv_get(iface, "get_mode");
	if (!mode)
		return "";

	/* Ref to phymode_strings of qca-wifi driver. */
	if (!strcmp(mode, "11A") || !strcmp(mode, "TA"))
		m = WL_A;
	else if (!strcmp(mode, "11G") || !strcmp(mode, "TG"))
		m = WL_G | WL_B;
	else if (!strcmp(mode, "11B"))
		m = WL_B;
	else if (!strncmp(mode, "11NA", 4))
		m = WL_N | WL_A;
	else if (!strncmp(mode, "11NG", 4))
		m = WL_N | WL_G | WL_B;
	else if (!strncmp(mode, "11ACVHT", 7))
		m = WL_AC | WL_N | WL_A;
	else {
		dbg("%s: Unknown mode [%s]\n", __func__, mode);
	}

	/* If puren is enabled, remove a/g/b. */
	puren = iwpriv_get(iface, "get_puren");
	if (atoi(puren))
		m &= ~(WL_A | WL_B | WL_G);

	p = result;
	*p = '\0';
	sep = "11";
	for (q = &mode_tbl[0]; m > 0 && q->mask; ++q) {
		if (!(m & q->mask))
			continue;

		m &= ~q->mask;
		strcat(p, sep);
		p += strlen(sep);
		strcat(p, q->mode);
		p += strlen(q->mode);
		sep = "/";
	}

	return result;
}

unsigned int getAPChannel(int unit)
{
	char buf[8192];
	FILE *fp;
	int len, i = 0;
	char *pt1, *pt2, ch_mhz[5];

	sprintf(buf, "iwconfig %s", get_wifname(unit));
	fp = popen(buf, "r");
	if (fp) {
		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len-1] = '\0';
			pt1 = strstr(buf, "Frequency:");
			if (pt1) {
				pt2 = pt1 + strlen("Frequency:");
				pt1 = strstr(pt2, " GHz");
				if (pt1) {
					*pt1 = '\0';
					memset(ch_mhz, 0, sizeof(ch_mhz));
					len = strlen(pt2);
					for (i = 0; i < 5; i++) {
						if (i < len) {
							if (pt2[i] == '.')
								continue;
							sprintf(ch_mhz, "%s%c", ch_mhz, pt2[i]);
						}
						else
							sprintf(ch_mhz, "%s0", ch_mhz);
					}
					//dbg("Frequency:%s MHz\n", ch_mhz);
					return ieee80211_mhz2ieee((unsigned int)atoi(ch_mhz));
				}
			}
		}
	}
	return 0;
}

int
ej_wl_control_channel(int eid, webs_t wp, int argc, char_t **argv)
{
        int ret = 0;
        int channel_24 = 0, channel_50 = 0;
        
	channel_24 = getAPChannel(0);

        if (!(channel_50 = getAPChannel(1)))
                ret = websWrite(wp, "[\"%d\", \"%d\"]", channel_24, 0);
        else
                ret = websWrite(wp, "[\"%d\", \"%d\"]", channel_24, channel_50);
	
        return ret;
}

long getSTAConnTime(char *ifname, char *bssid)
{
	char buf[8192];
	FILE *fp;
	int len;
	char *pt1,*pt2;

	sprintf(buf, "hostapd_cli -i%s sta %s", ifname, bssid);
	fp = popen(buf, "r");
	if (fp) {
		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len-1] = '\0';
			pt1 = strstr(buf, "connected_time=");
			if (pt1) {
				pt2 = pt1 + strlen("connected_time=");
				chomp(pt2);
				return atol(pt2);
			}
		}
	}
	return 0;
}

typedef struct _WLANCONFIG_LIST {
	char addr[18];
	unsigned int aid;
	unsigned int chan;
	char txrate[7];
	char rxrate[10];
	unsigned int rssi;
	unsigned int idle;
	unsigned int txseq;
	unsigned int rxseq;
	char caps[12];
	char acaps[10];
	char erp[7];
	char state_maxrate[20];
	char wps[4];
	char conn_time[12];
	char rsn[4];
	char wme[4];
	char mode[31];
	char ie[32];
	char htcaps[10];
	unsigned int u_acaps;
	unsigned int u_erp;
	unsigned int u_state_maxrate;
	unsigned int u_psmode;
	int subunit;
} WLANCONFIG_LIST;

#if defined(RTCONFIG_WIFI_QCA9990_QCA9990) || defined(RTCONFIG_WIFI_QCA9994_QCA9994)
#define MAX_STA_NUM 512
#else
#define MAX_STA_NUM 256
#endif
typedef struct _WIFI_STA_TABLE {
	int Num;
	WLANCONFIG_LIST Entry[ MAX_STA_NUM ];
} WIFI_STA_TABLE;


void
convert_mac_string(char *mac)
{
	int i;
	char mac_str[18];
	memset(mac_str,0,sizeof(mac_str));

	for(i=0;i<strlen(mac);i++)
        {
                if(*(mac+i)>0x60 && *(mac+i)<0x67) 
                       sprintf(mac_str,"%s%c",mac_str,*(mac+i)-0x20);
                else
                       sprintf(mac_str,"%s%c",mac_str,*(mac+i));
        }
	strncpy(mac,mac_str,strlen(mac_str));
}


static int getSTAInfo(int unit, WIFI_STA_TABLE *sta_info)
{
	#define STA_INFO_PATH "/tmp/wlanconfig_athX_list"
	FILE *fp;
	int ret = 0, l2_offset, subunit;
	char *unit_name;
	char *p, *ifname, *l2, *l3;
	char *wl_ifnames;
	char line_buf[300]; // max 14x

	memset(sta_info, 0, sizeof(*sta_info));
	unit_name = strdup(get_wifname(unit));
	if (!unit_name)
		return ret;
	wl_ifnames = strdup(nvram_safe_get("lan_ifnames"));
	if (!wl_ifnames) {
		free(unit_name);
		return ret;
	}
	p = wl_ifnames;
	while ((ifname = strsep(&p, " ")) != NULL) {
		while (*ifname == ' ') ++ifname;
		if (*ifname == 0) break;
		if(strncmp(ifname,unit_name,strlen(unit_name)))
			continue;

		subunit = get_wlsubnet(unit, ifname);
		if (subunit < 0)
			subunit = 0;

		doSystem("wlanconfig %s list > %s", ifname, STA_INFO_PATH);
		fp = fopen(STA_INFO_PATH, "r");
		if (fp) {
/* wlanconfig ath1 list
ADDR               AID CHAN TXRATE RXRATE RSSI IDLE  TXSEQ  RXSEQ  CAPS        ACAPS     ERP    STATE MAXRATE(DOT11) HTCAPS ASSOCTIME    IEs   MODE PSMODE
00:10:18:55:cc:08    1  149  55M   1299M   63    0      0   65535               0        807              0              Q 00:10:33 IEEE80211_MODE_11A  0
08:60:6e:8f:1e:e6    2  149 159M    866M   44    0      0   65535     E         0          b              0           WPSM 00:13:32 WME IEEE80211_MODE_11AC_VHT80  0
08:60:6e:8f:1e:e8    1  157 526M    526M   51 4320      0   65535    EP         0          b              0          AWPSM 00:00:10 RSN WME IEEE80211_MODE_11AC_VHT80 0
*/
			//fseek(fp, 131, SEEK_SET);	// ignore header
			fgets(line_buf, sizeof(line_buf), fp); // ignore header
			l2 = strstr(line_buf, "ACAPS");
			if (l2 != NULL)
				l2_offset = (int)(l2 - line_buf);
			else {
				l2_offset = 79;
				l2 = line_buf + l2_offset;
			}
			while ( fgets(line_buf, sizeof(line_buf), fp) ) {
				WLANCONFIG_LIST *r = &sta_info->Entry[sta_info->Num++];

				r->subunit = subunit;
				/* IEs may be empty string, find IEEE80211_MODE_ before parsing mode and psmode. */
				l3 = strstr(line_buf, "IEEE80211_MODE_");
				if (l3) {
					*(l3 - 1) = '\0';
					sscanf(l3, "IEEE80211_MODE_%s %d", r->mode, &r->u_psmode);
				}
				*(l2 - 1) = '\0';
				sscanf(line_buf, "%s%u%u%s%s%u%u%u%u%[^\n]",
					r->addr, &r->aid, &r->chan, r->txrate,
					r->rxrate, &r->rssi, &r->idle, &r->txseq,
					&r->rxseq, r->caps);
				sscanf(l2, "%u%x%u%s%s%[^\n]",
					&r->u_acaps, &r->u_erp, &r->u_state_maxrate, r->htcaps, r->conn_time, r->ie);
				if (strlen(r->rxrate) >= 6)
					strcpy(r->rxrate, "0M");
#if 0
				dbg("[%s][%u][%u][%s][%s][%u][%u][%u][%u][%s]"
					"[%u][%u][%x][%s][%s][%s][%d]\n",
					r->addr, r->aid, r->chan, r->txrate, r->rxrate,
					r->rssi, r->idle, r->txseq, r->rxseq, r->caps,
					r->u_acaps, r->u_erp, r->u_state_maxrate, r->htcaps, r->ie,
					r->mode, r->u_psmode);
#endif

				convert_mac_string(r->addr);
			}

			fclose(fp);
			unlink(STA_INFO_PATH);
		}
	}
	free(wl_ifnames);
	free(unit_name);
	return ret;
}

char* GetBW(int BW)
{
	switch(BW)
	{
		case BW_10:
			return "10M";

		case BW_20:
			return "20M";

		case BW_40:
			return "40M";

#if defined(RTAC52U) || defined(RTAC51U)
		case BW_80:
			return "80M";
#endif

		default:
			return "N/A";
	}
}

char* GetPhyMode(int Mode)
{
	switch(Mode)
	{
		case MODE_CCK:
			return "CCK";

		case MODE_OFDM:
			return "OFDM";
		case MODE_HTMIX:
			return "HTMIX";

		case MODE_HTGREENFIELD:
			return "GREEN";

#if defined(RTAC52U) || defined(RTAC51U)
		case MODE_VHT:
			return "VHT";
#endif

		default:
			return "N/A";
	}
}

int MCSMappingRateTable[] =
	{2,  4,   11,  22, // CCK
	12, 18,   24,  36, 48, 72, 96, 108, // OFDM
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 104, 156, 208, 234, 260, // 20MHz, 800ns GI, MCS: 0 ~ 15
	39, 78,  117, 156, 234, 312, 351, 390,										  // 20MHz, 800ns GI, MCS: 16 ~ 23
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540, // 40MHz, 800ns GI, MCS: 0 ~ 15
	81, 162, 243, 324, 486, 648, 729, 810,										  // 40MHz, 800ns GI, MCS: 16 ~ 23
	14, 29,   43,  57,  87, 115, 130, 144, 29, 59,   87, 115, 173, 230, 260, 288, // 20MHz, 400ns GI, MCS: 0 ~ 15
	43, 87,  130, 173, 260, 317, 390, 433,										  // 20MHz, 400ns GI, MCS: 16 ~ 23
	30, 60,   90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600, // 40MHz, 400ns GI, MCS: 0 ~ 15
	90, 180, 270, 360, 540, 720, 810, 900,
	13, 26,   39,  52,  78, 104, 117, 130, 156, /* 11ac: 20Mhz, 800ns GI, MCS: 0~8 */
	27, 54,   81, 108, 162, 216, 243, 270, 324, 360, /*11ac: 40Mhz, 800ns GI, MCS: 0~9 */
	59, 117, 176, 234, 351, 468, 527, 585, 702, 780, /*11ac: 80Mhz, 800ns GI, MCS: 0~9 */
	14, 29,   43,  57,  87, 115, 130, 144, 173, /* 11ac: 20Mhz, 400ns GI, MCS: 0~8 */
	30, 60,   90, 120, 180, 240, 270, 300, 360, 400, /*11ac: 40Mhz, 400ns GI, MCS: 0~9 */
	65, 130, 195, 260, 390, 520, 585, 650, 780, 867 /*11ac: 80Mhz, 400ns GI, MCS: 0~9 */
	};


#define FN_GETRATE(_fn_, _st_)						\
_fn_(_st_ HTSetting)							\
{									\
	int rate_count = sizeof(MCSMappingRateTable)/sizeof(int);	\
	int rate_index = 0;						\
									\
	if (HTSetting.field.MODE >= MODE_VHT)				\
	{								\
		if (HTSetting.field.BW == BW_20) {			\
			rate_index = 108 +				\
			((unsigned char)HTSetting.field.ShortGI * 29) +	\
			((unsigned char)HTSetting.field.MCS);		\
		}							\
		else if (HTSetting.field.BW == BW_40) {			\
			rate_index = 117 +				\
			((unsigned char)HTSetting.field.ShortGI * 29) +	\
			((unsigned char)HTSetting.field.MCS);		\
		}							\
		else if (HTSetting.field.BW == BW_80) {			\
			rate_index = 127 +				\
			((unsigned char)HTSetting.field.ShortGI * 29) +	\
			((unsigned char)HTSetting.field.MCS);		\
		}							\
	}								\
	else								\
	if (HTSetting.field.MODE >= MODE_HTMIX)				\
	{								\
		rate_index = 12 + ((unsigned char)HTSetting.field.BW *24) + ((unsigned char)HTSetting.field.ShortGI *48) + ((unsigned char)HTSetting.field.MCS);	\
	}								\
	else								\
	if (HTSetting.field.MODE == MODE_OFDM)				\
		rate_index = (unsigned char)(HTSetting.field.MCS) + 4;	\
	else if (HTSetting.field.MODE == MODE_CCK)			\
		rate_index = (unsigned char)(HTSetting.field.MCS);	\
									\
	if (rate_index < 0)						\
		rate_index = 0;						\
									\
	if (rate_index >= rate_count)					\
		rate_index = rate_count-1;				\
									\
	return (MCSMappingRateTable[rate_index] * 5)/10;		\
}

int FN_GETRATE(getRate,      MACHTTRANSMIT_SETTING)		//getRate(MACHTTRANSMIT_SETTING)
int FN_GETRATE(getRate_2g,   MACHTTRANSMIT_SETTING_2G)		//getRate_2g(MACHTTRANSMIT_SETTING_2G)
#if defined(RTAC52U) || defined(RTAC51U)
int FN_GETRATE(getRate_11ac, MACHTTRANSMIT_SETTING_11AC)	//getRate_11ac(MACHTTRANSMIT_SETTING_11AC)
#endif



int
ej_wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int retval = 0;
	int ii = 0;
	char word[256], *next;

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
		retval += wl_status(eid, wp, argc, argv, ii);
		retval += websWrite(wp, "\n");

		ii++;
	}

	return retval;
}

int
ej_wl_status_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_status(eid, wp, argc, argv, 0);
}

static int
show_wliface_info(webs_t wp, int unit, char *ifname, char *op_mode, char *ssid)
{
	int ret = 0;
	FILE *fp;
	unsigned char mac_addr[ETHER_ADDR_LEN];
	char tmpstr[1024], cmd[] = "iwconfig staXYYYYYY";
	char *p, ap_bssid[] = "00:00:00:00:00:00XXX";

	if (unit < 0 || !ifname || !op_mode || !ssid)
		return 0;

	memset(&mac_addr, 0, sizeof(mac_addr));
	get_iface_hwaddr(ifname, mac_addr);
	ret += websWrite(wp, "=======================================================================================\n"); // separator
	ret += websWrite(wp, "OP Mode		: %s\n", op_mode);
	ret += websWrite(wp, "SSID		: %s\n", ssid);
	sprintf(cmd, "iwconfig %s", ifname);
	if ((fp = popen(cmd, "r")) != NULL && fread(tmpstr, 1, sizeof(tmpstr), fp) > 1) {
		pclose(fp);
		*(tmpstr + sizeof(tmpstr) - 1) = '\0';
		*ap_bssid = '\0';
		if ((p = strstr(tmpstr, "Access Point: ")) != NULL) {
			strncpy(ap_bssid, p + 14, 17);
			ap_bssid[17] = '\0';
		}
		ret += websWrite(wp, "BSSID		: %s\n", ap_bssid);
	}
	ret += websWrite(wp, "MAC address	: %02X:%02X:%02X:%02X:%02X:%02X\n",
		mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
	*tmpstr = '\0';
	strcpy(tmpstr, getAPPhyMode(unit));
	ret += websWrite(wp, "Phy Mode	: %s\n", tmpstr);
	ret += websWrite(wp, "Channel		: %u\n", getAPChannel(unit));

	return ret;
}

static int
wl_status(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int ret = 0, wl_mode_x, i;
	WIFI_STA_TABLE *sta_info;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_", *ifname, *op_mode;
	char subunit_str[8];

#if defined(RTCONFIG_WIRELESSREPEATER) && defined(RTCONFIG_PROXYSTA)
	if (mediabridge_mode()) {
		/* Media bridge mode */
		snprintf(prefix, sizeof(prefix), "wl%d.1_", unit);
		ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
		if (unit != nvram_get_int("wlc_band")) {
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			ret += websWrite(wp, "%s radio is disabled\n",
				nvram_match(strcat_r(prefix, "nband", tmp), "1") ? "5 GHz" : "2.4 GHz");
			return ret;
		}
		ret += show_wliface_info(wp, unit, ifname, "Media Bridge", nvram_safe_get("wlc_ssid"));
	} else {
#endif
		/* Router mode, Repeater and AP mode */
#if defined(RTCONFIG_WIRELESSREPEATER)
		if (!unit && repeater_mode()) {
			/* Show P-AP information first, if we are about to show 2.4G information in repeater mode. */
			snprintf(prefix, sizeof(prefix), "wl%d.1_", nvram_get_int("wlc_band"));
			ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
			ret += show_wliface_info(wp, unit, ifname, "Repeater", nvram_safe_get("wlc_ssid"));
			ret += websWrite(wp, "\n");
		}
#endif
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
		if (!get_radio_status(ifname)) {
#if defined(BAND_2G_ONLY)
			ret += websWrite(wp, "2.4 GHz radio is disabled\n");
#else
			ret += websWrite(wp, "%s radio is disabled\n",
				nvram_match(strcat_r(prefix, "nband", tmp), "1") ? "5 GHz" : "2.4 GHz");
#endif
			return ret;
		}

		wl_mode_x = nvram_get_int(strcat_r(prefix, "mode_x", tmp));
		op_mode = "AP";
		if (wl_mode_x == 1)
			op_mode = "WDS Only";
		else if (wl_mode_x == 2)
			op_mode = "Hybrid";
		ret += show_wliface_info(wp, unit, ifname, op_mode, nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
		ret += websWrite(wp, "\nStations List\n");
		ret += websWrite(wp, "----------------------------------------------------------------\n");
#if 0 //barton++
		ret += websWrite(wp, "%-18s%-4s%-8s%-4s%-4s%-4s%-5s%-5s%-12s\n",
				   "MAC", "PSM", "PhyMode", "BW", "MCS", "SGI", "STBC", "Rate", "Connect Time");
#else
		ret += websWrite(wp, "%-3s %-17s %-15s %-6s %-6s %-12s\n",
			"idx", "MAC", "PhyMode", "TXRATE", "RXRATE", "Connect Time");
#endif

		if ((sta_info = malloc(sizeof(*sta_info))) != NULL) {
			getSTAInfo(unit, sta_info);
			for(i = 0; i < sta_info->Num; i++) {
				*subunit_str = '\0';
				if (sta_info->Entry[i].subunit)
					snprintf(subunit_str, sizeof(subunit_str), "%d", sta_info->Entry[i].subunit);
				ret += websWrite(wp, "%3s %-17s %-15s %6s %6s %12s\n",
					subunit_str,
					sta_info->Entry[i].addr,
					sta_info->Entry[i].mode,
					sta_info->Entry[i].txrate,
					sta_info->Entry[i].rxrate,
					sta_info->Entry[i].conn_time
					);
			}
			free(sta_info);
		}
#if defined(RTCONFIG_WIRELESSREPEATER) && defined(RTCONFIG_PROXYSTA)
	}
#endif

	return ret;
}

static int ej_wl_sta_list(int unit, webs_t wp)
{
	WIFI_STA_TABLE *sta_info;
	char *value;
	int firstRow = 1;
	int i;
	int from_app = 0;

	from_app = check_user_agent(user_agent);

	if ((sta_info = malloc(sizeof(*sta_info))) != NULL)
	{
		getSTAInfo(unit, sta_info);
		for(i = 0; i < sta_info->Num; i++)
		{
			if (firstRow == 1)
				firstRow = 0;
			else
				websWrite(wp, ", ");

			if (from_app == 0)
				websWrite(wp, "[");

			websWrite(wp, "\"%s\"", sta_info->Entry[i].addr);

			if (from_app != 0) {
				websWrite(wp, ":{");
				websWrite(wp, "\"isWL\":");
			}

			value = "Yes";
			if (from_app == 0)
				websWrite(wp, ", \"%s\"", value);
			else
				websWrite(wp, "\"%s\"", value);

			value = "";

			if (from_app == 0)
				websWrite(wp, ", \"%s\"", value);
	
			if (from_app != 0) {
				websWrite(wp, ",\"rssi\":");
			}

			if (from_app == 0)
				websWrite(wp, ", \"%d\"", sta_info->Entry[i].rssi);
			else
				websWrite(wp, "\"%d\"", sta_info->Entry[i].rssi);

			if (from_app == 0)
				websWrite(wp, "]");
			else
				websWrite(wp, "}");
		}
		free(sta_info);
	}
	return 0;
}

int ej_wl_sta_list_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	ej_wl_sta_list(0, wp);
	return 0;
}

int ej_wl_sta_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	ej_wl_sta_list(1, wp);
	return 0;
}

#if defined(RTCONFIG_STAINFO)
/**
 * Format:
 * 	[ MAC, TX_RATE, RX_RATE, CONNECT_TIME, IDX ]
 * IDX:	main/GN1/GN2/GN3
 */
static int wl_stainfo_list(int unit, webs_t wp)
{
	WIFI_STA_TABLE *sta_info;
	WLANCONFIG_LIST *r;
	char idx_str[8];
	int i, s, firstRow = 1;

	if ((sta_info = malloc(sizeof(*sta_info))) == NULL)
		return 0 ;

	getSTAInfo(unit, sta_info);
	for(i = 0, r = &sta_info->Entry[0]; i < sta_info->Num; i++, r++) {
		if (firstRow == 1)
			firstRow = 0;
		else
			websWrite(wp, ", ");

		websWrite(wp, "[");
		websWrite(wp, "\"%s\"", r->addr);
		websWrite(wp, ", \"%s\"", r->txrate);
		websWrite(wp, ", \"%s\"", r->rxrate);
		websWrite(wp, ", \"%s\"", r->conn_time);
		s = r->subunit;
		if (s < 0 || s > 3)
			s = 0;
		if (!s)
			strcpy(idx_str, "main");
		else
			sprintf(idx_str, "GN%d", s);
		websWrite(wp, ", \"%s\"", idx_str);
		websWrite(wp, "]");
	}
	free(sta_info);
	return 0;
}

int
ej_wl_stainfo_list_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_stainfo_list(0, wp);
}

int
ej_wl_stainfo_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_stainfo_list(1, wp);
}
#endif	/* RTCONFIG_STAINFO */

char *getWscStatus(int unit)
{
	char buf[512];
	FILE *fp;
	int len;
	char *pt1,*pt2;

	sprintf(buf, "hostapd_cli -i%s wps_get_status", get_wifname(unit));
	fp = popen(buf, "r");
	if (fp) {
		memset(buf, 0, sizeof(buf));
		len = fread(buf, 1, sizeof(buf), fp);
		pclose(fp);
		if (len > 1) {
			buf[len-1] = '\0';
			pt1 = strstr(buf, "Last WPS result: ");
			if (pt1) {
				pt2 = pt1 + strlen("Last WPS result: ");
				pt1 = strstr(pt2, "Peer Address: ");
				if (pt1) {
					*pt1 = '\0';
					chomp(pt2);
				}
				return pt2;
			}
		}
	}
	return "";
}

char *getAPPIN(int unit)
{
	static char buffer[128];
#if 0
	char cmd[64];
	FILE *fp;
	int len;

	buffer[0] = '\0';
	sprintf(cmd, "hostapd_cli -i%s wps_ap_pin get", get_wifname(unit));
	fp = popen(cmd, "r");
	if (fp) {
		len = fread(buffer, 1, sizeof(buffer), fp);
		pclose(fp);
		if (len > 1) {
			buffer[len] = '\0';
			//dbg("%s: AP PIN[%s]\n", __FUNCTION__, buffer);
			if(!strncmp(buffer,"FAIL",4))
			   strcpy(buffer,nvram_get("secret_code"));
			return buffer;
		}
	}
	return "";
#else
	snprintf(buffer, sizeof(buffer), "%s", nvram_safe_get("secret_code"));
	return buffer;
#endif
}

int
wl_wps_info(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int j = -1, u = unit;
	char tmpstr[128];
	WPS_CONFIGURED_VALUE result;
	int retval=0;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *wps_sta_pin;
	char tag1[] = "<wps_infoXXXXXX>", tag2[] = "</wps_infoXXXXXX>";

#if defined(RTCONFIG_WPSMULTIBAND)
	for (j = -1; j < MAX_NR_WL_IF; ++j) {
#endif
		switch (j) {
		case 0: /* fall through */
		case 1:
			u = j;
			sprintf(tag1, "<wps_info%d>", j);
			sprintf(tag2, "</wps_info%d>", j);
			break;
		case -1: /* fall through */
		default:
			u = unit;
			strcpy(tag1, "<wps_info>");
			strcpy(tag2, "</wps_info>");
		}

		snprintf(prefix, sizeof(prefix), "wl%d_", u);

#if defined(RTCONFIG_WPSMULTIBAND)
		if (!nvram_get(strcat_r(prefix, "ifname", tmp)))
			continue;
#endif

		memset(&result, 0, sizeof(result));
		getWPSConfig(u, &result);

		if (j == -1)
			retval += websWrite(wp, "<wps>\n");

		//0. WSC Status
		memset(tmpstr, 0, sizeof(tmpstr));
		strcpy(tmpstr, getWscStatus(u));
		retval += websWrite(wp, "%s%s%s\n", tag1, tmpstr, tag2);

		//1. WPS Configured
		if (result.Configured==2)
			retval += websWrite(wp, "%s%s%s\n", tag1, "Yes", tag2);
		else
			retval += websWrite(wp, "%s%s%s\n", tag1, "No", tag2);

		//2. WPS SSID
		memset(tmpstr, 0, sizeof(tmpstr));
		char_to_ascii(tmpstr, result.SSID);
		retval += websWrite(wp, "%s%s%s\n", tag1, tmpstr, tag2);

		//3. WPS AuthMode
		retval += websWrite(wp, "%s%s%s\n", tag1, result.AuthMode, tag2);

		//4. WPS Encryp
		retval += websWrite(wp, "%s%s%s\n", tag1, result.Encryp, tag2);

		//5. WPS DefaultKeyIdx
		memset(tmpstr, 0, sizeof(tmpstr));
		sprintf(tmpstr, "%d", result.DefaultKeyIdx);/* FIXME: TBD */
		retval += websWrite(wp, "%s%s%s\n", tag1, tmpstr, tag2);

		//6. WPS WPAKey
#if 0	//hide for security
		if (!strlen(result.WPAKey))
			retval += websWrite(wp, "%sNone%s\n", tag1, tag2);
		else
		{
			memset(tmpstr, 0, sizeof(tmpstr));
			char_to_ascii(tmpstr, result.WPAKey);
			retval += websWrite(wp, "%s%s%s\n", tag1, tmpstr, tag2);
		}
#else
		retval += websWrite(wp, "%s%s\n", tag1, tag2);
#endif
		//7. AP PIN Code
		memset(tmpstr, 0, sizeof(tmpstr));
		strcpy(tmpstr, getAPPIN(u));
		retval += websWrite(wp, "%s%s%s\n", tag1, tmpstr, tag2);

		//8. Saved WPAKey
#if 0	//hide for security
		if (!strlen(nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp))))
			retval += websWrite(wp, "%s%s%s\n", tag1, "None", tag2);
		else
		{
			char_to_ascii(tmpstr, nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
			retval += websWrite(wp, "%s%s%s\n", tag1, tmpstr, tag2);
		}
#else
		retval += websWrite(wp, "%s%s\n", tag1, tag2);
#endif
		//9. WPS enable?
		if (!strcmp(nvram_safe_get(strcat_r(prefix, "wps_mode", tmp)), "enabled"))
			retval += websWrite(wp, "%s%s%s\n", tag1, "None", tag2);
		else
			retval += websWrite(wp, "%s%s%s\n", tag1, nvram_safe_get("wps_enable"), tag2);

		//A. WPS mode
		wps_sta_pin = nvram_safe_get("wps_sta_pin");
		if (strlen(wps_sta_pin) && strcmp(wps_sta_pin, "00000000"))
			retval += websWrite(wp, "%s%s%s\n", tag1, "1", tag2);
		else
			retval += websWrite(wp, "%s%s%s\n", tag1, "2", tag2);

		//B. current auth mode
		if (!strlen(nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp))))
			retval += websWrite(wp, "%s%s%s\n", tag1, "None", tag2);
		else
			retval += websWrite(wp, "%s%s%s\n", tag1, nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp)), tag2);

		//C. WPS band
		retval += websWrite(wp, "%s%d%s\n", tag1, u, tag2);
#if defined(RTCONFIG_WPSMULTIBAND)
	}
#endif

	retval += websWrite(wp, "</wps>");

	return retval;
}

int
ej_wps_info(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_wps_info(eid, wp, argc, argv, 1);
}

int
ej_wps_info_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_wps_info(eid, wp, argc, argv, 0);
}

// Wireless Client List		 /* Start --Alicia, 08.09.23 */

int ej_wl_auth_list(int eid, webs_t wp, int argc, char_t **argv)
{
//only for ath0 & ath1
        WLANCONFIG_LIST *result;
	#define AUTH_INFO_PATH "/tmp/auth_athX_list"
	FILE *fp;
	int ret = 0;
	char *p, *ifname;
	char *wl_ifnames;
	char line_buf[300]; // max 14x
	char *value;
	int firstRow;
	result = (WLANCONFIG_LIST *)malloc(sizeof(WLANCONFIG_LIST));
	memset(result, 0, sizeof(result));

	wl_ifnames = strdup(nvram_safe_get("wl_ifnames"));
	if (!wl_ifnames) 
		return ret;
	
	p = wl_ifnames;

	firstRow = 1;
	while ((ifname = strsep(&p, " ")) != NULL) {
		while (*ifname == ' ') ++ifname;
		if (*ifname == 0) break;
		doSystem("wlanconfig %s list > %s", ifname, AUTH_INFO_PATH);
		fp = fopen(AUTH_INFO_PATH, "r");
		if (fp) {
			//fseek(fp, 131, SEEK_SET);	// ignore header
			fgets(line_buf, sizeof(line_buf), fp); // ignore header
			while ( fgets(line_buf, sizeof(line_buf), fp) ) {
				sscanf(line_buf, "%s%u%u%s%s%u%u%u%u%s%s%s%s%s%s%s%s%s", 
							result->addr, 
							&result->aid, 
							&result->chan, 
							result->txrate, 
							result->rxrate, 
							&result->rssi, 
							&result->idle, 
							&result->txseq, 
							&result->rxseq,
							result->caps, 
							result->acaps, 
							result->erp, 
							result->state_maxrate, 
							result->wps, 
							result->conn_time, 
							result->rsn, 
							result->wme,
							result->mode);
				
				if (firstRow == 1)
					firstRow = 0;
				else
					websWrite(wp, ", ");
				websWrite(wp, "[");

				websWrite(wp, "\"%s\"", result->addr);
				value = "YES";
				websWrite(wp, ", \"%s\"", value);
				value = "";
				websWrite(wp, ", \"%s\"", value);
				websWrite(wp, "]");
			
			}

			fclose(fp);
			unlink(AUTH_INFO_PATH);
		}
	}
	free(result);
	free(wl_ifnames);
	return ret;
}

#if 0
static void convertToUpper(char *str)
{
	if(str == NULL)
		return;
	while(*str)
	{
		if(*str >= 'a' && *str <= 'z')
		{
			*str &= (unsigned char)~0x20;
		}
		str++;
	}
}
#endif

#if 1
#define target 7
char str[target][40]={"Address:","ESSID:","Frequency:","Quality=","Encryption key:","IE:","Authentication Suites"};
static int wl_scan(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
   	int apCount=0,retval=0;
	char header[128];
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char cmd[300];
	FILE *fp;
	char buf[target][200];
	int i,fp_len;
	char *pt1,*pt2;
	char a1[10],a2[10];
	char ssid_str[256];
	char ch[4] = "", ssid[33] = "", address[18] = "", enc[9] = "";
	char auth[16] = "", sig[9] = "", wmode[8] = "";
	int  lock;

	dbg("Please wait...");
	lock = file_lock("nvramcommit");
	system("rm -f /tmp/wlist");
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	sprintf(cmd,"iwlist %s scanning >> /tmp/wlist",nvram_safe_get(strcat_r(prefix, "ifname", tmp)));
	system(cmd);
	file_unlock(lock);
	
	if((fp= fopen("/tmp/wlist", "r"))==NULL) 
	   return -1;
	
	memset(header, 0, sizeof(header));
	sprintf(header, "%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode");

	dbg("\n%s", header);

	retval += websWrite(wp, "[");
	while(1)
	{
		memset(buf,0,sizeof(buf));
		fp_len=0;
		for(i=0;i<target;i++)
		{
		   	while(fgets(buf[i], sizeof(buf[i]), fp))
			{
				fp_len += strlen(buf[i]);  	
				if(i!=0 && strstr(buf[i],"Cell") && strstr(buf[i],"Address"))
				{
					fseek(fp,-fp_len, SEEK_CUR);
					fp_len=0;
					break;
				}
				else
			  	{ 	   
					if(strstr(buf[i],str[i]))
					{
					 	fp_len =0;  	
						break;
					}	
					else
						memset(buf[i],0,sizeof(buf[i]));
				}	

			}
		        	
	      		//dbg("buf[%d]=%s\n",i,buf[i]);
		}

  		if(feof(fp)) 
		   break;

		apCount++;

		dbg("\napCount=%d\n",apCount);
		//ch
	        pt1 = strstr(buf[2], "Channel ");	
		if(pt1)
		{

			pt2 = strstr(pt1,")");
		   	memset(ch,0,sizeof(ch));
			strncpy(ch,pt1+strlen("Channel "),pt2-pt1-strlen("Channel "));
		}   

		//ssid
	        pt1 = strstr(buf[1], "ESSID:");	
		if(pt1)
		{
		   	memset(ssid,0,sizeof(ssid));
			strncpy(ssid,pt1+strlen("ESSID:")+1,strlen(buf[1])-2-(pt1+strlen("ESSID:")+1-buf[1]));
		}   


		//bssid
	        pt1 = strstr(buf[0], "Address: ");	
		if(pt1)
		{
		   	memset(address,0,sizeof(address));
			strncpy(address,pt1+strlen("Address: "),strlen(buf[0])-(pt1+strlen("Address: ")-buf[0])-1);
		}   
	

		//enc
		pt1=strstr(buf[4],"Encryption key:");
		if(pt1)
		{   
			if(strstr(pt1+strlen("Encryption key:"),"on"))
			{  	
				sprintf(enc,"ENC");
		
			} 
			else
				sprintf(enc,"NONE");
		}

		//auth
		memset(auth,0,sizeof(auth));
		sprintf(auth,"N/A");    

		//sig
	        pt1 = strstr(buf[3], "Quality=");	
		pt2 = NULL;
		if (pt1 != NULL)
			pt2 = strstr(pt1,"/");
		if(pt1 && pt2)
		{
			memset(sig,0,sizeof(sig));
			memset(a1,0,sizeof(a1));
			memset(a2,0,sizeof(a2));
			strncpy(a1,pt1+strlen("Quality="),pt2-pt1-strlen("Quality="));
			strncpy(a2,pt2+1,strstr(pt2," ")-(pt2+1));
			sprintf(sig,"%d",atoi(a1)/atoi(a2));

		}   

		//wmode
		memset(wmode,0,sizeof(wmode));
		sprintf(wmode,"11b/g/n");   


#if 1
		dbg("%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n",ch,ssid,address,enc,auth,sig,wmode);
#endif	


		memset(ssid_str, 0, sizeof(ssid_str));
		char_to_ascii(ssid_str, trim_r(ssid));
		if (apCount==1)
			retval += websWrite(wp, "[\"%s\", \"%s\"]", ssid_str, address);
		else
			retval += websWrite(wp, ", [\"%s\", \"%s\"]", ssid_str, address);

	}

	retval += websWrite(wp, "]");
	fclose(fp);
	return 0;
}   
#else
static int wl_scan(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int retval = 0, i = 0, apCount = 0;
	char data[8192];
	char ssid_str[256];
	char header[128];
	struct iwreq wrq;
	SSA *ssap;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	int lock;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	memset(data, 0x00, 255);
	strcpy(data, "SiteSurvey=1"); 
	wrq.u.data.length = strlen(data)+1; 
	wrq.u.data.pointer = data; 
	wrq.u.data.flags = 0; 

	lock = file_lock("nvramcommit");
	if (wl_ioctl(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), RTPRIV_IOCTL_SET, &wrq) < 0)
	{
		file_unlock(lock);
		dbg("Site Survey fails\n");
		return 0;
	}
	file_unlock(lock);
	dbg("Please wait");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".");
	sleep(1);
	dbg(".\n\n");
	memset(data, 0, 8192);
	strcpy(data, "");
	wrq.u.data.length = 8192;
	wrq.u.data.pointer = data;
	wrq.u.data.flags = 0;
	if (wl_ioctl(nvram_safe_get(strcat_r(prefix, "ifname", tmp)), RTPRIV_IOCTL_GSITESURVEY, &wrq) < 0)
	{
		dbg("errors in getting site survey result\n");
		return 0;
	}
	memset(header, 0, sizeof(header));
	//sprintf(header, "%-3s%-33s%-18s%-8s%-15s%-9s%-8s%-2s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode", "NT");
#if 0// defined(RTN14U)
	sprintf(header, "%-4s%-33s%-18s%-9s%-16s%-9s%-8s%-4s%-5s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode"," WPS", " DPID");
#else
	sprintf(header, "%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n", "Ch", "SSID", "BSSID", "Enc", "Auth", "Siganl(%)", "W-Mode");
#endif
	dbg("\n%s", header);
	if (wrq.u.data.length > 0)
	{
#if defined(RTN65U)
		if (unit == 0 && get_model() == MODEL_RTN65U)
		{
			char *encryption;
			SITE_SURVEY_RT3352_iNIC *pSsap, *ssAP;

			pSsap = ssAP = (SITE_SURVEY_RT3352_iNIC *) (1 /* '\n' */ + wrq.u.data.pointer +  sizeof(SITE_SURVEY_RT3352_iNIC) /* header */);
			while(((unsigned int)wrq.u.data.pointer + wrq.u.data.length) > (unsigned int) ssAP)
			{
				ssAP->channel   [sizeof(ssAP->channel)    -1] = '\0';
				ssAP->ssid      [32                         ] = '\0';
				ssAP->bssid     [17                         ] = '\0';
				ssAP->encryption[sizeof(ssAP->encryption) -1] = '\0';
				if((encryption = strchr(ssAP->authmode, '/')) != NULL)
				{
					memmove(ssAP->encryption, encryption +1, sizeof(ssAP->encryption) -1);
					memset(encryption, ' ', sizeof(ssAP->authmode) - (encryption - ssAP->authmode));
					*encryption = '\0';
				}
				ssAP->authmode  [sizeof(ssAP->authmode)   -1] = '\0';
				ssAP->signal    [sizeof(ssAP->signal)     -1] = '\0';
				ssAP->wmode     [sizeof(ssAP->wmode)      -1] = '\0';
				ssAP->extch     [sizeof(ssAP->extch)      -1] = '\0';
				ssAP->nt        [sizeof(ssAP->nt)         -1] = '\0';
				ssAP->wps       [sizeof(ssAP->wps)        -1] = '\0';
				ssAP->dpid      [sizeof(ssAP->dpid)       -1] = '\0';

				convertToUpper(ssAP->bssid);
				ssAP++;
				apCount++;
			}

			if (apCount)
			{
				retval += websWrite(wp, "[");
				for (i = 0; i < apCount; i++)
				{
					dbg("%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n",
						pSsap[i].channel,
						pSsap[i].ssid,
						pSsap[i].bssid,
						pSsap[i].encryption,
						pSsap[i].authmode,
						pSsap[i].signal,
						pSsap[i].wmode
					);

					memset(ssid_str, 0, sizeof(ssid_str));
					char_to_ascii(ssid_str, trim_r(pSsap[i].ssid));

					if (!i)
						retval += websWrite(wp, "[\"%s\", \"%s\"]", ssid_str, pSsap[i].bssid);
					else
						retval += websWrite(wp, ", [\"%s\", \"%s\"]", ssid_str, pSsap[i].bssid);
				}
				retval += websWrite(wp, "]");
				dbg("\n");
			}
			else
				retval += websWrite(wp, "[]");
			return retval;
		}
#endif
		ssap=(SSA *)(wrq.u.data.pointer+strlen(header)+1);
		int len = strlen(wrq.u.data.pointer+strlen(header))-1;
		char *sp, *op;
 		op = sp = wrq.u.data.pointer+strlen(header)+1;
		while (*sp && ((len - (sp-op)) >= 0))
		{
			ssap->SiteSurvey[i].channel[3] = '\0';
			ssap->SiteSurvey[i].ssid[32] = '\0';
			ssap->SiteSurvey[i].bssid[17] = '\0';
			ssap->SiteSurvey[i].encryption[8] = '\0';
			ssap->SiteSurvey[i].authmode[15] = '\0';
			ssap->SiteSurvey[i].signal[8] = '\0';
			ssap->SiteSurvey[i].wmode[7] = '\0';
#if 0//defined(RTN14U)
			ssap->SiteSurvey[i].wps[3] = '\0';
			ssap->SiteSurvey[i].dpid[4] = '\0';
#endif
			sp+=strlen(header);
			apCount=++i;
		}
		if (apCount)
		{
			retval += websWrite(wp, "[");
			for (i = 0; i < apCount; i++)
			{
			   	dbg("\napCount=%d\n",i);
				dbg(
#if 0//defined(RTN14U)
				"%-4s%-33s%-18s%-9s%-16s%-9s%-8s%-4s%-5s\n",
#else
				"%-4s%-33s%-18s%-9s%-16s%-9s%-8s\n",
#endif
					ssap->SiteSurvey[i].channel,
					(char*)ssap->SiteSurvey[i].ssid,
					ssap->SiteSurvey[i].bssid,
					ssap->SiteSurvey[i].encryption,
					ssap->SiteSurvey[i].authmode,
					ssap->SiteSurvey[i].signal,
					ssap->SiteSurvey[i].wmode
#if 0//defined(RTN14U)
					, ssap->SiteSurvey[i].wps
					, ssap->SiteSurvey[i].dpid
#endif
				);

				memset(ssid_str, 0, sizeof(ssid_str));
				char_to_ascii(ssid_str, trim_r(ssap->SiteSurvey[i].ssid));

				if (!i)
//					retval += websWrite(wp, "\"%s\"", ssap->SiteSurvey[i].bssid);
					retval += websWrite(wp, "[\"%s\", \"%s\"]", ssid_str, ssap->SiteSurvey[i].bssid);
				else
//					retval += websWrite(wp, ", \"%s\"", ssap->SiteSurvey[i].bssid);
					retval += websWrite(wp, ", [\"%s\", \"%s\"]", ssid_str, ssap->SiteSurvey[i].bssid);
			}
			retval += websWrite(wp, "]");
			dbg("\n");
		}
		else
			retval += websWrite(wp, "[]");
	}
	return retval;
}

#endif
int
ej_wl_scan(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 0);
}

int
ej_wl_scan_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 0);
}

int
ej_wl_scan_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	return wl_scan(eid, wp, argc, argv, 1);
}


static int ej_wl_channel_list(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
	int retval = 0;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_";
	char *country_code;
	char chList[256];
	int band;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	country_code = nvram_get(strcat_r(prefix, "country_code", tmp));
	band = unit;

	if (country_code == NULL || strlen(country_code) != 2) return retval;

	if (band != 0 && band != 1) return retval;

	//try getting channel list via wifi driver first
#if defined(RTAC58U)
	if (band == 0 && !strncmp(nvram_safe_get("territory_code"), "CX", 2))
		retval += websWrite(wp, "[1,2,3,4,5,6,7,8,9,10,11]");
	else
#endif
	if(get_channel_list_via_driver(unit, chList, sizeof(chList)) > 0)
	{
		retval += websWrite(wp, "[%s]", chList);
	}
	else if(get_channel_list_via_country(unit, country_code, chList, sizeof(chList)) > 0)
	{
		retval += websWrite(wp, "[%s]", chList);
	}
	return retval;
}


int
ej_wl_channel_list_2g(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_channel_list(eid, wp, argc, argv, 0);
}

int
ej_wl_channel_list_5g(int eid, webs_t wp, int argc, char_t **argv)
{
	return ej_wl_channel_list(eid, wp, argc, argv, 1);
}


static int ej_wl_rate(int eid, webs_t wp, int argc, char_t **argv, int unit)
{
#define ASUS_IOCTL_GET_STA_DATARATE (SIOCDEVPRIVATE+15) /* from qca-wifi/os/linux/include/ieee80211_ioctl.h */
        struct iwreq wrq;
	int retval = 0;
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	char *name;
	char word[256], *next;
	int unit_max = 0;
	unsigned int rate[2];
	char rate_buf[32];
	int sw_mode = nvram_get_int("sw_mode");
	int wlc_band = nvram_get_int("wlc_band");

	sprintf(rate_buf, "0 Mbps");

	if (!nvram_match("wlc_state", "2"))
		goto ERROR;

	foreach (word, nvram_safe_get("wl_ifnames"), next)
		unit_max++;

	if (unit > (unit_max - 1))
		goto ERROR;

	if (wlc_band == unit && (sw_mode == SW_MODE_REPEATER || sw_mode == SW_MODE_HOTSPOT))
		snprintf(prefix, sizeof(prefix), "wl%d.1_", unit);
	else
		goto ERROR;
	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	wrq.u.data.pointer = rate;
	wrq.u.data.length = sizeof(rate);

	if (wl_ioctl(name, ASUS_IOCTL_GET_STA_DATARATE, &wrq) < 0)
	{
		dbg("%s: errors in getting %s ASUS_IOCTL_GET_STA_DATARATE result\n", __func__, name);
		goto ERROR;
	}

	if (rate[0] > rate[1])
		sprintf(rate_buf, "%d Mbps", rate[0]);
	else
		sprintf(rate_buf, "%d Mbps", rate[1]);

ERROR:
	retval += websWrite(wp, "%s", rate_buf);
	return retval;
}


int
ej_wl_rate_2g(int eid, webs_t wp, int argc, char_t **argv)
{
   	if(nvram_match("sw_mode", "2"))
		return ej_wl_rate(eid, wp, argc, argv, 0);
	else
	   	return 0;
}

int
ej_wl_rate_5g(int eid, webs_t wp, int argc, char_t **argv)
{
   	if(nvram_match("sw_mode", "2"))
		return ej_wl_rate(eid, wp, argc, argv, 1);
	else
	   	return 0;
}

#ifdef RTCONFIG_PROXYSTA
int
ej_wl_auth_psta(int eid, webs_t wp, int argc, char_t **argv)
{
	int retval = 0;

	if(nvram_match("wlc_state", "2"))	//connected
		retval += websWrite(wp, "wlc_state=1;wlc_state_auth=0;");
	//else if(?)				//authorization failed
	//	retval += websWrite(wp, "wlc_state=2;wlc_state_auth=1;");
	else					//disconnected
		retval += websWrite(wp, "wlc_state=0;wlc_state_auth=0;");

	return retval;
}
#endif
