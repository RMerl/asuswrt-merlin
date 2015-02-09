/*
 * ams-delta.c  --  SoC audio for Amstrad E3 (Delta) videophone
 *
 * Copyright (C) 2009 Janusz Krzysztofik <jkrzyszt@tis.icnet.pl>
 *
 * Initially based on sound/soc/omap/osk5912.x
 * Copyright (C) 2008 Mistral Solutions
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

#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/tty.h>

#include <sound/soc-dapm.h>
#include <sound/jack.h>

#include <asm/mach-types.h>

#include <plat/board-ams-delta.h>
#include <plat/mcbsp.h>

#include "omap-mcbsp.h"
#include "omap-pcm.h"
#include "../codecs/cx20442.h"


/* Board specific DAPM widgets */
static const struct snd_soc_dapm_widget ams_delta_dapm_widgets[] = {
	/* Handset */
	SND_SOC_DAPM_MIC("Mouthpiece", NULL),
	SND_SOC_DAPM_HP("Earpiece", NULL),
	/* Handsfree/Speakerphone */
	SND_SOC_DAPM_MIC("Microphone", NULL),
	SND_SOC_DAPM_SPK("Speaker", NULL),
};

/* How they are connected to codec pins */
static const struct snd_soc_dapm_route ams_delta_audio_map[] = {
	{"TELIN", NULL, "Mouthpiece"},
	{"Earpiece", NULL, "TELOUT"},

	{"MIC", NULL, "Microphone"},
	{"Speaker", NULL, "SPKOUT"},
};

/*
 * Controls, functional after the modem line discipline is activated.
 */

/* Virtual switch: audio input/output constellations */
static const char *ams_delta_audio_mode[] =
	{"Mixed", "Handset", "Handsfree", "Speakerphone"};

/* Selection <-> pin translation */
#define AMS_DELTA_MOUTHPIECE	0
#define AMS_DELTA_EARPIECE	1
#define AMS_DELTA_MICROPHONE	2
#define AMS_DELTA_SPEAKER	3
#define AMS_DELTA_AGC		4

#define AMS_DELTA_MIXED		((1 << AMS_DELTA_EARPIECE) | \
						(1 << AMS_DELTA_MICROPHONE))
#define AMS_DELTA_HANDSET	((1 << AMS_DELTA_MOUTHPIECE) | \
						(1 << AMS_DELTA_EARPIECE))
#define AMS_DELTA_HANDSFREE	((1 << AMS_DELTA_MICROPHONE) | \
						(1 << AMS_DELTA_SPEAKER))
#define AMS_DELTA_SPEAKERPHONE	(AMS_DELTA_HANDSFREE | (1 << AMS_DELTA_AGC))

static const unsigned short ams_delta_audio_mode_pins[] = {
	AMS_DELTA_MIXED,
	AMS_DELTA_HANDSET,
	AMS_DELTA_HANDSFREE,
	AMS_DELTA_SPEAKERPHONE,
};

static unsigned short ams_delta_audio_agc;

static int ams_delta_set_audio_mode(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);
	struct soc_enum *control = (struct soc_enum *)kcontrol->private_value;
	unsigned short pins;
	int pin, changed = 0;

	/* Refuse any mode changes if we are not able to control the codec. */
	if (!codec->control_data)
		return -EUNATCH;

	if (ucontrol->value.enumerated.item[0] >= control->max)
		return -EINVAL;

	mutex_lock(&codec->mutex);

	/* Translate selection to bitmap */
	pins = ams_delta_audio_mode_pins[ucontrol->value.enumerated.item[0]];

	/* Setup pins after corresponding bits if changed */
	pin = !!(pins & (1 << AMS_DELTA_MOUTHPIECE));
	if (pin != snd_soc_dapm_get_pin_status(codec, "Mouthpiece")) {
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(codec, "Mouthpiece");
		else
			snd_soc_dapm_disable_pin(codec, "Mouthpiece");
	}
	pin = !!(pins & (1 << AMS_DELTA_EARPIECE));
	if (pin != snd_soc_dapm_get_pin_status(codec, "Earpiece")) {
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(codec, "Earpiece");
		else
			snd_soc_dapm_disable_pin(codec, "Earpiece");
	}
	pin = !!(pins & (1 << AMS_DELTA_MICROPHONE));
	if (pin != snd_soc_dapm_get_pin_status(codec, "Microphone")) {
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(codec, "Microphone");
		else
			snd_soc_dapm_disable_pin(codec, "Microphone");
	}
	pin = !!(pins & (1 << AMS_DELTA_SPEAKER));
	if (pin != snd_soc_dapm_get_pin_status(codec, "Speaker")) {
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(codec, "Speaker");
		else
			snd_soc_dapm_disable_pin(codec, "Speaker");
	}
	pin = !!(pins & (1 << AMS_DELTA_AGC));
	if (pin != ams_delta_audio_agc) {
		ams_delta_audio_agc = pin;
		changed = 1;
		if (pin)
			snd_soc_dapm_enable_pin(codec, "AGCIN");
		else
			snd_soc_dapm_disable_pin(codec, "AGCIN");
	}
	if (changed)
		snd_soc_dapm_sync(codec);

	mutex_unlock(&codec->mutex);

	return changed;
}

