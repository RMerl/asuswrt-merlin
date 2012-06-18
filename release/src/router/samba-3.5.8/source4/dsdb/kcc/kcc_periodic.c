/* 
   Unix SMB/CIFS mplementation.
   KCC service periodic handling
   
   Copyright (C) Andrew Tridgell 2009
   based on repl service code
    
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
#include "dsdb/kcc/kcc_service.h"
#include "lib/ldb/include/ldb_errors.h"
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "param/param.h"

/*
 * see if a repsFromToBlob is in a list
 */
static bool reps_in_list(struct repsFromToBlob *r, struct repsFromToBlob *reps, uint32_t count)
{
	int i;
	for (i=0; i<count; i++) {
		if (strcmp(r->ctr.ctr1.other_info->dns_name, 
			   reps[i].ctr.ctr1.other_info->dns_name) == 0 &&
		    GUID_compare(&r->ctr.ctr1.source_dsa_obj_guid, 
				 &reps[i].ctr.ctr1.source_dsa_obj_guid) == 0) {
			return true;
		}
	}
	return false;
}


/*
 * add any missing repsFrom structures to our partitions
 */
static NTSTATUS kccsrv_add_repsFrom(struct kccsrv_service *s, TALLOC_CTX *mem_ctx,
				    struct repsFromToBlob *reps, uint32_t count)
{
	struct kccsrv_partition *p;

	/* update the repsFrom on all partitions */
	for (p=s->partitions; p; p=p->next) {
		struct repsFromToBlob *old_reps;
		uint32_t old_count;
		WERROR werr;
		int i;
		bool modified = false;

		werr = dsdb_loadreps(s->samdb, mem_ctx, p->dn, "repsFrom", &old_reps, &old_count);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,(__location__ ": Failed to load repsFrom from %s - %s\n", 
				 ldb_dn_get_linearized(p->dn), ldb_errstring(s->samdb)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		/* add any new ones */
		for (i=0; i<count; i++) {
			if (!reps_in_list(&reps[i], old_reps, old_count)) {
				old_reps = talloc_realloc(mem_ctx, old_reps, struct repsFromToBlob, old_count+1);
				NT_STATUS_HAVE_NO_MEMORY(old_reps);
				old_reps[old_count] = reps[i];
				old_count++;
				modified = true;
			}
		}

		/* remove any stale ones */
		for (i=0; i<old_count; i++) {
			if (!reps_in_list(&old_reps[i], reps, count)) {
				memmove(&old_reps[i], &old_reps[i+1], (old_count-(i+1))*sizeof(old_reps[0]));
				old_count--;
				i--;
				modified = true;
			}
		}
		
		if (modified) {
			werr = dsdb_savereps(s->samdb, mem_ctx, p->dn, "repsFrom", old_reps, old_count);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,(__location__ ": Failed to save repsFrom to %s - %s\n", 
					 ldb_dn_get_linearized(p->dn), ldb_errstring(s->samdb)));
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}
		}
	}

	return NT_STATUS_OK;

}

/*
  this is the core of our initial simple KCC
  We just add a repsFrom entry for all DCs we find that have nTDSDSA
  objects, except for ourselves
 */
