/*
 * ismounted.c --- Check to see if the filesystem was mounted
 *
 * Copyright (C) 1995,1996,1997,1998,1999,2000,2008 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#ifdef HAVE_LINUX_FD_H
#include <linux/fd.h>
#endif
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#include "fsck.h"

/*
 * ext2fs_check_if_mounted flags
 */
#define MF_MOUNTED		1

#include "et/com_err.h"

static char *skip_over_blank(char *cp)
{
	while (*cp && isspace(*cp))
		cp++;
	return cp;
}

static char *skip_over_word(char *cp)
{
	while (*cp && !isspace(*cp))
		cp++;
	return cp;
}

static char *parse_word(char **buf)
{
	char *word, *next;

	word = *buf;
	if (*word == 0)
		return 0;

	word = skip_over_blank(word);
	next = skip_over_word(word);
	if (*next)
		*next++ = 0;
	*buf = next;
	return word;
}

/*
 * Helper function which checks a file in /etc/mtab format to see if a
 * filesystem is mounted.  Returns an error if the file doesn't exist
 * or can't be opened.
 */
static errcode_t check_mntent_file(const char *mtab_file, const char *file,
				   int *mount_flags)
{
#ifdef HAVE_MNTENT_H
	struct stat	st_buf;
	errcode_t	retval = 0;
	dev_t		file_dev=0, file_rdev=0;
	ino_t		file_ino=0;
	FILE 		*f;
	char		buf[1024], *device = 0, *mnt_dir = 0, *cp;

	*mount_flags = 0;
	if ((f = setmntent (mtab_file, "r")) == NULL)
		return errno;
	if (stat(file, &st_buf) == 0) {
		if (S_ISBLK(st_buf.st_mode)) {
#ifndef __GNU__ /* The GNU hurd is broken with respect to stat devices */
			file_rdev = st_buf.st_rdev;
#endif	/* __GNU__ */
		} else {
			file_dev = st_buf.st_dev;
			file_ino = st_buf.st_ino;
		}
	}
	while (1) {
		if (!fgets(buf, sizeof(buf), f)) {
			device = mnt_dir = 0;
			break;
		}
		buf[sizeof(buf)-1] = 0;

		cp = buf;
		device = parse_word(&cp);
		if (!device || *device == '#')
			return 0;	/* Ignore blank lines and comments */
		mnt_dir = parse_word(&cp);

		if (device[0] != '/')
			continue;

		if (strcmp(file, device) == 0)
			break;
		if (stat(device, &st_buf) == 0) {
			if (S_ISBLK(st_buf.st_mode)) {
#ifndef __GNU__
				if (file_rdev && (file_rdev == st_buf.st_rdev))
					break;
#endif	/* __GNU__ */
			} else {
				if (file_dev && ((file_dev == st_buf.st_dev) &&
						 (file_ino == st_buf.st_ino)))
					break;
			}
		}
	}

	if (mnt_dir == 0) {
#ifndef __GNU__ /* The GNU hurd is broken with respect to stat devices */
		/*
		 * Do an extra check to see if this is the root device.  We
		 * can't trust /etc/mtab, and /proc/mounts will only list
		 * /dev/root for the root filesystem.  Argh.  Instead we
		 * check if the given device has the same major/minor number
		 * as the device that the root directory is on.
		 */
		if (file_rdev && (stat("/", &st_buf) == 0) &&
		    (st_buf.st_dev == file_rdev))
			*mount_flags = MF_MOUNTED;
#endif	/* __GNU__ */
		goto errout;
	}
#ifndef __GNU__ /* The GNU hurd is deficient; what else is new? */
	/* Validate the entry in case /etc/mtab is out of date */
	/*
	 * We need to be paranoid, because some broken distributions
	 * (read: Slackware) don't initialize /etc/mtab before checking
	 * all of the non-root filesystems on the disk.
	 */
	if (stat(mnt_dir, &st_buf) < 0) {
		retval = errno;
		if (retval == ENOENT) {
#ifdef DEBUG
			printf("Bogus entry in %s!  (%s does not exist)\n",
			       mtab_file, mnt_dir);
#endif /* DEBUG */
			retval = 0;
		}
		goto errout;
	}
	if (file_rdev && (st_buf.st_dev != file_rdev)) {
#ifdef DEBUG
		printf("Bogus entry in %s!  (%s not mounted on %s)\n",
		       mtab_file, file, mnt_dir);
#endif /* DEBUG */
		goto errout;
	}
#endif /* __GNU__ */
	*mount_flags = MF_MOUNTED;

	retval = 0;
errout:
	endmntent (f);
	return retval;
#else /* !HAVE_MNTENT_H */
	return 0;
#endif /* HAVE_MNTENT_H */
}

int is_mounted(const char *file)
{
	errcode_t	retval;
	int		mount_flags = 0;

#ifdef __linux__
	retval = check_mntent_file("/proc/mounts", file, &mount_flags);
	if (retval)
		return 0;
	if (mount_flags)
		return 1;
#endif /* __linux__ */
	retval = check_mntent_file("/etc/mtab", file, &mount_flags);
	if (retval)
		return 0;
	return (mount_flags);
}

#ifdef DEBUG
int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s device\n", argv[0]);
		exit(1);
	}

	if (is_mounted(argv[1]))
		printf("\t%s is mounted.\n", argv[1]);
	exit(0);
}
#endif /* DEBUG */
