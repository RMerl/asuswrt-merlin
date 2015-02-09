/*
 *   (Tentative) USB Audio Driver for ALSA
 *
 *   Mixer control part
 *
 *   Copyright (c) 2002 by Takashi Iwai <tiwai@suse.de>
 *
 *   Many codes borrowed from audio.c by
 *	    Alan Cox (alan@lxorguk.ukuu.org.uk)
 *	    Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 * TODOs, for both the mixer and the streaming interfaces:
 *
 *  - support for UAC2 effect units
 *  - support for graphical equalizers
 *  - RANGE and MEM set commands (UAC2)
 *  - RANGE and MEM interrupt dispatchers (UAC2)
 *  - audio channel clustering (UAC2)
 *  - audio sample rate converter units (UAC2)
 *  - proper handling of clock multipliers (UAC2)
 *  - dispatch clock change notifications (UAC2)
 *  	- stop PCM streams which use a clock that became invalid
 *  	- stop PCM streams which use a clock selector that has changed
 *  	- parse available sample rates again when clock sources changed
 */

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/usb.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>

#include <sound/core.h>
#include <sound/control.h>
#include <sound/hwdep.h>
#include <sound/info.h>
#include <sound/tlv.h>

#include "usbaudio.h"
#include "mixer.h"
#include "helper.h"
#include "mixer_quirks.h"

#define MAX_ID_ELEMS	256

struct usb_audio_term {
	int id;
	int type;
	int channels;
	unsigned int chconfig;
	int name;
};

struct usbmix_name_map;

struct mixer_build {
	struct snd_usb_audio *chip;
	struct usb_mixer_interface *mixer;
	unsigned char *buffer;
	unsigned int buflen;
	DECLARE_BITMAP(unitbitmap, MAX_ID_ELEMS);
	struct usb_audio_term oterm;
	const struct usbmix_name_map *map;
	const struct usbmix_selector_map *selector_map;
};

enum {
	USB_MIXER_BOOLEAN,
	USB_MIXER_INV_BOOLEAN,
	USB_MIXER_S8,
	USB_MIXER_U8,
	USB_MIXER_S16,
	USB_MIXER_U16,
};


/*E-mu 0202(0404) eXtension Unit(XU) control*/
enum {
	USB_XU_CLOCK_RATE 		= 0xe301,
	USB_XU_CLOCK_SOURCE		= 0xe302,
	USB_XU_DIGITAL_IO_STATUS	= 0xe303,
	USB_XU_DEVICE_OPTIONS		= 0xe304,
	USB_XU_DIRECT_MONITORING	= 0xe305,
	USB_XU_METERING			= 0xe306
};
enum {
	USB_XU_CLOCK_SOURCE_SELECTOR = 0x02,	/* clock source*/
	USB_XU_CLOCK_RATE_SELECTOR = 0x03,	/* clock rate */
	USB_XU_DIGITAL_FORMAT_SELECTOR = 0x01,	/* the spdif format */
	USB_XU_SOFT_LIMIT_SELECTOR = 0x03	/* soft limiter */
};

/*
 * manual mapping of mixer names
 * if the mixer topology is too complicated and the parsed names are
 * ambiguous, add the entries in usbmixer_maps.c.
 */
#include "mixer_maps.c"

static const struct usbmix_name_map *
find_map(struct mixer_build *state, int unitid, int control)
{
	const struct usbmix_name_map *p = state->map;

	if (!p)
		return NULL;

	for (p = state->map; p->id; p++) {
		if (p->id == unitid &&
		    (!control || !p->control || control == p->control))
			return p;
	}
	return NULL;
}

/* get the mapped name if the unit matches */
static int
check_mapped_name(const struct usbmix_name_map *p, char *buf, int buflen)
{
	if (!p || !p->name)
		return 0;

	buflen--;
	return strlcpy(buf, p->name, buflen);
}

/* check whether the control should be ignored */
static inline int
check_ignored_ctl(const struct usbmix_name_map *p)
{
	if (!p || p->name || p->dB)
		return 0;
	return 1;
}

/* dB mapping */
static inline void check_mapped_dB(const struct usbmix_name_map *p,
				   struct usb_mixer_elem_info *cval)
{
	if (p && p->dB) {
		cval->dBmin = p->dB->min;
		cval->dBmax = p->dB->max;
	}
}

/* get the mapped selector source name */
static int check_mapped_selector_name(struct mixer_build *state, int unitid,
				      int index, char *buf, int buflen)
{
	const struct usbmix_selector_map *p;

	if (! state->selector_map)
		return 0;
	for (p = state->selector_map; p->id; p++) {
		if (p->id == unitid && index < p->count)
			return strlcpy(buf, p->names[index], buflen);
	}
	return 0;
}

/*
 * find an audio control unit with the given unit id
 */
static void *find_audio_control_unit(struct mixer_build *state, unsigned char unit)
{
	/* we just parse the header */
	struct uac_feature_unit_descriptor *hdr = NULL;

	while ((hdr = snd_usb_find_desc(state->buffer, state->buflen, hdr,
					USB_DT_CS_INTERFACE)) != NULL) {
		if (hdr->bLength >= 4 &&
		    hdr->bDescriptorSubtype >= UAC_INPUT_TERMINAL &&
		    hdr->bDescriptorSubtype <= UAC2_SAMPLE_RATE_CONVERTER &&
		    hdr->bUnitID == unit)
			return hdr;
	}

	return NULL;
}

/*
 * copy a string with the given id
 */
static int snd_usb_copy_string_desc(struct mixer_build *state, int index, char *buf, int maxlen)
{
	int len = usb_string(state->chip->dev, index, buf, maxlen - 1);
	buf[len] = 0;
	return len;
}

/*
 * convert from the byte/word on usb descriptor to the zero-based integer
 */
static int convert_signed_value(struct usb_mixer_elem_info *cval, int val)
{
	switch (cval->val_type) {
	case USB_MIXER_BOOLEAN:
		return !!val;
	case USB_MIXER_INV_BOOLEAN:
		return !val;
	case USB_MIXER_U8:
		val &= 0xff;
		break;
	case USB_MIXER_S8:
		val &= 0xff;
		if (val >= 0x80)
			val -= 0x100;
		break;
	case USB_MIXER_U16:
		val &= 0xffff;
		break;
	case USB_MIXER_S16:
		val &= 0xffff;
		if (val >= 0x8000)
			val -= 0x10000;
		break;
	}
	return val;
}

/*
 * convert from the zero-based int to the byte/word for usb descriptor
 */
static int convert_bytes_value(struct usb_mixer_elem_info *cval, int val)
{
	switch (cval->val_type) {
	case USB_MIXER_BOOLEAN:
		return !!val;
	case USB_MIXER_INV_BOOLEAN:
		return !val;
	case USB_MIXER_S8:
	case USB_MIXER_U8:
		return val & 0xff;
	case USB_MIXER_S16:
	case USB_MIXER_U16:
		return val & 0xffff;
	}
	return 0; /* not reached */
}

static int get_relative_value(struct usb_mixer_elem_info *cval, int val)
{
	if (! cval->res)
		cval->res = 1;
	if (val < cval->min)
		return 0;
	else if (val >= cval->max)
		return (cval->max - cval->min + cval->res - 1) / cval->res;
	else
		return (val - cval->min) / cval->res;
}

static int get_abs_value(struct usb_mixer_elem_info *cval, int val)
{
	if (val < 0)
		return cval->min;
	if (! cval->res)
		cval->res = 1;
	val *= cval->res;
	val += cval->min;
	if (val > cval->max)
		return cval->max;
	return val;
}


/*
 * retrieve a mixer value
 */

static int get_ctl_value_v1(struct usb_mixer_elem_info *cval, int request, int validx, int *value_ret)
{
	struct snd_usb_audio *chip = cval->mixer->chip;
	unsigned char buf[2];
	int val_len = cval->val_type >= USB_MIXER_S16 ? 2 : 1;
	int timeout = 10;

	while (timeout-- > 0) {
		if (snd_usb_ctl_msg(chip->dev, usb_rcvctrlpipe(chip->dev, 0), request,
				    USB_RECIP_INTERFACE | USB_TYPE_CLASS | USB_DIR_IN,
				    validx, snd_usb_ctrl_intf(chip) | (cval->id << 8),
				    buf, val_len, 100) >= val_len) {
			*value_ret = convert_signed_value(cval, snd_usb_combine_bytes(buf, val_len));
			return 0;
		}
	}
	snd_printdd(KERN_ERR "cannot get ctl value: req = %#x, wValue = %#x, wIndex = %#x, type = %d\n",
		    request, validx, snd_usb_ctrl_intf(chip) | (cval->id << 8), cval->val_type);
	return -EINVAL;
}

