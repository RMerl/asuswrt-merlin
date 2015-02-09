/*
 * wm8900.c  --  WM8900 ALSA Soc Audio driver
 *
 * Copyright 2007, 2008 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * TODO:
 *  - Tristating.
 *  - TDM.
 *  - Jack detect.
 *  - FLL source configuration, currently only MCLK is supported.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "wm8900.h"

/* WM8900 register space */
#define WM8900_REG_RESET	0x0
#define WM8900_REG_ID		0x0
#define WM8900_REG_POWER1	0x1
#define WM8900_REG_POWER2	0x2
#define WM8900_REG_POWER3	0x3
#define WM8900_REG_AUDIO1	0x4
#define WM8900_REG_AUDIO2	0x5
#define WM8900_REG_CLOCKING1    0x6
#define WM8900_REG_CLOCKING2    0x7
#define WM8900_REG_AUDIO3       0x8
#define WM8900_REG_AUDIO4       0x9
#define WM8900_REG_DACCTRL      0xa
#define WM8900_REG_LDAC_DV      0xb
#define WM8900_REG_RDAC_DV      0xc
#define WM8900_REG_SIDETONE     0xd
#define WM8900_REG_ADCCTRL      0xe
#define WM8900_REG_LADC_DV	0xf
#define WM8900_REG_RADC_DV      0x10
#define WM8900_REG_GPIO         0x12
#define WM8900_REG_INCTL	0x15
#define WM8900_REG_LINVOL	0x16
#define WM8900_REG_RINVOL	0x17
#define WM8900_REG_INBOOSTMIX1  0x18
#define WM8900_REG_INBOOSTMIX2  0x19
#define WM8900_REG_ADCPATH	0x1a
#define WM8900_REG_AUXBOOST	0x1b
#define WM8900_REG_ADDCTL       0x1e
#define WM8900_REG_FLLCTL1      0x24
#define WM8900_REG_FLLCTL2      0x25
#define WM8900_REG_FLLCTL3      0x26
#define WM8900_REG_FLLCTL4      0x27
#define WM8900_REG_FLLCTL5      0x28
#define WM8900_REG_FLLCTL6      0x29
#define WM8900_REG_LOUTMIXCTL1  0x2c
#define WM8900_REG_ROUTMIXCTL1  0x2d
#define WM8900_REG_BYPASS1	0x2e
#define WM8900_REG_BYPASS2	0x2f
#define WM8900_REG_AUXOUT_CTL   0x30
#define WM8900_REG_LOUT1CTL     0x33
#define WM8900_REG_ROUT1CTL     0x34
#define WM8900_REG_LOUT2CTL	0x35
#define WM8900_REG_ROUT2CTL	0x36
#define WM8900_REG_HPCTL1	0x3a
#define WM8900_REG_OUTBIASCTL   0x73

#define WM8900_MAXREG		0x80

#define WM8900_REG_ADDCTL_OUT1_DIS    0x80
#define WM8900_REG_ADDCTL_OUT2_DIS    0x40
#define WM8900_REG_ADDCTL_VMID_DIS    0x20
#define WM8900_REG_ADDCTL_BIAS_SRC    0x10
#define WM8900_REG_ADDCTL_VMID_SOFTST 0x04
#define WM8900_REG_ADDCTL_TEMP_SD     0x02

#define WM8900_REG_GPIO_TEMP_ENA   0x2

#define WM8900_REG_POWER1_STARTUP_BIAS_ENA 0x0100
#define WM8900_REG_POWER1_BIAS_ENA         0x0008
#define WM8900_REG_POWER1_VMID_BUF_ENA     0x0004
#define WM8900_REG_POWER1_FLL_ENA          0x0040

#define WM8900_REG_POWER2_SYSCLK_ENA  0x8000
#define WM8900_REG_POWER2_ADCL_ENA    0x0002
#define WM8900_REG_POWER2_ADCR_ENA    0x0001

#define WM8900_REG_POWER3_DACL_ENA    0x0002
#define WM8900_REG_POWER3_DACR_ENA    0x0001

#define WM8900_REG_AUDIO1_AIF_FMT_MASK 0x0018
#define WM8900_REG_AUDIO1_LRCLK_INV    0x0080
#define WM8900_REG_AUDIO1_BCLK_INV     0x0100

#define WM8900_REG_CLOCKING1_BCLK_DIR   0x1
#define WM8900_REG_CLOCKING1_MCLK_SRC   0x100
#define WM8900_REG_CLOCKING1_BCLK_MASK  (~0x01e)
#define WM8900_REG_CLOCKING1_OPCLK_MASK (~0x7000)

#define WM8900_REG_CLOCKING2_ADC_CLKDIV 0xe0
#define WM8900_REG_CLOCKING2_DAC_CLKDIV 0x1c

#define WM8900_REG_DACCTRL_MUTE          0x004
#define WM8900_REG_DACCTRL_DAC_SB_FILT   0x100
#define WM8900_REG_DACCTRL_AIF_LRCLKRATE 0x400

#define WM8900_REG_AUDIO3_ADCLRC_DIR    0x0800

#define WM8900_REG_AUDIO4_DACLRC_DIR    0x0800

#define WM8900_REG_FLLCTL1_OSC_ENA    0x100

#define WM8900_REG_FLLCTL6_FLL_SLOW_LOCK_REF 0x100

#define WM8900_REG_HPCTL1_HP_IPSTAGE_ENA 0x80
#define WM8900_REG_HPCTL1_HP_OPSTAGE_ENA 0x40
#define WM8900_REG_HPCTL1_HP_CLAMP_IP    0x20
#define WM8900_REG_HPCTL1_HP_CLAMP_OP    0x10
#define WM8900_REG_HPCTL1_HP_SHORT       0x08
#define WM8900_REG_HPCTL1_HP_SHORT2      0x04

#define WM8900_LRC_MASK 0xfc00

struct snd_soc_codec_device soc_codec_dev_wm8900;

struct wm8900_priv {
	struct snd_soc_codec codec;

	u16 reg_cache[WM8900_MAXREG];

	u32 fll_in; /* FLL input frequency */
	u32 fll_out; /* FLL output frequency */
};

/*
 * wm8900 register cache.  We can't read the entire register space and we
 * have slow control buses so we cache the registers.
 */
static const u16 wm8900_reg_defaults[WM8900_MAXREG] = {
	0x8900, 0x0000,
	0xc000, 0x0000,
	0x4050, 0x4000,
	0x0008, 0x0000,
	0x0040, 0x0040,
	0x1004, 0x00c0,
	0x00c0, 0x0000,
	0x0100, 0x00c0,
	0x00c0, 0x0000,
	0xb001, 0x0000,
	0x0000, 0x0044,
	0x004c, 0x004c,
	0x0044, 0x0044,
	0x0000, 0x0044,
	0x0000, 0x0000,
	0x0002, 0x0000,
	0x0000, 0x0000,
	0x0000, 0x0000,
	0x0008, 0x0000,
	0x0000, 0x0008,
	0x0097, 0x0100,
	0x0000, 0x0000,
	0x0050, 0x0050,
	0x0055, 0x0055,
	0x0055, 0x0000,
	0x0000, 0x0079,
	0x0079, 0x0079,
	0x0079, 0x0000,
	/* Remaining registers all zero */
};

