/*
 *  wm1133-ev1.c - Audio for WM1133-EV1 on i.MX31ADS
 *
 *  Copyright (c) 2010 Wolfson Microelectronics plc
 *  Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  Based on an earlier driver for the same hardware by Liam Girdwood.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <mach/audmux.h>

#include "imx-ssi.h"
#include "../codecs/wm8350.h"

/* There is a silicon mic on the board optionally connected via a solder pad
 * SP1.  Define this to enable it.
 */
#undef USE_SIMIC

struct _wm8350_audio {
	unsigned int channels;
	snd_pcm_format_t format;
	unsigned int rate;
	unsigned int sysclk;
	unsigned int bclkdiv;
	unsigned int clkdiv;
	unsigned int lr_rate;
};

/* in order of power consumption per rate (lowest first) */
static const struct _wm8350_audio wm8350_audio[] = {
	/* 16bit mono modes */
	{1, SNDRV_PCM_FORMAT_S16_LE, 8000, 12288000 >> 1,
	 WM8350_BCLK_DIV_48, WM8350_DACDIV_3, 16,},

	/* 16 bit stereo modes */
	{2, SNDRV_PCM_FORMAT_S16_LE, 8000, 12288000,
	 WM8350_BCLK_DIV_48, WM8350_DACDIV_6, 32,},
	{2, SNDRV_PCM_FORMAT_S16_LE, 16000, 12288000,
	 WM8350_BCLK_DIV_24, WM8350_DACDIV_3, 32,},
	{2, SNDRV_PCM_FORMAT_S16_LE, 32000, 12288000,
	 WM8350_BCLK_DIV_12, WM8350_DACDIV_1_5, 32,},
	{2, SNDRV_PCM_FORMAT_S16_LE, 48000, 12288000,
	 WM8350_BCLK_DIV_8, WM8350_DACDIV_1, 32,},
	{2, SNDRV_PCM_FORMAT_S16_LE, 96000, 24576000,
	 WM8350_BCLK_DIV_8, WM8350_DACDIV_1, 32,},
	{2, SNDRV_PCM_FORMAT_S16_LE, 11025, 11289600,
	 WM8350_BCLK_DIV_32, WM8350_DACDIV_4, 32,},
	{2, SNDRV_PCM_FORMAT_S16_LE, 22050, 11289600,
	 WM8350_BCLK_DIV_16, WM8350_DACDIV_2, 32,},
	{2, SNDRV_PCM_FORMAT_S16_LE, 44100, 11289600,
	 WM8350_BCLK_DIV_8, WM8350_DACDIV_1, 32,},
	{2, SNDRV_PCM_FORMAT_S16_LE, 88200, 22579200,
	 WM8350_BCLK_DIV_8, WM8350_DACDIV_1, 32,},

	/* 24bit stereo modes */
	{2, SNDRV_PCM_FORMAT_S24_LE, 48000, 12288000,
	 WM8350_BCLK_DIV_4, WM8350_DACDIV_1, 64,},
	{2, SNDRV_PCM_FORMAT_S24_LE, 96000, 24576000,
	 WM8350_BCLK_DIV_4, WM8350_DACDIV_1, 64,},
	{2, SNDRV_PCM_FORMAT_S24_LE, 44100, 11289600,
	 WM8350_BCLK_DIV_4, WM8350_DACDIV_1, 64,},
	{2, SNDRV_PCM_FORMAT_S24_LE, 88200, 22579200,
	 WM8350_BCLK_DIV_4, WM8350_DACDIV_1, 64,},
};

