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
  a composite API for saving a whole file from memory
*/

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"

/* the stages of this call */
enum savefile_stage {SAVEFILE_OPEN, SAVEFILE_WRITE, SAVEFILE_CLOSE};

static void savefile_handler(struct smbcli_request *req);

struct savefile_state {
	enum savefile_stage stage;
	off_t total_written;
	struct smb_composite_savefile *io;
	union smb_open *io_open;
	union smb_write *io_write;
	struct smbcli_request *req;
};


/*
  setup for the close
*/
static NTSTATUS setup_close(struct composite_context *c, 
			    struct smbcli_tree *tree, uint16_t fnum)
{
	struct savefile_state *state = talloc_get_type(c->private_data, struct savefile_state);
	union smb_close *io_close;

	/* nothing to write, setup the close */
	io_close = talloc(c, union smb_close);
	NT_STATUS_HAVE_NO_MEMORY(io_close);
	
	io_close->close.level = RAW_CLOSE_CLOSE;
	io_close->close.in.file.fnum = fnum;
	io_close->close.in.write_time = 0;

	state->req = smb_raw_close_send(tree, io_close);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* call the handler again when the close is done */
	state->stage = SAVEFILE_CLOSE;
	state->req->async.fn = savefile_handler;
	state->req->async.private_data = c;

	return NT_STATUS_OK;
}

/*
  called when the open is done - pull the results and setup for the
  first writex, or close if the file is zero size
*/
static NTSTATUS savefile_open(struct composite_context *c, 
			      struct smb_composite_savefile *io)
{
	struct savefile_state *state = talloc_get_type(c->private_data, struct savefile_state);
	union smb_write *io_write;
	struct smbcli_tree *tree = state->req->tree;
	NTSTATUS status;
	uint32_t max_xmit = tree->session->transport->negotiate.max_xmit;

	status = smb_raw_open_recv(state->req, c, state->io_open);
	NT_STATUS_NOT_OK_RETURN(status);
	
	if (io->in.size == 0) {
		return setup_close(c, tree, state->io_open->ntcreatex.out.file.fnum);
	}

	/* setup for the first write */
	io_write = talloc(c, union smb_write);
	NT_STATUS_HAVE_NO_MEMORY(io_write);
	
	io_write->writex.level        = RAW_WRITE_WRITEX;
	io_write->writex.in.file.fnum = state->io_open->ntcreatex.out.file.fnum;
	io_write->writex.in.offset    = 0;
	io_write->writex.in.wmode     = 0;
	io_write->writex.in.remaining = 0;
	io_write->writex.in.count     = MIN(max_xmit - 100, io->in.size);
	io_write->writex.in.data      = io->in.data;
	state->io_write = io_write;

	state->req = smb_raw_write_send(tree, io_write);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* call the handler again when the first write is done */
	state->stage = SAVEFILE_WRITE;
	state->req->async.fn = savefile_handler;
	state->req->async.private_data = c;
	talloc_free(state->io_open);

	return NT_STATUS_OK;
}


/*
  called when a write is done - pull the results and setup for the
  next write, or close if the file is all done
*/
static NTSTATUS savefile_write(struct composite_context *c, 
			      struct smb_composite_savefile *io)
{
	struct savefile_state *state = talloc_get_type(c->private_data, struct savefile_state);
	struct smbcli_tree *tree = state->req->tree;
	NTSTATUS status;
	uint32_t max_xmit = tree->session->transport->negotiate.max_xmit;

	status = smb_raw_write_recv(state->req, state->io_write);
	NT_STATUS_NOT_OK_RETURN(status);

	state->total_written += state->io_write->writex.out.nwritten;
	
	/* we might be done */
	if (state->io_write->writex.out.nwritten != state->io_write->writex.in.count ||
	    state->total_written == io->in.size) {
		return setup_close(c, tree, state->io_write->writex.in.file.fnum);
	}