static int get_ctl_value_v2(struct usb_mixer_elem_info *cval, int request, int validx, int *value_ret)
{
	struct snd_usb_audio *chip = cval->mixer->chip;
	unsigned char buf[2 + 3*sizeof(__u16)]; /* enough space for one range */
	unsigned char *val;
	int ret, size;
	__u8 bRequest;

	if (request == UAC_GET_CUR) {
		bRequest = UAC2_CS_CUR;
		size = sizeof(__u16);
	} else {
		bRequest = UAC2_CS_RANGE;
		size = sizeof(buf);
	}

	memset(buf, 0, sizeof(buf));

	ret = snd_usb_ctl_msg(chip->dev, usb_rcvctrlpipe(chip->dev, 0), bRequest,
			      USB_RECIP_INTERFACE | USB_TYPE_CLASS | USB_DIR_IN,
			      validx, snd_usb_ctrl_intf(chip) | (cval->id << 8),
			      buf, size, 1000);

	if (ret < 0) {
		snd_printk(KERN_ERR "cannot get ctl value: req = %#x, wValue = %#x, wIndex = %#x, type = %d\n",
			   request, validx, snd_usb_ctrl_intf(chip) | (cval->id << 8), cval->val_type);
		return ret;
	}


	switch (request) {
	case UAC_GET_CUR:
		val = buf;
		break;
	case UAC_GET_MIN:
		val = buf + sizeof(__u16);
		break;
	case UAC_GET_MAX:
		val = buf + sizeof(__u16) * 2;
		break;
	case UAC_GET_RES:
		val = buf + sizeof(__u16) * 3;
		break;
	default:
		return -EINVAL;
	}

	*value_ret = convert_signed_value(cval, snd_usb_combine_bytes(val, sizeof(__u16)));

	return 0;
}

static int get_ctl_value(struct usb_mixer_elem_info *cval, int request, int validx, int *value_ret)
{
	return (cval->mixer->protocol == UAC_VERSION_1) ?
		get_ctl_value_v1(cval, request, validx, value_ret) :
		get_ctl_value_v2(cval, request, validx, value_ret);
}

static int get_cur_ctl_value(struct usb_mixer_elem_info *cval, int validx, int *value)
{
	return get_ctl_value(cval, UAC_GET_CUR, validx, value);
}

/* channel = 0: master, 1 = first channel */
static inline int get_cur_mix_raw(struct usb_mixer_elem_info *cval,
				  int channel, int *value)
{
	return get_ctl_value(cval, UAC_GET_CUR, (cval->control << 8) | channel, value);
}

static int get_cur_mix_value(struct usb_mixer_elem_info *cval,
			     int channel, int index, int *value)
{
	int err;

	if (cval->cached & (1 << channel)) {
		*value = cval->cache_val[index];
		return 0;
	}
	err = get_cur_mix_raw(cval, channel, value);
	if (err < 0) {
		if (!cval->mixer->ignore_ctl_error)
			snd_printd(KERN_ERR "cannot get current value for control %d ch %d: err = %d\n",
				   cval->control, channel, err);
		return err;
	}
	cval->cached |= 1 << channel;
	cval->cache_val[index] = *value;
	return 0;
}


/*
 * set a mixer value
 */

int snd_usb_mixer_set_ctl_value(struct usb_mixer_elem_info *cval,
				int request, int validx, int value_set)
{
	struct snd_usb_audio *chip = cval->mixer->chip;
	unsigned char buf[2];
	int val_len, timeout = 10;

	if (cval->mixer->protocol == UAC_VERSION_1) {
		val_len = cval->val_type >= USB_MIXER_S16 ? 2 : 1;
	} else { /* UAC_VERSION_2 */
		/* audio class v2 controls are always 2 bytes in size */
		val_len = sizeof(__u16);

		if (request != UAC_SET_CUR) {
			snd_printdd(KERN_WARNING "RANGE setting not yet supported\n");
			return -EINVAL;
		}

		request = UAC2_CS_CUR;
	}

	value_set = convert_bytes_value(cval, value_set);
	buf[0] = value_set & 0xff;
	buf[1] = (value_set >> 8) & 0xff;
	while (timeout-- > 0)
		if (snd_usb_ctl_msg(chip->dev,
				    usb_sndctrlpipe(chip->dev, 0), request,
				    USB_RECIP_INTERFACE | USB_TYPE_CLASS | USB_DIR_OUT,
				    validx, snd_usb_ctrl_intf(chip) | (cval->id << 8),
				    buf, val_len, 100) >= 0)
			return 0;
	snd_printdd(KERN_ERR "cannot set ctl value: req = %#x, wValue = %#x, wIndex = %#x, type = %d, data = %#x/%#x\n",
		    request, validx, snd_usb_ctrl_intf(chip) | (cval->id << 8), cval->val_type, buf[0], buf[1]);
	return -EINVAL;
}

static int set_cur_ctl_value(struct usb_mixer_elem_info *cval, int validx, int value)
{
	return snd_usb_mixer_set_ctl_value(cval, UAC_SET_CUR, validx, value);
}

static int set_cur_mix_value(struct usb_mixer_elem_info *cval, int channel,
			     int index, int value)
{
	int err;
	unsigned int read_only = (channel == 0) ?
		cval->master_readonly :
		cval->ch_readonly & (1 << (channel - 1));

	if (read_only) {
		snd_printdd(KERN_INFO "%s(): channel %d of control %d is read_only\n",
			    __func__, channel, cval->control);
		return 0;
	}

	err = snd_usb_mixer_set_ctl_value(cval, UAC_SET_CUR, (cval->control << 8) | channel,
			    value);
	if (err < 0)
		return err;
	cval->cached |= 1 << channel;
	cval->cache_val[index] = value;
	return 0;
}

/*
 * TLV callback for mixer volume controls
 */
static int mixer_vol_tlv(struct snd_kcontrol *kcontrol, int op_flag,
			 unsigned int size, unsigned int __user *_tlv)
{
	struct usb_mixer_elem_info *cval = kcontrol->private_data;
	DECLARE_TLV_DB_MINMAX(scale, 0, 0);

	if (size < sizeof(scale))
		return -ENOMEM;
	scale[2] = cval->dBmin;
	scale[3] = cval->dBmax;
	if (copy_to_user(_tlv, scale, sizeof(scale)))
		return -EFAULT;
	return 0;
}

/*
 * parser routines begin here...
 */

static int parse_audio_unit(struct mixer_build *state, int unitid);


/*
 * check if the input/output channel routing is enabled on the given bitmap.
 * used for mixer unit parser
 */
static int check_matrix_bitmap(unsigned char *bmap, int ich, int och, int num_outs)
{
	int idx = ich * num_outs + och;
	return bmap[idx >> 3] & (0x80 >> (idx & 7));
}


/*
 * add an alsa control element
 * search and increment the index until an empty slot is found.
 *
 * if failed, give up and free the control instance.
 */

static int add_control_to_empty(struct mixer_build *state, struct snd_kcontrol *kctl)
{
	struct usb_mixer_elem_info *cval = kctl->private_data;
	int err;

	while (snd_ctl_find_id(state->chip->card, &kctl->id))
		kctl->id.index++;
	if ((err = snd_ctl_add(state->chip->card, kctl)) < 0) {
		snd_printd(KERN_ERR "cannot add control (err = %d)\n", err);
		return err;
	}
	cval->elem_id = &kctl->id;
	cval->next_id_elem = state->mixer->id_elems[cval->id];
	state->mixer->id_elems[cval->id] = cval;
	return 0;
}


/*
 * get a terminal name string
 */

static struct iterm_name_combo {
	int type;
	char *name;
} iterm_names[] = {
	{ 0x0300, "Output" },
	{ 0x0301, "Speaker" },
	{ 0x0302, "Headphone" },
	{ 0x0303, "HMD Audio" },
	{ 0x0304, "Desktop Speaker" },
	{ 0x0305, "Room Speaker" },
	{ 0x0306, "Com Speaker" },
	{ 0x0307, "LFE" },
	{ 0x0600, "External In" },
	{ 0x0601, "Analog In" },
	{ 0x0602, "Digital In" },
	{ 0x0603, "Line" },
	{ 0x0604, "Legacy In" },
	{ 0x0605, "IEC958 In" },
	{ 0x0606, "1394 DA Stream" },
	{ 0x0607, "1394 DV Stream" },
	{ 0x0700, "Embedded" },
	{ 0x0701, "Noise Source" },
	{ 0x0702, "Equalization Noise" },
	{ 0x0703, "CD" },
	{ 0x0704, "DAT" },
	{ 0x0705, "DCC" },
	{ 0x0706, "MiniDisk" },
	{ 0x0707, "Analog Tape" },
	{ 0x0708, "Phonograph" },
	{ 0x0709, "VCR Audio" },
	{ 0x070a, "Video Disk Audio" },
	{ 0x070b, "DVD Audio" },
	{ 0x070c, "TV Tuner Audio" },
	{ 0x070d, "Satellite Rec Audio" },
	{ 0x070e, "Cable Tuner Audio" },
	{ 0x070f, "DSS Audio" },
	{ 0x0710, "Radio Receiver" },
	{ 0x0711, "Radio Transmitter" },
	{ 0x0712, "Multi-Track Recorder" },
	{ 0x0713, "Synthesizer" },
	{ 0 },
};

static int get_term_name(struct mixer_build *state, struct usb_audio_term *iterm,
			 unsigned char *name, int maxlen, int term_only)
{
	struct iterm_name_combo *names;

	if (iterm->name)
		return snd_usb_copy_string_desc(state, iterm->name, name, maxlen);

	/* virtual type - not a real terminal */
	if (iterm->type >> 16) {
		if (term_only)
			return 0;
		switch (iterm->type >> 16) {
		case UAC_SELECTOR_UNIT:
			strcpy(name, "Selector"); return 8;
		case UAC1_PROCESSING_UNIT:
			strcpy(name, "Process Unit"); return 12;
		case UAC1_EXTENSION_UNIT:
			strcpy(name, "Ext Unit"); return 8;
		case UAC_MIXER_UNIT:
			strcpy(name, "Mixer"); return 5;
		default:
			return sprintf(name, "Unit %d", iterm->id);
		}
	}

	switch (iterm->type & 0xff00) {
	case 0x0100:
		strcpy(name, "PCM"); return 3;
	case 0x0200:
		strcpy(name, "Mic"); return 3;
	case 0x0400:
		strcpy(name, "Headset"); return 7;
	case 0x0500:
		strcpy(name, "Phone"); return 5;
	}

	for (names = iterm_names; names->type; names++)
		if (names->type == iterm->type) {
			strcpy(name, names->name);
			return strlen(names->name);
		}
	return 0;
}