static int wm1133_ev1_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int i, found = 0;
	snd_pcm_format_t format = params_format(params);
	unsigned int rate = params_rate(params);
	unsigned int channels = params_channels(params);
	u32 dai_format;

	/* find the correct audio parameters */
	for (i = 0; i < ARRAY_SIZE(wm8350_audio); i++) {
		if (rate == wm8350_audio[i].rate &&
		    format == wm8350_audio[i].format &&
		    channels == wm8350_audio[i].channels) {
			found = 1;
			break;
		}
	}
	if (!found)
		return -EINVAL;

	/* codec FLL input is 14.75 MHz from MCLK */
	snd_soc_dai_set_pll(codec_dai, 0, 0, 14750000, wm8350_audio[i].sysclk);

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBM_CFM;

	/* set codec DAI configuration */
	snd_soc_dai_set_fmt(codec_dai, dai_format);

	/* set cpu DAI configuration */
	snd_soc_dai_set_fmt(cpu_dai, dai_format);

	/* TODO: The SSI driver should figure this out for us */
	switch (channels) {
	case 2:
		snd_soc_dai_set_tdm_slot(cpu_dai, 0xffffffc, 0xffffffc, 2, 0);
		break;
	case 1:
		snd_soc_dai_set_tdm_slot(cpu_dai, 0xffffffe, 0xffffffe, 1, 0);
		break;
	default:
		return -EINVAL;
	}

	/* set MCLK as the codec system clock for DAC and ADC */
	snd_soc_dai_set_sysclk(codec_dai, WM8350_MCLK_SEL_PLL_MCLK,
			       wm8350_audio[i].sysclk, SND_SOC_CLOCK_IN);

	/* set codec BCLK division for sample rate */
	snd_soc_dai_set_clkdiv(codec_dai, WM8350_BCLK_CLKDIV,
			       wm8350_audio[i].bclkdiv);

	/* DAI is synchronous and clocked with DAC LRCLK & ADC LRC */
	snd_soc_dai_set_clkdiv(codec_dai,
			       WM8350_DACLR_CLKDIV, wm8350_audio[i].lr_rate);
	snd_soc_dai_set_clkdiv(codec_dai,
			       WM8350_ADCLR_CLKDIV, wm8350_audio[i].lr_rate);

	/* now configure DAC and ADC clocks */
	snd_soc_dai_set_clkdiv(codec_dai,
			       WM8350_DAC_CLKDIV, wm8350_audio[i].clkdiv);

	snd_soc_dai_set_clkdiv(codec_dai,
			       WM8350_ADC_CLKDIV, wm8350_audio[i].clkdiv);

	return 0;
}

static struct snd_soc_ops wm1133_ev1_ops = {
	.hw_params = wm1133_ev1_hw_params,
};

