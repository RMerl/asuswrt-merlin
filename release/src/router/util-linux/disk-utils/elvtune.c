/*
 *  elvtune.c - I/O elevator tuner
 *
 *  Copyright (C) 2000 Andrea Arcangeli <andrea@suse.de> SuSE
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *  This file may be redistributed under the terms of the GNU General
 *  Public License, version 2.
 */

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include "nls.h"
#include "blkdev.h"
#include "linux_version.h"

/* this has to match with the kernel structure */
/* current version for ac19 and 2.2.16 */
typedef struct blkelv_ioctl_arg_s {
	int queue_ID;
	int read_latency;
	int write_latency;
	int max_bomb_segments;
} blkelv_ioctl_arg_t;

static void
usage(void) {
	fprintf(stderr, "elvtune (%s)\n", PACKAGE_STRING);
	fprintf(stderr, _("usage:\n"));
	fprintf(stderr, "\telvtune [-r r_lat] [-w w_lat] [-b b_lat]"
		        " /dev/blkdev1 [/dev/blkdev2...]\n");
	fprintf(stderr, "\telvtune -h\n");
	fprintf(stderr, "\telvtune -v\n");
	fprintf(stderr, _("\tNOTE: elvtune only works with 2.4 kernels\n"));
	/* (ioctls exist in 2.2.16 - 2.5.57) */
}

static void
version(void) {
	fprintf(stderr, "elvtune (%s)\n", PACKAGE_STRING);
}

int
main(int argc, char * argv[]) {
	int read_value = 0xbeefbeef, write_value = 0xbeefbeef, bomb_value = 0xbeefbeef;
	int read_set, write_set, bomb_set, set;
	char * devname;
	int fd;
	blkelv_ioctl_arg_t elevator;

	read_set = write_set = bomb_set = set = 0;

	setlocale(LC_MESSAGES, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	for (;;) {
		int opt;

		opt = getopt(argc, argv, "r:w:b:hv");
		if (opt == -1)
			break;
		switch (opt) {
		case 'r':
			read_value = atoi(optarg);
			read_set = set = 1;
			break;
		case 'w':
			write_value = atoi(optarg);
			write_set = set = 1;
			break;
		case 'b':
			bomb_value = atoi(optarg);
			bomb_set = set = 1;
			break;

		case 'h':
			usage(), exit(0);
		case 'v':
			version(), exit(0);

		default:
			usage(), exit(1);
		}
	}

	if (optind >= argc)
		fprintf(stderr, _("missing blockdevice, use -h for help\n")), exit(1);

	while (optind < argc) {
		devname = argv[optind++];

		fd = open(devname, O_RDONLY|O_NONBLOCK);
		if (fd < 0) {
			perror("open");
			break;
		}

		/* mmj: If we get EINVAL it's not a 2.4 kernel, so warn about
		   that and exit. It should return ENOTTY however, so check for
		   that as well in case it gets corrected in the future */

		if (ioctl(fd, BLKELVGET, &elevator) < 0) {
			int errsv = errno;
			perror("ioctl get");
			if ((errsv == EINVAL || errsv == ENOTTY) &&
			    get_linux_version() >= KERNEL_VERSION(2,5,58)) {
				fprintf(stderr,
					_("\nelvtune is only useful on older "
					"kernels;\nfor 2.6 use IO scheduler "
					"sysfs tunables instead..\n"));
			}
			break;
		}

		if (set) {
			if (read_set)
				elevator.read_latency = read_value;
			if (write_set)
				elevator.write_latency = write_value;
			if (bomb_set)
				elevator.max_bomb_segments = bomb_value;

			if (ioctl(fd, BLKELVSET, &elevator) < 0) {
				perror("ioctl set");
				break;
			}
			if (ioctl(fd, BLKELVGET, &elevator) < 0) {
				perror("ioctl reget");
				break;
			}
		}

		printf("\n%s elevator ID\t\t%d\n", devname, elevator.queue_ID);
		printf("\tread_latency:\t\t%d\n", elevator.read_latency);
		printf("\twrite_latency:\t\t%d\n", elevator.write_latency);
		printf("\tmax_bomb_segments:\t%d\n\n", elevator.max_bomb_segments);

		if (close(fd) < 0) {
			perror("close");
			break;
		}
	}

	return 0;
}
