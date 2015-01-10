/*
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: update
 * @title: Tables update
 * @short_description: userspace mount information management
 *
 * The struct libmnt_update provides abstraction to manage mount options in
 * userspace independently on system configuration. This low-level API works on
 * system with and without /etc/mtab. On systems without the regular /etc/mtab
 * file are userspace mount options (e.g. user=) stored to the /run/mount/utab
 * file.
 *
 * It's recommended to use high-level struct libmnt_context API.
 */
#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>

#include "mountP.h"
#include "mangle.h"
#include "pathnames.h"

struct libmnt_update {
	char		*target;
	struct libmnt_fs *fs;
	char		*filename;
	unsigned long	mountflags;
	int		userspace_only;
	int		ready;

	struct libmnt_table *mountinfo;
};

static int set_fs_root(struct libmnt_update *upd, struct libmnt_fs *fs, unsigned long mountflags);
static int utab_new_entry(struct libmnt_update *upd, struct libmnt_fs *fs, unsigned long mountflags);

/**
 * mnt_new_update:
 *
 * Returns: newly allocated update handler
 */
struct libmnt_update *mnt_new_update(void)
{
	struct libmnt_update *upd;

	upd = calloc(1, sizeof(*upd));
	if (!upd)
		return NULL;

	DBG(UPDATE, mnt_debug_h(upd, "allocate"));
	return upd;
}

/**
 * mnt_free_update:
 * @upd: update
 *
 * Deallocates struct libmnt_update handler.
 */
void mnt_free_update(struct libmnt_update *upd)
{
	if (!upd)
		return;

	DBG(UPDATE, mnt_debug_h(upd, "free"));

	mnt_free_fs(upd->fs);
	mnt_free_table(upd->mountinfo);
	free(upd->target);
	free(upd->filename);
	free(upd);
}

/*
 * Returns 0 on success, <0 in case of error.
 */
int mnt_update_set_filename(struct libmnt_update *upd, const char *filename,
			    int userspace_only)
{
	const char *path = NULL;
	int rw = 0;

	assert(upd);

	/* filename explicitly defined */
	if (filename) {
		char *p = strdup(filename);
		if (!p)
			return -ENOMEM;

		upd->userspace_only = userspace_only;
		free(upd->filename);
		upd->filename = p;
	}

	if (upd->filename)
		return 0;

	/* detect tab filename -- /etc/mtab or /run/mount/utab
	 */
	mnt_has_regular_mtab(&path, &rw);
	if (!rw) {
		path = NULL;
		mnt_has_regular_utab(&path, &rw);
		if (!rw)
			return -EACCES;
		upd->userspace_only = TRUE;
	}
	upd->filename = strdup(path);
	if (!upd->filename)
		return -ENOMEM;

	return 0;
}

/**
 * mnt_update_get_filename:
 * @upd: update
 *
 * This function returns file name (e.g. /etc/mtab) for the up-dated file.
 *
 * Returns: pointer to filename that will be updated or NULL in case of error.
 */
const char *mnt_update_get_filename(struct libmnt_update *upd)
{
	return upd ? upd->filename : NULL;
}

/**
 * mnt_update_is_ready:
 * @upd: update handler
 *
 * Returns: 1 if entry described by @upd is successfully prepared and will be
 * written to mtab/utab file.
 */
int mnt_update_is_ready(struct libmnt_update *upd)
{
	return upd ? upd->ready : FALSE;
}

/**
 * mnt_update_set_fs:
 * @upd: update handler
 * @mountflags: MS_* flags
 * @target: umount target, must be NULL for mount
 * @fs: mount filesystem description, must be NULL for umount
 *
 * Returns: <0 in case on error, 0 on success, 1 if update is unnecessary.
 */
