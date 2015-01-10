/*
 * Copyright (C) 2010,2011,2012 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: context
 * @title: Library high-level context
 * @short_description: high-level API to mount/umount devices.
 *
 * <informalexample>
 *   <programlisting>
 *	struct libmnt_context *cxt = mnt_new_context();
 *
 *	mnt_context_set_options(cxt, "aaa,bbb,ccc=CCC");
 *	mnt_context_set_mflags(cxt, MS_NOATIME|MS_NOEXEC);
 *	mnt_context_set_target(cxt, "/mnt/foo");
 *
 *	if (!mnt_context_mount(cxt))
 *		printf("successfully mounted\n");
 *	mnt_free_context(cxt);
 *
 *   </programlisting>
 * </informalexample>
 *
 * This code is similar to:
 *
 *   mount -o aaa,bbb,ccc=CCC,noatime,noexec /mnt/foo
 *
 */

#include "mountP.h"

#include <sys/wait.h>

/**
 * mnt_new_context:
 *
 * Returns: newly allocated mount context
 */
struct libmnt_context *mnt_new_context(void)
{
	struct libmnt_context *cxt;
	uid_t ruid, euid;

	cxt = calloc(1, sizeof(*cxt));
	if (!cxt)
		return NULL;

	ruid = getuid();
	euid = geteuid();

	mnt_context_reset_status(cxt);

	cxt->loopdev_fd = -1;

	/* if we're really root and aren't running setuid */
	cxt->restricted = (uid_t) 0 == ruid && ruid == euid ? 0 : 1;

	DBG(CXT, mnt_debug_h(cxt, "----> allocate %s",
				cxt->restricted ? "[RESTRICTED]" : ""));

	mnt_has_regular_mtab(&cxt->mtab_path, &cxt->mtab_writable);

	if (!cxt->mtab_writable)
		/* use /run/mount/utab if /etc/mtab is useless */
		mnt_has_regular_utab(&cxt->utab_path, &cxt->utab_writable);

	return cxt;
}

/**
 * mnt_free_context:
 * @cxt: mount context
 *
 * Deallocates context struct.
 */
void mnt_free_context(struct libmnt_context *cxt)
{
	if (!cxt)
		return;

	mnt_reset_context(cxt);

	free(cxt->fstype_pattern);
	free(cxt->optstr_pattern);

	if (!(cxt->flags & MNT_FL_EXTERN_FSTAB))
		mnt_free_table(cxt->fstab);
	if (!(cxt->flags & MNT_FL_EXTERN_CACHE))
		mnt_free_cache(cxt->cache);

	mnt_context_clear_loopdev(cxt);
	mnt_free_lock(cxt->lock);
	mnt_free_update(cxt->update);

	free(cxt->children);

	DBG(CXT, mnt_debug_h(cxt, "<---- free"));
	free(cxt);
}

/**
 * mnt_reset_context:
 * @cxt: mount context
 *
 * Resets all information in the context that are directly related to
 * the latest mount (spec, source, target, mount options, ....)
 *
 * The match patters, cached fstab, cached canonicalized paths and tags and
 * [e]uid are not reseted. You have to use
 *
 *	mnt_context_set_fstab(cxt, NULL);
 *	mnt_context_set_cache(cxt, NULL);
 *	mnt_context_set_fstype_pattern(cxt, NULL);
 *	mnt_context_set_options_pattern(cxt, NULL);
 *
 *
 * to reset these stuff.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_reset_context(struct libmnt_context *cxt)
{
	int fl;

	if (!cxt)
		return -EINVAL;

	DBG(CXT, mnt_debug_h(cxt, "<---- reset [status=%d] ---->",
				mnt_context_get_status(cxt)));

	fl = cxt->flags;

	if (!(cxt->flags & MNT_FL_EXTERN_FS))
		mnt_free_fs(cxt->fs);

	mnt_free_table(cxt->mtab);

	free(cxt->helper);
	free(cxt->orig_user);

	cxt->fs = NULL;
	cxt->mtab = NULL;
	cxt->ambi = 0;
	cxt->helper = NULL;
	cxt->orig_user = NULL;
	cxt->mountflags = 0;
	cxt->user_mountflags = 0;
	cxt->mountdata = NULL;
	cxt->flags = MNT_FL_DEFAULT;

	mnt_context_reset_status(cxt);

	/* restore non-resetable flags */
	cxt->flags |= (fl & MNT_FL_EXTERN_FSTAB);
	cxt->flags |= (fl & MNT_FL_EXTERN_CACHE);
	cxt->flags |= (fl & MNT_FL_NOMTAB);
	cxt->flags |= (fl & MNT_FL_FAKE);
	cxt->flags |= (fl & MNT_FL_SLOPPY);
	cxt->flags |= (fl & MNT_FL_VERBOSE);
	cxt->flags |= (fl & MNT_FL_NOHELPERS);
	cxt->flags |= (fl & MNT_FL_LOOPDEL);
	cxt->flags |= (fl & MNT_FL_LAZY);
	cxt->flags |= (fl & MNT_FL_FORK);
	cxt->flags |= (fl & MNT_FL_FORCE);
	cxt->flags |= (fl & MNT_FL_NOCANONICALIZE);
	cxt->flags |= (fl & MNT_FL_RDONLY_UMOUNT);
	return 0;
}

/**
 * mnt_context_reset_status:
 * @cxt: context
 *
 * Resets mount(2) and mount.type statuses, so mnt_context_do_mount() or
 * mnt_context_do_umount() could be again called with the same settings.
 *
 * BE CAREFUL -- after this soft reset the libmount will NOT parse mount
 * options, evaluate permissions or apply stuff from fstab.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_reset_status(struct libmnt_context *cxt)
{
	if (!cxt)
		return -EINVAL;

	cxt->syscall_status = 1;		/* means not called yet */
	cxt->helper_exec_status = 1;
	cxt->helper_status = 0;
	return 0;
}

static int set_flag(struct libmnt_context *cxt, int flag, int enable)
{
	if (!cxt)
		return -EINVAL;
	if (enable) {
		DBG(CXT, mnt_debug_h(cxt, "enabling flag %04x", flag));
		cxt->flags |= flag;
	} else {
		DBG(CXT, mnt_debug_h(cxt, "disabling flag %04x", flag));
		cxt->flags &= ~flag;
	}
	return 0;
}

/**
 * mnt_context_is_restricted:
 * @cxt: mount context
 *
 * Returns: 0 for unrestricted mount (user is root), or 1 for non-root mounts
 */
int mnt_context_is_restricted(struct libmnt_context *cxt)
{
	assert(cxt);
	return cxt->restricted;
}

/**
 * mnt_context_set_optsmode
 * @cxt: mount context
 * @mode: MNT_OMASK_* flags
 *
 * Controls how to use mount optionsmsource and target paths from fstab/mtab.
 *
 * @MNT_OMODE_IGNORE: ignore mtab/fstab options
 *
 * @MNT_OMODE_APPEND: append mtab/fstab options to existing options
 *
 * @MNT_OMODE_PREPEND: prepend mtab/fstab options to existing options
 *
 * @MNT_OMODE_REPLACE: replace existing options with options from mtab/fstab
 *
 * @MNT_OMODE_FORCE: always read mtab/fstab (although source and target is defined)
 *
 * @MNT_OMODE_FSTAB: read from fstab
 *
 * @MNT_OMODE_MTAB: read from mtab if fstab not enabled or failed
 *
 * @MNT_OMODE_NOTAB: do not read fstab/mtab at all
 *
 * @MNT_OMODE_AUTO: default mode (MNT_OMODE_PREPEND | MNT_OMODE_FSTAB | MNT_OMODE_MTAB)
 *
 * @MNT_OMODE_USER: default for non-root users (MNT_OMODE_REPLACE | MNT_OMODE_FORCE | MNT_OMODE_FSTAB)
 *
 * Notes:
 *
 * - MNT_OMODE_USER is always used if mount context is in restricted mode
 * - MNT_OMODE_AUTO is used if nothing other is defined
 * - the flags are eavaluated in this order: MNT_OMODE_NOTAB, MNT_OMODE_FORCE,
 *   MNT_OMODE_FSTAB, MNT_OMODE_MTAB and then the mount options from fstab/mtab
 *   are set according to MNT_OMODE_{IGNORE,APPEND,PREPAND,REPLACE}
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_optsmode(struct libmnt_context *cxt, int mode)
{
	if (!cxt)
		return -EINVAL;
	cxt->optsmode = mode;
	return 0;
}

/**
 * mnt_context_get_optsmode
 * @cxt: mount context
 *
 * Returns: MNT_OMASK_* mask or zero.
 */

