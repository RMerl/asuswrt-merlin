/*
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#ifdef HAVE_SCANDIRAT
#ifndef __USE_GNU
#define __USE_GNU
#endif	/* !__USE_GNU */
#endif	/* HAVE_SCANDIRAT */

#include <ctype.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>

#include "at.h"
#include "mangle.h"
#include "mountP.h"
#include "pathnames.h"
#include "strutils.h"

static inline char *skip_spaces(char *s)
{
	assert(s);

	while (*s == ' ' || *s == '\t')
		s++;
	return s;
}

static int next_number(char **s, int *num)
{
	char *end = NULL;

	assert(num);
	assert(s);

	*s = skip_spaces(*s);
	if (!**s)
		return -1;
	*num = strtol(*s, &end, 10);
	if (end == NULL || *s == end)
	       return -1;

	*s = end;

	/* valid end of number is space or terminator */
	if (*end == ' ' || *end == '\t' || *end == '\0')
		return 0;
	return -1;
}

/*
 * Parses one line from {fs,m}tab
 */
static int mnt_parse_table_line(struct libmnt_fs *fs, char *s)
{
	int rc, n = 0, xrc;
	char *src = NULL, *fstype = NULL, *optstr = NULL;

	rc = sscanf(s,	UL_SCNsA" "	/* (1) source */
			UL_SCNsA" "	/* (2) target */
			UL_SCNsA" "	/* (3) FS type */
			UL_SCNsA" "	/* (4) options */
			"%n",		/* byte count */

			&src,
			&fs->target,
			&fstype,
			&optstr,
			&n);
	xrc = rc;

	if (rc == 3 || rc == 4) {			/* options are optional */
		unmangle_string(src);
		unmangle_string(fs->target);
		unmangle_string(fstype);

		if (optstr && *optstr)
			unmangle_string(optstr);

		/* note that __foo functions does not reallocate the string
		 */
		rc = __mnt_fs_set_source_ptr(fs, src);
		if (!rc) {
			src = NULL;
			rc = __mnt_fs_set_fstype_ptr(fs, fstype);
			if (!rc)
				fstype = NULL;
		}
		if (!rc && optstr)
			rc = mnt_fs_set_options(fs, optstr);
		free(optstr);
		optstr = NULL;
	} else {
		DBG(TAB, mnt_debug("tab parse error: [sscanf rc=%d]: '%s'", rc, s));
		rc = -EINVAL;
	}

	if (rc) {
		free(src);
		free(fstype);
		free(optstr);
		DBG(TAB, mnt_debug("tab parse error: [set vars, rc=%d]\n", rc));
		return rc;	/* error */
	}

	fs->passno = fs->freq = 0;

	if (xrc == 4 && n)
		s = skip_spaces(s + n);
	if (xrc == 4 && *s) {
		if (next_number(&s, &fs->freq) != 0) {
			if (*s) {
				DBG(TAB, mnt_debug("tab parse error: [freq]"));
				rc = -EINVAL;
			}
		} else if (next_number(&s, &fs->passno) != 0 && *s) {
			DBG(TAB, mnt_debug("tab parse error: [passno]"));
			rc = -EINVAL;
		}
	}

	return rc;
}

/*
 * Parses one line from mountinfo file
 */
