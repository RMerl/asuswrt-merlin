/*
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: context-umount
 * @title: Umount context
 * @short_description: high-level API to umount operation.
 */

#include <sys/wait.h>
#include <sys/mount.h>

#include "pathnames.h"
#include "strutils.h"
#include "mountP.h"

/*
 * umount2 flags
 */
#ifndef MNT_FORCE
# define MNT_FORCE        0x00000001	/* Attempt to forcibily umount */
#endif

#ifndef MNT_DETACH
# define MNT_DETACH       0x00000002	/* Just detach from the tree */
#endif

#ifndef UMOUNT_NOFOLLOW
# define UMOUNT_NOFOLLOW  0x00000008	/* Don't follow symlink on umount */
#endif

#ifndef UMOUNT_UNUSED
# define UMOUNT_UNUSED    0x80000000	/* Flag guaranteed to be unused */
#endif


static int lookup_umount_fs(struct libmnt_context *cxt)
{
	int rc;
	const char *tgt;
	struct libmnt_table *mtab = NULL;
	struct libmnt_fs *fs;

	assert(cxt);
	assert(cxt->fs);

	DBG(CXT, mnt_debug_h(cxt, "umount: lookup FS"));

	tgt = mnt_fs_get_target(cxt->fs);
	if (!tgt) {
		DBG(CXT, mnt_debug_h(cxt, "umount: undefined target"));
		return -EINVAL;
	}
	rc = mnt_context_get_mtab(cxt, &mtab);
	if (rc) {
		DBG(CXT, mnt_debug_h(cxt, "umount: failed to read mtab"));
		return rc;
	}
	fs = mnt_table_find_target(mtab, tgt, MNT_ITER_BACKWARD);
	if (!fs) {
		/* maybe the option is source rather than target (mountpoint) */
		fs = mnt_table_find_source(mtab, tgt, MNT_ITER_BACKWARD);

		if (fs) {
			struct libmnt_fs *fs1 = mnt_table_find_target(mtab,
							mnt_fs_get_target(fs),
							MNT_ITER_BACKWARD);
			if (!fs1) {
				DBG(CXT, mnt_debug_h(cxt, "mtab is broken?!?!"));
				return -EINVAL;
			}
			if (fs != fs1) {
				/* Something was stacked over `file' on the
				 * same mount point. */
				DBG(CXT, mnt_debug_h(cxt,
						"umount: %s: %s is mounted "
						"over it on the same point",
						tgt, mnt_fs_get_source(fs1)));
				return -EINVAL;
			}
		}
	}

	if (!fs) {
		DBG(CXT, mnt_debug_h(cxt, "umount: cannot find %s in mtab", tgt));
		return 0;
	}

	/* copy from mtab to our FS description
	 */
	mnt_fs_set_source(cxt->fs, NULL);
	mnt_fs_set_target(cxt->fs, NULL);

	if (!mnt_copy_fs(cxt->fs, fs)) {
		DBG(CXT, mnt_debug_h(cxt, "umount: failed to copy FS"));
		return -errno;
	}

	DBG(CXT, mnt_debug_h(cxt, "umount: mtab applied"));
	cxt->flags |= MNT_FL_TAB_APPLIED;
	return rc;
}

/* check if @devname is loopdev and if the device is associated
 * with a source from @fstab_fs
 *
 * TODO : move this to loopdev.c
 */
static int mnt_loopdev_associated_fs(const char *devname, struct libmnt_fs *fs)
{
	uintmax_t offset = 0;
	const char *src;
	char *val, *optstr;
	size_t valsz;

	/* check if it begins with /dev/loop */
	if (strncmp(devname, _PATH_DEV_LOOP, sizeof(_PATH_DEV_LOOP)))
		return 0;

	src = mnt_fs_get_srcpath(fs);
	if (!src)
		return 0;

	/* check for offset option in @fs */
	optstr = (char *) mnt_fs_get_user_options(fs);
	if (optstr && !mnt_optstr_get_option(optstr, "offset=", &val, &valsz)) {
		int rc;

		val = strndup(val, valsz);
		if (!val)
			return 0;
		rc = strtosize(val, &offset);
		free(val);
		if (rc)
			return 0;
	}

	/* TODO:
	 * if (mnt_loopdev_associated_file(devname, src, offset))
	 *	return 1;
	 */
	return 0;
}

