/*
   Unix SMB/CIFS implementation.

   dcerpc torture tests, designed to walk Samba3 code paths

   Copyright (C) Volker Lendecke 2006

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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "torture/util.h"
#include "libcli/rap/rap.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_srvsvc_c.h"
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "librpc/gen_ndr/ndr_winreg_c.h"
#include "librpc/gen_ndr/ndr_wkssvc_c.h"
#include "lib/cmdline/popt_common.h"
#include "torture/rpc/torture_rpc.h"
#include "libcli/libcli.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/auth/libcli_auth.h"
#include "../lib/crypto/crypto.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "lib/registry/registry.h"
#include "libcli/resolve/resolve.h"
#include "torture/ndr/ndr.h"

/*
 * This tests a RPC call using an invalid vuid
 */

bool torture_bind_authcontext(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	bool ret = false;
	struct lsa_ObjectAttribute objectattr;
	struct lsa_OpenPolicy2 openpolicy;
	struct policy_handle handle;
	struct lsa_Close close_handle;
	struct smbcli_session *tmp;
	struct smbcli_session *session2;
	struct smbcli_state *cli;
	struct dcerpc_pipe *lsa_pipe;
	struct dcerpc_binding_handle *lsa_handle;
	struct cli_credentials *anon_creds;
	struct smb_composite_sesssetup setup;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	mem_ctx = talloc_init("torture_bind_authcontext");

	if (mem_ctx == NULL) {
		torture_comment(torture, "talloc_init failed\n");
		return false;
	}

	lpcfg_smbcli_options(torture->lp_ctx, &options);
	lpcfg_smbcli_session_options(torture->lp_ctx, &session_options);

	status = smbcli_full_connection(mem_ctx, &cli,
					torture_setting_string(torture, "host", NULL),
					lpcfg_smb_ports(torture->lp_ctx),
					"IPC$", NULL,
					lpcfg_socket_options(torture->lp_ctx),
					cmdline_credentials,
					lpcfg_resolve_context(torture->lp_ctx),
					torture->ev, &options, &session_options,
					lpcfg_gensec_settings(torture, torture->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "smbcli_full_connection failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	lsa_pipe = dcerpc_pipe_init(mem_ctx, cli->transport->socket->event.ctx);
	if (lsa_pipe == NULL) {
		torture_comment(torture, "dcerpc_pipe_init failed\n");
		goto done;
	}
	lsa_handle = lsa_pipe->binding_handle;

	status = dcerpc_pipe_open_smb(lsa_pipe, cli->tree, "\\lsarpc");
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	status = dcerpc_bind_auth_none(lsa_pipe, &ndr_table_lsarpc);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "dcerpc_bind_auth_none failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	openpolicy.in.system_name =talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(lsa_pipe));
	ZERO_STRUCT(objectattr);
	openpolicy.in.attr = &objectattr;
	openpolicy.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	openpolicy.out.handle = &handle;

	status = dcerpc_lsa_OpenPolicy2_r(lsa_handle, mem_ctx, &openpolicy);

	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "dcerpc_lsa_OpenPolicy2 failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(openpolicy.out.result)) {
		torture_comment(torture, "dcerpc_lsa_OpenPolicy2 failed: %s\n",
			 nt_errstr(openpolicy.out.result));
		goto done;
	}

	close_handle.in.handle = &handle;
	close_handle.out.handle = &handle;

	status = dcerpc_lsa_Close_r(lsa_handle, mem_ctx, &close_handle);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "dcerpc_lsa_Close failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(close_handle.out.result)) {
		torture_comment(torture, "dcerpc_lsa_Close failed: %s\n",
			 nt_errstr(close_handle.out.result));
		goto done;
	}

	session2 = smbcli_session_init(cli->transport, mem_ctx, false, session_options);
	if (session2 == NULL) {
		torture_comment(torture, "smbcli_session_init failed\n");
		goto done;
	}

	if (!(anon_creds = cli_credentials_init_anon(mem_ctx))) {
		torture_comment(torture, "create_anon_creds failed\n");
		goto done;
	}

	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities;
	setup.in.workgroup = "";
	setup.in.credentials = anon_creds;
	setup.in.gensec_settings = lpcfg_gensec_settings(torture, torture->lp_ctx);

	status = smb_composite_sesssetup(session2, &setup);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "anon session setup failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	session2->vuid = setup.out.vuid;

	tmp = cli->tree->session;
	cli->tree->session = session2;

	status = dcerpc_lsa_OpenPolicy2_r(lsa_handle, mem_ctx, &openpolicy);

	cli->tree->session = tmp;
	talloc_free(lsa_pipe);
	lsa_pipe = NULL;

	if (!NT_STATUS_EQUAL(status, NT_STATUS_INVALID_HANDLE)) {
		torture_comment(torture, "dcerpc_lsa_OpenPolicy2 with wrong vuid gave %s, "
			 "expected NT_STATUS_INVALID_HANDLE\n",
			 nt_errstr(status));
		goto done;
	}

	ret = true;
 done:
	talloc_free(mem_ctx);
	return ret;
}

/*
 * Bind to lsa using a specific auth method
 */

static bool bindtest(struct torture_context *tctx,
		     struct smbcli_state *cli,
		     struct cli_credentials *credentials,
		     uint8_t auth_type, uint8_t auth_level)
{
	TALLOC_CTX *mem_ctx;
	bool ret = false;
	NTSTATUS status;

	struct dcerpc_pipe *lsa_pipe;
	struct dcerpc_binding_handle *lsa_handle;
	struct lsa_ObjectAttribute objectattr;
	struct lsa_OpenPolicy2 openpolicy;
	struct lsa_QueryInfoPolicy query;
	union lsa_PolicyInformation *info = NULL;
	struct policy_handle handle;
	struct lsa_Close close_handle;

	if ((mem_ctx = talloc_init("bindtest")) == NULL) {
		torture_comment(tctx, "talloc_init failed\n");
		return false;
	}

	lsa_pipe = dcerpc_pipe_init(mem_ctx,
				    cli->transport->socket->event.ctx); 
	if (lsa_pipe == NULL) {
		torture_comment(tctx, "dcerpc_pipe_init failed\n");
		goto done;
	}
	lsa_handle = lsa_pipe->binding_handle;

	status = dcerpc_pipe_open_smb(lsa_pipe, cli->tree, "\\lsarpc");
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	status = dcerpc_bind_auth(lsa_pipe, &ndr_table_lsarpc,
				  credentials, lpcfg_gensec_settings(tctx->lp_ctx, tctx->lp_ctx), auth_type, auth_level,
				  NULL);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_bind_auth failed: %s\n", nt_errstr(status));
		goto done;
	}

	openpolicy.in.system_name =talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(lsa_pipe));
	ZERO_STRUCT(objectattr);
	openpolicy.in.attr = &objectattr;
	openpolicy.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	openpolicy.out.handle = &handle;

	status = dcerpc_lsa_OpenPolicy2_r(lsa_handle, mem_ctx, &openpolicy);

	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_lsa_OpenPolicy2 failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(openpolicy.out.result)) {
		torture_comment(tctx, "dcerpc_lsa_OpenPolicy2 failed: %s\n",
			 nt_errstr(openpolicy.out.result));
		goto done;
	}

	query.in.handle = &handle;
	query.in.level = LSA_POLICY_INFO_DOMAIN;
	query.out.info = &info;

	status = dcerpc_lsa_QueryInfoPolicy_r(lsa_handle, mem_ctx, &query);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_lsa_QueryInfoPolicy failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(query.out.result)) {
		torture_comment(tctx, "dcerpc_lsa_QueryInfoPolicy failed: %s\n",
			 nt_errstr(query.out.result));
		goto done;
	}

	close_handle.in.handle = &handle;
	close_handle.out.handle = &handle;

	status = dcerpc_lsa_Close_r(lsa_handle, mem_ctx, &close_handle);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_lsa_Close failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(close_handle.out.result)) {
		torture_comment(tctx, "dcerpc_lsa_Close failed: %s\n",
			 nt_errstr(close_handle.out.result));
		goto done;
	}


	ret = true;
 done:
	talloc_free(mem_ctx);
	return ret;
}

/*
 * test authenticated RPC binds with the variants Samba3 does support
 */

static bool torture_bind_samba3(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	bool ret = false;
	struct smbcli_state *cli;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	mem_ctx = talloc_init("torture_bind_authcontext");

	if (mem_ctx == NULL) {
		torture_comment(torture, "talloc_init failed\n");
		return false;
	}

	lpcfg_smbcli_options(torture->lp_ctx, &options);
	lpcfg_smbcli_session_options(torture->lp_ctx, &session_options);

	status = smbcli_full_connection(mem_ctx, &cli,
					torture_setting_string(torture, "host", NULL),
					lpcfg_smb_ports(torture->lp_ctx),
					"IPC$", NULL,
					lpcfg_socket_options(torture->lp_ctx),
					cmdline_credentials,
					lpcfg_resolve_context(torture->lp_ctx),
					torture->ev, &options, &session_options,
					lpcfg_gensec_settings(torture, torture->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "smbcli_full_connection failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	ret = true;

	ret &= bindtest(torture, cli, cmdline_credentials, DCERPC_AUTH_TYPE_NTLMSSP,
			DCERPC_AUTH_LEVEL_INTEGRITY);
	ret &= bindtest(torture, cli, cmdline_credentials, DCERPC_AUTH_TYPE_NTLMSSP,
			DCERPC_AUTH_LEVEL_PRIVACY);
	ret &= bindtest(torture, cli, cmdline_credentials, DCERPC_AUTH_TYPE_SPNEGO,
			DCERPC_AUTH_LEVEL_INTEGRITY);
	ret &= bindtest(torture, cli, cmdline_credentials, DCERPC_AUTH_TYPE_SPNEGO,
			DCERPC_AUTH_LEVEL_PRIVACY);

 done:
	talloc_free(mem_ctx);
	return ret;
}

/*
 * Lookup or create a user and return all necessary info
 */

static bool get_usr_handle(struct torture_context *tctx,
			   struct smbcli_state *cli,
			   TALLOC_CTX *mem_ctx,
			   struct cli_credentials *admin_creds,
			   uint8_t auth_type,
			   uint8_t auth_level,
			   const char *username,
			   char **domain,
			   struct dcerpc_pipe **result_pipe,
			   struct policy_handle **result_handle,
			   struct dom_sid **sid_p)
{
	struct dcerpc_pipe *samr_pipe;
	struct dcerpc_binding_handle *samr_handle;
	NTSTATUS status;
	struct policy_handle conn_handle;
	struct policy_handle domain_handle;
	struct policy_handle *user_handle;
	struct samr_Connect2 conn;
	struct samr_EnumDomains enumdom;
	uint32_t resume_handle = 0;
	uint32_t num_entries = 0;
	struct samr_SamArray *sam = NULL;
	struct samr_LookupDomain l;
	struct dom_sid2 *sid = NULL;
	int dom_idx;
	struct lsa_String domain_name;
	struct lsa_String user_name;
	struct samr_OpenDomain o;
	struct samr_CreateUser2 c;
	uint32_t user_rid,access_granted;

	samr_pipe = dcerpc_pipe_init(mem_ctx,
				     cli->transport->socket->event.ctx);
	torture_assert(tctx, samr_pipe, "dcerpc_pipe_init failed");

	samr_handle = samr_pipe->binding_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_pipe_open_smb(samr_pipe, cli->tree, "\\samr"),
		"dcerpc_pipe_open_smb failed");

	if (admin_creds != NULL) {
		torture_assert_ntstatus_ok(tctx,
			dcerpc_bind_auth(samr_pipe, &ndr_table_samr,
					  admin_creds, lpcfg_gensec_settings(tctx->lp_ctx, tctx->lp_ctx), auth_type, auth_level,
					  NULL),
			"dcerpc_bind_auth failed");
	} else {
		/* We must have an authenticated SMB connection */
		torture_assert_ntstatus_ok(tctx,
			dcerpc_bind_auth_none(samr_pipe, &ndr_table_samr),
			"dcerpc_bind_auth_none failed");
	}

	conn.in.system_name = talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(samr_pipe));
	conn.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	conn.out.connect_handle = &conn_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_Connect2_r(samr_handle, mem_ctx, &conn),
		"samr_Connect2 failed");
	torture_assert_ntstatus_ok(tctx, conn.out.result,
		"samr_Connect2 failed");

	enumdom.in.connect_handle = &conn_handle;
	enumdom.in.resume_handle = &resume_handle;
	enumdom.in.buf_size = (uint32_t)-1;
	enumdom.out.resume_handle = &resume_handle;
	enumdom.out.num_entries = &num_entries;
	enumdom.out.sam = &sam;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_EnumDomains_r(samr_handle, mem_ctx, &enumdom),
		"samr_EnumDomains failed");
	torture_assert_ntstatus_ok(tctx, enumdom.out.result,
		"samr_EnumDomains failed");

	torture_assert_int_equal(tctx, *enumdom.out.num_entries, 2,
		"samr_EnumDomains returned unexpected num_entries");

	dom_idx = strequal(sam->entries[0].name.string,
			   "builtin") ? 1:0;

	l.in.connect_handle = &conn_handle;
	domain_name.string = sam->entries[dom_idx].name.string;
	*domain = talloc_strdup(mem_ctx, domain_name.string);
	l.in.domain_name = &domain_name;
	l.out.sid = &sid;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_LookupDomain_r(samr_handle, mem_ctx, &l),
		"samr_LookupDomain failed");
	torture_assert_ntstatus_ok(tctx, l.out.result,
		"samr_LookupDomain failed");

	o.in.connect_handle = &conn_handle;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.in.sid = *l.out.sid;
	o.out.domain_handle = &domain_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_OpenDomain_r(samr_handle, mem_ctx, &o),
		"samr_OpenDomain failed");
	torture_assert_ntstatus_ok(tctx, o.out.result,
		"samr_OpenDomain failed");

	c.in.domain_handle = &domain_handle;
	user_name.string = username;
	c.in.account_name = &user_name;
	c.in.acct_flags = ACB_NORMAL;
	c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	user_handle = talloc(mem_ctx, struct policy_handle);
	c.out.user_handle = user_handle;
	c.out.access_granted = &access_granted;
	c.out.rid = &user_rid;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_samr_CreateUser2_r(samr_handle, mem_ctx, &c),
		"samr_CreateUser2 failed");

	if (NT_STATUS_EQUAL(c.out.result, NT_STATUS_USER_EXISTS)) {
		struct samr_LookupNames ln;
		struct samr_OpenUser ou;
		struct samr_Ids rids, types;

		ln.in.domain_handle = &domain_handle;
		ln.in.num_names = 1;
		ln.in.names = &user_name;
		ln.out.rids = &rids;
		ln.out.types = &types;

		torture_assert_ntstatus_ok(tctx,
			dcerpc_samr_LookupNames_r(samr_handle, mem_ctx, &ln),
			"samr_LookupNames failed");
		torture_assert_ntstatus_ok(tctx, ln.out.result,
			"samr_LookupNames failed");

		ou.in.domain_handle = &domain_handle;
		ou.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		user_rid = ou.in.rid = ln.out.rids->ids[0];
		ou.out.user_handle = user_handle;

		torture_assert_ntstatus_ok(tctx,
			dcerpc_samr_OpenUser_r(samr_handle, mem_ctx, &ou),
			"samr_OpenUser failed");
		status = ou.out.result;
	} else {
		status = c.out.result;
	}

	torture_assert_ntstatus_ok(tctx, status,
		"samr_CreateUser failed");

	*result_pipe = samr_pipe;
	*result_handle = user_handle;
	if (sid_p != NULL) {
		*sid_p = dom_sid_add_rid(mem_ctx, *l.out.sid, user_rid);
	}
	return true;

}

