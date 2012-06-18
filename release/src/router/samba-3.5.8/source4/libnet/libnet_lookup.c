/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2005
   
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

/*
  a composite function for name resolving
*/

#include "includes.h"
#include "lib/events/events.h"
#include "libnet/libnet.h"
#include "libcli/composite/composite.h"
#include "auth/credentials/credentials.h"
#include "lib/messaging/messaging.h"
#include "lib/messaging/irpc.h"
#include "libcli/resolve/resolve.h"
#include "libcli/finddcs.h"
#include "libcli/security/security.h"
#include "librpc/gen_ndr/lsa.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"

#include "param/param.h"

struct lookup_state {
	struct nbt_name hostname;
	const char *address;
};


static void continue_name_resolved(struct composite_context *ctx);


/**
 * Sends asynchronous Lookup request
 *
 * @param io arguments and result of the call
 */

struct composite_context *libnet_Lookup_send(struct libnet_context *ctx,
					     struct libnet_Lookup *io)
{
	struct composite_context *c;
	struct lookup_state *s;
	struct composite_context *cresolve_req;
	struct resolve_context *resolve_ctx;

	/* allocate context and state structures */
	c = composite_create(ctx, ctx->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct lookup_state);
	if (composite_nomem(s, c)) return c;

	c->private_data	= s;

	if (io == NULL || io->in.hostname == NULL) {
		composite_error(c, NT_STATUS_INVALID_PARAMETER);
		return c;
	}

	/* parameters */
	s->hostname.name   = talloc_strdup(s, io->in.hostname);
	if (composite_nomem(s->hostname.name, c)) return c;

	s->hostname.type   = io->in.type;
	s->hostname.scope  = NULL;

	/* name resolution methods */
	if (io->in.resolve_ctx) {
		resolve_ctx = io->in.resolve_ctx;
	} else {
		resolve_ctx = ctx->resolve_ctx;
	}

	/* send resolve request */
	cresolve_req = resolve_name_send(resolve_ctx, s, &s->hostname, c->event_ctx);
	if (composite_nomem(cresolve_req, c)) return c;

	composite_continue(c, cresolve_req, continue_name_resolved, c);
	return c;
}


static void continue_name_resolved(struct composite_context *ctx)
{
	struct composite_context *c;
	struct lookup_state *s;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct lookup_state);

	c->status = resolve_name_recv(ctx, s, &s->address);
	
	composite_done(c);
}


/**
 * Waits for and receives results of asynchronous Lookup call
 *
 * @param c composite context returned by asynchronous Lookup call
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_Lookup_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
			    struct libnet_Lookup *io)
{
	NTSTATUS status;
	struct lookup_state *s;

	status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		s = talloc_get_type(c->private_data, struct lookup_state);

		io->out.address = (const char **)str_list_make_single(mem_ctx, s->address);
		NT_STATUS_HAVE_NO_MEMORY(io->out.address);
	}

	talloc_free(c);
	return status;
}


/**
 * Synchronous version of Lookup call
 *
 * @param mem_ctx memory context for the call
 * @param io arguments and results of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_Lookup(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
		       struct libnet_Lookup *io)
{
	struct composite_context *c = libnet_Lookup_send(ctx, io);
	return libnet_Lookup_recv(c, mem_ctx, io);
}


/*
 * Shortcut functions to find common types of name
 * (and skip nbt name type argument)
 */


/**
 * Sends asynchronous LookupHost request
 */
struct composite_context* libnet_LookupHost_send(struct libnet_context *ctx,
						 struct libnet_Lookup *io)
{
	io->in.type = NBT_NAME_SERVER;
	return libnet_Lookup_send(ctx, io);
}



/**
 * Synchronous version of LookupHost call
 */
NTSTATUS libnet_LookupHost(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			   struct libnet_Lookup *io)
{
	struct composite_context *c = libnet_LookupHost_send(ctx, io);
	return libnet_Lookup_recv(c, mem_ctx, io);
}


/**
 * Sends asynchronous LookupDCs request
 */
struct composite_context* libnet_LookupDCs_send(struct libnet_context *ctx,
						TALLOC_CTX *mem_ctx,
						struct libnet_LookupDCs *io)
{
	struct composite_context *c;
	struct messaging_context *msg_ctx = 
		messaging_client_init(mem_ctx, lp_messaging_path(mem_ctx, ctx->lp_ctx), 
				      lp_iconv_convenience(ctx->lp_ctx), ctx->event_ctx);

	c = finddcs_send(mem_ctx, lp_netbios_name(ctx->lp_ctx), lp_nbt_port(ctx->lp_ctx),
			 io->in.domain_name, io->in.name_type,
			 NULL, lp_iconv_convenience(ctx->lp_ctx), 
			 ctx->resolve_ctx, ctx->event_ctx, msg_ctx);
	return c;
}

/**
 * Waits for and receives results of asynchronous Lookup call
 *
 * @param c composite context returned by asynchronous Lookup call
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_LookupDCs_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
			       struct libnet_LookupDCs *io)
{
	NTSTATUS status;
	status = finddcs_recv(c, mem_ctx, &io->out.num_dcs, &io->out.dcs);
	if (!NT_STATUS_IS_OK(status)) {
		return status;
	}
	return status;
}


/**
 * Synchronous version of LookupDCs
 */
NTSTATUS libnet_LookupDCs(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			  struct libnet_LookupDCs *io)
{
	struct composite_context *c = libnet_LookupDCs_send(ctx, mem_ctx, io);
	return libnet_LookupDCs_recv(c, mem_ctx, io);
}