static int mnt_parse_mountinfo_line(struct libmnt_fs *fs, char *s)
{
	int rc, end = 0;
	unsigned int maj, min;
	char *fstype = NULL, *src = NULL, *p;

	rc = sscanf(s,	"%u "		/* (1) id */
			"%u "		/* (2) parent */
			"%u:%u "	/* (3) maj:min */
			UL_SCNsA" "	/* (4) mountroot */
			UL_SCNsA" "	/* (5) target */
			UL_SCNsA	/* (6) vfs options (fs-independent) */
			"%n",		/* number of read bytes */

			&fs->id,
			&fs->parent,
			&maj, &min,
			&fs->root,
			&fs->target,
			&fs->vfs_optstr,
			&end);

	if (rc >= 7 && end > 0)
		s += end;

	/* (7) optional fields, terminated by " - " */
	p = strstr(s, " - ");
	if (!p) {
		DBG(TAB, mnt_debug("mountinfo parse error: not found separator"));
		return -EINVAL;
	}
	s = p + 3;

	rc += sscanf(s,	UL_SCNsA" "	/* (8) FS type */
			UL_SCNsA" "	/* (9) source */
			UL_SCNsA,	/* (10) fs options (fs specific) */

			&fstype,
			&src,
			&fs->fs_optstr);

	if (rc >= 10) {
		fs->flags |= MNT_FS_KERNEL;
		fs->devno = makedev(maj, min);

		unmangle_string(fs->root);
		unmangle_string(fs->target);
		unmangle_string(fs->vfs_optstr);
		unmangle_string(fstype);
		unmangle_string(src);

		if (!strcmp(fs->fs_optstr, "none")) {
			free(fs->fs_optstr);
			fs->fs_optstr = NULL;
		} else
			unmangle_string(fs->fs_optstr);

		rc = __mnt_fs_set_fstype_ptr(fs, fstype);
		if (!rc) {
			fstype = NULL;
			rc = __mnt_fs_set_source_ptr(fs, src);
			if (!rc)
				src = NULL;
		}

		/* merge VFS and FS options to the one string */
		fs->optstr = mnt_fs_strdup_options(fs);
		if (!fs->optstr)
			rc = -ENOMEM;
	} else {
		free(fstype);
		free(src);
		DBG(TAB, mnt_debug(
			"mountinfo parse error [sscanf rc=%d]: '%s'", rc, s));
		rc = -EINVAL;
	}
	return rc;
}

/*
 * Parses one line from utab file
 */
static int mnt_parse_utab_line(struct libmnt_fs *fs, const char *s)
{
	const char *p = s;

	assert(fs);
	assert(s);
	assert(!fs->source);
	assert(!fs->target);

	while (p && *p) {
		char *end = NULL;

		while (*p == ' ') p++;
		if (!*p)
			break;

		if (!fs->source && !strncmp(p, "SRC=", 4)) {
			char *v = unmangle(p + 4, &end);
			if (!v)
				goto enomem;
			__mnt_fs_set_source_ptr(fs, v);

		} else if (!fs->target && !strncmp(p, "TARGET=", 7)) {
			fs->target = unmangle(p + 7, &end);
			if (!fs->target)
				goto enomem;

		} else if (!fs->root && !strncmp(p, "ROOT=", 5)) {
			fs->root = unmangle(p + 5, &end);
			if (!fs->root)
				goto enomem;

		} else if (!fs->bindsrc && !strncmp(p, "BINDSRC=", 8)) {
			fs->bindsrc = unmangle(p + 8, &end);
			if (!fs->bindsrc)
				goto enomem;

		} else if (!fs->user_optstr && !strncmp(p, "OPTS=", 5)) {
			fs->user_optstr = unmangle(p + 5, &end);
			if (!fs->user_optstr)
				goto enomem;

		} else if (!fs->attrs && !strncmp(p, "ATTRS=", 6)) {
			fs->attrs = unmangle(p + 6, &end);
			if (!fs->attrs)
				goto enomem;

		} else {
			/* unknown variable */
			while (*p && *p != ' ') p++;
		}
		if (end)
			p = end;
	}

	return 0;
enomem:
	DBG(TAB, mnt_debug("utab parse error: ENOMEM"));
	return -ENOMEM;
}

/*
 * Returns {m,fs}tab or mountinfo file format (MNT_FMT_*)
 *
 * Note that we aren't trying to guess utab file format, because this file has
 * to be always parsed by private libmount routines with explicitly defined
 * format.
 *
 * mountinfo: "<number> <number> ... "
 */
static int guess_table_format(char *line)
{
	unsigned int a, b;

	if (sscanf(line, "%u %u", &a, &b) == 2)
		return MNT_FMT_MOUNTINFO;

	return MNT_FMT_FSTAB;		/* fstab, mtab or /proc/mounts */
}

