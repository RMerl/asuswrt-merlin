/*
 * util.c --- utilities for the debugfs program
 *
 * Copyright (C) 1993, 1994 Theodore Ts'o.  This file may be
 * redistributed under the terms of the GNU Public License.
 *
 */

#define _XOPEN_SOURCE 600 /* needed for strptime */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif
#ifdef HAVE_OPTRESET
extern int optreset;		/* defined by BSD, but not others */
#endif

#include "debugfs.h"

/*
 * This function resets the libc getopt() function, which keeps
 * internal state.  Bad design!  Stupid libc API designers!  No
 * biscuit!
 *
 * BSD-derived getopt() functions require that optind be reset to 1 in
 * order to reset getopt() state.  This used to be generally accepted
 * way of resetting getopt().  However, glibc's getopt()
 * has additional getopt() state beyond optind, and requires that
 * optind be set zero to reset its state.  So the unfortunate state of
 * affairs is that BSD-derived versions of getopt() misbehave if
 * optind is set to 0 in order to reset getopt(), and glibc's getopt()
 * will core dump if optind is set 1 in order to reset getopt().
 *
 * More modern versions of BSD require that optreset be set to 1 in
 * order to reset getopt().   Sigh.  Standards, anyone?
 *
 * We hide the hair here.
 */
void reset_getopt(void)
{
#if defined(__GLIBC__) || defined(__linux__)
	optind = 0;
#else
	optind = 1;
#endif
#ifdef HAVE_OPTRESET
	optreset = 1;		/* Makes BSD getopt happy */
#endif
}

static const char *pager_search_list[] = { "pager", "more", "less", 0 };
static const char *pager_dir_list[] = { "/usr/bin", "/bin", 0 };

static const char *find_pager(char *buf)
{
	const char **i, **j;

	for (i = pager_search_list; *i; i++) {
		for (j = pager_dir_list; *j; j++) {
			sprintf(buf, "%s/%s", *j, *i);
			if (access(buf, X_OK) == 0)
				return(buf);
		}
	}
	return 0;
}

FILE *open_pager(void)
{
	FILE *outfile = 0;
	const char *pager = getenv("DEBUGFS_PAGER");
	char buf[80];

	signal(SIGPIPE, SIG_IGN);
	if (!isatty(1))
		return stdout;
	if (!pager)
		pager = getenv("PAGER");
	if (!pager)
		pager = find_pager(buf);
	if (!pager ||
	    (strcmp(pager, "__none__") == 0) ||
	    ((outfile = popen(pager, "w")) == 0))
		return stdout;
	return outfile;
}

void close_pager(FILE *stream)
{
	if (stream && stream != stdout) pclose(stream);
}

/*
 * This routine is used whenever a command needs to turn a string into
 * an inode.
 */
ext2_ino_t string_to_inode(char *str)
{
	ext2_ino_t	ino;
	int		len = strlen(str);
	char		*end;
	int		retval;

	/*
	 * If the string is of the form <ino>, then treat it as an
	 * inode number.
	 */
	if ((len > 2) && (str[0] == '<') && (str[len-1] == '>')) {
		ino = strtoul(str+1, &end, 0);
		if (*end=='>')
			return ino;
	}

	retval = ext2fs_namei(current_fs, root, cwd, str, &ino);
	if (retval) {
		com_err(str, retval, 0);
		return 0;
	}
	return ino;
}

/*
 * This routine returns 1 if the filesystem is not open, and prints an
 * error message to that effect.
 */
int check_fs_open(char *name)
{
	if (!current_fs) {
		com_err(name, 0, "Filesystem not open");
		return 1;
	}
	return 0;
}

/*
 * This routine returns 1 if a filesystem is open, and prints an
 * error message to that effect.
 */
int check_fs_not_open(char *name)
{
	if (current_fs) {
		com_err(name, 0,
			"Filesystem %s is still open.  Close it first.\n",
			current_fs->device_name);
		return 1;
	}
	return 0;
}

/*
 * This routine returns 1 if a filesystem is not opened read/write,
 * and prints an error message to that effect.
 */
int check_fs_read_write(char *name)
{
	if (!(current_fs->flags & EXT2_FLAG_RW)) {
		com_err(name, 0, "Filesystem opened read/only");
		return 1;
	}
	return 0;
}

/*
 * This routine returns 1 if a filesystem is doesn't have its inode
 * and block bitmaps loaded, and prints an error message to that
 * effect.
 */
int check_fs_bitmaps(char *name)
{
	if (!current_fs->block_map || !current_fs->inode_map) {
		com_err(name, 0, "Filesystem bitmaps not loaded");
		return 1;
	}
	return 0;
}

/*
 * This function takes a __u32 time value and converts it to a string,
 * using ctime
 */
char *time_to_string(__u32 cl)
{
	static int	do_gmt = -1;
	time_t		t = (time_t) cl;
	const char	*tz;

	if (do_gmt == -1) {
		/* The diet libc doesn't respect the TZ environemnt variable */
		tz = getenv("TZ");
		if (!tz)
			tz = "";
		do_gmt = !strcmp(tz, "GMT");
	}

	return asctime((do_gmt) ? gmtime(&t) : localtime(&t));
}

/*
 * Parse a string as a time.  Return ((time_t)-1) if the string
 * doesn't appear to be a sane time.
 */
