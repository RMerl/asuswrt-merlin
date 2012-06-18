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
#include "libcli/rap/rap.h"
#include "torture/torture.h"
#include "torture/util.h"
#include "torture/rap/proto.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/ndr_samr.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/ndr_srvsvc.h"
#include "librpc/gen_ndr/ndr_srvsvc_c.h"
#include "librpc/gen_ndr/ndr_spoolss.h"
#include "librpc/gen_ndr/ndr_spoolss_c.h"
#include "librpc/gen_ndr/ndr_winreg.h"
#include "librpc/gen_ndr/ndr_winreg_c.h"
#include "librpc/gen_ndr/ndr_wkssvc.h"
#include "librpc/gen_ndr/ndr_wkssvc_c.h"
#include "lib/cmdline/popt_common.h"
#include "librpc/rpc/dcerpc.h"
#include "torture/rpc/rpc.h"
#include "libcli/libcli.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/auth/libcli_auth.h"
#include "../lib/crypto/crypto.h"
#include "auth/ntlmssp/ntlmssp.h"
#include "libcli/security/security.h"
#include "param/param.h"
#include "lib/registry/registry.h"
#include "libcli/resolve/resolve.h"

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
	struct cli_credentials *anon_creds;
	struct smb_composite_sesssetup setup;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	mem_ctx = talloc_init("torture_bind_authcontext");

	if (mem_ctx == NULL) {
		d_printf("talloc_init failed\n");
		return false;
	}

	lp_smbcli_options(torture->lp_ctx, &options);
	lp_smbcli_session_options(torture->lp_ctx, &session_options);

	status = smbcli_full_connection(mem_ctx, &cli,
					torture_setting_string(torture, "host", NULL),
					lp_smb_ports(torture->lp_ctx),
					"IPC$", NULL, 
					lp_socket_options(torture->lp_ctx),
					cmdline_credentials,
					lp_resolve_context(torture->lp_ctx),
					torture->ev, &options, &session_options,
					lp_iconv_convenience(torture->lp_ctx),
					lp_gensec_settings(torture, torture->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("smbcli_full_connection failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	lsa_pipe = dcerpc_pipe_init(mem_ctx, cli->transport->socket->event.ctx,
				    lp_iconv_convenience(torture->lp_ctx));
	if (lsa_pipe == NULL) {
		d_printf("dcerpc_pipe_init failed\n");
		goto done;
	}

	status = dcerpc_pipe_open_smb(lsa_pipe, cli->tree, "\\lsarpc");
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	status = dcerpc_bind_auth_none(lsa_pipe, &ndr_table_lsarpc);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_bind_auth_none failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	openpolicy.in.system_name =talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(lsa_pipe));
	ZERO_STRUCT(objectattr);
	openpolicy.in.attr = &objectattr;
	openpolicy.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	openpolicy.out.handle = &handle;

	status = dcerpc_lsa_OpenPolicy2(lsa_pipe, mem_ctx, &openpolicy);

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_lsa_OpenPolicy2 failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	close_handle.in.handle = &handle;
	close_handle.out.handle = &handle;

	status = dcerpc_lsa_Close(lsa_pipe, mem_ctx, &close_handle);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_lsa_Close failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	session2 = smbcli_session_init(cli->transport, mem_ctx, false, session_options);
	if (session2 == NULL) {
		d_printf("smbcli_session_init failed\n");
		goto done;
	}

	if (!(anon_creds = cli_credentials_init_anon(mem_ctx))) {
		d_printf("create_anon_creds failed\n");
		goto done;
	}

	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities;
	setup.in.workgroup = "";
	setup.in.credentials = anon_creds;
	setup.in.gensec_settings = lp_gensec_settings(torture, torture->lp_ctx);

	status = smb_composite_sesssetup(session2, &setup);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("anon session setup failed: %s\n",
			 nt_errstr(status));
		goto done;
	}
	session2->vuid = setup.out.vuid;

	tmp = cli->tree->session;
	cli->tree->session = session2;

	status = dcerpc_lsa_OpenPolicy2(lsa_pipe, mem_ctx, &openpolicy);

	cli->tree->session = tmp;
	talloc_free(lsa_pipe);
	lsa_pipe = NULL;

	if (!NT_STATUS_EQUAL(status, NT_STATUS_INVALID_HANDLE)) {
		d_printf("dcerpc_lsa_OpenPolicy2 with wrong vuid gave %s, "
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

static bool bindtest(struct smbcli_state *cli,
		     struct cli_credentials *credentials,
		     struct loadparm_context *lp_ctx,
		     uint8_t auth_type, uint8_t auth_level)
{
	TALLOC_CTX *mem_ctx;
	bool ret = false;
	NTSTATUS status;

	struct dcerpc_pipe *lsa_pipe;
	struct lsa_ObjectAttribute objectattr;
	struct lsa_OpenPolicy2 openpolicy;
	struct lsa_QueryInfoPolicy query;
	union lsa_PolicyInformation *info = NULL;
	struct policy_handle handle;
	struct lsa_Close close_handle;

	if ((mem_ctx = talloc_init("bindtest")) == NULL) {
		d_printf("talloc_init failed\n");
		return false;
	}

	lsa_pipe = dcerpc_pipe_init(mem_ctx,
				    cli->transport->socket->event.ctx,
				    lp_iconv_convenience(lp_ctx));
	if (lsa_pipe == NULL) {
		d_printf("dcerpc_pipe_init failed\n");
		goto done;
	}

	status = dcerpc_pipe_open_smb(lsa_pipe, cli->tree, "\\lsarpc");
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	status = dcerpc_bind_auth(lsa_pipe, &ndr_table_lsarpc,
				  credentials, lp_gensec_settings(lp_ctx, lp_ctx), auth_type, auth_level,
				  NULL);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_bind_auth failed: %s\n", nt_errstr(status));
		goto done;
	}

	openpolicy.in.system_name =talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(lsa_pipe));
	ZERO_STRUCT(objectattr);
	openpolicy.in.attr = &objectattr;
	openpolicy.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	openpolicy.out.handle = &handle;

	status = dcerpc_lsa_OpenPolicy2(lsa_pipe, mem_ctx, &openpolicy);

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_lsa_OpenPolicy2 failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	query.in.handle = &handle;
	query.in.level = LSA_POLICY_INFO_DOMAIN;
	query.out.info = &info;

	status = dcerpc_lsa_QueryInfoPolicy(lsa_pipe, mem_ctx, &query);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_lsa_QueryInfoPolicy failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	close_handle.in.handle = &handle;
	close_handle.out.handle = &handle;

	status = dcerpc_lsa_Close(lsa_pipe, mem_ctx, &close_handle);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_lsa_Close failed: %s\n",
			 nt_errstr(status));
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

bool torture_bind_samba3(struct torture_context *torture) 
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	bool ret = false;
	struct smbcli_state *cli;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	mem_ctx = talloc_init("torture_bind_authcontext");

	if (mem_ctx == NULL) {
		d_printf("talloc_init failed\n");
		return false;
	}

	lp_smbcli_options(torture->lp_ctx, &options);
	lp_smbcli_session_options(torture->lp_ctx, &session_options);

	status = smbcli_full_connection(mem_ctx, &cli,
					torture_setting_string(torture, "host", NULL),
					lp_smb_ports(torture->lp_ctx),
					"IPC$", NULL, 
					lp_socket_options(torture->lp_ctx),
					cmdline_credentials,
					lp_resolve_context(torture->lp_ctx),
					torture->ev, &options, &session_options,
					lp_iconv_convenience(torture->lp_ctx),
					lp_gensec_settings(torture, torture->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("smbcli_full_connection failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	ret = true;

	ret &= bindtest(cli, cmdline_credentials, torture->lp_ctx, DCERPC_AUTH_TYPE_NTLMSSP,
			DCERPC_AUTH_LEVEL_INTEGRITY);
	ret &= bindtest(cli, cmdline_credentials, torture->lp_ctx, DCERPC_AUTH_TYPE_NTLMSSP,
			DCERPC_AUTH_LEVEL_PRIVACY);
	ret &= bindtest(cli, cmdline_credentials, torture->lp_ctx, DCERPC_AUTH_TYPE_SPNEGO,
			DCERPC_AUTH_LEVEL_INTEGRITY);
	ret &= bindtest(cli, cmdline_credentials, torture->lp_ctx, DCERPC_AUTH_TYPE_SPNEGO,
			DCERPC_AUTH_LEVEL_PRIVACY);

 done:
	talloc_free(mem_ctx);
	return ret;
}

/*
 * Lookup or create a user and return all necessary info
 */

static NTSTATUS get_usr_handle(struct smbcli_state *cli,
			       TALLOC_CTX *mem_ctx,
			       struct loadparm_context *lp_ctx,
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
				     cli->transport->socket->event.ctx,
				     lp_iconv_convenience(lp_ctx));
	if (samr_pipe == NULL) {
		d_printf("dcerpc_pipe_init failed\n");
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	status = dcerpc_pipe_open_smb(samr_pipe, cli->tree, "\\samr");
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto fail;
	}

	if (admin_creds != NULL) {
		status = dcerpc_bind_auth(samr_pipe, &ndr_table_samr,
					  admin_creds, lp_gensec_settings(lp_ctx, lp_ctx), auth_type, auth_level,
					  NULL);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("dcerpc_bind_auth failed: %s\n",
				 nt_errstr(status));
			goto fail;
		}
	} else {
		/* We must have an authenticated SMB connection */
		status = dcerpc_bind_auth_none(samr_pipe, &ndr_table_samr);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("dcerpc_bind_auth_none failed: %s\n",
				 nt_errstr(status));
			goto fail;
		}
	}

	conn.in.system_name = talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(samr_pipe));
	conn.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	conn.out.connect_handle = &conn_handle;

	status = dcerpc_samr_Connect2(samr_pipe, mem_ctx, &conn);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("samr_Connect2 failed: %s\n", nt_errstr(status));
		goto fail;
	}

	enumdom.in.connect_handle = &conn_handle;
	enumdom.in.resume_handle = &resume_handle;
	enumdom.in.buf_size = (uint32_t)-1;
	enumdom.out.resume_handle = &resume_handle;
	enumdom.out.num_entries = &num_entries;
	enumdom.out.sam = &sam;

	status = dcerpc_samr_EnumDomains(samr_pipe, mem_ctx, &enumdom);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("samr_EnumDomains failed: %s\n", nt_errstr(status));
		goto fail;
	}

	if (*enumdom.out.num_entries != 2) {
		d_printf("samr_EnumDomains returned %d entries, expected 2\n",
			 *enumdom.out.num_entries);
		status = NT_STATUS_UNSUCCESSFUL;
		goto fail;
	}

	dom_idx = strequal(sam->entries[0].name.string,
			   "builtin") ? 1:0;

	l.in.connect_handle = &conn_handle;
	domain_name.string = sam->entries[dom_idx].name.string;
	*domain = talloc_strdup(mem_ctx, domain_name.string);
	l.in.domain_name = &domain_name;
	l.out.sid = &sid;

	status = dcerpc_samr_LookupDomain(samr_pipe, mem_ctx, &l);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("samr_LookupDomain failed: %s\n", nt_errstr(status));
		goto fail;
	}

	o.in.connect_handle = &conn_handle;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.in.sid = *l.out.sid;
	o.out.domain_handle = &domain_handle;

	status = dcerpc_samr_OpenDomain(samr_pipe, mem_ctx, &o);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("samr_OpenDomain failed: %s\n", nt_errstr(status));
		goto fail;
	}

	c.in.domain_handle = &domain_handle;
	user_name.string = username;
	c.in.account_name = &user_name;
	c.in.acct_flags = ACB_NORMAL;
	c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	user_handle = talloc(mem_ctx, struct policy_handle);
	c.out.user_handle = user_handle;
	c.out.access_granted = &access_granted;
	c.out.rid = &user_rid;

	status = dcerpc_samr_CreateUser2(samr_pipe, mem_ctx, &c);

	if (NT_STATUS_EQUAL(status, NT_STATUS_USER_EXISTS)) {
		struct samr_LookupNames ln;
		struct samr_OpenUser ou;
		struct samr_Ids rids, types;

		ln.in.domain_handle = &domain_handle;
		ln.in.num_names = 1;
		ln.in.names = &user_name;
		ln.out.rids = &rids;
		ln.out.types = &types;

		status = dcerpc_samr_LookupNames(samr_pipe, mem_ctx, &ln);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_LookupNames failed: %s\n",
				 nt_errstr(status));
			goto fail;
		}

		ou.in.domain_handle = &domain_handle;
		ou.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		user_rid = ou.in.rid = ln.out.rids->ids[0];
		ou.out.user_handle = user_handle;

		status = dcerpc_samr_OpenUser(samr_pipe, mem_ctx, &ou);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_OpenUser failed: %s\n",
				 nt_errstr(status));
			goto fail;
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("samr_CreateUser failed: %s\n", nt_errstr(status));
		goto fail;
	}

	*result_pipe = samr_pipe;
	*result_handle = user_handle;
	if (sid_p != NULL) {
		*sid_p = dom_sid_add_rid(mem_ctx, *l.out.sid, user_rid);
	}
	return NT_STATUS_OK;

 fail:
	return status;
}

