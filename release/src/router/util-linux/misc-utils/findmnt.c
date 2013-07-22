/*
 * findmnt(8)
 *
 * Copyright (C) 2010,2011 Red Hat, Inc. All rights reserved.
 * Written by Karel Zak <kzak@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <termios.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <assert.h>
#include <poll.h>

#include <libmount.h>

#include "pathnames.h"
#include "nls.h"
#include "c.h"
#include "tt.h"
#include "strutils.h"

/* flags */
enum {
	FL_EVALUATE	= (1 << 1),
	FL_CANONICALIZE = (1 << 2),
	FL_FIRSTONLY	= (1 << 3),
	FL_INVERT	= (1 << 4),
	FL_NOSWAPMATCH	= (1 << 6),
	FL_NOFSROOT	= (1 << 7),
	FL_SUBMOUNTS	= (1 << 8),
	FL_POLL		= (1 << 9)
};

/* column IDs */
enum {
	COL_SOURCE,
	COL_TARGET,
	COL_FSTYPE,
	COL_OPTIONS,
	COL_VFS_OPTIONS,
	COL_FS_OPTIONS,
	COL_LABEL,
	COL_UUID,
	COL_MAJMIN,
	COL_ACTION,
	COL_OLD_TARGET,
	COL_OLD_OPTIONS,

	FINDMNT_NCOLUMNS
};

/* column names */
struct colinfo {
	const char	*name;		/* header */
	double		whint;		/* width hint (N < 1 is in percent of termwidth) */
	int		flags;		/* tt flags */
	const char      *help;		/* column description */
	const char	*match;		/* pattern for match_func() */
};

/* columns descriptions (don't use const, this is writable) */
static struct colinfo infos[FINDMNT_NCOLUMNS] = {
	[COL_SOURCE]       = { "SOURCE",       0.25, 0, N_("source device") },
	[COL_TARGET]       = { "TARGET",       0.30, TT_FL_TREE, N_("mountpoint") },
	[COL_FSTYPE]       = { "FSTYPE",       0.10, TT_FL_TRUNC, N_("filesystem type") },
	[COL_OPTIONS]      = { "OPTIONS",      0.10, TT_FL_TRUNC, N_("all mount options") },
	[COL_VFS_OPTIONS]  = { "VFS-OPTIONS",  0.20, TT_FL_TRUNC, N_("VFS specific mount options") },
	[COL_FS_OPTIONS]   = { "FS-OPTIONS",   0.10, TT_FL_TRUNC, N_("FS specific mount options") },
	[COL_LABEL]        = { "LABEL",        0.10, 0, N_("filesystem label") },
	[COL_UUID]         = { "UUID",           36, 0, N_("filesystem UUID") },
	[COL_MAJMIN]       = { "MAJ:MIN",         6, 0, N_("major:minor device number") },
	[COL_ACTION]       = { "ACTION",         10, TT_FL_STRICTWIDTH, N_("action detected by --poll") },
	[COL_OLD_OPTIONS]  = { "OLD-OPTIONS",  0.10, TT_FL_TRUNC, N_("old mount options saved by --poll") },
	[COL_OLD_TARGET]   = { "OLD-TARGET",   0.30, 0, N_("old mountpoint saved by --poll") },
};

/* global flags */
static int flags;
static int tt_flags;

/* array with IDs of enabled columns */
static int columns[FINDMNT_NCOLUMNS];
static int ncolumns;

/* poll actions (parsed --poll=<list> */
#define FINDMNT_NACTIONS	4		/* mount, umount, move, remount */
static int actions[FINDMNT_NACTIONS];
static int nactions;

/* libmount cache */
static struct libmnt_cache *cache;

static int get_column_id(int num)
{
	assert(num < ncolumns);
	assert(columns[num] < FINDMNT_NCOLUMNS);
	return columns[num];
}

static struct colinfo *get_column_info(int num)
{
	return &infos[ get_column_id(num) ];
}

static const char *column_id_to_name(int id)
{
	assert(id < FINDMNT_NCOLUMNS);
	return infos[id].name;
}

static const char *get_column_name(int num)
{
	return get_column_info(num)->name;
}

