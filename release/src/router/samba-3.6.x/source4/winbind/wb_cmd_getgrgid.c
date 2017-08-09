/*
   Unix SMB/CIFS implementation.

   Backend for getgrgid

   Copyright (C) Kai Blin 2007

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

struct cmd_getgrgid_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	gid_t gid;
	struct dom_sid *sid;
	char *workgroup;
	struct wbsrv_domain *domain;

	struct winbindd_gr *result;
};

static void cmd_getgrgid_recv_sid(struct composite_context *ctx);
static void cmd_getgrgid_recv_domain(struct composite_context *ctx);
static void cmd_getgrgid_recv_group_info(struct composite_context *ctx);

/* Get the SID using the gid */

struct composite_context *wb_cmd_getgrgid_send(TALLOC_CTX *mem_ctx,
						 struct wbsrv_service *service,
						 gid_t gid)
{
	struct composite_context *ctx, *result;
	struct cmd_getgrgid_state *state;

	DEBUG(5, ("wb_cmd_getgrgid_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(result, struct cmd_getgrgid_state);
	if (composite_nomem(state, result)) return result;
	state->ctx = result;
	result->private_data = state;
	state->service = service;
	state->gid = gid;

	ctx = wb_gid2sid_send(state, service, gid);
	if (composite_nomem(ctx, state->ctx)) return result;

	composite_continue(result, ctx, cmd_getgrgid_recv_sid, state);
	return result;
}


/* Receive the sid and get the domain structure with it */

static void cmd_getgrgid_recv_sid(struct composite_context *ctx)
{
	struct cmd_getgrgid_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getgrgid_state);

	DEBUG(5, ("cmd_getgrgid_recv_sid called %p\n", ctx->private_data));

	state->ctx->status = wb_gid2sid_recv(ctx, state, &state->sid);
	if (!composite_is_ok(state->ctx)) return;

	ctx = wb_sid2domain_send(state, state->service, state->sid);

	composite_continue(state->ctx, ctx, cmd_getgrgid_recv_domain, state);
}

/* Receive the domain struct and call libnet to get the user info struct */

static void cmd_getgrgid_recv_domain(struct composite_context *ctx)
{
	struct cmd_getgrgid_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getgrgid_state);
	struct libnet_GroupInfo *group_info;

	DEBUG(5, ("cmd_getgrgid_recv_domain called\n"));

	state->ctx->status = wb_sid2domain_recv(ctx, &state->domain);
	if (!composite_is_ok(state->ctx)) return;

	group_info = talloc(state, struct libnet_GroupInfo);
	if (composite_nomem(group_info, state->ctx)) return;

	group_info->in.level = GROUP_INFO_BY_SID;
	group_info->in.data.group_sid = state->sid;
	group_info->in.domain_name = state->domain->libnet_ctx->samr.name;

	/* We need the workgroup later, so copy it  */
	state->workgroup = talloc_strdup(state,
			state->domain->libnet_ctx->samr.name);
	if (composite_nomem(state->workgroup, state->ctx)) return;

	ctx = libnet_GroupInfo_send(state->domain->libnet_ctx, state,group_info,
			NULL);

	composite_continue(state->ctx, ctx, cmd_getgrgid_recv_group_info,state);
}

/* Receive the group info struct */

static void cmd_getgrgid_recv_group_info(struct composite_context *ctx)
{
	struct cmd_getgrgid_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getgrgid_state);
	struct libnet_GroupInfo *group_info;
	struct winbindd_gr *gr;

	DEBUG(5, ("cmd_getgrgid_recv_group_info called\n"));

	gr = talloc_zero(state, struct winbindd_gr);
	if (composite_nomem(gr, state->ctx)) return;

	group_info = talloc(state, struct libnet_GroupInfo);
	if(composite_nomem(group_info, state->ctx)) return;

	state->ctx->status = libnet_GroupInfo_recv(ctx, state, group_info);
	if (!composite_is_ok(state->ctx)) return;

	WBSRV_SAMBA3_SET_STRING(gr->gr_name, group_info->out.group_name);
	WBSRV_SAMBA3_SET_STRING(gr->gr_passwd, "*");

	gr->gr_gid = state->gid;

	state->result = gr;

	composite_done(state->ctx);
}

NTSTATUS wb_cmd_getgrgid_recv(struct composite_context *ctx,
		TALLOC_CTX *mem_ctx, struct winbindd_gr **gr)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_getgrgid_recv called\n"));

	DEBUG(5, ("status is %s\n", nt_errstr(status)));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_getgrgid_state *state =
			talloc_get_type(ctx->private_data,
					struct cmd_getgrgid_state);
		*gr = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(ctx);
	return status;

}

