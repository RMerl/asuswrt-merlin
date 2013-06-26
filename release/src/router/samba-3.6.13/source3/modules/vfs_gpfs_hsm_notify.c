/*
   Unix SMB/CIFS implementation.
   Make sure offline->online changes are propagated by notifies

   This module must come before aio_fork in the chain, because
   aio_fork (correcly!) does not propagate the aio calls further

   Copyright (C) Volker Lendecke 2011

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "smbd/smbd.h"
#include "librpc/gen_ndr/ndr_xattr.h"
#include "include/smbprofile.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_VFS

#include <gpfs_gpl.h>
#include "nfs4_acls.h"
#include "vfs_gpfs.h"

static ssize_t vfs_gpfs_hsm_notify_pread(vfs_handle_struct *handle, files_struct *fsp,
			      void *data, size_t n, SMB_OFF_T offset)
{
	ssize_t ret;

	ret = SMB_VFS_NEXT_PREAD(handle, fsp, data, n, offset);

	DEBUG(10, ("vfs_private = %x\n",
		   (unsigned int)fsp->fsp_name->st.vfs_private));

	if ((ret != -1) &&
	    ((fsp->fsp_name->st.vfs_private & GPFS_WINATTR_OFFLINE) != 0)) {
		fsp->fsp_name->st.vfs_private &= ~GPFS_WINATTR_OFFLINE;
		notify_fname(handle->conn, NOTIFY_ACTION_MODIFIED,
			     FILE_NOTIFY_CHANGE_ATTRIBUTES,
			     fsp->fsp_name->base_name);
	}

	return ret;
}

static ssize_t vfs_gpfs_hsm_notify_pwrite(struct vfs_handle_struct *handle,
			       struct files_struct *fsp,
			       const void *data, size_t n, SMB_OFF_T offset)
{
	ssize_t ret;

	ret = SMB_VFS_NEXT_PWRITE(handle, fsp, data, n, offset);

	if ((ret != -1) &&
	    ((fsp->fsp_name->st.vfs_private & GPFS_WINATTR_OFFLINE) != 0)) {
		fsp->fsp_name->st.vfs_private &= ~GPFS_WINATTR_OFFLINE;
		notify_fname(handle->conn, NOTIFY_ACTION_MODIFIED,
			     FILE_NOTIFY_CHANGE_ATTRIBUTES,
			     fsp->fsp_name->base_name);
	}

	return ret;
}

static ssize_t vfs_gpfs_hsm_notify_aio_return(struct vfs_handle_struct *handle,
				   struct files_struct *fsp,
				   SMB_STRUCT_AIOCB *aiocb)
{
	ssize_t ret;

	ret = SMB_VFS_NEXT_AIO_RETURN(handle, fsp, aiocb);

	DEBUG(10, ("vfs_gpfs_hsm_notify_aio_return: vfs_private = %x\n",
		   (unsigned int)fsp->fsp_name->st.vfs_private));

	if ((ret != -1) &&
	    ((fsp->fsp_name->st.vfs_private & GPFS_WINATTR_OFFLINE) != 0)) {
		fsp->fsp_name->st.vfs_private &= ~GPFS_WINATTR_OFFLINE;
		DEBUG(10, ("sending notify\n"));
		notify_fname(handle->conn, NOTIFY_ACTION_MODIFIED,
			     FILE_NOTIFY_CHANGE_ATTRIBUTES,
			     fsp->fsp_name->base_name);
	}

	return ret;
}

static struct vfs_fn_pointers vfs_gpfs_hsm_notify_fns = {
	.pread = vfs_gpfs_hsm_notify_pread,
	.pwrite = vfs_gpfs_hsm_notify_pwrite,
	.aio_return_fn = vfs_gpfs_hsm_notify_aio_return
};

NTSTATUS vfs_gpfs_hsm_notify_init(void);
NTSTATUS vfs_gpfs_hsm_notify_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION, "gpfs_hsm_notify",
				&vfs_gpfs_hsm_notify_fns);
}
