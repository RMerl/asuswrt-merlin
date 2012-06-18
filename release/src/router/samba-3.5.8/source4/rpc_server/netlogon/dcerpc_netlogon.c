/* 
   Unix SMB/CIFS implementation.

   endpoint server for the netlogon pipe

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2004-2008
   Copyright (C) Stefan Metzmacher <metze@samba.org>  2005
   Copyright (C) Matthias Dieter Wallnöfer            2009
   
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
#include "auth/gensec/schannel_state.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "lib/messaging/irpc.h"
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
	struct ldb_context *schannel_ldb;
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

	if (!pipe_state) {
		DEBUG(1, ("No challenge requested by client, cannot authenticate\n"));
		return NT_STATUS_ACCESS_DENIED;
	}

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, 
				system_session(mem_ctx, dce_call->conn->dce_ctx->lp_ctx));
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
			return NT_STATUS_ACCESS_DENIED;
		}
		
		if (num_records > 1) {
			DEBUG(0,("Found %d records matching user [%s]\n", num_records, r->in.account_name));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
		
		flatname = ldb_msg_find_attr_as_string(msgs[0], "flatname", NULL);
		if (!flatname) {
			/* No flatname for this trust - we can't proceed */
			return NT_STATUS_ACCESS_DENIED;
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
		return NT_STATUS_ACCESS_DENIED;
	}

	if (num_records > 1) {
		DEBUG(0,("Found %d records matching user [%s]\n", num_records, r->in.account_name));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	
	user_account_control = ldb_msg_find_attr_as_uint(msgs[0], "userAccountControl", 0);

	if (user_account_control & UF_ACCOUNTDISABLE) {
		DEBUG(1, ("Account [%s] is disabled\n", r->in.account_name));
		return NT_STATUS_ACCESS_DENIED;
	}

	if (r->in.secure_channel_type == SEC_CHAN_WKSTA) {
		if (!(user_account_control & UF_WORKSTATION_TRUST_ACCOUNT)) {
			DEBUG(1, ("Client asked for a workstation secure channel, but is not a workstation (member server) acb flags: 0x%x\n", user_account_control));
			return NT_STATUS_ACCESS_DENIED;
		}
	} else if (r->in.secure_channel_type == SEC_CHAN_DOMAIN || 
		   r->in.secure_channel_type == SEC_CHAN_DNS_DOMAIN) {
		if (!(user_account_control & UF_INTERDOMAIN_TRUST_ACCOUNT)) {
			DEBUG(1, ("Client asked for a trusted domain secure channel, but is not a trusted domain: acb flags: 0x%x\n", user_account_control));
			
			return NT_STATUS_ACCESS_DENIED;
		}
	} else if (r->in.secure_channel_type == SEC_CHAN_BDC) {
		if (!(user_account_control & UF_SERVER_TRUST_ACCOUNT)) {
			DEBUG(1, ("Client asked for a server secure channel, but is not a server (domain controller): acb flags: 0x%x\n", user_account_control));
			return NT_STATUS_ACCESS_DENIED;
		}
	} else {
		DEBUG(1, ("Client asked for an invalid secure channel type: %d\n", 
			  r->in.secure_channel_type));
		return NT_STATUS_ACCESS_DENIED;
	}

	*r->out.rid = samdb_result_rid_from_sid(mem_ctx, msgs[0], 
						"objectSid", 0);

	mach_pwd = samdb_result_hash(mem_ctx, msgs[0], "unicodePwd");
	if (mach_pwd == NULL) {
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

	schannel_ldb = schannel_db_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx);
	if (!schannel_ldb) {
		return NT_STATUS_ACCESS_DENIED;
	}

	nt_status = schannel_store_session_key_ldb(schannel_ldb, mem_ctx, creds);
	talloc_free(schannel_ldb);

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
  Validate an incoming authenticator against the credentials for the remote machine.

  The credentials are (re)read and from the schannel database, and
  written back after the caclulations are performed.

  The creds_out parameter (if not NULL) returns the credentials, if
  the caller needs some of that information.

*/
static NTSTATUS dcesrv_netr_creds_server_step_check(struct dcesrv_call_state *dce_call,
						    TALLOC_CTX *mem_ctx, 
						    const char *computer_name,
						    struct netr_Authenticator *received_authenticator,
						    struct netr_Authenticator *return_authenticator,
						    struct netlogon_creds_CredentialState **creds_out) 
{
	NTSTATUS nt_status;
	struct ldb_context *ldb;
	bool schannel_global_required = false; /* Should be lp_schannel_server() == true */
	bool schannel_in_use = dce_call->conn->auth_state.auth_info
		&& dce_call->conn->auth_state.auth_info->auth_type == DCERPC_AUTH_TYPE_SCHANNEL
		&& (dce_call->conn->auth_state.auth_info->auth_level == DCERPC_AUTH_LEVEL_INTEGRITY 
		    || dce_call->conn->auth_state.auth_info->auth_level == DCERPC_AUTH_LEVEL_PRIVACY);

	ldb = schannel_db_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx);
	if (!ldb) {
		return NT_STATUS_ACCESS_DENIED;
	}
	nt_status = schannel_creds_server_step_check_ldb(ldb, mem_ctx,
							 computer_name,
							 schannel_global_required,
							 schannel_in_use,
							 received_authenticator,
							 return_authenticator, creds_out);
	talloc_free(ldb);
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
	NTSTATUS nt_status;

	nt_status = dcesrv_netr_creds_server_step_check(dce_call,
							mem_ctx, 
							r->in.computer_name, 
							r->in.credential, r->out.return_authenticator,
							&creds);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, system_session(mem_ctx, dce_call->conn->dce_ctx->lp_ctx));
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	netlogon_creds_des_decrypt(creds, r->in.new_password);

	/* Using the sid for the account as the key, set the password */
	nt_status = samdb_set_password_sid(sam_ctx, mem_ctx, 
					   creds->sid,
					   NULL, /* Don't have plaintext */
					   NULL, r->in.new_password,
					   true, /* Password change */
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
	NTSTATUS nt_status;
	DATA_BLOB new_password;

	struct samr_CryptPassword password_buf;

	nt_status = dcesrv_netr_creds_server_step_check(dce_call,
							mem_ctx, 
							r->in.computer_name, 
							r->in.credential, r->out.return_authenticator,
							&creds);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, system_session(mem_ctx, dce_call->conn->dce_ctx->lp_ctx));
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
		
	/* Using the sid for the account as the key, set the password */
	nt_status = samdb_set_password_sid(sam_ctx, mem_ctx,
					   creds->sid,
					   &new_password, /* we have plaintext */
					   NULL, NULL,
					   true, /* Password change */
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
	struct auth_serversupplied_info *server_info;
	NTSTATUS nt_status;
	static const char zeros[16];
	struct netr_SamBaseInfo *sam;
	struct netr_SamInfo2 *sam2;
	struct netr_SamInfo3 *sam3;
	struct netr_SamInfo6 *sam6;
	
	user_info = talloc(mem_ctx, struct auth_usersupplied_info);
	NT_STATUS_HAVE_NO_MEMORY(user_info);

	user_info->flags = 0;
	user_info->mapped_state = false;
	user_info->remote_host = NULL;

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
			struct server_id *kdc;
			struct kdc_check_generic_kerberos check;
			struct netr_GenericInfo2 *generic = talloc_zero(mem_ctx, struct netr_GenericInfo2);
			NT_STATUS_HAVE_NO_MEMORY(generic);
			*r->out.authoritative = 1;
			
			/* TODO: Describe and deal with these flags */
			*r->out.flags = 0;

			r->out.validation->generic = generic;
	
			kdc = irpc_servers_byname(dce_call->msg_ctx, mem_ctx, "kdc_server");
			if ((kdc == NULL) || (kdc[0].id == 0)) {
				return NT_STATUS_NO_LOGON_SERVERS;
			}
			
			check.in.generic_request = 
				data_blob_const(r->in.logon->generic->data,
						r->in.logon->generic->length);
			
			status = irpc_call(dce_call->msg_ctx, kdc[0],
					   &ndr_table_irpc, NDR_KDC_CHECK_GENERIC_KERBEROS,
					   &check, mem_ctx);
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
	
	nt_status = auth_check_password(auth_context, mem_ctx, user_info, &server_info);
	NT_STATUS_NOT_OK_RETURN(nt_status);

	nt_status = auth_convert_server_info_sambaseinfo(mem_ctx, server_info, &sam);
	NT_STATUS_NOT_OK_RETURN(nt_status);

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

	switch (r->in.validation_level) {
	case 2:
		sam2 = talloc_zero(mem_ctx, struct netr_SamInfo2);
		NT_STATUS_HAVE_NO_MEMORY(sam2);
		sam2->base = *sam;
		r->out.validation->sam2 = sam2;
		break;

	case 3:
		sam3 = talloc_zero(mem_ctx, struct netr_SamInfo3);
		NT_STATUS_HAVE_NO_MEMORY(sam3);
		sam3->base = *sam;
		r->out.validation->sam3 = sam3;
		break;

	case 6:
		sam6 = talloc_zero(mem_ctx, struct netr_SamInfo6);
		NT_STATUS_HAVE_NO_MEMORY(sam6);
		sam6->base = *sam;
		sam6->forest.string = lp_realm(dce_call->conn->dce_ctx->lp_ctx);
		sam6->principle.string = talloc_asprintf(mem_ctx, "%s@%s", 
							 sam->account_name.string, sam6->forest.string);
		NT_STATUS_HAVE_NO_MEMORY(sam6->principle.string);
		r->out.validation->sam6 = sam6;
		break;

	default:
		break;
	}

	*r->out.authoritative = 1;

	/* TODO: Describe and deal with these flags */
	*r->out.flags = 0;

	return NT_STATUS_OK;
}

