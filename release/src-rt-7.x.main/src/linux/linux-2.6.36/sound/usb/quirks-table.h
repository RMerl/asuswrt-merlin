/*
 * ALSA USB Audio Driver
 *
 * Copyright (c) 2002 by Takashi Iwai <tiwai@suse.de>,
 *                       Clemens Ladisch <clemens@ladisch.de>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/*
 * The contents of this file are part of the driver's id_table.
 *
 * In a perfect world, this file would be empty.
 */

/*
 * Use this for devices where other interfaces are standard compliant,
 * to prevent the quirk being applied to those interfaces. (To work with
 * hotplugging, bDeviceClass must be set to USB_CLASS_PER_INTERFACE.)
 */
#define USB_DEVICE_VENDOR_SPEC(vend, prod) \
	.match_flags = USB_DEVICE_ID_MATCH_VENDOR | \
		       USB_DEVICE_ID_MATCH_PRODUCT | \
		       USB_DEVICE_ID_MATCH_INT_CLASS, \
	.idVendor = vend, \
	.idProduct = prod, \
	.bInterfaceClass = USB_CLASS_VENDOR_SPEC

/* Creative/Toshiba Multimedia Center SB-0500 */
{
	USB_DEVICE(0x041e, 0x3048),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Toshiba",
		.product_name = "SB-0500",
		.ifnum = QUIRK_NO_INTERFACE
	}
},

/* Creative/E-Mu devices */
{
	USB_DEVICE(0x041e, 0x3010),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Creative Labs",
		.product_name = "Sound Blaster MP3+",
		.ifnum = QUIRK_NO_INTERFACE
	}
},
{
	/* E-Mu 0202 USB */
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE,
	.idVendor = 0x041e,
	.idProduct = 0x3f02,
	.bInterfaceClass = USB_CLASS_AUDIO,
},
{
	/* E-Mu 0404 USB */
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE,
	.idVendor = 0x041e,
	.idProduct = 0x3f04,
	.bInterfaceClass = USB_CLASS_AUDIO,
},
{
	/* E-Mu Tracker Pre */
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE,
	.idVendor = 0x041e,
	.idProduct = 0x3f0a,
	.bInterfaceClass = USB_CLASS_AUDIO,
},

/*
 * Logitech QuickCam: bDeviceClass is vendor-specific, so generic interface
 * class matches do not take effect without an explicit ID match.
 */
{
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.idVendor = 0x046d,
	.idProduct = 0x0850,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL
},
{
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.idVendor = 0x046d,
	.idProduct = 0x08ae,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL
},
{
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.idVendor = 0x046d,
	.idProduct = 0x08c6,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL
},
{
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.idVendor = 0x046d,
	.idProduct = 0x08f0,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL
},
{
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.idVendor = 0x046d,
	.idProduct = 0x08f5,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL
},
{
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.idVendor = 0x046d,
	.idProduct = 0x08f6,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL
},
{
	USB_DEVICE(0x046d, 0x0990),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Logitech, Inc.",
		.product_name = "QuickCam Pro 9000",
		.ifnum = QUIRK_NO_INTERFACE
	}
},

/*
 * Yamaha devices
 */

