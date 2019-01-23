/* 
   Unix SMB/CIFS implementation.

   Get a struct wb_dom_info for a domain using DNS, netbios, possibly cldap
   etc.

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
#include "libcli/resolve/resolve.h"
#include "libcli/security/security.h"
#include "winbind/wb_server.h"
#include "smbd/service_task.h"
#include "libcli/finddc.h"

struct get_dom_info_state {
	struct composite_context *ctx;
	struct wb_dom_info *info;
};

static void get_dom_info_recv_addrs(struct tevent_req *req);

struct composite_context *wb_get_dom_info_send(TALLOC_CTX *mem_ctx,
					       struct wbsrv_service *service,
					       const char *domain_name,
					       const char *dns_domain_name,
					       const struct dom_sid *sid)
{
	struct composite_context *result;
	struct tevent_req *req;
	struct get_dom_info_state *state;
	struct dom_sid *dom_sid;
	struct finddcs finddcs_io;

	DEBUG(5, ("wb_get_dom_info_send called\n"));
	result = composite_create(mem_ctx, service->task->event_ctx);
	if (result == NULL) goto failed;

	state = talloc(result, struct get_dom_info_state);
	if (state == NULL) goto failed;
	state->ctx = result;
	result->private_data = state;

	state->info = talloc_zero(state, struct wb_dom_info);
	if (state->info == NULL) goto failed;

	state->info->name = talloc_strdup(state->info, domain_name);
	if (state->info->name == NULL) goto failed;

	state->info->sid = dom_sid_dup(state->info, sid);
	if (state->info->sid == NULL) goto failed;

	dom_sid = dom_sid_dup(mem_ctx, sid);
	if (dom_sid == NULL) goto failed;

	ZERO_STRUCT(finddcs_io);
	finddcs_io.in.domain_name      = dns_domain_name;
	finddcs_io.in.domain_sid       = dom_sid;
	finddcs_io.in.minimum_dc_flags = NBT_SERVER_LDAP | NBT_SERVER_DS;
	if (service->sec_channel_type == SEC_CHAN_RODC) {
		finddcs_io.in.minimum_dc_flags |= NBT_SERVER_WRITABLE;
	}

	req = finddcs_cldap_send(mem_ctx, &finddcs_io,
				 lpcfg_resolve_context(service->task->lp_ctx),
				 service->task->event_ctx);
	if (req == NULL) goto failed;

	tevent_req_set_callback(req, get_dom_info_recv_addrs, state);

	return result;

 failed:
	talloc_free(result);
	return NULL;
}

static void get_dom_info_recv_addrs(struct tevent_req *req)
{
	struct get_dom_info_state *state = tevent_req_callback_data(req, struct get_dom_info_state);
	struct finddcs finddcs_io;

	state->info->dc = talloc(state->info, struct nbt_dc_name);

	state->ctx->status = finddcs_cldap_recv(req, state->info, &finddcs_io);
	if (!composite_is_ok(state->ctx)) return;

	if (finddcs_io.out.netlogon.ntver != NETLOGON_NT_VERSION_5EX) {
		/* the finddcs code should have mapped the response to
		   the type we want */
		DEBUG(0,(__location__ ": unexpected ntver 0x%08x in finddcs response\n",
			 finddcs_io.out.netlogon.ntver));
		state->ctx->status = NT_STATUS_UNEXPECTED_NETWORK_ERROR;
		if (!composite_is_ok(state->ctx)) return;
	}

	state->info->dc->address = finddcs_io.out.address;
	state->info->dc->name    = finddcs_io.out.netlogon.data.nt5_ex.pdc_dns_name;

	composite_done(state->ctx);
}

NTSTATUS wb_get_dom_info_recv(struct composite_context *ctx,
			      TALLOC_CTX *mem_ctx,
			      struct wb_dom_info **result)
{
	NTSTATUS status = composite_wait(ctx);
	if (NT_STATUS_IS_OK(status)) {
		struct get_dom_info_state *state =
			talloc_get_type(ctx->private_data,
					struct get_dom_info_state);
		*result = talloc_steal(mem_ctx, state->info);
	}
	talloc_free(ctx);
	return status;
}

NTSTATUS wb_get_dom_info(TALLOC_CTX *mem_ctx,
			 struct wbsrv_service *service,
			 const char *domain_name,
			 const char *dns_domain_name,
			 const struct dom_sid *sid,
			 struct wb_dom_info **result)
{
	struct composite_context *ctx =
		wb_get_dom_info_send(mem_ctx, service, domain_name, dns_domain_name, sid);
	return wb_get_dom_info_recv(ctx, mem_ctx, result);
}
