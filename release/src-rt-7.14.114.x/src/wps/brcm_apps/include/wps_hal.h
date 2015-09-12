/*
 * WPS HAL include
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_hal.h 241182 2011-02-17 21:50:03Z $
 */
#ifndef __WPS_HAL_H__
#define __WPS_HAL_H__

#include <typedefs.h>

/* HAL led */
typedef enum wps_blinktype {
	WPS_BLINKTYPE_INPROGRESS = 0,
	WPS_BLINKTYPE_ERROR,
	WPS_BLINKTYPE_OVERLAP,
	WPS_BLINKTYPE_SUCCESS,
	WPS_BLINKTYPE_STOP,
	WPS_BLINKTYPE_STOP_MULTI
} wps_blinktype_t;

int wps_hal_led_init();
void wps_hal_led_deinit();
void wps_hal_led_blink();

int wps_hal_led_wl_max();
int wps_hal_led_wl_select_on();
int wps_hal_led_wl_select_off();
void wps_hal_led_wl_selecting(int led_id);
void wps_hal_led_wl_confirmed(int led_id);

/* HAL pb */
typedef enum wps_btnpress {
	WPS_NO_BTNPRESS = 0,
	WPS_SHORT_BTNPRESS,
	WPS_LONG_BTNPRESS
} wps_btnpress_t;

wps_btnpress_t wps_hal_btn_pressed();
int wps_hal_btn_init();
void wps_hal_btn_cleanup();


#endif /* __WPS_HAL_H__ */