/*
 * Read and parse the next line from {fs,m}tab or mountinfo
 */
static int mnt_table_parse_next(struct libmnt_table *tb, FILE *f, struct libmnt_fs *fs,
				const char *filename, int *nlines)
{
	char buf[BUFSIZ];
	char *s;

	assert(tb);
	assert(f);
	assert(fs);

	/* read the next non-blank non-comment line */
	do {
		if (fgets(buf, sizeof(buf), f) == NULL)
			return -EINVAL;
		++*nlines;
		s = index (buf, '\n');
		if (!s) {
			/* Missing final newline?  Otherwise extremely */
			/* long line - assume file was corrupted */
			if (feof(f)) {
				DBG(TAB, mnt_debug_h(tb,
					"%s: no final newline",	filename));
				s = index (buf, '\0');
			} else {
				DBG(TAB, mnt_debug_h(tb,
					"%s:%d: missing newline at line",
					filename, *nlines));
				goto err;
			}
		}
		*s = '\0';
		if (--s >= buf && *s == '\r')
			*s = '\0';
		s = skip_spaces(buf);
	} while (*s == '\0' || *s == '#');

	if (tb->fmt == MNT_FMT_GUESS)
		tb->fmt = guess_table_format(s);

	if (tb->fmt == MNT_FMT_FSTAB) {
		if (mnt_parse_table_line(fs, s) != 0)
			goto err;

	} else if (tb->fmt == MNT_FMT_MOUNTINFO) {
		if (mnt_parse_mountinfo_line(fs, s) != 0)
			goto err;

	} else if (tb->fmt == MNT_FMT_UTAB) {
		if (mnt_parse_utab_line(fs, s) != 0)
			goto err;
	}


	/*DBG(TAB, mnt_fs_print_debug(fs, stderr));*/

	return 0;
err:
	DBG(TAB, mnt_debug_h(tb, "%s:%d: %s parse error", filename, *nlines,
				tb->fmt == MNT_FMT_MOUNTINFO ? "mountinfo" :
				tb->fmt == MNT_FMT_FSTAB ? "tab" : "utab"));

	/* by default all errors are recoverable, otherwise behavior depends on
	 * errcb() function. See mnt_table_set_parser_errcb().
	 */
	return tb->errcb ? tb->errcb(tb, filename, *nlines) : 1;
}

/**
 * mnt_table_parse_stream:
 * @tb: tab pointer
 * @f: file stream
 * @filename: filename used for debug and error messages
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_table_parse_stream(struct libmnt_table *tb, FILE *f, const char *filename)
{
	int nlines = 0;
	int rc = -1;
	int flags = 0;

	assert(tb);
	assert(f);
	assert(filename);

	DBG(TAB, mnt_debug_h(tb, "%s: start parsing (%d entries)",
				filename, mnt_table_get_nents(tb)));

	/* necessary for /proc/mounts only, the /proc/self/mountinfo
	 * parser sets the flag properly
	 */
	if (filename && strcmp(filename, _PATH_PROC_MOUNTS) == 0)
		flags = MNT_FS_KERNEL;

	while (!feof(f)) {
		struct libmnt_fs *fs = mnt_new_fs();

		if (!fs)
			goto err;

		rc = mnt_table_parse_next(tb, f, fs, filename, &nlines);
		if (!rc) {
			rc = mnt_table_add_fs(tb, fs);
			fs->flags |= flags;
		}
		if (rc) {
			mnt_free_fs(fs);
			if (rc == 1)
				continue;	/* recoverable error */
			if (feof(f))
				break;
			goto err;		/* fatal error */
		}
	}

	DBG(TAB, mnt_debug_h(tb, "%s: stop parsing (%d entries)",
				filename, mnt_table_get_nents(tb)));
	return 0;
err:
	DBG(TAB, mnt_debug_h(tb, "%s: parse error (rc=%d)", filename, rc));
	return rc;
}

