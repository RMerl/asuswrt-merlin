/*
   Unix SMB/CIFS implementation.

   endpoint server for the netlogon pipe

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2008
   Copyright (C) Stefan Metzmacher <metze@samba.org>  2005
   Copyright (C) Matthias Dieter Wallnöfer            2009-2010

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
#include "rpc_server/dcerpc_server.h"
#include "auth/auth.h"
#include "auth/auth_sam_reply.h"
#include "dsdb/samdb/samdb.h"
#include "../lib/util/util_ldb.h"
#include "../libcli/auth/schannel.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_irpc_c.h"
#include "../libcli/ldap/ldap_ndr.h"
#include "cldap_server/cldap_server.h"
#include "lib/tsocket/tsocket.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_irpc.h"

struct netlogon_server_pipe_state {
	struct netr_Credential client_challenge;
	struct netr_Credential server_challenge;
};

static NTSTATUS dcesrv_netr_ServerReqChallenge(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					struct netr_ServerReqChallenge *r)
{
	struct netlogon_server_pipe_state *pipe_state =
		talloc_get_type(dce_call->context->private_data, struct netlogon_server_pipe_state);

	ZERO_STRUCTP(r->out.return_credentials);

	/* destroyed on pipe shutdown */

	if (pipe_state) {
		talloc_free(pipe_state);
		dce_call->context->private_data = NULL;
	}

	pipe_state = talloc(dce_call->context, struct netlogon_server_pipe_state);
	NT_STATUS_HAVE_NO_MEMORY(pipe_state);

	pipe_state->client_challenge = *r->in.credentials;

	generate_random_buffer(pipe_state->server_challenge.data,
			       sizeof(pipe_state->server_challenge.data));

	*r->out.return_credentials = pipe_state->server_challenge;

	dce_call->context->private_data = pipe_state;

	return NT_STATUS_OK;
}

static NTSTATUS dcesrv_netr_ServerAuthenticate3(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					 struct netr_ServerAuthenticate3 *r)
{
	struct netlogon_server_pipe_state *pipe_state =
		talloc_get_type(dce_call->context->private_data, struct netlogon_server_pipe_state);
	struct netlogon_creds_CredentialState *creds;
	struct ldb_context *sam_ctx;
	struct samr_Password *mach_pwd;
	uint32_t user_account_control;
	int num_records;
	struct ldb_message **msgs;
	NTSTATUS nt_status;
	const char *attrs[] = {"unicodePwd", "userAccountControl",
			       "objectSid", NULL};

	const char *trust_dom_attrs[] = {"flatname", NULL};
	const char *account_name;

	ZERO_STRUCTP(r->out.return_credentials);
	*r->out.rid = 0;

	/*
	 * According to Microsoft (see bugid #6099)
	 * Windows 7 looks at the negotiate_flags
	 * returned in this structure *even if the
	 * call fails with access denied!
	 */
	*r->out.negotiate_flags = NETLOGON_NEG_ACCOUNT_LOCKOUT |
				  NETLOGON_NEG_PERSISTENT_SAMREPL |
				  NETLOGON_NEG_ARCFOUR |
				  NETLOGON_NEG_PROMOTION_COUNT |
				  NETLOGON_NEG_CHANGELOG_BDC |
				  NETLOGON_NEG_FULL_SYNC_REPL |
				  NETLOGON_NEG_MULTIPLE_SIDS |
				  NETLOGON_NEG_REDO |
				  NETLOGON_NEG_PASSWORD_CHANGE_REFUSAL |
				  NETLOGON_NEG_SEND_PASSWORD_INFO_PDC |
				  NETLOGON_NEG_GENERIC_PASSTHROUGH |
				  NETLOGON_NEG_CONCURRENT_RPC |
				  NETLOGON_NEG_AVOID_ACCOUNT_DB_REPL |
				  NETLOGON_NEG_AVOID_SECURITYAUTH_DB_REPL |
				  NETLOGON_NEG_STRONG_KEYS |
				  NETLOGON_NEG_TRANSITIVE_TRUSTS |
				  NETLOGON_NEG_DNS_DOMAIN_TRUSTS |
				  NETLOGON_NEG_PASSWORD_SET2 |
				  NETLOGON_NEG_GETDOMAININFO |
				  NETLOGON_NEG_CROSS_FOREST_TRUSTS |
				  NETLOGON_NEG_NEUTRALIZE_NT4_EMULATION |
				  NETLOGON_NEG_RODC_PASSTHROUGH |
				  NETLOGON_NEG_AUTHENTICATED_RPC_LSASS |
				  NETLOGON_NEG_AUTHENTICATED_RPC;

	switch (r->in.secure_channel_type) {
	case SEC_CHAN_WKSTA:
	case SEC_CHAN_DNS_DOMAIN:
	case SEC_CHAN_DOMAIN:
	case SEC_CHAN_BDC:
	case SEC_CHAN_RODC:
		break;
	default:
		DEBUG(1, ("Client asked for an invalid secure channel type: %d\n",
			  r->in.secure_channel_type));
		return NT_STATUS_INVALID_PARAMETER;
	}

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx,
				system_session(dce_call->conn->dce_ctx->lp_ctx), 0);
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	if (r->in.secure_channel_type == SEC_CHAN_DNS_DOMAIN) {
		char *encoded_account = ldb_binary_encode_string(mem_ctx, r->in.account_name);
		const char *flatname;
		if (!encoded_account) {
			return NT_STATUS_NO_MEMORY;
		}

		/* Kill the trailing dot */
		if (encoded_account[strlen(encoded_account)-1] == '.') {
			encoded_account[strlen(encoded_account)-1] = '\0';
		}

		/* pull the user attributes */
		num_records = gendb_search(sam_ctx, mem_ctx, NULL, &msgs,
					   trust_dom_attrs,
					   "(&(trustPartner=%s)(objectclass=trustedDomain))",
					   encoded_account);

		if (num_records == 0) {
			DEBUG(3,("Couldn't find trust [%s] in samdb.\n",
				 encoded_account));
			return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
		}

		if (num_records > 1) {
			DEBUG(0,("Found %d records matching user [%s]\n", num_records, r->in.account_name));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		flatname = ldb_msg_find_attr_as_string(msgs[0], "flatname", NULL);
		if (!flatname) {
			/* No flatname for this trust - we can't proceed */
			return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
		}
		account_name = talloc_asprintf(mem_ctx, "%s$", flatname);

		if (!account_name) {
			return NT_STATUS_NO_MEMORY;
		}

	} else {
		account_name = r->in.account_name;
	}

	/* pull the user attributes */
	num_records = gendb_search(sam_ctx, mem_ctx, NULL, &msgs, attrs,
				   "(&(sAMAccountName=%s)(objectclass=user))",
				   ldb_binary_encode_string(mem_ctx, account_name));

	if (num_records == 0) {
		DEBUG(3,("Couldn't find user [%s] in samdb.\n",
			 r->in.account_name));
		return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
	}

	if (num_records > 1) {
		DEBUG(0,("Found %d records matching user [%s]\n", num_records, r->in.account_name));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	user_account_control = ldb_msg_find_attr_as_uint(msgs[0], "userAccountControl", 0);

	if (user_account_control & UF_ACCOUNTDISABLE) {
		DEBUG(1, ("Account [%s] is disabled\n", r->in.account_name));
		return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
	}

	if (r->in.secure_channel_type == SEC_CHAN_WKSTA) {
		if (!(user_account_control & UF_WORKSTATION_TRUST_ACCOUNT)) {
			DEBUG(1, ("Client asked for a workstation secure channel, but is not a workstation (member server) acb flags: 0x%x\n", user_account_control));
			return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
		}
	} else if (r->in.secure_channel_type == SEC_CHAN_DOMAIN ||
		   r->in.secure_channel_type == SEC_CHAN_DNS_DOMAIN) {
		if (!(user_account_control & UF_INTERDOMAIN_TRUST_ACCOUNT)) {
			DEBUG(1, ("Client asked for a trusted domain secure channel, but is not a trusted domain: acb flags: 0x%x\n", user_account_control));

			return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
		}
	} else if (r->in.secure_channel_type == SEC_CHAN_BDC) {
		if (!(user_account_control & UF_SERVER_TRUST_ACCOUNT)) {
			DEBUG(1, ("Client asked for a server secure channel, but is not a server (domain controller): acb flags: 0x%x\n", user_account_control));
			return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
		}
	} else if (r->in.secure_channel_type == SEC_CHAN_RODC) {
		if (!(user_account_control & UF_PARTIAL_SECRETS_ACCOUNT)) {
			DEBUG(1, ("Client asked for a RODC secure channel, but is not a RODC: acb flags: 0x%x\n", user_account_control));
			return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
		}
	} else {
		/* we should never reach this */
		return NT_STATUS_INTERNAL_ERROR;
	}

	*r->out.rid = samdb_result_rid_from_sid(mem_ctx, msgs[0],
						"objectSid", 0);

	mach_pwd = samdb_result_hash(mem_ctx, msgs[0], "unicodePwd");
	if (mach_pwd == NULL) {
		return NT_STATUS_ACCESS_DENIED;
	}

	if (!pipe_state) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	creds = netlogon_creds_server_init(mem_ctx,
					   r->in.account_name,
					   r->in.computer_name,
					   r->in.secure_channel_type,
					   &pipe_state->client_challenge,
					   &pipe_state->server_challenge,
					   mach_pwd,
					   r->in.credentials,
					   r->out.return_credentials,
					   *r->in.negotiate_flags);

	if (!creds) {
		return NT_STATUS_ACCESS_DENIED;
	}

	creds->sid = samdb_result_dom_sid(creds, msgs[0], "objectSid");

	nt_status = schannel_save_creds_state(mem_ctx,
					      lpcfg_private_dir(dce_call->conn->dce_ctx->lp_ctx),
					      creds);

	return nt_status;
}

static NTSTATUS dcesrv_netr_ServerAuthenticate(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					struct netr_ServerAuthenticate *r)
{
	struct netr_ServerAuthenticate3 a;
	uint32_t rid;
	/* TODO:
	 * negotiate_flags is used as an [in] parameter
	 * so it need to be initialised.
	 *
	 * (I think ... = 0; seems wrong here --metze)
	 */
	uint32_t negotiate_flags_in = 0;
	uint32_t negotiate_flags_out = 0;

	a.in.server_name		= r->in.server_name;
	a.in.account_name		= r->in.account_name;
	a.in.secure_channel_type	= r->in.secure_channel_type;
	a.in.computer_name		= r->in.computer_name;
	a.in.credentials		= r->in.credentials;
	a.in.negotiate_flags		= &negotiate_flags_in;

	a.out.return_credentials	= r->out.return_credentials;
	a.out.rid			= &rid;
	a.out.negotiate_flags		= &negotiate_flags_out;

	return dcesrv_netr_ServerAuthenticate3(dce_call, mem_ctx, &a);
}

static NTSTATUS dcesrv_netr_ServerAuthenticate2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					 struct netr_ServerAuthenticate2 *r)
{
	struct netr_ServerAuthenticate3 r3;
	uint32_t rid = 0;

	r3.in.server_name = r->in.server_name;
	r3.in.account_name = r->in.account_name;
	r3.in.secure_channel_type = r->in.secure_channel_type;
	r3.in.computer_name = r->in.computer_name;
	r3.in.credentials = r->in.credentials;
	r3.out.return_credentials = r->out.return_credentials;
	r3.in.negotiate_flags = r->in.negotiate_flags;
	r3.out.negotiate_flags = r->out.negotiate_flags;
	r3.out.rid = &rid;

	return dcesrv_netr_ServerAuthenticate3(dce_call, mem_ctx, &r3);
}