int mnt_context_get_optsmode(struct libmnt_context *cxt)
{
	return cxt ? cxt->optsmode : 0;
}

/**
 * mnt_context_disable_canonicalize:
 * @cxt: mount context
 * @disable: TRUE or FALSE
 *
 * Enable/disable paths canonicalization and tags evaluation. The libmount context
 * canonicalies paths when search in fstab and when prepare source and target paths
 * for mount(2) syscall.
 *
 * This fuction has effect to the private fstab instance only (see
 * mnt_context_set_fstab()). If you want to use an external fstab then you need
 * manage your private struct libmnt_cache (see mnt_table_set_cache(fstab, NULL).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_disable_canonicalize(struct libmnt_context *cxt, int disable)
{
	return set_flag(cxt, MNT_FL_NOCANONICALIZE, disable);
}

/**
 * mnt_context_enable_lazy:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable lazy umount (see umount(8) man page, option -l).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_lazy(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_LAZY, enable);
}

/**
 * mnt_context_enable_fork:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable fork(2) call in mnt_context_next_mount() (see mount(8) man
 * page, option -F).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_fork(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_FORK, enable);
}

/**
 * mnt_context_is_lazy:
 * @cxt: mount context
 *
 * Returns: 1 if lazy umount is enabled or 0
 */
int mnt_context_is_lazy(struct libmnt_context *cxt)
{
	return cxt && (cxt->flags & MNT_FL_LAZY) ? 1 : 0;
}


/**
 * mnt_context_enable_rdonly_umount:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable read-only remount on failed umount(2)
 * (see umount(8) man page, option -r).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_rdonly_umount(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_RDONLY_UMOUNT, enable);
}

/**
 * mnt_context_is_rdonly_umount
 * @cxt: mount context
 *
 * See also mnt_context_enable_rdonly_umount() and see umount(8) man page,
 * option -r.
 *
 * Returns: 1 if read-only remount failed umount(2) is enables or 0
 */
int mnt_context_is_rdonly_umount(struct libmnt_context *cxt)
{
	return cxt && (cxt->flags & MNT_FL_RDONLY_UMOUNT) ? 1 : 0;
}

/**
 * mnt_context_disable_helpers:
 * @cxt: mount context
 * @disable: TRUE or FALSE
 *
 * Enable/disable /sbin/[u]mount.* helpers (see mount(8) man page, option -i).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_disable_helpers(struct libmnt_context *cxt, int disable)
{
	return set_flag(cxt, MNT_FL_NOHELPERS, disable);
}

/**
 * mnt_context_enable_sloppy:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Set/unset sloppy mounting (see mount(8) man page, option -s).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_sloppy(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_SLOPPY, enable);
}

/**
 * mnt_context_is_sloppy:
 * @cxt: mount context
 *
 * Returns: 1 if sloppy flag is enabled or 0
 */
int mnt_context_is_sloppy(struct libmnt_context *cxt)
{
	return cxt && (cxt->flags & MNT_FL_SLOPPY) ? 1 : 0;
}

/**
 * mnt_context_enable_fake:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable fake mounting (see mount(8) man page, option -f).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_fake(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_FAKE, enable);
}

/**
 * mnt_context_is_fake:
 * @cxt: mount context
 *
 * Returns: 1 if fake flag is enabled or 0
 */
int mnt_context_is_fake(struct libmnt_context *cxt)
{
	return cxt && (cxt->flags & MNT_FL_FAKE) ? 1 : 0;
}

/**
 * mnt_context_disable_mtab:
 * @cxt: mount context
 * @disable: TRUE or FALSE
 *
 * Disable/enable mtab update (see mount(8) man page, option -n).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_disable_mtab(struct libmnt_context *cxt, int disable)
{
	return set_flag(cxt, MNT_FL_NOMTAB, disable);
}

/**
 * mnt_context_is_nomtab
 * @cxt: mount context
 *
 * Returns: 1 if no-mtab is enabled or 0
 */
int mnt_context_is_nomtab(struct libmnt_context *cxt)
{
	return cxt && (cxt->flags & MNT_FL_NOMTAB) ? 1 : 0;
}

/**
 * mnt_context_enable_force:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable force umounting (see umount(8) man page, option -f).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_force(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_FORCE, enable);
}

/**
 * mnt_context_is_force
 * @cxt: mount context
 *
 * Returns: 1 if force umounting flag is enabled or 0
 */
int mnt_context_is_force(struct libmnt_context *cxt)
{
	return cxt && (cxt->flags & MNT_FL_FORCE) ? 1 : 0;
}

/**
 * mnt_context_enable_verbose:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable verbose output (TODO: not implemented yet)
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_verbose(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_VERBOSE, enable);
}

/**
 * mnt_context_is_verbose
 * @cxt: mount context
 *
 * Returns: 1 if verbose flag is enabled or 0
 */
int mnt_context_is_verbose(struct libmnt_context *cxt)
{
	return cxt && (cxt->flags & MNT_FL_VERBOSE) ? 1 : 0;
}

/**
 * mnt_context_enable_loopdel:
 * @cxt: mount context
 * @enable: TRUE or FALSE
 *
 * Enable/disable loop delete (destroy) after umount (see umount(8), option -d)
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_enable_loopdel(struct libmnt_context *cxt, int enable)
{
	return set_flag(cxt, MNT_FL_LOOPDEL, enable);
}

/**
 * mnt_context_set_fs:
 * @cxt: mount context
 * @fs: filesystem description
 *
 * The mount context uses private @fs by default. This function allows to
 * overwrite the private @fs with an external instance. Note that the external
 * @fs instance is not deallocated by mnt_free_context() or mnt_reset_context().
 *
 * The @fs will be modified by mnt_context_set_{source,target,options,fstype}
 * functions, If the @fs is NULL then all current FS specific setting (source,
 * target, etc., exclude spec) is reseted.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_fs(struct libmnt_context *cxt, struct libmnt_fs *fs)
{
	if (!cxt)
		return -EINVAL;
	if (!(cxt->flags & MNT_FL_EXTERN_FS))
		mnt_free_fs(cxt->fs);

	set_flag(cxt, MNT_FL_EXTERN_FS, fs != NULL);
	cxt->fs = fs;
	return 0;
}

/**
 * mnt_context_get_fs:
 * @cxt: mount context
 *
 * The FS contains the basic description of mountpoint, fs type and so on.
 * Note that the FS is modified by mnt_context_set_{source,target,options,fstype}
 * functions.
 *
 * Returns: pointer to FS description or NULL in case of calloc() errrr.
 */
struct libmnt_fs *mnt_context_get_fs(struct libmnt_context *cxt)
{
	if (!cxt)
		return NULL;
	if (!cxt->fs) {
		cxt->fs = mnt_new_fs();
		cxt->flags &= ~MNT_FL_EXTERN_FS;
	}
	return cxt->fs;
}

