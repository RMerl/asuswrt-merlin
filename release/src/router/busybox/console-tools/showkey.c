/* vi: set sw=4 ts=4: */
/*
 * shows keys pressed. inspired by kbd package
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <linux/kd.h>

// set raw tty mode
// also used by microcom
// libbb candidates?
static void xget1(int fd, struct termios *t, struct termios *oldt)
{
	tcgetattr(fd, oldt);
	*t = *oldt;
	cfmakeraw(t);
}

static int xset1(int fd, struct termios *tio, const char *device)
{
	int ret = tcsetattr(fd, TCSAFLUSH, tio);

	if (ret) {
		bb_perror_msg("can't tcsetattr for %s", device);
	}
	return ret;
}

/*
 * GLOBALS
 */
struct globals {
	int kbmode;
	struct termios tio, tio0;
};
#define G (*ptr_to_globals)
#define kbmode	(G.kbmode)
#define tio	(G.tio)
#define tio0	(G.tio0)
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)


static void signal_handler(int signo)
{
	// restore keyboard and console settings
	xset1(STDIN_FILENO, &tio0, "stdin");
	xioctl(STDIN_FILENO, KDSKBMODE, (void *)(ptrdiff_t)kbmode);
	// alarmed? -> exit 0
	exit(SIGALRM == signo);
}

int showkey_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int showkey_main(int argc UNUSED_PARAM, char **argv)
{
	enum {
		OPT_a = (1<<0),	// display the decimal/octal/hex values of the keys
		OPT_k = (1<<1),	// display only the interpreted keycodes (default)
		OPT_s = (1<<2),	// display only the raw scan-codes
	};

	// FIXME: aks are all mutually exclusive
	getopt32(argv, "aks");

	INIT_G();

	// get keyboard settings
	xioctl(STDIN_FILENO, KDGKBMODE, &kbmode);
	printf("kb mode was %s\n\nPress any keys. Program terminates %s\n\n",
		kbmode == K_RAW ? "RAW" :
			(kbmode == K_XLATE ? "XLATE" :
				(kbmode == K_MEDIUMRAW ? "MEDIUMRAW" :
					(kbmode == K_UNICODE ? "UNICODE" : "?UNKNOWN?")))
		, (option_mask32 & OPT_a) ? "when CTRL+D pressed" : "10s after last keypress"
	);
	// prepare for raw mode
	xget1(STDIN_FILENO, &tio, &tio0);
	// put stdin in raw mode
	xset1(STDIN_FILENO, &tio, "stdin");

	if (option_mask32 & OPT_a) {
		char c;
		// just read stdin char by char
		while (1 == safe_read(STDIN_FILENO, &c, 1)) {
			printf("%3d 0%03o 0x%02x\r\n", c, c, c);
			if (04 /*CTRL-D*/ == c)
				break;
		}
	} else {
		// we should exit on any signal
		bb_signals(BB_FATAL_SIGS, signal_handler);
		// set raw keyboard mode
		xioctl(STDIN_FILENO, KDSKBMODE, (void *)(ptrdiff_t)((option_mask32 & OPT_k) ? K_MEDIUMRAW : K_RAW));

		// read and show scancodes
		while (1) {
			char buf[18];
			int i, n;
			// setup 10s watchdog
			alarm(10);
			// read scancodes
			n = read(STDIN_FILENO, buf, sizeof(buf));
			i = 0;
			while (i < n) {
				char c = buf[i];
				// show raw scancodes ordered? ->
				if (option_mask32 & OPT_s) {
					printf("0x%02x ", buf[i++]);
				// show interpreted scancodes (default) ? ->
				} else {
					int kc;
					if (i+2 < n && (c & 0x7f) == 0
						&& (buf[i+1] & 0x80) != 0
						&& (buf[i+2] & 0x80) != 0) {
						kc = ((buf[i+1] & 0x7f) << 7) | (buf[i+2] & 0x7f);
						i += 3;
					} else {
						kc = (c & 0x7f);
						i++;
					}
					printf("keycode %3d %s", kc, (c & 0x80) ? "release" : "press");
				}
			}
			puts("\r");
		}
	}

	// cleanup
	signal_handler(SIGALRM);

	// should never be here!
	return EXIT_SUCCESS;
}