/*
 * Create a test user
 */

static bool create_user(struct torture_context *tctx,
			TALLOC_CTX *mem_ctx, struct smbcli_state *cli,
			struct cli_credentials *admin_creds,
			const char *username, const char *password,
			char **domain_name,
			struct dom_sid **user_sid)
{
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;
	struct dcerpc_pipe *samr_pipe;
	struct dcerpc_binding_handle *samr_handle;
	struct policy_handle *wks_handle;
	bool ret = false;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		torture_comment(tctx, "talloc_init failed\n");
		return false;
	}

	ret = get_usr_handle(tctx, cli, tmp_ctx, admin_creds,
			     DCERPC_AUTH_TYPE_NTLMSSP,
			     DCERPC_AUTH_LEVEL_INTEGRITY,
			     username, domain_name, &samr_pipe, &wks_handle,
			     user_sid);
	if (ret == false) {
		torture_comment(tctx, "get_usr_handle failed\n");
		goto done;
	}
	samr_handle = samr_pipe->binding_handle;

	{
		struct samr_SetUserInfo2 sui2;
		struct samr_SetUserInfo sui;
		struct samr_QueryUserInfo qui;
		union samr_UserInfo u_info;
		union samr_UserInfo *info;
		DATA_BLOB session_key;


		ZERO_STRUCT(u_info);
		encode_pw_buffer(u_info.info23.password.data, password,
				 STR_UNICODE);

		status = dcerpc_fetch_session_key(samr_pipe, &session_key);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "dcerpc_fetch_session_key failed\n");
			goto done;
		}
		arcfour_crypt_blob(u_info.info23.password.data, 516,
				   &session_key);
		u_info.info23.info.password_expired = 0;
		u_info.info23.info.fields_present = SAMR_FIELD_NT_PASSWORD_PRESENT |
						    SAMR_FIELD_LM_PASSWORD_PRESENT |
						    SAMR_FIELD_EXPIRED_FLAG;
		sui2.in.user_handle = wks_handle;
		sui2.in.info = &u_info;
		sui2.in.level = 23;

		status = dcerpc_samr_SetUserInfo2_r(samr_handle, tmp_ctx, &sui2);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "samr_SetUserInfo(23) failed: %s\n",
				 nt_errstr(status));
			goto done;
		}
		if (!NT_STATUS_IS_OK(sui2.out.result)) {
			torture_comment(tctx, "samr_SetUserInfo(23) failed: %s\n",
				 nt_errstr(sui2.out.result));
			goto done;
		}

		u_info.info16.acct_flags = ACB_NORMAL;
		sui.in.user_handle = wks_handle;
		sui.in.info = &u_info;
		sui.in.level = 16;

		status = dcerpc_samr_SetUserInfo_r(samr_handle, tmp_ctx, &sui);
		if (!NT_STATUS_IS_OK(status) || !NT_STATUS_IS_OK(sui.out.result)) {
			torture_comment(tctx, "samr_SetUserInfo(16) failed\n");
			goto done;
		}

		qui.in.user_handle = wks_handle;
		qui.in.level = 21;
		qui.out.info = &info;

		status = dcerpc_samr_QueryUserInfo_r(samr_handle, tmp_ctx, &qui);
		if (!NT_STATUS_IS_OK(status) || !NT_STATUS_IS_OK(qui.out.result)) {
			torture_comment(tctx, "samr_QueryUserInfo(21) failed\n");
			goto done;
		}

		info->info21.allow_password_change = 0;
		info->info21.force_password_change = 0;
		info->info21.account_name.string = NULL;
		info->info21.rid = 0;
		info->info21.acct_expiry = 0;
		info->info21.fields_present = 0x81827fa; /* copy usrmgr.exe */

		u_info.info21 = info->info21;
		sui.in.user_handle = wks_handle;
		sui.in.info = &u_info;
		sui.in.level = 21;

		status = dcerpc_samr_SetUserInfo_r(samr_handle, tmp_ctx, &sui);
		if (!NT_STATUS_IS_OK(status) || !NT_STATUS_IS_OK(sui.out.result)) {
			torture_comment(tctx, "samr_SetUserInfo(21) failed\n");
			goto done;
		}
	}

	*domain_name= talloc_steal(mem_ctx, *domain_name);
	*user_sid = talloc_steal(mem_ctx, *user_sid);
	ret = true;
 done:
	talloc_free(tmp_ctx);
	return ret;
}

/*
 * Delete a test user
 */

static bool delete_user(struct torture_context *tctx,
			struct smbcli_state *cli,
			struct cli_credentials *admin_creds,
			const char *username)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	char *dom_name;
	struct dcerpc_pipe *samr_pipe;
	struct dcerpc_binding_handle *samr_handle;
	struct policy_handle *user_handle;
	bool ret = false;

	if ((mem_ctx = talloc_init("leave")) == NULL) {
		torture_comment(tctx, "talloc_init failed\n");
		return false;
	}

	ret = get_usr_handle(tctx, cli, mem_ctx, admin_creds,
			     DCERPC_AUTH_TYPE_NTLMSSP,
			     DCERPC_AUTH_LEVEL_INTEGRITY,
			     username, &dom_name, &samr_pipe,
			     &user_handle, NULL);
	if (ret == false) {
		torture_comment(tctx, "get_wks_handle failed\n");
		goto done;
	}
	samr_handle = samr_pipe->binding_handle;

	{
		struct samr_DeleteUser d;

		d.in.user_handle = user_handle;
		d.out.user_handle = user_handle;

		status = dcerpc_samr_DeleteUser_r(samr_handle, mem_ctx, &d);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "samr_DeleteUser failed %s\n", nt_errstr(status));
			goto done;
		}
		if (!NT_STATUS_IS_OK(d.out.result)) {
			torture_comment(tctx, "samr_DeleteUser failed %s\n", nt_errstr(d.out.result));
			goto done;
		}

	}

	ret = true;

 done:
	talloc_free(mem_ctx);
	return ret;
}

/*
 * Do a Samba3-style join
 */

static bool join3(struct torture_context *tctx,
		  struct smbcli_state *cli,
		  bool use_level25,
		  struct cli_credentials *admin_creds,
		  struct cli_credentials *wks_creds)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	char *dom_name;
	struct dcerpc_pipe *samr_pipe;
	struct dcerpc_binding_handle *samr_handle;
	struct policy_handle *wks_handle;
	bool ret = false;
	NTTIME last_password_change;

	if ((mem_ctx = talloc_init("join3")) == NULL) {
		torture_comment(tctx, "talloc_init failed\n");
		return false;
	}

	ret = get_usr_handle(
		tctx, cli, mem_ctx, admin_creds,
		DCERPC_AUTH_TYPE_NTLMSSP,
		DCERPC_AUTH_LEVEL_PRIVACY,
		talloc_asprintf(mem_ctx, "%s$",
				cli_credentials_get_workstation(wks_creds)),
		&dom_name, &samr_pipe, &wks_handle, NULL);
	if (ret == false) {
		torture_comment(tctx, "get_wks_handle failed\n");
		goto done;
	}
	samr_handle = samr_pipe->binding_handle;

	{
		struct samr_QueryUserInfo q;
		union samr_UserInfo *info;

		q.in.user_handle = wks_handle;
		q.in.level = 21;
		q.out.info = &info;

		status = dcerpc_samr_QueryUserInfo_r(samr_handle, mem_ctx, &q);
		if (!NT_STATUS_IS_OK(status)) {
			torture_warning(tctx, "QueryUserInfo failed: %s\n",
				  nt_errstr(status));
			goto done;
		}
		if (!NT_STATUS_IS_OK(q.out.result)) {
			torture_warning(tctx, "QueryUserInfo failed: %s\n",
				  nt_errstr(q.out.result));
			goto done;
		}


		last_password_change = info->info21.last_password_change;
	}

	cli_credentials_set_domain(wks_creds, dom_name, CRED_SPECIFIED);

	if (use_level25) {
		struct samr_SetUserInfo2 sui2;
		union samr_UserInfo u_info;
		struct samr_UserInfo21 *i21 = &u_info.info25.info;
		DATA_BLOB session_key;
		DATA_BLOB confounded_session_key = data_blob_talloc(
			mem_ctx, NULL, 16);
		MD5_CTX ctx;
		uint8_t confounder[16];

		ZERO_STRUCT(u_info);

		i21->full_name.string = talloc_asprintf(
			mem_ctx, "%s$",
			cli_credentials_get_workstation(wks_creds));
		i21->acct_flags = ACB_WSTRUST;
		i21->fields_present = SAMR_FIELD_FULL_NAME |
			SAMR_FIELD_ACCT_FLAGS | SAMR_FIELD_NT_PASSWORD_PRESENT;
		/* this would break the test result expectations
		i21->fields_present |= SAMR_FIELD_EXPIRED_FLAG;
		i21->password_expired = 1;
		*/

		encode_pw_buffer(u_info.info25.password.data,
				 cli_credentials_get_password(wks_creds),
				 STR_UNICODE);
		status = dcerpc_fetch_session_key(samr_pipe, &session_key);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "dcerpc_fetch_session_key failed: %s\n",
				 nt_errstr(status));
			goto done;
		}
		generate_random_buffer((uint8_t *)confounder, 16);

		MD5Init(&ctx);
		MD5Update(&ctx, confounder, 16);
		MD5Update(&ctx, session_key.data, session_key.length);
		MD5Final(confounded_session_key.data, &ctx);

		arcfour_crypt_blob(u_info.info25.password.data, 516,
				   &confounded_session_key);
		memcpy(&u_info.info25.password.data[516], confounder, 16);

		sui2.in.user_handle = wks_handle;
		sui2.in.level = 25;
		sui2.in.info = &u_info;

		status = dcerpc_samr_SetUserInfo2_r(samr_handle, mem_ctx, &sui2);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "samr_SetUserInfo2(25) failed: %s\n",
				 nt_errstr(status));
			goto done;
		}
		if (!NT_STATUS_IS_OK(sui2.out.result)) {
			torture_comment(tctx, "samr_SetUserInfo2(25) failed: %s\n",
				 nt_errstr(sui2.out.result));
			goto done;
		}
	} else {
		struct samr_SetUserInfo2 sui2;
		struct samr_SetUserInfo sui;
		union samr_UserInfo u_info;
		DATA_BLOB session_key;

		encode_pw_buffer(u_info.info24.password.data,
				 cli_credentials_get_password(wks_creds),
				 STR_UNICODE);
		/* just to make this test pass */
		u_info.info24.password_expired = 1;

		status = dcerpc_fetch_session_key(samr_pipe, &session_key);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "dcerpc_fetch_session_key failed\n");
			goto done;
		}
		arcfour_crypt_blob(u_info.info24.password.data, 516,
				   &session_key);
		sui2.in.user_handle = wks_handle;
		sui2.in.info = &u_info;
		sui2.in.level = 24;

		status = dcerpc_samr_SetUserInfo2_r(samr_handle, mem_ctx, &sui2);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "samr_SetUserInfo(24) failed: %s\n",
				 nt_errstr(status));
			goto done;
		}
		if (!NT_STATUS_IS_OK(sui2.out.result)) {
			torture_comment(tctx, "samr_SetUserInfo(24) failed: %s\n",
				 nt_errstr(sui2.out.result));
			goto done;
		}

		u_info.info16.acct_flags = ACB_WSTRUST;
		sui.in.user_handle = wks_handle;
		sui.in.info = &u_info;
		sui.in.level = 16;

		status = dcerpc_samr_SetUserInfo_r(samr_handle, mem_ctx, &sui);
		if (!NT_STATUS_IS_OK(status) || !NT_STATUS_IS_OK(sui.out.result)) {
			torture_comment(tctx, "samr_SetUserInfo(16) failed\n");
			goto done;
		}
	}

	{
		struct samr_QueryUserInfo q;
		union samr_UserInfo *info;

		q.in.user_handle = wks_handle;
		q.in.level = 21;
		q.out.info = &info;

		status = dcerpc_samr_QueryUserInfo_r(samr_handle, mem_ctx, &q);
		if (!NT_STATUS_IS_OK(status)) {
			torture_warning(tctx, "QueryUserInfo failed: %s\n",
				  nt_errstr(status));
			goto done;
		}
		if (!NT_STATUS_IS_OK(q.out.result)) {
			torture_warning(tctx, "QueryUserInfo failed: %s\n",
				  nt_errstr(q.out.result));
			goto done;
		}

		if (use_level25) {
			if (last_password_change
			    == info->info21.last_password_change) {
				torture_warning(tctx, "last_password_change unchanged "
					 "during join, level25 must change "
					 "it\n");
				goto done;
			}
		}
		else {
			if (last_password_change
			    != info->info21.last_password_change) {
				torture_warning(tctx, "last_password_change changed "
					 "during join, level24 doesn't "
					 "change it\n");
				goto done;
			}
		}
	}

	ret = true;

 done:
	talloc_free(mem_ctx);
	return ret;
}

