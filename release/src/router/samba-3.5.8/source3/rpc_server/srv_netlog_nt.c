/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1997,
 *  Copyright (C) Luke Kenneth Casson Leighton 1996-1997,
 *  Copyright (C) Paul Ashton                       1997.
 *  Copyright (C) Jeremy Allison               1998-2001.
 *  Copyright (C) Andrew Bartlett                   2001.
 *  Copyright (C) Guenther Deschner		    2008-2009.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/* This is the implementation of the netlogon pipe. */

#include "includes.h"
#include "../libcli/auth/schannel.h"
#include "../librpc/gen_ndr/srv_netlogon.h"

extern userdom_struct current_user_info;

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_RPC_SRV

struct netlogon_server_pipe_state {
	struct netr_Credential client_challenge;
	struct netr_Credential server_challenge;
};

/*************************************************************************
 _netr_LogonControl
 *************************************************************************/

WERROR _netr_LogonControl(pipes_struct *p,
			  struct netr_LogonControl *r)
{
	struct netr_LogonControl2Ex l;

	switch (r->in.level) {
	case 1:
		break;
	case 2:
		return WERR_NOT_SUPPORTED;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

	l.in.logon_server	= r->in.logon_server;
	l.in.function_code	= r->in.function_code;
	l.in.level		= r->in.level;
	l.in.data		= NULL;
	l.out.query		= r->out.query;

	return _netr_LogonControl2Ex(p, &l);
}

/****************************************************************************
Send a message to smbd to do a sam synchronisation
**************************************************************************/

static void send_sync_message(void)
{
        DEBUG(3, ("sending sam synchronisation message\n"));
        message_send_all(smbd_messaging_context(), MSG_SMB_SAM_SYNC, NULL, 0,
			 NULL);
}

/*************************************************************************
 _netr_LogonControl2
 *************************************************************************/

WERROR _netr_LogonControl2(pipes_struct *p,
			   struct netr_LogonControl2 *r)
{
	struct netr_LogonControl2Ex l;

	l.in.logon_server	= r->in.logon_server;
	l.in.function_code	= r->in.function_code;
	l.in.level		= r->in.level;
	l.in.data		= r->in.data;
	l.out.query		= r->out.query;

	return _netr_LogonControl2Ex(p, &l);
}

/*************************************************************************
 *************************************************************************/

static bool wb_change_trust_creds(const char *domain, WERROR *tc_status)
{
	wbcErr result;
	struct wbcAuthErrorInfo *error = NULL;

	result = wbcChangeTrustCredentials(domain, &error);
	switch (result) {
	case WBC_ERR_WINBIND_NOT_AVAILABLE:
		return false;
	case WBC_ERR_DOMAIN_NOT_FOUND:
		*tc_status = WERR_NO_SUCH_DOMAIN;
		return true;
	case WBC_ERR_SUCCESS:
		*tc_status = WERR_OK;
		return true;
	default:
		break;
	}

	if (error && error->nt_status != 0) {
		*tc_status = ntstatus_to_werror(NT_STATUS(error->nt_status));
	} else {
		*tc_status = WERR_TRUST_FAILURE;
	}
	wbcFreeMemory(error);
	return true;
}

/*************************************************************************
 *************************************************************************/

static bool wb_check_trust_creds(const char *domain, WERROR *tc_status)
{
	wbcErr result;
	struct wbcAuthErrorInfo *error = NULL;

	result = wbcCheckTrustCredentials(domain, &error);
	switch (result) {
	case WBC_ERR_WINBIND_NOT_AVAILABLE:
		return false;
	case WBC_ERR_DOMAIN_NOT_FOUND:
		*tc_status = WERR_NO_SUCH_DOMAIN;
		return true;
	case WBC_ERR_SUCCESS:
		*tc_status = WERR_OK;
		return true;
	default:
		break;
	}

	if (error && error->nt_status != 0) {
		*tc_status = ntstatus_to_werror(NT_STATUS(error->nt_status));
	} else {
		*tc_status = WERR_TRUST_FAILURE;
	}
	wbcFreeMemory(error);
	return true;
}

/****************************************************************
 _netr_LogonControl2Ex
****************************************************************/

WERROR _netr_LogonControl2Ex(pipes_struct *p,
			     struct netr_LogonControl2Ex *r)
{
	uint32_t flags = 0x0;
	WERROR pdc_connection_status = WERR_OK;
	uint32_t logon_attempts = 0x0;
	WERROR tc_status;
	fstring dc_name2;
	const char *dc_name = NULL;
	struct sockaddr_storage dc_ss;
	const char *domain = NULL;
	struct netr_NETLOGON_INFO_1 *info1;
	struct netr_NETLOGON_INFO_2 *info2;
	struct netr_NETLOGON_INFO_3 *info3;
	struct netr_NETLOGON_INFO_4 *info4;
	const char *fn;
	uint32_t acct_ctrl;

	switch (p->hdr_req.opnum) {
	case NDR_NETR_LOGONCONTROL:
		fn = "_netr_LogonControl";
		break;
	case NDR_NETR_LOGONCONTROL2:
		fn = "_netr_LogonControl2";
		break;
	case NDR_NETR_LOGONCONTROL2EX:
		fn = "_netr_LogonControl2Ex";
		break;
	default:
		return WERR_INVALID_PARAM;
	}

	acct_ctrl = pdb_get_acct_ctrl(p->server_info->sam_account);

	switch (r->in.function_code) {
	case NETLOGON_CONTROL_TC_VERIFY:
	case NETLOGON_CONTROL_CHANGE_PASSWORD:
	case NETLOGON_CONTROL_REDISCOVER:
		if ((geteuid() != sec_initial_uid()) &&
		    !nt_token_check_domain_rid(p->server_info->ptok, DOMAIN_RID_ADMINS) &&
		    !nt_token_check_sid(&global_sid_Builtin_Administrators, p->server_info->ptok) &&
		    !(acct_ctrl & (ACB_WSTRUST | ACB_SVRTRUST))) {
			return WERR_ACCESS_DENIED;
		}
		break;
	default:
		break;
	}

	tc_status = WERR_NO_SUCH_DOMAIN;

