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
  a composite function for domain handling on samr and lsa pipes
*/

#include "includes.h"
#include "libcli/composite/composite.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"


struct domain_open_samr_state {
	struct libnet_context     *ctx;
	struct dcerpc_pipe        *pipe;
	struct libnet_RpcConnect  rpcconn;
	struct samr_Connect       connect;
	struct samr_LookupDomain  lookup;
	struct samr_OpenDomain    open;
	struct samr_Close         close;
	struct lsa_String         domain_name;
	uint32_t                  access_mask;
	struct policy_handle      connect_handle;
	struct policy_handle      domain_handle;
	struct dom_sid2           *domain_sid;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg*);
};


static void continue_domain_open_close(struct tevent_req *subreq);
static void continue_domain_open_connect(struct tevent_req *subreq);
static void continue_domain_open_lookup(struct tevent_req *subreq);
static void continue_domain_open_open(struct tevent_req *subreq);


/**
 * Stage 0.5 (optional): Connect to samr rpc pipe
 */
static void continue_domain_open_rpc_connect(struct composite_context *ctx)
{
	struct composite_context *c;
	struct domain_open_samr_state *s;
	struct tevent_req *subreq;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_open_samr_state);

	c->status = libnet_RpcConnect_recv(ctx, s->ctx, c, &s->rpcconn);
	if (!composite_is_ok(c)) return;

	s->pipe = s->rpcconn.out.dcerpc_pipe;

	/* preparing parameters for samr_Connect rpc call */
	s->connect.in.system_name      = 0;
	s->connect.in.access_mask      = s->access_mask;
	s->connect.out.connect_handle  = &s->connect_handle;

	/* send request */
	subreq = dcerpc_samr_Connect_r_send(s, c->event_ctx,
					    s->pipe->binding_handle,
					    &s->connect);
	if (composite_nomem(subreq, c)) return;

	/* callback handler */
	tevent_req_set_callback(subreq, continue_domain_open_connect, c);
}


/**
 * Stage 0.5 (optional): Close existing (in libnet context) domain
 * handle
 */
static void continue_domain_open_close(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_open_samr_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_open_samr_state);

	/* receive samr_Close reply */
	c->status = dcerpc_samr_Close_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (s->monitor_fn) {
		struct monitor_msg msg;
		
		msg.type = mon_SamrClose;
		msg.data = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}

	/* reset domain handle and associated data in libnet_context */
	s->ctx->samr.name        = NULL;
	s->ctx->samr.access_mask = 0;
	ZERO_STRUCT(s->ctx->samr.handle);

	/* preparing parameters for samr_Connect rpc call */
	s->connect.in.system_name      = 0;
	s->connect.in.access_mask      = s->access_mask;
	s->connect.out.connect_handle  = &s->connect_handle;
	
	/* send request */
	subreq = dcerpc_samr_Connect_r_send(s, c->event_ctx,
					    s->pipe->binding_handle,
					    &s->connect);
	if (composite_nomem(subreq, c)) return;

	/* callback handler */
	tevent_req_set_callback(subreq, continue_domain_open_connect, c);
}


/**
 * Stage 1: Connect to SAM server.
 */
static void continue_domain_open_connect(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_open_samr_state *s;
	struct samr_LookupDomain *r;
	
	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_open_samr_state);

	/* receive samr_Connect reply */
	c->status = dcerpc_samr_Connect_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (s->monitor_fn) {
		struct monitor_msg msg;

		msg.type = mon_SamrConnect;
		msg.data = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}

	r = &s->lookup;

	/* prepare for samr_LookupDomain call */
	r->in.connect_handle = &s->connect_handle;
	r->in.domain_name    = &s->domain_name;
	r->out.sid           = talloc(s, struct dom_sid2 *);
	if (composite_nomem(r->out.sid, c)) return;

	subreq = dcerpc_samr_LookupDomain_r_send(s, c->event_ctx,
						 s->pipe->binding_handle,
						 r);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_domain_open_lookup, c);
}


/**
 * Stage 2: Lookup domain by name.
 */