extern time_t string_to_time(const char *arg)
{
	struct	tm	ts;
	time_t		ret;
	char *tmp;

	if (strcmp(arg, "now") == 0) {
		return time(0);
	}
	memset(&ts, 0, sizeof(ts));
#ifdef HAVE_STRPTIME
	strptime(arg, "%Y%m%d%H%M%S", &ts);
#else
	sscanf(arg, "%4d%2d%2d%2d%2d%2d", &ts.tm_year, &ts.tm_mon,
	       &ts.tm_mday, &ts.tm_hour, &ts.tm_min, &ts.tm_sec);
	ts.tm_year -= 1900;
	ts.tm_mon -= 1;
	if (ts.tm_year < 0 || ts.tm_mon < 0 || ts.tm_mon > 11 ||
	    ts.tm_mday < 0 || ts.tm_mday > 31 || ts.tm_hour > 23 ||
	    ts.tm_min > 59 || ts.tm_sec > 61)
		ts.tm_mday = 0;
#endif
	ts.tm_isdst = -1;
	ret = mktime(&ts);
	if (ts.tm_mday == 0 || ret == ((time_t) -1)) {
		/* Try it as an integer... */
		ret = strtoul(arg, &tmp, 0);
		if (*tmp)
			return ((time_t) -1);
	}
	return ret;
}

/*
 * This function will convert a string to an unsigned long, printing
 * an error message if it fails, and returning success or failure in err.
 */
unsigned long parse_ulong(const char *str, const char *cmd,
			  const char *descr, int *err)
{
	char		*tmp;
	unsigned long	ret;

	ret = strtoul(str, &tmp, 0);
	if (*tmp == 0) {
		if (err)
			*err = 0;
		return ret;
	}
	com_err(cmd, 0, "Bad %s - %s", descr, str);
	if (err)
		*err = 1;
	else
		exit(1);
	return 0;
}

/*
 * This function will convert a string to a block number.  It returns
 * 0 on success, 1 on failure.
 */
int strtoblk(const char *cmd, const char *str, blk_t *ret)
{
	blk_t	blk;
	int	err;

	blk = parse_ulong(str, cmd, "block number", &err);
	*ret = blk;
	if (err)
		com_err(cmd, 0, "Invalid block number: %s", str);
	return err;
}

/*
 * This is a common helper function used by the command processing
 * routines
 */
int common_args_process(int argc, char *argv[], int min_argc, int max_argc,
			const char *cmd, const char *usage, int flags)
{
	if (argc < min_argc || argc > max_argc) {
		com_err(argv[0], 0, "Usage: %s %s", cmd, usage);
		return 1;
	}
	if (flags & CHECK_FS_NOTOPEN) {
		if (check_fs_not_open(argv[0]))
			return 1;
	} else {
		if (check_fs_open(argv[0]))
			return 1;
	}
	if ((flags & CHECK_FS_RW) && check_fs_read_write(argv[0]))
		return 1;
	if ((flags & CHECK_FS_BITMAPS) && check_fs_bitmaps(argv[0]))
		return 1;
	return 0;
}

/*
 * This is a helper function used by do_stat, do_freei, do_seti, and
 * do_testi, etc.  Basically, any command which takes a single
 * argument which is a file/inode number specifier.
 */
int common_inode_args_process(int argc, char *argv[],
			      ext2_ino_t *inode, int flags)
{
	if (common_args_process(argc, argv, 2, 2, argv[0], "<file>", flags))
		return 1;

	*inode = string_to_inode(argv[1]);
	if (!*inode)
		return 1;
	return 0;
}

/*
 * This is a helper function used by do_freeb, do_setb, and do_testb
 */
int common_block_args_process(int argc, char *argv[],
			      blk_t *block, blk_t *count)
{
	int	err;

	if (common_args_process(argc, argv, 2, 3, argv[0],
				"<block> [count]", CHECK_FS_BITMAPS))
		return 1;

	if (strtoblk(argv[0], argv[1], block))
		return 1;
	if (*block == 0) {
		com_err(argv[0], 0, "Invalid block number 0");
		err = 1;
	}

	if (argc > 2) {
		*count = parse_ulong(argv[2], argv[0], "count", &err);
		if (err)
			return 1;
	}
	return 0;
}

int debugfs_read_inode_full(ext2_ino_t ino, struct ext2_inode * inode,
			const char *cmd, int bufsize)
{
	int retval;

	retval = ext2fs_read_inode_full(current_fs, ino, inode, bufsize);
	if (retval) {
		com_err(cmd, retval, "while reading inode %u", ino);
		return 1;
	}
	return 0;
}

int debugfs_read_inode(ext2_ino_t ino, struct ext2_inode * inode,
			const char *cmd)
{
	int retval;

	retval = ext2fs_read_inode(current_fs, ino, inode);
	if (retval) {
		com_err(cmd, retval, "while reading inode %u", ino);
		return 1;
	}
	return 0;
}

int debugfs_write_inode(ext2_ino_t ino, struct ext2_inode * inode,
			const char *cmd)
{
	int retval;

	retval = ext2fs_write_inode(current_fs, ino, inode);
	if (retval) {
		com_err(cmd, retval, "while writing inode %u", ino);
		return 1;
	}
	return 0;
}

int debugfs_write_new_inode(ext2_ino_t ino, struct ext2_inode * inode,
			    const char *cmd)
{
	int retval;

	retval = ext2fs_write_new_inode(current_fs, ino, inode);
	if (retval) {
		com_err(cmd, retval, "while creating inode %u", ino);
		return 1;
	}
	return 0;
}
