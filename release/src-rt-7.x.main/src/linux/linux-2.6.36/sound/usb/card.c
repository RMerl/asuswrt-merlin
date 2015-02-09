/*
 *   (Tentative) USB Audio Driver for ALSA
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
 *
 *  NOTES:
 *
 *   - async unlink should be used for avoiding the sleep inside lock.
 *     2.4.22 usb-uhci seems buggy for async unlinking and results in
 *     oops.  in such a cse, pass async_unlink=0 option.
 *   - the linked URBs would be preferred but not used so far because of
 *     the instability of unlinking.
 *   - type II is not supported properly.  there is no device which supports
 *     this type *correctly*.  SB extigy looks as if it supports, but it's
 *     indeed an AC3 stream packed in SPDIF frames (i.e. no real AC3 stream).
 */


#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/usb.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/usb/audio.h>
#include <linux/usb/audio-v2.h>

#include <sound/core.h>
#include <sound/info.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include "usbaudio.h"
#include "card.h"
#include "midi.h"
#include "mixer.h"
#include "proc.h"
#include "quirks.h"
#include "endpoint.h"
#include "helper.h"
#include "debug.h"
#include "pcm.h"
#include "urb.h"
#include "format.h"

MODULE_AUTHOR("Takashi Iwai <tiwai@suse.de>");
MODULE_DESCRIPTION("USB Audio");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{Generic,USB Audio}}");


static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;/* Enable this card */
/* Vendor/product IDs for this card */
static int vid[SNDRV_CARDS] = { [0 ... (SNDRV_CARDS-1)] = -1 };
static int pid[SNDRV_CARDS] = { [0 ... (SNDRV_CARDS-1)] = -1 };
static int nrpacks = 8;		/* max. number of packets per urb */
static int async_unlink = 1;
static int device_setup[SNDRV_CARDS]; /* device parameter for this card */
static int ignore_ctl_error;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for the USB audio adapter.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for the USB audio adapter.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable USB audio adapter.");
module_param_array(vid, int, NULL, 0444);
MODULE_PARM_DESC(vid, "Vendor ID for the USB audio device.");
module_param_array(pid, int, NULL, 0444);
MODULE_PARM_DESC(pid, "Product ID for the USB audio device.");
module_param(nrpacks, int, 0644);
MODULE_PARM_DESC(nrpacks, "Max. number of packets per URB.");
module_param(async_unlink, bool, 0444);
MODULE_PARM_DESC(async_unlink, "Use async unlink mode.");
module_param_array(device_setup, int, NULL, 0444);
MODULE_PARM_DESC(device_setup, "Specific device setup (if needed).");
module_param(ignore_ctl_error, bool, 0444);
MODULE_PARM_DESC(ignore_ctl_error,
		 "Ignore errors from USB controller for mixer interfaces.");

/*
 * we keep the snd_usb_audio_t instances by ourselves for merging
 * the all interfaces on the same card as one sound device.
 */

static DEFINE_MUTEX(register_mutex);
static struct snd_usb_audio *usb_chip[SNDRV_CARDS];
static struct usb_driver usb_audio_driver;

/*
 * disconnect streams
 * called from snd_usb_audio_disconnect()
 */
static void snd_usb_stream_disconnect(struct list_head *head)
{
	int idx;
	struct snd_usb_stream *as;
	struct snd_usb_substream *subs;

	as = list_entry(head, struct snd_usb_stream, list);
	for (idx = 0; idx < 2; idx++) {
		subs = &as->substream[idx];
		if (!subs->num_formats)
			continue;
		snd_usb_release_substream_urbs(subs, 1);
		subs->interface = -1;
	}
}