int mnt_update_set_fs(struct libmnt_update *upd, unsigned long mountflags,
		      const char *target, struct libmnt_fs *fs)
{
	int rc;

	assert(upd);
	assert(target || fs);

	if (!upd)
		return -EINVAL;
	if ((mountflags & MS_MOVE) && (!fs || !mnt_fs_get_srcpath(fs)))
		return -EINVAL;
	if (target && fs)
		return -EINVAL;

	DBG(UPDATE, mnt_debug_h(upd,
			"resetting FS [fs=0x%p, target=%s, flags=0x%08lx]",
			fs, target, mountflags));
	if (fs) {
		DBG(UPDATE, mnt_debug_h(upd, "FS template:"));
		DBG(UPDATE, mnt_fs_print_debug(fs, stderr));
	}

	mnt_free_fs(upd->fs);
	free(upd->target);
	upd->ready = FALSE;
	upd->fs = NULL;
	upd->target = NULL;
	upd->mountflags = 0;

	if (mountflags & MS_PROPAGATION)
		return 1;

	upd->mountflags = mountflags;

	rc = mnt_update_set_filename(upd, NULL, 0);
	if (rc) {
		DBG(UPDATE, mnt_debug_h(upd, "no writable file available [rc=%d]", rc));
		return rc;	/* error or no file available (rc = 1) */
	}
	if (target) {
		upd->target = strdup(target);
		if (!upd->target)
			return -ENOMEM;

	} else if (fs) {
		if (upd->userspace_only && !(mountflags & MS_MOVE)) {
			int rc = utab_new_entry(upd, fs, mountflags);
			if (rc)
				return rc;
		} else {
			upd->fs = mnt_copy_mtab_fs(fs);
			if (!upd->fs)
				return -ENOMEM;

		}
	}


	DBG(UPDATE, mnt_debug_h(upd, "ready"));
	upd->ready = TRUE;
	return 0;
}

/**
 * mnt_update_get_fs:
 * @upd: update
 *
 * Returns: update filesystem entry or NULL
 */
struct libmnt_fs *mnt_update_get_fs(struct libmnt_update *upd)
{
	return upd ? upd->fs : NULL;
}

/**
 * mnt_update_get_mflags:
 * @upd: update
 *
 * Returns: mount flags as was set by mnt_update_set_fs()
 */
unsigned long mnt_update_get_mflags(struct libmnt_update *upd)
{
	return upd ? upd->mountflags : 0;
}

/**
 * mnt_update_force_rdonly:
 * @upd: update
 * @rdonly: is read-only?
 *
 * Returns: 0 on success and negative number in case of error.
 */
int mnt_update_force_rdonly(struct libmnt_update *upd, int rdonly)
{
	int rc = 0;

	if (!upd || !upd->fs)
		return -EINVAL;

	if (rdonly && (upd->mountflags & MS_RDONLY))
		return 0;
	if (!rdonly && !(upd->mountflags & MS_RDONLY))
		return 0;

	if (!upd->userspace_only) {
		/* /etc/mtab -- we care about VFS options there */
		const char *o = mnt_fs_get_options(upd->fs);
		char *n = o ? strdup(o) : NULL;

		if (n)
			mnt_optstr_remove_option(&n, rdonly ? "rw" : "ro");
		if (!mnt_optstr_prepend_option(&n, rdonly ? "ro" : "rw", NULL))
			rc = mnt_fs_set_options(upd->fs, n);

		free(n);
	}

	if (rdonly)
		upd->mountflags &= ~MS_RDONLY;
	else
		upd->mountflags |= MS_RDONLY;

	return rc;
}

/*
 * Allocates utab entry (upd->fs) for mount/remount. This function should be
 * called *before* mount(2) syscall. The @fs is used as a read-only template.
 *
 * Returns: 0 on success, negative number on error, 1 if utabs update is
 *          unnecessary.
 */