/*
 * NOTE: The following functions are nearly identical to the ones available in
 * source3/rpc_server/srv_nelog_nt.c
 * The reason we keep 2 copies is that they use different structures to
 * represent the auth_info and the decrpc pipes.
 */

/*
 * If schannel is required for this call test that it actually is available.
 */
static NTSTATUS schannel_check_required(struct dcerpc_auth *auth_info,
					const char *computer_name,
					bool integrity, bool privacy)
{

	if (auth_info && auth_info->auth_type == DCERPC_AUTH_TYPE_SCHANNEL) {
		if (!privacy && !integrity) {
			return NT_STATUS_OK;
		}

		if ((!privacy && integrity) &&
		    auth_info->auth_level == DCERPC_AUTH_LEVEL_INTEGRITY) {
			return NT_STATUS_OK;
		}

		if ((privacy || integrity) &&
		    auth_info->auth_level == DCERPC_AUTH_LEVEL_PRIVACY) {
			return NT_STATUS_OK;
		}
	}

	/* test didn't pass */
	DEBUG(0, ("schannel_check_required: [%s] is not using schannel\n",
		  computer_name));

	return NT_STATUS_ACCESS_DENIED;
}

static NTSTATUS dcesrv_netr_creds_server_step_check(struct dcesrv_call_state *dce_call,
						    TALLOC_CTX *mem_ctx,
						    const char *computer_name,
						    struct netr_Authenticator *received_authenticator,
						    struct netr_Authenticator *return_authenticator,
						    struct netlogon_creds_CredentialState **creds_out)
{
	NTSTATUS nt_status;
	struct dcerpc_auth *auth_info = dce_call->conn->auth_state.auth_info;
	bool schannel_global_required = false; /* Should be lpcfg_schannel_server() == true */

	if (schannel_global_required) {
		nt_status = schannel_check_required(auth_info,
						    computer_name,
						    true, false);
		if (!NT_STATUS_IS_OK(nt_status)) {
			return nt_status;
		}
	}

	nt_status = schannel_check_creds_state(mem_ctx,
					       lpcfg_private_dir(dce_call->conn->dce_ctx->lp_ctx),
					       computer_name,
					       received_authenticator,
					       return_authenticator,
					       creds_out);
	return nt_status;
}

/*
  Change the machine account password for the currently connected
  client.  Supplies only the NT#.
*/

static NTSTATUS dcesrv_netr_ServerPasswordSet(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				       struct netr_ServerPasswordSet *r)
{
	struct netlogon_creds_CredentialState *creds;
	struct ldb_context *sam_ctx;
	const char * const attrs[] = { "unicodePwd", NULL };
	struct ldb_message **res;
	struct samr_Password *oldNtHash;
	NTSTATUS nt_status;
	int ret;

	nt_status = dcesrv_netr_creds_server_step_check(dce_call,
							mem_ctx,
							r->in.computer_name,
							r->in.credential, r->out.return_authenticator,
							&creds);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, system_session(dce_call->conn->dce_ctx->lp_ctx), 0);
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	netlogon_creds_des_decrypt(creds, r->in.new_password);

	/* fetch the old password hashes (the NT hash has to exist) */

	ret = gendb_search(sam_ctx, mem_ctx, NULL, &res, attrs,
			   "(&(objectClass=user)(objectSid=%s))",
			   ldap_encode_ndr_dom_sid(mem_ctx, creds->sid));
	if (ret != 1) {
		return NT_STATUS_WRONG_PASSWORD;
	}

	nt_status = samdb_result_passwords(mem_ctx,
					   dce_call->conn->dce_ctx->lp_ctx,
					   res[0], NULL, &oldNtHash);
	if (!NT_STATUS_IS_OK(nt_status) || !oldNtHash) {
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* Using the sid for the account as the key, set the password */
	nt_status = samdb_set_password_sid(sam_ctx, mem_ctx,
					   creds->sid,
					   NULL, /* Don't have plaintext */
					   NULL, r->in.new_password,
					   NULL, oldNtHash, /* Password change */
					   NULL, NULL);
	return nt_status;
}

/*
  Change the machine account password for the currently connected
  client.  Supplies new plaintext.
*/
static NTSTATUS dcesrv_netr_ServerPasswordSet2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				       struct netr_ServerPasswordSet2 *r)
{
	struct netlogon_creds_CredentialState *creds;
	struct ldb_context *sam_ctx;
	const char * const attrs[] = { "dBCSPwd", "unicodePwd", NULL };
	struct ldb_message **res;
	struct samr_Password *oldLmHash, *oldNtHash;
	NTSTATUS nt_status;
	DATA_BLOB new_password;
	int ret;

	struct samr_CryptPassword password_buf;

	nt_status = dcesrv_netr_creds_server_step_check(dce_call,
							mem_ctx,
							r->in.computer_name,
							r->in.credential, r->out.return_authenticator,
							&creds);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, system_session(dce_call->conn->dce_ctx->lp_ctx), 0);
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	memcpy(password_buf.data, r->in.new_password->data, 512);
	SIVAL(password_buf.data, 512, r->in.new_password->length);
	netlogon_creds_arcfour_crypt(creds, password_buf.data, 516);

	if (!extract_pw_from_buffer(mem_ctx, password_buf.data, &new_password)) {
		DEBUG(3,("samr: failed to decode password buffer\n"));
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* fetch the old password hashes (at least one of both has to exist) */

	ret = gendb_search(sam_ctx, mem_ctx, NULL, &res, attrs,
			   "(&(objectClass=user)(objectSid=%s))",
			   ldap_encode_ndr_dom_sid(mem_ctx, creds->sid));
	if (ret != 1) {
		return NT_STATUS_WRONG_PASSWORD;
	}

	nt_status = samdb_result_passwords(mem_ctx,
					   dce_call->conn->dce_ctx->lp_ctx,
					   res[0], &oldLmHash, &oldNtHash);
	if (!NT_STATUS_IS_OK(nt_status) || (!oldLmHash && !oldNtHash)) {
		return NT_STATUS_WRONG_PASSWORD;
	}

	/* Using the sid for the account as the key, set the password */
	nt_status = samdb_set_password_sid(sam_ctx, mem_ctx,
					   creds->sid,
					   &new_password, /* we have plaintext */
					   NULL, NULL,
					   oldLmHash, oldNtHash, /* Password change */
					   NULL, NULL);
	return nt_status;
}


