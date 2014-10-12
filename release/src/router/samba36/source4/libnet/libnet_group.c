/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Rafal Szczesniak  2007
   
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
#include "libnet/libnet.h"
#include "libcli/composite/composite.h"
#include "librpc/gen_ndr/lsa.h"
#include "librpc/gen_ndr/ndr_lsa_c.h"
#include "librpc/gen_ndr/samr.h"
#include "librpc/gen_ndr/ndr_samr_c.h"
#include "libcli/security/security.h"


struct create_group_state {
	struct libnet_context *ctx;
	struct libnet_CreateGroup r;
	struct libnet_DomainOpen domain_open;
	struct libnet_rpc_groupadd group_add;

	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg *);
};


static void continue_domain_opened(struct composite_context *ctx);
static void continue_rpc_group_added(struct composite_context *ctx);


struct composite_context* libnet_CreateGroup_send(struct libnet_context *ctx,
						  TALLOC_CTX *mem_ctx,
						  struct libnet_CreateGroup *r,
						  void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct create_group_state *s;
	struct composite_context *create_req;
	bool prereq_met = false;

	/* composite context allocation and setup */
	c = composite_create(mem_ctx, ctx->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct create_group_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;

	s->ctx = ctx;
	s->r   = *r;
	ZERO_STRUCT(s->r.out);

	/* prerequisite: make sure we have a valid samr domain handle */
	prereq_met = samr_domain_opened(ctx, s->r.in.domain_name, &c, &s->domain_open,
					continue_domain_opened, monitor);
	if (!prereq_met) return c;

	/* prepare arguments of rpc group add call */
	s->group_add.in.groupname     = r->in.group_name;
	s->group_add.in.domain_handle = ctx->samr.handle;

	/* send the request */
	create_req = libnet_rpc_groupadd_send(ctx->samr.pipe, &s->group_add, monitor);
	if (composite_nomem(create_req, c)) return c;

	composite_continue(c, create_req, continue_rpc_group_added, c);
	return c;
}


static void continue_domain_opened(struct composite_context *ctx)
{
	struct composite_context *c;
	struct create_group_state *s;
	struct composite_context *create_req;
	
	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct create_group_state);

	c->status = libnet_DomainOpen_recv(ctx, s->ctx, c, &s->domain_open);
	if (!composite_is_ok(c)) return;

	/* prepare arguments of groupadd call */
	s->group_add.in.groupname     = s->r.in.group_name;
	s->group_add.in.domain_handle = s->ctx->samr.handle;

	/* send the request */
	create_req = libnet_rpc_groupadd_send(s->ctx->samr.pipe, &s->group_add,
					      s->monitor_fn);
	if (composite_nomem(create_req, c)) return;

	composite_continue(c, create_req, continue_rpc_group_added, c);
}


static void continue_rpc_group_added(struct composite_context *ctx)
{
	struct composite_context *c;
	struct create_group_state *s;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct create_group_state);

	/* receive result of group add call */
	c->status = libnet_rpc_groupadd_recv(ctx, c, &s->group_add);
	if (!composite_is_ok(c)) return;

	/* we're done */
	composite_done(c);
}


/**
 * Receive result of CreateGroup call
 *
 * @param c composite context returned by send request routine
 * @param mem_ctx memory context of this call
 * @param r pointer to a structure containing arguments and result of this call
 * @return nt status
 */
NTSTATUS libnet_CreateGroup_recv(struct composite_context *c,
				 TALLOC_CTX *mem_ctx,
				 struct libnet_CreateGroup *r)
{
	NTSTATUS status;
	struct create_group_state *s;

	status = composite_wait(c);
	if (!NT_STATUS_IS_OK(status)) {
		s = talloc_get_type(c->private_data, struct create_group_state);
		r->out.error_string = talloc_strdup(mem_ctx, nt_errstr(status));
	}

	talloc_free(c);
	return status;
}


