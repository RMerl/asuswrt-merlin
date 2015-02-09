/*
 * ak4535.c  --  AK4535 ALSA Soc Audio driver
 *
 * Copyright 2005 Openedhand Ltd.
 *
 * Author: Richard Purdie <richard@openedhand.com>
 *
 * Based on wm8753.c by Liam Girdwood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
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

#include "ak4535.h"

#define AK4535_VERSION "0.3"

struct snd_soc_codec_device soc_codec_dev_ak4535;

/* codec private data */
struct ak4535_priv {
	unsigned int sysclk;
};

/*
 * ak4535 register cache
 */
static const u16 ak4535_reg[AK4535_CACHEREGNUM] = {
    0x0000, 0x0080, 0x0000, 0x0003,
    0x0002, 0x0000, 0x0011, 0x0001,
    0x0000, 0x0040, 0x0036, 0x0010,
    0x0000, 0x0000, 0x0057, 0x0000,
};

/*
 * read ak4535 register cache
 */
static inline unsigned int ak4535_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;
	if (reg >= AK4535_CACHEREGNUM)
		return -1;
	return cache[reg];
}

/*
 * write ak4535 register cache
 */
static inline void ak4535_write_reg_cache(struct snd_soc_codec *codec,
	u16 reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
	if (reg >= AK4535_CACHEREGNUM)
		return;
	cache[reg] = value;
}

/*
 * write to the AK4535 register space
 */
static int ak4535_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[2];

	/* data is
	 *   D15..D8 AK4535 register offset
	 *   D7...D0 register data
	 */
	data[0] = reg & 0xff;
	data[1] = value & 0xff;

	ak4535_write_reg_cache(codec, reg, value);
	if (codec->hw_write(codec->control_data, data, 2) == 2)
		return 0;
	else
		return -EIO;
}

static int ak4535_sync(struct snd_soc_codec *codec)
{
	u16 *cache = codec->reg_cache;
	int i, r = 0;

	for (i = 0; i < AK4535_CACHEREGNUM; i++)
		r |= ak4535_write(codec, i, cache[i]);

	return r;
};

static const char *ak4535_mono_gain[] = {"+6dB", "-17dB"};
static const char *ak4535_mono_out[] = {"(L + R)/2", "Hi-Z"};
static const char *ak4535_hp_out[] = {"Stereo", "Mono"};
static const char *ak4535_deemp[] = {"44.1kHz", "Off", "48kHz", "32kHz"};
static const char *ak4535_mic_select[] = {"Internal", "External"};

static const struct soc_enum ak4535_enum[] = {
	SOC_ENUM_SINGLE(AK4535_SIG1, 7, 2, ak4535_mono_gain),
	SOC_ENUM_SINGLE(AK4535_SIG1, 6, 2, ak4535_mono_out),
	SOC_ENUM_SINGLE(AK4535_MODE2, 2, 2, ak4535_hp_out),
	SOC_ENUM_SINGLE(AK4535_DAC, 0, 4, ak4535_deemp),
	SOC_ENUM_SINGLE(AK4535_MIC, 1, 2, ak4535_mic_select),
};

static const struct snd_kcontrol_new ak4535_snd_controls[] = {
	SOC_SINGLE("ALC2 Switch", AK4535_SIG1, 1, 1, 0),
	SOC_ENUM("Mono 1 Output", ak4535_enum[1]),
	SOC_ENUM("Mono 1 Gain", ak4535_enum[0]),
	SOC_ENUM("Headphone Output", ak4535_enum[2]),
	SOC_ENUM("Playback Deemphasis", ak4535_enum[3]),
	SOC_SINGLE("Bass Volume", AK4535_DAC, 2, 3, 0),
	SOC_SINGLE("Mic Boost (+20dB) Switch", AK4535_MIC, 0, 1, 0),
	SOC_ENUM("Mic Select", ak4535_enum[4]),
	SOC_SINGLE("ALC Operation Time", AK4535_TIMER, 0, 3, 0),
	SOC_SINGLE("ALC Recovery Time", AK4535_TIMER, 2, 3, 0),
	SOC_SINGLE("ALC ZC Time", AK4535_TIMER, 4, 3, 0),
	SOC_SINGLE("ALC 1 Switch", AK4535_ALC1, 5, 1, 0),
	SOC_SINGLE("ALC 2 Switch", AK4535_ALC1, 6, 1, 0),
	SOC_SINGLE("ALC Volume", AK4535_ALC2, 0, 127, 0),
	SOC_SINGLE("Capture Volume", AK4535_PGA, 0, 127, 0),
	SOC_SINGLE("Left Playback Volume", AK4535_LATT, 0, 127, 1),
	SOC_SINGLE("Right Playback Volume", AK4535_RATT, 0, 127, 1),
	SOC_SINGLE("AUX Bypass Volume", AK4535_VOL, 0, 15, 0),
	SOC_SINGLE("Mic Sidetone Volume", AK4535_VOL, 4, 7, 0),
};