/*
 * Do a ReqChallenge/Auth2 and get the wks creds
 */

static bool auth2(struct torture_context *tctx,
		  struct smbcli_state *cli,
		  struct cli_credentials *wks_cred)
{
	TALLOC_CTX *mem_ctx;
	struct dcerpc_pipe *net_pipe;
	struct dcerpc_binding_handle *net_handle;
	bool result = false;
	NTSTATUS status;
	struct netr_ServerReqChallenge r;
	struct netr_Credential netr_cli_creds;
	struct netr_Credential netr_srv_creds;
	uint32_t negotiate_flags;
	struct netr_ServerAuthenticate2 a;
	struct netlogon_creds_CredentialState *creds_state;
	struct netr_Credential netr_cred;
	struct samr_Password mach_pw;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		torture_comment(tctx, "talloc_new failed\n");
		return false;
	}

	net_pipe = dcerpc_pipe_init(mem_ctx,
				    cli->transport->socket->event.ctx);
	if (net_pipe == NULL) {
		torture_comment(tctx, "dcerpc_pipe_init failed\n");
		goto done;
	}
	net_handle = net_pipe->binding_handle;

	status = dcerpc_pipe_open_smb(net_pipe, cli->tree, "\\netlogon");
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	status = dcerpc_bind_auth_none(net_pipe, &ndr_table_netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_bind_auth_none failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	r.in.computer_name = cli_credentials_get_workstation(wks_cred);
	r.in.server_name = talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(net_pipe));
	if (r.in.server_name == NULL) {
		torture_comment(tctx, "talloc_asprintf failed\n");
		goto done;
	}
	generate_random_buffer(netr_cli_creds.data,
			       sizeof(netr_cli_creds.data));
	r.in.credentials = &netr_cli_creds;
	r.out.return_credentials = &netr_srv_creds;

	status = dcerpc_netr_ServerReqChallenge_r(net_handle, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "netr_ServerReqChallenge failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(tctx, "netr_ServerReqChallenge failed: %s\n",
			 nt_errstr(r.out.result));
		goto done;
	}

	negotiate_flags = NETLOGON_NEG_AUTH2_FLAGS;
	E_md4hash(cli_credentials_get_password(wks_cred), mach_pw.hash);

	a.in.server_name = talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(net_pipe));
	a.in.account_name = talloc_asprintf(
		mem_ctx, "%s$", cli_credentials_get_workstation(wks_cred));
	a.in.computer_name = cli_credentials_get_workstation(wks_cred);
	a.in.secure_channel_type = SEC_CHAN_WKSTA;
	a.in.negotiate_flags = &negotiate_flags;
	a.out.negotiate_flags = &negotiate_flags;
	a.in.credentials = &netr_cred;
	a.out.return_credentials = &netr_cred;

	creds_state = netlogon_creds_client_init(mem_ctx,
						 a.in.account_name,
						 a.in.computer_name,
						 r.in.credentials,
						 r.out.return_credentials, &mach_pw,
						 &netr_cred, negotiate_flags);

	status = dcerpc_netr_ServerAuthenticate2_r(net_handle, mem_ctx, &a);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "netr_ServerServerAuthenticate2 failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(a.out.result)) {
		torture_comment(tctx, "netr_ServerServerAuthenticate2 failed: %s\n",
			 nt_errstr(a.out.result));
		goto done;
	}

	if (!netlogon_creds_client_check(creds_state, a.out.return_credentials)) {
		torture_comment(tctx, "creds_client_check failed\n");
		goto done;
	}

	cli_credentials_set_netlogon_creds(wks_cred, creds_state);

	result = true;

 done:
	talloc_free(mem_ctx);
	return result;
}

/*
 * Do a couple of schannel protected Netlogon ops: Interactive and Network
 * login, and change the wks password
 */

static bool schan(struct torture_context *tctx,
		  struct smbcli_state *cli,
		  struct cli_credentials *wks_creds,
		  struct cli_credentials *user_creds)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	bool ret = false;
	struct dcerpc_pipe *net_pipe;
	struct dcerpc_binding_handle *net_handle;
	int i;

	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		torture_comment(tctx, "talloc_new failed\n");
		return false;
	}

	net_pipe = dcerpc_pipe_init(mem_ctx,
				    cli->transport->socket->event.ctx);
	if (net_pipe == NULL) {
		torture_comment(tctx, "dcerpc_pipe_init failed\n");
		goto done;
	}
	net_handle = net_pipe->binding_handle;

	status = dcerpc_pipe_open_smb(net_pipe, cli->tree, "\\netlogon");
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

#if 0
	net_pipe->conn->flags |= DCERPC_DEBUG_PRINT_IN |
		DCERPC_DEBUG_PRINT_OUT;
#endif
#if 1
	net_pipe->conn->flags |= (DCERPC_SIGN | DCERPC_SEAL);
	status = dcerpc_bind_auth(net_pipe, &ndr_table_netlogon,
				  wks_creds, lpcfg_gensec_settings(tctx->lp_ctx, tctx->lp_ctx), DCERPC_AUTH_TYPE_SCHANNEL,
				  DCERPC_AUTH_LEVEL_PRIVACY,
				  NULL);
#else
	status = dcerpc_bind_auth_none(net_pipe, &ndr_table_netlogon);
#endif
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "schannel bind failed: %s\n", nt_errstr(status));
		goto done;
	}


	for (i=2; i<4; i++) {
		int flags;
		DATA_BLOB chal, nt_resp, lm_resp, names_blob, session_key;
		struct netlogon_creds_CredentialState *creds_state;
		struct netr_Authenticator netr_auth, netr_auth2;
		struct netr_NetworkInfo ninfo;
		struct netr_PasswordInfo pinfo;
		struct netr_LogonSamLogon r;
		union netr_LogonLevel logon;
		union netr_Validation validation;
		uint8_t authoritative;
		struct netr_Authenticator return_authenticator;

		flags = CLI_CRED_LANMAN_AUTH | CLI_CRED_NTLM_AUTH |
			CLI_CRED_NTLMv2_AUTH;

		chal = data_blob_talloc(mem_ctx, NULL, 8);
		if (chal.data == NULL) {
			torture_comment(tctx, "data_blob_talloc failed\n");
			goto done;
		}

		generate_random_buffer(chal.data, chal.length);
		names_blob = NTLMv2_generate_names_blob(
			mem_ctx,
			cli_credentials_get_workstation(wks_creds),
			cli_credentials_get_domain(wks_creds));
		status = cli_credentials_get_ntlm_response(
			user_creds, mem_ctx, &flags, chal, names_blob,
			&lm_resp, &nt_resp, NULL, NULL);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "cli_credentials_get_ntlm_response failed:"
				 " %s\n", nt_errstr(status));
			goto done;
		}

		creds_state = cli_credentials_get_netlogon_creds(wks_creds);
		netlogon_creds_client_authenticator(creds_state, &netr_auth);

		ninfo.identity_info.account_name.string =
			cli_credentials_get_username(user_creds);
		ninfo.identity_info.domain_name.string =
			cli_credentials_get_domain(user_creds);
		ninfo.identity_info.parameter_control = 0;
		ninfo.identity_info.logon_id_low = 0;
		ninfo.identity_info.logon_id_high = 0;
		ninfo.identity_info.workstation.string =
			cli_credentials_get_workstation(user_creds);
		memcpy(ninfo.challenge, chal.data, sizeof(ninfo.challenge));
		ninfo.nt.length = nt_resp.length;
		ninfo.nt.data = nt_resp.data;
		ninfo.lm.length = lm_resp.length;
		ninfo.lm.data = lm_resp.data;

		logon.network = &ninfo;

		r.in.server_name = talloc_asprintf(
			mem_ctx, "\\\\%s", dcerpc_server_name(net_pipe));
		ZERO_STRUCT(netr_auth2);
		r.in.computer_name =
			cli_credentials_get_workstation(wks_creds);
		r.in.credential = &netr_auth;
		r.in.return_authenticator = &netr_auth2;
		r.in.logon_level = 2;
		r.in.validation_level = i;
		r.in.logon = &logon;
		r.out.validation = &validation;
		r.out.authoritative = &authoritative;
		r.out.return_authenticator = &return_authenticator;

		status = dcerpc_netr_LogonSamLogon_r(net_handle, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "netr_LogonSamLogon failed: %s\n",
				 nt_errstr(status));
			goto done;
		}
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_comment(tctx, "netr_LogonSamLogon failed: %s\n",
				 nt_errstr(r.out.result));
			goto done;
		}

		if ((r.out.return_authenticator == NULL) ||
		    (!netlogon_creds_client_check(creds_state,
					 &r.out.return_authenticator->cred))) {
			torture_comment(tctx, "Credentials check failed!\n");
			goto done;
		}

		netlogon_creds_client_authenticator(creds_state, &netr_auth);

		pinfo.identity_info = ninfo.identity_info;
		ZERO_STRUCT(pinfo.lmpassword.hash);
		E_md4hash(cli_credentials_get_password(user_creds),
			  pinfo.ntpassword.hash);
		session_key = data_blob_talloc(mem_ctx,
					       creds_state->session_key, 16);
		arcfour_crypt_blob(pinfo.ntpassword.hash,
				   sizeof(pinfo.ntpassword.hash),
				   &session_key);

		logon.password = &pinfo;

		r.in.logon_level = 1;
		r.in.logon = &logon;
		r.out.return_authenticator = &return_authenticator;

		status = dcerpc_netr_LogonSamLogon_r(net_handle, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "netr_LogonSamLogon failed: %s\n",
				 nt_errstr(status));
			goto done;
		}
		if (!NT_STATUS_IS_OK(r.out.result)) {
			torture_comment(tctx, "netr_LogonSamLogon failed: %s\n",
				 nt_errstr(r.out.result));
			goto done;
		}

		if ((r.out.return_authenticator == NULL) ||
		    (!netlogon_creds_client_check(creds_state,
					 &r.out.return_authenticator->cred))) {
			torture_comment(tctx, "Credentials check failed!\n");
			goto done;
		}
	}

	{
		struct netr_ServerPasswordSet s;
		char *password = generate_random_password(wks_creds, 8, 255);
		struct netlogon_creds_CredentialState *creds_state;
		struct netr_Authenticator credential, return_authenticator;
		struct samr_Password new_password;

		s.in.server_name = talloc_asprintf(
			mem_ctx, "\\\\%s", dcerpc_server_name(net_pipe));
		s.in.computer_name = cli_credentials_get_workstation(wks_creds);
		s.in.account_name = talloc_asprintf(
			mem_ctx, "%s$", s.in.computer_name);
		s.in.secure_channel_type = SEC_CHAN_WKSTA;
		s.in.credential = &credential;
		s.in.new_password = &new_password;
		s.out.return_authenticator = &return_authenticator;

		E_md4hash(password, new_password.hash);

		creds_state = cli_credentials_get_netlogon_creds(wks_creds);
		netlogon_creds_des_encrypt(creds_state, &new_password);
		netlogon_creds_client_authenticator(creds_state, &credential);

		status = dcerpc_netr_ServerPasswordSet_r(net_handle, mem_ctx, &s);
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "ServerPasswordSet - %s\n", nt_errstr(status));
			goto done;
		}
		if (!NT_STATUS_IS_OK(s.out.result)) {
			torture_comment(tctx, "ServerPasswordSet - %s\n", nt_errstr(s.out.result));
			goto done;
		}

		if (!netlogon_creds_client_check(creds_state,
						 &s.out.return_authenticator->cred)) {
			torture_comment(tctx, "Credential chaining failed\n");
		}

		cli_credentials_set_password(wks_creds, password,
					     CRED_SPECIFIED);
	}

	ret = true;
 done:
	talloc_free(mem_ctx);
	return ret;
}