static int utab_new_entry(struct libmnt_update *upd, struct libmnt_fs *fs,
			  unsigned long mountflags)
{
	int rc = 0;
	const char *o = NULL, *a = NULL;
	char *u = NULL;

	assert(fs);
	assert(upd);
	assert(upd->fs == NULL);
	assert(!(mountflags & MS_MOVE));

	DBG(UPDATE, mnt_debug("prepare utab entry"));

	o = mnt_fs_get_user_options(fs);
	a = mnt_fs_get_attributes(fs);
	upd->fs = NULL;

	if (o) {
		/* remove non-mtab options */
		rc = mnt_optstr_get_options(o, &u,
				mnt_get_builtin_optmap(MNT_USERSPACE_MAP),
				MNT_NOMTAB);
		if (rc)
			goto err;
	}

	if (!u && !a) {
		DBG(UPDATE, mnt_debug("utab entry unnecessary (no options)"));
		return 1;
	}

	/* allocate the entry */
	upd->fs = mnt_copy_fs(NULL, fs);
	if (!upd->fs) {
		rc = -ENOMEM;
		goto err;
	}

	rc = mnt_fs_set_options(upd->fs, u);
	if (rc)
		goto err;
	rc = mnt_fs_set_attributes(upd->fs, a);
	if (rc)
		goto err;

	if (!(mountflags & MS_REMOUNT)) {
		rc = set_fs_root(upd, fs, mountflags);
		if (rc)
			goto err;
	}

	free(u);
	DBG(UPDATE, mnt_debug("utab entry OK"));
	return 0;
err:
	free(u);
	mnt_free_fs(upd->fs);
	upd->fs = NULL;
	return rc;
}

/*
 * Sets fs-root and fs-type to @upd->fs according to the @fs template and
 * @mountfalgs. For MS_BIND mountflag it reads information about source
 * filesystem from /proc/self/mountinfo.
 */
static int set_fs_root(struct libmnt_update *upd, struct libmnt_fs *fs,
		       unsigned long mountflags)
{
	struct libmnt_fs *src_fs;
	char *fsroot = NULL;
	const char *src;
	int rc = 0;

	DBG(UPDATE, mnt_debug("setting FS root"));

	assert(upd);
	assert(upd->fs);
	assert(fs);

	if (mountflags & MS_BIND) {
		if (!upd->mountinfo)
			upd->mountinfo = mnt_new_table_from_file(_PATH_PROC_MOUNTINFO);

		src = mnt_fs_get_srcpath(fs);
		if (src) {
			 rc = mnt_fs_set_bindsrc(upd->fs, src);
			 if (rc)
				 goto err;
		}
	}

	src_fs = mnt_table_get_fs_root(upd->mountinfo, fs,
					mountflags, &fsroot);
	if (src_fs) {
		src = mnt_fs_get_srcpath(src_fs);
		rc = mnt_fs_set_source(upd->fs, src);
		if (rc)
			goto err;

		mnt_fs_set_fstype(upd->fs, mnt_fs_get_fstype(src_fs));
	}

	upd->fs->root = fsroot;
	return 0;
err:
	free(fsroot);
	return rc;
}

/* mtab and fstab update -- returns zero on success
 */
static int fprintf_mtab_fs(FILE *f, struct libmnt_fs *fs)
{
	const char *o, *src, *fstype;
	char *m1, *m2, *m3, *m4;
	int rc;

	assert(fs);
	assert(f);

	src = mnt_fs_get_source(fs);
	fstype = mnt_fs_get_fstype(fs);
	o = mnt_fs_get_options(fs);

	m1 = src ? mangle(src) : "none";
	m2 = mangle(mnt_fs_get_target(fs));
	m3 = fstype ? mangle(fstype) : "none";
	m4 = o ? mangle(o) : "rw";

	if (m1 && m2 && m3 && m4) {
		rc = fprintf(f, "%s %s %s %s %d %d\n",
				m1, m2, m3, m4,
				mnt_fs_get_freq(fs),
				mnt_fs_get_passno(fs));
		if (rc > 0)
			rc = 0;
	} else
		rc = -ENOMEM;

	if (src)
		free(m1);
	free(m2);
	if (fstype)
		free(m3);
	if (o)
		free(m4);

	return rc;
}

