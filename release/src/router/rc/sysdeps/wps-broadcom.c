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

#ifdef RTCONFIG_QTN
#include "web-qtn.h"
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

	if (getpid()!=1) {
		notify_rc("start_wps_method");
		return 0;
	}

	wps_band = nvram_get_int("wps_band");
	snprintf(prefix, sizeof(prefix), "wl%d_", wps_band);
	wps_action = nvram_get_int("wps_action");
//	wps_method = nvram_get_int("wps_method"); // useless
	wps_sta_pin = nvram_safe_get("wps_sta_pin");

#ifdef RTCONFIG_QTN
	int retval;

	if (wps_band)
	{
		if (strlen(wps_sta_pin) && strcmp(wps_sta_pin, "00000000") && (wl_wpsPincheck(wps_sta_pin) == 0))
		{
			retval = rpc_qcsapi_wps_registrar_report_pin(WIFINAME, wps_sta_pin);
			if (retval < 0)
				dbG("rpc_qcsapi_wps_registrar_report_pin %s error, return: %d\n", WIFINAME, retval);
		}
		else
		{
			retval = rpc_qcsapi_wps_registrar_report_button_press(WIFINAME);
			if (retval < 0)
				dbG("rpc_qcsapi_wps_registrar_report_button_press %s error, return: %d\n", WIFINAME, retval);
		}

		return;
	}
#endif

	if (strlen(wps_sta_pin) && strcmp(wps_sta_pin, "00000000") && (wl_wpsPincheck(wps_sta_pin) == 0))
		len += sprintf(buf + len, "wps_method=%d ", WPS_UI_METHOD_PIN);
	else
		len += sprintf(buf + len, "wps_method=%d ", WPS_UI_METHOD_PBC);

	if (nvram_match("wps_version2", "enabled") && strlen(nvram_safe_get("wps_autho_sta_mac")))
		len += sprintf(buf + len, "wps_autho_sta_mac=%s ", nvram_safe_get("wps_autho_sta_mac"));

	if (strlen(wps_sta_pin))
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

//	nvram_unset("wps_sta_devname");
//	nvram_unset("wps_sta_mac");
//	nvram_unset("wps_pinfail");
//	nvram_unset("wps_pinfail_mac");
//	nvram_unset("wps_pinfail_name");
//	nvram_unset("wps_pinfail_state");

	nvram_set("wps_env_buf", buf);
	nvram_set_int("wps_restart_war", 1);
	set_wps_env(buf);

	sprintf(tmp, "%lu", uptime());
	nvram_set("wps_uptime", tmp);

	return 0;
}

void
restart_wps_monitor(void)
{
	int unit;
	char word[256], *next;
	char tmp[100], prefix[]="wlXXXXXXX_";
	char *wps_argv[] = {"/bin/wps_monitor", NULL};
	pid_t pid;
	int wait_time = 3;

	unlink("/tmp/wps_monitor.pid");

	if (nvram_match("wps_enable", "1"))
	{
		unit = 0;
		foreach(word, nvram_safe_get("wl_ifnames"), next)
		{
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			nvram_set(strcat_r(prefix, "wps_mode", tmp), "enabled");

			unit++;
		}

		eval("killall", "wps_monitor");

		do {
			if ((pid = get_pid_by_name("/bin/wps_monitor")) <= 0)
				break;
			wait_time--;
			sleep(1);
		} while (wait_time);

		if (wait_time == 0)
			dbG("Unable to kill wps_monitor!\n");

		_eval(wps_argv, NULL, 0, &pid);
	}
}

int
stop_wps_method(void)
{
	char buf[256] = "SET ";
	int len = 4;

	if (getpid()!=1) {
		notify_rc("stop_wps_method");
		return 0;
	}

#ifdef RTCONFIG_QTN
	int retval = rpc_qcsapi_wps_cancel(WIFINAME);
	if (retval < 0)
		dbG("rpc_qcsapi_wps_cancel %s error, return: %d\n", WIFINAME, retval);
#endif

	len += sprintf(buf + len, "wps_config_command=%d ", WPS_UI_CMD_STOP);
	len += sprintf(buf + len, "wps_action=%d ", WPS_UI_ACT_NONE);

	set_wps_env(buf);

	usleep(100*1000);

	restart_wps_monitor();

	return 0;
}

