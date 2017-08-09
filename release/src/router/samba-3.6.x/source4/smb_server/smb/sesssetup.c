
/* 
   Unix SMB/CIFS implementation.
   handle SMBsessionsetup
   Copyright (C) Andrew Tridgell                      1998-2001
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2001-2005
   Copyright (C) Jim McDonough                        2002
   Copyright (C) Luke Howard                          2003
   Copyright (C) Stefan Metzmacher                    2005
   
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
#include "version.h"
#include "auth/gensec/gensec.h"
#include "auth/auth.h"
#include "smb_server/smb_server.h"
#include "smbd/service_stream.h"
#include "param/param.h"
#include "../lib/tsocket/tsocket.h"
#include "lib/stream/packet.h"

struct sesssetup_context {
	struct auth_context *auth_context;
	struct smbsrv_request *req;
};

/*
  setup the OS, Lanman and domain portions of a session setup reply
*/
static void sesssetup_common_strings(struct smbsrv_request *req,
				     char **os, char **lanman, char **domain)
{
	(*os) = talloc_asprintf(req, "Unix");
	(*lanman) = talloc_asprintf(req, "Samba %s", SAMBA_VERSION_STRING);
	(*domain) = talloc_asprintf(req, "%s", 
				    lpcfg_workgroup(req->smb_conn->lp_ctx));
}

static void smbsrv_sesssetup_backend_send(struct smbsrv_request *req,
					  union smb_sesssetup *sess,
					  NTSTATUS status)
{
	if (NT_STATUS_IS_OK(status)) {
		req->smb_conn->negotiate.done_sesssetup = true;
		/* we need to keep the session long term */
		req->session = talloc_steal(req->smb_conn, req->session);
	}
	smbsrv_reply_sesssetup_send(req, sess, status);
}

static void sesssetup_old_send(struct tevent_req *subreq)
{
	struct sesssetup_context *state = tevent_req_callback_data(subreq, struct sesssetup_context);
	struct smbsrv_request *req = state->req;

	union smb_sesssetup *sess = talloc_get_type(req->io_ptr, union smb_sesssetup);
	struct auth_user_info_dc *user_info_dc = NULL;
	struct auth_session_info *session_info;
	struct smbsrv_session *smb_sess;
	NTSTATUS status;
	uint32_t flags;

	status = auth_check_password_recv(subreq, req, &user_info_dc);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	flags = AUTH_SESSION_INFO_DEFAULT_GROUPS;
	if (user_info_dc->info->authenticated) {
		flags |= AUTH_SESSION_INFO_AUTHENTICATED;
	}
	/* This references user_info_dc into session_info */
	status = req->smb_conn->negotiate.auth_context->generate_session_info(req,
									      req->smb_conn->negotiate.auth_context,
									      user_info_dc, flags, &session_info);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	/* allocate a new session */
	smb_sess = smbsrv_session_new(req->smb_conn, req, NULL);
	if (!smb_sess) {
		status = NT_STATUS_INSUFFICIENT_RESOURCES;
		goto failed;
	}

	/* Ensure this is marked as a 'real' vuid, not one
	 * simply valid for the session setup leg */
	status = smbsrv_session_sesssetup_finished(smb_sess, session_info);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	/* To correctly process any AndX packet (like a tree connect)
	 * we need to fill in the session on the request here */
	req->session = smb_sess;
	sess->old.out.vuid = smb_sess->vuid;

failed:
	status = nt_status_squash(status);
	smbsrv_sesssetup_backend_send(req, sess, status);
}

