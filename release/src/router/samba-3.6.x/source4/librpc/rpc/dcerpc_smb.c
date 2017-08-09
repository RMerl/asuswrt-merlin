/* 
   Unix SMB/CIFS implementation.

   dcerpc over SMB transport

   Copyright (C) Tim Potter 2003
   Copyright (C) Andrew Tridgell 2003
   
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
#include "libcli/raw/libcliraw.h"
#include "libcli/composite/composite.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "librpc/rpc/rpc_common.h"

/* transport private information used by SMB pipe transport */
struct smb_private {
	uint16_t fnum;
	struct smbcli_tree *tree;
	const char *server_name;
	bool dead;
};


/*
  tell the dcerpc layer that the transport is dead
*/
static void pipe_dead(struct dcecli_connection *c, NTSTATUS status)
{
	struct smb_private *smb = (struct smb_private *)c->transport.private_data;

	if (smb->dead) {
		return;
	}

	smb->dead = true;

	if (NT_STATUS_EQUAL(NT_STATUS_UNSUCCESSFUL, status)) {
		status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
	}

	if (NT_STATUS_EQUAL(NT_STATUS_OK, status)) {
		status = NT_STATUS_END_OF_FILE;
	}

	if (c->transport.recv_data) {
		c->transport.recv_data(c, NULL, status);
	}
}


/* 
   this holds the state of an in-flight call
*/
struct smb_read_state {
	struct dcecli_connection *c;
	struct smbcli_request *req;
	size_t received;
	DATA_BLOB data;
	union smb_read *io;
};

/*
  called when a read request has completed
*/
static void smb_read_callback(struct smbcli_request *req)
{
	struct smb_private *smb;
	struct smb_read_state *state;
	union smb_read *io;
	uint16_t frag_length;
	NTSTATUS status;

	state = talloc_get_type(req->async.private_data, struct smb_read_state);
	smb = talloc_get_type(state->c->transport.private_data, struct smb_private);
	io = state->io;

	status = smb_raw_read_recv(state->req, io);
	if (NT_STATUS_IS_ERR(status)) {
		pipe_dead(state->c, status);
		talloc_free(state);
		return;
	}

	state->received += io->readx.out.nread;

	if (state->received < 16) {
		DEBUG(0,("dcerpc_smb: short packet (length %d) in read callback!\n",
			 (int)state->received));
		pipe_dead(state->c, NT_STATUS_INFO_LENGTH_MISMATCH);
		talloc_free(state);
		return;
	}

	frag_length = dcerpc_get_frag_length(&state->data);

	if (frag_length <= state->received) {
		DATA_BLOB data = state->data;
		struct dcecli_connection *c = state->c;
		data.length = state->received;
		talloc_steal(state->c, data.data);
		talloc_free(state);
		c->transport.recv_data(c, &data, NT_STATUS_OK);
		return;
	}

	/* initiate another read request, as we only got part of a fragment */
	state->data.data = talloc_realloc(state, state->data.data, uint8_t, frag_length);

	io->readx.in.mincnt = MIN(state->c->srv_max_xmit_frag, 
				  frag_length - state->received);
	io->readx.in.maxcnt = io->readx.in.mincnt;
	io->readx.out.data = state->data.data + state->received;

	state->req = smb_raw_read_send(smb->tree, io);
	if (state->req == NULL) {
		pipe_dead(state->c, NT_STATUS_NO_MEMORY);
		talloc_free(state);
		return;
	}

	state->req->async.fn = smb_read_callback;
	state->req->async.private_data = state;
}

