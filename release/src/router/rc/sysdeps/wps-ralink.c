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

	start_wsc();

	return 0;
}

int 
stop_wps_method(void)
{
	char prefix[] = "wlXXXXXXXXXX_", word[256], *next, ifnames[128];
	int i, wps_band = nvram_get_int("wps_band_x"), multiband = get_wps_multiband();

	if(getpid()!=1) {
		notify_rc("stop_wps_method");
		return 0;
	}

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

		if (!multiband) {
//			doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 0);		// WPS disabled
			doSystem("iwpriv %s set WscStatus=%d", get_wifname(i), 0);	// Not Used
//			doSystem("iwpriv %s set WscConfMode=%d", get_non_wpsifname(), 7);	// trigger Windows OS to give a popup about WPS PBC AP
		} else {
			/* Make sure WPS on all band are turned off */
			doSystem("iwpriv %s set WscConfMode=%d", get_wifname(i), 0);	// WPS disabled
			doSystem("iwpriv %s set WscStatus=%d", get_wifname(i), 0);	// Not Used
			doSystem("iwpriv %s set WscConfMode=%d", get_wifname(i), 7);	// trigger Windows OS to give a popup about WPS PBC AP
		}

		++i;
	}

	return 0;
}

extern int g_isEnrollee[MAX_NR_WL_IF];
int count[MAX_NR_WL_IF] = { 0, 0 };

int is_wps_stopped(void)
{
	int i, status, ret = 1;
	char tmp[128], prefix[] = "wlXXXXXXXXXX_", word[256], *next, ifnames[128];
	int wps_band = nvram_get_int("wps_band_x"), multiband = get_wps_multiband();

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
			
		status = getWscStatus(i);
		if (status != 1)
			count[i] = 0;
		else
			count[i]++;

//		dbG("band %d wps status: %d, count: %d\n", i, status, count[i]);
		switch (status) {
			case 1:		/* Idle */
				if (strstr(nvram_safe_get("rc_service"), "start_wps_method") != NULL)
					count[i] = 0;
				if (count[i] < 15) ret = 0; // 15 would delay 750ms to avoid error since WPS not start yet.
				break;
			case 34:	/* Configured */
				dbG("\nWPS Configured\n");
				ret = 1;
				break;
			case 0x109:	/* PBC_SESSION_OVERLAP */
				dbG("\nWPS PBC SESSION OVERLAP\n");
				ret = 1;
				break;
			case 2:		/* Failed */
				if (!g_isEnrollee[i]) {
					dbG("\nWPS Failed\n");
					break;
				}
			default:
				ret = 0;
				break;
		}

		if (ret)
			break;

		++i;
	}

	return ret;
	// TODO: handle enrollee
}