static int wm8900_volatile_register(unsigned int reg)
{
	switch (reg) {
	case WM8900_REG_ID:
		return 1;
	default:
		return 0;
	}
}

static void wm8900_reset(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, WM8900_REG_RESET, 0);

	memcpy(codec->reg_cache, wm8900_reg_defaults,
	       sizeof(wm8900_reg_defaults));
}

static int wm8900_hp_event(struct snd_soc_dapm_widget *w,
			   struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 hpctl1 = snd_soc_read(codec, WM8900_REG_HPCTL1);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Clamp headphone outputs */
		hpctl1 = WM8900_REG_HPCTL1_HP_CLAMP_IP |
			WM8900_REG_HPCTL1_HP_CLAMP_OP;
		snd_soc_write(codec, WM8900_REG_HPCTL1, hpctl1);
		break;

	case SND_SOC_DAPM_POST_PMU:
		/* Enable the input stage */
		hpctl1 &= ~WM8900_REG_HPCTL1_HP_CLAMP_IP;
		hpctl1 |= WM8900_REG_HPCTL1_HP_SHORT |
			WM8900_REG_HPCTL1_HP_SHORT2 |
			WM8900_REG_HPCTL1_HP_IPSTAGE_ENA;
		snd_soc_write(codec, WM8900_REG_HPCTL1, hpctl1);

		msleep(400);

		/* Enable the output stage */
		hpctl1 &= ~WM8900_REG_HPCTL1_HP_CLAMP_OP;
		hpctl1 |= WM8900_REG_HPCTL1_HP_OPSTAGE_ENA;
		snd_soc_write(codec, WM8900_REG_HPCTL1, hpctl1);

		/* Remove the shorts */
		hpctl1 &= ~WM8900_REG_HPCTL1_HP_SHORT2;
		snd_soc_write(codec, WM8900_REG_HPCTL1, hpctl1);
		hpctl1 &= ~WM8900_REG_HPCTL1_HP_SHORT;
		snd_soc_write(codec, WM8900_REG_HPCTL1, hpctl1);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* Short the output */
		hpctl1 |= WM8900_REG_HPCTL1_HP_SHORT;
		snd_soc_write(codec, WM8900_REG_HPCTL1, hpctl1);

		/* Disable the output stage */
		hpctl1 &= ~WM8900_REG_HPCTL1_HP_OPSTAGE_ENA;
		snd_soc_write(codec, WM8900_REG_HPCTL1, hpctl1);

		/* Clamp the outputs and power down input */
		hpctl1 |= WM8900_REG_HPCTL1_HP_CLAMP_IP |
			WM8900_REG_HPCTL1_HP_CLAMP_OP;
		hpctl1 &= ~WM8900_REG_HPCTL1_HP_IPSTAGE_ENA;
		snd_soc_write(codec, WM8900_REG_HPCTL1, hpctl1);
		break;

	case SND_SOC_DAPM_POST_PMD:
		/* Disable everything */
		snd_soc_write(codec, WM8900_REG_HPCTL1, 0);
		break;

	default:
		BUG();
	}

	return 0;
}

static const DECLARE_TLV_DB_SCALE(out_pga_tlv, -5700, 100, 0);

static const DECLARE_TLV_DB_SCALE(out_mix_tlv, -1500, 300, 0);

static const DECLARE_TLV_DB_SCALE(in_boost_tlv, -1200, 600, 0);

static const DECLARE_TLV_DB_SCALE(in_pga_tlv, -1200, 100, 0);

static const DECLARE_TLV_DB_SCALE(dac_boost_tlv, 0, 600, 0);

static const DECLARE_TLV_DB_SCALE(dac_tlv, -7200, 75, 1);

static const DECLARE_TLV_DB_SCALE(adc_svol_tlv, -3600, 300, 0);

static const DECLARE_TLV_DB_SCALE(adc_tlv, -7200, 75, 1);

static const char *mic_bias_level_txt[] = { "0.9*AVDD", "0.65*AVDD" };

static const struct soc_enum mic_bias_level =
SOC_ENUM_SINGLE(WM8900_REG_INCTL, 8, 2, mic_bias_level_txt);

static const char *dac_mute_rate_txt[] = { "Fast", "Slow" };

static const struct soc_enum dac_mute_rate =
SOC_ENUM_SINGLE(WM8900_REG_DACCTRL, 7, 2, dac_mute_rate_txt);

static const char *dac_deemphasis_txt[] = {
	"Disabled", "32kHz", "44.1kHz", "48kHz"
};

static const struct soc_enum dac_deemphasis =
SOC_ENUM_SINGLE(WM8900_REG_DACCTRL, 4, 4, dac_deemphasis_txt);

static const char *adc_hpf_cut_txt[] = {
	"Hi-fi mode", "Voice mode 1", "Voice mode 2", "Voice mode 3"
};

static const struct soc_enum adc_hpf_cut =
SOC_ENUM_SINGLE(WM8900_REG_ADCCTRL, 5, 4, adc_hpf_cut_txt);

static const char *lr_txt[] = {
	"Left", "Right"
};

static const struct soc_enum aifl_src =
SOC_ENUM_SINGLE(WM8900_REG_AUDIO1, 15, 2, lr_txt);

static const struct soc_enum aifr_src =
SOC_ENUM_SINGLE(WM8900_REG_AUDIO1, 14, 2, lr_txt);

static const struct soc_enum dacl_src =
SOC_ENUM_SINGLE(WM8900_REG_AUDIO2, 15, 2, lr_txt);

static const struct soc_enum dacr_src =
SOC_ENUM_SINGLE(WM8900_REG_AUDIO2, 14, 2, lr_txt);

static const char *sidetone_txt[] = {
	"Disabled", "Left ADC", "Right ADC"
};

static const struct soc_enum dacl_sidetone =
SOC_ENUM_SINGLE(WM8900_REG_SIDETONE, 2, 3, sidetone_txt);

static const struct soc_enum dacr_sidetone =
SOC_ENUM_SINGLE(WM8900_REG_SIDETONE, 0, 3, sidetone_txt);