/**
 * mnt_table_parse_file:
 * @tb: tab pointer
 * @filename: file
 *
 * Parses whole table (e.g. /etc/mtab) and appends new records to the @tab.
 *
 * The libmount parser ignores broken (syntax error) lines, these lines are
 * reported to caller by errcb() function (see mnt_table_set_parser_errcb()).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_table_parse_file(struct libmnt_table *tb, const char *filename)
{
	FILE *f;
	int rc;

	assert(tb);
	assert(filename);

	if (!filename || !tb)
		return -EINVAL;

	f = fopen(filename, "r");
	if (f) {
		rc = mnt_table_parse_stream(tb, f, filename);
		fclose(f);
	} else
		return -errno;

	return rc;
}

static int mnt_table_parse_dir_filter(const struct dirent *d)
{
	size_t namesz;

#ifdef _DIRENT_HAVE_D_TYPE
	if (d->d_type != DT_UNKNOWN && d->d_type != DT_REG &&
	    d->d_type != DT_LNK)
		return 0;
#endif
	if (*d->d_name == '.')
		return 0;

#define MNT_MNTTABDIR_EXTSIZ	(sizeof(MNT_MNTTABDIR_EXT) - 1)

	namesz = strlen(d->d_name);
	if (!namesz || namesz < MNT_MNTTABDIR_EXTSIZ + 1 ||
	    strcmp(d->d_name + (namesz - MNT_MNTTABDIR_EXTSIZ),
		   MNT_MNTTABDIR_EXT))
		return 0;

	/* Accept this */
	return 1;
}

#ifdef HAVE_SCANDIRAT
static int __mnt_table_parse_dir(struct libmnt_table *tb, const char *dirname)
{
	int n = 0, i;
	int dd;
	struct dirent **namelist = NULL;

	dd = open(dirname, O_RDONLY|O_CLOEXEC|O_DIRECTORY);
	if (dd < 0)
	        return -errno;

	n = scandirat(dd, ".", &namelist, mnt_table_parse_dir_filter, versionsort);
	if (n <= 0) {
	        close(dd);
	        return 0;
	}

	for (i = 0; i < n; i++) {
		struct dirent *d = namelist[i];
		struct stat st;
		FILE *f;

		if (fstat_at(dd, ".", d->d_name, &st, 0) ||
		    !S_ISREG(st.st_mode))
			continue;

		f = fopen_at(dd, ".", d->d_name, O_RDONLY, "r");
		if (f) {
			mnt_table_parse_stream(tb, f, d->d_name);
			fclose(f);
		}
	}

	for (i = 0; i < n; i++)
		free(namelist[i]);
	free(namelist);
	close(dd);
	return 0;
}
#else
static int __mnt_table_parse_dir(struct libmnt_table *tb, const char *dirname)
{
	int n = 0, i, r = 0;
	DIR *dir = NULL;
	struct dirent **namelist = NULL;

	n = scandir(dirname, &namelist, mnt_table_parse_dir_filter, versionsort);
	if (n <= 0)
		return 0;

	/* let use "at" functions rather than play crazy games with paths... */
	dir = opendir(dirname);
	if (!dir) {
		r = -errno;
		goto out;
	}

	for (i = 0; i < n; i++) {
		struct dirent *d = namelist[i];
		struct stat st;
		FILE *f;

		if (fstat_at(dirfd(dir), _PATH_MNTTAB_DIR, d->d_name, &st, 0) ||
		    !S_ISREG(st.st_mode))
			continue;

		f = fopen_at(dirfd(dir), _PATH_MNTTAB_DIR,
					d->d_name, O_RDONLY, "r");
		if (f) {
			mnt_table_parse_stream(tb, f, d->d_name);
			fclose(f);
		}
	}

out:
	for (i = 0; i < n; i++)
		free(namelist[i]);
	free(namelist);
	if (dir)
		closedir(dir);
	return r;
}
#endif

