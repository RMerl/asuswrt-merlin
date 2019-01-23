/* 
   Unix SMB/CIFS implementation.

   SMB2 client session handling

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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "libcli/composite/composite.h"
#include "auth/gensec/gensec.h"

#include <unistd.h>

/**
  initialise a smb2_session structure
 */
struct smb2_session *smb2_session_init(struct smb2_transport *transport,
				       struct gensec_settings *settings,
				       TALLOC_CTX *parent_ctx, bool primary)
{
	struct smb2_session *session;
	NTSTATUS status;

	session = talloc_zero(parent_ctx, struct smb2_session);
	if (!session) {
		return NULL;
	}
	if (primary) {
		session->transport = talloc_steal(session, transport);
	} else {
		session->transport = talloc_reference(session, transport);
	}

	session->pid = getpid();

	/* prepare a gensec context for later use */
	status = gensec_client_start(session, &session->gensec, 
				     session->transport->socket->event.ctx, 
				     settings);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(session);
		return NULL;
	}

	gensec_want_feature(session->gensec, GENSEC_FEATURE_SESSION_KEY);

	return session;
}

/**
  send a session setup request
*/
struct smb2_request *smb2_session_setup_send(struct smb2_session *session, 
					     struct smb2_session_setup *io)
{
	struct smb2_request *req;
	NTSTATUS status;
	
	req = smb2_request_init(session->transport, SMB2_OP_SESSSETUP, 
				0x18, true, io->in.secblob.length);
	if (req == NULL) return NULL;

	SBVAL(req->out.hdr,  SMB2_HDR_SESSION_ID, session->uid);
	SCVAL(req->out.body, 0x02, io->in.vc_number);
	SCVAL(req->out.body, 0x03, io->in.security_mode);
	SIVAL(req->out.body, 0x04, io->in.capabilities);
	SIVAL(req->out.body, 0x08, io->in.channel);
	SBVAL(req->out.body, 0x10, io->in.previous_sessionid);

	req->session = session;

	status = smb2_push_o16s16_blob(&req->out, 0x0C, io->in.secblob);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(req);
		return NULL;
	}

	smb2_transport_send(req);

	return req;
}


/**
  recv a session setup reply
*/
NTSTATUS smb2_session_setup_recv(struct smb2_request *req, TALLOC_CTX *mem_ctx, 
				 struct smb2_session_setup *io)
{
	NTSTATUS status;

	if (!smb2_request_receive(req) || 
	    (smb2_request_is_error(req) && 
	     !NT_STATUS_EQUAL(req->status, NT_STATUS_MORE_PROCESSING_REQUIRED))) {
		return smb2_request_destroy(req);
	}

	SMB2_CHECK_PACKET_RECV(req, 0x08, true);

	io->out.session_flags = SVAL(req->in.body, 0x02);
	io->out.uid           = BVAL(req->in.hdr,  SMB2_HDR_SESSION_ID);
	
	status = smb2_pull_o16s16_blob(&req->in, mem_ctx, req->in.body+0x04, &io->out.secblob);
	if (!NT_STATUS_IS_OK(status)) {
		smb2_request_destroy(req);
		return status;
	}

	return smb2_request_destroy(req);
}

/*
  sync session setup request
*/
NTSTATUS smb2_session_setup(struct smb2_session *session, 
			    TALLOC_CTX *mem_ctx, struct smb2_session_setup *io)
{
	struct smb2_request *req = smb2_session_setup_send(session, io);
	return smb2_session_setup_recv(req, mem_ctx, io);
}


struct smb2_session_state {
	struct smb2_session_setup io;
	struct smb2_request *req;
	NTSTATUS gensec_status;
};

