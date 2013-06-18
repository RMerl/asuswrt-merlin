/*
 * percent.c		- Take percentage of a number
 *
 * Copyright (C) 2006  Theodore Ts'o <tytso@mit.edu>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
#include "e2p.h"

#include <stdlib.h>

/*
 * We work really hard to calculate this accurately, while avoiding
 * an overflow.  "Is there a hyphen in anal-retentive?"  :-)
 */
unsigned int e2p_percent(int percent, unsigned int base)
{
	unsigned int mask = ~((1 << (sizeof(unsigned int) - 1) * 8) - 1);

	if (!percent)
		return 0;
	if (100 % percent == 0)
		return base / (100 / percent);
	if (mask & base)
		return (base / 100) * percent;
	return base * percent / 100;
}

#ifdef DEBUG
#include <unistd.h>
#include <stdio.h>

main(int argc, char **argv)
{
	unsigned int base;
	int percent;
	char *p;
	int log_block_size = 0;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s percent base\n", argv[0]);
		exit(1);
	}

	percent = strtoul(argv[1], &p, 0);
	if (p[0] && p[1]) {
		fprintf(stderr, "Bad percent: %s\n", argv[1]);
		exit(1);
	}

	base = strtoul(argv[2], &p, 0);
	if (p[0] && p[1]) {
		fprintf(stderr, "Bad base: %s\n", argv[2]);
		exit(1);
	}

	printf("%d percent of %u is %u.\n", percent, base,
	       e2p_percent(percent, base));

	exit(0);
}
#endif
