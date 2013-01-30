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
	RESERVE_CONFIG_BUFFER(flags, MAX_NR_KEYMAPS);

	/* When user accidentally runs "dumpkmap FILE"
	 * instead of "dumpkmap >FILE", we'd dump binary stuff to tty.
	 * Let's prevent it: */
	if (argv[1])
		bb_show_usage();
/*	bb_warn_ignoring_args(argv[1]);*/

	fd = get_console_fd_or_die();

	write(STDOUT_FILENO, "bkeymap", 7);

	/* Here we want to set everything to 0 except for indexes:
	 * [0-2] [4-6] [8-10] [12] */
	memset(flags, 0x00, MAX_NR_KEYMAPS);
	memset(flags, 0x01, 13);
	flags[3] = flags[7] = flags[11] = 0;

	/* dump flags */
	write(STDOUT_FILENO, flags, MAX_NR_KEYMAPS);

	for (i = 0; i < MAX_NR_KEYMAPS; i++) {
		if (flags[i] == 1) {
			for (j = 0; j < NR_KEYS; j++) {
				ke.kb_index = j;
				ke.kb_table = i;
				if (!ioctl_or_perror(fd, KDGKBENT, &ke,
						"ioctl failed with %s, %s, %p",
						(char *)&ke.kb_index,
						(char *)&ke.kb_table,
						&ke.kb_value)
				) {
					write(STDOUT_FILENO, (void*)&ke.kb_value, 2);
				}
			}
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP) {
		close(fd);
		RELEASE_CONFIG_BUFFER(flags);
	}
	return EXIT_SUCCESS;
}
