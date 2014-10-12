/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Join Support
 *  Copyright (C) Guenther Deschner 2007-2008
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

#include "includes.h"
#include "ads.h"
#include "librpc/gen_ndr/libnetapi.h"
#include "libcli/auth/libcli_auth.h"
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_private.h"
#include "lib/netapi/libnetapi.h"
#include "librpc/gen_ndr/libnet_join.h"
#include "libnet/libnet_join.h"
#include "../librpc/gen_ndr/ndr_wkssvc_c.h"
#include "rpc_client/cli_pipe.h"
#include "secrets.h"

/****************************************************************
****************************************************************/

WERROR NetJoinDomain_l(struct libnetapi_ctx *mem_ctx,
		       struct NetJoinDomain *r)
{
	struct libnet_JoinCtx *j = NULL;
	struct libnetapi_private_ctx *priv;
	WERROR werr;

	priv = talloc_get_type_abort(mem_ctx->private_data,
		struct libnetapi_private_ctx);

	if (!r->in.domain) {
		return WERR_INVALID_PARAM;
	}

	werr = libnet_init_JoinCtx(mem_ctx, &j);
	W_ERROR_NOT_OK_RETURN(werr);

	j->in.domain_name = talloc_strdup(mem_ctx, r->in.domain);
	W_ERROR_HAVE_NO_MEMORY(j->in.domain_name);

	if (r->in.join_flags & WKSSVC_JOIN_FLAGS_JOIN_TYPE) {
		NTSTATUS status;
		struct netr_DsRGetDCNameInfo *info = NULL;
		const char *dc = NULL;
		uint32_t flags = DS_DIRECTORY_SERVICE_REQUIRED |
				 DS_WRITABLE_REQUIRED |
				 DS_RETURN_DNS_NAME;
		status = dsgetdcname(mem_ctx, priv->msg_ctx, r->in.domain,
				     NULL, NULL, flags, &info);
		if (!NT_STATUS_IS_OK(status)) {
			libnetapi_set_error_string(mem_ctx,
				"%s", get_friendly_nt_error_msg(status));
			return ntstatus_to_werror(status);
		}

		dc = strip_hostname(info->dc_unc);
		j->in.dc_name = talloc_strdup(mem_ctx, dc);
		W_ERROR_HAVE_NO_MEMORY(j->in.dc_name);
	}

	if (r->in.account_ou) {
		j->in.account_ou = talloc_strdup(mem_ctx, r->in.account_ou);
		W_ERROR_HAVE_NO_MEMORY(j->in.account_ou);
	}

	if (r->in.account) {
		j->in.admin_account = talloc_strdup(mem_ctx, r->in.account);
		W_ERROR_HAVE_NO_MEMORY(j->in.admin_account);
	}

	if (r->in.password) {
		j->in.admin_password = talloc_strdup(mem_ctx, r->in.password);
		W_ERROR_HAVE_NO_MEMORY(j->in.admin_password);
	}

	j->in.join_flags = r->in.join_flags;
	j->in.modify_config = true;
	j->in.debug = true;

