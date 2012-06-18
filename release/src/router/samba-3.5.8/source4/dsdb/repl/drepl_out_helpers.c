/* 
   Unix SMB/CIFS mplementation.
   DSDB replication service helper function for outgoing traffic
   
   Copyright (C) Stefan Metzmacher 2007
    
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
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "lib/events/events.h"
#include "lib/messaging/irpc.h"
#include "dsdb/repl/drepl_service.h"
#include "lib/ldb/include/ldb_errors.h"
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "libcli/composite/composite.h"
#include "auth/gensec/gensec.h"
#include "param/param.h"

struct dreplsrv_out_drsuapi_state {
	struct composite_context *creq;

	struct dreplsrv_out_connection *conn;

	struct dreplsrv_drsuapi_connection *drsuapi;

	struct drsuapi_DsBindInfoCtr bind_info_ctr;
	struct drsuapi_DsBind bind_r;
};

static void dreplsrv_out_drsuapi_connect_recv(struct composite_context *creq);

struct composite_context *dreplsrv_out_drsuapi_send(struct dreplsrv_out_connection *conn)
{
	struct composite_context *c;
	struct composite_context *creq;
	struct dreplsrv_out_drsuapi_state *st;

	c = composite_create(conn, conn->service->task->event_ctx);
	if (c == NULL) return NULL;

	st = talloc_zero(c, struct dreplsrv_out_drsuapi_state);
	if (composite_nomem(st, c)) return c;

	c->private_data	= st;

	st->creq	= c;
	st->conn	= conn;
	st->drsuapi	= conn->drsuapi;

	if (st->drsuapi && !st->drsuapi->pipe->conn->dead) {
		composite_done(c);
		return c;
	} else if (st->drsuapi && st->drsuapi->pipe->conn->dead) {
		talloc_free(st->drsuapi);
		conn->drsuapi = NULL;
	}

	st->drsuapi	= talloc_zero(st, struct dreplsrv_drsuapi_connection);
	if (composite_nomem(st->drsuapi, c)) return c;

	creq = dcerpc_pipe_connect_b_send(st, conn->binding, &ndr_table_drsuapi,
					  conn->service->system_session_info->credentials,
					  c->event_ctx, conn->service->task->lp_ctx);
	composite_continue(c, creq, dreplsrv_out_drsuapi_connect_recv, st);

	return c;
}

static void dreplsrv_out_drsuapi_bind_send(struct dreplsrv_out_drsuapi_state *st);

static void dreplsrv_out_drsuapi_connect_recv(struct composite_context *creq)
{
	struct dreplsrv_out_drsuapi_state *st = talloc_get_type(creq->async.private_data,
						struct dreplsrv_out_drsuapi_state);
	struct composite_context *c = st->creq;

	c->status = dcerpc_pipe_connect_b_recv(creq, st->drsuapi, &st->drsuapi->pipe);
	if (!composite_is_ok(c)) return;

	c->status = gensec_session_key(st->drsuapi->pipe->conn->security_state.generic_state,
				       &st->drsuapi->gensec_skey);
	if (!composite_is_ok(c)) return;

	dreplsrv_out_drsuapi_bind_send(st);
}

static void dreplsrv_out_drsuapi_bind_recv(struct rpc_request *req);

static void dreplsrv_out_drsuapi_bind_send(struct dreplsrv_out_drsuapi_state *st)
{
	struct composite_context *c = st->creq;
	struct rpc_request *req;

	st->bind_info_ctr.length	= 28;
	st->bind_info_ctr.info.info28	= st->conn->service->bind_info28;

	st->bind_r.in.bind_guid = &st->conn->service->ntds_guid;
	st->bind_r.in.bind_info = &st->bind_info_ctr;
	st->bind_r.out.bind_handle = &st->drsuapi->bind_handle;

	req = dcerpc_drsuapi_DsBind_send(st->drsuapi->pipe, st, &st->bind_r);
	composite_continue_rpc(c, req, dreplsrv_out_drsuapi_bind_recv, st);
}

static void dreplsrv_out_drsuapi_bind_recv(struct rpc_request *req)
{
	struct dreplsrv_out_drsuapi_state *st = talloc_get_type(req->async.private_data,
						struct dreplsrv_out_drsuapi_state);
	struct composite_context *c = st->creq;

	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	if (!W_ERROR_IS_OK(st->bind_r.out.result)) {
		composite_error(c, werror_to_ntstatus(st->bind_r.out.result));
		return;
	}

	ZERO_STRUCT(st->drsuapi->remote_info28);
	if (st->bind_r.out.bind_info) {
		switch (st->bind_r.out.bind_info->length) {
		case 24: {
			struct drsuapi_DsBindInfo24 *info24;
			info24 = &st->bind_r.out.bind_info->info.info24;
			st->drsuapi->remote_info28.supported_extensions	= info24->supported_extensions;
			st->drsuapi->remote_info28.site_guid		= info24->site_guid;
			st->drsuapi->remote_info28.pid			= info24->pid;
			st->drsuapi->remote_info28.repl_epoch		= 0;
			break;
		}
		case 48: {
			struct drsuapi_DsBindInfo48 *info48;
			info48 = &st->bind_r.out.bind_info->info.info48;
			st->drsuapi->remote_info28.supported_extensions	= info48->supported_extensions;
			st->drsuapi->remote_info28.site_guid		= info48->site_guid;
			st->drsuapi->remote_info28.pid			= info48->pid;
			st->drsuapi->remote_info28.repl_epoch		= info48->repl_epoch;
			break;
		}
		case 28:
			st->drsuapi->remote_info28 = st->bind_r.out.bind_info->info.info28;
			break;
		}
	}

	composite_done(c);
}

NTSTATUS dreplsrv_out_drsuapi_recv(struct composite_context *c)
{
	NTSTATUS status;
	struct dreplsrv_out_drsuapi_state *st = talloc_get_type(c->private_data,
						struct dreplsrv_out_drsuapi_state);

	status = composite_wait(c);

	if (NT_STATUS_IS_OK(status)) {
		st->conn->drsuapi = talloc_steal(st->conn, st->drsuapi);
	}

	talloc_free(c);
	return status;
}

struct dreplsrv_op_pull_source_state {
	struct composite_context *creq;

	struct dreplsrv_out_operation *op;

	struct dreplsrv_drsuapi_connection *drsuapi;

	bool have_all;

	uint32_t ctr_level;
	struct drsuapi_DsGetNCChangesCtr1 *ctr1;
	struct drsuapi_DsGetNCChangesCtr6 *ctr6;
};

static void dreplsrv_op_pull_source_connect_recv(struct composite_context *creq);

struct composite_context *dreplsrv_op_pull_source_send(struct dreplsrv_out_operation *op)
{
	struct composite_context *c;
	struct composite_context *creq;
	struct dreplsrv_op_pull_source_state *st;

	c = composite_create(op, op->service->task->event_ctx);
	if (c == NULL) return NULL;

	st = talloc_zero(c, struct dreplsrv_op_pull_source_state);
	if (composite_nomem(st, c)) return c;

	st->creq	= c;
	st->op		= op;

	creq = dreplsrv_out_drsuapi_send(op->source_dsa->conn);
	composite_continue(c, creq, dreplsrv_op_pull_source_connect_recv, st);

	return c;
}

static void dreplsrv_op_pull_source_get_changes_send(struct dreplsrv_op_pull_source_state *st);

static void dreplsrv_op_pull_source_connect_recv(struct composite_context *creq)
{
	struct dreplsrv_op_pull_source_state *st = talloc_get_type(creq->async.private_data,
						   struct dreplsrv_op_pull_source_state);
	struct composite_context *c = st->creq;

	c->status = dreplsrv_out_drsuapi_recv(creq);
	if (!composite_is_ok(c)) return;

	dreplsrv_op_pull_source_get_changes_send(st);
}

static void dreplsrv_op_pull_source_get_changes_recv(struct rpc_request *req);

static void dreplsrv_op_pull_source_get_changes_send(struct dreplsrv_op_pull_source_state *st)
{
	struct composite_context *c = st->creq;
	struct repsFromTo1 *rf1 = st->op->source_dsa->repsFrom1;
	struct dreplsrv_service *service = st->op->service;
	struct dreplsrv_partition *partition = st->op->source_dsa->partition;
	struct dreplsrv_drsuapi_connection *drsuapi = st->op->source_dsa->conn->drsuapi;
	struct rpc_request *req;
	struct drsuapi_DsGetNCChanges *r;

	r = talloc(st, struct drsuapi_DsGetNCChanges);
	if (composite_nomem(r, c)) return;

	r->out.level_out = talloc(r, int32_t);
	if (composite_nomem(r->out.level_out, c)) return;
	r->in.req = talloc(r, union drsuapi_DsGetNCChangesRequest);
	if (composite_nomem(r->in.req, c)) return;
	r->out.ctr = talloc(r, union drsuapi_DsGetNCChangesCtr);
	if (composite_nomem(r->out.ctr, c)) return;

	r->in.bind_handle	= &drsuapi->bind_handle;
	if (drsuapi->remote_info28.supported_extensions & DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8) {
		r->in.level				= 8;
		r->in.req->req8.destination_dsa_guid	= service->ntds_guid;
		r->in.req->req8.source_dsa_invocation_id= rf1->source_dsa_invocation_id;
		r->in.req->req8.naming_context		= &partition->nc;
		r->in.req->req8.highwatermark		= rf1->highwatermark;
		r->in.req->req8.uptodateness_vector	= NULL;/*&partition->uptodatevector_ex;*/
		r->in.req->req8.replica_flags		= rf1->replica_flags;
		r->in.req->req8.max_object_count	= 133;
		r->in.req->req8.max_ndr_size		= 1336811;
		r->in.req->req8.extended_op		= DRSUAPI_EXOP_NONE;
		r->in.req->req8.fsmo_info		= 0;
		r->in.req->req8.partial_attribute_set	= NULL;
		r->in.req->req8.partial_attribute_set_ex= NULL;
		r->in.req->req8.mapping_ctr.num_mappings= 0;
		r->in.req->req8.mapping_ctr.mappings	= NULL;
	} else {
		r->in.level				= 5;
		r->in.req->req5.destination_dsa_guid	= service->ntds_guid;
		r->in.req->req5.source_dsa_invocation_id= rf1->source_dsa_invocation_id;
		r->in.req->req5.naming_context		= &partition->nc;
		r->in.req->req5.highwatermark		= rf1->highwatermark;
		r->in.req->req5.uptodateness_vector	= NULL;/*&partition->uptodatevector_ex;*/
		r->in.req->req5.replica_flags		= rf1->replica_flags;
		r->in.req->req5.max_object_count	= 133;
		r->in.req->req5.max_ndr_size		= 1336770;
		r->in.req->req5.extended_op		= DRSUAPI_EXOP_NONE;
		r->in.req->req5.fsmo_info		= 0;
	}

	req = dcerpc_drsuapi_DsGetNCChanges_send(drsuapi->pipe, r, r);
	composite_continue_rpc(c, req, dreplsrv_op_pull_source_get_changes_recv, st);
}

