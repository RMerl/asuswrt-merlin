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
#include "../libcli/auth/libcli_auth.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

static NTSTATUS netlogond_validate(TALLOC_CTX *mem_ctx,
				   const struct auth_context *auth_context,
				   const char *ncalrpc_sockname,
				   uint8_t schannel_key[16],
				   const auth_usersupplied_info *user_info,
				   struct netr_SamInfo3 **pinfo3,
				   NTSTATUS *schannel_bind_result)
{
	struct rpc_pipe_client *p = NULL;
	struct cli_pipe_auth_data *auth = NULL;
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

	/*
	 * We have to fake a struct dcinfo, so that
	 * rpccli_netlogon_sam_network_logon_ex can decrypt the session keys.
	 */

	p->dc = netlogon_creds_client_init_session_key(p, schannel_key);
	if (p->dc == NULL) {
		DEBUG(0, ("talloc failed\n"));
		TALLOC_FREE(p);
		return NT_STATUS_NO_MEMORY;
	}

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
		user_info->logon_parameters,/* flags such as 'allow
					     * workstation logon' */
		global_myname(),            /* server name */
		user_info->smb_name,        /* user name logging on. */
		user_info->client_domain,   /* domain name */
		user_info->wksta_name,      /* workstation name */
		(uchar *)auth_context->challenge.data, /* 8 byte challenge. */
		3,                          /* validation level */
		user_info->lm_resp,         /* lanman 24 byte response */
		user_info->nt_resp,         /* nt 24 byte response */
		&info3);                    /* info3 out */

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

static char *mymachinepw(TALLOC_CTX *mem_ctx)
{
	fstring pwd;
	const char *script;
	char *to_free = NULL;
	ssize_t nread;
	int ret, fd;

	script = lp_parm_const_string(
		GLOBAL_SECTION_SNUM, "auth_netlogond", "machinepwscript",
		NULL);

	if (script == NULL) {
		to_free = talloc_asprintf(talloc_tos(), "%s/%s",
					  get_dyn_SBINDIR(), "mymachinepw");
		script = to_free;
	}
	if (script == NULL) {
		return NULL;
	}

	ret = smbrun(script, &fd);
	DEBUG(ret ? 0 : 3, ("mymachinepw: Running the command `%s' gave %d\n",
			    script, ret));
	TALLOC_FREE(to_free);

	if (ret != 0) {
		return NULL;
	}

	nread = read(fd, pwd, sizeof(pwd)-1);
	close(fd);

	if (nread <= 0) {
		DEBUG(3, ("mymachinepwd: Could not read password\n"));
		return NULL;
	}

	pwd[nread] = '\0';

	if (pwd[nread-1] == '\n') {
		pwd[nread-1] = '\0';
	}

	return talloc_strdup(mem_ctx, pwd);
}

static NTSTATUS check_netlogond_security(const struct auth_context *auth_context,
					 void *my_private_data,
					 TALLOC_CTX *mem_ctx,
					 const auth_usersupplied_info *user_info,
					 auth_serversupplied_info **server_info)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct netr_SamInfo3 *info3 = NULL;
	struct rpc_pipe_client *p = NULL;
	struct cli_pipe_auth_data *auth = NULL;
	uint32_t neg_flags = NETLOGON_NEG_AUTH2_ADS_FLAGS;
	char *plaintext_machinepw = NULL;
	uint8_t machine_password[16];
	uint8_t schannel_key[16];
	NTSTATUS schannel_bind_result, status;
	struct named_mutex *mutex = NULL;
	const char *ncalrpcsock;

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

	if (!secrets_fetch_local_schannel_key(schannel_key)) {
		goto new_key;
	}

	status = netlogond_validate(talloc_tos(), auth_context, ncalrpcsock,
				    schannel_key, user_info, &info3,
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

	plaintext_machinepw = mymachinepw(talloc_tos());
	if (plaintext_machinepw == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto done;
	}

	E_md4hash(plaintext_machinepw, machine_password);

	TALLOC_FREE(plaintext_machinepw);

	status = rpccli_netlogon_setup_creds(
		p, global_myname(), lp_workgroup(), global_myname(),
		global_myname(), machine_password, SEC_CHAN_BDC, &neg_flags);

	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(10, ("rpccli_netlogon_setup_creds failed: %s\n",
			   nt_errstr(status)));
		goto done;
	}

	memcpy(schannel_key, p->dc->session_key, 16);
	secrets_store_local_schannel_key(schannel_key);

	TALLOC_FREE(p);

	/*
	 * Retry the authentication with the mutex held. This way nobody else
	 * can step on our toes.
	 */

	status = netlogond_validate(talloc_tos(), auth_context, ncalrpcsock,
				    schannel_key, user_info, &info3,
				    &schannel_bind_result);

	DEBUG(10, ("netlogond_validate returned %s\n", nt_errstr(status)));

	if (!NT_STATUS_IS_OK(status)) {
		goto done;
	}

 okay:

	status = make_server_info_info3(mem_ctx, user_info->smb_name,
					user_info->domain, server_info,
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
	if (!make_auth_methods(auth_context, auth_method)) {
		return NT_STATUS_NO_MEMORY;
	}

	(*auth_method)->name = "netlogond";
	(*auth_method)->auth = check_netlogond_security;
	return NT_STATUS_OK;
}

NTSTATUS auth_netlogond_init(void)
{
	smb_register_auth(AUTH_INTERFACE_VERSION, "netlogond",
			  auth_init_netlogond);
	return NT_STATUS_OK;
}