/*
  handle continuations of the spnego session setup
*/
static void session_request_handler(struct smb2_request *req)
{
	struct composite_context *c = talloc_get_type(req->async.private_data, 
						      struct composite_context);
	struct smb2_session_state *state = talloc_get_type(c->private_data, 
							   struct smb2_session_state);
	struct smb2_session *session = req->session;
	NTSTATUS session_key_err;
	DATA_BLOB session_key;
	NTSTATUS peer_status;

	c->status = smb2_session_setup_recv(req, c, &state->io);
	peer_status = c->status;

	if (NT_STATUS_EQUAL(peer_status, NT_STATUS_MORE_PROCESSING_REQUIRED) ||
	    (NT_STATUS_IS_OK(peer_status) &&
	     NT_STATUS_EQUAL(state->gensec_status, NT_STATUS_MORE_PROCESSING_REQUIRED))) {
		c->status = gensec_update(session->gensec, c, 
					  state->io.out.secblob,
					  &state->io.in.secblob);
		state->gensec_status = c->status;

		session->uid = state->io.out.uid;
	}

	if (!NT_STATUS_IS_OK(c->status) &&
	    !NT_STATUS_EQUAL(c->status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		composite_error(c, c->status);
		return;
	}

	if (NT_STATUS_EQUAL(peer_status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		state->req = smb2_session_setup_send(session, &state->io);
		if (state->req == NULL) {
			composite_error(c, NT_STATUS_NO_MEMORY);
			return;
		}

		state->req->async.fn = session_request_handler;
		state->req->async.private_data = c;
		return;
	}

	session_key_err = gensec_session_key(session->gensec, &session_key);
	if (NT_STATUS_IS_OK(session_key_err)) {
		session->session_key = session_key;
	}

	if (session->transport->signing_required) {
		if (session->session_key.length == 0) {
			DEBUG(0,("Wrong session key length %u for SMB2 signing\n",
				 (unsigned)session->session_key.length));
			composite_error(c, NT_STATUS_ACCESS_DENIED);
			return;
		}
		session->signing_active = true;
	}

	composite_done(c);
}

/*
  a composite function that does a full SPNEGO session setup
 */
struct composite_context *smb2_session_setup_spnego_send(struct smb2_session *session, 
							 struct cli_credentials *credentials)
{
	struct composite_context *c;
	struct smb2_session_state *state;
	const char *chosen_oid;

	c = composite_create(session, session->transport->socket->event.ctx);
	if (c == NULL) return NULL;

	state = talloc(c, struct smb2_session_state);
	if (composite_nomem(state, c)) return c;
	c->private_data = state;

	ZERO_STRUCT(state->io);
	state->io.in.vc_number          = 0;
	if (session->transport->signing_required) {
		state->io.in.security_mode = 
			SMB2_NEGOTIATE_SIGNING_ENABLED | SMB2_NEGOTIATE_SIGNING_REQUIRED;
	}
	state->io.in.capabilities       = 0;
	state->io.in.channel            = 0;
	state->io.in.previous_sessionid = 0;

	c->status = gensec_set_credentials(session->gensec, credentials);
	if (!composite_is_ok(c)) return c;

	c->status = gensec_set_target_hostname(session->gensec, 
					       session->transport->socket->hostname);
	if (!composite_is_ok(c)) return c;

	c->status = gensec_set_target_service(session->gensec, "cifs");
	if (!composite_is_ok(c)) return c;

	if (session->transport->negotiate.secblob.length > 0) {
		chosen_oid = GENSEC_OID_SPNEGO;
	} else {
		chosen_oid = GENSEC_OID_NTLMSSP;
	}

	c->status = gensec_start_mech_by_oid(session->gensec, chosen_oid);
	if (!composite_is_ok(c)) return c;

	c->status = gensec_update(session->gensec, c, 
				  session->transport->negotiate.secblob,
				  &state->io.in.secblob);
	if (!NT_STATUS_EQUAL(c->status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		composite_error(c, c->status);
		return c;
	}
	state->gensec_status = c->status;
		
	state->req = smb2_session_setup_send(session, &state->io);
	composite_continue_smb2(c, state->req, session_request_handler, c);
	return c;
}

/*
  receive a composite session setup reply
*/
NTSTATUS smb2_session_setup_spnego_recv(struct composite_context *c)
{
	NTSTATUS status;
	status = composite_wait(c);
	talloc_free(c);
	return status;
}

/*
  sync version of smb2_session_setup_spnego
*/
NTSTATUS smb2_session_setup_spnego(struct smb2_session *session, 
				   struct cli_credentials *credentials)
{
	struct composite_context *c = smb2_session_setup_spnego_send(session, credentials);
	return smb2_session_setup_spnego_recv(c);
}