/*
  handler for old style session setup
*/
static void sesssetup_old(struct smbsrv_request *req, union smb_sesssetup *sess)
{
	struct auth_usersupplied_info *user_info = NULL;
	struct tsocket_address *remote_address;
	const char *remote_machine = NULL;
	struct tevent_req *subreq;
	struct sesssetup_context *state;

	sess->old.out.vuid = 0;
	sess->old.out.action = 0;

	sesssetup_common_strings(req, 
				 &sess->old.out.os,
				 &sess->old.out.lanman,
				 &sess->old.out.domain);

	if (!req->smb_conn->negotiate.done_sesssetup) {
		req->smb_conn->negotiate.max_send = sess->old.in.bufsize;
	}

	if (req->smb_conn->negotiate.calling_name) {
		remote_machine = req->smb_conn->negotiate.calling_name->name;
	}
	
	remote_address = socket_get_remote_addr(req->smb_conn->connection->socket, req);
	if (!remote_address) goto nomem;

	if (!remote_machine) {
		remote_machine = tsocket_address_inet_addr_string(remote_address, req);
		if (!remote_machine) goto nomem;
	}

	user_info = talloc_zero(req, struct auth_usersupplied_info);
	if (!user_info) goto nomem;
	
	user_info->mapped_state = false;
	user_info->logon_parameters = 0;
	user_info->flags = 0;
	user_info->client.account_name = sess->old.in.user;
	user_info->client.domain_name = sess->old.in.domain;
	user_info->workstation_name = remote_machine;
	user_info->remote_host = talloc_steal(user_info, remote_address);
	
	user_info->password_state = AUTH_PASSWORD_RESPONSE;
	user_info->password.response.lanman = sess->old.in.password;
	user_info->password.response.lanman.data = talloc_steal(user_info, sess->old.in.password.data);
	user_info->password.response.nt = data_blob(NULL, 0);

	state = talloc(req, struct sesssetup_context);
	if (!state) goto nomem;

	if (req->smb_conn->negotiate.auth_context) {
		state->auth_context = req->smb_conn->negotiate.auth_context;
	} else {
		/* TODO: should we use just "anonymous" here? */
		NTSTATUS status = auth_context_create(state,
						      req->smb_conn->connection->event.ctx,
						      req->smb_conn->connection->msg_ctx,
						      req->smb_conn->lp_ctx,
						      &state->auth_context);
		if (!NT_STATUS_IS_OK(status)) {
			smbsrv_sesssetup_backend_send(req, sess, status);
			return;
		}
	}

	state->req = req;

	subreq = auth_check_password_send(state,
					  req->smb_conn->connection->event.ctx,
					  req->smb_conn->negotiate.auth_context,
					  user_info);
	if (!subreq) goto nomem;
	tevent_req_set_callback(subreq, sesssetup_old_send, state);
	return;

nomem:
	smbsrv_sesssetup_backend_send(req, sess, NT_STATUS_NO_MEMORY);
}

static void sesssetup_nt1_send(struct tevent_req *subreq)
{
	struct sesssetup_context *state = tevent_req_callback_data(subreq, struct sesssetup_context);
	struct smbsrv_request *req = state->req;
	union smb_sesssetup *sess = talloc_get_type(req->io_ptr, union smb_sesssetup);
	struct auth_user_info_dc *user_info_dc = NULL;
	struct auth_session_info *session_info;
	struct smbsrv_session *smb_sess;

	uint32_t flags;
	NTSTATUS status;

	status = auth_check_password_recv(subreq, req, &user_info_dc);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	flags = AUTH_SESSION_INFO_DEFAULT_GROUPS;
	if (user_info_dc->info->authenticated) {
		flags |= AUTH_SESSION_INFO_AUTHENTICATED;
	}
	/* This references user_info_dc into session_info */
	status = state->auth_context->generate_session_info(req,
							    state->auth_context,
							    user_info_dc,
							    flags,
							    &session_info);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	/* allocate a new session */
	smb_sess = smbsrv_session_new(req->smb_conn, req, NULL);
	if (!smb_sess) {
		status = NT_STATUS_INSUFFICIENT_RESOURCES;
		goto failed;
	}

	/* Ensure this is marked as a 'real' vuid, not one
	 * simply valid for the session setup leg */
	status = smbsrv_session_sesssetup_finished(smb_sess, session_info);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	/* To correctly process any AndX packet (like a tree connect)
	 * we need to fill in the session on the request here */
	req->session = smb_sess;
	sess->nt1.out.vuid = smb_sess->vuid;

 	if (!smbsrv_setup_signing(req->smb_conn, &session_info->session_key, &sess->nt1.in.password2)) {
		/* Already signing, or disabled */
		goto done;
	}

done:
	status = NT_STATUS_OK;
failed:
	status = nt_status_squash(status);
	smbsrv_sesssetup_backend_send(req, sess, status);
}

