/*
 * wm8728.h  --  WM8728 ASoC codec driver
 *
 * Copyright 2008 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _WM8728_H
#define _WM8728_H

#define WM8728_DACLVOL   0x00
#define WM8728_DACRVOL   0x01
#define WM8728_DACCTL    0x02
#define WM8728_IFCTL     0x03

struct wm8728_setup_data {
	int            spi;
	int            i2c_bus;
	unsigned short i2c_address;
};

extern struct snd_soc_dai wm8728_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8728;

#endif