/*
 * parse the source unit recursively until it reaches to a terminal
 * or a branched unit.
 */
static int check_input_term(struct mixer_build *state, int id, struct usb_audio_term *term)
{
	int err;
	void *p1;

	memset(term, 0, sizeof(*term));
	while ((p1 = find_audio_control_unit(state, id)) != NULL) {
		unsigned char *hdr = p1;
		term->id = id;
		switch (hdr[2]) {
		case UAC_INPUT_TERMINAL:
			if (state->mixer->protocol == UAC_VERSION_1) {
				struct uac_input_terminal_descriptor *d = p1;
				term->type = le16_to_cpu(d->wTerminalType);
				term->channels = d->bNrChannels;
				term->chconfig = le16_to_cpu(d->wChannelConfig);
				term->name = d->iTerminal;
			} else { /* UAC_VERSION_2 */
				struct uac2_input_terminal_descriptor *d = p1;
				term->type = le16_to_cpu(d->wTerminalType);
				term->channels = d->bNrChannels;
				term->chconfig = le32_to_cpu(d->bmChannelConfig);
				term->name = d->iTerminal;

				/* call recursively to get the clock selectors */
				err = check_input_term(state, d->bCSourceID, term);
				if (err < 0)
					return err;
			}
			return 0;
		case UAC_FEATURE_UNIT: {
			/* the header is the same for v1 and v2 */
			struct uac_feature_unit_descriptor *d = p1;
			id = d->bSourceID;
			break; /* continue to parse */
		}
		case UAC_MIXER_UNIT: {
			struct uac_mixer_unit_descriptor *d = p1;
			term->type = d->bDescriptorSubtype << 16; /* virtual type */
			term->channels = uac_mixer_unit_bNrChannels(d);
			term->chconfig = uac_mixer_unit_wChannelConfig(d, state->mixer->protocol);
			term->name = uac_mixer_unit_iMixer(d);
			return 0;
		}
		case UAC_SELECTOR_UNIT:
		case UAC2_CLOCK_SELECTOR: {
			struct uac_selector_unit_descriptor *d = p1;
			/* call recursively to retrieve the channel info */
			if (check_input_term(state, d->baSourceID[0], term) < 0)
				return -ENODEV;
			term->type = d->bDescriptorSubtype << 16; /* virtual type */
			term->id = id;
			term->name = uac_selector_unit_iSelector(d);
			return 0;
		}
		case UAC1_PROCESSING_UNIT:
		case UAC1_EXTENSION_UNIT: {
			struct uac_processing_unit_descriptor *d = p1;
			if (d->bNrInPins) {
				id = d->baSourceID[0];
				break; /* continue to parse */
			}
			term->type = d->bDescriptorSubtype << 16; /* virtual type */
			term->channels = uac_processing_unit_bNrChannels(d);
			term->chconfig = uac_processing_unit_wChannelConfig(d, state->mixer->protocol);
			term->name = uac_processing_unit_iProcessing(d, state->mixer->protocol);
			return 0;
		}
		case UAC2_CLOCK_SOURCE: {
			struct uac_clock_source_descriptor *d = p1;
			term->type = d->bDescriptorSubtype << 16; /* virtual type */
			term->id = id;
			term->name = d->iClockSource;
			return 0;
		}
		default:
			return -ENODEV;
		}
	}
	return -ENODEV;
}


/*
 * Feature Unit
 */

/* feature unit control information */
struct usb_feature_control_info {
	const char *name;
	unsigned int type;	/* control type (mute, volume, etc.) */
};

static struct usb_feature_control_info audio_feature_info[] = {
	{ "Mute",			USB_MIXER_INV_BOOLEAN },
	{ "Volume",			USB_MIXER_S16 },
	{ "Tone Control - Bass",	USB_MIXER_S8 },
	{ "Tone Control - Mid",		USB_MIXER_S8 },
	{ "Tone Control - Treble",	USB_MIXER_S8 },
	{ "Graphic Equalizer",		USB_MIXER_S8 },
	{ "Auto Gain Control",		USB_MIXER_BOOLEAN },
	{ "Delay Control",		USB_MIXER_U16 },
	{ "Bass Boost",			USB_MIXER_BOOLEAN },
	{ "Loudness",			USB_MIXER_BOOLEAN },
	/* UAC2 specific */
	{ "Input Gain Control",		USB_MIXER_U16 },
	{ "Input Gain Pad Control",	USB_MIXER_BOOLEAN },
	{ "Phase Inverter Control",	USB_MIXER_BOOLEAN },
};


/* private_free callback */
static void usb_mixer_elem_free(struct snd_kcontrol *kctl)
{
	kfree(kctl->private_data);
	kctl->private_data = NULL;
}


/*
 * interface to ALSA control for feature/mixer units
 */

/*
 * retrieve the minimum and maximum values for the specified control
 */
static int get_min_max(struct usb_mixer_elem_info *cval, int default_min)
{
	struct snd_usb_audio *chip = cval->mixer->chip;

	/* for failsafe */
	cval->min = default_min;
	cval->max = cval->min + 1;
	cval->res = 1;
	cval->dBmin = cval->dBmax = 0;

	if (cval->val_type == USB_MIXER_BOOLEAN ||
	    cval->val_type == USB_MIXER_INV_BOOLEAN) {
		cval->initialized = 1;
	} else {
		int minchn = 0;
		if (cval->cmask) {
			int i;
			for (i = 0; i < MAX_CHANNELS; i++)
				if (cval->cmask & (1 << i)) {
					minchn = i + 1;
					break;
				}
		}
		if (get_ctl_value(cval, UAC_GET_MAX, (cval->control << 8) | minchn, &cval->max) < 0 ||
		    get_ctl_value(cval, UAC_GET_MIN, (cval->control << 8) | minchn, &cval->min) < 0) {
			snd_printd(KERN_ERR "%d:%d: cannot get min/max values for control %d (id %d)\n",
				   cval->id, snd_usb_ctrl_intf(chip), cval->control, cval->id);
			return -EINVAL;
		}
		if (get_ctl_value(cval, UAC_GET_RES, (cval->control << 8) | minchn, &cval->res) < 0) {
			cval->res = 1;
		} else {
			int last_valid_res = cval->res;

			while (cval->res > 1) {
				if (snd_usb_mixer_set_ctl_value(cval, UAC_SET_RES,
								(cval->control << 8) | minchn, cval->res / 2) < 0)
					break;
				cval->res /= 2;
			}
			if (get_ctl_value(cval, UAC_GET_RES, (cval->control << 8) | minchn, &cval->res) < 0)
				cval->res = last_valid_res;
		}
		if (cval->res == 0)
			cval->res = 1;

		/* Additional checks for the proper resolution
		 *
		 * Some devices report smaller resolutions than actually
		 * reacting.  They don't return errors but simply clip
		 * to the lower aligned value.
		 */
		if (cval->min + cval->res < cval->max) {
			int last_valid_res = cval->res;
			int saved, test, check;
			get_cur_mix_raw(cval, minchn, &saved);
			for (;;) {
				test = saved;
				if (test < cval->max)
					test += cval->res;
				else
					test -= cval->res;
				if (test < cval->min || test > cval->max ||
				    set_cur_mix_value(cval, minchn, 0, test) ||
				    get_cur_mix_raw(cval, minchn, &check)) {
					cval->res = last_valid_res;
					break;
				}
				if (test == check)
					break;
				cval->res *= 2;
			}
			set_cur_mix_value(cval, minchn, 0, saved);
		}

		cval->initialized = 1;
	}

	/* USB descriptions contain the dB scale in 1/256 dB unit
	 * while ALSA TLV contains in 1/100 dB unit
	 */
	cval->dBmin = (convert_signed_value(cval, cval->min) * 100) / 256;
	cval->dBmax = (convert_signed_value(cval, cval->max) * 100) / 256;
	if (cval->dBmin > cval->dBmax) {
		/* something is wrong; assume it's either from/to 0dB */
		if (cval->dBmin < 0)
			cval->dBmax = 0;
		else if (cval->dBmin > 0)
			cval->dBmin = 0;
		if (cval->dBmin > cval->dBmax) {
			/* totally crap, return an error */
			return -EINVAL;
		}
	}

	return 0;
}


/* get a feature/mixer unit info */
static int mixer_ctl_feature_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct usb_mixer_elem_info *cval = kcontrol->private_data;

	if (cval->val_type == USB_MIXER_BOOLEAN ||
	    cval->val_type == USB_MIXER_INV_BOOLEAN)
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	else
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = cval->channels;
	if (cval->val_type == USB_MIXER_BOOLEAN ||
	    cval->val_type == USB_MIXER_INV_BOOLEAN) {
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = 1;
	} else {
		if (! cval->initialized)
			get_min_max(cval,  0);
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max =
			(cval->max - cval->min + cval->res - 1) / cval->res;
	}
	return 0;
}