/*
  netr_LogonUasLogon
*/
static WERROR dcesrv_netr_LogonUasLogon(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				 struct netr_LogonUasLogon *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_LogonUasLogoff
*/
static WERROR dcesrv_netr_LogonUasLogoff(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_LogonUasLogoff *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


static NTSTATUS dcesrv_netr_LogonSamLogon_check(const struct netr_LogonSamLogonEx *r)
{
	switch (r->in.logon_level) {
	case NetlogonInteractiveInformation:
	case NetlogonServiceInformation:
	case NetlogonInteractiveTransitiveInformation:
	case NetlogonServiceTransitiveInformation:
		if (r->in.logon->password == NULL) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		switch (r->in.validation_level) {
		case NetlogonValidationSamInfo:  /* 2 */
		case NetlogonValidationSamInfo2: /* 3 */
		case NetlogonValidationSamInfo4: /* 6 */
			break;
		default:
			return NT_STATUS_INVALID_INFO_CLASS;
		}

		break;
	case NetlogonNetworkInformation:
	case NetlogonNetworkTransitiveInformation:
		if (r->in.logon->network == NULL) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		switch (r->in.validation_level) {
		case NetlogonValidationSamInfo:  /* 2 */
		case NetlogonValidationSamInfo2: /* 3 */
		case NetlogonValidationSamInfo4: /* 6 */
			break;
		default:
			return NT_STATUS_INVALID_INFO_CLASS;
		}

		break;

	case NetlogonGenericInformation:
		if (r->in.logon->generic == NULL) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		switch (r->in.validation_level) {
		/* TODO: case NetlogonValidationGenericInfo: 4 */
		case NetlogonValidationGenericInfo2: /* 5 */
			break;
		default:
			return NT_STATUS_INVALID_INFO_CLASS;
		}

		break;
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

/*
  netr_LogonSamLogon_base

  This version of the function allows other wrappers to say 'do not check the credentials'

  We can't do the traditional 'wrapping' format completly, as this function must only run under schannel
*/
static NTSTATUS dcesrv_netr_LogonSamLogon_base(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					struct netr_LogonSamLogonEx *r, struct netlogon_creds_CredentialState *creds)
{
	struct auth_context *auth_context;
	struct auth_usersupplied_info *user_info;
	struct auth_user_info_dc *user_info_dc;
	NTSTATUS nt_status;
	static const char zeros[16];
	struct netr_SamBaseInfo *sam;
	struct netr_SamInfo2 *sam2;
	struct netr_SamInfo3 *sam3;
	struct netr_SamInfo6 *sam6;

	*r->out.authoritative = 1;

	user_info = talloc_zero(mem_ctx, struct auth_usersupplied_info);
	NT_STATUS_HAVE_NO_MEMORY(user_info);

	switch (r->in.logon_level) {
	case NetlogonInteractiveInformation:
	case NetlogonServiceInformation:
	case NetlogonInteractiveTransitiveInformation:
	case NetlogonServiceTransitiveInformation:
		if (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR) {
			netlogon_creds_arcfour_crypt(creds,
					    r->in.logon->password->lmpassword.hash,
					    sizeof(r->in.logon->password->lmpassword.hash));
			netlogon_creds_arcfour_crypt(creds,
					    r->in.logon->password->ntpassword.hash,
					    sizeof(r->in.logon->password->ntpassword.hash));
		} else {
			netlogon_creds_des_decrypt(creds, &r->in.logon->password->lmpassword);
			netlogon_creds_des_decrypt(creds, &r->in.logon->password->ntpassword);
		}

		/* TODO: we need to deny anonymous access here */
		nt_status = auth_context_create(mem_ctx,
						dce_call->event_ctx, dce_call->msg_ctx,
						dce_call->conn->dce_ctx->lp_ctx,
						&auth_context);
		NT_STATUS_NOT_OK_RETURN(nt_status);

		user_info->logon_parameters = r->in.logon->password->identity_info.parameter_control;
		user_info->client.account_name = r->in.logon->password->identity_info.account_name.string;
		user_info->client.domain_name = r->in.logon->password->identity_info.domain_name.string;
		user_info->workstation_name = r->in.logon->password->identity_info.workstation.string;

		user_info->flags |= USER_INFO_INTERACTIVE_LOGON;
		user_info->password_state = AUTH_PASSWORD_HASH;

		user_info->password.hash.lanman = talloc(user_info, struct samr_Password);
		NT_STATUS_HAVE_NO_MEMORY(user_info->password.hash.lanman);
		*user_info->password.hash.lanman = r->in.logon->password->lmpassword;

		user_info->password.hash.nt = talloc(user_info, struct samr_Password);
		NT_STATUS_HAVE_NO_MEMORY(user_info->password.hash.nt);
		*user_info->password.hash.nt = r->in.logon->password->ntpassword;

		break;
	case NetlogonNetworkInformation:
	case NetlogonNetworkTransitiveInformation:

		/* TODO: we need to deny anonymous access here */
		nt_status = auth_context_create(mem_ctx,
						dce_call->event_ctx, dce_call->msg_ctx,
						dce_call->conn->dce_ctx->lp_ctx,
						&auth_context);
		NT_STATUS_NOT_OK_RETURN(nt_status);

		nt_status = auth_context_set_challenge(auth_context, r->in.logon->network->challenge, "netr_LogonSamLogonWithFlags");
		NT_STATUS_NOT_OK_RETURN(nt_status);

		user_info->logon_parameters = r->in.logon->network->identity_info.parameter_control;
		user_info->client.account_name = r->in.logon->network->identity_info.account_name.string;
		user_info->client.domain_name = r->in.logon->network->identity_info.domain_name.string;
		user_info->workstation_name = r->in.logon->network->identity_info.workstation.string;

		user_info->password_state = AUTH_PASSWORD_RESPONSE;
		user_info->password.response.lanman = data_blob_talloc(mem_ctx, r->in.logon->network->lm.data, r->in.logon->network->lm.length);
		user_info->password.response.nt = data_blob_talloc(mem_ctx, r->in.logon->network->nt.data, r->in.logon->network->nt.length);

		break;


	case NetlogonGenericInformation:
	{
		if (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR) {
			netlogon_creds_arcfour_crypt(creds,
					    r->in.logon->generic->data, r->in.logon->generic->length);
		} else {
			/* Using DES to verify kerberos tickets makes no sense */
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (strcmp(r->in.logon->generic->package_name.string, "Kerberos") == 0) {
			NTSTATUS status;
			struct dcerpc_binding_handle *irpc_handle;
			struct kdc_check_generic_kerberos check;
			struct netr_GenericInfo2 *generic = talloc_zero(mem_ctx, struct netr_GenericInfo2);
			NT_STATUS_HAVE_NO_MEMORY(generic);
			*r->out.authoritative = 1;

			/* TODO: Describe and deal with these flags */
			*r->out.flags = 0;

			r->out.validation->generic = generic;

			irpc_handle = irpc_binding_handle_by_name(mem_ctx,
								  dce_call->msg_ctx,
								  "kdc_server",
								  &ndr_table_irpc);
			if (irpc_handle == NULL) {
				return NT_STATUS_NO_LOGON_SERVERS;
			}

			check.in.generic_request =
				data_blob_const(r->in.logon->generic->data,
						r->in.logon->generic->length);

			status = dcerpc_kdc_check_generic_kerberos_r(irpc_handle,
								     mem_ctx,
								     &check);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
			generic->length = check.out.generic_reply.length;
			generic->data = check.out.generic_reply.data;
			return NT_STATUS_OK;
		}

		/* Until we get an implemetnation of these other packages */
		return NT_STATUS_INVALID_PARAMETER;
	}
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	nt_status = auth_check_password(auth_context, mem_ctx, user_info, &user_info_dc);
	/* TODO: set *r->out.authoritative = 0 on specific errors */
	NT_STATUS_NOT_OK_RETURN(nt_status);

	switch (r->in.validation_level) {
	case 2:
		nt_status = auth_convert_user_info_dc_sambaseinfo(mem_ctx, user_info_dc, &sam);
		NT_STATUS_NOT_OK_RETURN(nt_status);

		sam2 = talloc_zero(mem_ctx, struct netr_SamInfo2);
		NT_STATUS_HAVE_NO_MEMORY(sam2);
		sam2->base = *sam;

		/* And put into the talloc tree */
		talloc_steal(sam2, sam);
		r->out.validation->sam2 = sam2;

		sam = &sam2->base;
		break;

	case 3:
		nt_status = auth_convert_user_info_dc_saminfo3(mem_ctx,
							      user_info_dc,
							      &sam3);
		NT_STATUS_NOT_OK_RETURN(nt_status);

		r->out.validation->sam3 = sam3;

		sam = &sam3->base;
		break;

	case 6:
		nt_status = auth_convert_user_info_dc_saminfo3(mem_ctx,
							   user_info_dc,
							   &sam3);
		NT_STATUS_NOT_OK_RETURN(nt_status);

		sam6 = talloc_zero(mem_ctx, struct netr_SamInfo6);
		NT_STATUS_HAVE_NO_MEMORY(sam6);
		sam6->base = sam3->base;
		sam = &sam6->base;
		sam6->sidcount = sam3->sidcount;
		sam6->sids = sam3->sids;

		sam6->dns_domainname.string = lpcfg_dnsdomain(dce_call->conn->dce_ctx->lp_ctx);
		sam6->principle.string = talloc_asprintf(mem_ctx, "%s@%s",
							 sam->account_name.string, sam6->dns_domainname.string);
		NT_STATUS_HAVE_NO_MEMORY(sam6->principle.string);
		/* And put into the talloc tree */
		talloc_steal(sam6, sam3);

		r->out.validation->sam6 = sam6;
		break;

	default:
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	/* Don't crypt an all-zero key, it would give away the NETLOGON pipe session key */
	/* It appears that level 6 is not individually encrypted */
	if ((r->in.validation_level != 6) &&
	    memcmp(sam->key.key, zeros, sizeof(sam->key.key)) != 0) {
		/* This key is sent unencrypted without the ARCFOUR flag set */
		if (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR) {
			netlogon_creds_arcfour_crypt(creds,
					    sam->key.key,
					    sizeof(sam->key.key));
		}
	}

	/* Don't crypt an all-zero key, it would give away the NETLOGON pipe session key */
	/* It appears that level 6 is not individually encrypted */
	if ((r->in.validation_level != 6) &&
	    memcmp(sam->LMSessKey.key, zeros, sizeof(sam->LMSessKey.key)) != 0) {
		if (creds->negotiate_flags & NETLOGON_NEG_ARCFOUR) {
			netlogon_creds_arcfour_crypt(creds,
					    sam->LMSessKey.key,
					    sizeof(sam->LMSessKey.key));
		} else {
			netlogon_creds_des_encrypt_LMKey(creds,
						&sam->LMSessKey);
		}
	}

	/* TODO: Describe and deal with these flags */
	*r->out.flags = 0;

	return NT_STATUS_OK;
}

static NTSTATUS dcesrv_netr_LogonSamLogonEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct netr_LogonSamLogonEx *r)
{
	NTSTATUS nt_status;
	struct netlogon_creds_CredentialState *creds;

	*r->out.authoritative = 1;

	nt_status = dcesrv_netr_LogonSamLogon_check(r);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	nt_status = schannel_get_creds_state(mem_ctx,
					     lpcfg_private_dir(dce_call->conn->dce_ctx->lp_ctx),
					     r->in.computer_name, &creds);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	if (!dce_call->conn->auth_state.auth_info ||
	    dce_call->conn->auth_state.auth_info->auth_type != DCERPC_AUTH_TYPE_SCHANNEL) {
		return NT_STATUS_ACCESS_DENIED;
	}
	return dcesrv_netr_LogonSamLogon_base(dce_call, mem_ctx, r, creds);
}

/*
  netr_LogonSamLogonWithFlags

*/
static NTSTATUS dcesrv_netr_LogonSamLogonWithFlags(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					    struct netr_LogonSamLogonWithFlags *r)
{
	NTSTATUS nt_status;
	struct netlogon_creds_CredentialState *creds;
	struct netr_LogonSamLogonEx r2;

	struct netr_Authenticator *return_authenticator;

	ZERO_STRUCT(r2);

	r2.in.server_name	= r->in.server_name;
	r2.in.computer_name	= r->in.computer_name;
	r2.in.logon_level	= r->in.logon_level;
	r2.in.logon		= r->in.logon;
	r2.in.validation_level	= r->in.validation_level;
	r2.in.flags		= r->in.flags;
	r2.out.validation	= r->out.validation;
	r2.out.authoritative	= r->out.authoritative;
	r2.out.flags		= r->out.flags;

	*r->out.authoritative = 1;

	nt_status = dcesrv_netr_LogonSamLogon_check(&r2);
	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	return_authenticator = talloc(mem_ctx, struct netr_Authenticator);
	NT_STATUS_HAVE_NO_MEMORY(return_authenticator);

	nt_status = dcesrv_netr_creds_server_step_check(dce_call,
							mem_ctx,
							r->in.computer_name,
							r->in.credential, return_authenticator,
							&creds);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	nt_status = dcesrv_netr_LogonSamLogon_base(dce_call, mem_ctx, &r2, creds);

	r->out.return_authenticator	= return_authenticator;

	return nt_status;
}

/*
  netr_LogonSamLogon
*/
static NTSTATUS dcesrv_netr_LogonSamLogon(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				   struct netr_LogonSamLogon *r)
{
	struct netr_LogonSamLogonWithFlags r2;
	uint32_t flags = 0;
	NTSTATUS status;

	ZERO_STRUCT(r2);

	r2.in.server_name = r->in.server_name;
	r2.in.computer_name = r->in.computer_name;
	r2.in.credential  = r->in.credential;
	r2.in.return_authenticator = r->in.return_authenticator;
	r2.in.logon_level = r->in.logon_level;
	r2.in.logon = r->in.logon;
	r2.in.validation_level = r->in.validation_level;
	r2.in.flags = &flags;
	r2.out.validation = r->out.validation;
	r2.out.authoritative = r->out.authoritative;
	r2.out.flags = &flags;

	status = dcesrv_netr_LogonSamLogonWithFlags(dce_call, mem_ctx, &r2);

	r->out.return_authenticator = r2.out.return_authenticator;

	return status;
}


/*
  netr_LogonSamLogoff
*/
static NTSTATUS dcesrv_netr_LogonSamLogoff(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_LogonSamLogoff *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}



/*
  netr_DatabaseDeltas
*/
static NTSTATUS dcesrv_netr_DatabaseDeltas(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DatabaseDeltas *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_DatabaseSync2
*/
static NTSTATUS dcesrv_netr_DatabaseSync2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DatabaseSync2 *r)
{
	/* win2k3 native mode returns  "NOT IMPLEMENTED" for this call */
	return NT_STATUS_NOT_IMPLEMENTED;
}


/*
  netr_DatabaseSync
*/
static NTSTATUS dcesrv_netr_DatabaseSync(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DatabaseSync *r)
{
	struct netr_DatabaseSync2 r2;
	NTSTATUS status;

	ZERO_STRUCT(r2);

	r2.in.logon_server = r->in.logon_server;
	r2.in.computername = r->in.computername;
	r2.in.credential = r->in.credential;
	r2.in.database_id = r->in.database_id;
	r2.in.restart_state = SYNCSTATE_NORMAL_STATE;
	r2.in.sync_context = r->in.sync_context;
	r2.out.sync_context = r->out.sync_context;
	r2.out.delta_enum_array = r->out.delta_enum_array;
	r2.in.preferredmaximumlength = r->in.preferredmaximumlength;

	status = dcesrv_netr_DatabaseSync2(dce_call, mem_ctx, &r2);

	return status;
}


/*
  netr_AccountDeltas
*/
static NTSTATUS dcesrv_netr_AccountDeltas(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_AccountDeltas *r)
{
	/* w2k3 returns "NOT IMPLEMENTED" for this call */
	return NT_STATUS_NOT_IMPLEMENTED;
}


/*
  netr_AccountSync
*/
static NTSTATUS dcesrv_netr_AccountSync(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_AccountSync *r)
{
	/* w2k3 returns "NOT IMPLEMENTED" for this call */
	return NT_STATUS_NOT_IMPLEMENTED;
}


/*
  netr_GetDcName
*/
static WERROR dcesrv_netr_GetDcName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_GetDcName *r)
{
	const char * const attrs[] = { NULL };
	struct ldb_context *sam_ctx;
	struct ldb_message **res;
	struct ldb_dn *domain_dn;
	int ret;
	const char *dcname;

	/*
	 * [MS-NRPC] 3.5.5.3.4 NetrGetDCName says
	 * that the domainname needs to be a valid netbios domain
	 * name, if it is not NULL.
	 */
	if (r->in.domainname) {
		const char *dot = strchr(r->in.domainname, '.');
		size_t len = strlen(r->in.domainname);

		if (dot || len > 15) {
			return WERR_DCNOTFOUND;
		}

		/*
		 * TODO: Should we also varify that only valid
		 *       netbios name characters are used?
		 */
	}

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return WERR_DS_UNAVAILABLE;
	}

	domain_dn = samdb_domain_to_dn(sam_ctx, mem_ctx,
				       r->in.domainname);
	if (domain_dn == NULL) {
		return WERR_DS_UNAVAILABLE;
	}

	ret = gendb_search_dn(sam_ctx, mem_ctx,
			      domain_dn, &res, attrs);
	if (ret != 1) {
		return WERR_NO_SUCH_DOMAIN;
	}

	/* TODO: - return real IP address
	 *       - check all r->in.* parameters (server_unc is ignored by w2k3!)
	 */
	dcname = talloc_asprintf(mem_ctx, "\\\\%s",
				 lpcfg_netbios_name(dce_call->conn->dce_ctx->lp_ctx));
	W_ERROR_HAVE_NO_MEMORY(dcname);

	*r->out.dcname = dcname;
	return WERR_OK;
}


/*
  netr_LogonControl2Ex
*/
static WERROR dcesrv_netr_LogonControl2Ex(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_LogonControl2Ex *r)
{
	return WERR_NOT_SUPPORTED;
}


/*
  netr_LogonControl
*/
static WERROR dcesrv_netr_LogonControl(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_LogonControl *r)
{
	struct netr_LogonControl2Ex r2;
	WERROR werr;

	if (r->in.level == 0x00000001) {
		ZERO_STRUCT(r2);

		r2.in.logon_server = r->in.logon_server;
		r2.in.function_code = r->in.function_code;
		r2.in.level = r->in.level;
		r2.in.data = NULL;
		r2.out.query = r->out.query;

		werr = dcesrv_netr_LogonControl2Ex(dce_call, mem_ctx, &r2);
	} else if (r->in.level == 0x00000002) {
		werr = WERR_NOT_SUPPORTED;
	} else {
		werr = WERR_UNKNOWN_LEVEL;
	}

	return werr;
}


/*
  netr_LogonControl2
*/
static WERROR dcesrv_netr_LogonControl2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_LogonControl2 *r)
{
	struct netr_LogonControl2Ex r2;
	WERROR werr;

	ZERO_STRUCT(r2);

	r2.in.logon_server = r->in.logon_server;
	r2.in.function_code = r->in.function_code;
	r2.in.level = r->in.level;
	r2.in.data = r->in.data;
	r2.out.query = r->out.query;

	werr = dcesrv_netr_LogonControl2Ex(dce_call, mem_ctx, &r2);

	return werr;
}

static WERROR fill_trusted_domains_array(TALLOC_CTX *mem_ctx,
					 struct ldb_context *sam_ctx,
					 struct netr_DomainTrustList *trusts,
					 uint32_t trust_flags);

/*
  netr_GetAnyDCName
*/
static WERROR dcesrv_netr_GetAnyDCName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_GetAnyDCName *r)
{
	struct netr_DomainTrustList *trusts;
	struct ldb_context *sam_ctx;
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;
	uint32_t i;
	WERROR werr;

	*r->out.dcname = NULL;

	if ((r->in.domainname == NULL) || (r->in.domainname[0] == '\0')) {
		/* if the domainname parameter wasn't set assume our domain */
		r->in.domainname = lpcfg_workgroup(lp_ctx);
	}

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return WERR_DS_UNAVAILABLE;
	}

	if (strcasecmp(r->in.domainname, lpcfg_workgroup(lp_ctx)) == 0) {
		/* well we asked for a DC of our own domain */
		if (samdb_is_pdc(sam_ctx)) {
			/* we are the PDC of the specified domain */
			return WERR_NO_SUCH_DOMAIN;
		}

		*r->out.dcname = talloc_asprintf(mem_ctx, "\\%s",
						lpcfg_netbios_name(lp_ctx));
		W_ERROR_HAVE_NO_MEMORY(*r->out.dcname);

		return WERR_OK;
	}

	/* Okay, now we have to consider the trusted domains */

	trusts = talloc_zero(mem_ctx, struct netr_DomainTrustList);
	W_ERROR_HAVE_NO_MEMORY(trusts);

	trusts->count = 0;

	werr = fill_trusted_domains_array(mem_ctx, sam_ctx, trusts,
					  NETR_TRUST_FLAG_INBOUND
					  | NETR_TRUST_FLAG_OUTBOUND);
	W_ERROR_NOT_OK_RETURN(werr);

	for (i = 0; i < trusts->count; i++) {
		if (strcasecmp(r->in.domainname, trusts->array[i].netbios_name) == 0) {
			/* FIXME: Here we need to find a DC for the specified
			 * trusted domain. */

			/* return WERR_OK; */
			return WERR_NO_SUCH_DOMAIN;
		}
	}

	return WERR_NO_SUCH_DOMAIN;
}


/*
  netr_DatabaseRedo
*/
static NTSTATUS dcesrv_netr_DatabaseRedo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DatabaseRedo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_NetrEnumerateTrustedDomains
*/
static NTSTATUS dcesrv_netr_NetrEnumerateTrustedDomains(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_NetrEnumerateTrustedDomains *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_LogonGetCapabilities
*/
static NTSTATUS dcesrv_netr_LogonGetCapabilities(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_LogonGetCapabilities *r)
{
	/* we don't support AES yet */
	return NT_STATUS_NOT_IMPLEMENTED;
}


/*
  netr_NETRLOGONSETSERVICEBITS
*/
static WERROR dcesrv_netr_NETRLOGONSETSERVICEBITS(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_NETRLOGONSETSERVICEBITS *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_LogonGetTrustRid
*/
static WERROR dcesrv_netr_LogonGetTrustRid(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_LogonGetTrustRid *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_NETRLOGONCOMPUTESERVERDIGEST
*/
static WERROR dcesrv_netr_NETRLOGONCOMPUTESERVERDIGEST(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_NETRLOGONCOMPUTESERVERDIGEST *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_NETRLOGONCOMPUTECLIENTDIGEST
*/
static WERROR dcesrv_netr_NETRLOGONCOMPUTECLIENTDIGEST(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_NETRLOGONCOMPUTECLIENTDIGEST *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}



/*
  netr_DsRGetSiteName
*/
static WERROR dcesrv_netr_DsRGetSiteName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  struct netr_DsRGetSiteName *r)
{
	struct ldb_context *sam_ctx;
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return WERR_DS_UNAVAILABLE;
	}

	*r->out.site = samdb_server_site_name(sam_ctx, mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(*r->out.site);

	return WERR_OK;
}


/*
  fill in a netr_OneDomainInfo from a ldb search result
*/
static NTSTATUS fill_one_domain_info(TALLOC_CTX *mem_ctx,
				     struct loadparm_context *lp_ctx,
				     struct ldb_context *sam_ctx,
				     struct ldb_message *res,
				     struct netr_OneDomainInfo *info,
				     bool is_local, bool is_trust_list)
{
	ZERO_STRUCTP(info);

	if (is_trust_list) {
		/* w2k8 only fills this on trusted domains */
		info->trust_extension.info = talloc_zero(mem_ctx, struct netr_trust_extension);
		info->trust_extension.length = 16;
		info->trust_extension.info->flags =
			NETR_TRUST_FLAG_TREEROOT |
			NETR_TRUST_FLAG_IN_FOREST |
			NETR_TRUST_FLAG_PRIMARY |
			NETR_TRUST_FLAG_NATIVE;

		info->trust_extension.info->parent_index = 0; /* should be index into array
								 of parent */
		info->trust_extension.info->trust_type = LSA_TRUST_TYPE_UPLEVEL; /* should be based on ldb search for trusts */
		info->trust_extension.info->trust_attributes = 0; /* 	TODO: base on ldb search? */
	}

	if (is_trust_list) {
		/* MS-NRPC 3.5.4.3.9 - must be set to NULL for trust list */
		info->dns_forestname.string = NULL;
	} else {
		info->dns_forestname.string = samdb_forest_name(sam_ctx, mem_ctx);
		NT_STATUS_HAVE_NO_MEMORY(info->dns_forestname.string);
		info->dns_forestname.string = talloc_asprintf(mem_ctx, "%s.", info->dns_forestname.string);
		NT_STATUS_HAVE_NO_MEMORY(info->dns_forestname.string);
	}

	if (is_local) {
		info->domainname.string = lpcfg_workgroup(lp_ctx);
		info->dns_domainname.string = lpcfg_dnsdomain(lp_ctx);
		info->domain_guid = samdb_result_guid(res, "objectGUID");
		info->domain_sid = samdb_result_dom_sid(mem_ctx, res, "objectSid");
	} else {
		info->domainname.string = ldb_msg_find_attr_as_string(res, "flatName", NULL);
		info->dns_domainname.string = ldb_msg_find_attr_as_string(res, "trustPartner", NULL);
		info->domain_guid = samdb_result_guid(res, "objectGUID");
		info->domain_sid = samdb_result_dom_sid(mem_ctx, res, "securityIdentifier");
	}
	if (!is_trust_list) {
		info->dns_domainname.string = talloc_asprintf(mem_ctx, "%s.", info->dns_domainname.string);
	}

	return NT_STATUS_OK;
}

/*
  netr_LogonGetDomainInfo
  this is called as part of the ADS domain logon procedure.

  It has an important role in convaying details about the client, such
  as Operating System, Version, Service Pack etc.
*/
static NTSTATUS dcesrv_netr_LogonGetDomainInfo(struct dcesrv_call_state *dce_call,
	TALLOC_CTX *mem_ctx, struct netr_LogonGetDomainInfo *r)
{
	struct netlogon_creds_CredentialState *creds;
	const char * const attrs[] = { "objectSid", "objectGUID", "flatName",
		"securityIdentifier", "trustPartner", NULL };
	const char * const attrs2[] = { "sAMAccountName", "dNSHostName",
		"msDS-SupportedEncryptionTypes", NULL };
	const char *sam_account_name, *old_dns_hostname, *prefix1, *prefix2;
	struct ldb_context *sam_ctx;
	struct ldb_message **res1, **res2, **res3, *new_msg;
	struct ldb_dn *workstation_dn;
	struct netr_DomainInformation *domain_info;
	struct netr_LsaPolicyInformation *lsa_policy_info;
	uint32_t default_supported_enc_types = 0xFFFFFFFF;
	bool update_dns_hostname = true;
	int ret, ret3, i;
	NTSTATUS status;

	status = dcesrv_netr_creds_server_step_check(dce_call,
						     mem_ctx,
						     r->in.computer_name,
						     r->in.credential,
						     r->out.return_authenticator,
						     &creds);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,(__location__ " Bad credentials - error\n"));
	}
	NT_STATUS_NOT_OK_RETURN(status);

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				system_session(dce_call->conn->dce_ctx->lp_ctx), 0);
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	switch (r->in.level) {
	case 1: /* Domain information */

		if (r->in.query->workstation_info == NULL) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		/* Prepares the workstation DN */
		workstation_dn = ldb_dn_new_fmt(mem_ctx, sam_ctx, "<SID=%s>",
						dom_sid_string(mem_ctx, creds->sid));
		NT_STATUS_HAVE_NO_MEMORY(workstation_dn);

		/* Lookup for attributes in workstation object */
		ret = gendb_search_dn(sam_ctx, mem_ctx, workstation_dn, &res1,
				      attrs2);
		if (ret != 1) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		/* Gets the sam account name which is checked against the DNS
		 * hostname parameter. */
		sam_account_name = ldb_msg_find_attr_as_string(res1[0],
							       "sAMAccountName",
							       NULL);
		if (sam_account_name == NULL) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		/*
		 * Checks that the sam account name without a possible "$"
		 * matches as prefix with the DNS hostname in the workstation
		 * info structure.
		 */
		prefix1 = talloc_strndup(mem_ctx, sam_account_name,
					 strcspn(sam_account_name, "$"));
		NT_STATUS_HAVE_NO_MEMORY(prefix1);
		if (r->in.query->workstation_info->dns_hostname != NULL) {
			prefix2 = talloc_strndup(mem_ctx,
						 r->in.query->workstation_info->dns_hostname,
						 strcspn(r->in.query->workstation_info->dns_hostname, "."));
			NT_STATUS_HAVE_NO_MEMORY(prefix2);

			if (strcasecmp(prefix1, prefix2) != 0) {
				update_dns_hostname = false;
			}
		} else {
			update_dns_hostname = false;
		}

		/* Gets the old DNS hostname */
		old_dns_hostname = ldb_msg_find_attr_as_string(res1[0],
							       "dNSHostName",
							       NULL);

		/*
		 * Updates the DNS hostname when the client wishes that the
		 * server should handle this for him
		 * ("NETR_WS_FLAG_HANDLES_SPN_UPDATE" not set). And this is
		 * obviously only checked when we do already have a
		 * "dNSHostName".
		 * See MS-NRPC section 3.5.4.3.9
		 */
		if ((old_dns_hostname != NULL) &&
		    (r->in.query->workstation_info->workstation_flags
		    & NETR_WS_FLAG_HANDLES_SPN_UPDATE) != 0) {
			update_dns_hostname = false;
		}

		/* Gets host information and put them into our directory */

		new_msg = ldb_msg_new(mem_ctx);
		NT_STATUS_HAVE_NO_MEMORY(new_msg);

		new_msg->dn = workstation_dn;

		/* Sets the OS name */

		if (r->in.query->workstation_info->os_name.string == NULL) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		ret = ldb_msg_add_string(new_msg, "operatingSystem",
					 r->in.query->workstation_info->os_name.string);
		if (ret != LDB_SUCCESS) {
			return NT_STATUS_NO_MEMORY;
		}

		/*
		 * Sets information from "os_version". On an empty structure
		 * the values are cleared.
		 */
		if (r->in.query->workstation_info->os_version.os != NULL) {
			struct netr_OsVersionInfoEx *os_version;
			const char *os_version_str;

			os_version = &r->in.query->workstation_info->os_version.os->os;

			if (os_version->CSDVersion == NULL) {
				return NT_STATUS_INVALID_PARAMETER;
			}

			os_version_str = talloc_asprintf(new_msg, "%u.%u (%u)",
							 os_version->MajorVersion,
							 os_version->MinorVersion,
							 os_version->BuildNumber);
			NT_STATUS_HAVE_NO_MEMORY(os_version_str);

			ret = ldb_msg_add_string(new_msg,
						 "operatingSystemServicePack",
						 os_version->CSDVersion);
			if (ret != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY;
			}

			ret = ldb_msg_add_string(new_msg,
						 "operatingSystemVersion",
						 os_version_str);
			if (ret != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY;
			}
		} else {
			ret = samdb_msg_add_delete(sam_ctx, mem_ctx, new_msg,
						   "operatingSystemServicePack");
			if (ret != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY;
			}

			ret = samdb_msg_add_delete(sam_ctx, mem_ctx, new_msg,
						   "operatingSystemVersion");
			if (ret != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY;
			}
		}

		/*
		 * If the boolean "update_dns_hostname" remained true, then we
		 * are fine to start the update.
		 */
		if (update_dns_hostname) {
			ret = ldb_msg_add_string(new_msg,
						 "dNSHostname",
						 r->in.query->workstation_info->dns_hostname);
			if (ret != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY;
			}

			/* This manual "servicePrincipalName" generation is
			 * still needed! Since the update in the samldb LDB
			 * module does only work if the entries already exist
			 * which isn't always the case. */
			ret = ldb_msg_add_string(new_msg,
						 "servicePrincipalName",
						 talloc_asprintf(new_msg, "HOST/%s",
						 r->in.computer_name));
			if (ret != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY;
			}

			ret = ldb_msg_add_string(new_msg,
						 "servicePrincipalName",
						 talloc_asprintf(new_msg, "HOST/%s",
						 r->in.query->workstation_info->dns_hostname));
			if (ret != LDB_SUCCESS) {
				return NT_STATUS_NO_MEMORY;
			}
		}

		if (dsdb_replace(sam_ctx, new_msg, 0) != LDB_SUCCESS) {
			DEBUG(3,("Impossible to update samdb: %s\n",
				ldb_errstring(sam_ctx)));
		}

		talloc_free(new_msg);

		/* Writes back the domain information */

		/* We need to do two searches. The first will pull our primary
		   domain and the second will pull any trusted domains. Our
		   primary domain is also a "trusted" domain, so we need to
		   put the primary domain into the lists of returned trusts as
		   well. */
		ret = gendb_search_dn(sam_ctx, mem_ctx, ldb_get_default_basedn(sam_ctx),
			&res2, attrs);
		if (ret != 1) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		ret3 = gendb_search(sam_ctx, mem_ctx, NULL, &res3, attrs,
			"(objectClass=trustedDomain)");
		if (ret3 == -1) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		domain_info = talloc(mem_ctx, struct netr_DomainInformation);
		NT_STATUS_HAVE_NO_MEMORY(domain_info);

		ZERO_STRUCTP(domain_info);

		/* Informations about the local and trusted domains */

		status = fill_one_domain_info(mem_ctx,
			dce_call->conn->dce_ctx->lp_ctx,
			sam_ctx, res2[0], &domain_info->primary_domain,
			true, false);
		NT_STATUS_NOT_OK_RETURN(status);

		domain_info->trusted_domain_count = ret3 + 1;
		domain_info->trusted_domains = talloc_array(mem_ctx,
			struct netr_OneDomainInfo,
			domain_info->trusted_domain_count);
		NT_STATUS_HAVE_NO_MEMORY(domain_info->trusted_domains);

		for (i=0;i<ret3;i++) {
			status = fill_one_domain_info(mem_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				sam_ctx, res3[i],
				&domain_info->trusted_domains[i],
				false, true);
			NT_STATUS_NOT_OK_RETURN(status);
		}

		status = fill_one_domain_info(mem_ctx,
			dce_call->conn->dce_ctx->lp_ctx, sam_ctx, res2[0],
			&domain_info->trusted_domains[i], true, true);
		NT_STATUS_NOT_OK_RETURN(status);

		/* Sets the supported encryption types */
		domain_info->supported_enc_types = ldb_msg_find_attr_as_uint(res1[0],
			"msDS-SupportedEncryptionTypes",
			default_supported_enc_types);

		/* Other host domain information */

		lsa_policy_info = talloc(mem_ctx,
			struct netr_LsaPolicyInformation);
		NT_STATUS_HAVE_NO_MEMORY(lsa_policy_info);
		ZERO_STRUCTP(lsa_policy_info);

		domain_info->lsa_policy = *lsa_policy_info;

		/* The DNS hostname is only returned back when there is a chance
		 * for a change. */
		if ((r->in.query->workstation_info->workstation_flags
		    & NETR_WS_FLAG_HANDLES_SPN_UPDATE) != 0) {
			domain_info->dns_hostname.string = old_dns_hostname;
		} else {
			domain_info->dns_hostname.string = NULL;
		}

		domain_info->workstation_flags =
			r->in.query->workstation_info->workstation_flags;

		r->out.info->domain_info = domain_info;
	break;
	case 2: /* LSA policy information - not used at the moment */
		lsa_policy_info = talloc(mem_ctx,
			struct netr_LsaPolicyInformation);
		NT_STATUS_HAVE_NO_MEMORY(lsa_policy_info);
		ZERO_STRUCTP(lsa_policy_info);

		r->out.info->lsa_policy_info = lsa_policy_info;
	break;
	default:
		return NT_STATUS_INVALID_LEVEL;
	break;
	}

	return NT_STATUS_OK;
}


/*
  netr_ServerPasswordGet
*/
static WERROR dcesrv_netr_ServerPasswordGet(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_ServerPasswordGet *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_NETRLOGONSENDTOSAM
*/
static WERROR dcesrv_netr_NETRLOGONSENDTOSAM(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_NETRLOGONSENDTOSAM *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_DsRGetDCNameEx2
*/
static WERROR dcesrv_netr_DsRGetDCNameEx2(struct dcesrv_call_state *dce_call,
					  TALLOC_CTX *mem_ctx,
					  struct netr_DsRGetDCNameEx2 *r)
{
	struct ldb_context *sam_ctx;
	struct netr_DsRGetDCNameInfo *info;
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;
	const struct tsocket_address *remote_address;
	char *addr = NULL;
	const char *server_site_name;
	char *guid_str;
	struct netlogon_samlogon_response response;
	NTSTATUS status;
	const char *dc_name = NULL;
	const char *domain_name = NULL;

	ZERO_STRUCTP(r->out.info);

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return WERR_DS_UNAVAILABLE;
	}

	remote_address = dcesrv_connection_get_remote_address(dce_call->conn);
	if (tsocket_address_is_inet(remote_address, "ip")) {
		addr = tsocket_address_inet_addr_string(remote_address, mem_ctx);
		W_ERROR_HAVE_NO_MEMORY(addr);
	}

	/* "server_unc" is ignored by w2k3 */

	if (r->in.flags & ~(DSGETDC_VALID_FLAGS)) {
		return WERR_INVALID_FLAGS;
	}

	if (r->in.flags & DS_GC_SERVER_REQUIRED &&
	    r->in.flags & DS_PDC_REQUIRED &&
	    r->in.flags & DS_KDC_REQUIRED) {
		return WERR_INVALID_FLAGS;
	}
	if (r->in.flags & DS_IS_FLAT_NAME &&
	    r->in.flags & DS_IS_DNS_NAME) {
		return WERR_INVALID_FLAGS;
	}
	if (r->in.flags & DS_RETURN_DNS_NAME &&
	    r->in.flags & DS_RETURN_FLAT_NAME) {
		return WERR_INVALID_FLAGS;
	}
	if (r->in.flags & DS_DIRECTORY_SERVICE_REQUIRED &&
	    r->in.flags & DS_DIRECTORY_SERVICE_6_REQUIRED) {
		return WERR_INVALID_FLAGS;
	}

	if (r->in.flags & DS_GOOD_TIMESERV_PREFERRED &&
	    r->in.flags &
	    (DS_DIRECTORY_SERVICE_REQUIRED |
	     DS_DIRECTORY_SERVICE_PREFERRED |
	     DS_GC_SERVER_REQUIRED |
	     DS_PDC_REQUIRED |
	     DS_KDC_REQUIRED)) {
		return WERR_INVALID_FLAGS;
	}

	if (r->in.flags & DS_TRY_NEXTCLOSEST_SITE &&
	    r->in.site_name) {
		return WERR_INVALID_FLAGS;
	}

	/* Proof server site parameter "site_name" if it was specified */
	server_site_name = samdb_server_site_name(sam_ctx, mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(server_site_name);
	if ((r->in.site_name != NULL) && (strcasecmp(r->in.site_name,
						     server_site_name) != 0)) {
		return WERR_NO_SUCH_DOMAIN;
	}

	guid_str = r->in.domain_guid != NULL ?
		 GUID_string(mem_ctx, r->in.domain_guid) : NULL;

	status = fill_netlogon_samlogon_response(sam_ctx, mem_ctx,
						 r->in.domain_name,
						 r->in.domain_name,
						 NULL, guid_str,
						 r->in.client_account,
						 r->in.mask, addr,
						 NETLOGON_NT_VERSION_5EX_WITH_IP,
						 lp_ctx, &response, true);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (r->in.flags & DS_RETURN_DNS_NAME) {
		dc_name = response.data.nt5_ex.pdc_dns_name;
		domain_name = response.data.nt5_ex.dns_domain;
	} else if (r->in.flags & DS_RETURN_FLAT_NAME) {
		dc_name = response.data.nt5_ex.pdc_name;
		domain_name = response.data.nt5_ex.domain_name;
	} else {

		/*
		 * TODO: autodetect what we need to return
		 * based on the given arguments
		 */
		dc_name = response.data.nt5_ex.pdc_name;
		domain_name = response.data.nt5_ex.domain_name;
	}

	if (!dc_name || !dc_name[0]) {
		return WERR_NO_SUCH_DOMAIN;
	}

	if (!domain_name || !domain_name[0]) {
		return WERR_NO_SUCH_DOMAIN;
	}

	info = talloc(mem_ctx, struct netr_DsRGetDCNameInfo);
	W_ERROR_HAVE_NO_MEMORY(info);
	info->dc_unc           = talloc_asprintf(mem_ctx, "\\\\%s", dc_name);
	W_ERROR_HAVE_NO_MEMORY(info->dc_unc);
	info->dc_address = talloc_asprintf(mem_ctx, "\\\\%s",
					   response.data.nt5_ex.sockaddr.pdc_ip);
	W_ERROR_HAVE_NO_MEMORY(info->dc_address);
	info->dc_address_type  = DS_ADDRESS_TYPE_INET; /* TODO: make this dynamic? for ipv6 */
	info->domain_guid      = response.data.nt5_ex.domain_uuid;
	info->domain_name      = domain_name;
	info->forest_name      = response.data.nt5_ex.forest;
	info->dc_flags         = response.data.nt5_ex.server_type;
	info->dc_site_name     = response.data.nt5_ex.server_site;
	info->client_site_name = response.data.nt5_ex.client_site;

	*r->out.info = info;

	return WERR_OK;
}

/*
  netr_DsRGetDCNameEx
*/
static WERROR dcesrv_netr_DsRGetDCNameEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				  struct netr_DsRGetDCNameEx *r)
{
	struct netr_DsRGetDCNameEx2 r2;
	WERROR werr;

	ZERO_STRUCT(r2);

	r2.in.server_unc = r->in.server_unc;
	r2.in.client_account = NULL;
	r2.in.mask = 0;
	r2.in.domain_guid = r->in.domain_guid;
	r2.in.domain_name = r->in.domain_name;
	r2.in.site_name = r->in.site_name;
	r2.in.flags = r->in.flags;
	r2.out.info = r->out.info;

	werr = dcesrv_netr_DsRGetDCNameEx2(dce_call, mem_ctx, &r2);

	return werr;
}

/*
  netr_DsRGetDCName
*/
static WERROR dcesrv_netr_DsRGetDCName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				struct netr_DsRGetDCName *r)
{
	struct netr_DsRGetDCNameEx2 r2;
	WERROR werr;

	ZERO_STRUCT(r2);

	r2.in.server_unc = r->in.server_unc;
	r2.in.client_account = NULL;
	r2.in.mask = 0;
	r2.in.domain_name = r->in.domain_name;
	r2.in.domain_guid = r->in.domain_guid;

	r2.in.site_name = NULL; /* this is correct, we should ignore site GUID */
	r2.in.flags = r->in.flags;
	r2.out.info = r->out.info;

	werr = dcesrv_netr_DsRGetDCNameEx2(dce_call, mem_ctx, &r2);

	return werr;
}
/*
  netr_NETRLOGONGETTIMESERVICEPARENTDOMAIN
*/
static WERROR dcesrv_netr_NETRLOGONGETTIMESERVICEPARENTDOMAIN(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_NETRLOGONGETTIMESERVICEPARENTDOMAIN *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_NetrEnumerateTrustedDomainsEx
*/
static WERROR dcesrv_netr_NetrEnumerateTrustedDomainsEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_NetrEnumerateTrustedDomainsEx *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_DsRAddressToSitenamesExW
*/
static WERROR dcesrv_netr_DsRAddressToSitenamesExW(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
						   struct netr_DsRAddressToSitenamesExW *r)
{
	struct ldb_context *sam_ctx;
	struct netr_DsRAddressToSitenamesExWCtr *ctr;
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;
	sa_family_t sin_family;
	struct sockaddr_in *addr;
#ifdef HAVE_IPV6
	struct sockaddr_in6 *addr6;
	char addr_str[INET6_ADDRSTRLEN];
#else
	char addr_str[INET_ADDRSTRLEN];
#endif
	char *subnet_name;
	const char *res;
	uint32_t i;

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return WERR_DS_UNAVAILABLE;
	}

	ctr = talloc(mem_ctx, struct netr_DsRAddressToSitenamesExWCtr);
	W_ERROR_HAVE_NO_MEMORY(ctr);

	*r->out.ctr = ctr;

	ctr->count = r->in.count;
	ctr->sitename = talloc_array(ctr, struct lsa_String, ctr->count);
	W_ERROR_HAVE_NO_MEMORY(ctr->sitename);
	ctr->subnetname = talloc_array(ctr, struct lsa_String, ctr->count);
	W_ERROR_HAVE_NO_MEMORY(ctr->subnetname);

	for (i=0; i<ctr->count; i++) {
		ctr->sitename[i].string = NULL;
		ctr->subnetname[i].string = NULL;

		if (r->in.addresses[i].size < sizeof(sa_family_t)) {
			continue;
		}
		/* The first two byte of the buffer are reserved for the
		 * "sin_family" but for now only the first one is used. */
		sin_family = r->in.addresses[i].buffer[0];

		switch (sin_family) {
		case AF_INET:
			if (r->in.addresses[i].size < sizeof(struct sockaddr_in)) {
				continue;
			}
			addr = (struct sockaddr_in *) r->in.addresses[i].buffer;
			res = inet_ntop(AF_INET, &addr->sin_addr,
					addr_str, sizeof(addr_str));
			break;
#ifdef HAVE_IPV6
		case AF_INET6:
			if (r->in.addresses[i].size < sizeof(struct sockaddr_in6)) {
				continue;
			}
			addr6 = (struct sockaddr_in6 *) r->in.addresses[i].buffer;
			res = inet_ntop(AF_INET6, &addr6->sin6_addr,
					addr_str, sizeof(addr_str));
			break;
#endif
		default:
			continue;
		}

		if (res == NULL) {
			continue;
		}

		ctr->sitename[i].string   = samdb_client_site_name(sam_ctx,
								   mem_ctx,
								   addr_str,
								   &subnet_name);
		W_ERROR_HAVE_NO_MEMORY(ctr->sitename[i].string);
		ctr->subnetname[i].string = subnet_name;
	}

	return WERR_OK;
}


/*
  netr_DsRAddressToSitenamesW
*/
static WERROR dcesrv_netr_DsRAddressToSitenamesW(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DsRAddressToSitenamesW *r)
{
	struct netr_DsRAddressToSitenamesExW r2;
	struct netr_DsRAddressToSitenamesWCtr *ctr;
	uint32_t i;
	WERROR werr;

	ZERO_STRUCT(r2);

	r2.in.server_name = r->in.server_name;
	r2.in.count = r->in.count;
	r2.in.addresses = r->in.addresses;

	r2.out.ctr = talloc(mem_ctx, struct netr_DsRAddressToSitenamesExWCtr *);
	W_ERROR_HAVE_NO_MEMORY(r2.out.ctr);

	ctr = talloc(mem_ctx, struct netr_DsRAddressToSitenamesWCtr);
	W_ERROR_HAVE_NO_MEMORY(ctr);

	*r->out.ctr = ctr;

	ctr->count = r->in.count;
	ctr->sitename = talloc_array(ctr, struct lsa_String, ctr->count);
	W_ERROR_HAVE_NO_MEMORY(ctr->sitename);

	werr = dcesrv_netr_DsRAddressToSitenamesExW(dce_call, mem_ctx, &r2);

	for (i=0; i<ctr->count; i++) {
		ctr->sitename[i].string   = (*r2.out.ctr)->sitename[i].string;
	}

	return werr;
}


/*
  netr_DsrGetDcSiteCoverageW
*/
static WERROR dcesrv_netr_DsrGetDcSiteCoverageW(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DsrGetDcSiteCoverageW *r)
{
	struct ldb_context *sam_ctx;
	struct DcSitesCtr *ctr;
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return WERR_DS_UNAVAILABLE;
	}

	ctr = talloc(mem_ctx, struct DcSitesCtr);
	W_ERROR_HAVE_NO_MEMORY(ctr);

	*r->out.ctr = ctr;

	/* For now only return our default site */
	ctr->num_sites = 1;
	ctr->sites = talloc_array(ctr, struct lsa_String, ctr->num_sites);
	W_ERROR_HAVE_NO_MEMORY(ctr->sites);
	ctr->sites[0].string = samdb_server_site_name(sam_ctx, mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(ctr->sites[0].string);

	return WERR_OK;
}


#define GET_CHECK_STR(dest, mem, msg, attr) \
do {\
	const char *s; \
	s = ldb_msg_find_attr_as_string(msg, attr, NULL); \
	if (!s) { \
		DEBUG(0, ("DB Error, TustedDomain entry (%s) " \
			  "without flatname\n", \
			  ldb_dn_get_linearized(msg->dn))); \
		continue; \
	} \
	dest = talloc_strdup(mem, s); \
	W_ERROR_HAVE_NO_MEMORY(dest); \
} while(0)


static WERROR fill_trusted_domains_array(TALLOC_CTX *mem_ctx,
					 struct ldb_context *sam_ctx,
					 struct netr_DomainTrustList *trusts,
					 uint32_t trust_flags)
{
	struct ldb_dn *system_dn;
	struct ldb_message **dom_res = NULL;
	const char *trust_attrs[] = { "flatname", "trustPartner",
				      "securityIdentifier", "trustDirection",
				      "trustType", "trustAttributes", NULL };
	uint32_t n;
	int i;
	int ret;

	if (!(trust_flags & (NETR_TRUST_FLAG_INBOUND |
			     NETR_TRUST_FLAG_OUTBOUND))) {
		return WERR_INVALID_FLAGS;
	}

	system_dn = samdb_search_dn(sam_ctx, mem_ctx,
				    ldb_get_default_basedn(sam_ctx),
				    "(&(objectClass=container)(cn=System))");
	if (!system_dn) {
		return WERR_GENERAL_FAILURE;
	}

	ret = gendb_search(sam_ctx, mem_ctx, system_dn,
			   &dom_res, trust_attrs,
			   "(objectclass=trustedDomain)");

	for (i = 0; i < ret; i++) {
		unsigned int trust_dir;
		uint32_t flags = 0;

		trust_dir = ldb_msg_find_attr_as_uint(dom_res[i],
						      "trustDirection", 0);

		if (trust_dir & LSA_TRUST_DIRECTION_INBOUND) {
			flags |= NETR_TRUST_FLAG_INBOUND;
		}
		if (trust_dir & LSA_TRUST_DIRECTION_OUTBOUND) {
			flags |= NETR_TRUST_FLAG_OUTBOUND;
		}

		if (!(flags & trust_flags)) {
			/* this trust direction was not requested */
			continue;
		}

		n = trusts->count;
		trusts->array = talloc_realloc(trusts, trusts->array,
					       struct netr_DomainTrust,
					       n + 1);
		W_ERROR_HAVE_NO_MEMORY(trusts->array);

		GET_CHECK_STR(trusts->array[n].netbios_name, trusts,
			      dom_res[i], "flatname");
		GET_CHECK_STR(trusts->array[n].dns_name, trusts,
			      dom_res[i], "trustPartner");

		trusts->array[n].trust_flags = flags;
		if ((trust_flags & NETR_TRUST_FLAG_IN_FOREST) &&
		    !(flags & NETR_TRUST_FLAG_TREEROOT)) {
			/* TODO: find if we have parent in the list */
			trusts->array[n].parent_index = 0;
		}

		trusts->array[n].trust_type =
				ldb_msg_find_attr_as_uint(dom_res[i],
						  "trustType", 0);
		trusts->array[n].trust_attributes =
				ldb_msg_find_attr_as_uint(dom_res[i],
						  "trustAttributes", 0);

		if ((trusts->array[n].trust_type == NETR_TRUST_TYPE_MIT) ||
		    (trusts->array[n].trust_type == NETR_TRUST_TYPE_DCE)) {
			struct dom_sid zero_sid;
			ZERO_STRUCT(zero_sid);
			trusts->array[n].sid =
				dom_sid_dup(trusts, &zero_sid);
		} else {
			trusts->array[n].sid =
				samdb_result_dom_sid(trusts, dom_res[i],
						     "securityIdentifier");
		}
		trusts->array[n].guid = GUID_zero();

		trusts->count = n + 1;
	}

	talloc_free(dom_res);
	return WERR_OK;
}

/*
  netr_DsrEnumerateDomainTrusts
*/
static WERROR dcesrv_netr_DsrEnumerateDomainTrusts(struct dcesrv_call_state *dce_call,
						   TALLOC_CTX *mem_ctx,
						   struct netr_DsrEnumerateDomainTrusts *r)
{
	struct netr_DomainTrustList *trusts;
	struct ldb_context *sam_ctx;
	int ret;
	struct ldb_message **dom_res;
	const char * const dom_attrs[] = { "objectSid", "objectGUID", NULL };
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;
	const char *dnsdomain = lpcfg_dnsdomain(lp_ctx);
	const char *p;
	WERROR werr;

	if (r->in.trust_flags & 0xFFFFFE00) {
		return WERR_INVALID_FLAGS;
	}

	/* TODO: turn to hard check once we are sure this is 100% correct */
	if (!r->in.server_name) {
		DEBUG(3, ("Invalid domain! Expected name in domain [%s]. "
			  "But received NULL!\n", dnsdomain));
	} else {
		p = strchr(r->in.server_name, '.');
		if (!p) {
			DEBUG(3, ("Invalid domain! Expected name in domain "
				  "[%s]. But received [%s]!\n",
				  dnsdomain, r->in.server_name));
			p = r->in.server_name;
		} else {
			p++;
                }
	        if (strcasecmp(p, dnsdomain)) {
			DEBUG(3, ("Invalid domain! Expected name in domain "
				  "[%s]. But received [%s]!\n",
				  dnsdomain, r->in.server_name));
		}
	}

	trusts = talloc_zero(mem_ctx, struct netr_DomainTrustList);
	W_ERROR_HAVE_NO_MEMORY(trusts);

	trusts->count = 0;
	r->out.trusts = trusts;

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return WERR_GENERAL_FAILURE;
	}

	if ((r->in.trust_flags & NETR_TRUST_FLAG_INBOUND) ||
	    (r->in.trust_flags & NETR_TRUST_FLAG_OUTBOUND)) {

		werr = fill_trusted_domains_array(mem_ctx, sam_ctx,
						  trusts, r->in.trust_flags);
		W_ERROR_NOT_OK_RETURN(werr);
	}

	/* NOTE: we currently are always the root of the forest */
	if (r->in.trust_flags & NETR_TRUST_FLAG_IN_FOREST) {
		uint32_t n = trusts->count;

		ret = gendb_search_dn(sam_ctx, mem_ctx, NULL,
				      &dom_res, dom_attrs);
		if (ret != 1) {
			return WERR_GENERAL_FAILURE;
		}

		trusts->count = n + 1;
		trusts->array = talloc_realloc(trusts, trusts->array,
					       struct netr_DomainTrust,
					       trusts->count);
		W_ERROR_HAVE_NO_MEMORY(trusts->array);

		trusts->array[n].netbios_name = lpcfg_workgroup(lp_ctx);
		trusts->array[n].dns_name = lpcfg_dnsdomain(lp_ctx);
		trusts->array[n].trust_flags =
			NETR_TRUST_FLAG_NATIVE |
			NETR_TRUST_FLAG_TREEROOT |
			NETR_TRUST_FLAG_IN_FOREST |
			NETR_TRUST_FLAG_PRIMARY;
		/* we are always the root domain for now */
		trusts->array[n].parent_index = 0;
		trusts->array[n].trust_type = NETR_TRUST_TYPE_UPLEVEL;
		trusts->array[n].trust_attributes = 0;
		trusts->array[n].sid = samdb_result_dom_sid(mem_ctx,
							    dom_res[0],
							    "objectSid");
		trusts->array[n].guid = samdb_result_guid(dom_res[0],
							  "objectGUID");
		talloc_free(dom_res);
	}

	return WERR_OK;
}


