/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - Linux AIO calls

   Copyright (C) Andrew Tridgell 2006

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
#include <tevent.h>
#include "vfs_posix.h"
#include "system/aio.h"

struct pvfs_aio_read_state {
	struct ntvfs_request *req;
	union smb_read *rd;
	struct pvfs_file *f;
	struct tevent_aio *ae;
};

struct pvfs_aio_write_state {
	struct ntvfs_request *req;
	union smb_write *wr;
	struct pvfs_file *f;
	struct tevent_aio *ae;
};

/*
  called when an aio read has finished
*/
static void pvfs_aio_read_handler(struct tevent_context *ev, struct tevent_aio *ae,
			     int ret, void *private_data)
{
	struct pvfs_aio_read_state *state = talloc_get_type(private_data,
							    struct pvfs_aio_read_state);
	struct pvfs_file *f = state->f;
	union smb_read *rd = state->rd;

	if (ret < 0) {
		/* errno is -ret on error */
		state->req->async_states->status = pvfs_map_errno(f->pvfs, -ret);
		state->req->async_states->send_fn(state->req);
		return;
	}

	f->handle->position = f->handle->seek_offset = rd->readx.in.offset + ret;

	rd->readx.out.nread = ret;
	rd->readx.out.remaining = 0xFFFF;
	rd->readx.out.compaction_mode = 0; 

	talloc_steal(ev, state->ae);

	state->req->async_states->status = NT_STATUS_OK;
	state->req->async_states->send_fn(state->req);
}


/*
  read from a file
*/
NTSTATUS pvfs_aio_pread(struct ntvfs_request *req, union smb_read *rd,
			struct pvfs_file *f, uint32_t maxcnt)
{
	struct iocb iocb;
	struct pvfs_aio_read_state *state;

	state = talloc(req, struct pvfs_aio_read_state);
	NT_STATUS_HAVE_NO_MEMORY(state);

        io_prep_pread(&iocb, f->handle->fd, rd->readx.out.data,
		      maxcnt, rd->readx.in.offset);
	state->ae = tevent_add_aio(req->ctx->event_ctx, req->ctx->event_ctx, &iocb,
				   pvfs_aio_read_handler, state);
	if (state->ae == NULL) {
		DEBUG(0,("Failed tevent_add_aio\n"));
		talloc_free(state);
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	state->req  = req;
	state->rd   = rd;
	state->f    = f;

	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;

	return NT_STATUS_OK;
}




/*
  called when an aio write has finished
*/
static void pvfs_aio_write_handler(struct tevent_context *ev, struct tevent_aio *ae,
			     int ret, void *private_data)
{
	struct pvfs_aio_write_state *state = talloc_get_type(private_data,
							    struct pvfs_aio_write_state);
	struct pvfs_file *f = state->f;
	union smb_write *wr = state->wr;

	if (ret < 0) {
		/* errno is -ret on error */
		state->req->async_states->status = pvfs_map_errno(f->pvfs, -ret);
		state->req->async_states->send_fn(state->req);
		return;
	}

	f->handle->seek_offset = wr->writex.in.offset + ret;

	wr->writex.out.nwritten = ret;
	wr->writex.out.remaining = 0;

	talloc_steal(ev, state->ae);

	state->req->async_states->status = NT_STATUS_OK;
	state->req->async_states->send_fn(state->req);
}


/*
  write to a file
*/
NTSTATUS pvfs_aio_pwrite(struct ntvfs_request *req, union smb_write *wr,
			 struct pvfs_file *f)
{
	struct iocb iocb;
	struct pvfs_aio_write_state *state;

	state = talloc(req, struct pvfs_aio_write_state);
	NT_STATUS_HAVE_NO_MEMORY(state);

	io_prep_pwrite(&iocb, f->handle->fd, discard_const(wr->writex.in.data),
		       wr->writex.in.count, wr->writex.in.offset);
	state->ae = tevent_add_aio(req->ctx->event_ctx, req->ctx->event_ctx, &iocb,
				   pvfs_aio_write_handler, state);
	if (state->ae == NULL) {
		DEBUG(0,("Failed tevent_add_aio\n"));
		talloc_free(state);
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	state->req  = req;
	state->wr   = wr;
	state->f    = f;

	req->async_states->state |= NTVFS_ASYNC_STATE_ASYNC;

	return NT_STATUS_OK;
}