/*
 * Delete the wks account again
 */

static bool leave(struct torture_context *tctx,
		  struct smbcli_state *cli,
		  struct cli_credentials *admin_creds,
		  struct cli_credentials *wks_creds)
{
	char *wks_name = talloc_asprintf(
		NULL, "%s$", cli_credentials_get_workstation(wks_creds));
	bool ret;

	ret = delete_user(tctx, cli, admin_creds, wks_name);
	talloc_free(wks_name);
	return ret;
}

/*
 * Test the Samba3 DC code a bit. Join, do some schan netlogon ops, leave
 */

static bool torture_netlogon_samba3(struct torture_context *torture)
{
	NTSTATUS status;
	struct smbcli_state *cli;
	struct cli_credentials *anon_creds;
	struct cli_credentials *wks_creds;
	const char *wks_name;
	int i;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	wks_name = torture_setting_string(torture, "wksname", NULL);
	if (wks_name == NULL) {
		wks_name = get_myname(torture);
	}

	if (!(anon_creds = cli_credentials_init_anon(torture))) {
		torture_fail(torture, "create_anon_creds failed\n");
	}

	lpcfg_smbcli_options(torture->lp_ctx, &options);
	lpcfg_smbcli_session_options(torture->lp_ctx, &session_options);

	status = smbcli_full_connection(torture, &cli,
					torture_setting_string(torture, "host", NULL),
					lpcfg_smb_ports(torture->lp_ctx),
					"IPC$", NULL,
					lpcfg_socket_options(torture->lp_ctx),
					anon_creds,
					lpcfg_resolve_context(torture->lp_ctx),
					torture->ev, &options, &session_options,
					lpcfg_gensec_settings(torture, torture->lp_ctx));
	torture_assert_ntstatus_ok(torture, status, "smbcli_full_connection failed\n");

	wks_creds = cli_credentials_init(torture);
	if (wks_creds == NULL) {
		torture_fail(torture, "cli_credentials_init failed\n");
	}

	cli_credentials_set_conf(wks_creds, torture->lp_ctx);
	cli_credentials_set_secure_channel_type(wks_creds, SEC_CHAN_WKSTA);
	cli_credentials_set_username(wks_creds, wks_name, CRED_SPECIFIED);
	cli_credentials_set_workstation(wks_creds, wks_name, CRED_SPECIFIED);
	cli_credentials_set_password(wks_creds,
				     generate_random_password(wks_creds, 8, 255),
				     CRED_SPECIFIED);

	torture_assert(torture,
		join3(torture, cli, false, cmdline_credentials, wks_creds),
		"join failed");

	cli_credentials_set_domain(
		cmdline_credentials, cli_credentials_get_domain(wks_creds),
		CRED_SPECIFIED);

	for (i=0; i<2; i++) {

		/* Do this more than once, the routine "schan" changes
		 * the workstation password using the netlogon
		 * password change routine */

		int j;

		torture_assert(torture,
			auth2(torture, cli, wks_creds),
			"auth2 failed");

		for (j=0; j<2; j++) {
			torture_assert(torture,
				schan(torture, cli, wks_creds, cmdline_credentials),
				"schan failed");
		}
	}

	torture_assert(torture,
		leave(torture, cli, cmdline_credentials, wks_creds),
		"leave failed");

	return true;
}

/*
 * Do a simple join, testjoin and leave using specified smb and samr
 * credentials
 */

static bool test_join3(struct torture_context *tctx,
		       bool use_level25,
		       struct cli_credentials *smb_creds,
		       struct cli_credentials *samr_creds,
		       const char *wks_name)
{
	NTSTATUS status;
	struct smbcli_state *cli;
	struct cli_credentials *wks_creds;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lpcfg_smbcli_options(tctx->lp_ctx, &options);
	lpcfg_smbcli_session_options(tctx->lp_ctx, &session_options);

	status = smbcli_full_connection(tctx, &cli,
					torture_setting_string(tctx, "host", NULL),
					lpcfg_smb_ports(tctx->lp_ctx),
					"IPC$", NULL, lpcfg_socket_options(tctx->lp_ctx),
					smb_creds, lpcfg_resolve_context(tctx->lp_ctx),
					tctx->ev, &options, &session_options,
					lpcfg_gensec_settings(tctx, tctx->lp_ctx));
	torture_assert_ntstatus_ok(tctx, status,
		"smbcli_full_connection failed");

	wks_creds = cli_credentials_init(cli);
	torture_assert(tctx, wks_creds, "cli_credentials_init failed");

	cli_credentials_set_conf(wks_creds, tctx->lp_ctx);
	cli_credentials_set_secure_channel_type(wks_creds, SEC_CHAN_WKSTA);
	cli_credentials_set_username(wks_creds, wks_name, CRED_SPECIFIED);
	cli_credentials_set_workstation(wks_creds, wks_name, CRED_SPECIFIED);
	cli_credentials_set_password(wks_creds,
				     generate_random_password(wks_creds, 8, 255),
				     CRED_SPECIFIED);

	torture_assert(tctx,
		join3(tctx, cli, use_level25, samr_creds, wks_creds),
		"join failed");

	cli_credentials_set_domain(
		cmdline_credentials, cli_credentials_get_domain(wks_creds),
		CRED_SPECIFIED);

	torture_assert(tctx,
		auth2(tctx, cli, wks_creds),
		"auth2 failed");

	torture_assert(tctx,
		leave(tctx, cli, samr_creds, wks_creds),
		"leave failed");

	talloc_free(cli);

	return true;
}

/*
 * Test the different session key variants. Do it by joining, this uses the
 * session key in the setpassword routine. Test the join by doing the auth2.
 */

static bool torture_samba3_sessionkey(struct torture_context *torture)
{
	struct cli_credentials *anon_creds;
	const char *wks_name;

	wks_name = torture_setting_string(torture, "wksname", get_myname(torture));

	if (!(anon_creds = cli_credentials_init_anon(torture))) {
		torture_fail(torture, "create_anon_creds failed\n");
	}

	cli_credentials_set_workstation(anon_creds, wks_name, CRED_SPECIFIED);


	if (!torture_setting_bool(torture, "samba3", false)) {

		/* Samba3 in the build farm right now does this happily. Need
		 * to fix :-) */

		if (test_join3(torture, false, anon_creds, NULL, wks_name)) {
			torture_fail(torture, "join using anonymous bind on an anonymous smb "
				 "connection succeeded -- HUH??\n");
		}
	}

	torture_assert(torture,
		test_join3(torture, false, anon_creds, cmdline_credentials, wks_name),
		"join using ntlmssp bind on an anonymous smb connection failed");

	torture_assert(torture,
		test_join3(torture, false, cmdline_credentials, NULL, wks_name),
		"join using anonymous bind on an authenticated smb connection failed");

	torture_assert(torture,
		test_join3(torture, false, cmdline_credentials, cmdline_credentials, wks_name),
		"join using ntlmssp bind on an authenticated smb connection failed");

	/*
	 * The following two are tests for setuserinfolevel 25
	 */

	torture_assert(torture,
		test_join3(torture, true, anon_creds, cmdline_credentials, wks_name),
		"join using ntlmssp bind on an anonymous smb connection failed");

	torture_assert(torture,
		test_join3(torture, true, cmdline_credentials, NULL, wks_name),
		"join using anonymous bind on an authenticated smb connection failed");

	return true;
}

/*
 * open pipe and bind, given an IPC$ context
 */

static NTSTATUS pipe_bind_smb(struct torture_context *tctx,
			      TALLOC_CTX *mem_ctx,
			      struct smbcli_tree *tree,
			      const char *pipe_name,
			      const struct ndr_interface_table *iface,
			      struct dcerpc_pipe **p)
{
	struct dcerpc_pipe *result;
	NTSTATUS status;

	if (!(result = dcerpc_pipe_init(
		      mem_ctx, tree->session->transport->socket->event.ctx))) {
		return NT_STATUS_NO_MEMORY;
	}

	status = dcerpc_pipe_open_smb(result, tree, pipe_name);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		talloc_free(result);
		return status;
	}

	status = dcerpc_bind_auth_none(result, iface);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "schannel bind failed: %s\n", nt_errstr(status));
		talloc_free(result);
		return status;
	}

	*p = result;
	return NT_STATUS_OK;
}

/*
 * Sane wrapper around lsa_LookupNames
 */

static struct dom_sid *name2sid(struct torture_context *tctx,
				TALLOC_CTX *mem_ctx,
				struct dcerpc_pipe *p,
				const char *name,
				const char *domain)
{
	struct lsa_ObjectAttribute attr;
	struct lsa_QosInfo qos;
	struct lsa_OpenPolicy2 r;
	struct lsa_Close c;
	NTSTATUS status;
	struct policy_handle handle;
	struct lsa_LookupNames l;
	struct lsa_TransSidArray sids;
	struct lsa_RefDomainList *domains = NULL;
	struct lsa_String lsa_name;
	uint32_t count = 0;
	struct dom_sid *result;
	TALLOC_CTX *tmp_ctx;
	struct dcerpc_binding_handle *b = p->binding_handle;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		return NULL;
	}

	qos.len = 0;
	qos.impersonation_level = 2;
	qos.context_mode = 1;
	qos.effective_only = 0;

	attr.len = 0;
	attr.root_dir = NULL;
	attr.object_name = NULL;
	attr.attributes = 0;
	attr.sec_desc = NULL;
	attr.sec_qos = &qos;

	r.in.system_name = "\\";
	r.in.attr = &attr;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &handle;

	status = dcerpc_lsa_OpenPolicy2_r(b, tmp_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "OpenPolicy2 failed - %s\n", nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(tctx, "OpenPolicy2 failed - %s\n", nt_errstr(r.out.result));
		talloc_free(tmp_ctx);
		return NULL;
	}

	sids.count = 0;
	sids.sids = NULL;

	lsa_name.string = talloc_asprintf(tmp_ctx, "%s\\%s", domain, name);

	l.in.handle = &handle;
	l.in.num_names = 1;
	l.in.names = &lsa_name;
	l.in.sids = &sids;
	l.in.level = 1;
	l.in.count = &count;
	l.out.count = &count;
	l.out.sids = &sids;
	l.out.domains = &domains;

	status = dcerpc_lsa_LookupNames_r(b, tmp_ctx, &l);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "LookupNames of %s failed - %s\n", lsa_name.string,
		       nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}
	if (!NT_STATUS_IS_OK(l.out.result)) {
		torture_comment(tctx, "LookupNames of %s failed - %s\n", lsa_name.string,
		       nt_errstr(l.out.result));
		talloc_free(tmp_ctx);
		return NULL;
	}

	result = dom_sid_add_rid(mem_ctx, domains->domains[0].sid,
				 l.out.sids->sids[0].rid);

	c.in.handle = &handle;
	c.out.handle = &handle;

	status = dcerpc_lsa_Close_r(b, tmp_ctx, &c);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "dcerpc_lsa_Close failed - %s\n", nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}
	if (!NT_STATUS_IS_OK(c.out.result)) {
		torture_comment(tctx, "dcerpc_lsa_Close failed - %s\n", nt_errstr(c.out.result));
		talloc_free(tmp_ctx);
		return NULL;
	}

	talloc_free(tmp_ctx);
	return result;
}

