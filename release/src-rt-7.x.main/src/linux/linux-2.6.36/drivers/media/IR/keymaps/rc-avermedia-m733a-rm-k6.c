/* avermedia-m733a-rm-k6.h - Keytable for avermedia_m733a_rm_k6 Remote Controller
 *
 * Copyright (c) 2010 by Herton Ronaldo Krzesinski <herton@mandriva.com.br>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <media/rc-map.h>

/*
 * Avermedia M733A with IR model RM-K6
 * This is the stock remote controller used with Positivo machines with M733A
 * Herton Ronaldo Krzesinski <herton@mandriva.com.br>
 */

static struct ir_scancode avermedia_m733a_rm_k6[] = {
	{ 0x0401, KEY_POWER2 },
	{ 0x0406, KEY_MUTE },
	{ 0x0408, KEY_MODE },     /* TV/FM */

	{ 0x0409, KEY_1 },
	{ 0x040a, KEY_2 },
	{ 0x040b, KEY_3 },
	{ 0x040c, KEY_4 },
	{ 0x040d, KEY_5 },
	{ 0x040e, KEY_6 },
	{ 0x040f, KEY_7 },
	{ 0x0410, KEY_8 },
	{ 0x0411, KEY_9 },
	{ 0x044c, KEY_DOT },      /* '.' */
	{ 0x0412, KEY_0 },
	{ 0x0407, KEY_REFRESH },  /* Refresh/Reload */

	{ 0x0413, KEY_AUDIO },
	{ 0x0440, KEY_SCREEN },   /* Full Screen toggle */
	{ 0x0441, KEY_HOME },
	{ 0x0442, KEY_BACK },
	{ 0x0447, KEY_UP },
	{ 0x0448, KEY_DOWN },
	{ 0x0449, KEY_LEFT },
	{ 0x044a, KEY_RIGHT },
	{ 0x044b, KEY_OK },
	{ 0x0404, KEY_VOLUMEUP },
	{ 0x0405, KEY_VOLUMEDOWN },
	{ 0x0402, KEY_CHANNELUP },
	{ 0x0403, KEY_CHANNELDOWN },

	{ 0x0443, KEY_RED },
	{ 0x0444, KEY_GREEN },
	{ 0x0445, KEY_YELLOW },
	{ 0x0446, KEY_BLUE },

	{ 0x0414, KEY_TEXT },
	{ 0x0415, KEY_EPG },
	{ 0x041a, KEY_TV2 },      /* PIP */
	{ 0x041b, KEY_MHP },      /* Snapshot */

	{ 0x0417, KEY_RECORD },
	{ 0x0416, KEY_PLAYPAUSE },
	{ 0x0418, KEY_STOP },
	{ 0x0419, KEY_PAUSE },

	{ 0x041f, KEY_PREVIOUS },
	{ 0x041c, KEY_REWIND },
	{ 0x041d, KEY_FORWARD },
	{ 0x041e, KEY_NEXT },
};

static struct rc_keymap avermedia_m733a_rm_k6_map = {
	.map = {
		.scan    = avermedia_m733a_rm_k6,
		.size    = ARRAY_SIZE(avermedia_m733a_rm_k6),
		.ir_type = IR_TYPE_NEC,
		.name    = RC_MAP_AVERMEDIA_M733A_RM_K6,
	}
};

static int __init init_rc_map_avermedia_m733a_rm_k6(void)
{
	return ir_register_map(&avermedia_m733a_rm_k6_map);
}

static void __exit exit_rc_map_avermedia_m733a_rm_k6(void)
{
	ir_unregister_map(&avermedia_m733a_rm_k6_map);
}

module_init(init_rc_map_avermedia_m733a_rm_k6)
module_exit(exit_rc_map_avermedia_m733a_rm_k6)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mauro Carvalho Chehab <mchehab@redhat.com>");