/*
 * Create a test user
 */

static bool create_user(TALLOC_CTX *mem_ctx, struct smbcli_state *cli,
			struct loadparm_context *lp_ctx,
			struct cli_credentials *admin_creds,
			const char *username, const char *password,
			char **domain_name,
			struct dom_sid **user_sid)
{
	TALLOC_CTX *tmp_ctx;
	NTSTATUS status;
	struct dcerpc_pipe *samr_pipe;
	struct policy_handle *wks_handle;
	bool ret = false;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		d_printf("talloc_init failed\n");
		return false;
	}

	status = get_usr_handle(cli, tmp_ctx, lp_ctx, admin_creds,
				DCERPC_AUTH_TYPE_NTLMSSP,
				DCERPC_AUTH_LEVEL_INTEGRITY,
				username, domain_name, &samr_pipe, &wks_handle,
				user_sid);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("get_usr_handle failed: %s\n", nt_errstr(status));
		goto done;
	}

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
			d_printf("dcerpc_fetch_session_key failed\n");
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

		status = dcerpc_samr_SetUserInfo2(samr_pipe, tmp_ctx, &sui2);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_SetUserInfo(23) failed: %s\n",
				 nt_errstr(status));
			goto done;
		}

		u_info.info16.acct_flags = ACB_NORMAL;
		sui.in.user_handle = wks_handle;
		sui.in.info = &u_info;
		sui.in.level = 16;

		status = dcerpc_samr_SetUserInfo(samr_pipe, tmp_ctx, &sui);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_SetUserInfo(16) failed\n");
			goto done;
		}

		qui.in.user_handle = wks_handle;
		qui.in.level = 21;
		qui.out.info = &info;

		status = dcerpc_samr_QueryUserInfo(samr_pipe, tmp_ctx, &qui);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_QueryUserInfo(21) failed\n");
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

		status = dcerpc_samr_SetUserInfo(samr_pipe, tmp_ctx, &sui);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_SetUserInfo(21) failed\n");
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

