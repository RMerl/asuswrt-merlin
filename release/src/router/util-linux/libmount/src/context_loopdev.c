/*
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/*
 * DOCS: - "lo@" prefix for fstype is unsupported
 *	 - encyption= mount option for loop device is unssuported
 */

#include <blkid.h>

#include "mountP.h"
#include "loopdev.h"
#include "linux_version.h"


int mnt_context_is_loopdev(struct libmnt_context *cxt)
{
	const char *type, *src;
	int fl;

	assert(cxt);
	/* The mount flags have to be merged, otherwise we have to use
	 * expensive mnt_context_get_user_mflags() instead of cxt->user_mountflags. */
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	if (!cxt->fs)
		return 0;
	src = mnt_fs_get_srcpath(cxt->fs);
	if (!src)
		return 0;		/* backing file not set */

	if (cxt->user_mountflags & (MNT_MS_LOOP |
				    MNT_MS_OFFSET |
				    MNT_MS_SIZELIMIT))
		return 1;

	if (cxt->mountflags & (MS_BIND | MS_MOVE | MS_PROPAGATION))
		return 0;

	/* Automatically create a loop device from a regular file if a filesystem
	 * is not specified or the filesystem is known for libblkid (these
	 * filesystems work with block devices only).
	 *
	 * Note that there is not a restriction (on kernel side) that prevents regular
	 * file as a mount(2) source argument. A filesystem that is able to mount
	 * regular files could be implemented.
	 */
	type = mnt_fs_get_fstype(cxt->fs);
	fl = __mnt_fs_get_flags(cxt->fs);

	if (!(fl && (MNT_FS_PSEUDO | MNT_FS_NET | MNT_FS_SWAP)) &&
	    (!type || strcmp(type, "auto") == 0 || blkid_known_fstype(type))) {
		struct stat st;

		if (stat(src, &st) || !S_ISREG(st.st_mode))
			return 0;
	}

	return 1;
}

int mnt_context_setup_loopdev(struct libmnt_context *cxt)
{
	const char *backing_file;
	char *loopdev = NULL;
	size_t len;
	struct loopdev_cxt lc;
	int rc, lo_flags = 0;

	assert(cxt);
	assert(cxt->fs);
	assert((cxt->flags & MNT_FL_MOUNTFLAGS_MERGED));

	backing_file = mnt_fs_get_srcpath(cxt->fs);
	if (!backing_file)
		return -EINVAL;

	DBG(CXT, mnt_debug_h(cxt, "trying to setup loopdev for %s", backing_file));

	if (cxt->mountflags & MS_RDONLY) {
		DBG(CXT, mnt_debug_h(cxt, "enabling READ-ONLY flag"));
		lo_flags |= LO_FLAGS_READ_ONLY;
	}
	loopcxt_init(&lc, 0);

	
	if ((cxt->user_mountflags & MNT_MS_LOOP) &&
	    mnt_fs_get_option(cxt->fs, "loop", &loopdev, &len) == 0 && loopdev) {

		char *tmp = strndup(loopdev, len);
		if (!tmp)
			rc = -ENOMEM;
		else {
			rc = loopcxt_set_device(&lc, tmp);
			free(tmp);
		}
	}

	/* since 2.6.37 we don't have to store backing filename to mtab
	 * because kernel provides the name in /sys.
	 */
	if (get_linux_version() >= KERNEL_VERSION(2, 6, 37) ||
	    !cxt->mtab_writable) {
		DBG(CXT, mnt_debug_h(cxt, "enabling AUTOCLEAR flag"));
		lo_flags |= LO_FLAGS_AUTOCLEAR;
	}

	do {
		/* found free device */
		if (!loopdev) {
			rc = loopcxt_find_unused(&lc);
			if (rc)
				goto done;
			DBG(CXT, mnt_debug_h(cxt, "trying to use %s",
						loopcxt_get_device(&lc)));
		}

		/* set device attributes */
		rc = loopcxt_set_backing_file(&lc, backing_file);
		if (rc)
			goto done;

		loopcxt_set_flags(&lc, lo_flags);

		/* setup the device */
		rc = loopcxt_setup_device(&lc);
		if (!rc)
			break;		/* success */

		if (loopdev || rc != -EBUSY) {
			DBG(CXT, mnt_debug_h(cxt, "failed to setup device"));
			break;
		}
		DBG(CXT, mnt_debug_h(cxt, "loopdev stolen...trying again"));
	} while (1);

	if (!rc)
		rc = mnt_fs_set_source(cxt->fs, loopcxt_get_device(&lc));

	if (!rc) {
		/* success */
		cxt->flags |= MNT_FL_LOOPDEV_READY;

		if ((cxt->user_mountflags & MNT_MS_LOOP) &&
		    loopcxt_is_autoclear(&lc))
			/*
			 * autoclear flag accepted by kernel, don't store
			 * the "loop=" option to mtab.
			 */
			cxt->user_mountflags &= ~MNT_MS_LOOP;

		if (!(cxt->mountflags & MS_RDONLY) &&
		    loopcxt_is_readonly(&lc))
			/*
			 * mount planned read-write, but loopdev is read-only,
			 * let's fix mount options...
			 */
			cxt->mountflags |= MS_RDONLY;


		/* we have to keep the device open until mount(1),
		 * otherwise it will auto-cleared by kernel
		 */
		cxt->loopdev_fd = loopcxt_get_fd(&lc);
		loopcxt_set_fd(&lc, -1, 0);
	}
done:
	loopcxt_deinit(&lc);
	return rc;
}

/*
 * Deletes loop device
 */
int mnt_context_delete_loopdev(struct libmnt_context *cxt)
{
	const char *src;
	int rc;

	assert(cxt);
	assert(cxt->fs);

	src = mnt_fs_get_srcpath(cxt->fs);
	if (!src)
		return -EINVAL;

	if (cxt->loopdev_fd > -1)
		close(cxt->loopdev_fd);

	rc = loopdev_delete(src);
	cxt->flags &= ~MNT_FL_LOOPDEV_READY;
	cxt->loopdev_fd = -1;

	DBG(CXT, mnt_debug_h(cxt, "loopdev deleted [rc=%d]", rc));
	return rc;
}

/*
 * Clears loopdev stuff in context, should be called after
 * failed or successful mount(2).
 */
int mnt_context_clear_loopdev(struct libmnt_context *cxt)
{
	assert(cxt);

	if (mnt_context_get_status(cxt) == 0 &&
	    (cxt->flags & MNT_FL_LOOPDEV_READY)) {
		/*
		 * mount(2) failed, delete loopdev
		 */
		mnt_context_delete_loopdev(cxt);

	} else if (cxt->loopdev_fd > -1) {
		/*
		 * mount(2) success, close the device
		 */
		DBG(CXT, mnt_debug_h(cxt, "closing loopdev FD"));
		close(cxt->loopdev_fd);
	}
	cxt->loopdev_fd = -1;
	return 0;
}