static float get_column_whint(int num)
{
	return get_column_info(num)->whint;
}

static int get_column_flags(int num)
{
	return get_column_info(num)->flags;
}

static const char *get_match(int id)
{
	assert(id < FINDMNT_NCOLUMNS);
	return infos[id].match;
}

static void set_match(int id, const char *match)
{
	assert(id < FINDMNT_NCOLUMNS);
	infos[id].match = match;
}

static int is_tabdiff_column(int id)
{
	assert(id < FINDMNT_NCOLUMNS);

	switch(id) {
	case COL_ACTION:
	case COL_OLD_TARGET:
	case COL_OLD_OPTIONS:
		return 1;
	default:
		break;
	}
	return 0;
}

/*
 * "findmnt" without any filter
 */
static int is_listall_mode(void)
{
	return (!get_match(COL_SOURCE) &&
		!get_match(COL_TARGET) &&
		!get_match(COL_FSTYPE) &&
		!get_match(COL_OPTIONS));
}

/*
 * Returns 1 if the @act is in the --poll=<list>
 */
static int has_poll_action(int act)
{
	int i;

	if (!nactions)
		return 1;	/* all actions enabled */
	for (i = 0; i < nactions; i++)
		if (actions[i] == act)
			return 1;
	return 0;
}

static int poll_action_name_to_id(const char *name, size_t namesz)
{
	int id = -1;

	if (strncasecmp(name, "move", namesz) == 0 && namesz == 4)
		id = MNT_TABDIFF_MOVE;
	else if (strncasecmp(name, "mount", namesz) == 0 && namesz == 5)
		id = MNT_TABDIFF_MOUNT;
	else if (strncasecmp(name, "umount", namesz) == 0 && namesz == 6)
		id = MNT_TABDIFF_UMOUNT;
	else if (strncasecmp(name, "remount", namesz) == 0 && namesz == 7)
		id = MNT_TABDIFF_REMOUNT;
	else
		warnx(_("unknown action: %s"), name);

	return id;
}

/*
 * findmnt --first-only <devname|TAG=|mountpoint>
 *
 * ... it works like "mount <devname|TAG=|mountpoint>"
 */
static int is_mount_compatible_mode(void)
{
	if (!get_match(COL_SOURCE))
	       return 0;		/* <devname|TAG=|mountpoint> is required */
	if (get_match(COL_FSTYPE) || get_match(COL_OPTIONS))
		return 0;		/* cannot be restricted by -t or -O */
	if (!(flags & FL_FIRSTONLY))
		return 0;		/* we have to return the first entry only */

	return 1;			/* ok */
}

static void disable_columns_truncate(void)
{
	int i;

	for (i = 0; i < FINDMNT_NCOLUMNS; i++)
		infos[i].flags &= ~TT_FL_TRUNC;
}

/*
 * converts @name to column ID
 */
static int column_name_to_id(const char *name, size_t namesz)
{
	int i;

	for (i = 0; i < FINDMNT_NCOLUMNS; i++) {
		const char *cn = column_id_to_name(i);

		if (!strncasecmp(name, cn, namesz) && !*(cn + namesz))
			return i;
	}
	warnx(_("unknown column: %s"), name);
	return -1;
}

/* Returns LABEL or UUID */
static const char *get_tag(struct libmnt_fs *fs, const char *tagname)
{
	const char *t, *v, *res;

	if (!mnt_fs_get_tag(fs, &t, &v) && !strcmp(t, tagname))
		res = v;
	else {
		res = mnt_fs_get_source(fs);
		if (res)
			res = mnt_resolve_spec(res, cache);
		if (res)
			res = mnt_cache_find_tag_value(cache, res, tagname);
	}

	return res;
}

/* reads FS data from libmount
 * TODO: add function that will deallocate data allocated by get_data()
 */
