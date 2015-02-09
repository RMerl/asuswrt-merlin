/*
 * ALSA SoC I2S (McBSP) Audio Layer for TI DAVINCI processor
 *
 * Author:      Vladimir Barinov, <vbarinov@embeddedalley.com>
 * Copyright:   (C) 2007 MontaVista Software, Inc., <source@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _DAVINCI_I2S_H
#define _DAVINCI_I2S_H

/* McBSP dividers */
enum davinci_mcbsp_div {
	DAVINCI_MCBSP_CLKGDV,              /* Sample rate generator divider */
};

extern struct snd_soc_dai davinci_i2s_dai;

#endif
