/* flydvb.h - Keytable for flydvb Remote Controller
 *
 * keymap imported from ir-keymaps.c
 *
 * Copyright (c) 2010 by Mauro Carvalho Chehab <mchehab@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <media/rc-map.h>

static struct ir_scancode flydvb[] = {
	{ 0x01, KEY_ZOOM },		/* Full Screen */
	{ 0x00, KEY_POWER },		/* Power */

	{ 0x03, KEY_1 },
	{ 0x04, KEY_2 },
	{ 0x05, KEY_3 },
	{ 0x07, KEY_4 },
	{ 0x08, KEY_5 },
	{ 0x09, KEY_6 },
	{ 0x0b, KEY_7 },
	{ 0x0c, KEY_8 },
	{ 0x0d, KEY_9 },
	{ 0x06, KEY_AGAIN },		/* Recall */
	{ 0x0f, KEY_0 },
	{ 0x10, KEY_MUTE },		/* Mute */
	{ 0x02, KEY_RADIO },		/* TV/Radio */
	{ 0x1b, KEY_LANGUAGE },		/* SAP (Second Audio Program) */

	{ 0x14, KEY_VOLUMEUP },		/* VOL+ */
	{ 0x17, KEY_VOLUMEDOWN },	/* VOL- */
	{ 0x12, KEY_CHANNELUP },	/* CH+ */
	{ 0x13, KEY_CHANNELDOWN },	/* CH- */
	{ 0x1d, KEY_ENTER },		/* Enter */

	{ 0x1a, KEY_MODE },		/* PIP */
	{ 0x18, KEY_TUNER },		/* Source */

	{ 0x1e, KEY_RECORD },		/* Record/Pause */
	{ 0x15, KEY_ANGLE },		/* Swap (no label on key) */
	{ 0x1c, KEY_PAUSE },		/* Timeshift/Pause */
	{ 0x19, KEY_BACK },		/* Rewind << */
	{ 0x0a, KEY_PLAYPAUSE },	/* Play/Pause */
	{ 0x1f, KEY_FORWARD },		/* Forward >> */
	{ 0x16, KEY_PREVIOUS },		/* Back |<< */
	{ 0x11, KEY_STOP },		/* Stop */
	{ 0x0e, KEY_NEXT },		/* End >>| */
};

static struct rc_keymap flydvb_map = {
	.map = {
		.scan    = flydvb,
		.size    = ARRAY_SIZE(flydvb),
		.ir_type = IR_TYPE_UNKNOWN,	/* Legacy IR type */
		.name    = RC_MAP_FLYDVB,
	}
};

static int __init init_rc_map_flydvb(void)
{
	return ir_register_map(&flydvb_map);
}

static void __exit exit_rc_map_flydvb(void)
{
	ir_unregister_map(&flydvb_map);
}

module_init(init_rc_map_flydvb)
module_exit(exit_rc_map_flydvb)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@redhat.com>");