/**
 * mnt_table_parse_dir:
 * @tb: mount table
 * @dirname: directory
 *
 * The directory:
 *	- files are sorted by strverscmp(3)
 *	- files that starts with "." are ignored (e.g. ".10foo.fstab")
 *	- files without the ".fstab" extension are ignored
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_parse_dir(struct libmnt_table *tb, const char *dirname)
{
	return __mnt_table_parse_dir(tb, dirname);
}

struct libmnt_table *__mnt_new_table_from_file(const char *filename, int fmt)
{
	struct libmnt_table *tb;
	struct stat st;

	assert(filename);

	if (!filename)
		return NULL;
	if (stat(filename, &st))
		return NULL;
	tb = mnt_new_table();
	if (tb) {
		tb->fmt = fmt;
		if (mnt_table_parse_file(tb, filename) != 0) {
			mnt_free_table(tb);
			tb = NULL;
		}
	}
	return tb;
}

/**
 * mnt_new_table_from_file:
 * @filename: /etc/{m,fs}tab or /proc/self/mountinfo path
 *
 * Same as mnt_new_table() + mnt_table_parse_file(). Use this function for private
 * files only. This function does not allow to use error callback, so you
 * cannot provide any feedback to end-users about broken records in files (e.g.
 * fstab).
 *
 * Returns: newly allocated tab on success and NULL in case of error.
 */
struct libmnt_table *mnt_new_table_from_file(const char *filename)
{
	return __mnt_new_table_from_file(filename, MNT_FMT_GUESS);
}

/**
 * mnt_new_table_from_dir
 * @dirname: directory with *.fstab files
 *
 * Returns: newly allocated tab on success and NULL in case of error.
 */
struct libmnt_table *mnt_new_table_from_dir(const char *dirname)
{
	struct libmnt_table *tb;

	assert(dirname);

	if (!dirname)
		return NULL;
	tb = mnt_new_table();
	if (tb && mnt_table_parse_dir(tb, dirname) != 0) {
		mnt_free_table(tb);
		tb = NULL;
	}
	return tb;
}

/**
 * mnt_table_set_parser_errcb:
 * @tb: pointer to table
 * @cb: pointer to callback function
 *
 * The error callback function is called by table parser (mnt_table_parse_file())
 * in case of syntax error. The callback function could be used for errors
 * evaluation, libmount will continue/stop parsing according to callback return
 * codes:
 *
 *   <0  : fatal error (abort parsing)
 *    0	 : success (parsing continue)
 *   >0  : recoverable error (the line is ignored, parsing continue).
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_set_parser_errcb(struct libmnt_table *tb,
		int (*cb)(struct libmnt_table *tb, const char *filename, int line))
{
	assert(tb);
	tb->errcb = cb;
	return 0;
}

/**
 * mnt_table_parse_fstab:
 * @tb: table
 * @filename: overwrites default (/etc/fstab or $LIBMOUNT_FSTAB) or NULL
 *
 * This function parses /etc/fstab and appends new lines to the @tab. If the
 * @filename is a directory then mnt_table_parse_dir() is called.
 *
 * See also mnt_table_set_parser_errcb().
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_parse_fstab(struct libmnt_table *tb, const char *filename)
{
	struct stat st;
	int rc = 0;

	assert(tb);

	if (!tb)
		return -EINVAL;
	if (!filename)
		filename = mnt_get_fstab_path();

	if (!filename || stat(filename, &st))
		return -EINVAL;

	tb->fmt = MNT_FMT_FSTAB;

	if (S_ISREG(st.st_mode))
		rc = mnt_table_parse_file(tb, filename);
	else if (S_ISDIR(st.st_mode))
		rc = mnt_table_parse_dir(tb, filename);
	else
		rc = -EINVAL;

	return rc;
}

/*
 * This function uses @uf to found corresponding record in @tb, then the record
 * from @tb is updated (user specific mount options are added).
 *
 * Note that @uf must contain only user specific mount options instead of
 * VFS options (note that FS options are ignored).
 *
 * Returns modified filesystem (from @tb) or NULL.
 */
