/*
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: context-mount
 * @title: Mount context
 * @short_description: high-level API to mount operation.
 */

#ifdef HAVE_LIBSELINUX
#include <selinux/selinux.h>
#include <selinux/context.h>
#endif

#include <sys/wait.h>
#include <sys/mount.h>

#include "linux_version.h"
#include "mountP.h"

/*
 * this has to be called after mnt_context_evaluate_permissions()
 */
static int fix_optstr(struct libmnt_context *cxt)
{
	int rc = 0;
	char *next;
	char *name, *val;
	size_t namesz, valsz;
	struct libmnt_fs *fs;
#ifdef HAVE_LIBSELINUX
	int se_fix = 0, se_rem = 0;
#endif
	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!cxt)
		return -EINVAL;
	if (!cxt->fs || (cxt->flags & MNT_FL_MOUNTOPTS_FIXED))
		return 0;

	DBG(CXT, mnt_debug_h(cxt, "mount: fixing optstr"));

	fs = cxt->fs;

	/* The propagation flags should not be used together with any other flags */
	if (cxt->mountflags & MS_PROPAGATION)
		cxt->mountflags &= MS_PROPAGATION;

	if (!mnt_optstr_get_option(fs->user_optstr, "user", &val, &valsz)) {
		if (val) {
			cxt->orig_user = strndup(val, valsz);
			if (!cxt->orig_user) {
				rc = -ENOMEM;
				goto done;
			}
		}
		cxt->flags |= MNT_FL_SAVED_USER;
	}

	/*
	 * Sync mount options with mount flags
	 */
	rc = mnt_optstr_apply_flags(&fs->vfs_optstr, cxt->mountflags,
				mnt_get_builtin_optmap(MNT_LINUX_MAP));
	if (rc)
		goto done;

	rc = mnt_optstr_apply_flags(&fs->user_optstr, cxt->user_mountflags,
				mnt_get_builtin_optmap(MNT_USERSPACE_MAP));
	if (rc)
		goto done;

	next = fs->fs_optstr;

#ifdef HAVE_LIBSELINUX
	if (!is_selinux_enabled())
		/* Always remove SELinux garbage if SELinux disabled */
		se_rem = 1;
	else if (cxt->mountflags & MS_REMOUNT)
		/*
		 * Linux kernel < 2.6.39 does not allow to remount with any
		 * selinux specific mount options.
		 *
		 * Kernel 2.6.39 commits:  ff36fe2c845cab2102e4826c1ffa0a6ebf487c65
		 *                         026eb167ae77244458fa4b4b9fc171209c079ba7
		 * fix this odd behavior, so we don't have to care about it in
		 * userspace.
		 */
		se_rem = get_linux_version() < KERNEL_VERSION(2, 6, 39);
	else
		/* For normal mount we have translate the contexts */
		se_fix = 1;
#endif
	while (!mnt_optstr_next_option(&next, &name, &namesz, &val, &valsz)) {

		if (namesz == 3 && !strncmp(name, "uid", 3))
			rc = mnt_optstr_fix_uid(&fs->fs_optstr, val, valsz, &next);
		else if (namesz == 3 && !strncmp(name, "gid", 3))
			rc = mnt_optstr_fix_gid(&fs->fs_optstr, val, valsz, &next);
#ifdef HAVE_LIBSELINUX
		else if ((se_rem || se_fix) &&
			 namesz >= 7 && (!strncmp(name, "context", 7) ||
					 !strncmp(name, "fscontext", 9) ||
					 !strncmp(name, "defcontext", 10) ||
					 !strncmp(name, "rootcontext", 11) ||
					 !strncmp(name, "seclabel", 8))) {
			if (se_rem) {
				/* remove context= option */
				next = name;
				rc = mnt_optstr_remove_option_at(&fs->fs_optstr,
						name,
						val ? val + valsz :
						      name + namesz);
			} else if (se_fix && val && valsz)
				/* translate selinux contexts */
				rc = mnt_optstr_fix_secontext(&fs->fs_optstr,
							val, valsz, &next);
		}
#endif
		if (rc)
			goto done;
	}

	if (!rc && cxt->user_mountflags & MNT_MS_USER)
		rc = mnt_optstr_fix_user(&fs->user_optstr);

	/* refresh merged optstr */
	free(fs->optstr);
	fs->optstr = NULL;
	fs->optstr = mnt_fs_strdup_options(fs);