#ifdef RTCONFIG_QTN
int rpc_qcsapi_wps_get_state(const char *ifname, char *wps_state, const qcsapi_unsigned_int max_len)
{
	int ret;

	ret = qcsapi_wps_get_state(ifname, wps_state, max_len);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_get_state %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wifi_disable_wps(const char *ifname, int disable_wps)
{
	int ret;

	ret = qcsapi_wifi_disable_wps(ifname, disable_wps);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wifi_disable_wps %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wps_cancel(const char *ifname)
{
	int ret;

	ret = qcsapi_wps_cancel(ifname);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_cancel %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wps_registrar_report_button_press(const char *ifname)
{
	int ret;

	ret = qcsapi_wps_registrar_report_button_press(ifname);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_registrar_report_button_press %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}

int rpc_qcsapi_wps_registrar_report_pin(const char *ifname, const char *wps_pin)
{
	int ret;

	ret = qcsapi_wps_registrar_report_pin(ifname, wps_pin);
	if (ret < 0) {
		dbG("Qcsapi qcsapi_wps_registrar_report_pin %s error, return: %d\n", ifname, ret);
		return ret;
	}

	return 0;
}
#endif

int is_wps_stopped(void)
{
	int ret = 1;
	int status = nvram_get_int("wps_proc_status");
	time_t now = uptime();
	time_t wps_uptime = strtoul(nvram_safe_get("wps_uptime"), NULL, 10);
	char tmp[100];

	if ((now - wps_uptime) < 2)
		return 0;

#ifdef RTCONFIG_QTN
	char wps_state[32], state_str[32];
	int retval, state = -1 ;

	if (nvram_get_int("wps_enable") && nvram_get_int("wps_band"))
	{
		retval = rpc_qcsapi_wps_get_state(WIFINAME, wps_state, sizeof(wps_state));
		if (retval < 0)
			dbG("rpc_qcsapi_wps_get_state %s error, return: %d\n", WIFINAME, retval);
		else
		{
			if (sscanf(wps_state, "%d %s", &state, state_str) != 2)
				dbG("prase wps state error!\n");

			switch (state) {
				case 0: /* WPS_INITIAL */
					dbg("Init\n");
					break;
				case 1: /* WPS_START */
					dbg("Processing WPS start...\n");
					ret = 0;
					break;
				case 2: /* WPS_SUCCESS */
					dbg("WPS Success\n");
					break;
				case 3: /* WPS_ERROR */
					dbg("WPS Fail due to message exange error!\n");
					break;
				case 4: /* WPS_TIMEOUT */
					dbg("WPS Fail due to time out!\n");
					break;
				case 5: /* WPS_OVERLAP */
					dbg("WPS Fail due to PBC session overlap!\n");
					break;
				default:
					ret = 0;
					break;
			}
		}

		return ret;
	}
#endif

	switch (status) {
		case 0: /* Init */
			dbg("Init again?\n");
			if (nvram_get_int("wps_restart_war") && (now - wps_uptime) < 3)
			{
				dbg("Re-send WPS env!!!\n");
				set_wps_env(nvram_safe_get("wps_env_buf"));
				nvram_unset("wps_env_buf");
				nvram_set_int("wps_restart_war", 0);
				sprintf(tmp, "%lu", uptime());
				nvram_set("wps_uptime", tmp);
				return 0;
			}
			break;
		case 1: /* WPS_ASSOCIATED */
			dbg("Processing WPS start...\n");
			ret = 0;
			break;
		case 2: /* WPS_OK */
		case 7: /* WPS_MSGDONE */
			dbg("WPS Success\n");
			break;
		case 3: /* WPS_MSG_ERR */
			dbg("WPS Fail due to message exange error!\n");
			break;
		case 4: /* WPS_TIMEOUT */
			dbg("WPS Fail due to time out!\n");
			break;
		default:
			ret = 0;
			break;
	}

	return ret;
	// TODO: handle enrollee
}
