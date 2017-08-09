/*
   Unix SMB/CIFS implementation.
   Authenticate against a netlogon pipe listening on a unix domain socket
   Copyright (C) Volker Lendecke 2008

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
#include "../libcli/auth/libcli_auth.h"
#include "../librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_schannel.h"
#include "rpc_client/cli_pipe.h"
#include "rpc_client/cli_netlogon.h"
#include "secrets.h"
#include "tldap.h"
#include "tldap_util.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

static bool secrets_store_local_schannel_creds(
	const struct netlogon_creds_CredentialState *creds)
{
	DATA_BLOB blob;
	enum ndr_err_code ndr_err;
	bool ret;

	ndr_err = ndr_push_struct_blob(
		&blob, talloc_tos(), creds,
		(ndr_push_flags_fn_t)ndr_push_netlogon_creds_CredentialState);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(10, ("ndr_push_netlogon_creds_CredentialState failed: "
			   "%s\n", ndr_errstr(ndr_err)));
		return false;
	}
	ret = secrets_store(SECRETS_LOCAL_SCHANNEL_KEY,
			    blob.data, blob.length);
	data_blob_free(&blob);
	return ret;
}

static struct netlogon_creds_CredentialState *
secrets_fetch_local_schannel_creds(TALLOC_CTX *mem_ctx)
{
	struct netlogon_creds_CredentialState *creds;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;

	blob.data = (uint8_t *)secrets_fetch(SECRETS_LOCAL_SCHANNEL_KEY,
					     &blob.length);
	if (blob.data == NULL) {
		DEBUG(10, ("secrets_fetch failed\n"));
		return NULL;
	}

	creds = talloc(mem_ctx, struct netlogon_creds_CredentialState);
	if (creds == NULL) {
		DEBUG(10, ("talloc failed\n"));
		SAFE_FREE(blob.data);
		return NULL;
	}
	ndr_err = ndr_pull_struct_blob(
		&blob, creds, creds,
		(ndr_pull_flags_fn_t)ndr_pull_netlogon_creds_CredentialState);
	SAFE_FREE(blob.data);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(10, ("ndr_pull_netlogon_creds_CredentialState failed: "
			   "%s\n", ndr_errstr(ndr_err)));
		TALLOC_FREE(creds);
		return NULL;
	}

	return creds;
}

static NTSTATUS netlogond_validate(TALLOC_CTX *mem_ctx,
				   const struct auth_context *auth_context,
				   const char *ncalrpc_sockname,
				   struct netlogon_creds_CredentialState *creds,
				   const struct auth_usersupplied_info *user_info,
				   struct netr_SamInfo3 **pinfo3,
				   NTSTATUS *schannel_bind_result)
{
	struct rpc_pipe_client *p = NULL;
	struct pipe_auth_data *auth = NULL;
	struct netr_SamInfo3 *info3 = NULL;
	NTSTATUS status;

	*schannel_bind_result = NT_STATUS_OK;

	status = rpc_pipe_open_ncalrpc(talloc_tos(), ncalrpc_sockname,
				       &ndr_table_netlogon.syntax_id, &p);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("rpc_pipe_open_ncalrpc failed: %s\n",
			   nt_errstr(status)));
		return status;
	}

	p->dc = creds;

	status = rpccli_schannel_bind_data(p, lp_workgroup(),
					   DCERPC_AUTH_LEVEL_PRIVACY,
					   p->dc, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("rpccli_schannel_bind_data failed: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(p);
		return status;
	}

	status = rpc_pipe_bind(p, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("rpc_pipe_bind failed: %s\n", nt_errstr(status)));
		TALLOC_FREE(p);
		*schannel_bind_result = status;
		return status;
	}

	status = rpccli_netlogon_sam_network_logon_ex(
		p, p,
		user_info->logon_parameters,           /* flags such as 'allow
					                * workstation logon' */
		global_myname(),                       /* server name */
		user_info->client.account_name,        /* user name logging on. */
		user_info->client.domain_name,         /* domain name */
		user_info->workstation_name,           /* workstation name */
		(uchar *)auth_context->challenge.data, /* 8 byte challenge. */
		3,				       /* validation level */
		user_info->password.response.lanman,   /* lanman 24 byte response */
		user_info->password.response.nt,       /* nt 24 byte response */
		&info3);                               /* info3 out */

	DEBUG(10, ("rpccli_netlogon_sam_network_logon_ex returned %s\n",
		   nt_errstr(status)));

	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(p);
		return status;
	}

	*pinfo3 = talloc_move(mem_ctx, &info3);

	TALLOC_FREE(p);
	return NT_STATUS_OK;
}

