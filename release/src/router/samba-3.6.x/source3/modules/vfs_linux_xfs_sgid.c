/*
 * Module to work around a bug in Linux XFS:
 * http://oss.sgi.com/bugzilla/show_bug.cgi?id=280
 *
 * Copyright (c) Volker Lendecke 2010
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
#include "system/filesys.h"
#include "smbd/smbd.h"

static int linux_xfs_sgid_mkdir(vfs_handle_struct *handle,  const char *path, mode_t mode)
{
	struct smb_filename fname = { 0, };
	int mkdir_res;
	int res;

	DEBUG(10, ("Calling linux_xfs_sgid_mkdir(%s)\n", path));

	mkdir_res = SMB_VFS_NEXT_MKDIR(handle, path, mode);
	if (mkdir_res == -1) {
		DEBUG(10, ("SMB_VFS_NEXT_MKDIR returned error: %s\n",
			   strerror(errno)));
		return mkdir_res;
	}

	if (!parent_dirname(talloc_tos(), path, &fname.base_name, NULL)) {
		DEBUG(1, ("parent_dirname failed\n"));
		/* return success, we did the mkdir */
		return mkdir_res;
	}

	res = SMB_VFS_NEXT_STAT(handle, &fname);
	if (res == -1) {
		DEBUG(10, ("NEXT_STAT(%s) failed: %s\n", fname.base_name,
			   strerror(errno)));
		/* return success, we did the mkdir */
		return mkdir_res;
	}
	TALLOC_FREE(fname.base_name);
	if ((fname.st.st_ex_mode & S_ISGID) == 0) {
		/* No SGID to inherit */
		DEBUG(10, ("No SGID to inherit\n"));
		return mkdir_res;
	}

	fname.base_name = discard_const_p(char, path);

	res = SMB_VFS_NEXT_STAT(handle, &fname);
	if (res == -1) {
		DEBUG(2, ("Could not stat just created dir %s: %s\n", path,
			  strerror(errno)));
		/* return success, we did the mkdir */
		return mkdir_res;
	}
	fname.st.st_ex_mode |= S_ISGID;
	fname.st.st_ex_mode &= ~S_IFDIR;

	/*
	 * Yes, we have to do this as root. If you do it as
	 * non-privileged user, XFS on Linux will just ignore us and
	 * return success. What can you do...
	 */
	become_root();
	res = SMB_VFS_NEXT_CHMOD(handle, path, fname.st.st_ex_mode);
	unbecome_root();

	if (res == -1) {
		DEBUG(2, ("CHMOD(%s, %o) failed: %s\n", path,
			  (int)fname.st.st_ex_mode, strerror(errno)));
		/* return success, we did the mkdir */
		return mkdir_res;
	}
	return mkdir_res;
}

static int linux_xfs_sgid_chmod_acl(vfs_handle_struct *handle,
				    const char *name, mode_t mode)
{
	errno = ENOSYS;
	return -1;
}

static struct vfs_fn_pointers linux_xfs_sgid_fns = {
	.mkdir = linux_xfs_sgid_mkdir,
	.chmod_acl = linux_xfs_sgid_chmod_acl,
};

NTSTATUS vfs_linux_xfs_sgid_init(void);
NTSTATUS vfs_linux_xfs_sgid_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				"linux_xfs_sgid", &linux_xfs_sgid_fns);
}
