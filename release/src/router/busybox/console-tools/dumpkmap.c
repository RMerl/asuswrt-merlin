/* vi: set sw=4 ts=4: */
/*
 * Mini dumpkmap implementation for busybox
 *
 * Copyright (C) Arne Bernin <arne@matrix.loopback.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 */
/* no options, no getopt */

//usage:#define dumpkmap_trivial_usage
//usage:       "> keymap"
//usage:#define dumpkmap_full_usage "\n\n"
//usage:       "Print a binary keyboard translation table to stdout"
//usage:
//usage:#define dumpkmap_example_usage
//usage:       "$ dumpkmap > keymap\n"

#include "libbb.h"
#include "common_bufsiz.h"

/* From <linux/kd.h> */
struct kbentry {
	unsigned char kb_table;
	unsigned char kb_index;
	unsigned short kb_value;
};
#define KDGKBENT 0x4B46  /* gets one entry in translation table */

/* From <linux/keyboard.h> */
#define NR_KEYS 128
#define MAX_NR_KEYMAPS 256

int dumpkmap_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dumpkmap_main(int argc UNUSED_PARAM, char **argv)
{
	struct kbentry ke;
	int i, j, fd;
#define flags bb_common_bufsiz1
	setup_common_bufsiz();

	/* When user accidentally runs "dumpkmap FILE"
	 * instead of "dumpkmap >FILE", we'd dump binary stuff to tty.
	 * Let's prevent it:
	 */
	if (argv[1])
		bb_show_usage();
/*	bb_warn_ignoring_args(argv[1]);*/

	fd = get_console_fd_or_die();

#if 0
	write(STDOUT_FILENO, "bkeymap", 7);
	/* Here we want to set everything to 0 except for indexes:
	 * [0-2] [4-6] [8-10] [12]
	 */
	/*memset(flags, 0x00, MAX_NR_KEYMAPS); - already is */
	memset(flags, 0x01, 13);
	flags[3] = flags[7] = flags[11] = 0;
	/* dump flags */
	write(STDOUT_FILENO, flags, MAX_NR_KEYMAPS);
#define flags7 flags
#else
	/* Same effect */
	/*                     0 1 2 3 4 5 6 7 8 9 a b c=12 */
	memcpy(flags, "bkeymap\1\1\1\0\1\1\1\0\1\1\1\0\1",
	/* Can use sizeof, or sizeof-1. sizeof is even, using that */
	/****/ sizeof("bkeymap\1\1\1\0\1\1\1\0\1\1\1\0\1")
	);
	write(STDOUT_FILENO, flags, 7 + MAX_NR_KEYMAPS);
#define flags7 (flags + 7)
#endif

	for (i = 0; i < 13; i++) {
		if (flags7[i]) {
			for (j = 0; j < NR_KEYS; j++) {
				ke.kb_index = j;
				ke.kb_table = i;
				if (!ioctl_or_perror(fd, KDGKBENT, &ke,
						"ioctl(KDGKBENT{%d,%d}) failed",
						j, i)
				) {
					write(STDOUT_FILENO, &ke.kb_value, 2);
				}
			}
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP) {
		close(fd);
	}
	return EXIT_SUCCESS;
}