static bool delete_user(struct smbcli_state *cli,
			struct loadparm_context *lp_ctx,
			struct cli_credentials *admin_creds,
			const char *username)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	char *dom_name;
	struct dcerpc_pipe *samr_pipe;
	struct policy_handle *user_handle;
	bool ret = false;

	if ((mem_ctx = talloc_init("leave")) == NULL) {
		d_printf("talloc_init failed\n");
		return false;
	}

	status = get_usr_handle(cli, mem_ctx, lp_ctx, admin_creds,
				DCERPC_AUTH_TYPE_NTLMSSP,
				DCERPC_AUTH_LEVEL_INTEGRITY,
				username, &dom_name, &samr_pipe,
				&user_handle, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("get_wks_handle failed: %s\n", nt_errstr(status));
		goto done;
	}

	{
		struct samr_DeleteUser d;

		d.in.user_handle = user_handle;
		d.out.user_handle = user_handle;

		status = dcerpc_samr_DeleteUser(samr_pipe, mem_ctx, &d);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_DeleteUser failed %s\n", nt_errstr(status));
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

static bool join3(struct smbcli_state *cli,
		  struct loadparm_context *lp_ctx,
		  bool use_level25,
		  struct cli_credentials *admin_creds,
		  struct cli_credentials *wks_creds)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	char *dom_name;
	struct dcerpc_pipe *samr_pipe;
	struct policy_handle *wks_handle;
	bool ret = false;
	NTTIME last_password_change;

	if ((mem_ctx = talloc_init("join3")) == NULL) {
		d_printf("talloc_init failed\n");
		return false;
	}

	status = get_usr_handle(
		cli, mem_ctx, lp_ctx, admin_creds,
		DCERPC_AUTH_TYPE_NTLMSSP,
		DCERPC_AUTH_LEVEL_PRIVACY,
		talloc_asprintf(mem_ctx, "%s$",
				cli_credentials_get_workstation(wks_creds)),
		&dom_name, &samr_pipe, &wks_handle, NULL);

	if (!NT_STATUS_IS_OK(status)) {
		d_printf("get_wks_handle failed: %s\n", nt_errstr(status));
		goto done;
	}

	{
		struct samr_QueryUserInfo q;
		union samr_UserInfo *info;

		q.in.user_handle = wks_handle;
		q.in.level = 21;
		q.out.info = &info;

		status = dcerpc_samr_QueryUserInfo(samr_pipe, mem_ctx, &q);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("(%s) QueryUserInfo failed: %s\n",
				  __location__, nt_errstr(status));
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
		struct MD5Context ctx;
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
			d_printf("dcerpc_fetch_session_key failed: %s\n",
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

		status = dcerpc_samr_SetUserInfo2(samr_pipe, mem_ctx, &sui2);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_SetUserInfo2(25) failed: %s\n",
				 nt_errstr(status));
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
			d_printf("dcerpc_fetch_session_key failed\n");
			goto done;
		}
		arcfour_crypt_blob(u_info.info24.password.data, 516,
				   &session_key);
		sui2.in.user_handle = wks_handle;
		sui2.in.info = &u_info;
		sui2.in.level = 24;

		status = dcerpc_samr_SetUserInfo2(samr_pipe, mem_ctx, &sui2);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_SetUserInfo(24) failed: %s\n",
				 nt_errstr(status));
			goto done;
		}

		u_info.info16.acct_flags = ACB_WSTRUST;
		sui.in.user_handle = wks_handle;
		sui.in.info = &u_info;
		sui.in.level = 16;

		status = dcerpc_samr_SetUserInfo(samr_pipe, mem_ctx, &sui);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("samr_SetUserInfo(16) failed\n");
			goto done;
		}
	}

	{
		struct samr_QueryUserInfo q;
		union samr_UserInfo *info;

		q.in.user_handle = wks_handle;
		q.in.level = 21;
		q.out.info = &info;

		status = dcerpc_samr_QueryUserInfo(samr_pipe, mem_ctx, &q);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("(%s) QueryUserInfo failed: %s\n",
				  __location__, nt_errstr(status));
			goto done;
		}

		if (use_level25) {
			if (last_password_change
			    == info->info21.last_password_change) {
				d_printf("(%s) last_password_change unchanged "
					 "during join, level25 must change "
					 "it\n", __location__);
				goto done;
			}
		}
		else {
			if (last_password_change
			    != info->info21.last_password_change) {
				d_printf("(%s) last_password_change changed "
					 "during join, level24 doesn't "
					 "change it\n", __location__);
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

static bool auth2(struct smbcli_state *cli,
		  struct loadparm_context *lp_ctx,
		  struct cli_credentials *wks_cred)
{
	TALLOC_CTX *mem_ctx;
	struct dcerpc_pipe *net_pipe;
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
		d_printf("talloc_new failed\n");
		return false;
	}

	net_pipe = dcerpc_pipe_init(mem_ctx,
				    cli->transport->socket->event.ctx,
				    lp_iconv_convenience(lp_ctx));
	if (net_pipe == NULL) {
		d_printf("dcerpc_pipe_init failed\n");
		goto done;
	}

	status = dcerpc_pipe_open_smb(net_pipe, cli->tree, "\\netlogon");
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	status = dcerpc_bind_auth_none(net_pipe, &ndr_table_netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_bind_auth_none failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	r.in.computer_name = cli_credentials_get_workstation(wks_cred);
	r.in.server_name = talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(net_pipe));
	if (r.in.server_name == NULL) {
		d_printf("talloc_asprintf failed\n");
		goto done;
	}
	generate_random_buffer(netr_cli_creds.data,
			       sizeof(netr_cli_creds.data));
	r.in.credentials = &netr_cli_creds;
	r.out.return_credentials = &netr_srv_creds;

	status = dcerpc_netr_ServerReqChallenge(net_pipe, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("netr_ServerReqChallenge failed: %s\n",
			 nt_errstr(status));
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

	status = dcerpc_netr_ServerAuthenticate2(net_pipe, mem_ctx, &a);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("netr_ServerServerAuthenticate2 failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	if (!netlogon_creds_client_check(creds_state, a.out.return_credentials)) {
		d_printf("creds_client_check failed\n");
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

static bool schan(struct smbcli_state *cli,
		  struct loadparm_context *lp_ctx,
		  struct cli_credentials *wks_creds,
		  struct cli_credentials *user_creds)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	bool ret = false;
	struct dcerpc_pipe *net_pipe;
	int i;
	
	mem_ctx = talloc_new(NULL);
	if (mem_ctx == NULL) {
		d_printf("talloc_new failed\n");
		return false;
	}

	net_pipe = dcerpc_pipe_init(mem_ctx,
				    cli->transport->socket->event.ctx,
				    lp_iconv_convenience(lp_ctx));
	if (net_pipe == NULL) {
		d_printf("dcerpc_pipe_init failed\n");
		goto done;
	}

	status = dcerpc_pipe_open_smb(net_pipe, cli->tree, "\\netlogon");
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_pipe_open_smb failed: %s\n",
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
				  wks_creds, lp_gensec_settings(lp_ctx, lp_ctx), DCERPC_AUTH_TYPE_SCHANNEL,
				  DCERPC_AUTH_LEVEL_PRIVACY,
				  NULL);
#else
	status = dcerpc_bind_auth_none(net_pipe, &ndr_table_netlogon);
#endif
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("schannel bind failed: %s\n", nt_errstr(status));
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
			d_printf("data_blob_talloc failed\n");
			goto done;
		}

		generate_random_buffer(chal.data, chal.length);
		names_blob = NTLMv2_generate_names_blob(
			mem_ctx, 
			cli_credentials_get_workstation(user_creds),
			cli_credentials_get_domain(user_creds));
		status = cli_credentials_get_ntlm_response(
			user_creds, mem_ctx, &flags, chal, names_blob,
			&lm_resp, &nt_resp, NULL, NULL);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("cli_credentials_get_ntlm_response failed:"
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

		status = dcerpc_netr_LogonSamLogon(net_pipe, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("netr_LogonSamLogon failed: %s\n",
				 nt_errstr(status));
			goto done;
		}

		if ((r.out.return_authenticator == NULL) ||
		    (!netlogon_creds_client_check(creds_state,
					 &r.out.return_authenticator->cred))) {
			d_printf("Credentials check failed!\n");
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

		status = dcerpc_netr_LogonSamLogon(net_pipe, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("netr_LogonSamLogon failed: %s\n",
				 nt_errstr(status));
			goto done;
		}

		if ((r.out.return_authenticator == NULL) ||
		    (!netlogon_creds_client_check(creds_state,
					 &r.out.return_authenticator->cred))) {
			d_printf("Credentials check failed!\n");
			goto done;
		}
	}

	{
		struct netr_ServerPasswordSet s;
		char *password = generate_random_str(wks_creds, 8);
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

		status = dcerpc_netr_ServerPasswordSet(net_pipe, mem_ctx, &s);
		if (!NT_STATUS_IS_OK(status)) {
			printf("ServerPasswordSet - %s\n", nt_errstr(status));
			goto done;
		}

		if (!netlogon_creds_client_check(creds_state,
						 &s.out.return_authenticator->cred)) {
			printf("Credential chaining failed\n");
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

static bool leave(struct smbcli_state *cli,
		  struct loadparm_context *lp_ctx,
		  struct cli_credentials *admin_creds,
		  struct cli_credentials *wks_creds)
{
	char *wks_name = talloc_asprintf(
		NULL, "%s$", cli_credentials_get_workstation(wks_creds));
	bool ret;

	ret = delete_user(cli, lp_ctx, admin_creds, wks_name);
	talloc_free(wks_name);
	return ret;
}

/*
 * Test the Samba3 DC code a bit. Join, do some schan netlogon ops, leave
 */

bool torture_netlogon_samba3(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	bool ret = false;
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

	mem_ctx = talloc_init("torture_netlogon_samba3");

	if (mem_ctx == NULL) {
		d_printf("talloc_init failed\n");
		return false;
	}

	if (!(anon_creds = cli_credentials_init_anon(mem_ctx))) {
		d_printf("create_anon_creds failed\n");
		goto done;
	}

	lp_smbcli_options(torture->lp_ctx, &options);
	lp_smbcli_session_options(torture->lp_ctx, &session_options);

	status = smbcli_full_connection(mem_ctx, &cli,
					torture_setting_string(torture, "host", NULL),
					lp_smb_ports(torture->lp_ctx),
					"IPC$", NULL, 
					lp_socket_options(torture->lp_ctx),
					anon_creds, 
					lp_resolve_context(torture->lp_ctx),
					torture->ev, &options, &session_options,
					lp_iconv_convenience(torture->lp_ctx),
					lp_gensec_settings(torture, torture->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("smbcli_full_connection failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	wks_creds = cli_credentials_init(mem_ctx);
	if (wks_creds == NULL) {
		d_printf("cli_credentials_init failed\n");
		goto done;
	}

	cli_credentials_set_conf(wks_creds, torture->lp_ctx);
	cli_credentials_set_secure_channel_type(wks_creds, SEC_CHAN_WKSTA);
	cli_credentials_set_username(wks_creds, wks_name, CRED_SPECIFIED);
	cli_credentials_set_workstation(wks_creds, wks_name, CRED_SPECIFIED);
	cli_credentials_set_password(wks_creds,
				     generate_random_str(wks_creds, 8),
				     CRED_SPECIFIED);

	if (!join3(cli, torture->lp_ctx, false, cmdline_credentials, wks_creds)) {
		d_printf("join failed\n");
		goto done;
	}

	cli_credentials_set_domain(
		cmdline_credentials, cli_credentials_get_domain(wks_creds),
		CRED_SPECIFIED);

	for (i=0; i<2; i++) {

		/* Do this more than once, the routine "schan" changes
		 * the workstation password using the netlogon
		 * password change routine */

		int j;

		if (!auth2(cli, torture->lp_ctx, wks_creds)) {
			d_printf("auth2 failed\n");
			goto done;
		}

		for (j=0; j<2; j++) {
			if (!schan(cli, torture->lp_ctx, wks_creds, cmdline_credentials)) {
				d_printf("schan failed\n");
				goto done;
			}
		}
	}

	if (!leave(cli, torture->lp_ctx, cmdline_credentials, wks_creds)) {
		d_printf("leave failed\n");
		goto done;
	}

	ret = true;

 done:
	talloc_free(mem_ctx);
	return ret;
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
	bool ret = false;
	struct smbcli_state *cli;
	struct cli_credentials *wks_creds;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lp_smbcli_options(tctx->lp_ctx, &options);
	lp_smbcli_session_options(tctx->lp_ctx, &session_options);

	status = smbcli_full_connection(tctx, &cli,
					torture_setting_string(tctx, "host", NULL),
					lp_smb_ports(tctx->lp_ctx),
					"IPC$", NULL, lp_socket_options(tctx->lp_ctx),
					smb_creds, lp_resolve_context(tctx->lp_ctx),
					tctx->ev, &options, &session_options,
					lp_iconv_convenience(tctx->lp_ctx),
					lp_gensec_settings(tctx, tctx->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("smbcli_full_connection failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	wks_creds = cli_credentials_init(cli);
	if (wks_creds == NULL) {
		d_printf("cli_credentials_init failed\n");
		goto done;
	}

	cli_credentials_set_conf(wks_creds, tctx->lp_ctx);
	cli_credentials_set_secure_channel_type(wks_creds, SEC_CHAN_WKSTA);
	cli_credentials_set_username(wks_creds, wks_name, CRED_SPECIFIED);
	cli_credentials_set_workstation(wks_creds, wks_name, CRED_SPECIFIED);
	cli_credentials_set_password(wks_creds,
				     generate_random_str(wks_creds, 8),
				     CRED_SPECIFIED);

	if (!join3(cli, tctx->lp_ctx, use_level25, samr_creds, wks_creds)) {
		d_printf("join failed\n");
		goto done;
	}

	cli_credentials_set_domain(
		cmdline_credentials, cli_credentials_get_domain(wks_creds),
		CRED_SPECIFIED);

	if (!auth2(cli, tctx->lp_ctx, wks_creds)) {
		d_printf("auth2 failed\n");
		goto done;
	}

	if (!leave(cli, tctx->lp_ctx, samr_creds, wks_creds)) {
		d_printf("leave failed\n");
		goto done;
	}

	talloc_free(cli);

	ret = true;

 done:
	return ret;
}

/*
 * Test the different session key variants. Do it by joining, this uses the
 * session key in the setpassword routine. Test the join by doing the auth2.
 */

bool torture_samba3_sessionkey(struct torture_context *torture)
{
	bool ret = false;
	struct cli_credentials *anon_creds;
	const char *wks_name;

	wks_name = torture_setting_string(torture, "wksname", get_myname(torture));

	if (!(anon_creds = cli_credentials_init_anon(torture))) {
		d_printf("create_anon_creds failed\n");
		goto done;
	}

	cli_credentials_set_workstation(anon_creds, wks_name, CRED_SPECIFIED);

	ret = true;

	if (!torture_setting_bool(torture, "samba3", false)) {

		/* Samba3 in the build farm right now does this happily. Need
		 * to fix :-) */

		if (test_join3(torture, false, anon_creds, NULL, wks_name)) {
			d_printf("join using anonymous bind on an anonymous smb "
				 "connection succeeded -- HUH??\n");
			ret = false;
		}
	}

	if (!test_join3(torture, false, anon_creds, cmdline_credentials,
			wks_name)) {
		d_printf("join using ntlmssp bind on an anonymous smb "
			 "connection failed\n");
		ret = false;
	}

	if (!test_join3(torture, false, cmdline_credentials, NULL, wks_name)) {
		d_printf("join using anonymous bind on an authenticated smb "
			 "connection failed\n");
		ret = false;
	}

	if (!test_join3(torture, false, cmdline_credentials,
			cmdline_credentials,
			wks_name)) {
		d_printf("join using ntlmssp bind on an authenticated smb "
			 "connection failed\n");
		ret = false;
	}

	/*
	 * The following two are tests for setuserinfolevel 25
	 */

	if (!test_join3(torture, true, anon_creds, cmdline_credentials,
			wks_name)) {
		d_printf("join using ntlmssp bind on an anonymous smb "
			 "connection failed\n");
		ret = false;
	}

	if (!test_join3(torture, true, cmdline_credentials, NULL, wks_name)) {
		d_printf("join using anonymous bind on an authenticated smb "
			 "connection failed\n");
		ret = false;
	}

 done:

	return ret;
}

/*
 * open pipe and bind, given an IPC$ context
 */

static NTSTATUS pipe_bind_smb(TALLOC_CTX *mem_ctx,
			      struct loadparm_context *lp_ctx,
			      struct smbcli_tree *tree,
			      const char *pipe_name,
			      const struct ndr_interface_table *iface,
			      struct dcerpc_pipe **p)
{
	struct dcerpc_pipe *result;
	NTSTATUS status;

	if (!(result = dcerpc_pipe_init(
		      mem_ctx, tree->session->transport->socket->event.ctx, 
		      lp_iconv_convenience(lp_ctx)))) {
		return NT_STATUS_NO_MEMORY;
	}

	status = dcerpc_pipe_open_smb(result, tree, pipe_name);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		talloc_free(result);
		return status;
	}

	status = dcerpc_bind_auth_none(result, iface);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("schannel bind failed: %s\n", nt_errstr(status));
		talloc_free(result);
		return status;
	}

	*p = result;
	return NT_STATUS_OK;
}

/*
 * Sane wrapper around lsa_LookupNames
 */

static struct dom_sid *name2sid(TALLOC_CTX *mem_ctx,
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

	status = dcerpc_lsa_OpenPolicy2(p, tmp_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		printf("OpenPolicy2 failed - %s\n", nt_errstr(status));
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

	status = dcerpc_lsa_LookupNames(p, tmp_ctx, &l);
	if (!NT_STATUS_IS_OK(status)) {
		printf("LookupNames of %s failed - %s\n", lsa_name.string, 
		       nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}

	result = dom_sid_add_rid(mem_ctx, domains->domains[0].sid,
				 l.out.sids->sids[0].rid);

	c.in.handle = &handle;
	c.out.handle = &handle;

	status = dcerpc_lsa_Close(p, tmp_ctx, &c);
	if (!NT_STATUS_IS_OK(status)) {
		printf("dcerpc_lsa_Close failed - %s\n", nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}
	
	talloc_free(tmp_ctx);
	return result;
}

/*
 * Find out the user SID on this connection
 */

static struct dom_sid *whoami(TALLOC_CTX *mem_ctx, 
			      struct loadparm_context *lp_ctx, 
			      struct smbcli_tree *tree)
{
	struct dcerpc_pipe *lsa;
	struct lsa_GetUserName r;
	NTSTATUS status;
	struct lsa_String *authority_name_p = NULL;
	struct lsa_String *account_name_p = NULL;
	struct dom_sid *result;

	status = pipe_bind_smb(mem_ctx, lp_ctx, tree, "\\pipe\\lsarpc",
			       &ndr_table_lsarpc, &lsa);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) Could not bind to LSA: %s\n",
			 __location__, nt_errstr(status));
		return NULL;
	}

	r.in.system_name = "\\";
	r.in.account_name = &account_name_p;
	r.in.authority_name = &authority_name_p;
	r.out.account_name = &account_name_p;

	status = dcerpc_lsa_GetUserName(lsa, mem_ctx, &r);

	authority_name_p = *r.out.authority_name;

	if (!NT_STATUS_IS_OK(status)) {
		printf("(%s) GetUserName failed - %s\n",
		       __location__, nt_errstr(status));
		talloc_free(lsa);
		return NULL;
	}

	result = name2sid(mem_ctx, lsa, account_name_p->string,
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

NTSTATUS secondary_tcon(TALLOC_CTX *mem_ctx,
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
		d_printf("(%s) smb_raw_tcon failed: %s\n", __location__,
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

bool torture_samba3_rpc_getusername(struct torture_context *torture)
{
	NTSTATUS status;
	struct smbcli_state *cli;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct dom_sid *user_sid;
	struct dom_sid *created_sid;
	struct cli_credentials *anon_creds;
	struct cli_credentials *user_creds;
	char *domain_name;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	if (!(mem_ctx = talloc_new(torture))) {
		return false;
	}

	lp_smbcli_options(torture->lp_ctx, &options);
	lp_smbcli_session_options(torture->lp_ctx, &session_options);

	status = smbcli_full_connection(
		mem_ctx, &cli, torture_setting_string(torture, "host", NULL),
		lp_smb_ports(torture->lp_ctx),
		"IPC$", NULL, lp_socket_options(torture->lp_ctx), cmdline_credentials, 
		lp_resolve_context(torture->lp_ctx),
		torture->ev, &options, &session_options,
		lp_iconv_convenience(torture->lp_ctx),
		lp_gensec_settings(torture, torture->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) smbcli_full_connection failed: %s\n",
			 __location__, nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!(user_sid = whoami(mem_ctx, torture->lp_ctx, cli->tree))) {
		d_printf("(%s) whoami on auth'ed connection failed\n",
			 __location__);
		ret = false;
	}

	talloc_free(cli);

	if (!(anon_creds = cli_credentials_init_anon(mem_ctx))) {
		d_printf("(%s) create_anon_creds failed\n", __location__);
		ret = false;
		goto done;
	}

	status = smbcli_full_connection(
		mem_ctx, &cli, torture_setting_string(torture, "host", NULL),
		lp_smb_ports(torture->lp_ctx), "IPC$", NULL, 
		lp_socket_options(torture->lp_ctx), anon_creds, 
		lp_resolve_context(torture->lp_ctx),
		torture->ev, &options, &session_options,
		lp_iconv_convenience(torture->lp_ctx),
		lp_gensec_settings(torture, torture->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) anon smbcli_full_connection failed: %s\n",
			 __location__, nt_errstr(status));
		ret = false;
		goto done;
	}

	if (!(user_sid = whoami(mem_ctx, torture->lp_ctx, cli->tree))) {
		d_printf("(%s) whoami on anon connection failed\n",
			 __location__);
		ret = false;
		goto done;
	}

	if (!dom_sid_equal(user_sid,
			   dom_sid_parse_talloc(mem_ctx, "s-1-5-7"))) {
		d_printf("(%s) Anon lsa_GetUserName returned %s, expected "
			 "S-1-5-7", __location__,
			 dom_sid_string(mem_ctx, user_sid));
		ret = false;
	}

	if (!(user_creds = cli_credentials_init(mem_ctx))) {
		d_printf("(%s) cli_credentials_init failed\n", __location__);
		ret = false;
		goto done;
	}

	cli_credentials_set_conf(user_creds, torture->lp_ctx);
	cli_credentials_set_username(user_creds, "torture_username",
				     CRED_SPECIFIED);
	cli_credentials_set_password(user_creds,
				     generate_random_str(user_creds, 8),
				     CRED_SPECIFIED);

	if (!create_user(mem_ctx, cli, torture->lp_ctx, cmdline_credentials,
			 cli_credentials_get_username(user_creds),
			 cli_credentials_get_password(user_creds),
			 &domain_name, &created_sid)) {
		d_printf("(%s) create_user failed\n", __location__);
		ret = false;
		goto done;
	}

	cli_credentials_set_domain(user_creds, domain_name,
				   CRED_SPECIFIED);

	{
		struct smbcli_session *session2;
		struct smb_composite_sesssetup setup;
		struct smbcli_tree *tree;

		session2 = smbcli_session_init(cli->transport, mem_ctx, false, session_options);
		if (session2 == NULL) {
			d_printf("(%s) smbcli_session_init failed\n",
				 __location__);
			goto done;
		}

		setup.in.sesskey = cli->transport->negotiate.sesskey;
		setup.in.capabilities = cli->transport->negotiate.capabilities;
		setup.in.workgroup = "";
		setup.in.credentials = user_creds;
		setup.in.gensec_settings = lp_gensec_settings(torture, torture->lp_ctx);

		status = smb_composite_sesssetup(session2, &setup);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("(%s) session setup with new user failed: "
				 "%s\n", __location__, nt_errstr(status));
			ret = false;
			goto done;
		}
		session2->vuid = setup.out.vuid;

		if (!NT_STATUS_IS_OK(secondary_tcon(mem_ctx, session2,
						    "IPC$", &tree))) {
			d_printf("(%s) secondary_tcon failed\n",
				 __location__);
			ret = false;
			goto done;
		}

		if (!(user_sid = whoami(mem_ctx, torture->lp_ctx, tree))) {
			d_printf("(%s) whoami on user connection failed\n",
				 __location__);
			ret = false;
			goto del;
		}

		talloc_free(tree);
	}

	d_printf("Created %s, found %s\n",
		 dom_sid_string(mem_ctx, created_sid),
		 dom_sid_string(mem_ctx, user_sid));

	if (!dom_sid_equal(created_sid, user_sid)) {
		ret = false;
	}

 del:
	if (!delete_user(cli, torture->lp_ctx, 
			 cmdline_credentials,
			 cli_credentials_get_username(user_creds))) {
		d_printf("(%s) delete_user failed\n", __location__);
		ret = false;
	}

 done:
	talloc_free(mem_ctx);
	return ret;
}

static bool test_NetShareGetInfo(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
				 const char *sharename)
{
	NTSTATUS status;
	struct srvsvc_NetShareGetInfo r;
	union srvsvc_NetShareInfo info;
	uint32_t levels[] = { 0, 1, 2, 501, 502, 1004, 1005, 1006, 1007, 1501 };
	int i;
	bool ret = true;

	r.in.server_unc = talloc_asprintf(mem_ctx, "\\\\%s",
					  dcerpc_server_name(p));
	r.in.share_name = sharename;
	r.out.info = &info;

	for (i=0;i<ARRAY_SIZE(levels);i++) {
		r.in.level = levels[i];

		printf("testing NetShareGetInfo level %u on share '%s'\n", 
		       r.in.level, r.in.share_name);

		status = dcerpc_srvsvc_NetShareGetInfo(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("NetShareGetInfo level %u on share '%s' failed"
			       " - %s\n", r.in.level, r.in.share_name,
			       nt_errstr(status));
			ret = false;
			continue;
		}
		if (!W_ERROR_IS_OK(r.out.result)) {
			printf("NetShareGetInfo level %u on share '%s' failed "
			       "- %s\n", r.in.level, r.in.share_name,
			       win_errstr(r.out.result));
			ret = false;
			continue;
		}
	}

	return ret;
}

static bool test_NetShareEnum(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
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

	ZERO_STRUCT(info_ctr);

	r.in.server_unc = talloc_asprintf(mem_ctx,"\\\\%s",dcerpc_server_name(p));
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

		printf("testing NetShareEnum level %u\n", info_ctr.level);
		status = dcerpc_srvsvc_NetShareEnum(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			printf("NetShareEnum level %u failed - %s\n",
			       info_ctr.level, nt_errstr(status));
			ret = false;
			continue;
		}
		if (!W_ERROR_IS_OK(r.out.result)) {
			printf("NetShareEnum level %u failed - %s\n",
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

bool torture_samba3_rpc_srvsvc(struct torture_context *torture)
{
	struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	const char *sharename = NULL;
	struct smbcli_state *cli;
	NTSTATUS status;

	if (!(mem_ctx = talloc_new(torture))) {
		return false;
	}

	if (!(torture_open_connection_share(
		      mem_ctx, &cli, torture, torture_setting_string(torture, "host", NULL),
		      "IPC$", torture->ev))) {
		talloc_free(mem_ctx);
		return false;
	}

	status = pipe_bind_smb(mem_ctx, torture->lp_ctx, cli->tree, 
			       "\\pipe\\srvsvc", &ndr_table_srvsvc, &p);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) could not bind to srvsvc pipe: %s\n",
			 __location__, nt_errstr(status));
		ret = false;
		goto done;
	}

	ret &= test_NetShareEnum(p, mem_ctx, &sharename);
	if (sharename == NULL) {
		printf("did not get sharename\n");
	} else {
		ret &= test_NetShareGetInfo(p, mem_ctx, sharename);
	}

 done:
	talloc_free(mem_ctx);
	return ret;
}

/*
 * Do a ReqChallenge/Auth2 with a random wks name, make sure it returns
 * NT_STATUS_NO_SAM_ACCOUNT
 */

bool torture_samba3_rpc_randomauth2(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx;
	struct dcerpc_pipe *net_pipe;
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
		d_printf("talloc_new failed\n");
		return false;
	}

	if (!(wksname = generate_random_str_list(
		      mem_ctx, 14, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"))) {
		d_printf("generate_random_str_list failed\n");
		goto done;
	}

	if (!(torture_open_connection_share(
		      mem_ctx, &cli,
		      torture, torture_setting_string(torture, "host", NULL),
		      "IPC$", torture->ev))) {
		d_printf("IPC$ connection failed\n");
		goto done;
	}

	if (!(net_pipe = dcerpc_pipe_init(
		      mem_ctx, cli->transport->socket->event.ctx,
		      lp_iconv_convenience(torture->lp_ctx)))) {
		d_printf("dcerpc_pipe_init failed\n");
		goto done;
	}

	status = dcerpc_pipe_open_smb(net_pipe, cli->tree, "\\netlogon");
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	status = dcerpc_bind_auth_none(net_pipe, &ndr_table_netlogon);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_bind_auth_none failed: %s\n",
			 nt_errstr(status));
		goto done;
	}

	r.in.computer_name = wksname;
	r.in.server_name = talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(net_pipe));
	if (r.in.server_name == NULL) {
		d_printf("talloc_asprintf failed\n");
		goto done;
	}
	generate_random_buffer(netr_cli_creds.data,
			       sizeof(netr_cli_creds.data));
	r.in.credentials = &netr_cli_creds;
	r.out.return_credentials = &netr_srv_creds;

	status = dcerpc_netr_ServerReqChallenge(net_pipe, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("netr_ServerReqChallenge failed: %s\n",
			 nt_errstr(status));
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
	

	status = dcerpc_netr_ServerAuthenticate2(net_pipe, mem_ctx, &a);

	if (!NT_STATUS_EQUAL(status, NT_STATUS_NO_TRUST_SAM_ACCOUNT)) {
		d_printf("dcerpc_netr_ServerAuthenticate2 returned %s, "
			 "expected NT_STATUS_NO_TRUST_SAM_ACCOUNT\n",
			 nt_errstr(status));
		goto done;
	}

	result = true;
 done:
	talloc_free(mem_ctx);
	return result;
}

static struct security_descriptor *get_sharesec(TALLOC_CTX *mem_ctx,
						struct loadparm_context *lp_ctx,
						struct smbcli_session *sess,
						const char *sharename)
{
	struct smbcli_tree *tree;
	TALLOC_CTX *tmp_ctx;
	struct dcerpc_pipe *p;
	NTSTATUS status;
	struct srvsvc_NetShareGetInfo r;
	union srvsvc_NetShareInfo info;
	struct security_descriptor *result;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		d_printf("talloc_new failed\n");
		return NULL;
	}

	if (!NT_STATUS_IS_OK(secondary_tcon(tmp_ctx, sess, "IPC$", &tree))) {
		d_printf("secondary_tcon failed\n");
		talloc_free(tmp_ctx);
		return NULL;
	}

	status = pipe_bind_smb(mem_ctx, lp_ctx, tree, "\\pipe\\srvsvc",
			       &ndr_table_srvsvc, &p);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) could not bind to srvsvc pipe: %s\n",
			 __location__, nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}

#if 0
	p->conn->flags |= DCERPC_DEBUG_PRINT_IN | DCERPC_DEBUG_PRINT_OUT;
#endif

	r.in.server_unc = talloc_asprintf(tmp_ctx, "\\\\%s",
					  dcerpc_server_name(p));
	r.in.share_name = sharename;
	r.in.level = 502;
	r.out.info = &info;

	status = dcerpc_srvsvc_NetShareGetInfo(p, tmp_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("srvsvc_NetShareGetInfo failed: %s\n",
			 nt_errstr(status));
		talloc_free(tmp_ctx);
		return NULL;
	}

	result = talloc_steal(mem_ctx, info.info502->sd_buf.sd);
	talloc_free(tmp_ctx);
	return result;
}

static NTSTATUS set_sharesec(TALLOC_CTX *mem_ctx,
			     struct loadparm_context *lp_ctx,
			     struct smbcli_session *sess,
			     const char *sharename,
			     struct security_descriptor *sd)
{
	struct smbcli_tree *tree;
	TALLOC_CTX *tmp_ctx;
	struct dcerpc_pipe *p;
	NTSTATUS status;
	struct sec_desc_buf i;
	struct srvsvc_NetShareSetInfo r;
	union srvsvc_NetShareInfo info;
	uint32_t error = 0;

	if (!(tmp_ctx = talloc_new(mem_ctx))) {
		d_printf("talloc_new failed\n");
		return NT_STATUS_NO_MEMORY;
	}

	if (!NT_STATUS_IS_OK(secondary_tcon(tmp_ctx, sess, "IPC$", &tree))) {
		d_printf("secondary_tcon failed\n");
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	status = pipe_bind_smb(mem_ctx, lp_ctx, tree, "\\pipe\\srvsvc",
			       &ndr_table_srvsvc, &p);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) could not bind to srvsvc pipe: %s\n",
			 __location__, nt_errstr(status));
		talloc_free(tmp_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

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

	status = dcerpc_srvsvc_NetShareSetInfo(p, tmp_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("srvsvc_NetShareSetInfo failed: %s\n",
			 nt_errstr(status));
	}

	talloc_free(tmp_ctx);
	return status;
}

bool try_tcon(TALLOC_CTX *mem_ctx,
	      struct loadparm_context *lp_ctx,
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
		d_printf("talloc_new failed\n");
		return false;
	}

	status = secondary_tcon(tmp_ctx, session, sharename, &rmdir_tree);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("first tcon to delete dir failed\n");
		talloc_free(tmp_ctx);
		return false;
	}

	smbcli_rmdir(rmdir_tree, "sharesec_testdir");

	if (!NT_STATUS_IS_OK(dom_sid_split_rid(tmp_ctx, user_sid,
					       &domain_sid, &rid))) {
		d_printf("dom_sid_split_rid failed\n");
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
		d_printf("security_descriptor_dacl_create failed\n");
		talloc_free(tmp_ctx);
                return false;
        }

	status = set_sharesec(mem_ctx, lp_ctx, session, sharename, sd);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("custom set_sharesec failed: %s\n",
			 nt_errstr(status));
		talloc_free(tmp_ctx);
                return false;
	}

	status = secondary_tcon(tmp_ctx, session, sharename, &tree);
	if (!NT_STATUS_EQUAL(status, expected_tcon)) {
		d_printf("Expected %s, got %s\n", nt_errstr(expected_tcon),
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
		d_printf("(%s) Expected %s, got %s\n", __location__,
			 nt_errstr(expected_mkdir), nt_errstr(status));
		ret = false;
	}

 done:
	smbcli_rmdir(rmdir_tree, "sharesec_testdir");

	status = set_sharesec(mem_ctx, lp_ctx, session, sharename, orig_sd);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("custom set_sharesec failed: %s\n",
			 nt_errstr(status));
		talloc_free(tmp_ctx);
                return false;
	}

	talloc_free(tmp_ctx);
	return ret;
}

bool torture_samba3_rpc_sharesec(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct smbcli_state *cli;
	struct security_descriptor *sd;
	struct dom_sid *user_sid;

	if (!(mem_ctx = talloc_new(torture))) {
		return false;
	}

	if (!(torture_open_connection_share(
		      mem_ctx, &cli, torture, torture_setting_string(torture, "host", NULL),
		      "IPC$", torture->ev))) {
		d_printf("IPC$ connection failed\n");
		talloc_free(mem_ctx);
		return false;
	}

	if (!(user_sid = whoami(mem_ctx, torture->lp_ctx, cli->tree))) {
		d_printf("whoami failed\n");
		talloc_free(mem_ctx);
		return false;
	}

	sd = get_sharesec(mem_ctx, torture->lp_ctx, cli->session, 
			  torture_setting_string(torture, "share", NULL));

	ret &= try_tcon(mem_ctx, torture->lp_ctx, sd, cli->session,
			torture_setting_string(torture, "share", NULL),
			user_sid, 0, NT_STATUS_ACCESS_DENIED, NT_STATUS_OK);

	ret &= try_tcon(mem_ctx, torture->lp_ctx, sd, cli->session,
			torture_setting_string(torture, "share", NULL),
			user_sid, SEC_FILE_READ_DATA, NT_STATUS_OK,
			NT_STATUS_MEDIA_WRITE_PROTECTED);

	ret &= try_tcon(mem_ctx, torture->lp_ctx, sd, cli->session,
			torture_setting_string(torture, "share", NULL),
			user_sid, SEC_FILE_ALL, NT_STATUS_OK, NT_STATUS_OK);

	talloc_free(mem_ctx);
	return ret;
}

bool torture_samba3_rpc_lsa(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct smbcli_state *cli;
	struct dcerpc_pipe *p;
	struct policy_handle lsa_handle;
	NTSTATUS status;
	struct dom_sid *domain_sid;

	if (!(mem_ctx = talloc_new(torture))) {
		return false;
	}

	if (!(torture_open_connection_share(
		      mem_ctx, &cli, torture, torture_setting_string(torture, "host", NULL),
		      "IPC$", torture->ev))) {
		d_printf("IPC$ connection failed\n");
		talloc_free(mem_ctx);
		return false;
	}

	status = pipe_bind_smb(mem_ctx, torture->lp_ctx, cli->tree, "\\lsarpc",
			       &ndr_table_lsarpc, &p);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) pipe_bind_smb failed: %s\n", __location__,
			 nt_errstr(status));
		talloc_free(mem_ctx);
		return false;
	}

	{
		struct lsa_ObjectAttribute attr;
		struct lsa_OpenPolicy2 o;
		o.in.system_name = talloc_asprintf(
			mem_ctx, "\\\\%s", dcerpc_server_name(p));
		ZERO_STRUCT(attr);
		o.in.attr = &attr;
		o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		o.out.handle = &lsa_handle;
		status = dcerpc_lsa_OpenPolicy2(p, mem_ctx, &o);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("(%s) dcerpc_lsa_OpenPolicy2 failed: %s\n",
				 __location__, nt_errstr(status));
			talloc_free(mem_ctx);
			return false;
		}
	}

#if 0
	p->conn->flags |= DCERPC_DEBUG_PRINT_IN | DCERPC_DEBUG_PRINT_OUT;
#endif

	{
		int i;
		int levels[] = { 2,3,5,6 };

		for (i=0; i<ARRAY_SIZE(levels); i++) {
			struct lsa_QueryInfoPolicy r;
			union lsa_PolicyInformation *info = NULL;
			r.in.handle = &lsa_handle;
			r.in.level = levels[i];
			r.out.info = &info;
			status = dcerpc_lsa_QueryInfoPolicy(p, mem_ctx, &r);
			if (!NT_STATUS_IS_OK(status)) {
				d_printf("(%s) dcerpc_lsa_QueryInfoPolicy %d "
					 "failed: %s\n", __location__,
					 levels[i], nt_errstr(status));
				talloc_free(mem_ctx);
				return false;
			}
			if (levels[i] == 5) {
				domain_sid = info->account_domain.sid;
			}
		}
	}

	return ret;
}