/*
  netr_DsrDeregisterDNSHostRecords
*/
static WERROR dcesrv_netr_DsrDeregisterDNSHostRecords(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DsrDeregisterDNSHostRecords *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_ServerTrustPasswordsGet
*/
static NTSTATUS dcesrv_netr_ServerTrustPasswordsGet(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_ServerTrustPasswordsGet *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


static WERROR fill_forest_trust_array(TALLOC_CTX *mem_ctx,
				      struct ldb_context *sam_ctx,
				      struct loadparm_context *lp_ctx,
				      struct lsa_ForestTrustInformation *info)
{
	struct lsa_ForestTrustDomainInfo *domain_info;
	struct lsa_ForestTrustRecord *e;
	struct ldb_message **dom_res;
	const char * const dom_attrs[] = { "objectSid", NULL };
	int ret;

	/* we need to provide 2 entries:
	 * 1. the Root Forest name
	 * 2. the Domain Information
	 */

	info->count = 2;
	info->entries = talloc_array(info, struct lsa_ForestTrustRecord *, 2);
	W_ERROR_HAVE_NO_MEMORY(info->entries);

	/* Forest root info */
	e = talloc(info, struct lsa_ForestTrustRecord);
	W_ERROR_HAVE_NO_MEMORY(e);

	e->flags = 0;
	e->type = LSA_FOREST_TRUST_TOP_LEVEL_NAME;
	e->time = 0; /* so far always 0 in trces. */
	e->forest_trust_data.top_level_name.string = samdb_forest_name(sam_ctx,
								       mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(e->forest_trust_data.top_level_name.string);

	info->entries[0] = e;

	/* Domain info */
	e = talloc(info, struct lsa_ForestTrustRecord);
	W_ERROR_HAVE_NO_MEMORY(e);

	/* get our own domain info */
	ret = gendb_search_dn(sam_ctx, mem_ctx, NULL, &dom_res, dom_attrs);
	if (ret != 1) {
		return WERR_GENERAL_FAILURE;
	}

	/* TODO: check if disabled and set flags accordingly */
	e->flags = 0;
	e->type = LSA_FOREST_TRUST_DOMAIN_INFO;
	e->time = 0; /* so far always 0 in traces. */

	domain_info = &e->forest_trust_data.domain_info;
	domain_info->domain_sid = samdb_result_dom_sid(info, dom_res[0],
						       "objectSid");
	domain_info->dns_domain_name.string = lpcfg_dnsdomain(lp_ctx);
	domain_info->netbios_domain_name.string = lpcfg_workgroup(lp_ctx);

	info->entries[1] = e;

	talloc_free(dom_res);

	return WERR_OK;
}

/*
  netr_DsRGetForestTrustInformation
*/
static WERROR dcesrv_netr_DsRGetForestTrustInformation(struct dcesrv_call_state *dce_call,
						       TALLOC_CTX *mem_ctx,
						       struct netr_DsRGetForestTrustInformation *r)
{
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;
	struct lsa_ForestTrustInformation *info, **info_ptr;
	struct ldb_context *sam_ctx;
	WERROR werr;

	if (r->in.flags & 0xFFFFFFFE) {
		return WERR_INVALID_FLAGS;
	}

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return WERR_GENERAL_FAILURE;
	}

	if (r->in.flags & DS_GFTI_UPDATE_TDO) {
		if (!samdb_is_pdc(sam_ctx)) {
			return WERR_NERR_NOTPRIMARY;
		}

		if (r->in.trusted_domain_name == NULL) {
			return WERR_INVALID_FLAGS;
		}

		/* TODO: establish an schannel connection with
		 * r->in.trusted_domain_name and perform a
		 * netr_GetForestTrustInformation call against it */

		/* for now return not implementd */
		return WERR_CALL_NOT_IMPLEMENTED;
	}

	/* TODO: check r->in.server_name is our name */

	info_ptr = talloc(mem_ctx, struct lsa_ForestTrustInformation *);
	W_ERROR_HAVE_NO_MEMORY(info_ptr);

	info = talloc_zero(info_ptr, struct lsa_ForestTrustInformation);
	W_ERROR_HAVE_NO_MEMORY(info);

	werr = fill_forest_trust_array(mem_ctx, sam_ctx, lp_ctx, info);
	W_ERROR_NOT_OK_RETURN(werr);

	*info_ptr = info;
	r->out.forest_trust_info = info_ptr;

	return WERR_OK;
}


/*
  netr_GetForestTrustInformation
*/
static NTSTATUS dcesrv_netr_GetForestTrustInformation(struct dcesrv_call_state *dce_call,
						      TALLOC_CTX *mem_ctx,
						      struct netr_GetForestTrustInformation *r)
{
	struct loadparm_context *lp_ctx = dce_call->conn->dce_ctx->lp_ctx;
	struct netlogon_creds_CredentialState *creds;
	struct lsa_ForestTrustInformation *info, **info_ptr;
	struct ldb_context *sam_ctx;
	NTSTATUS status;
	WERROR werr;

	status = dcesrv_netr_creds_server_step_check(dce_call,
						     mem_ctx,
						     r->in.computer_name,
						     r->in.credential,
						     r->out.return_authenticator,
						     &creds);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if ((creds->secure_channel_type != SEC_CHAN_DNS_DOMAIN) &&
	    (creds->secure_channel_type != SEC_CHAN_DOMAIN)) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, lp_ctx,
				dce_call->conn->auth_state.session_info, 0);
	if (sam_ctx == NULL) {
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* TODO: check r->in.server_name is our name */

	info_ptr = talloc(mem_ctx, struct lsa_ForestTrustInformation *);
	if (!info_ptr) {
		return NT_STATUS_NO_MEMORY;
	}
	info = talloc_zero(info_ptr, struct lsa_ForestTrustInformation);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}

	werr = fill_forest_trust_array(mem_ctx, sam_ctx, lp_ctx, info);
	if (!W_ERROR_IS_OK(werr)) {
		return werror_to_ntstatus(werr);
	}

	*info_ptr = info;
	r->out.forest_trust_info = info_ptr;

	return NT_STATUS_OK;
}


/*
  netr_ServerGetTrustInfo
*/
static NTSTATUS dcesrv_netr_ServerGetTrustInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_ServerGetTrustInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}

