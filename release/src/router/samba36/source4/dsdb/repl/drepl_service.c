/*
   Unix SMB/CIFS mplementation.
   DSDB replication service

   Copyright (C) Stefan Metzmacher 2007
   Copyright (C) Kamen Mazdrashki <kamenim@samba.org> 2010

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
#include "librpc/gen_ndr/ndr_irpc.h"
#include "param/param.h"

/**
 * Call-back data for _drepl_replica_sync_done_cb()
 */
struct drepl_replica_sync_cb_data {
	struct irpc_message *msg;
	struct drsuapi_DsReplicaSync *r;

	/* number of ops left to be completed */
	int ops_count;

	/* last failure error code */
	WERROR werr_last_failure;
};


static WERROR dreplsrv_init_creds(struct dreplsrv_service *service)
{
	service->system_session_info = system_session(service->task->lp_ctx);
	if (service->system_session_info == NULL) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}

static WERROR dreplsrv_connect_samdb(struct dreplsrv_service *service, struct loadparm_context *lp_ctx)
{
	const struct GUID *ntds_guid;
	struct drsuapi_DsBindInfo28 *bind_info28;

	service->samdb = samdb_connect(service, service->task->event_ctx, lp_ctx, service->system_session_info, 0);
	if (!service->samdb) {
		return WERR_DS_UNAVAILABLE;
	}

	ntds_guid = samdb_ntds_objectGUID(service->samdb);
	if (!ntds_guid) {
		return WERR_DS_UNAVAILABLE;
	}
	service->ntds_guid = *ntds_guid;

	if (samdb_rodc(service->samdb, &service->am_rodc) != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to determine RODC status\n"));
		return WERR_DS_UNAVAILABLE;
	}

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
	bind_info28->supported_extensions	|= DRSUAPI_SUPPORTED_EXTENSION_GETCHGREQ_V5;
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


/**
 * Callback for dreplsrv_out_operation operation completion.
 *
 * We just need to complete a waiting IRPC message here.
 * In case pull operation has failed,
 * caller of this callback will dump
 * failure information.
 *
 * NOTE: cb_data is allocated in IRPC msg's context
 * and will be freed during irpc_send_reply() call.
 */
static void _drepl_replica_sync_done_cb(struct dreplsrv_service *service,
					WERROR werr,
					enum drsuapi_DsExtendedError ext_err,
					void *cb_data)
{
	struct drepl_replica_sync_cb_data *data = talloc_get_type(cb_data,
	                                                          struct drepl_replica_sync_cb_data);
	struct irpc_message *msg = data->msg;
	struct drsuapi_DsReplicaSync *r = data->r;

	/* store last bad result */
	if (!W_ERROR_IS_OK(werr)) {
		data->werr_last_failure = werr;
	}

	/* decrement pending ops count */
	data->ops_count--;

	if (data->ops_count == 0) {
		/* Return result to client */
		r->out.result = data->werr_last_failure;

		/* complete IRPC message */
		irpc_send_reply(msg, NT_STATUS_OK);
	}
}

/**
 * Helper to schedule a replication operation with a source DSA.
 * If 'data' is valid pointer, then a callback
 * for the operation is passed and 'data->msg' is
 * marked as 'deferred' - defer_reply = true
 */
static WERROR _drepl_schedule_replication(struct dreplsrv_service *service,
					  struct dreplsrv_partition_source_dsa *dsa,
					  struct drsuapi_DsReplicaObjectIdentifier *nc,
					  uint32_t rep_options,
					  struct drepl_replica_sync_cb_data *data,
					  TALLOC_CTX *mem_ctx)
{
	WERROR werr;
	dreplsrv_extended_callback_t fn_callback = NULL;

	if (data) {
		fn_callback = _drepl_replica_sync_done_cb;
	}

	/* schedule replication item */
	werr = dreplsrv_schedule_partition_pull_source(service, dsa, rep_options,
	                                               DRSUAPI_EXOP_NONE, 0,
	                                               fn_callback, data);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("%s: failed setup of sync of partition (%s, %s, %s) - %s\n",
			 __FUNCTION__,
			 GUID_string(mem_ctx, &nc->guid),
			 nc->dn,
			 dsa->repsFrom1->other_info->dns_name,
			 win_errstr(werr)));
		return werr;
	}
	/* log we've scheduled a replication item */
	DEBUG(3,("%s: forcing sync of partition (%s, %s, %s)\n",
		 __FUNCTION__,
		 GUID_string(mem_ctx, &nc->guid),
		 nc->dn,
		 dsa->repsFrom1->other_info->dns_name));

	/* mark IRPC message as deferred if necessary */
	if (data) {
		data->ops_count++;
		data->msg->defer_reply = true;
	}

	return WERR_OK;
}

/*
  DsReplicaSync messages from the DRSUAPI server are forwarded here
 */