#define YAMAHA_DEVICE(id, name) { \
	USB_DEVICE(0x0499, id), \
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) { \
		.vendor_name = "Yamaha", \
		.product_name = name, \
		.ifnum = QUIRK_ANY_INTERFACE, \
		.type = QUIRK_MIDI_YAMAHA \
	} \
}
#define YAMAHA_INTERFACE(id, intf, name) { \
	USB_DEVICE_VENDOR_SPEC(0x0499, id), \
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) { \
		.vendor_name = "Yamaha", \
		.product_name = name, \
		.ifnum = intf, \
		.type = QUIRK_MIDI_YAMAHA \
	} \
}
YAMAHA_DEVICE(0x1000, "UX256"),
YAMAHA_DEVICE(0x1001, "MU1000"),
YAMAHA_DEVICE(0x1002, "MU2000"),
YAMAHA_DEVICE(0x1003, "MU500"),
YAMAHA_INTERFACE(0x1004, 3, "UW500"),
YAMAHA_DEVICE(0x1005, "MOTIF6"),
YAMAHA_DEVICE(0x1006, "MOTIF7"),
YAMAHA_DEVICE(0x1007, "MOTIF8"),
YAMAHA_DEVICE(0x1008, "UX96"),
YAMAHA_DEVICE(0x1009, "UX16"),
YAMAHA_INTERFACE(0x100a, 3, "EOS BX"),
YAMAHA_DEVICE(0x100c, "UC-MX"),
YAMAHA_DEVICE(0x100d, "UC-KX"),
YAMAHA_DEVICE(0x100e, "S08"),
YAMAHA_DEVICE(0x100f, "CLP-150"),
YAMAHA_DEVICE(0x1010, "CLP-170"),
YAMAHA_DEVICE(0x1011, "P-250"),
YAMAHA_DEVICE(0x1012, "TYROS"),
YAMAHA_DEVICE(0x1013, "PF-500"),
YAMAHA_DEVICE(0x1014, "S90"),
YAMAHA_DEVICE(0x1015, "MOTIF-R"),
YAMAHA_DEVICE(0x1016, "MDP-5"),
YAMAHA_DEVICE(0x1017, "CVP-204"),
YAMAHA_DEVICE(0x1018, "CVP-206"),
YAMAHA_DEVICE(0x1019, "CVP-208"),
YAMAHA_DEVICE(0x101a, "CVP-210"),
YAMAHA_DEVICE(0x101b, "PSR-1100"),
YAMAHA_DEVICE(0x101c, "PSR-2100"),
YAMAHA_DEVICE(0x101d, "CLP-175"),
YAMAHA_DEVICE(0x101e, "PSR-K1"),
YAMAHA_DEVICE(0x101f, "EZ-J24"),
YAMAHA_DEVICE(0x1020, "EZ-250i"),
YAMAHA_DEVICE(0x1021, "MOTIF ES 6"),
YAMAHA_DEVICE(0x1022, "MOTIF ES 7"),
YAMAHA_DEVICE(0x1023, "MOTIF ES 8"),
YAMAHA_DEVICE(0x1024, "CVP-301"),
YAMAHA_DEVICE(0x1025, "CVP-303"),
YAMAHA_DEVICE(0x1026, "CVP-305"),
YAMAHA_DEVICE(0x1027, "CVP-307"),
YAMAHA_DEVICE(0x1028, "CVP-309"),
YAMAHA_DEVICE(0x1029, "CVP-309GP"),
YAMAHA_DEVICE(0x102a, "PSR-1500"),
YAMAHA_DEVICE(0x102b, "PSR-3000"),
YAMAHA_DEVICE(0x102e, "ELS-01/01C"),
YAMAHA_DEVICE(0x1030, "PSR-295/293"),
YAMAHA_DEVICE(0x1031, "DGX-205/203"),
YAMAHA_DEVICE(0x1032, "DGX-305"),
YAMAHA_DEVICE(0x1033, "DGX-505"),
YAMAHA_DEVICE(0x1034, NULL),
YAMAHA_DEVICE(0x1035, NULL),
YAMAHA_DEVICE(0x1036, NULL),
YAMAHA_DEVICE(0x1037, NULL),
YAMAHA_DEVICE(0x1038, NULL),
YAMAHA_DEVICE(0x1039, NULL),
YAMAHA_DEVICE(0x103a, NULL),
YAMAHA_DEVICE(0x103b, NULL),
YAMAHA_DEVICE(0x103c, NULL),
YAMAHA_DEVICE(0x103d, NULL),
YAMAHA_DEVICE(0x103e, NULL),
YAMAHA_DEVICE(0x103f, NULL),
YAMAHA_DEVICE(0x1040, NULL),
YAMAHA_DEVICE(0x1041, NULL),
YAMAHA_DEVICE(0x1042, NULL),
YAMAHA_DEVICE(0x1043, NULL),
YAMAHA_DEVICE(0x1044, NULL),
YAMAHA_DEVICE(0x1045, NULL),
YAMAHA_INTERFACE(0x104e, 0, NULL),
YAMAHA_DEVICE(0x104f, NULL),
YAMAHA_DEVICE(0x1050, NULL),
YAMAHA_DEVICE(0x1051, NULL),
YAMAHA_DEVICE(0x1052, NULL),
YAMAHA_DEVICE(0x2000, "DGP-7"),
YAMAHA_DEVICE(0x2001, "DGP-5"),
YAMAHA_DEVICE(0x2002, NULL),
YAMAHA_DEVICE(0x5000, "CS1D"),
YAMAHA_DEVICE(0x5001, "DSP1D"),
YAMAHA_DEVICE(0x5002, "DME32"),
YAMAHA_DEVICE(0x5003, "DM2000"),
YAMAHA_DEVICE(0x5004, "02R96"),
YAMAHA_DEVICE(0x5005, "ACU16-C"),
YAMAHA_DEVICE(0x5006, "NHB32-C"),
YAMAHA_DEVICE(0x5007, "DM1000"),
YAMAHA_DEVICE(0x5008, "01V96"),
YAMAHA_DEVICE(0x5009, "SPX2000"),
YAMAHA_DEVICE(0x500a, "PM5D"),
YAMAHA_DEVICE(0x500b, "DME64N"),
YAMAHA_DEVICE(0x500c, "DME24N"),
YAMAHA_DEVICE(0x500d, NULL),
YAMAHA_DEVICE(0x500e, NULL),
YAMAHA_DEVICE(0x500f, NULL),
YAMAHA_DEVICE(0x7000, "DTX"),
YAMAHA_DEVICE(0x7010, "UB99"),
#undef YAMAHA_DEVICE
#undef YAMAHA_INTERFACE

