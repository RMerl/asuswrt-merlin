/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Samr Support
 *  Copyright (C) Guenther Deschner 2008
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
#include "lib/netapi/netapi.h"
#include "lib/netapi/netapi_private.h"
#include "rpc_client/rpc_client.h"
#include "../librpc/gen_ndr/ndr_samr_c.h"
#include "rpc_client/cli_samr.h"
#include "rpc_client/init_lsa.h"
#include "../libcli/security/security.h"

/****************************************************************
****************************************************************/

WERROR libnetapi_samr_open_domain(struct libnetapi_ctx *mem_ctx,
				  struct rpc_pipe_client *pipe_cli,
				  uint32_t connect_mask,
				  uint32_t domain_mask,
				  struct policy_handle *connect_handle,
				  struct policy_handle *domain_handle,
				  struct dom_sid2 **domain_sid)
{
	NTSTATUS status, result;
	WERROR werr;
	struct libnetapi_private_ctx *priv;
	uint32_t resume_handle = 0;
	uint32_t num_entries = 0;
	struct samr_SamArray *sam = NULL;
	const char *domain_name = NULL;
	struct lsa_String lsa_domain_name;
	bool domain_found = true;
	int i;
	struct dcerpc_binding_handle *b = pipe_cli->binding_handle;

	priv = talloc_get_type_abort(mem_ctx->private_data,
		struct libnetapi_private_ctx);

	if (is_valid_policy_hnd(&priv->samr.connect_handle)) {
		if ((priv->samr.connect_mask & connect_mask) == connect_mask) {
			*connect_handle = priv->samr.connect_handle;
		} else {
			libnetapi_samr_close_connect_handle(mem_ctx,
				&priv->samr.connect_handle);
		}
	}

	if (is_valid_policy_hnd(&priv->samr.domain_handle)) {
		if ((priv->samr.domain_mask & domain_mask) == domain_mask) {
			*domain_handle = priv->samr.domain_handle;
		} else {
			libnetapi_samr_close_domain_handle(mem_ctx,
				&priv->samr.domain_handle);
		}
	}

	if (priv->samr.domain_sid) {
		*domain_sid = priv->samr.domain_sid;
	}

	if (is_valid_policy_hnd(&priv->samr.connect_handle) &&
	    ((priv->samr.connect_mask & connect_mask) == connect_mask) &&
	    is_valid_policy_hnd(&priv->samr.domain_handle) &&
	    (priv->samr.domain_mask & domain_mask) == domain_mask) {
		return WERR_OK;
	}