static int fprintf_utab_fs(FILE *f, struct libmnt_fs *fs)
{
	char *p;
	int rc = 0;

	assert(fs);
	assert(f);

	if (!fs || !f)
		return -EINVAL;

	p = mangle(mnt_fs_get_source(fs));
	if (p) {
		rc = fprintf(f, "SRC=%s ", p);
		free(p);
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_target(fs));
		if (p) {
			rc = fprintf(f, "TARGET=%s ", p);
			free(p);
		}
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_root(fs));
		if (p) {
			rc = fprintf(f, "ROOT=%s ", p);
			free(p);
		}
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_bindsrc(fs));
		if (p) {
			rc = fprintf(f, "BINDSRC=%s ", p);
			free(p);
		}
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_attributes(fs));
		if (p) {
			rc = fprintf(f, "ATTRS=%s ", p);
			free(p);
		}
	}
	if (rc >= 0) {
		p = mangle(mnt_fs_get_user_options(fs));
		if (p) {
			rc = fprintf(f, "OPTS=%s", p);
			free(p);
		}
	}
	if (rc >= 0)
		rc = fprintf(f, "\n");

	if (rc > 0)
		rc = 0;	/* success */
	return rc;
}

static int update_table(struct libmnt_update *upd, struct libmnt_table *tb)
{
	FILE *f;
	int rc, fd;
	char *uq = NULL;

	if (!tb || !upd->filename)
		return -EINVAL;

	DBG(UPDATE, mnt_debug_h(upd, "%s: updating", upd->filename));

	fd = mnt_open_uniq_filename(upd->filename, &uq);
	if (fd < 0)
		return fd;	/* error */

	f = fdopen(fd, "w");
	if (f) {
		struct stat st;
		struct libmnt_iter itr;
		struct libmnt_fs *fs;

		mnt_reset_iter(&itr, MNT_ITER_FORWARD);
		while(mnt_table_next_fs(tb, &itr, &fs) == 0) {
			if (upd->userspace_only)
				rc = fprintf_utab_fs(f, fs);
			else
				rc = fprintf_mtab_fs(f, fs);
			if (rc) {
				DBG(UPDATE, mnt_debug_h(upd,
					"%s: write entry failed: %m", uq));
				goto leave;
			}
		}

		if (fflush(f) != 0) {
			rc = -errno;
			DBG(UPDATE, mnt_debug_h(upd, "%s: fflush failed: %m", uq));
			goto leave;
		}

		rc = fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) ? -errno : 0;

		if (!rc && stat(upd->filename, &st) == 0)
			/* Copy uid/gid from the present file before renaming. */
			rc = fchown(fd, st.st_uid, st.st_gid) ? -errno : 0;

		fclose(f);
		f = NULL;

		if (!rc)
			rc = rename(uq, upd->filename) ? -errno : 0;
	} else {
		rc = -errno;
		close(fd);
	}

leave:
	if (f)
		fclose(f);

	unlink(uq);	/* be paranoid */
	free(uq);
	return rc;
}

static int update_add_entry(struct libmnt_update *upd, struct libmnt_lock *lc)
{
	struct libmnt_table *tb;
	int rc = 0;

	assert(upd);
	assert(upd->fs);

	DBG(UPDATE, mnt_debug_h(upd, "%s: add entry", upd->filename));

	if (lc)
		rc = mnt_lock_file(lc);
	if (rc)
		return rc;

	tb = __mnt_new_table_from_file(upd->filename,
			upd->userspace_only ? MNT_FMT_UTAB : MNT_FMT_MTAB);
	if (tb) {
		struct libmnt_fs *fs = mnt_copy_fs(NULL, upd->fs);
		if (!fs)
			rc = -ENOMEM;
		else {
			mnt_table_add_fs(tb, fs);
			rc = update_table(upd, tb);
		}
	}

	if (lc)
		mnt_unlock_file(lc);

	mnt_free_table(tb);
	return rc;
}