/* get the current value from feature/mixer unit */
static int mixer_ctl_feature_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *cval = kcontrol->private_data;
	int c, cnt, val, err;

	ucontrol->value.integer.value[0] = cval->min;
	if (cval->cmask) {
		cnt = 0;
		for (c = 0; c < MAX_CHANNELS; c++) {
			if (!(cval->cmask & (1 << c)))
				continue;
			err = get_cur_mix_value(cval, c + 1, cnt, &val);
			if (err < 0)
				return cval->mixer->ignore_ctl_error ? 0 : err;
			val = get_relative_value(cval, val);
			ucontrol->value.integer.value[cnt] = val;
			cnt++;
		}
		return 0;
	} else {
		/* master channel */
		err = get_cur_mix_value(cval, 0, 0, &val);
		if (err < 0)
			return cval->mixer->ignore_ctl_error ? 0 : err;
		val = get_relative_value(cval, val);
		ucontrol->value.integer.value[0] = val;
	}
	return 0;
}

/* put the current value to feature/mixer unit */
static int mixer_ctl_feature_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *cval = kcontrol->private_data;
	int c, cnt, val, oval, err;
	int changed = 0;

	if (cval->cmask) {
		cnt = 0;
		for (c = 0; c < MAX_CHANNELS; c++) {
			if (!(cval->cmask & (1 << c)))
				continue;
			err = get_cur_mix_value(cval, c + 1, cnt, &oval);
			if (err < 0)
				return cval->mixer->ignore_ctl_error ? 0 : err;
			val = ucontrol->value.integer.value[cnt];
			val = get_abs_value(cval, val);
			if (oval != val) {
				set_cur_mix_value(cval, c + 1, cnt, val);
				changed = 1;
			}
			cnt++;
		}
	} else {
		/* master channel */
		err = get_cur_mix_value(cval, 0, 0, &oval);
		if (err < 0)
			return cval->mixer->ignore_ctl_error ? 0 : err;
		val = ucontrol->value.integer.value[0];
		val = get_abs_value(cval, val);
		if (val != oval) {
			set_cur_mix_value(cval, 0, 0, val);
			changed = 1;
		}
	}
	return changed;
}

static struct snd_kcontrol_new usb_feature_unit_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "", /* will be filled later manually */
	.info = mixer_ctl_feature_info,
	.get = mixer_ctl_feature_get,
	.put = mixer_ctl_feature_put,
};

/* the read-only variant */
static struct snd_kcontrol_new usb_feature_unit_ctl_ro = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "", /* will be filled later manually */
	.info = mixer_ctl_feature_info,
	.get = mixer_ctl_feature_get,
	.put = NULL,
};


/*
 * build a feature control
 */

static size_t append_ctl_name(struct snd_kcontrol *kctl, const char *str)
{
	return strlcat(kctl->id.name, str, sizeof(kctl->id.name));
}

static void build_feature_ctl(struct mixer_build *state, void *raw_desc,
			      unsigned int ctl_mask, int control,
			      struct usb_audio_term *iterm, int unitid,
			      int readonly_mask)
{
	struct uac_feature_unit_descriptor *desc = raw_desc;
	unsigned int len = 0;
	int mapped_name = 0;
	int nameid = uac_feature_unit_iFeature(desc);
	struct snd_kcontrol *kctl;
	struct usb_mixer_elem_info *cval;
	const struct usbmix_name_map *map;

	control++; /* change from zero-based to 1-based value */

	if (control == UAC_FU_GRAPHIC_EQUALIZER) {
		return;
	}

	map = find_map(state, unitid, control);
	if (check_ignored_ctl(map))
		return;

	cval = kzalloc(sizeof(*cval), GFP_KERNEL);
	if (! cval) {
		snd_printk(KERN_ERR "cannot malloc kcontrol\n");
		return;
	}
	cval->mixer = state->mixer;
	cval->id = unitid;
	cval->control = control;
	cval->cmask = ctl_mask;
	cval->val_type = audio_feature_info[control-1].type;
	if (ctl_mask == 0) {
		cval->channels = 1;	/* master channel */
		cval->master_readonly = readonly_mask;
	} else {
		int i, c = 0;
		for (i = 0; i < 16; i++)
			if (ctl_mask & (1 << i))
				c++;
		cval->channels = c;
		cval->ch_readonly = readonly_mask;
	}

	/* get min/max values */
	get_min_max(cval, 0);

	/* if all channels in the mask are marked read-only, make the control
	 * read-only. set_cur_mix_value() will check the mask again and won't
	 * issue write commands to read-only channels. */
	if (cval->channels == readonly_mask)
		kctl = snd_ctl_new1(&usb_feature_unit_ctl_ro, cval);
	else
		kctl = snd_ctl_new1(&usb_feature_unit_ctl, cval);

	if (! kctl) {
		snd_printk(KERN_ERR "cannot malloc kcontrol\n");
		kfree(cval);
		return;
	}
	kctl->private_free = usb_mixer_elem_free;

	len = check_mapped_name(map, kctl->id.name, sizeof(kctl->id.name));
	mapped_name = len != 0;
	if (! len && nameid)
		len = snd_usb_copy_string_desc(state, nameid,
				kctl->id.name, sizeof(kctl->id.name));

	switch (control) {
	case UAC_FU_MUTE:
	case UAC_FU_VOLUME:
		/* determine the control name.  the rule is:
		 * - if a name id is given in descriptor, use it.
		 * - if the connected input can be determined, then use the name
		 *   of terminal type.
		 * - if the connected output can be determined, use it.
		 * - otherwise, anonymous name.
		 */
		if (! len) {
			len = get_term_name(state, iterm, kctl->id.name, sizeof(kctl->id.name), 1);
			if (! len)
				len = get_term_name(state, &state->oterm, kctl->id.name, sizeof(kctl->id.name), 1);
			if (! len)
				len = snprintf(kctl->id.name, sizeof(kctl->id.name),
					       "Feature %d", unitid);
		}
		/* determine the stream direction:
		 * if the connected output is USB stream, then it's likely a
		 * capture stream.  otherwise it should be playback (hopefully :)
		 */
		if (! mapped_name && ! (state->oterm.type >> 16)) {
			if ((state->oterm.type & 0xff00) == 0x0100) {
				len = append_ctl_name(kctl, " Capture");
			} else {
				len = append_ctl_name(kctl, " Playback");
			}
		}
		append_ctl_name(kctl, control == UAC_FU_MUTE ?
				" Switch" : " Volume");
		if (control == UAC_FU_VOLUME) {
			kctl->tlv.c = mixer_vol_tlv;
			kctl->vd[0].access |= 
				SNDRV_CTL_ELEM_ACCESS_TLV_READ |
				SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK;
			check_mapped_dB(map, cval);
		}
		break;

	default:
		if (! len)
			strlcpy(kctl->id.name, audio_feature_info[control-1].name,
				sizeof(kctl->id.name));
		break;
	}

	/* volume control quirks */
	switch (state->chip->usb_id) {
	case USB_ID(0x0471, 0x0101):
	case USB_ID(0x0471, 0x0104):
	case USB_ID(0x0471, 0x0105):
	case USB_ID(0x0672, 0x1041):
	/* quirk for UDA1321/N101.
	 * note that detection between firmware 2.1.1.7 (N101)
	 * and later 2.1.1.21 is not very clear from datasheets.
	 * I hope that the min value is -15360 for newer firmware --jk
	 */
		if (!strcmp(kctl->id.name, "PCM Playback Volume") &&
		    cval->min == -15616) {
			snd_printk(KERN_INFO
				 "set volume quirk for UDA1321/N101 chip\n");
			cval->max = -256;
		}
		break;

	case USB_ID(0x046d, 0x09a4):
		if (!strcmp(kctl->id.name, "Mic Capture Volume")) {
			snd_printk(KERN_INFO
				"set volume quirk for QuickCam E3500\n");
			cval->min = 6080;
			cval->max = 8768;
			cval->res = 192;
		}
		break;

	case USB_ID(0x046d, 0x0809):
	case USB_ID(0x046d, 0x0991):
	/* Most audio usb devices lie about volume resolution.
	 * Most Logitech webcams have res = 384.
	 * Proboly there is some logitech magic behind this number --fishor
	 */
		if (!strcmp(kctl->id.name, "Mic Capture Volume")) {
			snd_printk(KERN_INFO
				"set resolution quirk: cval->res = 384\n");
			cval->res = 384;
		}
		break;

	}

	snd_printdd(KERN_INFO "[%d] FU [%s] ch = %d, val = %d/%d/%d\n",
		    cval->id, kctl->id.name, cval->channels, cval->min, cval->max, cval->res);
	add_control_to_empty(state, kctl);
}



/*
 * parse a feature unit
 *
 * most of controlls are defined here.
 */
