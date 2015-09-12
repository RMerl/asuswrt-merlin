/*
 * SoC audio for BCM947XX Board
 *
 * Copyright (C) 1999-2015, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: bcm947xx.c,v 1.1 2010-05-13 23:46:27 $
 */


#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/wm8955.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c.h>

#include <plat/plat-bcm5301x.h>

#include <typedefs.h>
#include <bcmdevs.h>
#include <pcicfg.h>
#include <hndsoc.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <i2s_core.h>

#include <bcmnvram.h>

#include "../codecs/wm8955.h"
#include "bcm947xx-pcm.h"
#include "bcm947xx-i2s.h"

#define BCM947XX_AP_DEBUG 0
#if BCM947XX_AP_DEBUG
#define DBG(x...) printk(KERN_ERR x)
#else
#define DBG(x...)
#endif


/* MCLK in Hz - to bcm947xx & Wolfson 8955 */
#define BCM947XX_MCLK_FREQ 20000000 /* 20 MHz */
#define BCM947XX_NVRAM_XTAL_FREQ "xtalfreq"

#define SDA_GPIO_NVRAM_NAME "i2c_sda_gpio"
#define SCL_GPIO_NVRAM_NAME "i2c_scl_gpio"

#define BCM947XX_I2C_ADAPTER_ID 0

#if defined(CONFIG_I2C_BCM5301X)
#define BCM947XX_I2C_HW		1
#elif defined(CONFIG_I2C_GPIO)
#define BCM947XX_I2C_BB		1
#else
#error "No I2C controller defined"
#endif

static int bcm947xx_startup(struct snd_pcm_substream *substream)
{
	//struct snd_soc_pcm_runtime *rtd = substream->private_data;
	//struct snd_soc_codec_dai *codec_dai = rtd->dai->codec_dai;
	//struct snd_soc_cpu_dai *cpu_dai = rtd->dai->cpu_dai;
	int ret = 0;

	DBG("%s:\n", __FUNCTION__);

	return ret;
}

static void bcm947xx_shutdown(struct snd_pcm_substream *substream)
{
	DBG("%s\n", __FUNCTION__);
	return;
}

static int bcm947xx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int fmt;
	int freq = 11289600;
	int mclk_input = BCM947XX_MCLK_FREQ;
	char *tmp;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* These settings are only applicable for configuring playback. */
		DBG("%s: skipping (capture stream)", __FUNCTION__);
		return 0;
	}

	fmt = SND_SOC_DAIFMT_I2S |		/* I2S mode audio */
	      SND_SOC_DAIFMT_NB_NF |		/* BCLK not inverted and normal LRCLK polarity */
	      SND_SOC_DAIFMT_CBM_CFM;		/* BCM947xx is I2S Slave */

	/* set codec DAI configuration */
	DBG("%s: calling set_fmt with fmt 0x%x\n", __FUNCTION__, fmt);
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	DBG("%s: calling cpu_dai set_fmt with fmt 0x%x\n", __FUNCTION__, fmt);
	ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
	if (ret < 0)
		return ret;

	/* We need to derive the correct pll output frequency */
	/* These two PLL frequencies should cover the sample rates we'll be asked to use */
	if (freq % params_rate(params))
		freq = 12288000;
	if (freq % params_rate(params))
		DBG("%s: Error, PLL not configured for this sample rate %d\n", __FUNCTION__,
		    params_rate(params));

	tmp = nvram_get(BCM947XX_NVRAM_XTAL_FREQ);

	/* Try to get xtal frequency from NVRAM, otherwise we'll just use our default */
	if (tmp && (strlen(tmp) > 0)) {
		mclk_input = simple_strtol(tmp, NULL, 10);
		mclk_input *= 1000; /* NVRAM param is in kHz, we want Hz */
	}

	DBG("%s: mclk_input=%d (target )freq=%d\n", __FUNCTION__, mclk_input, freq);
	
    /* set up the PLL in codec */
	if (codec_dai->ops->set_pll) {
		ret = codec_dai->ops->set_pll(codec_dai, 0, 0, mclk_input, freq);
		if (ret < 0) {
			DBG("%s: Error CODEC DAI set_pll returned %d\n", __FUNCTION__, ret);
			return ret;
		}
		mclk_input = freq;
	}

 	/* set the codec system clock for DAC and ADC */
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8955_CLK_MCLK, mclk_input,
		SND_SOC_CLOCK_IN);
	DBG("%s: codec set_sysclk returned %d\n", __FUNCTION__, ret);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as input (unused) */
	ret = snd_soc_dai_set_sysclk(cpu_dai, BCM947XX_I2S_SYSCLK, freq,
		SND_SOC_CLOCK_IN);

	DBG("%s: cpu set_sysclk returned %d\n", __FUNCTION__, ret);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops bcm947xx_ops = {
	.startup = bcm947xx_startup,
	.hw_params = bcm947xx_hw_params,
	.shutdown = bcm947xx_shutdown,
};