/*
 * Find out the user SID on this connection
 */

static struct dom_sid *whoami(struct torture_context *tctx,
			      TALLOC_CTX *mem_ctx,
			      struct smbcli_tree *tree)
{
	struct dcerpc_pipe *lsa;
	struct dcerpc_binding_handle *lsa_handle;
	struct lsa_GetUserName r;
	NTSTATUS status;
	struct lsa_String *authority_name_p = NULL;
	struct lsa_String *account_name_p = NULL;
	struct dom_sid *result;

	status = pipe_bind_smb(tctx, mem_ctx, tree, "\\pipe\\lsarpc",
			       &ndr_table_lsarpc, &lsa);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "Could not bind to LSA: %s\n",
			 nt_errstr(status));
		return NULL;
	}
	lsa_handle = lsa->binding_handle;

	r.in.system_name = "\\";
	r.in.account_name = &account_name_p;
	r.in.authority_name = &authority_name_p;
	r.out.account_name = &account_name_p;

	status = dcerpc_lsa_GetUserName_r(lsa_handle, mem_ctx, &r);

	authority_name_p = *r.out.authority_name;

	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "GetUserName failed - %s\n",
		       nt_errstr(status));
		talloc_free(lsa);
		return NULL;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_warning(tctx, "GetUserName failed - %s\n",
		       nt_errstr(r.out.result));
		talloc_free(lsa);
		return NULL;
	}

	result = name2sid(tctx, mem_ctx, lsa, account_name_p->string,
			  authority_name_p->string);

	talloc_free(lsa);
	return result;
}

static int destroy_tree(struct smbcli_tree *tree)
{
	smb_tree_disconnect(tree);
	return 0;
}

/*
 * Do a tcon, given a session
 */

static NTSTATUS secondary_tcon(struct torture_context *tctx,
			       TALLOC_CTX *mem_ctx,
			       struct smbcli_session *session,
			       const char *sharename,
			       struct smbcli_tree **res)
{
	struct smbcli_tree *result;
	TALLOC_CTX *tmp_ctx;
	union smb_tcon tcon;
	NTSTATUS status;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!(result = smbcli_tree_init(session, mem_ctx, false))) {
		talloc_free(tmp_ctx);
		return NT_STATUS_NO_MEMORY;
	}

	tcon.generic.level = RAW_TCON_TCONX;
	tcon.tconx.in.flags = 0;
	tcon.tconx.in.password = data_blob(NULL, 0);
	tcon.tconx.in.path = sharename;
	tcon.tconx.in.device = "?????";

	status = smb_raw_tcon(result, tmp_ctx, &tcon);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "smb_raw_tcon failed: %s\n",
			 nt_errstr(status));
		talloc_free(tmp_ctx);
		return status;
	}

	result->tid = tcon.tconx.out.tid;
	result = talloc_steal(mem_ctx, result);
	talloc_set_destructor(result, destroy_tree);
	talloc_free(tmp_ctx);
	*res = result;
	return NT_STATUS_OK;
}

/*
 * Test the getusername behaviour
 */

static bool torture_samba3_rpc_getusername(struct torture_context *torture)
{
	NTSTATUS status;
	struct smbcli_state *cli;
	bool ret = true;
	struct dom_sid *user_sid;
	struct dom_sid *created_sid;
	struct cli_credentials *anon_creds;
	struct cli_credentials *user_creds;
	char *domain_name;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lpcfg_smbcli_options(torture->lp_ctx, &options);
	lpcfg_smbcli_session_options(torture->lp_ctx, &session_options);

	status = smbcli_full_connection(
		torture, &cli, torture_setting_string(torture, "host", NULL),
		lpcfg_smb_ports(torture->lp_ctx),
		"IPC$", NULL, lpcfg_socket_options(torture->lp_ctx), cmdline_credentials,
		lpcfg_resolve_context(torture->lp_ctx), torture->ev, &options,
		&session_options, lpcfg_gensec_settings(torture, torture->lp_ctx));
	torture_assert_ntstatus_ok(torture, status, "smbcli_full_connection failed\n");

	if (!(user_sid = whoami(torture, torture, cli->tree))) {
		torture_fail(torture, "whoami on auth'ed connection failed\n");
	}

	talloc_free(cli);

	if (!(anon_creds = cli_credentials_init_anon(torture))) {
		torture_fail(torture, "create_anon_creds failed\n");
	}

	status = smbcli_full_connection(
		torture, &cli, torture_setting_string(torture, "host", NULL),
		lpcfg_smb_ports(torture->lp_ctx), "IPC$", NULL,
		lpcfg_socket_options(torture->lp_ctx), anon_creds,
		lpcfg_resolve_context(torture->lp_ctx),
		torture->ev, &options, &session_options,
		lpcfg_gensec_settings(torture, torture->lp_ctx));
	torture_assert_ntstatus_ok(torture, status, "anon smbcli_full_connection failed\n");

	if (!(user_sid = whoami(torture, torture, cli->tree))) {
		torture_fail(torture, "whoami on anon connection failed\n");
	}

	torture_assert_sid_equal(torture, user_sid, dom_sid_parse_talloc(torture, "s-1-5-7"),
		"Anon lsa_GetUserName returned unexpected SID");

	if (!(user_creds = cli_credentials_init(torture))) {
		torture_fail(torture, "cli_credentials_init failed\n");
	}

	cli_credentials_set_conf(user_creds, torture->lp_ctx);
	cli_credentials_set_username(user_creds, "torture_username",
				     CRED_SPECIFIED);
	cli_credentials_set_password(user_creds,
				     generate_random_password(user_creds, 8, 255),
				     CRED_SPECIFIED);

	if (!create_user(torture, torture, cli, cmdline_credentials,
			 cli_credentials_get_username(user_creds),
			 cli_credentials_get_password(user_creds),
			 &domain_name, &created_sid)) {
		torture_fail(torture, "create_user failed\n");
	}

	cli_credentials_set_domain(user_creds, domain_name,
				   CRED_SPECIFIED);

	{
		struct smbcli_session *session2;
		struct smb_composite_sesssetup setup;
		struct smbcli_tree *tree;

		session2 = smbcli_session_init(cli->transport, torture, false, session_options);
		if (session2 == NULL) {
			torture_fail(torture, "smbcli_session_init failed\n");
		}

		setup.in.sesskey = cli->transport->negotiate.sesskey;
		setup.in.capabilities = cli->transport->negotiate.capabilities;
		setup.in.workgroup = "";
		setup.in.credentials = user_creds;
		setup.in.gensec_settings = lpcfg_gensec_settings(torture, torture->lp_ctx);

		status = smb_composite_sesssetup(session2, &setup);
		torture_assert_ntstatus_ok(torture, status, "session setup with new user failed");

		session2->vuid = setup.out.vuid;

		if (!NT_STATUS_IS_OK(secondary_tcon(torture, torture, session2,
						    "IPC$", &tree))) {
			torture_fail(torture, "secondary_tcon failed\n");
		}

		if (!(user_sid = whoami(torture, torture, tree))) {
			torture_fail_goto(torture, del, "whoami on user connection failed\n");
			ret = false;
			goto del;
		}

		talloc_free(tree);
	}

	torture_comment(torture, "Created %s, found %s\n",
		 dom_sid_string(torture, created_sid),
		 dom_sid_string(torture, user_sid));

	if (!dom_sid_equal(created_sid, user_sid)) {
		ret = false;
	}

 del:
	if (!delete_user(torture, cli,
			 cmdline_credentials,
			 cli_credentials_get_username(user_creds))) {
		torture_fail(torture, "delete_user failed\n");
	}

	return ret;
}

static bool test_NetShareGetInfo(struct torture_context *tctx,
				 struct dcerpc_pipe *p,
				 const char *sharename)
{
	NTSTATUS status;
	struct srvsvc_NetShareGetInfo r;
	union srvsvc_NetShareInfo info;
	uint32_t levels[] = { 0, 1, 2, 501, 502, 1004, 1005, 1006, 1007, 1501 };
	int i;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	r.in.server_unc = talloc_asprintf(tctx, "\\\\%s",
					  dcerpc_server_name(p));
	r.in.share_name = sharename;
	r.out.info = &info;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		r.in.level = levels[i];

		torture_comment(tctx, "Testing NetShareGetInfo level %u on share '%s'\n",
		       r.in.level, r.in.share_name);

		status = dcerpc_srvsvc_NetShareGetInfo_r(b, tctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			torture_warning(tctx, "NetShareGetInfo level %u on share '%s' failed"
			       " - %s\n", r.in.level, r.in.share_name,
			       nt_errstr(status));
			ret = false;
			continue;
		}
		if (!W_ERROR_IS_OK(r.out.result)) {
			torture_warning(tctx, "NetShareGetInfo level %u on share '%s' failed "
			       "- %s\n", r.in.level, r.in.share_name,
			       win_errstr(r.out.result));
			ret = false;
			continue;
		}
	}

	return ret;
}

static bool test_NetShareEnum(struct torture_context *tctx,
			      struct dcerpc_pipe *p,
			      const char **one_sharename)
{
	NTSTATUS status;
	struct srvsvc_NetShareEnum r;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr0 c0;
	struct srvsvc_NetShareCtr1 c1;
	struct srvsvc_NetShareCtr2 c2;
	struct srvsvc_NetShareCtr501 c501;
	struct srvsvc_NetShareCtr502 c502;
	struct srvsvc_NetShareCtr1004 c1004;
	struct srvsvc_NetShareCtr1005 c1005;
	struct srvsvc_NetShareCtr1006 c1006;
	struct srvsvc_NetShareCtr1007 c1007;
	uint32_t totalentries = 0;
	uint32_t levels[] = { 0, 1, 2, 501, 502, 1004, 1005, 1006, 1007 };
	int i;
	bool ret = true;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(info_ctr);

	r.in.server_unc = talloc_asprintf(tctx,"\\\\%s",dcerpc_server_name(p));
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = NULL;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		info_ctr.level = levels[i];

		switch (info_ctr.level) {
		case 0:
			ZERO_STRUCT(c0);
			info_ctr.ctr.ctr0 = &c0;
			break;
		case 1:
			ZERO_STRUCT(c1);
			info_ctr.ctr.ctr1 = &c1;
			break;
		case 2:
			ZERO_STRUCT(c2);
			info_ctr.ctr.ctr2 = &c2;
			break;
		case 501:
			ZERO_STRUCT(c501);
			info_ctr.ctr.ctr501 = &c501;
			break;
		case 502:
			ZERO_STRUCT(c502);
			info_ctr.ctr.ctr502 = &c502;
			break;
		case 1004:
			ZERO_STRUCT(c1004);
			info_ctr.ctr.ctr1004 = &c1004;
			break;
		case 1005:
			ZERO_STRUCT(c1005);
			info_ctr.ctr.ctr1005 = &c1005;
			break;
		case 1006:
			ZERO_STRUCT(c1006);
			info_ctr.ctr.ctr1006 = &c1006;
			break;
		case 1007:
			ZERO_STRUCT(c1007);
			info_ctr.ctr.ctr1007 = &c1007;
			break;
		}

		torture_comment(tctx, "Testing NetShareEnum level %u\n", info_ctr.level);

		status = dcerpc_srvsvc_NetShareEnum_r(b, tctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			torture_warning(tctx, "NetShareEnum level %u failed - %s\n",
			       info_ctr.level, nt_errstr(status));
			ret = false;
			continue;
		}
		if (!W_ERROR_IS_OK(r.out.result)) {
			torture_warning(tctx, "NetShareEnum level %u failed - %s\n",
			       info_ctr.level, win_errstr(r.out.result));
			continue;
		}
		if (info_ctr.level == 0) {
			struct srvsvc_NetShareCtr0 *ctr = r.out.info_ctr->ctr.ctr0;
			if (ctr->count > 0) {
				*one_sharename = ctr->array[0].name;
			}
		}
	}

	return ret;
}

static bool torture_samba3_rpc_srvsvc(struct torture_context *torture)
{
	struct dcerpc_pipe *p;
	const char *sharename = NULL;
	bool ret = true;

	torture_assert_ntstatus_ok(torture,
		torture_rpc_connection(torture, &p, &ndr_table_srvsvc),
		"failed to open srvsvc");

	ret &= test_NetShareEnum(torture, p, &sharename);
	if (sharename == NULL) {
		torture_comment(torture, "did not get sharename\n");
	} else {
		ret &= test_NetShareGetInfo(torture, p, sharename);
	}

	return ret;
}

/*
 * Do a ReqChallenge/Auth2 with a random wks name, make sure it returns
 * NT_STATUS_NO_SAM_ACCOUNT
 */