done:
	cxt->flags |= MNT_FL_MOUNTOPTS_FIXED;

	DBG(CXT, mnt_debug_h(cxt, "fixed options [rc=%d]: "
		"vfs: '%s' fs: '%s' user: '%s', optstr: '%s'", rc,
		fs->vfs_optstr, fs->fs_optstr, fs->user_optstr, fs->optstr));
	return rc;
}

/*
 * Converts already evalulated and fixed options to the form that is compatible
 * with /sbin/mount.type helpers.
 */
static int generate_helper_optstr(struct libmnt_context *cxt, char **optstr)
{
	struct libmnt_optmap const *maps[1];
	char *next, *name, *val;
	size_t namesz, valsz;
	int rc = 0;

	assert(cxt);
	assert(cxt->fs);
	assert(optstr);

	DBG(CXT, mnt_debug_h(cxt, "mount: generate heper mount options"));

	*optstr = mnt_fs_strdup_options(cxt->fs);
	if (!*optstr)
		return -ENOMEM;

	if (cxt->flags & MNT_FL_SAVED_USER)
		rc = mnt_optstr_set_option(optstr, "user", cxt->orig_user);
	if (rc)
		goto err;

	/* remove userspace options with MNT_NOHLPS flag */
	maps[0] = mnt_get_builtin_optmap(MNT_USERSPACE_MAP);
	next = *optstr;

	while (!mnt_optstr_next_option(&next, &name, &namesz, &val, &valsz)) {
		const struct libmnt_optmap *ent;

		mnt_optmap_get_entry(maps, 1, name, namesz, &ent);
		if (ent && ent->id && (ent->mask & MNT_NOHLPS)) {
			next = name;
			rc = mnt_optstr_remove_option_at(optstr, name,
					val ? val + valsz : name + namesz);
			if (rc)
				goto err;
		}
	}

	return rc;
err:
	free(*optstr);
	*optstr = NULL;
	return rc;
}

/*
 * this has to be called before fix_optstr()
 */
static int evaluate_permissions(struct libmnt_context *cxt)
{
	unsigned long u_flags = 0;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!cxt)
		return -EINVAL;
	if (!cxt->fs)
		return 0;

	DBG(CXT, mnt_debug_h(cxt, "mount: evaluating permissions"));

	mnt_context_get_user_mflags(cxt, &u_flags);

	if (!mnt_context_is_restricted(cxt)) {
		/*
		 * superuser mount
		 */
		cxt->user_mountflags &= ~MNT_MS_OWNER;
		cxt->user_mountflags &= ~MNT_MS_GROUP;
		cxt->user_mountflags &= ~MNT_MS_USER;
		cxt->user_mountflags &= ~MNT_MS_USERS;
	} else {
		/*
		 * user mount
		 */
		if (!(cxt->flags & MNT_FL_TAB_APPLIED))
		{
			DBG(CXT, mnt_debug_h(cxt, "perms: fstab not applied, ignore user mount"));
			return -EPERM;
		}

		/*
		 * Note that MS_OWNERSECURE and MS_SECURE mount options
		 * are applied by mnt_optstr_get_flags() from mnt_context_merge_mflags()
		 */


		/*
		 * MS_OWNER: Allow owners to mount when fstab contains the
		 * owner option.  Note that this should never be used in a high
		 * security environment, but may be useful to give people at
		 * the console the possibility of mounting a floppy.  MS_GROUP:
		 * Allow members of device group to mount. (Martin Dickopp)
		 */
		if (u_flags & (MNT_MS_OWNER | MNT_MS_GROUP)) {
			struct stat sb;
			struct libmnt_cache *cache = NULL;
			char *xsrc = NULL;
			const char *srcpath = mnt_fs_get_srcpath(cxt->fs);

			if (!srcpath) {					/* Ah... source is TAG */
				cache = mnt_context_get_cache(cxt);
				xsrc = mnt_resolve_spec(
						mnt_context_get_source(cxt),
						cache);
				srcpath = xsrc;
			}
			if (!srcpath) {
				DBG(CXT, mnt_debug_h(cxt, "perms: src undefined"));
				return -EPERM;
			}

			if (strncmp(srcpath, "/dev/", 5) == 0 &&
			    stat(srcpath, &sb) == 0 &&
			    (((u_flags & MNT_MS_OWNER) && getuid() == sb.st_uid) ||
			     ((u_flags & MNT_MS_GROUP) && mnt_in_group(sb.st_gid))))

				cxt->user_mountflags |= MNT_MS_USER;

			if (!cache)
				free(xsrc);
		}

		if (!(cxt->user_mountflags & (MNT_MS_USER | MNT_MS_USERS))) {
			DBG(CXT, mnt_debug_h(cxt, "permissions evaluation ends with -EPERMS"));
			return -EPERM;
		}
	}

	return 0;
}

