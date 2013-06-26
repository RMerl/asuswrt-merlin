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
#include "../lib/util/dlinklist.h"
#include "smbd/service_task.h"
#include "smbd/service.h"
#include "lib/messaging/irpc.h"
#include "librpc/gen_ndr/winsrepl.h"
#include "wrepl_server/wrepl_server.h"
#include "nbt_server/wins/winsdb.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "auth/auth.h"
#include "ldb_wrap.h"
#include "param/param.h"
#include "lib/socket/netif.h"

static struct ldb_context *wins_config_db_connect(TALLOC_CTX *mem_ctx, 
						  struct tevent_context *ev_ctx,
						  struct loadparm_context *lp_ctx)
{
	return ldb_wrap_connect(mem_ctx, ev_ctx, lp_ctx, private_path(mem_ctx,
			        lp_ctx, lpcfg_wins_config_url(lp_ctx)),
				system_session(lp_ctx), NULL, 0);
}

static uint64_t wins_config_db_get_seqnumber(struct ldb_context *ldb)
{
	int ret;
	struct ldb_dn *dn;
	struct ldb_result *res = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	uint64_t seqnumber = 0;

	dn = ldb_dn_new(tmp_ctx, ldb, "@BASEINFO");
	if (!dn) goto failed;

	/* find the record in the WINS database */
	ret = ldb_search(ldb, tmp_ctx, &res, dn, LDB_SCOPE_BASE, NULL, NULL);
	if (ret != LDB_SUCCESS) goto failed;
	if (res->count > 1) goto failed;

	if (res->count == 1) {
		seqnumber = ldb_msg_find_attr_as_uint64(res->msgs[0], "sequenceNumber", 0);
	}

failed:
	talloc_free(tmp_ctx);
	return seqnumber;
}

/*
  open winsdb
*/
static NTSTATUS wreplsrv_open_winsdb(struct wreplsrv_service *service, 
				     struct loadparm_context *lp_ctx)
{
	const char *owner = lpcfg_parm_string(lp_ctx, NULL, "winsdb", "local_owner");

	if (owner == NULL) {
		struct interface *ifaces;
		load_interfaces(service, lpcfg_interfaces(lp_ctx), &ifaces);
		owner = iface_n_ip(ifaces, 0);
	}

	service->wins_db     = winsdb_connect(service, service->task->event_ctx, lp_ctx, owner, WINSDB_HANDLE_CALLER_WREPL);
	if (!service->wins_db) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	service->config.ldb = wins_config_db_connect(service, service->task->event_ctx, lp_ctx);
	if (!service->config.ldb) {
		return NT_STATUS_INTERNAL_DB_ERROR;
	}

	/* the default renew interval is 6 days */
	service->config.renew_interval	  = lpcfg_parm_int(lp_ctx, NULL,"wreplsrv","renew_interval", 6*24*60*60);

	/* the default tombstone (extinction) interval is 6 days */
	service->config.tombstone_interval= lpcfg_parm_int(lp_ctx, NULL,"wreplsrv","tombstone_interval", 6*24*60*60);

	/* the default tombstone (extinction) timeout is 1 day */
	service->config.tombstone_timeout = lpcfg_parm_int(lp_ctx, NULL,"wreplsrv","tombstone_timeout", 1*24*60*60);

	/* the default tombstone extra timeout is 3 days */
	service->config.tombstone_extra_timeout = lpcfg_parm_int(lp_ctx, NULL,"wreplsrv","tombstone_extra_timeout", 3*24*60*60);

	/* the default verify interval is 24 days */
	service->config.verify_interval   = lpcfg_parm_int(lp_ctx, NULL,"wreplsrv","verify_interval", 24*24*60*60);

	/* the default scavenging interval is 'renew_interval/2' */
	service->config.scavenging_interval=lpcfg_parm_int(lp_ctx, NULL,"wreplsrv","scavenging_interval",
							service->config.renew_interval/2);

	/* the maximun interval to the next periodic processing event */
	service->config.periodic_interval = lpcfg_parm_int(lp_ctx, NULL,"wreplsrv","periodic_interval", 15);

	return NT_STATUS_OK;
}

struct wreplsrv_partner *wreplsrv_find_partner(struct wreplsrv_service *service, const char *peer_addr)
{
	struct wreplsrv_partner *cur;