/*
 * Configure audio route as:
 * $ amixer cset name='Right Playback Switch' on
 * $ amixer cset name='Left Playback Switch' on
*/

static const struct snd_soc_dapm_widget bcm947xx_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("LOUT", NULL),
	SND_SOC_DAPM_SPK("ROUT", NULL),
};

static const struct snd_soc_dapm_route intercon[] = {
	{ "LOUT", NULL, "LOUT2" },
	{ "ROUT", NULL, "ROUT2" },
};

/*
 * Logic for a wm8955
 */
static int bcm947xx_wm8955_init(struct snd_soc_codec *codec)
{
	DBG("%s\n", __FUNCTION__);

	snd_soc_dapm_new_controls(codec, bcm947xx_dapm_widgets,
		ARRAY_SIZE(bcm947xx_dapm_widgets));
	snd_soc_dapm_add_routes(codec, intercon, ARRAY_SIZE(intercon));

	snd_soc_dapm_enable_pin(codec, "LOUT");
	snd_soc_dapm_enable_pin(codec, "ROUT");

	snd_soc_dapm_sync(codec);

	return 0;
}

/* bcm947xx digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link bcm947xx_dai = {
	.name = "WM8955",
	.stream_name = "WM8955",
	.cpu_dai = &bcm947xx_i2s_dai,
	.codec_dai = &wm8955_dai,
	.init = bcm947xx_wm8955_init,
	.ops = &bcm947xx_ops,
};

static struct snd_soc_card bcm947xx_snd_card = {
	.name = "bcm947xx",
	.platform = &bcm947xx_soc_platform,
	.dai_link = &bcm947xx_dai,
	.num_links = 1,
};

/* bcm947xx audio subsystem */
static struct snd_soc_device bcm947xx_snd_devdata = {
	.card = &bcm947xx_snd_card,
	.codec_dev = &soc_codec_dev_wm8955,
};

static struct platform_device *bcm947xx_snd_device;

static int machine_is_bcm947xx(void)
{
	DBG("%s\n", __FUNCTION__);
	return 1;
}

#ifdef BCM947XX_I2C_BB
static struct i2c_gpio_platform_data i2c_gpio_data = {
	/* Will be replaced with board specific values from NVRAM */
	.sda_pin        = 4,
	.scl_pin        = 5,
};

static struct platform_device i2c_device = {
	.name		= "i2c-gpio",
	.id		= BCM947XX_I2C_ADAPTER_ID,
	.dev		= {
		.platform_data	= &i2c_gpio_data,
	},
};
#endif