static int prepare_helper_from_options(struct libmnt_context *cxt,
				       const char *name)
{
	char *suffix = NULL;
	const char *opts;
	size_t valsz;

	if (cxt->flags & MNT_FL_NOHELPERS)
		return 0;

	opts = mnt_fs_get_user_options(cxt->fs);
	if (!opts)
		return 0;

	if (mnt_optstr_get_option(opts, name, &suffix, &valsz))
		return 0;

	suffix = strndup(suffix, valsz);
	if (!suffix)
		return -ENOMEM;

	DBG(CXT, mnt_debug_h(cxt, "umount: umount.%s %s requested", suffix, name));

	return mnt_context_prepare_helper(cxt, "umount", suffix);
}

/*
 * Note that cxt->fs contains relevant mtab entry!
 */
static int evaluate_permissions(struct libmnt_context *cxt)
{
	struct libmnt_table *fstab;
	unsigned long u_flags = 0;
	const char *tgt, *src, *optstr;
	int rc, ok = 0;
	struct libmnt_fs *fs;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!cxt || !cxt->fs)
		return -EINVAL;

	if (!mnt_context_is_restricted(cxt))
		 return 0;		/* superuser mount */

	DBG(CXT, mnt_debug_h(cxt, "umount: evaluating permissions"));

	if (!(cxt->flags & MNT_FL_TAB_APPLIED)) {
		DBG(CXT, mnt_debug_h(cxt,
				"cannot find %s in mtab and you are not root",
				mnt_fs_get_target(cxt->fs)));
		goto eperm;
	}

	if (cxt->user_mountflags & MNT_MS_UHELPER) {
		/* on uhelper= mount option based helper */
		rc = prepare_helper_from_options(cxt, "uhelper");
		if (rc)
			return rc;
		if (cxt->helper)
			return 0;	/* we'll call /sbin/umount.<uhelper> */
	}

	/*
	 * User mounts has to be in /etc/fstab
	 */
	rc = mnt_context_get_fstab(cxt, &fstab);
	if (rc)
		return rc;

	tgt = mnt_fs_get_target(cxt->fs);
	src = mnt_fs_get_source(cxt->fs);

	if (mnt_fs_get_bindsrc(cxt->fs)) {
		src = mnt_fs_get_bindsrc(cxt->fs);
		DBG(CXT, mnt_debug_h(cxt,
				"umount: using bind source: %s", src));
	}

	/* If fstab contains the two lines
	 *	/dev/sda1 /mnt/zip auto user,noauto  0 0
	 *	/dev/sda4 /mnt/zip auto user,noauto  0 0
	 * then "mount /dev/sda4" followed by "umount /mnt/zip" used to fail.
	 * So, we must not look for file, but for the pair (dev,file) in fstab.
	  */
	fs = mnt_table_find_pair(fstab, src, tgt, MNT_ITER_FORWARD);
	if (!fs) {
		/*
		 * It's possible that there is /path/file.img in fstab and
		 * /dev/loop0 in mtab -- then we have to check releation
		 * between loopdev and the file.
		 */
		fs = mnt_table_find_target(fstab, tgt, MNT_ITER_FORWARD);
		if (fs) {
			const char *dev = mnt_fs_get_srcpath(cxt->fs);		/* devname from mtab */

			if (!dev || !mnt_loopdev_associated_fs(dev, fs))
				fs = NULL;
		}
		if (!fs) {
			DBG(CXT, mnt_debug_h(cxt,
					"umount %s: mtab disagrees with fstab",
					tgt));
			goto eperm;
		}
	}

	/*
	 * User mounting and unmounting is allowed only if fstab contains one
	 * of the options `user', `users' or `owner' or `group'.
	 *
	 * The option `users' allows arbitrary users to mount and unmount -
	 * this may be a security risk.
	 *
	 * The options `user', `owner' and `group' only allow unmounting by the
	 * user that mounted (visible in mtab).
	 */
	optstr = mnt_fs_get_user_options(fs);	/* FSTAB mount options! */
	if (!optstr)
		goto eperm;

	if (mnt_optstr_get_flags(optstr, &u_flags,
				mnt_get_builtin_optmap(MNT_USERSPACE_MAP)))
		goto eperm;

	if (u_flags & MNT_MS_USERS) {
		DBG(CXT, mnt_debug_h(cxt,
			"umount: promiscuous setting ('users') in fstab"));
		return 0;
	}
	/*
	 * Check user=<username> setting from mtab if there is user, owner or
	 * group option in /etc/fstab
	 */
	if ((u_flags & MNT_MS_USER) || (u_flags & MNT_MS_OWNER) ||
	    (u_flags & MNT_MS_GROUP)) {

		char *curr_user = NULL;
		char *mtab_user = NULL;
		size_t sz;

		DBG(CXT, mnt_debug_h(cxt,
				"umount: checking user=<username> from mtab"));

		curr_user = mnt_get_username(getuid());

		if (!curr_user) {
			DBG(CXT, mnt_debug_h(cxt, "umount %s: cannot "
				"convert %d to username", tgt, getuid()));
			goto eperm;
		}

		/* get options from mtab */
		optstr = mnt_fs_get_user_options(cxt->fs);
		if (optstr && !mnt_optstr_get_option(optstr,
					"user", &mtab_user, &sz) && sz)
			ok = !strncmp(curr_user, mtab_user, sz);
	}

	if (ok) {
		DBG(CXT, mnt_debug_h(cxt, "umount %s is allowed", tgt));
		return 0;
	}
