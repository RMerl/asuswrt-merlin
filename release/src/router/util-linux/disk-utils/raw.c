/*
 * raw.c: User mode tool to bind and query raw character devices.
 *
 * Stephen Tweedie, 1999, 2000
 *
 * This file may be redistributed under the terms of the GNU General
 * Public License, version 2.
 * 
 * Copyright Red Hat Software, 1999, 2000
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <linux/raw.h>
#include <linux/major.h>
#include "nls.h"


#define RAWDEVDIR "/dev/raw/"
#define RAWDEVCTL RAWDEVDIR "rawctl"
/* deprecated */
#define RAWDEVCTL_OLD "/dev/rawctl"


#define RAW_NR_MINORS 8192

char *	progname;
int	do_query = 0;
int	do_query_all = 0;

int	master_fd;
int	raw_minor;

void open_raw_ctl(void);
static int query(int minor_raw, const char *raw_name, int quiet);
static int bind(int minor_raw, int block_major, int block_minor);


static void usage(int err)
{
	fprintf(stderr,
		_("Usage:\n"
		  "  %1$s %2$srawN <major> <minor>\n"
		  "  %1$s %2$srawN /dev/<blockdevice>\n"
		  "  %1$s -q %2$srawN\n"
		  "  %1$s -qa\n"),
		progname, RAWDEVDIR);
	exit(err);
}


int main(int argc, char *argv[])
{
	int  c;
	char * raw_name;
	char * block_name;
	int  err;
	int  block_major, block_minor;
	int  i, rc;

	struct stat statbuf;

	setlocale(LC_MESSAGES, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	progname = argv[0];

	while ((c = getopt(argc, argv, "ahq")) != -1) {
		switch (c) {
		case 'a':
			do_query_all = 1;
			break;
		case 'h':
			usage(0);
		case 'q':
			do_query = 1;
			break;
		default:
			usage(1);
		}
	}

	/*
	 * Check for, and open, the master raw device, /dev/raw
	 */

	open_raw_ctl();

	if (do_query_all) {
		if (optind < argc)
			usage(1);
		for (i = 1; i < RAW_NR_MINORS; i++)
			query(i, NULL, 1);
		exit(0);
	}

	/*
	 * It's a bind or a single query.  Either way we need a raw device.
	 */

	if (optind >= argc)
		usage(1);
	raw_name = argv[optind++];

	/*
	 * try to check the device name before stat(), because on systems with
	 * udev the raw0 causes a create udev event for char 162/0, which
	 * causes udev to *remove* /dev/rawctl
	 */
	rc = sscanf(raw_name, RAWDEVDIR "raw%d", &raw_minor);
	if (rc != 1)
		usage(1);

	if (raw_minor == 0) {
		fprintf (stderr,
			_("Device '%s' is the control raw device "
			"(use raw<N> where <N> is greater than zero)\n"),
			raw_name);
		exit(2);
	}

	if (do_query)
		return query(raw_minor, raw_name, 0);

	/*
	 * It's not a query, so we still have some parsing to do.  Have
	 * we been given a block device filename or a major/minor pair?
	 */

	switch (argc - optind) {
	case 1:
		block_name = argv[optind];
		err = stat(block_name, &statbuf);
		if (err) {
			fprintf (stderr,
				 _("Cannot locate block device '%s' (%m)\n"),
				 block_name);
			exit(2);
		}

		if (!S_ISBLK(statbuf.st_mode)) {
			fprintf (stderr, _("Device '%s' is not a block device\n"),
				 block_name);
			exit(2);
		}

		block_major = major(statbuf.st_rdev);
		block_minor = minor(statbuf.st_rdev);
		break;

	case 2:
		block_major = strtol(argv[optind], 0, 0);
		block_minor = strtol(argv[optind+1], 0, 0);
		break;

	default:
		block_major = block_minor = 0; /* just to keep gcc happy */
		usage(1);
	}

	return bind(raw_minor, block_major, block_minor);
}


void open_raw_ctl(void)
{
	int errsv;

	master_fd = open(RAWDEVCTL, O_RDWR, 0);
	if (master_fd < 0) {
		errsv = errno;
		master_fd = open(RAWDEVCTL_OLD, O_RDWR, 0);
		if (master_fd < 0) {
			fprintf (stderr,
				 _("Cannot open master raw device '%s' (%s)\n"),
				 RAWDEVCTL, strerror(errsv));
			exit(2);
		}
	}
}

static int query(int minor_raw, const char *raw_name, int quiet)
{
	struct raw_config_request rq;
	static int has_worked = 0;
	int err;

	if (raw_name) {
		struct stat statbuf;

		err = stat(raw_name, &statbuf);
		if (err) {
			fprintf (stderr, _("Cannot locate raw device '%s' (%m)\n"),
				 raw_name);
			exit(2);
		}

		if (!S_ISCHR(statbuf.st_mode)) {
			fprintf (stderr, _("Raw device '%s' is not a character dev\n"),
				 raw_name);
			exit(2);
		}
		if (major(statbuf.st_rdev) != RAW_MAJOR) {
			fprintf (stderr, _("Device '%s' is not a raw dev\n"),
				 raw_name);
			exit(2);
		}
		minor_raw = minor(statbuf.st_rdev);
	}

	rq.raw_minor = minor_raw;
	err = ioctl(master_fd, RAW_GETBIND, &rq);
	if (err < 0) {
		if (quiet && errno == ENODEV)
			return 3;
		if (has_worked && errno == EINVAL)
			return 0;
		fprintf (stderr,
			 _("Error querying raw device (%m)\n"));
		exit(3);
	}
	/* If one query has worked, mark that fact so that we don't
	 * report spurious fatal errors if raw(8) has been built to
	 * support more raw minor numbers than the kernel has. */
	has_worked = 1;
	if (quiet && !rq.block_major && !rq.block_minor)
		return 0;
	printf (_("%sraw%d:  bound to major %d, minor %d\n"),
		RAWDEVDIR, minor_raw, (int) rq.block_major, (int) rq.block_minor);
	return 0;
}

static int bind(int minor_raw, int block_major, int block_minor)
{
	struct raw_config_request rq;
	int err;

	rq.raw_minor   = minor_raw;
	rq.block_major = block_major;
	rq.block_minor = block_minor;
	err = ioctl(master_fd, RAW_SETBIND, &rq);
	if (err < 0) {
		fprintf (stderr,
			 _("Error setting raw device (%m)\n"));
		exit(3);
	}
	printf (_("%sraw%d:  bound to major %d, minor %d\n"),
		RAWDEVDIR, raw_minor, (int) rq.block_major, (int) rq.block_minor);
	return 0;
}