/*
  netr_Unused47
*/
static NTSTATUS dcesrv_netr_Unused47(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct netr_Unused47 *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


struct netr_dnsupdate_RODC_state {
	struct dcesrv_call_state *dce_call;
	struct netr_DsrUpdateReadOnlyServerDnsRecords *r;
	struct dnsupdate_RODC *r2;
};

/*
  called when the forwarded RODC dns update request is finished
 */
static void netr_dnsupdate_RODC_callback(struct tevent_req *subreq)
{
	struct netr_dnsupdate_RODC_state *st =
		tevent_req_callback_data(subreq,
					 struct netr_dnsupdate_RODC_state);
	NTSTATUS status;

	status = dcerpc_dnsupdate_RODC_r_recv(subreq, st->dce_call);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,(__location__ ": IRPC callback failed %s\n", nt_errstr(status)));
		st->dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
	}

	st->r->out.dns_names = talloc_steal(st->dce_call, st->r2->out.dns_names);

	status = dcesrv_reply(st->dce_call);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,(__location__ ": dcesrv_reply() failed - %s\n", nt_errstr(status)));
	}
}

/*
  netr_DsrUpdateReadOnlyServerDnsRecords
*/
static NTSTATUS dcesrv_netr_DsrUpdateReadOnlyServerDnsRecords(struct dcesrv_call_state *dce_call,
							      TALLOC_CTX *mem_ctx,
							      struct netr_DsrUpdateReadOnlyServerDnsRecords *r)
{
	struct netlogon_creds_CredentialState *creds;
	NTSTATUS nt_status;
	struct dcerpc_binding_handle *binding_handle;
	struct netr_dnsupdate_RODC_state *st;
	struct tevent_req *subreq;

