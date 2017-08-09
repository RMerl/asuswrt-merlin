/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - flush

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

/*
  flush a single open file
*/
static void pvfs_flush_file(struct pvfs_state *pvfs, struct pvfs_file *f)
{
	if (f->handle->fd == -1) {
		return;
	}
	if (pvfs->flags & PVFS_FLAG_STRICT_SYNC) {
		fsync(f->handle->fd);
	}
}

/*
  flush a fnum
*/
NTSTATUS pvfs_flush(struct ntvfs_module_context *ntvfs,
		    struct ntvfs_request *req,
		    union smb_flush *io)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_file *f;

	switch (io->generic.level) {
	case RAW_FLUSH_FLUSH:
	case RAW_FLUSH_SMB2:
		/* TODO: take care of io->smb2.in.unknown */
		f = pvfs_find_fd(pvfs, req, io->generic.in.file.ntvfs);
		if (!f) {
			return NT_STATUS_INVALID_HANDLE;
		}
		pvfs_flush_file(pvfs, f);
		io->smb2.out.reserved = 0;
		return NT_STATUS_OK;

	case RAW_FLUSH_ALL:
		if (!(pvfs->flags & PVFS_FLAG_STRICT_SYNC)) {
			return NT_STATUS_OK;
		}

		/* 
		 * they are asking to flush all open files
		 * for the given SMBPID
		 */
		for (f=pvfs->files.list;f;f=f->next) {
			if (f->ntvfs->smbpid != req->smbpid) continue;

			pvfs_flush_file(pvfs, f);
		}

		return NT_STATUS_OK;
	}

	return NT_STATUS_INVALID_LEVEL;
}