/**
 * mnt_context_set_source:
 * @cxt: mount context
 * @source: mount source (device, directory, UUID, LABEL, ...)
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_source(struct libmnt_context *cxt, const char *source)
{
	return mnt_fs_set_source(mnt_context_get_fs(cxt), source);
}

/**
 * mnt_context_get_source:
 * @cxt: mount context
 *
 * Returns: returns pointer or NULL in case of error pr if not set.
 */
const char *mnt_context_get_source(struct libmnt_context *cxt)
{
	return mnt_fs_get_source(mnt_context_get_fs(cxt));
}

/**
 * mnt_context_set_target:
 * @cxt: mount context
 * @target: mountpoint
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_target(struct libmnt_context *cxt, const char *target)
{
	return mnt_fs_set_target(mnt_context_get_fs(cxt), target);
}

/**
 * mnt_context_get_target:
 * @cxt: mount context
 *
 * Returns: returns pointer or NULL in case of error pr if not set.
 */
const char *mnt_context_get_target(struct libmnt_context *cxt)
{
	return mnt_fs_get_target(mnt_context_get_fs(cxt));
}

/**
 * mnt_context_set_fstype:
 * @cxt: mount context
 * @fstype: filesystem type
 *
 * Note that the @fstype has to be the real FS type. For comma-separated list of
 * filesystems or for "nofs" notation use mnt_context_set_fstype_pattern().
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_fstype(struct libmnt_context *cxt, const char *fstype)
{
	if (fstype && strchr(fstype, ','))
		return -EINVAL;
	return mnt_fs_set_fstype(mnt_context_get_fs(cxt), fstype);
}

/**
 * mnt_context_get_fstype:
 * @cxt: mount context
 *
 * Returns: returns pointer or NULL in case of error pr if not set.
 */
const char *mnt_context_get_fstype(struct libmnt_context *cxt)
{
	return mnt_fs_get_fstype(mnt_context_get_fs(cxt));
}

/**
 * mnt_context_set_options:
 * @cxt: mount context
 * @optstr: comma delimited mount options
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_options(struct libmnt_context *cxt, const char *optstr)
{
	return mnt_fs_set_options(mnt_context_get_fs(cxt), optstr);
}

/**
 * mnt_context_append_options:
 * @cxt: mount context
 * @optstr: comma delimited mount options
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_append_options(struct libmnt_context *cxt, const char *optstr)
{
	return mnt_fs_append_options(mnt_context_get_fs(cxt), optstr);
}

/**
 * mnt_context_set_fstype_pattern:
 * @cxt: mount context
 * @pattern: FS name pattern (or NULL to reset the current setting)
 *
 * See mount(8), option -t.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_fstype_pattern(struct libmnt_context *cxt, const char *pattern)
{
	char *p = NULL;

	if (!cxt)
		return -EINVAL;
	if (pattern) {
		p = strdup(pattern);
		if (!p)
			return -ENOMEM;
	}
	free(cxt->fstype_pattern);
	cxt->fstype_pattern = p;
	return 0;
}

/**
 * mnt_context_set_options_pattern:
 * @cxt: mount context
 * @pattern: options pattern (or NULL to reset the current setting)
 *
 * See mount(8), option -O.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_options_pattern(struct libmnt_context *cxt, const char *pattern)
{
	char *p = NULL;

	if (!cxt)
		return -EINVAL;
	if (pattern) {
		p = strdup(pattern);
		if (!p)
			return -ENOMEM;
	}
	free(cxt->optstr_pattern);
	cxt->optstr_pattern = p;
	return 0;
}

/**
 * mnt_context_set_fstab:
 * @cxt: mount context
 * @tb: fstab
 *
 * The mount context reads /etc/fstab to the the private struct libmnt_table by default.
 * This function allows to overwrite the private fstab with an external
 * instance. Note that the external instance is not deallocated by mnt_free_context().
 *
 * The fstab is used read-only and is not modified, it should be possible to
 * share the fstab between more mount contexts (TODO: tests it.)
 *
 * If the @tb argument is NULL then the current private fstab instance is
 * reseted.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_fstab(struct libmnt_context *cxt, struct libmnt_table *tb)
{
	if (!cxt)
		return -EINVAL;
	if (!(cxt->flags & MNT_FL_EXTERN_FSTAB))
		mnt_free_table(cxt->fstab);

	set_flag(cxt, MNT_FL_EXTERN_FSTAB, tb != NULL);
	cxt->fstab = tb;
	return 0;
}

/**
 * mnt_context_get_fstab:
 * @cxt: mount context
 * @tb: returns fstab
 *
 * See also mnt_table_parse_fstab() for more details about fstab.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_fstab(struct libmnt_context *cxt, struct libmnt_table **tb)
{
	struct libmnt_cache *cache;

	if (!cxt)
		return -EINVAL;

	if (!cxt->fstab) {
		int rc;

		cxt->fstab = mnt_new_table();
		if (!cxt->fstab)
			return -ENOMEM;
		if (cxt->table_errcb)
			mnt_table_set_parser_errcb(cxt->fstab, cxt->table_errcb);
		cxt->flags &= ~MNT_FL_EXTERN_FSTAB;
		rc = mnt_table_parse_fstab(cxt->fstab, NULL);
		if (rc)
			return rc;
	}

	cache = mnt_context_get_cache(cxt);

	/*  never touch an external fstab */
	if (!(cxt->flags & MNT_FL_EXTERN_FSTAB))
		mnt_table_set_cache(cxt->fstab, cache);

	if (tb)
		*tb = cxt->fstab;
	return 0;
}

/**
 * mnt_context_get_mtab:
 * @cxt: mount context
 * @tb: returns mtab
 *
 * See also mnt_table_parse_mtab() for more details about mtab/mountinfo. The
 * result will deallocated by mnt_free_context(@cxt).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_mtab(struct libmnt_context *cxt, struct libmnt_table **tb)
{
	struct libmnt_cache *cache;

	if (!cxt)
		return -EINVAL;

	if (!cxt->mtab) {
		int rc;

		cxt->mtab = mnt_new_table();
		if (!cxt->mtab)
			return -ENOMEM;

		if (cxt->table_errcb)
			mnt_table_set_parser_errcb(cxt->mtab, cxt->table_errcb);

		rc = mnt_table_parse_mtab(cxt->mtab, cxt->mtab_path);
		if (rc)
			return rc;
	}

	cache = mnt_context_get_cache(cxt);
	mnt_table_set_cache(cxt->mtab, cache);

	if (tb)
		*tb = cxt->mtab;
	return 0;
}

/**
 * mnt_context_get_table:
 * @cxt: mount context
 * @filename: e.g. /proc/self/mountinfo
 * @tb: returns the table
 *
 * This function allocates a new table and parses the @file. The parser error
 * callback and cache for tags and paths is set according to the @cxt setting.
 * See also mnt_table_parse_file().
 *
 * It's strongly recommended use mnt_context_get_mtab() and
 * mnt_context_get_fstab() functions for mtab and fstab files. This function
 * does not care about LIBMOUNT_* env.variables and does not merge userspace
 * options.
 *
 * The result will NOT be deallocated by mnt_free_context(@cxt).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_table(struct libmnt_context *cxt,
			  const char *filename, struct libmnt_table **tb)
{
	struct libmnt_cache *cache;
	int rc;

	if (!cxt || !tb)
		return -EINVAL;

	*tb = mnt_new_table();
	if (!*tb)
		return -ENOMEM;

	if (cxt->table_errcb)
		mnt_table_set_parser_errcb(*tb, cxt->table_errcb);

	rc = mnt_table_parse_file(*tb, filename);
	if (rc) {
		mnt_free_table(*tb);
		return rc;
	}

	cache = mnt_context_get_cache(cxt);
	if (cache)
		mnt_table_set_cache(*tb, cache);

	return 0;
}

/**
 * mnt_context_set_tables_errcb
 * @cxt: mount context
 * @cb: pointer to callback function
 *
 * The error callback is used for all tab files (e.g. mtab, fstab)
 * parsed within the context.
 *
 * See also mnt_context_get_mtab(),
 *          mnt_context_get_fstab(),
 *          mnt_table_set_parser_errcb().
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_tables_errcb(struct libmnt_context *cxt,
	int (*cb)(struct libmnt_table *tb, const char *filename, int line))
{
	if (!cxt)
		return -EINVAL;

	cxt->table_errcb = cb;
	return 0;
}

/**
 * mnt_context_set_cache:
 * @cxt: mount context
 * @cache: cache instance or nULL
 *
 * The mount context maintains a private struct libmnt_cache by default.  This function
 * allows to overwrite the private cache with an external instance. Note that
 * the external instance is not deallocated by mnt_free_context().
 *
 * If the @cache argument is NULL then the current private cache instance is
 * reseted.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_cache(struct libmnt_context *cxt, struct libmnt_cache *cache)
{
	if (!cxt)
		return -EINVAL;
	if (!(cxt->flags & MNT_FL_EXTERN_CACHE))
		mnt_free_cache(cxt->cache);

	set_flag(cxt, MNT_FL_EXTERN_CACHE, cache != NULL);
	cxt->cache = cache;
	return 0;
}

/**
 * mnt_context_get_cache
 * @cxt: mount context
 *
 * See also mnt_context_set_cache().
 *
 * Returns: pointer to cache or NULL if canonicalization is disabled.
 */