eperm:
	DBG(CXT, mnt_debug_h(cxt, "umount is not allowed for you"));
	return -EPERM;
}

static int exec_helper(struct libmnt_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));
	assert(cxt->helper_exec_status == 1);

	DBG_FLUSH;

	switch (fork()) {
	case 0:
	{
		const char *args[10], *type;
		int i = 0;

		if (setgid(getgid()) < 0)
			exit(EXIT_FAILURE);

		if (setuid(getuid()) < 0)
			exit(EXIT_FAILURE);

		type = mnt_fs_get_fstype(cxt->fs);

		args[i++] = cxt->helper;			/* 1 */
		args[i++] = mnt_fs_get_target(cxt->fs);		/* 2 */

		if (cxt->flags & MNT_FL_NOMTAB)
			args[i++] = "-n";			/* 3 */
		if (cxt->flags & MNT_FL_LAZY)
			args[i++] = "-l";			/* 4 */
		if (cxt->flags & MNT_FL_FORCE)
			args[i++] = "-f";			/* 5 */
		if (cxt->flags & MNT_FL_VERBOSE)
			args[i++] = "-v";			/* 6 */
		if (cxt->flags & MNT_FL_RDONLY_UMOUNT)
			args[i++] = "-r";			/* 7 */
		if (type && !endswith(cxt->helper, type)) {
			args[i++] = "-t";			/* 8 */
			args[i++] = (char *) type;	/* 9 */
		}

		args[i] = NULL;					/* 10 */
#ifdef CONFIG_LIBMOUNT_DEBUG
		i = 0;
		for (i = 0; args[i]; i++)
			DBG(CXT, mnt_debug_h(cxt, "argv[%d] = \"%s\"",
							i, args[i]));
#endif
		DBG_FLUSH;
		execv(cxt->helper, (char * const *) args);
		exit(EXIT_FAILURE);
	}
	default:
	{
		int st;
		wait(&st);
		cxt->helper_status = WIFEXITED(st) ? WEXITSTATUS(st) : -1;

		DBG(CXT, mnt_debug_h(cxt, "%s executed [status=%d]",
					cxt->helper, cxt->helper_status));
		cxt->helper_exec_status = rc = 0;
		break;
	}

	case -1:
		cxt->helper_exec_status = rc = -errno;
		DBG(CXT, mnt_debug_h(cxt, "fork() failed"));
		break;
	}

	return rc;
}

/*
 * mnt_context_helper_setopt() backend.
 *
 * This function applies umount.<type> command line option (for example parsed
 * by getopt() or getopt_long()) to @cxt. All unknown options are ignored and
 * then 1 is returned.
 *
 * Returns: negative number on error, 1 if @c is unknown option, 0 on success.
 */
int mnt_context_umount_setopt(struct libmnt_context *cxt, int c, char *arg)
{
	int rc = -EINVAL;

	assert(cxt);
	assert(cxt->action == MNT_ACT_UMOUNT);

	switch(c) {
	case 'n':
		rc = mnt_context_disable_mtab(cxt, TRUE);
		break;
	case 'l':
		rc = mnt_context_enable_lazy(cxt, TRUE);
		break;
	case 'f':
		rc = mnt_context_enable_fake(cxt, TRUE);
		break;
	case 'v':
		rc = mnt_context_enable_verbose(cxt, TRUE);
		break;
	case 'r':
		rc = mnt_context_enable_rdonly_umount(cxt, TRUE);
		break;
	case 't':
		if (arg)
			rc = mnt_context_set_fstype(cxt, arg);
		break;
	default:
		return 1;
		break;
	}

	return rc;
}