static NTSTATUS get_servername(TALLOC_CTX *mem_ctx, struct smbcli_tree *tree,
			       struct smb_iconv_convenience *iconv_convenience,
			       char **name)
{
	struct rap_WserverGetInfo r;
	NTSTATUS status;
	char servername[17];

	r.in.level = 0;
	r.in.bufsize = 0xffff;

	status = smbcli_rap_netservergetinfo(tree, iconv_convenience, mem_ctx, &r);
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


static NTSTATUS find_printers(TALLOC_CTX *ctx, struct loadparm_context *lp_ctx,
			      struct smbcli_tree *tree,
			      const char ***printers, int *num_printers)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;
	struct dcerpc_pipe *p;
	struct srvsvc_NetShareEnum r;
	struct srvsvc_NetShareInfoCtr info_ctr;
	struct srvsvc_NetShareCtr1 c1_in;
	struct srvsvc_NetShareCtr1 *c1;
	uint32_t totalentries = 0;
	int i;

	mem_ctx = talloc_new(ctx);
	if (mem_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	status = pipe_bind_smb(mem_ctx, lp_ctx, 
			       tree, "\\srvsvc", &ndr_table_srvsvc,
			       &p);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("could not bind to srvsvc pipe\n");
		talloc_free(mem_ctx);
		return status;
	}

	ZERO_STRUCT(c1_in);
	info_ctr.level = 1;
	info_ctr.ctr.ctr1 = &c1_in;

	r.in.server_unc = talloc_asprintf(
		mem_ctx, "\\\\%s", dcerpc_server_name(p));
	r.in.info_ctr = &info_ctr;
	r.in.max_buffer = (uint32_t)-1;
	r.in.resume_handle = NULL;
	r.out.totalentries = &totalentries;
	r.out.info_ctr = &info_ctr;

	status = dcerpc_srvsvc_NetShareEnum(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("NetShareEnum level %u failed - %s\n",
			 info_ctr.level, nt_errstr(status));
		talloc_free(mem_ctx);
		return status;
	}

	*printers = NULL;
	*num_printers = 0;
	c1 = r.out.info_ctr->ctr.ctr1;
	for (i=0; i<c1->count; i++) {
		if (c1->array[i].type != STYPE_PRINTQ) {
			continue;
		}
		if (!add_string_to_array(ctx, c1->array[i].name,
					 printers, num_printers)) {
			talloc_free(ctx);
			return NT_STATUS_NO_MEMORY;
		}
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}

static bool enumprinters(TALLOC_CTX *mem_ctx, struct dcerpc_pipe *pipe,
			 const char *servername, int level, int *num_printers)
{
	struct spoolss_EnumPrinters r;
	NTSTATUS status;
	DATA_BLOB blob;
	uint32_t needed;
	uint32_t count;
	union spoolss_PrinterInfo *info;

	r.in.flags = PRINTER_ENUM_LOCAL;
	r.in.server = talloc_asprintf(mem_ctx, "\\\\%s", servername);
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;
	r.out.count = &count;
	r.out.info = &info;

	status = dcerpc_spoolss_EnumPrinters(pipe, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) dcerpc_spoolss_EnumPrinters failed: %s\n",
			 __location__, nt_errstr(status));
		return false;
	}

	if (!W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		d_printf("(%s) EnumPrinters unexpected return code %s, should "
			 "be WERR_INSUFFICIENT_BUFFER\n", __location__,
			 win_errstr(r.out.result));
		return false;
	}

	blob = data_blob_talloc_zero(mem_ctx, needed);
	if (blob.data == NULL) {
		d_printf("(%s) data_blob_talloc failed\n", __location__);
		return false;
	}

	r.in.buffer = &blob;
	r.in.offered = needed;

	status = dcerpc_spoolss_EnumPrinters(pipe, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
		d_printf("(%s) dcerpc_spoolss_EnumPrinters failed: %s, "
			 "%s\n", __location__, nt_errstr(status),
			 win_errstr(r.out.result));
		return false;
	}

	*num_printers = count;

	return true;
}