static int ams_delta_get_audio_mode(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec =  snd_kcontrol_chip(kcontrol);
	unsigned short pins, mode;

	pins = ((snd_soc_dapm_get_pin_status(codec, "Mouthpiece") <<
							AMS_DELTA_MOUTHPIECE) |
			(snd_soc_dapm_get_pin_status(codec, "Earpiece") <<
							AMS_DELTA_EARPIECE));
	if (pins)
		pins |= (snd_soc_dapm_get_pin_status(codec, "Microphone") <<
							AMS_DELTA_MICROPHONE);
	else
		pins = ((snd_soc_dapm_get_pin_status(codec, "Microphone") <<
							AMS_DELTA_MICROPHONE) |
			(snd_soc_dapm_get_pin_status(codec, "Speaker") <<
							AMS_DELTA_SPEAKER) |
			(ams_delta_audio_agc << AMS_DELTA_AGC));

	for (mode = 0; mode < ARRAY_SIZE(ams_delta_audio_mode); mode++)
		if (pins == ams_delta_audio_mode_pins[mode])
			break;

	if (mode >= ARRAY_SIZE(ams_delta_audio_mode))
		return -EINVAL;

	ucontrol->value.enumerated.item[0] = mode;

	return 0;
}

static const struct soc_enum ams_delta_audio_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ams_delta_audio_mode),
						ams_delta_audio_mode),
};

static const struct snd_kcontrol_new ams_delta_audio_controls[] = {
	SOC_ENUM_EXT("Audio Mode", ams_delta_audio_enum[0],
			ams_delta_get_audio_mode, ams_delta_set_audio_mode),
};

/* Hook switch */
static struct snd_soc_jack ams_delta_hook_switch;
static struct snd_soc_jack_gpio ams_delta_hook_switch_gpios[] = {
	{
		.gpio = 4,
		.name = "hook_switch",
		.report = SND_JACK_HEADSET,
		.invert = 1,
		.debounce_time = 150,
	}
};

/* After we are able to control the codec over the modem,
 * the hook switch can be used for dynamic DAPM reconfiguration. */
static struct snd_soc_jack_pin ams_delta_hook_switch_pins[] = {
	/* Handset */
	{
		.pin = "Mouthpiece",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Earpiece",
		.mask = SND_JACK_HEADPHONE,
	},
	/* Handsfree */
	{
		.pin = "Microphone",
		.mask = SND_JACK_MICROPHONE,
		.invert = 1,
	},
	{
		.pin = "Speaker",
		.mask = SND_JACK_HEADPHONE,
		.invert = 1,
	},
};


/*
 * Modem line discipline, required for making above controls functional.
 * Activated from userspace with ldattach, possibly invoked from udev rule.
 */

/* To actually apply any modem controlled configuration changes to the codec,
 * we must connect codec DAI pins to the modem for a moment.  Be carefull not
 * to interfere with our digital mute function that shares the same hardware. */
static struct timer_list cx81801_timer;
static bool cx81801_cmd_pending;
static bool ams_delta_muted;
static DEFINE_SPINLOCK(ams_delta_lock);

static void cx81801_timeout(unsigned long data)
{
	int muted;

	spin_lock(&ams_delta_lock);
	cx81801_cmd_pending = 0;
	muted = ams_delta_muted;
	spin_unlock(&ams_delta_lock);

	/* Reconnect the codec DAI back from the modem to the CPU DAI
	 * only if digital mute still off */
	if (!muted)
		ams_delta_latch2_write(AMS_DELTA_LATCH2_MODEM_CODEC, 0);
}

