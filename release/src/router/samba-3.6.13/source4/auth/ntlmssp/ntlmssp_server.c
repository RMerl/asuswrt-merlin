/* 
   Unix SMB/Netbios implementation.
   Version 3.0
   handle NLTMSSP, client server side parsing

   Copyright (C) Andrew Tridgell      2001
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2001-2005
   Copyright (C) Stefan Metzmacher 2005

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
#include "system/network.h"
#include "lib/tsocket/tsocket.h"
#include "auth/ntlmssp/ntlmssp.h"
#include "../librpc/gen_ndr/ndr_ntlmssp.h"
#include "../libcli/auth/ntlmssp_ndr.h"
#include "../libcli/auth/ntlmssp_private.h"
#include "../libcli/auth/libcli_auth.h"
#include "../lib/crypto/crypto.h"
#include "auth/gensec/gensec.h"
#include "auth/gensec/gensec_proto.h"
#include "auth/auth.h"
#include "param/param.h"

/**
 * Next state function for the Negotiate packet (GENSEC wrapper)
 *
 * @param gensec_security GENSEC state
 * @param out_mem_ctx Memory context for *out
 * @param in The request, as a DATA_BLOB.  reply.data must be NULL
 * @param out The reply, as an allocated DATA_BLOB, caller to free.
 * @return Errors or MORE_PROCESSING_REQUIRED if (normal) a reply is required.
 */

NTSTATUS gensec_ntlmssp_server_negotiate(struct gensec_security *gensec_security,
					 TALLOC_CTX *out_mem_ctx,
					 const DATA_BLOB request, DATA_BLOB *reply)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gensec_ntlmssp_context);
	struct ntlmssp_state *ntlmssp_state = gensec_ntlmssp->ntlmssp_state;
	return ntlmssp_server_negotiate(ntlmssp_state, out_mem_ctx, request, reply);
}

/**
 * Next state function for the Authenticate packet (GENSEC wrapper)
 *
 * @param gensec_security GENSEC state
 * @param out_mem_ctx Memory context for *out
 * @param in The request, as a DATA_BLOB.  reply.data must be NULL
 * @param out The reply, as an allocated DATA_BLOB, caller to free.
 * @return Errors or NT_STATUS_OK if authentication sucessful
 */

NTSTATUS gensec_ntlmssp_server_auth(struct gensec_security *gensec_security,
				    TALLOC_CTX *out_mem_ctx,
				    const DATA_BLOB in, DATA_BLOB *out)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gensec_ntlmssp_context);
	struct ntlmssp_state *ntlmssp_state = gensec_ntlmssp->ntlmssp_state;
	return ntlmssp_server_auth(ntlmssp_state, out_mem_ctx, in, out);
}

/**
 * Return the challenge as determined by the authentication subsystem 
 * @return an 8 byte random challenge
 */

static NTSTATUS auth_ntlmssp_get_challenge(const struct ntlmssp_state *ntlmssp_state,
					   uint8_t chal[8])
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(ntlmssp_state->callback_private,
				      struct gensec_ntlmssp_context);
	struct auth_context *auth_context = gensec_ntlmssp->auth_context;
	NTSTATUS status;

	status = auth_context->get_challenge(auth_context, chal);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(1, ("auth_ntlmssp_get_challenge: failed to get challenge: %s\n",
			nt_errstr(status)));
		return status;
	}

	return NT_STATUS_OK;
}

/**
 * Some authentication methods 'fix' the challenge, so we may not be able to set it
 *
 * @return If the effective challenge used by the auth subsystem may be modified
 */
static bool auth_ntlmssp_may_set_challenge(const struct ntlmssp_state *ntlmssp_state)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(ntlmssp_state->callback_private,
				      struct gensec_ntlmssp_context);
	struct auth_context *auth_context = gensec_ntlmssp->auth_context;

	return auth_context->challenge_may_be_modified(auth_context);
}

/**
 * NTLM2 authentication modifies the effective challenge, 
 * @param challenge The new challenge value
 */