static int parse_audio_feature_unit(struct mixer_build *state, int unitid, void *_ftr)
{
	int channels, i, j;
	struct usb_audio_term iterm;
	unsigned int master_bits, first_ch_bits;
	int err, csize;
	struct uac_feature_unit_descriptor *hdr = _ftr;
	__u8 *bmaControls;

	if (state->mixer->protocol == UAC_VERSION_1) {
		csize = hdr->bControlSize;
		channels = (hdr->bLength - 7) / csize - 1;
		bmaControls = hdr->bmaControls;
	} else {
		struct uac2_feature_unit_descriptor *ftr = _ftr;
		csize = 4;
		channels = (hdr->bLength - 6) / 4 - 1;
		bmaControls = ftr->bmaControls;
	}

	if (hdr->bLength < 7 || !csize || hdr->bLength < 7 + csize) {
		snd_printk(KERN_ERR "usbaudio: unit %u: invalid UAC_FEATURE_UNIT descriptor\n", unitid);
		return -EINVAL;
	}

	/* parse the source unit */
	if ((err = parse_audio_unit(state, hdr->bSourceID)) < 0)
		return err;

	/* determine the input source type and name */
	if (check_input_term(state, hdr->bSourceID, &iterm) < 0)
		return -EINVAL;

	master_bits = snd_usb_combine_bytes(bmaControls, csize);
	/* master configuration quirks */
	switch (state->chip->usb_id) {
	case USB_ID(0x08bb, 0x2702):
		snd_printk(KERN_INFO
			   "usbmixer: master volume quirk for PCM2702 chip\n");
		/* disable non-functional volume control */
		master_bits &= ~UAC_CONTROL_BIT(UAC_FU_VOLUME);
		break;
	}
	if (channels > 0)
		first_ch_bits = snd_usb_combine_bytes(bmaControls + csize, csize);
	else
		first_ch_bits = 0;

	if (state->mixer->protocol == UAC_VERSION_1) {
		/* check all control types */
		for (i = 0; i < 10; i++) {
			unsigned int ch_bits = 0;
			for (j = 0; j < channels; j++) {
				unsigned int mask = snd_usb_combine_bytes(bmaControls + csize * (j+1), csize);
				if (mask & (1 << i))
					ch_bits |= (1 << j);
			}
			/* audio class v1 controls are never read-only */
			if (ch_bits & 1) /* the first channel must be set (for ease of programming) */
				build_feature_ctl(state, _ftr, ch_bits, i, &iterm, unitid, 0);
			if (master_bits & (1 << i))
				build_feature_ctl(state, _ftr, 0, i, &iterm, unitid, 0);
		}
	} else { /* UAC_VERSION_2 */
		for (i = 0; i < 30/2; i++) {
			unsigned int ch_bits = 0;
			unsigned int ch_read_only = 0;

			for (j = 0; j < channels; j++) {
				unsigned int mask = snd_usb_combine_bytes(bmaControls + csize * (j+1), csize);
				if (uac2_control_is_readable(mask, i)) {
					ch_bits |= (1 << j);
					if (!uac2_control_is_writeable(mask, i))
						ch_read_only |= (1 << j);
				}
			}

			/* NOTE: build_feature_ctl() will mark the control read-only if all channels
			 * are marked read-only in the descriptors. Otherwise, the control will be
			 * reported as writeable, but the driver will not actually issue a write
			 * command for read-only channels */
			if (ch_bits & 1) /* the first channel must be set (for ease of programming) */
				build_feature_ctl(state, _ftr, ch_bits, i, &iterm, unitid, ch_read_only);
			if (uac2_control_is_readable(master_bits, i))
				build_feature_ctl(state, _ftr, 0, i, &iterm, unitid,
						  !uac2_control_is_writeable(master_bits, i));
		}
	}

	return 0;
}


/*
 * Mixer Unit
 */

/*
 * build a mixer unit control
 *
 * the callbacks are identical with feature unit.
 * input channel number (zero based) is given in control field instead.
 */

static void build_mixer_unit_ctl(struct mixer_build *state,
				 struct uac_mixer_unit_descriptor *desc,
				 int in_pin, int in_ch, int unitid,
				 struct usb_audio_term *iterm)
{
	struct usb_mixer_elem_info *cval;
	unsigned int num_outs = uac_mixer_unit_bNrChannels(desc);
	unsigned int i, len;
	struct snd_kcontrol *kctl;
	const struct usbmix_name_map *map;

	map = find_map(state, unitid, 0);
	if (check_ignored_ctl(map))
		return;

	cval = kzalloc(sizeof(*cval), GFP_KERNEL);
	if (! cval)
		return;

	cval->mixer = state->mixer;
	cval->id = unitid;
	cval->control = in_ch + 1; /* based on 1 */
	cval->val_type = USB_MIXER_S16;
	for (i = 0; i < num_outs; i++) {
		if (check_matrix_bitmap(uac_mixer_unit_bmControls(desc, state->mixer->protocol), in_ch, i, num_outs)) {
			cval->cmask |= (1 << i);
			cval->channels++;
		}
	}

	/* get min/max values */
	get_min_max(cval, 0);

	kctl = snd_ctl_new1(&usb_feature_unit_ctl, cval);
	if (! kctl) {
		snd_printk(KERN_ERR "cannot malloc kcontrol\n");
		kfree(cval);
		return;
	}
	kctl->private_free = usb_mixer_elem_free;

	len = check_mapped_name(map, kctl->id.name, sizeof(kctl->id.name));
	if (! len)
		len = get_term_name(state, iterm, kctl->id.name, sizeof(kctl->id.name), 0);
	if (! len)
		len = sprintf(kctl->id.name, "Mixer Source %d", in_ch + 1);
	append_ctl_name(kctl, " Volume");

	snd_printdd(KERN_INFO "[%d] MU [%s] ch = %d, val = %d/%d\n",
		    cval->id, kctl->id.name, cval->channels, cval->min, cval->max);
	add_control_to_empty(state, kctl);
}


/*
 * parse a mixer unit
 */
static int parse_audio_mixer_unit(struct mixer_build *state, int unitid, void *raw_desc)
{
	struct uac_mixer_unit_descriptor *desc = raw_desc;
	struct usb_audio_term iterm;
	int input_pins, num_ins, num_outs;
	int pin, ich, err;

	if (desc->bLength < 11 || ! (input_pins = desc->bNrInPins) || ! (num_outs = uac_mixer_unit_bNrChannels(desc))) {
		snd_printk(KERN_ERR "invalid MIXER UNIT descriptor %d\n", unitid);
		return -EINVAL;
	}
	/* no bmControls field (e.g. Maya44) -> ignore */
	if (desc->bLength <= 10 + input_pins) {
		snd_printdd(KERN_INFO "MU %d has no bmControls field\n", unitid);
		return 0;
	}

	num_ins = 0;
	ich = 0;
	for (pin = 0; pin < input_pins; pin++) {
		err = parse_audio_unit(state, desc->baSourceID[pin]);
		if (err < 0)
			return err;
		err = check_input_term(state, desc->baSourceID[pin], &iterm);
		if (err < 0)
			return err;
		num_ins += iterm.channels;
		for (; ich < num_ins; ++ich) {
			int och, ich_has_controls = 0;

			for (och = 0; och < num_outs; ++och) {
				if (check_matrix_bitmap(uac_mixer_unit_bmControls(desc, state->mixer->protocol),
							ich, och, num_outs)) {
					ich_has_controls = 1;
					break;
				}
			}
			if (ich_has_controls)
				build_mixer_unit_ctl(state, desc, pin, ich,
						     unitid, &iterm);
		}
	}
	return 0;
}


/*
 * Processing Unit / Extension Unit
 */

/* get callback for processing/extension unit */
static int mixer_ctl_procunit_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *cval = kcontrol->private_data;
	int err, val;

	err = get_cur_ctl_value(cval, cval->control << 8, &val);
	if (err < 0 && cval->mixer->ignore_ctl_error) {
		ucontrol->value.integer.value[0] = cval->min;
		return 0;
	}
	if (err < 0)
		return err;
	val = get_relative_value(cval, val);
	ucontrol->value.integer.value[0] = val;
	return 0;
}

/* put callback for processing/extension unit */
static int mixer_ctl_procunit_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *cval = kcontrol->private_data;
	int val, oval, err;

	err = get_cur_ctl_value(cval, cval->control << 8, &oval);
	if (err < 0) {
		if (cval->mixer->ignore_ctl_error)
			return 0;
		return err;
	}
	val = ucontrol->value.integer.value[0];
	val = get_abs_value(cval, val);
	if (val != oval) {
		set_cur_ctl_value(cval, cval->control << 8, val);
		return 1;
	}
	return 0;
}

/* alsa control interface for processing/extension unit */
static struct snd_kcontrol_new mixer_procunit_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "", /* will be filled later */
	.info = mixer_ctl_feature_info,
	.get = mixer_ctl_procunit_get,
	.put = mixer_ctl_procunit_put,
};


/*
 * predefined data for processing units
 */
struct procunit_value_info {
	int control;
	char *suffix;
	int val_type;
	int min_value;
};

struct procunit_info {
	int type;
	char *name;
	struct procunit_value_info *values;
};