	switch (r->in.function_code) {
	case NETLOGON_CONTROL_QUERY:
		tc_status = WERR_OK;
		break;
	case NETLOGON_CONTROL_REPLICATE:
	case NETLOGON_CONTROL_SYNCHRONIZE:
	case NETLOGON_CONTROL_PDC_REPLICATE:
	case NETLOGON_CONTROL_BACKUP_CHANGE_LOG:
	case NETLOGON_CONTROL_BREAKPOINT:
		if (acct_ctrl & ACB_NORMAL) {
			return WERR_NOT_SUPPORTED;
		} else if (acct_ctrl & (ACB_WSTRUST | ACB_SVRTRUST)) {
			return WERR_ACCESS_DENIED;
		} else {
			return WERR_ACCESS_DENIED;
		}
	case NETLOGON_CONTROL_TRUNCATE_LOG:
		if (acct_ctrl & ACB_NORMAL) {
			break;
		} else if (acct_ctrl & (ACB_WSTRUST | ACB_SVRTRUST)) {
			return WERR_ACCESS_DENIED;
		} else {
			return WERR_ACCESS_DENIED;
		}

	case NETLOGON_CONTROL_TRANSPORT_NOTIFY:
	case NETLOGON_CONTROL_FORCE_DNS_REG:
	case NETLOGON_CONTROL_QUERY_DNS_REG:
		return WERR_NOT_SUPPORTED;
	case NETLOGON_CONTROL_FIND_USER:
		if (!r->in.data || !r->in.data->user) {
			return WERR_NOT_SUPPORTED;
		}
		break;
	case NETLOGON_CONTROL_SET_DBFLAG:
		if (!r->in.data) {
			return WERR_NOT_SUPPORTED;
		}
		break;
	case NETLOGON_CONTROL_TC_VERIFY:
		if (!r->in.data || !r->in.data->domain) {
			return WERR_NOT_SUPPORTED;
		}

		if (!wb_check_trust_creds(r->in.data->domain, &tc_status)) {
			return WERR_NOT_SUPPORTED;
		}
		break;
	case NETLOGON_CONTROL_TC_QUERY:
		if (!r->in.data || !r->in.data->domain) {
			return WERR_NOT_SUPPORTED;
		}

		domain = r->in.data->domain;

		if (!is_trusted_domain(domain)) {
			break;
		}

		if (!get_dc_name(domain, NULL, dc_name2, &dc_ss)) {
			tc_status = WERR_NO_LOGON_SERVERS;
			break;
		}

		dc_name = talloc_asprintf(p->mem_ctx, "\\\\%s", dc_name2);
		if (!dc_name) {
			return WERR_NOMEM;
		}

		tc_status = WERR_OK;

		break;

	case NETLOGON_CONTROL_REDISCOVER:
		if (!r->in.data || !r->in.data->domain) {
			return WERR_NOT_SUPPORTED;
		}

		domain = r->in.data->domain;

		if (!is_trusted_domain(domain)) {
			break;
		}

		if (!get_dc_name(domain, NULL, dc_name2, &dc_ss)) {
			tc_status = WERR_NO_LOGON_SERVERS;
			break;
		}

		dc_name = talloc_asprintf(p->mem_ctx, "\\\\%s", dc_name2);
		if (!dc_name) {
			return WERR_NOMEM;
		}

		tc_status = WERR_OK;

		break;

	case NETLOGON_CONTROL_CHANGE_PASSWORD:
		if (!r->in.data || !r->in.data->domain) {
			return WERR_NOT_SUPPORTED;
		}

		if (!wb_change_trust_creds(r->in.data->domain, &tc_status)) {
			return WERR_NOT_SUPPORTED;
		}
		break;

	default:
		/* no idea what this should be */
		DEBUG(0,("%s: unimplemented function level [%d]\n",
			fn, r->in.function_code));
		return WERR_UNKNOWN_LEVEL;
	}

	/* prepare the response */

	switch (r->in.level) {
	case 1:
		info1 = TALLOC_ZERO_P(p->mem_ctx, struct netr_NETLOGON_INFO_1);
		W_ERROR_HAVE_NO_MEMORY(info1);

		info1->flags			= flags;
		info1->pdc_connection_status	= pdc_connection_status;

		r->out.query->info1 = info1;
		break;
	case 2:
		info2 = TALLOC_ZERO_P(p->mem_ctx, struct netr_NETLOGON_INFO_2);
		W_ERROR_HAVE_NO_MEMORY(info2);

		info2->flags			= flags;
		info2->pdc_connection_status	= pdc_connection_status;
		info2->trusted_dc_name		= dc_name;
		info2->tc_connection_status	= tc_status;

		r->out.query->info2 = info2;
		break;
	case 3:
		info3 = TALLOC_ZERO_P(p->mem_ctx, struct netr_NETLOGON_INFO_3);
		W_ERROR_HAVE_NO_MEMORY(info3);

		info3->flags			= flags;
		info3->logon_attempts		= logon_attempts;

		r->out.query->info3 = info3;
		break;
	case 4:
		info4 = TALLOC_ZERO_P(p->mem_ctx, struct netr_NETLOGON_INFO_4);
		W_ERROR_HAVE_NO_MEMORY(info4);

		info4->trusted_dc_name		= dc_name;
		info4->trusted_domain_name	= r->in.data->domain;

		r->out.query->info4 = info4;
		break;
	default:
		return WERR_UNKNOWN_LEVEL;
	}

        if (lp_server_role() == ROLE_DOMAIN_BDC) {
                send_sync_message();
	}

	return WERR_OK;
}

/*************************************************************************
 _netr_NetrEnumerateTrustedDomains
 *************************************************************************/

WERROR _netr_NetrEnumerateTrustedDomains(pipes_struct *p,
					 struct netr_NetrEnumerateTrustedDomains *r)
{
	NTSTATUS status;
	DATA_BLOB blob;
	struct trustdom_info **domains;
	uint32_t num_domains;
	const char **trusted_domains;
	int i;

	DEBUG(6,("_netr_NetrEnumerateTrustedDomains: %d\n", __LINE__));

	/* set up the Trusted Domain List response */

	become_root();
	status = pdb_enum_trusteddoms(p->mem_ctx, &num_domains, &domains);
	unbecome_root();

	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	trusted_domains = talloc_zero_array(p->mem_ctx, const char *, num_domains + 1);
	if (!trusted_domains) {
		return WERR_NOMEM;
	}

	for (i = 0; i < num_domains; i++) {
		trusted_domains[i] = talloc_strdup(trusted_domains, domains[i]->name);
		if (!trusted_domains[i]) {
			TALLOC_FREE(trusted_domains);
			return WERR_NOMEM;
		}
	}

	if (!push_reg_multi_sz(trusted_domains, &blob, trusted_domains)) {
		TALLOC_FREE(trusted_domains);
		return WERR_NOMEM;
	}

	r->out.trusted_domains_blob->data = blob.data;
	r->out.trusted_domains_blob->length = blob.length;

