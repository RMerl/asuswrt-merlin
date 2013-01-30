/* vi: set sw=4 ts=4: */
/*
 * mountpoint implementation for busybox
 *
 * Copyright (C) 2005 Bernhard Reutner-Fischer
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * Based on sysvinit's mountpoint
 */

#include "libbb.h"

int mountpoint_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mountpoint_main(int argc UNUSED_PARAM, char **argv)
{
	struct stat st;
	const char *msg;
	char *arg;
	int rc, opt;

	opt_complementary = "=1"; /* must have one argument */
	opt = getopt32(argv, "qdxn");
#define OPT_q (1)
#define OPT_d (2)
#define OPT_x (4)
#define OPT_n (8)
	arg = argv[optind];
	msg = "%s";

	rc = (opt & OPT_x) ? stat(arg, &st) : lstat(arg, &st);
	if (rc != 0)
		goto err;

	if (opt & OPT_x) {
		if (S_ISBLK(st.st_mode)) {
			printf("%u:%u\n", major(st.st_rdev),
						minor(st.st_rdev));
			return EXIT_SUCCESS;
		}
		errno = 0; /* make perror_msg work as error_msg */
		msg = "%s: not a block device";
		goto err;
	}

	errno = ENOTDIR;
	if (S_ISDIR(st.st_mode)) {
		dev_t st_dev = st.st_dev;
		ino_t st_ino = st.st_ino;
		char *p = xasprintf("%s/..", arg);

		if (stat(p, &st) == 0) {
			//int is_mnt = (st_dev != st.st_dev) || (st_dev == st.st_dev && st_ino == st.st_ino);
			int is_not_mnt = (st_dev == st.st_dev) && (st_ino != st.st_ino);

			if (opt & OPT_d)
				printf("%u:%u\n", major(st_dev), minor(st_dev));
			if (opt & OPT_n) {
				const char *d = find_block_device(arg);
				/* name is undefined, but device is mounted -> anonymous superblock! */
				/* happens with btrfs */
				if (!d) {
					d = "UNKNOWN";
					/* TODO: iterate /proc/mounts, or /proc/self/mountinfo
					 * to find out the device name */
				}
				printf("%s %s\n", d, arg);
			}
			if (!(opt & (OPT_q | OPT_d | OPT_n)))
				printf("%s is %sa mountpoint\n", arg, is_not_mnt ? "not " : "");
			return is_not_mnt;
		}
		arg = p;
		/* else: stat had set errno, just fall through */
	}

 err:
	if (!(opt & OPT_q))
		bb_perror_msg(msg, arg);
	return EXIT_FAILURE;
}
