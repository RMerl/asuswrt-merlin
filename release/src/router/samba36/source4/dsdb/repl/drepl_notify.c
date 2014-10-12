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
#include "dsdb/repl/drepl_service.h"
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "libcli/composite/composite.h"
#include "../lib/util/tevent_ntstatus.h"


struct dreplsrv_op_notify_state {
	struct tevent_context *ev;
	struct dreplsrv_notify_operation *op;
	void *ndr_struct_ptr;
};

static void dreplsrv_op_notify_connect_done(struct tevent_req *subreq);

/*
  start the ReplicaSync async call
 */
static struct tevent_req *dreplsrv_op_notify_send(TALLOC_CTX *mem_ctx,
						  struct tevent_context *ev,
						  struct dreplsrv_notify_operation *op)
{
	struct tevent_req *req;
	struct dreplsrv_op_notify_state *state;
	struct tevent_req *subreq;

	req = tevent_req_create(mem_ctx, &state,
				struct dreplsrv_op_notify_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->op = op;

	subreq = dreplsrv_out_drsuapi_send(state,
					   ev,
					   op->source_dsa->conn);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, dreplsrv_op_notify_connect_done, req);

	return req;
}

static void dreplsrv_op_notify_replica_sync_trigger(struct tevent_req *req);

static void dreplsrv_op_notify_connect_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(subreq,
							  struct tevent_req);
	NTSTATUS status;

	status = dreplsrv_out_drsuapi_recv(subreq);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	dreplsrv_op_notify_replica_sync_trigger(req);
}

static void dreplsrv_op_notify_replica_sync_done(struct tevent_req *subreq);

static void dreplsrv_op_notify_replica_sync_trigger(struct tevent_req *req)
{
	struct dreplsrv_op_notify_state *state =
		tevent_req_data(req,
		struct dreplsrv_op_notify_state);
	struct dreplsrv_partition *partition = state->op->source_dsa->partition;
	struct dreplsrv_drsuapi_connection *drsuapi = state->op->source_dsa->conn->drsuapi;
	struct drsuapi_DsReplicaSync *r;
	struct tevent_req *subreq;

	r = talloc_zero(state, struct drsuapi_DsReplicaSync);
	if (tevent_req_nomem(r, req)) {
		return;
	}
	r->in.req = talloc_zero(r, union drsuapi_DsReplicaSyncRequest);
	if (tevent_req_nomem(r, req)) {
		return;
	}
	r->in.bind_handle	= &drsuapi->bind_handle;
	r->in.level = 1;
	r->in.req->req1.naming_context = &partition->nc;
	r->in.req->req1.source_dsa_guid = state->op->service->ntds_guid;
	r->in.req->req1.options =
		DRSUAPI_DRS_ASYNC_OP |
		DRSUAPI_DRS_UPDATE_NOTIFICATION |
		DRSUAPI_DRS_WRIT_REP;

	if (state->op->is_urgent) {
		r->in.req->req1.options |= DRSUAPI_DRS_SYNC_URGENT;
	}

	state->ndr_struct_ptr = r;

	if (DEBUGLVL(10)) {
		NDR_PRINT_IN_DEBUG(drsuapi_DsReplicaSync, r);
	}

	subreq = dcerpc_drsuapi_DsReplicaSync_r_send(state,
						     state->ev,
						     drsuapi->drsuapi_handle,
						     r);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, dreplsrv_op_notify_replica_sync_done, req);
}

static void dreplsrv_op_notify_replica_sync_done(struct tevent_req *subreq)
{
	struct tevent_req *req =
		tevent_req_callback_data(subreq,
		struct tevent_req);
	struct dreplsrv_op_notify_state *state =
		tevent_req_data(req,
		struct dreplsrv_op_notify_state);
	struct drsuapi_DsReplicaSync *r = talloc_get_type(state->ndr_struct_ptr,
							  struct drsuapi_DsReplicaSync);
	NTSTATUS status;

	state->ndr_struct_ptr = NULL;

	status = dcerpc_drsuapi_DsReplicaSync_r_recv(subreq, r);
	TALLOC_FREE(subreq);
	if (tevent_req_nterror(req, status)) {
		return;
	}

	if (!W_ERROR_IS_OK(r->out.result)) {
		status = werror_to_ntstatus(r->out.result);
		tevent_req_nterror(req, status);
		return;
	}

	tevent_req_done(req);
}

static NTSTATUS dreplsrv_op_notify_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

/*
  called when a notify operation has completed
 */
static void dreplsrv_notify_op_callback(struct tevent_req *subreq)
{
	struct dreplsrv_notify_operation *op =
		tevent_req_callback_data(subreq,
		struct dreplsrv_notify_operation);
	NTSTATUS status;
	struct dreplsrv_service *s = op->service;
	WERROR werr;

	status = dreplsrv_op_notify_recv(subreq);
	werr = ntstatus_to_werror(status);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(4,("dreplsrv_notify: Failed to send DsReplicaSync to %s for %s - %s : %s\n",
			 op->source_dsa->repsFrom1->other_info->dns_name,
			 ldb_dn_get_linearized(op->source_dsa->partition->dn),
			 nt_errstr(status), win_errstr(werr)));
	} else {
		DEBUG(2,("dreplsrv_notify: DsReplicaSync OK for %s\n",
			 op->source_dsa->repsFrom1->other_info->dns_name));
		op->source_dsa->notify_uSN = op->uSN;
	}

	drepl_reps_update(s, "repsTo", op->source_dsa->partition->dn,
			  &op->source_dsa->repsFrom1->source_dsa_obj_guid,
			  werr);

	talloc_free(op);
	s->ops.n_current = NULL;
	dreplsrv_run_pending_ops(s);
}

