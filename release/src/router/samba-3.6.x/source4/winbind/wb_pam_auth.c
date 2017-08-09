/* 
   Unix SMB/CIFS implementation.

   Authenticate a user

   Copyright (C) Volker Lendecke 2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   
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
#include "libcli/composite/composite.h"
#include "winbind/wb_server.h"
#include "smbd/service_task.h"
#include "auth/credentials/credentials.h"
#include "libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/winbind.h"
#include "param/param.h"

/* Oh, there is so much to keep an eye on when authenticating a user.  Oh my! */
struct pam_auth_crap_state {
	struct composite_context *ctx;
	struct tevent_context *event_ctx;
	struct loadparm_context *lp_ctx;

	struct winbind_SamLogon *req;
	char *unix_username;

        struct netr_NetworkInfo ninfo;
        struct netr_LogonSamLogon r;

	const char *user_name;
	const char *domain_name;

	struct netr_UserSessionKey user_session_key;
	struct netr_LMSessionKey lm_key;
	DATA_BLOB info3;
};

/*
 * NTLM authentication.
*/

static void pam_auth_crap_recv_logon(struct composite_context *ctx);

struct composite_context *wb_cmd_pam_auth_crap_send(TALLOC_CTX *mem_ctx,
						    struct wbsrv_service *service,
						    uint32_t logon_parameters,
						    const char *domain,
						    const char *user,
						    const char *workstation,
						    DATA_BLOB chal,
						    DATA_BLOB nt_resp,
						    DATA_BLOB lm_resp)
{
	struct composite_context *result, *ctx;
	struct pam_auth_crap_state *state;
	struct netr_NetworkInfo *ninfo;
	DATA_BLOB tmp_nt_resp, tmp_lm_resp;

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct pam_auth_crap_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	state->lp_ctx = service->task->lp_ctx;
	result->private_data = state;

	state->req = talloc(state, struct winbind_SamLogon);

	state->req->in.logon_level = 2;
	state->req->in.validation_level = 3;
	ninfo = state->req->in.logon.network = talloc(state, struct netr_NetworkInfo);
	if (ninfo == NULL) goto failed;
	
	ninfo->identity_info.account_name.string =  talloc_strdup(state, user);
	ninfo->identity_info.domain_name.string =  talloc_strdup(state, domain);
	ninfo->identity_info.parameter_control = logon_parameters;
	ninfo->identity_info.logon_id_low = 0;
	ninfo->identity_info.logon_id_high = 0;
	ninfo->identity_info.workstation.string = talloc_strdup(state, workstation);

	SMB_ASSERT(chal.length == sizeof(ninfo->challenge));
	memcpy(ninfo->challenge, chal.data,
	       sizeof(ninfo->challenge));

	tmp_nt_resp = data_blob_talloc(ninfo, nt_resp.data, nt_resp.length);
	if ((nt_resp.data != NULL) &&
	    (tmp_nt_resp.data == NULL)) goto failed;

	tmp_lm_resp = data_blob_talloc(ninfo, lm_resp.data, lm_resp.length);
	if ((lm_resp.data != NULL) &&
	    (tmp_lm_resp.data == NULL)) goto failed;

	ninfo->nt.length = tmp_nt_resp.length;
	ninfo->nt.data = tmp_nt_resp.data;
	ninfo->lm.length = tmp_lm_resp.length;
	ninfo->lm.data = tmp_lm_resp.data;

	state->unix_username = NULL;

	ctx = wb_sam_logon_send(mem_ctx, service, state->req);
	if (ctx == NULL) goto failed;

	composite_continue(result, ctx, pam_auth_crap_recv_logon, state);
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

/*  
    NTLM Authentication

    Send of a SamLogon request to authenticate a user.
*/
static void pam_auth_crap_recv_logon(struct composite_context *ctx)
{
	DATA_BLOB tmp_blob;
	enum ndr_err_code ndr_err;
	struct netr_SamBaseInfo *base;
	struct pam_auth_crap_state *state =
		talloc_get_type(ctx->async.private_data,
				struct pam_auth_crap_state);

	state->ctx->status = wb_sam_logon_recv(ctx, state, state->req);
	if (!composite_is_ok(state->ctx)) return;

	ndr_err = ndr_push_struct_blob(
		&tmp_blob, state, state->req->out.validation.sam3,
		(ndr_push_flags_fn_t)ndr_push_netr_SamInfo3);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		state->ctx->status = ndr_map_error2ntstatus(ndr_err);
		if (!composite_is_ok(state->ctx)) return;
	}

	/* The Samba3 protocol is a bit broken (due to non-IDL
	 * heritage, so for compatability we must add a non-zero 4
	 * bytes to the info3 */
	state->info3 = data_blob_talloc(state, NULL, tmp_blob.length+4);
	if (composite_nomem(state->info3.data, state->ctx)) return;

	SIVAL(state->info3.data, 0, 1);
	memcpy(state->info3.data+4, tmp_blob.data, tmp_blob.length);

	base = &state->req->out.validation.sam3->base;

	state->user_session_key = base->key;
	state->lm_key = base->LMSessKey;

	/* Give the caller the most accurate username possible.
	 * Assists where case sensitive comparisons may be done by our
	 * ntlm_auth callers */
	if (base->account_name.string) {
		state->user_name = base->account_name.string;
		talloc_steal(state, base->account_name.string);
	}
	if (base->domain.string) {
		state->domain_name = base->domain.string;
		talloc_steal(state, base->domain.string);
	}

	state->unix_username = talloc_asprintf(state, "%s%s%s", 
					       state->domain_name,
					       lpcfg_winbind_separator(state->lp_ctx),
					       state->user_name);
	if (composite_nomem(state->unix_username, state->ctx)) return;

	composite_done(state->ctx);
}