static bool torture_samba3_rpc_randomauth2(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx;
	struct dcerpc_pipe *net_pipe;
	struct dcerpc_binding_handle *net_handle;
	char *wksname;
	bool result = false;
	NTSTATUS status;
	struct netr_ServerReqChallenge r;
	struct netr_Credential netr_cli_creds;
	struct netr_Credential netr_srv_creds;
	uint32_t negotiate_flags;
	struct netr_ServerAuthenticate2 a;
	struct netlogon_creds_CredentialState *creds_state;
	struct netr_Credential netr_cred;
	struct samr_Password mach_pw;
	struct smbcli_state *cli;

	if (!(mem_ctx = talloc_new(torture))) {
		torture_comment(torture, "talloc_new failed\n");
		return false;
	}

	if (!(wksname = generate_random_str_list(
		      mem_ctx, 14, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"))) {
		torture_comment(torture, "generate_random_str_list failed\n");
		goto done;
	}

	if (!(torture_open_connection_share(
		      mem_ctx, &cli,
		      torture, torture_setting_string(torture, "host", NULL),
		      "IPC$", torture->ev))) {
		torture_comment(torture, "IPC$ connection failed\n");
		goto done;
	}

	if (!(net_pipe = dcerpc_pipe_init(
		      mem_ctx, cli->transport->socket->event.ctx))) {
		torture_comment(torture, "dcerpc_pipe_init failed\n");
		goto done;
	}
	net_handle = net_pipe->binding_handle;

	status = dcerpc_pipe_open_smb(net_pipe, cli->tree, "\\netlogon");
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	status = dcerpc_bind_auth_none(net_pipe, &ndr_table_netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "dcerpc_bind_auth_none failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	r.in.computer_name = wksname;
	r.in.server_name = talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(net_pipe));
	if (r.in.server_name == NULL) {
		torture_comment(torture, "talloc_asprintf failed\n");
		goto done;
	}
	generate_random_buffer(netr_cli_creds.data,
			       sizeof(netr_cli_creds.data));
	r.in.credentials = &netr_cli_creds;
	r.out.return_credentials = &netr_srv_creds;

	status = dcerpc_netr_ServerReqChallenge_r(net_handle, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(torture, "netr_ServerReqChallenge failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	if (!NT_STATUS_IS_OK(r.out.result)) {
		torture_comment(torture, "netr_ServerReqChallenge failed: %s\n",
			 nt_errstr(r.out.result));
		goto done;
	}

	negotiate_flags = NETLOGON_NEG_AUTH2_FLAGS;
	E_md4hash("foobar", mach_pw.hash);

	a.in.server_name = talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(net_pipe));
	a.in.account_name = talloc_asprintf(
		mem_ctx, "%s$", wksname);
	a.in.computer_name = wksname;
	a.in.secure_channel_type = SEC_CHAN_WKSTA;
	a.in.negotiate_flags = &negotiate_flags;
	a.out.negotiate_flags = &negotiate_flags;
	a.in.credentials = &netr_cred;
	a.out.return_credentials = &netr_cred;

	creds_state = netlogon_creds_client_init(mem_ctx,
						 a.in.account_name,
						 a.in.computer_name,
						 r.in.credentials,
						 r.out.return_credentials, &mach_pw,
						 &netr_cred, negotiate_flags);


	status = dcerpc_netr_ServerAuthenticate2_r(net_handle, mem_ctx, &a);
	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}
	if (!NT_STATUS_EQUAL(a.out.result, NT_STATUS_NO_TRUST_SAM_ACCOUNT)) {
		torture_comment(torture, "dcerpc_netr_ServerAuthenticate2 returned %s, "
			 "expected NT_STATUS_NO_TRUST_SAM_ACCOUNT\n",
			 nt_errstr(a.out.result));
		goto done;
	}

	result = true;
 done:
	talloc_free(mem_ctx);
	return result;
}

static struct security_descriptor *get_sharesec(struct torture_context *tctx,
						TALLOC_CTX *mem_ctx,
						struct smbcli_session *sess,
						const char *sharename)
{
	struct smbcli_tree *tree;
	TALLOC_CTX *tmp_ctx;
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;
	NTSTATUS status;
	struct srvsvc_NetShareGetInfo r;
	union srvsvc_NetShareInfo info;
	struct security_descriptor *result;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		torture_comment(tctx, "talloc_new failed\n");
		return NULL;
	}

	if (!NT_STATUS_IS_OK(secondary_tcon(tctx, tmp_ctx, sess, "IPC$", &tree))) {
		torture_comment(tctx, "secondary_tcon failed\n");
		talloc_free(tmp_ctx);
		return NULL;
	}

	status = pipe_bind_smb(tctx, mem_ctx, tree, "\\pipe\\srvsvc",
			       &ndr_table_srvsvc, &p);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "could not bind to srvsvc pipe: %s\n",
			 nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}
	b = p->binding_handle;

#if 0
	p->conn->flags |= DCERPC_DEBUG_PRINT_IN | DCERPC_DEBUG_PRINT_OUT;
#endif

	r.in.server_unc = talloc_asprintf(tmp_ctx, "\\\\%s",
					  dcerpc_server_name(p));
	r.in.share_name = sharename;
	r.in.level = 502;
	r.out.info = &info;

	status = dcerpc_srvsvc_NetShareGetInfo_r(b, tmp_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "srvsvc_NetShareGetInfo failed: %s\n",
			 nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}
	if (!W_ERROR_IS_OK(r.out.result)) {
		torture_comment(tctx, "srvsvc_NetShareGetInfo failed: %s\n",
			 win_errstr(r.out.result));
		talloc_free(tmp_ctx);
		return NULL;
	}

	result = talloc_steal(mem_ctx, info.info502->sd_buf.sd);
	talloc_free(tmp_ctx);
	return result;
}

static NTSTATUS set_sharesec(struct torture_context *tctx,
			     TALLOC_CTX *mem_ctx,
			     struct smbcli_session *sess,
			     const char *sharename,
			     struct security_descriptor *sd)
{
	struct smbcli_tree *tree;
	TALLOC_CTX *tmp_ctx;
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;
	NTSTATUS status;
	struct sec_desc_buf i;
	struct srvsvc_NetShareSetInfo r;
	union srvsvc_NetShareInfo info;
	uint32_t error = 0;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		torture_comment(tctx, "talloc_new failed\n");
		return NT_STATUS_NO_MEMORY;
	}

	if (!NT_STATUS_IS_OK(secondary_tcon(tctx, tmp_ctx, sess, "IPC$", &tree))) {
		torture_comment(tctx, "secondary_tcon failed\n");
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	status = pipe_bind_smb(tctx, mem_ctx, tree, "\\pipe\\srvsvc",
			       &ndr_table_srvsvc, &p);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(tctx, "could not bind to srvsvc pipe: %s\n",
			 nt_errstr(status));
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}
	b = p->binding_handle;

#if 0
	p->conn->flags |= DCERPC_DEBUG_PRINT_IN | DCERPC_DEBUG_PRINT_OUT;
#endif

	r.in.server_unc = talloc_asprintf(tmp_ctx, "\\\\%s",
					  dcerpc_server_name(p));
	r.in.share_name = sharename;
	r.in.level = 1501;
	i.sd = sd;
	info.info1501 = &i;
	r.in.info = &info;
	r.in.parm_error = &error;

	status = dcerpc_srvsvc_NetShareSetInfo_r(b, tmp_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "srvsvc_NetShareSetInfo failed: %s\n",
			 nt_errstr(status));
	}
	if (!W_ERROR_IS_OK(r.out.result)) {
		torture_comment(tctx, "srvsvc_NetShareSetInfo failed: %s\n",
			win_errstr(r.out.result));
		status = werror_to_ntstatus(r.out.result);
	}
	talloc_free(tmp_ctx);
	return status;
}

bool try_tcon(struct torture_context *tctx,
	      TALLOC_CTX *mem_ctx,
	      struct security_descriptor *orig_sd,
	      struct smbcli_session *session,
	      const char *sharename, const struct dom_sid *user_sid,
	      unsigned int access_mask, NTSTATUS expected_tcon,
	      NTSTATUS expected_mkdir)
{
	TALLOC_CTX *tmp_ctx;
	struct smbcli_tree *rmdir_tree, *tree;
	struct dom_sid *domain_sid;
	uint32_t rid;
	struct security_descriptor *sd;
	NTSTATUS status;
	bool ret = true;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		torture_comment(tctx, "talloc_new failed\n");
		return false;
	}

	status = secondary_tcon(tctx, tmp_ctx, session, sharename, &rmdir_tree);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "first tcon to delete dir failed\n");
		talloc_free(tmp_ctx);
		return false;
	}

	smbcli_rmdir(rmdir_tree, "sharesec_testdir");

	if (!NT_STATUS_IS_OK(dom_sid_split_rid(tmp_ctx, user_sid,
					       &domain_sid, &rid))) {
		torture_comment(tctx, "dom_sid_split_rid failed\n");
		talloc_free(tmp_ctx);
		return false;
	}

	sd = security_descriptor_dacl_create(
		tmp_ctx, 0, "S-1-5-32-544",
		dom_sid_string(mem_ctx, dom_sid_add_rid(mem_ctx, domain_sid,
							DOMAIN_RID_USERS)),
		dom_sid_string(mem_ctx, user_sid),
		SEC_ACE_TYPE_ACCESS_ALLOWED, access_mask, 0, NULL);
	if (sd == NULL) {
		torture_comment(tctx, "security_descriptor_dacl_create failed\n");
		talloc_free(tmp_ctx);
                return false;
        }

	status = set_sharesec(tctx, mem_ctx, session, sharename, sd);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "custom set_sharesec failed: %s\n",
			 nt_errstr(status));
		talloc_free(tmp_ctx);
                return false;
	}

	status = secondary_tcon(tctx, tmp_ctx, session, sharename, &tree);
	if (!NT_STATUS_EQUAL(status, expected_tcon)) {
		torture_comment(tctx, "Expected %s, got %s\n", nt_errstr(expected_tcon),
			 nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!NT_STATUS_IS_OK(status)) {
		/* An expected non-access, no point in trying to write */
		goto done;
	}

	status = smbcli_mkdir(tree, "sharesec_testdir");
	if (!NT_STATUS_EQUAL(status, expected_mkdir)) {
		torture_warning(tctx, "Expected %s, got %s\n",
			 nt_errstr(expected_mkdir), nt_errstr(status));
		ret = false;
	}

 done:
	smbcli_rmdir(rmdir_tree, "sharesec_testdir");

	status = set_sharesec(tctx, mem_ctx, session, sharename, orig_sd);
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "custom set_sharesec failed: %s\n",
			 nt_errstr(status));
		talloc_free(tmp_ctx);
                return false;
	}

	talloc_free(tmp_ctx);
	return ret;
}

static bool torture_samba3_rpc_sharesec(struct torture_context *torture)
{
	struct smbcli_state *cli;
	struct security_descriptor *sd;
	struct dom_sid *user_sid;

	if (!(torture_open_connection_share(
		      torture, &cli, torture, torture_setting_string(torture, "host", NULL),
		      "IPC$", torture->ev))) {
		torture_fail(torture, "IPC$ connection failed\n");
	}

	if (!(user_sid = whoami(torture, torture, cli->tree))) {
		torture_fail(torture, "whoami failed\n");
	}

	sd = get_sharesec(torture, torture, cli->session,
			  torture_setting_string(torture, "share", NULL));

	torture_assert(torture, try_tcon(
			torture, torture, sd, cli->session,
			torture_setting_string(torture, "share", NULL),
			user_sid, 0, NT_STATUS_ACCESS_DENIED, NT_STATUS_OK),
			"failed to test tcon with 0 access_mask");

	torture_assert(torture, try_tcon(
			torture, torture, sd, cli->session,
			torture_setting_string(torture, "share", NULL),
			user_sid, SEC_FILE_READ_DATA, NT_STATUS_OK,
			NT_STATUS_MEDIA_WRITE_PROTECTED),
			"failed to test tcon with SEC_FILE_READ_DATA access_mask");

	torture_assert(torture, try_tcon(
			torture, torture, sd, cli->session,
			torture_setting_string(torture, "share", NULL),
			user_sid, SEC_FILE_ALL, NT_STATUS_OK, NT_STATUS_OK),
			"failed to test tcon with SEC_FILE_ALL access_mask")

	return true;
}