static struct procunit_value_info updown_proc_info[] = {
	{ UAC_UD_ENABLE, "Switch", USB_MIXER_BOOLEAN },
	{ UAC_UD_MODE_SELECT, "Mode Select", USB_MIXER_U8, 1 },
	{ 0 }
};
static struct procunit_value_info prologic_proc_info[] = {
	{ UAC_DP_ENABLE, "Switch", USB_MIXER_BOOLEAN },
	{ UAC_DP_MODE_SELECT, "Mode Select", USB_MIXER_U8, 1 },
	{ 0 }
};
static struct procunit_value_info threed_enh_proc_info[] = {
	{ UAC_3D_ENABLE, "Switch", USB_MIXER_BOOLEAN },
	{ UAC_3D_SPACE, "Spaciousness", USB_MIXER_U8 },
	{ 0 }
};
static struct procunit_value_info reverb_proc_info[] = {
	{ UAC_REVERB_ENABLE, "Switch", USB_MIXER_BOOLEAN },
	{ UAC_REVERB_LEVEL, "Level", USB_MIXER_U8 },
	{ UAC_REVERB_TIME, "Time", USB_MIXER_U16 },
	{ UAC_REVERB_FEEDBACK, "Feedback", USB_MIXER_U8 },
	{ 0 }
};
static struct procunit_value_info chorus_proc_info[] = {
	{ UAC_CHORUS_ENABLE, "Switch", USB_MIXER_BOOLEAN },
	{ UAC_CHORUS_LEVEL, "Level", USB_MIXER_U8 },
	{ UAC_CHORUS_RATE, "Rate", USB_MIXER_U16 },
	{ UAC_CHORUS_DEPTH, "Depth", USB_MIXER_U16 },
	{ 0 }
};
static struct procunit_value_info dcr_proc_info[] = {
	{ UAC_DCR_ENABLE, "Switch", USB_MIXER_BOOLEAN },
	{ UAC_DCR_RATE, "Ratio", USB_MIXER_U16 },
	{ UAC_DCR_MAXAMPL, "Max Amp", USB_MIXER_S16 },
	{ UAC_DCR_THRESHOLD, "Threshold", USB_MIXER_S16 },
	{ UAC_DCR_ATTACK_TIME, "Attack Time", USB_MIXER_U16 },
	{ UAC_DCR_RELEASE_TIME, "Release Time", USB_MIXER_U16 },
	{ 0 }
};

static struct procunit_info procunits[] = {
	{ UAC_PROCESS_UP_DOWNMIX, "Up Down", updown_proc_info },
	{ UAC_PROCESS_DOLBY_PROLOGIC, "Dolby Prologic", prologic_proc_info },
	{ UAC_PROCESS_STEREO_EXTENDER, "3D Stereo Extender", threed_enh_proc_info },
	{ UAC_PROCESS_REVERB, "Reverb", reverb_proc_info },
	{ UAC_PROCESS_CHORUS, "Chorus", chorus_proc_info },
	{ UAC_PROCESS_DYN_RANGE_COMP, "DCR", dcr_proc_info },
	{ 0 },
};
/*
 * predefined data for extension units
 */
static struct procunit_value_info clock_rate_xu_info[] = {
	{ USB_XU_CLOCK_RATE_SELECTOR, "Selector", USB_MIXER_U8, 0 },
	{ 0 }
};
static struct procunit_value_info clock_source_xu_info[] = {
	{ USB_XU_CLOCK_SOURCE_SELECTOR, "External", USB_MIXER_BOOLEAN },
	{ 0 }
};
static struct procunit_value_info spdif_format_xu_info[] = {
	{ USB_XU_DIGITAL_FORMAT_SELECTOR, "SPDIF/AC3", USB_MIXER_BOOLEAN },
	{ 0 }
};
static struct procunit_value_info soft_limit_xu_info[] = {
	{ USB_XU_SOFT_LIMIT_SELECTOR, " ", USB_MIXER_BOOLEAN },
	{ 0 }
};
static struct procunit_info extunits[] = {
	{ USB_XU_CLOCK_RATE, "Clock rate", clock_rate_xu_info },
	{ USB_XU_CLOCK_SOURCE, "DigitalIn CLK source", clock_source_xu_info },
	{ USB_XU_DIGITAL_IO_STATUS, "DigitalOut format:", spdif_format_xu_info },
	{ USB_XU_DEVICE_OPTIONS, "AnalogueIn Soft Limit", soft_limit_xu_info },
	{ 0 }
};
/*
 * build a processing/extension unit
 */
static int build_audio_procunit(struct mixer_build *state, int unitid, void *raw_desc, struct procunit_info *list, char *name)
{
	struct uac_processing_unit_descriptor *desc = raw_desc;
	int num_ins = desc->bNrInPins;
	struct usb_mixer_elem_info *cval;
	struct snd_kcontrol *kctl;
	int i, err, nameid, type, len;
	struct procunit_info *info;
	struct procunit_value_info *valinfo;
	const struct usbmix_name_map *map;
	static struct procunit_value_info default_value_info[] = {
		{ 0x01, "Switch", USB_MIXER_BOOLEAN },
		{ 0 }
	};
	static struct procunit_info default_info = {
		0, NULL, default_value_info
	};

	if (desc->bLength < 13 || desc->bLength < 13 + num_ins ||
	    desc->bLength < num_ins + uac_processing_unit_bControlSize(desc, state->mixer->protocol)) {
		snd_printk(KERN_ERR "invalid %s descriptor (id %d)\n", name, unitid);
		return -EINVAL;
	}

	for (i = 0; i < num_ins; i++) {
		if ((err = parse_audio_unit(state, desc->baSourceID[i])) < 0)
			return err;
	}

	type = le16_to_cpu(desc->wProcessType);
	for (info = list; info && info->type; info++)
		if (info->type == type)
			break;
	if (! info || ! info->type)
		info = &default_info;

	for (valinfo = info->values; valinfo->control; valinfo++) {
		__u8 *controls = uac_processing_unit_bmControls(desc, state->mixer->protocol);

		if (! (controls[valinfo->control / 8] & (1 << ((valinfo->control % 8) - 1))))
			continue;
		map = find_map(state, unitid, valinfo->control);
		if (check_ignored_ctl(map))
			continue;
		cval = kzalloc(sizeof(*cval), GFP_KERNEL);
		if (! cval) {
			snd_printk(KERN_ERR "cannot malloc kcontrol\n");
			return -ENOMEM;
		}
		cval->mixer = state->mixer;
		cval->id = unitid;
		cval->control = valinfo->control;
		cval->val_type = valinfo->val_type;
		cval->channels = 1;

		/* get min/max values */
		if (type == UAC_PROCESS_UP_DOWNMIX && cval->control == UAC_UD_MODE_SELECT) {
			__u8 *control_spec = uac_processing_unit_specific(desc, state->mixer->protocol);
			cval->min = 1;
			cval->max = control_spec[0];
			cval->res = 1;
			cval->initialized = 1;
		} else {
			if (type == USB_XU_CLOCK_RATE) {
				/* E-Mu USB 0404/0202/TrackerPre
				 * samplerate control quirk
				 */
				cval->min = 0;
				cval->max = 5;
				cval->res = 1;
				cval->initialized = 1;
			} else
				get_min_max(cval, valinfo->min_value);
		}

		kctl = snd_ctl_new1(&mixer_procunit_ctl, cval);
		if (! kctl) {
			snd_printk(KERN_ERR "cannot malloc kcontrol\n");
			kfree(cval);
			return -ENOMEM;
		}
		kctl->private_free = usb_mixer_elem_free;

		if (check_mapped_name(map, kctl->id.name,
						sizeof(kctl->id.name)))
			/* nothing */ ;
		else if (info->name)
			strlcpy(kctl->id.name, info->name, sizeof(kctl->id.name));
		else {
			nameid = uac_processing_unit_iProcessing(desc, state->mixer->protocol);
			len = 0;
			if (nameid)
				len = snd_usb_copy_string_desc(state, nameid, kctl->id.name, sizeof(kctl->id.name));
			if (! len)
				strlcpy(kctl->id.name, name, sizeof(kctl->id.name));
		}
		append_ctl_name(kctl, " ");
		append_ctl_name(kctl, valinfo->suffix);

		snd_printdd(KERN_INFO "[%d] PU [%s] ch = %d, val = %d/%d\n",
			    cval->id, kctl->id.name, cval->channels, cval->min, cval->max);
		if ((err = add_control_to_empty(state, kctl)) < 0)
			return err;
	}
	return 0;
}


static int parse_audio_processing_unit(struct mixer_build *state, int unitid, void *raw_desc)
{
	return build_audio_procunit(state, unitid, raw_desc, procunits, "Processing Unit");
}

static int parse_audio_extension_unit(struct mixer_build *state, int unitid, void *raw_desc)
{
	/* Note that we parse extension units with processing unit descriptors.
	 * That's ok as the layout is the same */
	return build_audio_procunit(state, unitid, raw_desc, extunits, "Extension Unit");
}


/*
 * Selector Unit
 */

/* info callback for selector unit
 * use an enumerator type for routing
 */
static int mixer_ctl_selector_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	struct usb_mixer_elem_info *cval = kcontrol->private_data;
	char **itemlist = (char **)kcontrol->private_value;

	if (snd_BUG_ON(!itemlist))
		return -EINVAL;
	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = cval->max;
	if ((int)uinfo->value.enumerated.item >= cval->max)
		uinfo->value.enumerated.item = cval->max - 1;
	strcpy(uinfo->value.enumerated.name, itemlist[uinfo->value.enumerated.item]);
	return 0;
}

/* get callback for selector unit */
static int mixer_ctl_selector_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *cval = kcontrol->private_data;
	int val, err;

	err = get_cur_ctl_value(cval, cval->control << 8, &val);
	if (err < 0) {
		if (cval->mixer->ignore_ctl_error) {
			ucontrol->value.enumerated.item[0] = 0;
			return 0;
		}
		return err;
	}
	val = get_relative_value(cval, val);
	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

/* put callback for selector unit */
static int mixer_ctl_selector_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct usb_mixer_elem_info *cval = kcontrol->private_data;
	int val, oval, err;

	err = get_cur_ctl_value(cval, cval->control << 8, &oval);
	if (err < 0) {
		if (cval->mixer->ignore_ctl_error)
			return 0;
		return err;
	}
	val = ucontrol->value.enumerated.item[0];
	val = get_abs_value(cval, val);
	if (val != oval) {
		set_cur_ctl_value(cval, cval->control << 8, val);
		return 1;
	}
	return 0;
}

