/* vi: set sw=4 ts=4: */
/*
 * Mini kbd_mode implementation for busybox
 *
 * Copyright (C) 2007 Loic Grenie <loic.grenie@gmail.com>
 *   written using Andries Brouwer <aeb@cwi.nl>'s kbd_mode from
 *   console-utils v0.2.3, licensed under GNU GPLv2
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define kbd_mode_trivial_usage
//usage:       "[-a|k|s|u] [-C TTY]"
//usage:#define kbd_mode_full_usage "\n\n"
//usage:       "Report or set the keyboard mode\n"
//usage:     "\n	-a	Default (ASCII)"
//usage:     "\n	-k	Medium-raw (keyboard)"
//usage:     "\n	-s	Raw (scancode)"
//usage:     "\n	-u	Unicode (utf-8)"
//usage:     "\n	-C TTY	Affect TTY instead of /dev/tty"

#include "libbb.h"
#include <linux/kd.h>

int kbd_mode_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int kbd_mode_main(int argc UNUSED_PARAM, char **argv)
{
	enum {
		SCANCODE  = (1 << 0),
		ASCII     = (1 << 1),
		MEDIUMRAW = (1 << 2),
		UNICODE   = (1 << 3),
	};
	int fd;
	unsigned opt;
	const char *tty_name = CURRENT_TTY;

	opt = getopt32(argv, "sakuC:", &tty_name);
	fd = xopen_nonblocking(tty_name);
	opt &= 0xf; /* clear -C bit, see (*) */

	if (!opt) { /* print current setting */
		const char *mode = "unknown";
		int m;

		xioctl(fd, KDGKBMODE, &m);
		if (m == K_RAW)
			mode = "raw (scancode)";
		else if (m == K_XLATE)
			mode = "default (ASCII)";
		else if (m == K_MEDIUMRAW)
			mode = "mediumraw (keycode)";
		else if (m == K_UNICODE)
			mode = "Unicode (UTF-8)";
		printf("The keyboard is in %s mode\n", mode);
	} else {
		/* here we depend on specific bits assigned to options (*) */
		opt = opt & UNICODE ? 3 : opt >> 1;
		/* double cast prevents warnings about widening conversion */
		xioctl(fd, KDSKBMODE, (void*)(ptrdiff_t)opt);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);
	return EXIT_SUCCESS;
}
