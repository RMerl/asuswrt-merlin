/*
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
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
 * Header file for GPIO utility functions.
 *
 * $Id: bcmgpio.h 241398 2011-02-18 03:46:33Z stakita $
 */
#ifndef __bcmgpio_h__
#define __bcmgpio_h__

#include <bcmtimer.h>


#define BCMGPIO_MAXPINS		32
#define BCMGPIO_MAXINDEX	(BCMGPIO_MAXPINS - 1)
#define BCMGPIO_UNDEFINED	-1

/* GPIO type */
typedef enum bcmgpio_dirn {
	BCMGPIO_DIRN_IN = 0,
	BCMGPIO_DIRN_OUT
} bcmgpio_dirn_t;

/* GPIO strobe information */
typedef struct bcmgpio_strobe {
	int duty_percent;	/* duty cycle of strobe in percent of strobe period */
	bcm_timer_module_id timer_module;	/* timer module ID obtained by 
							* calling bcm_timer_module_init()
							*/
	unsigned long strobe_period_in_ms;	/* strobe period of the GPIO in milliseconds */
	unsigned long num_strobes;	/* total number of strobes */
	int *strobe_done;	/* pointer to memory which is used to signal strobe completion */
} bcmgpio_strobe_t;

/* Functions to implement Buttons and LEDs on the AP, using GPIOs */
int bcmgpio_connect(int gpio_pin, bcmgpio_dirn_t gpio_dirn);
int bcmgpio_disconnect(int gpio_pin);
int bcmgpio_in(unsigned long gpio_mask, unsigned long *value);
int bcmgpio_out(unsigned long gpio_mask, unsigned long value);
int bcmgpio_strobe_start(int gpio_pin, bcmgpio_strobe_t *strobe_info);
int bcmgpio_strobe_stop(int gpio_pin);
int bcmgpio_getpin(char *pin_name, uint def_pin);

#endif	/* __bcmgpio_h__ */
