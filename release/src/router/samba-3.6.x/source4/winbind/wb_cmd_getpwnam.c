/*
   Unix SMB/CIFS implementation.

   Command backend for wbinfo -i

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
#include "param/param.h"
#include "winbind/wb_helper.h"
#include "smbd/service_task.h"
#include "libcli/security/security.h"

struct cmd_getpwnam_state {
	struct composite_context *ctx;
	struct wbsrv_service *service;
	char *name;
	char *workgroup_name;
	struct dom_sid *group_sid;

	struct winbindd_pw *result;
};

static void cmd_getpwnam_recv_domain(struct composite_context *ctx);
static void cmd_getpwnam_recv_user_info(struct composite_context *ctx);
static void cmd_getpwnam_recv_uid(struct composite_context *ctx);
static void cmd_getpwnam_recv_gid(struct composite_context *ctx);

struct composite_context *wb_cmd_getpwnam_send(TALLOC_CTX *mem_ctx,
						 struct wbsrv_service *service,
						 const char *name)
{
	struct composite_context *result, *ctx;
	struct cmd_getpwnam_state *state;

	DEBUG(5, ("wb_cmd_getpwnam_send called\n"));

	result = composite_create(mem_ctx, service->task->event_ctx);
	if (!result) return NULL;

	state = talloc(result, struct cmd_getpwnam_state);
	if (composite_nomem(state, result)) return result;
	state->ctx = result;
	result->private_data = state;
	state->service = service;
	state->name = talloc_strdup(state, name);
	if(composite_nomem(state->name, result)) return result;

	ctx = wb_name2domain_send(state, service, name);
	if (composite_nomem(ctx, result)) return result;

	composite_continue(result, ctx, cmd_getpwnam_recv_domain, state);
	return result;
}

static void cmd_getpwnam_recv_domain(struct composite_context *ctx)
{
	struct cmd_getpwnam_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_getpwnam_state);
	struct wbsrv_domain *domain;
	struct libnet_UserInfo *user_info;
	char *user_dom, *user_name;
	bool ok;

	state->ctx->status = wb_name2domain_recv(ctx, &domain);
	if(!composite_is_ok(state->ctx)) return;

	user_info = talloc(state, struct libnet_UserInfo);
	if (composite_nomem(user_info, state->ctx)) return;

	ok= wb_samba3_split_username(state, state->service->task->lp_ctx, state->name, &user_dom, &user_name);
	if(!ok){
		composite_error(state->ctx, NT_STATUS_OBJECT_NAME_INVALID);
		return;
	}

	user_info->in.level = USER_INFO_BY_NAME;
	user_info->in.data.user_name = user_name;
	user_info->in.domain_name = domain->libnet_ctx->samr.name;
	state->workgroup_name = talloc_strdup(state,
			domain->libnet_ctx->samr.name);
	if(composite_nomem(state->workgroup_name, state->ctx)) return;

	ctx = libnet_UserInfo_send(domain->libnet_ctx, state, user_info, NULL);

	composite_continue(state->ctx, ctx, cmd_getpwnam_recv_user_info, state);
}

static void cmd_getpwnam_recv_user_info(struct composite_context *ctx)
{
	struct cmd_getpwnam_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_getpwnam_state);
	struct libnet_UserInfo *user_info;
	struct winbindd_pw *pw;

	DEBUG(5, ("cmd_getpwnam_recv_user_info called\n"));

	user_info = talloc(state, struct libnet_UserInfo);
	if(composite_nomem(user_info, state->ctx)) return;

	pw = talloc(state, struct winbindd_pw);
	if(composite_nomem(pw, state->ctx)) return;

	state->ctx->status = libnet_UserInfo_recv(ctx, state, user_info);
	if(!composite_is_ok(state->ctx)) return;

	WBSRV_SAMBA3_SET_STRING(pw->pw_name, user_info->out.account_name);
	WBSRV_SAMBA3_SET_STRING(pw->pw_passwd, "*");
	WBSRV_SAMBA3_SET_STRING(pw->pw_gecos, user_info->out.full_name);
	WBSRV_SAMBA3_SET_STRING(pw->pw_dir, 
		lpcfg_template_homedir(state->service->task->lp_ctx));
	all_string_sub(pw->pw_dir, "%WORKGROUP%", state->workgroup_name,
			sizeof(fstring) - 1);
	all_string_sub(pw->pw_dir, "%ACCOUNTNAME%", user_info->out.account_name,
			sizeof(fstring) - 1);
	WBSRV_SAMBA3_SET_STRING(pw->pw_shell, 
		lpcfg_template_shell(state->service->task->lp_ctx));

	state->group_sid = dom_sid_dup(state, user_info->out.primary_group_sid);
	if(composite_nomem(state->group_sid, state->ctx)) return;

	state->result = pw;

	ctx = wb_sid2uid_send(state, state->service, user_info->out.user_sid);
	composite_continue(state->ctx, ctx, cmd_getpwnam_recv_uid, state);
}

static void cmd_getpwnam_recv_uid(struct composite_context *ctx)
{
	struct cmd_getpwnam_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_getpwnam_state);
	uid_t uid;

	DEBUG(5, ("cmd_getpwnam_recv_uid called\n"));

	state->ctx->status = wb_sid2uid_recv(ctx, &uid);
	if(!composite_is_ok(state->ctx)) return;

	state->result->pw_uid = uid;

	ctx = wb_sid2gid_send(state, state->service, state->group_sid);
	composite_continue(state->ctx, ctx, cmd_getpwnam_recv_gid, state);
}

static void cmd_getpwnam_recv_gid(struct composite_context *ctx)
{
	struct cmd_getpwnam_state *state = talloc_get_type(
			ctx->async.private_data, struct cmd_getpwnam_state);
	gid_t gid;

	DEBUG(5, ("cmd_getpwnam_recv_gid called\n"));

	state->ctx->status = wb_sid2gid_recv(ctx, &gid);
	if(!composite_is_ok(state->ctx)) return;

	state->result->pw_gid = gid;

	composite_done(state->ctx);
}

NTSTATUS wb_cmd_getpwnam_recv(struct composite_context *ctx,
		TALLOC_CTX *mem_ctx, struct winbindd_pw **pw)
{
	NTSTATUS status = composite_wait(ctx);

	DEBUG(5, ("wb_cmd_getpwnam_recv called\n"));

	if (NT_STATUS_IS_OK(status)) {
		struct cmd_getpwnam_state *state =
			talloc_get_type(ctx->private_data,
					struct cmd_getpwnam_state);
		*pw = talloc_steal(mem_ctx, state->result);
	}
	talloc_free(ctx);
	return status;

}

