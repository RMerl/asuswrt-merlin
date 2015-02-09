/*
 * wm8350.h - WM8903 audio codec interface
 *
 * Copyright 2008 Wolfson Microelectronics PLC.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef _WM8350_H
#define _WM8350_H

#include <sound/soc.h>
#include <linux/mfd/wm8350/audio.h>

extern struct snd_soc_dai wm8350_dai;
extern struct snd_soc_codec_device soc_codec_dev_wm8350;

enum wm8350_jack {
	WM8350_JDL = 1,
	WM8350_JDR = 2,
};

int wm8350_hp_jack_detect(struct snd_soc_codec *codec, enum wm8350_jack which,
			  struct snd_soc_jack *jack, int report);
int wm8350_mic_jack_detect(struct snd_soc_codec *codec,
			   struct snd_soc_jack *jack,
			   int detect_report, int short_report);

#endif
