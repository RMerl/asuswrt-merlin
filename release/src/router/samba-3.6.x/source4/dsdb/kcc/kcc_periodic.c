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
#include "dsdb/kcc/kcc_connection.h"
#include "dsdb/kcc/kcc_service.h"
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "librpc/gen_ndr/ndr_irpc_c.h"
#include "param/param.h"

/*
 * see if two repsFromToBlob blobs are for the same source DSA
 */
static bool kccsrv_same_source_dsa(struct repsFromToBlob *r1, struct repsFromToBlob *r2)
{
	return GUID_compare(&r1->ctr.ctr1.source_dsa_obj_guid,
			    &r2->ctr.ctr1.source_dsa_obj_guid) == 0;
}

/*
 * see if a repsFromToBlob is in a list
 */
static bool reps_in_list(struct repsFromToBlob *r, struct repsFromToBlob *reps, uint32_t count)
{
	uint32_t i;
	for (i=0; i<count; i++) {
		if (kccsrv_same_source_dsa(r, &reps[i])) {
			return true;
		}
	}
	return false;
}

/*
  make sure we only add repsFrom entries for DCs who are masters for
  the partition
 */
static bool check_MasterNC(struct kccsrv_partition *p, struct repsFromToBlob *r,
			   struct ldb_result *res)
{
	struct repsFromTo1 *r1 = &r->ctr.ctr1;
	struct GUID invocation_id = r1->source_dsa_invocation_id;
	unsigned int i, j;

	/* we are expecting only version 1 */
	SMB_ASSERT(r->version == 1);

	for (i=0; i<res->count; i++) {
		struct ldb_message *msg = res->msgs[i];
		struct ldb_message_element *el;
		struct ldb_dn *dn;

		struct GUID id2 = samdb_result_guid(msg, "invocationID");
		if (GUID_all_zero(&id2) ||
		    !GUID_equal(&invocation_id, &id2)) {
			continue;
		}

		el = ldb_msg_find_element(msg, "msDS-hasMasterNCs");
		if (!el || el->num_values == 0) {
			el = ldb_msg_find_element(msg, "hasMasterNCs");
			if (!el || el->num_values == 0) {
				continue;
			}
		}
		for (j=0; j<el->num_values; j++) {
			dn = ldb_dn_from_ldb_val(p, p->service->samdb, &el->values[j]);
			if (!ldb_dn_validate(dn)) {
				talloc_free(dn);
				continue;
			}
			if (ldb_dn_compare(dn, p->dn) == 0) {
				talloc_free(dn);
				DEBUG(5,("%s %s match on %s in %s\n",
					 r1->other_info->dns_name,
					 el->name,
					 ldb_dn_get_linearized(dn),
					 ldb_dn_get_linearized(msg->dn)));
				return true;
			}
			talloc_free(dn);
		}
	}
	return false;
}

struct kccsrv_notify_drepl_server_state {
	struct dreplsrv_refresh r;
};

static void kccsrv_notify_drepl_server_done(struct tevent_req *subreq);

/**
 * Force dreplsrv to update its state as topology is changed
 */
static void kccsrv_notify_drepl_server(struct kccsrv_service *s,
				       TALLOC_CTX *mem_ctx)
{
	struct kccsrv_notify_drepl_server_state *state;
	struct dcerpc_binding_handle *irpc_handle;
	struct tevent_req *subreq;

	state = talloc_zero(s, struct kccsrv_notify_drepl_server_state);
	if (state == NULL) {
		return;
	}

	irpc_handle = irpc_binding_handle_by_name(state, s->task->msg_ctx,
						  "dreplsrv", &ndr_table_irpc);
	if (irpc_handle == NULL) {
		/* dreplsrv is not running yet */
		TALLOC_FREE(state);
		return;
	}

	subreq = dcerpc_dreplsrv_refresh_r_send(state, s->task->event_ctx,
						irpc_handle, &state->r);
	if (subreq == NULL) {
		TALLOC_FREE(state);
		return;
	}
	tevent_req_set_callback(subreq, kccsrv_notify_drepl_server_done, state);
}