static int snd_usb_create_stream(struct snd_usb_audio *chip, int ctrlif, int interface)
{
	struct usb_device *dev = chip->dev;
	struct usb_host_interface *alts;
	struct usb_interface_descriptor *altsd;
	struct usb_interface *iface = usb_ifnum_to_if(dev, interface);

	if (!iface) {
		snd_printk(KERN_ERR "%d:%u:%d : does not exist\n",
			   dev->devnum, ctrlif, interface);
		return -EINVAL;
	}

	if (usb_interface_claimed(iface)) {
		snd_printdd(KERN_INFO "%d:%d:%d: skipping, already claimed\n",
						dev->devnum, ctrlif, interface);
		return -EINVAL;
	}

	alts = &iface->altsetting[0];
	altsd = get_iface_desc(alts);
	if ((altsd->bInterfaceClass == USB_CLASS_AUDIO ||
	     altsd->bInterfaceClass == USB_CLASS_VENDOR_SPEC) &&
	    altsd->bInterfaceSubClass == USB_SUBCLASS_MIDISTREAMING) {
		int err = snd_usbmidi_create(chip->card, iface,
					     &chip->midi_list, NULL);
		if (err < 0) {
			snd_printk(KERN_ERR "%d:%u:%d: cannot create sequencer device\n",
						dev->devnum, ctrlif, interface);
			return -EINVAL;
		}
		usb_driver_claim_interface(&usb_audio_driver, iface, (void *)-1L);

		return 0;
	}

	if ((altsd->bInterfaceClass != USB_CLASS_AUDIO &&
	     altsd->bInterfaceClass != USB_CLASS_VENDOR_SPEC) ||
	    altsd->bInterfaceSubClass != USB_SUBCLASS_AUDIOSTREAMING) {
		snd_printdd(KERN_ERR "%d:%u:%d: skipping non-supported interface %d\n",
					dev->devnum, ctrlif, interface, altsd->bInterfaceClass);
		/* skip non-supported classes */
		return -EINVAL;
	}

	if (snd_usb_get_speed(dev) == USB_SPEED_LOW) {
		snd_printk(KERN_ERR "low speed audio streaming not supported\n");
		return -EINVAL;
	}

	if (! snd_usb_parse_audio_endpoints(chip, interface)) {
		usb_set_interface(dev, interface, 0); /* reset the current interface */
		usb_driver_claim_interface(&usb_audio_driver, iface, (void *)-1L);
		return -EINVAL;
	}

	return 0;
}

/*
 * parse audio control descriptor and create pcm/midi streams
 */
static int snd_usb_create_streams(struct snd_usb_audio *chip, int ctrlif)
{
	struct usb_device *dev = chip->dev;
	struct usb_host_interface *host_iface;
	struct usb_interface_descriptor *altsd;
	void *control_header;
	int i, protocol;

	/* find audiocontrol interface */
	host_iface = &usb_ifnum_to_if(dev, ctrlif)->altsetting[0];
	control_header = snd_usb_find_csint_desc(host_iface->extra,
						 host_iface->extralen,
						 NULL, UAC_HEADER);
	altsd = get_iface_desc(host_iface);
	protocol = altsd->bInterfaceProtocol;

	if (!control_header) {
		snd_printk(KERN_ERR "cannot find UAC_HEADER\n");
		return -EINVAL;
	}

	switch (protocol) {
	default:
		snd_printdd(KERN_WARNING "unknown interface protocol %#02x, assuming v1\n",
			    protocol);
		/* fall through */

	case UAC_VERSION_1: {
		struct uac1_ac_header_descriptor *h1 = control_header;

		if (!h1->bInCollection) {
			snd_printk(KERN_INFO "skipping empty audio interface (v1)\n");
			return -EINVAL;
		}

		if (h1->bLength < sizeof(*h1) + h1->bInCollection) {
			snd_printk(KERN_ERR "invalid UAC_HEADER (v1)\n");
			return -EINVAL;
		}

		for (i = 0; i < h1->bInCollection; i++)
			snd_usb_create_stream(chip, ctrlif, h1->baInterfaceNr[i]);

		break;
	}

	case UAC_VERSION_2: {
		struct usb_interface_assoc_descriptor *assoc =
			usb_ifnum_to_if(dev, ctrlif)->intf_assoc;

		if (!assoc) {
			snd_printk(KERN_ERR "Audio class v2 interfaces need an interface association\n");
			return -EINVAL;
		}

		for (i = 0; i < assoc->bInterfaceCount; i++) {
			int intf = assoc->bFirstInterface + i;

			if (intf != ctrlif)
				snd_usb_create_stream(chip, ctrlif, intf);
		}

		break;
	}
	}

	return 0;
}