	for (cur = service->partners; cur; cur = cur->next) {
		if (strcmp(cur->address, peer_addr) == 0) {
			return cur;
		}
	}

	return NULL;
}

/*
  load our replication partners
*/
NTSTATUS wreplsrv_load_partners(struct wreplsrv_service *service)
{
	struct wreplsrv_partner *partner;
	struct ldb_result *res = NULL;
	int ret;
	TALLOC_CTX *tmp_ctx;
	unsigned int i;
	uint64_t new_seqnumber;

	new_seqnumber = wins_config_db_get_seqnumber(service->config.ldb);

	/* if it's not the first run and nothing changed we're done */
	if (service->config.seqnumber != 0 && service->config.seqnumber == new_seqnumber) {
		return NT_STATUS_OK;
	}

	tmp_ctx = talloc_new(service);
	NT_STATUS_HAVE_NO_MEMORY(tmp_ctx);

	service->config.seqnumber = new_seqnumber;

	/* find the record in the WINS database */
	ret = ldb_search(service->config.ldb, tmp_ctx, &res,
			 ldb_dn_new(tmp_ctx, service->config.ldb, "CN=PARTNERS"),
			 LDB_SCOPE_SUBTREE, NULL, "(objectClass=wreplPartner)");
	if (ret != LDB_SUCCESS) goto failed;

	/* first disable all existing partners */
	for (partner=service->partners; partner; partner = partner->next) {
		partner->type = WINSREPL_PARTNER_NONE;
	}

	for (i=0; i < res->count; i++) {
		const char *address;

		address	= ldb_msg_find_attr_as_string(res->msgs[i], "address", NULL);
		if (!address) {
			goto failed;
		}

		partner = wreplsrv_find_partner(service, address);
		if (partner) {
			if (partner->name != partner->address) {
				talloc_free(discard_const(partner->name));
			}
			partner->name = NULL;
			talloc_free(discard_const(partner->our_address));
			partner->our_address = NULL;

			/* force rescheduling of pulling */
			partner->pull.next_run = timeval_zero();
		} else {
			partner = talloc_zero(service, struct wreplsrv_partner);
			if (partner == NULL) goto failed;

			partner->service = service;
			partner->address = address;
			talloc_steal(partner, partner->address);

			DLIST_ADD_END(service->partners, partner, struct wreplsrv_partner *);
		}

		partner->name			= ldb_msg_find_attr_as_string(res->msgs[i], "name", partner->address);
		talloc_steal(partner, partner->name);
		partner->our_address		= ldb_msg_find_attr_as_string(res->msgs[i], "ourAddress", NULL);
		talloc_steal(partner, partner->our_address);

		partner->type			= ldb_msg_find_attr_as_uint(res->msgs[i], "type", WINSREPL_PARTNER_BOTH);
		partner->pull.interval		= ldb_msg_find_attr_as_uint(res->msgs[i], "pullInterval",
								    WINSREPL_DEFAULT_PULL_INTERVAL);
		partner->pull.retry_interval	= ldb_msg_find_attr_as_uint(res->msgs[i], "pullRetryInterval",
								    WINSREPL_DEFAULT_PULL_RETRY_INTERVAL);
		partner->push.change_count	= ldb_msg_find_attr_as_uint(res->msgs[i], "pushChangeCount",
								    WINSREPL_DEFAULT_PUSH_CHANGE_COUNT);
		partner->push.use_inform	= ldb_msg_find_attr_as_uint(res->msgs[i], "pushUseInform", true);

		DEBUG(3,("wreplsrv_load_partners: found partner: %s type: 0x%X\n",
			partner->address, partner->type));
	}

	DEBUG(2,("wreplsrv_load_partners: %u partners found: wins_config_db seqnumber %llu\n",
		res->count, (unsigned long long)service->config.seqnumber));

	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
failed:
	talloc_free(tmp_ctx);
	return NT_STATUS_FOOBAR;
}

