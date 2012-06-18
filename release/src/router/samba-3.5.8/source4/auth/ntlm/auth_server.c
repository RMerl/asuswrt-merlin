/* 
   Unix SMB/CIFS implementation.
   Authenticate by using a remote server
   Copyright (C) Andrew Bartlett         2001-2002, 2008
   Copyright (C) Jelmer Vernooij              2002
   Copyright (C) Stefan Metzmacher            2005
   
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
#include "auth/auth.h"
#include "auth/ntlm/auth_proto.h"
#include "auth/credentials/credentials.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/ndr_samr.h"
#include "libcli/smb_composite/smb_composite.h"
#include "param/param.h"
#include "libcli/resolve/resolve.h"

/* This version of 'security=server' rewirtten from scratch for Samba4
 * libraries in 2008 */


static NTSTATUS server_want_check(struct auth_method_context *ctx,
			      		    TALLOC_CTX *mem_ctx,
					    const struct auth_usersupplied_info *user_info)
{
	return NT_STATUS_OK;
}
/** 
 * The challenge from the target server, when operating in security=server
 **/
static NTSTATUS server_get_challenge(struct auth_method_context *ctx, TALLOC_CTX *mem_ctx, DATA_BLOB *_blob)
{
	struct smb_composite_connect io;
	struct smbcli_options smb_options;
	const char **host_list;
	NTSTATUS status;

	/* Make a connection to the target server, found by 'password server' in smb.conf */
	
	lp_smbcli_options(ctx->auth_ctx->lp_ctx, &smb_options);

	/* Make a negprot, WITHOUT SPNEGO, so we get a challenge nice an easy */
	io.in.options.use_spnego = false;

	/* Hope we don't get * (the default), as this won't work... */
	host_list = lp_passwordserver(ctx->auth_ctx->lp_ctx); 
	if (!host_list) {
		return NT_STATUS_INTERNAL_ERROR;
	}
	io.in.dest_host = host_list[0];
	if (strequal(io.in.dest_host, "*")) {
		return NT_STATUS_INTERNAL_ERROR;
	}
	io.in.dest_ports = lp_smb_ports(ctx->auth_ctx->lp_ctx); 
	io.in.socket_options = lp_socket_options(ctx->auth_ctx->lp_ctx);
	io.in.gensec_settings = lp_gensec_settings(mem_ctx, ctx->auth_ctx->lp_ctx);

	io.in.called_name = strupper_talloc(mem_ctx, io.in.dest_host);

	/* We don't want to get as far as the session setup */
	io.in.credentials = cli_credentials_init_anon(mem_ctx);
	cli_credentials_set_workstation(io.in.credentials,
					lp_netbios_name(ctx->auth_ctx->lp_ctx),
					CRED_SPECIFIED);

	io.in.service = NULL;

	io.in.workgroup = ""; /* only used with SPNEGO, disabled above */

	io.in.options = smb_options;
	
	io.in.iconv_convenience = lp_iconv_convenience(ctx->auth_ctx->lp_ctx);
	lp_smbcli_session_options(ctx->auth_ctx->lp_ctx, &io.in.session_options);

	status = smb_composite_connect(&io, mem_ctx, lp_resolve_context(ctx->auth_ctx->lp_ctx),
				       ctx->auth_ctx->event_ctx);
	NT_STATUS_NOT_OK_RETURN(status);

	*_blob = io.out.tree->session->transport->negotiate.secblob;
	ctx->private_data = talloc_steal(ctx, io.out.tree->session);
	return NT_STATUS_OK;
}

/** 
 * Return an error based on username
 *
 * This function allows the testing of obsure errors, as well as the generation
 * of NT_STATUS -> DOS error mapping tables.
 *
 * This module is of no value to end-users.
 *
 * The password is ignored.
 *
 * @return An NTSTATUS value based on the username
 **/

static NTSTATUS server_check_password(struct auth_method_context *ctx,
				      TALLOC_CTX *mem_ctx,
				      const struct auth_usersupplied_info *user_info, 
				      struct auth_serversupplied_info **_server_info)
{
	NTSTATUS nt_status;
	struct auth_serversupplied_info *server_info;
	struct cli_credentials *creds;
	struct smb_composite_sesssetup session_setup;