static NTSTATUS get_ldapi_ctx(TALLOC_CTX *mem_ctx, struct tldap_context **pld)
{
	struct tldap_context *ld;
	struct sockaddr_un addr;
	char *sockaddr;
	int fd;
	NTSTATUS status;
	int res;

	sockaddr = talloc_asprintf(talloc_tos(), "/%s/ldap_priv/ldapi",
				   lp_private_dir());
	if (sockaddr == NULL) {
		DEBUG(10, ("talloc failed\n"));
		return NT_STATUS_NO_MEMORY;
	}

	ZERO_STRUCT(addr);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockaddr, sizeof(addr.sun_path));
	TALLOC_FREE(sockaddr);

	status = open_socket_out((struct sockaddr_storage *)(void *)&addr,
				 0, 0, &fd);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("Could not connect to %s: %s\n", addr.sun_path,
			   nt_errstr(status)));
		return status;
	}
	set_blocking(fd, false);

	ld = tldap_context_create(mem_ctx, fd);
	if (ld == NULL) {
		close(fd);
		return NT_STATUS_NO_MEMORY;
	}
	res = tldap_fetch_rootdse(ld);
	if (res != TLDAP_SUCCESS) {
		DEBUG(10, ("tldap_fetch_rootdse failed: %s\n",
			   tldap_errstr(talloc_tos(), ld, res)));
		TALLOC_FREE(ld);
		return NT_STATUS_LDAP(res);
	}
	*pld = ld;
	return NT_STATUS_OK;;
}

static NTSTATUS mymachinepw(uint8_t pwd[16])
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tldap_context *ld = NULL;
	struct tldap_message *rootdse, **msg;
	const char *attrs[1] = { "unicodePwd" };
	char *default_nc, *myname;
	int rc, num_msg;
	DATA_BLOB pwdblob;
	NTSTATUS status;

	status = get_ldapi_ctx(talloc_tos(), &ld);
	if (!NT_STATUS_IS_OK(status)) {
		goto fail;
	}
	rootdse = tldap_rootdse(ld);
	if (rootdse == NULL) {
		DEBUG(10, ("Could not get rootdse\n"));
		status = NT_STATUS_INTERNAL_ERROR;
		goto fail;
	}
	default_nc = tldap_talloc_single_attribute(
		rootdse, "defaultNamingContext", talloc_tos());
	if (default_nc == NULL) {
		DEBUG(10, ("Could not get defaultNamingContext\n"));
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}
	DEBUG(10, ("default_nc = %s\n", default_nc));

	myname = talloc_asprintf_strupper_m(talloc_tos(), "%s$",
					    global_myname());
	if (myname == NULL) {
		DEBUG(10, ("talloc failed\n"));
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	rc = tldap_search_fmt(
		ld, default_nc, TLDAP_SCOPE_SUB, attrs, ARRAY_SIZE(attrs), 0,
		talloc_tos(), &msg,
		"(&(sAMAccountName=%s)(objectClass=computer))", myname);
	if (rc != TLDAP_SUCCESS) {
		DEBUG(10, ("Could not retrieve our account: %s\n",
			   tldap_errstr(talloc_tos(), ld, rc)));
		status = NT_STATUS_LDAP(rc);
		goto fail;
	}
	num_msg = talloc_array_length(msg);
	if (num_msg != 1) {
		DEBUG(10, ("Got %d accounts, expected one\n", num_msg));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto fail;
	}
	if (!tldap_get_single_valueblob(msg[0], "unicodePwd", &pwdblob)) {
		char *dn = NULL;
		tldap_entry_dn(msg[0], &dn);
		DEBUG(10, ("No unicodePwd attribute in %s\n",
			   dn ? dn : "<unknown DN>"));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto fail;
	}
	if (pwdblob.length != 16) {
		DEBUG(10, ("Password hash hash has length %d, expected 16\n",
			   (int)pwdblob.length));
		status = NT_STATUS_INTERNAL_DB_CORRUPTION;
		goto fail;
	}
	memcpy(pwd, pwdblob.data, 16);

fail:
	TALLOC_FREE(frame);
	return status;
}

