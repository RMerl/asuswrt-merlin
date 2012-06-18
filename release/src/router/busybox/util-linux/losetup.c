/* vi: set sw=4 ts=4: */
/*
 * Mini losetup implementation for busybox
 *
 * Copyright (C) 2002  Matt Kraai.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

int losetup_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int losetup_main(int argc UNUSED_PARAM, char **argv)
{
	unsigned opt;
	int n;
	char *opt_o;
	unsigned long long offset = 0;
	enum {
		OPT_d = (1 << 0),
		OPT_o = (1 << 1),
		OPT_f = (1 << 2),
	};

	/* max 2 args, all opts are mutually exclusive */
	opt_complementary = "?2:d--of:o--df:f--do";
	opt = getopt32(argv, "do:f", &opt_o);
	argv += optind;

	if (opt == OPT_o)
		offset = xatoull(opt_o);

	if (opt == OPT_d) {
		/* -d BLOCKDEV */
		if (!argv[0] || argv[1])
			bb_show_usage();
		if (del_loop(argv[0]))
			bb_simple_perror_msg_and_die(argv[0]);
		return EXIT_SUCCESS;
	}

	if (argv[0]) {
		char *s;

		if (opt == OPT_f) /* -f should not have arguments */
			bb_show_usage();

		if (argv[1]) {
			/* [-o OFS] BLOCKDEV FILE */
			if (set_loop(&argv[0], argv[1], offset) < 0)
				bb_simple_perror_msg_and_die(argv[0]);
			return EXIT_SUCCESS;
		}
		/* [-o OFS] BLOCKDEV */
		s = query_loop(argv[0]);
		if (!s)
			bb_simple_perror_msg_and_die(argv[0]);
		printf("%s: %s\n", argv[0], s);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(s);
		return EXIT_SUCCESS;
	}

	/* [-o OFS|-f] with no params */
	n = 0;
	while (1) {
		char *s;
		char dev[LOOP_NAMESIZE];

		sprintf(dev, LOOP_FORMAT, n);
		s = query_loop(dev);
		n++;
		if (!s) {
			if (n > 9 && errno && errno != ENXIO)
				return EXIT_SUCCESS;
			if (opt == OPT_f) {
				puts(dev);
				return EXIT_SUCCESS;
			}
		} else {
			if (opt != OPT_f)
				printf("%s: %s\n", dev, s);
			if (ENABLE_FEATURE_CLEAN_UP)
				free(s);
		}
	}
	return EXIT_SUCCESS;
}