/* Mono 1 Mixer */
static const struct snd_kcontrol_new ak4535_mono1_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic Sidetone Switch", AK4535_SIG1, 4, 1, 0),
	SOC_DAPM_SINGLE("Mono Playback Switch", AK4535_SIG1, 5, 1, 0),
};

/* Stereo Mixer */
static const struct snd_kcontrol_new ak4535_stereo_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic Sidetone Switch", AK4535_SIG2, 4, 1, 0),
	SOC_DAPM_SINGLE("Playback Switch", AK4535_SIG2, 7, 1, 0),
	SOC_DAPM_SINGLE("Aux Bypass Switch", AK4535_SIG2, 5, 1, 0),
};

/* Input Mixer */
static const struct snd_kcontrol_new ak4535_input_mixer_controls[] = {
	SOC_DAPM_SINGLE("Mic Capture Switch", AK4535_MIC, 2, 1, 0),
	SOC_DAPM_SINGLE("Aux Capture Switch", AK4535_MIC, 5, 1, 0),
};

/* Input mux */
static const struct snd_kcontrol_new ak4535_input_mux_control =
	SOC_DAPM_ENUM("Input Select", ak4535_enum[4]);

/* HP L switch */
static const struct snd_kcontrol_new ak4535_hpl_control =
	SOC_DAPM_SINGLE("Switch", AK4535_SIG2, 1, 1, 1);

/* HP R switch */
static const struct snd_kcontrol_new ak4535_hpr_control =
	SOC_DAPM_SINGLE("Switch", AK4535_SIG2, 0, 1, 1);

/* mono 2 switch */
static const struct snd_kcontrol_new ak4535_mono2_control =
	SOC_DAPM_SINGLE("Switch", AK4535_SIG1, 0, 1, 0);

/* Line out switch */
static const struct snd_kcontrol_new ak4535_line_control =
	SOC_DAPM_SINGLE("Switch", AK4535_SIG2, 6, 1, 0);