/**
 * Create domain group
 *
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param io pointer to structure containing arguments and result of this call
 * @return nt status
 */
NTSTATUS libnet_CreateGroup(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			    struct libnet_CreateGroup *io)
{
	struct composite_context *c;

	c = libnet_CreateGroup_send(ctx, mem_ctx, io, NULL);
	return libnet_CreateGroup_recv(c, mem_ctx, io);
}


struct group_info_state {
	struct libnet_context *ctx;
	const char *domain_name;
	enum libnet_GroupInfo_level level;
	const char *group_name;
	const char *sid_string;
	struct libnet_LookupName lookup;
	struct libnet_DomainOpen domopen;
	struct libnet_rpc_groupinfo info;
	
	/* information about the progress */
	void (*monitor_fn)(struct monitor_msg *);
};


static void continue_domain_open_info(struct composite_context *ctx);
static void continue_name_found(struct composite_context *ctx);
static void continue_group_info(struct composite_context *ctx);

/**
 * Sends request to get group information
 *
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param io pointer to structure containing arguments the call
 * @param monitor function pointer for receiving monitor messages
 * @return composite context of this request
 */
struct composite_context* libnet_GroupInfo_send(struct libnet_context *ctx,
						TALLOC_CTX *mem_ctx,
						struct libnet_GroupInfo *io,
						void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct group_info_state *s;
	bool prereq_met = false;
	struct composite_context *lookup_req, *info_req;

	/* composite context allocation and setup */
	c = composite_create(mem_ctx, ctx->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct group_info_state);
	if (composite_nomem(s, c)) return c;

	c->private_data = s;

	/* store arguments in the state structure */
	s->monitor_fn = monitor;
	s->ctx = ctx;
	s->domain_name = talloc_strdup(c, io->in.domain_name);
	s->level = io->in.level;
	switch(s->level) {
	case GROUP_INFO_BY_NAME:
		s->group_name = talloc_strdup(c, io->in.data.group_name);
		s->sid_string = NULL;
		break;
	case GROUP_INFO_BY_SID:
		s->group_name = NULL;
		s->sid_string = dom_sid_string(c, io->in.data.group_sid);
		break;
	}

	/* prerequisite: make sure the domain is opened */
	prereq_met = samr_domain_opened(ctx, s->domain_name, &c, &s->domopen,
					continue_domain_open_info, monitor);
	if (!prereq_met) return c;

	switch(s->level) {
	case GROUP_INFO_BY_NAME:
		/* prepare arguments for LookupName call */
		s->lookup.in.name        = s->group_name;
		s->lookup.in.domain_name = s->domain_name;

		/* send the request */
		lookup_req = libnet_LookupName_send(s->ctx, c, &s->lookup, s->monitor_fn);
		if (composite_nomem(lookup_req, c)) return c;

		/* set the next stage */
		composite_continue(c, lookup_req, continue_name_found, c);
		break;
	case GROUP_INFO_BY_SID:
		/* prepare arguments for groupinfo call */
		s->info.in.domain_handle = s->ctx->samr.handle;
		s->info.in.sid           = s->sid_string;
		/* we're looking for all information available */
		s->info.in.level         = GROUPINFOALL;

		/* send the request */
		info_req = libnet_rpc_groupinfo_send(s->ctx->samr.pipe, &s->info, s->monitor_fn);
		if (composite_nomem(info_req, c)) return c;

		/* set the next stage */
		composite_continue(c, info_req, continue_group_info, c);
		break;
	}

	return c;
}


/*
 * Stage 0.5 (optional): receive opened domain and send lookup name request
 */