static NTSTATUS check_netlogond_security(const struct auth_context *auth_context,
					 void *my_private_data,
					 TALLOC_CTX *mem_ctx,
					 const struct auth_usersupplied_info *user_info,
					 struct auth_serversupplied_info **server_info)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct netr_SamInfo3 *info3 = NULL;
	struct rpc_pipe_client *p = NULL;
	struct pipe_auth_data *auth = NULL;
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	uint8_t machine_password[16];
	struct netlogon_creds_CredentialState *creds;
	NTSTATUS schannel_bind_result, status;
	struct named_mutex *mutex = NULL;
	const char *ncalrpcsock;

	DEBUG(10, ("Check auth for: [%s]\n", user_info->mapped.account_name));

	ncalrpcsock = lp_parm_const_string(
		GLOBAL_SECTION_SNUM, "auth_netlogond", "socket", NULL);

	if (ncalrpcsock == NULL) {
		ncalrpcsock = talloc_asprintf(talloc_tos(), "%s/%s",
					      get_dyn_NCALRPCDIR(), "DEFAULT");
	}

	if (ncalrpcsock == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	creds = secrets_fetch_local_schannel_creds(talloc_tos());
	if (creds == NULL) {
		goto new_key;
	}

	status = netlogond_validate(talloc_tos(), auth_context, ncalrpcsock,
				    creds, user_info, &info3,
				    &schannel_bind_result);

	DEBUG(10, ("netlogond_validate returned %s\n", nt_errstr(status)));

	if (NT_STATUS_IS_OK(status)) {
		goto okay;
	}

	if (NT_STATUS_IS_OK(schannel_bind_result)) {
		/*
		 * This is a real failure from the DC
		 */
		goto done;
	}

 new_key:

	mutex = grab_named_mutex(talloc_tos(), "LOCAL_SCHANNEL_KEY", 60);
	if (mutex == NULL) {
		DEBUG(10, ("Could not get mutex LOCAL_SCHANNEL_KEY\n"));
		status = NT_STATUS_ACCESS_DENIED;
		goto done;
	}

	DEBUG(10, ("schannel bind failed, setting up new key\n"));

	status = rpc_pipe_open_ncalrpc(talloc_tos(), ncalrpcsock,
				       &ndr_table_netlogon.syntax_id, &p);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("rpc_pipe_open_ncalrpc failed: %s\n",
			   nt_errstr(status)));
		goto done;
	}

	status = rpccli_anon_bind_data(p, &auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("rpccli_anon_bind_data failed: %s\n",
			   nt_errstr(status)));
		goto done;
	}

	status = rpc_pipe_bind(p, auth);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("rpc_pipe_bind failed: %s\n", nt_errstr(status)));
		goto done;
	}

	status = mymachinepw(machine_password);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("mymachinepw failed: %s\n", nt_errstr(status)));
		goto done;
	}

	DEBUG(10, ("machinepw "));
	dump_data(10, machine_password, 16);

	status = rpccli_netlogon_setup_creds(
		p, global_myname(), lp_workgroup(), global_myname(),
		global_myname(), machine_password, SEC_CHAN_BDC, &neg_flags);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("rpccli_netlogon_setup_creds failed: %s\n",
			   nt_errstr(status)));
		goto done;
	}

	secrets_store_local_schannel_creds(p->dc);

	/*
	 * Retry the authentication with the mutex held. This way nobody else
	 * can step on our toes.
	 */

	status = netlogond_validate(talloc_tos(), auth_context, ncalrpcsock,
				    p->dc, user_info, &info3,
				    &schannel_bind_result);

	TALLOC_FREE(p);

	DEBUG(10, ("netlogond_validate returned %s\n", nt_errstr(status)));

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

 okay:

	status = make_server_info_info3(mem_ctx, user_info->client.account_name,
					user_info->mapped.domain_name, server_info,
					info3);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("make_server_info_info3 failed: %s\n",
			   nt_errstr(status)));
		TALLOC_FREE(frame);
		return status;
	}

	status = NT_STATUS_OK;

 done:
	TALLOC_FREE(frame);
	return status;
}

/* module initialisation */
static NTSTATUS auth_init_netlogond(struct auth_context *auth_context,
				    const char *param,
				    auth_methods **auth_method)
{
	struct auth_methods *result;

	result = TALLOC_ZERO_P(auth_context, struct auth_methods);
	if (result == NULL) {
		return NT_STATUS_NO_MEMORY;
	}
	result->name = "netlogond";
	result->auth = check_netlogond_security;

        *auth_method = result;
	return NT_STATUS_OK;
}

NTSTATUS auth_netlogond_init(void)
{
	smb_register_auth(AUTH_INTERFACE_VERSION, "netlogond",
			  auth_init_netlogond);
	return NT_STATUS_OK;
}