/* ak4535 dapm widgets */
static const struct snd_soc_dapm_widget ak4535_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("Stereo Mixer", SND_SOC_NOPM, 0, 0,
		&ak4535_stereo_mixer_controls[0],
		ARRAY_SIZE(ak4535_stereo_mixer_controls)),
	SND_SOC_DAPM_MIXER("Mono1 Mixer", SND_SOC_NOPM, 0, 0,
		&ak4535_mono1_mixer_controls[0],
		ARRAY_SIZE(ak4535_mono1_mixer_controls)),
	SND_SOC_DAPM_MIXER("Input Mixer", SND_SOC_NOPM, 0, 0,
		&ak4535_input_mixer_controls[0],
		ARRAY_SIZE(ak4535_input_mixer_controls)),
	SND_SOC_DAPM_MUX("Input Mux", SND_SOC_NOPM, 0, 0,
		&ak4535_input_mux_control),
	SND_SOC_DAPM_DAC("DAC", "Playback", AK4535_PM2, 0, 0),
	SND_SOC_DAPM_SWITCH("Mono 2 Enable", SND_SOC_NOPM, 0, 0,
		&ak4535_mono2_control),
	/* speaker powersave bit */
	SND_SOC_DAPM_PGA("Speaker Enable", AK4535_MODE2, 0, 0, NULL, 0),
	SND_SOC_DAPM_SWITCH("Line Out Enable", SND_SOC_NOPM, 0, 0,
		&ak4535_line_control),
	SND_SOC_DAPM_SWITCH("Left HP Enable", SND_SOC_NOPM, 0, 0,
		&ak4535_hpl_control),
	SND_SOC_DAPM_SWITCH("Right HP Enable", SND_SOC_NOPM, 0, 0,
		&ak4535_hpr_control),
	SND_SOC_DAPM_OUTPUT("LOUT"),
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("ROUT"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("SPP"),
	SND_SOC_DAPM_OUTPUT("SPN"),
	SND_SOC_DAPM_OUTPUT("MOUT1"),
	SND_SOC_DAPM_OUTPUT("MOUT2"),
	SND_SOC_DAPM_OUTPUT("MICOUT"),
	SND_SOC_DAPM_ADC("ADC", "Capture", AK4535_PM1, 0, 0),
	SND_SOC_DAPM_PGA("Spk Amp", AK4535_PM2, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HP R Amp", AK4535_PM2, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("HP L Amp", AK4535_PM2, 2, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mic", AK4535_PM1, 1, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Line Out", AK4535_PM1, 4, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Mono Out", AK4535_PM1, 3, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AUX In", AK4535_PM1, 2, 0, NULL, 0),

	SND_SOC_DAPM_MICBIAS("Mic Int Bias", AK4535_MIC, 3, 0),
	SND_SOC_DAPM_MICBIAS("Mic Ext Bias", AK4535_MIC, 4, 0),
	SND_SOC_DAPM_INPUT("MICIN"),
	SND_SOC_DAPM_INPUT("MICEXT"),
	SND_SOC_DAPM_INPUT("AUX"),
	SND_SOC_DAPM_INPUT("MIN"),
	SND_SOC_DAPM_INPUT("AIN"),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/*stereo mixer */
	{"Stereo Mixer", "Playback Switch", "DAC"},
	{"Stereo Mixer", "Mic Sidetone Switch", "Mic"},
	{"Stereo Mixer", "Aux Bypass Switch", "AUX In"},

	/* mono1 mixer */
	{"Mono1 Mixer", "Mic Sidetone Switch", "Mic"},
	{"Mono1 Mixer", "Mono Playback Switch", "DAC"},

	/* Mic */
	{"Mic", NULL, "AIN"},
	{"Input Mux", "Internal", "Mic Int Bias"},
	{"Input Mux", "External", "Mic Ext Bias"},
	{"Mic Int Bias", NULL, "MICIN"},
	{"Mic Ext Bias", NULL, "MICEXT"},
	{"MICOUT", NULL, "Input Mux"},

	/* line out */
	{"LOUT", NULL, "Line Out Enable"},
	{"ROUT", NULL, "Line Out Enable"},
	{"Line Out Enable", "Switch", "Line Out"},
	{"Line Out", NULL, "Stereo Mixer"},

	/* mono1 out */
	{"MOUT1", NULL, "Mono Out"},
	{"Mono Out", NULL, "Mono1 Mixer"},

	/* left HP */
	{"HPL", NULL, "Left HP Enable"},
	{"Left HP Enable", "Switch", "HP L Amp"},
	{"HP L Amp", NULL, "Stereo Mixer"},

	/* right HP */
	{"HPR", NULL, "Right HP Enable"},
	{"Right HP Enable", "Switch", "HP R Amp"},
	{"HP R Amp", NULL, "Stereo Mixer"},

	/* speaker */
	{"SPP", NULL, "Speaker Enable"},
	{"SPN", NULL, "Speaker Enable"},
	{"Speaker Enable", "Switch", "Spk Amp"},
	{"Spk Amp", NULL, "MIN"},

	/* mono 2 */
	{"MOUT2", NULL, "Mono 2 Enable"},
	{"Mono 2 Enable", "Switch", "Stereo Mixer"},

	/* Aux In */
	{"Aux In", NULL, "AUX"},

	/* ADC */
	{"ADC", NULL, "Input Mixer"},
	{"Input Mixer", "Mic Capture Switch", "Mic"},
	{"Input Mixer", "Aux Capture Switch", "Aux In"},
};

static int ak4535_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, ak4535_dapm_widgets,
				  ARRAY_SIZE(ak4535_dapm_widgets));

	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	return 0;
}