	if (!is_valid_policy_hnd(connect_handle)) {
		status = dcerpc_try_samr_connects(pipe_cli->binding_handle, mem_ctx,
						  pipe_cli->srv_name_slash,
						  connect_mask,
						  connect_handle,
						  &result);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			werr = ntstatus_to_werror(result);
			goto done;
		}
	}

	status = dcerpc_samr_EnumDomains(b, mem_ctx,
					 connect_handle,
					 &resume_handle,
					 &sam,
					 0xffffffff,
					 &num_entries,
					 &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	for (i=0; i<num_entries; i++) {

		domain_name = sam->entries[i].name.string;

		if (strequal(domain_name, builtin_domain_name())) {
			continue;
		}

		domain_found = true;
		break;
	}

	if (!domain_found) {
		werr = WERR_NO_SUCH_DOMAIN;
		goto done;
	}

	init_lsa_String(&lsa_domain_name, domain_name);

	status = dcerpc_samr_LookupDomain(b, mem_ctx,
					  connect_handle,
					  &lsa_domain_name,
					  domain_sid,
					  &result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					connect_handle,
					domain_mask,
					*domain_sid,
					domain_handle,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	priv->samr.cli			= pipe_cli;

	priv->samr.domain_name		= domain_name;
	priv->samr.domain_sid		= *domain_sid;

	priv->samr.connect_mask		= connect_mask;
	priv->samr.connect_handle	= *connect_handle;

	priv->samr.domain_mask		= domain_mask;
	priv->samr.domain_handle	= *domain_handle;

	werr = WERR_OK;

 done:
	return werr;
}

/****************************************************************
****************************************************************/

WERROR libnetapi_samr_open_builtin_domain(struct libnetapi_ctx *mem_ctx,
					  struct rpc_pipe_client *pipe_cli,
					  uint32_t connect_mask,
					  uint32_t builtin_mask,
					  struct policy_handle *connect_handle,
					  struct policy_handle *builtin_handle)
{
	NTSTATUS status, result;
	WERROR werr;
	struct libnetapi_private_ctx *priv;
	struct dcerpc_binding_handle *b = pipe_cli->binding_handle;

	priv = talloc_get_type_abort(mem_ctx->private_data,
		struct libnetapi_private_ctx);

	if (is_valid_policy_hnd(&priv->samr.connect_handle)) {
		if ((priv->samr.connect_mask & connect_mask) == connect_mask) {
			*connect_handle = priv->samr.connect_handle;
		} else {
			libnetapi_samr_close_connect_handle(mem_ctx,
				&priv->samr.connect_handle);
		}
	}

	if (is_valid_policy_hnd(&priv->samr.builtin_handle)) {
		if ((priv->samr.builtin_mask & builtin_mask) == builtin_mask) {
			*builtin_handle = priv->samr.builtin_handle;
		} else {
			libnetapi_samr_close_builtin_handle(mem_ctx,
				&priv->samr.builtin_handle);
		}
	}

	if (is_valid_policy_hnd(&priv->samr.connect_handle) &&
	    ((priv->samr.connect_mask & connect_mask) == connect_mask) &&
	    is_valid_policy_hnd(&priv->samr.builtin_handle) &&
	    (priv->samr.builtin_mask & builtin_mask) == builtin_mask) {
		return WERR_OK;
	}

	if (!is_valid_policy_hnd(connect_handle)) {
		status = dcerpc_try_samr_connects(pipe_cli->binding_handle, mem_ctx,
						  pipe_cli->srv_name_slash,
						  connect_mask,
						  connect_handle,
						  &result);
		if (!NT_STATUS_IS_OK(status)) {
			werr = ntstatus_to_werror(status);
			goto done;
		}
		if (!NT_STATUS_IS_OK(result)) {
			werr = ntstatus_to_werror(result);
			goto done;
		}
	}

	status = dcerpc_samr_OpenDomain(b, mem_ctx,
					connect_handle,
					builtin_mask,
					CONST_DISCARD(struct dom_sid *, &global_sid_Builtin),
					builtin_handle,
					&result);
	if (!NT_STATUS_IS_OK(status)) {
		werr = ntstatus_to_werror(status);
		goto done;
	}
	if (!NT_STATUS_IS_OK(result)) {
		werr = ntstatus_to_werror(result);
		goto done;
	}

	priv->samr.cli			= pipe_cli;

	priv->samr.connect_mask		= connect_mask;
	priv->samr.connect_handle	= *connect_handle;

	priv->samr.builtin_mask		= builtin_mask;
	priv->samr.builtin_handle	= *builtin_handle;

	werr = WERR_OK;

 done:
	return werr;
}

/****************************************************************
****************************************************************/

void libnetapi_samr_close_domain_handle(struct libnetapi_ctx *ctx,
					struct policy_handle *handle)
{
	struct libnetapi_private_ctx *priv;
	struct dcerpc_binding_handle *b;
	NTSTATUS result;

	if (!is_valid_policy_hnd(handle)) {
		return;
	}

	priv = talloc_get_type_abort(ctx->private_data,
		struct libnetapi_private_ctx);

	if (!policy_handle_equal(handle, &priv->samr.domain_handle)) {
		return;
	}

	b = priv->samr.cli->binding_handle;

	dcerpc_samr_Close(b, ctx, handle, &result);

	ZERO_STRUCT(priv->samr.domain_handle);
}

/****************************************************************
****************************************************************/

void libnetapi_samr_close_builtin_handle(struct libnetapi_ctx *ctx,
					 struct policy_handle *handle)
{
	struct libnetapi_private_ctx *priv;
	struct dcerpc_binding_handle *b;
	NTSTATUS result;

	if (!is_valid_policy_hnd(handle)) {
		return;
	}

	priv = talloc_get_type_abort(ctx->private_data,
		struct libnetapi_private_ctx);

	if (!policy_handle_equal(handle, &priv->samr.builtin_handle)) {
		return;
	}

	b = priv->samr.cli->binding_handle;

	dcerpc_samr_Close(b, ctx, handle, &result);

	ZERO_STRUCT(priv->samr.builtin_handle);
}

/****************************************************************
****************************************************************/

void libnetapi_samr_close_connect_handle(struct libnetapi_ctx *ctx,
					 struct policy_handle *handle)
{
	struct libnetapi_private_ctx *priv;
	struct dcerpc_binding_handle *b;
	NTSTATUS result;

	if (!is_valid_policy_hnd(handle)) {
		return;
	}

	priv = talloc_get_type_abort(ctx->private_data,
		struct libnetapi_private_ctx);

	if (!policy_handle_equal(handle, &priv->samr.connect_handle)) {
		return;
	}

	b = priv->samr.cli->binding_handle;

	dcerpc_samr_Close(b, ctx, handle, &result);

	ZERO_STRUCT(priv->samr.connect_handle);
}

/****************************************************************
****************************************************************/

void libnetapi_samr_free(struct libnetapi_ctx *ctx)
{
	struct libnetapi_private_ctx *priv;

	if (!ctx->private_data) {
		return;
	}

	priv = talloc_get_type_abort(ctx->private_data,
		struct libnetapi_private_ctx);

	libnetapi_samr_close_domain_handle(ctx, &priv->samr.domain_handle);
	libnetapi_samr_close_builtin_handle(ctx, &priv->samr.builtin_handle);
	libnetapi_samr_close_connect_handle(ctx, &priv->samr.connect_handle);
}
