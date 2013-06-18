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

#define _LARGEFILE64_SOURCE

#include "config.h"
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/param.h>
#include <sys/stat.h>
#include "ext2fs/ext2_fs.h"

#ifdef __GNUC__
#define EXT2FS_ATTR(x) __attribute__(x)
#else
#define EXT2FS_ATTR(x)
#endif

#ifndef S_ISLNK			/* So we can compile even with gcc-warn */
# ifdef __S_IFLNK
#  define S_ISLNK(mode)	 __S_ISTYPE((mode), __S_IFLNK)
# else
#  define S_ISLNK(mode)  0
# endif
#endif

#include "et/com_err.h"
#include "e2p/e2p.h"

#include "../version.h"
#include "nls-enable.h"

static const char * program_name = "chattr";

static int add;
static int rem;
static int set;
static int set_version;

static unsigned long version;

static int recursive;
static int verbose;
static int silent;

static unsigned long af;
static unsigned long rf;
static unsigned long sf;

#ifdef _LFS64_LARGEFILE
#define LSTAT		lstat64
#define STRUCT_STAT	struct stat64
#else
#define LSTAT		lstat
#define STRUCT_STAT	struct stat
#endif

static void usage(void)
{
	fprintf(stderr,
		_("Usage: %s [-RVf] [-+=AaCcDdeijsSu] [-v version] files...\n"),
		program_name);
	exit(1);
}

struct flags_char {
	unsigned long	flag;
	char 		optchar;
};

static const struct flags_char flags_array[] = {
	{ EXT2_NOATIME_FL, 'A' },
	{ EXT2_SYNC_FL, 'S' },
	{ EXT2_DIRSYNC_FL, 'D' },
	{ EXT2_APPEND_FL, 'a' },
	{ EXT2_COMPR_FL, 'c' },
	{ EXT2_NODUMP_FL, 'd' },
	{ EXT4_EXTENTS_FL, 'e'},
	{ EXT2_IMMUTABLE_FL, 'i' },
	{ EXT3_JOURNAL_DATA_FL, 'j' },
	{ EXT2_SECRM_FL, 's' },
	{ EXT2_UNRM_FL, 'u' },
	{ EXT2_NOTAIL_FL, 't' },
	{ EXT2_TOPDIR_FL, 'T' },
	{ FS_NOCOW_FL, 'C' },
	{ 0, 0 }
};

static unsigned long get_flag(char c)
{
	const struct flags_char *fp;

	for (fp = flags_array; fp->flag != 0; fp++) {
		if (fp->optchar == c)
			return fp->flag;
	}
	return 0;
}


static int decode_arg (int * i, int argc, char ** argv)
{
	char * p;
	char * tmp;
	unsigned long fl;

	switch (argv[*i][0])
	{
	case '-':
		for (p = &argv[*i][1]; *p; p++) {
			if (*p == 'R') {
				recursive = 1;
				continue;
			}
			if (*p == 'V') {
				verbose = 1;
				continue;
			}
			if (*p == 'f') {
				silent = 1;
				continue;
			}
			if (*p == 'v') {
				(*i)++;
				if (*i >= argc)
					usage ();
				version = strtol (argv[*i], &tmp, 0);
				if (*tmp) {
					com_err (program_name, 0,
						 _("bad version - %s\n"),
						 argv[*i]);
					usage ();
				}
				set_version = 1;
				continue;
			}
			if ((fl = get_flag(*p)) == 0)
				usage();
			rf |= fl;
			rem = 1;
		}
		break;
	case '+':
		add = 1;
		for (p = &argv[*i][1]; *p; p++) {
			if ((fl = get_flag(*p)) == 0)
				usage();
			af |= fl;
		}
		break;
	case '=':
		set = 1;
		for (p = &argv[*i][1]; *p; p++) {
			if ((fl = get_flag(*p)) == 0)
				usage();
			sf |= fl;
		}
		break;
	default:
		return EOF;
		break;
	}
	return 1;
}

static int chattr_dir_proc(const char *, struct dirent *, void *);