static const struct snd_kcontrol_new wm8900_snd_controls[] = {
SOC_ENUM("Mic Bias Level", mic_bias_level),

SOC_SINGLE_TLV("Left Input PGA Volume", WM8900_REG_LINVOL, 0, 31, 0,
	       in_pga_tlv),
SOC_SINGLE("Left Input PGA Switch", WM8900_REG_LINVOL, 6, 1, 1),
SOC_SINGLE("Left Input PGA ZC Switch", WM8900_REG_LINVOL, 7, 1, 0),

SOC_SINGLE_TLV("Right Input PGA Volume", WM8900_REG_RINVOL, 0, 31, 0,
	       in_pga_tlv),
SOC_SINGLE("Right Input PGA Switch", WM8900_REG_RINVOL, 6, 1, 1),
SOC_SINGLE("Right Input PGA ZC Switch", WM8900_REG_RINVOL, 7, 1, 0),

SOC_SINGLE("DAC Soft Mute Switch", WM8900_REG_DACCTRL, 6, 1, 1),
SOC_ENUM("DAC Mute Rate", dac_mute_rate),
SOC_SINGLE("DAC Mono Switch", WM8900_REG_DACCTRL, 9, 1, 0),
SOC_ENUM("DAC Deemphasis", dac_deemphasis),
SOC_SINGLE("DAC Sigma-Delta Modulator Clock Switch", WM8900_REG_DACCTRL,
	   12, 1, 0),

SOC_SINGLE("ADC HPF Switch", WM8900_REG_ADCCTRL, 8, 1, 0),
SOC_ENUM("ADC HPF Cut-Off", adc_hpf_cut),
SOC_DOUBLE("ADC Invert Switch", WM8900_REG_ADCCTRL, 1, 0, 1, 0),
SOC_SINGLE_TLV("Left ADC Sidetone Volume", WM8900_REG_SIDETONE, 9, 12, 0,
	       adc_svol_tlv),
SOC_SINGLE_TLV("Right ADC Sidetone Volume", WM8900_REG_SIDETONE, 5, 12, 0,
	       adc_svol_tlv),
SOC_ENUM("Left Digital Audio Source", aifl_src),
SOC_ENUM("Right Digital Audio Source", aifr_src),

SOC_SINGLE_TLV("DAC Input Boost Volume", WM8900_REG_AUDIO2, 10, 4, 0,
	       dac_boost_tlv),
SOC_ENUM("Left DAC Source", dacl_src),
SOC_ENUM("Right DAC Source", dacr_src),
SOC_ENUM("Left DAC Sidetone", dacl_sidetone),
SOC_ENUM("Right DAC Sidetone", dacr_sidetone),
SOC_DOUBLE("DAC Invert Switch", WM8900_REG_DACCTRL, 1, 0, 1, 0),

SOC_DOUBLE_R_TLV("Digital Playback Volume",
		 WM8900_REG_LDAC_DV, WM8900_REG_RDAC_DV,
		 1, 96, 0, dac_tlv),
SOC_DOUBLE_R_TLV("Digital Capture Volume",
		 WM8900_REG_LADC_DV, WM8900_REG_RADC_DV, 1, 119, 0, adc_tlv),

SOC_SINGLE_TLV("LINPUT3 Bypass Volume", WM8900_REG_LOUTMIXCTL1, 4, 7, 0,
	       out_mix_tlv),
SOC_SINGLE_TLV("RINPUT3 Bypass Volume", WM8900_REG_ROUTMIXCTL1, 4, 7, 0,
	       out_mix_tlv),
SOC_SINGLE_TLV("Left AUX Bypass Volume", WM8900_REG_AUXOUT_CTL, 4, 7, 0,
	       out_mix_tlv),
SOC_SINGLE_TLV("Right AUX Bypass Volume", WM8900_REG_AUXOUT_CTL, 0, 7, 0,
	       out_mix_tlv),

SOC_SINGLE_TLV("LeftIn to RightOut Mixer Volume", WM8900_REG_BYPASS1, 0, 7, 0,
	       out_mix_tlv),
SOC_SINGLE_TLV("LeftIn to LeftOut Mixer Volume", WM8900_REG_BYPASS1, 4, 7, 0,
	       out_mix_tlv),
SOC_SINGLE_TLV("RightIn to LeftOut Mixer Volume", WM8900_REG_BYPASS2, 0, 7, 0,
	       out_mix_tlv),
SOC_SINGLE_TLV("RightIn to RightOut Mixer Volume", WM8900_REG_BYPASS2, 4, 7, 0,
	       out_mix_tlv),

SOC_SINGLE_TLV("IN2L Boost Volume", WM8900_REG_INBOOSTMIX1, 0, 3, 0,
	       in_boost_tlv),
SOC_SINGLE_TLV("IN3L Boost Volume", WM8900_REG_INBOOSTMIX1, 4, 3, 0,
	       in_boost_tlv),
SOC_SINGLE_TLV("IN2R Boost Volume", WM8900_REG_INBOOSTMIX2, 0, 3, 0,
	       in_boost_tlv),
SOC_SINGLE_TLV("IN3R Boost Volume", WM8900_REG_INBOOSTMIX2, 4, 3, 0,
	       in_boost_tlv),
SOC_SINGLE_TLV("Left AUX Boost Volume", WM8900_REG_AUXBOOST, 4, 3, 0,
	       in_boost_tlv),
SOC_SINGLE_TLV("Right AUX Boost Volume", WM8900_REG_AUXBOOST, 0, 3, 0,
	       in_boost_tlv),

SOC_DOUBLE_R_TLV("LINEOUT1 Volume", WM8900_REG_LOUT1CTL, WM8900_REG_ROUT1CTL,
	       0, 63, 0, out_pga_tlv),
SOC_DOUBLE_R("LINEOUT1 Switch", WM8900_REG_LOUT1CTL, WM8900_REG_ROUT1CTL,
	     6, 1, 1),
SOC_DOUBLE_R("LINEOUT1 ZC Switch", WM8900_REG_LOUT1CTL, WM8900_REG_ROUT1CTL,
	     7, 1, 0),

SOC_DOUBLE_R_TLV("LINEOUT2 Volume",
		 WM8900_REG_LOUT2CTL, WM8900_REG_ROUT2CTL,
		 0, 63, 0, out_pga_tlv),
SOC_DOUBLE_R("LINEOUT2 Switch",
	     WM8900_REG_LOUT2CTL, WM8900_REG_ROUT2CTL, 6, 1, 1),
SOC_DOUBLE_R("LINEOUT2 ZC Switch",
	     WM8900_REG_LOUT2CTL, WM8900_REG_ROUT2CTL, 7, 1, 0),
SOC_SINGLE("LINEOUT2 LP -12dB", WM8900_REG_LOUTMIXCTL1,
	   0, 1, 1),

};

static const struct snd_kcontrol_new wm8900_dapm_loutput2_control =
SOC_DAPM_SINGLE("LINEOUT2L Switch", WM8900_REG_POWER3, 6, 1, 0);

static const struct snd_kcontrol_new wm8900_dapm_routput2_control =
SOC_DAPM_SINGLE("LINEOUT2R Switch", WM8900_REG_POWER3, 5, 1, 0);

