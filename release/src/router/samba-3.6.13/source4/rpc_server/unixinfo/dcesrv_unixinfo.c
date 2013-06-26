/* 
   Unix SMB/CIFS implementation.

   endpoint server for the unixinfo pipe

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
#include "rpc_server/dcerpc_server.h"
#include "librpc/gen_ndr/ndr_unixinfo.h"
#include "libcli/wbclient/wbclient.h"
#include "system/passwd.h"

static NTSTATUS dcerpc_unixinfo_bind(struct dcesrv_call_state *dce_call,
				     const struct dcesrv_interface *iface)
{
	struct wbc_context *wbc_ctx;

	wbc_ctx = wbc_init(dce_call->context, dce_call->msg_ctx,
			   dce_call->event_ctx);
	NT_STATUS_HAVE_NO_MEMORY(wbc_ctx);

	dce_call->context->private_data = wbc_ctx;

	return NT_STATUS_OK;
}

#define DCESRV_INTERFACE_UNIXINFO_BIND dcerpc_unixinfo_bind

static NTSTATUS dcesrv_unixinfo_SidToUid(struct dcesrv_call_state *dce_call,
				  TALLOC_CTX *mem_ctx,
				  struct unixinfo_SidToUid *r)
{
	NTSTATUS status;
	struct wbc_context *wbc_ctx = talloc_get_type_abort(
						dce_call->context->private_data,
						struct wbc_context);
	struct id_map *ids;
	struct composite_context *ctx;

	DEBUG(5, ("dcesrv_unixinfo_SidToUid called\n"));

	ids = talloc(mem_ctx, struct id_map);
	NT_STATUS_HAVE_NO_MEMORY(ids);

	ids->sid = &r->in.sid;
	ids->status = ID_UNKNOWN;
	ZERO_STRUCT(ids->xid);
	ctx = wbc_sids_to_xids_send(wbc_ctx, ids, 1, ids);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	status = wbc_sids_to_xids_recv(ctx, &ids);
	NT_STATUS_NOT_OK_RETURN(status);

	if (ids->xid.type == ID_TYPE_BOTH ||
	    ids->xid.type == ID_TYPE_UID) {
		*r->out.uid = ids->xid.id;
		return NT_STATUS_OK;
	} else {
		return NT_STATUS_INVALID_SID;
	}
}

static NTSTATUS dcesrv_unixinfo_UidToSid(struct dcesrv_call_state *dce_call,
				  TALLOC_CTX *mem_ctx,
				  struct unixinfo_UidToSid *r)
{
	struct wbc_context *wbc_ctx = talloc_get_type_abort(
						dce_call->context->private_data,
						struct wbc_context);
	struct id_map *ids;
	struct composite_context *ctx;
	uint32_t uid;
	NTSTATUS status;

	DEBUG(5, ("dcesrv_unixinfo_UidToSid called\n"));

	uid = r->in.uid; 	/* This cuts uid to 32 bit */
	if ((uint64_t)uid != r->in.uid) {
		DEBUG(10, ("uid out of range\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	ids = talloc(mem_ctx, struct id_map);
	NT_STATUS_HAVE_NO_MEMORY(ids);

	ids->sid = NULL;
	ids->status = ID_UNKNOWN;

	ids->xid.id = uid;
	ids->xid.type = ID_TYPE_UID;

	ctx = wbc_xids_to_sids_send(wbc_ctx, ids, 1, ids);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	status = wbc_xids_to_sids_recv(ctx, &ids);
	NT_STATUS_NOT_OK_RETURN(status);

	r->out.sid = ids->sid;
	return NT_STATUS_OK;
}

static NTSTATUS dcesrv_unixinfo_SidToGid(struct dcesrv_call_state *dce_call,
				  TALLOC_CTX *mem_ctx,
				  struct unixinfo_SidToGid *r)
{
	NTSTATUS status;
	struct wbc_context *wbc_ctx = talloc_get_type_abort(
						dce_call->context->private_data,
						struct wbc_context);
	struct id_map *ids;
	struct composite_context *ctx;

	DEBUG(5, ("dcesrv_unixinfo_SidToGid called\n"));

	ids = talloc(mem_ctx, struct id_map);
	NT_STATUS_HAVE_NO_MEMORY(ids);

	ids->sid = &r->in.sid;
	ids->status = ID_UNKNOWN;
	ZERO_STRUCT(ids->xid);
	ctx = wbc_sids_to_xids_send(wbc_ctx, ids, 1, ids);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	status = wbc_sids_to_xids_recv(ctx, &ids);
	NT_STATUS_NOT_OK_RETURN(status);

	if (ids->xid.type == ID_TYPE_BOTH ||
	    ids->xid.type == ID_TYPE_GID) {
		*r->out.gid = ids->xid.id;
		return NT_STATUS_OK;
	} else {
		return NT_STATUS_INVALID_SID;
	}
}

static NTSTATUS dcesrv_unixinfo_GidToSid(struct dcesrv_call_state *dce_call,
				  TALLOC_CTX *mem_ctx,
				  struct unixinfo_GidToSid *r)
{
	struct wbc_context *wbc_ctx = talloc_get_type_abort(
						dce_call->context->private_data,
						struct wbc_context);
	struct id_map *ids;
	struct composite_context *ctx;
	uint32_t gid;
	NTSTATUS status;

	DEBUG(5, ("dcesrv_unixinfo_GidToSid called\n"));

	gid = r->in.gid; 	/* This cuts gid to 32 bit */
	if ((uint64_t)gid != r->in.gid) {
		DEBUG(10, ("gid out of range\n"));
		return NT_STATUS_INVALID_PARAMETER;
	}

	ids = talloc(mem_ctx, struct id_map);
	NT_STATUS_HAVE_NO_MEMORY(ids);

	ids->sid = NULL;
	ids->status = ID_UNKNOWN;

	ids->xid.id = gid;
	ids->xid.type = ID_TYPE_GID;

	ctx = wbc_xids_to_sids_send(wbc_ctx, ids, 1, ids);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	status = wbc_xids_to_sids_recv(ctx, &ids);
	NT_STATUS_NOT_OK_RETURN(status);

	r->out.sid = ids->sid;
	return NT_STATUS_OK;
}

static NTSTATUS dcesrv_unixinfo_GetPWUid(struct dcesrv_call_state *dce_call,
				  TALLOC_CTX *mem_ctx,
				  struct unixinfo_GetPWUid *r)
{
	unsigned int i;

	*r->out.count = 0;

	r->out.infos = talloc_zero_array(mem_ctx, struct unixinfo_GetPWUidInfo,
					 *r->in.count);
	NT_STATUS_HAVE_NO_MEMORY(r->out.infos);
	*r->out.count = *r->in.count;

	for (i=0; i < *r->in.count; i++) {
		uid_t uid;
		struct passwd *pwd;

		uid = r->in.uids[i];
		pwd = getpwuid(uid);
		if (pwd == NULL) {
			DEBUG(10, ("uid %d not found\n", uid));
			r->out.infos[i].homedir = "";
			r->out.infos[i].shell = "";
			r->out.infos[i].status = NT_STATUS_NO_SUCH_USER;
			continue;
		}

		r->out.infos[i].homedir = talloc_strdup(mem_ctx, pwd->pw_dir);
		r->out.infos[i].shell = talloc_strdup(mem_ctx, pwd->pw_shell);

		if ((r->out.infos[i].homedir == NULL) ||
		    (r->out.infos[i].shell == NULL)) {
			r->out.infos[i].homedir = "";
			r->out.infos[i].shell = "";
			r->out.infos[i].status = NT_STATUS_NO_MEMORY;
			continue;
		}

		r->out.infos[i].status = NT_STATUS_OK;
	}

	return NT_STATUS_OK;
}

/* include the generated boilerplate */
#include "librpc/gen_ndr/ndr_unixinfo_s.c"