NTSTATUS wreplsrv_fill_wrepl_table(struct wreplsrv_service *service,
				   TALLOC_CTX *mem_ctx,
				   struct wrepl_table *table_out,
				   const char *initiator,
				   bool full_table)
{
	struct wreplsrv_owner *cur;
	uint32_t i = 0;

	table_out->partner_count	= 0;
	table_out->partners		= NULL;
	table_out->initiator		= initiator;

	for (cur = service->table; cur; cur = cur->next) {
		if (full_table) {
			table_out->partner_count++;
			continue;
		}

		if (strcmp(initiator, cur->owner.address) != 0) continue;

		table_out->partner_count++;
		break;
	}

	table_out->partners = talloc_array(mem_ctx, struct wrepl_wins_owner, table_out->partner_count);
	NT_STATUS_HAVE_NO_MEMORY(table_out->partners);

	for (cur = service->table; cur && i < table_out->partner_count; cur = cur->next) {
		/*
		 * if it's our local entry
		 * update the max version
		 */
		if (cur == service->owner) {
			cur->owner.max_version = winsdb_get_maxVersion(service->wins_db);
		}

		if (full_table) {
			table_out->partners[i] = cur->owner;
			i++;
			continue;
		}

		if (strcmp(initiator, cur->owner.address) != 0) continue;

		table_out->partners[i] = cur->owner;
		i++;
		break;
	}

	return NT_STATUS_OK;
}

struct wreplsrv_owner *wreplsrv_find_owner(struct wreplsrv_service *service,
					   struct wreplsrv_owner *table,
					   const char *wins_owner)
{
	struct wreplsrv_owner *cur;

	for (cur = table; cur; cur = cur->next) {
		if (strcmp(cur->owner.address, wins_owner) == 0) {
			/*
			 * if it's our local entry
			 * update the max version
			 */
			if (cur == service->owner) {
				cur->owner.max_version = winsdb_get_maxVersion(service->wins_db);
			}
			return cur;
		}
	}

	return NULL;
}

/*
 update the wins_owner_table max_version, if the given version is the highest version
 if no entry for the wins_owner exists yet, create one
*/
NTSTATUS wreplsrv_add_table(struct wreplsrv_service *service,
			    TALLOC_CTX *mem_ctx, struct wreplsrv_owner **_table,
			    const char *wins_owner, uint64_t version)
{
	struct wreplsrv_owner *table = *_table;
	struct wreplsrv_owner *cur;

	if (!wins_owner || strcmp(wins_owner, "0.0.0.0") == 0) {
		wins_owner = service->wins_db->local_owner;
	}

	cur = wreplsrv_find_owner(service, table, wins_owner);

	/* if it doesn't exists yet, create one */
	if (!cur) {
		cur = talloc_zero(mem_ctx, struct wreplsrv_owner);
		NT_STATUS_HAVE_NO_MEMORY(cur);

		cur->owner.address	= talloc_strdup(cur, wins_owner);
		NT_STATUS_HAVE_NO_MEMORY(cur->owner.address);
		cur->owner.min_version	= 0;
		cur->owner.max_version	= 0;
		cur->owner.type		= 1; /* don't know why this is always 1 */

		cur->partner		= wreplsrv_find_partner(service, wins_owner);

		DLIST_ADD_END(table, cur, struct wreplsrv_owner *);
		*_table = table;
	}

	/* the min_version is always 0 here, and won't be updated */

	/* if the given version is higher than the current max_version, update */
	if (cur->owner.max_version < version) {
		cur->owner.max_version = version;
		/* if it's for our local db, we need to update the wins.ldb too */
		if (cur == service->owner) {
			uint64_t ret;
			ret = winsdb_set_maxVersion(service->wins_db, cur->owner.max_version);
			if (ret != cur->owner.max_version) {
				DEBUG(0,("winsdb_set_maxVersion(%llu) failed: %llu\n",
					 (unsigned long long)cur->owner.max_version, 
					 (unsigned long long)ret));
				return NT_STATUS_INTERNAL_DB_CORRUPTION;
			}
		}
	}

	return NT_STATUS_OK;
}

