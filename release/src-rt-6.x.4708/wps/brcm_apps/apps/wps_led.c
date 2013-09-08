/*
 * WPS LED (include LAN leds control functions)
 *
 * Copyright (C) 2013, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_led.c 397281 2013-04-18 01:19:51Z $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wpscommon.h>
#include <wps_hal.h>
#include <wps_led.h>
#include <wps.h>

static int wps_prevstatus = -1;


int
wps_led_wl_select_on()
{
	return wps_hal_led_wl_select_on();
}

int
wps_led_wl_select_off()
{
	return wps_hal_led_wl_select_off();
}

void
wps_led_wl_selecting(int led_id)
{
	wps_hal_led_wl_selecting(led_id);
}

void
wps_led_wl_confirmed(int led_id)
{
	wps_hal_led_wl_confirmed(led_id);
}

static int
wps_led(int status)
{
	switch (status) {
	case WPS_INIT:
		wps_hal_led_blink(WPS_BLINKTYPE_STOP);
		break;

	case WPS_ASSOCIATED:
	case WPS_SENDM2:
	case WPS_SENDM7:
	case WPS_MSGDONE:
		wps_hal_led_blink(WPS_BLINKTYPE_INPROGRESS);
		break;

	case WPS_OK:
		wps_hal_led_blink(WPS_BLINKTYPE_SUCCESS);
		break;

	case WPS_TIMEOUT:
	case WPS_MSG_ERR:
		wps_hal_led_blink(WPS_BLINKTYPE_ERROR);
		break;

	case WPS_PBCOVERLAP:
		wps_hal_led_blink(WPS_BLINKTYPE_OVERLAP);
		break;

	case WPS_FIND_PBC_AP:
	case WPS_ASSOCIATING:
		wps_hal_led_blink(WPS_BLINKTYPE_INPROGRESS);
		break;

	default:
		wps_hal_led_blink(WPS_BLINKTYPE_STOP);
		break;
	}

	return 0;
}

void
wps_led_update()
{
	int val;

	val = wps_getProcessStates();
	if (val != -1) {
		if (wps_prevstatus != val) {
			wps_prevstatus = val;
			wps_led(val);
		}
	}
}

int
wps_led_init()
{
	int ret;
	int val;

	ret = wps_hal_led_init();
	if (ret == 0) {
		/* sync wps led */
		wps_prevstatus = WPS_INIT;
		val = wps_getProcessStates();
		if (val != -1) {
			wps_prevstatus = val;
		}

		/* off all wps multi-color led */
		if (val == WPS_INIT)
			wps_hal_led_blink(WPS_BLINKTYPE_STOP_MULTI);

		/* set wps led blink */
		wps_led(wps_prevstatus);
	}

	return ret;
}

void
wps_led_deinit()
{
	/* Show WPS OK or ERROR led before deinit */
	wps_led_update();
	WpsSleepMs(500); /* 500 ms */

	wps_hal_led_deinit();
}