#ifdef BCM947XX_I2C_HW
static struct resource i2c_device_resources[] = {
	{
		.start	= IRQ_SMBUS,
		.end	= true, /* WM8955 support up to 526Khz I2C freq, so 'fast' (400Khz) */
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device i2c_device = {
	.name		= "i2c-bcm5301x",
	.id		= BCM947XX_I2C_ADAPTER_ID,
	.resource	= i2c_device_resources,
	.num_resources	= ARRAY_SIZE(i2c_device_resources)
};
#endif

#ifdef NOTYET
/* Set WM8955 CODEC defaults. */
static struct wm8955_pdata wm8955_settings = {
	.out2_speaker	= 1,
};
#endif /* NOTYET */

static struct i2c_client *i2c_client;
static struct i2c_board_info i2c_board_info = {
	I2C_BOARD_INFO("wm8955", 0x1a),
#ifdef NOTYET
	.platform_data	= &wm8955_settings,
#endif /* NOTYET */
};

static int __init bcm947xx_i2c_device_register(void)
{
	int ret;

#ifdef BCM947XX_I2C_BB
	char *tmp;

	tmp = nvram_get(SDA_GPIO_NVRAM_NAME);

	if (tmp && (strlen(tmp) > 0))
		i2c_gpio_data.sda_pin = simple_strtol(tmp, NULL, 10);
	else {
		printk(KERN_ERR "%s: NVRAM variable %s missing for I2C interface\n",
		       __FUNCTION__, SDA_GPIO_NVRAM_NAME);
		return -ENODEV;
	}

	tmp = nvram_get(SCL_GPIO_NVRAM_NAME);

	if (tmp && (strlen(tmp) > 0))
		i2c_gpio_data.scl_pin = simple_strtol(tmp, NULL, 10);
	else {
		printk(KERN_ERR "%s: NVRAM variable %s missing for I2C interface\n",
		       __FUNCTION__, SCL_GPIO_NVRAM_NAME);
		return -ENODEV;
	}
#endif

	ret = platform_device_register(&i2c_device);
	if (ret) {
		printk(KERN_ERR "%s: Couldn't register i2c_device 0x%x\n",
		       __FUNCTION__, ret);
		return ret;
	}

	return 0;
}

static int __init bcm947xx_init(void)
{
	int ret;
	struct i2c_adapter *adap;

	DBG("%s\n", __FUNCTION__);

	if (!machine_is_bcm947xx())
		return -ENODEV;

	ret = bcm947xx_i2c_device_register();
	if (ret) {
		printk(KERN_ERR "%s: Couldn't register i2c device 0x%x\n",
		       __FUNCTION__, ret);
		goto error_i2c_device_register;
	}

	adap = i2c_get_adapter(BCM947XX_I2C_ADAPTER_ID);
	if (!adap) {
		printk(KERN_ERR "%s: Couldn't find i2c adapter\n", __FUNCTION__);
		ret = -ENODEV;
		goto error_i2c_get_adapter;
	}

	i2c_client = i2c_new_device(adap, &i2c_board_info);
	if (!i2c_client) {
		i2c_put_adapter(adap);
		printk(KERN_ERR "%s: Couldn't add new i2c device\n", __FUNCTION__);
		ret = -ENOMEM;
		goto error_i2c_new_device;
	}

	bcm947xx_snd_device = platform_device_alloc("soc-audio", -1);
	if (!bcm947xx_snd_device) {
		printk(KERN_ERR "%s: Couldn't alloc soc-audio\n", __FUNCTION__);
		ret = -ENOMEM;
		goto error_platform_device_alloc;
	}

	platform_set_drvdata(bcm947xx_snd_device, &bcm947xx_snd_devdata);
	bcm947xx_snd_devdata.dev = &bcm947xx_snd_device->dev;
	ret = platform_device_add(bcm947xx_snd_device);
	if (ret) {
		printk(KERN_ERR "%s: Couldn't add bcm947xx_snd_device 0x%x\n",
		       __FUNCTION__, ret);
		goto error_platform_device_add;
	}

	return ret;

error_platform_device_add:
	platform_device_put(&bcm947xx_snd_device);
error_platform_device_alloc:
	i2c_unregister_device(i2c_client);
error_i2c_new_device:
error_i2c_get_adapter:
	platform_device_unregister(&i2c_device);
error_i2c_device_register:

	return ret;
}

static void __exit bcm947xx_exit(void)
{
	DBG("%s\n", __FUNCTION__);
	platform_device_unregister(bcm947xx_snd_device);
	i2c_unregister_device(i2c_client);
	platform_device_unregister(&i2c_device);
}

module_init(bcm947xx_init);
module_exit(bcm947xx_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC BCM947XX");
MODULE_LICENSE("GPL and additional rights");
