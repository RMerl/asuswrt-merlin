/*
 * flushb.c --- This routine flushes the disk buffers for a disk
 *
 * Copyright 1997, 2000, by Theodore Ts'o.
 *
 * WARNING: use of flushb on some older 2.2 kernels on a heavily loaded
 * system will corrupt filesystems.  This program is not really useful
 * beyond for benchmarking scripts.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include "../misc/nls-enable.h"

/* For Linux, define BLKFLSBUF if necessary */
#if (!defined(BLKFLSBUF) && defined(__linux__))
#define BLKFLSBUF	_IO(0x12,97)	/* flush buffer cache */
#endif

const char *progname;

static void usage(void)
{
	fprintf(stderr, _("Usage: %s disk\n"), progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int	fd;

	progname = argv[0];
	if (argc != 2)
		usage();

	fd = open(argv[1], O_RDONLY, 0);
	if (fd < 0) {
		perror("open");
		exit(1);
	}
	/*
	 * Note: to reread the partition table, use the ioctl
	 * BLKRRPART instead of BLKFSLBUF.
	 */
#ifdef BLKFLSBUF
	if (ioctl(fd, BLKFLSBUF, 0) < 0) {
		perror("ioctl BLKFLSBUF");
		exit(1);
	}
	return 0;
#else
	fprintf(stderr,
		_("BLKFLSBUF ioctl not supported!  Can't flush buffers.\n"));
	return 1;
#endif
}
