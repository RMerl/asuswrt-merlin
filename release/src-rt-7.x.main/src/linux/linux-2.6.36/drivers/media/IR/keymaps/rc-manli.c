/* manli.h - Keytable for manli Remote Controller
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

/* Michael Tokarev <mjt@tls.msk.ru>
   http://www.corpit.ru/mjt/beholdTV/remote_control.jpg
   keytable is used by MANLI MTV00[0x0c] and BeholdTV 40[13] at
   least, and probably other cards too.
   The "ascii-art picture" below (in comments, first row
   is the keycode in hex, and subsequent row(s) shows
   the button labels (several variants when appropriate)
   helps to descide which keycodes to assign to the buttons.
 */

static struct ir_scancode manli[] = {

	/*  0x1c            0x12  *
	 * FUNCTION         POWER *
	 *   FM              (|)  *
	 *                        */
	{ 0x1c, KEY_RADIO },
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
	 * RECALL    0      +100  *
	 *                  PLUS  *
	 *                        */
	{ 0x0a, KEY_AGAIN },
	{ 0x00, KEY_0 },
	{ 0x17, KEY_DIGITS },

	/*  0x14            0x10  *
	 *  MENU            INFO  *
	 *  OSD                   */
	{ 0x14, KEY_MENU },
	{ 0x10, KEY_INFO },

	/*          0x0b          *
	 *           Up           *
	 *                        *
	 *  0x18    0x16    0x0c  *
	 *  Left     Ok     Right *
	 *                        *
	 *         0x015          *
	 *         Down           *
	 *                        */
	{ 0x0b, KEY_UP },
	{ 0x18, KEY_LEFT },
	{ 0x16, KEY_OK },
	{ 0x0c, KEY_RIGHT },
	{ 0x15, KEY_DOWN },

	/*  0x11            0x0d  *
	 *  TV/AV           MODE  *
	 *  SOURCE         STEREO *
	 *                        */
	{ 0x11, KEY_TV },
	{ 0x0d, KEY_MODE },

	/*  0x0f    0x1b    0x1a  *
	 *  AUDIO   Vol+    Chan+ *
	 *        TIMESHIFT???    *
	 *                        *
	 *  0x0e    0x1f    0x1e  *
	 *  SLEEP   Vol-    Chan- *
	 *                        */
	{ 0x0f, KEY_AUDIO },
	{ 0x1b, KEY_VOLUMEUP },
	{ 0x1a, KEY_CHANNELUP },
	{ 0x0e, KEY_TIME },
	{ 0x1f, KEY_VOLUMEDOWN },
	{ 0x1e, KEY_CHANNELDOWN },

	/*         0x13     0x19  *
	 *         MUTE   SNAPSHOT*
	 *                        */
	{ 0x13, KEY_MUTE },
	{ 0x19, KEY_CAMERA },

	/* 0x1d unused ? */
};

static struct rc_keymap manli_map = {
	.map = {
		.scan    = manli,
		.size    = ARRAY_SIZE(manli),
		.ir_type = IR_TYPE_UNKNOWN,	/* Legacy IR type */
		.name    = RC_MAP_MANLI,
	}
};

static int __init init_rc_map_manli(void)
{
	return ir_register_map(&manli_map);
}

static void __exit exit_rc_map_manli(void)
{
	ir_unregister_map(&manli_map);
}

module_init(init_rc_map_manli)
module_exit(exit_rc_map_manli)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@redhat.com>");
