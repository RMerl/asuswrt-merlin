/* 
   Unix SMB/CIFS implementation.
   
   WINS Replication server
   
   Copyright (C) Stefan Metzmacher	2005
   
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
#include "librpc/gen_ndr/ndr_winsrepl.h"
#include "wrepl_server/wrepl_server.h"
#include "nbt_server/wins/winsdb.h"
#include "ldb/include/ldb.h"
#include "ldb/include/ldb_errors.h"
#include "system/time.h"
#include "smbd/service_task.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/ndr_irpc.h"
#include "librpc/gen_ndr/ndr_nbt.h"
#include "param/param.h"

const char *wreplsrv_owner_filter(struct wreplsrv_service *service,
				  TALLOC_CTX *mem_ctx,
				  const char *wins_owner)
{
	if (strcmp(wins_owner, service->wins_db->local_owner) == 0) {
		return talloc_asprintf(mem_ctx, "(|(winsOwner=%s)(winsOwner=0.0.0.0))",
				       wins_owner);
	}

	return talloc_asprintf(mem_ctx, "(&(winsOwner=%s)(!(winsOwner=0.0.0.0)))",
			       wins_owner);
}

static NTSTATUS wreplsrv_scavenging_owned_records(struct wreplsrv_service *service, TALLOC_CTX *tmp_mem)
{
	NTSTATUS status;
	struct winsdb_record *rec = NULL;
	struct ldb_result *res = NULL;
	const char *owner_filter;
	const char *filter;
	uint32_t i;
	int ret;
	time_t now = time(NULL);
	const char *now_timestr;
	const char *action;
	const char *old_state=NULL;
	const char *new_state=NULL;
	uint32_t modify_flags;
	bool modify_record;
	bool delete_record;
	bool delete_tombstones;
	struct timeval tombstone_extra_time;
	const char *local_owner = service->wins_db->local_owner;
	bool propagate = lp_parm_bool(service->task->lp_ctx, NULL, "wreplsrv", "propagate name releases", false);

	now_timestr = ldb_timestring(tmp_mem, now);
	NT_STATUS_HAVE_NO_MEMORY(now_timestr);
	owner_filter = wreplsrv_owner_filter(service, tmp_mem, local_owner);
	NT_STATUS_HAVE_NO_MEMORY(owner_filter);
	filter = talloc_asprintf(tmp_mem,
				 "(&%s(objectClass=winsRecord)"
				 "(expireTime<=%s))",
				 owner_filter, now_timestr);
	NT_STATUS_HAVE_NO_MEMORY(filter);
	ret = ldb_search(service->wins_db->ldb, tmp_mem, &res, NULL, LDB_SCOPE_SUBTREE, NULL, "%s", filter);
	if (ret != LDB_SUCCESS) return NT_STATUS_INTERNAL_DB_CORRUPTION;
	DEBUG(10,("WINS scavenging: filter '%s' count %d\n", filter, res->count));

	tombstone_extra_time = timeval_add(&service->startup_time,
					   service->config.tombstone_extra_timeout,
					   0);
	delete_tombstones = timeval_expired(&tombstone_extra_time);

	for (i=0; i < res->count; i++) {
		bool has_replicas = false;

		/*
		 * we pass '0' as 'now' here,
		 * because we want to get the raw timestamps which are in the DB
		 */
		status = winsdb_record(service->wins_db, res->msgs[i], tmp_mem, 0, &rec);
		NT_STATUS_NOT_OK_RETURN(status);
		talloc_free(res->msgs[i]);

		modify_flags	= 0;
		modify_record	= false;
		delete_record	= false;

		switch (rec->state) {
		case WREPL_STATE_ACTIVE:
			old_state	= "active";
			if (rec->is_static) {
				/*
				 *we store it again, so that it won't appear
				 * in the scavenging the next time
				 */
				old_state	= "active(static)";
				new_state	= "active(static)";
				modify_flags	= 0;
				modify_record	= true;
				break;
			}
			if (rec->type != WREPL_TYPE_SGROUP || !propagate) {
				new_state	= "released";
				rec->state	= WREPL_STATE_RELEASED;
				rec->expire_time= service->config.tombstone_interval + now;
				modify_flags	= 0;
				modify_record	= true;
				break;
			}
			/* check if there's any replica address */
			for (i=0;rec->addresses[i];i++) {
				if (strcmp(rec->addresses[i]->wins_owner, local_owner) != 0) {
					has_replicas = true;
					rec->addresses[i]->expire_time= service->config.renew_interval + now;
				}
			}
			if (has_replicas) {
				/* if it has replica addresses propagate them */
				new_state	= "active(propagated)";
				rec->state	= WREPL_STATE_ACTIVE;
				rec->expire_time= service->config.renew_interval + now;
				modify_flags	= WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;
				modify_record	= true;
				break;
			}
			/*
			 * if it doesn't have replica addresses, make it a tombstone,
			 * so that the released owned addresses are propagated
			 */
			new_state	= "tombstone";
			rec->state	= WREPL_STATE_TOMBSTONE;
			rec->expire_time= time(NULL) +
					  service->config.tombstone_interval +
					  service->config.tombstone_timeout;
			modify_flags	= WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;
			modify_record	= true;
			break;

		case WREPL_STATE_RELEASED:
			old_state	= "released";
			new_state	= "tombstone";
			rec->state	= WREPL_STATE_TOMBSTONE;
			rec->expire_time= service->config.tombstone_timeout + now;
			modify_flags	= WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;
			modify_record	= true;
			break;

		case WREPL_STATE_TOMBSTONE:
			old_state	= "tombstone";
			new_state	= "tombstone";
			if (!delete_tombstones) break;
			new_state	= "deleted";
			delete_record = true;
			break;

		case WREPL_STATE_RESERVED:
			DEBUG(0,("%s: corrupted record: %s\n",
				__location__, nbt_name_string(rec, rec->name)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (modify_record) {
			action = "modify";
			ret = winsdb_modify(service->wins_db, rec, modify_flags);
		} else if (delete_record) {
			action = "delete";
			ret = winsdb_delete(service->wins_db, rec);
		} else {
			action = "skip";
			ret = NBT_RCODE_OK;
		}

		if (ret != NBT_RCODE_OK) {
			DEBUG(2,("WINS scavenging: failed to %s name %s (owned:%s -> owned:%s): error:%u\n",
				action, nbt_name_string(rec, rec->name), old_state, new_state, ret));
		} else {
			DEBUG(4,("WINS scavenging: %s name: %s (owned:%s -> owned:%s)\n",
				action, nbt_name_string(rec, rec->name), old_state, new_state));
		}

		talloc_free(rec);
	}

	return NT_STATUS_OK;
}

static NTSTATUS wreplsrv_scavenging_replica_non_active_records(struct wreplsrv_service *service, TALLOC_CTX *tmp_mem)
{
	NTSTATUS status;
	struct winsdb_record *rec = NULL;
	struct ldb_result *res = NULL;
	const char *owner_filter;
	const char *filter;
	uint32_t i;
	int ret;
	time_t now = time(NULL);
	const char *now_timestr;
	const char *action;
	const char *old_state=NULL;
	const char *new_state=NULL;
	uint32_t modify_flags;
	bool modify_record;
	bool delete_record;
	bool delete_tombstones;
	struct timeval tombstone_extra_time;

	now_timestr = ldb_timestring(tmp_mem, now);
	NT_STATUS_HAVE_NO_MEMORY(now_timestr);
	owner_filter = wreplsrv_owner_filter(service, tmp_mem,
					     service->wins_db->local_owner);
	NT_STATUS_HAVE_NO_MEMORY(owner_filter);
	filter = talloc_asprintf(tmp_mem,
				 "(&(!%s)(objectClass=winsRecord)"
				 "(!(recordState=%u))(expireTime<=%s))",
				 owner_filter, WREPL_STATE_ACTIVE, now_timestr);
	NT_STATUS_HAVE_NO_MEMORY(filter);
	ret = ldb_search(service->wins_db->ldb, tmp_mem, &res, NULL, LDB_SCOPE_SUBTREE, NULL, "%s", filter);
	if (ret != LDB_SUCCESS) return NT_STATUS_INTERNAL_DB_CORRUPTION;
	DEBUG(10,("WINS scavenging: filter '%s' count %d\n", filter, res->count));

	tombstone_extra_time = timeval_add(&service->startup_time,
					   service->config.tombstone_extra_timeout,
					   0);
	delete_tombstones = timeval_expired(&tombstone_extra_time);

	for (i=0; i < res->count; i++) {
		/*
		 * we pass '0' as 'now' here,
		 * because we want to get the raw timestamps which are in the DB
		 */
		status = winsdb_record(service->wins_db, res->msgs[i], tmp_mem, 0, &rec);
		NT_STATUS_NOT_OK_RETURN(status);
		talloc_free(res->msgs[i]);

		modify_flags	= 0;
		modify_record	= false;
		delete_record	= false;

		switch (rec->state) {
		case WREPL_STATE_ACTIVE:
			DEBUG(0,("%s: corrupted record: %s\n",
				__location__, nbt_name_string(rec, rec->name)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;

		case WREPL_STATE_RELEASED:
			old_state	= "released";
			new_state	= "tombstone";
			rec->state	= WREPL_STATE_TOMBSTONE;
			rec->expire_time= service->config.tombstone_timeout + now;
			modify_flags	= 0;
			modify_record	= true;
			break;

		case WREPL_STATE_TOMBSTONE:
			old_state	= "tombstone";
			new_state	= "tombstone";
			if (!delete_tombstones) break;
			new_state	= "deleted";
			delete_record = true;
			break;

		case WREPL_STATE_RESERVED:
			DEBUG(0,("%s: corrupted record: %s\n",
				__location__, nbt_name_string(rec, rec->name)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		if (modify_record) {
			action = "modify";
			ret = winsdb_modify(service->wins_db, rec, modify_flags);
		} else if (delete_record) {
			action = "delete";
			ret = winsdb_delete(service->wins_db, rec);
		} else {
			action = "skip";
			ret = NBT_RCODE_OK;
		}

		if (ret != NBT_RCODE_OK) {
			DEBUG(2,("WINS scavenging: failed to %s name %s (replica:%s -> replica:%s): error:%u\n",
				action, nbt_name_string(rec, rec->name), old_state, new_state, ret));
		} else {
			DEBUG(4,("WINS scavenging: %s name: %s (replica:%s -> replica:%s)\n",
				action, nbt_name_string(rec, rec->name), old_state, new_state));
		}

		talloc_free(rec);
	}

	return NT_STATUS_OK;
}

struct verify_state {
	struct messaging_context *msg_ctx;
	struct wreplsrv_service *service;
	struct winsdb_record *rec;
	struct nbtd_proxy_wins_challenge r;
};

static void verify_handler(struct irpc_request *ireq)
{
	struct verify_state *s = talloc_get_type(ireq->async.private_data,
				 struct verify_state);
	struct winsdb_record *rec = s->rec;
	const char *action;
	const char *old_state = "active";
	const char *new_state = "active";
	const char *new_owner = "replica";
	uint32_t modify_flags = 0;
	bool modify_record = false;
	bool delete_record = false;
	bool different = false;
	int ret;
	NTSTATUS status;
	uint32_t i, j;

	/*
	 * - if the name isn't present anymore remove our record
	 * - if the name is found and not a normal group check if the addresses match,
	 *   - if they don't match remove the record
	 *   - if they match do nothing
	 * - if an error happens do nothing
	 */
	status = irpc_call_recv(ireq);
	if (NT_STATUS_EQUAL(NT_STATUS_OBJECT_NAME_NOT_FOUND, status)) {
		delete_record = true;
		new_state = "deleted";
	} else if (NT_STATUS_IS_OK(status) && rec->type != WREPL_TYPE_GROUP) {
		for (i=0; i < s->r.out.num_addrs; i++) {
			bool found = false;
			for (j=0; rec->addresses[j]; j++) {
				if (strcmp(s->r.out.addrs[i].addr, rec->addresses[j]->address) == 0) {
					found = true;
					break;
				}
			}
			if (!found) {
				different = true;
				break;
			}
		}
	} else if (NT_STATUS_IS_OK(status) && rec->type == WREPL_TYPE_GROUP) {
		if (s->r.out.num_addrs != 1 || strcmp(s->r.out.addrs[0].addr, "255.255.255.255") != 0) {
			different = true;
		}
	}

	if (different) {
		/*
		 * if the reply from the owning wins server has different addresses
		 * then take the ownership of the record and make it a tombstone
		 * this will then hopefully replicated to the original owner of the record
		 * which will then propagate it's own record, so that the current record will
		 * be replicated to to us
		 */
		DEBUG(2,("WINS scavenging: replica %s verify got different addresses from winsserver: %s: tombstoning record\n",
			nbt_name_string(rec, rec->name), rec->wins_owner));

		rec->state	= WREPL_STATE_TOMBSTONE;
		rec->expire_time= time(NULL) + s->service->config.tombstone_timeout;
		for (i=0; rec->addresses[i]; i++) {
			rec->addresses[i]->expire_time = rec->expire_time;
		}
		modify_record	= true;
		modify_flags	= WINSDB_FLAG_ALLOC_VERSION | WINSDB_FLAG_TAKE_OWNERSHIP;
		new_state	= "tombstone";
		new_owner	= "owned";
	} else if (NT_STATUS_IS_OK(status)) {
		/* if the addresses are the same, just update the timestamps */
		rec->expire_time = time(NULL) + s->service->config.verify_interval;
		for (i=0; rec->addresses[i]; i++) {
			rec->addresses[i]->expire_time = rec->expire_time;
		}
		modify_record	= true;
		modify_flags	= 0;
		new_state	= "active";
	}

	if (modify_record) {
		action = "modify";
		ret = winsdb_modify(s->service->wins_db, rec, modify_flags);
	} else if (delete_record) {
		action = "delete";
		ret = winsdb_delete(s->service->wins_db, rec);
	} else {
		action = "skip";
		ret = NBT_RCODE_OK;
	}

	if (ret != NBT_RCODE_OK) {
		DEBUG(2,("WINS scavenging: failed to %s name %s (replica:%s -> %s:%s): error:%u\n",
			action, nbt_name_string(rec, rec->name), old_state, new_owner, new_state, ret));
	} else {
		DEBUG(4,("WINS scavenging: %s name: %s (replica:%s -> %s:%s): %s: %s\n",
			action, nbt_name_string(rec, rec->name), old_state, new_owner, new_state,
			rec->wins_owner, nt_errstr(status)));
	}

	talloc_free(s);
}

static NTSTATUS wreplsrv_scavenging_replica_active_records(struct wreplsrv_service *service, TALLOC_CTX *tmp_mem)
{
	NTSTATUS status;
	struct winsdb_record *rec = NULL;
	struct ldb_result *res = NULL;
	const char *owner_filter;
	const char *filter;
	uint32_t i;
	int ret;
	time_t now = time(NULL);
	const char *now_timestr;
	struct irpc_request *ireq;
	struct verify_state *s;
	struct server_id *nbt_servers;

	nbt_servers = irpc_servers_byname(service->task->msg_ctx, tmp_mem, "nbt_server");
	if ((nbt_servers == NULL) || (nbt_servers[0].id == 0)) {
		return NT_STATUS_INTERNAL_ERROR;
	}

	now_timestr = ldb_timestring(tmp_mem, now);
	NT_STATUS_HAVE_NO_MEMORY(now_timestr);
	owner_filter = wreplsrv_owner_filter(service, tmp_mem,
					     service->wins_db->local_owner);
	NT_STATUS_HAVE_NO_MEMORY(owner_filter);
	filter = talloc_asprintf(tmp_mem,
				 "(&(!%s)(objectClass=winsRecord)"
				 "(recordState=%u)(expireTime<=%s))",
				 owner_filter, WREPL_STATE_ACTIVE, now_timestr);
	NT_STATUS_HAVE_NO_MEMORY(filter);
	ret = ldb_search(service->wins_db->ldb, tmp_mem, &res, NULL, LDB_SCOPE_SUBTREE, NULL, "%s", filter);
	if (ret != LDB_SUCCESS) return NT_STATUS_INTERNAL_DB_CORRUPTION;
	DEBUG(10,("WINS scavenging: filter '%s' count %d\n", filter, res->count));

	for (i=0; i < res->count; i++) {
		/*
		 * we pass '0' as 'now' here,
		 * because we want to get the raw timestamps which are in the DB
		 */
		status = winsdb_record(service->wins_db, res->msgs[i], tmp_mem, 0, &rec);
		NT_STATUS_NOT_OK_RETURN(status);
		talloc_free(res->msgs[i]);

		if (rec->state != WREPL_STATE_ACTIVE) {
			DEBUG(0,("%s: corrupted record: %s\n",
				__location__, nbt_name_string(rec, rec->name)));
			return NT_STATUS_INTERNAL_DB_CORRUPTION;
		}

		/* 
		 * ask the owning wins server if the record still exists,
		 * if not delete the record
		 *
		 * TODO: NOTE: this is a simpliefied version, to verify that
		 *             a record still exist, I assume that w2k3 uses
		 *             DCERPC calls or some WINSREPL packets for this,
		 *             but we use a wins name query
		 */
		DEBUG(2,("ask wins server '%s' if '%s' with version_id:%llu still exists\n",
			 rec->wins_owner, nbt_name_string(rec, rec->name), 
			 (unsigned long long)rec->version));

		s = talloc_zero(tmp_mem, struct verify_state);
		NT_STATUS_HAVE_NO_MEMORY(s);
		s->msg_ctx	= service->task->msg_ctx;
		s->service	= service;
		s->rec		= talloc_steal(s, rec);

		s->r.in.name		= *rec->name;
		s->r.in.num_addrs	= 1;
		s->r.in.addrs		= talloc_array(s, struct nbtd_proxy_wins_addr, s->r.in.num_addrs);
		NT_STATUS_HAVE_NO_MEMORY(s->r.in.addrs);
		/* TODO: fix pidl to handle inline ipv4address arrays */
		s->r.in.addrs[0].addr	= rec->wins_owner;

		ireq = IRPC_CALL_SEND(s->msg_ctx, nbt_servers[0],
				      irpc, NBTD_PROXY_WINS_CHALLENGE,
				      &s->r, s);
		NT_STATUS_HAVE_NO_MEMORY(ireq);

		ireq->async.fn		= verify_handler;
		ireq->async.private_data= s;

		talloc_steal(service, s);
	}

	return NT_STATUS_OK;
}

NTSTATUS wreplsrv_scavenging_run(struct wreplsrv_service *service)
{
	NTSTATUS status;
	TALLOC_CTX *tmp_mem;
	bool skip_first_run = false;

	if (!timeval_expired(&service->scavenging.next_run)) {
		return NT_STATUS_OK;
	}

	if (timeval_is_zero(&service->scavenging.next_run)) {
		skip_first_run = true;
	}

	service->scavenging.next_run = timeval_current_ofs(service->config.scavenging_interval, 0);
	status = wreplsrv_periodic_schedule(service, service->config.scavenging_interval);
	NT_STATUS_NOT_OK_RETURN(status);

	/*
	 * if it's the first time this functions is called (startup)
	 * the next_run is zero, in this case we should not do scavenging
	 */
	if (skip_first_run) {
		return NT_STATUS_OK;
	}

	if (service->scavenging.processing) {
		return NT_STATUS_OK;
	}

	DEBUG(2,("wreplsrv_scavenging_run(): start\n"));

	tmp_mem = talloc_new(service);
	NT_STATUS_HAVE_NO_MEMORY(tmp_mem);
	service->scavenging.processing = true;
	status = wreplsrv_scavenging_owned_records(service,tmp_mem);
	service->scavenging.processing = false;
	talloc_free(tmp_mem);
	NT_STATUS_NOT_OK_RETURN(status);

	tmp_mem = talloc_new(service);	
	NT_STATUS_HAVE_NO_MEMORY(tmp_mem);
	service->scavenging.processing = true;
	status = wreplsrv_scavenging_replica_non_active_records(service, tmp_mem);
	service->scavenging.processing = false;
	talloc_free(tmp_mem);
	NT_STATUS_NOT_OK_RETURN(status);

	tmp_mem = talloc_new(service);
	NT_STATUS_HAVE_NO_MEMORY(tmp_mem);
	service->scavenging.processing = true;
	status = wreplsrv_scavenging_replica_active_records(service, tmp_mem);
	service->scavenging.processing = false;
	talloc_free(tmp_mem);
	NT_STATUS_NOT_OK_RETURN(status);

	DEBUG(2,("wreplsrv_scavenging_run(): end\n"));

	return NT_STATUS_OK;
}