/*
  trigger a read request from the server, possibly with some initial
  data in the read buffer
*/
static NTSTATUS send_read_request_continue(struct dcecli_connection *c, DATA_BLOB *blob)
{
	struct smb_private *smb = (struct smb_private *)c->transport.private_data;
	union smb_read *io;
	struct smb_read_state *state;
	struct smbcli_request *req;

	state = talloc(smb, struct smb_read_state);
	if (state == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	state->c = c;
	if (blob == NULL) {
		state->received = 0;
		state->data = data_blob_talloc(state, NULL, 0x2000);
	} else {
		uint32_t frag_length = blob->length>=16?
			dcerpc_get_frag_length(blob):0x2000;

		if (frag_length < state->data.length) {
			talloc_free(state);
			return NT_STATUS_RPC_PROTOCOL_ERROR;
		}

		state->received = blob->length;
		state->data = data_blob_talloc(state, NULL, frag_length);
		if (!state->data.data) {
			talloc_free(state);
			return NT_STATUS_NO_MEMORY;
		}
		memcpy(state->data.data, blob->data, blob->length);
	}

	state->io = talloc(state, union smb_read);

	io = state->io;
	io->generic.level = RAW_READ_READX;
	io->readx.in.file.fnum = smb->fnum;
	io->readx.in.mincnt = state->data.length - state->received;
	io->readx.in.maxcnt = io->readx.in.mincnt;
	io->readx.in.offset = 0;
	io->readx.in.remaining = 0;
	io->readx.in.read_for_execute = false;
	io->readx.out.data = state->data.data + state->received;
	req = smb_raw_read_send(smb->tree, io);
	if (req == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	req->async.fn = smb_read_callback;
	req->async.private_data = state;

	state->req = req;

	return NT_STATUS_OK;
}


/*
  trigger a read request from the server
*/
static NTSTATUS send_read_request(struct dcecli_connection *c)
{
	struct smb_private *smb = (struct smb_private *)c->transport.private_data;

	if (smb->dead) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}

	return send_read_request_continue(c, NULL);
}

/* 
   this holds the state of an in-flight trans call
*/
struct smb_trans_state {
	struct dcecli_connection *c;
	struct smbcli_request *req;
	struct smb_trans2 *trans;
};

/*
  called when a trans request has completed
*/
static void smb_trans_callback(struct smbcli_request *req)
{
	struct smb_trans_state *state = (struct smb_trans_state *)req->async.private_data;
	struct dcecli_connection *c = state->c;
	NTSTATUS status;

	status = smb_raw_trans_recv(req, state, state->trans);

	if (NT_STATUS_IS_ERR(status)) {
		pipe_dead(c, status);
		return;
	}

	if (!NT_STATUS_EQUAL(status, STATUS_BUFFER_OVERFLOW)) {
		DATA_BLOB data = state->trans->out.data;
		talloc_steal(c, data.data);
		talloc_free(state);
		c->transport.recv_data(c, &data, NT_STATUS_OK);
		return;
	}

	/* there is more to receive - setup a readx */
	send_read_request_continue(c, &state->trans->out.data);
	talloc_free(state);
}

/*
  send a SMBtrans style request
*/
static NTSTATUS smb_send_trans_request(struct dcecli_connection *c, DATA_BLOB *blob)
{
        struct smb_private *smb = (struct smb_private *)c->transport.private_data;
        struct smb_trans2 *trans;
        uint16_t setup[2];
	struct smb_trans_state *state;
	uint16_t max_data;

	state = talloc(smb, struct smb_trans_state);
	if (state == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	state->c = c;
	state->trans = talloc(state, struct smb_trans2);
	trans = state->trans;

        trans->in.data = *blob;
        trans->in.params = data_blob(NULL, 0);
        
        setup[0] = TRANSACT_DCERPCCMD;
        setup[1] = smb->fnum;

	if (c->srv_max_xmit_frag > 0) {
	        max_data = MIN(UINT16_MAX, c->srv_max_xmit_frag);
	} else {
		max_data = UINT16_MAX;
	}

        trans->in.max_param = 0;
        trans->in.max_data = max_data;
        trans->in.max_setup = 0;
        trans->in.setup_count = 2;
        trans->in.flags = 0;
        trans->in.timeout = 0;
        trans->in.setup = setup;
        trans->in.trans_name = "\\PIPE\\";

        state->req = smb_raw_trans_send(smb->tree, trans);
	if (state->req == NULL) {
		talloc_free(state);
		return NT_STATUS_NO_MEMORY;
	}

	state->req->async.fn = smb_trans_callback;
	state->req->async.private_data = state;

	talloc_steal(state, state->req);

        return NT_STATUS_OK;
}

/*
  called when a write request has completed
*/
static void smb_write_callback(struct smbcli_request *req)
{
	struct dcecli_connection *c = (struct dcecli_connection *)req->async.private_data;

	if (!NT_STATUS_IS_OK(req->status)) {
		DEBUG(0,("dcerpc_smb: write callback error\n"));
		pipe_dead(c, req->status);
	}

	smbcli_request_destroy(req);
}

/* 
   send a packet to the server
*/
static NTSTATUS smb_send_request(struct dcecli_connection *c, DATA_BLOB *blob, 
				 bool trigger_read)
{
	struct smb_private *smb = (struct smb_private *)c->transport.private_data;
	union smb_write io;
	struct smbcli_request *req;

	if (!smb || smb->dead) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}

	if (trigger_read) {
		return smb_send_trans_request(c, blob);
	}

	io.generic.level = RAW_WRITE_WRITEX;
	io.writex.in.file.fnum = smb->fnum;
	io.writex.in.offset = 0;
	io.writex.in.wmode = PIPE_START_MESSAGE;
	io.writex.in.remaining = blob->length;
	io.writex.in.count = blob->length;
	io.writex.in.data = blob->data;

	/* we must not timeout at the smb level for rpc requests, as otherwise
	   signing/sealing can be messed up */
	smb->tree->session->transport->options.request_timeout = 0;

	req = smb_raw_write_send(smb->tree, &io);
	if (req == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	req->async.fn = smb_write_callback;
	req->async.private_data = c;

	if (trigger_read) {
		send_read_request(c);
	}

	return NT_STATUS_OK;
}


static void free_request(struct smbcli_request *req)
{
	talloc_free(req);
}

/* 
   shutdown SMB pipe connection
*/
static NTSTATUS smb_shutdown_pipe(struct dcecli_connection *c, NTSTATUS status)
{
	struct smb_private *smb = (struct smb_private *)c->transport.private_data;
	union smb_close io;
	struct smbcli_request *req;

	/* maybe we're still starting up */
	if (!smb) return status;

	io.close.level = RAW_CLOSE_CLOSE;
	io.close.in.file.fnum = smb->fnum;
	io.close.in.write_time = 0;
	req = smb_raw_close_send(smb->tree, &io);
	if (req != NULL) {
		/* we don't care if this fails, so just free it if it succeeds */
		req->async.fn = free_request;
	}

	talloc_free(smb);
	c->transport.private_data = NULL;

	return status;
}

/*
  return SMB server name (called name)
*/
static const char *smb_peer_name(struct dcecli_connection *c)
{
	struct smb_private *smb = (struct smb_private *)c->transport.private_data;
	if (smb == NULL) return "";
	return smb->server_name;
}

/*
  return remote name we make the actual connection (good for kerberos) 
*/
static const char *smb_target_hostname(struct dcecli_connection *c)
{
	struct smb_private *smb = talloc_get_type(c->transport.private_data, struct smb_private);
	if (smb == NULL) return "";
	return smb->tree->session->transport->socket->hostname;
}

/*
  fetch the user session key 
*/
static NTSTATUS smb_session_key(struct dcecli_connection *c, DATA_BLOB *session_key)
{
	struct smb_private *smb = (struct smb_private *)c->transport.private_data;

	if (smb == NULL) return NT_STATUS_CONNECTION_DISCONNECTED;
	if (smb->tree->session->user_session_key.data) {
		*session_key = smb->tree->session->user_session_key;
		return NT_STATUS_OK;
	}
	return NT_STATUS_NO_USER_SESSION_KEY;
}

struct pipe_open_smb_state {
	union smb_open *open;
	struct dcecli_connection *c;
	struct smbcli_tree *tree;
	struct composite_context *ctx;
};

static void pipe_open_recv(struct smbcli_request *req);

struct composite_context *dcerpc_pipe_open_smb_send(struct dcerpc_pipe *p, 
						    struct smbcli_tree *tree,
						    const char *pipe_name)
{
	struct composite_context *ctx;
	struct pipe_open_smb_state *state;
	struct smbcli_request *req;
	struct dcecli_connection *c = p->conn;

	/* if we don't have a binding on this pipe yet, then create one */
	if (p->binding == NULL) {
		NTSTATUS status;
		char *s;
		SMB_ASSERT(tree->session->transport->socket->hostname != NULL);
		s = talloc_asprintf(p, "ncacn_np:%s", tree->session->transport->socket->hostname);
		if (s == NULL) return NULL;
		status = dcerpc_parse_binding(p, s, &p->binding);
		talloc_free(s);
		if (!NT_STATUS_IS_OK(status)) {
			return NULL;
		}
	}

	ctx = composite_create(c, c->event_ctx);
	if (ctx == NULL) return NULL;

	state = talloc(ctx, struct pipe_open_smb_state);
	if (composite_nomem(state, ctx)) return ctx;
	ctx->private_data = state;

	state->c = c;
	state->tree = tree;
	state->ctx = ctx;

	state->open = talloc(state, union smb_open);
	if (composite_nomem(state->open, ctx)) return ctx;

	state->open->ntcreatex.level = RAW_OPEN_NTCREATEX;
	state->open->ntcreatex.in.flags = 0;
	state->open->ntcreatex.in.root_fid.fnum = 0;
	state->open->ntcreatex.in.access_mask = 
		SEC_STD_READ_CONTROL |
		SEC_FILE_WRITE_ATTRIBUTE |
		SEC_FILE_WRITE_EA |
		SEC_FILE_READ_DATA |
		SEC_FILE_WRITE_DATA;
	state->open->ntcreatex.in.file_attr = 0;
	state->open->ntcreatex.in.alloc_size = 0;
	state->open->ntcreatex.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	state->open->ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	state->open->ntcreatex.in.create_options = 0;
	state->open->ntcreatex.in.impersonation =
		NTCREATEX_IMPERSONATION_IMPERSONATION;
	state->open->ntcreatex.in.security_flags = 0;

	if ((strncasecmp(pipe_name, "/pipe/", 6) == 0) || 
	    (strncasecmp(pipe_name, "\\pipe\\", 6) == 0)) {
		pipe_name += 6;
	}
	state->open->ntcreatex.in.fname =
		(pipe_name[0] == '\\') ?
		talloc_strdup(state->open, pipe_name) :
		talloc_asprintf(state->open, "\\%s", pipe_name);
	if (composite_nomem(state->open->ntcreatex.in.fname, ctx)) return ctx;

	req = smb_raw_open_send(tree, state->open);
	composite_continue_smb(ctx, req, pipe_open_recv, state);
	return ctx;
}

static void pipe_open_recv(struct smbcli_request *req)
{
	struct pipe_open_smb_state *state = talloc_get_type(req->async.private_data,
					    struct pipe_open_smb_state);
	struct composite_context *ctx = state->ctx;
	struct dcecli_connection *c = state->c;
	struct smb_private *smb;
	
	ctx->status = smb_raw_open_recv(req, state, state->open);
	if (!composite_is_ok(ctx)) return;

	/*
	  fill in the transport methods
	*/
	c->transport.transport       = NCACN_NP;
	c->transport.private_data    = NULL;
	c->transport.shutdown_pipe   = smb_shutdown_pipe;
	c->transport.peer_name       = smb_peer_name;
	c->transport.target_hostname = smb_target_hostname;

	c->transport.send_request    = smb_send_request;
	c->transport.send_read       = send_read_request;
	c->transport.recv_data       = NULL;
	
	/* Over-ride the default session key with the SMB session key */
	c->security_state.session_key = smb_session_key;

	smb = talloc(c, struct smb_private);
	if (composite_nomem(smb, ctx)) return;

	smb->fnum	= state->open->ntcreatex.out.file.fnum;
	smb->tree	= talloc_reference(smb, state->tree);
	smb->server_name= strupper_talloc(smb,
			  state->tree->session->transport->called.name);
	if (composite_nomem(smb->server_name, ctx)) return;
	smb->dead	= false;

	c->transport.private_data = smb;

	composite_done(ctx);
}

NTSTATUS dcerpc_pipe_open_smb_recv(struct composite_context *c)
{
	NTSTATUS status = composite_wait(c);
	talloc_free(c);
	return status;
}

_PUBLIC_ NTSTATUS dcerpc_pipe_open_smb(struct dcerpc_pipe *p,
			      struct smbcli_tree *tree,
			      const char *pipe_name)
{
	struct composite_context *ctx =	dcerpc_pipe_open_smb_send(p, tree,
								  pipe_name);
	return dcerpc_pipe_open_smb_recv(ctx);
}

/*
  return the SMB tree used for a dcerpc over SMB pipe
*/
_PUBLIC_ struct smbcli_tree *dcerpc_smb_tree(struct dcecli_connection *c)
{
	struct smb_private *smb;

	if (c->transport.transport != NCACN_NP) return NULL;

	smb = talloc_get_type(c->transport.private_data, struct smb_private);
	if (!smb) return NULL;

	return smb->tree;
}

/*
  return the SMB fnum used for a dcerpc over SMB pipe (hack for torture operations)
*/
_PUBLIC_ uint16_t dcerpc_smb_fnum(struct dcecli_connection *c)
{
	struct smb_private *smb;

	if (c->transport.transport != NCACN_NP) return 0;

	smb = talloc_get_type(c->transport.private_data, struct smb_private);
	if (!smb) return 0;

	return smb->fnum;
}