/* Line discipline .open() */
static int cx81801_open(struct tty_struct *tty)
{
	return v253_ops.open(tty);
}

/* Line discipline .close() */
static void cx81801_close(struct tty_struct *tty)
{
	struct snd_soc_codec *codec = tty->disc_data;

	del_timer_sync(&cx81801_timer);

	v253_ops.close(tty);

	/* Prevent the hook switch from further changing the DAPM pins */
	INIT_LIST_HEAD(&ams_delta_hook_switch.pins);

	/* Revert back to default audio input/output constellation */
	snd_soc_dapm_disable_pin(codec, "Mouthpiece");
	snd_soc_dapm_enable_pin(codec, "Earpiece");
	snd_soc_dapm_enable_pin(codec, "Microphone");
	snd_soc_dapm_disable_pin(codec, "Speaker");
	snd_soc_dapm_disable_pin(codec, "AGCIN");
	snd_soc_dapm_sync(codec);
}

/* Line discipline .hangup() */
static int cx81801_hangup(struct tty_struct *tty)
{
	cx81801_close(tty);
	return 0;
}

/* Line discipline .recieve_buf() */
static void cx81801_receive(struct tty_struct *tty,
				const unsigned char *cp, char *fp, int count)
{
	struct snd_soc_codec *codec = tty->disc_data;
	const unsigned char *c;
	int apply, ret;

	if (!codec->control_data) {
		/* First modem response, complete setup procedure */

		/* Initialize timer used for config pulse generation */
		setup_timer(&cx81801_timer, cx81801_timeout, 0);

		v253_ops.receive_buf(tty, cp, fp, count);

		/* Link hook switch to DAPM pins */
		ret = snd_soc_jack_add_pins(&ams_delta_hook_switch,
					ARRAY_SIZE(ams_delta_hook_switch_pins),
					ams_delta_hook_switch_pins);
		if (ret)
			dev_warn(codec->socdev->card->dev,
				"Failed to link hook switch to DAPM pins, "
				"will continue with hook switch unlinked.\n");

		return;
	}

	v253_ops.receive_buf(tty, cp, fp, count);

	for (c = &cp[count - 1]; c >= cp; c--) {
		if (*c != '\r')
			continue;
		/* Complete modem response received, apply config to codec */

		spin_lock_bh(&ams_delta_lock);
		mod_timer(&cx81801_timer, jiffies + msecs_to_jiffies(150));
		apply = !ams_delta_muted && !cx81801_cmd_pending;
		cx81801_cmd_pending = 1;
		spin_unlock_bh(&ams_delta_lock);

		/* Apply config pulse by connecting the codec to the modem
		 * if not already done */
		if (apply)
			ams_delta_latch2_write(AMS_DELTA_LATCH2_MODEM_CODEC,
						AMS_DELTA_LATCH2_MODEM_CODEC);
		break;
	}
}

/* Line discipline .write_wakeup() */
static void cx81801_wakeup(struct tty_struct *tty)
{
	v253_ops.write_wakeup(tty);
}

static struct tty_ldisc_ops cx81801_ops = {
	.magic = TTY_LDISC_MAGIC,
	.name = "cx81801",
	.owner = THIS_MODULE,
	.open = cx81801_open,
	.close = cx81801_close,
	.hangup = cx81801_hangup,
	.receive_buf = cx81801_receive,
	.write_wakeup = cx81801_wakeup,
};


/*
 * Even if not very usefull, the sound card can still work without any of the
 * above functonality activated.  You can still control its audio input/output
 * constellation and speakerphone gain from userspace by issueing AT commands
 * over the modem port.
 */

static int ams_delta_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	/* Set cpu DAI configuration */
	return snd_soc_dai_set_fmt(rtd->dai->cpu_dai,
				   SND_SOC_DAIFMT_DSP_A |
				   SND_SOC_DAIFMT_NB_NF |
				   SND_SOC_DAIFMT_CBM_CFM);
}

static struct snd_soc_ops ams_delta_ops = {
	.hw_params = ams_delta_hw_params,
};


