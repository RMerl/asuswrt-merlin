/* ASB2303 peripheral 7-segment LEDs x1 support
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/processor.h>
#include <asm/intctl-regs.h>
#include <asm/rtc-regs.h>
#include <unit/leds.h>


static const u8 asb2303_led_chase_tbl[6] = {
	~0x02,	/* top		- segA */
	~0x04,	/* right top	- segB */
	~0x08,	/* right bottom	- segC */
	~0x10,	/* bottom	- segD */
	~0x20,	/* left bottom	- segE */
	~0x40,	/* left top	- segF */
};

static unsigned asb2303_led_chase;

void peripheral_leds_display_exception(enum exception_code code)
{
	ASB2303_GPIO0DEF = 0x5555;	/* configure as an output port */
	ASB2303_7SEGLEDS = 0x6d;	/* triple horizontal bar */
}

void peripheral_leds_led_chase(void)
{
	ASB2303_GPIO0DEF = 0x5555;	/* configure as an output port */
	ASB2303_7SEGLEDS = asb2303_led_chase_tbl[asb2303_led_chase];
	asb2303_led_chase++;
	if (asb2303_led_chase >= 6)
		asb2303_led_chase = 0;
}