/*
  handler for NT1 style session setup
*/
static void sesssetup_nt1(struct smbsrv_request *req, union smb_sesssetup *sess)
{
	NTSTATUS status;
	struct auth_usersupplied_info *user_info = NULL;
	struct tsocket_address *remote_address;
	const char *remote_machine = NULL;
	struct tevent_req *subreq;
	struct sesssetup_context *state;

	sess->nt1.out.vuid = 0;
	sess->nt1.out.action = 0;

	sesssetup_common_strings(req, 
				 &sess->nt1.out.os,
				 &sess->nt1.out.lanman,
				 &sess->nt1.out.domain);

	if (!req->smb_conn->negotiate.done_sesssetup) {
		req->smb_conn->negotiate.max_send = sess->nt1.in.bufsize;
		req->smb_conn->negotiate.client_caps = sess->nt1.in.capabilities;
	}

	state = talloc(req, struct sesssetup_context);
	if (!state) goto nomem;

	state->req = req;

	if (req->smb_conn->negotiate.oid) {
		if (sess->nt1.in.user && *sess->nt1.in.user) {
			/* We can't accept a normal login, because we
			 * don't have a challenge */
			status = NT_STATUS_LOGON_FAILURE;
			goto failed;
		}

		/* TODO: should we use just "anonymous" here? */
		status = auth_context_create(state,
					     req->smb_conn->connection->event.ctx,
					     req->smb_conn->connection->msg_ctx,
					     req->smb_conn->lp_ctx,
					     &state->auth_context);
		if (!NT_STATUS_IS_OK(status)) goto failed;
	} else if (req->smb_conn->negotiate.auth_context) {
		state->auth_context = req->smb_conn->negotiate.auth_context;
	} else {
		/* TODO: should we use just "anonymous" here? */
		status = auth_context_create(state,
					     req->smb_conn->connection->event.ctx,
					     req->smb_conn->connection->msg_ctx,
					     req->smb_conn->lp_ctx,
					     &state->auth_context);
		if (!NT_STATUS_IS_OK(status)) goto failed;
	}

	if (req->smb_conn->negotiate.calling_name) {
		remote_machine = req->smb_conn->negotiate.calling_name->name;
	}

	remote_address = socket_get_remote_addr(req->smb_conn->connection->socket, req);
	if (!remote_address) goto nomem;

	if (!remote_machine) {
		remote_machine = tsocket_address_inet_addr_string(remote_address, req);
		if (!remote_machine) goto nomem;
	}

	user_info = talloc_zero(req, struct auth_usersupplied_info);
	if (!user_info) goto nomem;

	user_info->mapped_state = false;
	user_info->logon_parameters = 0;
	user_info->flags = 0;
	user_info->client.account_name = sess->nt1.in.user;
	user_info->client.domain_name = sess->nt1.in.domain;
	user_info->workstation_name = remote_machine;
	user_info->remote_host = talloc_steal(user_info, remote_address);
	
	user_info->password_state = AUTH_PASSWORD_RESPONSE;
	user_info->password.response.lanman = sess->nt1.in.password1;
	user_info->password.response.lanman.data = talloc_steal(user_info, sess->nt1.in.password1.data);
	user_info->password.response.nt = sess->nt1.in.password2;
	user_info->password.response.nt.data = talloc_steal(user_info, sess->nt1.in.password2.data);

	subreq = auth_check_password_send(state,
					  req->smb_conn->connection->event.ctx,
					  state->auth_context,
					  user_info);
	if (!subreq) goto nomem;
	tevent_req_set_callback(subreq, sesssetup_nt1_send, state);

	return;

nomem:
	status = NT_STATUS_NO_MEMORY;
failed:
	status = nt_status_squash(status);
	smbsrv_sesssetup_backend_send(req, sess, status);
}

struct sesssetup_spnego_state {
	struct smbsrv_request *req;
	union smb_sesssetup *sess;
	struct smbsrv_session *smb_sess;
};

static void sesssetup_spnego_send(struct tevent_req *subreq)
{
	struct sesssetup_spnego_state *s = tevent_req_callback_data(subreq,
					   struct sesssetup_spnego_state);
	struct smbsrv_request *req = s->req;
	union smb_sesssetup *sess = s->sess;
	struct smbsrv_session *smb_sess = s->smb_sess;
	struct auth_session_info *session_info = NULL;
	NTSTATUS status;
	NTSTATUS skey_status;
	DATA_BLOB session_key;

	status = gensec_update_recv(subreq, req, &sess->spnego.out.secblob);
	packet_recv_enable(req->smb_conn->packet);
	TALLOC_FREE(subreq);
	if (NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		goto done;
	} else if (!NT_STATUS_IS_OK(status)) {
		goto failed;
	}

	status = gensec_session_info(smb_sess->gensec_ctx, &session_info);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	skey_status = gensec_session_key(smb_sess->gensec_ctx, &session_key);
	if (NT_STATUS_IS_OK(skey_status)) {
		smbsrv_setup_signing(req->smb_conn, &session_key, NULL);
	}

	/* Ensure this is marked as a 'real' vuid, not one
	 * simply valid for the session setup leg */
	status = smbsrv_session_sesssetup_finished(smb_sess, session_info);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	req->session = smb_sess;

done:
	sess->spnego.out.vuid = smb_sess->vuid;
failed:
	status = nt_status_squash(status);
	smbsrv_sesssetup_backend_send(req, sess, status);
	if (!NT_STATUS_IS_OK(status) && 
	    !NT_STATUS_EQUAL(status, NT_STATUS_MORE_PROCESSING_REQUIRED)) {
		talloc_free(smb_sess);
	}
}

