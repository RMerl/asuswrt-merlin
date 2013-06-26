/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Andrew Tridgell 2005
   
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
/*
  a composite API for loading a whole file into memory
*/

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"

/* the stages of this call */
enum loadfile_stage {LOADFILE_OPEN, LOADFILE_READ, LOADFILE_CLOSE};


static void loadfile_handler(struct smbcli_request *req);

struct loadfile_state {
	enum loadfile_stage stage;
	struct smb_composite_loadfile *io;
	struct smbcli_request *req;
	union smb_open *io_open;
	union smb_read *io_read;
};

/*
  setup for the close
*/
static NTSTATUS setup_close(struct composite_context *c, 
			    struct smbcli_tree *tree, uint16_t fnum)
{
	struct loadfile_state *state = talloc_get_type(c->private_data, struct loadfile_state);
	union smb_close *io_close;

	/* nothing to read, setup the close */
	io_close = talloc(c, union smb_close);
	NT_STATUS_HAVE_NO_MEMORY(io_close);
	
	io_close->close.level = RAW_CLOSE_CLOSE;
	io_close->close.in.file.fnum = fnum;
	io_close->close.in.write_time = 0;

	state->req = smb_raw_close_send(tree, io_close);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* call the handler again when the close is done */
	state->req->async.fn = loadfile_handler;
	state->req->async.private_data = c;
	state->stage = LOADFILE_CLOSE;

	return NT_STATUS_OK;
}

/*
  called when the open is done - pull the results and setup for the
  first readx, or close if the file is zero size
*/
static NTSTATUS loadfile_open(struct composite_context *c, 
			      struct smb_composite_loadfile *io)
{
	struct loadfile_state *state = talloc_get_type(c->private_data, struct loadfile_state);
	struct smbcli_tree *tree = state->req->tree;
	NTSTATUS status;

	status = smb_raw_open_recv(state->req, c, state->io_open);
	NT_STATUS_NOT_OK_RETURN(status);
	
	/* don't allow stupidly large loads */
	if (state->io_open->ntcreatex.out.size > 100*1000*1000) {
		return NT_STATUS_INSUFFICIENT_RESOURCES;
	}

	/* allocate space for the file data */
	io->out.size = state->io_open->ntcreatex.out.size;
	io->out.data = talloc_array(c, uint8_t, io->out.size);
	NT_STATUS_HAVE_NO_MEMORY(io->out.data);

	if (io->out.size == 0) {
		return setup_close(c, tree, state->io_open->ntcreatex.out.file.fnum);
	}

	/* setup for the read */
	state->io_read = talloc(c, union smb_read);
	NT_STATUS_HAVE_NO_MEMORY(state->io_read);
	
	state->io_read->readx.level        = RAW_READ_READX;
	state->io_read->readx.in.file.fnum = state->io_open->ntcreatex.out.file.fnum;
	state->io_read->readx.in.offset    = 0;
	state->io_read->readx.in.mincnt    = MIN(32768, io->out.size);
	state->io_read->readx.in.maxcnt    = state->io_read->readx.in.mincnt;
	state->io_read->readx.in.remaining = 0;
	state->io_read->readx.in.read_for_execute = false;
	state->io_read->readx.out.data     = io->out.data;

	state->req = smb_raw_read_send(tree, state->io_read);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* call the handler again when the first read is done */
	state->req->async.fn = loadfile_handler;
	state->req->async.private_data = c;
	state->stage = LOADFILE_READ;

	talloc_free(state->io_open);

	return NT_STATUS_OK;
}


/*
  called when a read is done - pull the results and setup for the
  next read, or close if the file is all done
*/
static NTSTATUS loadfile_read(struct composite_context *c, 
			      struct smb_composite_loadfile *io)
{
	struct loadfile_state *state = talloc_get_type(c->private_data, struct loadfile_state);
	struct smbcli_tree *tree = state->req->tree;
	NTSTATUS status;

	status = smb_raw_read_recv(state->req, state->io_read);
	NT_STATUS_NOT_OK_RETURN(status);
	
	/* we might be done */
	if (state->io_read->readx.in.offset +
	    state->io_read->readx.out.nread == io->out.size) {
		return setup_close(c, tree, state->io_read->readx.in.file.fnum);
	}

