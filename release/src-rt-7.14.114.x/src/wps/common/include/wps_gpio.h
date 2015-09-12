/*
 * WPS GPIO Header file
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_gpio.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _WPS_GPIO_H_
#define _WPS_GPIO_H_

#include <wps_hal.h>

#define WPS_LED_FASTBLINK		100		/* fast blink period */
#define WPS_LED_MEDIUMBLINK	1000	/* medium blink period */
#define WPS_LED_SLOWBLINK		5000	/* slow blink period */

#define WPS_LONG_PRESSTIME	5			/* seconds */
#define WPS_BTNSAMPLE_PERIOD	(500 * 1000)	/* 500 ms */
#define WPS_BTN_ASSERTLVL	0
#define WPS_LED_ASSERTLVL	1
#define WPS_LED_BLINK_TIME_UNIT	100 /* 100ms (0.1 second) wps_led_blink_timer() period */

int wps_gpio_btn_init(void);
int wps_gpio_led_init(void);
void wps_gpio_btn_cleanup(void);
void wps_gpio_led_cleanup(void);
wps_btnpress_t wps_gpio_btn_pressed(void);
void wps_gpio_led_blink(wps_blinktype_t blinktype);
void wps_gpio_led_blink_timer();

#endif /* _WPS_GPIO_H_ */
