/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher	2004
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005
   Copyright (C) Brad Henry 2005
 
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
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_drsuapi_c.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "dsdb/samdb/samdb.h"
#include "ldb_wrap.h"
#include "libcli/security/security.h"
#include "auth/credentials/credentials.h"
#include "auth/credentials/credentials_krb5.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "param/param.h"
#include "param/provision.h"
#include "system/kerberos.h"
#include "auth/kerberos/kerberos.h"

/*
 * complete a domain join, when joining to a AD domain:
 * 1.) connect and bind to the DRSUAPI pipe
 * 2.) do a DsCrackNames() to find the machine account dn
 * 3.) connect to LDAP
 * 4.) do an ldap search to find the "msDS-KeyVersionNumber" of the machine account
 * 5.) set the servicePrincipalName's of the machine account via LDAP, (maybe we should use DsWriteAccountSpn()...)
 * 6.) do a DsCrackNames() to find the domain dn
 * 7.) find out Site specific stuff, look at libnet_JoinSite() for details
 */
static NTSTATUS libnet_JoinADSDomain(struct libnet_context *ctx, struct libnet_JoinDomain *r)
{
	NTSTATUS status;

	TALLOC_CTX *tmp_ctx;

	const char *realm = r->out.realm;

	struct dcerpc_binding *samr_binding = r->out.samr_binding;

	struct dcerpc_pipe *drsuapi_pipe;
	struct dcerpc_binding *drsuapi_binding;
	struct drsuapi_DsBind r_drsuapi_bind;
	struct drsuapi_DsCrackNames r_crack_names;
	struct drsuapi_DsNameString names[1];
	struct policy_handle drsuapi_bind_handle;
	struct GUID drsuapi_bind_guid;

	struct ldb_context *remote_ldb;
	struct ldb_dn *account_dn;
	const char *account_dn_str;
	const char *remote_ldb_url;
	struct ldb_result *res;
	struct ldb_message *msg;

	int ret, rtn;

	const char * const attrs[] = {
		"msDS-KeyVersionNumber",
		"servicePrincipalName",
		"dNSHostName",
		"objectGUID",
		NULL,
	};

	r->out.error_string = NULL;
	
	/* We need to convert between a samAccountName and domain to a
	 * DN in the directory.  The correct way to do this is with
	 * DRSUAPI CrackNames */

	/* Fiddle with the bindings, so get to DRSUAPI on
	 * NCACN_IP_TCP, sealed */
	tmp_ctx = talloc_named(r, 0, "libnet_JoinADSDomain temp context");  
	if (!tmp_ctx) {
		r->out.error_string = NULL;
		return NT_STATUS_NO_MEMORY;
	}
	                                           
	drsuapi_binding = talloc_zero(tmp_ctx, struct dcerpc_binding);
	if (!drsuapi_binding) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	
	*drsuapi_binding = *samr_binding;

	/* DRSUAPI is only available on IP_TCP, and locally on NCALRPC */
	if (drsuapi_binding->transport != NCALRPC) {
		drsuapi_binding->transport = NCACN_IP_TCP;
	}
	drsuapi_binding->endpoint = NULL;
	drsuapi_binding->flags |= DCERPC_SEAL;

	status = dcerpc_pipe_connect_b(tmp_ctx, 
				       &drsuapi_pipe,
				       drsuapi_binding,
				       &ndr_table_drsuapi,
				       ctx->cred, 
				       ctx->event_ctx,
				       ctx->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(r,
					"Connection to DRSUAPI pipe of PDC of domain '%s' failed: %s",
					r->out.domain_name,
					nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	}

	/* get a DRSUAPI pipe handle */
	GUID_from_string(DRSUAPI_DS_BIND_GUID, &drsuapi_bind_guid);

	r_drsuapi_bind.in.bind_guid = &drsuapi_bind_guid;
	r_drsuapi_bind.in.bind_info = NULL;
	r_drsuapi_bind.out.bind_handle = &drsuapi_bind_handle;

	status = dcerpc_drsuapi_DsBind_r(drsuapi_pipe->binding_handle, tmp_ctx, &r_drsuapi_bind);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string
			= talloc_asprintf(r,
					  "dcerpc_drsuapi_DsBind failed - %s",
					  nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	} else if (!W_ERROR_IS_OK(r_drsuapi_bind.out.result)) {
		r->out.error_string
				= talloc_asprintf(r,
						  "DsBind failed - %s", 
						  win_errstr(r_drsuapi_bind.out.result));
			talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Actually 'crack' the names */
	ZERO_STRUCT(r_crack_names);
	r_crack_names.in.bind_handle		= &drsuapi_bind_handle;
	r_crack_names.in.level			= 1;
	r_crack_names.in.req			= talloc(r, union drsuapi_DsNameRequest);
	if (!r_crack_names.in.req) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	r_crack_names.in.req->req1.codepage	= 1252; /* western european */
	r_crack_names.in.req->req1.language	= 0x00000407; /* german */
	r_crack_names.in.req->req1.count	= 1;
	r_crack_names.in.req->req1.names	= names;
	r_crack_names.in.req->req1.format_flags	= DRSUAPI_DS_NAME_FLAG_NO_FLAGS;
	r_crack_names.in.req->req1.format_offered = DRSUAPI_DS_NAME_FORMAT_SID_OR_SID_HISTORY;
	r_crack_names.in.req->req1.format_desired = DRSUAPI_DS_NAME_FORMAT_FQDN_1779;
	names[0].str = dom_sid_string(tmp_ctx, r->out.account_sid);
	if (!names[0].str) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	r_crack_names.out.ctr			= talloc(r, union drsuapi_DsNameCtr);
	r_crack_names.out.level_out		= talloc(r, uint32_t);
	if (!r_crack_names.out.ctr || !r_crack_names.out.level_out) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	status = dcerpc_drsuapi_DsCrackNames_r(drsuapi_pipe->binding_handle, tmp_ctx, &r_crack_names);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string
			= talloc_asprintf(r,
					  "dcerpc_drsuapi_DsCrackNames for [%s] failed - %s",
					  names[0].str,
					  nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	} else if (!W_ERROR_IS_OK(r_crack_names.out.result)) {
		r->out.error_string
				= talloc_asprintf(r,
						  "DsCrackNames failed - %s", win_errstr(r_crack_names.out.result));
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	} else if (*r_crack_names.out.level_out != 1
		   || !r_crack_names.out.ctr->ctr1
		   || r_crack_names.out.ctr->ctr1->count != 1) {
		r->out.error_string = talloc_asprintf(r, "DsCrackNames failed");
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_PARAMETER;
	} else if (r_crack_names.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
		r->out.error_string = talloc_asprintf(r, "DsCrackNames failed: %d", r_crack_names.out.ctr->ctr1->array[0].status);
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	} else if (r_crack_names.out.ctr->ctr1->array[0].result_name == NULL) {
		r->out.error_string = talloc_asprintf(r, "DsCrackNames failed: no result name");
		talloc_free(tmp_ctx);
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Store the DN of our machine account. */
	account_dn_str = r_crack_names.out.ctr->ctr1->array[0].result_name;

	/* Now we know the user's DN, open with LDAP, read and modify a few things */

	remote_ldb_url = talloc_asprintf(tmp_ctx, "ldap://%s", 
					 drsuapi_binding->target_hostname);
	if (!remote_ldb_url) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	remote_ldb = ldb_wrap_connect(tmp_ctx, ctx->event_ctx, ctx->lp_ctx,
				      remote_ldb_url, 
				      NULL, ctx->cred, 0);
	if (!remote_ldb) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	account_dn = ldb_dn_new(tmp_ctx, remote_ldb, account_dn_str);
	if (account_dn == NULL) {
		r->out.error_string = talloc_asprintf(r, "Invalid account dn: %s",
						      account_dn_str);
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* search for the user's record */
	ret = ldb_search(remote_ldb, tmp_ctx, &res,
			 account_dn, LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		r->out.error_string = talloc_asprintf(r, "ldb_search for %s failed - %s",
						      account_dn_str, ldb_errstring(remote_ldb));
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	if (res->count != 1) {
		r->out.error_string = talloc_asprintf(r, "ldb_search for %s failed - found %d entries",
						      account_dn_str, res->count);
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Prepare a new message, for the modify */
	msg = ldb_msg_new(tmp_ctx);
	if (!msg) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	msg->dn = res->msgs[0]->dn;

	{
		unsigned int i;
		const char *service_principal_name[2];
		const char *dns_host_name = strlower_talloc(msg,
							    talloc_asprintf(msg, 
									    "%s.%s", 
									    r->in.netbios_name, 
									    realm));

		if (!dns_host_name) {
			r->out.error_string = NULL;
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		service_principal_name[0] = talloc_asprintf(msg, "HOST/%s",
							    dns_host_name);
		service_principal_name[1] = talloc_asprintf(msg, "HOST/%s",
							    r->in.netbios_name);
		
		for (i=0; i < ARRAY_SIZE(service_principal_name); i++) {
			if (!service_principal_name[i]) {
				r->out.error_string = NULL;
				talloc_free(tmp_ctx);
				return NT_STATUS_NO_MEMORY;
			}
			rtn = ldb_msg_add_string(msg, "servicePrincipalName",
						 service_principal_name[i]);
			if (rtn != LDB_SUCCESS) {
				r->out.error_string = NULL;
				talloc_free(tmp_ctx);
				return NT_STATUS_NO_MEMORY;
			}
		}

		rtn = ldb_msg_add_string(msg, "dNSHostName", dns_host_name);
		if (rtn != LDB_SUCCESS) {
			r->out.error_string = NULL;
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}

		rtn = dsdb_replace(remote_ldb, msg, 0);
		if (rtn != LDB_SUCCESS) {
			r->out.error_string
				= talloc_asprintf(r, 
						  "Failed to replace entries on %s", 
						  ldb_dn_get_linearized(msg->dn));
			talloc_free(tmp_ctx);
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}
	}
				
	msg = ldb_msg_new(tmp_ctx);
	if (!msg) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	msg->dn = res->msgs[0]->dn;

	rtn = samdb_msg_add_uint(remote_ldb, msg, msg,
				 "msDS-SupportedEncryptionTypes", ENC_ALL_TYPES);
	if (rtn != LDB_SUCCESS) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	rtn = dsdb_replace(remote_ldb, msg, 0);
	/* The remote server may not support this attribute, if it
	 * isn't a modern schema */
	if (rtn != LDB_SUCCESS && rtn != LDB_ERR_NO_SUCH_ATTRIBUTE) {
		r->out.error_string
			= talloc_asprintf(r,
					  "Failed to replace msDS-SupportedEncryptionTypes on %s",
					  ldb_dn_get_linearized(msg->dn));
		talloc_free(tmp_ctx);
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* DsCrackNames to find out the DN of the domain. */
	r_crack_names.in.req->req1.format_offered = DRSUAPI_DS_NAME_FORMAT_NT4_ACCOUNT;
	r_crack_names.in.req->req1.format_desired = DRSUAPI_DS_NAME_FORMAT_FQDN_1779;
	names[0].str = talloc_asprintf(tmp_ctx, "%s\\", r->out.domain_name);
	if (!names[0].str) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	status = dcerpc_drsuapi_DsCrackNames_r(drsuapi_pipe->binding_handle, tmp_ctx, &r_crack_names);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string
			= talloc_asprintf(r,
					  "dcerpc_drsuapi_DsCrackNames for [%s] failed - %s",
					  r->in.domain_name,
					  nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	} else if (!W_ERROR_IS_OK(r_crack_names.out.result)) {
		r->out.error_string
			= talloc_asprintf(r,
					  "DsCrackNames failed - %s", win_errstr(r_crack_names.out.result));
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	} else if (*r_crack_names.out.level_out != 1
		   || !r_crack_names.out.ctr->ctr1
		   || r_crack_names.out.ctr->ctr1->count != 1
		   || !r_crack_names.out.ctr->ctr1->array[0].result_name
		   || r_crack_names.out.ctr->ctr1->array[0].status != DRSUAPI_DS_NAME_STATUS_OK) {
		r->out.error_string = talloc_asprintf(r, "DsCrackNames failed");
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	/* Store the account DN. */
	r->out.account_dn_str = account_dn_str;
	talloc_steal(r, account_dn_str);

	/* Store the domain DN. */
	r->out.domain_dn_str = r_crack_names.out.ctr->ctr1->array[0].result_name;
	talloc_steal(r, r_crack_names.out.ctr->ctr1->array[0].result_name);

	/* Store the KVNO of the account, critical for some kerberos
	 * operations */
	r->out.kvno = ldb_msg_find_attr_as_uint(res->msgs[0], "msDS-KeyVersionNumber", 0);

	/* Store the account GUID. */
	r->out.account_guid = samdb_result_guid(res->msgs[0], "objectGUID");

	if (r->in.acct_type == ACB_SVRTRUST) {
		status = libnet_JoinSite(ctx, remote_ldb, r);
	}
	talloc_free(tmp_ctx);

	return status;
}

/*
 * do a domain join using DCERPC/SAMR calls
 * - connect to the LSA pipe, to try and find out information about the domain
 * - create a secondary connection to SAMR pipe
 * - do a samr_Connect to get a policy handle
 * - do a samr_LookupDomain to get the domain sid
 * - do a samr_OpenDomain to get a domain handle
 * - do a samr_CreateAccount to try and get a new account 
 * 
 * If that fails, do:
 * - do a samr_LookupNames to get the users rid
 * - do a samr_OpenUser to get a user handle
 * - potentially delete and recreate the user
 * - assert the account is of the right type with samrQueryUserInfo
 * 
 * - call libnet_SetPassword_samr_handle to set the password,
 *   and pass a samr_UserInfo21 struct to set full_name and the account flags
 *
 * - do some ADS specific things when we join as Domain Controller,
 *    look at libnet_joinADSDomain() for the details
 */
NTSTATUS libnet_JoinDomain(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, struct libnet_JoinDomain *r)
{
	TALLOC_CTX *tmp_ctx;

	NTSTATUS status, cu_status;

	struct libnet_RpcConnect *connect_with_info;
	struct dcerpc_pipe *samr_pipe;

	struct samr_Connect sc;
	struct policy_handle p_handle;
	struct samr_OpenDomain od;
	struct policy_handle d_handle;
	struct samr_LookupNames ln;
	struct samr_Ids rids, types;
	struct samr_OpenUser ou;
	struct samr_CreateUser2 cu;
	struct policy_handle *u_handle = NULL;
	struct samr_QueryUserInfo qui;
	union samr_UserInfo *uinfo;
	struct samr_UserInfo21 u_info21;
	union libnet_SetPassword r2;
	struct samr_GetUserPwInfo pwp;
	struct samr_PwInfo info;
	struct lsa_String samr_account_name;
	
	uint32_t acct_flags, old_acct_flags;
	uint32_t rid, access_granted;
	int policy_min_pw_len = 0;

	struct dom_sid *account_sid = NULL;
	const char *password_str = NULL;
	
	r->out.error_string = NULL;
	r2.samr_handle.out.error_string = NULL;
	
	tmp_ctx = talloc_named(mem_ctx, 0, "libnet_Join temp context");
	if (!tmp_ctx) {
		r->out.error_string = NULL;
		return NT_STATUS_NO_MEMORY;
	}
	
	u_handle = talloc(tmp_ctx, struct policy_handle);
	if (!u_handle) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	
	connect_with_info = talloc_zero(tmp_ctx, struct libnet_RpcConnect);
	if (!connect_with_info) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* prepare connect to the SAMR pipe of PDC */
	if (r->in.level == LIBNET_JOINDOMAIN_AUTOMATIC) {
		connect_with_info->in.binding = NULL;
		connect_with_info->in.name    = r->in.domain_name;
	} else {
		connect_with_info->in.binding = r->in.binding;
		connect_with_info->in.name    = NULL;
	}

	/* This level makes a connection to the LSA pipe on the way,
	 * to get some useful bits of information about the domain */
	connect_with_info->level              = LIBNET_RPC_CONNECT_DC_INFO;
	connect_with_info->in.dcerpc_iface    = &ndr_table_samr;

	/*
	  establish the SAMR connection
	*/
	status = libnet_RpcConnect(ctx, tmp_ctx, connect_with_info);
	if (!NT_STATUS_IS_OK(status)) {
		if (r->in.binding) {
			r->out.error_string = talloc_asprintf(mem_ctx,
							      "Connection to SAMR pipe of DC %s failed: %s",
							      r->in.binding, connect_with_info->out.error_string);
		} else {
			r->out.error_string = talloc_asprintf(mem_ctx,
							      "Connection to SAMR pipe of PDC for %s failed: %s",
							      r->in.domain_name, connect_with_info->out.error_string);
		}
		talloc_free(tmp_ctx);
		return status;
	}

	samr_pipe = connect_with_info->out.dcerpc_pipe;

	status = dcerpc_pipe_auth(tmp_ctx, &samr_pipe,
				  connect_with_info->out.dcerpc_pipe->binding, 
				  &ndr_table_samr, ctx->cred, ctx->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						"SAMR bind failed: %s",
						nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	}

	/* prepare samr_Connect */
	ZERO_STRUCT(p_handle);
	sc.in.system_name = NULL;
	sc.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	sc.out.connect_handle = &p_handle;

	/* 2. do a samr_Connect to get a policy handle */
	status = dcerpc_samr_Connect_r(samr_pipe->binding_handle, tmp_ctx, &sc);
	if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(sc.out.result)) {
		status = sc.out.result;
	}
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						"samr_Connect failed: %s",
						nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	}

	/* If this is a connection on ncacn_ip_tcp to Win2k3 SP1, we don't get back this useful info */
	if (!connect_with_info->out.domain_name) {
		if (r->in.level == LIBNET_JOINDOMAIN_AUTOMATIC) {
			connect_with_info->out.domain_name = talloc_strdup(tmp_ctx, r->in.domain_name);
		} else {
			/* Bugger, we just lost our way to automatically find the domain name */
			connect_with_info->out.domain_name = talloc_strdup(tmp_ctx, lpcfg_workgroup(ctx->lp_ctx));
			connect_with_info->out.realm = talloc_strdup(tmp_ctx, lpcfg_realm(ctx->lp_ctx));
		}
	}

	/* Perhaps we didn't get a SID above, because we are against ncacn_ip_tcp */
	if (!connect_with_info->out.domain_sid) {
		struct lsa_String name;
		struct samr_LookupDomain l;
		struct dom_sid2 *sid = NULL;
		name.string = connect_with_info->out.domain_name;
		l.in.connect_handle = &p_handle;
		l.in.domain_name = &name;
		l.out.sid = &sid;
		
		status = dcerpc_samr_LookupDomain_r(samr_pipe->binding_handle, tmp_ctx, &l);
		if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(l.out.result)) {
			status = l.out.result;
		}
		if (!NT_STATUS_IS_OK(status)) {
			r->out.error_string = talloc_asprintf(mem_ctx,
							      "SAMR LookupDomain failed: %s",
							      nt_errstr(status));
			talloc_free(tmp_ctx);
			return status;
		}
		connect_with_info->out.domain_sid = *l.out.sid;
	}

	/* prepare samr_OpenDomain */
	ZERO_STRUCT(d_handle);
	od.in.connect_handle = &p_handle;
	od.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	od.in.sid = connect_with_info->out.domain_sid;
	od.out.domain_handle = &d_handle;

	/* do a samr_OpenDomain to get a domain handle */
	status = dcerpc_samr_OpenDomain_r(samr_pipe->binding_handle, tmp_ctx, &od);
	if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(od.out.result)) {
		status = od.out.result;
	}
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "samr_OpenDomain for [%s] failed: %s",
						      dom_sid_string(tmp_ctx, connect_with_info->out.domain_sid),
						      nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	}
	
	/* prepare samr_CreateUser2 */
	ZERO_STRUCTP(u_handle);
	cu.in.domain_handle  = &d_handle;
	cu.in.access_mask     = SEC_FLAG_MAXIMUM_ALLOWED;
	samr_account_name.string = r->in.account_name;
	cu.in.account_name    = &samr_account_name;
	cu.in.acct_flags      = r->in.acct_type;
	cu.out.user_handle    = u_handle;
	cu.out.rid            = &rid;
	cu.out.access_granted = &access_granted;

	/* do a samr_CreateUser2 to get an account handle, or an error */
	cu_status = dcerpc_samr_CreateUser2_r(samr_pipe->binding_handle, tmp_ctx, &cu);
	if (NT_STATUS_IS_OK(cu_status) && !NT_STATUS_IS_OK(cu.out.result)) {
		cu_status = cu.out.result;
	}
	status = cu_status;
	if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
		/* prepare samr_LookupNames */
		ln.in.domain_handle = &d_handle;
		ln.in.num_names = 1;
		ln.in.names = talloc_array(tmp_ctx, struct lsa_String, 1);
		ln.out.rids = &rids;
		ln.out.types = &types;
		if (!ln.in.names) {
			r->out.error_string = NULL;
			talloc_free(tmp_ctx);
			return NT_STATUS_NO_MEMORY;
		}
		ln.in.names[0].string = r->in.account_name;
		
		/* 5. do a samr_LookupNames to get the users rid */
		status = dcerpc_samr_LookupNames_r(samr_pipe->binding_handle, tmp_ctx, &ln);
		if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(ln.out.result)) {
			status = ln.out.result;
		}
		if (!NT_STATUS_IS_OK(status)) {
			r->out.error_string = talloc_asprintf(mem_ctx,
						"samr_LookupNames for [%s] failed: %s",
						r->in.account_name,
						nt_errstr(status));
			talloc_free(tmp_ctx);
			return status;
		}
		
		/* check if we got one RID for the user */
		if (ln.out.rids->count != 1) {
			r->out.error_string = talloc_asprintf(mem_ctx,
							      "samr_LookupNames for [%s] returns %d RIDs",
							      r->in.account_name, ln.out.rids->count);
			talloc_free(tmp_ctx);
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}

		if (ln.out.types->count != 1) {
			r->out.error_string = talloc_asprintf(mem_ctx,
								"samr_LookupNames for [%s] returns %d RID TYPEs",
								r->in.account_name, ln.out.types->count);
			talloc_free(tmp_ctx);
			return NT_STATUS_INVALID_NETWORK_RESPONSE;
		}

		/* prepare samr_OpenUser */
		ZERO_STRUCTP(u_handle);
		ou.in.domain_handle = &d_handle;
		ou.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		ou.in.rid = ln.out.rids->ids[0];
		rid = ou.in.rid;
		ou.out.user_handle = u_handle;
		
		/* 6. do a samr_OpenUser to get a user handle */
		status = dcerpc_samr_OpenUser_r(samr_pipe->binding_handle, tmp_ctx, &ou);
		if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(ou.out.result)) {
			status = ou.out.result;
		}
		if (!NT_STATUS_IS_OK(status)) {
			r->out.error_string = talloc_asprintf(mem_ctx,
							"samr_OpenUser for [%s] failed: %s",
							r->in.account_name,
							nt_errstr(status));
			talloc_free(tmp_ctx);
			return status;
		}

		if (r->in.recreate_account) {
			struct samr_DeleteUser d;
			d.in.user_handle = u_handle;
			d.out.user_handle = u_handle;
			status = dcerpc_samr_DeleteUser_r(samr_pipe->binding_handle, mem_ctx, &d);
			if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(d.out.result)) {
				status = d.out.result;
			}
			if (!NT_STATUS_IS_OK(status)) {
				r->out.error_string = talloc_asprintf(mem_ctx,
								      "samr_DeleteUser (for recreate) of [%s] failed: %s",
								      r->in.account_name,
								      nt_errstr(status));
				talloc_free(tmp_ctx);
				return status;
			}

			/* We want to recreate, so delete and another samr_CreateUser2 */
			
			/* &cu filled in above */
			status = dcerpc_samr_CreateUser2_r(samr_pipe->binding_handle, tmp_ctx, &cu);
			if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(cu.out.result)) {
				status = cu.out.result;
			}
			if (!NT_STATUS_IS_OK(status)) {
				r->out.error_string = talloc_asprintf(mem_ctx,
								      "samr_CreateUser2 (recreate) for [%s] failed: %s",
								      r->in.account_name, nt_errstr(status));
				talloc_free(tmp_ctx);
				return status;
			}
		}
	} else if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						      "samr_CreateUser2 for [%s] failed: %s",
						      r->in.account_name, nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	}

	/* prepare samr_QueryUserInfo (get flags) */
	qui.in.user_handle = u_handle;
	qui.in.level = 16;
	qui.out.info = &uinfo;
	
	status = dcerpc_samr_QueryUserInfo_r(samr_pipe->binding_handle, tmp_ctx, &qui);
	if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(qui.out.result)) {
		status = qui.out.result;
	}
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_asprintf(mem_ctx,
						"samr_QueryUserInfo for [%s] failed: %s",
						r->in.account_name,
						nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	}
	
	if (!uinfo) {
		status = NT_STATUS_INVALID_PARAMETER;
		r->out.error_string
			= talloc_asprintf(mem_ctx,
					  "samr_QueryUserInfo failed to return qui.out.info for [%s]: %s",
					  r->in.account_name, nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	}

	old_acct_flags = (uinfo->info16.acct_flags & (ACB_WSTRUST | ACB_SVRTRUST | ACB_DOMTRUST));
	/* Possibly bail if the account is of the wrong type */
	if (old_acct_flags
	    != r->in.acct_type) {
		const char *old_account_type, *new_account_type;
		switch (old_acct_flags) {
		case ACB_WSTRUST:
			old_account_type = "domain member (member)";
			break;
		case ACB_SVRTRUST:
			old_account_type = "domain controller (bdc)";
			break;
		case ACB_DOMTRUST:
			old_account_type = "trusted domain";
			break;
		default:
			return NT_STATUS_INVALID_PARAMETER;
		}
		switch (r->in.acct_type) {
		case ACB_WSTRUST:
			new_account_type = "domain member (member)";
			break;
		case ACB_SVRTRUST:
			new_account_type = "domain controller (bdc)";
			break;
		case ACB_DOMTRUST:
			new_account_type = "trusted domain";
			break;
		default:
			return NT_STATUS_INVALID_PARAMETER;
		}

		if (!NT_STATUS_EQUAL(cu_status, NT_STATUS_USER_EXISTS)) {
			/* We created a new user, but they didn't come out the right type?!? */
			r->out.error_string
				= talloc_asprintf(mem_ctx,
						  "We asked to create a new machine account (%s) of type %s, "
						  "but we got an account of type %s.  This is unexpected.  "
						  "Perhaps delete the account and try again.",
						  r->in.account_name, new_account_type, old_account_type);
			talloc_free(tmp_ctx);
			return NT_STATUS_INVALID_PARAMETER;
		} else {
			/* The account is of the wrong type, so bail */

			/* TODO: We should allow a --force option to override, and redo this from the top setting r.in.recreate_account */
			r->out.error_string
				= talloc_asprintf(mem_ctx,
						  "The machine account (%s) already exists in the domain %s, "
						  "but is a %s.  You asked to join as a %s.  Please delete "
						  "the account and try again.",
						  r->in.account_name, connect_with_info->out.domain_name, old_account_type, new_account_type);
			talloc_free(tmp_ctx);
			return NT_STATUS_USER_EXISTS;
		}
	} else {
		acct_flags = uinfo->info16.acct_flags;
	}
	
	acct_flags = (acct_flags & ~(ACB_DISABLED|ACB_PWNOTREQ));

	/* Find out what password policy this user has */
	pwp.in.user_handle = u_handle;
	pwp.out.info = &info;

	status = dcerpc_samr_GetUserPwInfo_r(samr_pipe->binding_handle, tmp_ctx, &pwp);
	if (NT_STATUS_IS_OK(status) && !NT_STATUS_IS_OK(pwp.out.result)) {
		status = pwp.out.result;
	}
	if (NT_STATUS_IS_OK(status)) {
		policy_min_pw_len = pwp.out.info->min_password_length;
	}
	
	/* Grab a password of that minimum length */
	
	password_str = generate_random_password(tmp_ctx, MAX(8, policy_min_pw_len), 255);

	/* set full_name and reset flags */
	ZERO_STRUCT(u_info21);
	u_info21.full_name.string	= r->in.account_name;
	u_info21.acct_flags		= acct_flags;
	u_info21.fields_present		= SAMR_FIELD_FULL_NAME | SAMR_FIELD_ACCT_FLAGS;

	r2.samr_handle.level		= LIBNET_SET_PASSWORD_SAMR_HANDLE;
	r2.samr_handle.in.account_name	= r->in.account_name;
	r2.samr_handle.in.newpassword	= password_str;
	r2.samr_handle.in.user_handle   = u_handle;
	r2.samr_handle.in.dcerpc_pipe   = samr_pipe;
	r2.samr_handle.in.info21	= &u_info21;

	status = libnet_SetPassword(ctx, tmp_ctx, &r2);	
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_steal(mem_ctx, r2.samr_handle.out.error_string);
		talloc_free(tmp_ctx);
		return status;
	}

	account_sid = dom_sid_add_rid(mem_ctx, connect_with_info->out.domain_sid, rid);
	if (!account_sid) {
		r->out.error_string = NULL;
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	/* Finish out by pushing various bits of status data out for the caller to use */
	r->out.join_password = password_str;
	talloc_steal(mem_ctx, r->out.join_password);

	r->out.domain_sid = connect_with_info->out.domain_sid;
	talloc_steal(mem_ctx, r->out.domain_sid);

	r->out.account_sid = account_sid;
	talloc_steal(mem_ctx, r->out.account_sid);

	r->out.domain_name = connect_with_info->out.domain_name;
	talloc_steal(mem_ctx, r->out.domain_name);
	r->out.realm = connect_with_info->out.realm;
	talloc_steal(mem_ctx, r->out.realm);
	r->out.samr_pipe = samr_pipe;
	talloc_reparent(tmp_ctx, mem_ctx, samr_pipe);
	r->out.samr_binding = samr_pipe->binding;
	talloc_steal(mem_ctx, r->out.samr_binding);
	r->out.user_handle = u_handle;
	talloc_steal(mem_ctx, u_handle);
	r->out.error_string = r2.samr_handle.out.error_string;
	talloc_steal(mem_ctx, r2.samr_handle.out.error_string);
	r->out.kvno = 0;
	r->out.server_dn_str = NULL;
	talloc_free(tmp_ctx); 

	/* Now, if it was AD, then we want to start looking changing a
	 * few more things.  Otherwise, we are done. */
	if (r->out.realm) {
		status = libnet_JoinADSDomain(ctx, r);
		return status;
	}

	return status;
}