/*
 * Roland/RolandED/Edirol/BOSS devices
 */
{
	USB_DEVICE(0x0582, 0x0000),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "UA-100",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_FIXED_ENDPOINT,
				.data = & (const struct audioformat) {
					.formats = SNDRV_PCM_FMTBIT_S16_LE,
					.channels = 4,
					.iface = 0,
					.altsetting = 1,
					.altset_idx = 1,
					.attributes = 0,
					.endpoint = 0x01,
					.ep_attr = 0x09,
					.rates = SNDRV_PCM_RATE_CONTINUOUS,
					.rate_min = 44100,
					.rate_max = 44100,
				}
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_FIXED_ENDPOINT,
				.data = & (const struct audioformat) {
					.formats = SNDRV_PCM_FMTBIT_S16_LE,
					.channels = 2,
					.iface = 1,
					.altsetting = 1,
					.altset_idx = 1,
					.attributes = UAC_EP_CS_ATTR_FILL_MAX,
					.endpoint = 0x81,
					.ep_attr = 0x05,
					.rates = SNDRV_PCM_RATE_CONTINUOUS,
					.rate_min = 44100,
					.rate_max = 44100,
				}
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0007,
					.in_cables  = 0x0007
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0582, 0x0002),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UM-4",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x000f,
					.in_cables  = 0x000f
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0582, 0x0003),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "SC-8850",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x003f,
					.in_cables  = 0x003f
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0582, 0x0004),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "U-8",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0005,
					.in_cables  = 0x0005
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* Has ID 0x0099 when not in "Advanced Driver" mode.
	 * The UM-2EX has only one input, but we cannot detect this. */
	USB_DEVICE(0x0582, 0x0005),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UM-2",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0003,
					.in_cables  = 0x0003
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0582, 0x0007),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "SC-8820",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0013,
					.in_cables  = 0x0013
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0582, 0x0008),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "PC-300",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x009d when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0009),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UM-1",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0582, 0x000b),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "SK-500",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0013,
					.in_cables  = 0x0013
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* thanks to Emiliano Grilli <emillo@libero.it>
	 * for helping researching this data */
	USB_DEVICE(0x0582, 0x000c),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "SC-D70",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_FIXED_ENDPOINT,
				.data = & (const struct audioformat) {
					.formats = SNDRV_PCM_FMTBIT_S24_3LE,
					.channels = 2,
					.iface = 0,
					.altsetting = 1,
					.altset_idx = 1,
					.attributes = 0,
					.endpoint = 0x01,
					.ep_attr = 0x01,
					.rates = SNDRV_PCM_RATE_CONTINUOUS,
					.rate_min = 44100,
					.rate_max = 44100,
				}
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_FIXED_ENDPOINT,
				.data = & (const struct audioformat) {
					.formats = SNDRV_PCM_FMTBIT_S24_3LE,
					.channels = 2,
					.iface = 1,
					.altsetting = 1,
					.altset_idx = 1,
					.attributes = 0,
					.endpoint = 0x81,
					.ep_attr = 0x01,
					.rates = SNDRV_PCM_RATE_CONTINUOUS,
					.rate_min = 44100,
					.rate_max = 44100,
				}
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0007,
					.in_cables  = 0x0007
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{	/*
	 * This quirk is for the "Advanced Driver" mode of the Edirol UA-5.
	 * If the advanced mode switch at the back of the unit is off, the
	 * UA-5 has ID 0x0582/0x0011 and is standard compliant (no quirks),
	 * but offers only 16-bit PCM.
	 * In advanced mode, the UA-5 will output S24_3LE samples (two
	 * channels) at the rate indicated on the front switch, including
	 * the 96kHz sample rate.
	 */
	USB_DEVICE(0x0582, 0x0010),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UA-5",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x0013 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0012),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "XV-5050",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	/* has ID 0x0015 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0014),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UM-880",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x01ff,
			.in_cables  = 0x01ff
		}
	}
},
{
	/* has ID 0x0017 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0016),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "SD-90",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x000f,
					.in_cables  = 0x000f
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x001c when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x001b),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "MMP-2",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x001e when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x001d),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "V-SYNTH",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	/* has ID 0x0024 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0023),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UM-550",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x003f,
			.in_cables  = 0x003f
		}
	}
},
{
	/*
	 * This quirk is for the "Advanced Driver" mode. If off, the UA-20
	 * has ID 0x0026 and is standard compliant, but has only 16-bit PCM
	 * and no MIDI.
	 */
	USB_DEVICE(0x0582, 0x0025),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UA-20",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_FIXED_ENDPOINT,
				.data = & (const struct audioformat) {
					.formats = SNDRV_PCM_FMTBIT_S24_3LE,
					.channels = 2,
					.iface = 1,
					.altsetting = 1,
					.altset_idx = 1,
					.attributes = 0,
					.endpoint = 0x01,
					.ep_attr = 0x01,
					.rates = SNDRV_PCM_RATE_CONTINUOUS,
					.rate_min = 44100,
					.rate_max = 44100,
				}
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_FIXED_ENDPOINT,
				.data = & (const struct audioformat) {
					.formats = SNDRV_PCM_FMTBIT_S24_3LE,
					.channels = 2,
					.iface = 2,
					.altsetting = 1,
					.altset_idx = 1,
					.attributes = 0,
					.endpoint = 0x82,
					.ep_attr = 0x01,
					.rates = SNDRV_PCM_RATE_CONTINUOUS,
					.rate_min = 44100,
					.rate_max = 44100,
				}
			},
			{
				.ifnum = 3,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x0028 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0027),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "SD-20",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0003,
			.in_cables  = 0x0007
		}
	}
},
{
	/* has ID 0x002a when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0029),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "SD-80",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x000f,
			.in_cables  = 0x000f
		}
	}
},
{	/*
	 * This quirk is for the "Advanced" modes of the Edirol UA-700.
	 * If the sample format switch is not in an advanced setting, the
	 * UA-700 has ID 0x0582/0x002c and is standard compliant (no quirks),
	 * but offers only 16-bit PCM and no MIDI.
	 */
	USB_DEVICE_VENDOR_SPEC(0x0582, 0x002b),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UA-700",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = 3,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x002e when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x002d),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "XV-2020",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	/* has ID 0x0030 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x002f),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "VariOS",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0007,
			.in_cables  = 0x0007
		}
	}
},
{
	/* has ID 0x0034 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0033),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "PCR",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0003,
			.in_cables  = 0x0007
		}
	}
},
	/* TODO: add Roland M-1000 support */
{
	/*
	 * Has ID 0x0038 when not in "Advanced Driver" mode;
	 * later revisions use IDs 0x0054 and 0x00a2.
	 */
	USB_DEVICE(0x0582, 0x0037),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "Digital Piano",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	/*
	 * This quirk is for the "Advanced Driver" mode.  If off, the GS-10
	 * has ID 0x003c and is standard compliant, but has only 16-bit PCM
	 * and no MIDI.
	 */
	USB_DEVICE_VENDOR_SPEC(0x0582, 0x003b),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "BOSS",
		.product_name = "GS-10",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = & (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 3,
				.type = QUIRK_MIDI_STANDARD_INTERFACE
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x0041 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0040),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "GI-20",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	/* has ID 0x0043 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0042),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "RS-70",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	/* has ID 0x0049 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0047),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		/* .vendor_name = "EDIROL", */
		/* .product_name = "UR-80", */
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			/* in the 96 kHz modes, only interface 1 is there */
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x004a when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0048),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		/* .vendor_name = "EDIROL", */
		/* .product_name = "UR-80", */
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0003,
			.in_cables  = 0x0007
		}
	}
},
	/* TODO: add Edirol M-100FX support */
{
	/* has ID 0x004e when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x004c),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "PCR-A",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x004f when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x004d),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "PCR-A",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0003,
			.in_cables  = 0x0007
		}
	}
},
{
	/*
	 * This quirk is for the "Advanced Driver" mode. If off, the UA-3FX
	 * is standard compliant, but has only 16-bit PCM.
	 */
	USB_DEVICE(0x0582, 0x0050),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UA-3FX",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0582, 0x0052),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UM-1SX",
		.ifnum = 0,
		.type = QUIRK_MIDI_STANDARD_INTERFACE
	}
},
{
	USB_DEVICE(0x0582, 0x0060),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "EXR Series",
		.ifnum = 0,
		.type = QUIRK_MIDI_STANDARD_INTERFACE
	}
},
{
	/* has ID 0x0067 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0065),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "PCR-1",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0003
		}
	}
},
{
	/* has ID 0x006b when not in "Advanced Driver" mode */
	USB_DEVICE_VENDOR_SPEC(0x0582, 0x006a),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "SP-606",
		.ifnum = 3,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	/* has ID 0x006e when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x006d),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "FANTOM-X",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{	/*
	 * This quirk is for the "Advanced" modes of the Edirol UA-25.
	 * If the switch is not in an advanced setting, the UA-25 has
	 * ID 0x0582/0x0073 and is standard compliant (no quirks), but
	 * offers only 16-bit PCM at 44.1 kHz and no MIDI.
	 */
	USB_DEVICE_VENDOR_SPEC(0x0582, 0x0074),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UA-25",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x0076 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0075),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "BOSS",
		.product_name = "DR-880",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	/* has ID 0x007b when not in "Advanced Driver" mode */
	USB_DEVICE_VENDOR_SPEC(0x0582, 0x007a),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		/* "RD" or "RD-700SX"? */
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0003,
			.in_cables  = 0x0003
		}
	}
},
{
	/* has ID 0x0081 when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x0080),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "G-70",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
	/* TODO: add Roland V-SYNTH XT support */
	/* TODO: add BOSS GT-PRO support */
{
	/* has ID 0x008c when not in "Advanced Driver" mode */
	USB_DEVICE(0x0582, 0x008b),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "PC-50",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
	/* TODO: add Edirol PC-80 support */
{
	USB_DEVICE(0x0582, 0x0096),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UA-1EX",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0582, 0x009a),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UM-3EX",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x000f,
			.in_cables  = 0x000f
		}
	}
},
{
	/*
	 * This quirk is for the "Advanced Driver" mode. If off, the UA-4FX
	 * is standard compliant, but has only 16-bit PCM and no MIDI.
	 */
	USB_DEVICE(0x0582, 0x00a3),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UA-4FX",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = -1
			}
		}
	}
},
	/* TODO: add Edirol MD-P1 support */
{
	USB_DEVICE(0x582, 0x00a6),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "Juno-G",
		.ifnum = 0,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	/* Roland SH-201 */
	USB_DEVICE(0x0582, 0x00ad),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "SH-201",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* Roland SonicCell */
	USB_DEVICE(0x0582, 0x00c2),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Roland",
		.product_name = "SonicCell",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* Edirol M-16DX */
	USB_DEVICE(0x0582, 0x00c4),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* BOSS GT-10 */
	USB_DEVICE(0x0582, 0x00da),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* Advanced modes of the Edirol UA-25EX.
	 * For the standard mode, UA-25EX has ID 0582:00e7, which
	 * offers only 16-bit PCM at 44.1 kHz and no MIDI.
	 */
	USB_DEVICE_VENDOR_SPEC(0x0582, 0x00e6),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "EDIROL",
		.product_name = "UA-25EX",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_EDIROL_UAXX
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	/* has ID 0x00ea when not in Advanced Driver mode */
	USB_DEVICE_VENDOR_SPEC(0x0582, 0x00e9),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		/* .vendor_name = "Roland", */
		/* .product_name = "UA-1G", */
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = -1
			}
		}
	}
},