struct lookup_name_state {
	struct libnet_context *ctx;
	const char *name;
	uint32_t count;
	struct libnet_DomainOpen domopen;
	struct lsa_LookupNames lookup;
	struct lsa_TransSidArray sids;
	struct lsa_String *names;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg *);
};


static bool prepare_lookup_params(struct libnet_context *ctx,
				  struct composite_context *c,
				  struct lookup_name_state *s);
static void continue_lookup_name(struct composite_context *ctx);
static void continue_name_found(struct rpc_request *req);


struct composite_context* libnet_LookupName_send(struct libnet_context *ctx,
						 TALLOC_CTX *mem_ctx,
						 struct libnet_LookupName *io,
						 void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct lookup_name_state *s;
	struct rpc_request *lookup_req;
	bool prereq_met = false;

	c = composite_create(mem_ctx, ctx->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct lookup_name_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;
	
	s->name = talloc_strdup(c, io->in.name);
	s->monitor_fn = monitor;
	s->ctx = ctx;

	prereq_met = lsa_domain_opened(ctx, io->in.domain_name, &c, &s->domopen,
				       continue_lookup_name, monitor);
	if (!prereq_met) return c;

	if (!prepare_lookup_params(ctx, c, s)) return c;

	lookup_req = dcerpc_lsa_LookupNames_send(ctx->lsa.pipe, c, &s->lookup);
	if (composite_nomem(lookup_req, c)) return c;

	composite_continue_rpc(c, lookup_req, continue_name_found, c);
	return c;
}


static bool prepare_lookup_params(struct libnet_context *ctx,
				  struct composite_context *c,
				  struct lookup_name_state *s)
{
	const int single_name = 1;

	s->sids.count = 0;
	s->sids.sids  = NULL;
	
	s->names = talloc_array(ctx, struct lsa_String, single_name);
	if (composite_nomem(s->names, c)) return false;
	s->names[0].string = s->name;
	
	s->lookup.in.handle    = &ctx->lsa.handle;
	s->lookup.in.num_names = single_name;
	s->lookup.in.names     = s->names;
	s->lookup.in.sids      = &s->sids;
	s->lookup.in.level     = 1;
	s->lookup.in.count     = &s->count;
	s->lookup.out.count    = &s->count;
	s->lookup.out.sids     = &s->sids;
	s->lookup.out.domains  = talloc_zero(ctx, struct lsa_RefDomainList *);
	if (composite_nomem(s->lookup.out.domains, c)) return false;
	
	return true;
}


static void continue_lookup_name(struct composite_context *ctx)
{
	struct composite_context *c;
	struct lookup_name_state *s;
	struct rpc_request *lookup_req;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct lookup_name_state);

	c->status = libnet_DomainOpen_recv(ctx, s->ctx, c, &s->domopen);
	if (!composite_is_ok(c)) return;
	
	if (!prepare_lookup_params(s->ctx, c, s)) return;

	lookup_req = dcerpc_lsa_LookupNames_send(s->ctx->lsa.pipe, c, &s->lookup);
	if (composite_nomem(lookup_req, c)) return;
	
	composite_continue_rpc(c, lookup_req, continue_name_found, c);
}


static void continue_name_found(struct rpc_request *req)
{
	struct composite_context *c;
	struct lookup_name_state *s;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct lookup_name_state);

	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	c->status = s->lookup.out.result;
	if (!composite_is_ok(c)) return;

	composite_done(c);
}


NTSTATUS libnet_LookupName_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
				struct libnet_LookupName *io)
{
	NTSTATUS status;
	struct lookup_name_state *s;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		s = talloc_get_type(c->private_data, struct lookup_name_state);

		io->out.rid = 0;
		io->out.sid = NULL;
		io->out.sidstr = NULL;

		if (*s->lookup.out.count > 0) {
			struct lsa_RefDomainList *domains = *s->lookup.out.domains;
			struct lsa_TransSidArray *sids = s->lookup.out.sids;

			if (domains == NULL || sids == NULL) {
				status = NT_STATUS_UNSUCCESSFUL;
				io->out.error_string = talloc_asprintf(mem_ctx, "Error: %s", nt_errstr(status));
				goto done;
			}

			if (sids->count > 0) {
				io->out.rid        = sids->sids[0].rid;
				io->out.sid_type   = sids->sids[0].sid_type;
				if (domains->count > 0) {
					io->out.sid = dom_sid_add_rid(mem_ctx, domains->domains[0].sid, io->out.rid);
					NT_STATUS_HAVE_NO_MEMORY(io->out.sid);
					io->out.sidstr = dom_sid_string(mem_ctx, io->out.sid);
					NT_STATUS_HAVE_NO_MEMORY(io->out.sidstr);
				}
			}
		}

		io->out.error_string = talloc_strdup(mem_ctx, "Success");

	} else if (!NT_STATUS_IS_OK(status)) {
		io->out.error_string = talloc_asprintf(mem_ctx, "Error: %s", nt_errstr(status));
	}

done:
	talloc_free(c);
	return status;
}


NTSTATUS libnet_LookupName(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			   struct libnet_LookupName *io)
{
	struct composite_context *c;
	
	c = libnet_LookupName_send(ctx, mem_ctx, io, NULL);
	return libnet_LookupName_recv(c, mem_ctx, io);
}