/*
 * mnt_context_helper_setopt() backend
 *
 * This function applies mount.type command line option (for example parsed
 * by getopt() or getopt_long()) to @cxt. All unknown options are ignored and
 * then 1 is returned.
 *
 * Returns: negative number on error, 1 if @c is unknown option, 0 on success.
 */
int mnt_context_mount_setopt(struct libmnt_context *cxt, int c, char *arg)
{
	int rc = -EINVAL;

	assert(cxt);
	assert(cxt->action == MNT_ACT_MOUNT);

	switch(c) {
	case 'f':
		rc = mnt_context_enable_fake(cxt, TRUE);
		break;
	case 'n':
		rc = mnt_context_disable_mtab(cxt, TRUE);
		break;
	case 'r':
		rc = mnt_context_append_options(cxt, "ro");
		break;
	case 'v':
		rc = mnt_context_enable_verbose(cxt, TRUE);
		break;
	case 'w':
		rc = mnt_context_append_options(cxt, "rw");
		break;
	case 'o':
		if (arg)
			rc = mnt_context_append_options(cxt, arg);
		break;
	case 's':
		rc = mnt_context_enable_sloppy(cxt, TRUE);
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

static int exec_helper(struct libmnt_context *cxt)
{
	char *o = NULL;
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	DBG(CXT, mnt_debug_h(cxt, "mount: executing helper %s", cxt->helper));

	rc = generate_helper_optstr(cxt, &o);
	if (rc)
		return -EINVAL;

	DBG_FLUSH;

	switch (fork()) {
	case 0:
	{
		const char *args[12], *type;
		int i = 0;

		if (setgid(getgid()) < 0)
			exit(EXIT_FAILURE);

		if (setuid(getuid()) < 0)
			exit(EXIT_FAILURE);

		type = mnt_fs_get_fstype(cxt->fs);

		args[i++] = cxt->helper;		/* 1 */
		args[i++] = mnt_fs_get_srcpath(cxt->fs);/* 2 */
		args[i++] = mnt_fs_get_target(cxt->fs);	/* 3 */

		if (mnt_context_is_sloppy(cxt))
			args[i++] = "-s";		/* 4 */
		if (mnt_context_is_fake(cxt))
			args[i++] = "-f";		/* 5 */
		if (mnt_context_is_nomtab(cxt))
			args[i++] = "-n";		/* 6 */
		if (mnt_context_is_verbose(cxt))
			args[i++] = "-v";		/* 7 */
		if (o) {
			args[i++] = "-o";		/* 8 */
			args[i++] = o;			/* 9 */
		}
		if (type && !endswith(cxt->helper, type)) {
			args[i++] = "-t";		/* 10 */
			args[i++] = type;		/* 11 */
		}
		args[i] = NULL;				/* 12 */
#ifdef CONFIG_LIBMOUNT_DEBUG
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
 * The default is to use fstype from cxt->fs, this could be overwritten by
 * @try_type argument.
 *
 * Returns: 0 on success,
 *         >0 in case of mount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
static int do_mount(struct libmnt_context *cxt, const char *try_type)
{
	int rc = 0;
	const char *src, *target, *type;
	unsigned long flags;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (try_type && !cxt->helper) {
		rc = mnt_context_prepare_helper(cxt, "mount", try_type);
		if (!rc)
			return rc;
	}
	if (cxt->helper)
		return exec_helper(cxt);

	flags = cxt->mountflags;
	src = mnt_fs_get_srcpath(cxt->fs);
	target = mnt_fs_get_target(cxt->fs);

	if (!src || !target)
		return -EINVAL;

	type = try_type ? : mnt_fs_get_fstype(cxt->fs);

	if (!(flags & MS_MGC_MSK))
		flags |= MS_MGC_VAL;

	DBG(CXT, mnt_debug_h(cxt, "%smount(2) "
			"[source=%s, target=%s, type=%s, "
			" mountflags=0x%08lx, mountdata=%s]",
			(cxt->flags & MNT_FL_FAKE) ? "(FAKE) " : "",
			src, target, type,
			flags, cxt->mountdata ? "yes" : "<none>"));

	if (cxt->flags & MNT_FL_FAKE)
		cxt->syscall_status = 0;
	else {
		if (mount(src, target, type, flags, cxt->mountdata)) {
			cxt->syscall_status = -errno;
			DBG(CXT, mnt_debug_h(cxt, "mount(2) failed [errno=%d %m]",
							-cxt->syscall_status));
			return -cxt->syscall_status;
		}
		DBG(CXT, mnt_debug_h(cxt, "mount(2) success"));
		cxt->syscall_status = 0;
	}

	if (try_type && cxt->update) {
		struct libmnt_fs *fs = mnt_update_get_fs(cxt->update);
		if (fs)
			rc = mnt_fs_set_fstype(fs, try_type);
	}

	return rc;
}

static int do_mount_by_pattern(struct libmnt_context *cxt, const char *pattern)
{
	int neg = pattern && strncmp(pattern, "no", 2) == 0;
	int rc = -EINVAL;
	char **filesystems, **fp;

	assert(cxt);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!neg && pattern) {
		/*
		 * try all types from the list
		 */
		char *p, *p0;

		DBG(CXT, mnt_debug_h(cxt, "trying to mount by FS pattern list"));

		p0 = p = strdup(pattern);
		if (!p)
			return -ENOMEM;
		do {
			char *end = strchr(p, ',');
			if (end)
				*end = '\0';
			rc = do_mount(cxt, p);
			p = end ? end + 1 : NULL;
		} while (!mnt_context_get_status(cxt) && p);

		free(p0);

		if (mnt_context_get_status(cxt))
			return rc;
	}

	/*
	 * try /etc/filesystems and /proc/filesystems
	 */
	DBG(CXT, mnt_debug_h(cxt, "trying to mount by filesystems lists"));

	rc = mnt_get_filesystems(&filesystems, neg ? pattern : NULL);
	if (rc)
		return rc;

	for (fp = filesystems; *fp; fp++) {
		rc = do_mount(cxt, *fp);
		if (mnt_context_get_status(cxt))
			break;
	}
	mnt_free_filesystems(filesystems);
	return rc;
}

/**
 * mnt_context_prepare_mount:
 * @cxt: context
 *
 * Prepare context for mounting, unnecessary for mnt_context_mount().
 *
 * Returns: negative number on error, zero on success
 */
int mnt_context_prepare_mount(struct libmnt_context *cxt)
{
	int rc = -EINVAL;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);

	if (!cxt || !cxt->fs || mnt_fs_is_swaparea(cxt->fs))
		return -EINVAL;
	if (!mnt_fs_get_source(cxt->fs) && !mnt_fs_get_target(cxt->fs))
		return -EINVAL;
	if (cxt->flags & MNT_FL_PREPARED)
		return 0;

	cxt->action = MNT_ACT_MOUNT;

	DBG(CXT, mnt_debug_h(cxt, "mount: preparing"));

	/* TODO: fstab is unnecessary for MS_{MOVE,BIND,STARED,...} */
	rc = mnt_context_apply_fstab(cxt);
	if (!rc)
		rc = mnt_context_merge_mflags(cxt);
	if (!rc)
		rc = evaluate_permissions(cxt);
	if (!rc)
		rc = fix_optstr(cxt);
	if (!rc)
		rc = mnt_context_prepare_srcpath(cxt);
	if (!rc)
		rc = mnt_context_prepare_target(cxt);
	if (!rc)
		rc = mnt_context_guess_fstype(cxt);
	if (!rc)
		rc = mnt_context_prepare_helper(cxt, "mount", NULL);
	if (rc) {
		DBG(CXT, mnt_debug_h(cxt, "mount: preparing failed"));
		return rc;
	}
	cxt->flags |= MNT_FL_PREPARED;
	return rc;
}

/**
 * mnt_context_do_mount
 * @cxt: context
 *
 * Call mount(2) or mount.type helper. Unnecessary for mnt_context_mount().
 *
 * Note that this function could be called only once. If you want to mount
 * another source or target than you have to call mnt_reset_context().
 *
 * If you want to call mount(2) for the same source and target with a diffrent
 * mount flags or fstype then you call mnt_context_reset_status() and then try
 * again mnt_context_do_mount().
 *
 * WARNING: non-zero return code does not mean that mount(2) syscall or
 *          umount.type helper wasn't sucessfully called.
 *
 *          Check mnt_context_get_status() after error!
*
 * Returns: 0 on success;
 *         >0 in case of mount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
int mnt_context_do_mount(struct libmnt_context *cxt)
{
	const char *type;
	int res;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));
	assert((cxt->flags & MNT_FL_PREPARED));
	assert((cxt->action == MNT_ACT_MOUNT));

	DBG(CXT, mnt_debug_h(cxt, "mount: do mount"));

	if (!(cxt->flags & MNT_FL_MOUNTDATA))
		cxt->mountdata = (char *) mnt_fs_get_fs_options(cxt->fs);

	type = mnt_fs_get_fstype(cxt->fs);
	if (type)
		res = do_mount(cxt, NULL);
	else
		res = do_mount_by_pattern(cxt, cxt->fstype_pattern);

	if (mnt_context_get_status(cxt)
	    && !(cxt->flags & MNT_FL_FAKE)
	    && !cxt->helper) {
		/*
		 * Mounted by mount(2), do some post-mount checks
		 *
		 * Kernel allows to use MS_RDONLY for bind mounts, but the
		 * read-only request could be silently ignored. Check it to
		 * avoid 'ro' in mtab and 'rw' in /proc/mounts.
		 */
		if ((cxt->mountflags & MS_BIND)
		    && (cxt->mountflags & MS_RDONLY)
		    && !mnt_is_readonly(mnt_context_get_target(cxt)))

			mnt_context_set_mflags(cxt,
					cxt->mountflags & ~MS_RDONLY);


		/* Kernel can silently add MS_RDONLY flag when mounting file
		 * system that does not have write support. Check this to avoid
		 * 'ro' in /proc/mounts and 'rw' in mtab.
		 */
		if (!(cxt->mountflags & (MS_RDONLY | MS_PROPAGATION | MS_MOVE))
		    && mnt_is_readonly(mnt_context_get_target(cxt)))

			mnt_context_set_mflags(cxt,
					cxt->mountflags | MS_RDONLY);
	}

	return res;
}

/**
 * mnt_context_finalize_mount:
 * @cxt: context
 *
 * Mtab update, etc. Unnecessary for mnt_context_mount(), but should be called
 * after mnt_context_do_mount(). See also mnt_context_set_syscall_status().
 *
 * Returns: negative number on error, 0 on success.
 */
int mnt_context_finalize_mount(struct libmnt_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));
	assert((cxt->flags & MNT_FL_PREPARED));

	rc = mnt_context_prepare_update(cxt);
	if (!rc)
		rc = mnt_context_update_tabs(cxt);;
	return rc;
}

