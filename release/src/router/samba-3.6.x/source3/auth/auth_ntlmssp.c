/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   handle NLTMSSP, server side

   Copyright (C) Andrew Tridgell      2001
   Copyright (C) Andrew Bartlett 2001-2003

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
#include "auth.h"
#include "../libcli/auth/ntlmssp.h"
#include "ntlmssp_wrap.h"
#include "../librpc/gen_ndr/netlogon.h"
#include "smbd/smbd.h"

NTSTATUS auth_ntlmssp_steal_session_info(TALLOC_CTX *mem_ctx,
					struct auth_ntlmssp_state *auth_ntlmssp_state,
					struct auth_serversupplied_info **session_info)
{
	/* Free the current server_info user_session_key and reset it from the
	 * current ntlmssp_state session_key */
	data_blob_free(&auth_ntlmssp_state->server_info->user_session_key);
	/* Set up the final session key for the connection */
	auth_ntlmssp_state->server_info->user_session_key =
		data_blob_talloc(
			auth_ntlmssp_state->server_info,
			auth_ntlmssp_state->ntlmssp_state->session_key.data,
			auth_ntlmssp_state->ntlmssp_state->session_key.length);
	if (auth_ntlmssp_state->ntlmssp_state->session_key.length &&
	    !auth_ntlmssp_state->server_info->user_session_key.data) {
		*session_info = NULL;
		return NT_STATUS_NO_MEMORY;
	}
	/* Steal session_info away from auth_ntlmssp_state */
	*session_info = talloc_move(mem_ctx, &auth_ntlmssp_state->server_info);
	return NT_STATUS_OK;
}

/**
 * Return the challenge as determined by the authentication subsystem 
 * @return an 8 byte random challenge
 */

static NTSTATUS auth_ntlmssp_get_challenge(const struct ntlmssp_state *ntlmssp_state,
					   uint8_t chal[8])
{
	struct auth_ntlmssp_state *auth_ntlmssp_state =
		(struct auth_ntlmssp_state *)ntlmssp_state->callback_private;
	auth_ntlmssp_state->auth_context->get_ntlm_challenge(
		auth_ntlmssp_state->auth_context, chal);
	return NT_STATUS_OK;
}

/**
 * Some authentication methods 'fix' the challenge, so we may not be able to set it
 *
 * @return If the effective challenge used by the auth subsystem may be modified
 */
static bool auth_ntlmssp_may_set_challenge(const struct ntlmssp_state *ntlmssp_state)
{
	struct auth_ntlmssp_state *auth_ntlmssp_state =
		(struct auth_ntlmssp_state *)ntlmssp_state->callback_private;
	struct auth_context *auth_context = auth_ntlmssp_state->auth_context;

	return auth_context->challenge_may_be_modified;
}

/**
 * NTLM2 authentication modifies the effective challenge, 
 * @param challenge The new challenge value
 */
static NTSTATUS auth_ntlmssp_set_challenge(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *challenge)
{
	struct auth_ntlmssp_state *auth_ntlmssp_state =
		(struct auth_ntlmssp_state *)ntlmssp_state->callback_private;
	struct auth_context *auth_context = auth_ntlmssp_state->auth_context;

	SMB_ASSERT(challenge->length == 8);

	auth_context->challenge = data_blob_talloc(auth_context,
						   challenge->data, challenge->length);

	auth_context->challenge_set_by = "NTLMSSP callback (NTLM2)";

	DEBUG(5, ("auth_context challenge set by %s\n", auth_context->challenge_set_by));
	DEBUG(5, ("challenge is: \n"));
	dump_data(5, auth_context->challenge.data, auth_context->challenge.length);
	return NT_STATUS_OK;
}

/**
 * Check the password on an NTLMSSP login.  
 *
 * Return the session keys used on the connection.
 */