static const char *get_data(struct libmnt_fs *fs, int num)
{
	const char *str = NULL;

	switch(get_column_id(num)) {
	case COL_SOURCE:
	{
		const char *root = mnt_fs_get_root(fs);

		str = mnt_fs_get_srcpath(fs);

		if (str && (flags & FL_CANONICALIZE))
			str = mnt_resolve_path(str, cache);
		if (!str) {
			str = mnt_fs_get_source(fs);

			if (str && (flags & FL_EVALUATE))
				str = mnt_resolve_spec(str, cache);
		}
		if (root && str && !(flags & FL_NOFSROOT) && strcmp(root, "/")) {
			char *tmp;

			if (asprintf(&tmp, "%s[%s]", str, root) > 0)
				str = tmp;
		}
		break;
	}
	case COL_TARGET:
		str = mnt_fs_get_target(fs);
		break;
	case COL_FSTYPE:
		str = mnt_fs_get_fstype(fs);
		break;
	case COL_OPTIONS:
		str = mnt_fs_get_options(fs);
		break;
	case COL_VFS_OPTIONS:
		str = mnt_fs_get_vfs_options(fs);
		break;
	case COL_FS_OPTIONS:
		str = mnt_fs_get_fs_options(fs);
		break;
	case COL_UUID:
		str = get_tag(fs, "UUID");
		break;
	case COL_LABEL:
		str = get_tag(fs, "LABEL");
		break;
	case COL_MAJMIN:
	{
		dev_t devno = mnt_fs_get_devno(fs);
		if (devno) {
			char *tmp;
			int rc = 0;
			if ((tt_flags & TT_FL_RAW) || (tt_flags & TT_FL_EXPORT))
				rc = asprintf(&tmp, "%u:%u",
					      major(devno), minor(devno));
			else
				rc = asprintf(&tmp, "%3u:%-3u",
					      major(devno), minor(devno));
			if (rc)
				str = tmp;
		}
	}
	default:
		break;
	}
	return str;
}

static const char *get_tabdiff_data(struct libmnt_fs *old_fs,
				    struct libmnt_fs *new_fs,
				    int change,
				    int num)
{
	const char *str = NULL;

	switch (get_column_id(num)) {
	case COL_ACTION:
		switch (change) {
		case MNT_TABDIFF_MOUNT:
			str = _("mount");
			break;
		case MNT_TABDIFF_UMOUNT:
			str = _("umount");
			break;
		case MNT_TABDIFF_REMOUNT:
			str = _("remount");
			break;
		case MNT_TABDIFF_MOVE:
			str = _("move");
			break;
		default:
			str = _("unknown");
			break;
		}
		break;
	case COL_OLD_OPTIONS:
		if (old_fs && (change == MNT_TABDIFF_REMOUNT ||
			       change == MNT_TABDIFF_UMOUNT))
			str = mnt_fs_get_options(old_fs);
		break;
	case COL_OLD_TARGET:
		if (old_fs && (change == MNT_TABDIFF_MOVE ||
			       change == MNT_TABDIFF_UMOUNT))
			str = mnt_fs_get_target(old_fs);
		break;
	default:
		if (new_fs)
			str = get_data(new_fs, num);
		else
			str = get_data(old_fs, num);
		break;
	}
	return str;
}

/* adds one line to the output @tab */
static struct tt_line *add_line(struct tt *tt, struct libmnt_fs *fs,
					struct tt_line *parent)
{
	int i;
	struct tt_line *line = tt_add_line(tt, parent);

	if (!line) {
		warn(_("failed to add line to output"));
		return NULL;
	}
	for (i = 0; i < ncolumns; i++)
		tt_line_set_data(line, i, get_data(fs, i));

	tt_line_set_userdata(line, fs);
	return line;
}

static struct tt_line *add_tabdiff_line(struct tt *tt, struct libmnt_fs *new_fs,
			struct libmnt_fs *old_fs, int change)
{
	int i;
	struct tt_line *line = tt_add_line(tt, NULL);

	if (!line) {
		warn(_("failed to add line to output"));
		return NULL;
	}
	for (i = 0; i < ncolumns; i++)
		tt_line_set_data(line, i,
				get_tabdiff_data(old_fs, new_fs, change, i));

	return line;
}

