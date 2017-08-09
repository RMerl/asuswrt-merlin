/* 
   Unix SMB/CIFS mplementation.
   DSDB replication service outgoing Pull-Replication
   
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
#include "dsdb/repl/drepl_service.h"
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "libcli/composite/composite.h"
#include "libcli/security/security.h"

/*
  update repsFrom/repsTo error information
 */
void drepl_reps_update(struct dreplsrv_service *s, const char *reps_attr,
		       struct ldb_dn *dn,
		       struct GUID *source_dsa_obj_guid, WERROR status)
{
	struct repsFromToBlob *reps;
	uint32_t count, i;
	WERROR werr;
	TALLOC_CTX *tmp_ctx = talloc_new(s);
	time_t t;
	NTTIME now;

	t = time(NULL);
	unix_to_nt_time(&now, t);

	werr = dsdb_loadreps(s->samdb, tmp_ctx, dn, reps_attr, &reps, &count);
	if (!W_ERROR_IS_OK(werr)) {
		talloc_free(tmp_ctx);
		return;
	}

	for (i=0; i<count; i++) {
		if (GUID_compare(source_dsa_obj_guid,
				 &reps[i].ctr.ctr1.source_dsa_obj_guid) == 0) {
			break;
		}
	}

	if (i == count) {
		/* no record to update */
		talloc_free(tmp_ctx);
		return;
	}

	/* only update the status fields */
	reps[i].ctr.ctr1.last_attempt = now;
	reps[i].ctr.ctr1.result_last_attempt = status;
	if (W_ERROR_IS_OK(status)) {
		reps[i].ctr.ctr1.last_success = now;
		reps[i].ctr.ctr1.consecutive_sync_failures = 0;
	} else {
		reps[i].ctr.ctr1.consecutive_sync_failures++;
	}

	werr = dsdb_savereps(s->samdb, tmp_ctx, dn, reps_attr, reps, count);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(2,("drepl_reps_update: Failed to save %s for %s: %s\n",
			 reps_attr, ldb_dn_get_linearized(dn), win_errstr(werr)));
	}
	talloc_free(tmp_ctx);
}

WERROR dreplsrv_schedule_partition_pull_source(struct dreplsrv_service *s,
					       struct dreplsrv_partition_source_dsa *source,
					       uint32_t options,
					       enum drsuapi_DsExtendedOperation extended_op,
					       uint64_t fsmo_info,
					       dreplsrv_extended_callback_t callback,
					       void *cb_data)
{
	struct dreplsrv_out_operation *op;

	op = talloc_zero(s, struct dreplsrv_out_operation);
	W_ERROR_HAVE_NO_MEMORY(op);

	op->service	= s;
	op->source_dsa	= source;
	op->options	= options;
	op->extended_op = extended_op;
	op->fsmo_info   = fsmo_info;
	op->callback    = callback;
	op->cb_data	= cb_data;
	op->schedule_time = time(NULL);

	DLIST_ADD_END(s->ops.pending, op, struct dreplsrv_out_operation *);

	return WERR_OK;
}

static WERROR dreplsrv_schedule_partition_pull(struct dreplsrv_service *s,
					       struct dreplsrv_partition *p,
					       TALLOC_CTX *mem_ctx)
{
	WERROR status;
	struct dreplsrv_partition_source_dsa *cur;

	for (cur = p->sources; cur; cur = cur->next) {
		status = dreplsrv_schedule_partition_pull_source(s, cur,
		                                                 0, DRSUAPI_EXOP_NONE, 0,
		                                                 NULL, NULL);
		W_ERROR_NOT_OK_RETURN(status);
	}

	return WERR_OK;
}

WERROR dreplsrv_schedule_pull_replication(struct dreplsrv_service *s, TALLOC_CTX *mem_ctx)
{
	WERROR status;
	struct dreplsrv_partition *p;

	for (p = s->partitions; p; p = p->next) {
		status = dreplsrv_schedule_partition_pull(s, p, mem_ctx);
		W_ERROR_NOT_OK_RETURN(status);
	}

	return WERR_OK;
}


static void dreplsrv_pending_op_callback(struct tevent_req *subreq)
{
	struct dreplsrv_out_operation *op = tevent_req_callback_data(subreq,
					    struct dreplsrv_out_operation);
	struct repsFromTo1 *rf = op->source_dsa->repsFrom1;
	struct dreplsrv_service *s = op->service;
	WERROR werr;

	werr = dreplsrv_op_pull_source_recv(subreq);
	TALLOC_FREE(subreq);

	DEBUG(4,("dreplsrv_op_pull_source(%s) for %s\n", win_errstr(werr),
		 ldb_dn_get_linearized(op->source_dsa->partition->dn)));

	if (op->extended_op == DRSUAPI_EXOP_NONE) {
		drepl_reps_update(s, "repsFrom", op->source_dsa->partition->dn,
				  &rf->source_dsa_obj_guid, werr);
	}

	if (op->callback) {
		op->callback(s, werr, op->extended_ret, op->cb_data);
	}
	talloc_free(op);
	s->ops.current = NULL;
	dreplsrv_run_pending_ops(s);
}

void dreplsrv_run_pull_ops(struct dreplsrv_service *s)
{
	struct dreplsrv_out_operation *op;
	time_t t;
	NTTIME now;
	struct tevent_req *subreq;
	WERROR werr;

	if (s->ops.current) {
		/* if there's still one running, we're done */
		return;
	}

	if (!s->ops.pending) {
		/* if there're no pending operations, we're done */
		return;
	}

	t = time(NULL);
	unix_to_nt_time(&now, t);

	op = s->ops.pending;
	s->ops.current = op;
	DLIST_REMOVE(s->ops.pending, op);

	op->source_dsa->repsFrom1->last_attempt = now;

	/* check if inbound replication is enabled */
	if (!(op->options & DRSUAPI_DRS_SYNC_FORCED)) {
		uint32_t rep_options;
		if (samdb_ntds_options(op->service->samdb, &rep_options) != LDB_SUCCESS) {
			werr = WERR_DS_DRA_INTERNAL_ERROR;
			goto failed;
		}

		if ((rep_options & DS_NTDSDSA_OPT_DISABLE_INBOUND_REPL)) {
			werr = WERR_DS_DRA_SINK_DISABLED;
			goto failed;
		}
	}

	subreq = dreplsrv_op_pull_source_send(op, s->task->event_ctx, op);
	if (!subreq) {
		werr = WERR_NOMEM;
		goto failed;
	}

	tevent_req_set_callback(subreq, dreplsrv_pending_op_callback, op);
	return;

failed:
	if (op->extended_op == DRSUAPI_EXOP_NONE) {
		drepl_reps_update(s, "repsFrom", op->source_dsa->partition->dn,
				  &op->source_dsa->repsFrom1->source_dsa_obj_guid, werr);
	}
	/* unblock queue processing */
	s->ops.current = NULL;
	/*
	 * let the callback do its job just like in any other failure situation
	 */
	if (op->callback) {
		op->callback(s, werr, op->extended_ret, op->cb_data);
	}
}