	werr = libnet_Join(mem_ctx, j);
	if (!W_ERROR_IS_OK(werr) && j->out.error_string) {
		libnetapi_set_error_string(mem_ctx, "%s", j->out.error_string);
	}
	TALLOC_FREE(j);

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetJoinDomain_r(struct libnetapi_ctx *ctx,
		       struct NetJoinDomain *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct wkssvc_PasswordBuffer *encrypted_password = NULL;
	NTSTATUS status;
	WERROR werr;
	unsigned int old_timeout = 0;
	struct dcerpc_binding_handle *b;
	DATA_BLOB session_key;

	werr = libnetapi_open_pipe(ctx, r->in.server,
				   &ndr_table_wkssvc.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	if (r->in.password) {

		status = cli_get_session_key(talloc_tos(), pipe_cli, &session_key);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}

		encode_wkssvc_join_password_buffer(ctx,
						   r->in.password,
						   &session_key,
						   &encrypted_password);
	}

	old_timeout = rpccli_set_timeout(pipe_cli, 600000);

	status = dcerpc_wkssvc_NetrJoinDomain2(b, talloc_tos(),
					       r->in.server,
					       r->in.domain,
					       r->in.account_ou,
					       r->in.account,
					       encrypted_password,
					       r->in.join_flags,
					       &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	if (pipe_cli && old_timeout) {
		rpccli_set_timeout(pipe_cli, old_timeout);
	}

	return werr;
}
/****************************************************************
****************************************************************/

WERROR NetUnjoinDomain_l(struct libnetapi_ctx *mem_ctx,
			 struct NetUnjoinDomain *r)
{
	struct libnet_UnjoinCtx *u = NULL;
	struct dom_sid domain_sid;
	const char *domain = NULL;
	WERROR werr;
	struct libnetapi_private_ctx *priv;

	priv = talloc_get_type_abort(mem_ctx->private_data,
		struct libnetapi_private_ctx);

	if (!secrets_fetch_domain_sid(lp_workgroup(), &domain_sid)) {
		return WERR_SETUP_NOT_JOINED;
	}

	werr = libnet_init_UnjoinCtx(mem_ctx, &u);
	W_ERROR_NOT_OK_RETURN(werr);

	if (lp_realm()) {
		domain = lp_realm();
	} else {
		domain = lp_workgroup();
	}

	if (r->in.server_name) {
		u->in.dc_name = talloc_strdup(mem_ctx, r->in.server_name);
		W_ERROR_HAVE_NO_MEMORY(u->in.dc_name);
	} else {
		NTSTATUS status;
		struct netr_DsRGetDCNameInfo *info = NULL;
		const char *dc = NULL;
		uint32_t flags = DS_DIRECTORY_SERVICE_REQUIRED |
				 DS_WRITABLE_REQUIRED |
				 DS_RETURN_DNS_NAME;
		status = dsgetdcname(mem_ctx, priv->msg_ctx, domain,
				     NULL, NULL, flags, &info);
		if (!NT_STATUS_IS_OK(status)) {
			libnetapi_set_error_string(mem_ctx,
				"failed to find DC for domain %s: %s",
				domain,
				get_friendly_nt_error_msg(status));
			return ntstatus_to_werror(status);
		}

		dc = strip_hostname(info->dc_unc);
		u->in.dc_name = talloc_strdup(mem_ctx, dc);
		W_ERROR_HAVE_NO_MEMORY(u->in.dc_name);

		u->in.domain_name = domain;
	}

	if (r->in.account) {
		u->in.admin_account = talloc_strdup(mem_ctx, r->in.account);
		W_ERROR_HAVE_NO_MEMORY(u->in.admin_account);
	}

	if (r->in.password) {
		u->in.admin_password = talloc_strdup(mem_ctx, r->in.password);
		W_ERROR_HAVE_NO_MEMORY(u->in.admin_password);
	}

	u->in.domain_name = domain;
	u->in.unjoin_flags = r->in.unjoin_flags;
	u->in.delete_machine_account = false;
	u->in.modify_config = true;
	u->in.debug = true;

	u->in.domain_sid = &domain_sid;

	werr = libnet_Unjoin(mem_ctx, u);
	if (!W_ERROR_IS_OK(werr) && u->out.error_string) {
		libnetapi_set_error_string(mem_ctx, "%s", u->out.error_string);
	}
	TALLOC_FREE(u);

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetUnjoinDomain_r(struct libnetapi_ctx *ctx,
			 struct NetUnjoinDomain *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct wkssvc_PasswordBuffer *encrypted_password = NULL;
	NTSTATUS status;
	WERROR werr;
	unsigned int old_timeout = 0;
	struct dcerpc_binding_handle *b;
	DATA_BLOB session_key;

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_wkssvc.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	if (r->in.password) {

		status = cli_get_session_key(talloc_tos(), pipe_cli, &session_key);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}

		encode_wkssvc_join_password_buffer(ctx,
						   r->in.password,
						   &session_key,
						   &encrypted_password);
	}

	old_timeout = rpccli_set_timeout(pipe_cli, 60000);

	status = dcerpc_wkssvc_NetrUnjoinDomain2(b, talloc_tos(),
						 r->in.server_name,
						 r->in.account,
						 encrypted_password,
						 r->in.unjoin_flags,
						 &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	if (pipe_cli && old_timeout) {
		rpccli_set_timeout(pipe_cli, old_timeout);
	}

	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetGetJoinInformation_r(struct libnetapi_ctx *ctx,
			       struct NetGetJoinInformation *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	NTSTATUS status;
	WERROR werr;
	const char *buffer = NULL;
	struct dcerpc_binding_handle *b;

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_wkssvc.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	status = dcerpc_wkssvc_NetrGetJoinInformation(b, talloc_tos(),
						      r->in.server_name,
						      &buffer,
						      (enum wkssvc_NetJoinStatus *)r->out.name_type,
						      &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	*r->out.name_buffer = talloc_strdup(ctx, buffer);
	W_ERROR_HAVE_NO_MEMORY(*r->out.name_buffer);

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetGetJoinInformation_l(struct libnetapi_ctx *ctx,
			       struct NetGetJoinInformation *r)
{
	if ((lp_security() == SEC_ADS) && lp_realm()) {
		*r->out.name_buffer = talloc_strdup(ctx, lp_realm());
	} else {
		*r->out.name_buffer = talloc_strdup(ctx, lp_workgroup());
	}
	if (!*r->out.name_buffer) {
		return WERR_NOMEM;
	}

	switch (lp_server_role()) {
		case ROLE_DOMAIN_MEMBER:
		case ROLE_DOMAIN_PDC:
		case ROLE_DOMAIN_BDC:
			*r->out.name_type = NetSetupDomainName;
			break;
		case ROLE_STANDALONE:
		default:
			*r->out.name_type = NetSetupWorkgroupName;
			break;
	}

	return WERR_OK;
}

/****************************************************************
****************************************************************/

WERROR NetGetJoinableOUs_l(struct libnetapi_ctx *ctx,
			   struct NetGetJoinableOUs *r)
{
#ifdef HAVE_ADS
	NTSTATUS status;
	ADS_STATUS ads_status;
	ADS_STRUCT *ads = NULL;
	struct netr_DsRGetDCNameInfo *info = NULL;
	const char *dc = NULL;
	uint32_t flags = DS_DIRECTORY_SERVICE_REQUIRED |
			 DS_RETURN_DNS_NAME;
	struct libnetapi_private_ctx *priv;

	priv = talloc_get_type_abort(ctx->private_data,
		struct libnetapi_private_ctx);

	status = dsgetdcname(ctx, priv->msg_ctx, r->in.domain,
			     NULL, NULL, flags, &info);
	if (!NT_STATUS_IS_OK(status)) {
		libnetapi_set_error_string(ctx, "%s",
			get_friendly_nt_error_msg(status));
		return ntstatus_to_werror(status);
	}

	dc = strip_hostname(info->dc_unc);

	ads = ads_init(info->domain_name, info->domain_name, dc);
	if (!ads) {
		return WERR_GENERAL_FAILURE;
	}

	SAFE_FREE(ads->auth.user_name);
	if (r->in.account) {
		ads->auth.user_name = SMB_STRDUP(r->in.account);
	} else if (ctx->username) {
		ads->auth.user_name = SMB_STRDUP(ctx->username);
	}

	SAFE_FREE(ads->auth.password);
	if (r->in.password) {
		ads->auth.password = SMB_STRDUP(r->in.password);
	} else if (ctx->password) {
		ads->auth.password = SMB_STRDUP(ctx->password);
	}

	ads_status = ads_connect_user_creds(ads);
	if (!ADS_ERR_OK(ads_status)) {
		ads_destroy(&ads);
		return WERR_DEFAULT_JOIN_REQUIRED;
	}

	ads_status = ads_get_joinable_ous(ads, ctx,
					  (char ***)r->out.ous,
					  (size_t *)r->out.ou_count);
	if (!ADS_ERR_OK(ads_status)) {
		ads_destroy(&ads);
		return WERR_DEFAULT_JOIN_REQUIRED;
	}

	ads_destroy(&ads);
	return WERR_OK;
#else
	return WERR_NOT_SUPPORTED;
#endif
}

/****************************************************************
****************************************************************/

WERROR NetGetJoinableOUs_r(struct libnetapi_ctx *ctx,
			   struct NetGetJoinableOUs *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct wkssvc_PasswordBuffer *encrypted_password = NULL;
	NTSTATUS status;
	WERROR werr;
	struct dcerpc_binding_handle *b;
	DATA_BLOB session_key;

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_wkssvc.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	if (r->in.password) {

		status = cli_get_session_key(talloc_tos(), pipe_cli, &session_key);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}

		encode_wkssvc_join_password_buffer(ctx,
						   r->in.password,
						   &session_key,
						   &encrypted_password);
	}

	status = dcerpc_wkssvc_NetrGetJoinableOus2(b, talloc_tos(),
						   r->in.server_name,
						   r->in.domain,
						   r->in.account,
						   encrypted_password,
						   r->out.ou_count,
						   r->out.ous,
						   &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetRenameMachineInDomain_r(struct libnetapi_ctx *ctx,
				  struct NetRenameMachineInDomain *r)
{
	struct rpc_pipe_client *pipe_cli = NULL;
	struct wkssvc_PasswordBuffer *encrypted_password = NULL;
	NTSTATUS status;
	WERROR werr;
	struct dcerpc_binding_handle *b;
	DATA_BLOB session_key;

	werr = libnetapi_open_pipe(ctx, r->in.server_name,
				   &ndr_table_wkssvc.syntax_id,
				   &pipe_cli);
	if (!W_ERROR_IS_OK(werr)) {
		goto done;
	}

	b = pipe_cli->binding_handle;

	if (r->in.password) {

		status = cli_get_session_key(talloc_tos(), pipe_cli, &session_key);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}

		encode_wkssvc_join_password_buffer(ctx,
						   r->in.password,
						   &session_key,
						   &encrypted_password);
	}

	status = dcerpc_wkssvc_NetrRenameMachineInDomain2(b, talloc_tos(),
							  r->in.server_name,
							  r->in.new_machine_name,
							  r->in.account,
							  encrypted_password,
							  r->in.rename_options,
							  &werr);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR NetRenameMachineInDomain_l(struct libnetapi_ctx *ctx,
				  struct NetRenameMachineInDomain *r)
{
	LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, NetRenameMachineInDomain);
}