static NTSTATUS libnet_Join_primary_domain(struct libnet_context *ctx, 
					   TALLOC_CTX *mem_ctx, 
					   struct libnet_Join *r)
{
	NTSTATUS status;
	TALLOC_CTX *tmp_mem;
	struct libnet_JoinDomain *r2;
	struct provision_store_self_join_settings *set_secrets;
	uint32_t acct_type = 0;
	const char *account_name;
	const char *netbios_name;
	const char *error_string;

	r->out.error_string = NULL;

	tmp_mem = talloc_new(mem_ctx);
	if (!tmp_mem) {
		return NT_STATUS_NO_MEMORY;
	}

	r2 = talloc(tmp_mem, struct libnet_JoinDomain);
	if (!r2) {
		r->out.error_string = NULL;
		talloc_free(tmp_mem);
		return NT_STATUS_NO_MEMORY;
	}
	
	if (r->in.join_type == SEC_CHAN_BDC) {
		acct_type = ACB_SVRTRUST;
	} else if (r->in.join_type == SEC_CHAN_WKSTA) {
		acct_type = ACB_WSTRUST;
	} else {
		r->out.error_string = NULL;
		talloc_free(tmp_mem);	
		return NT_STATUS_INVALID_PARAMETER;
	}

	if (r->in.netbios_name != NULL) {
		netbios_name = r->in.netbios_name;
	} else {
		netbios_name = talloc_strdup(tmp_mem, lpcfg_netbios_name(ctx->lp_ctx));
		if (!netbios_name) {
			r->out.error_string = NULL;
			talloc_free(tmp_mem);
			return NT_STATUS_NO_MEMORY;
		}
	}

