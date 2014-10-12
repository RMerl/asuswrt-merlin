/*
   Unix SMB/CIFS implementation.

   Backend for getpwuid

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
#include "param/param.h"

struct cmd_getpwuid_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	uid_t uid;
	struct dom_sid *sid;
	char *workgroup;
	struct wbsrv_domain *domain;

	struct winbindd_pw *result;
};

static void cmd_getpwuid_recv_sid(struct composite_context *ctx);
static void cmd_getpwuid_recv_domain(struct composite_context *ctx);
static void cmd_getpwuid_recv_user_info(struct composite_context *ctx);
static void cmd_getpwuid_recv_gid(struct composite_context *ctx);

/* Get the SID using the uid */

struct composite_context *wb_cmd_getpwuid_send(TALLOC_CTX *mem_ctx,
						 struct wbsrv_service *service,
						 uid_t uid)
{
	struct composite_context *ctx, *result;
	struct cmd_getpwuid_state *state;

	DEBUG(5, ("wb_cmd_getpwuid_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(result, struct cmd_getpwuid_state);
	if (composite_nomem(state, result)) return result;
	state->ctx = result;
	result->private_data = state;
	state->service = service;
	state->uid = uid;

	ctx = wb_uid2sid_send(state, service, uid);
	if (composite_nomem(ctx, state->ctx)) return result;

	composite_continue(result, ctx, cmd_getpwuid_recv_sid, state);
	return result;
}


/* Receive the sid and get the domain structure with it */

static void cmd_getpwuid_recv_sid(struct composite_context *ctx)
{
	struct cmd_getpwuid_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getpwuid_state);

	DEBUG(5, ("cmd_getpwuid_recv_sid called %p\n", ctx->private_data));

	state->ctx->status = wb_uid2sid_recv(ctx, state, &state->sid);
	if (!composite_is_ok(state->ctx)) return;

	ctx = wb_sid2domain_send(state, state->service, state->sid);

	composite_continue(state->ctx, ctx, cmd_getpwuid_recv_domain, state);
}

/* Receive the domain struct and call libnet to get the user info struct */

static void cmd_getpwuid_recv_domain(struct composite_context *ctx)
{
	struct cmd_getpwuid_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getpwuid_state);
	struct libnet_UserInfo *user_info;

	DEBUG(5, ("cmd_getpwuid_recv_domain called\n"));

	state->ctx->status = wb_sid2domain_recv(ctx, &state->domain);
	if (!composite_is_ok(state->ctx)) return;

	user_info = talloc(state, struct libnet_UserInfo);
	if (composite_nomem(user_info, state->ctx)) return;

	user_info->in.level = USER_INFO_BY_SID;
	user_info->in.data.user_sid = state->sid;
	user_info->in.domain_name = state->domain->libnet_ctx->samr.name;

	/* We need the workgroup later, so copy it  */
	state->workgroup = talloc_strdup(state,
			state->domain->libnet_ctx->samr.name);
	if (composite_nomem(state->workgroup, state->ctx)) return;

	ctx = libnet_UserInfo_send(state->domain->libnet_ctx, state, user_info,
			NULL);

	composite_continue(state->ctx, ctx, cmd_getpwuid_recv_user_info, state);
}

/* Receive the user info struct and get the gid for the user */

static void cmd_getpwuid_recv_user_info(struct composite_context *ctx)
{
	struct cmd_getpwuid_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getpwuid_state);
	struct libnet_UserInfo *user_info;
	struct winbindd_pw *pw;

	DEBUG(5, ("cmd_getpwuid_recv_user_info called\n"));

	pw = talloc(state, struct winbindd_pw);
	if (composite_nomem(pw, state->ctx)) return;

	user_info = talloc(state, struct libnet_UserInfo);
	if(composite_nomem(user_info, state->ctx)) return;

	state->ctx->status = libnet_UserInfo_recv(ctx, state, user_info);
	if (!composite_is_ok(state->ctx)) return;

	WBSRV_SAMBA3_SET_STRING(pw->pw_name, user_info->out.account_name);
	WBSRV_SAMBA3_SET_STRING(pw->pw_passwd, "*");
	WBSRV_SAMBA3_SET_STRING(pw->pw_gecos, user_info->out.full_name);
	WBSRV_SAMBA3_SET_STRING(pw->pw_dir, 
		lpcfg_template_homedir(state->service->task->lp_ctx));
	all_string_sub(pw->pw_dir, "%WORKGROUP%", state->workgroup,
			sizeof(fstring) - 1);
	all_string_sub(pw->pw_dir, "%ACCOUNTNAME%", user_info->out.account_name,
			sizeof(fstring) - 1);
	WBSRV_SAMBA3_SET_STRING(pw->pw_shell, 
				lpcfg_template_shell(state->service->task->lp_ctx));

	pw->pw_uid = state->uid;

	state->result = pw;

	ctx = wb_sid2gid_send(state, state->service,
			user_info->out.primary_group_sid);

	composite_continue(state->ctx, ctx, cmd_getpwuid_recv_gid, state);
}

static void cmd_getpwuid_recv_gid(struct composite_context *ctx)
{
	struct cmd_getpwuid_state *state =
		talloc_get_type(ctx->async.private_data,
				struct cmd_getpwuid_state);
	gid_t gid;

	DEBUG(5, ("cmd_getpwuid_recv_gid called\n"));

	state->ctx->status = wb_sid2gid_recv(ctx, &gid);
	if (!composite_is_ok(state->ctx)) return;

	state->result->pw_gid = gid;

	composite_done(state->ctx);
}

NTSTATUS wb_cmd_getpwuid_recv(struct composite_context *ctx,
		TALLOC_CTX *mem_ctx, struct winbindd_pw **pw)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_getpwuid_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_getpwuid_state *state =
			talloc_get_type(ctx->private_data,
					struct cmd_getpwuid_state);
		*pw = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(ctx);
	return status;

}