static int ak4535_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct ak4535_priv *ak4535 = snd_soc_codec_get_drvdata(codec);

	ak4535->sysclk = freq;
	return 0;
}

static int ak4535_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct ak4535_priv *ak4535 = snd_soc_codec_get_drvdata(codec);
	u8 mode2 = ak4535_read_reg_cache(codec, AK4535_MODE2) & ~(0x3 << 5);
	int rate = params_rate(params), fs = 256;

	if (rate)
		fs = ak4535->sysclk / rate;

	/* set fs */
	switch (fs) {
	case 1024:
		mode2 |= (0x2 << 5);
		break;
	case 512:
		mode2 |= (0x1 << 5);
		break;
	case 256:
		break;
	}

	/* set rate */
	ak4535_write(codec, AK4535_MODE2, mode2);
	return 0;
}

static int ak4535_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 mode1 = 0;

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		mode1 = 0x0002;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		mode1 = 0x0001;
		break;
	default:
		return -EINVAL;
	}

	/* use 32 fs for BCLK to save power */
	mode1 |= 0x4;

	ak4535_write(codec, AK4535_MODE1, mode1);
	return 0;
}

static int ak4535_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = ak4535_read_reg_cache(codec, AK4535_DAC) & 0xffdf;
	if (!mute)
		ak4535_write(codec, AK4535_DAC, mute_reg);
	else
		ak4535_write(codec, AK4535_DAC, mute_reg | 0x20);
	return 0;
}

static int ak4535_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	u16 i;

	switch (level) {
	case SND_SOC_BIAS_ON:
		ak4535_mute(codec->dai, 0);
		break;
	case SND_SOC_BIAS_PREPARE:
		ak4535_mute(codec->dai, 1);
		break;
	case SND_SOC_BIAS_STANDBY:
		i = ak4535_read_reg_cache(codec, AK4535_PM1);
		ak4535_write(codec, AK4535_PM1, i | 0x80);
		i = ak4535_read_reg_cache(codec, AK4535_PM2);
		ak4535_write(codec, AK4535_PM2, i & (~0x80));
		break;
	case SND_SOC_BIAS_OFF:
		i = ak4535_read_reg_cache(codec, AK4535_PM1);
		ak4535_write(codec, AK4535_PM1, i & (~0x80));
		break;
	}
	codec->bias_level = level;
	return 0;
}

#define AK4535_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |\
		SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_22050 |\
		SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

static struct snd_soc_dai_ops ak4535_dai_ops = {
	.hw_params	= ak4535_hw_params,
	.set_fmt	= ak4535_set_dai_fmt,
	.digital_mute	= ak4535_mute,
	.set_sysclk	= ak4535_set_dai_sysclk,
};

struct snd_soc_dai ak4535_dai = {
	.name = "AK4535",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AK4535_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AK4535_RATES,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,},
	.ops = &ak4535_dai_ops,
};
EXPORT_SYMBOL_GPL(ak4535_dai);

static int ak4535_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	ak4535_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int ak4535_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	ak4535_sync(codec);
	ak4535_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	return 0;
}

/*
 * initialise the AK4535 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int ak4535_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->card->codec;
	int ret = 0;

	codec->name = "AK4535";
	codec->owner = THIS_MODULE;
	codec->read = ak4535_read_reg_cache;
	codec->write = ak4535_write;
	codec->set_bias_level = ak4535_set_bias_level;
	codec->dai = &ak4535_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = ARRAY_SIZE(ak4535_reg);
	codec->reg_cache = kmemdup(ak4535_reg, sizeof(ak4535_reg), GFP_KERNEL);

	if (codec->reg_cache == NULL)
		return -ENOMEM;

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "ak4535: failed to create pcms\n");
		goto pcm_err;
	}

	/* power on device */
	ak4535_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	snd_soc_add_controls(codec, ak4535_snd_controls,
				ARRAY_SIZE(ak4535_snd_controls));
	ak4535_add_widgets(codec);

	return ret;