/**
 * mnt_context_mount:
 * @cxt: mount context
 *
 * High-level, mounts filesystem by mount(2) or fork()+exec(/sbin/mount.type).
 *
 * This is similar to:
 *
 *	mnt_context_prepare_mount(cxt);
 *	mnt_context_do_mount(cxt);
 *	mnt_context_finalize_mount(cxt);
 *
 * See also mnt_context_disable_helpers().
 *
 * Note that this function could be called only once. If you want to mount with
 * different setting than you have to call mnt_reset_context(). It's NOT enough
 * to call mnt_context_reset_status() if you want call this function more than
 * once, whole context has to be reseted.
 *
 * WARNING: non-zero return code does not mean that mount(2) syscall or
 *          mount.type helper wasn't sucessfully called.
 *
 *          Check mnt_context_get_status() after error!
 *
 * Returns: 0 on success;
 *         >0 in case of mount(2) error (returns syscall errno),
 *         <0 in case of other errors.
 */
int mnt_context_mount(struct libmnt_context *cxt)
{
	int rc;

	assert(cxt);
	assert(cxt->fs);
	assert(cxt->helper_exec_status == 1);
	assert(cxt->syscall_status == 1);

	rc = mnt_context_prepare_mount(cxt);
	if (!rc)
		rc = mnt_context_prepare_update(cxt);
	if (!rc)
		rc = mnt_context_do_mount(cxt);

	/* TODO: if mtab update is expected then check if the
	 * target is really mounted read-write to avoid 'ro' in
	 * mtab and 'rw' in /proc/mounts.
	 */
	if (!rc)
		rc = mnt_context_update_tabs(cxt);
	return rc;
}

