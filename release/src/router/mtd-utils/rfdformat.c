/*
 * rfdformat.c
 *
 * Copyright (C) 2005 Sean Young <sean@mess.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is very easy: just erase all the blocks and put the magic at
 * the beginning of each block.
 */

#define PROGRAM_NAME "rfdformat"
#define VERSION "$Revision 1.0 $"

#define _XOPEN_SOURCE 500 /* For pread/pwrite */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include <mtd/mtd-user.h>
#include <linux/types.h>

void display_help(void)
{
	printf("Usage: %s [OPTIONS] MTD-device\n"
			"Formats NOR flash for resident flash disk\n"
			"\n"
			"-h         --help               display this help and exit\n"
			"-V         --version            output version information and exit\n",
			PROGRAM_NAME);
	exit(0);
}

void display_version(void)
{
	printf("%s " VERSION "\n"
			"\n"
			"This is free software; see the source for copying conditions.  There is NO\n"
			"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n",
			PROGRAM_NAME);

	exit(0);
}

void process_options(int argc, char *argv[], const char **mtd_filename)
{
	int error = 0;

	for (;;) {
		int option_index = 0;
		static const char *short_options = "hV";
		static const struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "version", no_argument, 0, 'V', },
			{ NULL, 0, 0, 0 }
		};

		int c = getopt_long(argc, argv, short_options,
				long_options, &option_index);
		if (c == EOF)
			break;

		switch (c) {
			case 'h':
				display_help();
				break;
			case 'V':
				display_version();
				break;
			case '?':
				error = 1;
				break;
		}
	}

	if ((argc - optind) != 1 || error)
		display_help();

	*mtd_filename = argv[optind];
}

int main(int argc, char *argv[])
{
	static const uint8_t magic[] = { 0x93, 0x91 };
	int fd, block_count, i;
	struct mtd_info_user mtd_info;
	char buf[512];
	const char *mtd_filename;

	process_options(argc, argv, &mtd_filename);

	fd = open(mtd_filename, O_RDWR);
	if (fd == -1) {
		perror(mtd_filename);
		return 1;
	}

	if (ioctl(fd, MEMGETINFO, &mtd_info)) {
		perror(mtd_filename);
		close(fd);
		return 1;
	}

	if (mtd_info.type != MTD_NORFLASH) {
		fprintf(stderr, "%s: not NOR flash\n", mtd_filename);
		close(fd);
		return 2;
	}

	if (mtd_info.size > 32*1024*1024) {
		fprintf(stderr, "%s: flash larger than 32MiB not supported\n",
				mtd_filename);
		close(fd);
		return 2;
	}

	block_count = mtd_info.size / mtd_info.erasesize;

	if (block_count < 2) {
		fprintf(stderr, "%s: at least two erase units required\n",
				mtd_filename);
		close(fd);
		return 2;
	}

	for (i=0; i<block_count; i++) {
		struct erase_info_user erase_info;

		erase_info.start = i * mtd_info.erasesize;
		erase_info.length = mtd_info.erasesize;

		if (ioctl(fd, MEMERASE, &erase_info) != 0) {
			snprintf(buf, sizeof(buf), "%s: erase", mtd_filename);
			perror(buf);
			close(fd);
			return 2;
		}

		if (pwrite(fd, magic, sizeof(magic), i * mtd_info.erasesize)
				!= sizeof(magic)) {
			snprintf(buf, sizeof(buf), "%s: write", mtd_filename);
			perror(buf);
			close(fd);
			return 2;
		}
	}

	close(fd);

	return 0;
}