	struct smbcli_session *session = talloc_get_type(ctx->private_data, struct smbcli_session);

	creds = cli_credentials_init(mem_ctx);

	NT_STATUS_HAVE_NO_MEMORY(creds);
	
	cli_credentials_set_username(creds, user_info->client.account_name, CRED_SPECIFIED);
	cli_credentials_set_domain(creds, user_info->client.domain_name, CRED_SPECIFIED);

	switch (user_info->password_state) {
	case AUTH_PASSWORD_PLAIN:
		cli_credentials_set_password(creds, user_info->password.plaintext, 
					     CRED_SPECIFIED);
		break;
	case AUTH_PASSWORD_HASH:
		cli_credentials_set_nt_hash(creds, user_info->password.hash.nt,
					    CRED_SPECIFIED);
		break;
		
	case AUTH_PASSWORD_RESPONSE:
		cli_credentials_set_ntlm_response(creds, &user_info->password.response.lanman, &user_info->password.response.nt, CRED_SPECIFIED);
		break;
	}

	session_setup.in.sesskey = session->transport->negotiate.sesskey;
	session_setup.in.capabilities = session->transport->negotiate.capabilities;

	session_setup.in.credentials = creds;
	session_setup.in.workgroup = ""; /* Only used with SPNEGO, which we are not doing */
	session_setup.in.gensec_settings = lp_gensec_settings(session, ctx->auth_ctx->lp_ctx);

	/* Check password with remove server - this should be async some day */
	nt_status = smb_composite_sesssetup(session, &session_setup);

	if (!NT_STATUS_IS_OK(nt_status)) {
		return nt_status;
	}

	server_info = talloc(mem_ctx, struct auth_serversupplied_info);
	NT_STATUS_HAVE_NO_MEMORY(server_info);

	server_info->account_sid = dom_sid_parse_talloc(server_info, SID_NT_ANONYMOUS);
	NT_STATUS_HAVE_NO_MEMORY(server_info->account_sid);

	/* is this correct? */
	server_info->primary_group_sid = dom_sid_parse_talloc(server_info, SID_BUILTIN_GUESTS);
	NT_STATUS_HAVE_NO_MEMORY(server_info->primary_group_sid);

	server_info->n_domain_groups = 0;
	server_info->domain_groups = NULL;

	/* annoying, but the Anonymous really does have a session key, 
	   and it is all zeros! */
	server_info->user_session_key = data_blob(NULL, 0);
	server_info->lm_session_key = data_blob(NULL, 0);

	server_info->account_name = talloc_strdup(server_info, user_info->client.account_name);
	NT_STATUS_HAVE_NO_MEMORY(server_info->account_name);

	server_info->domain_name = talloc_strdup(server_info, user_info->client.domain_name);
	NT_STATUS_HAVE_NO_MEMORY(server_info->domain_name);

	server_info->full_name = NULL;

	server_info->logon_script = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->logon_script);

	server_info->profile_path = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->profile_path);

	server_info->home_directory = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->home_directory);

	server_info->home_drive = talloc_strdup(server_info, "");
	NT_STATUS_HAVE_NO_MEMORY(server_info->home_drive);

	server_info->last_logon = 0;
	server_info->last_logoff = 0;
	server_info->acct_expiry = 0;
	server_info->last_password_change = 0;
	server_info->allow_password_change = 0;
	server_info->force_password_change = 0;

	server_info->logon_count = 0;
	server_info->bad_password_count = 0;

	server_info->acct_flags = ACB_NORMAL;

	server_info->authenticated = false;

	*_server_info = server_info;

	return nt_status;
}

static const struct auth_operations server_auth_ops = {
	.name		= "server",
	.get_challenge	= server_get_challenge,
	.want_check	= server_want_check,
	.check_password	= server_check_password
};

_PUBLIC_ NTSTATUS auth_server_init(void)
{
	NTSTATUS ret;

	ret = auth_register(&server_auth_ops);
	if (!NT_STATUS_IS_OK(ret)) {
		DEBUG(0,("Failed to register 'server' auth backend!\n"));
		return ret;
	}

	return ret;
}
