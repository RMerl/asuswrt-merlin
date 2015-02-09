/*
 * rx51.c  --  SoC audio for Nokia RX-51
 *
 * Copyright (C) 2008 - 2009 Nokia Corporation
 *
 * Contact: Peter Ujfalusi <peter.ujfalusi@nokia.com>
 *          Eduardo Valentin <eduardo.valentin@nokia.com>
 *          Jarkko Nikula <jhnikula@gmail.com>
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/jack.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>

#include "omap-mcbsp.h"
#include "omap-pcm.h"
#include "../codecs/tlv320aic3x.h"

#define RX51_TVOUT_SEL_GPIO		40
#define RX51_JACK_DETECT_GPIO		177
/*
 * REVISIT: TWL4030 GPIO base in RX-51. Now statically defined to 192. This
 * gpio is reserved in arch/arm/mach-omap2/board-rx51-peripherals.c
 */
#define RX51_SPEAKER_AMP_TWL_GPIO	(192 + 7)

enum {
	RX51_JACK_DISABLED,
	RX51_JACK_TVOUT,		/* tv-out */
};

static int rx51_spk_func;
static int rx51_dmic_func;
static int rx51_jack_func;

static void rx51_ext_control(struct snd_soc_codec *codec)
{
	if (rx51_spk_func)
		snd_soc_dapm_enable_pin(codec, "Ext Spk");
	else
		snd_soc_dapm_disable_pin(codec, "Ext Spk");
	if (rx51_dmic_func)
		snd_soc_dapm_enable_pin(codec, "DMic");
	else
		snd_soc_dapm_disable_pin(codec, "DMic");

	gpio_set_value(RX51_TVOUT_SEL_GPIO,
		       rx51_jack_func == RX51_JACK_TVOUT);

	snd_soc_dapm_sync(codec);
}

static int rx51_startup(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->socdev->card->codec;

	snd_pcm_hw_constraint_minmax(runtime,
				     SNDRV_PCM_HW_PARAM_CHANNELS, 2, 2);
	rx51_ext_control(codec);

	return 0;
}

static int rx51_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	int err;

	/* Set codec DAI configuration */
	err = snd_soc_dai_set_fmt(codec_dai,
				  SND_SOC_DAIFMT_DSP_A |
				  SND_SOC_DAIFMT_IB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (err < 0)
		return err;

	/* Set cpu DAI configuration */
	err = snd_soc_dai_set_fmt(cpu_dai,
				  SND_SOC_DAIFMT_DSP_A |
				  SND_SOC_DAIFMT_IB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (err < 0)
		return err;

	/* Set the codec system clock for DAC and ADC */
	return snd_soc_dai_set_sysclk(codec_dai, 0, 19200000,
				      SND_SOC_CLOCK_IN);
}

static struct snd_soc_ops rx51_ops = {
	.startup = rx51_startup,
	.hw_params = rx51_hw_params,
};

static int rx51_get_spk(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = rx51_spk_func;

	return 0;
}

static int rx51_set_spk(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (rx51_spk_func == ucontrol->value.integer.value[0])
		return 0;

	rx51_spk_func = ucontrol->value.integer.value[0];
	rx51_ext_control(codec);

	return 1;
}

static int rx51_spk_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *k, int event)
{
	if (SND_SOC_DAPM_EVENT_ON(event))
		gpio_set_value(RX51_SPEAKER_AMP_TWL_GPIO, 1);
	else
		gpio_set_value(RX51_SPEAKER_AMP_TWL_GPIO, 0);

	return 0;
}

static int rx51_get_input(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = rx51_dmic_func;

	return 0;
}

static int rx51_set_input(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (rx51_dmic_func == ucontrol->value.integer.value[0])
		return 0;

	rx51_dmic_func = ucontrol->value.integer.value[0];
	rx51_ext_control(codec);

	return 1;
}

static int rx51_get_jack(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = rx51_jack_func;

	return 0;
}

static int rx51_set_jack(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (rx51_jack_func == ucontrol->value.integer.value[0])
		return 0;

	rx51_jack_func = ucontrol->value.integer.value[0];
	rx51_ext_control(codec);

	return 1;
}

static struct snd_soc_jack rx51_av_jack;

static struct snd_soc_jack_gpio rx51_av_jack_gpios[] = {
	{
		.gpio = RX51_JACK_DETECT_GPIO,
		.name = "avdet-gpio",
		.report = SND_JACK_VIDEOOUT,
		.invert = 1,
		.debounce_time = 200,
	},
};