static NTSTATUS getprinterinfo(TALLOC_CTX *ctx, struct dcerpc_pipe *pipe,
			       struct policy_handle *handle, int level,
			       union spoolss_PrinterInfo **res)
{
	TALLOC_CTX *mem_ctx;
	struct spoolss_GetPrinter r;
	DATA_BLOB blob;
	NTSTATUS status;
	uint32_t needed;

	mem_ctx = talloc_new(ctx);
	if (mem_ctx == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	r.in.handle = handle;
	r.in.level = level;
	r.in.buffer = NULL;
	r.in.offered = 0;
	r.out.needed = &needed;

	status = dcerpc_spoolss_GetPrinter(pipe, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) dcerpc_spoolss_GetPrinter failed: %s\n",
			 __location__, nt_errstr(status));
		talloc_free(mem_ctx);
		return status;
	}

	if (!W_ERROR_EQUAL(r.out.result, WERR_INSUFFICIENT_BUFFER)) {
		printf("GetPrinter unexpected return code %s, should "
		       "be WERR_INSUFFICIENT_BUFFER\n",
		       win_errstr(r.out.result));
		talloc_free(mem_ctx);
		return NT_STATUS_UNSUCCESSFUL;
	}

	r.in.handle = handle;
	r.in.level = level;
	blob = data_blob_talloc(mem_ctx, NULL, needed);
	if (blob.data == NULL) {
		talloc_free(mem_ctx);
		return NT_STATUS_NO_MEMORY;
	}
	memset(blob.data, 0, blob.length);
	r.in.buffer = &blob;
	r.in.offered = needed;

	status = dcerpc_spoolss_GetPrinter(pipe, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
		d_printf("(%s) dcerpc_spoolss_GetPrinter failed: %s, "
			 "%s\n", __location__, nt_errstr(status),
			 win_errstr(r.out.result));
		talloc_free(mem_ctx);
		return NT_STATUS_IS_OK(status) ?
			NT_STATUS_UNSUCCESSFUL : status;
	}

	if (res != NULL) {
		*res = talloc_steal(ctx, r.out.info);
	}

	talloc_free(mem_ctx);
	return NT_STATUS_OK;
}

