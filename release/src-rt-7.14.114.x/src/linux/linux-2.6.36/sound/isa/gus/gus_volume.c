/*
 *  Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/time.h>
#include <sound/core.h>
#include <sound/gus.h>
#define __GUS_TABLES_ALLOC__
#include "gus_tables.h"

EXPORT_SYMBOL(snd_gf1_atten_table); /* for snd-gus-synth module */

unsigned short snd_gf1_lvol_to_gvol_raw(unsigned int vol)
{
	unsigned short e, m, tmp;

	if (vol > 65535)
		vol = 65535;
	tmp = vol;
	e = 7;
	if (tmp < 128) {
		while (e > 0 && tmp < (1 << e))
			e--;
	} else {
		while (tmp > 255) {
			tmp >>= 1;
			e++;
		}
	}
	m = vol - (1 << e);
	if (m > 0) {
		if (e > 8)
			m >>= e - 8;
		else if (e < 8)
			m <<= 8 - e;
		m &= 255;
	}
	return (e << 8) | m;
}


unsigned short snd_gf1_translate_freq(struct snd_gus_card * gus, unsigned int freq16)
{
	freq16 >>= 3;
	if (freq16 < 50)
		freq16 = 50;
	if (freq16 & 0xf8000000) {
		freq16 = ~0xf8000000;
		snd_printk(KERN_ERR "snd_gf1_translate_freq: overflow - freq = 0x%x\n", freq16);
	}
	return ((freq16 << 9) + (gus->gf1.playback_freq >> 1)) / gus->gf1.playback_freq;
}