static int update_remove_entry(struct libmnt_update *upd, struct libmnt_lock *lc)
{
	struct libmnt_table *tb;
	int rc = 0;

	assert(upd);
	assert(upd->target);

	DBG(UPDATE, mnt_debug_h(upd, "%s: remove entry", upd->filename));

	if (lc)
		rc = mnt_lock_file(lc);
	if (rc)
		return rc;

	tb = __mnt_new_table_from_file(upd->filename,
			upd->userspace_only ? MNT_FMT_UTAB : MNT_FMT_MTAB);
	if (tb) {
		struct libmnt_fs *rem = mnt_table_find_target(tb, upd->target, MNT_ITER_BACKWARD);
		if (rem) {
			mnt_table_remove_fs(tb, rem);
			rc = update_table(upd, tb);
			mnt_free_fs(rem);
		}
	}

	if (lc)
		mnt_unlock_file(lc);

	mnt_free_table(tb);
	return rc;
}

static int update_modify_target(struct libmnt_update *upd, struct libmnt_lock *lc)
{
	struct libmnt_table *tb = NULL;
	int rc = 0;

	DBG(UPDATE, mnt_debug_h(upd, "%s: modify target", upd->filename));

	if (lc)
		rc = mnt_lock_file(lc);
	if (rc)
		return rc;

	tb = __mnt_new_table_from_file(upd->filename,
			upd->userspace_only ? MNT_FMT_UTAB : MNT_FMT_MTAB);
	if (tb) {
		struct libmnt_fs *cur = mnt_table_find_target(tb,
				mnt_fs_get_srcpath(upd->fs), MNT_ITER_BACKWARD);
		if (cur) {
			rc = mnt_fs_set_target(cur, mnt_fs_get_target(upd->fs));
			if (!rc)
				rc = update_table(upd, tb);
		}
	}

	if (lc)
		mnt_unlock_file(lc);

	mnt_free_table(tb);
	return rc;
}

static int update_modify_options(struct libmnt_update *upd, struct libmnt_lock *lc)
{
	struct libmnt_table *tb = NULL;
	int rc = 0;
	struct libmnt_fs *fs;

	assert(upd);
	assert(upd->fs);

	DBG(UPDATE, mnt_debug_h(upd, "%s: modify options", upd->filename));

	fs = upd->fs;

	if (lc)
		rc = mnt_lock_file(lc);
	if (rc)
		return rc;

	tb = __mnt_new_table_from_file(upd->filename,
			upd->userspace_only ? MNT_FMT_UTAB : MNT_FMT_MTAB);
	if (tb) {
		struct libmnt_fs *cur = mnt_table_find_target(tb,
					mnt_fs_get_target(fs),
					MNT_ITER_BACKWARD);
		if (cur) {
			if (upd->userspace_only)
				rc = mnt_fs_set_attributes(cur,	mnt_fs_get_attributes(fs));
			if (!rc)
				rc = mnt_fs_set_options(cur, mnt_fs_get_options(fs));
			if (!rc)
				rc = update_table(upd, tb);
		}
	}

	if (lc)
		mnt_unlock_file(lc);

	mnt_free_table(tb);
	return rc;
}

/**
 * mnt_update_table:
 * @upd: update
 * @lc: lock or NULL
 *
 * High-level API to update /etc/mtab (or private /run/mount/utab file).
 *
 * The @lc lock is optional and will be created if necessary. Note that
 * the automatically created lock blocks all signals.
 *
 * See also mnt_lock_block_signals() and mnt_context_get_lock().
 *
 * Returns: 0 on success, negative number on error.
 */
