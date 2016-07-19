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
#include <unistd.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <wlutils.h>
#include <shutils.h>
#include <shared.h>
#include <wlioctl.h>
#include <rc.h>

#include <wlscan.h>
#include <bcmendian.h>
#ifdef RTCONFIG_BCM_7114
#include <bcmutils.h>
#include <security_ipc.h>
#endif

#ifdef RTCONFIG_BCMWL6
#ifdef RTCONFIG_PROXYSTA

#define NORMAL_PERIOD	30
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
#if 0
	bool	used;
#endif
	uint8	ssid[33];
	uint8	ssidLen;
	uint8	BSSID[6];
#if 0
	uint8	*ie_buf;
	uint32	ie_buflen;
#endif
	uint8	channel;
#if 0
	uint8	wep;
#endif
} wlc_ap_list_info_t;

#define WLC_SCAN_RETRY_TIMES		5
#define NUMCHANS			64
#define MAX_SSID_LEN			32

static wlc_ap_list_info_t ap_list[MAX_NUMBER_OF_APINFO];
static char scan_result[WLC_SCAN_RESULT_BUF_LEN];

/* The below macro handle endian mis-matches between wl utility and wl driver. */
static bool g_swap = FALSE;
#define htod32(i) (g_swap?bcmswap32(i):(uint32)(i))
#define dtoh32(i) (g_swap?bcmswap32(i):(uint32)(i))

#ifdef RTCONFIG_BCM_7114
typedef struct escan_wksp_s {
	uint8 packet[4096];
	int event_fd;
} escan_wksp_t;

static escan_wksp_t *d_info;