static const struct snd_kcontrol_new wm8900_loutmix_controls[] = {
SOC_DAPM_SINGLE("LINPUT3 Bypass Switch", WM8900_REG_LOUTMIXCTL1, 7, 1, 0),
SOC_DAPM_SINGLE("AUX Bypass Switch", WM8900_REG_AUXOUT_CTL, 7, 1, 0),
SOC_DAPM_SINGLE("Left Input Mixer Switch", WM8900_REG_BYPASS1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Input Mixer Switch", WM8900_REG_BYPASS2, 3, 1, 0),
SOC_DAPM_SINGLE("DACL Switch", WM8900_REG_LOUTMIXCTL1, 8, 1, 0),
};

static const struct snd_kcontrol_new wm8900_routmix_controls[] = {
SOC_DAPM_SINGLE("RINPUT3 Bypass Switch", WM8900_REG_ROUTMIXCTL1, 7, 1, 0),
SOC_DAPM_SINGLE("AUX Bypass Switch", WM8900_REG_AUXOUT_CTL, 3, 1, 0),
SOC_DAPM_SINGLE("Left Input Mixer Switch", WM8900_REG_BYPASS1, 3, 1, 0),
SOC_DAPM_SINGLE("Right Input Mixer Switch", WM8900_REG_BYPASS2, 7, 1, 0),
SOC_DAPM_SINGLE("DACR Switch", WM8900_REG_ROUTMIXCTL1, 8, 1, 0),
};

static const struct snd_kcontrol_new wm8900_linmix_controls[] = {
SOC_DAPM_SINGLE("LINPUT2 Switch", WM8900_REG_INBOOSTMIX1, 2, 1, 1),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM8900_REG_INBOOSTMIX1, 6, 1, 1),
SOC_DAPM_SINGLE("AUX Switch", WM8900_REG_AUXBOOST, 6, 1, 1),
SOC_DAPM_SINGLE("Input PGA Switch", WM8900_REG_ADCPATH, 6, 1, 0),
};

static const struct snd_kcontrol_new wm8900_rinmix_controls[] = {
SOC_DAPM_SINGLE("RINPUT2 Switch", WM8900_REG_INBOOSTMIX2, 2, 1, 1),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM8900_REG_INBOOSTMIX2, 6, 1, 1),
SOC_DAPM_SINGLE("AUX Switch", WM8900_REG_AUXBOOST, 2, 1, 1),
SOC_DAPM_SINGLE("Input PGA Switch", WM8900_REG_ADCPATH, 2, 1, 0),
};

static const struct snd_kcontrol_new wm8900_linpga_controls[] = {
SOC_DAPM_SINGLE("LINPUT1 Switch", WM8900_REG_INCTL, 6, 1, 0),
SOC_DAPM_SINGLE("LINPUT2 Switch", WM8900_REG_INCTL, 5, 1, 0),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM8900_REG_INCTL, 4, 1, 0),
};

static const struct snd_kcontrol_new wm8900_rinpga_controls[] = {
SOC_DAPM_SINGLE("RINPUT1 Switch", WM8900_REG_INCTL, 2, 1, 0),
SOC_DAPM_SINGLE("RINPUT2 Switch", WM8900_REG_INCTL, 1, 1, 0),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM8900_REG_INCTL, 0, 1, 0),
};

static const char *wm9700_lp_mux[] = { "Disabled", "Enabled" };

static const struct soc_enum wm8900_lineout2_lp_mux =
SOC_ENUM_SINGLE(WM8900_REG_LOUTMIXCTL1, 1, 2, wm9700_lp_mux);

static const struct snd_kcontrol_new wm8900_lineout2_lp =
SOC_DAPM_ENUM("Route", wm8900_lineout2_lp_mux);

static const struct snd_soc_dapm_widget wm8900_dapm_widgets[] = {

/* Externally visible pins */
SND_SOC_DAPM_OUTPUT("LINEOUT1L"),
SND_SOC_DAPM_OUTPUT("LINEOUT1R"),
SND_SOC_DAPM_OUTPUT("LINEOUT2L"),
SND_SOC_DAPM_OUTPUT("LINEOUT2R"),
SND_SOC_DAPM_OUTPUT("HP_L"),
SND_SOC_DAPM_OUTPUT("HP_R"),

SND_SOC_DAPM_INPUT("RINPUT1"),
SND_SOC_DAPM_INPUT("LINPUT1"),
SND_SOC_DAPM_INPUT("RINPUT2"),
SND_SOC_DAPM_INPUT("LINPUT2"),
SND_SOC_DAPM_INPUT("RINPUT3"),
SND_SOC_DAPM_INPUT("LINPUT3"),
SND_SOC_DAPM_INPUT("AUX"),

SND_SOC_DAPM_VMID("VMID"),

/* Input */
SND_SOC_DAPM_MIXER("Left Input PGA", WM8900_REG_POWER2, 3, 0,
		   wm8900_linpga_controls,
		   ARRAY_SIZE(wm8900_linpga_controls)),
SND_SOC_DAPM_MIXER("Right Input PGA", WM8900_REG_POWER2, 2, 0,
		   wm8900_rinpga_controls,
		   ARRAY_SIZE(wm8900_rinpga_controls)),

SND_SOC_DAPM_MIXER("Left Input Mixer", WM8900_REG_POWER2, 5, 0,
		   wm8900_linmix_controls,
		   ARRAY_SIZE(wm8900_linmix_controls)),
SND_SOC_DAPM_MIXER("Right Input Mixer", WM8900_REG_POWER2, 4, 0,
		   wm8900_rinmix_controls,
		   ARRAY_SIZE(wm8900_rinmix_controls)),

SND_SOC_DAPM_MICBIAS("Mic Bias", WM8900_REG_POWER1, 4, 0),

SND_SOC_DAPM_ADC("ADCL", "Left HiFi Capture", WM8900_REG_POWER2, 1, 0),
SND_SOC_DAPM_ADC("ADCR", "Right HiFi Capture", WM8900_REG_POWER2, 0, 0),

/* Output */
SND_SOC_DAPM_DAC("DACL", "Left HiFi Playback", WM8900_REG_POWER3, 1, 0),
SND_SOC_DAPM_DAC("DACR", "Right HiFi Playback", WM8900_REG_POWER3, 0, 0),

SND_SOC_DAPM_PGA_E("Headphone Amplifier", WM8900_REG_POWER3, 7, 0, NULL, 0,
		   wm8900_hp_event,
		   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

SND_SOC_DAPM_PGA("LINEOUT1L PGA", WM8900_REG_POWER2, 8, 0, NULL, 0),
SND_SOC_DAPM_PGA("LINEOUT1R PGA", WM8900_REG_POWER2, 7, 0, NULL, 0),

SND_SOC_DAPM_MUX("LINEOUT2 LP", SND_SOC_NOPM, 0, 0, &wm8900_lineout2_lp),
SND_SOC_DAPM_PGA("LINEOUT2L PGA", WM8900_REG_POWER3, 6, 0, NULL, 0),
SND_SOC_DAPM_PGA("LINEOUT2R PGA", WM8900_REG_POWER3, 5, 0, NULL, 0),

SND_SOC_DAPM_MIXER("Left Output Mixer", WM8900_REG_POWER3, 3, 0,
		   wm8900_loutmix_controls,
		   ARRAY_SIZE(wm8900_loutmix_controls)),
SND_SOC_DAPM_MIXER("Right Output Mixer", WM8900_REG_POWER3, 2, 0,
		   wm8900_routmix_controls,
		   ARRAY_SIZE(wm8900_routmix_controls)),
};

/* Target, Path, Source */
static const struct snd_soc_dapm_route audio_map[] = {
/* Inputs */
{"Left Input PGA", "LINPUT1 Switch", "LINPUT1"},
{"Left Input PGA", "LINPUT2 Switch", "LINPUT2"},
{"Left Input PGA", "LINPUT3 Switch", "LINPUT3"},

{"Right Input PGA", "RINPUT1 Switch", "RINPUT1"},
{"Right Input PGA", "RINPUT2 Switch", "RINPUT2"},
{"Right Input PGA", "RINPUT3 Switch", "RINPUT3"},

{"Left Input Mixer", "LINPUT2 Switch", "LINPUT2"},
{"Left Input Mixer", "LINPUT3 Switch", "LINPUT3"},
{"Left Input Mixer", "AUX Switch", "AUX"},
{"Left Input Mixer", "Input PGA Switch", "Left Input PGA"},

{"Right Input Mixer", "RINPUT2 Switch", "RINPUT2"},
{"Right Input Mixer", "RINPUT3 Switch", "RINPUT3"},
{"Right Input Mixer", "AUX Switch", "AUX"},
{"Right Input Mixer", "Input PGA Switch", "Right Input PGA"},

{"ADCL", NULL, "Left Input Mixer"},
{"ADCR", NULL, "Right Input Mixer"},

/* Outputs */
{"LINEOUT1L", NULL, "LINEOUT1L PGA"},
{"LINEOUT1L PGA", NULL, "Left Output Mixer"},
{"LINEOUT1R", NULL, "LINEOUT1R PGA"},
{"LINEOUT1R PGA", NULL, "Right Output Mixer"},

{"LINEOUT2L PGA", NULL, "Left Output Mixer"},
{"LINEOUT2 LP", "Disabled", "LINEOUT2L PGA"},
{"LINEOUT2 LP", "Enabled", "Left Output Mixer"},
{"LINEOUT2L", NULL, "LINEOUT2 LP"},

{"LINEOUT2R PGA", NULL, "Right Output Mixer"},
{"LINEOUT2 LP", "Disabled", "LINEOUT2R PGA"},
{"LINEOUT2 LP", "Enabled", "Right Output Mixer"},
{"LINEOUT2R", NULL, "LINEOUT2 LP"},

{"Left Output Mixer", "LINPUT3 Bypass Switch", "LINPUT3"},
{"Left Output Mixer", "AUX Bypass Switch", "AUX"},
{"Left Output Mixer", "Left Input Mixer Switch", "Left Input Mixer"},
{"Left Output Mixer", "Right Input Mixer Switch", "Right Input Mixer"},
{"Left Output Mixer", "DACL Switch", "DACL"},

{"Right Output Mixer", "RINPUT3 Bypass Switch", "RINPUT3"},
{"Right Output Mixer", "AUX Bypass Switch", "AUX"},
{"Right Output Mixer", "Left Input Mixer Switch", "Left Input Mixer"},
{"Right Output Mixer", "Right Input Mixer Switch", "Right Input Mixer"},
{"Right Output Mixer", "DACR Switch", "DACR"},

/* Note that the headphone output stage needs to be connected
 * externally to LINEOUT2 via DC blocking capacitors.  Other
 * configurations are not supported.
 *
 * Note also that left and right headphone paths are treated as a
 * mono path.
 */
{"Headphone Amplifier", NULL, "LINEOUT2 LP"},
{"Headphone Amplifier", NULL, "LINEOUT2 LP"},
{"HP_L", NULL, "Headphone Amplifier"},
{"HP_R", NULL, "Headphone Amplifier"},
};

static int wm8900_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, wm8900_dapm_widgets,
				  ARRAY_SIZE(wm8900_dapm_widgets));

	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	return 0;
}

