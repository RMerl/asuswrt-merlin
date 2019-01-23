/*
   Unix SMB/CIFS implementation.

   Command backend for wbinfo --group-info

   Copyright (C) Kai Blin 2008

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
#include "winbind/wb_helper.h"
#include "smbd/service_task.h"

struct cmd_getgrnam_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	char *name;
	char *workgroup_name;
	struct dom_sid *group_sid;

	struct winbindd_gr *result;
};

static void cmd_getgrnam_recv_domain(struct composite_context *ctx);
static void cmd_getgrnam_recv_group_info(struct composite_context *ctx);
static void cmd_getgrnam_recv_gid(struct composite_context *ctx);

struct composite_context *wb_cmd_getgrnam_send(TALLOC_CTX *mem_ctx,
						 struct wbsrv_service *service,
						 const char *name)
{
	struct composite_context *result, *ctx;
	struct cmd_getgrnam_state *state;

	DEBUG(5, ("wb_cmd_getgrnam_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(result, struct cmd_getgrnam_state);
	if (composite_nomem(state, result)) return result;
	state->ctx = result;
	result->private_data = state;
	state->service = service;
	state->name = talloc_strdup(state, name);
	if(composite_nomem(state->name, result)) return result;

	ctx = wb_name2domain_send(state, service, name);
	if (composite_nomem(ctx, result)) return result;

	composite_continue(result, ctx, cmd_getgrnam_recv_domain, state);
	return result;
}

static void cmd_getgrnam_recv_domain(struct composite_context *ctx)
{
	struct cmd_getgrnam_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_getgrnam_state);
	struct wbsrv_domain *domain;
	struct libnet_GroupInfo *group_info;
	char *group_dom, *group_name;
	bool ok;

	state->ctx->status = wb_name2domain_recv(ctx, &domain);
	if(!composite_is_ok(state->ctx)) return;

	group_info = talloc(state, struct libnet_GroupInfo);
	if (composite_nomem(group_info, state->ctx)) return;

	ok = wb_samba3_split_username(state, state->service->task->lp_ctx,
				      state->name, &group_dom, &group_name);
	if(!ok){
		composite_error(state->ctx, NT_STATUS_OBJECT_NAME_INVALID);
		return;
	}

	group_info->in.level = GROUP_INFO_BY_NAME;
	group_info->in.data.group_name = group_name;
	group_info->in.domain_name = group_dom;
	state->workgroup_name = talloc_strdup(state, group_dom);
	if(composite_nomem(state->workgroup_name, state->ctx)) return;

	ctx = libnet_GroupInfo_send(domain->libnet_ctx, state, group_info,NULL);

	composite_continue(state->ctx, ctx, cmd_getgrnam_recv_group_info,state);
}

static void cmd_getgrnam_recv_group_info(struct composite_context *ctx)
{
	struct cmd_getgrnam_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_getgrnam_state);
	struct libnet_GroupInfo *group_info;
	struct winbindd_gr *gr;

	DEBUG(5, ("cmd_getgrnam_recv_group_info called\n"));

	group_info = talloc(state, struct libnet_GroupInfo);
	if(composite_nomem(group_info, state->ctx)) return;

	gr = talloc(state, struct winbindd_gr);
	if(composite_nomem(gr, state->ctx)) return;

	state->ctx->status = libnet_GroupInfo_recv(ctx, state, group_info);
	if(!composite_is_ok(state->ctx)) return;

	WBSRV_SAMBA3_SET_STRING(gr->gr_name, group_info->out.group_name);
	WBSRV_SAMBA3_SET_STRING(gr->gr_passwd, "*");
	gr->num_gr_mem = group_info->out.num_members;
	gr->gr_mem_ofs = 0;

	state->result = gr;

	ctx = wb_sid2gid_send(state, state->service, group_info->out.group_sid);
	composite_continue(state->ctx, ctx, cmd_getgrnam_recv_gid, state);
}

static void cmd_getgrnam_recv_gid(struct composite_context *ctx)
{
	struct cmd_getgrnam_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_getgrnam_state);
	gid_t gid;

	DEBUG(5, ("cmd_getgrnam_recv_gid called\n"));

	state->ctx->status = wb_sid2gid_recv(ctx, &gid);
	if(!composite_is_ok(state->ctx)) return;

	state->result->gr_gid = gid;

	composite_done(state->ctx);
}

NTSTATUS wb_cmd_getgrnam_recv(struct composite_context *ctx,
		TALLOC_CTX *mem_ctx, struct winbindd_gr **gr)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_getgrnam_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_getgrnam_state *state =
			talloc_get_type(ctx->private_data,
					struct cmd_getgrnam_state);
		*gr = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(ctx);
	return status;

}

