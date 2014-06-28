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
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <wlutils.h>
#include <shutils.h>
#include <shared.h>
#include <wlioctl.h>
#ifdef PSTA_DEBUG
#include <rc.h>
#endif

#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA

#define NORMAL_PERIOD   30
#define	MAX_STA_COUNT	128
#define	NVRAM_BUFSIZE	100

int psta_debug = 0;
int count_bss_down = 0;
#ifdef PSTA_DEBUG
static int count = -1;
#endif

/* WPS ENR mode APIs */
typedef struct wlc_ap_list_info
{
	bool	used;
	uint8	ssid[33];
	uint8	ssidLen;
	uint8	BSSID[6];
	uint8	*ie_buf;
	uint32	ie_buflen;
	uint8	channel;
	uint8	wep;
} wlc_ap_list_info_t;

#define WLC_DUMP_BUF_LEN		(32 * 1024)
#define WLC_MAX_AP_SCAN_LIST_LEN	50
#define WLC_SCAN_RETRY_TIMES		5
#define NUMCHANS			64
#define MAX_SSID_LEN			32

static wlc_ap_list_info_t ap_list[WLC_MAX_AP_SCAN_LIST_LEN];
static char scan_result[WLC_DUMP_BUF_LEN];

/* The below macro handle endian mis-matches between wl utility and wl driver. */
static bool g_swap = FALSE;
#define dtoh32(i) (g_swap?bcmswap32(i):(uint32)(i))

static char *
wl_get_scan_results(int unit)
{
	int ret, retry_times = 0;
	wl_scan_params_t *params;
	wl_scan_results_t *list = (wl_scan_results_t*)scan_result;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + NUMCHANS * sizeof(uint16);
	wlc_ssid_t wst = {0, ""};
	int org_scan_time = 20, scan_time = 40;
	char tmp[NVRAM_BUFSIZE], prefix[] = "wlXXXXXXXXXX_";
	char *ifname = NULL;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	wst.SSID_len = strlen(nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
	if (wst.SSID_len <= MAX_SSID_LEN)
		memcpy(wst.SSID, nvram_safe_get(strcat_r(prefix, "ssid", tmp)), wst.SSID_len);
	else
		wst.SSID_len = 0;

	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL) {
		return NULL;
	}

	memset(params, 0, params_size);
	params->ssid = wst;
	params->bss_type = DOT11_BSSTYPE_ANY;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->scan_type = -1;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	/* extend scan channel time to get more AP probe resp */
	wl_ioctl(ifname, WLC_GET_SCAN_CHANNEL_TIME, &org_scan_time, sizeof(org_scan_time));
	if (org_scan_time < scan_time)
		wl_ioctl(ifname, WLC_SET_SCAN_CHANNEL_TIME, &scan_time,	sizeof(scan_time));

retry:
	ret = wl_ioctl(ifname, WLC_SCAN, params, params_size);
	if (ret < 0) {
		if (retry_times++ < WLC_SCAN_RETRY_TIMES) {
			if (psta_debug)
			dbg("set scan command failed, retry %d\n", retry_times);
			sleep(1);
			goto retry;
		}
	}

	sleep(2);

	list->buflen = WLC_DUMP_BUF_LEN;
	ret = wl_ioctl(ifname, WLC_SCAN_RESULTS, scan_result, WLC_DUMP_BUF_LEN);
	if (ret < 0 && retry_times++ < WLC_SCAN_RETRY_TIMES) {
		if (psta_debug)
		dbg("get scan result failed, retry %d\n", retry_times);
		sleep(1);
		goto retry;
	}

	free(params);

	/* restore original scan channel time */
	wl_ioctl(ifname, WLC_SET_SCAN_CHANNEL_TIME, &org_scan_time, sizeof(org_scan_time));

	if (ret < 0)
		return NULL;

	return scan_result;
}