/* Guillemot devices */
{
	/*
	 * This is for the "Windows Edition" where the external MIDI ports are
	 * the only MIDI ports; the control data is reported through HID
	 * interfaces.  The "Macintosh Edition" has ID 0xd002 and uses standard
	 * compliant USB MIDI ports for external MIDI and controls.
	 */
	USB_DEVICE_VENDOR_SPEC(0x06f8, 0xb000),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Hercules",
		.product_name = "DJ Console (WE)",
		.ifnum = 4,
		.type = QUIRK_MIDI_FIXED_ENDPOINT,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables = 0x0001
		}
	}
},

/* Midiman/M-Audio devices */
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x1002),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "MidiSport 2x2",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_MIDI_MIDIMAN,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0003,
			.in_cables  = 0x0003
		}
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x1011),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "MidiSport 1x1",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_MIDI_MIDIMAN,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x1015),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "Keystation",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_MIDI_MIDIMAN,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x1021),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "MidiSport 4x4",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_MIDI_MIDIMAN,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x000f,
			.in_cables  = 0x000f
		}
	}
},
{
	/*
	 * For hardware revision 1.05; in the later revisions (1.10 and
	 * 1.21), 0x1031 is the ID for the device without firmware.
	 * Thanks to Olaf Giesbrecht <Olaf_Giesbrecht@yahoo.de>
	 */
	USB_DEVICE_VER(0x0763, 0x1031, 0x0100, 0x0109),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "MidiSport 8x8",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_MIDI_MIDIMAN,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x01ff,
			.in_cables  = 0x01ff
		}
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x1033),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "MidiSport 8x8",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_MIDI_MIDIMAN,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x01ff,
			.in_cables  = 0x01ff
		}
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x1041),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "MidiSport 2x4",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_MIDI_MIDIMAN,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x000f,
			.in_cables  = 0x0003
		}
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x2001),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "Quattro",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = & (const struct snd_usb_audio_quirk[]) {
			/*
			 * Interfaces 0-2 are "Windows-compatible", 16-bit only,
			 * and share endpoints with the other interfaces.
			 * Ignore them.  The other interfaces can do 24 bits,
			 * but captured samples are big-endian (see usbaudio.c).
			 */
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 3,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 4,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 5,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 6,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 7,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 8,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 9,
				.type = QUIRK_MIDI_MIDIMAN,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x2003),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "AudioPhile",
		.ifnum = 6,
		.type = QUIRK_MIDI_MIDIMAN,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x2008),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "Ozone",
		.ifnum = 3,
		.type = QUIRK_MIDI_MIDIMAN,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x0001,
			.in_cables  = 0x0001
		}
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0763, 0x200d),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "M-Audio",
		.product_name = "OmniStudio",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = & (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 3,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 4,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 5,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 6,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 7,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 8,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 9,
				.type = QUIRK_MIDI_MIDIMAN,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0763, 0x2019),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		/* .vendor_name = "M-Audio", */
		/* .product_name = "Ozone Academic", */
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = & (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 3,
				.type = QUIRK_MIDI_MIDIMAN,
				.data = & (const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0001,
					.in_cables  = 0x0001
				}
			},
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0763, 0x2080),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		/* .vendor_name = "M-Audio", */
		/* .product_name = "Fast Track Ultra 8", */
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = & (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			/* interface 3 (MIDI) is standard compliant */
			{
				.ifnum = -1
			}
		}
	}
},
{
	USB_DEVICE(0x0763, 0x2081),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		/* .vendor_name = "M-Audio", */
		/* .product_name = "Fast Track Ultra 8R", */
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = & (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			{
				.ifnum = 2,
				.type = QUIRK_AUDIO_STANDARD_INTERFACE
			},
			/* interface 3 (MIDI) is standard compliant */
			{
				.ifnum = -1
			}
		}
	}
},

