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
  a composite function for getting group information via samr pipe
*/


#include "includes.h"
#include "libcli/composite/composite.h"
#include "librpc/gen_ndr/security.h"
#include "libcli/security/security.h"
#include "libnet/libnet.h"
#include "librpc/gen_ndr/ndr_samr_c.h"


struct groupinfo_state {
	struct dcerpc_pipe         *pipe;
	struct policy_handle       domain_handle;
	struct policy_handle       group_handle;
	uint16_t                   level;
	struct samr_LookupNames    lookup;
	struct samr_OpenGroup      opengroup;
	struct samr_QueryGroupInfo querygroupinfo;
	struct samr_Close          samrclose;
	union  samr_GroupInfo      *info;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg *);
};


static void continue_groupinfo_lookup(struct rpc_request *req);
static void continue_groupinfo_opengroup(struct rpc_request *req);
static void continue_groupinfo_getgroup(struct rpc_request *req);
static void continue_groupinfo_closegroup(struct rpc_request *req);


/**
 * Stage 1 (optional): Look for a group name in SAM server.
 */
static void continue_groupinfo_lookup(struct rpc_request *req)
{
	struct composite_context *c;
	struct groupinfo_state *s;
	struct rpc_request *opengroup_req;
	struct monitor_msg msg;
	struct msg_rpc_lookup_name *msg_lookup;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct groupinfo_state);

	/* receive samr_Lookup reply */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;
	
	/* there could be a problem with name resolving itself */
	if (!NT_STATUS_IS_OK(s->lookup.out.result)) {
		composite_error(c, s->lookup.out.result);
		return;
	}

	/* issue a monitor message */
	if (s->monitor_fn) {
		msg.type = mon_SamrLookupName;
		msg_lookup = talloc(s, struct msg_rpc_lookup_name);
		msg_lookup->rid = s->lookup.out.rids->ids;
		msg_lookup->count = s->lookup.out.rids->count;
		msg.data = (void*)msg_lookup;
		msg.data_size = sizeof(*msg_lookup);
		
		s->monitor_fn(&msg);
	}
	

	/* have we actually got name resolved
	   - we're looking for only one at the moment */
	if (s->lookup.out.rids->count == 0) {
		composite_error(c, NT_STATUS_NO_SUCH_USER);
	}

	/* TODO: find proper status code for more than one rid found */

	/* prepare parameters for LookupNames */
	s->opengroup.in.domain_handle   = &s->domain_handle;
	s->opengroup.in.access_mask     = SEC_FLAG_MAXIMUM_ALLOWED;
	s->opengroup.in.rid             = s->lookup.out.rids->ids[0];
	s->opengroup.out.group_handle   = &s->group_handle;

	/* send request */
	opengroup_req = dcerpc_samr_OpenGroup_send(s->pipe, c, &s->opengroup);
	if (composite_nomem(opengroup_req, c)) return;

	composite_continue_rpc(c, opengroup_req, continue_groupinfo_opengroup, c);
}


/**
 * Stage 2: Open group policy handle.
 */
static void continue_groupinfo_opengroup(struct rpc_request *req)
{
	struct composite_context *c;
	struct groupinfo_state *s;
	struct rpc_request *querygroup_req;
	struct monitor_msg msg;
	struct msg_rpc_open_group *msg_open;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct groupinfo_state);

	/* receive samr_OpenGroup reply */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	if (!NT_STATUS_IS_OK(s->querygroupinfo.out.result)) {
		composite_error(c, s->querygroupinfo.out.result);
		return;
	}

	/* issue a monitor message */
	if (s->monitor_fn) {
		msg.type = mon_SamrOpenGroup;
		msg_open = talloc(s, struct msg_rpc_open_group);
		msg_open->rid = s->opengroup.in.rid;
		msg_open->access_mask = s->opengroup.in.access_mask;
		msg.data = (void*)msg_open;
		msg.data_size = sizeof(*msg_open);
		
		s->monitor_fn(&msg);
	}
	
	/* prepare parameters for QueryGroupInfo call */
	s->querygroupinfo.in.group_handle = &s->group_handle;
	s->querygroupinfo.in.level        = s->level;
	s->querygroupinfo.out.info        = talloc(s, union samr_GroupInfo *);
	if (composite_nomem(s->querygroupinfo.out.info, c)) return;
	
	/* queue rpc call, set event handling and new state */
	querygroup_req = dcerpc_samr_QueryGroupInfo_send(s->pipe, c, &s->querygroupinfo);
	if (composite_nomem(querygroup_req, c)) return;
	
	composite_continue_rpc(c, querygroup_req, continue_groupinfo_getgroup, c);
}


/**
 * Stage 3: Get requested group information.
 */
static void continue_groupinfo_getgroup(struct rpc_request *req)
{
	struct composite_context *c;
	struct groupinfo_state *s;
	struct rpc_request *close_req;
	struct monitor_msg msg;
	struct msg_rpc_query_group *msg_query;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct groupinfo_state);

	/* receive samr_QueryGroupInfo reply */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	/* check if querygroup itself went ok */
	if (!NT_STATUS_IS_OK(s->querygroupinfo.out.result)) {
		composite_error(c, s->querygroupinfo.out.result);
		return;
	}

	s->info = talloc_steal(s, *s->querygroupinfo.out.info);

	/* issue a monitor message */
	if (s->monitor_fn) {
		msg.type = mon_SamrQueryGroup;
		msg_query = talloc(s, struct msg_rpc_query_group);
		msg_query->level = s->querygroupinfo.in.level;
		msg.data = (void*)msg_query;
		msg.data_size = sizeof(*msg_query);
		
		s->monitor_fn(&msg);
	}
	
	/* prepare arguments for Close call */
	s->samrclose.in.handle  = &s->group_handle;
	s->samrclose.out.handle = &s->group_handle;
	
	/* queue rpc call, set event handling and new state */
	close_req = dcerpc_samr_Close_send(s->pipe, c, &s->samrclose);
	if (composite_nomem(close_req, c)) return;
	
	composite_continue_rpc(c, close_req, continue_groupinfo_closegroup, c);
}