static const struct snd_soc_dapm_widget aic34_dapm_widgets[] = {
	SND_SOC_DAPM_SPK("Ext Spk", rx51_spk_event),
	SND_SOC_DAPM_MIC("DMic", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	{"Ext Spk", NULL, "HPLOUT"},
	{"Ext Spk", NULL, "HPROUT"},

	{"DMic Rate 64", NULL, "Mic Bias 2V"},
	{"Mic Bias 2V", NULL, "DMic"},
};

static const char *spk_function[] = {"Off", "On"};
static const char *input_function[] = {"ADC", "Digital Mic"};
static const char *jack_function[] = {"Off", "TV-OUT"};

static const struct soc_enum rx51_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_function), spk_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(input_function), input_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(jack_function), jack_function),
};

static const struct snd_kcontrol_new aic34_rx51_controls[] = {
	SOC_ENUM_EXT("Speaker Function", rx51_enum[0],
		     rx51_get_spk, rx51_set_spk),
	SOC_ENUM_EXT("Input Select",  rx51_enum[1],
		     rx51_get_input, rx51_set_input),
	SOC_ENUM_EXT("Jack Function", rx51_enum[2],
		     rx51_get_jack, rx51_set_jack),
};

static int rx51_aic34_init(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->socdev->card;
	int err;

	/* Set up NC codec pins */
	snd_soc_dapm_nc_pin(codec, "MIC3L");
	snd_soc_dapm_nc_pin(codec, "MIC3R");
	snd_soc_dapm_nc_pin(codec, "LINE1R");

	/* Add RX-51 specific controls */
	err = snd_soc_add_controls(codec, aic34_rx51_controls,
				   ARRAY_SIZE(aic34_rx51_controls));
	if (err < 0)
		return err;

	/* Add RX-51 specific widgets */
	snd_soc_dapm_new_controls(codec, aic34_dapm_widgets,
				  ARRAY_SIZE(aic34_dapm_widgets));

	/* Set up RX-51 specific audio path audio_map */
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_dapm_sync(codec);

	/* AV jack detection */
	err = snd_soc_jack_new(card, "AV Jack",
			       SND_JACK_VIDEOOUT, &rx51_av_jack);
	if (err)
		return err;
	err = snd_soc_jack_add_gpios(&rx51_av_jack,
				     ARRAY_SIZE(rx51_av_jack_gpios),
				     rx51_av_jack_gpios);

	return err;
}

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link rx51_dai[] = {
	{
		.name = "TLV320AIC34",
		.stream_name = "AIC34",
		.cpu_dai = &omap_mcbsp_dai[0],
		.codec_dai = &aic3x_dai,
		.init = rx51_aic34_init,
		.ops = &rx51_ops,
	},
};

/* Audio private data */
static struct aic3x_setup_data rx51_aic34_setup = {
	.gpio_func[0] = AIC3X_GPIO1_FUNC_DISABLED,
	.gpio_func[1] = AIC3X_GPIO2_FUNC_DIGITAL_MIC_INPUT,
};

/* Audio card */
static struct snd_soc_card rx51_sound_card = {
	.name = "RX-51",
	.dai_link = rx51_dai,
	.num_links = ARRAY_SIZE(rx51_dai),
	.platform = &omap_soc_platform,
};

/* Audio subsystem */
static struct snd_soc_device rx51_snd_devdata = {
	.card = &rx51_sound_card,
	.codec_dev = &soc_codec_dev_aic3x,
	.codec_data = &rx51_aic34_setup,
};

static struct platform_device *rx51_snd_device;

static int __init rx51_soc_init(void)
{
	int err;

	if (!machine_is_nokia_rx51())
		return -ENODEV;

	err = gpio_request(RX51_TVOUT_SEL_GPIO, "tvout_sel");
	if (err)
		goto err_gpio_tvout_sel;
	gpio_direction_output(RX51_TVOUT_SEL_GPIO, 0);

	rx51_snd_device = platform_device_alloc("soc-audio", -1);
	if (!rx51_snd_device) {
		err = -ENOMEM;
		goto err1;
	}

	platform_set_drvdata(rx51_snd_device, &rx51_snd_devdata);
	rx51_snd_devdata.dev = &rx51_snd_device->dev;
	*(unsigned int *)rx51_dai[0].cpu_dai->private_data = 1; /* McBSP2 */

	err = platform_device_add(rx51_snd_device);
	if (err)
		goto err2;

	return 0;
err2:
	platform_device_put(rx51_snd_device);
err1:
	gpio_free(RX51_TVOUT_SEL_GPIO);
err_gpio_tvout_sel:

	return err;
}

static void __exit rx51_soc_exit(void)
{
	snd_soc_jack_free_gpios(&rx51_av_jack, ARRAY_SIZE(rx51_av_jack_gpios),
				rx51_av_jack_gpios);

	platform_device_unregister(rx51_snd_device);
	gpio_free(RX51_TVOUT_SEL_GPIO);
}

module_init(rx51_soc_init);
module_exit(rx51_soc_exit);

MODULE_AUTHOR("Nokia Corporation");
MODULE_DESCRIPTION("ALSA SoC Nokia RX-51");
MODULE_LICENSE("GPL");