static void continue_domain_open_info(struct composite_context *ctx)
{
	struct composite_context *c;
	struct group_info_state *s;
	struct composite_context *lookup_req, *info_req;
	
	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct group_info_state);
	
	/* receive domain handle */
	c->status = libnet_DomainOpen_recv(ctx, s->ctx, c, &s->domopen);
	if (!composite_is_ok(c)) return;

	switch(s->level) {
	case GROUP_INFO_BY_NAME:
		/* prepare arguments for LookupName call */
		s->lookup.in.name        = s->group_name;
		s->lookup.in.domain_name = s->domain_name;

		/* send the request */
		lookup_req = libnet_LookupName_send(s->ctx, c, &s->lookup, s->monitor_fn);
		if (composite_nomem(lookup_req, c)) return;

		/* set the next stage */
		composite_continue(c, lookup_req, continue_name_found, c);
		break;
	case GROUP_INFO_BY_SID:
		/* prepare arguments for groupinfo call */
		s->info.in.domain_handle = s->ctx->samr.handle;
		s->info.in.sid           = s->sid_string;
		/* we're looking for all information available */
		s->info.in.level         = GROUPINFOALL;

		/* send the request */
		info_req = libnet_rpc_groupinfo_send(s->ctx->samr.pipe, &s->info, s->monitor_fn);
		if (composite_nomem(info_req, c)) return;

		/* set the next stage */
		composite_continue(c, info_req, continue_group_info, c);
		break;

	}
}


/*
 * Stage 1: Receive SID found and send request for group info
 */
static void continue_name_found(struct composite_context *ctx)
{
	struct composite_context *c;
	struct group_info_state *s;
	struct composite_context *info_req;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct group_info_state);

	/* receive SID assiociated with name found */
	c->status = libnet_LookupName_recv(ctx, c, &s->lookup);
	if (!composite_is_ok(c)) return;

	/* Is is a group SID actually ? */
	if (s->lookup.out.sid_type != SID_NAME_DOM_GRP &&
	    s->lookup.out.sid_type != SID_NAME_ALIAS) {
		composite_error(c, NT_STATUS_NO_SUCH_GROUP);
	}

	/* prepare arguments for groupinfo call */
	s->info.in.domain_handle = s->ctx->samr.handle;
	s->info.in.groupname     = s->group_name;
	s->info.in.sid           = s->lookup.out.sidstr;
	/* we're looking for all information available */
	s->info.in.level         = GROUPINFOALL;

	/* send the request */
	info_req = libnet_rpc_groupinfo_send(s->ctx->samr.pipe, &s->info, s->monitor_fn);
	if (composite_nomem(info_req, c)) return;

	/* set the next stage */
	composite_continue(c, info_req, continue_group_info, c);
}


/*
 * Stage 2: Receive group information
 */
static void continue_group_info(struct composite_context *ctx)
{
	struct composite_context *c;
	struct group_info_state *s;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct group_info_state);

	/* receive group information */
	c->status = libnet_rpc_groupinfo_recv(ctx, c, &s->info);
	if (!composite_is_ok(c)) return;

	/* we're done */
	composite_done(c);
}


/*
 * Receive group information
 *
 * @param c composite context returned by libnet_GroupInfo_send
 * @param mem_ctx memory context of this call
 * @param io pointer to structure receiving results of the call
 * @result nt status
 */
NTSTATUS libnet_GroupInfo_recv(struct composite_context* c, TALLOC_CTX *mem_ctx,
			       struct libnet_GroupInfo *io)
{
	NTSTATUS status;
	struct group_info_state *s;
	
	status = composite_wait(c);
	if (NT_STATUS_IS_OK(status)) {
		/* put the results into io structure if everything went fine */
		s = talloc_get_type(c->private_data, struct group_info_state);

		io->out.group_name = talloc_steal(mem_ctx,
					s->info.out.info.all.name.string);
		io->out.group_sid = talloc_steal(mem_ctx, s->lookup.out.sid);
		io->out.num_members = s->info.out.info.all.num_members;
		io->out.description = talloc_steal(mem_ctx, s->info.out.info.all.description.string);

		io->out.error_string = talloc_strdup(mem_ctx, "Success");

	} else {
		io->out.error_string = talloc_asprintf(mem_ctx, "Error: %s", nt_errstr(status));
	}

