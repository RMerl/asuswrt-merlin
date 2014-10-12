/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - read

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
#include "lib/events/events.h"

/*
  read from a file
*/
NTSTATUS pvfs_read(struct ntvfs_module_context *ntvfs,
		   struct ntvfs_request *req, union smb_read *rd)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	ssize_t ret;
	struct pvfs_file *f;
	NTSTATUS status;
	uint32_t maxcnt;
	uint32_t mask;

	if (rd->generic.level != RAW_READ_READX) {
		return ntvfs_map_read(ntvfs, req, rd);
	}

	f = pvfs_find_fd(pvfs, req, rd->readx.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (f->handle->fd == -1) {
		return NT_STATUS_INVALID_DEVICE_REQUEST;
	}

	mask = SEC_FILE_READ_DATA;
	if (rd->readx.in.read_for_execute) {
		mask |= SEC_FILE_EXECUTE;
	}
	if (!(f->access_mask & mask)) {
		return NT_STATUS_ACCESS_DENIED;
	}

	maxcnt = rd->readx.in.maxcnt;
	if (maxcnt > 2*UINT16_MAX && req->ctx->protocol < PROTOCOL_SMB2) {
		DEBUG(3,(__location__ ": Invalid SMB maxcnt 0x%x\n", maxcnt));
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = pvfs_check_lock(pvfs, f, req->smbpid, 
				 rd->readx.in.offset,
				 maxcnt,
				 READ_LOCK);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (f->handle->name->stream_name) {
		ret = pvfs_stream_read(pvfs, f->handle, 
				       rd->readx.out.data, maxcnt, rd->readx.in.offset);
	} else {
#if HAVE_LINUX_AIO
		/* possibly try an aio read */
		if ((req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC) &&
		    (pvfs->flags & PVFS_FLAG_LINUX_AIO)) {
			status = pvfs_aio_pread(req, rd, f, maxcnt);
			if (NT_STATUS_IS_OK(status)) {
				return NT_STATUS_OK;
			}
		}
#endif
		ret = pread(f->handle->fd, 
			    rd->readx.out.data, 
			    maxcnt,
			    rd->readx.in.offset);
	}
	if (ret == -1) {
		return pvfs_map_errno(pvfs, errno);
	}

	/* only SMB2 honors mincnt */
	if (req->ctx->protocol == PROTOCOL_SMB2) {
		if (rd->readx.in.mincnt > ret ||
		    (ret == 0 && maxcnt > 0)) {
			return NT_STATUS_END_OF_FILE;
		}
	}

	f->handle->position = f->handle->seek_offset = rd->readx.in.offset + ret;

	rd->readx.out.nread = ret;
	rd->readx.out.remaining = 0xFFFF;
	rd->readx.out.compaction_mode = 0; 

	return NT_STATUS_OK;
}