struct libmnt_cache *mnt_context_get_cache(struct libmnt_context *cxt)
{
	if (!cxt || (cxt->flags & MNT_FL_NOCANONICALIZE))
		return NULL;

	if (!cxt->cache) {
		cxt->cache = mnt_new_cache();
		if (!cxt->cache)
			return NULL;
		cxt->flags &= ~MNT_FL_EXTERN_CACHE;
	}
	return cxt->cache;
}

/**
 * mnt_context_set_passwd_cb:
 * @cxt: mount context
 * @get: callback to get password
 * @release: callback to release (delallocate) password
 *
 * Sets callbacks for encryption password (e.g encrypted loopdev)
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_passwd_cb(struct libmnt_context *cxt,
			      char *(*get)(struct libmnt_context *),
			      void (*release)(struct libmnt_context *, char *))
{
	if (!cxt)
		return -EINVAL;

	cxt->pwd_get_cb = get;
	cxt->pwd_release_cb = release;
	return 0;
}

/**
 * mnt_context_get_lock:
 * @cxt: mount context
 *
 * The libmount applications don't have to care about mtab locking, but with a
 * small exception: the application has to be able to remove the lock file when
 * interrupted by signal or signals have to be ignored when the lock is locked.
 *
 * The default behavior is to ignore all signals (except SIGALRM and
 * SIGTRAP for mtab udate) when the lock is locked. If this behavior
 * is unacceptable then use:
 *
 *	lc = mnt_context_get_lock(cxt);
 *	if (lc)
 *		mnt_lock_block_signals(lc, FALSE);
 *
 * and don't forget to call mnt_unlock_file(lc) before exit.
 *
 * Returns: pointer to lock struct or NULL.
 */
struct libmnt_lock *mnt_context_get_lock(struct libmnt_context *cxt)
{
	/*
	 * DON'T call this function within libmount, it will always allocate
	 * the lock. The mnt_update_* functions are able to allocate the lock
	 * only when mtab/utab update is really necessary.
	 */
	if (!cxt || (cxt->flags & MNT_FL_NOMTAB))
		return NULL;

	if (!cxt->lock) {
		cxt->lock = mnt_new_lock(cxt->mtab_writable ?
				cxt->mtab_path : cxt->utab_path, 0);
		if (cxt->lock)
			mnt_lock_block_signals(cxt->lock, TRUE);
	}
	return cxt->lock;
}

/**
 * mnt_context_set_mflags:
 * @cxt: mount context
 * @flags: mount(2) flags (MS_* flags)
 *
 * Sets mount flags (see mount(2) man page).
 *
 * Note that mount context allows to define mount options by mount flags. It
 * means you can for example use
 *
 *	mnt_context_set_mflags(cxt, MS_NOEXEC | MS_NOSUID);
 *
 * rather than
 *
 *	mnt_context_set_options(cxt, "noexec,nosuid");
 *
 * these both calls have the same effect.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_mflags(struct libmnt_context *cxt, unsigned long flags)
{
	if (!cxt)
		return -EINVAL;

	cxt->mountflags = flags;

	if ((cxt->flags & MNT_FL_MOUNTOPTS_FIXED) && cxt->fs)
		/*
		 * the final mount options are already generated, refresh...
		 */
		return mnt_optstr_apply_flags(
				&cxt->fs->vfs_optstr,
				cxt->mountflags,
				mnt_get_builtin_optmap(MNT_LINUX_MAP));

	return 0;
}

/**
 * mnt_context_get_mflags:
 * @cxt: mount context
 * @flags: returns MS_* mount flags
 *
 * Converts mount options string to MS_* flags and bitewise-OR the result with
 * already defined flags (see mnt_context_set_mflags()).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_mflags(struct libmnt_context *cxt, unsigned long *flags)
{
	int rc = 0;
	if (!cxt || !flags)
		return -EINVAL;

	*flags = 0;
	if (!(cxt->flags & MNT_FL_MOUNTFLAGS_MERGED) && cxt->fs) {
		const char *o = mnt_fs_get_options(cxt->fs);
		if (o)
			rc = mnt_optstr_get_flags(o, flags,
				    mnt_get_builtin_optmap(MNT_LINUX_MAP));
	}
	if (!rc)
		*flags |= cxt->mountflags;
	return rc;
}

/**
 * mnt_context_set_user_mflags:
 * @cxt: mount context
 * @flags: mount(2) flags (MNT_MS_* flags, e.g. MNT_MS_LOOP)
 *
 * Sets userspace mount flags.
 *
 * See also notest for mnt_context_set_mflags().
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_user_mflags(struct libmnt_context *cxt, unsigned long flags)
{
	if (!cxt)
		return -EINVAL;
	cxt->user_mountflags = flags;
	return 0;
}

/**
 * mnt_context_get_user_mflags:
 * @cxt: mount context
 * @flags: returns mount flags
 *
 * Converts mount options string to MNT_MS_* flags and bitewise-OR the result
 * with already defined flags (see mnt_context_set_user_mflags()).
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_get_user_mflags(struct libmnt_context *cxt, unsigned long *flags)
{
	int rc = 0;
	if (!cxt || !flags)
		return -EINVAL;

	*flags = 0;
	if (!(cxt->flags & MNT_FL_MOUNTFLAGS_MERGED) && cxt->fs) {
		const char *o = mnt_fs_get_user_options(cxt->fs);
		if (o)
			rc = mnt_optstr_get_flags(o, flags,
				mnt_get_builtin_optmap(MNT_USERSPACE_MAP));
	}
	if (!rc)
		*flags |= cxt->user_mountflags;
	return rc;
}

/**
 * mnt_context_set_mountdata:
 * @cxt: mount context
 * @data: mount(2) data
 *
 * The mount context generates mountdata from mount options by default. This
 * function allows to overwrite this behavior, and @data will be used instead
 * of mount options.
 *
 * The libmount does not deallocated the data by mnt_free_context(). Note that
 * NULL is also valid mount data.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_set_mountdata(struct libmnt_context *cxt, void *data)
{
	if (!cxt)
		return -EINVAL;
	cxt->mountdata = data;
	cxt->flags |= MNT_FL_MOUNTDATA;
	return 0;
}

/*
 * Translates LABEL/UUID/path to mountable path
 */
