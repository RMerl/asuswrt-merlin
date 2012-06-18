/*
 * Copyright 
 *
 * Author: 
 *
 * Based on WM8750.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _WM8955_H
#define _WM8955_H

/* WM8955 register space */

#define WM8955_LOUT1V    0x02
#define WM8955_ROUT1V    0x03
#define WM8955_DACCTL    0x05
#define WM8955_IFACE     0x07
#define WM8955_SRATE     0x08
#define WM8955_LDAC      0x0a
#define WM8955_RDAC      0x0b
#define WM8955_BASS      0x0c
#define WM8955_TREBLE    0x0d
#define WM8955_RESET     0x0f
#define WM8955_ADCTL1    0x17
#define WM8955_ADCTL2    0x18
#define WM8955_PWR1      0x19
#define WM8955_PWR2      0x1a
#define WM8955_ADCTL3    0x1b
#define WM8955_LOUTM1    0x22
#define WM8955_LOUTM2    0x23
#define WM8955_ROUTM1    0x24
#define WM8955_ROUTM2    0x25
#define WM8955_MOUTM1    0x26
#define WM8955_MOUTM2    0x27
#define WM8955_LOUT2V    0x28
#define WM8955_ROUT2V    0x29
#define WM8955_MOUTV     0x2a
#define WM8955_CLOCK     0x2b
#define WM8955_PLLCTL1   0x2c
#define WM8955_PLLCTL2   0x2d
#define WM8955_PLLCTL3   0x2e
#define WM8955_PLLCTL4   0x3b


#define WM8955_CACHE_REGNUM 0x3b

#define WM8955_SYSCLK	0

struct wm8955_setup_data {
	unsigned short i2c_address;
};

extern struct snd_soc_codec_dai wm8955_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8955;

#endif
