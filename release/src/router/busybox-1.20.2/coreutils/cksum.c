/* vi: set sw=4 ts=4: */
/*
 * cksum - calculate the CRC32 checksum of a file
 *
 * Copyright (C) 2006 by Rob Sullivan, with ideas from code by Walter Harms
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define cksum_trivial_usage
//usage:       "FILES..."
//usage:#define cksum_full_usage "\n\n"
//usage:       "Calculate the CRC32 checksums of FILES"

#include "libbb.h"

/* This is a NOEXEC applet. Be very careful! */

int cksum_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cksum_main(int argc UNUSED_PARAM, char **argv)
{
	uint32_t *crc32_table = crc32_filltable(NULL, 1);
	uint32_t crc;
	off_t length, filesize;
	int bytes_read;
	int exit_code = EXIT_SUCCESS;

#if ENABLE_DESKTOP
	getopt32(argv, ""); /* coreutils 6.9 compat */
	argv += optind;
#else
	argv++;
#endif

	do {
		int fd = open_or_warn_stdin(*argv ? *argv : bb_msg_standard_input);

		if (fd < 0) {
			exit_code = EXIT_FAILURE;
			continue;
		}
		crc = 0;
		length = 0;

#define read_buf bb_common_bufsiz1
		while ((bytes_read = safe_read(fd, read_buf, sizeof(read_buf))) > 0) {
			length += bytes_read;
			crc = crc32_block_endian1(crc, read_buf, bytes_read, crc32_table);
		}
		close(fd);

		filesize = length;

		while (length) {
			crc = (crc << 8) ^ crc32_table[(uint8_t)(crc >> 24) ^ (uint8_t)length];
			/* must ensure that shift is unsigned! */
			if (sizeof(length) <= sizeof(unsigned))
				length = (unsigned)length >> 8;
			else if (sizeof(length) <= sizeof(unsigned long))
				length = (unsigned long)length >> 8;
			else
				length = (unsigned long long)length >> 8;
		}
		crc = ~crc;

		printf((*argv ? "%"PRIu32" %"OFF_FMT"i %s\n" : "%"PRIu32" %"OFF_FMT"i\n"),
				crc, filesize, *argv);
	} while (*argv && *++argv);

	fflush_stdout_and_exit(exit_code);
}
