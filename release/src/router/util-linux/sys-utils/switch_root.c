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
#include "nls.h"

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#ifndef MNT_DETACH
#define MNT_DETACH       0x00000002	/* Just detach from the tree */
#endif

/* remove all files/directories below dirName -- don't cross mountpoints */
static int recursiveRemove(int fd)
{
	struct stat rb;
	DIR *dir;
	int rc = -1;
	int dfd;

	if (!(dir = fdopendir(fd))) {
		warn(_("failed to open directory"));
		goto done;
	}

	/* fdopendir() precludes us from continuing to use the input fd */
	dfd = dirfd(dir);

	if (fstat(dfd, &rb)) {
		warn(_("failed to stat directory"));
		goto done;
	}

	while(1) {
		struct dirent *d;

		errno = 0;
		if (!(d = readdir(dir))) {
			if (errno) {
				warn(_("failed to read directory"));
				goto done;
			}
			break;	/* end of directory */
		}

		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;

		if (d->d_type == DT_DIR) {
			struct stat sb;

			if (fstatat(dfd, d->d_name, &sb, AT_SYMLINK_NOFOLLOW)) {
				warn(_("failed to stat %s"), d->d_name);
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
			warn(_("failed to unlink %s"), d->d_name);
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
	const char *umounts[] = { "/dev", "/proc", "/sys", "/run", NULL };
	int i;
	int cfd;
	pid_t pid;
	struct stat newroot_stat, sb;

	if (stat(newroot, &newroot_stat) != 0) {
		warn(_("failed to stat directory %s"), newroot);
		return -1;
	}

	for (i = 0; umounts[i] != NULL; i++) {
		char newmount[PATH_MAX];

		snprintf(newmount, sizeof(newmount), "%s%s", newroot, umounts[i]);

		if ((stat(newmount, &sb) != 0) || (sb.st_dev != newroot_stat.st_dev)) {
			/* mount point seems to be mounted already or stat failed */
			umount2(umounts[i], MNT_DETACH);
			continue;
		}

		if (mount(umounts[i], newmount, NULL, MS_MOVE, NULL) < 0) {
			warn(_("failed to mount moving %s to %s"),
				umounts[i], newmount);
			warnx(_("forcing unmount of %s"), umounts[i]);
			umount2(umounts[i], MNT_FORCE);
		}
	}

	if (chdir(newroot)) {
		warn(_("failed to change directory to %s"), newroot);
		return -1;
	}

	cfd = open("/", O_RDONLY);

	if (mount(newroot, "/", NULL, MS_MOVE, NULL) < 0) {
		close(cfd);
		warn(_("failed to mount moving %s to /"), newroot);
		return -1;
	}

	if (chroot(".")) {
		close(cfd);
		warn(_("failed to change root"));
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

static void __attribute__((__noreturn__)) usage(FILE *output)
{
	fputs(USAGE_HEADER, output);
	fprintf(output, _(" %s [options] <newrootdir> <init> <args to init>\n"),
		program_invocation_short_name);
	fputs(USAGE_OPTIONS, output);
	fputs(USAGE_HELP, output);
	fputs(USAGE_VERSION, output);
	fprintf(output, USAGE_MAN_TAIL("switch_root(8)"));

	exit(output == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	char *newroot, *init, **initargs;

	if (argv[1] && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
		usage(stdout);
	if (argv[1] && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-V"))) {
		printf(UTIL_LINUX_VERSION);
		return EXIT_SUCCESS;
	}
	if (argc < 3)
		usage(stderr);

	newroot = argv[1];
	init = argv[2];
	initargs = &argv[2];

	if (!*newroot || !*init)
		usage(stderr);

	if (switchroot(newroot))
		errx(EXIT_FAILURE, _("failed. Sorry."));

	if (access(init, X_OK))
		warn(_("cannot access %s"), init);

	execv(init, initargs);
	err(EXIT_FAILURE, _("failed to execute %s"), init);
}

