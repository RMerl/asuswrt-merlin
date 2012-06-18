/* 
   Unix SMB/CIFS mplementation.

   DSDB replication service periodic notification handling
   
   Copyright (C) Andrew Tridgell 2009
   based on drepl_periodic
    
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
#include "lib/events/events.h"
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "lib/messaging/irpc.h"
#include "dsdb/repl/drepl_service.h"
#include "lib/ldb/include/ldb_errors.h"
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "libcli/composite/composite.h"


struct dreplsrv_op_notify_state {
	struct composite_context *creq;

	struct dreplsrv_out_connection *conn;

	struct dreplsrv_drsuapi_connection *drsuapi;

	struct drsuapi_DsBindInfoCtr bind_info_ctr;
	struct drsuapi_DsBind bind_r;
	struct dreplsrv_notify_operation *op;
};

/*
  receive a DsReplicaSync reply
 */
static void dreplsrv_op_notify_replica_sync_recv(struct rpc_request *req)
{
	struct dreplsrv_op_notify_state *st = talloc_get_type(req->async.private_data,
							      struct dreplsrv_op_notify_state);
	struct composite_context *c = st->creq;
	struct drsuapi_DsReplicaSync *r = talloc_get_type(req->ndr.struct_ptr,
							  struct drsuapi_DsReplicaSync);

	c->status = dcerpc_ndr_request_recv(req);
	if (!composite_is_ok(c)) return;

	if (!W_ERROR_IS_OK(r->out.result)) {
		composite_error(c, werror_to_ntstatus(r->out.result));
		return;
	}

	composite_done(c);
}

/*
  send a DsReplicaSync
*/
static void dreplsrv_op_notify_replica_sync_send(struct dreplsrv_op_notify_state *st)
{
	struct composite_context *c = st->creq;
	struct dreplsrv_partition *partition = st->op->source_dsa->partition;
	struct dreplsrv_drsuapi_connection *drsuapi = st->op->source_dsa->conn->drsuapi;
	struct rpc_request *req;
	struct drsuapi_DsReplicaSync *r;

	r = talloc_zero(st, struct drsuapi_DsReplicaSync);
	if (composite_nomem(r, c)) return;

	r->in.bind_handle	= &drsuapi->bind_handle;
	r->in.level = 1;
	r->in.req.req1.naming_context = &partition->nc;
	r->in.req.req1.source_dsa_guid = st->op->service->ntds_guid;
	r->in.req.req1.options = 
		DRSUAPI_DS_REPLICA_SYNC_ASYNCHRONOUS_OPERATION |
		DRSUAPI_DS_REPLICA_SYNC_WRITEABLE |
		DRSUAPI_DS_REPLICA_SYNC_ALL_SOURCES;
	

	req = dcerpc_drsuapi_DsReplicaSync_send(drsuapi->pipe, r, r);
	composite_continue_rpc(c, req, dreplsrv_op_notify_replica_sync_recv, st);
}

/*
  called when we have an established connection
 */
static void dreplsrv_op_notify_connect_recv(struct composite_context *creq)
{
	struct dreplsrv_op_notify_state *st = talloc_get_type(creq->async.private_data,
							      struct dreplsrv_op_notify_state);
	struct composite_context *c = st->creq;

	c->status = dreplsrv_out_drsuapi_recv(creq);
	if (!composite_is_ok(c)) return;

	dreplsrv_op_notify_replica_sync_send(st);
}

/*
  start the ReplicaSync async call
 */
static struct composite_context *dreplsrv_op_notify_send(struct dreplsrv_notify_operation *op)
{
	struct composite_context *c;
	struct composite_context *creq;
	struct dreplsrv_op_notify_state *st;

	c = composite_create(op, op->service->task->event_ctx);
	if (c == NULL) return NULL;

	st = talloc_zero(c, struct dreplsrv_op_notify_state);
	if (composite_nomem(st, c)) return c;

	st->creq	= c;
	st->op		= op;

	creq = dreplsrv_out_drsuapi_send(op->source_dsa->conn);
	composite_continue(c, creq, dreplsrv_op_notify_connect_recv, st);

	return c;
}

