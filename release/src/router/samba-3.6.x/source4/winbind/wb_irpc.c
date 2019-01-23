/* 
   Unix SMB/CIFS implementation.
   Main winbindd irpc handlers

   Copyright (C) Stefan Metzmacher	2006
   
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
#include "winbind/wb_server.h"
#include "lib/messaging/irpc.h"
#include "libcli/composite/composite.h"
#include "librpc/gen_ndr/ndr_winbind.h"
#include "smbd/service_task.h"

struct wb_irpc_SamLogon_state {
	struct irpc_message *msg;
	struct winbind_SamLogon *req;
};

static void wb_irpc_SamLogon_callback(struct composite_context *ctx);

static NTSTATUS wb_irpc_SamLogon(struct irpc_message *msg, 
				 struct winbind_SamLogon *req)
{
	struct wbsrv_service *service = talloc_get_type(msg->private_data,
					struct wbsrv_service);
	struct wb_irpc_SamLogon_state *s;
	struct composite_context *ctx;

	DEBUG(5, ("wb_irpc_SamLogon called\n"));

	s = talloc(msg, struct wb_irpc_SamLogon_state);
	NT_STATUS_HAVE_NO_MEMORY(s);

	s->msg = msg;
	s->req = req;

	ctx = wb_sam_logon_send(msg, service, req);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = wb_irpc_SamLogon_callback;
	ctx->async.private_data = s;

	msg->defer_reply = true;
	return NT_STATUS_OK;
}

static void wb_irpc_SamLogon_callback(struct composite_context *ctx)
{
	struct wb_irpc_SamLogon_state *s = talloc_get_type(ctx->async.private_data,
					   struct wb_irpc_SamLogon_state);
	NTSTATUS status;

	DEBUG(5, ("wb_irpc_SamLogon_callback called\n"));

	status = wb_sam_logon_recv(ctx, s, s->req);

	irpc_send_reply(s->msg, status);
}

struct wb_irpc_DsrUpdateReadOnlyServerDnsRecords_state {
	struct irpc_message *msg;
	struct winbind_DsrUpdateReadOnlyServerDnsRecords *req;
};

static void wb_irpc_DsrUpdateReadOnlyServerDnsRecords_callback(struct composite_context *ctx);

static NTSTATUS wb_irpc_DsrUpdateReadOnlyServerDnsRecords(struct irpc_message *msg,
				 struct winbind_DsrUpdateReadOnlyServerDnsRecords *req)
{
	struct wbsrv_service *service = talloc_get_type(msg->private_data,
					struct wbsrv_service);
	struct wb_irpc_DsrUpdateReadOnlyServerDnsRecords_state *s;
	struct composite_context *ctx;

	DEBUG(5, ("wb_irpc_DsrUpdateReadOnlyServerDnsRecords called\n"));

	s = talloc(msg, struct wb_irpc_DsrUpdateReadOnlyServerDnsRecords_state);
	NT_STATUS_HAVE_NO_MEMORY(s);

	s->msg = msg;
	s->req = req;

	ctx = wb_update_rodc_dns_send(msg, service, req);
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	ctx->async.fn = wb_irpc_DsrUpdateReadOnlyServerDnsRecords_callback;
	ctx->async.private_data = s;

	msg->defer_reply = true;
	return NT_STATUS_OK;
}

static void wb_irpc_DsrUpdateReadOnlyServerDnsRecords_callback(struct composite_context *ctx)
{
	struct wb_irpc_DsrUpdateReadOnlyServerDnsRecords_state *s = talloc_get_type(ctx->async.private_data,
					   struct wb_irpc_DsrUpdateReadOnlyServerDnsRecords_state);
	NTSTATUS status;

	DEBUG(5, ("wb_irpc_DsrUpdateReadOnlyServerDnsRecords_callback called\n"));

	status = wb_update_rodc_dns_recv(ctx, s, s->req);

	irpc_send_reply(s->msg, status);
}

struct wb_irpc_get_idmap_state {
	struct irpc_message *msg;
	struct winbind_get_idmap *req;
	int level;
};

static void wb_irpc_get_idmap_callback(struct composite_context *ctx);

static NTSTATUS wb_irpc_get_idmap(struct irpc_message *msg,
				  struct winbind_get_idmap *req)
{
	struct wbsrv_service *service = talloc_get_type(msg->private_data,
					struct wbsrv_service);
	struct wb_irpc_get_idmap_state *s;
	struct composite_context *ctx = NULL;

	DEBUG(5, ("wb_irpc_get_idmap called\n"));

	s = talloc(msg, struct wb_irpc_get_idmap_state);
	NT_STATUS_HAVE_NO_MEMORY(s);

	s->msg = msg;
	s->req = req;
	s->level = req->in.level;

	switch(s->level) {
		case WINBIND_IDMAP_LEVEL_SIDS_TO_XIDS:
			ctx = wb_sids2xids_send(msg, service, req->in.count,
						req->in.ids);
			break;
		case WINBIND_IDMAP_LEVEL_XIDS_TO_SIDS:
			ctx = wb_xids2sids_send(msg, service, req->in.count,
						req->in.ids);
			break;
	}
	NT_STATUS_HAVE_NO_MEMORY(ctx);

	composite_continue(ctx, ctx, wb_irpc_get_idmap_callback, s);
	msg->defer_reply = true;

	return NT_STATUS_OK;
}

static void wb_irpc_get_idmap_callback(struct composite_context *ctx)
{
	struct wb_irpc_get_idmap_state *s;
	NTSTATUS status;

	DEBUG(5, ("wb_irpc_get_idmap_callback called\n"));

	s = talloc_get_type(ctx->async.private_data,
			    struct wb_irpc_get_idmap_state);

	switch(s->level) {
		case WINBIND_IDMAP_LEVEL_SIDS_TO_XIDS:
			status = wb_sids2xids_recv(ctx, &s->req->out.ids);
			break;
		case WINBIND_IDMAP_LEVEL_XIDS_TO_SIDS:
			status = wb_xids2sids_recv(ctx, &s->req->out.ids);
			break;
		default:
			status = NT_STATUS_INTERNAL_ERROR;
			break;
	}

	irpc_send_reply(s->msg, status);
}

NTSTATUS wbsrv_init_irpc(struct wbsrv_service *service)
{
	NTSTATUS status;

	irpc_add_name(service->task->msg_ctx, "winbind_server");

	status = IRPC_REGISTER(service->task->msg_ctx, winbind, WINBIND_SAMLOGON,
			       wb_irpc_SamLogon, service);
	NT_STATUS_NOT_OK_RETURN(status);

	status = IRPC_REGISTER(service->task->msg_ctx, winbind, WINBIND_DSRUPDATEREADONLYSERVERDNSRECORDS,
			       wb_irpc_DsrUpdateReadOnlyServerDnsRecords, service);
	NT_STATUS_NOT_OK_RETURN(status);

	status = IRPC_REGISTER(service->task->msg_ctx, winbind, WINBIND_GET_IDMAP,
			       wb_irpc_get_idmap, service);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}
