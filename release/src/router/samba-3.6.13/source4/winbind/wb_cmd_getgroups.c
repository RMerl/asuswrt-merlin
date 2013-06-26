/*
   Unix SMB/CIFS implementation.

   Backend for getgroups

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
#include "libcli/security/security.h"

struct cmd_getgroups_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	char* username;
	uint32_t num_groups;
	uint32_t current_group;
	struct dom_sid **sids;

	gid_t *gids;
};

/* The idea is to get the groups for a user
   We receive one user from this we search for his uid
   From the uid we search for his SID
   From the SID we search for the list of groups
   And with the list of groups we search for each group its gid
*/
static void cmd_getgroups_recv_pwnam(struct composite_context *ctx);
static void wb_getgroups_uid2sid_recv(struct composite_context *ctx);
static void wb_getgroups_userdomsgroups_recv(struct composite_context *ctx);
static void cmd_getgroups_recv_gid(struct composite_context *ctx);

/*
  Ask for the uid from the username
*/
struct composite_context *wb_cmd_getgroups_send(TALLOC_CTX *mem_ctx,
						struct wbsrv_service *service,
						const char* username)
{
	struct composite_context *ctx, *result;
	struct cmd_getgroups_state *state;

	DEBUG(5, ("wb_cmd_getgroups_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(mem_ctx, struct cmd_getgroups_state);
	if (composite_nomem(state, result)) return result;

	state->ctx = result;
	result->private_data = state;
	state->service = service;
	state->num_groups = 0;

	state->username = talloc_strdup(state,username);
	if (composite_nomem(state->username, result)) return result;

	ctx = wb_cmd_getpwnam_send(state, service, username);
	if (composite_nomem(ctx, result)) return result;

	composite_continue(result, ctx, cmd_getgroups_recv_pwnam, state);
	return result;
}

/*
  Receive the uid and send request for SID
*/
static void cmd_getgroups_recv_pwnam(struct composite_context *ctx)
{
	struct composite_context *res;
        struct cmd_getgroups_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getgroups_state);
	struct winbindd_pw *pw;
	struct wbsrv_service *service = state->service;

	DEBUG(5, ("cmd_getgroups_recv_pwnam called\n"));

	state->ctx->status = wb_cmd_getpwnam_recv(ctx, state, &pw);
	if (composite_is_ok(state->ctx)) {
		res = wb_uid2sid_send(state, service, pw->pw_uid);
		if (res == NULL) {
			composite_error(state->ctx, NT_STATUS_NO_MEMORY);
			return;
		}
		DEBUG(6, ("cmd_getgroups_recv_pwnam uid %d\n",pw->pw_uid));

		composite_continue(ctx, res, wb_getgroups_uid2sid_recv, state);
	}
}

/*
  Receive the SID and request groups through the userdomgroups helper
*/
static void wb_getgroups_uid2sid_recv(struct composite_context *ctx)
{
	struct composite_context *res;
        struct cmd_getgroups_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getgroups_state);
	NTSTATUS status;
	struct dom_sid *sid;
	char *sid_str;

	DEBUG(5, ("wb_getgroups_uid2sid_recv called\n"));

	status = wb_uid2sid_recv(ctx, state, &sid);
	if(NT_STATUS_IS_OK(status)) {
		sid_str = dom_sid_string(state, sid);

		/* If the conversion failed, bail out with a failure. */
		if (sid_str != NULL) {
			DEBUG(7, ("wb_getgroups_uid2sid_recv SID = %s\n",sid_str));
			/* Ok got the SID now get the groups */
			res = wb_cmd_userdomgroups_send(state, state->service, sid);
			if (res == NULL) {
				composite_error(state->ctx,
						NT_STATUS_NO_MEMORY);
				return;
			}

			composite_continue(ctx, res, wb_getgroups_userdomsgroups_recv, state);
		} else {
			composite_error(state->ctx, NT_STATUS_UNSUCCESSFUL);
		}
	}
}

/*
  Receive groups and search for uid for the first group
*/
static void wb_getgroups_userdomsgroups_recv(struct composite_context *ctx) {
        struct cmd_getgroups_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getgroups_state);
	uint32_t num_sids;
	struct dom_sid **sids;

	DEBUG(5, ("wb_getgroups_userdomsgroups_recv called\n"));
	state->ctx->status = wb_cmd_userdomgroups_recv(ctx,state,&num_sids,&sids);
	if (!composite_is_ok(state->ctx)) return;

	DEBUG(5, ("wb_getgroups_userdomsgroups_recv %d groups\n",num_sids));

	state->sids=sids;
	state->num_groups=num_sids;
	state->current_group=0;

	if(num_sids > 0) {
		state->gids = talloc_array(state, gid_t, state->num_groups);
		ctx = wb_sid2gid_send(state, state->service, state->sids[state->current_group]);
		composite_continue(state->ctx, ctx, cmd_getgroups_recv_gid, state);
	} else {
		composite_done(state->ctx);
	}
}

/*
  Receive and uid the previous searched group and request the uid for the next one
*/
static void cmd_getgroups_recv_gid(struct composite_context *ctx)
{
        struct cmd_getgroups_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getgroups_state);
	gid_t gid;

	DEBUG(5, ("cmd_getgroups_recv_gid called\n"));

	state->ctx->status = wb_sid2gid_recv(ctx, &gid);
	if(!composite_is_ok(state->ctx)) return;

	state->gids[state->current_group] = gid;
	DEBUG(5, ("cmd_getgroups_recv_gid group %d \n",state->current_group));

	state->current_group++;
	if(state->current_group < state->num_groups ) {
		ctx = wb_sid2gid_send(state, state->service, state->sids[state->current_group]);
		composite_continue(state->ctx, ctx, cmd_getgroups_recv_gid, state);
	} else {
		composite_done(state->ctx);
	}
}

/*
  Return list of uids when finished
*/
NTSTATUS wb_cmd_getgroups_recv(struct composite_context *ctx,
			       TALLOC_CTX *mem_ctx, gid_t **groups,
			       uint32_t *num_groups)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_getgroups_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_getgroups_state *state =
			talloc_get_type(ctx->private_data,
					struct cmd_getgroups_state);
		*groups = talloc_steal(mem_ctx, state->gids);
		*num_groups = state->num_groups;
	}
	talloc_free(ctx);
	return status;
}
