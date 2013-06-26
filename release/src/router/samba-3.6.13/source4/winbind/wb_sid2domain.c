/* 
   Unix SMB/CIFS implementation.

   Find and init a domain struct for a SID

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
#include "winbind/wb_server.h"
#include "smbd/service_task.h"
#include "libcli/security/security.h"
#include "../lib/util/dlinklist.h"
#include "param/param.h"

static struct wbsrv_domain *find_domain_from_sid(struct wbsrv_service *service,
						 const struct dom_sid *sid)
{
	struct wbsrv_domain *domain;

	for (domain = service->domains; domain!=NULL; domain = domain->next) {
		if (dom_sid_equal(domain->info->sid, sid)) {
			break;
		}
		if (dom_sid_in_domain(domain->info->sid, sid)) {
			break;
		}
	}
	return domain;
}

struct sid2domain_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	struct dom_sid *sid;

	struct wbsrv_domain *domain;
};

static void sid2domain_recv_dom_info(struct composite_context *ctx);
static void sid2domain_recv_name(struct composite_context *ctx);
static void sid2domain_recv_trusted_dom_info(struct composite_context *ctx);
static void sid2domain_recv_init(struct composite_context *ctx);

struct composite_context *wb_sid2domain_send(TALLOC_CTX *mem_ctx,
					     struct wbsrv_service *service,
					     const struct dom_sid *sid)
{
	struct composite_context *result, *ctx;
	struct sid2domain_state *state;
	DEBUG(5, ("wb_sid2domain_send called\n"));
	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct sid2domain_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->service = service;
	state->sid = dom_sid_dup(state, sid);
	if (state->sid == NULL) goto failed;

	state->domain = find_domain_from_sid(service, sid);
	if (state->domain != NULL) {
		result->status = NT_STATUS_OK;
		composite_done(result);
		return result;
	}

	if (dom_sid_equal(service->primary_sid, sid) ||
	    dom_sid_in_domain(service->primary_sid, sid)) {
		ctx = wb_get_dom_info_send(state, service,
					   lpcfg_workgroup(service->task->lp_ctx),
					   lpcfg_realm(service->task->lp_ctx),
					   service->primary_sid);
		if (ctx == NULL) goto failed;
		ctx->async.fn = sid2domain_recv_dom_info;
		ctx->async.private_data = state;
		return result;
	}

	ctx = wb_cmd_lookupsid_send(state, service, state->sid);
	if (ctx == NULL) goto failed;
	composite_continue(result, ctx, sid2domain_recv_name, state);

	return result;

 failed:
	talloc_free(result);
	return NULL;

}

static void sid2domain_recv_dom_info(struct composite_context *ctx)
{
	struct sid2domain_state *state =
		talloc_get_type(ctx->async.private_data,
				struct sid2domain_state);
	struct wb_dom_info *info;

	state->ctx->status = wb_get_dom_info_recv(ctx, state, &info);
	if (!composite_is_ok(state->ctx)) return;

	ctx = wb_init_domain_send(state, state->service, info);

	composite_continue(state->ctx, ctx, sid2domain_recv_init, state);
}

static void sid2domain_recv_name(struct composite_context *ctx)
{
	struct sid2domain_state *state =
		talloc_get_type(ctx->async.private_data,
				struct sid2domain_state);
	struct wb_sid_object *name;

	state->ctx->status = wb_cmd_lookupsid_recv(ctx, state, &name);
	if (!composite_is_ok(state->ctx)) return;

	if (name->type == SID_NAME_UNKNOWN) {
		composite_error(state->ctx, NT_STATUS_NO_SUCH_DOMAIN);
		return;
	}

	if (name->type != SID_NAME_DOMAIN) {
		state->sid->num_auths -= 1;
	}

	ctx = wb_trusted_dom_info_send(state, state->service, name->domain,
				       state->sid);

	composite_continue(state->ctx, ctx, sid2domain_recv_trusted_dom_info,
			   state);
}

static void sid2domain_recv_trusted_dom_info(struct composite_context *ctx)
{
	struct sid2domain_state *state =
		talloc_get_type(ctx->async.private_data,
				struct sid2domain_state);
	struct wb_dom_info *info;

	state->ctx->status = wb_trusted_dom_info_recv(ctx, state, &info);
	if (!composite_is_ok(state->ctx)) return;

	ctx = wb_init_domain_send(state, state->service, info);

	composite_continue(state->ctx, ctx, sid2domain_recv_init, state);
}

static void sid2domain_recv_init(struct composite_context *ctx)
{
	struct sid2domain_state *state =
		talloc_get_type(ctx->async.private_data,
				struct sid2domain_state);
	struct wbsrv_domain *existing;

	state->ctx->status = wb_init_domain_recv(ctx, state,
						 &state->domain);
	if (!composite_is_ok(state->ctx)) {
		DEBUG(10, ("Could not init domain\n"));
		return;
	}

	existing = find_domain_from_sid(state->service, state->sid);
	if (existing != NULL) {
		DEBUG(5, ("Initialized domain twice, dropping second one\n"));
		talloc_free(state->domain);
		state->domain = existing;
	}

	talloc_steal(state->service, state->domain);
	DLIST_ADD(state->service->domains, state->domain);

	composite_done(state->ctx);
}

NTSTATUS wb_sid2domain_recv(struct composite_context *ctx,
			    struct wbsrv_domain **result)
{
	NTSTATUS status = composite_wait(ctx);
	if (NT_STATUS_IS_OK(status)) {
		struct sid2domain_state *state =
			talloc_get_type(ctx->private_data,
					struct sid2domain_state);
		*result = state->domain;
	}
	talloc_free(ctx);
	return status;
}

NTSTATUS wb_sid2domain(TALLOC_CTX *mem_ctx, struct wbsrv_service *service,
		       const struct dom_sid *sid,
		       struct wbsrv_domain **result)
{
	struct composite_context *c = wb_sid2domain_send(mem_ctx, service,
							 sid);
	return wb_sid2domain_recv(c, result);
}
