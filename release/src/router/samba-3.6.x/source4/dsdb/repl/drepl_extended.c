/*
   Unix SMB/CIFS mplementation.

   DSDB replication service - extended operation code

   Copyright (C) Andrew Tridgell 2010
   Copyright (C) Andrew Bartlett 2010
   Copyright (C) Nadezhda Ivanova 2010

   based on drepl_notify.c

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
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"
#include "smbd/service.h"
#include "dsdb/repl/drepl_service.h"
#include "param/param.h"


/*
  create the role owner source dsa structure

  nc_dn: the DN of the subtree being replicated
  source_dsa_dn: the DN of the server that we are replicating from
 */
static WERROR drepl_create_extended_source_dsa(struct dreplsrv_service *service,
					       struct ldb_dn *nc_dn,
					       struct ldb_dn *source_dsa_dn,
					       uint64_t min_usn,
					       struct dreplsrv_partition_source_dsa **_sdsa)
{
	struct dreplsrv_partition_source_dsa *sdsa;
	struct ldb_context *ldb = service->samdb;
	int ret;
	WERROR werr;
	struct ldb_dn *nc_root;
	struct dreplsrv_partition *p;

	sdsa = talloc_zero(service, struct dreplsrv_partition_source_dsa);
	W_ERROR_HAVE_NO_MEMORY(sdsa);

	sdsa->partition = talloc_zero(sdsa, struct dreplsrv_partition);
	if (!sdsa->partition) {
		talloc_free(sdsa);
		return WERR_NOMEM;
	}

	sdsa->partition->dn = ldb_dn_copy(sdsa->partition, nc_dn);
	if (!sdsa->partition->dn) {
		talloc_free(sdsa);
		return WERR_NOMEM;
	}
	sdsa->partition->nc.dn = ldb_dn_alloc_linearized(sdsa->partition, nc_dn);
	if (!sdsa->partition->nc.dn) {
		talloc_free(sdsa);
		return WERR_NOMEM;
	}
	ret = dsdb_find_guid_by_dn(ldb, nc_dn, &sdsa->partition->nc.guid);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find GUID for %s\n",
			 ldb_dn_get_linearized(nc_dn)));
		talloc_free(sdsa);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	sdsa->repsFrom1 = &sdsa->_repsFromBlob.ctr.ctr1;
	ret = dsdb_find_guid_by_dn(ldb, source_dsa_dn, &sdsa->repsFrom1->source_dsa_obj_guid);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find objectGUID for %s\n",
			 ldb_dn_get_linearized(source_dsa_dn)));
		talloc_free(sdsa);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	sdsa->repsFrom1->other_info = talloc_zero(sdsa, struct repsFromTo1OtherInfo);
	if (!sdsa->repsFrom1->other_info) {
		talloc_free(sdsa);
		return WERR_NOMEM;
	}

	sdsa->repsFrom1->other_info->dns_name =
		talloc_asprintf(sdsa->repsFrom1->other_info, "%s._msdcs.%s",
				GUID_string(sdsa->repsFrom1->other_info, &sdsa->repsFrom1->source_dsa_obj_guid),
				lpcfg_dnsdomain(service->task->lp_ctx));
	if (!sdsa->repsFrom1->other_info->dns_name) {
		talloc_free(sdsa);
		return WERR_NOMEM;
	}

	werr = dreplsrv_out_connection_attach(service, sdsa->repsFrom1, &sdsa->conn);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": Failed to attach connection to %s\n",
			 ldb_dn_get_linearized(nc_dn)));
		talloc_free(sdsa);
		return werr;
	}

	ret = dsdb_find_nc_root(service->samdb, sdsa, nc_dn, &nc_root);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find nc_root for %s\n",
			 ldb_dn_get_linearized(nc_dn)));
		talloc_free(sdsa);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	/* use the partition uptodateness vector */
	ret = dsdb_load_udv_v2(service->samdb, nc_root, sdsa->partition,
			       &sdsa->partition->uptodatevector.cursors,
			       &sdsa->partition->uptodatevector.count);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to load UDV for %s\n",
			 ldb_dn_get_linearized(nc_root)));
		talloc_free(sdsa);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	/* find the highwatermark from the partitions list */
	for (p=service->partitions; p; p=p->next) {
		if (ldb_dn_compare(p->dn, nc_root) == 0) {
			struct dreplsrv_partition_source_dsa *s;
			werr = dreplsrv_partition_source_dsa_by_guid(p,
								     &sdsa->repsFrom1->source_dsa_obj_guid,
								     &s);
			if (W_ERROR_IS_OK(werr)) {
				sdsa->repsFrom1->highwatermark = s->repsFrom1->highwatermark;
				sdsa->repsFrom1->replica_flags = s->repsFrom1->replica_flags;
			}
		}
	}

	if (!service->am_rodc) {
		sdsa->repsFrom1->replica_flags |= DRSUAPI_DRS_WRIT_REP;
	}

	*_sdsa = sdsa;
	return WERR_OK;
}

struct extended_op_data {
	dreplsrv_extended_callback_t callback;
	void *callback_data;
	struct dreplsrv_partition_source_dsa *sdsa;
};

/*
  called when an extended op finishes
 */
static void extended_op_callback(struct dreplsrv_service *service,
				 WERROR err,
				 enum drsuapi_DsExtendedError exop_error,
				 void *cb_data)
{
	struct extended_op_data *data = talloc_get_type_abort(cb_data, struct extended_op_data);
	talloc_free(data->sdsa);
	data->callback(service, err, exop_error, data->callback_data);
	talloc_free(data);
}

/*
  schedule a getncchanges request to the role owner for an extended operation
 */
WERROR drepl_request_extended_op(struct dreplsrv_service *service,
				 struct ldb_dn *nc_dn,
				 struct ldb_dn *source_dsa_dn,
				 enum drsuapi_DsExtendedOperation extended_op,
				 uint64_t fsmo_info,
				 uint64_t min_usn,
				 dreplsrv_extended_callback_t callback,
				 void *callback_data)
{
	WERROR werr;
	struct extended_op_data *data;
	struct dreplsrv_partition_source_dsa *sdsa;

	werr = drepl_create_extended_source_dsa(service, nc_dn, source_dsa_dn, min_usn, &sdsa);
	W_ERROR_NOT_OK_RETURN(werr);

	data = talloc(service, struct extended_op_data);
	W_ERROR_HAVE_NO_MEMORY(data);

	data->callback = callback;
	data->callback_data = callback_data;
	data->sdsa = sdsa;

	werr = dreplsrv_schedule_partition_pull_source(service, sdsa,
						       0, extended_op, fsmo_info,
						       extended_op_callback, data);
	if (!W_ERROR_IS_OK(werr)) {
		talloc_free(sdsa);
		talloc_free(data);
	}

	dreplsrv_run_pending_ops(service);

	return werr;
}