static int wm8900_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	u16 reg;

	reg = snd_soc_read(codec, WM8900_REG_AUDIO1) & ~0x60;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		reg |= 0x20;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		reg |= 0x40;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		reg |= 0x60;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, WM8900_REG_AUDIO1, reg);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg = snd_soc_read(codec, WM8900_REG_DACCTRL);

		if (params_rate(params) <= 24000)
			reg |= WM8900_REG_DACCTRL_DAC_SB_FILT;
		else
			reg &= ~WM8900_REG_DACCTRL_DAC_SB_FILT;

		snd_soc_write(codec, WM8900_REG_DACCTRL, reg);
	}

	return 0;
}

/* FLL divisors */
struct _fll_div {
	u16 fll_ratio;
	u16 fllclk_div;
	u16 fll_slow_lock_ref;
	u16 n;
	u16 k;
};

/* The size in bits of the FLL divide multiplied by 10
 * to allow rounding later */
#define FIXED_FLL_SIZE ((1 << 16) * 10)

static int fll_factors(struct _fll_div *fll_div, unsigned int Fref,
		       unsigned int Fout)
{
	u64 Kpart;
	unsigned int K, Ndiv, Nmod, target;
	unsigned int div;

	BUG_ON(!Fout);

	/* The FLL must run at 90-100MHz which is then scaled down to
	 * the output value by FLLCLK_DIV. */
	target = Fout;
	div = 1;
	while (target < 90000000) {
		div *= 2;
		target *= 2;
	}

	if (target > 100000000)
		printk(KERN_WARNING "wm8900: FLL rate %u out of range, Fref=%u"
		       " Fout=%u\n", target, Fref, Fout);
	if (div > 32) {
		printk(KERN_ERR "wm8900: Invalid FLL division rate %u, "
		       "Fref=%u, Fout=%u, target=%u\n",
		       div, Fref, Fout, target);
		return -EINVAL;
	}

	fll_div->fllclk_div = div >> 2;

	if (Fref < 48000)
		fll_div->fll_slow_lock_ref = 1;
	else
		fll_div->fll_slow_lock_ref = 0;

	Ndiv = target / Fref;

	if (Fref < 1000000)
		fll_div->fll_ratio = 8;
	else
		fll_div->fll_ratio = 1;

	fll_div->n = Ndiv / fll_div->fll_ratio;
	Nmod = (target / fll_div->fll_ratio) % Fref;

	/* Calculate fractional part - scale up so we can round. */
	Kpart = FIXED_FLL_SIZE * (long long)Nmod;

	do_div(Kpart, Fref);

	K = Kpart & 0xFFFFFFFF;

	if ((K % 10) >= 5)
		K += 5;

	/* Move down to proper range now rounding is done */
	fll_div->k = K / 10;

	BUG_ON(target != Fout * (fll_div->fllclk_div << 2));
	BUG_ON(!K && target != Fref * fll_div->fll_ratio * fll_div->n);

	return 0;
}