	nt_status = dcesrv_netr_creds_server_step_check(dce_call,
							mem_ctx,
							r->in.computer_name,
							r->in.credential,
							r->out.return_authenticator,
							&creds);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	if (creds->secure_channel_type != SEC_CHAN_RODC) {
		return NT_STATUS_ACCESS_DENIED;
	}

	st = talloc_zero(mem_ctx, struct netr_dnsupdate_RODC_state);
	NT_STATUS_HAVE_NO_MEMORY(st);

	st->dce_call = dce_call;
	st->r = r;
	st->r2 = talloc_zero(st, struct dnsupdate_RODC);
	NT_STATUS_HAVE_NO_MEMORY(st->r2);

	st->r2->in.dom_sid = creds->sid;
	st->r2->in.site_name = r->in.site_name;
	st->r2->in.dns_ttl = r->in.dns_ttl;
	st->r2->in.dns_names = r->in.dns_names;
	st->r2->out.dns_names = r->out.dns_names;

	binding_handle = irpc_binding_handle_by_name(st, dce_call->msg_ctx,
						     "dnsupdate", &ndr_table_irpc);
	if (binding_handle == NULL) {
		DEBUG(0,("Failed to get binding_handle for dnsupdate task\n"));
		dce_call->fault_code = DCERPC_FAULT_CANT_PERFORM;
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* forward the call */
	subreq = dcerpc_dnsupdate_RODC_r_send(st, dce_call->event_ctx,
					      binding_handle, st->r2);
	NT_STATUS_HAVE_NO_MEMORY(subreq);

	dce_call->state_flags |= DCESRV_CALL_STATE_FLAG_ASYNC;

	/* setup the callback */
	tevent_req_set_callback(subreq, netr_dnsupdate_RODC_callback, st);

	return NT_STATUS_OK;
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_netlogon_s.c"