static int has_line(struct tt *tt, struct libmnt_fs *fs)
{
	struct list_head *p;

	list_for_each(p, &tt->tb_lines) {
		struct tt_line *ln = list_entry(p, struct tt_line, ln_lines);
		if ((struct libmnt_fs *) ln->userdata == fs)
			return 1;
	}
	return 0;
}

/* reads filesystems from @tb (libmount) and fillin @tt (output table) */
static int create_treenode(struct tt *tt, struct libmnt_table *tb,
			   struct libmnt_fs *fs, struct tt_line *parent_line)
{
	struct libmnt_fs *chld = NULL;
	struct libmnt_iter *itr = NULL;
	struct tt_line *line;
	int rc = -1;

	if (!fs) {
		/* first call, get root FS */
		if (mnt_table_get_root_fs(tb, &fs))
			goto leave;
		parent_line = NULL;

	} else if ((flags & FL_SUBMOUNTS) && has_line(tt, fs))
		return 0;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		goto leave;

	line = add_line(tt, fs, parent_line);
	if (!line)
		goto leave;

	/*
	 * add all children to the output table
	 */
	while(mnt_table_next_child_fs(tb, itr, fs, &chld) == 0) {
		if (create_treenode(tt, tb, chld, line))
			goto leave;
	}
	rc = 0;
leave:
	mnt_free_iter(itr);
	return rc;
}

/* error callback */
static int parser_errcb(struct libmnt_table *tb __attribute__ ((__unused__)),
			const char *filename, int line)
{
	warn(_("%s: parse error at line %d"), filename, line);
	return 0;
}

/* calls libmount fstab/mtab/mountinfo parser */
static struct libmnt_table *parse_tabfile(const char *path)
{
	int rc;
	struct libmnt_table *tb = mnt_new_table();

	if (!tb) {
		warn(_("failed to initialize libmount table"));
		return NULL;
	}

	mnt_table_set_parser_errcb(tb, parser_errcb);

	if (!strcmp(path, _PATH_MNTTAB))
		rc = mnt_table_parse_fstab(tb, NULL);
	else if (!strcmp(path, _PATH_MOUNTED))
		rc = mnt_table_parse_mtab(tb, NULL);
	else
		rc = mnt_table_parse_file(tb, path);

	if (rc) {
		mnt_free_table(tb);
		warn(_("can't read %s"), path);
		return NULL;
	}
	return tb;
}

/* filter function for libmount (mnt_table_find_next_fs()) */
static int match_func(struct libmnt_fs *fs,
		      void *data __attribute__ ((__unused__)))
{
	int rc = flags & FL_INVERT ? 1 : 0;
	const char *m;

	m = get_match(COL_TARGET);
	if (m && !mnt_fs_match_target(fs, m, cache))
		return rc;

	m = get_match(COL_SOURCE);
	if (m && !mnt_fs_match_source(fs, m, cache))
		return rc;

	m = get_match(COL_FSTYPE);
	if (m && !mnt_fs_match_fstype(fs, m))
		return rc;

	m = get_match(COL_OPTIONS);
	if (m && !mnt_fs_match_options(fs, m))
		return rc;

	return !rc;
}

/* iterate over filesystems in @tb */
static struct libmnt_fs *get_next_fs(struct libmnt_table *tb,
				     struct libmnt_iter *itr)
{
	struct libmnt_fs *fs = NULL;

	if (is_listall_mode()) {
		/*
		 * Print whole file
		 */
		if (mnt_table_next_fs(tb, itr, &fs) != 0)
			return NULL;

	} else if (is_mount_compatible_mode()) {
		/*
		 * Look up for FS in the same way how mount(8) searchs in fstab
		 *
		 *   findmnt -f <spec>
		 */
		fs = mnt_table_find_source(tb, get_match(COL_SOURCE),
					mnt_iter_get_direction(itr));

		if (!fs && !(flags & FL_NOSWAPMATCH))
			fs = mnt_table_find_target(tb, get_match(COL_SOURCE),
					mnt_iter_get_direction(itr));
	} else {
		/*
		 * Look up for all matching entries
		 *
		 *    findmnt [-l] <source> <target> [-O <options>] [-t <types>]
		 *    findmnt [-l] <spec> [-O <options>] [-t <types>]
		 */
again:
		mnt_table_find_next_fs(tb, itr, match_func,  NULL, &fs);

		if (!fs &&
		    !(flags & FL_NOSWAPMATCH) &&
		    !get_match(COL_TARGET) && get_match(COL_SOURCE)) {

			/* swap 'spec' and target. */
			set_match(COL_TARGET, get_match(COL_SOURCE));
			set_match(COL_SOURCE, NULL);
			mnt_reset_iter(itr, -1);

			goto again;
		}
	}