/* Casio devices */
{
	USB_DEVICE(0x07cf, 0x6801),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Casio",
		.product_name = "PL-40R",
		.ifnum = 0,
		.type = QUIRK_MIDI_YAMAHA
	}
},
{
	/* this ID is used by several devices without a product ID */
	USB_DEVICE(0x07cf, 0x6802),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Casio",
		.product_name = "Keyboard",
		.ifnum = 0,
		.type = QUIRK_MIDI_YAMAHA
	}
},

/* Mark of the Unicorn devices */
{
	/* thanks to Robert A. Lerche <ral 'at' msbit.com> */
	.match_flags = USB_DEVICE_ID_MATCH_VENDOR |
		       USB_DEVICE_ID_MATCH_PRODUCT |
		       USB_DEVICE_ID_MATCH_DEV_SUBCLASS,
	.idVendor = 0x07fd,
	.idProduct = 0x0001,
	.bDeviceSubClass = 2,
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "MOTU",
		.product_name = "Fastlane",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = & (const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 0,
				.type = QUIRK_MIDI_FASTLANE
			},
			{
				.ifnum = 1,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = -1
			}
		}
	}
},

/* Emagic devices */
{
	USB_DEVICE(0x086a, 0x0001),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Emagic",
		/* .product_name = "Unitor8", */
		.ifnum = 2,
		.type = QUIRK_MIDI_EMAGIC,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x80ff,
			.in_cables  = 0x80ff
		}
	}
},
{
	USB_DEVICE(0x086a, 0x0002),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Emagic",
		/* .product_name = "AMT8", */
		.ifnum = 2,
		.type = QUIRK_MIDI_EMAGIC,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x80ff,
			.in_cables  = 0x80ff
		}
	}
},
{
	USB_DEVICE(0x086a, 0x0003),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Emagic",
		/* .product_name = "MT4", */
		.ifnum = 2,
		.type = QUIRK_MIDI_EMAGIC,
		.data = & (const struct snd_usb_midi_endpoint_info) {
			.out_cables = 0x800f,
			.in_cables  = 0x8003
		}
	}
},

