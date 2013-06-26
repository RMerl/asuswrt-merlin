/*
   Unix SMB/CIFS implementation.

   Command backend for setgrent

   Copyright (C) Matthieu Patou 2010

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

struct cmd_setgrent_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	struct libnet_context *libnet_ctx;

	struct wbsrv_grent *result;
	char *domain_name;
};

static void cmd_setgrent_recv_domain(struct composite_context *ctx);
static void cmd_setgrent_recv_group_list(struct composite_context *ctx);

struct composite_context *wb_cmd_setgrent_send(TALLOC_CTX *mem_ctx,
		struct wbsrv_service *service)
{
	struct composite_context *ctx, *result;
	struct cmd_setgrent_state *state;

	DEBUG(5, ("wb_cmd_setgrent_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(mem_ctx, struct cmd_setgrent_state);
	if (composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;

	state->result = talloc(state, struct wbsrv_grent);
	if (composite_nomem(state->result, state->ctx)) return result;

	ctx = wb_sid2domain_send(state, service, service->primary_sid);
	if (composite_nomem(ctx, state->ctx)) return result;

	composite_continue(state->ctx, ctx, cmd_setgrent_recv_domain, state);
	return result;
}

static void cmd_setgrent_recv_domain(struct composite_context *ctx)
{
	struct cmd_setgrent_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_setgrent_state);
	struct wbsrv_domain *domain;
	struct libnet_GroupList *group_list;

	DEBUG(5, ("cmd_setgrent_recv_domain called\n"));

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if (!composite_is_ok(state->ctx)) return;

	state->libnet_ctx = domain->libnet_ctx;

	group_list = talloc(state->result, struct libnet_GroupList);
	if (composite_nomem(group_list, state->ctx)) return;

	state->domain_name = talloc_strdup(state,
			domain->libnet_ctx->samr.name);
	group_list->in.domain_name = talloc_strdup(state,
			domain->libnet_ctx->samr.name);
	if (composite_nomem(group_list->in.domain_name, state->ctx)) return;

	/* Page size recommended by Rafal */
	group_list->in.page_size = 128;

	/* Always get the start of the list */
	group_list->in.resume_index = 0;

	ctx = libnet_GroupList_send(domain->libnet_ctx, state->result, group_list,
			NULL);

	state->result->page_index = -1;
	composite_continue(state->ctx, ctx, cmd_setgrent_recv_group_list, state);
}

static void cmd_setgrent_recv_group_list(struct composite_context *ctx)
{
	struct cmd_setgrent_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_setgrent_state);
	struct libnet_GroupList *group_list;
	struct libnet_GroupList *group_list_send;
	DEBUG(5, ("cmd_setgrent_recv_group_list called\n"));

	group_list = talloc(state->result, struct libnet_GroupList);
	if (composite_nomem(group_list, state->ctx)) return;

	state->ctx->status = libnet_GroupList_recv(ctx, state->result,
			group_list);
	if (NT_STATUS_IS_OK(state->ctx->status) ||
		NT_STATUS_EQUAL(state->ctx->status, STATUS_MORE_ENTRIES)) {
		if( state->result->page_index == -1) { /* First run*/
			state->result->group_list = group_list;
			state->result->page_index = 0;
			state->result->libnet_ctx = state->libnet_ctx;
		} else {
			int i;
			struct grouplist *tmp;
			tmp = state->result->group_list->out.groups;
			state->result->group_list->out.groups = talloc_realloc(state->result,tmp,struct grouplist,
			state->result->group_list->out.count+group_list->out.count);
			tmp = state->result->group_list->out.groups;
			for(i=0;i<group_list->out.count;i++ ) {
				tmp[i+state->result->group_list->out.count].groupname = talloc_steal(state->result,group_list->out.groups[i].groupname);
			}
			state->result->group_list->out.count += group_list->out.count;
			talloc_free(group_list);
		}


		if (NT_STATUS_IS_OK(state->ctx->status) ) {
			composite_done(state->ctx);
		} else {
			group_list_send = talloc(state->result, struct libnet_GroupList);
			if (composite_nomem(group_list_send, state->ctx)) return;
			group_list_send->in.domain_name =  talloc_strdup(state, state->domain_name);
			group_list_send->in.resume_index = group_list->out.resume_index;
			group_list_send->in.page_size = 128;
			ctx = libnet_GroupList_send(state->libnet_ctx, state->result, group_list_send, NULL);
			composite_continue(state->ctx, ctx, cmd_setgrent_recv_group_list, state);
		}
	} else {
		composite_error(state->ctx, state->ctx->status);
	}
	return;
}

NTSTATUS wb_cmd_setgrent_recv(struct composite_context *ctx,
		TALLOC_CTX *mem_ctx, struct wbsrv_grent **grent)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_setgrent_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_setgrent_state *state =
			talloc_get_type(ctx->private_data,
				struct cmd_setgrent_state);

		*grent = talloc_steal(mem_ctx, state->result);
	}

	talloc_free(ctx);
	return status;
}
