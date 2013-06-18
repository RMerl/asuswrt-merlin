/*
 * parse_num.c		- Parse the number of blocks
 *
 * Copyright (C) 2004,2005  Theodore Ts'o <tytso@mit.edu>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#include "config.h"
#include "e2p.h"

#include <stdlib.h>

unsigned long long parse_num_blocks2(const char *arg, int log_block_size)
{
	char *p;
	unsigned long long num;

	num = strtoull(arg, &p, 0);

	if (p[0] && p[1])
		return 0;

	switch (*p) {		/* Using fall-through logic */
	case 'T': case 't':
		num <<= 10;
		/* fallthrough */
	case 'G': case 'g':
		num <<= 10;
		/* fallthrough */
	case 'M': case 'm':
		num <<= 10;
		/* fallthrough */
	case 'K': case 'k':
		if (log_block_size < 0)
			num <<= 10;
		else
			num >>= log_block_size;
		break;
	case 's':
		if (log_block_size < 0)
			num << 1;
		else
			num >>= (1+log_block_size);
		break;
	case '\0':
		break;
	default:
		return 0;
	}
	return num;
}

unsigned long parse_num_blocks(const char *arg, int log_block_size)
{
	return parse_num_blocks2(arg, log_block_size);
}

#ifdef DEBUG
#include <unistd.h>
#include <stdio.h>

main(int argc, char **argv)
{
	unsigned long num;
	int log_block_size = 0;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "Usage: %s arg [log_block_size]\n", argv[0]);
		exit(1);
	}

	if (argc == 3) {
		char *p;

		log_block_size = strtol(argv[2], &p, 0);
		if (*p) {
			fprintf(stderr, "Bad log_block_size: %s\n", argv[2]);
			exit(1);
		}
	}

	num = parse_num_blocks(argv[1], log_block_size);

	printf("Parsed number: %lu\n", num);
	exit(0);
}
#endif
