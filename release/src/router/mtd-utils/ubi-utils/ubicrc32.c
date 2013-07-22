/*
 * Copyright (c) International Business Machines Corp., 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Calculate CRC32 with UBI start value (0xFFFFFFFF) for a given binary image.
 *
 * Author: Oliver Lohmann
 */

#define PROGRAM_NAME    "ubicrc32"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <mtd/ubi-media.h>
#include <crc32.h>

#include "common.h"

#define BUFSIZE 4096

static const char doc[] = PROGRAM_NAME " version " VERSION
			 " - a tool to calculate CRC32 with UBI start value (0xFFFFFFFF)";

static const char optionsstr[] =
"-h, --help                    print help message\n"
"-V, --version                 print program version";

static const char usage[] =
"Usage: " PROGRAM_NAME " <file to calculate CRC32 for> [-h] [--help]";

static const struct option long_options[] = {
	{ .name = "help",      .has_arg = 0, .flag = NULL, .val = 'h' },
	{ .name = "version",   .has_arg = 0, .flag = NULL, .val = 'V' },
	{ NULL, 0, NULL, 0},
};

static int parse_opt(int argc, char * const argv[])
{
	while (1) {
		int key;

		key = getopt_long(argc, argv, "hV", long_options, NULL);
		if (key == -1)
			break;

		switch (key) {
		case 'h':
			printf("%s\n\n", doc);
			printf("%s\n\n", usage);
			printf("%s\n", optionsstr);
			exit(EXIT_SUCCESS);

		case 'V':
			common_print_version();
			exit(EXIT_SUCCESS);

		case ':':
			return errmsg("parameter is missing");

		default:
			fprintf(stderr, "Use -h for help\n");
			return -1;
		}
	}

	return 0;
}

int main(int argc, char * const argv[])
{
	int err = 0;
	uint32_t crc = UBI_CRC32_INIT;
	char buf[BUFSIZE];
	FILE *fp;

	if (argc > 1) {
		fp = fopen(argv[1], "r");
		if (!fp)
			return sys_errmsg("cannot open \"%s\"", argv[1]);
	} else
		fp = stdin;

	err = parse_opt(argc, argv);
	if (err)
		return err;

	while (!feof(fp)) {
		size_t read;

		read = fread(buf, 1, BUFSIZE, fp);
		if (ferror(fp)) {
			sys_errmsg("cannot read input file");
			err = -1;
			goto out_close;
		}
		crc = mtd_crc32(crc, buf, read);
	}

	printf("0x%08x\n", crc);

out_close:
	if (fp != stdin)
		fclose(fp);
	return err;
}