static void continue_domain_open_lookup(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_open_samr_state *s;
	struct samr_OpenDomain *r;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_open_samr_state);
	
	/* receive samr_LookupDomain reply */
	c->status = dcerpc_samr_LookupDomain_r_recv(subreq, s);
	TALLOC_FREE(subreq);

	if (s->monitor_fn) {
		struct monitor_msg msg;
		struct msg_rpc_lookup_domain data;

		data.domain_name = s->domain_name.string;

		msg.type = mon_SamrLookupDomain;
		msg.data = (void*)&data;
		msg.data_size = sizeof(data);
		s->monitor_fn(&msg);
	}

	r = &s->open;

	/* check the rpc layer status */
	if (!composite_is_ok(c));

	/* check the rpc call itself status */
	if (!NT_STATUS_IS_OK(s->lookup.out.result)) {
		composite_error(c, s->lookup.out.result);
		return;
	}

	/* prepare for samr_OpenDomain call */
	r->in.connect_handle = &s->connect_handle;
	r->in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
	r->in.sid            = *s->lookup.out.sid;
	r->out.domain_handle = &s->domain_handle;

	subreq = dcerpc_samr_OpenDomain_r_send(s, c->event_ctx,
					       s->pipe->binding_handle,
					       r);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_domain_open_open, c);
}


/*
 * Stage 3: Open domain.
 */
static void continue_domain_open_open(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_open_samr_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_open_samr_state);

	/* receive samr_OpenDomain reply */
	c->status = dcerpc_samr_OpenDomain_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (s->monitor_fn) {
		struct monitor_msg msg;
		
		msg.type = mon_SamrOpenDomain;
		msg.data = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}

	composite_done(c);
}


/**
 * Sends asynchronous DomainOpenSamr request
 *
 * @param ctx initialised libnet context
 * @param io arguments and results of the call
 * @param monitor pointer to monitor function that is passed monitor message
 */

struct composite_context *libnet_DomainOpenSamr_send(struct libnet_context *ctx,
						     struct libnet_DomainOpen *io,
						     void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct domain_open_samr_state *s;
	struct composite_context *rpcconn_req;
	struct tevent_req *subreq;

	c = composite_create(ctx, ctx->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct domain_open_samr_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;
	s->monitor_fn   = monitor;

	s->ctx                 = ctx;
	s->pipe                = ctx->samr.pipe;
	s->access_mask         = io->in.access_mask;
	s->domain_name.string  = talloc_strdup(c, io->in.domain_name);

	/* check, if there's samr pipe opened already, before opening a domain */
	if (ctx->samr.pipe == NULL) {

		/* attempting to connect a domain controller */
		s->rpcconn.level           = LIBNET_RPC_CONNECT_DC;
		s->rpcconn.in.name         = io->in.domain_name;
		s->rpcconn.in.dcerpc_iface = &ndr_table_samr;
		
		/* send rpc pipe connect request */
		rpcconn_req = libnet_RpcConnect_send(ctx, c, &s->rpcconn, s->monitor_fn);
		if (composite_nomem(rpcconn_req, c)) return c;

		composite_continue(c, rpcconn_req, continue_domain_open_rpc_connect, c);
		return c;
	}

	/* libnet context's domain handle is not empty, so check out what
	   was opened first, before doing anything */
	if (!policy_handle_empty(&ctx->samr.handle)) {
		if (strequal(ctx->samr.name, io->in.domain_name) &&
		    ctx->samr.access_mask == io->in.access_mask) {

			/* this domain is already opened */
			composite_done(c);
			return c;

		} else {
			/* another domain or access rights have been
			   requested - close the existing handle first */
			s->close.in.handle = &ctx->samr.handle;

			/* send request to close domain handle */
			subreq = dcerpc_samr_Close_r_send(s, c->event_ctx,
							  s->pipe->binding_handle,
							  &s->close);
			if (composite_nomem(subreq, c)) return c;

			/* callback handler */
			tevent_req_set_callback(subreq, continue_domain_open_close, c);
			return c;
		}
	}

	/* preparing parameters for samr_Connect rpc call */
	s->connect.in.system_name      = 0;
	s->connect.in.access_mask      = s->access_mask;
	s->connect.out.connect_handle  = &s->connect_handle;
	
	/* send request */
	subreq = dcerpc_samr_Connect_r_send(s, c->event_ctx,
					    s->pipe->binding_handle,
					    &s->connect);
	if (composite_nomem(subreq, c)) return c;

	/* callback handler */
	tevent_req_set_callback(subreq, continue_domain_open_connect, c);
	return c;
}


/**
 * Waits for and receives result of asynchronous DomainOpenSamr call
 * 
 * @param c composite context returned by asynchronous DomainOpen call
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_DomainOpenSamr_recv(struct composite_context *c, struct libnet_context *ctx,
				    TALLOC_CTX *mem_ctx, struct libnet_DomainOpen *io)
{
	NTSTATUS status;
	struct domain_open_samr_state *s;

	/* wait for results of sending request */
	status = composite_wait(c);
	
	if (NT_STATUS_IS_OK(status) && io) {
		s = talloc_get_type(c->private_data, struct domain_open_samr_state);
		io->out.domain_handle = s->domain_handle;

		/* store the resulting handle and related data for use by other
		   libnet functions */
		ctx->samr.connect_handle = s->connect_handle;
		ctx->samr.handle      = s->domain_handle;
		ctx->samr.sid         = talloc_steal(ctx, *s->lookup.out.sid);
		ctx->samr.name        = talloc_steal(ctx, s->domain_name.string);
		ctx->samr.access_mask = s->access_mask;
	}

	talloc_free(c);
	return status;
}