	DEBUG(6,("_netr_NetrEnumerateTrustedDomains: %d\n", __LINE__));

	return WERR_OK;
}

/******************************************************************
 gets a machine password entry.  checks access rights of the host.
 ******************************************************************/

static NTSTATUS get_md4pw(struct samr_Password *md4pw, const char *mach_acct,
			  enum netr_SchannelType sec_chan_type, struct dom_sid *sid)
{
	struct samu *sampass = NULL;
	const uint8 *pass;
	bool ret;
	uint32 acct_ctrl;

#if 0
	char addr[INET6_ADDRSTRLEN];

    /*
     * Currently this code is redundent as we already have a filter
     * by hostname list. What this code really needs to do is to
     * get a hosts allowed/hosts denied list from the SAM database
     * on a per user basis, and make the access decision there.
     * I will leave this code here for now as a reminder to implement
     * this at a later date. JRA.
     */

	if (!allow_access(lp_domain_hostsdeny(), lp_domain_hostsallow(),
			client_name(get_client_fd()),
			client_addr(get_client_fd(),addr,sizeof(addr)))) {
		DEBUG(0,("get_md4pw: Workstation %s denied access to domain\n", mach_acct));
		return False;
	}
#endif /* 0 */

	if ( !(sampass = samu_new( NULL )) ) {
		return NT_STATUS_NO_MEMORY;
	}

	/* JRA. This is ok as it is only used for generating the challenge. */
	become_root();
	ret = pdb_getsampwnam(sampass, mach_acct);
	unbecome_root();

 	if (!ret) {
 		DEBUG(0,("get_md4pw: Workstation %s: no account in domain\n", mach_acct));
		TALLOC_FREE(sampass);
		return NT_STATUS_ACCESS_DENIED;
	}

	acct_ctrl = pdb_get_acct_ctrl(sampass);
	if (acct_ctrl & ACB_DISABLED) {
		DEBUG(0,("get_md4pw: Workstation %s: account is disabled\n", mach_acct));
		TALLOC_FREE(sampass);
		return NT_STATUS_ACCOUNT_DISABLED;
	}

	if (!(acct_ctrl & ACB_SVRTRUST) &&
	    !(acct_ctrl & ACB_WSTRUST) &&
	    !(acct_ctrl & ACB_DOMTRUST))
	{
		DEBUG(0,("get_md4pw: Workstation %s: account is not a trust account\n", mach_acct));
		TALLOC_FREE(sampass);
		return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
	}

	switch (sec_chan_type) {
		case SEC_CHAN_BDC:
			if (!(acct_ctrl & ACB_SVRTRUST)) {
				DEBUG(0,("get_md4pw: Workstation %s: BDC secure channel requested "
					 "but not a server trust account\n", mach_acct));
				TALLOC_FREE(sampass);
				return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
			}
			break;
		case SEC_CHAN_WKSTA:
			if (!(acct_ctrl & ACB_WSTRUST)) {
				DEBUG(0,("get_md4pw: Workstation %s: WORKSTATION secure channel requested "
					 "but not a workstation trust account\n", mach_acct));
				TALLOC_FREE(sampass);
				return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
			}
			break;
		case SEC_CHAN_DOMAIN:
			if (!(acct_ctrl & ACB_DOMTRUST)) {
				DEBUG(0,("get_md4pw: Workstation %s: DOMAIN secure channel requested "
					 "but not a interdomain trust account\n", mach_acct));
				TALLOC_FREE(sampass);
				return NT_STATUS_NO_TRUST_SAM_ACCOUNT;
			}
			break;
		default:
			break;
	}

	if ((pass = pdb_get_nt_passwd(sampass)) == NULL) {
		DEBUG(0,("get_md4pw: Workstation %s: account does not have a password\n", mach_acct));
		TALLOC_FREE(sampass);
		return NT_STATUS_LOGON_FAILURE;
	}

	memcpy(md4pw->hash, pass, 16);
	dump_data(5, md4pw->hash, 16);

	sid_copy(sid, pdb_get_user_sid(sampass));

	TALLOC_FREE(sampass);

	return NT_STATUS_OK;


}

/*************************************************************************
 _netr_ServerReqChallenge
 *************************************************************************/

NTSTATUS _netr_ServerReqChallenge(pipes_struct *p,
				  struct netr_ServerReqChallenge *r)
{
	struct netlogon_server_pipe_state *pipe_state =
		talloc_get_type(p->private_data, struct netlogon_server_pipe_state);

	if (pipe_state) {
		DEBUG(10,("_netr_ServerReqChallenge: new challenge requested. Clearing old state.\n"));
		talloc_free(pipe_state);
		p->private_data = NULL;
	}

	pipe_state = talloc(p, struct netlogon_server_pipe_state);
	NT_STATUS_HAVE_NO_MEMORY(pipe_state);

	pipe_state->client_challenge = *r->in.credentials;

	generate_random_buffer(pipe_state->server_challenge.data,
			       sizeof(pipe_state->server_challenge.data));

	*r->out.return_credentials = pipe_state->server_challenge;

	p->private_data = pipe_state;

	return NT_STATUS_OK;
}

/*************************************************************************
 _netr_ServerAuthenticate
 Create the initial credentials.
 *************************************************************************/

NTSTATUS _netr_ServerAuthenticate(pipes_struct *p,
				  struct netr_ServerAuthenticate *r)
{
	struct netr_ServerAuthenticate3 a;
	uint32_t negotiate_flags = 0;
	uint32_t rid;

	a.in.server_name		= r->in.server_name;
	a.in.account_name		= r->in.account_name;
	a.in.secure_channel_type	= r->in.secure_channel_type;
	a.in.computer_name		= r->in.computer_name;
	a.in.credentials		= r->in.credentials;
	a.in.negotiate_flags		= &negotiate_flags;

	a.out.return_credentials	= r->out.return_credentials;
	a.out.rid			= &rid;
	a.out.negotiate_flags		= &negotiate_flags;

	return _netr_ServerAuthenticate3(p, &a);

}

/*************************************************************************
 _netr_ServerAuthenticate3
 *************************************************************************/

NTSTATUS _netr_ServerAuthenticate3(pipes_struct *p,
				   struct netr_ServerAuthenticate3 *r)
{
	NTSTATUS status;
	uint32_t srv_flgs;
	/* r->in.negotiate_flags is an aliased pointer to r->out.negotiate_flags,
	 * so use a copy to avoid destroying the client values. */
	uint32_t in_neg_flags = *r->in.negotiate_flags;
	const char *fn;
	struct dom_sid sid;
	struct samr_Password mach_pwd;
	struct netlogon_creds_CredentialState *creds;
	struct netlogon_server_pipe_state *pipe_state =
		talloc_get_type(p->private_data, struct netlogon_server_pipe_state);