/* alsa control interface for selector unit */
static struct snd_kcontrol_new mixer_selectunit_ctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "", /* will be filled later */
	.info = mixer_ctl_selector_info,
	.get = mixer_ctl_selector_get,
	.put = mixer_ctl_selector_put,
};


/* private free callback.
 * free both private_data and private_value
 */
static void usb_mixer_selector_elem_free(struct snd_kcontrol *kctl)
{
	int i, num_ins = 0;

	if (kctl->private_data) {
		struct usb_mixer_elem_info *cval = kctl->private_data;
		num_ins = cval->max;
		kfree(cval);
		kctl->private_data = NULL;
	}
	if (kctl->private_value) {
		char **itemlist = (char **)kctl->private_value;
		for (i = 0; i < num_ins; i++)
			kfree(itemlist[i]);
		kfree(itemlist);
		kctl->private_value = 0;
	}
}

/*
 * parse a selector unit
 */
static int parse_audio_selector_unit(struct mixer_build *state, int unitid, void *raw_desc)
{
	struct uac_selector_unit_descriptor *desc = raw_desc;
	unsigned int i, nameid, len;
	int err;
	struct usb_mixer_elem_info *cval;
	struct snd_kcontrol *kctl;
	const struct usbmix_name_map *map;
	char **namelist;

	if (!desc->bNrInPins || desc->bLength < 5 + desc->bNrInPins) {
		snd_printk(KERN_ERR "invalid SELECTOR UNIT descriptor %d\n", unitid);
		return -EINVAL;
	}

	for (i = 0; i < desc->bNrInPins; i++) {
		if ((err = parse_audio_unit(state, desc->baSourceID[i])) < 0)
			return err;
	}

	if (desc->bNrInPins == 1) /* only one ? nonsense! */
		return 0;

	map = find_map(state, unitid, 0);
	if (check_ignored_ctl(map))
		return 0;

	cval = kzalloc(sizeof(*cval), GFP_KERNEL);
	if (! cval) {
		snd_printk(KERN_ERR "cannot malloc kcontrol\n");
		return -ENOMEM;
	}
	cval->mixer = state->mixer;
	cval->id = unitid;
	cval->val_type = USB_MIXER_U8;
	cval->channels = 1;
	cval->min = 1;
	cval->max = desc->bNrInPins;
	cval->res = 1;
	cval->initialized = 1;

	if (desc->bDescriptorSubtype == UAC2_CLOCK_SELECTOR)
		cval->control = UAC2_CX_CLOCK_SELECTOR;
	else
		cval->control = 0;

	namelist = kmalloc(sizeof(char *) * desc->bNrInPins, GFP_KERNEL);
	if (! namelist) {
		snd_printk(KERN_ERR "cannot malloc\n");
		kfree(cval);
		return -ENOMEM;
	}
#define MAX_ITEM_NAME_LEN	64
	for (i = 0; i < desc->bNrInPins; i++) {
		struct usb_audio_term iterm;
		len = 0;
		namelist[i] = kmalloc(MAX_ITEM_NAME_LEN, GFP_KERNEL);
		if (! namelist[i]) {
			snd_printk(KERN_ERR "cannot malloc\n");
			while (i--)
				kfree(namelist[i]);
			kfree(namelist);
			kfree(cval);
			return -ENOMEM;
		}
		len = check_mapped_selector_name(state, unitid, i, namelist[i],
						 MAX_ITEM_NAME_LEN);
		if (! len && check_input_term(state, desc->baSourceID[i], &iterm) >= 0)
			len = get_term_name(state, &iterm, namelist[i], MAX_ITEM_NAME_LEN, 0);
		if (! len)
			sprintf(namelist[i], "Input %d", i);
	}

	kctl = snd_ctl_new1(&mixer_selectunit_ctl, cval);
	if (! kctl) {
		snd_printk(KERN_ERR "cannot malloc kcontrol\n");
		kfree(namelist);
		kfree(cval);
		return -ENOMEM;
	}
	kctl->private_value = (unsigned long)namelist;
	kctl->private_free = usb_mixer_selector_elem_free;

	nameid = uac_selector_unit_iSelector(desc);
	len = check_mapped_name(map, kctl->id.name, sizeof(kctl->id.name));
	if (len)
		;
	else if (nameid)
		snd_usb_copy_string_desc(state, nameid, kctl->id.name, sizeof(kctl->id.name));
	else {
		len = get_term_name(state, &state->oterm,
				    kctl->id.name, sizeof(kctl->id.name), 0);
		if (! len)
			strlcpy(kctl->id.name, "USB", sizeof(kctl->id.name));

		if (desc->bDescriptorSubtype == UAC2_CLOCK_SELECTOR)
			append_ctl_name(kctl, " Clock Source");
		else if ((state->oterm.type & 0xff00) == 0x0100)
			append_ctl_name(kctl, " Capture Source");
		else
			append_ctl_name(kctl, " Playback Source");
	}

	snd_printdd(KERN_INFO "[%d] SU [%s] items = %d\n",
		    cval->id, kctl->id.name, desc->bNrInPins);
	if ((err = add_control_to_empty(state, kctl)) < 0)
		return err;

	return 0;
}


/*
 * parse an audio unit recursively
 */

static int parse_audio_unit(struct mixer_build *state, int unitid)
{
	unsigned char *p1;

	if (test_and_set_bit(unitid, state->unitbitmap))
		return 0; /* the unit already visited */

	p1 = find_audio_control_unit(state, unitid);
	if (!p1) {
		snd_printk(KERN_ERR "usbaudio: unit %d not found!\n", unitid);
		return -EINVAL;
	}

	switch (p1[2]) {
	case UAC_INPUT_TERMINAL:
	case UAC2_CLOCK_SOURCE:
		return 0; /* NOP */
	case UAC_MIXER_UNIT:
		return parse_audio_mixer_unit(state, unitid, p1);
	case UAC_SELECTOR_UNIT:
	case UAC2_CLOCK_SELECTOR:
		return parse_audio_selector_unit(state, unitid, p1);
	case UAC_FEATURE_UNIT:
		return parse_audio_feature_unit(state, unitid, p1);
	case UAC1_PROCESSING_UNIT:
	/*   UAC2_EFFECT_UNIT has the same value */
		if (state->mixer->protocol == UAC_VERSION_1)
			return parse_audio_processing_unit(state, unitid, p1);
		else
			return 0;
	case UAC1_EXTENSION_UNIT:
	/*   UAC2_PROCESSING_UNIT_V2 has the same value */
		if (state->mixer->protocol == UAC_VERSION_1)
			return parse_audio_extension_unit(state, unitid, p1);
		else /* UAC_VERSION_2 */
			return parse_audio_processing_unit(state, unitid, p1);
	default:
		snd_printk(KERN_ERR "usbaudio: unit %u: unexpected type 0x%02x\n", unitid, p1[2]);
		return -EINVAL;
	}
}

static void snd_usb_mixer_free(struct usb_mixer_interface *mixer)
{
	kfree(mixer->id_elems);
	if (mixer->urb) {
		kfree(mixer->urb->transfer_buffer);
		usb_free_urb(mixer->urb);
	}
	usb_free_urb(mixer->rc_urb);
	kfree(mixer->rc_setup_packet);
	kfree(mixer);
}

static int snd_usb_mixer_dev_free(struct snd_device *device)
{
	struct usb_mixer_interface *mixer = device->device_data;
	snd_usb_mixer_free(mixer);
	return 0;
}

/*
 * create mixer controls
 *
 * walk through all UAC_OUTPUT_TERMINAL descriptors to search for mixers
 */
