/*
 * Miscellaneous services
 *
 * Copyright (C) 2009, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: services.c,v 1.100 2010/03/04 09:39:18 Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <rc.h>
#include <semaphore_mfp.h>
#include <shared.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <security_ipc.h>
#include <bcmutils.h>
#include <wlutils.h>
#include <wlscan.h>
#ifdef RTCONFIG_WPS
#include <wps_ui.h>
#endif

static int
set_wps_env(char *uibuf)
{
	int wps_fd = -1;
	struct sockaddr_in to;
	int sentBytes = 0;
	uint32 uilen = strlen(uibuf);

	//if (is_wps_enabled() == 0)
	//	return -1;

	if ((wps_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		goto exit;
	}
	
	/* send to WPS */
	to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	to.sin_family = AF_INET;
	to.sin_port = htons(WPS_UI_PORT);

	sentBytes = sendto(wps_fd, uibuf, uilen, 0, (struct sockaddr *) &to,
		sizeof(struct sockaddr_in));

	if (sentBytes != uilen) {
		goto exit;
	}

	/* Sleep 100 ms to make sure
	   WPS have received socket */
	usleep(100*1000);
	close(wps_fd);
	return 0;

exit:
	if (wps_fd >= 0)
		close(wps_fd);

	/* Show error message ?  */
	return -1;
}



/*
 * input variables:
 * 	nvram: wps_band: 
 * 	nvram: wps_action: WPS_UI_ACT_ADDENROLLEE/WPS_UI_ACT_CONFIGAe
 *	       (wps_method: (according to wps_sta_pin)
 *	nvram: wps_sta_pin:
 *	nvram: wps_version2: 
 *	nvram: wps_autho_sta_mac: 
 *
 * output variables:
 * 	wps_proc_status
 */

int 
start_wps_method(void)
{
	int wps_band;
	int wps_action;
//	int wps_method;
	char *wps_sta_pin;
	char prefix[]="wlXXXXXX_", tmp[100];
	char buf[256] = "SET ";
	int len = 4;

	if(getpid()!=1) {
		notify_rc("start_wps_method");
		return 0;
	}

	wps_band = nvram_get_int("wps_band");
	snprintf(prefix, sizeof(prefix), "wl%d_", wps_band);
	wps_action = nvram_get_int("wps_action");
//	wps_method = nvram_get_int("wps_method"); // useless
	wps_sta_pin = nvram_safe_get("wps_sta_pin");

	if(strlen(wps_sta_pin) && strcmp(wps_sta_pin, "00000000") && (wl_wpsPincheck(wps_sta_pin) == 0))
		len += sprintf(buf + len, "wps_method=%d ", WPS_UI_METHOD_PIN);	
	else
		len += sprintf(buf + len, "wps_method=%d ", WPS_UI_METHOD_PBC);
	
	if(nvram_match("wps_version2", "enabled") && strlen(nvram_safe_get("wps_autho_sta_mac")))
		len += sprintf(buf + len, "wps_autho_sta_mac=%s ", nvram_safe_get("wps_autho_sta_mac"));

	if(strlen(wps_sta_pin))
		len += sprintf(buf + len, "wps_sta_pin=%s ", wps_sta_pin);
	else
		len += sprintf(buf + len, "wps_sta_pin=00000000 ");

//	len += sprintf(buf + len, "wps_action=%d ", wps_action);
	len += sprintf(buf + len, "wps_action=%d ", WPS_UI_ACT_ADDENROLLEE);

	len += sprintf(buf + len, "wps_config_command=%d ", WPS_UI_CMD_START);

	nvram_set("wps_proc_status", "0");

	len += sprintf(buf + len, "wps_pbc_method=%d ", WPS_UI_PBC_SW);
	len += sprintf(buf + len, "wps_ifname=%s ", nvram_safe_get(strcat_r(prefix, "ifname", tmp)));

	dbG("wps env buffer: %s\n", buf);

	nvram_unset("wps_sta_devname");
	nvram_unset("wps_sta_mac");
	nvram_unset("wps_pinfail");
	nvram_unset("wps_pinfail_mac");
	nvram_unset("wps_pinfail_name");
	nvram_unset("wps_pinfail_state");

	set_wps_env(buf);

	sprintf(tmp, "%lu", uptime());
	nvram_set("wps_uptime", tmp);

	return 0;
}

int 
stop_wps_method(void)
{
	char buf[256] = "SET ";
	int len = 4;

	if(getpid()!=1) {
		notify_rc("stop_wps_method");
		return 0;
	}

	len += sprintf(buf + len, "wps_config_command=%d ", WPS_UI_CMD_STOP);
	len += sprintf(buf + len, "wps_action=%d ", WPS_UI_ACT_NONE);

	set_wps_env(buf);

	return 0;
}

int is_wps_stopped(void)
{
	int ret = 1;
	int status = nvram_get_int("wps_proc_status");
	time_t now = uptime();
	time_t wps_uptime = strtoul(nvram_safe_get("wps_uptime"), NULL, 10);

	if ((now - wps_uptime) < 5)
		return 0;

	switch (status) {
		case 0: /* Init */
			fprintf(stderr, "Init again?\n");
			break;
		case 1: /* WPS_ASSOCIATED */
			fprintf(stderr, "Processing WPS start...\n");
			ret = 0;
			break;
		case 2: /* WPS_OK */
		case 7: /* WPS_MSGDONE */
			fprintf(stderr, "WPS Success\n");
			break;
		case 3: /* WPS_MSG_ERR */
			fprintf(stderr, "WPS Fail due to message exange error!\n");
			break;
		case 4: /* WPS_TIMEOUT */
			fprintf(stderr, "WPS Fail due to time out!\n");
			break;
		default:
			ret = 0;
			break;
	}

	return ret;
	// TODO: handle enrollee 
}