/* Board specific codec bias level control */
static int ams_delta_set_bias_level(struct snd_soc_card *card,
					enum snd_soc_bias_level level)
{
	struct snd_soc_codec *codec = card->codec;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		if (codec->bias_level == SND_SOC_BIAS_OFF)
			ams_delta_latch2_write(AMS_DELTA_LATCH2_MODEM_NRESET,
						AMS_DELTA_LATCH2_MODEM_NRESET);
		break;
	case SND_SOC_BIAS_OFF:
		if (codec->bias_level != SND_SOC_BIAS_OFF)
			ams_delta_latch2_write(AMS_DELTA_LATCH2_MODEM_NRESET,
						0);
	}
	codec->bias_level = level;

	return 0;
}

/* Digital mute implemented using modem/CPU multiplexer.
 * Shares hardware with codec config pulse generation */
static bool ams_delta_muted = 1;

static int ams_delta_digital_mute(struct snd_soc_dai *dai, int mute)
{
	int apply;

	if (ams_delta_muted == mute)
		return 0;

	spin_lock_bh(&ams_delta_lock);
	ams_delta_muted = mute;
	apply = !cx81801_cmd_pending;
	spin_unlock_bh(&ams_delta_lock);

	if (apply)
		ams_delta_latch2_write(AMS_DELTA_LATCH2_MODEM_CODEC,
				mute ? AMS_DELTA_LATCH2_MODEM_CODEC : 0);
	return 0;
}

/* Our codec DAI probably doesn't have its own .ops structure */
static struct snd_soc_dai_ops ams_delta_dai_ops = {
	.digital_mute = ams_delta_digital_mute,
};

/* Will be used if the codec ever has its own digital_mute function */
static int ams_delta_startup(struct snd_pcm_substream *substream)
{
	return ams_delta_digital_mute(NULL, 0);
}

static void ams_delta_shutdown(struct snd_pcm_substream *substream)
{
	ams_delta_digital_mute(NULL, 1);
}


/*
 * Card initialization
 */

static int ams_delta_cx20442_init(struct snd_soc_codec *codec)
{
	struct snd_soc_dai *codec_dai = codec->dai;
	struct snd_soc_card *card = codec->socdev->card;
	int ret;
	/* Codec is ready, now add/activate board specific controls */

	/* Set up digital mute if not provided by the codec */
	if (!codec_dai->ops) {
		codec_dai->ops = &ams_delta_dai_ops;
	} else if (!codec_dai->ops->digital_mute) {
		codec_dai->ops->digital_mute = ams_delta_digital_mute;
	} else {
		ams_delta_ops.startup = ams_delta_startup;
		ams_delta_ops.shutdown = ams_delta_shutdown;
	}

	/* Set codec bias level */
	ams_delta_set_bias_level(card, SND_SOC_BIAS_STANDBY);

	/* Add hook switch - can be used to control the codec from userspace
	 * even if line discipline fails */
	ret = snd_soc_jack_new(card, "hook_switch",
				SND_JACK_HEADSET, &ams_delta_hook_switch);
	if (ret)
		dev_warn(card->dev,
				"Failed to allocate resources for hook switch, "
				"will continue without one.\n");
	else {
		ret = snd_soc_jack_add_gpios(&ams_delta_hook_switch,
					ARRAY_SIZE(ams_delta_hook_switch_gpios),
					ams_delta_hook_switch_gpios);
		if (ret)
			dev_warn(card->dev,
				"Failed to set up hook switch GPIO line, "
				"will continue with hook switch inactive.\n");
	}

	/* Register optional line discipline for over the modem control */
	ret = tty_register_ldisc(N_V253, &cx81801_ops);
	if (ret) {
		dev_warn(card->dev,
				"Failed to register line discipline, "
				"will continue without any controls.\n");
		return 0;
	}

	/* Add board specific DAPM widgets and routes */
	ret = snd_soc_dapm_new_controls(codec, ams_delta_dapm_widgets,
					ARRAY_SIZE(ams_delta_dapm_widgets));
	if (ret) {
		dev_warn(card->dev,
				"Failed to register DAPM controls, "
				"will continue without any.\n");
		return 0;
	}

	ret = snd_soc_dapm_add_routes(codec, ams_delta_audio_map,
					ARRAY_SIZE(ams_delta_audio_map));
	if (ret) {
		dev_warn(card->dev,
				"Failed to set up DAPM routes, "
				"will continue with codec default map.\n");
		return 0;
	}

	/* Set up initial pin constellation */
	snd_soc_dapm_disable_pin(codec, "Mouthpiece");
	snd_soc_dapm_enable_pin(codec, "Earpiece");
	snd_soc_dapm_enable_pin(codec, "Microphone");
	snd_soc_dapm_disable_pin(codec, "Speaker");
	snd_soc_dapm_disable_pin(codec, "AGCIN");
	snd_soc_dapm_disable_pin(codec, "AGCOUT");
	snd_soc_dapm_sync(codec);

	/* Add virtual switch */
	ret = snd_soc_add_controls(codec, ams_delta_audio_controls,
					ARRAY_SIZE(ams_delta_audio_controls));
	if (ret)
		dev_warn(card->dev,
				"Failed to register audio mode control, "
				"will continue without it.\n");

	return 0;
}