struct domain_open_lsa_state {
	const char *name;
	uint32_t access_mask;
	struct libnet_context *ctx;
	struct libnet_RpcConnect rpcconn;
	struct lsa_OpenPolicy2   openpol;
	struct policy_handle handle;
	struct dcerpc_pipe *pipe;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg*);
};


static void continue_rpc_connect_lsa(struct composite_context *ctx);
static void continue_lsa_policy_open(struct tevent_req *subreq);


/**
 * Sends asynchronous DomainOpenLsa request
 *
 * @param ctx initialised libnet context
 * @param io arguments and results of the call
 * @param monitor pointer to monitor function that is passed monitor message
 */

struct composite_context* libnet_DomainOpenLsa_send(struct libnet_context *ctx,
						    struct libnet_DomainOpen *io,
						    void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct domain_open_lsa_state *s;
	struct composite_context *rpcconn_req;
	struct tevent_req *subreq;
	struct lsa_QosInfo *qos;

	/* create composite context and state */
	c = composite_create(ctx, ctx->event_ctx);
	if (c == NULL) return c;

	s = talloc_zero(c, struct domain_open_lsa_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;

	/* store arguments in the state structure */
	s->name         = talloc_strdup(c, io->in.domain_name);
	s->access_mask  = io->in.access_mask;
	s->ctx          = ctx;

	/* check, if there's lsa pipe opened already, before opening a handle */
	if (ctx->lsa.pipe == NULL) {

		ZERO_STRUCT(s->rpcconn);

		/* attempting to connect a domain controller */
		s->rpcconn.level           = LIBNET_RPC_CONNECT_DC;
		s->rpcconn.in.name         = talloc_strdup(c, io->in.domain_name);
		s->rpcconn.in.dcerpc_iface = &ndr_table_lsarpc;
		
		/* send rpc pipe connect request */
		rpcconn_req = libnet_RpcConnect_send(ctx, c, &s->rpcconn, s->monitor_fn);
		if (composite_nomem(rpcconn_req, c)) return c;

		composite_continue(c, rpcconn_req, continue_rpc_connect_lsa, c);
		return c;
	}

	s->pipe = ctx->lsa.pipe;

	/* preparing parameters for lsa_OpenPolicy2 rpc call */
	s->openpol.in.system_name = s->name;
	s->openpol.in.access_mask = s->access_mask;
	s->openpol.in.attr        = talloc_zero(c, struct lsa_ObjectAttribute);

	qos = talloc_zero(c, struct lsa_QosInfo);
	qos->len                 = 0;
	qos->impersonation_level = 2;
	qos->context_mode        = 1;
	qos->effective_only      = 0;

	s->openpol.in.attr->sec_qos = qos;
	s->openpol.out.handle       = &s->handle;
	
	/* send rpc request */
	subreq = dcerpc_lsa_OpenPolicy2_r_send(s, c->event_ctx,
					       s->pipe->binding_handle,
					       &s->openpol);
	if (composite_nomem(subreq, c)) return c;

	tevent_req_set_callback(subreq, continue_lsa_policy_open, c);
	return c;
}


/*
  Stage 0.5 (optional): Rpc pipe connected, send lsa open policy request
 */
static void continue_rpc_connect_lsa(struct composite_context *ctx)
{
	struct composite_context *c;
	struct domain_open_lsa_state *s;
	struct lsa_QosInfo *qos;
	struct tevent_req *subreq;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_open_lsa_state);

	/* receive rpc connection */
	c->status = libnet_RpcConnect_recv(ctx, s->ctx, c, &s->rpcconn);
	if (!composite_is_ok(c)) return;

	/* RpcConnect function leaves the pipe in libnet context,
	   so get it from there */
	s->pipe = s->ctx->lsa.pipe;

	/* prepare lsa_OpenPolicy2 call */
	s->openpol.in.system_name = s->name;
	s->openpol.in.access_mask = s->access_mask;
	s->openpol.in.attr        = talloc_zero(c, struct lsa_ObjectAttribute);

	qos = talloc_zero(c, struct lsa_QosInfo);
	qos->len                 = 0;
	qos->impersonation_level = 2;
	qos->context_mode        = 1;
	qos->effective_only      = 0;

	s->openpol.in.attr->sec_qos = qos;
	s->openpol.out.handle       = &s->handle;

	/* send rpc request */
	subreq = dcerpc_lsa_OpenPolicy2_r_send(s, c->event_ctx,
					       s->pipe->binding_handle,
					       &s->openpol);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_lsa_policy_open, c);
}


