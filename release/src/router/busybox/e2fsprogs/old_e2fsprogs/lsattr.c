/* vi: set sw=4 ts=4: */
/*
 * lsattr.c		- List file attributes on an ext2 file system
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU General
 * Public License
 */

/*
 * History:
 * 93/10/30	- Creation
 * 93/11/13	- Replace stat() calls by lstat() to avoid loops
 * 94/02/27	- Integrated in Ted's distribution
 * 98/12/29	- Display version info only when -V specified (G M Sipe)
 */

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "ext2fs/ext2_fs.h"
#include "e2fsbb.h"
#include "e2p/e2p.h"

#define OPT_RECUR 1
#define OPT_ALL 2
#define OPT_DIRS_OPT 4
#define OPT_PF_LONG 8
#define OPT_GENERATION 16
static int flags;

static void list_attributes(const char *name)
{
	unsigned long fsflags;
	unsigned long generation;

	if (fgetflags(name, &fsflags) == -1)
		goto read_err;
	if (flags & OPT_GENERATION) {
		if (fgetversion(name, &generation) == -1)
			goto read_err;
		printf("%5lu ", generation);
	}

	if (flags & OPT_PF_LONG) {
		printf("%-28s ", name);
		print_e2flags(stdout, fsflags, PFOPT_LONG);
		bb_putchar('\n');
	} else {
		print_e2flags(stdout, fsflags, 0);
		printf(" %s\n", name);
	}

	return;
read_err:
	bb_perror_msg("reading %s", name);
}

static int lsattr_dir_proc(const char *, struct dirent *, void *);

static void lsattr_args(const char *name)
{
	struct stat st;

	if (lstat(name, &st) == -1) {
		bb_perror_msg("stating %s", name);
	} else {
		if (S_ISDIR(st.st_mode) && !(flags & OPT_DIRS_OPT))
			iterate_on_dir(name, lsattr_dir_proc, NULL);
		else
			list_attributes(name);
	}
}

static int lsattr_dir_proc(const char *dir_name, struct dirent *de,
			   void *private)
{
	struct stat st;
	char *path;

	path = concat_path_file(dir_name, de->d_name);

	if (lstat(path, &st) == -1)
		bb_perror_msg(path);
	else {
		if (de->d_name[0] != '.' || (flags & OPT_ALL)) {
			list_attributes(path);
			if (S_ISDIR(st.st_mode) && (flags & OPT_RECUR) &&
			   (de->d_name[0] != '.' && (de->d_name[1] != '\0' ||
			   (de->d_name[1] != '.' && de->d_name[2] != '\0')))) {
				printf("\n%s:\n", path);
				iterate_on_dir(path, lsattr_dir_proc, NULL);
				bb_putchar('\n');
			}
		}
	}

	free(path);

	return 0;
}

int lsattr_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int lsattr_main(int argc, char **argv)
{
	int i;

	flags = getopt32(argv, "Radlv");

	if (optind > argc - 1)
		lsattr_args(".");
	else
		for (i = optind; i < argc; i++)
			lsattr_args(argv[i]);

	return EXIT_SUCCESS;
}