/* AKAI devices */
{
	USB_DEVICE(0x09e8, 0x0062),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "AKAI",
		.product_name = "MPD16",
		.ifnum = 0,
		.type = QUIRK_MIDI_AKAI,
	}
},

/* TerraTec devices */
{
	USB_DEVICE_VENDOR_SPEC(0x0ccd, 0x0012),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "TerraTec",
		.product_name = "PHASE 26",
		.ifnum = 3,
		.type = QUIRK_MIDI_STANDARD_INTERFACE
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0ccd, 0x0013),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "TerraTec",
		.product_name = "PHASE 26",
		.ifnum = 3,
		.type = QUIRK_MIDI_STANDARD_INTERFACE
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0ccd, 0x0014),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "TerraTec",
		.product_name = "PHASE 26",
		.ifnum = 3,
		.type = QUIRK_MIDI_STANDARD_INTERFACE
	}
},
{
	USB_DEVICE(0x0ccd, 0x0028),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "TerraTec",
		.product_name = "Aureon5.1MkII",
		.ifnum = QUIRK_NO_INTERFACE
	}
},
{
	USB_DEVICE(0x0ccd, 0x0035),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Miditech",
		.product_name = "Play'n Roll",
		.ifnum = 0,
		.type = QUIRK_MIDI_CME
	}
},