static int snd_usb_mixer_controls(struct usb_mixer_interface *mixer)
{
	struct mixer_build state;
	int err;
	const struct usbmix_ctl_map *map;
	struct usb_host_interface *hostif;
	void *p;

	hostif = mixer->chip->ctrl_intf;
	memset(&state, 0, sizeof(state));
	state.chip = mixer->chip;
	state.mixer = mixer;
	state.buffer = hostif->extra;
	state.buflen = hostif->extralen;

	/* check the mapping table */
	for (map = usbmix_ctl_maps; map->id; map++) {
		if (map->id == state.chip->usb_id) {
			state.map = map->map;
			state.selector_map = map->selector_map;
			mixer->ignore_ctl_error = map->ignore_ctl_error;
			break;
		}
	}

	p = NULL;
	while ((p = snd_usb_find_csint_desc(hostif->extra, hostif->extralen, p, UAC_OUTPUT_TERMINAL)) != NULL) {
		if (mixer->protocol == UAC_VERSION_1) {
			struct uac1_output_terminal_descriptor *desc = p;

			if (desc->bLength < sizeof(*desc))
				continue; /* invalid descriptor? */
			set_bit(desc->bTerminalID, state.unitbitmap);  /* mark terminal ID as visited */
			state.oterm.id = desc->bTerminalID;
			state.oterm.type = le16_to_cpu(desc->wTerminalType);
			state.oterm.name = desc->iTerminal;
			err = parse_audio_unit(&state, desc->bSourceID);
			if (err < 0)
				return err;
		} else { /* UAC_VERSION_2 */
			struct uac2_output_terminal_descriptor *desc = p;

			if (desc->bLength < sizeof(*desc))
				continue; /* invalid descriptor? */
			set_bit(desc->bTerminalID, state.unitbitmap);  /* mark terminal ID as visited */
			state.oterm.id = desc->bTerminalID;
			state.oterm.type = le16_to_cpu(desc->wTerminalType);
			state.oterm.name = desc->iTerminal;
			err = parse_audio_unit(&state, desc->bSourceID);
			if (err < 0)
				return err;

			/* for UAC2, use the same approach to also add the clock selectors */
			err = parse_audio_unit(&state, desc->bCSourceID);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

void snd_usb_mixer_notify_id(struct usb_mixer_interface *mixer, int unitid)
{
	struct usb_mixer_elem_info *info;

	for (info = mixer->id_elems[unitid]; info; info = info->next_id_elem)
		snd_ctl_notify(mixer->chip->card, SNDRV_CTL_EVENT_MASK_VALUE,
			       info->elem_id);
}

static void snd_usb_mixer_dump_cval(struct snd_info_buffer *buffer,
				    int unitid,
				    struct usb_mixer_elem_info *cval)
{
	static char *val_types[] = {"BOOLEAN", "INV_BOOLEAN",
				    "S8", "U8", "S16", "U16"};
	snd_iprintf(buffer, "  Unit: %i\n", unitid);
	if (cval->elem_id)
		snd_iprintf(buffer, "    Control: name=\"%s\", index=%i\n",
				cval->elem_id->name, cval->elem_id->index);
	snd_iprintf(buffer, "    Info: id=%i, control=%i, cmask=0x%x, "
			    "channels=%i, type=\"%s\"\n", cval->id,
			    cval->control, cval->cmask, cval->channels,
			    val_types[cval->val_type]);
	snd_iprintf(buffer, "    Volume: min=%i, max=%i, dBmin=%i, dBmax=%i\n",
			    cval->min, cval->max, cval->dBmin, cval->dBmax);
}

static void snd_usb_mixer_proc_read(struct snd_info_entry *entry,
				    struct snd_info_buffer *buffer)
{
	struct snd_usb_audio *chip = entry->private_data;
	struct usb_mixer_interface *mixer;
	struct usb_mixer_elem_info *cval;
	int unitid;

	list_for_each_entry(mixer, &chip->mixer_list, list) {
		snd_iprintf(buffer,
			"USB Mixer: usb_id=0x%08x, ctrlif=%i, ctlerr=%i\n",
				chip->usb_id, snd_usb_ctrl_intf(chip),
				mixer->ignore_ctl_error);
		snd_iprintf(buffer, "Card: %s\n", chip->card->longname);
		for (unitid = 0; unitid < MAX_ID_ELEMS; unitid++) {
			for (cval = mixer->id_elems[unitid]; cval;
						cval = cval->next_id_elem)
				snd_usb_mixer_dump_cval(buffer, unitid, cval);
		}
	}
}

static void snd_usb_mixer_interrupt_v2(struct usb_mixer_interface *mixer,
				       int attribute, int value, int index)
{
	struct usb_mixer_elem_info *info;
	__u8 unitid = (index >> 8) & 0xff;
	__u8 control = (value >> 8) & 0xff;
	__u8 channel = value & 0xff;

	if (channel >= MAX_CHANNELS) {
		snd_printk(KERN_DEBUG "%s(): bogus channel number %d\n",
				__func__, channel);
		return;
	}

	for (info = mixer->id_elems[unitid]; info; info = info->next_id_elem) {
		if (info->control != control)
			continue;

		switch (attribute) {
		case UAC2_CS_CUR:
			/* invalidate cache, so the value is read from the device */
			if (channel)
				info->cached &= ~(1 << channel);
			else /* master channel */
				info->cached = 0;

			snd_ctl_notify(mixer->chip->card, SNDRV_CTL_EVENT_MASK_VALUE,
					info->elem_id);
			break;

		case UAC2_CS_RANGE:
			/* TODO */
			break;

		case UAC2_CS_MEM:
			/* TODO */
			break;

		default:
			snd_printk(KERN_DEBUG "unknown attribute %d in interrupt\n",
						attribute);
			break;
		} /* switch */
	}
}

static void snd_usb_mixer_interrupt(struct urb *urb)
{
	struct usb_mixer_interface *mixer = urb->context;
	int len = urb->actual_length;

	if (urb->status != 0)
		goto requeue;

	if (mixer->protocol == UAC_VERSION_1) {
		struct uac1_status_word *status;

		for (status = urb->transfer_buffer;
		     len >= sizeof(*status);
		     len -= sizeof(*status), status++) {
			snd_printd(KERN_DEBUG "status interrupt: %02x %02x\n",
						status->bStatusType,
						status->bOriginator);

			/* ignore any notifications not from the control interface */
			if ((status->bStatusType & UAC1_STATUS_TYPE_ORIG_MASK) !=
				UAC1_STATUS_TYPE_ORIG_AUDIO_CONTROL_IF)
				continue;

			if (status->bStatusType & UAC1_STATUS_TYPE_MEM_CHANGED)
				snd_usb_mixer_rc_memory_change(mixer, status->bOriginator);
			else
				snd_usb_mixer_notify_id(mixer, status->bOriginator);
		}
	} else { /* UAC_VERSION_2 */
		struct uac2_interrupt_data_msg *msg;

		for (msg = urb->transfer_buffer;
		     len >= sizeof(*msg);
		     len -= sizeof(*msg), msg++) {
			/* drop vendor specific and endpoint requests */
			if ((msg->bInfo & UAC2_INTERRUPT_DATA_MSG_VENDOR) ||
			    (msg->bInfo & UAC2_INTERRUPT_DATA_MSG_EP))
				continue;

			snd_usb_mixer_interrupt_v2(mixer, msg->bAttribute,
						   le16_to_cpu(msg->wValue),
						   le16_to_cpu(msg->wIndex));
		}
	}

requeue:
	if (urb->status != -ENOENT && urb->status != -ECONNRESET) {
		urb->dev = mixer->chip->dev;
		usb_submit_urb(urb, GFP_ATOMIC);
	}
}

/* create the handler for the optional status interrupt endpoint */
static int snd_usb_mixer_status_create(struct usb_mixer_interface *mixer)
{
	struct usb_host_interface *hostif;
	struct usb_endpoint_descriptor *ep;
	void *transfer_buffer;
	int buffer_length;
	unsigned int epnum;

	hostif = mixer->chip->ctrl_intf;
	/* we need one interrupt input endpoint */
	if (get_iface_desc(hostif)->bNumEndpoints < 1)
		return 0;
	ep = get_endpoint(hostif, 0);
	if (!usb_endpoint_dir_in(ep) || !usb_endpoint_xfer_int(ep))
		return 0;

	epnum = usb_endpoint_num(ep);
	buffer_length = le16_to_cpu(ep->wMaxPacketSize);
	transfer_buffer = kmalloc(buffer_length, GFP_KERNEL);
	if (!transfer_buffer)
		return -ENOMEM;
	mixer->urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!mixer->urb) {
		kfree(transfer_buffer);
		return -ENOMEM;
	}
	usb_fill_int_urb(mixer->urb, mixer->chip->dev,
			 usb_rcvintpipe(mixer->chip->dev, epnum),
			 transfer_buffer, buffer_length,
			 snd_usb_mixer_interrupt, mixer, ep->bInterval);
	usb_submit_urb(mixer->urb, GFP_KERNEL);
	return 0;
}

int snd_usb_create_mixer(struct snd_usb_audio *chip, int ctrlif,
			 int ignore_error)
{
	static struct snd_device_ops dev_ops = {
		.dev_free = snd_usb_mixer_dev_free
	};
	struct usb_mixer_interface *mixer;
	struct snd_info_entry *entry;
	struct usb_host_interface *host_iface;
	int err;

	strcpy(chip->card->mixername, "USB Mixer");

	mixer = kzalloc(sizeof(*mixer), GFP_KERNEL);
	if (!mixer)
		return -ENOMEM;
	mixer->chip = chip;
	mixer->ignore_ctl_error = ignore_error;
	mixer->id_elems = kcalloc(MAX_ID_ELEMS, sizeof(*mixer->id_elems),
				  GFP_KERNEL);
	if (!mixer->id_elems) {
		kfree(mixer);
		return -ENOMEM;
	}

	host_iface = &usb_ifnum_to_if(chip->dev, ctrlif)->altsetting[0];
	switch (get_iface_desc(host_iface)->bInterfaceProtocol) {
	case UAC_VERSION_1:
	default:
		mixer->protocol = UAC_VERSION_1;
		break;
	case UAC_VERSION_2:
		mixer->protocol = UAC_VERSION_2;
		break;
	}

	if ((err = snd_usb_mixer_controls(mixer)) < 0 ||
	    (err = snd_usb_mixer_status_create(mixer)) < 0)
		goto _error;

	snd_usb_mixer_apply_create_quirk(mixer);

	err = snd_device_new(chip->card, SNDRV_DEV_LOWLEVEL, mixer, &dev_ops);
	if (err < 0)
		goto _error;

	if (list_empty(&chip->mixer_list) &&
	    !snd_card_proc_new(chip->card, "usbmixer", &entry))
		snd_info_set_text_ops(entry, chip, snd_usb_mixer_proc_read);

	list_add(&mixer->list, &chip->mixer_list);
	return 0;

_error:
	snd_usb_mixer_free(mixer);
	return err;
}

void snd_usb_mixer_disconnect(struct list_head *p)
{
	struct usb_mixer_interface *mixer;

	mixer = list_entry(p, struct usb_mixer_interface, list);
	usb_kill_urb(mixer->urb);
	usb_kill_urb(mixer->rc_urb);
}