	talloc_free(c);
	return status;
}


/**
 * Obtains specified group information
 * 
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of the call
 * @param io pointer to a structure containing arguments and results of the call
 */
NTSTATUS libnet_GroupInfo(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			  struct libnet_GroupInfo *io)
{
	struct composite_context *c = libnet_GroupInfo_send(ctx, mem_ctx,
							    io, NULL);
	return libnet_GroupInfo_recv(c, mem_ctx, io);
}


struct grouplist_state {
	struct libnet_context *ctx;
	const char *domain_name;
	struct lsa_DomainInfo dominfo;
	int page_size;
	uint32_t resume_index;
	struct grouplist *groups;
	uint32_t count;

	struct libnet_DomainOpen domain_open;
	struct lsa_QueryInfoPolicy query_domain;
	struct samr_EnumDomainGroups group_list;

	void (*monitor_fn)(struct monitor_msg*);
};


static void continue_lsa_domain_opened(struct composite_context *ctx);
static void continue_domain_queried(struct tevent_req *subreq);
static void continue_samr_domain_opened(struct composite_context *ctx);
static void continue_groups_enumerated(struct tevent_req *subreq);


/**
 * Sends request to list (enumerate) group accounts
 *
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param io pointer to structure containing arguments and results of this call
 * @param monitor function pointer for receiving monitor messages
 * @return compostite context of this request
 */
struct composite_context *libnet_GroupList_send(struct libnet_context *ctx,
						TALLOC_CTX *mem_ctx,
						struct libnet_GroupList *io,
						void (*monitor)(struct monitor_msg*))
{
	struct composite_context *c;
	struct grouplist_state *s;
	struct tevent_req *subreq;
	bool prereq_met = false;

	/* composite context allocation and setup */
	c = composite_create(mem_ctx, ctx->event_ctx);
	if (c == NULL) return NULL;

	s = talloc_zero(c, struct grouplist_state);
	if (composite_nomem(s, c)) return c;
	
	c->private_data = s;

	/* store the arguments in the state structure */
	s->ctx          = ctx;
	s->page_size    = io->in.page_size;
	s->resume_index = io->in.resume_index;
	s->domain_name  = talloc_strdup(c, io->in.domain_name);
	s->monitor_fn   = monitor;

	/* make sure we have lsa domain handle before doing anything */
	prereq_met = lsa_domain_opened(ctx, s->domain_name, &c, &s->domain_open,
				       continue_lsa_domain_opened, monitor);
	if (!prereq_met) return c;

	/* prepare arguments of QueryDomainInfo call */
	s->query_domain.in.handle = &ctx->lsa.handle;
	s->query_domain.in.level  = LSA_POLICY_INFO_DOMAIN;
	s->query_domain.out.info  = talloc_zero(c, union lsa_PolicyInformation *);
	if (composite_nomem(s->query_domain.out.info, c)) return c;

	/* send the request */
	subreq = dcerpc_lsa_QueryInfoPolicy_r_send(s, c->event_ctx,
						   ctx->lsa.pipe->binding_handle,
						   &s->query_domain);
	if (composite_nomem(subreq, c)) return c;
	
	tevent_req_set_callback(subreq, continue_domain_queried, c);
	return c;
}


/*
 * Stage 0.5 (optional): receive lsa domain handle and send
 * request to query domain info
 */
static void continue_lsa_domain_opened(struct composite_context *ctx)
{
	struct composite_context *c;
	struct grouplist_state *s;
	struct tevent_req *subreq;
	
	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct grouplist_state);

	/* receive lsa domain handle */
	c->status = libnet_DomainOpen_recv(ctx, s->ctx, c, &s->domain_open);
	if (!composite_is_ok(c)) return;

	/* prepare arguments of QueryDomainInfo call */
	s->query_domain.in.handle = &s->ctx->lsa.handle;
	s->query_domain.in.level  = LSA_POLICY_INFO_DOMAIN;
	s->query_domain.out.info  = talloc_zero(c, union lsa_PolicyInformation *);
	if (composite_nomem(s->query_domain.out.info, c)) return;

	/* send the request */
	subreq = dcerpc_lsa_QueryInfoPolicy_r_send(s, c->event_ctx,
						   s->ctx->lsa.pipe->binding_handle,
						   &s->query_domain);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_domain_queried, c);
}