/*
  load the partner table
*/
static NTSTATUS wreplsrv_load_table(struct wreplsrv_service *service)
{
	struct ldb_result *res = NULL;
	int ret;
	NTSTATUS status;
	TALLOC_CTX *tmp_ctx = talloc_new(service);
	struct ldb_context *ldb = service->wins_db->ldb;
	unsigned int i;
	struct wreplsrv_owner *local_owner;
	const char *wins_owner;
	uint64_t version;
	const char * const attrs[] = {
		"winsOwner",
		"versionID",
		NULL
	};

	/*
	 * make sure we have our local entry in the list,
	 * but we set service->owner when we're done
	 * to avoid to many calls to wreplsrv_local_max_version()
	 */
	status = wreplsrv_add_table(service,
				    service, &service->table,
				    service->wins_db->local_owner, 0);
	if (!NT_STATUS_IS_OK(status)) goto failed;
	local_owner = wreplsrv_find_owner(service, service->table, service->wins_db->local_owner);
	if (!local_owner) {
		status = NT_STATUS_INTERNAL_ERROR;
		goto failed;
	}

	/* find the record in the WINS database */
	ret = ldb_search(ldb, tmp_ctx, &res, NULL, LDB_SCOPE_SUBTREE,
			 attrs, "(objectClass=winsRecord)");
	status = NT_STATUS_INTERNAL_DB_CORRUPTION;
	if (ret != LDB_SUCCESS) goto failed;

	for (i=0; i < res->count; i++) {
		wins_owner     = ldb_msg_find_attr_as_string(res->msgs[i], "winsOwner", NULL);
		version        = ldb_msg_find_attr_as_uint64(res->msgs[i], "versionID", 0);

		status = wreplsrv_add_table(service,
					    service, &service->table,
					    wins_owner, version);
		if (!NT_STATUS_IS_OK(status)) goto failed;
		talloc_free(res->msgs[i]);
	}

	/*
	 * this makes sure we call wreplsrv_local_max_version() before returning in
	 * wreplsrv_find_owner()
	 */
	service->owner = local_owner;

	/*
	 * this makes sure the maxVersion in the database is updated,
	 * with the highest version we found, if this is higher than the current stored one
	 */
	status = wreplsrv_add_table(service,
				    service, &service->table,
				    service->wins_db->local_owner, local_owner->owner.max_version);
	if (!NT_STATUS_IS_OK(status)) goto failed;

	talloc_free(tmp_ctx);
	return NT_STATUS_OK;
failed:
	talloc_free(tmp_ctx);
	return status;
}

/*
  setup our replication partners
*/
static NTSTATUS wreplsrv_setup_partners(struct wreplsrv_service *service)
{
	NTSTATUS status;

	status = wreplsrv_load_partners(service);
	NT_STATUS_NOT_OK_RETURN(status);

	status = wreplsrv_load_table(service);
	NT_STATUS_NOT_OK_RETURN(status);

	return NT_STATUS_OK;
}

/*
  startup the wrepl task
*/
static void wreplsrv_task_init(struct task_server *task)
{
	NTSTATUS status;
	struct wreplsrv_service *service;

	if (!lpcfg_wins_support(task->lp_ctx)) {
		return;
	}

	task_server_set_title(task, "task[wreplsrv]");

	service = talloc_zero(task, struct wreplsrv_service);
	if (!service) {
		task_server_terminate(task, "wreplsrv_task_init: out of memory", true);
		return;
	}
	service->task		= task;
	service->startup_time	= timeval_current();
	task->private_data	= service;

	/*
	 * setup up all partners, and open the winsdb
	 */
	status = wreplsrv_open_winsdb(service, task->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "wreplsrv_task_init: wreplsrv_open_winsdb() failed", true);
		return;
	}

	/*
	 * setup timed events for each partner we want to pull from
	 */
	status = wreplsrv_setup_partners(service);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "wreplsrv_task_init: wreplsrv_setup_partners() failed", true);
		return;
	}

	/* 
	 * setup listen sockets, so we can answer requests from our partners,
	 * which pull from us
	 */
	status = wreplsrv_setup_sockets(service, task->lp_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "wreplsrv_task_init: wreplsrv_setup_sockets() failed", true);
		return;
	}

	status = wreplsrv_setup_periodic(service);
	if (!NT_STATUS_IS_OK(status)) {
		task_server_terminate(task, "wreplsrv_task_init: wreplsrv_setup_periodic() failed", true);
		return;
	}

	irpc_add_name(task->msg_ctx, "wrepl_server");
}

/*
  register ourselves as a available server
*/
NTSTATUS server_service_wrepl_init(void)
{
	return register_server_service("wrepl", wreplsrv_task_init);
}