static NTSTATUS drepl_replica_sync(struct irpc_message *msg,
				   struct drsuapi_DsReplicaSync *r)
{
	WERROR werr;
	struct dreplsrv_partition *p;
	struct drepl_replica_sync_cb_data *cb_data;
	struct dreplsrv_partition_source_dsa *dsa;
	struct drsuapi_DsReplicaSyncRequest1 *req1;
	struct drsuapi_DsReplicaObjectIdentifier *nc;
	struct dreplsrv_service *service = talloc_get_type(msg->private_data,
							   struct dreplsrv_service);

#define REPLICA_SYNC_FAIL(_msg, _werr) do {\
		if (!W_ERROR_IS_OK(_werr)) { \
			DEBUG(0,(__location__ ": Failure - %s. werr = %s\n", \
				 _msg, win_errstr(_werr))); \
			NDR_PRINT_IN_DEBUG(drsuapi_DsReplicaSync, r); \
		} \
		r->out.result = _werr; \
		goto done;\
	} while(0)


	if (r->in.level != 1) {
		REPLICA_SYNC_FAIL("Unsupported level",
				  WERR_DS_DRA_INVALID_PARAMETER);
	}

	req1 = &r->in.req->req1;
	nc   = req1->naming_context;

	/* Check input parameters */
	if (!nc) {
		REPLICA_SYNC_FAIL("Invalid Naming Context",
		                  WERR_DS_DRA_INVALID_PARAMETER);
	}

	/* Find Naming context to be synchronized */
	werr = dreplsrv_partition_find_for_nc(service,
	                                      &nc->guid, &nc->sid, nc->dn,
	                                      &p);
	if (!W_ERROR_IS_OK(werr)) {
		REPLICA_SYNC_FAIL("Failed to find requested Naming Context",
				  werr);
	}

	/* should we process it asynchronously? */
	if (req1->options & DRSUAPI_DRS_ASYNC_OP) {
		cb_data = NULL;
	} else {
		cb_data = talloc_zero(msg, struct drepl_replica_sync_cb_data);
		if (!cb_data) {
			REPLICA_SYNC_FAIL("Not enought memory",
					  WERR_DS_DRA_INTERNAL_ERROR);
		}

		cb_data->msg = msg;
		cb_data->r   = r;
		cb_data->werr_last_failure = WERR_OK;
	}

	/* collect source DSAs to sync with */
	if (req1->options & DRSUAPI_DRS_SYNC_ALL) {
		for (dsa = p->sources; dsa; dsa = dsa->next) {
			/* schedule replication item */
			werr = _drepl_schedule_replication(service, dsa, nc,
							   req1->options, cb_data, msg);
			if (!W_ERROR_IS_OK(werr)) {
				REPLICA_SYNC_FAIL("_drepl_schedule_replication() failed",
				                  werr);
			}
		}
	} else {
		if (req1->options & DRSUAPI_DRS_SYNC_BYNAME) {
			/* client should pass at least valid string */
			if (!req1->source_dsa_dns) {
				REPLICA_SYNC_FAIL("'source_dsa_dns' is not valid",
				                  WERR_DS_DRA_INVALID_PARAMETER);
			}

			werr = dreplsrv_partition_source_dsa_by_dns(p,
			                                            req1->source_dsa_dns,
			                                            &dsa);
		} else {
			/* client should pass at least some GUID */
			if (GUID_all_zero(&req1->source_dsa_guid)) {
				REPLICA_SYNC_FAIL("'source_dsa_guid' is not valid",
				                  WERR_DS_DRA_INVALID_PARAMETER);
			}

			werr = dreplsrv_partition_source_dsa_by_guid(p,
			                                             &req1->source_dsa_guid,
			                                             &dsa);
		}
		if (!W_ERROR_IS_OK(werr)) {
			REPLICA_SYNC_FAIL("Failed to locate source DSA for given NC",
					  werr);
		}

		/* schedule replication item */
		werr = _drepl_schedule_replication(service, dsa, nc,
						   req1->options, cb_data, msg);
		if (!W_ERROR_IS_OK(werr)) {
			REPLICA_SYNC_FAIL("_drepl_schedule_replication() failed",
			                  werr);
		}
	}

	/* if we got here, everything is OK */
	r->out.result = WERR_OK;

	/*
	 * schedule replication event to force
	 * replication as soon as possible
	 */
	dreplsrv_periodic_schedule(service, 0);

done:
	return NT_STATUS_OK;
}

/**
 * Called when drplsrv should refresh its state.
 * For example, when KCC change topology, dreplsrv
 * should update its cache
 *
 * @param partition_dn If not empty/NULL, partition to update
 */
static NTSTATUS dreplsrv_refresh(struct irpc_message *msg,
				 struct dreplsrv_refresh *r)
{
	struct dreplsrv_service *s = talloc_get_type(msg->private_data,
						     struct dreplsrv_service);

	r->out.result = dreplsrv_refresh_partitions(s);

	return NT_STATUS_OK;
}

