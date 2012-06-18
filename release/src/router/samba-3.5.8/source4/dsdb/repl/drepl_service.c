/* 
   Unix SMB/CIFS mplementation.
   DSDB replication service
   
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
#include "param/param.h"

static WERROR dreplsrv_init_creds(struct dreplsrv_service *service)
{
	NTSTATUS status;

	status = auth_system_session_info(service, service->task->lp_ctx, 
					  &service->system_session_info);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	return WERR_OK;
}

static WERROR dreplsrv_connect_samdb(struct dreplsrv_service *service, struct loadparm_context *lp_ctx)
{
	const struct GUID *ntds_guid;
	struct drsuapi_DsBindInfo28 *bind_info28;

	service->samdb = samdb_connect(service, service->task->event_ctx, lp_ctx, service->system_session_info);
	if (!service->samdb) {
		return WERR_DS_UNAVAILABLE;
	}

	ntds_guid = samdb_ntds_objectGUID(service->samdb);
	if (!ntds_guid) {
		return WERR_DS_UNAVAILABLE;
	}

	service->ntds_guid = *ntds_guid;

	bind_info28				= &service->bind_info28;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_BASE;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ASYNC_REPLICATION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_REMOVEAPI;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_MOVEREQ_V2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHG_COMPRESS;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V1;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_RESTORE_USN_OPTIMIZATION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_KCC_EXECUTE;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRY_V2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_LINKED_VALUE_REPLICATION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_INSTANCE_TYPE_NOT_REQ_ON_MOD;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_CRYPTO_BIND;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_REPL_INFO;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_STRONG_ENCRYPTION;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_DCINFO_V01;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_TRANSITIVE_MEMBERSHIP;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADD_SID_HISTORY;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_POST_BETA3;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_00100000;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GET_MEMBERSHIPS2;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V6;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_NONDOMAIN_NCS;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V8;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V5;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V6;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_ADDENTRYREPLY_V3;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREPLY_V7;
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_VERIFY_OBJECT;
#if 0 /* we don't support XPRESS compression yet */
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_XPRESS_COMPRESS;
#endif
	/* TODO: fill in site_guid */
	bind_info28->site_guid			= GUID_zero();
	/* TODO: find out how this is really triggered! */
	bind_info28->pid			= 0;
	bind_info28->repl_epoch			= 0;

	return WERR_OK;
}

/*
  DsReplicaSync messages from the DRSUAPI server are forwarded here
 */
static NTSTATUS drepl_replica_sync(struct irpc_message *msg, 
				   struct drsuapi_DsReplicaSync *r)
{
	struct dreplsrv_service *service = talloc_get_type(msg->private_data,
							   struct dreplsrv_service);
	struct GUID *guid = &r->in.req.req1.naming_context->guid;

	r->out.result = dreplsrv_schedule_partition_pull_by_guid(service, msg, guid);
	if (W_ERROR_IS_OK(r->out.result)) {
		DEBUG(3,("drepl_replica_sync: forcing sync of partition %s\n",
			 GUID_string(msg, guid)));
		dreplsrv_run_pending_ops(service);
	} else {
		DEBUG(3,("drepl_replica_sync: failed setup of sync of partition %s - %s\n",
			 GUID_string(msg, guid), win_errstr(r->out.result)));
	}
	return NT_STATUS_OK;
}

/*
  startup the dsdb replicator service task
*/
static void dreplsrv_task_init(struct task_server *task)
{
	WERROR status;
	struct dreplsrv_service *service;
	uint32_t periodic_startup_interval;

	switch (lp_server_role(task->lp_ctx)) {
	case ROLE_STANDALONE:
		task_server_terminate(task, "dreplsrv: no DSDB replication required in standalone configuration", 
				      false);
		return;
	case ROLE_DOMAIN_MEMBER:
		task_server_terminate(task, "dreplsrv: no DSDB replication required in domain member configuration", 
				      false);
		return;
	case ROLE_DOMAIN_CONTROLLER:
		/* Yes, we want DSDB replication */
		break;
	}

	task_server_set_title(task, "task[dreplsrv]");

	service = talloc_zero(task, struct dreplsrv_service);
	if (!service) {
		task_server_terminate(task, "dreplsrv_task_init: out of memory", true);
		return;
	}
	service->task		= task;
	service->startup_time	= timeval_current();
	task->private_data	= service;

	status = dreplsrv_init_creds(service);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to obtain server credentials: %s\n",
							    win_errstr(status)), true);
		return;
	}

	status = dreplsrv_connect_samdb(service, task->lp_ctx);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to connect to local samdb: %s\n",
							    win_errstr(status)), true);
		return;
	}

	status = dreplsrv_load_partitions(service);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to load partitions: %s\n",
							    win_errstr(status)), true);
		return;
	}

	periodic_startup_interval	= lp_parm_int(task->lp_ctx, NULL, "dreplsrv", "periodic_startup_interval", 15); /* in seconds */
	service->periodic.interval	= lp_parm_int(task->lp_ctx, NULL, "dreplsrv", "periodic_interval", 300); /* in seconds */

	status = dreplsrv_periodic_schedule(service, periodic_startup_interval);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to periodic schedule: %s\n",
							    win_errstr(status)), true);
		return;
	}

	service->notify.interval = lp_parm_int(task->lp_ctx, NULL, "dreplsrv", 
					       "notify_interval", 5); /* in seconds */
	status = dreplsrv_notify_schedule(service, service->notify.interval);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to setup notify schedule: %s\n",
							    win_errstr(status)), true);
		return;
	}

	irpc_add_name(task->msg_ctx, "dreplsrv");

	IRPC_REGISTER(task->msg_ctx, drsuapi, DRSUAPI_DSREPLICASYNC, drepl_replica_sync, service);
}

/*
  register ourselves as a available server
*/
NTSTATUS server_service_drepl_init(void)
{
	return register_server_service("drepl", dreplsrv_task_init);
}