/*
  handler for SPNEGO style session setup
*/
static void sesssetup_spnego(struct smbsrv_request *req, union smb_sesssetup *sess)
{
	NTSTATUS status;
	struct smbsrv_session *smb_sess = NULL;
	struct sesssetup_spnego_state *s = NULL;
	uint16_t vuid;
	struct tevent_req *subreq;

	sess->spnego.out.vuid = 0;
	sess->spnego.out.action = 0;

	sesssetup_common_strings(req, 
				 &sess->spnego.out.os,
				 &sess->spnego.out.lanman,
				 &sess->spnego.out.workgroup);

	if (!req->smb_conn->negotiate.done_sesssetup) {
		req->smb_conn->negotiate.max_send = sess->spnego.in.bufsize;
		req->smb_conn->negotiate.client_caps = sess->spnego.in.capabilities;
	}

	vuid = SVAL(req->in.hdr,HDR_UID);

	/* lookup an existing session */
	smb_sess = smbsrv_session_find_sesssetup(req->smb_conn, vuid);
	if (!smb_sess) {
		struct gensec_security *gensec_ctx;

		status = samba_server_gensec_start(req,
						   req->smb_conn->connection->event.ctx,
						   req->smb_conn->connection->msg_ctx,
						   req->smb_conn->lp_ctx,
						   req->smb_conn->negotiate.server_credentials,
						   "cifs",
						   &gensec_ctx);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to start GENSEC server code: %s\n", nt_errstr(status)));
			goto failed;
		}

		gensec_want_feature(gensec_ctx, GENSEC_FEATURE_SESSION_KEY);

		status = gensec_start_mech_by_oid(gensec_ctx, req->smb_conn->negotiate.oid);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(1, ("Failed to start GENSEC %s server code: %s\n", 
				  gensec_get_name_by_oid(gensec_ctx, req->smb_conn->negotiate.oid), nt_errstr(status)));
			goto failed;
		}

		/* allocate a new session */
		smb_sess = smbsrv_session_new(req->smb_conn, req->smb_conn, gensec_ctx);
		if (!smb_sess) {
			status = NT_STATUS_INSUFFICIENT_RESOURCES;
			goto failed;
		}
	}

	if (!smb_sess) {
		status = NT_STATUS_ACCESS_DENIED;
		goto failed;
	}

	if (!smb_sess->gensec_ctx) {
		status = NT_STATUS_INTERNAL_ERROR;
		DEBUG(1, ("Internal ERROR: no gensec_ctx on session: %s\n", nt_errstr(status)));
		goto failed;
	}

	s = talloc(req, struct sesssetup_spnego_state);
	if (!s) goto nomem;
	s->req		= req;
	s->sess		= sess;
	s->smb_sess	= smb_sess;

	subreq = gensec_update_send(s,
				    req->smb_conn->connection->event.ctx,
				    smb_sess->gensec_ctx,
				    sess->spnego.in.secblob);
	if (!subreq) {
		goto nomem;
	}
	/* disable receipt of more packets on this socket until we've
	   finished with the session setup. This avoids a problem with
	   crashes if we get EOF on the socket while processing a session
	   setup */
	packet_recv_disable(req->smb_conn->packet);
	tevent_req_set_callback(subreq, sesssetup_spnego_send, s);

	return;

nomem:
	status = NT_STATUS_NO_MEMORY;
failed:
	talloc_free(smb_sess);
	status = nt_status_squash(status);
	smbsrv_sesssetup_backend_send(req, sess, status);
}

/*
  backend for sessionsetup call - this takes all 3 variants of the call
*/
void smbsrv_sesssetup_backend(struct smbsrv_request *req,
			      union smb_sesssetup *sess)
{
	switch (sess->old.level) {
		case RAW_SESSSETUP_OLD:
			sesssetup_old(req, sess);
			return;

		case RAW_SESSSETUP_NT1:
			sesssetup_nt1(req, sess);
			return;

		case RAW_SESSSETUP_SPNEGO:
			sesssetup_spnego(req, sess);
			return;

		case RAW_SESSSETUP_SMB2:
			break;
	}

	smbsrv_sesssetup_backend_send(req, sess, NT_STATUS_INVALID_LEVEL);
}