	/* setup for the next read */
	state->io_read->readx.in.offset += state->io_read->readx.out.nread;
	state->io_read->readx.in.mincnt = MIN(32768, io->out.size - state->io_read->readx.in.offset);
	state->io_read->readx.out.data = io->out.data + state->io_read->readx.in.offset;

	state->req = smb_raw_read_send(tree, state->io_read);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* call the handler again when the read is done */
	state->req->async.fn = loadfile_handler;
	state->req->async.private_data = c;

	return NT_STATUS_OK;
}

/*
  called when the close is done, check the status and cleanup
*/
static NTSTATUS loadfile_close(struct composite_context *c, 
			       struct smb_composite_loadfile *io)
{
	struct loadfile_state *state = talloc_get_type(c->private_data, struct loadfile_state);
	NTSTATUS status;

	status = smbcli_request_simple_recv(state->req);
	NT_STATUS_NOT_OK_RETURN(status);
	
	c->state = COMPOSITE_STATE_DONE;

	return NT_STATUS_OK;
}
						     

/*
  handler for completion of a sub-request in loadfile
*/
static void loadfile_handler(struct smbcli_request *req)
{
	struct composite_context *c = (struct composite_context *)req->async.private_data;
	struct loadfile_state *state = talloc_get_type(c->private_data, struct loadfile_state);

	/* when this handler is called, the stage indicates what
	   call has just finished */
	switch (state->stage) {
	case LOADFILE_OPEN:
		c->status = loadfile_open(c, state->io);
		break;

	case LOADFILE_READ:
		c->status = loadfile_read(c, state->io);
		break;

	case LOADFILE_CLOSE:
		c->status = loadfile_close(c, state->io);
		break;
	}

	if (!NT_STATUS_IS_OK(c->status)) {
		c->state = COMPOSITE_STATE_ERROR;
	}

	if (c->state >= COMPOSITE_STATE_DONE &&
	    c->async.fn) {
		c->async.fn(c);
	}
}

/*
  composite loadfile call - does an openx followed by a number of readx calls,
  followed by a close
*/
struct composite_context *smb_composite_loadfile_send(struct smbcli_tree *tree, 
						     struct smb_composite_loadfile *io)
{
	struct composite_context *c;
	struct loadfile_state *state;

	c = talloc_zero(tree, struct composite_context);
	if (c == NULL) goto failed;

	state = talloc(c, struct loadfile_state);
	if (state == NULL) goto failed;

	state->io = io;

	c->private_data = state;
	c->state = COMPOSITE_STATE_IN_PROGRESS;
	c->event_ctx = tree->session->transport->socket->event.ctx;

	/* setup for the open */
	state->io_open = talloc_zero(c, union smb_open);
	if (state->io_open == NULL) goto failed;
	
	state->io_open->ntcreatex.level               = RAW_OPEN_NTCREATEX;
	state->io_open->ntcreatex.in.flags            = NTCREATEX_FLAGS_EXTENDED;
	state->io_open->ntcreatex.in.access_mask      = SEC_FILE_READ_DATA;
	state->io_open->ntcreatex.in.file_attr        = FILE_ATTRIBUTE_NORMAL;
	state->io_open->ntcreatex.in.share_access     = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	state->io_open->ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	state->io_open->ntcreatex.in.impersonation    = NTCREATEX_IMPERSONATION_ANONYMOUS;
	state->io_open->ntcreatex.in.fname            = io->in.fname;

	/* send the open on its way */
	state->req = smb_raw_open_send(tree, state->io_open);
	if (state->req == NULL) goto failed;

	/* setup the callback handler */
	state->req->async.fn = loadfile_handler;
	state->req->async.private_data = c;
	state->stage = LOADFILE_OPEN;

	return c;

failed:
	talloc_free(c);
	return NULL;
}


/*
  composite loadfile call - recv side
*/
NTSTATUS smb_composite_loadfile_recv(struct composite_context *c, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct loadfile_state *state = talloc_get_type(c->private_data, struct loadfile_state);
		talloc_steal(mem_ctx, state->io->out.data);
	}

	talloc_free(c);
	return status;
}


/*
  composite loadfile call - sync interface
*/
NTSTATUS smb_composite_loadfile(struct smbcli_tree *tree, 
				TALLOC_CTX *mem_ctx,
				struct smb_composite_loadfile *io)
{
	struct composite_context *c = smb_composite_loadfile_send(tree, io);
	return smb_composite_loadfile_recv(c, mem_ctx);
}

