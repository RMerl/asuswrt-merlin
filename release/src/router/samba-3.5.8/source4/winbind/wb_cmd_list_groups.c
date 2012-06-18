/*
   Unix SMB/CIFS implementation.

   Command backend for wbinfo -g

   Copyright (C) Kai Blin 2009

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
#include "winbind/wb_async_helpers.h"
#include "winbind/wb_helper.h"
#include "smbd/service_task.h"
#include "libnet/libnet_proto.h"

struct cmd_list_groups_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;

	struct wbsrv_domain *domain;
	char *domain_name;
	uint32_t resume_index;
	char *result;
};

static void cmd_list_groups_recv_domain(struct composite_context *ctx);
static void cmd_list_groups_recv_group_list(struct composite_context *ctx);

struct composite_context *wb_cmd_list_groups_send(TALLOC_CTX *mem_ctx,
		struct wbsrv_service *service, const char *domain_name)
{
	struct composite_context *ctx, *result;
	struct cmd_list_groups_state *state;

	DEBUG(5, ("wb_cmd_list_groups_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(result, struct cmd_list_groups_state);
	if (composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;
	state->resume_index = 0;
	state->result = talloc_strdup(state, "");
	if (composite_nomem(state->result, state->ctx)) return result;

	/*FIXME: We should look up the domain in the winbind request if it is
	 * set, not just take the primary domain. However, I want to get the
	 * libnet logic to work first. */

	if (domain_name && *domain_name != '\0') {
		state->domain_name = talloc_strdup(state, domain_name);
		if (composite_nomem(state->domain_name, state->ctx))
			return result;
	} else {
		state->domain_name = NULL;
	}

	ctx = wb_sid2domain_send(state, service, service->primary_sid);
	if (composite_nomem(ctx, state->ctx)) return result;

	composite_continue(state->ctx, ctx, cmd_list_groups_recv_domain, state);
	return result;
}

static void cmd_list_groups_recv_domain(struct composite_context *ctx)
{
	struct cmd_list_groups_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_list_groups_state);
	struct wbsrv_domain *domain;
	struct libnet_GroupList *group_list;

	DEBUG(5, ("cmd_list_groups_recv_domain called\n"));

	state->ctx->status = wb_sid2domain_recv(ctx, &domain);
	if (!composite_is_ok(state->ctx)) return;

	state->domain = domain;

	/* If this is non-null, we've looked up the domain given in the winbind
	 * request, otherwise we'll just use the default name.*/
	if (state->domain_name == NULL) {
		state->domain_name = talloc_strdup(state,
				domain->libnet_ctx->samr.name);
		if (composite_nomem(state->domain_name, state->ctx)) return;
	}

	group_list = talloc(state, struct libnet_GroupList);
	if (composite_nomem(group_list, state->ctx)) return;

	group_list->in.domain_name = state->domain_name;

	/* Rafal suggested that 128 is a good number here. I don't like magic
	 * numbers too much, but for now it'll have to do.
	 */
	group_list->in.page_size = 128;
	group_list->in.resume_index = state->resume_index;

	ctx = libnet_GroupList_send(domain->libnet_ctx, state, group_list,NULL);

	composite_continue(state->ctx, ctx, cmd_list_groups_recv_group_list,
			state);
}

static void cmd_list_groups_recv_group_list(struct composite_context *ctx)
{
	struct cmd_list_groups_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_list_groups_state);
	struct libnet_GroupList *group_list;
	NTSTATUS status;
	int i;

	DEBUG(5, ("cmd_list_groups_recv_group_list called\n"));

	group_list = talloc(state, struct libnet_GroupList);
	if (composite_nomem(group_list, state->ctx)) return;

	status = libnet_GroupList_recv(ctx, state, group_list);

	/* If NTSTATUS is neither OK nor MORE_ENTRIES, something broke */
	if (!NT_STATUS_IS_OK(status) &&
	     !NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES)) {
		composite_error(state->ctx, status);
		return;
	}

	for (i = 0; i < group_list->out.count; ++i) {
		DEBUG(5, ("Appending group '%s'\n",
			  group_list->out.groups[i].groupname));
		state->result = talloc_asprintf_append_buffer(state->result,
					"%s,",
					group_list->out.groups[i].groupname);
	}

	/* If the status is OK, we're finished, there's no more groups.
	 * So we'll trim off the trailing ',' and are done.*/
	if (NT_STATUS_IS_OK(status)) {
		int str_len = strlen(state->result);
		DEBUG(5, ("list_GroupList_recv returned NT_STATUS_OK\n"));
		state->result[str_len - 1] = '\0';
		composite_done(state->ctx);
		return;
	}

	DEBUG(5, ("list_GroupList_recv returned NT_STATUS_MORE_ENTRIES\n"));

	/* Otherwise there's more groups to get, so call out to libnet and
	 * continue on this function here. */

	group_list->in.domain_name = state->domain_name;
	/* See comment above about the page size. 128 seems like a good default.
	 */
	group_list->in.page_size = 128;
	group_list->in.resume_index = group_list->out.resume_index;

	ctx = libnet_GroupList_send(state->domain->libnet_ctx, state,group_list,
			NULL);

	composite_continue(state->ctx, ctx, cmd_list_groups_recv_group_list,
			state);
}

NTSTATUS wb_cmd_list_groups_recv(struct composite_context *ctx,
		TALLOC_CTX *mem_ctx, uint32_t *extra_data_len,
		char **extra_data)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_list_groups_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_list_groups_state *state = talloc_get_type(
			ctx->private_data, struct cmd_list_groups_state);

		*extra_data_len = strlen(state->result);
		*extra_data = talloc_steal(mem_ctx, state->result);
	}

	talloc_free(ctx);
	return status;
}