/*
  Stage 1: Lsa policy opened - we're done, if successfully
 */
static void continue_lsa_policy_open(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_open_lsa_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_open_lsa_state);

	c->status = dcerpc_lsa_OpenPolicy2_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (s->monitor_fn) {
		struct monitor_msg msg;
		
		msg.type      = mon_LsaOpenPolicy;
		msg.data      = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}

	composite_done(c);
}


/**
 * Receives result of asynchronous DomainOpenLsa call
 *
 * @param c composite context returned by asynchronous DomainOpenLsa call
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_DomainOpenLsa_recv(struct composite_context *c, struct libnet_context *ctx,
				   TALLOC_CTX *mem_ctx, struct libnet_DomainOpen *io)
{
	NTSTATUS status;
	struct domain_open_lsa_state *s;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status) && io) {
		/* everything went fine - get the results and
		   return the error string */
		s = talloc_get_type(c->private_data, struct domain_open_lsa_state);
		io->out.domain_handle = s->handle;

		ctx->lsa.handle      = s->handle;
		ctx->lsa.name        = talloc_steal(ctx, s->name);
		ctx->lsa.access_mask = s->access_mask;
		
		io->out.error_string = talloc_strdup(mem_ctx, "Success");

	} else if (!NT_STATUS_IS_OK(status)) {
		/* there was an error, so provide nt status code description */
		io->out.error_string = talloc_asprintf(mem_ctx,
						       "Failed to open domain: %s",
						       nt_errstr(status));
	}

	talloc_free(c);
	return status;
}


/**
 * Sends a request to open a domain in desired service
 *
 * @param ctx initalised libnet context
 * @param io arguments and results of the call
 * @param monitor pointer to monitor function that is passed monitor message
 */

struct composite_context* libnet_DomainOpen_send(struct libnet_context *ctx,
						 struct libnet_DomainOpen *io,
						 void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;

	switch (io->in.type) {
	case DOMAIN_LSA:
		/* reques to open a policy handle on \pipe\lsarpc */
		c = libnet_DomainOpenLsa_send(ctx, io, monitor);
		break;

	case DOMAIN_SAMR:
	default:
		/* request to open a domain policy handle on \pipe\samr */
		c = libnet_DomainOpenSamr_send(ctx, io, monitor);
		break;
	}

	return c;
}


/**
 * Receive result of domain open request
 *
 * @param c composite context returned by DomainOpen_send function
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of the call
 * @param io results and arguments of the call
 */

NTSTATUS libnet_DomainOpen_recv(struct composite_context *c, struct libnet_context *ctx,
				TALLOC_CTX *mem_ctx, struct libnet_DomainOpen *io)
{
	NTSTATUS status;

	switch (io->in.type) {
	case DOMAIN_LSA:
		status = libnet_DomainOpenLsa_recv(c, ctx, mem_ctx, io);
		break;

	case DOMAIN_SAMR:
	default:
		status = libnet_DomainOpenSamr_recv(c, ctx, mem_ctx, io);
		break;
	}
	
	return status;
}


