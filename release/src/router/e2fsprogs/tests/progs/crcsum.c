/*
 * crcsum.c
 *
 * Copyright (C) 2013 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <fcntl.h>

#include "et/com_err.h"
#include "ss/ss.h"
#include "ext2fs/ext2fs.h"


int main(int argc, char **argv)
{
	int		c;
	uint32_t	crc = ~0;
	uint32_t	(*csum_func)(uint32_t crc, unsigned char const *p,
				     size_t len);
	FILE		*f;

	csum_func = ext2fs_crc32c_le;

	while ((c = getopt (argc, argv, "h")) != EOF) {
		switch (c) {
		case 'h':
		default:
			com_err(argv[0], 0, "Usage: crcsum [file]\n");
			return 1;
		}
	}

	if (optind == argc)
		f = stdin;
	else {
		f = fopen(argv[optind], "r");
		if (!f) {
			com_err(argv[0], errno, "while trying to open %s\n",
				argv[optind]);
			exit(1);
		}
	}

	while (!feof(f)) {
		unsigned char buf[4096];

		int c = fread(buf, 1, sizeof(buf), f);

		if (c)
			crc = csum_func(crc, buf, c);
	}
	printf("%u\n", crc);
	return 0;
}
