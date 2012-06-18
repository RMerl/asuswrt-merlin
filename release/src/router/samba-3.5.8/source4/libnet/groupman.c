/* 
   Unix SMB/CIFS implementation.

   Copyright (C) Rafal Szczesniak 2007
   
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
  a composite function for manipulating (add/edit/del) groups via samr pipe
*/

#include "includes.h"
#include "libcli/composite/composite.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"


struct groupadd_state {
	struct dcerpc_pipe *pipe;
	struct policy_handle domain_handle;
	struct samr_CreateDomainGroup creategroup;
	struct policy_handle group_handle;
	uint32_t group_rid;
	
	void (*monitor_fn)(struct monitor_msg*);
};


static void continue_groupadd_created(struct rpc_request *req);


struct composite_context* libnet_rpc_groupadd_send(struct dcerpc_pipe *p,
						   struct libnet_rpc_groupadd *io,
						   void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct groupadd_state *s;
	struct rpc_request *create_req;

	if (!p || !io) return NULL;

	c = composite_create(p, dcerpc_event_context(p));
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct groupadd_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;

	s->domain_handle = io->in.domain_handle;
	s->pipe          = p;
	s->monitor_fn    = monitor;

	s->creategroup.in.domain_handle  = &s->domain_handle;

	s->creategroup.in.name           = talloc_zero(c, struct lsa_String);
	if (composite_nomem(s->creategroup.in.name, c)) return c;

	s->creategroup.in.name->string   = talloc_strdup(c, io->in.groupname);
	if (composite_nomem(s->creategroup.in.name->string, c)) return c;
	
	s->creategroup.in.access_mask    = 0;
	
	s->creategroup.out.group_handle  = &s->group_handle;
	s->creategroup.out.rid           = &s->group_rid;
 	
	create_req = dcerpc_samr_CreateDomainGroup_send(s->pipe, c, &s->creategroup);
	if (composite_nomem(create_req, c)) return c;

	composite_continue_rpc(c, create_req, continue_groupadd_created, c);
	return c;
}


NTSTATUS libnet_rpc_groupadd_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
				  struct libnet_rpc_groupadd *io)
{
	NTSTATUS status;
	struct groupadd_state *s;
	
	status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		s = talloc_get_type(c, struct groupadd_state);
	}

	return status;
}


static void continue_groupadd_created(struct rpc_request *req)
{
	struct composite_context *c;
	struct groupadd_state *s;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct groupadd_state);

	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	c->status = s->creategroup.out.result;
	if (!composite_is_ok(c)) return;
	
	composite_done(c);
}


NTSTATUS libnet_rpc_groupadd(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			     struct libnet_rpc_groupadd *io)
{
	struct composite_context *c;

	c = libnet_rpc_groupadd_send(p, io, NULL);
	return libnet_rpc_groupadd_recv(c, mem_ctx, io);
}


struct groupdel_state {
	struct dcerpc_pipe             *pipe;
	struct policy_handle           domain_handle;
	struct policy_handle           group_handle;
	struct samr_LookupNames        lookupname;
	struct samr_OpenGroup          opengroup;
	struct samr_DeleteDomainGroup  deletegroup;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg *);
};


static void continue_groupdel_name_found(struct rpc_request *req);
static void continue_groupdel_group_opened(struct rpc_request *req);
static void continue_groupdel_deleted(struct rpc_request *req);