static int change_attributes(const char * name)
{
	unsigned long flags;
	STRUCT_STAT	st;
	int extent_file = 0;

	if (LSTAT (name, &st) == -1) {
		if (!silent)
			com_err (program_name, errno,
				 _("while trying to stat %s"), name);
		return -1;
	}

	if (fgetflags(name, &flags) == -1) {
		if (!silent)
			com_err(program_name, errno,
					_("while reading flags on %s"), name);
		return -1;
	}
	if (flags & EXT4_EXTENTS_FL)
		extent_file = 1;
	if (set) {
		if (extent_file && !(sf & EXT4_EXTENTS_FL)) {
			if (!silent)
				com_err(program_name, 0,
				_("Clearing extent flag not supported on %s"),
					name);
			return -1;
		}
		if (verbose) {
			printf (_("Flags of %s set as "), name);
			print_flags (stdout, sf, 0);
			printf ("\n");
		}
		if (fsetflags (name, sf) == -1)
			perror (name);
	} else {
		if (rem)
			flags &= ~rf;
		if (add)
			flags |= af;
		if (extent_file && !(flags & EXT4_EXTENTS_FL)) {
			if (!silent)
				com_err(program_name, 0,
				_("Clearing extent flag not supported on %s"),
					name);
			return -1;
		}
		if (verbose) {
			printf(_("Flags of %s set as "), name);
			print_flags(stdout, flags, 0);
			printf("\n");
		}
		if (!S_ISDIR(st.st_mode))
			flags &= ~EXT2_DIRSYNC_FL;
		if (fsetflags(name, flags) == -1) {
			if (!silent) {
				com_err(program_name, errno,
						_("while setting flags on %s"),
						name);
			}
			return -1;
		}
	}
	if (set_version) {
		if (verbose)
			printf (_("Version of %s set as %lu\n"), name, version);
		if (fsetversion (name, version) == -1) {
			if (!silent)
				com_err (program_name, errno,
					 _("while setting version on %s"),
					 name);
			return -1;
		}
	}
	if (S_ISDIR(st.st_mode) && recursive)
		return iterate_on_dir (name, chattr_dir_proc, NULL);
	return 0;
}

static int chattr_dir_proc (const char * dir_name, struct dirent * de,
			    void * private EXT2FS_ATTR((unused)))
{
	int ret = 0;

	if (strcmp (de->d_name, ".") && strcmp (de->d_name, "..")) {
	        char *path;

		path = malloc(strlen (dir_name) + 1 + strlen (de->d_name) + 1);
		if (!path) {
			fprintf(stderr, _("Couldn't allocate path variable "
					  "in chattr_dir_proc"));
			return -1;
		}
		sprintf(path, "%s/%s", dir_name, de->d_name);
		ret = change_attributes(path);
		free(path);
	}
	return ret;
}

int main (int argc, char ** argv)
{
	int i, j;
	int end_arg = 0;
	int err, retval = 0;

#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");
	bindtextdomain(NLS_CAT_NAME, LOCALEDIR);
	textdomain(NLS_CAT_NAME);
	set_com_err_gettext(gettext);
#endif
	if (argc && *argv)
		program_name = *argv;
	i = 1;
	while (i < argc && !end_arg) {
		/* '--' arg should end option processing */
		if (strcmp(argv[i], "--") == 0) {
			i++;
			end_arg = 1;
		} else if (decode_arg (&i, argc, argv) == EOF)
			end_arg = 1;
		else
			i++;
	}
	if (i >= argc)
		usage ();
	if (set && (add || rem)) {
		fputs(_("= is incompatible with - and +\n"), stderr);
		exit (1);
	}
	if ((rf & af) != 0) {
		fputs("Can't both set and unset same flag.\n", stderr);
		exit (1);
	}
	if (!(add || rem || set || set_version)) {
		fputs(_("Must use '-v', =, - or +\n"), stderr);
		exit (1);
	}
	if (verbose)
		fprintf (stderr, "chattr %s (%s)\n",
			 E2FSPROGS_VERSION, E2FSPROGS_DATE);
	for (j = i; j < argc; j++) {
		err = change_attributes (argv[j]);
		if (err)
			retval = 1;
	}
	exit(retval);
}
