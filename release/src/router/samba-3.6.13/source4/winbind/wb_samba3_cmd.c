/* 
   Unix SMB/CIFS implementation.
   Main winbindd samba3 server routines

   Copyright (C) Stefan Metzmacher	2005
   Copyright (C) Volker Lendecke	2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Kai Blin		2009

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
#include "winbind/wb_server.h"
#include "param/param.h"
#include "winbind/wb_helper.h"
#include "libcli/composite/composite.h"
#include "version.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "libcli/security/security.h"
#include "../libcli/auth/pam_errors.h"
#include "auth/credentials/credentials.h"
#include "smbd/service_task.h"

/*
  support the old Samba3 TXT form of the info3
 */
static NTSTATUS wb_samba3_append_info3_as_txt(TALLOC_CTX *mem_ctx,
					      struct wbsrv_samba3_call *s3call,
					      DATA_BLOB info3b)
{
	struct netr_SamInfo3 *info3;
	char *ex;
	uint32_t i;
	enum ndr_err_code ndr_err;

	info3 = talloc(mem_ctx, struct netr_SamInfo3);
	NT_STATUS_HAVE_NO_MEMORY(info3);

	/* The Samba3 protocol has a redundent 4 bytes at the start */
	info3b.data += 4;
	info3b.length -= 4;

	ndr_err = ndr_pull_struct_blob(&info3b,
				       mem_ctx,
				       info3,
				       (ndr_pull_flags_fn_t)ndr_pull_netr_SamInfo3);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return ndr_map_error2ntstatus(ndr_err);
	}

	s3call->response->data.auth.info3.logon_time =
		nt_time_to_unix(info3->base.last_logon);
	s3call->response->data.auth.info3.logoff_time =
		nt_time_to_unix(info3->base.last_logoff);
	s3call->response->data.auth.info3.kickoff_time =
		nt_time_to_unix(info3->base.acct_expiry);
	s3call->response->data.auth.info3.pass_last_set_time =
		nt_time_to_unix(info3->base.last_password_change);
	s3call->response->data.auth.info3.pass_can_change_time =
		nt_time_to_unix(info3->base.allow_password_change);
	s3call->response->data.auth.info3.pass_must_change_time =
		nt_time_to_unix(info3->base.force_password_change);

	s3call->response->data.auth.info3.logon_count = info3->base.logon_count;
	s3call->response->data.auth.info3.bad_pw_count = info3->base.bad_password_count;

	s3call->response->data.auth.info3.user_rid = info3->base.rid;
	s3call->response->data.auth.info3.group_rid = info3->base.primary_gid;
	fstrcpy(s3call->response->data.auth.info3.dom_sid, dom_sid_string(mem_ctx, info3->base.domain_sid));

	s3call->response->data.auth.info3.num_groups = info3->base.groups.count;
	s3call->response->data.auth.info3.user_flgs = info3->base.user_flags;

	s3call->response->data.auth.info3.acct_flags = info3->base.acct_flags;
	s3call->response->data.auth.info3.num_other_sids = info3->sidcount;

	fstrcpy(s3call->response->data.auth.info3.user_name,
		info3->base.account_name.string);
	fstrcpy(s3call->response->data.auth.info3.full_name,
		info3->base.full_name.string);
	fstrcpy(s3call->response->data.auth.info3.logon_script,
		info3->base.logon_script.string);
	fstrcpy(s3call->response->data.auth.info3.profile_path,
		info3->base.profile_path.string);
	fstrcpy(s3call->response->data.auth.info3.home_dir,
		info3->base.home_directory.string);
	fstrcpy(s3call->response->data.auth.info3.dir_drive,
		info3->base.home_drive.string);

	fstrcpy(s3call->response->data.auth.info3.logon_srv,
		info3->base.logon_server.string);
	fstrcpy(s3call->response->data.auth.info3.logon_dom,
		info3->base.domain.string);

	ex = talloc_strdup(mem_ctx, "");
	NT_STATUS_HAVE_NO_MEMORY(ex);

	for (i=0; i < info3->base.groups.count; i++) {
		ex = talloc_asprintf_append_buffer(ex, "0x%08X:0x%08X\n",
						   info3->base.groups.rids[i].rid,
						   info3->base.groups.rids[i].attributes);
		NT_STATUS_HAVE_NO_MEMORY(ex);
	}

	for (i=0; i < info3->sidcount; i++) {
		char *sid;

		sid = dom_sid_string(mem_ctx, info3->sids[i].sid);
		NT_STATUS_HAVE_NO_MEMORY(sid);

		ex = talloc_asprintf_append_buffer(ex, "%s:0x%08X\n",
						   sid,
						   info3->sids[i].attributes);
		NT_STATUS_HAVE_NO_MEMORY(ex);

		talloc_free(sid);
	}

	s3call->response->extra_data.data = ex;
	s3call->response->length += talloc_get_size(ex);

	return NT_STATUS_OK;
}