static NTSTATUS auth_ntlmssp_set_challenge(struct ntlmssp_state *ntlmssp_state, DATA_BLOB *challenge)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(ntlmssp_state->callback_private,
				      struct gensec_ntlmssp_context);
	struct auth_context *auth_context = gensec_ntlmssp->auth_context;
	NTSTATUS nt_status;
	const uint8_t *chal;

	if (challenge->length != 8) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	chal = challenge->data;

	nt_status = auth_context->set_challenge(auth_context,
						chal,
						"NTLMSSP callback (NTLM2)");

	return nt_status;
}

/**
 * Check the password on an NTLMSSP login.  
 *
 * Return the session keys used on the connection.
 */

static NTSTATUS auth_ntlmssp_check_password(struct ntlmssp_state *ntlmssp_state,
					    TALLOC_CTX *mem_ctx,
					    DATA_BLOB *user_session_key, DATA_BLOB *lm_session_key)
{
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(ntlmssp_state->callback_private,
				      struct gensec_ntlmssp_context);
	struct auth_context *auth_context = gensec_ntlmssp->auth_context;
	NTSTATUS nt_status;
	struct auth_usersupplied_info *user_info;

	user_info = talloc_zero(ntlmssp_state, struct auth_usersupplied_info);
	if (!user_info) {
		return NT_STATUS_NO_MEMORY;
	}

	user_info->logon_parameters = MSV1_0_ALLOW_SERVER_TRUST_ACCOUNT | MSV1_0_ALLOW_WORKSTATION_TRUST_ACCOUNT;
	user_info->flags = 0;
	user_info->mapped_state = false;
	user_info->client.account_name = ntlmssp_state->user;
	user_info->client.domain_name = ntlmssp_state->domain;
	user_info->workstation_name = ntlmssp_state->client.netbios_name;
	user_info->remote_host = gensec_get_remote_address(gensec_ntlmssp->gensec_security);

	user_info->password_state = AUTH_PASSWORD_RESPONSE;
	user_info->password.response.lanman = ntlmssp_state->lm_resp;
	user_info->password.response.lanman.data = talloc_steal(user_info, ntlmssp_state->lm_resp.data);
	user_info->password.response.nt = ntlmssp_state->nt_resp;
	user_info->password.response.nt.data = talloc_steal(user_info, ntlmssp_state->nt_resp.data);

	nt_status = auth_context->check_password(auth_context,
						 gensec_ntlmssp,
						 user_info,
						 &gensec_ntlmssp->user_info_dc);
	talloc_free(user_info);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	if (gensec_ntlmssp->user_info_dc->user_session_key.length) {
		DEBUG(10, ("Got NT session key of length %u\n",
			   (unsigned)gensec_ntlmssp->user_info_dc->user_session_key.length));
		*user_session_key = gensec_ntlmssp->user_info_dc->user_session_key;
		talloc_steal(mem_ctx, user_session_key->data);
		gensec_ntlmssp->user_info_dc->user_session_key = data_blob_null;
	}
	if (gensec_ntlmssp->user_info_dc->lm_session_key.length) {
		DEBUG(10, ("Got LM session key of length %u\n",
			   (unsigned)gensec_ntlmssp->user_info_dc->lm_session_key.length));
		*lm_session_key = gensec_ntlmssp->user_info_dc->lm_session_key;
		talloc_steal(mem_ctx, lm_session_key->data);
		gensec_ntlmssp->user_info_dc->lm_session_key = data_blob_null;
	}
	return nt_status;
}

/** 
 * Return the credentials of a logged on user, including session keys
 * etc.
 *
 * Only valid after a successful authentication
 *
 * May only be called once per authentication.
 *
 */

NTSTATUS gensec_ntlmssp_session_info(struct gensec_security *gensec_security,
				     struct auth_session_info **session_info) 
{
	NTSTATUS nt_status;
	struct gensec_ntlmssp_context *gensec_ntlmssp =
		talloc_get_type_abort(gensec_security->private_data,
				      struct gensec_ntlmssp_context);
	struct ntlmssp_state *ntlmssp_state = gensec_ntlmssp->ntlmssp_state;

