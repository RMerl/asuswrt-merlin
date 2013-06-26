/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Volker Lendecke 2005
   
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
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/resolve/resolve.h"

enum fetchfile_stage {FETCHFILE_CONNECT,
		      FETCHFILE_READ};

struct fetchfile_state {
	enum fetchfile_stage stage;
	struct smb_composite_fetchfile *io;
	struct composite_context *creq;
	struct smb_composite_connect *connect;
	struct smb_composite_loadfile *loadfile;
};

static void fetchfile_composite_handler(struct composite_context *req);

static NTSTATUS fetchfile_connect(struct composite_context *c,
				  struct smb_composite_fetchfile *io)
{
	NTSTATUS status;
	struct fetchfile_state *state;
	state = talloc_get_type(c->private_data, struct fetchfile_state);

	status = smb_composite_connect_recv(state->creq, c);
	NT_STATUS_NOT_OK_RETURN(status);

	state->loadfile = talloc(state, struct smb_composite_loadfile);
	NT_STATUS_HAVE_NO_MEMORY(state->loadfile);

	state->loadfile->in.fname = io->in.filename;

	state->creq = smb_composite_loadfile_send(state->connect->out.tree,
						 state->loadfile);
	NT_STATUS_HAVE_NO_MEMORY(state->creq);

	state->creq->async.private_data = c;
	state->creq->async.fn = fetchfile_composite_handler;

	state->stage = FETCHFILE_READ;

	return NT_STATUS_OK;
}

static NTSTATUS fetchfile_read(struct composite_context *c,
			       struct smb_composite_fetchfile *io)
{
	NTSTATUS status;
	struct fetchfile_state *state;
	state = talloc_get_type(c->private_data, struct fetchfile_state);

	status = smb_composite_loadfile_recv(state->creq, NULL);
	NT_STATUS_NOT_OK_RETURN(status);

	io->out.data = state->loadfile->out.data;
	io->out.size = state->loadfile->out.size;

	c->state = COMPOSITE_STATE_DONE;
	if (c->async.fn)
		c->async.fn(c);

	return NT_STATUS_OK;
}

static void fetchfile_state_handler(struct composite_context *c)
{
	struct fetchfile_state *state;
	NTSTATUS status = NT_STATUS_UNSUCCESSFUL;
	
	state = talloc_get_type(c->private_data, struct fetchfile_state);

	/* when this handler is called, the stage indicates what
	   call has just finished */
	switch (state->stage) {
	case FETCHFILE_CONNECT:
		status = fetchfile_connect(c, state->io);
		break;
	case FETCHFILE_READ:
		status = fetchfile_read(c, state->io);
		break;
	}

	if (!NT_STATUS_IS_OK(status)) {
		c->status = status;
		c->state = COMPOSITE_STATE_ERROR;
		if (c->async.fn) {
			c->async.fn(c);
		}
	}
}

static void fetchfile_composite_handler(struct composite_context *creq)
{
	struct composite_context *c = talloc_get_type(creq->async.private_data, 
						      struct composite_context);
	fetchfile_state_handler(c);
}

struct composite_context *smb_composite_fetchfile_send(struct smb_composite_fetchfile *io,
						       struct tevent_context *event_ctx)
{
	struct composite_context *c;
	struct fetchfile_state *state;

	c = talloc_zero(NULL, struct composite_context);
	if (c == NULL) goto failed;

	state = talloc(c, struct fetchfile_state);
	if (state == NULL) goto failed;

	state->connect = talloc(state, struct smb_composite_connect);
	if (state->connect == NULL) goto failed;

	state->io = io;

	state->connect->in.dest_host    = io->in.dest_host;
	state->connect->in.dest_ports   = io->in.ports;
	state->connect->in.socket_options = io->in.socket_options;
	state->connect->in.called_name  = io->in.called_name;
	state->connect->in.service      = io->in.service;
	state->connect->in.service_type = io->in.service_type;
	state->connect->in.credentials  = io->in.credentials;
	state->connect->in.fallback_to_anonymous = false;
	state->connect->in.workgroup    = io->in.workgroup;
	state->connect->in.gensec_settings = io->in.gensec_settings;

	state->connect->in.options	= io->in.options;
	state->connect->in.session_options = io->in.session_options;

	state->creq = smb_composite_connect_send(state->connect, state, 
						 io->in.resolve_ctx, event_ctx);
	if (state->creq == NULL) goto failed;

	state->creq->async.private_data = c;
	state->creq->async.fn = fetchfile_composite_handler;

	c->state = COMPOSITE_STATE_IN_PROGRESS;
	state->stage = FETCHFILE_CONNECT;
	c->private_data = state;

	return c;
 failed:
	talloc_free(c);
	return NULL;
}

NTSTATUS smb_composite_fetchfile_recv(struct composite_context *c,
				      TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		struct fetchfile_state *state = talloc_get_type(c->private_data, struct fetchfile_state);
		talloc_steal(mem_ctx, state->io->out.data);
	}

	talloc_free(c);
	return status;
}

NTSTATUS smb_composite_fetchfile(struct smb_composite_fetchfile *io,
				 TALLOC_CTX *mem_ctx)
{
	struct composite_context *c = smb_composite_fetchfile_send(io, NULL);
	return smb_composite_fetchfile_recv(c, mem_ctx);
}