	/* According to Microsoft (see bugid #6099)
	 * Windows 7 looks at the negotiate_flags
	 * returned in this structure *even if the
	 * call fails with access denied* ! So in order
	 * to allow Win7 to connect to a Samba NT style
	 * PDC we set the flags before we know if it's
	 * an error or not.
	 */

	/* 0x000001ff */
	srv_flgs = NETLOGON_NEG_ACCOUNT_LOCKOUT |
		   NETLOGON_NEG_PERSISTENT_SAMREPL |
		   NETLOGON_NEG_ARCFOUR |
		   NETLOGON_NEG_PROMOTION_COUNT |
		   NETLOGON_NEG_CHANGELOG_BDC |
		   NETLOGON_NEG_FULL_SYNC_REPL |
		   NETLOGON_NEG_MULTIPLE_SIDS |
		   NETLOGON_NEG_REDO |
		   NETLOGON_NEG_PASSWORD_CHANGE_REFUSAL |
		   NETLOGON_NEG_PASSWORD_SET2;

	/* Ensure we support strong (128-bit) keys. */
	if (in_neg_flags & NETLOGON_NEG_STRONG_KEYS) {
		srv_flgs |= NETLOGON_NEG_STRONG_KEYS;
	}

	if (lp_server_schannel() != false) {
		srv_flgs |= NETLOGON_NEG_SCHANNEL;
	}

	switch (p->hdr_req.opnum) {
		case NDR_NETR_SERVERAUTHENTICATE:
			fn = "_netr_ServerAuthenticate";
			break;
		case NDR_NETR_SERVERAUTHENTICATE2:
			fn = "_netr_ServerAuthenticate2";
			break;
		case NDR_NETR_SERVERAUTHENTICATE3:
			fn = "_netr_ServerAuthenticate3";
			break;
		default:
			return NT_STATUS_INTERNAL_ERROR;
	}

	/* We use this as the key to store the creds: */
	/* r->in.computer_name */

	if (!pipe_state) {
		DEBUG(0,("%s: no challenge sent to client %s\n", fn,
			r->in.computer_name));
		status = NT_STATUS_ACCESS_DENIED;
		goto out;
	}

	if ( (lp_server_schannel() == true) &&
	     ((in_neg_flags & NETLOGON_NEG_SCHANNEL) == 0) ) {

		/* schannel must be used, but client did not offer it. */
		DEBUG(0,("%s: schannel required but client failed "
			"to offer it. Client was %s\n",
			fn, r->in.account_name));
		status = NT_STATUS_ACCESS_DENIED;
		goto out;
	}

	status = get_md4pw(&mach_pwd,
			   r->in.account_name,
			   r->in.secure_channel_type,
			   &sid);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("%s: failed to get machine password for "
			"account %s: %s\n",
			fn, r->in.account_name, nt_errstr(status) ));
		/* always return NT_STATUS_ACCESS_DENIED */
		status = NT_STATUS_ACCESS_DENIED;
		goto out;
	}

	/* From the client / server challenges and md4 password, generate sess key */
	/* Check client credentials are valid. */
	creds = netlogon_creds_server_init(p->mem_ctx,
					   r->in.account_name,
					   r->in.computer_name,
					   r->in.secure_channel_type,
					   &pipe_state->client_challenge,
					   &pipe_state->server_challenge,
					   &mach_pwd,
					   r->in.credentials,
					   r->out.return_credentials,
					   *r->in.negotiate_flags);
	if (!creds) {
		DEBUG(0,("%s: netlogon_creds_server_check failed. Rejecting auth "
			"request from client %s machine account %s\n",
			fn, r->in.computer_name,
			r->in.account_name));
		status = NT_STATUS_ACCESS_DENIED;
		goto out;
	}

	creds->sid = sid_dup_talloc(creds, &sid);
	if (!creds->sid) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	/* Store off the state so we can continue after client disconnect. */
	become_root();
	status = schannel_store_session_key(p->mem_ctx, creds);
	unbecome_root();

	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	sid_peek_rid(&sid, r->out.rid);

	status = NT_STATUS_OK;

  out:

	*r->out.negotiate_flags = srv_flgs;
	return status;
}

/*************************************************************************
 _netr_ServerAuthenticate2
 *************************************************************************/

NTSTATUS _netr_ServerAuthenticate2(pipes_struct *p,
				   struct netr_ServerAuthenticate2 *r)
{
	struct netr_ServerAuthenticate3 a;
	uint32_t rid;

	a.in.server_name		= r->in.server_name;
	a.in.account_name		= r->in.account_name;
	a.in.secure_channel_type	= r->in.secure_channel_type;
	a.in.computer_name		= r->in.computer_name;
	a.in.credentials		= r->in.credentials;
	a.in.negotiate_flags		= r->in.negotiate_flags;

	a.out.return_credentials	= r->out.return_credentials;
	a.out.rid			= &rid;
	a.out.negotiate_flags		= r->out.negotiate_flags;

	return _netr_ServerAuthenticate3(p, &a);
}

/*************************************************************************
 *************************************************************************/

static NTSTATUS netr_creds_server_step_check(pipes_struct *p,
					     TALLOC_CTX *mem_ctx,
					     const char *computer_name,
					     struct netr_Authenticator *received_authenticator,
					     struct netr_Authenticator *return_authenticator,
					     struct netlogon_creds_CredentialState **creds_out)
{
	NTSTATUS status;
	struct tdb_context *tdb;
	bool schannel_global_required = (lp_server_schannel() == true) ? true:false;
	bool schannel_in_use = (p->auth.auth_type == PIPE_AUTH_TYPE_SCHANNEL) ? true:false; /* &&
		(p->auth.auth_level == DCERPC_AUTH_LEVEL_INTEGRITY ||
		 p->auth.auth_level == DCERPC_AUTH_LEVEL_PRIVACY); */

	tdb = open_schannel_session_store(mem_ctx);
	if (!tdb) {
		return NT_STATUS_ACCESS_DENIED;
	}

	status = schannel_creds_server_step_check_tdb(tdb, mem_ctx,
						      computer_name,
						      schannel_global_required,
						      schannel_in_use,
						      received_authenticator,
						      return_authenticator,
						      creds_out);
	tdb_close(tdb);

	return status;
}

/*************************************************************************
 *************************************************************************/

static NTSTATUS netr_find_machine_account(TALLOC_CTX *mem_ctx,
					  const char *account_name,
					  struct samu **sampassp)
{
	struct samu *sampass;
	bool ret = false;
	uint32_t acct_ctrl;