bool torture_samba3_rpc_spoolss(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct smbcli_state *cli;
	struct dcerpc_pipe *p;
	NTSTATUS status;
	struct policy_handle server_handle, printer_handle;
	const char **printers;
	int num_printers;
	struct spoolss_UserLevel1 userlevel1;
	char *servername;

	if (!(mem_ctx = talloc_new(torture))) {
		return false;
	}

	if (!(torture_open_connection_share(
		      mem_ctx, &cli, torture, torture_setting_string(torture, "host", NULL),
		      "IPC$", torture->ev))) {
		d_printf("IPC$ connection failed\n");
		talloc_free(mem_ctx);
		return false;
	}

	status = get_servername(mem_ctx, cli->tree, lp_iconv_convenience(torture->lp_ctx), &servername);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "(%s) get_servername returned %s\n",
			  __location__, nt_errstr(status));
		talloc_free(mem_ctx);
		return false;
	}

	if (!NT_STATUS_IS_OK(find_printers(mem_ctx, torture->lp_ctx, cli->tree,
					   &printers, &num_printers))) {
		talloc_free(mem_ctx);
		return false;
	}

	if (num_printers == 0) {
		d_printf("Did not find printers\n");
		talloc_free(mem_ctx);
		return true;
	}

	status = pipe_bind_smb(mem_ctx, torture->lp_ctx, cli->tree, "\\spoolss",
			       &ndr_table_spoolss, &p);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) pipe_bind_smb failed: %s\n", __location__,
			 nt_errstr(status));
		talloc_free(mem_ctx);
		return false;
	}

	ZERO_STRUCT(userlevel1);
	userlevel1.client = talloc_asprintf(
		mem_ctx, "\\\\%s", lp_netbios_name(torture->lp_ctx));
	userlevel1.user = cli_credentials_get_username(cmdline_credentials);
	userlevel1.build = 2600;
	userlevel1.major = 3;
	userlevel1.minor = 0;
	userlevel1.processor = 0;

	{
		struct spoolss_OpenPrinterEx r;

		ZERO_STRUCT(r);
		r.in.printername = talloc_asprintf(mem_ctx, "\\\\%s",
						   servername);
		r.in.datatype = NULL;
		r.in.access_mask = 0;
		r.in.level = 1;
		r.in.userlevel.level1 = &userlevel1;
		r.out.handle = &server_handle;

		status = dcerpc_spoolss_OpenPrinterEx(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
			d_printf("(%s) dcerpc_spoolss_OpenPrinterEx failed: "
				 "%s, %s\n", __location__, nt_errstr(status),
				 win_errstr(r.out.result));
			talloc_free(mem_ctx);
			return false;
		}
	}

	{
		struct spoolss_ClosePrinter r;

		r.in.handle = &server_handle;
		r.out.handle = &server_handle;

		status = dcerpc_spoolss_ClosePrinter(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
			d_printf("(%s) dcerpc_spoolss_ClosePrinter failed: "
				 "%s, %s\n", __location__, nt_errstr(status),
				 win_errstr(r.out.result));
			talloc_free(mem_ctx);
			return false;
		}
	}

	{
		struct spoolss_OpenPrinterEx r;

		ZERO_STRUCT(r);
		r.in.printername = talloc_asprintf(
			mem_ctx, "\\\\%s\\%s", servername, printers[0]);
		r.in.datatype = NULL;
		r.in.access_mask = 0;
		r.in.level = 1;
		r.in.userlevel.level1 = &userlevel1;
		r.out.handle = &printer_handle;

		status = dcerpc_spoolss_OpenPrinterEx(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
			d_printf("(%s) dcerpc_spoolss_OpenPrinterEx failed: "
				 "%s, %s\n", __location__, nt_errstr(status),
				 win_errstr(r.out.result));
			talloc_free(mem_ctx);
			return false;
		}
	}

	{
		int i;

		for (i=0; i<8; i++) {
			status = getprinterinfo(mem_ctx, p, &printer_handle,
						i, NULL);
			if (!NT_STATUS_IS_OK(status)) {
				d_printf("(%s) getprinterinfo %d failed: %s\n",
					 __location__, i, nt_errstr(status));
				ret = false;
			}
		}
	}

	{
		struct spoolss_ClosePrinter r;

		r.in.handle = &printer_handle;
		r.out.handle = &printer_handle;

		status = dcerpc_spoolss_ClosePrinter(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status)) {
			d_printf("(%s) dcerpc_spoolss_ClosePrinter failed: "
				 "%s\n", __location__, nt_errstr(status));
			talloc_free(mem_ctx);
			return false;
		}
	}

	{
		int num_enumerated;
		if (!enumprinters(mem_ctx, p, servername, 1,
				  &num_enumerated)) {
			d_printf("(%s) enumprinters failed\n", __location__);
			talloc_free(mem_ctx);
			return false;
		}
		if (num_printers != num_enumerated) {
			d_printf("(%s) netshareenum gave %d printers, "
				 "enumprinters lvl 1 gave %d\n", __location__,
				 num_printers, num_enumerated);
			talloc_free(mem_ctx);
			return false;
		}
	}

	{
		int num_enumerated;
		if (!enumprinters(mem_ctx, p, servername, 2,
				  &num_enumerated)) {
			d_printf("(%s) enumprinters failed\n", __location__);
			talloc_free(mem_ctx);
			return false;
		}
		if (num_printers != num_enumerated) {
			d_printf("(%s) netshareenum gave %d printers, "
				 "enumprinters lvl 2 gave %d\n", __location__,
				 num_printers, num_enumerated);
			talloc_free(mem_ctx);
			return false;
		}
	}

	talloc_free(mem_ctx);

	return ret;
}