/*
 * free the chip instance
 *
 * here we have to do not much, since pcm and controls are already freed
 *
 */

static int snd_usb_audio_free(struct snd_usb_audio *chip)
{
	kfree(chip);
	return 0;
}

static int snd_usb_audio_dev_free(struct snd_device *device)
{
	struct snd_usb_audio *chip = device->device_data;
	return snd_usb_audio_free(chip);
}


/*
 * create a chip instance and set its names.
 */
static int snd_usb_audio_create(struct usb_device *dev, int idx,
				const struct snd_usb_audio_quirk *quirk,
				struct snd_usb_audio **rchip)
{
	struct snd_card *card;
	struct snd_usb_audio *chip;
	int err, len;
	char component[14];
	static struct snd_device_ops ops = {
		.dev_free =	snd_usb_audio_dev_free,
	};

	*rchip = NULL;

	if (snd_usb_get_speed(dev) != USB_SPEED_LOW &&
	    snd_usb_get_speed(dev) != USB_SPEED_FULL &&
	    snd_usb_get_speed(dev) != USB_SPEED_HIGH) {
		snd_printk(KERN_ERR "unknown device speed %d\n", snd_usb_get_speed(dev));
		return -ENXIO;
	}

	err = snd_card_create(index[idx], id[idx], THIS_MODULE, 0, &card);
	if (err < 0) {
		snd_printk(KERN_ERR "cannot create card instance %d\n", idx);
		return err;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (! chip) {
		snd_card_free(card);
		return -ENOMEM;
	}

	chip->index = idx;
	chip->dev = dev;
	chip->card = card;
	chip->setup = device_setup[idx];
	chip->nrpacks = nrpacks;
	chip->async_unlink = async_unlink;

	chip->usb_id = USB_ID(le16_to_cpu(dev->descriptor.idVendor),
			      le16_to_cpu(dev->descriptor.idProduct));
	INIT_LIST_HEAD(&chip->pcm_list);
	INIT_LIST_HEAD(&chip->midi_list);
	INIT_LIST_HEAD(&chip->mixer_list);

	if ((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops)) < 0) {
		snd_usb_audio_free(chip);
		snd_card_free(card);
		return err;
	}

	strcpy(card->driver, "USB-Audio");
	sprintf(component, "USB%04x:%04x",
		USB_ID_VENDOR(chip->usb_id), USB_ID_PRODUCT(chip->usb_id));
	snd_component_add(card, component);

	/* retrieve the device string as shortname */
	if (quirk && quirk->product_name) {
		strlcpy(card->shortname, quirk->product_name, sizeof(card->shortname));
	} else {
		if (!dev->descriptor.iProduct ||
		    usb_string(dev, dev->descriptor.iProduct,
		    card->shortname, sizeof(card->shortname)) <= 0) {
			/* no name available from anywhere, so use ID */
			sprintf(card->shortname, "USB Device %#04x:%#04x",
				USB_ID_VENDOR(chip->usb_id),
				USB_ID_PRODUCT(chip->usb_id));
		}
	}

	/* retrieve the vendor and device strings as longname */
	if (quirk && quirk->vendor_name) {
		len = strlcpy(card->longname, quirk->vendor_name, sizeof(card->longname));
	} else {
		if (dev->descriptor.iManufacturer)
			len = usb_string(dev, dev->descriptor.iManufacturer,
					 card->longname, sizeof(card->longname));
		else
			len = 0;
		/* we don't really care if there isn't any vendor string */
	}
	if (len > 0)
		strlcat(card->longname, " ", sizeof(card->longname));

	strlcat(card->longname, card->shortname, sizeof(card->longname));

	len = strlcat(card->longname, " at ", sizeof(card->longname));

	if (len < sizeof(card->longname))
		usb_make_path(dev, card->longname + len, sizeof(card->longname) - len);