/**
 * Synchronous version of DomainOpen call
 *
 * @param ctx initialised libnet context
 * @param mem_ctx memory context for the call
 * @param io arguments and results of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_DomainOpen(struct libnet_context *ctx,
			   TALLOC_CTX *mem_ctx,
			   struct libnet_DomainOpen *io)
{
	struct composite_context *c = libnet_DomainOpen_send(ctx, io, NULL);
	return libnet_DomainOpen_recv(c, ctx, mem_ctx, io);
}


struct domain_close_lsa_state {
	struct dcerpc_pipe *pipe;
	struct lsa_Close close;
	struct policy_handle handle;

	void (*monitor_fn)(struct monitor_msg*);
};


static void continue_lsa_close(struct tevent_req *subreq);


struct composite_context* libnet_DomainCloseLsa_send(struct libnet_context *ctx,
						     struct libnet_DomainClose *io,
						     void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct domain_close_lsa_state *s;
	struct tevent_req *subreq;

	/* composite context and state structure allocation */
	c = composite_create(ctx, ctx->event_ctx);
	if (c == NULL) return c;

	s = talloc_zero(c, struct domain_close_lsa_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;
	s->monitor_fn   = monitor;

	/* TODO: check if lsa pipe pointer is non-null */

	if (!strequal(ctx->lsa.name, io->in.domain_name)) {
		composite_error(c, NT_STATUS_INVALID_PARAMETER);
		return c;
	}

	/* get opened lsarpc pipe pointer */
	s->pipe = ctx->lsa.pipe;
	
	/* prepare close handle call arguments */
	s->close.in.handle  = &ctx->lsa.handle;
	s->close.out.handle = &s->handle;

	/* send the request */
	subreq = dcerpc_lsa_Close_r_send(s, c->event_ctx,
					 s->pipe->binding_handle,
					 &s->close);
	if (composite_nomem(subreq, c)) return c;

	tevent_req_set_callback(subreq, continue_lsa_close, c);
	return c;
}


/*
  Stage 1: Receive result of lsa close call
*/
static void continue_lsa_close(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_close_lsa_state *s;
	
	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_close_lsa_state);

	c->status = dcerpc_lsa_Close_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (s->monitor_fn) {
		struct monitor_msg msg;

		msg.type      = mon_LsaClose;
		msg.data      = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}

	composite_done(c);
}


NTSTATUS libnet_DomainCloseLsa_recv(struct composite_context *c, struct libnet_context *ctx,
				    TALLOC_CTX *mem_ctx, struct libnet_DomainClose *io)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status) && io) {
		/* policy handle closed successfully */

		ctx->lsa.name = NULL;
		ZERO_STRUCT(ctx->lsa.handle);

		io->out.error_string = talloc_asprintf(mem_ctx, "Success");

	} else if (!NT_STATUS_IS_OK(status)) {
		/* there was an error, so return description of the status code */
		io->out.error_string = talloc_asprintf(mem_ctx, "Error: %s", nt_errstr(status));
	}

	talloc_free(c);
	return status;
}


struct domain_close_samr_state {
	struct samr_Close close;
	struct policy_handle handle;
	
	void (*monitor_fn)(struct monitor_msg*);
};


static void continue_samr_close(struct tevent_req *subreq);


struct composite_context* libnet_DomainCloseSamr_send(struct libnet_context *ctx,
						      struct libnet_DomainClose *io,
						      void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct domain_close_samr_state *s;
	struct tevent_req *subreq;

	/* composite context and state structure allocation */
	c = composite_create(ctx, ctx->event_ctx);
	if (c == NULL) return c;

	s = talloc_zero(c, struct domain_close_samr_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;
	s->monitor_fn   = monitor;

	/* TODO: check if samr pipe pointer is non-null */

	if (!strequal(ctx->samr.name, io->in.domain_name)) {
		composite_error(c, NT_STATUS_INVALID_PARAMETER);
		return c;
	}

	/* prepare close domain handle call arguments */
	ZERO_STRUCT(s->close);
	s->close.in.handle  = &ctx->samr.handle;
	s->close.out.handle = &s->handle;

	/* send the request */
	subreq = dcerpc_samr_Close_r_send(s, c->event_ctx,
					  ctx->samr.pipe->binding_handle,
					  &s->close);
	if (composite_nomem(subreq, c)) return c;

	tevent_req_set_callback(subreq, continue_samr_close, c);
	return c;
}


/*
  Stage 1: Receive result of samr close call
*/
static void continue_samr_close(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_close_samr_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_close_samr_state);
	
	c->status = dcerpc_samr_Close_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (s->monitor_fn) {
		struct monitor_msg msg;
		
		msg.type      = mon_SamrClose;
		msg.data      = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}
	
	composite_done(c);
}