static void kccsrv_notify_drepl_server_done(struct tevent_req *subreq)
{
	struct kccsrv_notify_drepl_server_state *state =
		tevent_req_callback_data(subreq,
		struct kccsrv_notify_drepl_server_state);
	NTSTATUS status;

	status = dcerpc_dreplsrv_refresh_r_recv(subreq, state);
	TALLOC_FREE(subreq);

	/* we don't care about errors */
	TALLOC_FREE(state);
}

static uint32_t kccsrv_replica_flags(struct kccsrv_service *s)
{
	if (s->am_rodc) {
		return DRSUAPI_DRS_INIT_SYNC |
			DRSUAPI_DRS_PER_SYNC |
			DRSUAPI_DRS_ADD_REF |
			DRSUAPI_DRS_SPECIAL_SECRET_PROCESSING |
			DRSUAPI_DRS_GET_ALL_GROUP_MEMBERSHIP |
			DRSUAPI_DRS_NONGC_RO_REP;
	}
	return DRSUAPI_DRS_INIT_SYNC |
		DRSUAPI_DRS_PER_SYNC |
		DRSUAPI_DRS_ADD_REF |
		DRSUAPI_DRS_WRIT_REP;
}

/*
 * add any missing repsFrom structures to our partitions
 */
static NTSTATUS kccsrv_add_repsFrom(struct kccsrv_service *s, TALLOC_CTX *mem_ctx,
				    struct repsFromToBlob *reps, uint32_t count,
				    struct ldb_result *res)
{
	struct kccsrv_partition *p;
	bool notify_dreplsrv = false;
	uint32_t replica_flags = kccsrv_replica_flags(s);

	/* update the repsFrom on all partitions */
	for (p=s->partitions; p; p=p->next) {
		struct repsFromToBlob *our_reps;
		uint32_t our_count;
		WERROR werr;
		uint32_t i, j;
		bool modified = false;

		werr = dsdb_loadreps(s->samdb, mem_ctx, p->dn, "repsFrom", &our_reps, &our_count);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,(__location__ ": Failed to load repsFrom from %s - %s\n", 
				 ldb_dn_get_linearized(p->dn), ldb_errstring(s->samdb)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		/* see if the entry already exists */
		for (i=0; i<count; i++) {
			for (j=0; j<our_count; j++) {
				if (kccsrv_same_source_dsa(&reps[i], &our_reps[j])) {
					/* we already have this one -
					   check the replica_flags are right */
					if (replica_flags != our_reps[j].ctr.ctr1.replica_flags) {
						/* we need to update the old one with
						 * the new flags
						 */
						our_reps[j].ctr.ctr1.replica_flags = replica_flags;
						modified = true;
					}
					break;
				}
			}
			if (j == our_count) {
				/* we don't have the new one - add it
				 * if it is a master
				 */
				if (!check_MasterNC(p, &reps[i], res)) {
					/* its not a master, we don't
					   want to pull from it */
					continue;
				}
				/* we need to add it to our repsFrom */
				our_reps = talloc_realloc(mem_ctx, our_reps, struct repsFromToBlob, our_count+1);
				NT_STATUS_HAVE_NO_MEMORY(our_reps);
				our_reps[our_count] = reps[i];
				our_reps[our_count].ctr.ctr1.replica_flags = replica_flags;
				our_count++;
				modified = true;
				DEBUG(4,(__location__ ": Added repsFrom for %s\n",
					 reps[i].ctr.ctr1.other_info->dns_name));
			}
		}

		/* remove any stale ones */
		for (i=0; i<our_count; i++) {
			if (!reps_in_list(&our_reps[i], reps, count) ||
			    !check_MasterNC(p, &our_reps[i], res)) {
				DEBUG(4,(__location__ ": Removed repsFrom for %s\n",
					 our_reps[i].ctr.ctr1.other_info->dns_name));
				memmove(&our_reps[i], &our_reps[i+1], (our_count-(i+1))*sizeof(our_reps[0]));
				our_count--;
				i--;
				modified = true;
			}
		}

		if (modified) {
			werr = dsdb_savereps(s->samdb, mem_ctx, p->dn, "repsFrom", our_reps, our_count);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,(__location__ ": Failed to save repsFrom to %s - %s\n", 
					 ldb_dn_get_linearized(p->dn), ldb_errstring(s->samdb)));
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}
			/* dreplsrv should refresh its state */
			notify_dreplsrv = true;
		}