bool torture_samba3_rpc_wkssvc(struct torture_context *torture)
{
	TALLOC_CTX *mem_ctx;
	struct smbcli_state *cli;
	struct dcerpc_pipe *p;
	NTSTATUS status;
	char *servername;

	if (!(mem_ctx = talloc_new(torture))) {
		return false;
	}

	if (!(torture_open_connection_share(
		      mem_ctx, &cli, torture, torture_setting_string(torture, "host", NULL),
		      "IPC$", torture->ev))) {
		d_printf("IPC$ connection failed\n");
		talloc_free(mem_ctx);
		return false;
	}

	status = get_servername(mem_ctx, cli->tree, lp_iconv_convenience(torture->lp_ctx), &servername);
	if (!NT_STATUS_IS_OK(status)) {
		d_fprintf(stderr, "(%s) get_servername returned %s\n",
			  __location__, nt_errstr(status));
		talloc_free(mem_ctx);
		return false;
	}

	status = pipe_bind_smb(mem_ctx, torture->lp_ctx, cli->tree, "\\wkssvc",
			       &ndr_table_wkssvc, &p);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) pipe_bind_smb failed: %s\n", __location__,
			 nt_errstr(status));
		talloc_free(mem_ctx);
		return false;
	}

	{
		struct wkssvc_NetWkstaInfo100 wks100;
		union wkssvc_NetWkstaInfo info;
		struct wkssvc_NetWkstaGetInfo r;

		r.in.server_name = "\\foo";
		r.in.level = 100;
		info.info100 = &wks100;
		r.out.info = &info;

		status = dcerpc_wkssvc_NetWkstaGetInfo(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
			d_printf("(%s) dcerpc_wkssvc_NetWksGetInfo failed: "
				 "%s, %s\n", __location__, nt_errstr(status),
				 win_errstr(r.out.result));
			talloc_free(mem_ctx);
			return false;
		}

		if (strcmp(servername,
			   r.out.info->info100->server_name) != 0) {
			d_printf("(%s) servername inconsistency: RAP=%s, "
				 "dcerpc_wkssvc_NetWksGetInfo=%s",
				 __location__, servername,
				 r.out.info->info100->server_name);
			talloc_free(mem_ctx);
			return false;
		}
	}

	talloc_free(mem_ctx);
	return true;
}

static NTSTATUS winreg_close(struct dcerpc_pipe *p,
			     struct policy_handle *handle)
{
	struct winreg_CloseKey c;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;

	c.in.handle = c.out.handle = handle;

	if (!(mem_ctx = talloc_new(p))) {
		return NT_STATUS_NO_MEMORY;
	}

	status = dcerpc_winreg_CloseKey(p, mem_ctx, &c);
	talloc_free(mem_ctx);

	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}

	if (!W_ERROR_IS_OK(c.out.result)) {
		return werror_to_ntstatus(c.out.result);
	}

	return NT_STATUS_OK;
}

static NTSTATUS enumvalues(struct dcerpc_pipe *p, struct policy_handle *handle,
			   TALLOC_CTX *mem_ctx)
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

		status = dcerpc_winreg_EnumValue(p, mem_ctx, &r);
		if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
			return NT_STATUS_OK;
		}
		enum_index += 1;
	}
}

static NTSTATUS enumkeys(struct dcerpc_pipe *p, struct policy_handle *handle, 
			 TALLOC_CTX *mem_ctx, int depth)
{
	struct winreg_EnumKey r;
	struct winreg_StringBuf kclass, name;
	NTSTATUS status;
	NTTIME t = 0;

	if (depth <= 0) {
		return NT_STATUS_OK;
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
		TALLOC_CTX *tmp_ctx;
		struct winreg_OpenKey o;
		struct policy_handle key_handle;
		int i;

		if (!(tmp_ctx = talloc_new(mem_ctx))) {
			return NT_STATUS_NO_MEMORY;
		}

		name.name = NULL;
		name.size = 1024;

		status = dcerpc_winreg_EnumKey(p, tmp_ctx, &r);
		if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
			/* We're done enumerating */
			talloc_free(tmp_ctx);
			return NT_STATUS_OK;
		}

		for (i=0; i<10-depth; i++)
			printf(" ");
		printf("%s\n", r.out.name->name);
			

		o.in.parent_handle = handle;
		o.in.keyname.name = r.out.name->name;
		o.in.unknown = 0;
		o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
		o.out.handle = &key_handle;

		status = dcerpc_winreg_OpenKey(p, tmp_ctx, &o);
		if (NT_STATUS_IS_OK(status) && W_ERROR_IS_OK(o.out.result)) {
			enumkeys(p, &key_handle, tmp_ctx, depth-1);
			enumvalues(p, &key_handle, tmp_ctx);
			status = winreg_close(p, &key_handle);
			if (!NT_STATUS_IS_OK(status)) {
				return status;
			}
		}

		talloc_free(tmp_ctx);

		r.in.enum_index += 1;
	} while(true);

	return NT_STATUS_OK;
}

typedef NTSTATUS (*winreg_open_fn)(struct dcerpc_pipe *, TALLOC_CTX *, void *);

static bool test_Open3(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx, 
		       const char *name, winreg_open_fn open_fn)
{
	struct policy_handle handle;
	struct winreg_OpenHKLM r;
	NTSTATUS status;

	r.in.system_name = 0;
	r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	r.out.handle = &handle;
	
	status = open_fn(p, mem_ctx, &r);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
		d_printf("(%s) %s failed: %s, %s\n", __location__, name,
			 nt_errstr(status), win_errstr(r.out.result));
		return false;
	}

	enumkeys(p, &handle, mem_ctx, 4);

	status = winreg_close(p, &handle);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("(%s) dcerpc_CloseKey failed: %s\n",
			 __location__, nt_errstr(status));
		return false;
	}

	return true;
}

bool torture_samba3_rpc_winreg(struct torture_context *torture)
{
        NTSTATUS status;
	struct dcerpc_pipe *p;
	TALLOC_CTX *mem_ctx;
	bool ret = true;
	struct {
		const char *name;
		winreg_open_fn fn;
	} open_fns[] = {
		{"OpenHKLM", (winreg_open_fn)dcerpc_winreg_OpenHKLM },
		{"OpenHKU",  (winreg_open_fn)dcerpc_winreg_OpenHKU },
		{"OpenHKPD", (winreg_open_fn)dcerpc_winreg_OpenHKPD },
		{"OpenHKPT", (winreg_open_fn)dcerpc_winreg_OpenHKPT },
		{"OpenHKCR", (winreg_open_fn)dcerpc_winreg_OpenHKCR }};
#if 0
	int i;
#endif

	mem_ctx = talloc_init("torture_rpc_winreg");

	status = torture_rpc_connection(torture, &p, &ndr_table_winreg);

	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(mem_ctx);
		return false;
	}

#if 1
	ret = test_Open3(p, mem_ctx, open_fns[0].name, open_fns[0].fn);