int mnt_update_table(struct libmnt_update *upd, struct libmnt_lock *lc)
{
	struct libmnt_lock *lc0 = lc;
	int rc = -EINVAL;

	assert(upd);

	if (!upd || !upd->filename)
		return -EINVAL;
	if (!upd->ready)
		return 0;

	DBG(UPDATE, mnt_debug_h(upd, "%s: update tab", upd->filename));
	if (upd->fs) {
		DBG(UPDATE, mnt_fs_print_debug(upd->fs, stderr));
	}
	if (!lc) {
		lc = mnt_new_lock(upd->filename, 0);
		if (lc)
			mnt_lock_block_signals(lc, TRUE);
	}
	if (lc && upd->userspace_only)
		mnt_lock_use_simplelock(lc, TRUE);	/* use flock */

	if (!upd->fs && upd->target)
		rc = update_remove_entry(upd, lc);	/* umount */
	else if (upd->mountflags & MS_MOVE)
		rc = update_modify_target(upd, lc);	/* move */
	else if (upd->mountflags & MS_REMOUNT)
		rc = update_modify_options(upd, lc);	/* remount */
	else if (upd->fs)
		rc = update_add_entry(upd, lc);	/* mount */

	upd->ready = FALSE;
	DBG(UPDATE, mnt_debug_h(upd, "%s: update tab: done [rc=%d]",
				upd->filename, rc));
	if (lc != lc0)
		 mnt_free_lock(lc);
	return rc;
}

#ifdef TEST_PROGRAM

static int update(const char *target, struct libmnt_fs *fs, unsigned long mountflags)
{
	int rc;
	struct libmnt_update *upd;

	DBG(UPDATE, mnt_debug("update test"));

	upd = mnt_new_update();
	if (!upd)
		return -ENOMEM;

	rc = mnt_update_set_fs(upd, mountflags, target, fs);
	if (rc == 1) {
		/* update is unnecessary */
		rc = 0;
		goto done;
	}
	if (rc) {
		fprintf(stderr, "failed to set FS\n");
		goto done;
	}

	/* [... here should be mount(2) call ...]  */

	rc = mnt_update_table(upd, NULL);
done:
	mnt_free_update(upd);
	return rc;
}

static int test_add(struct libmnt_test *ts, int argc, char *argv[])
{
	struct libmnt_fs *fs = mnt_new_fs();
	int rc;

	if (argc < 5 || !fs)
		return -1;
	mnt_fs_set_source(fs, argv[1]);
	mnt_fs_set_target(fs, argv[2]);
	mnt_fs_set_fstype(fs, argv[3]);
	mnt_fs_set_options(fs, argv[4]);

	rc = update(NULL, fs, 0);
	mnt_free_fs(fs);
	return rc;
}

static int test_remove(struct libmnt_test *ts, int argc, char *argv[])
{
	if (argc < 2)
		return -1;
	return update(argv[1], NULL, 0);
}

static int test_move(struct libmnt_test *ts, int argc, char *argv[])
{
	struct libmnt_fs *fs = mnt_new_fs();
	int rc;

	if (argc < 3)
		return -1;
	mnt_fs_set_source(fs, argv[1]);
	mnt_fs_set_target(fs, argv[2]);

	rc = update(NULL, fs, MS_MOVE);

	mnt_free_fs(fs);
	return rc;
}

static int test_remount(struct libmnt_test *ts, int argc, char *argv[])
{
	struct libmnt_fs *fs = mnt_new_fs();
	int rc;

	if (argc < 3)
		return -1;
	mnt_fs_set_target(fs, argv[1]);
	mnt_fs_set_options(fs, argv[2]);

	rc = update(NULL, fs, MS_REMOUNT);
	mnt_free_fs(fs);
	return rc;
}

int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
	{ "--add",    test_add,     "<src> <target> <type> <options>  add line to mtab" },
	{ "--remove", test_remove,  "<target>                      MS_REMOUNT mtab change" },
	{ "--move",   test_move,    "<old_target>  <target>        MS_MOVE mtab change" },
	{ "--remount",test_remount, "<target>  <options>           MS_REMOUNT mtab change" },
	{ NULL }
	};

	return mnt_run_test(tss, argc, argv);
}

#endif /* TEST_PROGRAM */