/**
 * mnt_context_next_mount:
 * @cxt: context
 * @itr: iterator
 * @fs: returns the current filesystem
 * @mntrc: returns the return code from mnt_context_mount()
 * @ignored: returns 1 for not matching and 2 for already mounted filesystems
 *
 * This function tries to mount the next filesystem from fstab (as returned by
 * mnt_context_get_fstab()). See also mnt_context_set_fstab().
 *
 * You can filter out filesystems by:
 *	mnt_context_set_options_pattern() to simulate mount -a -O pattern
 *	mnt_context_set_fstype_pattern()  to simulate mount -a -t pattern
 *
 * If the filesystem is already mounted or does not match defined criteria,
 * then the mnt_context_next_mount() function returns zero, but the @ignored is
 * non-zero. Note that the root filesystem and filesystems with "noauto" option
 * are always ignored.
 *
 * If mount(2) syscall or mount.type helper failed, then the
 * mnt_context_next_mount() function returns zero, but the @mntrc is non-zero.
 * Use also mnt_context_get_status() to check if the filesystem was
 * successfully mounted.
 *
 * Returns: 0 on success,
 *         <0 in case of error (!= mount(2) errors)
 *          1 at the end of the list.
 */
int mnt_context_next_mount(struct libmnt_context *cxt,
			   struct libmnt_iter *itr,
			   struct libmnt_fs **fs,
			   int *mntrc,
			   int *ignored)
{
	struct libmnt_table *fstab, *mtab;
	const char *o, *tgt;
	int rc, mounted = 0;

	if (ignored)
		*ignored = 0;
	if (mntrc)
		*mntrc = 0;

	if (!cxt || !fs || !itr)
		return -EINVAL;

	mtab = cxt->mtab;
	cxt->mtab = NULL;		/* do not reset mtab */
	mnt_reset_context(cxt);
	cxt->mtab = mtab;

	rc = mnt_context_get_fstab(cxt, &fstab);
	if (rc)
		return rc;

	rc = mnt_table_next_fs(fstab, itr, fs);
	if (rc != 0)
		return rc;	/* more filesystems (or error) */

	o = mnt_fs_get_user_options(*fs);
	tgt = mnt_fs_get_target(*fs);

	DBG(CXT, mnt_debug_h(cxt, "next-mount: trying %s", tgt));

	/*  ignore swap */
	if (mnt_fs_is_swaparea(*fs) ||

	/* ignore root filesystem */
	   (tgt && (strcmp(tgt, "/") == 0 || strcmp(tgt, "root") == 0)) ||

	/* ignore noauto filesystems */
	   (o && mnt_optstr_get_option(o, "noauto", NULL, NULL) == 0) ||

	/* ignore filesystems not match with options patterns */
	   (cxt->fstype_pattern && !mnt_fs_match_fstype(*fs,
					cxt->fstype_pattern)) ||

	/* ignore filesystems not match with type patterns */
	   (cxt->optstr_pattern && !mnt_fs_match_options(*fs,
					cxt->optstr_pattern))) {
		if (ignored)
			*ignored = 1;
		DBG(CXT, mnt_debug_h(cxt, "next-mount: not-match "
				"[fstype: %s, t-pattern: %s, options: %s, O-pattern: %s]",
				mnt_fs_get_fstype(*fs),
				cxt->fstype_pattern,
				mnt_fs_get_options(*fs),
				cxt->optstr_pattern));
		return 0;
	}

	/* ignore already mounted filesystems */
	rc = mnt_context_is_fs_mounted(cxt, *fs, &mounted);
	if (rc)
		return rc;
	if (mounted) {
		if (ignored)
			*ignored = 2;
		return 0;
	}

	if (mnt_context_is_fork(cxt)) {
		rc = mnt_fork_context(cxt);
		if (rc)
			return rc;		/* fork error */

		if (mnt_context_is_parent(cxt)) {
			return 0;		/* parent */
		}
	}

	/* child or non-forked */

	rc = mnt_context_set_fs(cxt, *fs);
	if (!rc) {
		rc = mnt_context_mount(cxt);
		if (mntrc)
			*mntrc = rc;
	}

	if (mnt_context_is_child(cxt)) {
		DBG(CXT, mnt_debug_h(cxt, "next-mount: child exit [rc=%d]", rc));
		DBG_FLUSH;
		exit(rc);
	}
	return 0;
}