static int
wl_scan(int unit)
{
	wl_scan_results_t *list = (wl_scan_results_t*)scan_result;
	wl_bss_info_t *bi;
	wl_bss_info_107_t *old_bi;
	uint i, ap_count = 0;
	char ssid_str[128], macstr[18], tmp[NVRAM_BUFSIZE];

	if (wl_get_scan_results(unit) == NULL)
		return 0;

	memset(ap_list, 0, sizeof(ap_list));
	if (list->count == 0)
		return 0;
	else if (list->version != WL_BSS_INFO_VERSION &&
			list->version != LEGACY_WL_BSS_INFO_VERSION &&
			list->version != LEGACY2_WL_BSS_INFO_VERSION) {
		/* fprintf(stderr, "Sorry, your driver has bss_info_version %d "
		    "but this program supports only version %d.\n",
		    list->version, WL_BSS_INFO_VERSION); */
		return 0;
	}

	bi = list->bss_info;
	for (i = 0; i < list->count; i++) {
		/* Convert version 107 to 109 */
		if (dtoh32(bi->version) == LEGACY_WL_BSS_INFO_VERSION) {
			old_bi = (wl_bss_info_107_t *)bi;
			bi->chanspec = CH20MHZ_CHSPEC(old_bi->channel);
			bi->ie_length = old_bi->ie_length;
			bi->ie_offset = sizeof(wl_bss_info_107_t);
		}

		if (bi->ie_length) {
			if (ap_count < WLC_MAX_AP_SCAN_LIST_LEN){
				ap_list[ap_count].used = TRUE;
				memcpy(ap_list[ap_count].BSSID, (uint8 *)&bi->BSSID, 6);
				strncpy((char *)ap_list[ap_count].ssid, (char *)bi->SSID, bi->SSID_len);
				ap_list[ap_count].ssid[bi->SSID_len] = '\0';
				ap_list[ap_count].ssidLen= bi->SSID_len;
				ap_list[ap_count].ie_buf = (uint8 *)(((uint8 *)bi) + bi->ie_offset);
				ap_list[ap_count].ie_buflen = bi->ie_length;
				ap_list[ap_count].channel = (uint8)(bi->chanspec & WL_CHANSPEC_CHAN_MASK);
				ap_list[ap_count].wep = bi->capability & DOT11_CAP_PRIVACY;
				ap_count++;
			}
		}
		bi = (wl_bss_info_t*)((int8*)bi + bi->length);
	}

	if (ap_count)
	{
		if (psta_debug)
		dbg("%-4s%-33s%-18s\n", "Ch", "SSID", "BSSID");

		for (i = 0; i < ap_count; i++)
		{
			memset(ssid_str, 0, sizeof(ssid_str));
			char_to_ascii(ssid_str, trim_r(ap_list[i].ssid));

			ether_etoa(&ap_list[i].BSSID, macstr);
			if (psta_debug)
			dbg("%-4d%-33s%-18s\n",
				ap_list[i].channel,
				ap_list[i].ssid,
				macstr
			);
		}
	}

	return ap_count;
}

static struct itimerval itv;
static void
alarmtimer(unsigned long sec, unsigned long usec)
{
	itv.it_value.tv_sec  = sec;
	itv.it_value.tv_usec = usec;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}