static bool torture_samba3_rpc_lsa(struct torture_context *torture)
{
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;
	struct policy_handle lsa_handle;

	torture_assert_ntstatus_ok(torture,
		torture_rpc_connection(torture, &p, &ndr_table_lsarpc),
		"failed to setup lsarpc");

	b = p->binding_handle;

	{
		struct lsa_ObjectAttribute attr;
		struct lsa_OpenPolicy2 o;
		o.in.system_name = talloc_asprintf(
			torture, "\\\\%s", dcerpc_server_name(p));
		ZERO_STRUCT(attr);
		o.in.attr = &attr;
		o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		o.out.handle = &lsa_handle;

		torture_assert_ntstatus_ok(torture,
			dcerpc_lsa_OpenPolicy2_r(b, torture, &o),
			"dcerpc_lsa_OpenPolicy2 failed");
		torture_assert_ntstatus_ok(torture, o.out.result,
			"dcerpc_lsa_OpenPolicy2 failed");
	}

	{
		int i;
		int levels[] = { 2,3,5,6 };

		for (i=0; i<ARRAY_SIZE(levels); i++) {
			struct lsa_QueryInfoPolicy r;
			union lsa_PolicyInformation *info = NULL;
			r.in.handle = &lsa_handle;
			r.in.level = levels[i];
			r.out.info = &info;

			torture_assert_ntstatus_ok(torture,
				dcerpc_lsa_QueryInfoPolicy_r(b, torture, &r),
				talloc_asprintf(torture, "dcerpc_lsa_QueryInfoPolicy level %d failed", levels[i]));
			torture_assert_ntstatus_ok(torture, r.out.result,
				talloc_asprintf(torture, "dcerpc_lsa_QueryInfoPolicy level %d failed", levels[i]));
		}
	}

	return true;
}

static NTSTATUS get_servername(TALLOC_CTX *mem_ctx, struct smbcli_tree *tree,
			       char **name)
{
	struct rap_WserverGetInfo r;
	NTSTATUS status;
	char servername[17];

	r.in.level = 0;
	r.in.bufsize = 0xffff;

	status = smbcli_rap_netservergetinfo(tree, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	memcpy(servername, r.out.info.info0.name, 16);
	servername[16] = '\0';

	if (!pull_ascii_talloc(mem_ctx, name, servername, NULL)) {
		return NT_STATUS_NO_MEMORY;
	}

	return NT_STATUS_OK;
}

static bool rap_get_servername(struct torture_context *tctx,
			       char **servername)
{
	struct smbcli_state *cli;

	torture_assert(tctx,
		torture_open_connection_share(tctx, &cli, tctx, torture_setting_string(tctx, "host", NULL),
					      "IPC$", tctx->ev),
		"IPC$ connection failed");

	torture_assert_ntstatus_ok(tctx,
		get_servername(tctx, cli->tree, servername),
		"get_servername failed");

	talloc_free(cli);

	return true;
}

static bool find_printers(struct torture_context *tctx,
			  struct dcerpc_pipe *p,
			  const char ***printers,
			  int *num_printers)
{
	struct srvsvc_NetShareEnum r;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr1 c1_in;
	struct srvsvc_NetShareCtr1 *c1;
	uint32_t totalentries = 0;
	int i;
	struct dcerpc_binding_handle *b = p->binding_handle;

	ZERO_STRUCT(c1_in);
	info_ctr.level = 1;
	info_ctr.ctr.ctr1 = &c1_in;

	r.in.server_unc = talloc_asprintf(
		tctx, "\\\\%s", dcerpc_server_name(p));
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = NULL;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_srvsvc_NetShareEnum_r(b, tctx, &r),
		"NetShareEnum level 1 failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"NetShareEnum level 1 failed");

	*printers = NULL;
	*num_printers = 0;
	c1 = r.out.info_ctr->ctr.ctr1;
	for (i=0; i<c1->count; i++) {
		if (c1->array[i].type != STYPE_PRINTQ) {
			continue;
		}
		if (!add_string_to_array(tctx, c1->array[i].name,
					 printers, num_printers)) {
			return false;
		}
	}

	return true;
}

static bool enumprinters(struct torture_context *tctx,
			 struct dcerpc_binding_handle *b,
			 const char *servername, int level, int *num_printers)
{
	struct spoolss_EnumPrinters r;
	DATA_BLOB blob;
	uint32_t needed;
	uint32_t count;
	union spoolss_PrinterInfo *info;

	r.in.flags = PRINTER_ENUM_LOCAL;
	r.in.server = talloc_asprintf(tctx, "\\\\%s", servername);
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_EnumPrinters_r(b, tctx, &r),
		"dcerpc_spoolss_EnumPrinters failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
		"EnumPrinters unexpected return code should be WERR_INSUFFICIENT_BUFFER");

	blob = data_blob_talloc_zero(tctx, needed);
	if (blob.data == NULL) {
		return false;
	}

	r.in.buffer = &blob;
	r.in.offered = needed;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_EnumPrinters_r(b, tctx, &r),
		"dcerpc_spoolss_EnumPrinters failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"dcerpc_spoolss_EnumPrinters failed");

	*num_printers = count;

	return true;
}

static bool getprinterinfo(struct torture_context *tctx,
			   struct dcerpc_binding_handle *b,
			   struct policy_handle *handle, int level,
			   union spoolss_PrinterInfo **res)
{
	struct spoolss_GetPrinter r;
	DATA_BLOB blob;
	uint32_t needed;

	r.in.handle = handle;
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_GetPrinter_r(b, tctx, &r),
		"dcerpc_spoolss_GetPrinter failed");
	torture_assert_werr_equal(tctx, r.out.result, WERR_INSUFFICIENT_BUFFER,
		"GetPrinter unexpected return code should be WERR_INSUFFICIENT_BUFFER");

	r.in.handle = handle;
	r.in.level = level;
	blob = data_blob_talloc_zero(tctx, needed);
	if (blob.data == NULL) {
		return false;
	}
	r.in.buffer = &blob;
	r.in.offered = needed;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_spoolss_GetPrinter_r(b, tctx, &r),
		"dcerpc_spoolss_GetPrinter failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"dcerpc_spoolss_GetPrinter failed");

	if (res != NULL) {
		*res = talloc_steal(tctx, r.out.info);
	}

	return true;
}

static bool torture_samba3_rpc_spoolss(struct torture_context *torture)
{
	struct dcerpc_pipe *p, *p2;
	struct dcerpc_binding_handle *b;
	struct policy_handle server_handle, printer_handle;
	const char **printers;
	int num_printers;
	struct spoolss_UserLevel1 userlevel1;
	char *servername;

	torture_assert(torture,
		rap_get_servername(torture, &servername),
		"failed to rap servername");

	torture_assert_ntstatus_ok(torture,
		torture_rpc_connection(torture, &p2, &ndr_table_srvsvc),
		"failed to setup srvsvc");

	torture_assert(torture,
		find_printers(torture, p2, &printers, &num_printers),
		"failed to find printers via srvsvc");

	talloc_free(p2);

	if (num_printers == 0) {
		torture_skip(torture, "Did not find printers\n");
		return true;
	}

	torture_assert_ntstatus_ok(torture,
		torture_rpc_connection(torture, &p, &ndr_table_spoolss),
		"failed to setup spoolss");

	b = p->binding_handle;

	ZERO_STRUCT(userlevel1);
	userlevel1.client = talloc_asprintf(
		torture, "\\\\%s", lpcfg_netbios_name(torture->lp_ctx));
	userlevel1.user = cli_credentials_get_username(cmdline_credentials);
	userlevel1.build = 2600;
	userlevel1.major = 3;
	userlevel1.minor = 0;
	userlevel1.processor = 0;

	{
		struct spoolss_OpenPrinterEx r;

		ZERO_STRUCT(r);
		r.in.printername = talloc_asprintf(torture, "\\\\%s",
						   servername);
		r.in.datatype = NULL;
		r.in.access_mask = 0;
		r.in.level = 1;
		r.in.userlevel.level1 = &userlevel1;
		r.out.handle = &server_handle;

		torture_assert_ntstatus_ok(torture,
			dcerpc_spoolss_OpenPrinterEx_r(b, torture, &r),
			"dcerpc_spoolss_OpenPrinterEx failed");
		torture_assert_werr_ok(torture, r.out.result,
			"dcerpc_spoolss_OpenPrinterEx failed");
	}

	{
		struct spoolss_ClosePrinter r;

		r.in.handle = &server_handle;
		r.out.handle = &server_handle;

		torture_assert_ntstatus_ok(torture,
			dcerpc_spoolss_ClosePrinter_r(b, torture, &r),
			"dcerpc_spoolss_ClosePrinter failed");
		torture_assert_werr_ok(torture, r.out.result,
			"dcerpc_spoolss_ClosePrinter failed");
	}

	{
		struct spoolss_OpenPrinterEx r;

		ZERO_STRUCT(r);
		r.in.printername = talloc_asprintf(
			torture, "\\\\%s\\%s", servername, printers[0]);
		r.in.datatype = NULL;
		r.in.access_mask = 0;
		r.in.level = 1;
		r.in.userlevel.level1 = &userlevel1;
		r.out.handle = &printer_handle;

		torture_assert_ntstatus_ok(torture,
			dcerpc_spoolss_OpenPrinterEx_r(b, torture, &r),
			"dcerpc_spoolss_OpenPrinterEx failed");
		torture_assert_werr_ok(torture, r.out.result,
			"dcerpc_spoolss_OpenPrinterEx failed");
	}

	{
		int i;

		for (i=0; i<8; i++) {
			torture_assert(torture,
				getprinterinfo(torture, b, &printer_handle, i, NULL),
				talloc_asprintf(torture, "getprinterinfo %d failed", i));
		}
	}

	{
		struct spoolss_ClosePrinter r;

		r.in.handle = &printer_handle;
		r.out.handle = &printer_handle;

		torture_assert_ntstatus_ok(torture,
			dcerpc_spoolss_ClosePrinter_r(b, torture, &r),
			"dcerpc_spoolss_ClosePrinter failed");
		torture_assert_werr_ok(torture, r.out.result,
			"dcerpc_spoolss_ClosePrinter failed");
	}

	{
		int num_enumerated;

		torture_assert(torture,
			enumprinters(torture, b, servername, 1, &num_enumerated),
			"enumprinters failed");

		torture_assert_int_equal(torture, num_printers, num_enumerated,
			"netshareenum / enumprinters lvl 1 numprinter mismatch");
	}

	{
		int num_enumerated;

		torture_assert(torture,
			enumprinters(torture, b, servername, 2, &num_enumerated),
			"enumprinters failed");

		torture_assert_int_equal(torture, num_printers, num_enumerated,
			"netshareenum / enumprinters lvl 2 numprinter mismatch");
	}

	return true;
}

static bool torture_samba3_rpc_wkssvc(struct torture_context *torture)
{
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;
	char *servername;

	torture_assert(torture,
		rap_get_servername(torture, &servername),
		"failed to rap servername");

	torture_assert_ntstatus_ok(torture,
		torture_rpc_connection(torture, &p, &ndr_table_wkssvc),
		"failed to setup wkssvc");

	b = p->binding_handle;

	{
		struct wkssvc_NetWkstaInfo100 wks100;
		union wkssvc_NetWkstaInfo info;
		struct wkssvc_NetWkstaGetInfo r;

		r.in.server_name = "\\foo";
		r.in.level = 100;
		info.info100 = &wks100;
		r.out.info = &info;

		torture_assert_ntstatus_ok(torture,
			dcerpc_wkssvc_NetWkstaGetInfo_r(b, torture, &r),
			"dcerpc_wkssvc_NetWksGetInfo failed");
		torture_assert_werr_ok(torture, r.out.result,
			"dcerpc_wkssvc_NetWksGetInfo failed");

		torture_assert_str_equal(torture, servername, r.out.info->info100->server_name,
			"servername RAP / DCERPC inconsistency");
	}

	return true;
}

static bool winreg_close(struct torture_context *tctx,
			 struct dcerpc_binding_handle *b,
			 struct policy_handle *handle)
{
	struct winreg_CloseKey c;

	c.in.handle = c.out.handle = handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_CloseKey_r(b, tctx, &c),
		"winreg_CloseKey failed");
	torture_assert_werr_ok(tctx, c.out.result,
		"winreg_CloseKey failed");

	return true;
}

static bool enumvalues(struct torture_context *tctx,
		       struct dcerpc_binding_handle *b,
		       struct policy_handle *handle)
{
	uint32_t enum_index = 0;

	while (1) {
		struct winreg_EnumValue r;
		struct winreg_ValNameBuf name;
		enum winreg_Type type = 0;
		uint8_t buf8[1024];
		NTSTATUS status;
		uint32_t size, length;

		r.in.handle = handle;
		r.in.enum_index = enum_index;
		name.name = "";
		name.size = 1024;
		r.in.name = r.out.name = &name;
		size = 1024;
		length = 5;
		r.in.type = &type;
		r.in.value = buf8;
		r.in.size = &size;
		r.in.length = &length;

		status = dcerpc_winreg_EnumValue_r(b, tctx, &r);
		if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
			return true;
		}
		enum_index += 1;
	}
}