	return fs;
}

static int add_matching_lines(struct libmnt_table *tb,
			      struct tt *tt, int direction)
{
	struct libmnt_iter *itr = NULL;
	struct libmnt_fs *fs;
	int nlines = 0, rc = -1;

	itr = mnt_new_iter(direction);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		goto done;
	}

	while((fs = get_next_fs(tb, itr))) {
		if ((tt_flags & TT_FL_TREE) || (flags & FL_SUBMOUNTS))
			rc = create_treenode(tt, tb, fs, NULL);
		else
			rc = !add_line(tt, fs, NULL);
		if (rc)
			goto done;
		nlines++;
		if (flags & FL_FIRSTONLY)
			break;
		flags |= FL_NOSWAPMATCH;
	}

	if (nlines)
		rc = 0;
done:
	mnt_free_iter(itr);
	return rc;
}

static int poll_match(struct libmnt_fs *fs)
{
	int rc = match_func(fs, NULL);

	if (rc == 0 && !(flags & FL_NOSWAPMATCH) &&
	    get_match(COL_SOURCE) && !get_match(COL_TARGET)) {
		/*
		 * findmnt --poll /foo
		 * The '/foo' source as well as target.
		 */
		const char *str = get_match(COL_SOURCE);

		set_match(COL_TARGET, str);	/* swap */
		set_match(COL_SOURCE, NULL);

		rc = match_func(fs, NULL);

		set_match(COL_TARGET, NULL);	/* restore */
		set_match(COL_SOURCE, str);

	}
	return rc;
}

