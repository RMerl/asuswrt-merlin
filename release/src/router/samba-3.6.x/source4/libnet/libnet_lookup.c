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
#include "libnet/libnet.h"
#include "libcli/composite/composite.h"
#include "auth/credentials/credentials.h"
#include "libcli/resolve/resolve.h"
#include "libcli/finddc.h"
#include "libcli/security/security.h"
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
struct tevent_req *libnet_LookupDCs_send(struct libnet_context *ctx,
					 TALLOC_CTX *mem_ctx,
					 struct libnet_LookupDCs *io)
{
	struct tevent_req *req;
	struct finddcs finddcs_io;

	ZERO_STRUCT(finddcs_io);

	if (strcasecmp_m(io->in.domain_name, lpcfg_workgroup(ctx->lp_ctx)) == 0) {
		finddcs_io.in.domain_name = lpcfg_dnsdomain(ctx->lp_ctx);
	} else {
		finddcs_io.in.domain_name = io->in.domain_name;
	}
	finddcs_io.in.minimum_dc_flags = NBT_SERVER_LDAP | NBT_SERVER_DS | NBT_SERVER_WRITABLE;
	finddcs_io.in.server_address = ctx->server_address;

	req = finddcs_cldap_send(mem_ctx, &finddcs_io, ctx->resolve_ctx, ctx->event_ctx);
	return req;
}

/**
 * Waits for and receives results of asynchronous Lookup call
 *
 * @param c composite context returned by asynchronous Lookup call
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_LookupDCs_recv(struct tevent_req *req, TALLOC_CTX *mem_ctx,
			       struct libnet_LookupDCs *io)
{
	NTSTATUS status;
	struct finddcs finddcs_io;
	status = finddcs_cldap_recv(req, mem_ctx, &finddcs_io);
	talloc_free(req);
	io->out.num_dcs = 1;
	io->out.dcs = talloc(mem_ctx, struct nbt_dc_name);
	NT_STATUS_HAVE_NO_MEMORY(io->out.dcs);
	io->out.dcs[0].address = finddcs_io.out.address;
	io->out.dcs[0].name = finddcs_io.out.netlogon.data.nt5_ex.pdc_dns_name;
	return status;
}


/**
 * Synchronous version of LookupDCs
 */
NTSTATUS libnet_LookupDCs(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			  struct libnet_LookupDCs *io)
{
	struct tevent_req *req = libnet_LookupDCs_send(ctx, mem_ctx, io);
	return libnet_LookupDCs_recv(req, mem_ctx, io);
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
static void continue_name_found(struct tevent_req *subreq);


struct composite_context* libnet_LookupName_send(struct libnet_context *ctx,
						 TALLOC_CTX *mem_ctx,
						 struct libnet_LookupName *io,
						 void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct lookup_name_state *s;
	struct tevent_req *subreq;
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

	subreq = dcerpc_lsa_LookupNames_r_send(s, c->event_ctx,
					       ctx->lsa.pipe->binding_handle,
					       &s->lookup);
	if (composite_nomem(subreq, c)) return c;

	tevent_req_set_callback(subreq, continue_name_found, c);
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
	struct tevent_req *subreq;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct lookup_name_state);

	c->status = libnet_DomainOpen_recv(ctx, s->ctx, c, &s->domopen);
	if (!composite_is_ok(c)) return;
	
	if (!prepare_lookup_params(s->ctx, c, s)) return;

	subreq = dcerpc_lsa_LookupNames_r_send(s, c->event_ctx,
					       s->ctx->lsa.pipe->binding_handle,
					       &s->lookup);
	if (composite_nomem(subreq, c)) return;
	
	tevent_req_set_callback(subreq, continue_name_found, c);
}


static void continue_name_found(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct lookup_name_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct lookup_name_state);

	c->status = dcerpc_lsa_LookupNames_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	c->status = s->lookup.out.result;
	if (!composite_is_ok(c)) return;

	if (s->lookup.out.sids->count != s->lookup.in.num_names) {
		composite_error(c, NT_STATUS_INVALID_NETWORK_RESPONSE);
		return;
	}

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