static NTSTATUS auth_ntlmssp_check_password(struct ntlmssp_state *ntlmssp_state, TALLOC_CTX *mem_ctx,
					    DATA_BLOB *user_session_key, DATA_BLOB *lm_session_key)
{
	struct auth_ntlmssp_state *auth_ntlmssp_state =
		(struct auth_ntlmssp_state *)ntlmssp_state->callback_private;
	struct auth_usersupplied_info *user_info = NULL;
	NTSTATUS nt_status;
	bool username_was_mapped;

	/* the client has given us its machine name (which we otherwise would not get on port 445).
	   we need to possibly reload smb.conf if smb.conf includes depend on the machine name */

	set_remote_machine_name(auth_ntlmssp_state->ntlmssp_state->client.netbios_name, True);

	/* setup the string used by %U */
	/* sub_set_smb_name checks for weird internally */
	sub_set_smb_name(auth_ntlmssp_state->ntlmssp_state->user);

	reload_services(smbd_messaging_context(), -1, True);

	nt_status = make_user_info_map(&user_info, 
				       auth_ntlmssp_state->ntlmssp_state->user, 
				       auth_ntlmssp_state->ntlmssp_state->domain, 
				       auth_ntlmssp_state->ntlmssp_state->client.netbios_name,
	                               auth_ntlmssp_state->ntlmssp_state->lm_resp.data ? &auth_ntlmssp_state->ntlmssp_state->lm_resp : NULL, 
	                               auth_ntlmssp_state->ntlmssp_state->nt_resp.data ? &auth_ntlmssp_state->ntlmssp_state->nt_resp : NULL, 
				       NULL, NULL, NULL,
				       AUTH_PASSWORD_RESPONSE);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	user_info->logon_parameters = MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT | MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT;

	nt_status = auth_ntlmssp_state->auth_context->check_ntlm_password(auth_ntlmssp_state->auth_context, 
									  user_info, &auth_ntlmssp_state->server_info); 

	username_was_mapped = user_info->was_mapped;

	free_user_info(&user_info);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	auth_ntlmssp_state->server_info->nss_token |= username_was_mapped;

	nt_status = create_local_token(auth_ntlmssp_state->server_info);

	if (!NT_STATUS_IS_OK(nt_status)) {
		DEBUG(10, ("create_local_token failed: %s\n",
			nt_errstr(nt_status)));
		return nt_status;
	}

	/* Clear out the session keys, and pass them to the caller.
	 * They will not be used in this form again - instead the
	 * NTLMSSP code will decide on the final correct session key,
	 * and put it back here at the end of
	 * auth_ntlmssp_steal_server_info */
	if (auth_ntlmssp_state->server_info->user_session_key.length) {
		DEBUG(10, ("Got NT session key of length %u\n",
			(unsigned int)auth_ntlmssp_state->server_info->user_session_key.length));
		*user_session_key = auth_ntlmssp_state->server_info->user_session_key;
		talloc_steal(mem_ctx, auth_ntlmssp_state->server_info->user_session_key.data);
		auth_ntlmssp_state->server_info->user_session_key = data_blob_null;
	}
	if (auth_ntlmssp_state->server_info->lm_session_key.length) {
		DEBUG(10, ("Got LM session key of length %u\n",
			(unsigned int)auth_ntlmssp_state->server_info->lm_session_key.length));
		*lm_session_key = auth_ntlmssp_state->server_info->lm_session_key;
		talloc_steal(mem_ctx, auth_ntlmssp_state->server_info->lm_session_key.data);
		auth_ntlmssp_state->server_info->lm_session_key = data_blob_null;
	}
	return nt_status;
}

static int auth_ntlmssp_state_destructor(void *ptr);

NTSTATUS auth_ntlmssp_start(struct auth_ntlmssp_state **auth_ntlmssp_state)
{
	NTSTATUS nt_status;
	bool is_standalone;
	const char *netbios_name;
	const char *netbios_domain;
	const char *dns_name;
	char *dns_domain;
	struct auth_ntlmssp_state *ans;
	struct auth_context *auth_context;

	if ((enum server_types)lp_server_role() == ROLE_STANDALONE) {
		is_standalone = true;
	} else {
		is_standalone = false;
	}

	netbios_name = global_myname();
	netbios_domain = lp_workgroup();
	/* This should be a 'netbios domain -> DNS domain' mapping */
	dns_domain = get_mydnsdomname(talloc_tos());
	if (dns_domain) {
		strlower_m(dns_domain);
	}
	dns_name = get_mydnsfullname();

	ans = talloc_zero(NULL, struct auth_ntlmssp_state);
	if (!ans) {
		DEBUG(0,("auth_ntlmssp_start: talloc failed!\n"));
		return NT_STATUS_NO_MEMORY;
	}

	nt_status = ntlmssp_server_start(ans,
					 is_standalone,
					 netbios_name,
					 netbios_domain,
					 dns_name,
					 dns_domain,
					 &ans->ntlmssp_state);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	nt_status = make_auth_context_subsystem(talloc_tos(), &auth_context);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}
	ans->auth_context = talloc_steal(ans, auth_context);

	ans->ntlmssp_state->callback_private = ans;
	ans->ntlmssp_state->get_challenge = auth_ntlmssp_get_challenge;
	ans->ntlmssp_state->may_set_challenge = auth_ntlmssp_may_set_challenge;
	ans->ntlmssp_state->set_challenge = auth_ntlmssp_set_challenge;
	ans->ntlmssp_state->check_password = auth_ntlmssp_check_password;

	talloc_set_destructor((TALLOC_CTX *)ans, auth_ntlmssp_state_destructor);

	*auth_ntlmssp_state = ans;
	return NT_STATUS_OK;
}

static int auth_ntlmssp_state_destructor(void *ptr)
{
	struct auth_ntlmssp_state *ans;

	ans = talloc_get_type(ptr, struct auth_ntlmssp_state);

	TALLOC_FREE(ans->server_info);
	TALLOC_FREE(ans->ntlmssp_state);
	return 0;
}