static void dreplsrv_notify_del_repsTo(struct dreplsrv_notify_operation *op)
{
	uint32_t count;
	struct repsFromToBlob *reps;
	WERROR werr;
	struct dreplsrv_service *s = op->service;
	int i;

	werr = dsdb_loadreps(s->samdb, op, op->source_dsa->partition->dn, "repsTo", &reps, &count);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": Failed to load repsTo for %s\n",
			 ldb_dn_get_linearized(op->source_dsa->partition->dn)));
		return;
	}

	for (i=0; i<count; i++) {
		if (GUID_compare(&reps[i].ctr.ctr1.source_dsa_obj_guid, 
				 &op->source_dsa->repsFrom1->source_dsa_obj_guid) == 0) {
			memmove(&reps[i], &reps[i+1],
				sizeof(reps[i])*(count-(i+1)));
			count--;
		}
	}

	werr = dsdb_savereps(s->samdb, op, op->source_dsa->partition->dn, "repsTo", reps, count);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": Failed to save repsTo for %s\n",
			 ldb_dn_get_linearized(op->source_dsa->partition->dn)));
		return;
	}
}

/*
  called when a notify operation has completed
 */
static void dreplsrv_notify_op_callback(struct dreplsrv_notify_operation *op)
{
	NTSTATUS status;
	struct dreplsrv_service *s = op->service;

	status = composite_wait(op->creq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("dreplsrv_notify: Failed to send DsReplicaSync to %s for %s - %s\n",
			 op->source_dsa->repsFrom1->other_info->dns_name,
			 ldb_dn_get_linearized(op->source_dsa->partition->dn),
			 nt_errstr(status)));
	} else {
		DEBUG(2,("dreplsrv_notify: DsReplicaSync OK for %s\n",
			 op->source_dsa->repsFrom1->other_info->dns_name));
		op->source_dsa->notify_uSN = op->uSN;
		/* delete the repsTo for this replication partner in the
		   partition, as we have successfully told him to sync */
		dreplsrv_notify_del_repsTo(op);
	}
	talloc_free(op->creq);

	talloc_free(op);
	s->ops.n_current = NULL;
	dreplsrv_notify_run_ops(s);
}


static void dreplsrv_notify_op_callback_creq(struct composite_context *creq)
{
	struct dreplsrv_notify_operation *op = talloc_get_type(creq->async.private_data,
							       struct dreplsrv_notify_operation);
	dreplsrv_notify_op_callback(op);
}

/*
  run any pending replica sync calls
 */
void dreplsrv_notify_run_ops(struct dreplsrv_service *s)
{
	struct dreplsrv_notify_operation *op;

	if (s->ops.n_current || s->ops.current) {
		/* if there's still one running, we're done */
		return;
	}

	if (!s->ops.notifies) {
		/* if there're no pending operations, we're done */
		return;
	}

	op = s->ops.notifies;
	s->ops.n_current = op;
	DLIST_REMOVE(s->ops.notifies, op);

	op->creq = dreplsrv_op_notify_send(op);
	if (!op->creq) {
		dreplsrv_notify_op_callback(op);
		return;
	}

	op->creq->async.fn		= dreplsrv_notify_op_callback_creq;
	op->creq->async.private_data	= op;
}


/*
  find a source_dsa for a given guid
 */
static struct dreplsrv_partition_source_dsa *dreplsrv_find_source_dsa(struct dreplsrv_partition *p,
								      struct GUID *guid)
{
	struct dreplsrv_partition_source_dsa *s;

	for (s=p->sources; s; s=s->next) {
		if (GUID_compare(&s->repsFrom1->source_dsa_obj_guid, guid) == 0) {
			return s;
		}
	}
	return NULL;
}


/*
  schedule a replicaSync message
 */
static WERROR dreplsrv_schedule_notify_sync(struct dreplsrv_service *service,
					    struct dreplsrv_partition *p,
					    struct repsFromToBlob *reps,
					    TALLOC_CTX *mem_ctx,
					    uint64_t uSN)
{
	struct dreplsrv_notify_operation *op;
	struct dreplsrv_partition_source_dsa *s;

	s = dreplsrv_find_source_dsa(p, &reps->ctr.ctr1.source_dsa_obj_guid);
	if (s == NULL) {
		DEBUG(0,(__location__ ": Unable to find source_dsa for %s\n",
			 GUID_string(mem_ctx, &reps->ctr.ctr1.source_dsa_obj_guid)));
		return WERR_DS_UNAVAILABLE;
	}

	op = talloc_zero(mem_ctx, struct dreplsrv_notify_operation);
	W_ERROR_HAVE_NO_MEMORY(op);

	op->service	= service;
	op->source_dsa	= s;
	op->uSN         = uSN;

	DLIST_ADD_END(service->ops.notifies, op, struct dreplsrv_notify_operation *);
	talloc_steal(service, op);
	return WERR_OK;
}