static NTSTATUS dcesrv_netr_LogonSamLogonEx(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				     struct netr_LogonSamLogonEx *r) 
{
	NTSTATUS nt_status;
	struct netlogon_creds_CredentialState *creds;
	struct ldb_context *ldb = schannel_db_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx);
	if (!ldb) {
		return NT_STATUS_ACCESS_DENIED;
	}
	
	nt_status = schannel_fetch_session_key_ldb(ldb, mem_ctx, r->in.computer_name, &creds);
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

	return_authenticator = talloc(mem_ctx, struct netr_Authenticator);
	NT_STATUS_HAVE_NO_MEMORY(return_authenticator);

	nt_status = dcesrv_netr_creds_server_step_check(dce_call,
							mem_ctx, 
							r->in.computer_name, 
							r->in.credential, return_authenticator,
							&creds);
	NT_STATUS_NOT_OK_RETURN(nt_status);

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

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				dce_call->conn->auth_state.session_info);
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
				 lp_netbios_name(dce_call->conn->dce_ctx->lp_ctx));
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


/* 
  netr_GetAnyDCName 
*/
static WERROR dcesrv_netr_GetAnyDCName(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_GetAnyDCName *r)
{
	struct netr_GetDcName r2;
	WERROR werr;

	ZERO_STRUCT(r2);

	r2.in.logon_server	= r->in.logon_server;
	r2.in.domainname	= r->in.domainname;
	r2.out.dcname		= r->out.dcname;

	werr = dcesrv_netr_GetDcName(dce_call, mem_ctx, &r2);

	return werr;
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
  netr_NetrEnumerateTurstedDomains
*/
static WERROR dcesrv_netr_NetrEnumerateTrustedDomains(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
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
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
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
		char *p;
		/* TODO: we need a common function for pulling the forest */
		info->dns_forestname.string = ldb_dn_canonical_string(info, ldb_get_root_basedn(sam_ctx));
		if (!info->dns_forestname.string) {
			return NT_STATUS_NO_SUCH_DOMAIN;		
		}
		p = strchr(info->dns_forestname.string, '/');
		if (p) {
			*p = '\0';
		}
		info->dns_forestname.string = talloc_asprintf(mem_ctx, "%s.", info->dns_forestname.string);
					
	}

	if (is_local) {
		info->domainname.string = lp_sam_name(lp_ctx);
		info->dns_domainname.string = lp_realm(lp_ctx);
		info->domain_guid = samdb_result_guid(res, "objectGUID");
		info->domain_sid = samdb_result_dom_sid(mem_ctx, res, "objectSid");
	} else {
		info->domainname.string = samdb_result_string(res, "flatName", NULL);
		info->dns_domainname.string = samdb_result_string(res, "trustPartner", NULL);
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
	const char *temp_str;
	const char *old_dns_hostname;
	struct ldb_context *sam_ctx;
	struct ldb_message **res1, **res2, *new_msg;
	struct ldb_dn *workstation_dn;
	struct netr_DomainInformation *domain_info;
	struct netr_LsaPolicyInformation *lsa_policy_info;
	struct netr_OsVersionInfoEx *os_version;
	uint32_t default_supported_enc_types = 0xFFFFFFFF;
	int ret1, ret2, i;
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
		system_session(mem_ctx, dce_call->conn->dce_ctx->lp_ctx));
	if (sam_ctx == NULL) {
		return NT_STATUS_INVALID_SYSTEM_SERVICE;
	}

	switch (r->in.level) {
	case 1: /* Domain information */

		/* TODO: check NTSTATUS results - and fail also on SAMDB
		 * errors (needs some testing against Windows Server 2008) */

		/*
		 * Check that the computer name parameter matches as prefix with
		 * the DNS hostname in the workstation info structure.
		 */
		temp_str = strndup(r->in.query->workstation_info->dns_hostname,
			strcspn(r->in.query->workstation_info->dns_hostname,
			"."));
		if (strcasecmp(r->in.computer_name, temp_str) != 0)
			return NT_STATUS_INVALID_PARAMETER;

		workstation_dn = ldb_dn_new_fmt(mem_ctx, sam_ctx, "<SID=%s>",
			dom_sid_string(mem_ctx, creds->sid));
		NT_STATUS_HAVE_NO_MEMORY(workstation_dn);

		/* Gets the old DNS hostname */
		old_dns_hostname = samdb_search_string(sam_ctx, mem_ctx,
							workstation_dn,
							"dNSHostName",
							NULL);

		/* Gets host informations and put them in our directory */
		new_msg = ldb_msg_new(mem_ctx);
		NT_STATUS_HAVE_NO_MEMORY(new_msg);

		new_msg->dn = workstation_dn;

		/* Deletes old OS version values */
		samdb_msg_add_delete(sam_ctx, mem_ctx, new_msg,
			"operatingSystemServicePack");
		samdb_msg_add_delete(sam_ctx, mem_ctx, new_msg,
			"operatingSystemVersion");

		if (samdb_replace(sam_ctx, mem_ctx, new_msg) != LDB_SUCCESS) {
			DEBUG(3,("Impossible to update samdb: %s\n",
				ldb_errstring(sam_ctx)));
		}

		talloc_free(new_msg);

		new_msg = ldb_msg_new(mem_ctx);
		NT_STATUS_HAVE_NO_MEMORY(new_msg);

		new_msg->dn = workstation_dn;

		/* Sets the OS name */
		samdb_msg_set_string(sam_ctx, mem_ctx, new_msg,
			"operatingSystem",
			r->in.query->workstation_info->os_name.string);

		if (r->in.query->workstation_info->dns_hostname) {
			/* TODO: should this always be done? */
			samdb_msg_add_string(sam_ctx, mem_ctx, new_msg,
					     "dNSHostname",
					     r->in.query->workstation_info->dns_hostname);
		}

		/*
		 * Sets informations from "os_version". On a empty structure
		 * the values are cleared.
		 */
		if (r->in.query->workstation_info->os_version.os != NULL) {
			os_version = &r->in.query->workstation_info->os_version.os->os;

			samdb_msg_set_string(sam_ctx, mem_ctx, new_msg,
					     "operatingSystemServicePack",
					     os_version->CSDVersion);

			samdb_msg_set_string(sam_ctx, mem_ctx, new_msg,
				"operatingSystemVersion",
				talloc_asprintf(mem_ctx, "%d.%d (%d)",
					os_version->MajorVersion,
					os_version->MinorVersion,
					os_version->BuildNumber
				)
			);
		}

		/*
		 * Updates the "dNSHostname" and the "servicePrincipalName"s
		 * since the client wishes that the server should handle this
		 * for him ("NETR_WS_FLAG_HANDLES_SPN_UPDATE" not set).
		 * See MS-NRPC section 3.5.4.3.9
		 */
		if ((r->in.query->workstation_info->workstation_flags
			& NETR_WS_FLAG_HANDLES_SPN_UPDATE) == 0) {

			samdb_msg_add_string(sam_ctx, mem_ctx, new_msg,
				"servicePrincipalName",
				talloc_asprintf(mem_ctx, "HOST/%s",
				r->in.computer_name)
			);
			samdb_msg_add_string(sam_ctx, mem_ctx, new_msg,
				"servicePrincipalName",
				talloc_asprintf(mem_ctx, "HOST/%s",
				r->in.query->workstation_info->dns_hostname)
			);
		}

		if (samdb_replace(sam_ctx, mem_ctx, new_msg) != LDB_SUCCESS) {
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
		ret1 = gendb_search_dn(sam_ctx, mem_ctx, samdb_base_dn(sam_ctx),
			&res1, attrs);
		if (ret1 != 1) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		ret2 = gendb_search(sam_ctx, mem_ctx, NULL, &res2, attrs,
			"(objectClass=trustedDomain)");
		if (ret2 == -1) {
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		domain_info = talloc(mem_ctx, struct netr_DomainInformation);
		NT_STATUS_HAVE_NO_MEMORY(domain_info);

		ZERO_STRUCTP(domain_info);

		/* Informations about the local and trusted domains */

		status = fill_one_domain_info(mem_ctx,
			dce_call->conn->dce_ctx->lp_ctx,
			sam_ctx, res1[0], &domain_info->primary_domain,
			true, false);
		NT_STATUS_NOT_OK_RETURN(status);

		domain_info->trusted_domain_count = ret2 + 1;
		domain_info->trusted_domains = talloc_array(mem_ctx,
			struct netr_OneDomainInfo,
			domain_info->trusted_domain_count);
		NT_STATUS_HAVE_NO_MEMORY(domain_info->trusted_domains);

		for (i=0;i<ret2;i++) {
			status = fill_one_domain_info(mem_ctx,
				dce_call->conn->dce_ctx->lp_ctx,
				sam_ctx, res2[i],
				&domain_info->trusted_domains[i],
				false, true);
			NT_STATUS_NOT_OK_RETURN(status);
		}

		status = fill_one_domain_info(mem_ctx,
			dce_call->conn->dce_ctx->lp_ctx, sam_ctx, res1[0],
			&domain_info->trusted_domains[i], true, true);
		NT_STATUS_NOT_OK_RETURN(status);

		/* Sets the supported encryption types */
		domain_info->supported_enc_types = samdb_search_uint(
			sam_ctx, mem_ctx,
			default_supported_enc_types, workstation_dn,
			"msDS-SupportedEncryptionTypes", NULL);

		/* Other host domain informations */

		lsa_policy_info = talloc(mem_ctx,
			struct netr_LsaPolicyInformation);
		NT_STATUS_HAVE_NO_MEMORY(lsa_policy_info);
		ZERO_STRUCTP(lsa_policy_info);

		domain_info->lsa_policy = *lsa_policy_info;

		domain_info->dns_hostname.string = old_dns_hostname;
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
  netr_DsRAddressToSitenamesW 
*/
static WERROR dcesrv_netr_DsRAddressToSitenamesW(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DsRAddressToSitenamesW *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  netr_DsRGetDCNameEx2
*/
static WERROR dcesrv_netr_DsRGetDCNameEx2(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
				   struct netr_DsRGetDCNameEx2 *r)
{
	const char * const attrs[] = { "objectGUID", NULL };
	struct ldb_context *sam_ctx;
	struct ldb_message **res;
	struct ldb_dn *domain_dn;
	int ret;
	struct netr_DsRGetDCNameInfo *info;

	ZERO_STRUCTP(r->out.info);

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, dce_call->conn->auth_state.session_info);
	if (sam_ctx == NULL) {
		return WERR_DS_UNAVAILABLE;
	}

	/* Win7-beta will send the domain name in the form the user typed, so we have to cope
	   with both the short and long form here */
	if (r->in.domain_name != NULL && !lp_is_my_domain_or_realm(dce_call->conn->dce_ctx->lp_ctx, 
								r->in.domain_name)) {
		return WERR_NO_SUCH_DOMAIN;
	}

	domain_dn = ldb_get_default_basedn(sam_ctx);
	if (domain_dn == NULL) {
		return WERR_DS_UNAVAILABLE;
	}

	ret = gendb_search_dn(sam_ctx, mem_ctx,
			      domain_dn, &res, attrs);
	if (ret != 1) {
	}

	info = talloc(mem_ctx, struct netr_DsRGetDCNameInfo);
	W_ERROR_HAVE_NO_MEMORY(info);

	/* TODO: - return real IP address
	 *       - check all r->in.* parameters (server_unc is ignored by w2k3!)
	 */
	info->dc_unc			= talloc_asprintf(mem_ctx, "\\\\%s.%s",
							  lp_netbios_name(dce_call->conn->dce_ctx->lp_ctx), 
							  lp_realm(dce_call->conn->dce_ctx->lp_ctx));
	W_ERROR_HAVE_NO_MEMORY(info->dc_unc);
	info->dc_address		= talloc_strdup(mem_ctx, "\\\\0.0.0.0");
	W_ERROR_HAVE_NO_MEMORY(info->dc_address);
	info->dc_address_type		= DS_ADDRESS_TYPE_INET;
	info->domain_guid		= samdb_result_guid(res[0], "objectGUID");
	info->domain_name		= lp_realm(dce_call->conn->dce_ctx->lp_ctx);
	info->forest_name		= lp_realm(dce_call->conn->dce_ctx->lp_ctx);
	info->dc_flags			= DS_DNS_FOREST |
					  DS_DNS_DOMAIN |
					  DS_DNS_CONTROLLER |
					  DS_SERVER_WRITABLE |
					  DS_SERVER_CLOSEST |
					  DS_SERVER_TIMESERV |
					  DS_SERVER_KDC |
					  DS_SERVER_DS |
					  DS_SERVER_LDAP |
					  DS_SERVER_GC |
					  DS_SERVER_PDC;
	info->dc_site_name	= talloc_strdup(mem_ctx, "Default-First-Site-Name");
	W_ERROR_HAVE_NO_MEMORY(info->dc_site_name);
	info->client_site_name	= talloc_strdup(mem_ctx, "Default-First-Site-Name");
	W_ERROR_HAVE_NO_MEMORY(info->client_site_name);

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
	
	r2.in.site_name = NULL; /* should fill in from site GUID */
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
	struct netr_DsRAddressToSitenamesExWCtr *ctr;
	int i;

	/* we should map the provided IPs to site names, once we have
	 * sites support
	 */
	ctr = talloc(mem_ctx, struct netr_DsRAddressToSitenamesExWCtr);
	W_ERROR_HAVE_NO_MEMORY(ctr);

	*r->out.ctr = ctr;

	ctr->count = r->in.count;
	ctr->sitename = talloc_array(ctr, struct lsa_String, ctr->count);
	W_ERROR_HAVE_NO_MEMORY(ctr->sitename);
	ctr->subnetname = talloc_array(ctr, struct lsa_String, ctr->count);
	W_ERROR_HAVE_NO_MEMORY(ctr->subnetname);

	for (i=0; i<ctr->count; i++) {
		ctr->sitename[i].string   = "Default-First-Site-Name";
		ctr->subnetname[i].string = NULL;
	}

	return WERR_OK;
}


/* 
  netr_DsrGetDcSiteCoverageW
*/
static WERROR dcesrv_netr_DsrGetDcSiteCoverageW(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DsrGetDcSiteCoverageW *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* 
  netr_DsrEnumerateDomainTrusts 
*/
static WERROR dcesrv_netr_DsrEnumerateDomainTrusts(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
					      struct netr_DsrEnumerateDomainTrusts *r)
{
	struct netr_DomainTrustList *trusts;
	struct ldb_context *sam_ctx;
	int ret;
	struct ldb_message **dom_res;
	const char * const dom_attrs[] = { "objectSid", "objectGUID", NULL };

	ZERO_STRUCT(r->out);

	sam_ctx = samdb_connect(mem_ctx, dce_call->event_ctx, dce_call->conn->dce_ctx->lp_ctx, dce_call->conn->auth_state.session_info);
	if (sam_ctx == NULL) {
		return WERR_GENERAL_FAILURE;
	}

	ret = gendb_search_dn(sam_ctx, mem_ctx, NULL,
			      &dom_res, dom_attrs);
	if (ret == -1) {
		return WERR_GENERAL_FAILURE;		
	}
	if (ret != 1) {
		return WERR_GENERAL_FAILURE;
	}

	trusts = talloc(mem_ctx, struct netr_DomainTrustList);
	W_ERROR_HAVE_NO_MEMORY(trusts);

	trusts->array = talloc_array(trusts, struct netr_DomainTrust, ret);
	W_ERROR_HAVE_NO_MEMORY(trusts->array);

	trusts->count = 1; /* ?? */

	r->out.trusts = trusts;

	/* TODO: add filtering by trust_flags, and correct trust_type
	   and attributes */
	trusts->array[0].netbios_name = lp_sam_name(dce_call->conn->dce_ctx->lp_ctx);
	trusts->array[0].dns_name     = lp_realm(dce_call->conn->dce_ctx->lp_ctx);
	trusts->array[0].trust_flags =
		NETR_TRUST_FLAG_TREEROOT | 
		NETR_TRUST_FLAG_IN_FOREST | 
		NETR_TRUST_FLAG_PRIMARY;
	trusts->array[0].parent_index = 0;
	trusts->array[0].trust_type = 2;
	trusts->array[0].trust_attributes = 0;
	trusts->array[0].sid  = samdb_result_dom_sid(mem_ctx, dom_res[0], "objectSid");
	trusts->array[0].guid = samdb_result_guid(dom_res[0], "objectGUID");

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


/* 
  netr_DsRGetForestTrustInformation 
*/
static WERROR dcesrv_netr_DsRGetForestTrustInformation(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_DsRGetForestTrustInformation *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_GetForestTrustInformation
*/
static WERROR dcesrv_netr_GetForestTrustInformation(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_GetForestTrustInformation *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/*
  netr_ServerGetTrustInfo
*/
static NTSTATUS dcesrv_netr_ServerGetTrustInfo(struct dcesrv_call_state *dce_call, TALLOC_CTX *mem_ctx,
		       struct netr_ServerGetTrustInfo *r)
{
	DCESRV_FAULT(DCERPC_FAULT_OP_RNG_ERROR);
}


/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_netlogon_s.c"