/* Check whether the kernel supports UMOUNT_NOFOLLOW flag */
static int umount_nofollow_support(void)
{
	int res = umount2("", UMOUNT_UNUSED);
	if (res != -1 || errno != EINVAL)
		return 0;

	res = umount2("", UMOUNT_NOFOLLOW);
	if (res != -1 || errno != ENOENT)
		return 0;

	return 1;
}

static int do_umount(struct libmnt_context *cxt)
{
	int rc = 0, flags = 0;
	const char *src, *target;
	char *tgtbuf = NULL;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));
	assert(cxt->syscall_status == 1);

	if (cxt->helper)
		return exec_helper(cxt);

	src = mnt_fs_get_srcpath(cxt->fs);
	target = mnt_fs_get_target(cxt->fs);

	if (!target)
		return -EINVAL;

	if (cxt->flags & MNT_FL_FAKE)
		return 0;

	DBG(CXT, mnt_debug_h(cxt, "do umount"));

	if (cxt->restricted) {
		/*
		 * extra paranoa for non-root users
		 * -- chdir to the parent of the mountpoint and use NOFOLLOW
		 *    flag to avoid races and symlink attacks.
		 */
		if (umount_nofollow_support())
			flags |= UMOUNT_NOFOLLOW;

		rc = mnt_chdir_to_parent(target, &tgtbuf);
		if (rc)
			return rc;
		target = tgtbuf;
	}

	if (cxt->flags & MNT_FL_LAZY)
		flags |= MNT_DETACH;

	else if (cxt->flags & MNT_FL_FORCE)
		flags |= MNT_FORCE;

	DBG(CXT, mnt_debug_h(cxt, "umount(2) [target='%s', flags=0x%08x]",
				target, flags));

	rc = flags ? umount2(target, flags) : umount(target);
	if (rc < 0)
		cxt->syscall_status = -errno;

	free(tgtbuf);

	/*
	 * try remount read-only
	 */
	if (rc < 0 && cxt->syscall_status == -EBUSY &&
	    (cxt->flags & MNT_FL_RDONLY_UMOUNT) && src) {

		cxt->mountflags |= MS_REMOUNT | MS_RDONLY;
		cxt->flags &= ~MNT_FL_LOOPDEL;
		DBG(CXT, mnt_debug_h(cxt,
			"umount(2) failed [errno=%d] -- tring remount read-only",
			-cxt->syscall_status));

		rc = mount(src, mnt_fs_get_target(cxt->fs), NULL,
			    MS_MGC_VAL | MS_REMOUNT | MS_RDONLY, NULL);
		if (rc < 0) {
			cxt->syscall_status = -errno;
			DBG(CXT, mnt_debug_h(cxt,
				"read-only re-mount(2) failed [errno=%d]",
				-cxt->syscall_status));

			return -cxt->syscall_status;
		}
		cxt->syscall_status = 0;
		DBG(CXT, mnt_debug_h(cxt, "read-only re-mount(2) success"));
		return 0;
	}

	if (rc < 0) {
		DBG(CXT, mnt_debug_h(cxt, "umount(2) failed [errno=%d]",
			-cxt->syscall_status));
		return -cxt->syscall_status;
	}

	cxt->syscall_status = 0;
	DBG(CXT, mnt_debug_h(cxt, "umount(2) success"));
	return 0;
}

/**
 * mnt_context_prepare_umount:
 * @cxt: mount context
 *
 * Prepare context for umounting, unnecessary for mnt_context_umount().
 *
 * Returns: 0 on success, and negative number in case of error.
 */
int mnt_context_prepare_umount(struct libmnt_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);

	if (!cxt || !cxt->fs || (cxt->fs->flags & MNT_FS_SWAP))
		return -EINVAL;
	if (!mnt_fs_get_source(cxt->fs) && !mnt_fs_get_target(cxt->fs))
		return -EINVAL;
	if (cxt->flags & MNT_FL_PREPARED)
		return 0;

	free(cxt->helper);	/* be paranoid */
	cxt->helper = NULL;
	cxt->action = MNT_ACT_UMOUNT;

	rc = lookup_umount_fs(cxt);
	if (!rc)
		rc = mnt_context_merge_mflags(cxt);
	if (!rc)
		rc = evaluate_permissions(cxt);
	if (!rc)
	       rc = mnt_context_prepare_target(cxt);

	if (!rc && !cxt->helper) {

		if (cxt->user_mountflags & MNT_MS_HELPER)
			/* on helper= mount option based helper */
			rc = prepare_helper_from_options(cxt, "helper");

		if (!rc && !cxt->helper)
			/* on fstype based helper */
			rc = mnt_context_prepare_helper(cxt, "umount", NULL);
	}