/*
 * Stage 1: receive domain info and request to enum groups
 * provided a valid samr handle is opened
 */
static void continue_domain_queried(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct grouplist_state *s;
	bool prereq_met = false;
	
	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct grouplist_state);

	/* receive result of rpc request */
	c->status = dcerpc_lsa_QueryInfoPolicy_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	/* get the returned domain info */
	s->dominfo = (*s->query_domain.out.info)->domain;

	/* make sure we have samr domain handle before continuing */
	prereq_met = samr_domain_opened(s->ctx, s->domain_name, &c, &s->domain_open,
					continue_samr_domain_opened, s->monitor_fn);
	if (!prereq_met) return;

	/* prepare arguments od EnumDomainGroups call */
	s->group_list.in.domain_handle  = &s->ctx->samr.handle;
	s->group_list.in.max_size       = s->page_size;
	s->group_list.in.resume_handle  = &s->resume_index;
	s->group_list.out.resume_handle = &s->resume_index;
	s->group_list.out.num_entries   = talloc(s, uint32_t);
	if (composite_nomem(s->group_list.out.num_entries, c)) return;
	s->group_list.out.sam           = talloc(s, struct samr_SamArray *);
	if (composite_nomem(s->group_list.out.sam, c)) return;

	/* send the request */
	subreq = dcerpc_samr_EnumDomainGroups_r_send(s, c->event_ctx,
						     s->ctx->samr.pipe->binding_handle,
						     &s->group_list);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_groups_enumerated, c);
}


/*
 * Stage 1.5 (optional): receive samr domain handle
 * and request to enumerate accounts
 */
static void continue_samr_domain_opened(struct composite_context *ctx)
{
	struct composite_context *c;
	struct grouplist_state *s;
	struct tevent_req *subreq;

	c = talloc_get_type(ctx->async.private_data, struct composite_context);
	s = talloc_get_type(c->private_data, struct grouplist_state);

	/* receive samr domain handle */
	c->status = libnet_DomainOpen_recv(ctx, s->ctx, c, &s->domain_open);
	if (!composite_is_ok(c)) return;

	/* prepare arguments of EnumDomainGroups call */
	s->group_list.in.domain_handle  = &s->ctx->samr.handle;
	s->group_list.in.max_size       = s->page_size;
	s->group_list.in.resume_handle  = &s->resume_index;
	s->group_list.out.resume_handle = &s->resume_index;
	s->group_list.out.num_entries   = talloc(s, uint32_t);
	if (composite_nomem(s->group_list.out.num_entries, c)) return;
	s->group_list.out.sam           = talloc(s, struct samr_SamArray *);
	if (composite_nomem(s->group_list.out.sam, c)) return;

	/* send the request */
	subreq = dcerpc_samr_EnumDomainGroups_r_send(s, c->event_ctx,
						     s->ctx->samr.pipe->binding_handle,
						     &s->group_list);
	if (composite_nomem(subreq, c)) return;

	tevent_req_set_callback(subreq, continue_groups_enumerated, c);
}


/*
 * Stage 2: receive enumerated groups and their rids
 */