/* Stanton/N2IT Final Scratch v1 device ('Scratchamp') */
{
	USB_DEVICE(0x103d, 0x0100),
		.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Stanton",
		.product_name = "ScratchAmp",
		.ifnum = QUIRK_NO_INTERFACE
	}
},
{
	USB_DEVICE(0x103d, 0x0101),
		.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Stanton",
		.product_name = "ScratchAmp",
		.ifnum = QUIRK_NO_INTERFACE
	}
},

/* Novation EMS devices */
{
	USB_DEVICE_VENDOR_SPEC(0x1235, 0x0001),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Novation",
		.product_name = "ReMOTE Audio/XStation",
		.ifnum = 4,
		.type = QUIRK_MIDI_NOVATION
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x1235, 0x0002),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Novation",
		.product_name = "Speedio",
		.ifnum = 3,
		.type = QUIRK_MIDI_NOVATION
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x1235, 0x4661),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Novation",
		.product_name = "ReMOTE25",
		.ifnum = 0,
		.type = QUIRK_MIDI_NOVATION
	}
},

/* Access Music devices */
{
	/* VirusTI Desktop */
	USB_DEVICE_VENDOR_SPEC(0x133e, 0x0815),
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = &(const struct snd_usb_audio_quirk[]) {
			{
				.ifnum = 3,
				.type = QUIRK_MIDI_FIXED_ENDPOINT,
				.data = &(const struct snd_usb_midi_endpoint_info) {
					.out_cables = 0x0003,
					.in_cables  = 0x0003
				}
			},
			{
				.ifnum = 4,
				.type = QUIRK_IGNORE_INTERFACE
			},
			{
				.ifnum = -1
			}
		}
	}
},