static NTSTATUS drepl_take_FSMO_role(struct irpc_message *msg,
				     struct drepl_takeFSMORole *r)
{
	struct dreplsrv_service *service = talloc_get_type(msg->private_data,
							   struct dreplsrv_service);
	r->out.result = dreplsrv_fsmo_role_check(service, r->in.role);
	return NT_STATUS_OK;
}

/**
 * Called when the auth code wants us to try and replicate
 * a users secrets
 */
static NTSTATUS drepl_trigger_repl_secret(struct irpc_message *msg,
					  struct drepl_trigger_repl_secret *r)
{
	struct dreplsrv_service *service = talloc_get_type(msg->private_data,
							   struct dreplsrv_service);


	drepl_repl_secret(service, r->in.user_dn);

	/* we are not going to be sending a reply to this request */
	msg->no_reply = true;

	return NT_STATUS_OK;
}


/*
  DsReplicaAdd messages from the DRSUAPI server are forwarded here
 */
static NTSTATUS dreplsrv_replica_add(struct irpc_message *msg,
				  struct drsuapi_DsReplicaAdd *r)
{
	struct dreplsrv_service *service = talloc_get_type(msg->private_data,
							   struct dreplsrv_service);
	return drepl_replica_add(service, r);
}

/*
  DsReplicaDel messages from the DRSUAPI server are forwarded here
 */
static NTSTATUS dreplsrv_replica_del(struct irpc_message *msg,
				  struct drsuapi_DsReplicaDel *r)
{
	struct dreplsrv_service *service = talloc_get_type(msg->private_data,
							   struct dreplsrv_service);
	return drepl_replica_del(service, r);
}

/*
  DsReplicaMod messages from the DRSUAPI server are forwarded here
 */
static NTSTATUS dreplsrv_replica_mod(struct irpc_message *msg,
				  struct drsuapi_DsReplicaMod *r)
{
	struct dreplsrv_service *service = talloc_get_type(msg->private_data,
							   struct dreplsrv_service);
	return drepl_replica_mod(service, r);
}


/*
  startup the dsdb replicator service task
*/
static void dreplsrv_task_init(struct task_server *task)
{
	WERROR status;
	struct dreplsrv_service *service;
	uint32_t periodic_startup_interval;

	switch (lpcfg_server_role(task->lp_ctx)) {
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

	periodic_startup_interval	= lpcfg_parm_int(task->lp_ctx, NULL, "dreplsrv", "periodic_startup_interval", 15); /* in seconds */
	service->periodic.interval	= lpcfg_parm_int(task->lp_ctx, NULL, "dreplsrv", "periodic_interval", 300); /* in seconds */

	status = dreplsrv_periodic_schedule(service, periodic_startup_interval);
	if (!W_ERROR_IS_OK(status)) {
		task_server_terminate(task, talloc_asprintf(task,
				      "dreplsrv: Failed to periodic schedule: %s\n",
							    win_errstr(status)), true);
		return;
	}

	/* if we are a RODC then we do not send DSReplicaSync*/
	if (!service->am_rodc) {
		service->notify.interval = lpcfg_parm_int(task->lp_ctx, NULL, "dreplsrv",
							   "notify_interval", 5); /* in seconds */
		status = dreplsrv_notify_schedule(service, service->notify.interval);
		if (!W_ERROR_IS_OK(status)) {
			task_server_terminate(task, talloc_asprintf(task,
						  "dreplsrv: Failed to setup notify schedule: %s\n",
									win_errstr(status)), true);
			return;
		}
	}

	irpc_add_name(task->msg_ctx, "dreplsrv");

	IRPC_REGISTER(task->msg_ctx, irpc, DREPLSRV_REFRESH, dreplsrv_refresh, service);
	IRPC_REGISTER(task->msg_ctx, drsuapi, DRSUAPI_DSREPLICASYNC, drepl_replica_sync, service);
	IRPC_REGISTER(task->msg_ctx, drsuapi, DRSUAPI_DSREPLICAADD, dreplsrv_replica_add, service);
	IRPC_REGISTER(task->msg_ctx, drsuapi, DRSUAPI_DSREPLICADEL, dreplsrv_replica_del, service);
	IRPC_REGISTER(task->msg_ctx, drsuapi, DRSUAPI_DSREPLICAMOD, dreplsrv_replica_mod, service);
	IRPC_REGISTER(task->msg_ctx, irpc, DREPL_TAKEFSMOROLE, drepl_take_FSMO_role, service);
	IRPC_REGISTER(task->msg_ctx, irpc, DREPL_TRIGGER_REPL_SECRET, drepl_trigger_repl_secret, service);
	messaging_register(task->msg_ctx, service, MSG_DREPL_ALLOCATE_RID, dreplsrv_allocate_rid);
}

/*
  register ourselves as a available server
*/
NTSTATUS server_service_drepl_init(void)
{
	return register_server_service("drepl", dreplsrv_task_init);
}
