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
#include "ntdomain.h"
#include "../libcli/auth/schannel.h"
#include "../librpc/gen_ndr/srv_netlogon.h"
#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "../librpc/gen_ndr/ndr_lsa_c.h"
#include "rpc_client/cli_lsarpc.h"
#include "../lib/crypto/md4.h"
#include "rpc_client/init_lsa.h"
#include "rpc_server/rpc_ncacn_np.h"
#include "../libcli/security/security.h"
#include "../libcli/security/dom_sid.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "lib/crypto/arcfour.h"
#include "lib/crypto/md4.h"
#include "nsswitch/libwbclient/wbclient.h"
#include "../libcli/registry/util_reg.h"
#include "passdb.h"
#include "auth.h"
#include "messages.h"

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

WERROR _netr_LogonControl(struct pipes_struct *p,
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

static void send_sync_message(struct messaging_context *msg_ctx)
{
        DEBUG(3, ("sending sam synchronisation message\n"));
        message_send_all(msg_ctx, MSG_SMB_SAM_SYNC, NULL, 0, NULL);
}

/*************************************************************************
 _netr_LogonControl2
 *************************************************************************/

WERROR _netr_LogonControl2(struct pipes_struct *p,
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

WERROR _netr_LogonControl2Ex(struct pipes_struct *p,
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

	switch (p->opnum) {
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

	acct_ctrl = p->session_info->info3->base.acct_flags;

	switch (r->in.function_code) {
	case NETLOGON_CONTROL_TC_VERIFY:
	case NETLOGON_CONTROL_CHANGE_PASSWORD:
	case NETLOGON_CONTROL_REDISCOVER:
		if ((geteuid() != sec_initial_uid()) &&
		    !nt_token_check_domain_rid(p->session_info->security_token, DOMAIN_RID_ADMINS) &&
		    !nt_token_check_sid(&global_sid_Builtin_Administrators, p->session_info->security_token) &&
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
                send_sync_message(p->msg_ctx);
	}

	return WERR_OK;
}

/*************************************************************************
 _netr_NetrEnumerateTrustedDomains
 *************************************************************************/

NTSTATUS _netr_NetrEnumerateTrustedDomains(struct pipes_struct *p,
					   struct netr_NetrEnumerateTrustedDomains *r)
{
	NTSTATUS status;
	NTSTATUS result = NT_STATUS_OK;
	DATA_BLOB blob;
	int num_domains = 0;
	const char **trusted_domains = NULL;
	struct lsa_DomainList domain_list;
	struct dcerpc_binding_handle *h = NULL;
	struct policy_handle pol;
	uint32_t enum_ctx = 0;
	int i;
	uint32_t max_size = (uint32_t)-1;

	DEBUG(6,("_netr_NetrEnumerateTrustedDomains: %d\n", __LINE__));

	status = rpcint_binding_handle(p->mem_ctx,
				       &ndr_table_lsarpc,
				       p->client_id,
				       p->session_info,
				       p->msg_ctx,
				       &h);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = dcerpc_lsa_open_policy2(h,
					 p->mem_ctx,
					 NULL,
					 true,
					 LSA_POLICY_VIEW_LOCAL_INFORMATION,
					 &pol,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

	do {
		/* Lookup list of trusted domains */
		status = dcerpc_lsa_EnumTrustDom(h,
						 p->mem_ctx,
						 &pol,
						 &enum_ctx,
						 &domain_list,
						 max_size,
						 &result);
		if (!NT_STATUS_IS_OK(status)) {
			goto out;
		}
		if (!NT_STATUS_IS_OK(result) &&
		    !NT_STATUS_EQUAL(result, NT_STATUS_NO_MORE_ENTRIES) &&
		    !NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES)) {
			status = result;
			goto out;
		}

		for (i = 0; i < domain_list.count; i++) {
			if (!add_string_to_array(p->mem_ctx, domain_list.domains[i].name.string,
						 &trusted_domains, &num_domains)) {
				status = NT_STATUS_NO_MEMORY;
				goto out;
			}
		}
	} while (NT_STATUS_EQUAL(result, STATUS_MORE_ENTRIES));

	if (num_domains > 0) {
		/* multi sz terminate */
		trusted_domains = talloc_realloc(p->mem_ctx, trusted_domains, const char *, num_domains + 1);
		if (trusted_domains == NULL) {
			status = NT_STATUS_NO_MEMORY;
			goto out;
		}

		trusted_domains[num_domains] = NULL;
	}

	if (!push_reg_multi_sz(trusted_domains, &blob, trusted_domains)) {
		TALLOC_FREE(trusted_domains);
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	r->out.trusted_domains_blob->data = blob.data;
	r->out.trusted_domains_blob->length = blob.length;

	DEBUG(6,("_netr_NetrEnumerateTrustedDomains: %d\n", __LINE__));

	status = NT_STATUS_OK;

 out:
	if (h && is_valid_policy_hnd(&pol)) {
		dcerpc_lsa_Close(h, p->mem_ctx, &pol, &result);
	}

	return status;
}

/*************************************************************************
 *************************************************************************/

static NTSTATUS samr_find_machine_account(TALLOC_CTX *mem_ctx,
					  struct dcerpc_binding_handle *b,
					  const char *account_name,
					  uint32_t access_mask,
					  struct dom_sid2 **domain_sid_p,
					  uint32_t *user_rid_p,
					  struct policy_handle *user_handle)
{
	NTSTATUS status;
	NTSTATUS result = NT_STATUS_OK;
	struct policy_handle connect_handle, domain_handle;
	struct lsa_String domain_name;
	struct dom_sid2 *domain_sid;
	struct lsa_String names;
	struct samr_Ids rids;
	struct samr_Ids types;
	uint32_t rid;

	status = dcerpc_samr_Connect2(b, mem_ctx,
				      global_myname(),
				      SAMR_ACCESS_CONNECT_TO_SERVER |
				      SAMR_ACCESS_ENUM_DOMAINS |
				      SAMR_ACCESS_LOOKUP_DOMAIN,
				      &connect_handle,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

	init_lsa_String(&domain_name, get_global_sam_name());

	status = dcerpc_samr_LookupDomain(b, mem_ctx,
					  &connect_handle,
					  &domain_name,
					  &domain_sid,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					&connect_handle,
					SAMR_DOMAIN_ACCESS_OPEN_ACCOUNT,
					domain_sid,
					&domain_handle,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

	init_lsa_String(&names, account_name);

	status = dcerpc_samr_LookupNames(b, mem_ctx,
					 &domain_handle,
					 1,
					 &names,
					 &rids,
					 &types,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

	if (rids.count != 1) {
		status = NT_STATUS_NO_SUCH_USER;
		goto out;
	}
	if (types.count != 1) {
		status = NT_STATUS_INVALID_PARAMETER;
		goto out;
	}
	if (types.ids[0] != SID_NAME_USER) {
		status = NT_STATUS_NO_SUCH_USER;
		goto out;
	}

	rid = rids.ids[0];

	status = dcerpc_samr_OpenUser(b, mem_ctx,
				      &domain_handle,
				      access_mask,
				      rid,
				      user_handle,
				      &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

	if (user_rid_p) {
		*user_rid_p = rid;
	}

	if (domain_sid_p) {
		*domain_sid_p = domain_sid;
	}

 out:
	if (b && is_valid_policy_hnd(&domain_handle)) {
		dcerpc_samr_Close(b, mem_ctx, &domain_handle, &result);
	}
	if (b && is_valid_policy_hnd(&connect_handle)) {
		dcerpc_samr_Close(b, mem_ctx, &connect_handle, &result);
	}

	return status;
}

/******************************************************************
 gets a machine password entry.  checks access rights of the host.
 ******************************************************************/

static NTSTATUS get_md4pw(struct samr_Password *md4pw, const char *mach_acct,
			  enum netr_SchannelType sec_chan_type,
			  struct dom_sid *sid,
			  struct messaging_context *msg_ctx)
{
	NTSTATUS status;
	NTSTATUS result = NT_STATUS_OK;
	TALLOC_CTX *mem_ctx;
	struct dcerpc_binding_handle *h = NULL;
	static struct client_address client_id;
	struct policy_handle user_handle;
	uint32_t user_rid;
	struct dom_sid *domain_sid;
	uint32_t acct_ctrl;
	union samr_UserInfo *info;
	struct auth_serversupplied_info *session_info;
#if 0

    /*
     * Currently this code is redundent as we already have a filter
     * by hostname list. What this code really needs to do is to
     * get a hosts allowed/hosts denied list from the SAM database
     * on a per user basis, and make the access decision there.
     * I will leave this code here for now as a reminder to implement
     * this at a later date. JRA.
     */

	if (!allow_access(lp_domain_hostsdeny(), lp_domain_hostsallow(),
			  p->client_id.name,
			  p->client_id.addr)) {
		DEBUG(0,("get_md4pw: Workstation %s denied access to domain\n", mach_acct));
		return False;
	}
#endif /* 0 */

	mem_ctx = talloc_stackframe();
	if (mem_ctx == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	status = make_session_info_system(mem_ctx, &session_info);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	ZERO_STRUCT(user_handle);

	strlcpy(client_id.addr, "127.0.0.1", sizeof(client_id.addr));
	client_id.name = "127.0.0.1";

	status = rpcint_binding_handle(mem_ctx,
				       &ndr_table_samr,
				       &client_id,
				       session_info,
				       msg_ctx,
				       &h);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	become_root();
	status = samr_find_machine_account(mem_ctx, h, mach_acct,
					   SEC_FLAG_MAXIMUM_ALLOWED,
					   &domain_sid, &user_rid,
					   &user_handle);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	status = dcerpc_samr_QueryUserInfo2(h,
					    mem_ctx,
					    &user_handle,
					    UserControlInformation,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

	acct_ctrl = info->info16.acct_flags;

	if (acct_ctrl & ACB_DISABLED) {
		DEBUG(0,("get_md4pw: Workstation %s: account is disabled\n", mach_acct));
		status = NT_STATUS_ACCOUNT_DISABLED;
		goto out;
	}

	if (!(acct_ctrl & ACB_SVRTRUST) &&
	    !(acct_ctrl & ACB_WSTRUST) &&
	    !(acct_ctrl & ACB_DOMTRUST))
	{
		DEBUG(0,("get_md4pw: Workstation %s: account is not a trust account\n", mach_acct));
		status = NT_STATUS_NO_TRUST_SAM_ACCOUNT;
		goto out;
	}

	switch (sec_chan_type) {
		case SEC_CHAN_BDC:
			if (!(acct_ctrl & ACB_SVRTRUST)) {
				DEBUG(0,("get_md4pw: Workstation %s: BDC secure channel requested "
					 "but not a server trust account\n", mach_acct));
				status = NT_STATUS_NO_TRUST_SAM_ACCOUNT;
				goto out;
			}
			break;
		case SEC_CHAN_WKSTA:
			if (!(acct_ctrl & ACB_WSTRUST)) {
				DEBUG(0,("get_md4pw: Workstation %s: WORKSTATION secure channel requested "
					 "but not a workstation trust account\n", mach_acct));
				status = NT_STATUS_NO_TRUST_SAM_ACCOUNT;
				goto out;
			}
			break;
		case SEC_CHAN_DOMAIN:
			if (!(acct_ctrl & ACB_DOMTRUST)) {
				DEBUG(0,("get_md4pw: Workstation %s: DOMAIN secure channel requested "
					 "but not a interdomain trust account\n", mach_acct));
				status = NT_STATUS_NO_TRUST_SAM_ACCOUNT;
				goto out;
			}
			break;
		default:
			break;
	}

	become_root();
	status = dcerpc_samr_QueryUserInfo2(h,
					    mem_ctx,
					    &user_handle,
					    UserInternal1Information,
					    &info,
					    &result);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

	if (info->info18.nt_pwd_active == 0) {
		DEBUG(0,("get_md4pw: Workstation %s: account does not have a password\n", mach_acct));
		status = NT_STATUS_LOGON_FAILURE;
		goto out;
	}

	/* samr gives out nthash unencrypted (!) */
	memcpy(md4pw->hash, info->info18.nt_pwd.hash, 16);

	sid_compose(sid, domain_sid, user_rid);

 out:
	if (h && is_valid_policy_hnd(&user_handle)) {
		dcerpc_samr_Close(h, mem_ctx, &user_handle, &result);
	}

	talloc_free(mem_ctx);

	return status;
}

/*************************************************************************
 _netr_ServerReqChallenge
 *************************************************************************/

NTSTATUS _netr_ServerReqChallenge(struct pipes_struct *p,
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

NTSTATUS _netr_ServerAuthenticate(struct pipes_struct *p,
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

NTSTATUS _netr_ServerAuthenticate3(struct pipes_struct *p,
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

	switch (p->opnum) {
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
			   &sid, p->msg_ctx);
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

	creds->sid = dom_sid_dup(creds, &sid);
	if (!creds->sid) {
		status = NT_STATUS_NO_MEMORY;
		goto out;
	}

	/* Store off the state so we can continue after client disconnect. */
	become_root();
	status = schannel_save_creds_state(p->mem_ctx, lp_private_dir(), creds);
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

NTSTATUS _netr_ServerAuthenticate2(struct pipes_struct *p,
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
 * If schannel is required for this call test that it actually is available.
 *************************************************************************/
static NTSTATUS schannel_check_required(struct pipe_auth_data *auth_info,
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

/*************************************************************************
 *************************************************************************/

static NTSTATUS netr_creds_server_step_check(struct pipes_struct *p,
					     TALLOC_CTX *mem_ctx,
					     const char *computer_name,
					     struct netr_Authenticator *received_authenticator,
					     struct netr_Authenticator *return_authenticator,
					     struct netlogon_creds_CredentialState **creds_out)
{
	NTSTATUS status;
	bool schannel_global_required = (lp_server_schannel() == true) ? true:false;

	if (creds_out != NULL) {
		*creds_out = NULL;
	}

	if (schannel_global_required) {
		status = schannel_check_required(&p->auth,
						 computer_name,
						 false, false);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}

	status = schannel_check_creds_state(mem_ctx, lp_private_dir(),
					    computer_name, received_authenticator,
					    return_authenticator, creds_out);

	return status;
}

/*************************************************************************
 *************************************************************************/

static NTSTATUS netr_set_machine_account_password(TALLOC_CTX *mem_ctx,
						  struct auth_serversupplied_info *session_info,
						  struct messaging_context *msg_ctx,
						  const char *account_name,
						  struct samr_Password *nt_hash)
{
	NTSTATUS status;
	NTSTATUS result = NT_STATUS_OK;
	struct dcerpc_binding_handle *h = NULL;
	static struct client_address client_id;
	struct policy_handle user_handle;
	uint32_t acct_ctrl;
	union samr_UserInfo *info;
	struct samr_UserInfo18 info18;
	DATA_BLOB in,out;

	ZERO_STRUCT(user_handle);

	strlcpy(client_id.addr, "127.0.0.1", sizeof(client_id.addr));
	client_id.name = "127.0.0.1";

	status = rpcint_binding_handle(mem_ctx,
				       &ndr_table_samr,
				       &client_id,
				       session_info,
				       msg_ctx,
				       &h);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	become_root();
	status = samr_find_machine_account(mem_ctx,
					   h,
					   account_name,
					   SEC_FLAG_MAXIMUM_ALLOWED,
					   NULL,
					   NULL,
					   &user_handle);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}

	status = dcerpc_samr_QueryUserInfo2(h,
					    mem_ctx,
					    &user_handle,
					    UserControlInformation,
					    &info,
					    &result);
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

	acct_ctrl = info->info16.acct_flags;

	if (!(acct_ctrl & ACB_WSTRUST ||
	      acct_ctrl & ACB_SVRTRUST ||
	      acct_ctrl & ACB_DOMTRUST)) {
		status = NT_STATUS_NO_SUCH_USER;
		goto out;
	}

	if (acct_ctrl & ACB_DISABLED) {
		status = NT_STATUS_ACCOUNT_DISABLED;
		goto out;
	}

	ZERO_STRUCT(info18);

	in = data_blob_const(nt_hash->hash, 16);
	out = data_blob_talloc_zero(mem_ctx, 16);
	sess_crypt_blob(&out, &in, &session_info->user_session_key, true);
	memcpy(info18.nt_pwd.hash, out.data, out.length);

	info18.nt_pwd_active = true;

	info->info18 = info18;

	become_root();
	status = dcerpc_samr_SetUserInfo2(h,
					  mem_ctx,
					  &user_handle,
					  UserInternal1Information,
					  info,
					  &result);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		goto out;
	}
	if (!NT_STATUS_IS_OK(result)) {
		status = result;
		goto out;
	}

 out:
	if (h && is_valid_policy_hnd(&user_handle)) {
		dcerpc_samr_Close(h, mem_ctx, &user_handle, &result);
	}

	return status;
}

/*************************************************************************
 _netr_ServerPasswordSet
 *************************************************************************/

NTSTATUS _netr_ServerPasswordSet(struct pipes_struct *p,
				 struct netr_ServerPasswordSet *r)
{
	NTSTATUS status = NT_STATUS_OK;
	int i;
	struct netlogon_creds_CredentialState *creds = NULL;

	DEBUG(5,("_netr_ServerPasswordSet: %d\n", __LINE__));

	become_root();
	status = netr_creds_server_step_check(p, p->mem_ctx,
					      r->in.computer_name,
					      r->in.credential,
					      r->out.return_authenticator,
					      &creds);
	unbecome_root();

	if (!NT_STATUS_IS_OK(status)) {
		const char *computer_name = "<unknown>";

		if (creds != NULL && creds->computer_name != NULL) {
			computer_name = creds->computer_name;
		}
		DEBUG(2,("_netr_ServerPasswordSet: netlogon_creds_server_step failed. Rejecting auth "
			"request from client %s machine account %s\n",
			r->in.computer_name, computer_name));
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

	status = netr_set_machine_account_password(p->mem_ctx,
						   p->session_info,
						   p->msg_ctx,
						   creds->account_name,
						   r->in.new_password);
	return status;
}

/****************************************************************
 _netr_ServerPasswordSet2
****************************************************************/

NTSTATUS _netr_ServerPasswordSet2(struct pipes_struct *p,
				  struct netr_ServerPasswordSet2 *r)
{
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds = NULL;
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
		const char *computer_name = "<unknown>";

		if (creds && creds->computer_name) {
			computer_name = creds->computer_name;
		}
		DEBUG(2,("_netr_ServerPasswordSet2: netlogon_creds_server_step "
			"failed. Rejecting auth request from client %s machine account %s\n",
			r->in.computer_name, computer_name));
		TALLOC_FREE(creds);
		return status;
	}

	memcpy(password_buf.data, r->in.new_password->data, 512);
	SIVAL(password_buf.data, 512, r->in.new_password->length);
	netlogon_creds_arcfour_crypt(creds, password_buf.data, 516);

	if (!extract_pw_from_buffer(p->mem_ctx, password_buf.data, &plaintext)) {
		TALLOC_FREE(creds);
		return NT_STATUS_WRONG_PASSWORD;
	}

	mdfour(nt_hash.hash, plaintext.data, plaintext.length);

	status = netr_set_machine_account_password(p->mem_ctx,
						   p->session_info,
						   p->msg_ctx,
						   creds->account_name,
						   &nt_hash);
	TALLOC_FREE(creds);
	return status;
}

/*************************************************************************
 _netr_LogonSamLogoff
 *************************************************************************/

NTSTATUS _netr_LogonSamLogoff(struct pipes_struct *p,
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

static NTSTATUS _netr_LogonSamLogon_check(const struct netr_LogonSamLogonEx *r)
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
			break;
		case NetlogonValidationSamInfo4: /* 6 */
			if ((pdb_capabilities() & PDB_CAP_ADS) == 0) {
				DEBUG(10,("Not adding validation info level 6 "
				   "without ADS passdb backend\n"));
				return NT_STATUS_INVALID_INFO_CLASS;
			}
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
			break;
		case NetlogonValidationSamInfo4: /* 6 */
			if ((pdb_capabilities() & PDB_CAP_ADS) == 0) {
				DEBUG(10,("Not adding validation info level 6 "
				   "without ADS passdb backend\n"));
				return NT_STATUS_INVALID_INFO_CLASS;
			}
			break;
		default:
			return NT_STATUS_INVALID_INFO_CLASS;
		}

		break;

	case NetlogonGenericInformation:
		if (r->in.logon->generic == NULL) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		/* we don't support this here */
		return NT_STATUS_INVALID_PARAMETER;
#if 0
		switch (r->in.validation_level) {
		/* TODO: case NetlogonValidationGenericInfo: 4 */
		case NetlogonValidationGenericInfo2: /* 5 */
			break;
		default:
			return NT_STATUS_INVALID_INFO_CLASS;
		}

		break;
#endif
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}

/*************************************************************************
 _netr_LogonSamLogon_base
 *************************************************************************/

static NTSTATUS _netr_LogonSamLogon_base(struct pipes_struct *p,
					 struct netr_LogonSamLogonEx *r,
					 struct netlogon_creds_CredentialState *creds)
{
	NTSTATUS status = NT_STATUS_OK;
	union netr_LogonLevel *logon = r->in.logon;
	const char *nt_username, *nt_domain, *nt_workstation;
	struct auth_usersupplied_info *user_info = NULL;
	struct auth_serversupplied_info *server_info = NULL;
	struct auth_context *auth_context = NULL;
	uint8_t pipe_session_key[16];
	bool process_creds = true;
	const char *fn;

	switch (p->opnum) {
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
		const char *workgroup = lp_workgroup();

		status = make_auth_context_fixed(talloc_tos(), &auth_context,
						 logon->network->challenge);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		/* For a network logon, the workstation name comes in with two
		 * backslashes in the front. Strip them if they are there. */

		if (*wksname == '\\') wksname++;
		if (*wksname == '\\') wksname++;

		/* Standard challenge/response authentication */
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

		if (NT_STATUS_IS_OK(status)) {
			status = NTLMv2_RESPONSE_verify_netlogon_creds(
						user_info->client.account_name,
						user_info->client.domain_name,
						user_info->password.response.nt,
						creds, workgroup);
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

		status = make_auth_context_subsystem(talloc_tos(),
						     &auth_context);
		if (!NT_STATUS_IS_OK(status)) {
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

	TALLOC_FREE(auth_context);
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
		struct schannel_state *schannel_auth;
		/* Get the pipe session key from the schannel. */
		if ((p->auth.auth_type != DCERPC_AUTH_TYPE_SCHANNEL)
		    || (p->auth.auth_ctx == NULL)) {
			return NT_STATUS_INVALID_HANDLE;
		}

		schannel_auth = talloc_get_type_abort(p->auth.auth_ctx,
						      struct schannel_state);
		memcpy(pipe_session_key, schannel_auth->creds->session_key, 16);
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
		/* Only allow this if the pipe is protected. */
		if (p->auth.auth_level < DCERPC_AUTH_LEVEL_PRIVACY) {
			DEBUG(0,("netr_Validation6: client %s not using privacy for netlogon\n",
				get_remote_machine_name()));
			status = NT_STATUS_INVALID_PARAMETER;
			break;
		}

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

NTSTATUS _netr_LogonSamLogonWithFlags(struct pipes_struct *p,
				      struct netr_LogonSamLogonWithFlags *r)
{
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds;
	struct netr_LogonSamLogonEx r2;
	struct netr_Authenticator return_authenticator;

	*r->out.authoritative = true;

	r2.in.server_name	= r->in.server_name;
	r2.in.computer_name	= r->in.computer_name;
	r2.in.logon_level	= r->in.logon_level;
	r2.in.logon		= r->in.logon;
	r2.in.validation_level	= r->in.validation_level;
	r2.in.flags		= r->in.flags;
	r2.out.validation	= r->out.validation;
	r2.out.authoritative	= r->out.authoritative;
	r2.out.flags		= r->out.flags;

	status = _netr_LogonSamLogon_check(&r2);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

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

	status = _netr_LogonSamLogon_base(p, &r2, creds);

	*r->out.return_authenticator = return_authenticator;

	return status;
}

/*************************************************************************
 _netr_LogonSamLogon
 *************************************************************************/

NTSTATUS _netr_LogonSamLogon(struct pipes_struct *p,
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

NTSTATUS _netr_LogonSamLogonEx(struct pipes_struct *p,
			       struct netr_LogonSamLogonEx *r)
{
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds = NULL;

	*r->out.authoritative = true;

	status = _netr_LogonSamLogon_check(r);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	/* Only allow this if the pipe is protected. */
	if (p->auth.auth_type != DCERPC_AUTH_TYPE_SCHANNEL) {
		DEBUG(0,("_netr_LogonSamLogonEx: client %s not using schannel for netlogon\n",
			get_remote_machine_name() ));
		return NT_STATUS_INVALID_PARAMETER;
        }

	become_root();
	status = schannel_get_creds_state(p->mem_ctx, lp_private_dir(),
					  r->in.computer_name, &creds);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	status = _netr_LogonSamLogon_base(p, r, creds);
	TALLOC_FREE(creds);

	return status;
}

/*************************************************************************
 _ds_enum_dom_trusts
 *************************************************************************/
#if 0	/* JERRY -- not correct */
 NTSTATUS _ds_enum_dom_trusts(struct pipes_struct *p, DS_Q_ENUM_DOM_TRUSTS *q_u,
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

WERROR _netr_LogonUasLogon(struct pipes_struct *p,
			   struct netr_LogonUasLogon *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_LogonUasLogoff(struct pipes_struct *p,
			    struct netr_LogonUasLogoff *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_DatabaseDeltas(struct pipes_struct *p,
			      struct netr_DatabaseDeltas *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_DatabaseSync(struct pipes_struct *p,
			    struct netr_DatabaseSync *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_AccountDeltas(struct pipes_struct *p,
			     struct netr_AccountDeltas *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_AccountSync(struct pipes_struct *p,
			   struct netr_AccountSync *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
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

WERROR _netr_GetDcName(struct pipes_struct *p,
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
			     p->msg_ctx,
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

WERROR _netr_GetAnyDCName(struct pipes_struct *p,
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
			     p->msg_ctx,
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

NTSTATUS _netr_DatabaseSync2(struct pipes_struct *p,
			     struct netr_DatabaseSync2 *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_DatabaseRedo(struct pipes_struct *p,
			    struct netr_DatabaseRedo *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetDCName(struct pipes_struct *p,
			  struct netr_DsRGetDCName *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_LogonGetCapabilities(struct pipes_struct *p,
				    struct netr_LogonGetCapabilities *r)
{
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONSETSERVICEBITS(struct pipes_struct *p,
				     struct netr_NETRLOGONSETSERVICEBITS *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_LogonGetTrustRid(struct pipes_struct *p,
			      struct netr_LogonGetTrustRid *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONCOMPUTESERVERDIGEST(struct pipes_struct *p,
					  struct netr_NETRLOGONCOMPUTESERVERDIGEST *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONCOMPUTECLIENTDIGEST(struct pipes_struct *p,
					  struct netr_NETRLOGONCOMPUTECLIENTDIGEST *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetDCNameEx(struct pipes_struct *p,
			    struct netr_DsRGetDCNameEx *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetSiteName(struct pipes_struct *p,
			    struct netr_DsRGetSiteName *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_LogonGetDomainInfo(struct pipes_struct *p,
				  struct netr_LogonGetDomainInfo *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_ServerPasswordGet(struct pipes_struct *p,
			       struct netr_ServerPasswordGet *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONSENDTOSAM(struct pipes_struct *p,
				struct netr_NETRLOGONSENDTOSAM *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRAddressToSitenamesW(struct pipes_struct *p,
				    struct netr_DsRAddressToSitenamesW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetDCNameEx2(struct pipes_struct *p,
			     struct netr_DsRGetDCNameEx2 *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NETRLOGONGETTIMESERVICEPARENTDOMAIN(struct pipes_struct *p,
						 struct netr_NETRLOGONGETTIMESERVICEPARENTDOMAIN *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_NetrEnumerateTrustedDomainsEx(struct pipes_struct *p,
					   struct netr_NetrEnumerateTrustedDomainsEx *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRAddressToSitenamesExW(struct pipes_struct *p,
				      struct netr_DsRAddressToSitenamesExW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsrGetDcSiteCoverageW(struct pipes_struct *p,
				   struct netr_DsrGetDcSiteCoverageW *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsrEnumerateDomainTrusts(struct pipes_struct *p,
				      struct netr_DsrEnumerateDomainTrusts *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsrDeregisterDNSHostRecords(struct pipes_struct *p,
					 struct netr_DsrDeregisterDNSHostRecords *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_ServerTrustPasswordsGet(struct pipes_struct *p,
				       struct netr_ServerTrustPasswordsGet *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

WERROR _netr_DsRGetForestTrustInformation(struct pipes_struct *p,
					  struct netr_DsRGetForestTrustInformation *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return WERR_NOT_SUPPORTED;
}

/****************************************************************
****************************************************************/

static NTSTATUS fill_forest_trust_array(TALLOC_CTX *mem_ctx,
					struct lsa_ForestTrustInformation *info)
{
	struct lsa_ForestTrustRecord *e;
	struct pdb_domain_info *dom_info;
	struct lsa_ForestTrustDomainInfo *domain_info;

	dom_info = pdb_get_domain_info(mem_ctx);
	if (dom_info == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	info->count = 2;
	info->entries = talloc_array(info, struct lsa_ForestTrustRecord *, 2);
	if (info->entries == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	e = talloc(info, struct lsa_ForestTrustRecord);
	if (e == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	e->flags = 0;
	e->type = LSA_FOREST_TRUST_TOP_LEVEL_NAME;
	e->time = 0; /* so far always 0 in trces. */
	e->forest_trust_data.top_level_name.string = talloc_steal(info,
								  dom_info->dns_forest);

	info->entries[0] = e;

	e = talloc(info, struct lsa_ForestTrustRecord);
	if (e == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	/* TODO: check if disabled and set flags accordingly */
	e->flags = 0;
	e->type = LSA_FOREST_TRUST_DOMAIN_INFO;
	e->time = 0; /* so far always 0 in traces. */

	domain_info = &e->forest_trust_data.domain_info;
	domain_info->domain_sid = dom_sid_dup(info, &dom_info->sid);

	domain_info->dns_domain_name.string = talloc_steal(info,
							   dom_info->dns_domain);
	domain_info->netbios_domain_name.string = talloc_steal(info,
							       dom_info->name);

	info->entries[1] = e;

	return NT_STATUS_OK;
}

/****************************************************************
 _netr_GetForestTrustInformation
****************************************************************/

NTSTATUS _netr_GetForestTrustInformation(struct pipes_struct *p,
					 struct netr_GetForestTrustInformation *r)
{
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds;
	struct lsa_ForestTrustInformation *info, **info_ptr;

	/* TODO: check server name */

	become_root();
	status = netr_creds_server_step_check(p, p->mem_ctx,
					      r->in.computer_name,
					      r->in.credential,
					      r->out.return_authenticator,
					      &creds);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if ((creds->secure_channel_type != SEC_CHAN_DNS_DOMAIN) &&
	    (creds->secure_channel_type != SEC_CHAN_DOMAIN)) {
		return NT_STATUS_NOT_IMPLEMENTED;
	}

	info_ptr = talloc(p->mem_ctx, struct lsa_ForestTrustInformation *);
	if (!info_ptr) {
		return NT_STATUS_NO_MEMORY;
	}
	info = talloc_zero(info_ptr, struct lsa_ForestTrustInformation);
	if (!info) {
		return NT_STATUS_NO_MEMORY;
	}

	status = fill_forest_trust_array(p->mem_ctx, info);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	*info_ptr = info;
	r->out.forest_trust_info = info_ptr;

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

static NTSTATUS get_password_from_trustAuth(TALLOC_CTX *mem_ctx,
					    const DATA_BLOB *trustAuth_blob,
					    const DATA_BLOB *session_key,
					    struct samr_Password *current_pw_enc,
					    struct samr_Password *previous_pw_enc)
{
	enum ndr_err_code ndr_err;
	struct trustAuthInOutBlob trustAuth;

	ndr_err = ndr_pull_struct_blob_all(trustAuth_blob, mem_ctx, &trustAuth,
					   (ndr_pull_flags_fn_t)ndr_pull_trustAuthInOutBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return NT_STATUS_UNSUCCESSFUL;
	}


	if (trustAuth.count != 0 && trustAuth.current.count != 0 &&
	    trustAuth.current.array[0].AuthType == TRUST_AUTH_TYPE_CLEAR) {
		mdfour(previous_pw_enc->hash,
		       trustAuth.current.array[0].AuthInfo.clear.password,
		       trustAuth.current.array[0].AuthInfo.clear.size);
	} else {
		return NT_STATUS_UNSUCCESSFUL;
	}

	arcfour_crypt_blob(current_pw_enc->hash, sizeof(current_pw_enc->hash),
			   session_key);

	if (trustAuth.previous.count != 0 &&
	    trustAuth.previous.array[0].AuthType == TRUST_AUTH_TYPE_CLEAR) {
		mdfour(previous_pw_enc->hash,
		       trustAuth.previous.array[0].AuthInfo.clear.password,
		       trustAuth.previous.array[0].AuthInfo.clear.size);
	} else {
		mdfour(previous_pw_enc->hash, NULL, 0);
	}
	arcfour_crypt_blob(previous_pw_enc->hash, sizeof(previous_pw_enc->hash),
			   session_key);

	return NT_STATUS_OK;
}

/****************************************************************
 _netr_ServerGetTrustInfo
****************************************************************/

NTSTATUS _netr_ServerGetTrustInfo(struct pipes_struct *p,
				  struct netr_ServerGetTrustInfo *r)
{
	NTSTATUS status;
	struct netlogon_creds_CredentialState *creds;
	char *account_name;
	size_t account_name_last;
	bool trusted;
	struct netr_TrustInfo *trust_info;
	struct pdb_trusted_domain *td;
	DATA_BLOB trustAuth_blob;
	struct samr_Password *new_owf_enc;
	struct samr_Password *old_owf_enc;
	DATA_BLOB session_key;

	/* TODO: check server name */

	become_root();
	status = netr_creds_server_step_check(p, p->mem_ctx,
					      r->in.computer_name,
					      r->in.credential,
					      r->out.return_authenticator,
					      &creds);
	unbecome_root();
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	account_name = talloc_strdup(p->mem_ctx, r->in.account_name);
	if (account_name == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	account_name_last = strlen(account_name);
	if (account_name_last == 0) {
		return NT_STATUS_INVALID_PARAMETER;
	}
	account_name_last--;
	if (account_name[account_name_last] == '.') {
		account_name[account_name_last] = '\0';
	}

	if ((creds->secure_channel_type != SEC_CHAN_DNS_DOMAIN) &&
	    (creds->secure_channel_type != SEC_CHAN_DOMAIN)) {
		trusted = false;
	} else {
		trusted = true;
	}


	if (trusted) {
		account_name_last = strlen(account_name);
		if (account_name_last == 0) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		account_name_last--;
		if (account_name[account_name_last] == '$') {
			account_name[account_name_last] = '\0';
		}

		status = pdb_get_trusted_domain(p->mem_ctx, account_name, &td);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		if (r->out.trust_info != NULL) {
			trust_info = talloc_zero(p->mem_ctx, struct netr_TrustInfo);
			if (trust_info == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
			trust_info->count = 1;

			trust_info->data = talloc_array(trust_info, uint32_t, 1);
			if (trust_info->data == NULL) {
				return NT_STATUS_NO_MEMORY;
			}
			trust_info->data[0] = td->trust_attributes;

			*r->out.trust_info = trust_info;
		}

		new_owf_enc = talloc_zero(p->mem_ctx, struct samr_Password);
		old_owf_enc = talloc_zero(p->mem_ctx, struct samr_Password);
		if (new_owf_enc == NULL || old_owf_enc == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

/* TODO: which trustAuth shall we use if we have in/out trust or do they have to
 * be equal ? */
		if (td->trust_direction & NETR_TRUST_FLAG_INBOUND) {
			trustAuth_blob = td->trust_auth_incoming;
		} else if (td->trust_direction & NETR_TRUST_FLAG_OUTBOUND) {
			trustAuth_blob = td->trust_auth_outgoing;
		}

		session_key.data = creds->session_key;
		session_key.length = sizeof(creds->session_key);
		status = get_password_from_trustAuth(p->mem_ctx, &trustAuth_blob,
						     &session_key,
						     new_owf_enc, old_owf_enc);

		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		r->out.new_owf_password = new_owf_enc;
		r->out.old_owf_password = old_owf_enc;
	} else {
/* TODO: look for machine password */
		r->out.new_owf_password = NULL;
		r->out.old_owf_password = NULL;

		return NT_STATUS_NOT_IMPLEMENTED;
	}

	return NT_STATUS_OK;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_Unused47(struct pipes_struct *p,
			struct netr_Unused47 *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}

/****************************************************************
****************************************************************/

NTSTATUS _netr_DsrUpdateReadOnlyServerDnsRecords(struct pipes_struct *p,
						 struct netr_DsrUpdateReadOnlyServerDnsRecords *r)
{
	p->fault_state = DCERPC_FAULT_OP_RNG_ERROR;
	return NT_STATUS_NOT_IMPLEMENTED;
}