static void continue_groups_enumerated(struct tevent_req *subreq)
{
	struct composite_context *c;
	struct grouplist_state *s;
	uint32_t i;

	c = tevent_req_callback_data(subreq, struct composite_context);
	s = talloc_get_type(c->private_data, struct grouplist_state);

	/* receive result of rpc request */
	c->status = dcerpc_samr_EnumDomainGroups_r_recv(subreq, s);
	TALLOC_FREE(subreq);
	if (!composite_is_ok(c)) return;

	/* get the actual status of the rpc call result
	   (instead of rpc layer) */
	c->status = s->group_list.out.result;

	/* we're interested in status "ok" as well as two
	   enum-specific status codes */
	if (NT_STATUS_IS_OK(c->status) ||
	    NT_STATUS_EQUAL(c->status, STATUS_MORE_ENTRIES) ||
	    NT_STATUS_EQUAL(c->status, NT_STATUS_NO_MORE_ENTRIES)) {
		
		/* get enumerated accounts counter and resume handle (the latter allows
		   making subsequent call to continue enumeration) */
		s->resume_index = *s->group_list.out.resume_handle;
		s->count        = *s->group_list.out.num_entries;

		/* prepare returned group accounts array */
		s->groups       = talloc_array(c, struct grouplist, (*s->group_list.out.sam)->count);
		if (composite_nomem(s->groups, c)) return;

		for (i = 0; i < (*s->group_list.out.sam)->count; i++) {
			struct dom_sid *group_sid;
			struct samr_SamEntry *entry = &(*s->group_list.out.sam)->entries[i];
			struct dom_sid *domain_sid = (*s->query_domain.out.info)->domain.sid;
			
			/* construct group sid from returned rid and queried domain sid */
			group_sid = dom_sid_add_rid(c, domain_sid, entry->idx);
			if (composite_nomem(group_sid, c)) return;

			/* groupname */
			s->groups[i].groupname = talloc_strdup(s->groups, entry->name.string);
			if (composite_nomem(s->groups[i].groupname, c)) return;

			/* sid string */
			s->groups[i].sid = dom_sid_string(s->groups, group_sid);
			if (composite_nomem(s->groups[i].sid, c)) return;
		}

		/* that's it */
		composite_done(c);

	} else {
		/* something went wrong */
		composite_error(c, c->status);
	}
}


/**
 * Receive result of GroupList call
 *
 * @param c composite context returned by send request routine
 * @param mem_ctx memory context of this call
 * @param io pointer to structure containing arguments and result of this call
 * @param nt status
 */
NTSTATUS libnet_GroupList_recv(struct composite_context *c, TALLOC_CTX *mem_ctx,
			       struct libnet_GroupList *io)
{
	NTSTATUS status;
	struct grouplist_state *s;

	if (c == NULL || mem_ctx == NULL || io == NULL) {
		talloc_free(c);
		return NT_STATUS_INVALID_PARAMETER;
	}

	status = composite_wait(c);
	if (NT_STATUS_IS_OK(status) ||
	    NT_STATUS_EQUAL(status, STATUS_MORE_ENTRIES) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_NO_MORE_ENTRIES)) {
		
		s = talloc_get_type(c->private_data, struct grouplist_state);
		
		/* get results from composite context */
		io->out.count        = s->count;
		io->out.resume_index = s->resume_index;
		io->out.groups       = talloc_steal(mem_ctx, s->groups);

		if (NT_STATUS_IS_OK(status)) {
			io->out.error_string = talloc_asprintf(mem_ctx, "Success");
		} else {
			/* success, but we're not done yet */
			io->out.error_string = talloc_asprintf(mem_ctx, "Success (status: %s)",
							       nt_errstr(status));
		}

	} else {
		io->out.error_string = talloc_asprintf(mem_ctx, "Error: %s", nt_errstr(status));
	}

	talloc_free(c);
	return status;
}


/**
 * Enumerate domain groups
 *
 * @param ctx initialised libnet context
 * @param mem_ctx memory context of this call
 * @param io pointer to structure containing arguments and result of this call
 * @return nt status
 */
NTSTATUS libnet_GroupList(struct libnet_context *ctx, TALLOC_CTX *mem_ctx,
			  struct libnet_GroupList *io)
{
	struct composite_context *c;

	c = libnet_GroupList_send(ctx, mem_ctx, io, NULL);
	return libnet_GroupList_recv(c, mem_ctx, io);
}