static int poll_table(struct libmnt_table *tb, const char *tabfile,
		  int timeout, struct tt *tt, int direction)
{
	FILE *f;
	int rc = -1;
	struct libmnt_iter *itr = NULL;
	struct libmnt_table *tb_new = NULL;
	struct libmnt_tabdiff *diff = NULL;
	struct pollfd fds[1];

	tb_new = mnt_new_table();
	if (!tb_new) {
		warn(_("failed to initialize libmount table"));
		goto done;
	}

	itr = mnt_new_iter(direction);
	if (!itr) {
		warn(_("failed to initialize libmount iterator"));
		goto done;
	}

	diff = mnt_new_tabdiff();
	if (!diff) {
		warn(_("failed to initialize libmount tabdiff"));
		goto done;
	}

	/* cache is unnecessary to detect changes */
	mnt_table_set_cache(tb, NULL);
	mnt_table_set_cache(tb_new, NULL);

	f = fopen(tabfile, "r");
	if (!f) {
		warn(_("%s: open failed"), tabfile);
		goto done;
	}

	mnt_table_set_parser_errcb(tb_new, parser_errcb);

	fds[0].fd = fileno(f);
	fds[0].events = POLLPRI;

	while (1) {
		struct libmnt_table *tmp;
		struct libmnt_fs *old, *new;
		int change, count;

		count = poll(fds, 1, timeout);
		if (count == 0)
			break;	/* timeout */
		if (count < 0) {
			warn(_("poll() failed"));
			goto done;
		}

		rewind(f);
		rc = mnt_table_parse_stream(tb_new, f, tabfile);
		if (!rc)
			rc = mnt_diff_tables(diff, tb, tb_new);
		if (rc < 0)
			goto done;

		count = 0;
		mnt_reset_iter(itr, direction);
		while(mnt_tabdiff_next_change(
				diff, itr, &old, &new, &change) == 0) {

			if (!has_poll_action(change))
				continue;
			if (!poll_match(new ? new : old))
				continue;
			count++;
			rc = !add_tabdiff_line(tt, new, old, change);
			if (rc)
				goto done;
			if (flags & FL_FIRSTONLY)
				break;
		}

		if (count) {
			rc = tt_print_table(tt);
			if (rc)
				goto done;
		}

		/* swap tables */
		tmp = tb;
		tb = tb_new;
		tb_new = tmp;

		tt_remove_lines(tt);
		mnt_reset_table(tb_new);

		if (count && (flags & FL_FIRSTONLY))
			break;
	}

	rc = 0;
done:
	mnt_free_table(tb_new);
	mnt_free_tabdiff(diff);
	mnt_free_iter(itr);
	return rc;
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	int i;

	fprintf(out, _(
	"\nUsage:\n"
	" %1$s [options]\n"
	" %1$s [options] <device> | <mountpoint>\n"
	" %1$s [options] <device> <mountpoint>\n"
	" %1$s [options] [--source <device>] [--target <mountpoint>]\n"),
		program_invocation_short_name);

	fprintf(out, _(
	"\nOptions:\n"
	" -s, --fstab            search in static table of filesystems\n"
	" -m, --mtab             search in table of mounted filesystems\n"
	" -k, --kernel           search in kernel table of mounted\n"
        "                          filesystems (default)\n\n"));

	fprintf(out, _(
	" -p, --poll[=<list>]    monitor changes in table of mounted filesystems\n"
	" -w, --timeout <num>    upper limit in milliseconds that --poll will block\n\n"));

	fprintf(out, _(
	" -a, --ascii            use ASCII chars for tree formatting\n"
	" -c, --canonicalize     canonicalize printed paths\n"
	" -d, --direction <word> direction of search, 'forward' or 'backward'\n"
	" -e, --evaluate         convert tags (LABEL/UUID) to device names\n"
	" -f, --first-only       print the first found filesystem only\n"));

	fprintf(out, _(
	" -h, --help             display this help text and exit\n"
	" -i, --invert           invert the sense of matching\n"
	" -l, --list             use list format output\n"
	" -n, --noheadings       don't print column headings\n"
	" -u, --notruncate       don't truncate text in columns\n"));
	fprintf(out, _(
	" -O, --options <list>   limit the set of filesystems by mount options\n"
	" -o, --output <list>    the output columns to be shown\n"
	" -P, --pairs            use key=\"value\" output format\n"
	" -r, --raw              use raw output format\n"
	" -t, --types <list>     limit the set of filesystems by FS types\n"));
	fprintf(out, _(
	" -v, --nofsroot         don't print [/dir] for bind or btrfs mounts\n"
	" -R, --submounts        print all submounts for the matching filesystems\n"
	" -S, --source <string>  the device to mount (by name, LABEL= or UUID=)\n"
	" -T, --target <string>  the mountpoint to use\n\n"));


	fprintf(out, _("\nAvailable columns:\n"));

	for (i = 0; i < FINDMNT_NCOLUMNS; i++)
		fprintf(out, " %11s  %s\n", infos[i].name, _(infos[i].help));


	fprintf(out, _("\nFor more information see findmnt(1).\n"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void __attribute__((__noreturn__))
errx_mutually_exclusive(const char *opts)
{
	errx(EXIT_FAILURE, "%s %s", opts, _("options are mutually exclusive"));
}

int main(int argc, char *argv[])
{
	/* libmount */
	struct libmnt_table *tb = NULL;
	char *tabfile = NULL;
	int direction = MNT_ITER_FORWARD;
	int i, c, rc = -1, timeout = -1;

	/* table.h */
	struct tt *tt = NULL;

	static const struct option longopts[] = {
	    { "ascii",        0, 0, 'a' },
	    { "canonicalize", 0, 0, 'c' },
	    { "direction",    1, 0, 'd' },
	    { "evaluate",     0, 0, 'e' },
	    { "first-only",   0, 0, 'f' },
	    { "fstab",        0, 0, 's' },
	    { "help",         0, 0, 'h' },
	    { "invert",       0, 0, 'i' },
	    { "kernel",       0, 0, 'k' },
	    { "list",         0, 0, 'l' },
	    { "mtab",         0, 0, 'm' },
	    { "noheadings",   0, 0, 'n' },
	    { "notruncate",   0, 0, 'u' },
	    { "options",      1, 0, 'O' },
	    { "output",       1, 0, 'o' },
	    { "poll",         2, 0, 'p' },
	    { "pairs",        0, 0, 'P' },
	    { "raw",          0, 0, 'r' },
	    { "types",        1, 0, 't' },
	    { "fsroot",       0, 0, 'v' },
	    { "submounts",    0, 0, 'R' },
	    { "source",       1, 0, 'S' },
	    { "target",       1, 0, 'T' },
	    { "timeout",      1, 0, 'w' },

	    { NULL,           0, 0, 0 }
	};

	assert(ARRAY_SIZE(columns) == FINDMNT_NCOLUMNS);

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* default output format */
	tt_flags |= TT_FL_TREE;

	while ((c = getopt_long(argc, argv,
				"acd:ehifo:O:p::Pklmnrst:uvRS:T:w:", longopts, NULL)) != -1) {
		switch(c) {
		case 'a':
			tt_flags |= TT_FL_ASCII;
			break;
		case 'c':
			flags |= FL_CANONICALIZE;
			break;
		case 'd':
			if (!strcmp(optarg, "forward"))
				direction = MNT_ITER_FORWARD;
			else if (!strcmp(optarg, "backward"))
				direction = MNT_ITER_BACKWARD;
			else
				errx(EXIT_FAILURE,
					_("unknown direction '%s'"), optarg);
			break;
		case 'e':
			flags |= FL_EVALUATE;
			break;
		case 'h':
			usage(stdout);
			break;
		case 'i':
			flags |= FL_INVERT;
			break;
		case 'f':
			flags |= FL_FIRSTONLY;
			break;
		case 'u':
			disable_columns_truncate();
			break;
		case 'o':
			ncolumns = string_to_idarray(optarg,
						columns, ARRAY_SIZE(columns),
						column_name_to_id);
			if (ncolumns < 0)
				exit(EXIT_FAILURE);
			break;
		case 'O':
			set_match(COL_OPTIONS, optarg);
			break;
		case 'p':
			if (optarg) {
				nactions = string_to_idarray(optarg,
						actions, ARRAY_SIZE(actions),
						poll_action_name_to_id);
				if (nactions < 0)
					exit(EXIT_FAILURE);
			}
			flags |= FL_POLL;
			tt_flags &= ~TT_FL_TREE;
			break;
		case 'P':
			tt_flags |= TT_FL_EXPORT;
			tt_flags &= ~TT_FL_TREE;
			break;
		case 'm':		/* mtab */
			if (tabfile)
				errx_mutually_exclusive("--{fstab,mtab,kernel}");
			tabfile = _PATH_MOUNTED;
			tt_flags &= ~TT_FL_TREE;
			break;
		case 's':		/* fstab */
			if (tabfile)
				errx_mutually_exclusive("--{fstab,mtab,kernel}");
			tabfile = _PATH_MNTTAB;
			tt_flags &= ~TT_FL_TREE;
			break;
		case 'k':		/* kernel (mountinfo) */
			if (tabfile)
				 errx_mutually_exclusive("--{fstab,mtab,kernel}");
			tabfile = _PATH_PROC_MOUNTINFO;
			break;
		case 't':
			set_match(COL_FSTYPE, optarg);
			break;
		case 'r':
			tt_flags &= ~TT_FL_TREE;	/* disable the default */
			tt_flags |= TT_FL_RAW;		/* enable raw */
			break;
		case 'l':
			if ((tt_flags & TT_FL_RAW) && (tt_flags & TT_FL_EXPORT))
				errx_mutually_exclusive("--{raw,list,pairs}");

			tt_flags &= ~TT_FL_TREE; /* disable the default */
			break;
		case 'n':
			tt_flags |= TT_FL_NOHEADINGS;
			break;
		case 'v':
			flags |= FL_NOFSROOT;
			break;
		case 'R':
			flags |= FL_SUBMOUNTS;
			break;
		case 'S':
			set_match(COL_SOURCE, optarg);
			flags |= FL_NOSWAPMATCH;
			break;
		case 'T':
			set_match(COL_TARGET, optarg);
			flags |= FL_NOSWAPMATCH;
			break;
		case 'w':
			timeout = strtol_or_err(optarg,
					_("failed to parse timeout"));
			break;
		default:
			usage(stderr);
			break;
		}
	}

	/* default columns */
	if (!ncolumns) {
		if (flags & FL_POLL)
			columns[ncolumns++] = COL_ACTION;

		columns[ncolumns++] = COL_TARGET;
		columns[ncolumns++] = COL_SOURCE;
		columns[ncolumns++] = COL_FSTYPE;
		columns[ncolumns++] = COL_OPTIONS;
	}

	if (!tabfile) {
		tabfile = _PATH_PROC_MOUNTINFO;

		if (access(tabfile, R_OK)) {		/* old kernel? */
			tabfile = _PATH_PROC_MOUNTS;
			tt_flags &= ~TT_FL_TREE;
		}
	}

	if (optind < argc && (get_match(COL_SOURCE) || get_match(COL_TARGET)))
		errx(EXIT_FAILURE, _(
			"options --target and --source can't be used together "
			"with command line element that is not an option"));

	if (optind < argc)
		set_match(COL_SOURCE, argv[optind++]);	/* dev/tag/mountpoint */
	if (optind < argc)
		set_match(COL_TARGET, argv[optind++]);	/* mountpoint */

	if ((flags & FL_SUBMOUNTS) && is_listall_mode())
		/* don't care about submounts if list all mounts */
		flags &= ~FL_SUBMOUNTS;

	if (!(flags & FL_SUBMOUNTS) &&
	    (!is_listall_mode() || (flags & FL_FIRSTONLY)))
		tt_flags &= ~TT_FL_TREE;

	if (!(flags & FL_NOSWAPMATCH) &&
	    !get_match(COL_TARGET) && get_match(COL_SOURCE)) {
		/*
		 * Check if we can swap source and target, it's
		 * not possible if the source is LABEL=/UUID=
		 */
		const char *x = get_match(COL_SOURCE);

		if (!strncmp(x, "LABEL=", 6) || !strncmp(x, "UUID=", 5))
			flags |= FL_NOSWAPMATCH;
	}

	/*
	 * initialize libmount
	 */
	mnt_init_debug(0);

	tb = parse_tabfile(tabfile);
	if (!tb)
		goto leave;

	cache = mnt_new_cache();
	if (!cache) {
		warn(_("failed to initialize libmount cache"));
		goto leave;
	}
	mnt_table_set_cache(tb, cache);

	/*
	 * initialize output formatting (tt.h)
	 */
	tt = tt_new_table(tt_flags);
	if (!tt) {
		warn(_("failed to initialize output table"));
		goto leave;
	}

	for (i = 0; i < ncolumns; i++) {
		int fl = get_column_flags(i);
		int id = get_column_id(i);

		if (!(tt_flags & TT_FL_TREE))
			fl &= ~TT_FL_TREE;

		if (!(flags & FL_POLL) && is_tabdiff_column(id)) {
			warnx(_("%s column is requested, but --poll "
			       "is not enabled"), get_column_name(i));
			goto leave;
		}
		if (!tt_define_column(tt, get_column_name(i),
					get_column_whint(i), fl)) {
			warn(_("failed to initialize output column"));
			goto leave;
		}
	}

	/*
	 * Fill in data to the output table
	 */
	if (flags & FL_POLL)
		/* poll mode */
		rc = poll_table(tb, tabfile, timeout, tt, direction);

	else if ((tt_flags & TT_FL_TREE) && is_listall_mode())
		/* whole tree */
		rc = create_treenode(tt, tb, NULL, NULL);
	else
		/* whole lits of sub-tree */
		rc = add_matching_lines(tb, tt, direction);

	/*
	 * Print the output table for non-poll modes
	 */
	if (!rc && !(flags & FL_POLL))
		tt_print_table(tt);
leave:
	tt_free_table(tt);

	mnt_free_table(tb);
	mnt_free_cache(cache);

	return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