int mnt_context_prepare_srcpath(struct libmnt_context *cxt)
{
	const char *path = NULL;
	struct libmnt_cache *cache;
	const char *t, *v, *src;
	int rc = 0;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!cxt || !cxt->fs)
		return -EINVAL;

	DBG(CXT, mnt_debug_h(cxt, "preparing source path"));

	src = mnt_fs_get_source(cxt->fs);

	/* ignore filesystems without source or filesystems
	 * where the source is quasi-path (//foo/bar)
	 */
	if (!src || mnt_fs_is_netfs(cxt->fs))
		return 0;

	DBG(CXT, mnt_debug_h(cxt, "srcpath '%s'", src));

	cache = mnt_context_get_cache(cxt);

	if (!mnt_fs_get_tag(cxt->fs, &t, &v)) {
		/*
		 * Source is TAG (evaluate)
		 */
		if (cache)
			path = mnt_resolve_tag(t, v, cache);

		rc = path ? mnt_fs_set_source(cxt->fs, path) : -EINVAL;

	} else if (cache && !mnt_fs_is_pseudofs(cxt->fs)) {
		/*
		 * Source is PATH (canonicalize)
		 */
		path = mnt_resolve_path(src, cache);
		if (path && strcmp(path, src))
			rc = mnt_fs_set_source(cxt->fs, path);
	 }

	if (rc) {
		DBG(CXT, mnt_debug_h(cxt, "failed to prepare srcpath [rc=%d]", rc));
		return rc;
	}

	if (!path)
		path = src;

	if ((cxt->mountflags & (MS_BIND | MS_MOVE | MS_PROPAGATION)) ||
	    mnt_fs_is_pseudofs(cxt->fs)) {
		DBG(CXT, mnt_debug_h(cxt, "PROPAGATION/pseudo FS source: %s", path));
		return rc;
	}

	/*
	 * Initialize loop device
	 */
	if (mnt_context_is_loopdev(cxt)) {
		rc = mnt_context_setup_loopdev(cxt);
		if (rc)
			return rc;
	}

	DBG(CXT, mnt_debug_h(cxt, "final srcpath '%s'",
				mnt_fs_get_source(cxt->fs)));
	return 0;
}

int mnt_context_prepare_target(struct libmnt_context *cxt)
{
	const char *tgt;
	struct libmnt_cache *cache;
	int rc = 0;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!cxt || !cxt->fs)
		return -EINVAL;

	DBG(CXT, mnt_debug_h(cxt, "preparing target path"));

	tgt = mnt_fs_get_target(cxt->fs);
	if (!tgt)
		return 0;

	cache = mnt_context_get_cache(cxt);
	if (cache) {
		char *path = mnt_resolve_path(tgt, cache);
		if (strcmp(path, tgt))
			rc = mnt_fs_set_target(cxt->fs, path);
	}

	if (rc)
		DBG(CXT, mnt_debug_h(cxt, "failed to prepare target '%s'", tgt));
	else
		DBG(CXT, mnt_debug_h(cxt, "final target '%s'",
					mnt_fs_get_target(cxt->fs)));
	return 0;
}

int mnt_context_guess_fstype(struct libmnt_context *cxt)
{
	char *type;
	const char *dev;
	int rc = -EINVAL;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!cxt || !cxt->fs)
		return -EINVAL;

	if (cxt->mountflags & (MS_BIND | MS_MOVE | MS_PROPAGATION))
		goto none;

	type = (char *) mnt_fs_get_fstype(cxt->fs);
	if (type && !strcmp(type, "auto")) {
		mnt_fs_set_fstype(cxt->fs, NULL);
		type = NULL;
	}

	if (type)
		goto done;
	if (cxt->flags & MS_REMOUNT)
		goto none;
	if (cxt->fstype_pattern)
		goto done;

	dev = mnt_fs_get_srcpath(cxt->fs);
	if (!dev)
		goto err;

	if (access(dev, F_OK) == 0) {
		struct libmnt_cache *cache = mnt_context_get_cache(cxt);

		type = mnt_get_fstype(dev, &cxt->ambi, cache);
		if (type) {
			rc = mnt_fs_set_fstype(cxt->fs, type);
			if (!cache)
				free(type);	/* type is not cached */
		}
	} else {
		if (strchr(dev, ':') != NULL)
			rc = mnt_fs_set_fstype(cxt->fs, "nfs");
		else if (!strncmp(dev, "//", 2))
			rc = mnt_fs_set_fstype(cxt->fs, "cifs");
	}
	if (rc)
		goto err;
done:
	DBG(CXT, mnt_debug_h(cxt, "FS type: %s",
				mnt_fs_get_fstype(cxt->fs)));
	return 0;
none:
	return mnt_fs_set_fstype(cxt->fs, "none");
err:
	DBG(CXT, mnt_debug_h(cxt, "failed to detect FS type"));
	return rc;
}

/*
 * The default is to use fstype from cxt->fs, this could be overwritten by
 * @type. The @act is MNT_ACT_{MOUNT,UMOUNT}.
 *
 * Returns: 0 on success or negative number in case of error. Note that success
 * does not mean that there is any usable helper, you have to check cxt->helper.
 */
int mnt_context_prepare_helper(struct libmnt_context *cxt, const char *name,
				const char *type)
{
	char search_path[] = FS_SEARCH_PATH;		/* from config.h */
	char *p = NULL, *path;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!type)
		type = mnt_fs_get_fstype(cxt->fs);

	if ((cxt->flags & MNT_FL_NOHELPERS) || !type ||
	    !strcmp(type, "none") || mnt_fs_is_swaparea(cxt->fs))
		return 0;

	path = strtok_r(search_path, ":", &p);
	while (path) {
		char helper[PATH_MAX];
		struct stat st;
		int rc;

		rc = snprintf(helper, sizeof(helper), "%s/%s.%s",
						path, name, type);
		path = strtok_r(NULL, ":", &p);

		if (rc < 0 || (size_t) rc >= sizeof(helper))
			continue;

		rc = stat(helper, &st);
		if (rc == -1 && errno == ENOENT && strchr(type, '.')) {
			/* If type ends with ".subtype" try without it */
			char *hs = strrchr(helper, '.');
			if (hs)
				*hs = '\0';
			rc = stat(helper, &st);
		}

		DBG(CXT, mnt_debug_h(cxt, "%-25s ... %s", helper,
					rc ? "not found" : "found"));
		if (rc)
			continue;

		free(cxt->helper);
		cxt->helper = strdup(helper);
		if (!cxt->helper)
			return -ENOMEM;
		return 0;
	}

	return 0;
}

int mnt_context_merge_mflags(struct libmnt_context *cxt)
{
	unsigned long fl = 0;
	int rc;

	assert(cxt);

	DBG(CXT, mnt_debug_h(cxt, "merging mount flags"));

	rc = mnt_context_get_mflags(cxt, &fl);
	if (rc)
		return rc;
	cxt->mountflags = fl;

	fl = 0;
	rc = mnt_context_get_user_mflags(cxt, &fl);
	if (rc)
		return rc;
	cxt->user_mountflags = fl;

	DBG(CXT, mnt_debug_h(cxt, "final flags: VFS=%08lx user=%08lx",
			cxt->mountflags, cxt->user_mountflags));

	cxt->flags |= MNT_FL_MOUNTFLAGS_MERGED;
	return 0;
}

/*
 * Prepare /etc/mtab or /run/mount/utab
 */