/* */
{
	/* aka. Serato Scratch Live DJ Box */
	USB_DEVICE(0x13e5, 0x0001),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Rane",
		.product_name = "SL-1",
		.ifnum = QUIRK_NO_INTERFACE
	}
},

/* Miditech devices */
{
	USB_DEVICE(0x4752, 0x0011),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.vendor_name = "Miditech",
		.product_name = "Midistart-2",
		.ifnum = 0,
		.type = QUIRK_MIDI_CME
	}
},

/* Central Music devices */
{
	/* this ID used by both Miditech MidiStudio-2 and CME UF-x */
	USB_DEVICE(0x7104, 0x2202),
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.ifnum = 0,
		.type = QUIRK_MIDI_CME
	}
},

/* Hauppauge HVR-950Q and HVR-850 */
{
	USB_DEVICE_VENDOR_SPEC(0x2040, 0x7200),
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Hauppauge",
		.product_name = "HVR-950Q",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_AUDIO_ALIGN_TRANSFER,
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x2040, 0x7240),
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Hauppauge",
		.product_name = "HVR-850",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_AUDIO_ALIGN_TRANSFER,
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x2040, 0x7210),
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Hauppauge",
		.product_name = "HVR-950Q",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_AUDIO_ALIGN_TRANSFER,
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x2040, 0x7217),
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Hauppauge",
		.product_name = "HVR-950Q",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_AUDIO_ALIGN_TRANSFER,
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x2040, 0x721b),
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Hauppauge",
		.product_name = "HVR-950Q",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_AUDIO_ALIGN_TRANSFER,
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x2040, 0x721e),
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Hauppauge",
		.product_name = "HVR-950Q",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_AUDIO_ALIGN_TRANSFER,
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x2040, 0x721f),
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Hauppauge",
		.product_name = "HVR-950Q",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_AUDIO_ALIGN_TRANSFER,
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x2040, 0x7280),
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Hauppauge",
		.product_name = "HVR-950Q",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_AUDIO_ALIGN_TRANSFER,
	}
},
{
	USB_DEVICE_VENDOR_SPEC(0x0fd9, 0x0008),
	.match_flags = USB_DEVICE_ID_MATCH_DEVICE |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Hauppauge",
		.product_name = "HVR-950Q",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_AUDIO_ALIGN_TRANSFER,
	}
},

/* Digidesign Mbox */
{
	/* Thanks to Clemens Ladisch <clemens@ladisch.de> */
	USB_DEVICE(0x0dba, 0x1000),
	.driver_info = (unsigned long) &(const struct snd_usb_audio_quirk) {
		.vendor_name = "Digidesign",
		.product_name = "MBox",
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_COMPOSITE,
		.data = (const struct snd_usb_audio_quirk[]){
			{
				.ifnum = 0,
				.type = QUIRK_IGNORE_INTERFACE,
			},
			{
				.ifnum = 1,
				.type = QUIRK_AUDIO_FIXED_ENDPOINT,
				.data = &(const struct audioformat) {
					.formats = SNDRV_PCM_FMTBIT_S24_3BE,
					.channels = 2,
					.iface = 1,
					.altsetting = 1,
					.altset_idx = 1,
					.attributes = UAC_EP_CS_ATTR_SAMPLE_RATE,
					.endpoint = 0x02,
					.ep_attr = 0x01,
					.maxpacksize = 0x130,
					.rates = SNDRV_PCM_RATE_44100 |
						 SNDRV_PCM_RATE_48000,
					.rate_min = 44100,
					.rate_max = 48000,
					.nr_rates = 2,
					.rate_table = (unsigned int[]) {
						44100, 48000
					}
				}
			},
			{
				.ifnum = -1
			}
		}

	}
},

{
	/*
	 * Some USB MIDI devices don't have an audio control interface,
	 * so we have to grab MIDI streaming interfaces here.
	 */
	.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_MIDISTREAMING,
	.driver_info = (unsigned long) & (const struct snd_usb_audio_quirk) {
		.ifnum = QUIRK_ANY_INTERFACE,
		.type = QUIRK_MIDI_STANDARD_INTERFACE
	}
},

#undef USB_DEVICE_VENDOR_SPEC