pcm_err:
	kfree(codec->reg_cache);

	return ret;
}

static struct snd_soc_device *ak4535_socdev;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)

static int ak4535_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct snd_soc_device *socdev = ak4535_socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	int ret;

	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = ak4535_init(socdev);
	if (ret < 0)
		printk(KERN_ERR "failed to initialise AK4535\n");

	return ret;
}

static int ak4535_i2c_remove(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	kfree(codec->reg_cache);
	return 0;
}

static const struct i2c_device_id ak4535_i2c_id[] = {
	{ "ak4535", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak4535_i2c_id);

static struct i2c_driver ak4535_i2c_driver = {
	.driver = {
		.name = "AK4535 I2C Codec",
		.owner = THIS_MODULE,
	},
	.probe =    ak4535_i2c_probe,
	.remove =   ak4535_i2c_remove,
	.id_table = ak4535_i2c_id,
};

static int ak4535_add_i2c_device(struct platform_device *pdev,
				 const struct ak4535_setup_data *setup)
{
	struct i2c_board_info info;
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	int ret;

	ret = i2c_add_driver(&ak4535_i2c_driver);
	if (ret != 0) {
		dev_err(&pdev->dev, "can't add i2c driver\n");
		return ret;
	}

	memset(&info, 0, sizeof(struct i2c_board_info));
	info.addr = setup->i2c_address;
	strlcpy(info.type, "ak4535", I2C_NAME_SIZE);

	adapter = i2c_get_adapter(setup->i2c_bus);
	if (!adapter) {
		dev_err(&pdev->dev, "can't get i2c adapter %d\n",
			setup->i2c_bus);
		goto err_driver;
	}

	client = i2c_new_device(adapter, &info);
	i2c_put_adapter(adapter);
	if (!client) {
		dev_err(&pdev->dev, "can't add i2c device at 0x%x\n",
			(unsigned int)info.addr);
		goto err_driver;
	}

	return 0;

err_driver:
	i2c_del_driver(&ak4535_i2c_driver);
	return -ENODEV;
}
#endif

static int ak4535_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct ak4535_setup_data *setup;
	struct snd_soc_codec *codec;
	struct ak4535_priv *ak4535;
	int ret;

	printk(KERN_INFO "AK4535 Audio Codec %s", AK4535_VERSION);

	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	ak4535 = kzalloc(sizeof(struct ak4535_priv), GFP_KERNEL);
	if (ak4535 == NULL) {
		kfree(codec);
		return -ENOMEM;
	}

	snd_soc_codec_set_drvdata(codec, ak4535);
	socdev->card->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	ak4535_socdev = socdev;
	ret = -ENODEV;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		codec->hw_write = (hw_write_t)i2c_master_send;
		ret = ak4535_add_i2c_device(pdev, setup);
	}
#endif

	if (ret != 0) {
		kfree(snd_soc_codec_get_drvdata(codec));
		kfree(codec);
	}
	return ret;
}

/* power down chip */
static int ak4535_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	if (codec->control_data)
		ak4535_set_bias_level(codec, SND_SOC_BIAS_OFF);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (codec->control_data)
		i2c_unregister_device(codec->control_data);
	i2c_del_driver(&ak4535_i2c_driver);
#endif
	kfree(snd_soc_codec_get_drvdata(codec));
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_ak4535 = {
	.probe = 	ak4535_probe,
	.remove = 	ak4535_remove,
	.suspend = 	ak4535_suspend,
	.resume =	ak4535_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_ak4535);

static int __init ak4535_modinit(void)
{
	return snd_soc_register_dai(&ak4535_dai);
}
module_init(ak4535_modinit);

static void __exit ak4535_exit(void)
{
	snd_soc_unregister_dai(&ak4535_dai);
}
module_exit(ak4535_exit);

MODULE_DESCRIPTION("Soc AK4535 driver");
MODULE_AUTHOR("Richard Purdie");
MODULE_LICENSE("GPL");