int mnt_context_prepare_update(struct libmnt_context *cxt)
{
	int rc;
	const char *target;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->action);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	DBG(CXT, mnt_debug_h(cxt, "prepare update"));

	if (cxt->mountflags & MS_PROPAGATION) {
		DBG(CXT, mnt_debug_h(cxt, "skip update: MS_PROPAGATION"));
		return 0;
	}

	target = mnt_fs_get_target(cxt->fs);

	if (cxt->action == MNT_ACT_UMOUNT && target && !strcmp(target, "/"))
		/* Don't try to touch mtab if umounting root FS */
		cxt->flags |= MNT_FL_NOMTAB;

	if (cxt->flags & MNT_FL_NOMTAB) {
		DBG(CXT, mnt_debug_h(cxt, "skip update: NOMTAB flag"));
		return 0;
	}
	if (cxt->helper) {
		DBG(CXT, mnt_debug_h(cxt, "skip update: external helper"));
		return 0;
	}
	if (!cxt->mtab_writable && !cxt->utab_writable) {
		DBG(CXT, mnt_debug_h(cxt, "skip update: no writable destination"));
		return 0;
	}
	/* 0 = success, 1 = not called yet */
	if (cxt->syscall_status != 1 && cxt->syscall_status != 0) {
		DBG(CXT, mnt_debug_h(cxt,
				"skip update: syscall failed [status=%d]",
				cxt->syscall_status));
		return 0;
	}
	if (!cxt->update) {
		cxt->update = mnt_new_update();
		if (!cxt->update)
			return -ENOMEM;

		mnt_update_set_filename(cxt->update,
				cxt->mtab_writable ? cxt->mtab_path : cxt->utab_path,
				!cxt->mtab_writable);
	}

	if (cxt->action == MNT_ACT_UMOUNT)
		rc = mnt_update_set_fs(cxt->update, cxt->mountflags,
					mnt_context_get_target(cxt), NULL);
	else
		rc = mnt_update_set_fs(cxt->update, cxt->mountflags,
					NULL, cxt->fs);

	return rc < 0 ? rc : 0;
}

int mnt_context_update_tabs(struct libmnt_context *cxt)
{
	unsigned long fl;

	assert(cxt);

	if (cxt->flags & MNT_FL_NOMTAB) {
		DBG(CXT, mnt_debug_h(cxt, "don't update: NOMTAB flag"));
		return 0;
	}
	if (cxt->helper) {
		DBG(CXT, mnt_debug_h(cxt, "don't update: external helper"));
		return 0;
	}
	if (!cxt->update || !mnt_update_is_ready(cxt->update)) {
		DBG(CXT, mnt_debug_h(cxt, "don't update: no update prepared"));
		return 0;
	}
	if (cxt->syscall_status) {
		DBG(CXT, mnt_debug_h(cxt, "don't update: syscall failed/not called"));
		return 0;
	}

	fl = mnt_update_get_mflags(cxt->update);
	if ((cxt->mountflags & MS_RDONLY) != (fl & MS_RDONLY))
		/*
		 * fix MS_RDONLY in options
		 */
		mnt_update_force_rdonly(cxt->update,
				cxt->mountflags & MS_RDONLY);

	return mnt_update_table(cxt->update, cxt->lock);
}

static int apply_table(struct libmnt_context *cxt, struct libmnt_table *tb,
		     int direction)
{
	struct libmnt_fs *fs = NULL;
	const char *src = NULL, *tgt = NULL;
	int rc;

	assert(cxt);
	assert(cxt->fs);

	if (!cxt->fs)
		return -EINVAL;

	src = mnt_fs_get_source(cxt->fs);
	tgt = mnt_fs_get_target(cxt->fs);

	if (tgt && src)
		fs = mnt_table_find_pair(tb, src, tgt, direction);
	else {
		if (src)
			fs = mnt_table_find_source(tb, src, direction);
		else if (tgt)
			fs = mnt_table_find_target(tb, tgt, direction);

		if (!fs) {
			/* swap source and target (if @src is not LABEL/UUID),
			 * for example in
			 *
			 *	mount /foo/bar
			 *
			 * the path could be a mountpoint as well as source (for
			 * example bind mount, symlink to device, ...).
			 */
			if (src && !mnt_fs_get_tag(cxt->fs, NULL, NULL))
				fs = mnt_table_find_target(tb, src, direction);
			if (!fs && tgt)
				fs = mnt_table_find_source(tb, tgt, direction);
		}
	}

	if (!fs)
		return -EINVAL;

	DBG(CXT, mnt_debug_h(cxt, "apply entry:"));
	DBG(CXT, mnt_fs_print_debug(fs, stderr));

	/* copy from tab to our FS description
	 */
	rc = mnt_fs_set_source(cxt->fs, mnt_fs_get_source(fs));
	if (!rc)
		rc = mnt_fs_set_target(cxt->fs, mnt_fs_get_target(fs));

	if (!rc && !mnt_fs_get_fstype(cxt->fs))
		rc = mnt_fs_set_fstype(cxt->fs, mnt_fs_get_fstype(fs));

	if (rc)
		return rc;

	if (cxt->optsmode & MNT_OMODE_IGNORE)
		;
	else if (cxt->optsmode & MNT_OMODE_REPLACE)
		rc = mnt_fs_set_options(cxt->fs, mnt_fs_get_options(fs));

	else if (cxt->optsmode & MNT_OMODE_APPEND)
		rc = mnt_fs_append_options(cxt->fs, mnt_fs_get_options(fs));

	else if (cxt->optsmode & MNT_OMODE_PREPEND)
		rc = mnt_fs_prepend_options(cxt->fs, mnt_fs_get_options(fs));

	if (!rc)
		cxt->flags |= MNT_FL_TAB_APPLIED;
	return rc;
}

/**
 * mnt_context_apply_fstab:
 * @cxt: mount context
 *
 * This function is optional.
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_apply_fstab(struct libmnt_context *cxt)
{
	int rc = -1;
	struct libmnt_table *tab = NULL;
	const char *src = NULL, *tgt = NULL;

	assert(cxt);
	assert(cxt->fs);

	if (!cxt)
		return -EINVAL;

	if (cxt->flags & MNT_FL_TAB_APPLIED)
		return 0;

	if (mnt_context_is_restricted(cxt)) {
		DBG(CXT, mnt_debug_h(cxt, "force fstab usage for non-root users!"));
		cxt->optsmode = MNT_OMODE_USER;
	} else if (cxt->optsmode == 0) {
		DBG(CXT, mnt_debug_h(cxt, "use default optmode"));
		cxt->optsmode = MNT_OMODE_AUTO;
	} else if (cxt->optsmode & MNT_OMODE_NOTAB) {
		cxt->optsmode &= ~MNT_OMODE_FSTAB;
		cxt->optsmode &= ~MNT_OMODE_MTAB;
		cxt->optsmode &= ~MNT_OMODE_FORCE;
	}

	if (cxt->fs) {
		src = mnt_fs_get_source(cxt->fs);
		tgt = mnt_fs_get_target(cxt->fs);
	}

	DBG(CXT, mnt_debug_h(cxt, "OPTSMODE: ignore=%d, append=%d, prepend=%d, "
				  "replace=%d, force=%d, fstab=%d, mtab=%d",
				  cxt->optsmode & MNT_OMODE_IGNORE ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_APPEND ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_PREPEND ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_REPLACE ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_FORCE ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_FSTAB ? 1 : 0,
				  cxt->optsmode & MNT_OMODE_MTAB ? 1 : 0));

	/* fstab is not required if source and target are specified */
	if (src && tgt && !(cxt->optsmode & MNT_OMODE_FORCE)) {
		DBG(CXT, mnt_debug_h(cxt, "fstab not required -- skip"));
		return 0;
	}

	if (!src && tgt
	    && !(cxt->optsmode & MNT_OMODE_FSTAB)
	    && !(cxt->optsmode & MNT_OMODE_MTAB)) {
		DBG(CXT, mnt_debug_h(cxt, "only target; fstab/mtab not required "
					  "-- skip, probably MS_PROPAGATION"));
		return 0;
	}

	DBG(CXT, mnt_debug_h(cxt,
		"trying to apply fstab (src=%s, target=%s)", src, tgt));

	/* let's initialize cxt->fs */
	mnt_context_get_fs(cxt);

	/* try fstab */
	if (cxt->optsmode & MNT_OMODE_FSTAB) {
		rc = mnt_context_get_fstab(cxt, &tab);
		if (!rc)
			rc = apply_table(cxt, tab, MNT_ITER_FORWARD);
	}

	/* try mtab */
	if (rc < 0 && (cxt->optsmode & MNT_OMODE_MTAB)) {
		DBG(CXT, mnt_debug_h(cxt, "trying to apply from mtab"));
		rc = mnt_context_get_mtab(cxt, &tab);
		if (!rc)
			rc = apply_table(cxt, tab, MNT_ITER_BACKWARD);
	}
	if (rc)
		DBG(CXT, mnt_debug_h(cxt, "failed to find entry in fstab/mtab"));
	return rc;
}