static struct libmnt_fs *mnt_table_merge_user_fs(struct libmnt_table *tb, struct libmnt_fs *uf)
{
	struct libmnt_fs *fs;
	struct libmnt_iter itr;
	const char *optstr, *src, *target, *root, *attrs;

	assert(tb);
	assert(uf);
	if (!tb || !uf)
		return NULL;

	DBG(TAB, mnt_debug_h(tb, "merging user fs"));

	src = mnt_fs_get_srcpath(uf);
	target = mnt_fs_get_target(uf);
	optstr = mnt_fs_get_user_options(uf);
	attrs = mnt_fs_get_attributes(uf);
	root = mnt_fs_get_root(uf);

	if (!src || !target || !root || (!attrs && !optstr))
		return NULL;

	mnt_reset_iter(&itr, MNT_ITER_BACKWARD);

	while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
		const char *s = mnt_fs_get_srcpath(fs),
			   *t = mnt_fs_get_target(fs),
			   *r = mnt_fs_get_root(fs);

		if (fs->flags & MNT_FS_MERGED)
			continue;

		/*
		 * Note that kernel can add tailing slash to the network
		 * filesystem source path
		 */
		if (s && t && r &&
		    strcmp(t, target) == 0 &&
		    streq_except_trailing_slash(s, src) &&
		    strcmp(r, root) == 0)
			break;
	}

	if (fs) {
		DBG(TAB, mnt_debug_h(tb, "found fs -- appending user optstr"));
		mnt_fs_append_options(fs, optstr);
		mnt_fs_append_attributes(fs, attrs);
		mnt_fs_set_bindsrc(fs, mnt_fs_get_bindsrc(uf));
		fs->flags |= MNT_FS_MERGED;

		DBG(TAB, mnt_debug_h(tb, "found fs:"));
		DBG(TAB, mnt_fs_print_debug(fs, stderr));
	}
	return fs;
}

/**
 * mnt_table_parse_mtab:
 * @tb: table
 * @filename: overwrites default (/etc/mtab or $LIBMOUNT_MTAB) or NULL
 *
 * This function parses /etc/mtab or /proc/self/mountinfo +
 * /run/mount/utabs or /proc/mounts.
 *
 * See also mnt_table_set_parser_errcb().
 *
 * Returns: 0 on success or negative number in case of error.
 */
int mnt_table_parse_mtab(struct libmnt_table *tb, const char *filename)
{
	int rc;
	const char *utab = NULL;

	if (mnt_has_regular_mtab(&filename, NULL)) {

		DBG(TAB, mnt_debug_h(tb, "force %s usage", filename));

		rc = mnt_table_parse_file(tb, filename);
		if (!rc)
			return 0;
		filename = NULL;	/* failed */
	}

	/*
	 * useless /etc/mtab
	 * -- read kernel information from /proc/self/mountinfo
	 */
	tb->fmt = MNT_FMT_MOUNTINFO;
	rc = mnt_table_parse_file(tb, _PATH_PROC_MOUNTINFO);
	if (rc) {
		/* hmm, old kernel? ...try /proc/mounts */
		tb->fmt = MNT_FMT_MTAB;
		return mnt_table_parse_file(tb, _PATH_PROC_MOUNTS);
	}

	/*
	 * try to read user specific information from /run/mount/utabs
	 */
	utab = mnt_get_utab_path();
	if (utab) {
		struct libmnt_table *u_tb = __mnt_new_table_from_file(utab, MNT_FMT_UTAB);

		if (u_tb) {
			struct libmnt_fs *u_fs;
			struct libmnt_iter itr;

			mnt_reset_iter(&itr, MNT_ITER_BACKWARD);

			/*  merge user options into mountinfo from kernel */
			while(mnt_table_next_fs(u_tb, &itr, &u_fs) == 0)
				mnt_table_merge_user_fs(tb, u_fs);

			mnt_free_table(u_tb);
		}
	}
	return 0;
}