static int wm8900_set_fll(struct snd_soc_codec *codec,
	int fll_id, unsigned int freq_in, unsigned int freq_out)
{
	struct wm8900_priv *wm8900 = snd_soc_codec_get_drvdata(codec);
	struct _fll_div fll_div;
	unsigned int reg;

	if (wm8900->fll_in == freq_in && wm8900->fll_out == freq_out)
		return 0;

	/* The digital side should be disabled during any change. */
	reg = snd_soc_read(codec, WM8900_REG_POWER1);
	snd_soc_write(codec, WM8900_REG_POWER1,
		     reg & (~WM8900_REG_POWER1_FLL_ENA));

	/* Disable the FLL? */
	if (!freq_in || !freq_out) {
		reg = snd_soc_read(codec, WM8900_REG_CLOCKING1);
		snd_soc_write(codec, WM8900_REG_CLOCKING1,
			     reg & (~WM8900_REG_CLOCKING1_MCLK_SRC));

		reg = snd_soc_read(codec, WM8900_REG_FLLCTL1);
		snd_soc_write(codec, WM8900_REG_FLLCTL1,
			     reg & (~WM8900_REG_FLLCTL1_OSC_ENA));

		wm8900->fll_in = freq_in;
		wm8900->fll_out = freq_out;

		return 0;
	}

	if (fll_factors(&fll_div, freq_in, freq_out) != 0)
		goto reenable;

	wm8900->fll_in = freq_in;
	wm8900->fll_out = freq_out;

	/* The osclilator *MUST* be enabled before we enable the
	 * digital circuit. */
	snd_soc_write(codec, WM8900_REG_FLLCTL1,
		     fll_div.fll_ratio | WM8900_REG_FLLCTL1_OSC_ENA);

	snd_soc_write(codec, WM8900_REG_FLLCTL4, fll_div.n >> 5);
	snd_soc_write(codec, WM8900_REG_FLLCTL5,
		     (fll_div.fllclk_div << 6) | (fll_div.n & 0x1f));

	if (fll_div.k) {
		snd_soc_write(codec, WM8900_REG_FLLCTL2,
			     (fll_div.k >> 8) | 0x100);
		snd_soc_write(codec, WM8900_REG_FLLCTL3, fll_div.k & 0xff);
	} else
		snd_soc_write(codec, WM8900_REG_FLLCTL2, 0);

	if (fll_div.fll_slow_lock_ref)
		snd_soc_write(codec, WM8900_REG_FLLCTL6,
			     WM8900_REG_FLLCTL6_FLL_SLOW_LOCK_REF);
	else
		snd_soc_write(codec, WM8900_REG_FLLCTL6, 0);

	reg = snd_soc_read(codec, WM8900_REG_POWER1);
	snd_soc_write(codec, WM8900_REG_POWER1,
		     reg | WM8900_REG_POWER1_FLL_ENA);

reenable:
	reg = snd_soc_read(codec, WM8900_REG_CLOCKING1);
	snd_soc_write(codec, WM8900_REG_CLOCKING1,
		     reg | WM8900_REG_CLOCKING1_MCLK_SRC);

	return 0;
}

static int wm8900_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
		int source, unsigned int freq_in, unsigned int freq_out)
{
	return wm8900_set_fll(codec_dai->codec, pll_id, freq_in, freq_out);
}

static int wm8900_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
				 int div_id, int div)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	unsigned int reg;

	switch (div_id) {
	case WM8900_BCLK_DIV:
		reg = snd_soc_read(codec, WM8900_REG_CLOCKING1);
		snd_soc_write(codec, WM8900_REG_CLOCKING1,
			     div | (reg & WM8900_REG_CLOCKING1_BCLK_MASK));
		break;
	case WM8900_OPCLK_DIV:
		reg = snd_soc_read(codec, WM8900_REG_CLOCKING1);
		snd_soc_write(codec, WM8900_REG_CLOCKING1,
			     div | (reg & WM8900_REG_CLOCKING1_OPCLK_MASK));
		break;
	case WM8900_DAC_LRCLK:
		reg = snd_soc_read(codec, WM8900_REG_AUDIO4);
		snd_soc_write(codec, WM8900_REG_AUDIO4,
			     div | (reg & WM8900_LRC_MASK));
		break;
	case WM8900_ADC_LRCLK:
		reg = snd_soc_read(codec, WM8900_REG_AUDIO3);
		snd_soc_write(codec, WM8900_REG_AUDIO3,
			     div | (reg & WM8900_LRC_MASK));
		break;
	case WM8900_DAC_CLKDIV:
		reg = snd_soc_read(codec, WM8900_REG_CLOCKING2);
		snd_soc_write(codec, WM8900_REG_CLOCKING2,
			     div | (reg & WM8900_REG_CLOCKING2_DAC_CLKDIV));
		break;
	case WM8900_ADC_CLKDIV:
		reg = snd_soc_read(codec, WM8900_REG_CLOCKING2);
		snd_soc_write(codec, WM8900_REG_CLOCKING2,
			     div | (reg & WM8900_REG_CLOCKING2_ADC_CLKDIV));
		break;
	case WM8900_LRCLK_MODE:
		reg = snd_soc_read(codec, WM8900_REG_DACCTRL);
		snd_soc_write(codec, WM8900_REG_DACCTRL,
			     div | (reg & WM8900_REG_DACCTRL_AIF_LRCLKRATE));
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int wm8900_set_dai_fmt(struct snd_soc_dai *codec_dai,
			      unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	unsigned int clocking1, aif1, aif3, aif4;

	clocking1 = snd_soc_read(codec, WM8900_REG_CLOCKING1);
	aif1 = snd_soc_read(codec, WM8900_REG_AUDIO1);
	aif3 = snd_soc_read(codec, WM8900_REG_AUDIO3);
	aif4 = snd_soc_read(codec, WM8900_REG_AUDIO4);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		clocking1 &= ~WM8900_REG_CLOCKING1_BCLK_DIR;
		aif3 &= ~WM8900_REG_AUDIO3_ADCLRC_DIR;
		aif4 &= ~WM8900_REG_AUDIO4_DACLRC_DIR;
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
		clocking1 &= ~WM8900_REG_CLOCKING1_BCLK_DIR;
		aif3 |= WM8900_REG_AUDIO3_ADCLRC_DIR;
		aif4 |= WM8900_REG_AUDIO4_DACLRC_DIR;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		clocking1 |= WM8900_REG_CLOCKING1_BCLK_DIR;
		aif3 |= WM8900_REG_AUDIO3_ADCLRC_DIR;
		aif4 |= WM8900_REG_AUDIO4_DACLRC_DIR;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		clocking1 |= WM8900_REG_CLOCKING1_BCLK_DIR;
		aif3 &= ~WM8900_REG_AUDIO3_ADCLRC_DIR;
		aif4 &= ~WM8900_REG_AUDIO4_DACLRC_DIR;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		aif1 |= WM8900_REG_AUDIO1_AIF_FMT_MASK;
		aif1 &= ~WM8900_REG_AUDIO1_LRCLK_INV;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		aif1 |= WM8900_REG_AUDIO1_AIF_FMT_MASK;
		aif1 |= WM8900_REG_AUDIO1_LRCLK_INV;
		break;
	case SND_SOC_DAIFMT_I2S:
		aif1 &= ~WM8900_REG_AUDIO1_AIF_FMT_MASK;
		aif1 |= 0x10;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		aif1 &= ~WM8900_REG_AUDIO1_AIF_FMT_MASK;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		aif1 &= ~WM8900_REG_AUDIO1_AIF_FMT_MASK;
		aif1 |= 0x8;
		break;
	default:
		return -EINVAL;
	}

	/* Clock inversion */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		/* frame inversion not valid for DSP modes */
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			aif1 &= ~WM8900_REG_AUDIO1_BCLK_INV;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			aif1 |= WM8900_REG_AUDIO1_BCLK_INV;
			break;
		default:
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_I2S:
	case SND_SOC_DAIFMT_RIGHT_J:
	case SND_SOC_DAIFMT_LEFT_J:
		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			aif1 &= ~WM8900_REG_AUDIO1_BCLK_INV;
			aif1 &= ~WM8900_REG_AUDIO1_LRCLK_INV;
			break;
		case SND_SOC_DAIFMT_IB_IF:
			aif1 |= WM8900_REG_AUDIO1_BCLK_INV;
			aif1 |= WM8900_REG_AUDIO1_LRCLK_INV;
			break;
		case SND_SOC_DAIFMT_IB_NF:
			aif1 |= WM8900_REG_AUDIO1_BCLK_INV;
			aif1 &= ~WM8900_REG_AUDIO1_LRCLK_INV;
			break;
		case SND_SOC_DAIFMT_NB_IF:
			aif1 &= ~WM8900_REG_AUDIO1_BCLK_INV;
			aif1 |= WM8900_REG_AUDIO1_LRCLK_INV;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, WM8900_REG_CLOCKING1, clocking1);
	snd_soc_write(codec, WM8900_REG_AUDIO1, aif1);
	snd_soc_write(codec, WM8900_REG_AUDIO3, aif3);
	snd_soc_write(codec, WM8900_REG_AUDIO4, aif4);

	return 0;
}

