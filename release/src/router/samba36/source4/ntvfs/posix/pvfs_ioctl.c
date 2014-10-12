/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - open and close

   Copyright (C) Andrew Tridgell 2004

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
#include "vfs_posix.h"
#include "libcli/raw/ioctl.h"

/*
  old ioctl interface 
*/
static NTSTATUS pvfs_ioctl_old(struct ntvfs_module_context *ntvfs,
			struct ntvfs_request *req, union smb_ioctl *io)
{
	return NT_STATUS_DOS(ERRSRV, ERRerror);
}

/*
  nt ioctl interface 
*/
static NTSTATUS pvfs_ntioctl(struct ntvfs_module_context *ntvfs,
			     struct ntvfs_request *req, union smb_ioctl *io)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_file *f;

	f = pvfs_find_fd(pvfs, req, io->ntioctl.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	switch (io->ntioctl.in.function) {
	case FSCTL_SET_SPARSE:
		/* maybe some posix systems have a way of marking
		   a file non-sparse? */
		io->ntioctl.out.blob = data_blob(NULL, 0);
		return NT_STATUS_OK;
	}

	return NT_STATUS_NOT_SUPPORTED;
}

/*
  ioctl interface 
*/
NTSTATUS pvfs_ioctl(struct ntvfs_module_context *ntvfs,
		    struct ntvfs_request *req,
		    union smb_ioctl *io)
{
	switch (io->generic.level) {
	case RAW_IOCTL_IOCTL:
		return pvfs_ioctl_old(ntvfs, req, io);

	case RAW_IOCTL_NTIOCTL:
		return pvfs_ntioctl(ntvfs, req, io);

	case RAW_IOCTL_SMB2:
	case RAW_IOCTL_SMB2_NO_HANDLE:
		/* see WSPP SMB2 test 46 */
		return NT_STATUS_INVALID_DEVICE_REQUEST;
	}

	return NT_STATUS_INVALID_LEVEL;
}