	account_name = talloc_asprintf(tmp_mem, "%s$", netbios_name);
	if (!account_name) {
		r->out.error_string = NULL;
		talloc_free(tmp_mem);
		return NT_STATUS_NO_MEMORY;
	}
	
	/*
	 * join the domain
	 */
	ZERO_STRUCTP(r2);
	r2->in.domain_name	= r->in.domain_name;
	r2->in.account_name	= account_name;
	r2->in.netbios_name	= netbios_name;
	r2->in.level		= LIBNET_JOINDOMAIN_AUTOMATIC;
	r2->in.acct_type	= acct_type;
	r2->in.recreate_account = false;
	status = libnet_JoinDomain(ctx, r2, r2);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_steal(mem_ctx, r2->out.error_string);
		talloc_free(tmp_mem);
		return status;
	}

	set_secrets = talloc(tmp_mem, struct provision_store_self_join_settings);
	if (!set_secrets) {
		r->out.error_string = NULL;
		talloc_free(tmp_mem);
		return NT_STATUS_NO_MEMORY;
	}
	
	ZERO_STRUCTP(set_secrets);
	set_secrets->domain_name = r2->out.domain_name;
	set_secrets->realm = r2->out.realm;
	set_secrets->netbios_name = netbios_name;
	set_secrets->secure_channel_type = r->in.join_type;
	set_secrets->machine_password = r2->out.join_password;
	set_secrets->key_version_number = r2->out.kvno;
	set_secrets->domain_sid = r2->out.domain_sid;
	
	status = provision_store_self_join(ctx, ctx->lp_ctx, ctx->event_ctx, set_secrets, &error_string);
	if (!NT_STATUS_IS_OK(status)) {
		r->out.error_string = talloc_steal(mem_ctx, error_string);
		talloc_free(tmp_mem);
		return status;
	}

	/* move all out parameter to the callers TALLOC_CTX */
	r->out.error_string	= NULL;
	r->out.join_password	= r2->out.join_password;
	talloc_reparent(r2, mem_ctx, r2->out.join_password);
	r->out.domain_sid	= r2->out.domain_sid;
	talloc_reparent(r2, mem_ctx, r2->out.domain_sid);
	r->out.domain_name      = r2->out.domain_name;
	talloc_reparent(r2, mem_ctx, r2->out.domain_name);
	talloc_free(tmp_mem);
	return NT_STATUS_OK;
}

NTSTATUS libnet_Join(struct libnet_context *ctx, TALLOC_CTX *mem_ctx, struct libnet_Join *r)
{
	switch (r->in.join_type) {
		case SEC_CHAN_WKSTA:
			return libnet_Join_primary_domain(ctx, mem_ctx, r);
		case SEC_CHAN_BDC:
			return libnet_Join_primary_domain(ctx, mem_ctx, r);
		case SEC_CHAN_DOMAIN:
		case SEC_CHAN_DNS_DOMAIN:
		case SEC_CHAN_NULL:
			break;
	}

	r->out.error_string = talloc_asprintf(mem_ctx,
					      "Invalid join type specified (%08X) attempting to join domain %s",
					      r->in.join_type, r->in.domain_name);
	return NT_STATUS_INVALID_PARAMETER;
}
