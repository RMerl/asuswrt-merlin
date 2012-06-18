/* 
   Unix SMB/CIFS implementation.

   Get a struct wb_dom_info for a domain using DNS, netbios, possibly cldap
   etc.

   Copyright (C) Volker Lendecke 2005
   
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
#include "libcli/composite/composite.h"
#include "libcli/resolve/resolve.h"
#include "libcli/security/security.h"
#include "winbind/wb_server.h"
#include "smbd/service_task.h"
#include "librpc/gen_ndr/ndr_irpc.h"
#include "librpc/gen_ndr/samr.h"
#include "lib/messaging/irpc.h"
#include "libcli/finddcs.h"
#include "param/param.h"

struct get_dom_info_state {
	struct composite_context *ctx;
	struct wb_dom_info *info;
};

static void get_dom_info_recv_addrs(struct composite_context *ctx);

struct composite_context *wb_get_dom_info_send(TALLOC_CTX *mem_ctx,
					       struct wbsrv_service *service,
					       const char *domain_name,
					       const struct dom_sid *sid)
{
	struct composite_context *result, *ctx;
	struct get_dom_info_state *state;
	struct dom_sid *dom_sid;
	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct get_dom_info_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->info = talloc_zero(state, struct wb_dom_info);
	if (state->info == NULL) goto failed;

	state->info->name = talloc_strdup(state->info, domain_name);
	if (state->info->name == NULL) goto failed;

	state->info->sid = dom_sid_dup(state->info, sid);
	if (state->info->sid == NULL) goto failed;

	dom_sid = dom_sid_dup(mem_ctx, sid);
	if (dom_sid == NULL) goto failed;

	ctx = finddcs_send(mem_ctx, lp_netbios_name(service->task->lp_ctx),
			   lp_nbt_port(service->task->lp_ctx),
			   domain_name, NBT_NAME_LOGON, 
			   dom_sid, 
			   lp_iconv_convenience(service->task->lp_ctx),
			   lp_resolve_context(service->task->lp_ctx), 
			   service->task->event_ctx, 
			   service->task->msg_ctx);
	if (ctx == NULL) goto failed;

	composite_continue(state->ctx, ctx, get_dom_info_recv_addrs, state);
	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void get_dom_info_recv_addrs(struct composite_context *ctx)
{
	struct get_dom_info_state *state =
		talloc_get_type(ctx->async.private_data,
				struct get_dom_info_state);

	state->ctx->status = finddcs_recv(ctx, state->info,
					  &state->info->num_dcs,
					  &state->info->dcs);
	if (!composite_is_ok(state->ctx)) return;

	composite_done(state->ctx);
}

NTSTATUS wb_get_dom_info_recv(struct composite_context *ctx,
			      TALLOC_CTX *mem_ctx,
			      struct wb_dom_info **result)
{
	NTSTATUS status = composite_wait(ctx);
	if (NT_STATUS_IS_OK(status)) {
		struct get_dom_info_state *state =
			talloc_get_type(ctx->private_data,
					struct get_dom_info_state);
		*result = talloc_steal(mem_ctx, state->info);
	}
	talloc_free(ctx);
	return status;
}

NTSTATUS wb_get_dom_info(TALLOC_CTX *mem_ctx,
			 struct wbsrv_service *service,
			 const char *domain_name,
			 const struct dom_sid *sid,
			 struct wb_dom_info **result)
{
	struct composite_context *ctx =
		wb_get_dom_info_send(mem_ctx, service, domain_name, sid);
	return wb_get_dom_info_recv(ctx, mem_ctx, result);
}
