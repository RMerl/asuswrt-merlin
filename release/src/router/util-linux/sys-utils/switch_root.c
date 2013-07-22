/*
 * switchroot.c - switch to new root directory and start init.
 *
 * Copyright 2002-2009 Red Hat, Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Peter Jones <pjones@redhat.com>
 *	Jeremy Katz <katzj@redhat.com>
 */
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include "c.h"

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

/* remove all files/directories below dirName -- don't cross mountpoints */
static int recursiveRemove(int fd)
{
	struct stat rb;
	DIR *dir;
	int rc = -1;
	int dfd;

	if (!(dir = fdopendir(fd))) {
		warn("failed to open directory");
		goto done;
	}

	/* fdopendir() precludes us from continuing to use the input fd */
	dfd = dirfd(dir);

	if (fstat(dfd, &rb)) {
		warn("failed to stat directory");
		goto done;
	}

	while(1) {
		struct dirent *d;

		errno = 0;
		if (!(d = readdir(dir))) {
			if (errno) {
				warn("failed to read directory");
				goto done;
			}
			break;	/* end of directory */
		}

		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;

		if (d->d_type == DT_DIR) {
			struct stat sb;

			if (fstatat(dfd, d->d_name, &sb, AT_SYMLINK_NOFOLLOW)) {
				warn("failed to stat %s", d->d_name);
				continue;
			}

			/* remove subdirectories if device is same as dir */
			if (sb.st_dev == rb.st_dev) {
				int cfd;

				cfd = openat(dfd, d->d_name, O_RDONLY);
				if (cfd >= 0) {
					recursiveRemove(cfd);
					close(cfd);
				}
			} else
				continue;
		}

		if (unlinkat(dfd, d->d_name,
			     d->d_type == DT_DIR ? AT_REMOVEDIR : 0))
			warn("failed to unlink %s", d->d_name);
	}

	rc = 0;	/* success */

done:
	if (dir)
		closedir(dir);
	return rc;
}

static int switchroot(const char *newroot)
{
	/*  Don't try to unmount the old "/", there's no way to do it. */
	const char *umounts[] = { "/dev", "/proc", "/sys", NULL };
	int i;
	int cfd;
	pid_t pid;

	for (i = 0; umounts[i] != NULL; i++) {
		char newmount[PATH_MAX];

		snprintf(newmount, sizeof(newmount), "%s%s", newroot, umounts[i]);

		if (mount(umounts[i], newmount, NULL, MS_MOVE, NULL) < 0) {
			warn("failed to mount moving %s to %s",
				umounts[i], newmount);
			warnx("forcing unmount of %s", umounts[i]);
			umount2(umounts[i], MNT_FORCE);
		}
	}

	if (chdir(newroot)) {
		warn("failed to change directory to %s", newroot);
		return -1;
	}

	cfd = open("/", O_RDONLY);

	if (mount(newroot, "/", NULL, MS_MOVE, NULL) < 0) {
		warn("failed to mount moving %s to /", newroot);
		return -1;
	}

	if (chroot(".")) {
		warn("failed to change root");
		return -1;
	}

	if (cfd >= 0) {
		pid = fork();
		if (pid <= 0) {
			recursiveRemove(cfd);
			if (pid == 0)
				exit(EXIT_SUCCESS);
		}
		close(cfd);
	}
	return 0;
}

static void usage(FILE *output)
{
	fprintf(output, "usage: %s <newrootdir> <init> <args to init>\n",
			program_invocation_short_name);
	exit(output == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void version(void)
{
	fprintf(stdout,  "%s from %s\n", program_invocation_short_name,
			PACKAGE_STRING);
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	char *newroot, *init, **initargs;

	if (argv[1] && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
		usage(stdout);
	if (argv[1] && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V")))
		version();
	if (argc < 3)
		usage(stderr);

	newroot = argv[1];
	init = argv[2];
	initargs = &argv[2];

	if (!*newroot || !*init)
		usage(stderr);

	if (switchroot(newroot))
		errx(EXIT_FAILURE, "failed. Sorry.");

	if (access(init, X_OK))
		warn("cannot access %s", init);

	execv(init, initargs);
	err(EXIT_FAILURE, "failed to execute %s", init);
}