/*
  see if a partition has a hugher uSN than what is in the repsTo and
  if so then send a DsReplicaSync
 */
static WERROR dreplsrv_notify_check(struct dreplsrv_service *s, 
				    struct dreplsrv_partition *p,
				    TALLOC_CTX *mem_ctx)
{
	uint32_t count=0;
	struct repsFromToBlob *reps;
	WERROR werr;
	uint64_t uSN;
	int ret, i;

	werr = dsdb_loadreps(s->samdb, mem_ctx, p->dn, "repsTo", &reps, &count);
	if (count == 0) {
		werr = dsdb_loadreps(s->samdb, mem_ctx, p->dn, "repsFrom", &reps, &count);
	}
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": Failed to load repsTo for %s\n",
			 ldb_dn_get_linearized(p->dn)));
		return werr;
	}

	/* loads the partition uSNHighest */
	ret = dsdb_load_partition_usn(s->samdb, p->dn, &uSN);
	if (ret != LDB_SUCCESS || uSN == 0) {
		/* nothing to do */
		return WERR_OK;
	}

	/* see if any of our partners need some of our objects */
	for (i=0; i<count; i++) {
		struct dreplsrv_partition_source_dsa *sdsa;
		sdsa = dreplsrv_find_source_dsa(p, &reps[i].ctr.ctr1.source_dsa_obj_guid);
		if (sdsa == NULL) continue;
		if (sdsa->notify_uSN < uSN) {
			/* we need to tell this partner to replicate
			   with us */
			werr = dreplsrv_schedule_notify_sync(s, p, &reps[i], mem_ctx, uSN);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,(__location__ ": Failed to setup notify to %s for %s\n",
					 reps[i].ctr.ctr1.other_info->dns_name,
					 ldb_dn_get_linearized(p->dn)));
				return werr;
			}
		}
	}

	return WERR_OK;
}

/*
  see if any of the partitions have changed, and if so then send a
  DsReplicaSync to all the replica partners in the repsTo object
 */
static WERROR dreplsrv_notify_check_all(struct dreplsrv_service *s, TALLOC_CTX *mem_ctx)
{
	WERROR status;
	struct dreplsrv_partition *p;

	for (p = s->partitions; p; p = p->next) {
		status = dreplsrv_notify_check(s, p, mem_ctx);
		W_ERROR_NOT_OK_RETURN(status);
	}

	return WERR_OK;
}

static void dreplsrv_notify_run(struct dreplsrv_service *service);

static void dreplsrv_notify_handler_te(struct tevent_context *ev, struct tevent_timer *te,
				       struct timeval t, void *ptr)
{
	struct dreplsrv_service *service = talloc_get_type(ptr, struct dreplsrv_service);
	WERROR status;

	service->notify.te = NULL;

	dreplsrv_notify_run(service);

	status = dreplsrv_notify_schedule(service, service->notify.interval);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(service->task, win_errstr(status), false);
		return;
	}
}

WERROR dreplsrv_notify_schedule(struct dreplsrv_service *service, uint32_t next_interval)
{
	TALLOC_CTX *tmp_mem;
	struct tevent_timer *new_te;
	struct timeval next_time;

	/* prevent looping */
	if (next_interval == 0) next_interval = 1;

	next_time = timeval_current_ofs(next_interval, 50);

	if (service->notify.te) {
		/*
		 * if the timestamp of the new event is higher,
		 * as current next we don't need to reschedule
		 */
		if (timeval_compare(&next_time, &service->notify.next_event) > 0) {
			return WERR_OK;
		}
	}

	/* reset the next scheduled timestamp */
	service->notify.next_event = next_time;

	new_te = event_add_timed(service->task->event_ctx, service,
			         service->notify.next_event,
			         dreplsrv_notify_handler_te, service);
	W_ERROR_HAVE_NO_MEMORY(new_te);

	tmp_mem = talloc_new(service);
	DEBUG(2,("dreplsrv_notify_schedule(%u) %sscheduled for: %s\n",
		next_interval,
		(service->notify.te?"re":""),
		nt_time_string(tmp_mem, timeval_to_nttime(&next_time))));
	talloc_free(tmp_mem);

	talloc_free(service->notify.te);
	service->notify.te = new_te;

	return WERR_OK;
}

static void dreplsrv_notify_run(struct dreplsrv_service *service)
{
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_new(service);
	dreplsrv_notify_check_all(service, mem_ctx);
	talloc_free(mem_ctx);

	dreplsrv_run_pending_ops(service);
	dreplsrv_notify_run_ops(service);
}