static void dreplsrv_op_pull_source_apply_changes_send(struct dreplsrv_op_pull_source_state *st,
						       struct drsuapi_DsGetNCChanges *r,
						       uint32_t ctr_level,
						       struct drsuapi_DsGetNCChangesCtr1 *ctr1,
						       struct drsuapi_DsGetNCChangesCtr6 *ctr6);

static void dreplsrv_op_pull_source_get_changes_recv(struct rpc_request *req)
{
	struct dreplsrv_op_pull_source_state *st = talloc_get_type(req->async.private_data,
						   struct dreplsrv_op_pull_source_state);
	struct composite_context *c = st->creq;
	struct drsuapi_DsGetNCChanges *r = talloc_get_type(req->ndr.struct_ptr,
					   struct drsuapi_DsGetNCChanges);
	uint32_t ctr_level = 0;
	struct drsuapi_DsGetNCChangesCtr1 *ctr1 = NULL;
	struct drsuapi_DsGetNCChangesCtr6 *ctr6 = NULL;

	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	if (!W_ERROR_IS_OK(r->out.result)) {
		composite_error(c, werror_to_ntstatus(r->out.result));
		return;
	}

	if (*r->out.level_out == 1) {
		ctr_level = 1;
		ctr1 = &r->out.ctr->ctr1;
	} else if (*r->out.level_out == 2 &&
		   r->out.ctr->ctr2.mszip1.ts) {
		ctr_level = 1;
		ctr1 = &r->out.ctr->ctr2.mszip1.ts->ctr1;
	} else if (*r->out.level_out == 6) {
		ctr_level = 6;
		ctr6 = &r->out.ctr->ctr6;
	} else if (*r->out.level_out == 7 &&
		   r->out.ctr->ctr7.level == 6 &&
		   r->out.ctr->ctr7.type == DRSUAPI_COMPRESSION_TYPE_MSZIP &&
		   r->out.ctr->ctr7.ctr.mszip6.ts) {
		ctr_level = 6;
		ctr6 = &r->out.ctr->ctr7.ctr.mszip6.ts->ctr6;
	} else if (*r->out.level_out == 7 &&
		   r->out.ctr->ctr7.level == 6 &&
		   r->out.ctr->ctr7.type == DRSUAPI_COMPRESSION_TYPE_XPRESS &&
		   r->out.ctr->ctr7.ctr.xpress6.ts) {
		ctr_level = 6;
		ctr6 = &r->out.ctr->ctr7.ctr.xpress6.ts->ctr6;
	} else {
		composite_error(c, werror_to_ntstatus(WERR_BAD_NET_RESP));
		return;
	}

	if (!ctr1 && !ctr6) {
		composite_error(c, werror_to_ntstatus(WERR_BAD_NET_RESP));
		return;
	}

	if (ctr_level == 6) {
		if (!W_ERROR_IS_OK(ctr6->drs_error)) {
			composite_error(c, werror_to_ntstatus(ctr6->drs_error));
			return;
		}
	}

	dreplsrv_op_pull_source_apply_changes_send(st, r, ctr_level, ctr1, ctr6);
}