NTSTATUS libnet_DomainCloseSamr_recv(struct composite_context *c, struct libnet_context *ctx,
				     TALLOC_CTX *mem_ctx, struct libnet_DomainClose *io)
{
	NTSTATUS status;

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status) && io) {
		/* domain policy handle closed successfully */

		ZERO_STRUCT(ctx->samr.handle);
		talloc_free(discard_const_p(char, ctx->samr.name));
		talloc_free(ctx->samr.sid);
		ctx->samr.name = NULL;
		ctx->samr.sid = NULL;

		io->out.error_string = talloc_asprintf(mem_ctx, "Success");

	} else if (!NT_STATUS_IS_OK(status)) {
		/* there was an error, so return description of the status code */
		io->out.error_string = talloc_asprintf(mem_ctx, "Error: %s", nt_errstr(status));
	}

	talloc_free(c);
	return status;
}


struct composite_context* libnet_DomainClose_send(struct libnet_context *ctx,
						  struct libnet_DomainClose *io,
						  void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;

	switch (io->in.type) {
	case DOMAIN_LSA:
		/* request to close policy handle on \pipe\lsarpc */
		c = libnet_DomainCloseLsa_send(ctx, io, monitor);
		break;

	case DOMAIN_SAMR:
	default:
		/* request to close domain policy handle on \pipe\samr */
		c = libnet_DomainCloseSamr_send(ctx, io, monitor);
		break;
	}
	
	return c;
}


NTSTATUS libnet_DomainClose_recv(struct composite_context *c, struct libnet_context *ctx,
				 TALLOC_CTX *mem_ctx, struct libnet_DomainClose *io)
{
	NTSTATUS status;

	switch (io->in.type) {
	case DOMAIN_LSA:
		/* receive result of closing lsa policy handle */
		status = libnet_DomainCloseLsa_recv(c, ctx, mem_ctx, io);
		break;

	case DOMAIN_SAMR:
	default:
		/* receive result of closing samr domain policy handle */
		status = libnet_DomainCloseSamr_recv(c, ctx, mem_ctx, io);
		break;
	}
	
	return status;
}


NTSTATUS libnet_DomainClose(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			    struct libnet_DomainClose *io)
{
	struct composite_context *c;
	
	c = libnet_DomainClose_send(ctx, io, NULL);
	return libnet_DomainClose_recv(c, ctx, mem_ctx, io);
}


struct domain_list_state {	
	struct libnet_context *ctx;
	struct libnet_RpcConnect rpcconn;
	struct samr_Connect samrconn;
	struct samr_EnumDomains enumdom;
	struct samr_Close samrclose;
	const char *hostname;
	struct policy_handle connect_handle;
	int buf_size;
	struct domainlist *domains;
	uint32_t resume_handle;
	uint32_t count;

	void (*monitor_fn)(struct monitor_msg*);
};


static void continue_rpc_connect(struct composite_context *c);
static void continue_samr_connect(struct tevent_req *subreq);
static void continue_samr_enum_domains(struct tevent_req *subreq);
static void continue_samr_close_handle(struct tevent_req *subreq);

static struct domainlist* get_domain_list(TALLOC_CTX *mem_ctx, struct domain_list_state *s);


/*
  Stage 1: Receive connected rpc pipe and send connection
  request to SAMR service
*/
static void continue_rpc_connect(struct composite_context *ctx)
{
	struct composite_context *c;
	struct domain_list_state *s;
	struct tevent_req *subreq;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_list_state);
	
	c->status = libnet_RpcConnect_recv(ctx, s->ctx, c, &s->rpcconn);
	if (!composite_is_ok(c)) return;

	s->samrconn.in.system_name     = 0;
	s->samrconn.in.access_mask     = SEC_GENERIC_READ;     /* should be enough */
	s->samrconn.out.connect_handle = &s->connect_handle;

	subreq = dcerpc_samr_Connect_r_send(s, c->event_ctx,
					    s->ctx->samr.pipe->binding_handle,
					    &s->samrconn);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_samr_connect, c);
}