/* DAI glue - connects codec <--> CPU */
static struct snd_soc_dai_link ams_delta_dai_link = {
	.name = "CX20442",
	.stream_name = "CX20442",
	.cpu_dai = &omap_mcbsp_dai[0],
	.codec_dai = &cx20442_dai,
	.init = ams_delta_cx20442_init,
	.ops = &ams_delta_ops,
};

/* Audio card driver */
static struct snd_soc_card ams_delta_audio_card = {
	.name = "AMS_DELTA",
	.platform = &omap_soc_platform,
	.dai_link = &ams_delta_dai_link,
	.num_links = 1,
	.set_bias_level = ams_delta_set_bias_level,
};

/* Audio subsystem */
static struct snd_soc_device ams_delta_snd_soc_device = {
	.card = &ams_delta_audio_card,
	.codec_dev = &cx20442_codec_dev,
};

/* Module init/exit */
static struct platform_device *ams_delta_audio_platform_device;
static struct platform_device *cx20442_platform_device;

static int __init ams_delta_module_init(void)
{
	int ret;

	if (!(machine_is_ams_delta()))
		return -ENODEV;

	ams_delta_audio_platform_device =
			platform_device_alloc("soc-audio", -1);
	if (!ams_delta_audio_platform_device)
		return -ENOMEM;

	platform_set_drvdata(ams_delta_audio_platform_device,
				&ams_delta_snd_soc_device);
	ams_delta_snd_soc_device.dev = &ams_delta_audio_platform_device->dev;
	*(unsigned int *)ams_delta_dai_link.cpu_dai->private_data = OMAP_MCBSP1;

	ret = platform_device_add(ams_delta_audio_platform_device);
	if (ret)
		goto err;

	/*
	 * Codec platform device could be registered from elsewhere (board?),
	 * but I do it here as it makes sense only if used with the card.
	 */
	cx20442_platform_device = platform_device_register_simple("cx20442",
								-1, NULL, 0);
	return 0;
err:
	platform_device_put(ams_delta_audio_platform_device);
	return ret;
}
module_init(ams_delta_module_init);

static void __exit ams_delta_module_exit(void)
{
	struct snd_soc_codec *codec;
	struct tty_struct *tty;

	if (ams_delta_audio_card.codec) {
		codec = ams_delta_audio_card.codec;

		if (codec->control_data) {
			tty = codec->control_data;

			tty_hangup(tty);
		}
	}

	if (tty_unregister_ldisc(N_V253) != 0)
		dev_warn(&ams_delta_audio_platform_device->dev,
			"failed to unregister V253 line discipline\n");

	snd_soc_jack_free_gpios(&ams_delta_hook_switch,
			ARRAY_SIZE(ams_delta_hook_switch_gpios),
			ams_delta_hook_switch_gpios);

	/* Keep modem power on */
	ams_delta_set_bias_level(&ams_delta_audio_card, SND_SOC_BIAS_STANDBY);

	platform_device_unregister(cx20442_platform_device);
	platform_device_unregister(ams_delta_audio_platform_device);
}
module_exit(ams_delta_module_exit);

MODULE_AUTHOR("Janusz Krzysztofik <jkrzyszt@tis.icnet.pl>");
MODULE_DESCRIPTION("ALSA SoC driver for Amstrad E3 (Delta) videophone");
MODULE_LICENSE("GPL");
