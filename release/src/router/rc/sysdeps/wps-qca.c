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
#include <sys/types.h>
#include <unistd.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <rc.h>
#include <shared.h>

/*
 * input variables:
 *	nvram: wps_sta_pin:
 *
 */

int 
start_wps_method(void)
{
	if(getpid()!=1) {
		notify_rc("start_wps_method");
		return 0;
	}

#ifdef RTCONFIG_WPS_ENROLLEE
	if (nvram_match("wps_enrollee", "1"))
		start_wsc_enrollee();
	else
#endif
		start_wsc();

	return 0;
}

int 
stop_wps_method(void)
{
	if(getpid()!=1) {
		notify_rc("stop_wps_method");
		return 0;
	}

#ifdef RTCONFIG_WPS_ENROLLEE
	if (nvram_match("wps_enrollee", "1")) {
		stop_wsc_enrollee();
	}
	else
#endif
		stop_wsc();

	return 0;
}

extern int g_isEnrollee[MAX_NR_WL_IF];

int is_wps_stopped(void)
{
	int i, ret = 1;
	char status[16], tmp[128], prefix[] = "wlXXXXXXXXXX_", word[256], *next, ifnames[128];
	int wps_band = nvram_get_int("wps_band"), multiband = get_wps_multiband();

	i = 0;
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));
	foreach (word, ifnames, next) {
		if (i >= MAX_NR_WL_IF)
			break;
		if (!multiband && wps_band != i) {
			++i;
			continue;
		}
		snprintf(prefix, sizeof(prefix), "wl%d_", i);
		if (!__need_to_start_wps_band(prefix) || nvram_match(strcat_r(prefix, "radio", tmp), "0")) {
			ret = 0;
			++i;
			continue;
		}

#ifdef RTCONFIG_WPS_ENROLLEE
		if (nvram_match("wps_enrollee", "1"))
			strcpy(status, getWscStatus_enrollee(i));
		else
#endif
			strcpy(status, getWscStatus(i));

		//dbG("band %d wps status: %s\n", i, status);
		if (!strcmp(status, "Success") 
#ifdef RTCONFIG_WPS_ENROLLEE
				|| !strcmp(status, "COMPLETED")
#endif
		) {
			dbG("\nWPS %s\n", status);
#ifdef RTCONFIG_WPS_LED
			nvram_set("wps_success", "1");
#endif
#if (defined(RTCONFIG_WPS_ENROLLEE) && defined(RTCONFIG_WIFI_CLONE))
			if (nvram_match("wps_enrollee", "1")) {
				nvram_set("wps_e_success", "1");
#if (defined(PLN12) || defined(PLAC56))
				set_wifiled(4);
#endif
				wifi_clone(i);
			}
#endif
			ret = 1;
		}
		else if (!strcmp(status, "Failed") 
#ifdef RTCONFIG_WPS_ENROLLEE
				|| !strcmp(status, "INACTIVE")
#endif
		) {
			dbG("\nWPS %s\n", status);
			ret = 1;
		}
		else
			ret = 0;

		if (ret)
			break;

		++i;
	}

	return ret;
}

/*
 * save new AP configuration from WPS external registrar
 */
int get_wps_er_main(int argc, char *argv[])
{
	int i;
	char word[32], *next, ifnames[64];
	char wps_ifname[32], _ssid[33], _auth[8], _encr[8], _key[65];

	if (nvram_invmatch("w_Setting", "0"))
		return 0;

	sleep(2);

	memset(wps_ifname, 0x0, sizeof(wps_ifname));
	memset(_ssid, 0x0, sizeof(_ssid));
	memset(_auth, 0x0, sizeof(_auth));
	memset(_encr, 0x0, sizeof(_encr));
	memset(_key, 0x0, sizeof(_key));
	strcpy(ifnames, nvram_safe_get("wl_ifnames"));

	while (nvram_match("w_Setting", "0")) {
		foreach (word, ifnames, next) {
			char buf[1024];
			FILE *fp;
			int len;
			char *pt1,*pt2 = NULL;

			sprintf(buf, "hostapd_cli -i%s get_config", word);
			fp = popen(buf, "r");
			if (fp) {
				memset(buf, 0, sizeof(buf));
				len = fread(buf, 1, sizeof(buf), fp);
				pclose(fp);

				if (len > 1) {
					buf[len-1] = '\0';
					pt1 = strstr(buf, "wps_state=configured");
					if (pt1) {
						strcpy(wps_ifname, word);
						//BSSID
						if ((pt1 = strstr(buf, "bssid=")))
							pt2 = pt1 + strlen("bssid=");
						//SSID
						if ((pt1 = strstr(pt2, "ssid="))) {
							pt1 += strlen("ssid=");
							if ((pt2 = strstr(pt1, "\n")))
								*pt2 = '\0';
							//dbG("ssid=%s\n", pt1);
							nvram_set("wl0_ssid", pt1);
							nvram_set("wl1_ssid", pt1);
							strcpy(_ssid, pt1);
							pt2++;
						}
						//Encryp
						if (strstr(pt2, "wpa=2")) {
							nvram_set("wl0_auth_mode_x", "pskpsk2");
							nvram_set("wl0_crypto", "tkip+aes");
							nvram_set("wl1_auth_mode_x", "pskpsk2");
							nvram_set("wl1_crypto", "tkip+aes");
							strcpy(_auth, "WPA2PSK");
							if (strstr(pt2, "group_cipher=TKIP"))
								strcpy(_encr, "TKIP");
							else
								strcpy(_encr, "CCMP");
						}
						else if (strstr(pt2, "wpa=3")) {
							nvram_set("wl0_auth_mode_x", "pskpsk2");
							nvram_set("wl0_crypto", "tkip+aes");
							nvram_set("wl1_auth_mode_x", "pskpsk2");
							nvram_set("wl1_crypto", "tkip+aes");
							strcpy(_auth, "WPAPSK");
							if (strstr(pt2, "group_cipher=TKIP"))
								strcpy(_encr, "TKIP");
							else
								strcpy(_encr, "CCMP");
						}
						else {
							nvram_set("wl0_auth_mode_x", "open");
							nvram_set("wl0_crypto", "tkip+aes");
							nvram_set("wl1_auth_mode_x", "open");
							nvram_set("wl1_crypto", "tkip+aes");
							strcpy(_auth, "OPEN");
							strcpy(_encr, "NONE");
						}
						//WPAKey
						if ((pt1 = strstr(pt2, "passphrase="))) {
							pt1 += strlen("passphrase=");
							if ((pt2 = strstr(pt1, "\n")))
								*pt2 = '\0';
							//dbG("passphrase=%s\n", pt1);
							nvram_set("wl0_wpa_psk", pt1);
							nvram_set("wl1_wpa_psk", pt1);
							strcpy(_key, pt1);
							pt2++;
						}

						nvram_set("w_Setting", "1");
						nvram_commit();
					}
				}
			}
		}

		sleep(1);
	}

	i = 0;
	foreach (word, ifnames, next) {
		if (i >= MAX_NR_WL_IF)
			break;
		if (!strcmp(word, wps_ifname))
			continue;
		eval("hostapd_cli", "-i", word, "wps_config", _ssid, _auth, _encr, _key);
	}

	return 0;
}
