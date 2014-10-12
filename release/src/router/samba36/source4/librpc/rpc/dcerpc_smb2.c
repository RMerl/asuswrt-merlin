/* 
   Unix SMB/CIFS implementation.

   dcerpc over SMB2 transport

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

#include "includes.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/composite/composite.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "libcli/raw/ioctl.h"
#include "librpc/rpc/dcerpc.h"
#include "librpc/rpc/dcerpc_proto.h"
#include "librpc/rpc/rpc_common.h"

/* transport private information used by SMB2 pipe transport */
struct smb2_private {
	struct smb2_handle handle;
	struct smb2_tree *tree;
	const char *server_name;
	bool dead;
};


/*
  tell the dcerpc layer that the transport is dead
*/
static void pipe_dead(struct dcecli_connection *c, NTSTATUS status)
{
	struct smb2_private *smb = (struct smb2_private *)c->transport.private_data;

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
struct smb2_read_state {
	struct dcecli_connection *c;
	DATA_BLOB data;
};

/*
  called when a read request has completed
*/
static void smb2_read_callback(struct smb2_request *req)
{
	struct smb2_private *smb;
	struct smb2_read_state *state;
	struct smb2_read io;
	uint16_t frag_length;
	NTSTATUS status;

	state = talloc_get_type(req->async.private_data, struct smb2_read_state);
	smb = talloc_get_type(state->c->transport.private_data, struct smb2_private);

	status = smb2_read_recv(req, state, &io);
	if (NT_STATUS_IS_ERR(status)) {
		pipe_dead(state->c, status);
		talloc_free(state);
		return;
	}

	if (!data_blob_append(state, &state->data, 
				  io.out.data.data, io.out.data.length)) {
		pipe_dead(state->c, NT_STATUS_NO_MEMORY);
		talloc_free(state);
		return;
	}

	if (state->data.length < 16) {
		DEBUG(0,("dcerpc_smb2: short packet (length %d) in read callback!\n",
			 (int)state->data.length));
		pipe_dead(state->c, NT_STATUS_INFO_LENGTH_MISMATCH);
		talloc_free(state);
		return;
	}

	frag_length = dcerpc_get_frag_length(&state->data);

	if (frag_length <= state->data.length) {
		DATA_BLOB data = state->data;
		struct dcecli_connection *c = state->c;
		talloc_steal(c, data.data);
		talloc_free(state);
		c->transport.recv_data(c, &data, NT_STATUS_OK);
		return;
	}

	/* initiate another read request, as we only got part of a fragment */
	ZERO_STRUCT(io);
	io.in.file.handle = smb->handle;
	io.in.length = MIN(state->c->srv_max_xmit_frag, 
			   frag_length - state->data.length);
	if (io.in.length < 16) {
		io.in.length = 16;
	}
	
	req = smb2_read_send(smb->tree, &io);
	if (req == NULL) {
		pipe_dead(state->c, NT_STATUS_NO_MEMORY);
		talloc_free(state);
		return;
	}

	req->async.fn = smb2_read_callback;
	req->async.private_data = state;
}


/*
  trigger a read request from the server, possibly with some initial
  data in the read buffer
*/
static NTSTATUS send_read_request_continue(struct dcecli_connection *c, DATA_BLOB *blob)
{
	struct smb2_private *smb = (struct smb2_private *)c->transport.private_data;
	struct smb2_read io;
	struct smb2_read_state *state;
	struct smb2_request *req;

	state = talloc(smb, struct smb2_read_state);
	if (state == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	state->c = c;
	if (blob == NULL) {
		state->data = data_blob(NULL, 0);
	} else {
		state->data = *blob;
		talloc_steal(state, state->data.data);
	}

	ZERO_STRUCT(io);
	io.in.file.handle = smb->handle;

	if (state->data.length >= 16) {
		uint16_t frag_length = dcerpc_get_frag_length(&state->data);

		if (frag_length < state->data.length) {
			talloc_free(state);
			return NT_STATUS_RPC_PROTOCOL_ERROR;
		}

		io.in.length = frag_length - state->data.length;
	} else {
		io.in.length = 0x2000;
	}

	req = smb2_read_send(smb->tree, &io);
	if (req == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	req->async.fn = smb2_read_callback;
	req->async.private_data = state;

	return NT_STATUS_OK;
}


/*
  trigger a read request from the server
*/
static NTSTATUS send_read_request(struct dcecli_connection *c)
{
	struct smb2_private *smb = (struct smb2_private *)c->transport.private_data;

	if (smb->dead) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}

	return send_read_request_continue(c, NULL);
}

/* 
   this holds the state of an in-flight trans call
*/
struct smb2_trans_state {
	struct dcecli_connection *c;
};

/*
  called when a trans request has completed
*/
static void smb2_trans_callback(struct smb2_request *req)
{
	struct smb2_trans_state *state = talloc_get_type(req->async.private_data,
							struct smb2_trans_state);
	struct dcecli_connection *c = state->c;
	NTSTATUS status;
	struct smb2_ioctl io;

	status = smb2_ioctl_recv(req, state, &io);
	if (NT_STATUS_IS_ERR(status)) {
		pipe_dead(c, status);
		return;
	}

	if (!NT_STATUS_EQUAL(status, STATUS_BUFFER_OVERFLOW)) {
		DATA_BLOB data = io.out.out;
		talloc_steal(c, data.data);
		talloc_free(state);
		c->transport.recv_data(c, &data, NT_STATUS_OK);
		return;
	}

	/* there is more to receive - setup a read */
	send_read_request_continue(c, &io.out.out);
	talloc_free(state);
}

/*
  send a SMBtrans style request, using a named pipe read_write fsctl
*/
static NTSTATUS smb2_send_trans_request(struct dcecli_connection *c, DATA_BLOB *blob)
{
        struct smb2_private *smb = talloc_get_type(c->transport.private_data,
						   struct smb2_private);
        struct smb2_ioctl io;
	struct smb2_trans_state *state;
	struct smb2_request *req;

	state = talloc(smb, struct smb2_trans_state);
	if (state == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	state->c = c;
	
	ZERO_STRUCT(io);
	io.in.file.handle	= smb->handle;
	io.in.function		= FSCTL_NAMED_PIPE_READ_WRITE;
	io.in.max_response_size	= 0x2000;
	io.in.flags		= 1;
	io.in.out		= *blob;

        req = smb2_ioctl_send(smb->tree, &io);
	if (req == NULL) {
		talloc_free(state);
		return NT_STATUS_NO_MEMORY;
	}

	req->async.fn = smb2_trans_callback;
	req->async.private_data = state;

	talloc_steal(state, req);

        return NT_STATUS_OK;
}

/*
  called when a write request has completed
*/
static void smb2_write_callback(struct smb2_request *req)
{
	struct dcecli_connection *c = (struct dcecli_connection *)req->async.private_data;

	if (!NT_STATUS_IS_OK(req->status)) {
		DEBUG(0,("dcerpc_smb2: write callback error\n"));
		pipe_dead(c, req->status);
	}

	smb2_request_destroy(req);
}

/* 
   send a packet to the server
*/
static NTSTATUS smb2_send_request(struct dcecli_connection *c, DATA_BLOB *blob, 
				  bool trigger_read)
{
	struct smb2_private *smb = (struct smb2_private *)c->transport.private_data;
	struct smb2_write io;
	struct smb2_request *req;

	if (smb->dead) {
		return NT_STATUS_CONNECTION_DISCONNECTED;
	}

	if (trigger_read) {
		return smb2_send_trans_request(c, blob);
	}

	ZERO_STRUCT(io);
	io.in.file.handle	= smb->handle;
	io.in.data		= *blob;

	req = smb2_write_send(smb->tree, &io);
	if (req == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	req->async.fn = smb2_write_callback;
	req->async.private_data = c;

	return NT_STATUS_OK;
}

static void free_request(struct smb2_request *req)
{
	talloc_free(req);
}

/* 
   shutdown SMB pipe connection
*/
static NTSTATUS smb2_shutdown_pipe(struct dcecli_connection *c, NTSTATUS status)
{
	struct smb2_private *smb = (struct smb2_private *)c->transport.private_data;
	struct smb2_close io;
	struct smb2_request *req;

	/* maybe we're still starting up */
	if (!smb) return status;

	ZERO_STRUCT(io);
	io.in.file.handle = smb->handle;
	req = smb2_close_send(smb->tree, &io);
	if (req != NULL) {
		/* we don't care if this fails, so just free it if it succeeds */
		req->async.fn = free_request;
	}

	talloc_free(smb);

	return status;
}

/*
  return SMB server name
*/
static const char *smb2_peer_name(struct dcecli_connection *c)
{
	struct smb2_private *smb = talloc_get_type(c->transport.private_data,
						   struct smb2_private);
	return smb->server_name;
}

/*
  return remote name we make the actual connection (good for kerberos) 
*/
static const char *smb2_target_hostname(struct dcecli_connection *c)
{
	struct smb2_private *smb = talloc_get_type(c->transport.private_data, 
						   struct smb2_private);
	return smb->tree->session->transport->socket->hostname;
}

/*
  fetch the user session key 
*/
static NTSTATUS smb2_session_key(struct dcecli_connection *c, DATA_BLOB *session_key)
{
	struct smb2_private *smb = talloc_get_type(c->transport.private_data,
						   struct smb2_private);
	*session_key = smb->tree->session->session_key;
	if (session_key->data == NULL) {
		return NT_STATUS_NO_USER_SESSION_KEY;
	}
	return NT_STATUS_OK;
}

struct pipe_open_smb2_state {
	struct dcecli_connection *c;
	struct composite_context *ctx;
};

static void pipe_open_recv(struct smb2_request *req);

struct composite_context *dcerpc_pipe_open_smb2_send(struct dcerpc_pipe *p, 
						     struct smb2_tree *tree,
						     const char *pipe_name)
{
	struct composite_context *ctx;
	struct pipe_open_smb2_state *state;
	struct smb2_create io;
	struct smb2_request *req;
	struct dcecli_connection *c = p->conn;

	ctx = composite_create(c, c->event_ctx);
	if (ctx == NULL) return NULL;

	state = talloc(ctx, struct pipe_open_smb2_state);
	if (composite_nomem(state, ctx)) return ctx;
	ctx->private_data = state;

	state->c = c;
	state->ctx = ctx;

	ZERO_STRUCT(io);
	io.in.desired_access = 
		SEC_STD_READ_CONTROL |
		SEC_FILE_READ_ATTRIBUTE |
		SEC_FILE_WRITE_ATTRIBUTE |
		SEC_STD_SYNCHRONIZE |
		SEC_FILE_READ_EA |
		SEC_FILE_WRITE_EA |
		SEC_FILE_READ_DATA |
		SEC_FILE_WRITE_DATA |
		SEC_FILE_APPEND_DATA;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_disposition = NTCREATEX_DISP_OPEN;
	io.in.create_options   = 
		NTCREATEX_OPTIONS_NON_DIRECTORY_FILE | 
		NTCREATEX_OPTIONS_NO_RECALL;
	io.in.impersonation_level = NTCREATEX_IMPERSONATION_IMPERSONATION;

	if ((strncasecmp(pipe_name, "/pipe/", 6) == 0) || 
	    (strncasecmp(pipe_name, "\\pipe\\", 6) == 0)) {
		pipe_name += 6;
	}
	io.in.fname = pipe_name;

	req = smb2_create_send(tree, &io);
	composite_continue_smb2(ctx, req, pipe_open_recv, state);
	return ctx;
}

static void pipe_open_recv(struct smb2_request *req)
{
	struct pipe_open_smb2_state *state =
		talloc_get_type(req->async.private_data,
				struct pipe_open_smb2_state);
	struct composite_context *ctx = state->ctx;
	struct dcecli_connection *c = state->c;
	struct smb2_tree *tree = req->tree;
	struct smb2_private *smb;
	struct smb2_create io;

	ctx->status = smb2_create_recv(req, state, &io);
	if (!composite_is_ok(ctx)) return;

	/*
	  fill in the transport methods
	*/
	c->transport.transport = NCACN_NP;
	c->transport.private_data = NULL;
	c->transport.shutdown_pipe = smb2_shutdown_pipe;
	c->transport.peer_name = smb2_peer_name;
	c->transport.target_hostname = smb2_target_hostname;

	c->transport.send_request = smb2_send_request;
	c->transport.send_read = send_read_request;
	c->transport.recv_data = NULL;
	
	/* Over-ride the default session key with the SMB session key */
	c->security_state.session_key = smb2_session_key;

	smb = talloc(c, struct smb2_private);
	if (composite_nomem(smb, ctx)) return;

	smb->handle	= io.out.file.handle;
	smb->tree	= talloc_reference(smb, tree);
	smb->server_name= strupper_talloc(smb, 
					  tree->session->transport->socket->hostname);
	if (composite_nomem(smb->server_name, ctx)) return;
	smb->dead	= false;

	c->transport.private_data = smb;

	composite_done(ctx);
}

NTSTATUS dcerpc_pipe_open_smb2_recv(struct composite_context *c)
{
	NTSTATUS status = composite_wait(c);
	talloc_free(c);
	return status;
}

NTSTATUS dcerpc_pipe_open_smb2(struct dcerpc_pipe *p,
			       struct smb2_tree *tree,
			       const char *pipe_name)
{
	struct composite_context *ctx =	dcerpc_pipe_open_smb2_send(p, tree, pipe_name);
	return dcerpc_pipe_open_smb2_recv(ctx);
}

/*
  return the SMB2 tree used for a dcerpc over SMB2 pipe
*/
struct smb2_tree *dcerpc_smb2_tree(struct dcecli_connection *c)
{
	struct smb2_private *smb = talloc_get_type(c->transport.private_data,
						   struct smb2_private);
	return smb->tree;
}