	strlcat(card->longname,
		snd_usb_get_speed(dev) == USB_SPEED_LOW ? ", low speed" :
		snd_usb_get_speed(dev) == USB_SPEED_FULL ? ", full speed" :
		", high speed",
		sizeof(card->longname));

	snd_usb_audio_create_proc(chip);

	*rchip = chip;
	return 0;
}

/*
 * probe the active usb device
 *
 * note that this can be called multiple times per a device, when it
 * includes multiple audio control interfaces.
 *
 * thus we check the usb device pointer and creates the card instance
 * only at the first time.  the successive calls of this function will
 * append the pcm interface to the corresponding card.
 */
static void *snd_usb_audio_probe(struct usb_device *dev,
				 struct usb_interface *intf,
				 const struct usb_device_id *usb_id)
{
	const struct snd_usb_audio_quirk *quirk = (const struct snd_usb_audio_quirk *)usb_id->driver_info;
	int i, err;
	struct snd_usb_audio *chip;
	struct usb_host_interface *alts;
	int ifnum;
	u32 id;

	alts = &intf->altsetting[0];
	ifnum = get_iface_desc(alts)->bInterfaceNumber;
	id = USB_ID(le16_to_cpu(dev->descriptor.idVendor),
		    le16_to_cpu(dev->descriptor.idProduct));
	if (quirk && quirk->ifnum >= 0 && ifnum != quirk->ifnum)
		goto __err_val;

	if (snd_usb_apply_boot_quirk(dev, intf, quirk) < 0)
		goto __err_val;

	/*
	 * found a config.  now register to ALSA
	 */

	/* check whether it's already registered */
	chip = NULL;
	mutex_lock(&register_mutex);
	for (i = 0; i < SNDRV_CARDS; i++) {
		if (usb_chip[i] && usb_chip[i]->dev == dev) {
			if (usb_chip[i]->shutdown) {
				snd_printk(KERN_ERR "USB device is in the shutdown state, cannot create a card instance\n");
				goto __error;
			}
			chip = usb_chip[i];
			break;
		}
	}
	if (! chip) {
		/* it's a fresh one.
		 * now look for an empty slot and create a new card instance
		 */
		for (i = 0; i < SNDRV_CARDS; i++)
			if (enable[i] && ! usb_chip[i] &&
			    (vid[i] == -1 || vid[i] == USB_ID_VENDOR(id)) &&
			    (pid[i] == -1 || pid[i] == USB_ID_PRODUCT(id))) {
				if (snd_usb_audio_create(dev, i, quirk, &chip) < 0) {
					goto __error;
				}
				snd_card_set_dev(chip->card, &intf->dev);
				break;
			}
		if (!chip) {
			printk(KERN_ERR "no available usb audio device\n");
			goto __error;
		}
	}

	chip->txfr_quirk = 0;
	err = 1; /* continue */
	if (quirk && quirk->ifnum != QUIRK_NO_INTERFACE) {
		/* need some special handlings */
		if ((err = snd_usb_create_quirk(chip, intf, &usb_audio_driver, quirk)) < 0)
			goto __error;
	}

	/*
	 * For devices with more than one control interface, we assume the
	 * first contains the audio controls. We might need a more specific
	 * check here in the future.
	 */
	if (!chip->ctrl_intf)
		chip->ctrl_intf = alts;

	if (err > 0) {
		/* create normal USB audio interfaces */
		if (snd_usb_create_streams(chip, ifnum) < 0 ||
		    snd_usb_create_mixer(chip, ifnum, ignore_ctl_error) < 0) {
			goto __error;
		}
	}

	/* we are allowed to call snd_card_register() many times */
	if (snd_card_register(chip->card) < 0) {
		goto __error;
	}

	usb_chip[chip->index] = chip;
	chip->num_interfaces++;
	mutex_unlock(&register_mutex);
	return chip;

 __error:
	if (chip && !chip->num_interfaces)
		snd_card_free(chip->card);
	mutex_unlock(&register_mutex);
 __err_val:
	return NULL;
}