/**
 * mnt_context_get_status:
 * @cxt: mount context
 *
 * Global libmount status.
 *
 * The real exit code of the mount.type helper has to be tested by
 * mnt_context_get_helper_status(). The mnt_context_get_status() only inform
 * that exec() has been sucessful.
 *
 * Returns: 1 if mount.type or mount(2) syscall has been succesfully called.
 */
int mnt_context_get_status(struct libmnt_context *cxt)
{
	return cxt && (!cxt->syscall_status || !cxt->helper_exec_status);
}

/**
 * mnt_context_helper_executed:
 * @cxt: mount context
 *
 * Returns: 1 if mount.type helper has been executed, or 0.
 */
int mnt_context_helper_executed(struct libmnt_context *cxt)
{
	return cxt->helper_exec_status != 1;
}

/**
 * mnt_context_get_helper_status:
 * @cxt: mount context
 *
 * Return: mount.type helper exit status, result is reliable only if
 *         mnt_context_helper_executed() returns 1.
 */
int mnt_context_get_helper_status(struct libmnt_context *cxt)
{
	return cxt->helper_status;
}

/**
 * mnt_context_syscall_called:
 * @cxt: mount context
 *
 * Returns: 1 if mount(2) syscall has been called, or 0.
 */
int mnt_context_syscall_called(struct libmnt_context *cxt)
{
	return cxt->syscall_status != 1;
}

/**
 * mnt_context_get_syscall_errno:
 * @cxt: mount context
 *
 * The result from this function is reliable only if
 * mnt_context_syscall_called() returns 1.
 *
 * Returns: mount(2) errno if the syscall failed or 0.
 */
int mnt_context_get_syscall_errno(struct libmnt_context *cxt)
{
	if (cxt->syscall_status < 0)
		return -cxt->syscall_status;

	return 0;
}

/**
 * mnt_context_set_syscall_status:
 * @cxt: mount context
 * @status: mount(2) status
 *
 * The @status should be 0 on success, or negative number on error (-errno).
 *
 * This function should be used only if [u]mount(2) syscall is NOT called by
 * libmount code.
 *
 * Returns: 0 or negative number in case of error.
 */
int mnt_context_set_syscall_status(struct libmnt_context *cxt, int status)
{
	if (!cxt)
		return -EINVAL;

	DBG(CXT, mnt_debug_h(cxt, "syscall status set to: %d", status));
	cxt->syscall_status = status;
	return 0;
}

/**
 * mnt_context_strerror
 * @cxt: context
 * @buf: buffer
 * @bufsiz: size of the buffer
 *
 * Not implemented yet.
 *
 * Returns: 0 or negative number in case of error.
 */
int mnt_context_strerror(struct libmnt_context *cxt __attribute__((__unused__)),
			 char *buf __attribute__((__unused__)),
			 size_t bufsiz __attribute__((__unused__)))
{
	/* TODO: based on cxt->syscall_errno or cxt->helper_status */
	return 0;
}

/**
 * mnt_context_init_helper
 * @cxt: mount context
 * @action: MNT_ACT_{UMOUNT,MOUNT}
 * @flags: not used now
 *
 * This function infors libmount that used from [u]mount.type helper.
 *
 * The function also calls mnt_context_disable_helpers() to avoid recursive
 * mount.type helpers calling. It you really want to call another
 * mount.type helper from your helper than you have to explicitly enable this
 * feature by:
 *
 *	 mnt_context_disable_helpers(cxt, FALSE);
 *
 * Returns: 0 on success, negative number in case of error.
 */
int mnt_context_init_helper(struct libmnt_context *cxt, int action,
			    int flags __attribute__((__unused__)))
{
	int rc = mnt_context_disable_helpers(cxt, TRUE);

	if (!rc)
		rc = set_flag(cxt, MNT_FL_HELPER, 1);
	if (!rc)
		cxt->action = action;

	DBG(CXT, mnt_debug_h(cxt, "initialized for [u]mount.<type> helper [rc=%d]", rc));
	return rc;
}

/**
 * mnt_context_helper_setopt:
 * @cxt: context
 * @c: getopt() result
 * @arg: getopt() optarg
 *
 * This function applies [u]mount.type command line option (for example parsed
 * by getopt or getopt_long) to @cxt. All unknown options are ignored and
 * then 1 is returned.
 *
 * Returns: negative number on error, 1 if @c is unknown option, 0 on success.
 */
int mnt_context_helper_setopt(struct libmnt_context *cxt, int c, char *arg)
{
	if (cxt) {
		switch(cxt->action) {
		case MNT_ACT_MOUNT:
			return mnt_context_mount_setopt(cxt, c, arg);
		case MNT_ACT_UMOUNT:
			return mnt_context_umount_setopt(cxt, c, arg);
		}
	}
	return -EINVAL;
}

/**
 * mnt_context_is_fs_mounted:
 * @cxt: context
 * @fs: filesystem
 * @mounted: returns 1 for mounted and 0 for non-mounted filesystems
 *
 * Returns: 0 on success and negative number in case of error.
 */
int mnt_context_is_fs_mounted(struct libmnt_context *cxt,
			      struct libmnt_fs *fs, int *mounted)
{
	struct libmnt_table *mtab;
	int rc;

	if (!cxt || !fs || !mounted)
		return -EINVAL;

	rc = mnt_context_get_mtab(cxt, &mtab);
	if (rc)
		return rc;

	*mounted = mnt_table_is_fs_mounted(mtab, fs);
	return 0;
}

static int mnt_context_add_child(struct libmnt_context *cxt, pid_t pid)
{
	pid_t *pids;

	if (!cxt)
		return -EINVAL;

	pids = realloc(cxt->children, sizeof(pid_t) * cxt->nchildren + 1);
	if (!pids)
		return -ENOMEM;

	DBG(CXT, mnt_debug_h(cxt, "add new child %d", pid));
	cxt->children = pids;
	cxt->children[cxt->nchildren++] = pid;

	return 0;
}

int mnt_fork_context(struct libmnt_context *cxt)
{
	int rc = 0;
	pid_t pid;

	if (!mnt_context_is_parent(cxt))
		return -EINVAL;

	DBG(CXT, mnt_debug_h(cxt, "forking context"));

	DBG_FLUSH;

	pid = fork();

	switch (pid) {
	case -1: /* error */
		DBG(CXT, mnt_debug_h(cxt, "fork failed %m"));
		return -errno;

	case 0: /* child */
		cxt->pid = getpid();
		cxt->flags &= ~MNT_FL_FORK;
		DBG(CXT, mnt_debug_h(cxt, "child created"));
		break;

	default:
		rc = mnt_context_add_child(cxt, pid);
		break;
	}

	return rc;
}