	/* setup for the next write */
	state->io_write->writex.in.offset = state->total_written;
	state->io_write->writex.in.count = MIN(max_xmit - 100, 
					       io->in.size - state->total_written);
	state->io_write->writex.in.data = io->in.data + state->total_written;

	state->req = smb_raw_write_send(tree, state->io_write);
	NT_STATUS_HAVE_NO_MEMORY(state->req);

	/* call the handler again when the write is done */
	state->req->async.fn = savefile_handler;
	state->req->async.private_data = c;

	return NT_STATUS_OK;
}

/*
  called when the close is done, check the status and cleanup
*/
static NTSTATUS savefile_close(struct composite_context *c, 
			       struct smb_composite_savefile *io)
{
	struct savefile_state *state = talloc_get_type(c->private_data, struct savefile_state);
	NTSTATUS status;

	status = smbcli_request_simple_recv(state->req);
	NT_STATUS_NOT_OK_RETURN(status);

	if (state->total_written != io->in.size) {
		return NT_STATUS_DISK_FULL;
	}
	
	c->state = COMPOSITE_STATE_DONE;

	return NT_STATUS_OK;
}
						     

/*
  handler for completion of a sub-request in savefile
*/
static void savefile_handler(struct smbcli_request *req)
{
	struct composite_context *c = (struct composite_context *)req->async.private_data;
	struct savefile_state *state = talloc_get_type(c->private_data, struct savefile_state);

	/* when this handler is called, the stage indicates what
	   call has just finished */
	switch (state->stage) {
	case SAVEFILE_OPEN:
		c->status = savefile_open(c, state->io);
		break;

	case SAVEFILE_WRITE:
		c->status = savefile_write(c, state->io);
		break;

	case SAVEFILE_CLOSE:
		c->status = savefile_close(c, state->io);
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
  composite savefile call - does an openx followed by a number of writex calls,
  followed by a close
*/
struct composite_context *smb_composite_savefile_send(struct smbcli_tree *tree, 
						      struct smb_composite_savefile *io)
{
	struct composite_context *c;
	struct savefile_state *state;
	union smb_open *io_open;

	c = talloc_zero(tree, struct composite_context);
	if (c == NULL) goto failed;

	c->state = COMPOSITE_STATE_IN_PROGRESS;
	c->event_ctx = tree->session->transport->socket->event.ctx;

	state = talloc(c, struct savefile_state);
	if (state == NULL) goto failed;

	state->stage = SAVEFILE_OPEN;
	state->total_written = 0;
	state->io = io;

	/* setup for the open */
	io_open = talloc_zero(c, union smb_open);
	if (io_open == NULL) goto failed;
	
	io_open->ntcreatex.level               = RAW_OPEN_NTCREATEX;
	io_open->ntcreatex.in.flags            = NTCREATEX_FLAGS_EXTENDED;
	io_open->ntcreatex.in.access_mask      = SEC_FILE_WRITE_DATA;
	io_open->ntcreatex.in.file_attr        = FILE_ATTRIBUTE_NORMAL;
	io_open->ntcreatex.in.share_access     = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io_open->ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io_open->ntcreatex.in.impersonation    = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io_open->ntcreatex.in.fname            = io->in.fname;
	state->io_open = io_open;

	/* send the open on its way */
	state->req = smb_raw_open_send(tree, io_open);
	if (state->req == NULL) goto failed;

	/* setup the callback handler */
	state->req->async.fn = savefile_handler;
	state->req->async.private_data = c;
	c->private_data = state;

	return c;

failed:
	talloc_free(c);
	return NULL;
}


/*
  composite savefile call - recv side
*/
NTSTATUS smb_composite_savefile_recv(struct composite_context *c)
{
	NTSTATUS status;
	status = composite_wait(c);
	talloc_free(c);
	return status;
}


/*
  composite savefile call - sync interface
*/
NTSTATUS smb_composite_savefile(struct smbcli_tree *tree, 
				struct smb_composite_savefile *io)
{
	struct composite_context *c = smb_composite_savefile_send(tree, io);
	return smb_composite_savefile_recv(c);
}
