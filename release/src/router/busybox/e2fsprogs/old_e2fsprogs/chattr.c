/* vi: set sw=4 ts=4: */
/*
 * chattr.c		- Change file attributes on an ext2 file system
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
 * 98/12/29	- Ignore symlinks when working recursively (G M Sipe)
 * 98/12/29	- Display version info only when -V specified (G M Sipe)
 */

#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "ext2fs/ext2_fs.h"

#ifdef __GNUC__
# define EXT2FS_ATTR(x) __attribute__(x)
#else
# define EXT2FS_ATTR(x)
#endif

#include "e2fsbb.h"
#include "e2p/e2p.h"

#define OPT_ADD 1
#define OPT_REM 2
#define OPT_SET 4
#define OPT_SET_VER 8
static int flags;
static int recursive;

static unsigned long version;

static unsigned long af;
static unsigned long rf;
static unsigned long sf;

struct flags_char {
	unsigned long flag;
	char optchar;
};

static const struct flags_char flags_array[] = {
	{ EXT2_NOATIME_FL,      'A' },
	{ EXT2_SYNC_FL,         'S' },
	{ EXT2_DIRSYNC_FL,      'D' },
	{ EXT2_APPEND_FL,       'a' },
	{ EXT2_COMPR_FL,        'c' },
	{ EXT2_NODUMP_FL,       'd' },
	{ EXT2_IMMUTABLE_FL,    'i' },
	{ EXT3_JOURNAL_DATA_FL, 'j' },
	{ EXT2_SECRM_FL,        's' },
	{ EXT2_UNRM_FL,         'u' },
	{ EXT2_NOTAIL_FL,       't' },
	{ EXT2_TOPDIR_FL,       'T' },
	{ 0, 0 }
};

static unsigned long get_flag(char c)
{
	const struct flags_char *fp;
	for (fp = flags_array; fp->flag; fp++)
		if (fp->optchar == c)
			return fp->flag;
	bb_show_usage();
	return 0;
}

static int decode_arg(char *arg)
{
	unsigned long *fl;
	char opt = *arg++;

	if (opt == '-') {
		flags |= OPT_REM;
		fl = &rf;
	} else if (opt == '+') {
		flags |= OPT_ADD;
		fl = &af;
	} else if (opt == '=') {
		flags |= OPT_SET;
		fl = &sf;
	} else
		return EOF;

	for (; *arg; ++arg)
		(*fl) |= get_flag(*arg);

	return 1;
}

static int chattr_dir_proc(const char *, struct dirent *, void *);

static void change_attributes(const char * name)
{
	unsigned long fsflags;
	struct stat st;

	if (lstat(name, &st) == -1) {
		bb_error_msg("stat %s failed", name);
		return;
	}
	if (S_ISLNK(st.st_mode) && recursive)
		return;

	/* Don't try to open device files, fifos etc.  We probably
	 * ought to display an error if the file was explicitly given
	 * on the command line (whether or not recursive was
	 * requested).  */
	if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode) && !S_ISDIR(st.st_mode))
		return;

	if (flags & OPT_SET_VER)
		if (fsetversion(name, version) == -1)
			bb_error_msg("setting version on %s", name);

	if (flags & OPT_SET) {
		fsflags = sf;
	} else {
		if (fgetflags(name, &fsflags) == -1) {
			bb_error_msg("reading flags on %s", name);
			goto skip_setflags;
		}
		if (flags & OPT_REM)
			fsflags &= ~rf;
		if (flags & OPT_ADD)
			fsflags |= af;
		if (!S_ISDIR(st.st_mode))
			fsflags &= ~EXT2_DIRSYNC_FL;
	}
	if (fsetflags(name, fsflags) == -1)
		bb_error_msg("setting flags on %s", name);

skip_setflags:
	if (S_ISDIR(st.st_mode) && recursive)
		iterate_on_dir(name, chattr_dir_proc, NULL);
}

static int chattr_dir_proc(const char *dir_name, struct dirent *de,
			   void *private EXT2FS_ATTR((unused)))
{
	/*if (strcmp(de->d_name, ".") || strcmp(de->d_name, "..")) {*/
	if (de->d_name[0] == '.'
	 && (!de->d_name[1] || (de->d_name[1] == '.' && !de->d_name[2]))
	) {
		char *path = concat_subpath_file(dir_name, de->d_name);
		if (path) {
			change_attributes(path);
			free(path);
		}
	}
	return 0;
}

int chattr_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chattr_main(int argc, char **argv)
{
	int i;
	char *arg;

	/* parse the args */
	for (i = 1; i < argc; ++i) {
		arg = argv[i];

		/* take care of -R and -v <version> */
		if (arg[0] == '-') {
			if (arg[1] == 'R' && arg[2] == '\0') {
				recursive = 1;
				continue;
			} else if (arg[1] == 'v' && arg[2] == '\0') {
				char *tmp;
				++i;
				if (i >= argc)
					bb_show_usage();
				version = strtol(argv[i], &tmp, 0);
				if (*tmp)
					bb_error_msg_and_die("bad version '%s'", arg);
				flags |= OPT_SET_VER;
				continue;
			}
		}

		if (decode_arg(arg) == EOF)
			break;
	}

	/* run sanity checks on all the arguments given us */
	if (i >= argc)
		bb_show_usage();
	if ((flags & OPT_SET) && ((flags & OPT_ADD) || (flags & OPT_REM)))
		bb_error_msg_and_die("= is incompatible with - and +");
	if ((rf & af) != 0)
		bb_error_msg_and_die("Can't set and unset a flag");
	if (!flags)
		bb_error_msg_and_die("Must use '-v', =, - or +");

	/* now run chattr on all the files passed to us */
	while (i < argc)
		change_attributes(argv[i++]);

	return EXIT_SUCCESS;
}
