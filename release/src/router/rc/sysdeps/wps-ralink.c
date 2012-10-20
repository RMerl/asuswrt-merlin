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
	if(getpid()!=1) {
		notify_rc("stop_wps_method");
		return 0;
	}

//	doSystem("iwpriv %s set WscConfMode=%d", get_wpsifname(), 0);	// WPS disabled
	doSystem("iwpriv %s set WscStatus=%d", get_wpsifname(), 0);	// Not Used
//	doSystem("iwpriv %s set WscConfMode=%d", get_non_wpsifname(), 7);	// trigger Windows OS to give a popup about WPS PBC AP

	return 0;
}

extern int g_isEnrollee;
int count = 0;

int is_wps_stopped(void)
{
	int status, ret = 1;
	status = getWscStatus();
	if (status != 1)
		count = 0;
	else
		count++;

//	dbG("wps status: %d, count: %d\n", status, count);

	switch (status) {
		case 1:		/* Idle */
			if (count < 5) ret = 0;
			break;
		case 34:	/* Configured */
			dbG("\nWPS Configured\n");
			break;
		case 0x109:	/* PBC_SESSION_OVERLAP */
			dbG("\nWPS PBC SESSION OVERLAP\n");
			break;
		case 2:		/* Failed */
			if (!g_isEnrollee)
			{
				dbG("\nWPS Failed\n");
				break;
			}
		default:
			ret = 0;
			break;
	}

	return ret;
	// TODO: handle enrollee
}
