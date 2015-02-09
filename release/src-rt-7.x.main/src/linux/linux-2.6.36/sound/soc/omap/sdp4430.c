/*
 * sdp4430.c  --  SoC audio for TI OMAP4430 SDP
 *
 * Author: Misael Lopez Cruz <x0052729@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>
#include <plat/hardware.h>
#include <plat/mux.h>

#include "mcpdm.h"
#include "omap-mcpdm.h"
#include "omap-pcm.h"
#include "../codecs/twl6040.h"

static int twl6040_power_mode;

static int sdp4430_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	int clk_id, freq;
	int ret;

	if (twl6040_power_mode) {
		clk_id = TWL6040_SYSCLK_SEL_HPPLL;
		freq = 38400000;
	} else {
		clk_id = TWL6040_SYSCLK_SEL_LPPLL;
		freq = 32768;
	}

	/* set the codec mclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, clk_id, freq,
				SND_SOC_CLOCK_IN);
	if (ret) {
		printk(KERN_ERR "can't set codec system clock\n");
		return ret;
	}
}

static struct snd_soc_ops sdp4430_ops = {
	.hw_params = sdp4430_hw_params,
};

static int sdp4430_get_power_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = twl6040_power_mode;
	return 0;
}

static int sdp4430_set_power_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	if (twl6040_power_mode == ucontrol->value.integer.value[0])
		return 0;

	twl6040_power_mode = ucontrol->value.integer.value[0];

	return 1;
}

static const char *power_texts[] = {"Low-Power", "High-Performance"};

static const struct soc_enum sdp4430_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, power_texts),
};

static const struct snd_kcontrol_new sdp4430_controls[] = {
	SOC_ENUM_EXT("TWL6040 Power Mode", sdp4430_enum[0],
		sdp4430_get_power_mode, sdp4430_set_power_mode),
};

/* SDP4430 machine DAPM */
static const struct snd_soc_dapm_widget sdp4430_twl6040_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Ext Mic", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_HP("Headset Stereophone", NULL),
	SND_SOC_DAPM_SPK("Earphone Spk", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* External Mics: MAINMIC, SUBMIC with bias*/
	{"MAINMIC", NULL, "Main Mic Bias"},
	{"SUBMIC", NULL, "Main Mic Bias"},
	{"Main Mic Bias", NULL, "Ext Mic"},

	/* External Speakers: HFL, HFR */
	{"Ext Spk", NULL, "HFL"},
	{"Ext Spk", NULL, "HFR"},

	/* Headset Mic: HSMIC with bias */
	{"HSMIC", NULL, "Headset Mic Bias"},
	{"Headset Mic Bias", NULL, "Headset Mic"},

	/* Headset Stereophone (Headphone): HSOL, HSOR */
	{"Headset Stereophone", NULL, "HSOL"},
	{"Headset Stereophone", NULL, "HSOR"},

	/* Earphone speaker */
	{"Earphone Spk", NULL, "EP"},
};

static int sdp4430_twl6040_init(struct snd_soc_codec *codec)
{
	int ret;

	/* Add SDP4430 specific controls */
	ret = snd_soc_add_controls(codec, sdp4430_controls,
				ARRAY_SIZE(sdp4430_controls));
	if (ret)
		return ret;

	/* Add SDP4430 specific widgets */
	ret = snd_soc_dapm_new_controls(codec, sdp4430_twl6040_dapm_widgets,
				ARRAY_SIZE(sdp4430_twl6040_dapm_widgets));
	if (ret)
		return ret;

	/* Set up SDP4430 specific audio path audio_map */
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	/* SDP4430 connected pins */
	snd_soc_dapm_enable_pin(codec, "Ext Mic");
	snd_soc_dapm_enable_pin(codec, "Ext Spk");
	snd_soc_dapm_enable_pin(codec, "Headset Mic");
	snd_soc_dapm_enable_pin(codec, "Headset Stereophone");

	/* TWL6040 not connected pins */
	snd_soc_dapm_nc_pin(codec, "AFML");
	snd_soc_dapm_nc_pin(codec, "AFMR");

	ret = snd_soc_dapm_sync(codec);

	return ret;
}

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link sdp4430_dai = {
	.name = "TWL6040",
	.stream_name = "TWL6040",
	.cpu_dai = &omap_mcpdm_dai,
	.codec_dai = &twl6040_dai,
	.init = sdp4430_twl6040_init,
	.ops = &sdp4430_ops,
};

/* Audio machine driver */
static struct snd_soc_card snd_soc_sdp4430 = {
	.name = "SDP4430",
	.platform = &omap_soc_platform,
	.dai_link = &sdp4430_dai,
	.num_links = 1,
};

/* Audio subsystem */
static struct snd_soc_device sdp4430_snd_devdata = {
	.card = &snd_soc_sdp4430,
	.codec_dev = &soc_codec_dev_twl6040,
};

static struct platform_device *sdp4430_snd_device;

static int __init sdp4430_soc_init(void)
{
	int ret;

	if (!machine_is_omap_4430sdp()) {
		pr_debug("Not SDP4430!\n");
		return -ENODEV;
	}
	printk(KERN_INFO "SDP4430 SoC init\n");

	sdp4430_snd_device = platform_device_alloc("soc-audio", -1);
	if (!sdp4430_snd_device) {
		printk(KERN_ERR "Platform device allocation failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(sdp4430_snd_device, &sdp4430_snd_devdata);
	sdp4430_snd_devdata.dev = &sdp4430_snd_device->dev;

	ret = platform_device_add(sdp4430_snd_device);
	if (ret)
		goto err;

	/* Codec starts in HP mode */
	twl6040_power_mode = 1;

	return 0;

err:
	printk(KERN_ERR "Unable to add platform device\n");
	platform_device_put(sdp4430_snd_device);
	return ret;
}
module_init(sdp4430_soc_init);

static void __exit sdp4430_soc_exit(void)
{
	platform_device_unregister(sdp4430_snd_device);
}
module_exit(sdp4430_soc_exit);

MODULE_AUTHOR("Misael Lopez Cruz <x0052729@ti.com>");
MODULE_DESCRIPTION("ALSA SoC SDP4430");
MODULE_LICENSE("GPL");
