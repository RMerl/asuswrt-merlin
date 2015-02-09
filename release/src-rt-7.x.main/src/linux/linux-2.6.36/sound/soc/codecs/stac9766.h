/*
 * stac9766.h  --  STAC9766 Soc Audio driver
 */

#ifndef _STAC9766_H
#define _STAC9766_H

#define AC97_STAC_PAGE0 0x1000
#define AC97_STAC_DA_CONTROL (AC97_STAC_PAGE0 | 0x6A)
#define AC97_STAC_ANALOG_SPECIAL (AC97_STAC_PAGE0 | 0x6E)
#define AC97_STAC_STEREO_MIC 0x78

/* STAC9766 DAI ID's */
#define STAC9766_DAI_AC97_ANALOG		0
#define STAC9766_DAI_AC97_DIGITAL		1

extern struct snd_soc_dai stac9766_dai[];
extern struct snd_soc_codec_device soc_codec_dev_stac9766;


#endif
