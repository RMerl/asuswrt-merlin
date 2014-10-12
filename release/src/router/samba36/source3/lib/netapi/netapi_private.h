/*
 *  Unix SMB/CIFS implementation.
 *  NetApi Support
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

#ifndef __LIB_NETAPI_PRIVATE_H__
#define __LIB_NETAPI_PRIVATE_H__

#include "lib/netapi/netapi_net.h"

#define LIBNETAPI_REDIRECT_TO_LOCALHOST(ctx, r, fn) \
	DEBUG(10,("redirecting call %s to localhost\n", #fn)); \
	if (!r->in.server_name) { \
		r->in.server_name = "localhost"; \
	} \
	return fn ## _r(ctx, r);

struct dcerpc_binding_handle;

struct libnetapi_private_ctx {
	struct {
		const char *domain_name;
		struct dom_sid *domain_sid;
		struct rpc_pipe_client *cli;

		uint32_t connect_mask;
		struct policy_handle connect_handle;

		uint32_t domain_mask;
		struct policy_handle domain_handle;

		uint32_t builtin_mask;
		struct policy_handle builtin_handle;
	} samr;

	struct client_ipc_connection *ipc_connections;

	struct messaging_context *msg_ctx;
};

NET_API_STATUS libnetapi_get_password(struct libnetapi_ctx *ctx, char **password);
NET_API_STATUS libnetapi_get_username(struct libnetapi_ctx *ctx, char **username);
NET_API_STATUS libnetapi_set_error_string(struct libnetapi_ctx *ctx, const char *format, ...);
NET_API_STATUS libnetapi_get_debuglevel(struct libnetapi_ctx *ctx, char **debuglevel);

WERROR libnetapi_shutdown_cm(struct libnetapi_ctx *ctx);
WERROR libnetapi_open_pipe(struct libnetapi_ctx *ctx,
			   const char *server_name,
			   const struct ndr_syntax_id *interface,
			   struct rpc_pipe_client **presult);
WERROR libnetapi_get_binding_handle(struct libnetapi_ctx *ctx,
				    const char *server_name,
				    const struct ndr_syntax_id *interface,
				    struct dcerpc_binding_handle **binding_handle);
WERROR libnetapi_samr_open_domain(struct libnetapi_ctx *mem_ctx,
				  struct rpc_pipe_client *pipe_cli,
				  uint32_t connect_mask,
				  uint32_t domain_mask,
				  struct policy_handle *connect_handle,
				  struct policy_handle *domain_handle,
				  struct dom_sid2 **domain_sid);
WERROR libnetapi_samr_open_builtin_domain(struct libnetapi_ctx *mem_ctx,
					  struct rpc_pipe_client *pipe_cli,
					  uint32_t connect_mask,
					  uint32_t builtin_mask,
					  struct policy_handle *connect_handle,
					  struct policy_handle *builtin_handle);
void libnetapi_samr_close_domain_handle(struct libnetapi_ctx *ctx,
					struct policy_handle *handle);
void libnetapi_samr_close_builtin_handle(struct libnetapi_ctx *ctx,
					 struct policy_handle *handle);
void libnetapi_samr_close_connect_handle(struct libnetapi_ctx *ctx,
					 struct policy_handle *handle);
void libnetapi_samr_free(struct libnetapi_ctx *ctx);

NTSTATUS add_GROUP_USERS_INFO_X_buffer(TALLOC_CTX *mem_ctx,
				       uint32_t level,
				       const char *group_name,
				       uint32_t attributes,
				       uint8_t **buffer,
				       uint32_t *num_entries);

#endif
