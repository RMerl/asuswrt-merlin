/* vi: set sw=4 ts=4: */
/*
 * parse_num.c		- Parse the number of blocks
 *
 * Copyright (C) 2004,2005  Theodore Ts'o <tytso@mit.edu>
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

#include "e2p.h"

#include <stdlib.h>

unsigned long parse_num_blocks(const char *arg, int log_block_size)
{
	char *p;
	unsigned long long num;

	num = strtoull(arg, &p, 0);

	if (p[0] && p[1])
		return 0;

	switch (*p) {		/* Using fall-through logic */
	case 'T': case 't':
		num <<= 10;
	case 'G': case 'g':
		num <<= 10;
	case 'M': case 'm':
		num <<= 10;
	case 'K': case 'k':
		num >>= log_block_size;
		break;
	case 's':
		num >>= 1;
		break;
	case '\0':
		break;
	default:
		return 0;
	}
	return num;
}

#ifdef DEBUG
#include <unistd.h>
#include <stdio.h>

main(int argc, char **argv)
{
	unsigned long num;
	int log_block_size = 0;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s arg\n", argv[0]);
		exit(1);
	}

	num = parse_num_blocks(argv[1], log_block_size);

	printf("Parsed number: %lu\n", num);
	exit(0);
}
#endif