/* 
   Send off the reply to an async Samba3 query, handling filling in the PAM, NTSTATUS and string errors.
*/

static void wbsrv_samba3_async_auth_epilogue(NTSTATUS status,
					     struct wbsrv_samba3_call *s3call)
{
	struct winbindd_response *resp = s3call->response;
	if (!NT_STATUS_IS_OK(status)) {
		resp->result = WINBINDD_ERROR;
	} else {
		resp->result = WINBINDD_OK;
	}
	
	WBSRV_SAMBA3_SET_STRING(resp->data.auth.nt_status_string,
				nt_errstr(status));
	WBSRV_SAMBA3_SET_STRING(resp->data.auth.error_string,
				get_friendly_nt_error_msg(status));

	resp->data.auth.pam_error = nt_status_to_pam(status);
	resp->data.auth.nt_status = NT_STATUS_V(status);

	wbsrv_samba3_send_reply(s3call);
}

/* 
   Send of a generic reply to a Samba3 query
*/

static void wbsrv_samba3_async_epilogue(NTSTATUS status,
					struct wbsrv_samba3_call *s3call)
{
	struct winbindd_response *resp = s3call->response;
	if (NT_STATUS_IS_OK(status)) {
		resp->result = WINBINDD_OK;
	} else {
		resp->result = WINBINDD_ERROR;
	}

	wbsrv_samba3_send_reply(s3call);
}

/* 
   Boilerplate commands, simple queries without network traffic 
*/