/* TODO
	if ((cxt->flags & MNT_FL_LOOPDEL) &&
	    (!mnt_is_loopdev(src) || mnt_loopdev_is_autoclear(src)))
		cxt->flags &= ~MNT_FL_LOOPDEL;
*/
	if (rc) {
		DBG(CXT, mnt_debug_h(cxt, "umount: preparing failed"));
		return rc;
	}
	cxt->flags |= MNT_FL_PREPARED;
	return rc;
}

/**
 * mnt_context_do_umount:
 * @cxt: mount context
 *
 * Umount filesystem by umount(2) or fork()+exec(/sbin/umount.type).
 * Unnecessary for mnt_context_umount().
 *
 * See also mnt_context_disable_helpers().
 *
 * WARNING: non-zero return code does not mean that umount(2) syscall or
 *          umount.type helper wasn't sucessfully called.
 *
 *          Check mnt_context_get_status() after error!
*
 * Returns: 0 on success;
 *         >0 in case of umount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
int mnt_context_do_umount(struct libmnt_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);
	assert((cxt->flags & MNT_FL_PREPARED));
	assert((cxt->action == MNT_ACT_UMOUNT));
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	rc = do_umount(cxt);
	if (rc)
		return rc;
/* TODO
	if (cxt->flags & MNT_FL_LOOPDEL)
		rc = mnt_loopdev_clean(mnt_fs_get_source(cxt->fs));
*/
	if (cxt->flags & MNT_FL_NOMTAB)
		return rc;

	if ((cxt->flags & MNT_FL_RDONLY_UMOUNT) &&
	    (cxt->mountflags & (MS_RDONLY | MS_REMOUNT))) {
		/*
		 * fix options, remount --> read-only mount
		 */
		const char *o = mnt_fs_get_options(cxt->fs);
		char *n = o ? strdup(o) : NULL;

		DBG(CXT, mnt_debug_h(cxt, "fix remount-on-umount update"));

		if (n)
			mnt_optstr_remove_option(&n, "rw");
		rc = mnt_optstr_prepend_option(&n, "ro", NULL);
		if (!rc)
			rc = mnt_fs_set_options(cxt->fs, n);

		/* use "remount" instead of "umount" in /etc/mtab */
		if (!rc && cxt->update && cxt->mtab_writable)
			rc = mnt_update_set_fs(cxt->update,
					       cxt->mountflags, NULL, cxt->fs);
	}

	return rc;
}

/**
 * mnt_context_finalize_umount:
 * @cxt: context
 *
 * Mtab update, etc. Unnecessary for mnt_context_umount(), but should be called
 * after mnt_context_do_umount(). See also mnt_context_set_syscall_status().
 *
 * Returns: negative number on error, 0 on success.
 */
int mnt_context_finalize_umount(struct libmnt_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_PREPARED));
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	rc = mnt_context_prepare_update(cxt);
	if (!rc)
		rc = mnt_context_update_tabs(cxt);;
	return rc;
}


/**
 * mnt_context_umount:
 * @cxt: umount context
 *
 * High-level, umounts filesystem by umount(2) or fork()+exec(/sbin/umount.type).
 *
 * This is similar to:
 *
 *	mnt_context_prepare_umount(cxt);
 *	mnt_context_do_umount(cxt);
 *	mnt_context_finalize_umount(cxt);
 *
 * See also mnt_context_disable_helpers().
 *
 * WARNING: non-zero return code does not mean that umount(2) syscall or
 *          umount.type helper wasn't sucessfully called.
 *
 *          Check mnt_context_get_status() after error!
 *
 * Returns: 0 on success;
 *         >0 in case of umount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
int mnt_context_umount(struct libmnt_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);

	rc = mnt_context_prepare_umount(cxt);
	if (!rc)
		rc = mnt_context_prepare_update(cxt);
	if (!rc)
		rc = mnt_context_do_umount(cxt);
	if (!rc)
		rc = mnt_context_update_tabs(cxt);
	return rc;
}