/*
  run any pending replica sync calls
 */
void dreplsrv_notify_run_ops(struct dreplsrv_service *s)
{
	struct dreplsrv_notify_operation *op;
	struct tevent_req *subreq;

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

	subreq = dreplsrv_op_notify_send(op, s->task->event_ctx, op);
	if (!subreq) {
		DEBUG(0,("dreplsrv_notify_run_ops: dreplsrv_op_notify_send[%s][%s] - no memory\n",
			 op->source_dsa->repsFrom1->other_info->dns_name,
			 ldb_dn_get_linearized(op->source_dsa->partition->dn)));
		return;
	}
	tevent_req_set_callback(subreq, dreplsrv_notify_op_callback, op);
	DEBUG(4,("started DsReplicaSync for %s to %s\n",
		 ldb_dn_get_linearized(op->source_dsa->partition->dn),
		 op->source_dsa->repsFrom1->other_info->dns_name));
}


/*
  find a source_dsa for a given guid
 */
static struct dreplsrv_partition_source_dsa *dreplsrv_find_notify_dsa(struct dreplsrv_partition *p,
								      struct GUID *guid)
{
	struct dreplsrv_partition_source_dsa *s;

	/* first check the sources list */
	for (s=p->sources; s; s=s->next) {
		if (GUID_compare(&s->repsFrom1->source_dsa_obj_guid, guid) == 0) {
			return s;
		}
	}

	/* then the notifies list */
	for (s=p->notifies; s; s=s->next) {
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
					    uint64_t uSN,
					    bool is_urgent,
					    uint32_t replica_flags)
{
	struct dreplsrv_notify_operation *op;
	struct dreplsrv_partition_source_dsa *s;

	s = dreplsrv_find_notify_dsa(p, &reps->ctr.ctr1.source_dsa_obj_guid);
	if (s == NULL) {
		DEBUG(0,(__location__ ": Unable to find source_dsa for %s\n",
			 GUID_string(mem_ctx, &reps->ctr.ctr1.source_dsa_obj_guid)));
		return WERR_DS_UNAVAILABLE;
	}

	/* first try to find an existing notify operation */
	for (op = service->ops.notifies; op; op = op->next) {
		if (op->source_dsa != s) {
			continue;
		}

		if (op->is_urgent != is_urgent) {
			continue;
		}

		if (op->replica_flags != replica_flags) {
			continue;
		}

		if (op->uSN < uSN) {
			op->uSN = uSN;
		}

		/* reuse the notify operation, as it's not yet started */
		return WERR_OK;
	}

	op = talloc_zero(mem_ctx, struct dreplsrv_notify_operation);
	W_ERROR_HAVE_NO_MEMORY(op);

	op->service	  = service;
	op->source_dsa	  = s;
	op->uSN           = uSN;
	op->is_urgent	  = is_urgent;
	op->replica_flags = replica_flags;
	op->schedule_time = time(NULL);

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
	uint64_t uSNHighest;
	uint64_t uSNUrgent;
	uint32_t i;
	int ret;

	werr = dsdb_loadreps(s->samdb, mem_ctx, p->dn, "repsTo", &reps, &count);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": Failed to load repsTo for %s\n",
			 ldb_dn_get_linearized(p->dn)));
		return werr;
	}

	/* loads the partition uSNHighest and uSNUrgent */
	ret = dsdb_load_partition_usn(s->samdb, p->dn, &uSNHighest, &uSNUrgent);
	if (ret != LDB_SUCCESS || uSNHighest == 0) {
		/* nothing to do */
		return WERR_OK;
	}

	/* see if any of our partners need some of our objects */
	for (i=0; i<count; i++) {
		struct dreplsrv_partition_source_dsa *sdsa;
		uint32_t replica_flags;
		sdsa = dreplsrv_find_notify_dsa(p, &reps[i].ctr.ctr1.source_dsa_obj_guid);
		replica_flags = reps[i].ctr.ctr1.replica_flags;
		if (sdsa == NULL) continue;
		if (sdsa->notify_uSN < uSNHighest) {
			/* we need to tell this partner to replicate
			   with us */
			bool is_urgent = sdsa->notify_uSN < uSNUrgent;

			/* check if urgent replication is needed */
			werr = dreplsrv_schedule_notify_sync(s, p, &reps[i], mem_ctx,
							     uSNHighest, is_urgent, replica_flags);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,(__location__ ": Failed to setup notify to %s for %s\n",
					 reps[i].ctr.ctr1.other_info->dns_name,
					 ldb_dn_get_linearized(p->dn)));
				return werr;
			}
			DEBUG(4,("queued DsReplicaSync for %s to %s (urgent=%s) uSN=%llu:%llu\n",
				 ldb_dn_get_linearized(p->dn),
				 reps[i].ctr.ctr1.other_info->dns_name,
				 is_urgent?"true":"false",
				 (unsigned long long)sdsa->notify_uSN,
				 (unsigned long long)uSNHighest));
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
	DEBUG(4,("dreplsrv_notify_schedule(%u) %sscheduled for: %s\n",
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
}