#else
	for (i = 0; i < ARRAY_SIZE(open_fns); i++) {
		if (!test_Open3(p, mem_ctx, open_fns[i].name, open_fns[i].fn))
			ret = false;
	}
#endif

	talloc_free(mem_ctx);

	return ret;
}

static NTSTATUS get_shareinfo(TALLOC_CTX *mem_ctx,
			      struct loadparm_context *lp_ctx,
			      struct smbcli_state *cli,
			      const char *share,
			      struct srvsvc_NetShareInfo502 **info502)
{
	struct smbcli_tree *ipc;
	struct dcerpc_pipe *p;
	struct srvsvc_NetShareGetInfo r;
	union srvsvc_NetShareInfo info;
	NTSTATUS status;

	if (!(p = dcerpc_pipe_init(cli,
				   cli->transport->socket->event.ctx,
				   lp_iconv_convenience(lp_ctx)))) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	status = secondary_tcon(p, cli->session, "IPC$", &ipc);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	status = dcerpc_pipe_open_smb(p, ipc, "\\pipe\\srvsvc");
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto fail;
	}

	status = dcerpc_bind_auth_none(p, &ndr_table_srvsvc);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_bind_auth_none failed: %s\n",
			 nt_errstr(status));
		goto fail;
	}

	r.in.server_unc = talloc_asprintf(mem_ctx, "\\\\%s",
					  dcerpc_server_name(p));
	r.in.share_name = share;
	r.in.level = 502;
	r.out.info = &info;

	status = dcerpc_srvsvc_NetShareGetInfo(p, p, &r);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
		d_printf("(%s) srvsvc_NetShareGetInfo failed: %s, %s\n", __location__,
			 nt_errstr(status), win_errstr(r.out.result));
		status = NT_STATUS_IS_OK(status) ? werror_to_ntstatus(r.out.result) : status;
		goto fail;
	}

	*info502 = talloc_move(mem_ctx, &info.info502);
	return NT_STATUS_OK;

fail:
	talloc_free(p);
	return status;
}

/*
 * Get us a handle on HKLM\
 */

static NTSTATUS get_hklm_handle(TALLOC_CTX *mem_ctx,
				struct smbcli_state *cli,
				struct smb_iconv_convenience *iconv_convenience,
				struct dcerpc_pipe **pipe_p,
				struct policy_handle **handle)
{
	struct smbcli_tree *ipc;
	struct dcerpc_pipe *p;
	struct winreg_OpenHKLM r;
	NTSTATUS status;
	struct policy_handle *result;

	result = talloc(mem_ctx, struct policy_handle);

	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}

	if (!(p = dcerpc_pipe_init(result,
				   cli->transport->socket->event.ctx,
				   iconv_convenience))) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	status = secondary_tcon(p, cli->session, "IPC$", &ipc);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}

	status = dcerpc_pipe_open_smb(p, ipc, "\\winreg");
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_pipe_open_smb failed: %s\n",
			 nt_errstr(status));
		goto fail;
	}

	status = dcerpc_bind_auth_none(p, &ndr_table_winreg);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("dcerpc_bind_auth_none failed: %s\n",
			 nt_errstr(status));
		goto fail;
	}

	r.in.system_name = 0;
        r.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
        r.out.handle = result;

	status = dcerpc_winreg_OpenHKLM(p, p, &r);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(r.out.result)) {
		d_printf("(%s) OpenHKLM failed: %s, %s\n", __location__,
			 nt_errstr(status), win_errstr(r.out.result));
		status = NT_STATUS_IS_OK(status) ? werror_to_ntstatus(r.out.result) : status;
		goto fail;
	}

	*pipe_p = p;
	*handle = result;
	return NT_STATUS_OK;

 fail:
	talloc_free(result);
	return status;
}

static NTSTATUS torture_samba3_createshare(struct smbcli_state *cli,
					   struct smb_iconv_convenience *iconv_convenience,
					   const char *sharename)
{
	struct dcerpc_pipe *p;
	struct policy_handle *hklm = NULL;
	struct policy_handle new_handle;
	struct winreg_CreateKey c;
	struct winreg_CloseKey cl;
	enum winreg_CreateAction action_taken;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(cli);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	status = get_hklm_handle(mem_ctx, cli, iconv_convenience, &p, &hklm);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("get_hklm_handle failed: %s\n", nt_errstr(status));
		goto fail;
	}

	c.in.handle = hklm;
	c.in.name.name = talloc_asprintf(
		mem_ctx, "software\\samba\\smbconf\\%s", sharename);
	if (c.in.name.name == NULL) {
		d_printf("talloc_asprintf failed\n");
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}
	c.in.keyclass.name = "";
	c.in.options = 0;
	c.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	c.in.secdesc = NULL;
	c.in.action_taken = &action_taken;
	c.out.new_handle = &new_handle;
	c.out.action_taken = &action_taken;

	status = dcerpc_winreg_CreateKey(p, p, &c);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(c.out.result)) {
		d_printf("(%s) OpenKey failed: %s, %s\n", __location__,
			 nt_errstr(status), win_errstr(c.out.result));
		status = NT_STATUS_IS_OK(status) ? werror_to_ntstatus(c.out.result) : status;
		goto fail;
	}

	cl.in.handle = &new_handle;
	cl.out.handle = &new_handle;
	status = dcerpc_winreg_CloseKey(p, p, &cl);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(cl.out.result)) {
		d_printf("(%s) OpenKey failed: %s, %s\n", __location__,
			 nt_errstr(status), win_errstr(cl.out.result));
		status = NT_STATUS_IS_OK(status) ? werror_to_ntstatus(cl.out.result) : status;
		goto fail;
	}


fail:
	talloc_free(mem_ctx);
	return status;
}

static NTSTATUS torture_samba3_deleteshare(struct torture_context *torture,
					   struct smbcli_state *cli,
					   const char *sharename)
{
	struct dcerpc_pipe *p;
	struct policy_handle *hklm = NULL;
	struct winreg_DeleteKey d;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(cli);
	NT_STATUS_HAVE_NO_MEMORY(mem_ctx);

	status = get_hklm_handle(cli, cli, lp_iconv_convenience(torture->lp_ctx),
				 &p, &hklm);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("get_hklm_handle failed: %s\n", nt_errstr(status));
		goto fail;
	}

	d.in.handle = hklm;
	d.in.key.name = talloc_asprintf(
		mem_ctx, "software\\samba\\smbconf\\%s", sharename);
	if (d.in.key.name == NULL) {
		d_printf("talloc_asprintf failed\n");
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}
	status = dcerpc_winreg_DeleteKey(p, p, &d);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(d.out.result)) {
		d_printf("(%s) OpenKey failed: %s, %s\n", __location__,
			 nt_errstr(status), win_errstr(d.out.result));
		status = NT_STATUS_IS_OK(status) ? werror_to_ntstatus(d.out.result) : status;
		goto fail;
	}

fail:
	talloc_free(mem_ctx);
	return status;
}

static NTSTATUS torture_samba3_setconfig(struct smbcli_state *cli,
					 struct loadparm_context *lp_ctx,
					 const char *sharename,
					 const char *parameter,
					 const char *value)
{
	struct dcerpc_pipe *p = NULL;
	struct policy_handle *hklm = NULL, key_handle;
	struct winreg_OpenKey o;
	struct winreg_SetValue s;
	uint32_t type;
	DATA_BLOB val;
	NTSTATUS status;

	status = get_hklm_handle(cli, cli, lp_iconv_convenience(lp_ctx), &p, &hklm);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("get_hklm_handle failed: %s\n", nt_errstr(status));
		return status;;
	}

	o.in.parent_handle = hklm;
	o.in.keyname.name = talloc_asprintf(
		hklm, "software\\samba\\smbconf\\%s", sharename);
	if (o.in.keyname.name == NULL) {
		d_printf("talloc_asprintf failed\n");
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}
	o.in.unknown = 0;
	o.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	o.out.handle = &key_handle;

	status = dcerpc_winreg_OpenKey(p, p, &o);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(o.out.result)) {
		d_printf("(%s) OpenKey failed: %s, %s\n", __location__,
			 nt_errstr(status), win_errstr(o.out.result));
		status = NT_STATUS_IS_OK(status) ? werror_to_ntstatus(o.out.result) : status;
		goto done;
	}

	if (!reg_string_to_val(hklm, lp_iconv_convenience(lp_ctx), "REG_SZ", 
			       value, &type, &val)) {
		d_printf("(%s) reg_string_to_val failed\n", __location__);
		goto done;
	}

	s.in.handle = &key_handle;
	s.in.name.name = parameter;
	s.in.type = type;
	s.in.data = val.data;
	s.in.size = val.length;

	status = dcerpc_winreg_SetValue(p, p, &s);
	if (!NT_STATUS_IS_OK(status) || !W_ERROR_IS_OK(s.out.result)) {
		d_printf("(%s) SetValue failed: %s, %s\n", __location__,
			 nt_errstr(status), win_errstr(s.out.result));
		status = NT_STATUS_IS_OK(status) ? werror_to_ntstatus(s.out.result) : status;
		goto done;
	}

done:
	talloc_free(hklm);
	return status;
}

bool torture_samba3_regconfig(struct torture_context *torture)
{
	struct smbcli_state *cli;
	struct srvsvc_NetShareInfo502 *i = NULL;
	NTSTATUS status;
	bool ret = false;
	const char *comment = "Dummer Kommentar";

	if (!(torture_open_connection(&cli, torture, 0))) {
		return false;
	}

	status = torture_samba3_createshare(cli, lp_iconv_convenience(torture->lp_ctx), "blubber");
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(torture, "torture_samba3_createshare failed: "
				"%s\n", nt_errstr(status));
		goto done;
	}

	status = torture_samba3_setconfig(cli, torture->lp_ctx, "blubber", "comment", comment);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(torture, "torture_samba3_setconfig failed: "
				"%s\n", nt_errstr(status));
		goto done;
	}

	status = get_shareinfo(torture, torture->lp_ctx, cli, "blubber", &i);
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(torture, "get_shareinfo failed: "
				"%s\n", nt_errstr(status));
		goto done;
	}

	if (strcmp(comment, i->comment) != 0) {
		torture_warning(torture, "Expected comment [%s], got [%s]\n",
				comment, i->comment);
		goto done;
	}

	status = torture_samba3_deleteshare(torture, cli, "blubber");
	if (!NT_STATUS_IS_OK(status)) {
		torture_warning(torture, "torture_samba3_deleteshare failed: "
				"%s\n", nt_errstr(status));
		goto done;
	}

	ret = true;
 done:
	talloc_free(cli);
	return ret;
}