static NTSTATUS kccsrv_simple_update(struct kccsrv_service *s, TALLOC_CTX *mem_ctx)
{
	struct ldb_result *res;
	int ret, i;
	const char *attrs[] = { "objectGUID", "invocationID", NULL };
	struct repsFromToBlob *reps = NULL;
	uint32_t count = 0;

	ret = ldb_search(s->samdb, mem_ctx, &res, s->config_dn, LDB_SCOPE_SUBTREE, 
			 attrs, "objectClass=nTDSDSA");
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed nTDSDSA search - %s\n", ldb_errstring(s->samdb)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	for (i=0; i<res->count; i++) {
		struct repsFromTo1 *r1;
		struct GUID ntds_guid, invocation_id;

		ntds_guid = samdb_result_guid(res->msgs[i], "objectGUID");
		if (GUID_compare(&ntds_guid, &s->ntds_guid) == 0) {
			/* don't replicate with ourselves */
			continue;
		}

		invocation_id = samdb_result_guid(res->msgs[i], "invocationID");

		reps = talloc_realloc(mem_ctx, reps, struct repsFromToBlob, count+1);
		NT_STATUS_HAVE_NO_MEMORY(reps);

		ZERO_STRUCT(reps[count]);
		reps[count].version = 1;
		r1 = &reps[count].ctr.ctr1;

		r1->other_info               = talloc_zero(reps, struct repsFromTo1OtherInfo);
		r1->other_info->dns_name     = talloc_asprintf(r1->other_info, "%s._msdcs.%s",
							       GUID_string(mem_ctx, &ntds_guid),
							       lp_realm(s->task->lp_ctx));
		r1->source_dsa_obj_guid      = ntds_guid;
		r1->source_dsa_invocation_id = invocation_id;
		r1->replica_flags            = 
			DRSUAPI_DS_REPLICA_NEIGHBOUR_WRITEABLE | 
			DRSUAPI_DS_REPLICA_NEIGHBOUR_SYNC_ON_STARTUP | 
			DRSUAPI_DS_REPLICA_NEIGHBOUR_DO_SCHEDULED_SYNCS;
		memset(r1->schedule, 0x11, sizeof(r1->schedule));
		count++;
	}

	return kccsrv_add_repsFrom(s, mem_ctx, reps, count);
}


static void kccsrv_periodic_run(struct kccsrv_service *service);

static void kccsrv_periodic_handler_te(struct tevent_context *ev, struct tevent_timer *te,
					 struct timeval t, void *ptr)
{
	struct kccsrv_service *service = talloc_get_type(ptr, struct kccsrv_service);
	WERROR status;

	service->periodic.te = NULL;

	kccsrv_periodic_run(service);

	status = kccsrv_periodic_schedule(service, service->periodic.interval);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(service->task, win_errstr(status), true);
		return;
	}
}

WERROR kccsrv_periodic_schedule(struct kccsrv_service *service, uint32_t next_interval)
{
	TALLOC_CTX *tmp_mem;
	struct tevent_timer *new_te;
	struct timeval next_time;

	/* prevent looping */
	if (next_interval == 0) next_interval = 1;

	next_time = timeval_current_ofs(next_interval, 50);

	if (service->periodic.te) {
		/*
		 * if the timestamp of the new event is higher,
		 * as current next we don't need to reschedule
		 */
		if (timeval_compare(&next_time, &service->periodic.next_event) > 0) {
			return WERR_OK;
		}
	}

	/* reset the next scheduled timestamp */
	service->periodic.next_event = next_time;

	new_te = event_add_timed(service->task->event_ctx, service,
			         service->periodic.next_event,
			         kccsrv_periodic_handler_te, service);
	W_ERROR_HAVE_NO_MEMORY(new_te);

	tmp_mem = talloc_new(service);
	DEBUG(2,("kccsrv_periodic_schedule(%u) %sscheduled for: %s\n",
		next_interval,
		(service->periodic.te?"re":""),
		nt_time_string(tmp_mem, timeval_to_nttime(&next_time))));
	talloc_free(tmp_mem);

	talloc_free(service->periodic.te);
	service->periodic.te = new_te;

	return WERR_OK;
}

static void kccsrv_periodic_run(struct kccsrv_service *service)
{
	TALLOC_CTX *mem_ctx;
	NTSTATUS status;

	DEBUG(2,("kccsrv_periodic_run(): simple update\n"));

	mem_ctx = talloc_new(service);
	status = kccsrv_simple_update(service, mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("kccsrv_simple_update failed - %s\n", nt_errstr(status)));
	}
	talloc_free(mem_ctx);
}