/**
 * Stage 4: Close policy handle associated with opened group.
 */
static void continue_groupinfo_closegroup(struct rpc_request *req)
{
	struct composite_context *c;
	struct groupinfo_state *s;
	struct monitor_msg msg;
	struct msg_rpc_close_group *msg_close;

	c = talloc_get_type(req->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct groupinfo_state);

	/* receive samr_Close reply */
	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	if (!NT_STATUS_IS_OK(s->samrclose.out.result)) {
		composite_error(c, s->samrclose.out.result);
		return;
	}

	/* issue a monitor message */
	if (s->monitor_fn) {
		msg.type = mon_SamrClose;
		msg_close = talloc(s, struct msg_rpc_close_group);
		msg_close->rid = s->opengroup.in.rid;
		msg.data = (void*)msg_close;
		msg.data_size = sizeof(*msg_close);

		s->monitor_fn(&msg);
	}

	composite_done(c);
}


/**
 * Sends asynchronous groupinfo request
 *
 * @param p dce/rpc call pipe 
 * @param io arguments and results of the call
 */
struct composite_context *libnet_rpc_groupinfo_send(struct dcerpc_pipe *p,
						    struct libnet_rpc_groupinfo *io,
						    void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct groupinfo_state *s;
	struct dom_sid *sid;
	struct rpc_request *opengroup_req, *lookup_req;

	if (!p || !io) return NULL;
	
	c = composite_create(p, dcerpc_event_context(p));
	if (c == NULL) return c;
	
	s = talloc_zero(c, struct groupinfo_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;

	s->level         = io->in.level;
	s->pipe          = p;
	s->domain_handle = io->in.domain_handle;
	s->monitor_fn    = monitor;

	if (io->in.sid) {
		sid = dom_sid_parse_talloc(s, io->in.sid);
		if (composite_nomem(sid, c)) return c;

		s->opengroup.in.domain_handle  = &s->domain_handle;
		s->opengroup.in.access_mask    = SEC_FLAG_MAXIMUM_ALLOWED;
		s->opengroup.in.rid            = sid->sub_auths[sid->num_auths - 1];
		s->opengroup.out.group_handle  = &s->group_handle;
		
		/* send request */
		opengroup_req = dcerpc_samr_OpenGroup_send(p, c, &s->opengroup);
		if (composite_nomem(opengroup_req, c)) return c;

		composite_continue_rpc(c, opengroup_req, continue_groupinfo_opengroup, c);

	} else {
		/* preparing parameters to send rpc request */
		s->lookup.in.domain_handle    = &s->domain_handle;
		s->lookup.in.num_names        = 1;
		s->lookup.in.names            = talloc_array(s, struct lsa_String, 1);
		if (composite_nomem(s->lookup.in.names, c)) return c;

		s->lookup.in.names[0].string  = talloc_strdup(s, io->in.groupname);
		if (composite_nomem(s->lookup.in.names[0].string, c)) return c;
		s->lookup.out.rids         = talloc_zero(s, struct samr_Ids);
		s->lookup.out.types        = talloc_zero(s, struct samr_Ids);
		if (composite_nomem(s->lookup.out.rids, c)) return c;
		if (composite_nomem(s->lookup.out.types, c)) return c;

		/* send request */
		lookup_req = dcerpc_samr_LookupNames_send(p, c, &s->lookup);
		if (composite_nomem(lookup_req, c)) return c;
		
		composite_continue_rpc(c, lookup_req, continue_groupinfo_lookup, c);
	}

	return c;
}


/**
 * Waits for and receives result of asynchronous groupinfo call
 * 
 * @param c composite context returned by asynchronous groupinfo call
 * @param mem_ctx memory context of the call
 * @param io pointer to results (and arguments) of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_groupinfo_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
				   struct libnet_rpc_groupinfo *io)
{
	NTSTATUS status;
	struct groupinfo_state *s;
	
	/* wait for results of sending request */
	status = composite_wait(c);
	
	if (NT_STATUS_IS_OK(status) && io) {
		s = talloc_get_type(c->private_data, struct groupinfo_state);
		talloc_steal(mem_ctx, s->info);
		io->out.info = *s->info;
	}
	
	/* memory context associated to composite context is no longer needed */
	talloc_free(c);
	return status;
}


/**
 * Synchronous version of groupinfo call
 *
 * @param pipe dce/rpc call pipe
 * @param mem_ctx memory context for the call
 * @param io arguments and results of the call
 * @return nt status code of execution
 */

NTSTATUS libnet_rpc_groupinfo(struct dcerpc_pipe *p,
			      TALLOC_CTX *mem_ctx,
			      struct libnet_rpc_groupinfo *io)
{
	struct composite_context *c = libnet_rpc_groupinfo_send(p, io, NULL);
	return libnet_rpc_groupinfo_recv(c, mem_ctx, io);
}