static const struct snd_soc_dapm_widget wm1133_ev1_widgets[] = {
#ifdef USE_SIMIC
	SND_SOC_DAPM_MIC("SiMIC", NULL),
#endif
	SND_SOC_DAPM_MIC("Mic1 Jack", NULL),
	SND_SOC_DAPM_MIC("Mic2 Jack", NULL),
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
	SND_SOC_DAPM_LINE("Line Out Jack", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
};

/* imx32ads soc_card audio map */
static const struct snd_soc_dapm_route wm1133_ev1_map[] = {

#ifdef USE_SIMIC
	/* SiMIC --> IN1LN (with automatic bias) via SP1 */
	{ "IN1LN", NULL, "Mic Bias" },
	{ "Mic Bias", NULL, "SiMIC" },
#endif

	/* Mic 1 Jack --> IN1LN and IN1LP (with automatic bias) */
	{ "IN1LN", NULL, "Mic Bias" },
	{ "IN1LP", NULL, "Mic1 Jack" },
	{ "Mic Bias", NULL, "Mic1 Jack" },

	/* Mic 2 Jack --> IN1RN and IN1RP (with automatic bias) */
	{ "IN1RN", NULL, "Mic Bias" },
	{ "IN1RP", NULL, "Mic2 Jack" },
	{ "Mic Bias", NULL, "Mic2 Jack" },

	/* Line in Jack --> AUX (L+R) */
	{ "IN3R", NULL, "Line In Jack" },
	{ "IN3L", NULL, "Line In Jack" },

	/* Out1 --> Headphone Jack */
	{ "Headphone Jack", NULL, "OUT1R" },
	{ "Headphone Jack", NULL, "OUT1L" },

	/* Out1 --> Line Out Jack */
	{ "Line Out Jack", NULL, "OUT2R" },
	{ "Line Out Jack", NULL, "OUT2L" },
};

static struct snd_soc_jack hp_jack;

static struct snd_soc_jack_pin hp_jack_pins[] = {
	{ .pin = "Headphone Jack", .mask = SND_JACK_HEADPHONE },
};

static struct snd_soc_jack mic_jack;

static struct snd_soc_jack_pin mic_jack_pins[] = {
	{ .pin = "Mic1 Jack", .mask = SND_JACK_MICROPHONE },
	{ .pin = "Mic2 Jack", .mask = SND_JACK_MICROPHONE },
};

static int wm1133_ev1_init(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->socdev->card;

	snd_soc_dapm_new_controls(codec, wm1133_ev1_widgets,
				  ARRAY_SIZE(wm1133_ev1_widgets));

	snd_soc_dapm_add_routes(codec, wm1133_ev1_map,
				ARRAY_SIZE(wm1133_ev1_map));

	/* Headphone jack detection */
	snd_soc_jack_new(card, "Headphone", SND_JACK_HEADPHONE, &hp_jack);
	snd_soc_jack_add_pins(&hp_jack, ARRAY_SIZE(hp_jack_pins),
			      hp_jack_pins);
	wm8350_hp_jack_detect(codec, WM8350_JDR, &hp_jack, SND_JACK_HEADPHONE);

	/* Microphone jack detection */
	snd_soc_jack_new(card, "Microphone",
			 SND_JACK_MICROPHONE | SND_JACK_BTN_0, &mic_jack);
	snd_soc_jack_add_pins(&mic_jack, ARRAY_SIZE(mic_jack_pins),
			      mic_jack_pins);
	wm8350_mic_jack_detect(codec, &mic_jack, SND_JACK_MICROPHONE,
			       SND_JACK_BTN_0);

	snd_soc_dapm_force_enable_pin(codec, "Mic Bias");

	return 0;
}


static struct snd_soc_dai_link wm1133_ev1_dai = {
	.name = "WM1133-EV1",
	.stream_name = "Audio",
	.cpu_dai = &imx_ssi_pcm_dai[0],
	.codec_dai = &wm8350_dai,
	.init = wm1133_ev1_init,
	.ops = &wm1133_ev1_ops,
	.symmetric_rates = 1,
};

static struct snd_soc_card wm1133_ev1 = {
	.name = "WM1133-EV1",
	.platform = &imx_soc_platform,
	.dai_link = &wm1133_ev1_dai,
	.num_links = 1,
};

static struct snd_soc_device wm1133_ev1_snd_devdata = {
	.card = &wm1133_ev1,
	.codec_dev = &soc_codec_dev_wm8350,
};

static struct platform_device *wm1133_ev1_snd_device;

static int __init wm1133_ev1_audio_init(void)
{
	int ret;
	unsigned int ptcr, pdcr;

	/* SSI0 mastered by port 5 */
	ptcr = MXC_AUDMUX_V2_PTCR_SYN |
		MXC_AUDMUX_V2_PTCR_TFSDIR |
		MXC_AUDMUX_V2_PTCR_TFSEL(MX31_AUDMUX_PORT5_SSI_PINS_5) |
		MXC_AUDMUX_V2_PTCR_TCLKDIR |
		MXC_AUDMUX_V2_PTCR_TCSEL(MX31_AUDMUX_PORT5_SSI_PINS_5);
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(MX31_AUDMUX_PORT5_SSI_PINS_5);
	mxc_audmux_v2_configure_port(MX31_AUDMUX_PORT1_SSI0, ptcr, pdcr);

	ptcr = MXC_AUDMUX_V2_PTCR_SYN;
	pdcr = MXC_AUDMUX_V2_PDCR_RXDSEL(MX31_AUDMUX_PORT1_SSI0);
	mxc_audmux_v2_configure_port(MX31_AUDMUX_PORT5_SSI_PINS_5, ptcr, pdcr);

	wm1133_ev1_snd_device = platform_device_alloc("soc-audio", -1);
	if (!wm1133_ev1_snd_device)
		return -ENOMEM;

	platform_set_drvdata(wm1133_ev1_snd_device, &wm1133_ev1_snd_devdata);
	wm1133_ev1_snd_devdata.dev = &wm1133_ev1_snd_device->dev;
	ret = platform_device_add(wm1133_ev1_snd_device);

	if (ret)
		platform_device_put(wm1133_ev1_snd_device);

	return ret;
}
module_init(wm1133_ev1_audio_init);

static void __exit wm1133_ev1_audio_exit(void)
{
	platform_device_unregister(wm1133_ev1_snd_device);
}
module_exit(wm1133_ev1_audio_exit);

MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_DESCRIPTION("Audio for WM1133-EV1 on i.MX31ADS");
MODULE_LICENSE("GPL");