/* open a UDP packet to event dispatcher for receiving/sending data */
static int
escan_open_eventfd()
{
	int reuse = 1;
	struct sockaddr_in sockaddr;
	int fd = -1;

	d_info->event_fd = -1;

	/* open loopback socket to communicate with event dispatcher */
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sockaddr.sin_port = htons(EAPD_WKSP_DCS_UDP_SPORT);

	if ((fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		dbg("Unable to create loopback socket\n");
		goto exit;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
		dbg("Unable to setsockopt to loopback socket %d.\n", fd);
		goto exit;
	}

	if (bind(fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
		dbg("Unable to bind to loopback socket %d\n", fd);
		goto exit;
	}

	d_info->event_fd = fd;

	return 0;

	/* error handling */
exit:
	if (fd != -1) {
		close(fd);
	}

	return errno;
}

static bool escan_swap = FALSE;
#define htod16(i) (escan_swap?bcmswap16(i):(uint16)(i))
#define WL_EVENT_TIMEOUT 10

struct escan_bss {
	struct escan_bss *next;
	wl_bss_info_t bss[1];
};
#define ESCAN_BSS_FIXED_SIZE 4

/* listen to sockets and receive escan results */
static int
get_scan_escan(char *scan_buf, uint buf_len)
{
	fd_set fdset;
	int fd;
	struct timeval tv;
	uint8 *pkt;
	int len;
	int retval;
	wl_escan_result_t *escan_data;
	struct escan_bss *escan_bss_head = NULL;
	struct escan_bss *escan_bss_tail = NULL;
	struct escan_bss *result;

	d_info = (escan_wksp_t*)malloc(sizeof(escan_wksp_t));

	escan_open_eventfd();

	if (d_info->event_fd == -1) {
		return -1;
	}

	fd = d_info->event_fd;

	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);

	pkt = d_info->packet;
	len = sizeof(d_info->packet);

	tv.tv_sec = WL_EVENT_TIMEOUT;
	tv.tv_usec = 0;

	/* listen to data availible on all sockets */
	while ((retval = select(fd+1, &fdset, NULL, NULL, &tv)) > 0) {
		bcm_event_t *pvt_data;
		uint32 evt_type;
		uint32 status;

		if (recv(fd, pkt, len, 0) <= 0)
			continue;

		pvt_data = (bcm_event_t *)(pkt + IFNAMSIZ);
		evt_type = ntoh32(pvt_data->event.event_type);

		if (evt_type == WLC_E_ESCAN_RESULT) {
			escan_data = (wl_escan_result_t*)(pvt_data + 1);
			status = ntoh32(pvt_data->event.status);

			if (status == WLC_E_STATUS_PARTIAL) {
				wl_bss_info_t *bi = &escan_data->bss_info[0];
				wl_bss_info_t *bss = NULL;

				/* check if we've received info of same BSSID */
				for (result = escan_bss_head; result; result = result->next) {
					bss = result->bss;

					if (!memcmp(bi->BSSID.octet, bss->BSSID.octet,
						ETHER_ADDR_LEN) &&
						CHSPEC_BAND(bi->chanspec) ==
						CHSPEC_BAND(bss->chanspec) &&
						bi->SSID_len == bss->SSID_len &&
						!memcmp(bi->SSID, bss->SSID, bi->SSID_len))
						break;
					}

				if (!result) {
					/* New BSS. Allocate memory and save it */
					struct escan_bss *ebss = (struct escan_bss *)malloc(
						OFFSETOF(struct escan_bss, bss)	+ bi->length);

					if (!ebss) {
						dbg("can't allocate memory for bss");
						goto exit;
					}

					ebss->next = NULL;
					memcpy(&ebss->bss, bi, bi->length);
					if (escan_bss_tail) {
						escan_bss_tail->next = ebss;
					}
					else {
						escan_bss_head = ebss;
					}
					escan_bss_tail = ebss;
				}
				else if (bi->RSSI != WLC_RSSI_INVALID) {
					/* We've got this BSS. Update rssi if necessary */
					if (((bss->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) ==
						(bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL)) &&
					    ((bss->RSSI == WLC_RSSI_INVALID) ||
						(bss->RSSI < bi->RSSI))) {
						/* preserve max RSSI if the measurements are
						 * both on-channel or both off-channel
						 */
						bss->RSSI = bi->RSSI;
						bss->SNR = bi->SNR;
						bss->phy_noise = bi->phy_noise;
					} else if ((bi->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) &&
						(bss->flags & WL_BSS_FLAGS_RSSI_ONCHANNEL) == 0) {
						/* preserve the on-channel rssi measurement
						 * if the new measurement is off channel
						*/
						bss->RSSI = bi->RSSI;
						bss->SNR = bi->SNR;
						bss->phy_noise = bi->phy_noise;
						bss->flags |= WL_BSS_FLAGS_RSSI_ONCHANNEL;
					}
				}
			}
			else if (status == WLC_E_STATUS_SUCCESS) {
				/* Escan finished. Let's go dump the results. */
				break;
			}
			else {
				dbg("sync_id: %d, status:%d, misc. error/abort\n",
					escan_data->sync_id, status);
				goto exit;
			}
		}
	}

	if (retval > 0) {
		wl_scan_results_t* s_result = (wl_scan_results_t*)scan_buf;
		wl_bss_info_t *bi = s_result->bss_info;
		wl_bss_info_t *bss;

		s_result->count = 0;
		len = buf_len - WL_SCAN_RESULTS_FIXED_SIZE;

		for (result = escan_bss_head; result; result = result->next) {
			bss = result->bss;
			if (buf_len < bss->length) {
				dbg("Memory not enough for scan results\n");
				break;
			}
			memcpy(bi, bss, bss->length);
			bi = (wl_bss_info_t*)((int8*)bi + bss->length);
			len -= bss->length;
			s_result->count++;
		}
	} else if (retval == 0) {
		dbg("Scan timeout!\n");
	} else {
		dbg("Receive scan results failed!\n");
	}

exit:
	if (d_info) {
		if (d_info->event_fd != -1) {
			close(d_info->event_fd);
			d_info->event_fd = -1;
		}

		free(d_info);
	}

	/* free scan results */
	result = escan_bss_head;
	while (result) {
		struct escan_bss *tmp = result->next;
		free(result);
		result = tmp;
	}

	return (retval > 0) ? BCME_OK : BCME_ERROR;
}

static char *
wl_get_scan_results_escan(int unit)
{
	int ret, retry_times = 0;
	wl_escan_params_t *params = NULL;
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

	params = (wl_escan_params_t*)malloc(params_size);
	if (params == NULL) {
		return NULL;
	}

	memset(params, 0, params_size);
	params->params.ssid = wst;
	params->params.bss_type = DOT11_BSSTYPE_ANY;
	memcpy(&params->params.bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->params.scan_type = -1;
	params->params.nprobes = -1;
	params->params.active_time = -1;
	params->params.passive_time = -1;
	params->params.home_time = -1;
	params->params.channel_num = 0;

	params->version = htod32(ESCAN_REQ_VERSION);
	params->action = htod16(WL_SCAN_ACTION_START);

	srand((unsigned int)uptime());
	params->sync_id = htod16(rand() & 0xffff);

	params_size += OFFSETOF(wl_escan_params_t, params);

	ifname = nvram_safe_get(strcat_r(prefix, "ifname", tmp));
	/* extend scan channel time to get more AP probe resp */
	wl_ioctl(ifname, WLC_GET_SCAN_CHANNEL_TIME, &org_scan_time, sizeof(org_scan_time));
	if (org_scan_time < scan_time)
		wl_ioctl(ifname, WLC_SET_SCAN_CHANNEL_TIME, &scan_time,	sizeof(scan_time));

retry:
	ret = wl_iovar_set(ifname, "escan", params, params_size);
	if (ret < 0) {
		if (retry_times++ < WLC_SCAN_RETRY_TIMES) {
			if (psta_debug)
			dbg("set escan command failed, retry %d\n", retry_times);
			sleep(1);
			goto retry;
		}
	}

	sleep(2);

	ret = get_scan_escan(scan_result, WLC_SCAN_RESULT_BUF_LEN);
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

#else

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

	list->buflen = WLC_SCAN_RESULT_BUF_LEN;
	ret = wl_ioctl(ifname, WLC_SCAN_RESULTS, scan_result, WLC_SCAN_RESULT_BUF_LEN);
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
#endif

static int
wl_scan(int unit)
{
	wl_scan_results_t *list = (wl_scan_results_t*)scan_result;
	wl_bss_info_t *bi;
	wl_bss_info_107_t *old_bi;
	uint i, ap_count = 0;
	char ssid_str[128], macstr[18];

#ifdef RTCONFIG_BCM_7114
	if (wl_get_scan_results_escan(unit) == NULL)
#else
	if (wl_get_scan_results(unit) == NULL)
#endif
		return 0;

	if (list->count == 0)
		return 0;
#ifndef RTCONFIG_BCM_7114
	else if (list->version != WL_BSS_INFO_VERSION &&
			list->version != LEGACY_WL_BSS_INFO_VERSION &&
			list->version != LEGACY2_WL_BSS_INFO_VERSION) {
		dbg("Sorry, your driver has bss_info_version %d "
		    "but this program supports only version %d.\n",
		    list->version, WL_BSS_INFO_VERSION);
		return 0;
	}
#endif

	memset(ap_list, 0, sizeof(ap_list));
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
			if (ap_count < MAX_NUMBER_OF_APINFO) {
#if 0
				ap_list[ap_count].used = TRUE;
#endif
				memcpy(ap_list[ap_count].BSSID, (uint8 *)&bi->BSSID, 6);
				strncpy((char *)ap_list[ap_count].ssid, (char *)bi->SSID, bi->SSID_len);
				ap_list[ap_count].ssid[bi->SSID_len] = '\0';
				ap_list[ap_count].ssidLen= bi->SSID_len;
#if 0
				ap_list[ap_count].ie_buf = (uint8 *)(((uint8 *)bi) + bi->ie_offset);
				ap_list[ap_count].ie_buflen = bi->ie_length;
#endif
				if (dtoh32(bi->version) != LEGACY_WL_BSS_INFO_VERSION && bi->n_cap)
					ap_list[ap_count].channel= bi->ctl_ch;
				else
					ap_list[ap_count].channel= (bi->chanspec & WL_CHANSPEC_CHAN_MASK);
#if 0
				ap_list[ap_count].wep = bi->capability & DOT11_CAP_PRIVACY;
#endif
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
			char_to_ascii(ssid_str, (const char *) trim_r((char *) ap_list[i].ssid));

			ether_etoa((const unsigned char *) &ap_list[i].BSSID, macstr);
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
static void check_wl_rate(char *ifname)
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
psta_keepalive(int ex)
{
	char tmp[NVRAM_BUFSIZE], tmp2[NVRAM_BUFSIZE], prefix[] = "wlXXXXXXXXXX_";
	char *name = NULL;
	struct maclist *mac_list = NULL;
	int mac_list_size, i, unit;
	int psta = 0;
	struct ether_addr bssid;
	unsigned char bssid_null[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
	char macaddr[18];
	char band_var[16];

	sprintf(band_var, "wlc_band%s", ex ? "_ex" : "");
	unit = nvram_get_int(band_var);
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);

	if (!nvram_match(strcat_r(prefix, "mode", tmp), "psta") &&
	    !nvram_match(strcat_r(prefix, "mode", tmp2), "psr"))
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
		ether_etoa((const unsigned char *) &bssid, macaddr);
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
		if (++count_bss_down > 3)
		{
			count_bss_down = 0;
			if (wl_scan(unit))
			{
				eval("wl", "-i", name, "disassoc");
				char *amode, *sec = nvram_safe_get(strcat_r(prefix, "akm", tmp));
				char *argv[] = { "wl", "-i", name, "join", nvram_safe_get(strcat_r(prefix, "ssid", tmp)), "amode", NULL, NULL, NULL, NULL };
				int index = 6;
				unsigned char ea[ETHER_ADDR_LEN];

				if (strstr(sec, "psk2")) amode = "wpa2psk";
				else if (strstr(sec, "psk")) amode = "wpapsk";
				else if (strstr(sec, "wpa2")) amode = "wpa2";
				else if (strstr(sec, "wpa")) amode = "wpa";
				else if (nvram_get_int(strcat_r(prefix, "auth", tmp))) amode = "shared";
				else amode = "open";

				argv[index++] = amode;
				if (ether_atoe(nvram_safe_get("wlc_hwaddr"), ea)) {
					argv[index++] = "-b";
					argv[index++] = nvram_safe_get("wlc_hwaddr");
				}

				_eval(argv, NULL, 0, NULL);
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
		psta_keepalive(0);
#ifdef PXYSTA_DUALBAND
		if (!nvram_match("dpsta_ifnames", ""))
			psta_keepalive(1);
#endif
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

	if (!psta_exist() && !psr_exist())
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

	/* turn off wireless led of other bands under psta mode */
	if (is_psta(nvram_get_int("wlc_band")))
	setWlOffLed();

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