NTSTATUS wbsrv_samba3_interface_version(struct wbsrv_samba3_call *s3call)
{
	s3call->response->result			= WINBINDD_OK;
	s3call->response->data.interface_version	= WINBIND_INTERFACE_VERSION;
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_info(struct wbsrv_samba3_call *s3call)
{
	s3call->response->result			= WINBINDD_OK;
	s3call->response->data.info.winbind_separator = *lpcfg_winbind_separator(s3call->wbconn->lp_ctx);
	WBSRV_SAMBA3_SET_STRING(s3call->response->data.info.samba_version,
				SAMBA_VERSION_STRING);
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_domain_name(struct wbsrv_samba3_call *s3call)
{
	s3call->response->result			= WINBINDD_OK;
	WBSRV_SAMBA3_SET_STRING(s3call->response->data.domain_name,
				lpcfg_workgroup(s3call->wbconn->lp_ctx));
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_netbios_name(struct wbsrv_samba3_call *s3call)
{
	s3call->response->result			= WINBINDD_OK;
	WBSRV_SAMBA3_SET_STRING(s3call->response->data.netbios_name,
				lpcfg_netbios_name(s3call->wbconn->lp_ctx));
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_priv_pipe_dir(struct wbsrv_samba3_call *s3call)
{
	struct loadparm_context *lp_ctx = s3call->wbconn->listen_socket->service->task->lp_ctx;
	const char *priv_socket_dir = lpcfg_winbindd_privileged_socket_directory(lp_ctx);

	s3call->response->result		 = WINBINDD_OK;
	s3call->response->extra_data.data = discard_const(priv_socket_dir);

	s3call->response->length += strlen(priv_socket_dir) + 1;
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_ping(struct wbsrv_samba3_call *s3call)
{
	s3call->response->result			= WINBINDD_OK;
	return NT_STATUS_OK;
}

NTSTATUS wbsrv_samba3_domain_info(struct wbsrv_samba3_call *s3call)
{
	DEBUG(5, ("wbsrv_samba3_domain_info called, stub\n"));
	s3call->response->result = WINBINDD_OK;
	fstrcpy(s3call->response->data.domain_info.name,
		s3call->request->domain_name);
	fstrcpy(s3call->response->data.domain_info.alt_name,
		s3call->request->domain_name);
	fstrcpy(s3call->response->data.domain_info.sid, "S-1-2-3-4");
	s3call->response->data.domain_info.native_mode = false;
	s3call->response->data.domain_info.active_directory = false;
	s3call->response->data.domain_info.primary = false;

	return NT_STATUS_OK;
}

/* Plaintext authentication 
   
   This interface is used by ntlm_auth in it's 'basic' authentication
   mode, as well as by pam_winbind to authenticate users where we are
   given a plaintext password.
*/

static void check_machacc_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_check_machacc(struct wbsrv_samba3_call *s3call)
{
	NTSTATUS status;
	struct cli_credentials *creds;
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;

	/* Create a credentials structure */
	creds = cli_credentials_init(s3call);
	if (creds == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	cli_credentials_set_conf(creds, service->task->lp_ctx);

	/* Connect the machine account to the credentials */
	status = cli_credentials_set_machine_account(creds, service->task->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(creds);
		return status;
	}

	ctx = wb_cmd_pam_auth_send(s3call, service, creds);

	if (!ctx) {
		talloc_free(creds);
		return NT_STATUS_NO_MEMORY;
	}

	ctx->async.fn = check_machacc_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void check_machacc_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;

	status = wb_cmd_pam_auth_recv(ctx, s3call, NULL, NULL, NULL, NULL);

	if (!NT_STATUS_IS_OK(status)) goto done;

 done:
	wbsrv_samba3_async_auth_epilogue(status, s3call);
}

/*
  Find the name of a suitable domain controller, by query on the
  netlogon pipe to the DC.  
*/

static void getdcname_recv_dc(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_getdcname(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_getdcname called\n"));

	ctx = wb_cmd_getdcname_send(s3call, service,
				    s3call->request->domain_name);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = getdcname_recv_dc;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void getdcname_recv_dc(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	const char *dcname;
	NTSTATUS status;

	status = wb_cmd_getdcname_recv(ctx, s3call, &dcname);
	if (!NT_STATUS_IS_OK(status)) goto done;

	s3call->response->result = WINBINDD_OK;
	WBSRV_SAMBA3_SET_STRING(s3call->response->data.dc_name, dcname);

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

/* 
   Lookup a user's domain groups
*/

static void userdomgroups_recv_groups(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_userdomgroups(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct dom_sid *sid;

	DEBUG(5, ("wbsrv_samba3_userdomgroups called\n"));

	sid = dom_sid_parse_talloc(s3call, s3call->request->data.sid);
	if (sid == NULL) {
		DEBUG(5, ("Could not parse sid %s\n",
			  s3call->request->data.sid));
		return NT_STATUS_NO_MEMORY;
	}

	ctx = wb_cmd_userdomgroups_send(
		s3call, s3call->wbconn->listen_socket->service, sid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = userdomgroups_recv_groups;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void userdomgroups_recv_groups(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	uint32_t i, num_sids;
	struct dom_sid **sids;
	char *sids_string;
	NTSTATUS status;

	status = wb_cmd_userdomgroups_recv(ctx, s3call, &num_sids, &sids);
	if (!NT_STATUS_IS_OK(status)) goto done;

	sids_string = talloc_strdup(s3call, "");
	if (sids_string == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<num_sids; i++) {
		sids_string = talloc_asprintf_append_buffer(
			sids_string, "%s\n", dom_sid_string(s3call, sids[i]));
	}

	if (sids_string == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	s3call->response->result = WINBINDD_OK;
	s3call->response->extra_data.data = sids_string;
	s3call->response->length += strlen(sids_string)+1;
	s3call->response->data.num_entries = num_sids;

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

/* 
   Lookup the list of SIDs for a user 
*/
static void usersids_recv_sids(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_usersids(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct dom_sid *sid;

	DEBUG(5, ("wbsrv_samba3_usersids called\n"));

	sid = dom_sid_parse_talloc(s3call, s3call->request->data.sid);
	if (sid == NULL) {
		DEBUG(5, ("Could not parse sid %s\n",
			  s3call->request->data.sid));
		return NT_STATUS_NO_MEMORY;
	}

	ctx = wb_cmd_usersids_send(
		s3call, s3call->wbconn->listen_socket->service, sid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = usersids_recv_sids;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void usersids_recv_sids(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	uint32_t i, num_sids;
	struct dom_sid **sids;
	char *sids_string;
	NTSTATUS status;

	status = wb_cmd_usersids_recv(ctx, s3call, &num_sids, &sids);
	if (!NT_STATUS_IS_OK(status)) goto done;

	sids_string = talloc_strdup(s3call, "");
	if (sids_string == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<num_sids; i++) {
		sids_string = talloc_asprintf_append_buffer(
			sids_string, "%s\n", dom_sid_string(s3call, sids[i]));
		if (sids_string == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto done;
		}
	}

	s3call->response->result = WINBINDD_OK;
	s3call->response->extra_data.data = sids_string;
	s3call->response->length += strlen(sids_string);
	s3call->response->data.num_entries = num_sids;

	/* Hmmmm. Nasty protocol -- who invented the zeros between the
	 * SIDs? Hmmm. Could have been me -- vl */

	while (*sids_string != '\0') {
		if ((*sids_string) == '\n') {
			*sids_string = '\0';
		}
		sids_string += 1;
	}

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

/* 
   Lookup a DOMAIN\\user style name, and return a SID
*/

static void lookupname_recv_sid(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_lookupname(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_lookupname called\n"));

	ctx = wb_cmd_lookupname_send(s3call, service,
				     s3call->request->data.name.dom_name,
				     s3call->request->data.name.name);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	/* setup the callbacks */
	ctx->async.fn = lookupname_recv_sid;
	ctx->async.private_data	= s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void lookupname_recv_sid(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	struct wb_sid_object *sid;
	NTSTATUS status;

	status = wb_cmd_lookupname_recv(ctx, s3call, &sid);
	if (!NT_STATUS_IS_OK(status)) goto done;

	s3call->response->result = WINBINDD_OK;
	s3call->response->data.sid.type = sid->type;
	WBSRV_SAMBA3_SET_STRING(s3call->response->data.sid.sid,
				dom_sid_string(s3call, sid->sid));

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

/* 
   Lookup a SID, and return a DOMAIN\\user style name
*/

static void lookupsid_recv_name(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_lookupsid(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;
	struct dom_sid *sid;

	DEBUG(5, ("wbsrv_samba3_lookupsid called\n"));

	sid = dom_sid_parse_talloc(s3call, s3call->request->data.sid);
	if (sid == NULL) {
		DEBUG(5, ("Could not parse sid %s\n",
			  s3call->request->data.sid));
		return NT_STATUS_NO_MEMORY;
	}

	ctx = wb_cmd_lookupsid_send(s3call, service, sid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	/* setup the callbacks */
	ctx->async.fn = lookupsid_recv_name;
	ctx->async.private_data	= s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void lookupsid_recv_name(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	struct wb_sid_object *sid;
	NTSTATUS status;

	status = wb_cmd_lookupsid_recv(ctx, s3call, &sid);
	if (!NT_STATUS_IS_OK(status)) goto done;

	s3call->response->result = WINBINDD_OK;
	s3call->response->data.name.type = sid->type;
	WBSRV_SAMBA3_SET_STRING(s3call->response->data.name.dom_name,
				sid->domain);
	WBSRV_SAMBA3_SET_STRING(s3call->response->data.name.name, sid->name);

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

/*
  This is a stub function in order to limit error message in the pam_winbind module
*/
NTSTATUS wbsrv_samba3_pam_logoff(struct wbsrv_samba3_call *s3call)
{
	NTSTATUS status;
	struct winbindd_response *resp = s3call->response;

	status = NT_STATUS_OK;

	DEBUG(5, ("wbsrv_samba3_pam_logoff called\n"));
	DEBUG(10, ("Winbind logoff not implemented\n"));
	resp->result = WINBINDD_OK;

	WBSRV_SAMBA3_SET_STRING(resp->data.auth.nt_status_string,
				nt_errstr(status));
	WBSRV_SAMBA3_SET_STRING(resp->data.auth.error_string,
				get_friendly_nt_error_msg(status));

	resp->data.auth.pam_error = nt_status_to_pam(status);
	resp->data.auth.nt_status = NT_STATUS_V(status);
	DEBUG(5, ("wbsrv_samba3_pam_logoff called\n"));

	return NT_STATUS_OK;
}

/*
  Challenge-response authentication.  This interface is used by
  ntlm_auth and the smbd auth subsystem to pass NTLM authentication
  requests along a common pipe to the domain controller.  

  The return value (in the async reply) may include the 'info3'
  (effectivly most things you would want to know about the user), or
  the NT and LM session keys seperated.
*/

static void pam_auth_crap_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_pam_auth_crap(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;
	DATA_BLOB chal, nt_resp, lm_resp;

	DEBUG(5, ("wbsrv_samba3_pam_auth_crap called\n"));

	chal.data       = s3call->request->data.auth_crap.chal;
	chal.length     = sizeof(s3call->request->data.auth_crap.chal);
	nt_resp.data    = (uint8_t *)s3call->request->data.auth_crap.nt_resp;
	nt_resp.length  = s3call->request->data.auth_crap.nt_resp_len;
	lm_resp.data    = (uint8_t *)s3call->request->data.auth_crap.lm_resp;
	lm_resp.length  = s3call->request->data.auth_crap.lm_resp_len;

	ctx = wb_cmd_pam_auth_crap_send(
		s3call, service,
		s3call->request->data.auth_crap.logon_parameters,
		s3call->request->data.auth_crap.domain,
		s3call->request->data.auth_crap.user,
		s3call->request->data.auth_crap.workstation,
		chal, nt_resp, lm_resp);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = pam_auth_crap_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void pam_auth_crap_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	DATA_BLOB info3;
	struct netr_UserSessionKey user_session_key;
	struct netr_LMSessionKey lm_key;
	char *unix_username;
	
	status = wb_cmd_pam_auth_crap_recv(ctx, s3call, &info3,
					   &user_session_key, &lm_key, &unix_username);
	if (!NT_STATUS_IS_OK(status)) goto done;

	if (s3call->request->flags & WBFLAG_PAM_USER_SESSION_KEY) {
		memcpy(s3call->response->data.auth.user_session_key,
		       &user_session_key.key,
		       sizeof(s3call->response->data.auth.user_session_key));
	}

	if (s3call->request->flags & WBFLAG_PAM_INFO3_TEXT) {
		status = wb_samba3_append_info3_as_txt(ctx, s3call, info3);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10,("Failed to append INFO3 (TXT): %s\n",
				  nt_errstr(status)));
			goto done;
		}
	}

	if (s3call->request->flags & WBFLAG_PAM_INFO3_NDR) {
		s3call->response->extra_data.data = info3.data;
		s3call->response->length += info3.length;
	}

	if (s3call->request->flags & WBFLAG_PAM_LMKEY) {
		memcpy(s3call->response->data.auth.first_8_lm_hash,
		       lm_key.key,
		       sizeof(s3call->response->data.auth.first_8_lm_hash));
	}
	
	if (s3call->request->flags & WBFLAG_PAM_UNIX_NAME) {
		WBSRV_SAMBA3_SET_STRING(s3call->response->data.auth.unix_username,unix_username);
	}

 done:
	wbsrv_samba3_async_auth_epilogue(status, s3call);
}

/* Plaintext authentication 
   
   This interface is used by ntlm_auth in it's 'basic' authentication
   mode, as well as by pam_winbind to authenticate users where we are
   given a plaintext password.
*/

static void pam_auth_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_pam_auth(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;
	struct cli_credentials *credentials;
	char *user, *domain;

	if (!wb_samba3_split_username(s3call, s3call->wbconn->lp_ctx,
				 s3call->request->data.auth.user,
				 &domain, &user)) {
		return NT_STATUS_NO_SUCH_USER;
	}

	credentials = cli_credentials_init(s3call);
	if (!credentials) {
		return NT_STATUS_NO_MEMORY;
	}
	cli_credentials_set_conf(credentials, service->task->lp_ctx);
	cli_credentials_set_domain(credentials, domain, CRED_SPECIFIED);
	cli_credentials_set_username(credentials, user, CRED_SPECIFIED);

	cli_credentials_set_password(credentials, s3call->request->data.auth.pass, CRED_SPECIFIED);

	ctx = wb_cmd_pam_auth_send(s3call, service, credentials);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = pam_auth_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void pam_auth_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	DATA_BLOB info3;
	struct netr_UserSessionKey user_session_key;
	struct netr_LMSessionKey lm_key;
	char *unix_username;

	status = wb_cmd_pam_auth_recv(ctx, s3call, &info3, 
				      &user_session_key, &lm_key, &unix_username);

	if (!NT_STATUS_IS_OK(status)) goto done;

	if (s3call->request->flags & WBFLAG_PAM_USER_SESSION_KEY) {
		memcpy(s3call->response->data.auth.user_session_key,
		       &user_session_key.key,
		       sizeof(s3call->response->data.auth.user_session_key));
	}

	if (s3call->request->flags & WBFLAG_PAM_INFO3_TEXT) {
		status = wb_samba3_append_info3_as_txt(ctx, s3call, info3);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(10,("Failed to append INFO3 (TXT): %s\n",
				  nt_errstr(status)));
			goto done;
		}
	}

	if (s3call->request->flags & WBFLAG_PAM_INFO3_NDR) {
		s3call->response->extra_data.data = info3.data;
		s3call->response->length += info3.length;
	}

	if (s3call->request->flags & WBFLAG_PAM_LMKEY) {
		memcpy(s3call->response->data.auth.first_8_lm_hash,
		       lm_key.key,
		       sizeof(s3call->response->data.auth.first_8_lm_hash));
	}
	
	if (s3call->request->flags & WBFLAG_PAM_UNIX_NAME) {
		WBSRV_SAMBA3_SET_STRING(s3call->response->data.auth.unix_username,unix_username);
	}
	

 done:
	wbsrv_samba3_async_auth_epilogue(status, s3call);
}

/* 
   List trusted domains
*/

static void list_trustdom_recv_doms(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_list_trustdom(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_list_trustdom called\n"));

	ctx = wb_cmd_list_trustdoms_send(s3call, service);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = list_trustdom_recv_doms;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void list_trustdom_recv_doms(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	uint32_t i, num_domains;
	struct wb_dom_info **domains;
	NTSTATUS status;
	char *result;

	status = wb_cmd_list_trustdoms_recv(ctx, s3call, &num_domains,
					    &domains);
	if (!NT_STATUS_IS_OK(status)) goto done;

	result = talloc_strdup(s3call, "");
	if (result == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	for (i=0; i<num_domains; i++) {
		result = talloc_asprintf_append_buffer(
			result, "%s\\%s\\%s",
			domains[i]->name, domains[i]->name,
			dom_sid_string(s3call, domains[i]->sid));
	}

	if (result == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	s3call->response->result = WINBINDD_OK;
	if (num_domains > 0) {
		s3call->response->extra_data.data = result;
		s3call->response->length += strlen(result)+1;
		s3call->response->data.num_entries = num_domains;
	}

 done:
	wbsrv_samba3_async_epilogue(status, s3call);
}

/* list groups */
static void list_groups_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_list_groups(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service = s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba4_list_groups called\n"));

	ctx = wb_cmd_list_groups_send(s3call, service,
				      s3call->request->domain_name);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = list_groups_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void list_groups_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call = talloc_get_type_abort(
						ctx->async.private_data,
						struct wbsrv_samba3_call);
	uint32_t extra_data_len;
	char *extra_data;
	uint32_t num_groups;
	NTSTATUS status;

	DEBUG(5, ("list_groups_recv called\n"));

	status = wb_cmd_list_groups_recv(ctx, s3call, &extra_data_len,
			&extra_data, &num_groups);

	if (NT_STATUS_IS_OK(status)) {
		s3call->response->extra_data.data = extra_data;
		s3call->response->length += extra_data_len;
		if (extra_data) {
			s3call->response->length += 1;
			s3call->response->data.num_entries = num_groups;
		}
	}

	wbsrv_samba3_async_epilogue(status, s3call);
}

/* List users */

static void list_users_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_list_users(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_list_users called\n"));

	ctx = wb_cmd_list_users_send(s3call, service,
			s3call->request->domain_name);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = list_users_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void list_users_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	uint32_t extra_data_len;
	char *extra_data;
	uint32_t num_users;
	NTSTATUS status;

	DEBUG(5, ("list_users_recv called\n"));

	status = wb_cmd_list_users_recv(ctx, s3call, &extra_data_len,
			&extra_data, &num_users);

	if (NT_STATUS_IS_OK(status)) {
		s3call->response->extra_data.data = extra_data;
		s3call->response->length += extra_data_len;
		if (extra_data) {
			s3call->response->length += 1;
			s3call->response->data.num_entries = num_users;
		}
	}

	wbsrv_samba3_async_epilogue(status, s3call);
}

/* NSS calls */

static void getpwnam_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_getpwnam(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_getpwnam called\n"));

	ctx = wb_cmd_getpwnam_send(s3call, service,
			s3call->request->data.username);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = getpwnam_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void getpwnam_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct winbindd_pw *pw;

	DEBUG(5, ("getpwnam_recv called\n"));

	status = wb_cmd_getpwnam_recv(ctx, s3call, &pw);
	if(NT_STATUS_IS_OK(status))
		s3call->response->data.pw = *pw;

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void getpwuid_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_getpwuid(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service = s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_getpwuid called\n"));

	ctx = wb_cmd_getpwuid_send(s3call, service,
			s3call->request->data.uid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = getpwuid_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void getpwuid_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct winbindd_pw *pw;

	DEBUG(5, ("getpwuid_recv called\n"));

	status = wb_cmd_getpwuid_recv(ctx, s3call, &pw);
	if (NT_STATUS_IS_OK(status))
		s3call->response->data.pw = *pw;

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void setpwent_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_setpwent(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service = s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_setpwent called\n"));

	ctx = wb_cmd_setpwent_send(s3call, service);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = setpwent_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void setpwent_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct wbsrv_pwent *pwent;

	DEBUG(5, ("setpwent_recv called\n"));

	status = wb_cmd_setpwent_recv(ctx, s3call->wbconn, &pwent);
	if (NT_STATUS_IS_OK(status)) {
		s3call->wbconn->protocol_private_data = pwent;
	}

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void getpwent_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_getpwent(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service = s3call->wbconn->listen_socket->service;
	struct wbsrv_pwent *pwent;

	DEBUG(5, ("wbsrv_samba3_getpwent called\n"));

	NT_STATUS_HAVE_NO_MEMORY(s3call->wbconn->protocol_private_data);

	pwent = talloc_get_type(s3call->wbconn->protocol_private_data,
			struct wbsrv_pwent);
	NT_STATUS_HAVE_NO_MEMORY(pwent);

	ctx = wb_cmd_getpwent_send(s3call, service, pwent,
			s3call->request->data.num_entries);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = getpwent_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void getpwent_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct winbindd_pw *pw;
	uint32_t num_users;

	DEBUG(5, ("getpwent_recv called\n"));

	status = wb_cmd_getpwent_recv(ctx, s3call, &pw, &num_users);
	if (NT_STATUS_IS_OK(status)) {
		uint32_t extra_len = sizeof(struct winbindd_pw) * num_users;

		s3call->response->data.num_entries = num_users;
		s3call->response->extra_data.data = pw;
		s3call->response->length += extra_len;
	}

	wbsrv_samba3_async_epilogue(status, s3call);
}

NTSTATUS wbsrv_samba3_endpwent(struct wbsrv_samba3_call *s3call)
{
	struct wbsrv_pwent *pwent =
		talloc_get_type(s3call->wbconn->protocol_private_data,
				struct wbsrv_pwent);
	DEBUG(5, ("wbsrv_samba3_endpwent called\n"));

	talloc_free(pwent);

	s3call->wbconn->protocol_private_data = NULL;
	s3call->response->result = WINBINDD_OK;
	return NT_STATUS_OK;
}


static void getgrnam_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_getgrnam(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_getgrnam called\n"));

	ctx = wb_cmd_getgrnam_send(s3call, service,
			s3call->request->data.groupname);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = getgrnam_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void getgrnam_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct winbindd_gr *gr;

	DEBUG(5, ("getgrnam_recv called\n"));

	status = wb_cmd_getgrnam_recv(ctx, s3call, &gr);
	if(NT_STATUS_IS_OK(status))
		s3call->response->data.gr = *gr;

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void getgrgid_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_getgrgid(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service = s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_getgrgid called\n"));

	ctx = wb_cmd_getgrgid_send(s3call, service,
			s3call->request->data.gid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = getgrgid_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void getgrgid_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct winbindd_gr *gr;

	DEBUG(5, ("getgrgid_recv called\n"));

	status = wb_cmd_getgrgid_recv(ctx, s3call, &gr);
	if (NT_STATUS_IS_OK(status))
		s3call->response->data.gr = *gr;

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void getgroups_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_getgroups(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service = s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_getgroups called\n"));
	/* S3 code do the same so why not ... */
	s3call->request->data.username[sizeof(s3call->request->data.username)-1]='\0';
	ctx = wb_cmd_getgroups_send(s3call, service, s3call->request->data.username);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = getgroups_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void getgroups_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	gid_t *gids;
	uint32_t num_groups;
	NTSTATUS status;
	DEBUG(5, ("getgroups_recv called\n"));

	status = wb_cmd_getgroups_recv(ctx, s3call, &gids, &num_groups);
	if (NT_STATUS_IS_OK(status)) {
		uint32_t extra_len = sizeof(gid_t) * num_groups;

		s3call->response->data.num_entries = num_groups;
		s3call->response->extra_data.data = gids;
		s3call->response->length += extra_len;
	} else {
		s3call->response->result = WINBINDD_ERROR;
	}

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void setgrent_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_setgrent(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service = s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_setgrent called\n"));

	ctx = wb_cmd_setgrent_send(s3call, service);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = setgrent_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void setgrent_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct wbsrv_grent *grent;

	DEBUG(5, ("setpwent_recv called\n"));

	status = wb_cmd_setgrent_recv(ctx, s3call->wbconn, &grent);
	if (NT_STATUS_IS_OK(status)) {
		s3call->wbconn->protocol_private_data = grent;
	}

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void getgrent_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_getgrent(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service = s3call->wbconn->listen_socket->service;
	struct wbsrv_grent *grent;

	DEBUG(5, ("wbsrv_samba3_getgrent called\n"));

	NT_STATUS_HAVE_NO_MEMORY(s3call->wbconn->protocol_private_data);

	grent = talloc_get_type(s3call->wbconn->protocol_private_data,
			struct wbsrv_grent);
	NT_STATUS_HAVE_NO_MEMORY(grent);

	ctx = wb_cmd_getgrent_send(s3call, service, grent,
			s3call->request->data.num_entries);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = getgrent_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;
}

static void getgrent_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct winbindd_gr *gr;
	uint32_t num_groups;

	DEBUG(5, ("getgrent_recv called\n"));

	status = wb_cmd_getgrent_recv(ctx, s3call, &gr, &num_groups);
	if (NT_STATUS_IS_OK(status)) {
		uint32_t extra_len = sizeof(struct winbindd_gr) * num_groups;

		s3call->response->data.num_entries = num_groups;
		s3call->response->extra_data.data = gr;
		s3call->response->length += extra_len;
	}

	wbsrv_samba3_async_epilogue(status, s3call);
}

NTSTATUS wbsrv_samba3_endgrent(struct wbsrv_samba3_call *s3call)
{
	DEBUG(5, ("wbsrv_samba3_endgrent called\n"));
	s3call->response->result = WINBINDD_OK;
	return NT_STATUS_OK;
}

static void sid2uid_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_sid2uid(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;
	struct dom_sid *sid;

	DEBUG(5, ("wbsrv_samba3_sid2uid called\n"));

	sid = dom_sid_parse_talloc(s3call, s3call->request->data.sid);
	NT_STATUS_HAVE_NO_MEMORY(sid);

	ctx = wb_sid2uid_send(s3call, service, sid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = sid2uid_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;

}

static void sid2uid_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;

	DEBUG(5, ("sid2uid_recv called\n"));

	status = wb_sid2uid_recv(ctx, &s3call->response->data.uid);

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void sid2gid_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_sid2gid(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;
	struct dom_sid *sid;

	DEBUG(5, ("wbsrv_samba3_sid2gid called\n"));

	sid = dom_sid_parse_talloc(s3call, s3call->request->data.sid);
	NT_STATUS_HAVE_NO_MEMORY(sid);

	ctx = wb_sid2gid_send(s3call, service, sid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = sid2gid_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;

}

static void sid2gid_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;

	DEBUG(5, ("sid2gid_recv called\n"));

	status = wb_sid2gid_recv(ctx, &s3call->response->data.gid);

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void uid2sid_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_uid2sid(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_uid2sid called\n"));

	ctx = wb_uid2sid_send(s3call, service, s3call->request->data.uid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = uid2sid_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;

}

static void uid2sid_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct dom_sid *sid;
	char *sid_str;

	DEBUG(5, ("uid2sid_recv called\n"));

	status = wb_uid2sid_recv(ctx, s3call, &sid);
	if(NT_STATUS_IS_OK(status)) {
		sid_str = dom_sid_string(s3call, sid);

		/* If the conversion failed, bail out with a failure. */
		if (sid_str == NULL)
			wbsrv_samba3_async_epilogue(NT_STATUS_NO_MEMORY,s3call);

		/* But we assume this worked, so we'll set the string. Work
		 * done. */
		WBSRV_SAMBA3_SET_STRING(s3call->response->data.sid.sid, sid_str);
		s3call->response->data.sid.type = SID_NAME_USER;
	}

	wbsrv_samba3_async_epilogue(status, s3call);
}

static void gid2sid_recv(struct composite_context *ctx);

NTSTATUS wbsrv_samba3_gid2sid(struct wbsrv_samba3_call *s3call)
{
	struct composite_context *ctx;
	struct wbsrv_service *service =
		s3call->wbconn->listen_socket->service;

	DEBUG(5, ("wbsrv_samba3_gid2sid called\n"));

	ctx = wb_gid2sid_send(s3call, service, s3call->request->data.gid);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = gid2sid_recv;
	ctx->async.private_data = s3call;
	s3call->flags |= WBSRV_CALL_FLAGS_REPLY_ASYNC;
	return NT_STATUS_OK;

}

static void gid2sid_recv(struct composite_context *ctx)
{
	struct wbsrv_samba3_call *s3call =
		talloc_get_type(ctx->async.private_data,
				struct wbsrv_samba3_call);
	NTSTATUS status;
	struct dom_sid *sid;
	char *sid_str;

	DEBUG(5, ("gid2sid_recv called\n"));

	status = wb_gid2sid_recv(ctx, s3call, &sid);
	if(NT_STATUS_IS_OK(status)) {
		sid_str = dom_sid_string(s3call, sid);

		if (sid_str == NULL)
			wbsrv_samba3_async_epilogue(NT_STATUS_NO_MEMORY,s3call);

		WBSRV_SAMBA3_SET_STRING(s3call->response->data.sid.sid, sid_str);
		s3call->response->data.sid.type = SID_NAME_DOMAIN;
	}

	wbsrv_samba3_async_epilogue(status, s3call);
}