static int
wl_autho(char *name, struct ether_addr *ea)
{
	char buf[sizeof(sta_info_t)];

	strcpy(buf, "sta_info");
	memcpy(buf + strlen(buf) + 1, (unsigned char *)ea, ETHER_ADDR_LEN);

	if (!wl_ioctl(name, WLC_GET_VAR, buf, sizeof(buf))) {
		sta_info_t *sta = (sta_info_t *)buf;
		uint32 f = sta->flags;

		if (f & WL_STA_AUTHO)
			return 1;
	}

	return 0;
}
#ifdef PSTA_DEBUG
static void check_wl_rate(const char *ifname)
{
	int rate = 0;
	char rate_buf[32];

	sprintf(rate_buf, "0 Mbps");

        if (wl_ioctl(ifname, WLC_GET_RATE, &rate, sizeof(int)))
	{
                dbG("can not get rate info of %s\n", ifname);
		goto ERROR;
	}
        else
        {
                rate = dtoh32(rate);
		if ((rate == -1) || (rate == 0))
			sprintf(rate_buf, "auto");
		else
			sprintf(rate_buf, "%d%s Mbps", (rate / 2), (rate & 1) ? ".5" : "");
        }

ERROR:
	logmessage(LOGNAME, "wl interface %s data rate: %s", ifname, rate_buf);
}
#endif
static void
psta_keepalive()
{
	char tmp[NVRAM_BUFSIZE], prefix[] = "wlXXXXXXXXXX_";
	char *name = NULL;
	struct maclist *mac_list = NULL;
	int mac_list_size, i, unit;
	int psta = 0;
	struct ether_addr bssid;
	unsigned char bssid_null[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
	char macaddr[18];

	unit = nvram_get_int("wlc_band");
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	if (!nvram_match(strcat_r(prefix, "mode", tmp), "psta"))
		goto PSTA_ERR;

	name = nvram_safe_get(strcat_r(prefix, "ifname", tmp));

	if (wl_ioctl(name, WLC_GET_BSSID, &bssid, ETHER_ADDR_LEN) != 0)
		goto PSTA_ERR;
	else if (!memcmp(&bssid, bssid_null, 6))
		goto PSTA_ERR;

	/* buffers and length */
	mac_list_size = sizeof(mac_list->count) + MAX_STA_COUNT * sizeof(struct ether_addr);
	mac_list = malloc(mac_list_size);

	if (!mac_list)
		goto PSTA_ERR;

	/* query wl for authenticated sta list */
	strcpy((char*)mac_list, "authe_sta_list");
	if (wl_ioctl(name, WLC_GET_VAR, mac_list, mac_list_size)) {
		free(mac_list);
		goto PSTA_ERR;
	}

	/* query sta_info for each STA and output one table row each */
	if (mac_list->count)
	{
		if (nvram_match(strcat_r(prefix, "akm", tmp), ""))
			psta = 1;
		else
		for (i = 0; i < mac_list->count; i++) {
			if (wl_autho(name, &mac_list->ea[i]))
			{
				psta = 1;
				break;
			}
		}
	}

PSTA_ERR:
	if (psta)
	{
		count_bss_down = 0;
		ether_etoa(&bssid, macaddr);
		if (psta_debug) dbg("psta send keepalive nulldata to %s\n", macaddr);
		eval("wl", "-i", name, "send_nulldata", macaddr);
#ifdef PSTA_DEBUG
		count = (count + 1) % 10;
		if (!count) check_wl_rate(name);
#endif
	}
	else
	{
		if (psta_debug) dbg("psta disconnected\n");
		if (++count_bss_down > 9)
		{
			count_bss_down = 0;
			if (wl_scan(unit))
			{
				eval("wlconf", name, "down");
				eval("wlconf", name, "up");
				eval("wlconf", name, "start");
			}
		}
		else
		{
			eval("wl", "-i", name, "bss", "down");
			eval("wl", "-i", name, "down");
			eval("wl", "-i", name, "up");
			eval("wl", "-i", name, "bss", "up");
		}
	}

	if (mac_list) free(mac_list);
}

static void
psta_monitor(int sig)
{
	if (sig == SIGALRM)
	{
		psta_keepalive();
		alarm(NORMAL_PERIOD);
	}
}

static void
psta_monitor_exit(int sig)
{
	if (sig == SIGTERM)
	{
        	alarmtimer(0, 0);
        	remove("/var/run/psta_monitor.pid");
        	exit(0);
        }
}

int 
psta_monitor_main(int argc, char *argv[])
{
	FILE *fp;
	sigset_t sigs_to_catch;
	if (!is_psta(0) && !is_psta(1))
		return 0;
#ifdef RTCONFIG_QTN
	if (nvram_get_int("wlc_band") == 1)
		return 0;
#endif
	/* write pid */
	if ((fp = fopen("/var/run/psta_monitor.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	if (nvram_match("psta_debug", "1"))
		psta_debug = 1;

	/* set the signal handler */
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

	signal(SIGALRM, psta_monitor);
	signal(SIGTERM, psta_monitor_exit);

	alarm(NORMAL_PERIOD);

	/* Most of time it goes to sleep */
	while (1)
	{
		pause();
	}

	return 0;
}
#endif
#endif