static void dreplsrv_update_refs_send(struct dreplsrv_op_pull_source_state *st);

static void dreplsrv_op_pull_source_apply_changes_send(struct dreplsrv_op_pull_source_state *st,
						       struct drsuapi_DsGetNCChanges *r,
						       uint32_t ctr_level,
						       struct drsuapi_DsGetNCChangesCtr1 *ctr1,
						       struct drsuapi_DsGetNCChangesCtr6 *ctr6)
{
	struct composite_context *c = st->creq;
	struct repsFromTo1 rf1 = *st->op->source_dsa->repsFrom1;
	struct dreplsrv_service *service = st->op->service;
	struct dreplsrv_partition *partition = st->op->source_dsa->partition;
	struct dreplsrv_drsuapi_connection *drsuapi = st->op->source_dsa->conn->drsuapi;
	const struct drsuapi_DsReplicaOIDMapping_Ctr *mapping_ctr;
	uint32_t object_count;
	struct drsuapi_DsReplicaObjectListItemEx *first_object;
	uint32_t linked_attributes_count;
	struct drsuapi_DsReplicaLinkedAttribute *linked_attributes;
	const struct drsuapi_DsReplicaCursor2CtrEx *uptodateness_vector;
	bool more_data = false;
	WERROR status;

	switch (ctr_level) {
	case 1:
		mapping_ctr			= &ctr1->mapping_ctr;
		object_count			= ctr1->object_count;
		first_object			= ctr1->first_object;
		linked_attributes_count		= 0;
		linked_attributes		= NULL;
		rf1.highwatermark		= ctr1->new_highwatermark;
		uptodateness_vector		= NULL; /* TODO: map it */
		more_data			= ctr1->more_data;
		break;
	case 6:
		mapping_ctr			= &ctr6->mapping_ctr;
		object_count			= ctr6->object_count;
		first_object			= ctr6->first_object;
		linked_attributes_count		= ctr6->linked_attributes_count;
		linked_attributes		= ctr6->linked_attributes;
		rf1.highwatermark		= ctr6->new_highwatermark;
		uptodateness_vector		= ctr6->uptodateness_vector;
		more_data			= ctr6->more_data;
		break;
	default:
		composite_error(c, werror_to_ntstatus(WERR_BAD_NET_RESP));
		return;
	}

