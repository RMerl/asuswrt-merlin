/*
   Unix SMB/CIFS implementation.

   Do a netr_DsrUpdateReadOnlyServerDnsRecords to a remote DC

   Copyright (C) Andrew Bartlett 2010
   Copyright (C) Andrew Tridgell 2010

   based heavily on wb_sam_logon.c which is copyright:

   Copyright (C) Volker Lendecke 2005
   Copyright (C) Andrew Bartlett 2005
   Copyright (C) Stefan Metzmacher 2006

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
#include "auth/credentials/credentials.h"
#include "libcli/auth/libcli_auth.h"
#include "librpc/gen_ndr/ndr_netlogon_c.h"
#include "librpc/gen_ndr/winbind.h"

struct wb_update_rodc_dns_state {
	struct composite_context *ctx;

	struct winbind_DsrUpdateReadOnlyServerDnsRecords *req;

        struct netlogon_creds_CredentialState *creds_state;
        struct netr_Authenticator auth1, auth2;

	TALLOC_CTX *r_mem_ctx;
        struct netr_DsrUpdateReadOnlyServerDnsRecords r;
};

static void wb_update_rodc_dns_recv_domain(struct composite_context *ctx);
static void wb_update_rodc_dns_recv_response(struct tevent_req *subreq);

/*
    Find the connection to the DC (or find an existing connection)
*/
struct composite_context *wb_update_rodc_dns_send(TALLOC_CTX *mem_ctx,
					    struct wbsrv_service *service,
					    struct winbind_DsrUpdateReadOnlyServerDnsRecords *req)
{
	struct composite_context *c, *creq;
	struct wb_update_rodc_dns_state *s;

	c = composite_create(mem_ctx, service->task->event_ctx);
	if (!c) return NULL;

	s = talloc_zero(c, struct wb_update_rodc_dns_state);
	if (composite_nomem(s, c)) return c;
	s->ctx = c;
	s->req = req;

	c->private_data = s;

	creq = wb_sid2domain_send(s, service, service->primary_sid);
	composite_continue(c, creq, wb_update_rodc_dns_recv_domain, s);
	return c;
}

/*
    Having finished making the connection to the DC
    Send of a DsrUpdateReadOnlyServerDnsRecords request to authenticate a user.
*/
static void wb_update_rodc_dns_recv_domain(struct composite_context *creq)
{
	struct wb_update_rodc_dns_state *s = talloc_get_type(creq->async.private_data,
				       struct wb_update_rodc_dns_state);
	struct wbsrv_domain *domain;
	struct tevent_req *subreq;

	s->ctx->status = wb_sid2domain_recv(creq, &domain);
	if (!composite_is_ok(s->ctx)) return;

	s->creds_state = cli_credentials_get_netlogon_creds(domain->libnet_ctx->cred);
	netlogon_creds_client_authenticator(s->creds_state, &s->auth1);

	s->r.in.server_name = talloc_asprintf(s, "\\\\%s",
			      dcerpc_server_name(domain->netlogon_pipe));
	if (composite_nomem(s->r.in.server_name, s->ctx)) return;

	s->r.in.computer_name = cli_credentials_get_workstation(domain->libnet_ctx->cred);
	s->r.in.credential = &s->auth1;
	s->r.out.return_authenticator = &s->auth2;
	s->r.in.site_name = s->req->in.site_name;
	s->r.in.dns_ttl = s->req->in.dns_ttl;
	s->r.in.dns_names = s->req->in.dns_names;
	s->r.out.dns_names = s->req->in.dns_names;

	/*
	 * use a new talloc context for the DsrUpdateReadOnlyServerDnsRecords call
	 * because then we can just to a talloc_steal on this context
	 * in the final _recv() function to give the caller all the content of
	 * the s->r.out.dns_names
	 */
	s->r_mem_ctx = talloc_new(s);
	if (composite_nomem(s->r_mem_ctx, s->ctx)) return;

	subreq = dcerpc_netr_DsrUpdateReadOnlyServerDnsRecords_r_send(s,
						  s->ctx->event_ctx,
						  domain->netlogon_pipe->binding_handle,
						  &s->r);
	if (composite_nomem(subreq, s->ctx)) return;
	tevent_req_set_callback(subreq, wb_update_rodc_dns_recv_response, s);
}

/*
   NTLM Authentication

   Check the DsrUpdateReadOnlyServerDnsRecords reply and decrypt the session keys
*/
static void wb_update_rodc_dns_recv_response(struct tevent_req *subreq)
{
	struct wb_update_rodc_dns_state *s = tevent_req_callback_data(subreq,
				       struct wb_update_rodc_dns_state);

	s->ctx->status = dcerpc_netr_DsrUpdateReadOnlyServerDnsRecords_r_recv(subreq, s->r_mem_ctx);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(s->ctx)) return;

	s->ctx->status = s->r.out.result;
	if (!composite_is_ok(s->ctx)) return;

	if ((s->r.out.return_authenticator == NULL) ||
	    (!netlogon_creds_client_check(s->creds_state,
					  &s->r.out.return_authenticator->cred))) {
		DEBUG(0, ("Credentials check failed!\n"));
		composite_error(s->ctx, NT_STATUS_ACCESS_DENIED);
		return;
	}

	composite_done(s->ctx);
}

NTSTATUS wb_update_rodc_dns_recv(struct composite_context *c,
			   TALLOC_CTX *mem_ctx,
			   struct winbind_DsrUpdateReadOnlyServerDnsRecords *req)
{
	struct wb_update_rodc_dns_state *s = talloc_get_type(c->private_data,
				       struct wb_update_rodc_dns_state);
	NTSTATUS status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		talloc_steal(mem_ctx, s->r_mem_ctx);
		req->out.dns_names	= s->r.out.dns_names;
	}

	talloc_free(s);
	return status;
}
