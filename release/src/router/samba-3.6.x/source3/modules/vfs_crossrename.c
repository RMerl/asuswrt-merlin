/*
 * Copyright (c) Björn Jacke 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "smbd/smbd.h"
#include "system/filesys.h"
#include "transfer_file.h"
#include "smbprofile.h"

#define MODULE "crossrename"
static SMB_OFF_T module_sizelimit;

static int crossrename_connect(
                struct vfs_handle_struct *  handle,
                const char *                service,
                const char *                user)
{
	int ret = SMB_VFS_NEXT_CONNECT(handle, service, user);

	if (ret < 0) {
		return ret;
	}

	module_sizelimit = (SMB_OFF_T) lp_parm_int(SNUM(handle->conn),
					MODULE, "sizelimit", 20);
	/* convert from MiB to byte: */
	module_sizelimit *= 1048576;

	return 0;
}

/*********************************************************
 For rename across filesystems initial Patch from Warren Birnbaum
 <warrenb@hpcvscdp.cv.hp.com>
**********************************************************/

static int copy_reg(const char *source, const char *dest)
{
	SMB_STRUCT_STAT source_stats;
	int saved_errno;
	int ifd = -1;
	int ofd = -1;

	if (sys_lstat(source, &source_stats, false) == -1)
		return -1;

	if (!S_ISREG (source_stats.st_ex_mode))
		return -1;

	if (source_stats.st_ex_size > module_sizelimit) {
		DEBUG(5,
			("%s: size of %s larger than sizelimit (%lld > %lld), rename prohititted\n",
			MODULE, source,
			(long long)source_stats.st_ex_size,
			(long long)module_sizelimit));
		return -1;
	}

	if((ifd = sys_open (source, O_RDONLY, 0)) < 0)
		return -1;

	if (unlink (dest) && errno != ENOENT)
		return -1;

#ifdef O_NOFOLLOW
	if((ofd = sys_open (dest, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600)) < 0 )
#else
	if((ofd = sys_open (dest, O_WRONLY | O_CREAT | O_TRUNC , 0600)) < 0 )
#endif
		goto err;

	if (transfer_file(ifd, ofd, source_stats.st_ex_size) == -1)
		goto err;

	/*
	 * Try to preserve ownership.  For non-root it might fail, but that's ok.
	 * But root probably wants to know, e.g. if NFS disallows it.
	 */

#ifdef HAVE_FCHOWN
	if ((fchown(ofd, source_stats.st_ex_uid, source_stats.st_ex_gid) == -1) && (errno != EPERM))
#else
	if ((chown(dest, source_stats.st_ex_uid, source_stats.st_ex_gid) == -1) && (errno != EPERM))
#endif
		goto err;

	/*
	 * fchown turns off set[ug]id bits for non-root,
	 * so do the chmod last.
	 */

#if defined(HAVE_FCHMOD)
	if (fchmod (ofd, source_stats.st_ex_mode & 07777))
#else
	if (chmod (dest, source_stats.st_ex_mode & 07777))
#endif
		goto err;

	if (close (ifd) == -1)
		goto err;

	if (close (ofd) == -1)
		return -1;

	/* Try to copy the old file's modtime and access time.  */
#if defined(HAVE_UTIMENSAT)
	{
		struct timespec ts[2];

		ts[0] = source_stats.st_ex_atime;
		ts[1] = source_stats.st_ex_mtime;
		utimensat(AT_FDCWD, dest, ts, AT_SYMLINK_NOFOLLOW);
	}
#elif defined(HAVE_UTIMES)
	{
		struct timeval tv[2];

		tv[0] = convert_timespec_to_timeval(source_stats.st_ex_atime);
		tv[1] = convert_timespec_to_timeval(source_stats.st_ex_mtime);
#ifdef HAVE_LUTIMES
		lutimes(dest, tv);
#else
		utimes(dest, tv);
#endif
	}
#elif defined(HAVE_UTIME)
	{
		struct utimbuf tv;

		tv.actime = convert_timespec_to_time_t(source_stats.st_ex_atime);
		tv.modtime = convert_timespec_to_time_t(source_stats.st_ex_mtime);
		utime(dest, &tv);
	}
#endif

	if (unlink (source) == -1)
		return -1;

	return 0;

  err:

	saved_errno = errno;
	if (ifd != -1)
		close(ifd);
	if (ofd != -1)
		close(ofd);
	errno = saved_errno;
	return -1;
}


static int crossrename_rename(vfs_handle_struct *handle,
			  const struct smb_filename *smb_fname_src,
			  const struct smb_filename *smb_fname_dst)
{
	int result = -1;

	START_PROFILE(syscall_rename);

	if (smb_fname_src->stream_name || smb_fname_dst->stream_name) {
		errno = ENOENT;
		goto out;
	}

	result = rename(smb_fname_src->base_name, smb_fname_dst->base_name);
	if ((result == -1) && (errno == EXDEV)) {
		/* Rename across filesystems needed. */
		result = copy_reg(smb_fname_src->base_name,
				  smb_fname_dst->base_name);
	}

 out:
	END_PROFILE(syscall_rename);
	return result;
}

static struct vfs_fn_pointers vfs_crossrename_fns = {
	.connect_fn = crossrename_connect,
	.rename = crossrename_rename
};

NTSTATUS vfs_crossrename_init(void);
NTSTATUS vfs_crossrename_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, MODULE,
				&vfs_crossrename_fns);
}