	sampass = samu_new(mem_ctx);
	if (!sampass) {
		return NT_STATUS_NO_MEMORY;
	}

	become_root();
	ret = pdb_getsampwnam(sampass, account_name);
	unbecome_root();

	if (!ret) {
		TALLOC_FREE(sampass);
		return NT_STATUS_ACCESS_DENIED;
	}

	/* Ensure the account exists and is a machine account. */

	acct_ctrl = pdb_get_acct_ctrl(sampass);

	if (!(acct_ctrl & ACB_WSTRUST ||
	      acct_ctrl & ACB_SVRTRUST ||
	      acct_ctrl & ACB_DOMTRUST)) {
		TALLOC_FREE(sampass);
		return NT_STATUS_NO_SUCH_USER;
	}

	if (acct_ctrl & ACB_DISABLED) {
		TALLOC_FREE(sampass);
		return NT_STATUS_ACCOUNT_DISABLED;
	}

	*sampassp = sampass;

	return NT_STATUS_OK;
}

/*************************************************************************
 *************************************************************************/

static NTSTATUS netr_set_machine_account_password(TALLOC_CTX *mem_ctx,
						  struct samu *sampass,
						  DATA_BLOB *plaintext_blob,
						  struct samr_Password *nt_hash,
						  struct samr_Password *lm_hash)
{
	NTSTATUS status;
	const uchar *old_pw;
	const char *plaintext = NULL;
	size_t plaintext_len;
	struct samr_Password nt_hash_local;

	if (!sampass) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (plaintext_blob) {
		if (!convert_string_talloc(mem_ctx, CH_UTF16, CH_UNIX,
					   plaintext_blob->data, plaintext_blob->length,
					   &plaintext, &plaintext_len, false))
		{
			plaintext = NULL;
			mdfour(nt_hash_local.hash, plaintext_blob->data, plaintext_blob->length);
			nt_hash = &nt_hash_local;
		}
	}

	if (plaintext) {
		if (!pdb_set_plaintext_passwd(sampass, plaintext)) {
			return NT_STATUS_ACCESS_DENIED;
		}

		goto done;
	}

	if (nt_hash) {
		old_pw = pdb_get_nt_passwd(sampass);

		if (old_pw && memcmp(nt_hash->hash, old_pw, 16) == 0) {
			/* Avoid backend modificiations and other fun if the
			   client changed the password to the *same thing* */
		} else {
			/* LM password should be NULL for machines */
			if (!pdb_set_lanman_passwd(sampass, NULL, PDB_CHANGED)) {
				return NT_STATUS_NO_MEMORY;
			}
			if (!pdb_set_nt_passwd(sampass, nt_hash->hash, PDB_CHANGED)) {
				return NT_STATUS_NO_MEMORY;
			}

			if (!pdb_set_pass_last_set_time(sampass, time(NULL), PDB_CHANGED)) {
				/* Not quite sure what this one qualifies as, but this will do */
				return NT_STATUS_UNSUCCESSFUL;
			}
		}
	}

 done:
	become_root();
	status = pdb_update_sam_account(sampass);
	unbecome_root();

	return status;
}

/*************************************************************************
 _netr_ServerPasswordSet
 *************************************************************************/

NTSTATUS _netr_ServerPasswordSet(pipes_struct *p,
				 struct netr_ServerPasswordSet *r)
{
	NTSTATUS status = NT_STATUS_OK;
	struct samu *sampass=NULL;
	int i;
	struct netlogon_creds_CredentialState *creds;

	DEBUG(5,("_netr_ServerPasswordSet: %d\n", __LINE__));

	become_root();
	status = netr_creds_server_step_check(p, p->mem_ctx,
					      r->in.computer_name,
					      r->in.credential,
					      r->out.return_authenticator,
					      &creds);
	unbecome_root();

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2,("_netr_ServerPasswordSet: netlogon_creds_server_step failed. Rejecting auth "
			"request from client %s machine account %s\n",
			r->in.computer_name, creds->computer_name));
		TALLOC_FREE(creds);
		return status;
	}

	DEBUG(3,("_netr_ServerPasswordSet: Server Password Set by remote machine:[%s] on account [%s]\n",
			r->in.computer_name, creds->computer_name));

	netlogon_creds_des_decrypt(creds, r->in.new_password);

	DEBUG(100,("_netr_ServerPasswordSet: new given value was :\n"));
	for(i = 0; i < sizeof(r->in.new_password->hash); i++)
		DEBUG(100,("%02X ", r->in.new_password->hash[i]));
	DEBUG(100,("\n"));

	status = netr_find_machine_account(p->mem_ctx,
					   creds->account_name,
					   &sampass);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = netr_set_machine_account_password(sampass,
						   sampass,
						   NULL,
						   r->in.new_password,
						   NULL);
	TALLOC_FREE(sampass);
	return status;
}

/****************************************************************
 _netr_ServerPasswordSet2
****************************************************************/

NTSTATUS _netr_ServerPasswordSet2(pipes_struct *p,
				  struct netr_ServerPasswordSet2 *r)
{
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds;
	struct samu *sampass;
	DATA_BLOB plaintext;
	struct samr_CryptPassword password_buf;
	struct samr_Password nt_hash;

	become_root();
	status = netr_creds_server_step_check(p, p->mem_ctx,
					      r->in.computer_name,
					      r->in.credential,
					      r->out.return_authenticator,
					      &creds);
	unbecome_root();

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(2,("_netr_ServerPasswordSet2: netlogon_creds_server_step "
			"failed. Rejecting auth request from client %s machine account %s\n",
			r->in.computer_name, creds->computer_name));
		TALLOC_FREE(creds);
		return status;
	}

	memcpy(password_buf.data, r->in.new_password->data, 512);
	SIVAL(password_buf.data, 512, r->in.new_password->length);
	netlogon_creds_arcfour_crypt(creds, password_buf.data, 516);

	if (!extract_pw_from_buffer(p->mem_ctx, password_buf.data, &plaintext)) {
		return NT_STATUS_WRONG_PASSWORD;
	}

	mdfour(nt_hash.hash, plaintext.data, plaintext.length);

	status = netr_find_machine_account(p->mem_ctx,
					   creds->account_name,
					   &sampass);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = netr_set_machine_account_password(sampass,
						   sampass,
						   NULL,
						   &nt_hash,
						   NULL);
	TALLOC_FREE(sampass);
	return status;
}

/*************************************************************************
 _netr_LogonSamLogoff
 *************************************************************************/