/* Having received a NTLM authentication reply, parse out the useful
 * reply data for the caller */
NTSTATUS wb_cmd_pam_auth_crap_recv(struct composite_context *c,
				   TALLOC_CTX *mem_ctx,
				   DATA_BLOB *info3,
				   struct netr_UserSessionKey *user_session_key,
				   struct netr_LMSessionKey *lm_key,
				   char **unix_username)
{
	struct pam_auth_crap_state *state =
		talloc_get_type(c->private_data, struct pam_auth_crap_state);
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		info3->length = state->info3.length;
		info3->data = talloc_steal(mem_ctx, state->info3.data);
		*user_session_key = state->user_session_key;
		*lm_key = state->lm_key;
		*unix_username = talloc_steal(mem_ctx, state->unix_username);
	}
	talloc_free(state);
	return status;
}

/* Handle plaintext authentication, by encrypting the password and
 * then sending via the NTLM calls */

struct composite_context *wb_cmd_pam_auth_send(TALLOC_CTX *mem_ctx,
					       struct wbsrv_service *service,
					       struct cli_credentials *credentials)
{
	const char *workstation;
	NTSTATUS status;
	const char *user, *domain;
	DATA_BLOB chal, nt_resp, lm_resp, names_blob;
	int flags = CLI_CRED_NTLM_AUTH;
	if (lpcfg_client_lanman_auth(service->task->lp_ctx)) {
		flags |= CLI_CRED_LANMAN_AUTH;
	}

	if (lpcfg_client_ntlmv2_auth(service->task->lp_ctx)) {
		flags |= CLI_CRED_NTLMv2_AUTH;
	}

	DEBUG(5, ("wbsrv_samba3_pam_auth called\n"));

	chal = data_blob_talloc(mem_ctx, NULL, 8);
	if (!chal.data) {
		return NULL;
	}
	generate_random_buffer(chal.data, chal.length);
	cli_credentials_get_ntlm_username_domain(credentials, mem_ctx,
						 &user, &domain);
	/* for best compatability with multiple vitual netbios names
	 * on the host, this should be generated from the
	 * cli_credentials associated with the machine account */
	workstation = cli_credentials_get_workstation(credentials);

	names_blob = NTLMv2_generate_names_blob(
		mem_ctx,
		cli_credentials_get_workstation(credentials), 
		cli_credentials_get_domain(credentials));

	status = cli_credentials_get_ntlm_response(
		credentials, mem_ctx, &flags, chal, names_blob,
		&lm_resp, &nt_resp, NULL, NULL);
	if (!NT_STATUS_IS_OK(status)) {
		return NULL;
	}
	return wb_cmd_pam_auth_crap_send(mem_ctx, service,
					 MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT|MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT /* logon parameters */, 
					 domain, user, workstation,
					 chal, nt_resp, lm_resp);
}

NTSTATUS wb_cmd_pam_auth_recv(struct composite_context *c,
			      TALLOC_CTX *mem_ctx,
			      DATA_BLOB *info3,
			      struct netr_UserSessionKey *user_session_key,
			      struct netr_LMSessionKey *lm_key,
			      char **unix_username)
{
	struct pam_auth_crap_state *state =
		talloc_get_type(c->private_data, struct pam_auth_crap_state);
	NTSTATUS status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		if (info3) {
			info3->length = state->info3.length;
			info3->data = talloc_steal(mem_ctx, state->info3.data);
		}
		if (user_session_key) {
			*user_session_key = state->user_session_key;
		}
		if (lm_key) {
			*lm_key = state->lm_key;
		}
		if (unix_username) {
			*unix_username = talloc_steal(mem_ctx, state->unix_username);
		}
	}
	talloc_free(state);
	return status;
}