int mnt_context_wait_for_children(struct libmnt_context *cxt,
				  int *nchildren, int *nerrs)
{
	int i;

	if (!cxt)
		return -EINVAL;

	assert(mnt_context_is_parent(cxt));

	for (i = 0; i < cxt->nchildren; i++) {
		pid_t pid = cxt->children[i];
		int rc = 0, ret = 0;

		if (!pid)
			continue;
		do {
			DBG(CXT, mnt_debug_h(cxt,
					"waiting for child (%d/%d): %d",
					i + 1, cxt->nchildren, pid));
			errno = 0;
			rc = waitpid(pid, &ret, 0);

		} while (rc == -1 && errno == EINTR);

		if (nchildren)
			(*nchildren)++;

		if (rc != -1 && nerrs) {
			if (WIFEXITED(ret))
				(*nerrs) += WEXITSTATUS(ret) == 0 ? 0 : 1;
			else
				(*nerrs)++;
		}
		cxt->children[i] = 0;
	}

	cxt->nchildren = 0;
	free(cxt->children);
	cxt->children = NULL;
	return 0;
}

int mnt_context_is_fork(struct libmnt_context *cxt)
{
	return cxt && (cxt->flags & MNT_FL_FORK);
}


int mnt_context_is_parent(struct libmnt_context *cxt)
{
	return mnt_context_is_fork(cxt) && cxt->pid == 0;
}

int mnt_context_is_child(struct libmnt_context *cxt)
{
	return !mnt_context_is_fork(cxt) && cxt->pid;
}

#ifdef TEST_PROGRAM

struct libmnt_lock *lock;

static void lock_fallback(void)
{
	if (lock)
		mnt_unlock_file(lock);
}

int test_mount(struct libmnt_test *ts, int argc, char *argv[])
{
	int idx = 1, rc = 0;
	struct libmnt_context *cxt;

	if (argc < 2)
		return -EINVAL;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	if (!strcmp(argv[idx], "-o")) {
		mnt_context_set_options(cxt, argv[idx + 1]);
		idx += 2;
	}
	if (!strcmp(argv[idx], "-t")) {
		/* TODO: use mnt_context_set_fstype_pattern() */
		mnt_context_set_fstype(cxt, argv[idx + 1]);
		idx += 2;
	}

	if (argc == idx + 1)
		/* mount <mountpont>|<device> */
		mnt_context_set_target(cxt, argv[idx++]);

	else if (argc == idx + 2) {
		/* mount <device> <mountpoint> */
		mnt_context_set_source(cxt, argv[idx++]);
		mnt_context_set_target(cxt, argv[idx++]);
	}

	/* this is unnecessary! -- libmount is able to internaly
	 * create and manage the lock
	 */
	lock = mnt_context_get_lock(cxt);
	if (lock)
		atexit(lock_fallback);

	rc = mnt_context_mount(cxt);
	if (rc)
		printf("failed to mount: %m\n");
	else
		printf("successfully mounted\n");

	lock = NULL;	/* because we use atexit lock_fallback */
	mnt_free_context(cxt);
	return rc;
}

int test_umount(struct libmnt_test *ts, int argc, char *argv[])
{
	int idx = 1, rc = 0;
	struct libmnt_context *cxt;

	if (argc < 2)
		return -EINVAL;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	if (!strcmp(argv[idx], "-t")) {
		mnt_context_set_fstype(cxt, argv[idx + 1]);
		idx += 2;
	}

	if (!strcmp(argv[idx], "-f")) {
		mnt_context_enable_force(cxt, TRUE);
		idx++;
	}

	if (!strcmp(argv[idx], "-l")) {
		mnt_context_enable_lazy(cxt, TRUE);
		idx++;
	}

	if (!strcmp(argv[idx], "-r")) {
		mnt_context_enable_rdonly_umount(cxt, TRUE);
		idx++;
	}

	if (argc == idx + 1) {
		/* mount <mountpont>|<device> */
		mnt_context_set_target(cxt, argv[idx++]);
	} else {
		rc = -EINVAL;
		goto err;
	}

	lock = mnt_context_get_lock(cxt);
	if (lock)
		atexit(lock_fallback);

	rc = mnt_context_umount(cxt);
	if (rc)
		printf("failed to umount\n");
	else
		printf("successfully umounted\n");
err:
	lock = NULL;	/* because we use atexit lock_fallback */
	mnt_free_context(cxt);
	return rc;
}

int test_flags(struct libmnt_test *ts, int argc, char *argv[])
{
	int idx = 1, rc = 0;
	struct libmnt_context *cxt;
	const char *opt = NULL;
	unsigned long flags = 0;

	if (argc < 2)
		return -EINVAL;

	cxt = mnt_new_context();
	if (!cxt)
		return -ENOMEM;

	if (!strcmp(argv[idx], "-o")) {
		mnt_context_set_options(cxt, argv[idx + 1]);
		idx += 2;
	}

	if (argc == idx + 1)
		/* mount <mountpont>|<device> */
		mnt_context_set_target(cxt, argv[idx++]);

	rc = mnt_context_prepare_mount(cxt);
	if (rc)
		printf("failed to prepare mount %s\n", strerror(-rc));

	opt = mnt_fs_get_options(cxt->fs);
	if (opt)
		fprintf(stdout, "options: %s\n", opt);

	mnt_context_get_mflags(cxt, &flags);
	fprintf(stdout, "flags: %08lx\n", flags);

	mnt_free_context(cxt);
	return rc;
}

int test_mountall(struct libmnt_test *ts, int argc, char *argv[])
{
	struct libmnt_context *cxt;
	struct libmnt_iter *itr;
	struct libmnt_fs *fs;
	int mntrc, ignored, idx = 1;

	cxt = mnt_new_context();
	itr = mnt_new_iter(MNT_ITER_FORWARD);

	if (!cxt || !itr)
		return -ENOMEM;

	if (argc > 2) {
		if (!strcmp(argv[idx], "-O")) {
			mnt_context_set_options_pattern(cxt, argv[idx + 1]);
			idx += 2;
		}
		if (!strcmp(argv[idx], "-t")) {
			mnt_context_set_fstype_pattern(cxt, argv[idx + 1]);
			idx += 2;
		}
	}

	while (mnt_context_next_mount(cxt, itr, &fs, &mntrc, &ignored) == 0) {

		const char *tgt = mnt_fs_get_target(fs);

		if (ignored == 1)
			printf("%s: ignored: not match\n", tgt);
		else if (ignored == 2)
			printf("%s: ignored: already mounted\n", tgt);

		else if (!mnt_context_get_status(cxt)) {
			if (mntrc > 0) {
				errno = mntrc;
				warn("%s: mount failed", tgt);
			} else
				warnx("%s: mount failed", tgt);
		} else
			printf("%s: successfully mounted\n", tgt);
	}

	mnt_free_context(cxt);
	return 0;
}

int main(int argc, char *argv[])
{
	struct libmnt_test tss[] = {
	{ "--mount",  test_mount,  "[-o <opts>] [-t <type>] <spec>|<src> <target>" },
	{ "--umount", test_umount, "[-t <type>] [-f][-l][-r] <src>|<target>" },
	{ "--mount-all", test_mountall,  "[-O <pattern>] [-t <pattern] mount all filesystems from fstab" },
	{ "--flags", test_flags,   "[-o <opts>] <spec>" },
	{ NULL }};

	umask(S_IWGRP|S_IWOTH);	/* to be compatible with mount(8) */

	return mnt_run_test(tss, argc, argv);
}

#endif /* TEST_PROGRAM */