	status = dsdb_extended_replicated_objects_commit(service->samdb,
							 partition->nc.dn,
							 mapping_ctr,
							 object_count,
							 first_object,
							 linked_attributes_count,
							 linked_attributes,
							 &rf1,
							 uptodateness_vector,
							 &drsuapi->gensec_skey,
							 st, NULL, 
							 &st->op->source_dsa->notify_uSN);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("Failed to commit objects: %s\n", win_errstr(status)));
		composite_error(c, werror_to_ntstatus(status));
		return;
	}

	/* if it applied fine, we need to update the highwatermark */
	*st->op->source_dsa->repsFrom1 = rf1;

	/*
	 * TODO: update our uptodatevector!
	 */

	if (more_data) {
		dreplsrv_op_pull_source_get_changes_send(st);
		return;
	}

	/* now we need to update the repsTo record for this partition
	   on the server. These records are initially established when
	   we join the domain, but they quickly expire.  We do it here
	   so we can use the already established DRSUAPI pipe
	*/
	dreplsrv_update_refs_send(st);
}

WERROR dreplsrv_op_pull_source_recv(struct composite_context *c)
{
	NTSTATUS status;

	status = composite_wait(c);

	talloc_free(c);
	return ntstatus_to_werror(status);
}

/*
  receive a UpdateRefs reply
 */