NTSTATUS _netr_LogonSamLogoff(pipes_struct *p,
			      struct netr_LogonSamLogoff *r)
{
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds;

	become_root();
	status = netr_creds_server_step_check(p, p->mem_ctx,
					      r->in.computer_name,
					      r->in.credential,
					      r->out.return_authenticator,
					      &creds);
	unbecome_root();

	return status;
}

/*************************************************************************
 _netr_LogonSamLogon_base
 *************************************************************************/

static NTSTATUS _netr_LogonSamLogon_base(pipes_struct *p,
					 struct netr_LogonSamLogonEx *r,
					 struct netlogon_creds_CredentialState *creds)
{
	NTSTATUS status = NT_STATUS_OK;
	union netr_LogonLevel *logon = r->in.logon;
	const char *nt_username, *nt_domain, *nt_workstation;
	auth_usersupplied_info *user_info = NULL;
	auth_serversupplied_info *server_info = NULL;
	struct auth_context *auth_context = NULL;
	uint8_t pipe_session_key[16];
	bool process_creds = true;
	const char *fn;

	switch (p->hdr_req.opnum) {
		case NDR_NETR_LOGONSAMLOGON:
			process_creds = true;
			fn = "_netr_LogonSamLogon";
			break;
		case NDR_NETR_LOGONSAMLOGONWITHFLAGS:
			process_creds = true;
			fn = "_netr_LogonSamLogonWithFlags";
			break;
		case NDR_NETR_LOGONSAMLOGONEX:
			process_creds = false;
			fn = "_netr_LogonSamLogonEx";
			break;
		default:
			return NT_STATUS_INTERNAL_ERROR;
	}

	*r->out.authoritative = true; /* authoritative response */

	switch (r->in.validation_level) {
	case 2:
		r->out.validation->sam2 = TALLOC_ZERO_P(p->mem_ctx, struct netr_SamInfo2);
		if (!r->out.validation->sam2) {
			return NT_STATUS_NO_MEMORY;
		}
		break;
	case 3:
		r->out.validation->sam3 = TALLOC_ZERO_P(p->mem_ctx, struct netr_SamInfo3);
		if (!r->out.validation->sam3) {
			return NT_STATUS_NO_MEMORY;
		}
		break;
	case 6:
		r->out.validation->sam6 = TALLOC_ZERO_P(p->mem_ctx, struct netr_SamInfo6);
		if (!r->out.validation->sam6) {
			return NT_STATUS_NO_MEMORY;
		}
		break;
	default:
		DEBUG(0,("%s: bad validation_level value %d.\n",
			fn, (int)r->in.validation_level));
		return NT_STATUS_INVALID_INFO_CLASS;
	}

	switch (r->in.logon_level) {
	case NetlogonInteractiveInformation:
	case NetlogonServiceInformation:
	case NetlogonInteractiveTransitiveInformation:
	case NetlogonServiceTransitiveInformation:
		nt_username	= logon->password->identity_info.account_name.string ?
				  logon->password->identity_info.account_name.string : "";
		nt_domain	= logon->password->identity_info.domain_name.string ?
				  logon->password->identity_info.domain_name.string : "";
		nt_workstation	= logon->password->identity_info.workstation.string ?
				  logon->password->identity_info.workstation.string : "";

		DEBUG(3,("SAM Logon (Interactive). Domain:[%s].  ", lp_workgroup()));
		break;
	case NetlogonNetworkInformation:
	case NetlogonNetworkTransitiveInformation:
		nt_username	= logon->network->identity_info.account_name.string ?
				  logon->network->identity_info.account_name.string : "";
		nt_domain	= logon->network->identity_info.domain_name.string ?
				  logon->network->identity_info.domain_name.string : "";
		nt_workstation	= logon->network->identity_info.workstation.string ?
				  logon->network->identity_info.workstation.string : "";

		DEBUG(3,("SAM Logon (Network). Domain:[%s].  ", lp_workgroup()));
		break;
	default:
		DEBUG(2,("SAM Logon: unsupported switch value\n"));
		return NT_STATUS_INVALID_INFO_CLASS;
	} /* end switch */

	DEBUG(3,("User:[%s@%s] Requested Domain:[%s]\n", nt_username, nt_workstation, nt_domain));
	fstrcpy(current_user_info.smb_name, nt_username);
	sub_set_smb_name(nt_username);

	DEBUG(5,("Attempting validation level %d for unmapped username %s.\n",
		r->in.validation_level, nt_username));

	status = NT_STATUS_OK;

	switch (r->in.logon_level) {
	case NetlogonNetworkInformation:
	case NetlogonNetworkTransitiveInformation:
	{
		const char *wksname = nt_workstation;

		status = make_auth_context_fixed(&auth_context,
						 logon->network->challenge);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/* For a network logon, the workstation name comes in with two
		 * backslashes in the front. Strip them if they are there. */

		if (*wksname == '\\') wksname++;
		if (*wksname == '\\') wksname++;

		/* Standard challenge/response authenticaion */
		if (!make_user_info_netlogon_network(&user_info,
						     nt_username, nt_domain,
						     wksname,
						     logon->network->identity_info.parameter_control,
						     logon->network->lm.data,
						     logon->network->lm.length,
						     logon->network->nt.data,
						     logon->network->nt.length)) {
			status = NT_STATUS_NO_MEMORY;
		}
		break;
	}
	case NetlogonInteractiveInformation:
	case NetlogonServiceInformation:
	case NetlogonInteractiveTransitiveInformation:
	case NetlogonServiceTransitiveInformation:

		/* 'Interactive' authentication, supplies the password in its
		   MD4 form, encrypted with the session key.  We will convert
		   this to challenge/response for the auth subsystem to chew
		   on */
	{
		uint8_t chal[8];

		if (!NT_STATUS_IS_OK(status = make_auth_context_subsystem(&auth_context))) {
			return status;
		}

		auth_context->get_ntlm_challenge(auth_context, chal);

		if (!make_user_info_netlogon_interactive(&user_info,
							 nt_username, nt_domain,
							 nt_workstation,
							 logon->password->identity_info.parameter_control,
							 chal,
							 logon->password->lmpassword.hash,
							 logon->password->ntpassword.hash,
							 creds->session_key)) {
			status = NT_STATUS_NO_MEMORY;
		}
		break;
	}
	default:
		DEBUG(2,("SAM Logon: unsupported switch value\n"));
		return NT_STATUS_INVALID_INFO_CLASS;
	} /* end switch */

	if ( NT_STATUS_IS_OK(status) ) {
		status = auth_context->check_ntlm_password(auth_context,
			user_info, &server_info);
	}