struct composite_context* libnet_rpc_groupdel_send(struct dcerpc_pipe *p,
						   struct libnet_rpc_groupdel *io,
						   void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct groupdel_state *s;
	struct rpc_request *lookup_req;

	/* composite context allocation and setup */
	c = composite_create(p, dcerpc_event_context(p));
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct groupdel_state);
	if (composite_nomem(s, c)) return c;

	c->private_data  = s;

	/* store function parameters in the state structure */
	s->pipe          = p;
	s->domain_handle = io->in.domain_handle;
	s->monitor_fn    = monitor;
	
	/* prepare parameters to send rpc request */
	s->lookupname.in.domain_handle = &io->in.domain_handle;
	s->lookupname.in.num_names     = 1;
	s->lookupname.in.names         = talloc_zero(s, struct lsa_String);
	s->lookupname.in.names->string = io->in.groupname;
	s->lookupname.out.rids         = talloc_zero(s, struct samr_Ids);
	s->lookupname.out.types        = talloc_zero(s, struct samr_Ids);
	if (composite_nomem(s->lookupname.out.rids, c)) return c;
	if (composite_nomem(s->lookupname.out.types, c)) return c;

	/* send the request */
	lookup_req = dcerpc_samr_LookupNames_send(p, c, &s->lookupname);
	if (composite_nomem(lookup_req, c)) return c;

	composite_continue_rpc(c, lookup_req, continue_groupdel_name_found, c);
	return c;
}


static void continue_groupdel_name_found(struct rpc_request *req)
{
	struct composite_context *c;
	struct groupdel_state *s;
	struct rpc_request *opengroup_req;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct groupdel_state);

	/* receive samr_LookupNames result */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	c->status = s->lookupname.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}

	/* what to do when there's no group account to delete
	   and what if there's more than one rid resolved */
	if (!s->lookupname.out.rids->count) {
		c->status = NT_STATUS_NO_SUCH_GROUP;
		composite_error(c, c->status);
		return;

	} else if (!s->lookupname.out.rids->count > 1) {
		c->status = NT_STATUS_INVALID_ACCOUNT_NAME;
		composite_error(c, c->status);
		return;
	}

	/* prepare the arguments for rpc call */
	s->opengroup.in.domain_handle = &s->domain_handle;
	s->opengroup.in.rid           = s->lookupname.out.rids->ids[0];
	s->opengroup.in.access_mask   = SEC_FLAG_MAXIMUM_ALLOWED;
	s->opengroup.out.group_handle  = &s->group_handle;

	/* send rpc request */
	opengroup_req = dcerpc_samr_OpenGroup_send(s->pipe, c, &s->opengroup);
	if (composite_nomem(opengroup_req, c)) return;

	composite_continue_rpc(c, opengroup_req, continue_groupdel_group_opened, c);
}


static void continue_groupdel_group_opened(struct rpc_request *req)
{
	struct composite_context *c;
	struct groupdel_state *s;
	struct rpc_request *delgroup_req;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct groupdel_state);

	/* receive samr_OpenGroup result */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	c->status = s->opengroup.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}
	
	/* prepare the final rpc call arguments */
	s->deletegroup.in.group_handle   = &s->group_handle;
	s->deletegroup.out.group_handle  = &s->group_handle;
	
	/* send rpc request */
	delgroup_req = dcerpc_samr_DeleteDomainGroup_send(s->pipe, c, &s->deletegroup);
	if (composite_nomem(delgroup_req, c)) return;

	/* callback handler setup */
	composite_continue_rpc(c, delgroup_req, continue_groupdel_deleted, c);
}


static void continue_groupdel_deleted(struct rpc_request *req)
{
	struct composite_context *c;
	struct groupdel_state *s;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct groupdel_state);

	/* receive samr_DeleteGroup result */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	/* return the actual function call status */
	c->status = s->deletegroup.out.result;
	if (!NT_STATUS_IS_OK(c->status)) {
		composite_error(c, c->status);
		return;
	}
	
	composite_done(c);
}


NTSTATUS libnet_rpc_groupdel_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
				  struct libnet_rpc_groupdel *io)
{
	NTSTATUS status;
	struct groupdel_state *s;

	status = composite_wait(c);
	if (NT_STATUS_IS_OK(status) && io) {
		s = talloc_get_type(c->private_data, struct groupdel_state);
		io->out.group_handle = s->group_handle;
	}

	talloc_free(c);
	return status;
}


NTSTATUS libnet_rpc_groupdel(struct dcerpc_pipe *p, TALLOC_CTX *mem_ctx,
			     struct libnet_rpc_groupdel *io)
{
	struct composite_context *c;

	c = libnet_rpc_groupdel_send(p, io, NULL);
	return libnet_rpc_groupdel_recv(c, mem_ctx, io);
}