static int wm8900_digital_mute(struct snd_soc_dai *codec_dai, int mute)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;

	reg = snd_soc_read(codec, WM8900_REG_DACCTRL);

	if (mute)
		reg |= WM8900_REG_DACCTRL_MUTE;
	else
		reg &= ~WM8900_REG_DACCTRL_MUTE;

	snd_soc_write(codec, WM8900_REG_DACCTRL, reg);

	return 0;
}

#define WM8900_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		      SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
		      SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

#define WM8900_PCM_FORMATS \
	(SNDRV_PCM_FORMAT_S16_LE | SNDRV_PCM_FORMAT_S20_3LE | \
	 SNDRV_PCM_FORMAT_S24_LE)

static struct snd_soc_dai_ops wm8900_dai_ops = {
	.hw_params	= wm8900_hw_params,
	.set_clkdiv	= wm8900_set_dai_clkdiv,
	.set_pll	= wm8900_set_dai_pll,
	.set_fmt	= wm8900_set_dai_fmt,
	.digital_mute	= wm8900_digital_mute,
};

struct snd_soc_dai wm8900_dai = {
	.name = "WM8900 HiFi",
	.playback = {
		.stream_name = "HiFi Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8900_RATES,
		.formats = WM8900_PCM_FORMATS,
	},
	.capture = {
		.stream_name = "HiFi Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8900_RATES,
		.formats = WM8900_PCM_FORMATS,
	 },
	.ops = &wm8900_dai_ops,
};
EXPORT_SYMBOL_GPL(wm8900_dai);

static int wm8900_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	u16 reg;

	switch (level) {
	case SND_SOC_BIAS_ON:
		/* Enable thermal shutdown */
		reg = snd_soc_read(codec, WM8900_REG_GPIO);
		snd_soc_write(codec, WM8900_REG_GPIO,
			     reg | WM8900_REG_GPIO_TEMP_ENA);
		reg = snd_soc_read(codec, WM8900_REG_ADDCTL);
		snd_soc_write(codec, WM8900_REG_ADDCTL,
			     reg | WM8900_REG_ADDCTL_TEMP_SD);
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		/* Charge capacitors if initial power up */
		if (codec->bias_level == SND_SOC_BIAS_OFF) {
			/* STARTUP_BIAS_ENA on */
			snd_soc_write(codec, WM8900_REG_POWER1,
				     WM8900_REG_POWER1_STARTUP_BIAS_ENA);

			/* Startup bias mode */
			snd_soc_write(codec, WM8900_REG_ADDCTL,
				     WM8900_REG_ADDCTL_BIAS_SRC |
				     WM8900_REG_ADDCTL_VMID_SOFTST);

			/* VMID 2x50k */
			snd_soc_write(codec, WM8900_REG_POWER1,
				     WM8900_REG_POWER1_STARTUP_BIAS_ENA | 0x1);

			/* Allow capacitors to charge */
			schedule_timeout_interruptible(msecs_to_jiffies(400));

			/* Enable bias */
			snd_soc_write(codec, WM8900_REG_POWER1,
				     WM8900_REG_POWER1_STARTUP_BIAS_ENA |
				     WM8900_REG_POWER1_BIAS_ENA | 0x1);

			snd_soc_write(codec, WM8900_REG_ADDCTL, 0);

			snd_soc_write(codec, WM8900_REG_POWER1,
				     WM8900_REG_POWER1_BIAS_ENA | 0x1);
		}

		reg = snd_soc_read(codec, WM8900_REG_POWER1);
		snd_soc_write(codec, WM8900_REG_POWER1,
			     (reg & WM8900_REG_POWER1_FLL_ENA) |
			     WM8900_REG_POWER1_BIAS_ENA | 0x1);
		snd_soc_write(codec, WM8900_REG_POWER2,
			     WM8900_REG_POWER2_SYSCLK_ENA);
		snd_soc_write(codec, WM8900_REG_POWER3, 0);
		break;

	case SND_SOC_BIAS_OFF:
		/* Startup bias enable */
		reg = snd_soc_read(codec, WM8900_REG_POWER1);
		snd_soc_write(codec, WM8900_REG_POWER1,
			     reg & WM8900_REG_POWER1_STARTUP_BIAS_ENA);
		snd_soc_write(codec, WM8900_REG_ADDCTL,
			     WM8900_REG_ADDCTL_BIAS_SRC |
			     WM8900_REG_ADDCTL_VMID_SOFTST);

		/* Discharge caps */
		snd_soc_write(codec, WM8900_REG_POWER1,
			     WM8900_REG_POWER1_STARTUP_BIAS_ENA);
		schedule_timeout_interruptible(msecs_to_jiffies(500));

		/* Remove clamp */
		snd_soc_write(codec, WM8900_REG_HPCTL1, 0);

		/* Power down */
		snd_soc_write(codec, WM8900_REG_ADDCTL, 0);
		snd_soc_write(codec, WM8900_REG_POWER1, 0);
		snd_soc_write(codec, WM8900_REG_POWER2, 0);
		snd_soc_write(codec, WM8900_REG_POWER3, 0);

		/* Need to let things settle before stopping the clock
		 * to ensure that restart works, see "Stopping the
		 * master clock" in the datasheet. */
		schedule_timeout_interruptible(msecs_to_jiffies(1));
		snd_soc_write(codec, WM8900_REG_POWER2,
			     WM8900_REG_POWER2_SYSCLK_ENA);
		break;
	}
	codec->bias_level = level;
	return 0;
}