/*
 * we need to take care of counter, since disconnection can be called also
 * many times as well as usb_audio_probe().
 */
static void snd_usb_audio_disconnect(struct usb_device *dev, void *ptr)
{
	struct snd_usb_audio *chip;
	struct snd_card *card;
	struct list_head *p;

	if (ptr == (void *)-1L)
		return;

	chip = ptr;
	card = chip->card;
	mutex_lock(&register_mutex);
	chip->shutdown = 1;
	chip->num_interfaces--;
	if (chip->num_interfaces <= 0) {
		snd_card_disconnect(card);
		/* release the pcm resources */
		list_for_each(p, &chip->pcm_list) {
			snd_usb_stream_disconnect(p);
		}
		/* release the midi resources */
		list_for_each(p, &chip->midi_list) {
			snd_usbmidi_disconnect(p);
		}
		/* release mixer resources */
		list_for_each(p, &chip->mixer_list) {
			snd_usb_mixer_disconnect(p);
		}
		usb_chip[chip->index] = NULL;
		mutex_unlock(&register_mutex);
		snd_card_free_when_closed(card);
	} else {
		mutex_unlock(&register_mutex);
	}
}

/*
 * new 2.5 USB kernel API
 */
static int usb_audio_probe(struct usb_interface *intf,
			   const struct usb_device_id *id)
{
	void *chip;
	chip = snd_usb_audio_probe(interface_to_usbdev(intf), intf, id);
	if (chip) {
		usb_set_intfdata(intf, chip);
		return 0;
	} else
		return -EIO;
}

static void usb_audio_disconnect(struct usb_interface *intf)
{
	snd_usb_audio_disconnect(interface_to_usbdev(intf),
				 usb_get_intfdata(intf));
}

#ifdef CONFIG_PM
static int usb_audio_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct snd_usb_audio *chip = usb_get_intfdata(intf);
	struct list_head *p;
	struct snd_usb_stream *as;

	if (chip == (void *)-1L)
		return 0;

	snd_power_change_state(chip->card, SNDRV_CTL_POWER_D3hot);
	if (!chip->num_suspended_intf++) {
		list_for_each(p, &chip->pcm_list) {
			as = list_entry(p, struct snd_usb_stream, list);
			snd_pcm_suspend_all(as->pcm);
		}
	}

	return 0;
}

static int usb_audio_resume(struct usb_interface *intf)
{
	struct snd_usb_audio *chip = usb_get_intfdata(intf);

	if (chip == (void *)-1L)
		return 0;
	if (--chip->num_suspended_intf)
		return 0;
	/*
	 * ALSA leaves material resumption to user space
	 * we just notify
	 */

	snd_power_change_state(chip->card, SNDRV_CTL_POWER_D0);

	return 0;
}
#else
#define usb_audio_suspend	NULL
#define usb_audio_resume	NULL
#endif		/* CONFIG_PM */

static struct usb_device_id usb_audio_ids [] = {
#include "quirks-table.h"
    { .match_flags = (USB_DEVICE_ID_MATCH_INT_CLASS | USB_DEVICE_ID_MATCH_INT_SUBCLASS),
      .bInterfaceClass = USB_CLASS_AUDIO,
      .bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL },
    { }						/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, usb_audio_ids);

/*
 * entry point for linux usb interface
 */

static struct usb_driver usb_audio_driver = {
	.name =		"snd-usb-audio",
	.probe =	usb_audio_probe,
	.disconnect =	usb_audio_disconnect,
	.suspend =	usb_audio_suspend,
	.resume =	usb_audio_resume,
	.id_table =	usb_audio_ids,
};

static int __init snd_usb_audio_init(void)
{
	if (nrpacks < 1 || nrpacks > MAX_PACKS) {
		printk(KERN_WARNING "invalid nrpacks value.\n");
		return -EINVAL;
	}
	return usb_register(&usb_audio_driver);
}

static void __exit snd_usb_audio_cleanup(void)
{
	usb_deregister(&usb_audio_driver);
}

module_init(snd_usb_audio_init);
module_exit(snd_usb_audio_cleanup);