		/* remove stale repsTo entries */
		modified = false;
		werr = dsdb_loadreps(s->samdb, mem_ctx, p->dn, "repsTo", &our_reps, &our_count);
		if (!W_ERROR_IS_OK(werr)) {
			DEBUG(0,(__location__ ": Failed to load repsTo from %s - %s\n", 
				 ldb_dn_get_linearized(p->dn), ldb_errstring(s->samdb)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		/* remove any stale ones */
		for (i=0; i<our_count; i++) {
			if (!reps_in_list(&our_reps[i], reps, count)) {
				DEBUG(4,(__location__ ": Removed repsTo for %s\n",
					 our_reps[i].ctr.ctr1.other_info->dns_name));
				memmove(&our_reps[i], &our_reps[i+1], (our_count-(i+1))*sizeof(our_reps[0]));
				our_count--;
				i--;
				modified = true;
			}
		}

		if (modified) {
			werr = dsdb_savereps(s->samdb, mem_ctx, p->dn, "repsTo", our_reps, our_count);
			if (!W_ERROR_IS_OK(werr)) {
				DEBUG(0,(__location__ ": Failed to save repsTo to %s - %s\n", 
					 ldb_dn_get_linearized(p->dn), ldb_errstring(s->samdb)));
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}
			/* dreplsrv should refresh its state */
			notify_dreplsrv = true;
		}
	}

	/* notify dreplsrv toplogy has changed */
	if (notify_dreplsrv) {
		kccsrv_notify_drepl_server(s, mem_ctx);
	}

	return NT_STATUS_OK;

}

/*
  this is the core of our initial simple KCC
  We just add a repsFrom entry for all DCs we find that have nTDSDSA
  objects, except for ourselves
 */
NTSTATUS kccsrv_simple_update(struct kccsrv_service *s, TALLOC_CTX *mem_ctx)
{
	struct ldb_result *res;
	unsigned int i;
	int ret;
	const char *attrs[] = { "objectGUID", "invocationID", "msDS-hasMasterNCs", "hasMasterNCs", NULL };
	struct repsFromToBlob *reps = NULL;
	uint32_t count = 0;
	struct kcc_connection_list *ntds_conn, *dsa_conn;

	ret = ldb_search(s->samdb, mem_ctx, &res, s->config_dn, LDB_SCOPE_SUBTREE, 
			 attrs, "objectClass=nTDSDSA");
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed nTDSDSA search - %s\n", ldb_errstring(s->samdb)));
		return NT_STATUS_INTERNAL_DB_CORRUPTION;
	}

	/* get the current list of connections */
	ntds_conn = kccsrv_find_connections(s, mem_ctx);

	dsa_conn = talloc_zero(mem_ctx, struct kcc_connection_list);

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
							       lpcfg_dnsdomain(s->task->lp_ctx));
		r1->source_dsa_obj_guid      = ntds_guid;
		r1->source_dsa_invocation_id = invocation_id;
		r1->replica_flags = kccsrv_replica_flags(s);
		memset(r1->schedule, 0x11, sizeof(r1->schedule));

		dsa_conn->servers = talloc_realloc(dsa_conn, dsa_conn->servers,
						  struct kcc_connection,
						  dsa_conn->count + 1);
		NT_STATUS_HAVE_NO_MEMORY(dsa_conn->servers);
		dsa_conn->servers[dsa_conn->count].dsa_guid = r1->source_dsa_obj_guid;
		dsa_conn->count++;

		count++;
	}

	kccsrv_apply_connections(s, ntds_conn, dsa_conn);

	return kccsrv_add_repsFrom(s, mem_ctx, reps, count, res);
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
	DEBUG(4,("kccsrv_periodic_schedule(%u) %sscheduled for: %s\n",
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

	DEBUG(4,("kccsrv_periodic_run(): simple update\n"));

	mem_ctx = talloc_new(service);
	status = kccsrv_simple_update(service, mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("kccsrv_simple_update failed - %s\n", nt_errstr(status)));
	}

	status = kccsrv_check_deleted(service, mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		DEBUG(0,("kccsrv_check_deleted failed - %s\n", nt_errstr(status)));
	}
	talloc_free(mem_ctx);
}