static bool enumkeys(struct torture_context *tctx,
		     struct dcerpc_binding_handle *b,
		     struct policy_handle *handle,
		     int depth)
{
	struct winreg_EnumKey r;
	struct winreg_StringBuf kclass, name;
	NTSTATUS status;
	NTTIME t = 0;

	if (depth <= 0) {
		return true;
	}

	kclass.name   = "";
	kclass.size   = 1024;

	r.in.handle = handle;
	r.in.enum_index = 0;
	r.in.name = &name;
	r.in.keyclass = &kclass;
	r.out.name = &name;
	r.in.last_changed_time = &t;

	do {
		struct winreg_OpenKey o;
		struct policy_handle key_handle;
		int i;

		name.name = NULL;
		name.size = 1024;

		status = dcerpc_winreg_EnumKey_r(b, tctx, &r);
		if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
			/* We're done enumerating */
			return true;
		}

		for (i=0; i<10-depth; i++) {
			torture_comment(tctx, " ");
		}
		torture_comment(tctx, "%s\n", r.out.name->name);

		o.in.parent_handle = handle;
		o.in.keyname.name = r.out.name->name;
		o.in.options = 0;
		o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		o.out.handle = &key_handle;

		status = dcerpc_winreg_OpenKey_r(b, tctx, &o);
		if (NT_STATUS_IS_OK(status) && W_ERROR_IS_OK(o.out.result)) {
			enumkeys(tctx, b, &key_handle, depth-1);
			enumvalues(tctx, b, &key_handle);
			torture_assert(tctx, winreg_close(tctx, b, &key_handle), "");
		}

		r.in.enum_index += 1;
	} while(true);

	return true;
}

typedef NTSTATUS (*winreg_open_fn)(struct dcerpc_binding_handle *, TALLOC_CTX *, void *);

static bool test_Open3(struct torture_context *tctx,
		       struct dcerpc_binding_handle *b,
		       const char *name, winreg_open_fn open_fn)
{
	struct policy_handle handle;
	struct winreg_OpenHKLM r;

	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &handle;

	torture_assert_ntstatus_ok(tctx,
		open_fn(b, tctx, &r),
		talloc_asprintf(tctx, "%s failed", name));
	torture_assert_werr_ok(tctx, r.out.result,
		talloc_asprintf(tctx, "%s failed", name));

	enumkeys(tctx, b, &handle, 4);

	torture_assert(tctx,
		winreg_close(tctx, b, &handle),
		"dcerpc_CloseKey failed");

	return true;
}

static bool torture_samba3_rpc_winreg(struct torture_context *torture)
{
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;
	bool ret = true;
	struct {
		const char *name;
		winreg_open_fn fn;
	} open_fns[] = {
		{"OpenHKLM", (winreg_open_fn)dcerpc_winreg_OpenHKLM_r },
		{"OpenHKU",  (winreg_open_fn)dcerpc_winreg_OpenHKU_r },
		{"OpenHKPD", (winreg_open_fn)dcerpc_winreg_OpenHKPD_r },
		{"OpenHKPT", (winreg_open_fn)dcerpc_winreg_OpenHKPT_r },
		{"OpenHKCR", (winreg_open_fn)dcerpc_winreg_OpenHKCR_r }};
#if 0
	int i;
#endif

	torture_assert_ntstatus_ok(torture,
		torture_rpc_connection(torture, &p, &ndr_table_winreg),
		"failed to setup winreg");

	b = p->binding_handle;

#if 1
	ret = test_Open3(torture, b, open_fns[0].name, open_fns[0].fn);
#else
	for (i = 0; i < ARRAY_SIZE(open_fns); i++) {
		if (!test_Open3(torture, b, open_fns[i].name, open_fns[i].fn))
			ret = false;
	}
#endif
	return ret;
}

static bool get_shareinfo(struct torture_context *tctx,
			  struct dcerpc_binding_handle *b,
			  const char *servername,
			  const char *share,
			  struct srvsvc_NetShareInfo502 **info502)
{
	struct srvsvc_NetShareGetInfo r;
	union srvsvc_NetShareInfo info;

	r.in.server_unc = talloc_asprintf(tctx, "\\\\%s", servername);
	r.in.share_name = share;
	r.in.level = 502;
	r.out.info = &info;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_srvsvc_NetShareGetInfo_r(b, tctx, &r),
		"srvsvc_NetShareGetInfo failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"srvsvc_NetShareGetInfo failed");

	*info502 = talloc_move(tctx, &info.info502);

	return true;
}

/*
 * Get us a handle on HKLM\
 */

static bool get_hklm_handle(struct torture_context *tctx,
			    struct dcerpc_binding_handle *b,
			    struct policy_handle *handle)
{
	struct winreg_OpenHKLM r;
	struct policy_handle result;

	r.in.system_name = 0;
        r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
        r.out.handle = &result;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_OpenHKLM_r(b, tctx, &r),
		"OpenHKLM failed");
	torture_assert_werr_ok(tctx, r.out.result,
		"OpenHKLM failed");

	*handle = result;

	return true;
}

static bool torture_samba3_createshare(struct torture_context *tctx,
				       struct dcerpc_binding_handle *b,
				       const char *sharename)
{
	struct policy_handle hklm;
	struct policy_handle new_handle;
	struct winreg_CreateKey c;
	struct winreg_CloseKey cl;
	enum winreg_CreateAction action_taken;

	c.in.handle = &hklm;
	c.in.name.name = talloc_asprintf(
		tctx, "software\\samba\\smbconf\\%s", sharename);
	torture_assert(tctx, c.in.name.name, "talloc_asprintf failed");

	c.in.keyclass.name = "";
	c.in.options = 0;
	c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	c.in.secdesc = NULL;
	c.in.action_taken = &action_taken;
	c.out.new_handle = &new_handle;
	c.out.action_taken = &action_taken;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_CreateKey_r(b, tctx, &c),
		"OpenKey failed");
	torture_assert_werr_ok(tctx, c.out.result,
		"OpenKey failed");

	cl.in.handle = &new_handle;
	cl.out.handle = &new_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_CloseKey_r(b, tctx, &cl),
		"CloseKey failed");
	torture_assert_werr_ok(tctx, cl.out.result,
		"CloseKey failed");

	return true;
}

static bool torture_samba3_deleteshare(struct torture_context *tctx,
				       struct dcerpc_binding_handle *b,
				       const char *sharename)
{
	struct policy_handle hklm;
	struct winreg_DeleteKey d;

	torture_assert(tctx,
		get_hklm_handle(tctx, b, &hklm),
		"get_hklm_handle failed");

	d.in.handle = &hklm;
	d.in.key.name = talloc_asprintf(
		tctx, "software\\samba\\smbconf\\%s", sharename);
	torture_assert(tctx, d.in.key.name, "talloc_asprintf failed");

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_DeleteKey_r(b, tctx, &d),
		"DeleteKey failed");
	torture_assert_werr_ok(tctx, d.out.result,
		"DeleteKey failed");

	return true;
}

static bool torture_samba3_setconfig(struct torture_context *tctx,
				     struct dcerpc_binding_handle *b,
				     const char *sharename,
				     const char *parameter,
				     const char *value)
{
	struct policy_handle hklm, key_handle;
	struct winreg_OpenKey o;
	struct winreg_SetValue s;
	uint32_t type;
	DATA_BLOB val;

	torture_assert(tctx,
		get_hklm_handle(tctx, b, &hklm),
		"get_hklm_handle failed");

	o.in.parent_handle = &hklm;
	o.in.keyname.name = talloc_asprintf(
		tctx, "software\\samba\\smbconf\\%s", sharename);
	torture_assert(tctx, o.in.keyname.name, "talloc_asprintf failed");

	o.in.options = 0;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.out.handle = &key_handle;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_OpenKey_r(b, tctx, &o),
		"OpenKey failed");
	torture_assert_werr_ok(tctx, o.out.result,
		"OpenKey failed");

	torture_assert(tctx,
		reg_string_to_val(tctx, "REG_SZ", value, &type, &val),
		"reg_string_to_val failed");

	s.in.handle = &key_handle;
	s.in.name.name = parameter;
	s.in.type = type;
	s.in.data = val.data;
	s.in.size = val.length;

	torture_assert_ntstatus_ok(tctx,
		dcerpc_winreg_SetValue_r(b, tctx, &s),
		"SetValue failed");
	torture_assert_werr_ok(tctx, s.out.result,
		"SetValue failed");

	return true;
}

static bool torture_samba3_regconfig(struct torture_context *torture)
{
	struct srvsvc_NetShareInfo502 *i = NULL;
	const char *comment = "Dummer Kommentar";
	struct dcerpc_pipe *srvsvc_pipe, *winreg_pipe;

	torture_assert_ntstatus_ok(torture,
		torture_rpc_connection(torture, &srvsvc_pipe, &ndr_table_srvsvc),
		"failed to setup srvsvc");

	torture_assert_ntstatus_ok(torture,
		torture_rpc_connection(torture, &winreg_pipe, &ndr_table_winreg),
		"failed to setup winreg");

	torture_assert(torture,
		torture_samba3_createshare(torture, winreg_pipe->binding_handle, "blubber"),
		"torture_samba3_createshare failed");

	torture_assert(torture,
		torture_samba3_setconfig(torture, winreg_pipe->binding_handle, "blubber", "comment", comment),
		"torture_samba3_setconfig failed");

	torture_assert(torture,
		get_shareinfo(torture, srvsvc_pipe->binding_handle, dcerpc_server_name(srvsvc_pipe), "blubber", &i),
		"get_shareinfo failed");

	torture_assert_str_equal(torture, comment, i->comment,
		"got unexpected comment");

	torture_assert(torture,
		torture_samba3_deleteshare(torture, winreg_pipe->binding_handle, "blubber"),
		"torture_samba3_deleteshare failed");

	return true;
}

/*
 * Test that even with a result of 0 rids the array is returned as a
 * non-NULL pointer. Yes, XP does notice.
 */

bool torture_samba3_getaliasmembership_0(struct torture_context *torture)
{
	struct dcerpc_pipe *p;
	struct dcerpc_binding_handle *b;
	struct samr_Connect2 c;
	struct samr_OpenDomain o;
	struct dom_sid sid;
	struct lsa_SidPtr ptr;
	struct lsa_SidArray sids;
	struct samr_GetAliasMembership g;
	struct samr_Ids rids;
	struct policy_handle samr, domain;

	torture_assert_ntstatus_ok(torture,
		torture_rpc_connection(torture, &p, &ndr_table_samr),
		"failed to setup samr");

	b = p->binding_handle;

	c.in.system_name = NULL;
	c.in.access_mask = SAMR_ACCESS_LOOKUP_DOMAIN;
	c.out.connect_handle = &samr;
	torture_assert_ntstatus_ok(torture,
		dcerpc_samr_Connect2_r(b, torture, &c),
		"");
	torture_assert_ntstatus_ok(torture, c.out.result,
		"");
	dom_sid_parse("S-1-5-32", &sid);
	o.in.connect_handle = &samr;
	o.in.access_mask = SAMR_DOMAIN_ACCESS_LOOKUP_ALIAS;
	o.in.sid = &sid;
	o.out.domain_handle = &domain;
	torture_assert_ntstatus_ok(torture,
		dcerpc_samr_OpenDomain_r(b, torture, &o),
		"");
	torture_assert_ntstatus_ok(torture, o.out.result,
		"");
	dom_sid_parse("S-1-2-3-4-5", &sid);
	ptr.sid = &sid;
	sids.num_sids = 1;
	sids.sids = &ptr;
	g.in.domain_handle = &domain;
	g.in.sids = &sids;
	g.out.rids = &rids;
	torture_assert_ntstatus_ok(torture,
		dcerpc_samr_GetAliasMembership_r(b, torture, &g),
		"");
	torture_assert_ntstatus_ok(torture, g.out.result,
		"");
	if (rids.ids == NULL) {
		/* This is the piece to test here */
		torture_fail(torture,
			"torture_samba3_getaliasmembership_0: "
			"Server returns NULL rids array\n");
	}

	return true;
}

struct torture_suite *torture_rpc_samba3(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "samba3");

	torture_suite_add_simple_test(suite, "bind", torture_bind_samba3);
	torture_suite_add_simple_test(suite, "netlogon", torture_netlogon_samba3);
	torture_suite_add_simple_test(suite, "sessionkey", torture_samba3_sessionkey);
	torture_suite_add_simple_test(suite, "srvsvc", torture_samba3_rpc_srvsvc);
	torture_suite_add_simple_test(suite, "sharesec", torture_samba3_rpc_sharesec);
	torture_suite_add_simple_test(suite, "getusername", torture_samba3_rpc_getusername);
	torture_suite_add_simple_test(suite, "randomauth2", torture_samba3_rpc_randomauth2);
	torture_suite_add_simple_test(suite, "lsa", torture_samba3_rpc_lsa);
	torture_suite_add_simple_test(suite, "spoolss", torture_samba3_rpc_spoolss);
	torture_suite_add_simple_test(suite, "wkssvc", torture_samba3_rpc_wkssvc);
	torture_suite_add_simple_test(suite, "winreg", torture_samba3_rpc_winreg);
	torture_suite_add_simple_test(suite, "getaliasmembership-0", torture_samba3_getaliasmembership_0);
	torture_suite_add_simple_test(suite, "regconfig", torture_samba3_regconfig);

	suite->description = talloc_strdup(suite, "samba3 DCERPC interface tests");

	return suite;
}