	(auth_context->free)(&auth_context);
	free_user_info(&user_info);

	DEBUG(5,("%s: check_password returned status %s\n",
		  fn, nt_errstr(status)));

	/* Check account and password */

	if (!NT_STATUS_IS_OK(status)) {
		/* If we don't know what this domain is, we need to
		   indicate that we are not authoritative.  This
		   allows the client to decide if it needs to try
		   a local user.  Fix by jpjanosi@us.ibm.com, #2976 */
                if ( NT_STATUS_EQUAL(status, NT_STATUS_NO_SUCH_USER)
		     && !strequal(nt_domain, get_global_sam_name())
		     && !is_trusted_domain(nt_domain) )
			*r->out.authoritative = false; /* We are not authoritative */

		TALLOC_FREE(server_info);
		return status;
	}

	if (server_info->guest) {
		/* We don't like guest domain logons... */
		DEBUG(5,("%s: Attempted domain logon as GUEST "
			 "denied.\n", fn));
		TALLOC_FREE(server_info);
		return NT_STATUS_LOGON_FAILURE;
	}

	/* This is the point at which, if the login was successful, that
           the SAM Local Security Authority should record that the user is
           logged in to the domain.  */

	if (process_creds) {
		/* Get the pipe session key from the creds. */
		memcpy(pipe_session_key, creds->session_key, 16);
	} else {
		/* Get the pipe session key from the schannel. */
		if ((p->auth.auth_type != PIPE_AUTH_TYPE_SCHANNEL)
		    || (p->auth.a_u.schannel_auth == NULL)) {
			return NT_STATUS_INVALID_HANDLE;
		}
		memcpy(pipe_session_key, p->auth.a_u.schannel_auth->creds->session_key, 16);
	}

	switch (r->in.validation_level) {
	case 2:
		status = serverinfo_to_SamInfo2(server_info, pipe_session_key, 16,
						r->out.validation->sam2);
		break;
	case 3:
		status = serverinfo_to_SamInfo3(server_info, pipe_session_key, 16,
						r->out.validation->sam3);
		break;
	case 6:
		status = serverinfo_to_SamInfo6(server_info, pipe_session_key, 16,
						r->out.validation->sam6);
		break;
	}

	TALLOC_FREE(server_info);

	return status;
}

/****************************************************************
 _netr_LogonSamLogonWithFlags
****************************************************************/

NTSTATUS _netr_LogonSamLogonWithFlags(pipes_struct *p,
				      struct netr_LogonSamLogonWithFlags *r)
{
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds;
	struct netr_LogonSamLogonEx r2;
	struct netr_Authenticator return_authenticator;

	become_root();
	status = netr_creds_server_step_check(p, p->mem_ctx,
					      r->in.computer_name,
					      r->in.credential,
					      &return_authenticator,
					      &creds);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	r2.in.server_name	= r->in.server_name;
	r2.in.computer_name	= r->in.computer_name;
	r2.in.logon_level	= r->in.logon_level;
	r2.in.logon		= r->in.logon;
	r2.in.validation_level	= r->in.validation_level;
	r2.in.flags		= r->in.flags;
	r2.out.validation	= r->out.validation;
	r2.out.authoritative	= r->out.authoritative;
	r2.out.flags		= r->out.flags;

	status = _netr_LogonSamLogon_base(p, &r2, creds);

	*r->out.return_authenticator = return_authenticator;

	return status;
}

/*************************************************************************
 _netr_LogonSamLogon
 *************************************************************************/

NTSTATUS _netr_LogonSamLogon(pipes_struct *p,
			     struct netr_LogonSamLogon *r)
{
	NTSTATUS status;
	struct netr_LogonSamLogonWithFlags r2;
	uint32_t flags = 0;

	r2.in.server_name		= r->in.server_name;
	r2.in.computer_name		= r->in.computer_name;
	r2.in.credential		= r->in.credential;
	r2.in.logon_level		= r->in.logon_level;
	r2.in.logon			= r->in.logon;
	r2.in.validation_level		= r->in.validation_level;
	r2.in.return_authenticator	= r->in.return_authenticator;
	r2.in.flags			= &flags;
	r2.out.validation		= r->out.validation;
	r2.out.authoritative		= r->out.authoritative;
	r2.out.flags			= &flags;
	r2.out.return_authenticator	= r->out.return_authenticator;

	status = _netr_LogonSamLogonWithFlags(p, &r2);

	return status;
}

/*************************************************************************
 _netr_LogonSamLogonEx
 - no credential chaining. Map into net sam logon.
 *************************************************************************/

NTSTATUS _netr_LogonSamLogonEx(pipes_struct *p,
			       struct netr_LogonSamLogonEx *r)
{
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds = NULL;

	become_root();
	status = schannel_fetch_session_key(p->mem_ctx, r->in.computer_name, &creds);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Only allow this if the pipe is protected. */
	if (p->auth.auth_type != PIPE_AUTH_TYPE_SCHANNEL) {
		DEBUG(0,("_netr_LogonSamLogonEx: client %s not using schannel for netlogon\n",
			get_remote_machine_name() ));
		return NT_STATUS_INVALID_PARAMETER;
        }

	status = _netr_LogonSamLogon_base(p, r, creds);
	TALLOC_FREE(creds);

	return status;
}

/*************************************************************************
 _ds_enum_dom_trusts
 *************************************************************************/
#if 0	/* JERRY -- not correct */
 NTSTATUS _ds_enum_dom_trusts(pipes_struct *p, DS_Q_ENUM_DOM_TRUSTS *q_u,
			     DS_R_ENUM_DOM_TRUSTS *r_u)
{
	NTSTATUS status = NT_STATUS_OK;

	/* TODO: According to MSDN, the can only be executed against a
	   DC or domain member running Windows 2000 or later.  Need
	   to test against a standalone 2k server and see what it
	   does.  A windows 2000 DC includes its own domain in the
	   list.  --jerry */

	return status;
}
#endif	/* JERRY */


/****************************************************************
****************************************************************/