static void dreplsrv_update_refs_recv(struct rpc_request *req)
{
	struct dreplsrv_op_pull_source_state *st = talloc_get_type(req->async.private_data,
						   struct dreplsrv_op_pull_source_state);
	struct composite_context *c = st->creq;
	struct drsuapi_DsReplicaUpdateRefs *r = talloc_get_type(req->ndr.struct_ptr,
								struct drsuapi_DsReplicaUpdateRefs);

	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) {
		DEBUG(0,("UpdateRefs failed with %s\n", 
			 nt_errstr(c->status)));
		return;
	}

	if (!W_ERROR_IS_OK(r->out.result)) {
		DEBUG(0,("UpdateRefs failed with %s for %s %s\n", 
			 win_errstr(r->out.result),
			 r->in.req.req1.dest_dsa_dns_name,
			 r->in.req.req1.naming_context->dn));
		composite_error(c, werror_to_ntstatus(r->out.result));
		return;
	}

	DEBUG(4,("UpdateRefs OK for %s %s\n", 
		 r->in.req.req1.dest_dsa_dns_name,
		 r->in.req.req1.naming_context->dn));

	composite_done(c);
}

/*
  send a UpdateRefs request to refresh our repsTo record on the server
 */
static void dreplsrv_update_refs_send(struct dreplsrv_op_pull_source_state *st)
{
	struct composite_context *c = st->creq;
	struct dreplsrv_service *service = st->op->service;
	struct dreplsrv_partition *partition = st->op->source_dsa->partition;
	struct dreplsrv_drsuapi_connection *drsuapi = st->op->source_dsa->conn->drsuapi;
	struct rpc_request *req;
	struct drsuapi_DsReplicaUpdateRefs *r;
	char *ntds_guid_str;
	char *ntds_dns_name;

	r = talloc(st, struct drsuapi_DsReplicaUpdateRefs);
	if (composite_nomem(r, c)) return;

	ntds_guid_str = GUID_string(r, &service->ntds_guid);
	if (composite_nomem(ntds_guid_str, c)) return;

	/* lp_realm() is not really right here */
	ntds_dns_name = talloc_asprintf(r, "%s._msdcs.%s",
					ntds_guid_str,
					lp_realm(service->task->lp_ctx));
	if (composite_nomem(ntds_dns_name, c)) return;

	r->in.bind_handle	= &drsuapi->bind_handle;
	r->in.level             = 1;
	r->in.req.req1.naming_context	  = &partition->nc;
	r->in.req.req1.dest_dsa_dns_name  = ntds_dns_name;
	r->in.req.req1.dest_dsa_guid	  = service->ntds_guid;
	r->in.req.req1.options	          = 
		DRSUAPI_DS_REPLICA_UPDATE_ADD_REFERENCE |
		DRSUAPI_DS_REPLICA_UPDATE_DELETE_REFERENCE;
	if (!lp_parm_bool(service->task->lp_ctx, NULL, "repl", "RODC", false)) {
		r->in.req.req1.options |= DRSUAPI_DS_REPLICA_UPDATE_WRITEABLE;
	}

	req = dcerpc_drsuapi_DsReplicaUpdateRefs_send(drsuapi->pipe, r, r);
	composite_continue_rpc(c, req, dreplsrv_update_refs_recv, st);
}
