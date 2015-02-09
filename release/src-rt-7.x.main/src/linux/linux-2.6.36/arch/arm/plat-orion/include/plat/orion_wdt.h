/*
 * arch/arm/plat-orion/include/plat/orion_wdt.h
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __PLAT_ORION_WDT_H
#define __PLAT_ORION_WDT_H

struct orion_wdt_platform_data {
	u32	tclk;		/* no <linux/clk.h> support yet */
};


#endif