WERROR _netr_LogonUasLogon(pipes_struct *p,
			   struct netr_LogonUasLogon *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_LogonUasLogoff(pipes_struct *p,
			    struct netr_LogonUasLogoff *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_DatabaseDeltas(pipes_struct *p,
			      struct netr_DatabaseDeltas *r)
{
	p->rng_fault_state = true;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_DatabaseSync(pipes_struct *p,
			    struct netr_DatabaseSync *r)
{
	p->rng_fault_state = true;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_AccountDeltas(pipes_struct *p,
			     struct netr_AccountDeltas *r)
{
	p->rng_fault_state = true;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_AccountSync(pipes_struct *p,
			   struct netr_AccountSync *r)
{
	p->rng_fault_state = true;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

static bool wb_getdcname(TALLOC_CTX *mem_ctx,
			 const char *domain,
			 const char **dcname,
			 uint32_t flags,
			 WERROR *werr)
{
	wbcErr result;
	struct wbcDomainControllerInfo *dc_info = NULL;

	result = wbcLookupDomainController(domain,
					   flags,
					   &dc_info);
	switch (result) {
	case WBC_ERR_SUCCESS:
		break;
	case WBC_ERR_WINBIND_NOT_AVAILABLE:
		return false;
	case WBC_ERR_DOMAIN_NOT_FOUND:
		*werr = WERR_NO_SUCH_DOMAIN;
		return true;
	default:
		*werr = WERR_DOMAIN_CONTROLLER_NOT_FOUND;
		return true;
	}

	*dcname = talloc_strdup(mem_ctx, dc_info->dc_name);
	wbcFreeMemory(dc_info);
	if (!*dcname) {
		*werr = WERR_NOMEM;
		return false;
	}

	*werr = WERR_OK;

	return true;
}

/****************************************************************
 _netr_GetDcName
****************************************************************/

WERROR _netr_GetDcName(pipes_struct *p,
		       struct netr_GetDcName *r)
{
	NTSTATUS status;
	WERROR werr;
	uint32_t flags;
	struct netr_DsRGetDCNameInfo *info;
	bool ret;

	ret = wb_getdcname(p->mem_ctx,
			   r->in.domainname,
			   r->out.dcname,
			   WBC_LOOKUP_DC_IS_FLAT_NAME |
			   WBC_LOOKUP_DC_RETURN_FLAT_NAME |
			   WBC_LOOKUP_DC_PDC_REQUIRED,
			   &werr);
	if (ret == true) {
		return werr;
	}

	flags = DS_PDC_REQUIRED | DS_IS_FLAT_NAME | DS_RETURN_FLAT_NAME;

	status = dsgetdcname(p->mem_ctx,
			     smbd_messaging_context(),
			     r->in.domainname,
			     NULL,
			     NULL,
			     flags,
			     &info);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	*r->out.dcname = talloc_strdup(p->mem_ctx, info->dc_unc);
	talloc_free(info);
	if (!*r->out.dcname) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

/****************************************************************
 _netr_GetAnyDCName
****************************************************************/

WERROR _netr_GetAnyDCName(pipes_struct *p,
			  struct netr_GetAnyDCName *r)
{
	NTSTATUS status;
	WERROR werr;
	uint32_t flags;
	struct netr_DsRGetDCNameInfo *info;
	bool ret;

	ret = wb_getdcname(p->mem_ctx,
			   r->in.domainname,
			   r->out.dcname,
			   WBC_LOOKUP_DC_IS_FLAT_NAME |
			   WBC_LOOKUP_DC_RETURN_FLAT_NAME,
			   &werr);
	if (ret == true) {
		return werr;
	}

	flags = DS_IS_FLAT_NAME | DS_RETURN_FLAT_NAME;

	status = dsgetdcname(p->mem_ctx,
			     smbd_messaging_context(),
			     r->in.domainname,
			     NULL,
			     NULL,
			     flags,
			     &info);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	*r->out.dcname = talloc_strdup(p->mem_ctx, info->dc_unc);
	talloc_free(info);
	if (!*r->out.dcname) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_DatabaseSync2(pipes_struct *p,
			     struct netr_DatabaseSync2 *r)
{
	p->rng_fault_state = true;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_DatabaseRedo(pipes_struct *p,
			    struct netr_DatabaseRedo *r)
{
	p->rng_fault_state = true;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetDCName(pipes_struct *p,
			  struct netr_DsRGetDCName *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_LogonGetCapabilities(pipes_struct *p,
				    struct netr_LogonGetCapabilities *r)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONSETSERVICEBITS(pipes_struct *p,
				     struct netr_NETRLOGONSETSERVICEBITS *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_LogonGetTrustRid(pipes_struct *p,
			      struct netr_LogonGetTrustRid *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONCOMPUTESERVERDIGEST(pipes_struct *p,
					  struct netr_NETRLOGONCOMPUTESERVERDIGEST *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONCOMPUTECLIENTDIGEST(pipes_struct *p,
					  struct netr_NETRLOGONCOMPUTECLIENTDIGEST *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetDCNameEx(pipes_struct *p,
			    struct netr_DsRGetDCNameEx *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetSiteName(pipes_struct *p,
			    struct netr_DsRGetSiteName *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_LogonGetDomainInfo(pipes_struct *p,
				  struct netr_LogonGetDomainInfo *r)
{
	p->rng_fault_state = true;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_ServerPasswordGet(pipes_struct *p,
			       struct netr_ServerPasswordGet *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONSENDTOSAM(pipes_struct *p,
				struct netr_NETRLOGONSENDTOSAM *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRAddressToSitenamesW(pipes_struct *p,
				    struct netr_DsRAddressToSitenamesW *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetDCNameEx2(pipes_struct *p,
			     struct netr_DsRGetDCNameEx2 *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONGETTIMESERVICEPARENTDOMAIN(pipes_struct *p,
						 struct netr_NETRLOGONGETTIMESERVICEPARENTDOMAIN *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NetrEnumerateTrustedDomainsEx(pipes_struct *p,
					   struct netr_NetrEnumerateTrustedDomainsEx *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRAddressToSitenamesExW(pipes_struct *p,
				      struct netr_DsRAddressToSitenamesExW *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsrGetDcSiteCoverageW(pipes_struct *p,
				   struct netr_DsrGetDcSiteCoverageW *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsrEnumerateDomainTrusts(pipes_struct *p,
				      struct netr_DsrEnumerateDomainTrusts *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsrDeregisterDNSHostRecords(pipes_struct *p,
					 struct netr_DsrDeregisterDNSHostRecords *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_ServerTrustPasswordsGet(pipes_struct *p,
				       struct netr_ServerTrustPasswordsGet *r)
{
	p->rng_fault_state = true;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetForestTrustInformation(pipes_struct *p,
					  struct netr_DsRGetForestTrustInformation *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_GetForestTrustInformation(pipes_struct *p,
				       struct netr_GetForestTrustInformation *r)
{
	p->rng_fault_state = true;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_ServerGetTrustInfo(pipes_struct *p,
				  struct netr_ServerGetTrustInfo *r)
{
	p->rng_fault_state = true;
	return NT_STATUS_NOT_IMPLEMENTED;
}