/*
  Stage 2: Receive policy handle to the connected SAMR service and issue
  a request to enumerate domain databases available
*/
static void continue_samr_connect(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_list_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_list_state);
	
	c->status = dcerpc_samr_Connect_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (s->monitor_fn) {
		struct monitor_msg msg;
		
		msg.type      = mon_SamrConnect;
		msg.data      = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}

	s->enumdom.in.connect_handle = &s->connect_handle;
	s->enumdom.in.resume_handle  = &s->resume_handle;
	s->enumdom.in.buf_size       = s->buf_size;
	s->enumdom.out.resume_handle = &s->resume_handle;
	s->enumdom.out.num_entries   = talloc(s, uint32_t);
	if (composite_nomem(s->enumdom.out.num_entries, c)) return;
	s->enumdom.out.sam           = talloc(s, struct samr_SamArray *);
	if (composite_nomem(s->enumdom.out.sam, c)) return;

	subreq = dcerpc_samr_EnumDomains_r_send(s, c->event_ctx,
						s->ctx->samr.pipe->binding_handle,
						&s->enumdom);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_samr_enum_domains, c);
}


/*
  Stage 3: Receive domain names available and repeat the request
  enumeration is not complete yet. Close samr connection handle
  upon completion.
*/
static void continue_samr_enum_domains(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_list_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_list_state);
	
	c->status = dcerpc_samr_EnumDomains_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (s->monitor_fn) {
		struct monitor_msg msg;
		
		msg.type      = mon_SamrEnumDomains;
		msg.data      = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}

	if (NT_STATUS_IS_OK(s->enumdom.out.result)) {

		s->domains = get_domain_list(c, s);

	} else if (NT_STATUS_EQUAL(s->enumdom.out.result, STATUS_MORE_ENTRIES)) {
		
		s->domains = get_domain_list(c, s);
		
		/* prepare next round of enumeration */
		s->enumdom.in.connect_handle = &s->connect_handle;
		s->enumdom.in.resume_handle  = &s->resume_handle;
		s->enumdom.in.buf_size       = s->ctx->samr.buf_size;
		s->enumdom.out.resume_handle = &s->resume_handle;

		/* send the request */
		subreq = dcerpc_samr_EnumDomains_r_send(s, c->event_ctx,
							s->ctx->samr.pipe->binding_handle,
							&s->enumdom);
		if (composite_nomem(subreq, c)) return;

		tevent_req_set_callback(subreq, continue_samr_enum_domains, c);

	} else {
		composite_error(c, s->enumdom.out.result);
		return;
	}

	/* close samr connection handle */
	s->samrclose.in.handle  = &s->connect_handle;
	s->samrclose.out.handle = &s->connect_handle;
	
	/* send the request */
	subreq = dcerpc_samr_Close_r_send(s, c->event_ctx,
					  s->ctx->samr.pipe->binding_handle,
					  &s->samrclose);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_samr_close_handle, c);
}


/*
  Stage 4: Receive result of closing samr connection handle.
*/
static void continue_samr_close_handle(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct domain_list_state *s;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct domain_list_state);

	c->status = dcerpc_samr_Close_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	if (s->monitor_fn) {
		struct monitor_msg msg;
		
		msg.type      = mon_SamrClose;
		msg.data      = NULL;
		msg.data_size = 0;
		s->monitor_fn(&msg);
	}

	/* did everything go fine ? */
	if (!NT_STATUS_IS_OK(s->samrclose.out.result)) {
		composite_error(c, s->samrclose.out.result);
	}

	composite_done(c);
}


/*
  Utility function to copy domain names from result of samr_EnumDomains call
*/
static struct domainlist* get_domain_list(TALLOC_CTX *mem_ctx, struct domain_list_state *s)
{
	uint32_t i;
	if (mem_ctx == NULL || s == NULL) return NULL;

	/* prepare domains array */
	if (s->domains == NULL) {
		s->domains = talloc_array(mem_ctx, struct domainlist,
					  *s->enumdom.out.num_entries);
	} else {
		s->domains = talloc_realloc(mem_ctx, s->domains, struct domainlist,
					    s->count + *s->enumdom.out.num_entries);
	}