static int wm8900_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	struct wm8900_priv *wm8900 = snd_soc_codec_get_drvdata(codec);
	int fll_out = wm8900->fll_out;
	int fll_in  = wm8900->fll_in;
	int ret;

	/* Stop the FLL in an orderly fashion */
	ret = wm8900_set_fll(codec, 0, 0, 0);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to stop FLL\n");
		return ret;
	}

	wm8900->fll_out = fll_out;
	wm8900->fll_in = fll_in;

	wm8900_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int wm8900_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	struct wm8900_priv *wm8900 = snd_soc_codec_get_drvdata(codec);
	u16 *cache;
	int i, ret;

	cache = kmemdup(codec->reg_cache, sizeof(wm8900_reg_defaults),
			GFP_KERNEL);

	wm8900_reset(codec);
	wm8900_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Restart the FLL? */
	if (wm8900->fll_out) {
		int fll_out = wm8900->fll_out;
		int fll_in  = wm8900->fll_in;

		wm8900->fll_in = 0;
		wm8900->fll_out = 0;

		ret = wm8900_set_fll(codec, 0, fll_in, fll_out);
		if (ret != 0) {
			dev_err(&pdev->dev, "Failed to restart FLL\n");
			return ret;
		}
	}

	if (cache) {
		for (i = 0; i < WM8900_MAXREG; i++)
			snd_soc_write(codec, i, cache[i]);
		kfree(cache);
	} else
		dev_err(&pdev->dev, "Unable to allocate register cache\n");

	return 0;
}

static struct snd_soc_codec *wm8900_codec;

static __devinit int wm8900_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct wm8900_priv *wm8900;
	struct snd_soc_codec *codec;
	unsigned int reg;
	int ret;

	wm8900 = kzalloc(sizeof(struct wm8900_priv), GFP_KERNEL);
	if (wm8900 == NULL)
		return -ENOMEM;

	codec = &wm8900->codec;
	snd_soc_codec_set_drvdata(codec, wm8900);
	codec->reg_cache = &wm8900->reg_cache[0];
	codec->reg_cache_size = WM8900_MAXREG;

	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	codec->name = "WM8900";
	codec->owner = THIS_MODULE;
	codec->dai = &wm8900_dai;
	codec->num_dai = 1;
	codec->control_data = i2c;
	codec->set_bias_level = wm8900_set_bias_level;
	codec->volatile_register = wm8900_volatile_register;
	codec->dev = &i2c->dev;

	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_I2C);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to set cache I/O: %d\n", ret);
		goto err;
	}

	reg = snd_soc_read(codec, WM8900_REG_ID);
	if (reg != 0x8900) {
		dev_err(&i2c->dev, "Device is not a WM8900 - ID %x\n", reg);
		ret = -ENODEV;
		goto err;
	}

	wm8900_reset(codec);

	/* Turn the chip on */
	wm8900_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	/* Latch the volume update bits */
	snd_soc_write(codec, WM8900_REG_LINVOL,
		      snd_soc_read(codec, WM8900_REG_LINVOL) | 0x100);
	snd_soc_write(codec, WM8900_REG_RINVOL,
		      snd_soc_read(codec, WM8900_REG_RINVOL) | 0x100);
	snd_soc_write(codec, WM8900_REG_LOUT1CTL,
		      snd_soc_read(codec, WM8900_REG_LOUT1CTL) | 0x100);
	snd_soc_write(codec, WM8900_REG_ROUT1CTL,
		      snd_soc_read(codec, WM8900_REG_ROUT1CTL) | 0x100);
	snd_soc_write(codec, WM8900_REG_LOUT2CTL,
		      snd_soc_read(codec, WM8900_REG_LOUT2CTL) | 0x100);
	snd_soc_write(codec, WM8900_REG_ROUT2CTL,
		      snd_soc_read(codec, WM8900_REG_ROUT2CTL) | 0x100);
	snd_soc_write(codec, WM8900_REG_LDAC_DV,
		      snd_soc_read(codec, WM8900_REG_LDAC_DV) | 0x100);
	snd_soc_write(codec, WM8900_REG_RDAC_DV,
		      snd_soc_read(codec, WM8900_REG_RDAC_DV) | 0x100);
	snd_soc_write(codec, WM8900_REG_LADC_DV,
		      snd_soc_read(codec, WM8900_REG_LADC_DV) | 0x100);
	snd_soc_write(codec, WM8900_REG_RADC_DV,
		      snd_soc_read(codec, WM8900_REG_RADC_DV) | 0x100);

	/* Set the DAC and mixer output bias */
	snd_soc_write(codec, WM8900_REG_OUTBIASCTL, 0x81);

	wm8900_dai.dev = &i2c->dev;

	wm8900_codec = codec;

	ret = snd_soc_register_codec(codec);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);
		goto err;
	}

	ret = snd_soc_register_dai(&wm8900_dai);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to register DAI: %d\n", ret);
		goto err_codec;
	}

	return ret;

err_codec:
	snd_soc_unregister_codec(codec);
err:
	kfree(wm8900);
	wm8900_codec = NULL;
	return ret;
}

static __devexit int wm8900_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_dai(&wm8900_dai);
	snd_soc_unregister_codec(wm8900_codec);

	wm8900_set_bias_level(wm8900_codec, SND_SOC_BIAS_OFF);

	wm8900_dai.dev = NULL;
	kfree(snd_soc_codec_get_drvdata(wm8900_codec));
	wm8900_codec = NULL;

	return 0;
}

static const struct i2c_device_id wm8900_i2c_id[] = {
	{ "wm8900", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, wm8900_i2c_id);

static struct i2c_driver wm8900_i2c_driver = {
	.driver = {
		.name = "WM8900",
		.owner = THIS_MODULE,
	},
	.probe = wm8900_i2c_probe,
	.remove = __devexit_p(wm8900_i2c_remove),
	.id_table = wm8900_i2c_id,
};

static int wm8900_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	int ret = 0;

	if (!wm8900_codec) {
		dev_err(&pdev->dev, "I2C client not yet instantiated\n");
		return -ENODEV;
	}

	codec = wm8900_codec;
	socdev->card->codec = codec;

	/* Register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register new PCMs\n");
		goto pcm_err;
	}

	snd_soc_add_controls(codec, wm8900_snd_controls,
				ARRAY_SIZE(wm8900_snd_controls));
	wm8900_add_widgets(codec);

pcm_err:
	return ret;
}

/* power down chip */
static int wm8900_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm8900 = {
	.probe = 	wm8900_probe,
	.remove = 	wm8900_remove,
	.suspend = 	wm8900_suspend,
	.resume =	wm8900_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_wm8900);

static int __init wm8900_modinit(void)
{
	return i2c_add_driver(&wm8900_i2c_driver);
}
module_init(wm8900_modinit);

static void __exit wm8900_exit(void)
{
	i2c_del_driver(&wm8900_i2c_driver);
}
module_exit(wm8900_exit);

MODULE_DESCRIPTION("ASoC WM8900 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfonmicro.com>");
MODULE_LICENSE("GPL");
