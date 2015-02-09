/* behold.h - Keytable for behold Remote Controller
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

/*
 * Igor Kuznetsov <igk72@ya.ru>
 * Andrey J. Melnikov <temnota@kmv.ru>
 *
 * Keytable is used by BeholdTV 60x series, M6 series at
 * least, and probably other cards too.
 * The "ascii-art picture" below (in comments, first row
 * is the keycode in hex, and subsequent row(s) shows
 * the button labels (several variants when appropriate)
 * helps to descide which keycodes to assign to the buttons.
 */

static struct ir_scancode behold[] = {

	/*  0x1c            0x12  *
	 *  TV/FM          POWER  *
	 *                        */
	{ 0x1c, KEY_TUNER },
	{ 0x12, KEY_POWER },

	/*  0x01    0x02    0x03  *
	 *   1       2       3    *
	 *                        *
	 *  0x04    0x05    0x06  *
	 *   4       5       6    *
	 *                        *
	 *  0x07    0x08    0x09  *
	 *   7       8       9    *
	 *                        */
	{ 0x01, KEY_1 },
	{ 0x02, KEY_2 },
	{ 0x03, KEY_3 },
	{ 0x04, KEY_4 },
	{ 0x05, KEY_5 },
	{ 0x06, KEY_6 },
	{ 0x07, KEY_7 },
	{ 0x08, KEY_8 },
	{ 0x09, KEY_9 },

	/*  0x0a    0x00    0x17  *
	 * RECALL    0      MODE  *
	 *                        */
	{ 0x0a, KEY_AGAIN },
	{ 0x00, KEY_0 },
	{ 0x17, KEY_MODE },

	/*  0x14          0x10    *
	 * ASPECT      FULLSCREEN *
	 *                        */
	{ 0x14, KEY_SCREEN },
	{ 0x10, KEY_ZOOM },

	/*          0x0b          *
	 *           Up           *
	 *                        *
	 *  0x18    0x16    0x0c  *
	 *  Left     Ok     Right *
	 *                        *
	 *         0x015          *
	 *         Down           *
	 *                        */
	{ 0x0b, KEY_CHANNELUP },
	{ 0x18, KEY_VOLUMEDOWN },
	{ 0x16, KEY_OK },
	{ 0x0c, KEY_VOLUMEUP },
	{ 0x15, KEY_CHANNELDOWN },

	/*  0x11            0x0d  *
	 *  MUTE            INFO  *
	 *                        */
	{ 0x11, KEY_MUTE },
	{ 0x0d, KEY_INFO },

	/*  0x0f    0x1b    0x1a  *
	 * RECORD PLAY/PAUSE STOP *
	 *                        *
	 *  0x0e    0x1f    0x1e  *
	 *TELETEXT  AUDIO  SOURCE *
	 *           RED   YELLOW *
	 *                        */
	{ 0x0f, KEY_RECORD },
	{ 0x1b, KEY_PLAYPAUSE },
	{ 0x1a, KEY_STOP },
	{ 0x0e, KEY_TEXT },
	{ 0x1f, KEY_RED },
	{ 0x1e, KEY_YELLOW },

	/*  0x1d   0x13     0x19  *
	 * SLEEP  PREVIEW   DVB   *
	 *         GREEN    BLUE  *
	 *                        */
	{ 0x1d, KEY_SLEEP },
	{ 0x13, KEY_GREEN },
	{ 0x19, KEY_BLUE },

	/*  0x58           0x5c   *
	 * FREEZE        SNAPSHOT *
	 *                        */
	{ 0x58, KEY_SLOW },
	{ 0x5c, KEY_CAMERA },

};

static struct rc_keymap behold_map = {
	.map = {
		.scan    = behold,
		.size    = ARRAY_SIZE(behold),
		.ir_type = IR_TYPE_UNKNOWN,	/* Legacy IR type */
		.name    = RC_MAP_BEHOLD,
	}
};

static int __init init_rc_map_behold(void)
{
	return ir_register_map(&behold_map);
}

static void __exit exit_rc_map_behold(void)
{
	ir_unregister_map(&behold_map);
}

module_init(init_rc_map_behold)
module_exit(exit_rc_map_behold)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@redhat.com>");
