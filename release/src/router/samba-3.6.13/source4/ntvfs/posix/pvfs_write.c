/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - write

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
#include "librpc/gen_ndr/security.h"
#include "lib/events/events.h"

static void pvfs_write_time_update_handler(struct tevent_context *ev,
					   struct tevent_timer *te,
					   struct timeval tv,
					   void *private_data)
{
	struct pvfs_file_handle *h = talloc_get_type(private_data,
				     struct pvfs_file_handle);
	struct odb_lock *lck;
	NTSTATUS status;
	NTTIME write_time;

	lck = odb_lock(h, h->pvfs->odb_context, &h->odb_locking_key);
	if (lck == NULL) {
		DEBUG(0,("Unable to lock opendb for write time update\n"));
		return;
	}

	write_time = timeval_to_nttime(&tv);

	status = odb_set_write_time(lck, write_time, false);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("Unable to update write time: %s\n",
			nt_errstr(status)));
		return;
	}

	talloc_free(lck);

	h->write_time.update_event = NULL;
}

static void pvfs_trigger_write_time_update(struct pvfs_file_handle *h)
{
	struct pvfs_state *pvfs = h->pvfs;
	struct timeval tv;

	if (h->write_time.update_triggered) {
		return;
	}

	tv = timeval_current_ofs(0, pvfs->writetime_delay);

	h->write_time.update_triggered = true;
	h->write_time.update_on_close = true;
	h->write_time.update_event = event_add_timed(pvfs->ntvfs->ctx->event_ctx,
						     h, tv,
						     pvfs_write_time_update_handler,
						     h);
	if (!h->write_time.update_event) {
		DEBUG(0,("Failed event_add_timed\n"));
	}
}

/*
  write to a file
*/
NTSTATUS pvfs_write(struct ntvfs_module_context *ntvfs,
		    struct ntvfs_request *req, union smb_write *wr)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	ssize_t ret;
	struct pvfs_file *f;
	NTSTATUS status;

	if (wr->generic.level != RAW_WRITE_WRITEX) {
		return ntvfs_map_write(ntvfs, req, wr);
	}

	f = pvfs_find_fd(pvfs, req, wr->writex.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (f->handle->fd == -1) {
		return NT_STATUS_INVALID_DEVICE_REQUEST;
	}

	if (!(f->access_mask & (SEC_FILE_WRITE_DATA | SEC_FILE_APPEND_DATA))) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = pvfs_check_lock(pvfs, f, req->smbpid, 
				 wr->writex.in.offset,
				 wr->writex.in.count,
				 WRITE_LOCK);
	NT_STATUS_NOT_OK_RETURN(status);

	status = pvfs_break_level2_oplocks(f);
	NT_STATUS_NOT_OK_RETURN(status);

	pvfs_trigger_write_time_update(f->handle);

	if (f->handle->name->stream_name) {
		ret = pvfs_stream_write(pvfs,
					f->handle,
					wr->writex.in.data, 
					wr->writex.in.count,
					wr->writex.in.offset);
	} else {
#if HAVE_LINUX_AIO
		/* possibly try an aio write */
		if ((req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC) &&
		    (pvfs->flags & PVFS_FLAG_LINUX_AIO)) {
			status = pvfs_aio_pwrite(req, wr, f);
			if (NT_STATUS_IS_OK(status)) {
				return NT_STATUS_OK;
			}
		}
#endif
		ret = pwrite(f->handle->fd, 
			     wr->writex.in.data, 
			     wr->writex.in.count,
			     wr->writex.in.offset);
	}
	if (ret == -1) {
		if (errno == EFBIG) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		return pvfs_map_errno(pvfs, errno);
	}

	f->handle->seek_offset = wr->writex.in.offset + ret;
	
	wr->writex.out.nwritten = ret;
	wr->writex.out.remaining = 0; /* should fill this in? */

	return NT_STATUS_OK;
}