	/* copy domain names returned from samr_EnumDomains call */
	for (i = s->count; i < s->count + *s->enumdom.out.num_entries; i++)
	{
		struct lsa_String *domain_name = &(*s->enumdom.out.sam)->entries[i - s->count].name;

		/* strdup name as a child of allocated array to make it follow the array
		   in case of talloc_steal or talloc_free */
		s->domains[i].name = talloc_strdup(s->domains, domain_name->string);
		s->domains[i].sid  = NULL;  /* this is to be filled out later */
	}

	/* number of entries returned (domains enumerated) */
	s->count += *s->enumdom.out.num_entries;
	
	return s->domains;
}


/**
 * Sends a request to list domains on given host
 *
 * @param ctx initalised libnet context
 * @param mem_ctx memory context
 * @param io arguments and results of the call
 * @param monitor pointer to monitor function that is passed monitor messages
 */

struct composite_context* libnet_DomainList_send(struct libnet_context *ctx,
						 TALLOC_CTX *mem_ctx,
						 struct libnet_DomainList *io,
						 void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct domain_list_state *s;
	struct composite_context *rpcconn_req;
	struct tevent_req *subreq;

	/* composite context and state structure allocation */
	c = composite_create(ctx, ctx->event_ctx);
	if (c == NULL) return c;

	s = talloc_zero(c, struct domain_list_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;
	s->monitor_fn   = monitor;

	s->ctx      = ctx;
	s->hostname = talloc_strdup(c, io->in.hostname);
	if (composite_nomem(s->hostname, c)) return c;

	/* check whether samr pipe has already been opened */
	if (ctx->samr.pipe == NULL) {
		ZERO_STRUCT(s->rpcconn);

		/* prepare rpc connect call */
		s->rpcconn.level           = LIBNET_RPC_CONNECT_SERVER;
		s->rpcconn.in.name         = s->hostname;
		s->rpcconn.in.dcerpc_iface = &ndr_table_samr;

		rpcconn_req = libnet_RpcConnect_send(ctx, c, &s->rpcconn, s->monitor_fn);
		if (composite_nomem(rpcconn_req, c)) return c;
		
		composite_continue(c, rpcconn_req, continue_rpc_connect, c);

	} else {
		/* prepare samr_Connect call */
		s->samrconn.in.system_name     = 0;
		s->samrconn.in.access_mask     = SEC_GENERIC_READ;
		s->samrconn.out.connect_handle = &s->connect_handle;
		
		subreq = dcerpc_samr_Connect_r_send(s, c->event_ctx,
						    s->ctx->samr.pipe->binding_handle,
						    &s->samrconn);
		if (composite_nomem(subreq, c)) return c;

		tevent_req_set_callback(subreq, continue_samr_connect, c);
	}

	return c;
}


/**
 * Receive result of domain list request
 *
 * @param c composite context returned by DomainList_send function
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of the call
 * @param io results and arguments of the call
 */

NTSTATUS libnet_DomainList_recv(struct composite_context *c, struct libnet_context *ctx,
				TALLOC_CTX *mem_ctx, struct libnet_DomainList *io)
{
	NTSTATUS status;
	struct domain_list_state *s;

	status = composite_wait(c);

	s = talloc_get_type(c->private_data, struct domain_list_state);

	if (NT_STATUS_IS_OK(status) && ctx && mem_ctx && io) {
		/* fetch the results to be returned by io structure */
		io->out.count        = s->count;
		io->out.domains      = talloc_steal(mem_ctx, s->domains);
		io->out.error_string = talloc_asprintf(mem_ctx, "Success");

	} else if (!NT_STATUS_IS_OK(status)) {
		/* there was an error, so return description of the status code */
		io->out.error_string = talloc_asprintf(mem_ctx, "Error: %s", nt_errstr(status));
	}

	talloc_free(c);
	return status;
}


/**
 * Synchronous version of DomainList call
 *
 * @param ctx initialised libnet context
 * @param mem_ctx memory context for the call
 * @param io arguments and results of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_DomainList(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			   struct libnet_DomainList *io)
{
	struct composite_context *c;

	c = libnet_DomainList_send(ctx, mem_ctx, io, NULL);
	return libnet_DomainList_recv(c, ctx, mem_ctx, io);
}
