/* 
   Unix SMB/CIFS mplementation.

   KCC service
   
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
#include "dsdb/samdb/samdb.h"
#include "auth/auth.h"
#include "smbd/service.h"
#include "lib/events/events.h"
#include "lib/messaging/irpc.h"
#include "dsdb/kcc/kcc_service.h"
#include "lib/ldb/include/ldb_errors.h"
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "param/param.h"

/*
  establish system creds
 */
static WERROR kccsrv_init_creds(struct kccsrv_service *service)
{
	NTSTATUS status;

	status = auth_system_session_info(service, service->task->lp_ctx, 
					  &service->system_session_info);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return WERR_OK;
}

/*
  connect to the local SAM
 */
static WERROR kccsrv_connect_samdb(struct kccsrv_service *service, struct loadparm_context *lp_ctx)
{
	const struct GUID *ntds_guid;

	service->samdb = samdb_connect(service, service->task->event_ctx, lp_ctx, service->system_session_info);
	if (!service->samdb) {
		return WERR_DS_UNAVAILABLE;
	}

	ntds_guid = samdb_ntds_objectGUID(service->samdb);
	if (!ntds_guid) {
		return WERR_DS_UNAVAILABLE;
	}

	service->ntds_guid = *ntds_guid;

	return WERR_OK;
}


/*
  load our local partition list
 */
static WERROR kccsrv_load_partitions(struct kccsrv_service *s)
{
	struct ldb_dn *basedn;
	struct ldb_result *r;
	struct ldb_message_element *el;
	static const char *attrs[] = { "namingContexts", "configurationNamingContext", NULL };
	uint32_t i;
	int ret;

	basedn = ldb_dn_new(s, s->samdb, NULL);
	W_ERROR_HAVE_NO_MEMORY(basedn);

	ret = ldb_search(s->samdb, s, &r, basedn, LDB_SCOPE_BASE, attrs,
			 "(objectClass=*)");
	talloc_free(basedn);
	if (ret != LDB_SUCCESS) {
		return WERR_FOOBAR;
	} else if (r->count != 1) {
		talloc_free(r);
		return WERR_FOOBAR;
	}

	el = ldb_msg_find_element(r->msgs[0], "namingContexts");
	if (!el) {
		return WERR_FOOBAR;
	}

	for (i=0; el && i < el->num_values; i++) {
		const char *v = (const char *)el->values[i].data;
		struct ldb_dn *pdn;
		struct kccsrv_partition *p;

		pdn = ldb_dn_new(s, s->samdb, v);
		if (!ldb_dn_validate(pdn)) {
			return WERR_FOOBAR;
		}

		p = talloc_zero(s, struct kccsrv_partition);
		W_ERROR_HAVE_NO_MEMORY(p);

		p->dn = talloc_steal(p, pdn);
		p->service = s;

		DLIST_ADD(s->partitions, p);

		DEBUG(2, ("kccsrv_partition[%s] loaded\n", v));
	}

	el = ldb_msg_find_element(r->msgs[0], "configurationNamingContext");
	if (!el) {
		return WERR_FOOBAR;
	}
	s->config_dn = ldb_dn_new(s, s->samdb, (const char *)el->values[0].data);
	if (!ldb_dn_validate(s->config_dn)) {
		return WERR_FOOBAR;
	}

	talloc_free(r);

	return WERR_OK;
}


/*
  startup the kcc service task
*/
static void kccsrv_task_init(struct task_server *task)
{
	WERROR status;
	struct kccsrv_service *service;
	uint32_t periodic_startup_interval;

	switch (lp_server_role(task->lp_ctx)) {
	case ROLE_STANDALONE:
		task_server_terminate(task, "kccsrv: no KCC required in standalone configuration", false);
		return;
	case ROLE_DOMAIN_MEMBER:
		task_server_terminate(task, "kccsrv: no KCC required in domain member configuration", false);
		return;
	case ROLE_DOMAIN_CONTROLLER:
		/* Yes, we want a KCC */
		break;
	}

	task_server_set_title(task, "task[kccsrv]");

	service = talloc_zero(task, struct kccsrv_service);
	if (!service) {
		task_server_terminate(task, "kccsrv_task_init: out of memory", true);
		return;
	}
	service->task		= task;
	service->startup_time	= timeval_current();
	task->private_data	= service;

	status = kccsrv_init_creds(service);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, 
				      talloc_asprintf(task,
						      "kccsrv: Failed to obtain server credentials: %s\n",
						      win_errstr(status)), true);
		return;
	}

	status = kccsrv_connect_samdb(service, task->lp_ctx);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "kccsrv: Failed to connect to local samdb: %s\n",
							    win_errstr(status)), true);
		return;
	}

	status = kccsrv_load_partitions(service);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "kccsrv: Failed to load partitions: %s\n",
							    win_errstr(status)), true);
		return;
	}

	periodic_startup_interval	= lp_parm_int(task->lp_ctx, NULL, "kccsrv", 
						      "periodic_startup_interval", 15); /* in seconds */
	service->periodic.interval	= lp_parm_int(task->lp_ctx, NULL, "kccsrv", 
						      "periodic_interval", 300); /* in seconds */

	status = kccsrv_periodic_schedule(service, periodic_startup_interval);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "kccsrv: Failed to periodic schedule: %s\n",
							    win_errstr(status)), true);
		return;
	}

	irpc_add_name(task->msg_ctx, "kccsrv");
}

/*
  register ourselves as a available server
*/
NTSTATUS server_service_kcc_init(void)
{
	return register_server_service("kcc", kccsrv_task_init);
}