	nt_status = gensec_generate_session_info(ntlmssp_state,
						 gensec_security,
						 gensec_ntlmssp->user_info_dc,
						 session_info);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	(*session_info)->session_key = data_blob_talloc(*session_info, 
							ntlmssp_state->session_key.data,
							ntlmssp_state->session_key.length);

	return NT_STATUS_OK;
}

/**
 * Start NTLMSSP on the server side 
 *
 */
NTSTATUS gensec_ntlmssp_server_start(struct gensec_security *gensec_security)
{
	NTSTATUS nt_status;
	struct ntlmssp_state *ntlmssp_state;
	struct gensec_ntlmssp_context *gensec_ntlmssp;

	nt_status = gensec_ntlmssp_start(gensec_security);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	gensec_ntlmssp = talloc_get_type_abort(gensec_security->private_data,
					       struct gensec_ntlmssp_context);
	ntlmssp_state = gensec_ntlmssp->ntlmssp_state;

	ntlmssp_state->role = NTLMSSP_SERVER;

	ntlmssp_state->expected_state = NTLMSSP_NEGOTIATE;

	ntlmssp_state->allow_lm_key = (lpcfg_lanman_auth(gensec_security->settings->lp_ctx)
					  && gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "allow_lm_key", false));

	ntlmssp_state->neg_flags =
		NTLMSSP_NEGOTIATE_NTLM | NTLMSSP_NEGOTIATE_VERSION;

	ntlmssp_state->lm_resp = data_blob(NULL, 0);
	ntlmssp_state->nt_resp = data_blob(NULL, 0);

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "128bit", true)) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_128;
	}

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "56bit", true)) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_56;
	}

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "keyexchange", true)) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_KEY_EXCH;
	}

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "alwayssign", true)) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
	}

	if (gensec_setting_bool(gensec_security->settings, "ntlmssp_server", "ntlm2", true)) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_NTLM2;
	}

	if (gensec_security->want_features & GENSEC_FEATURE_SIGN) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SIGN;
	}
	if (gensec_security->want_features & GENSEC_FEATURE_SEAL) {
		ntlmssp_state->neg_flags |= NTLMSSP_NEGOTIATE_SEAL;
	}

	gensec_ntlmssp->auth_context = gensec_security->auth_context;

	ntlmssp_state->get_challenge = auth_ntlmssp_get_challenge;
	ntlmssp_state->may_set_challenge = auth_ntlmssp_may_set_challenge;
	ntlmssp_state->set_challenge = auth_ntlmssp_set_challenge;
	ntlmssp_state->check_password = auth_ntlmssp_check_password;
	if (lpcfg_server_role(gensec_security->settings->lp_ctx) == ROLE_STANDALONE) {
		ntlmssp_state->server.is_standalone = true;
	} else {
		ntlmssp_state->server.is_standalone = false;
	}

	ntlmssp_state->server.netbios_name = lpcfg_netbios_name(gensec_security->settings->lp_ctx);

	ntlmssp_state->server.netbios_domain = lpcfg_workgroup(gensec_security->settings->lp_ctx);

	{
		char dnsdomname[MAXHOSTNAMELEN], dnsname[MAXHOSTNAMELEN];

		/* Find out the DNS domain name */
		dnsdomname[0] = '\0';
		safe_strcpy(dnsdomname, lpcfg_dnsdomain(gensec_security->settings->lp_ctx), sizeof(dnsdomname) - 1);

		/* Find out the DNS host name */
		safe_strcpy(dnsname, ntlmssp_state->server.netbios_name, sizeof(dnsname) - 1);
		if (dnsdomname[0] != '\0') {
			safe_strcat(dnsname, ".", sizeof(dnsname) - 1);
			safe_strcat(dnsname, dnsdomname, sizeof(dnsname) - 1);
		}
		strlower_m(dnsname);

		ntlmssp_state->server.dns_name = talloc_strdup(ntlmssp_state,
								      dnsname);
		NT_STATUS_HAVE_NO_MEMORY(ntlmssp_state->server.dns_name);

		ntlmssp_state->server.dns_domain = talloc_strdup(ntlmssp_state,
								        dnsdomname);
		NT_STATUS_HAVE_NO_MEMORY(ntlmssp_state->server.dns_domain);
	}

	return NT_STATUS_OK;
}

