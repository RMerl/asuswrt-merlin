/*
   Unix SMB/CIFS mplementation.

   DSDB replication service - RID allocation code

   Copyright (C) Andrew Tridgell 2010
   Copyright (C) Andrew Bartlett 2010

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
  called when a rid allocation request has completed
 */
static void drepl_new_rid_pool_callback(struct dreplsrv_service *service,
					WERROR werr,
					enum drsuapi_DsExtendedError ext_err,
					void *cb_data)
{
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": RID Manager failed RID allocation - %s - extended_ret[0x%X]\n",
			 win_errstr(werr), ext_err));
	} else {
		DEBUG(3,(__location__ ": RID Manager completed RID allocation OK\n"));
	}

	service->rid_alloc_in_progress = false;
}

/*
  schedule a getncchanges request to the RID Manager to ask for a new
  set of RIDs using DRSUAPI_EXOP_FSMO_RID_ALLOC
 */
static WERROR drepl_request_new_rid_pool(struct dreplsrv_service *service,
					 struct ldb_dn *rid_manager_dn, struct ldb_dn *fsmo_role_dn,
					 uint64_t alloc_pool)
{
	WERROR werr = drepl_request_extended_op(service,
						rid_manager_dn,
						fsmo_role_dn,
						DRSUAPI_EXOP_FSMO_RID_ALLOC,
						alloc_pool,
						0,
						drepl_new_rid_pool_callback, NULL);
	if (W_ERROR_IS_OK(werr)) {
		service->rid_alloc_in_progress = true;
	}
	return werr;
}


/*
  see if we are on the last pool we have
 */
static int drepl_ridalloc_pool_exhausted(struct ldb_context *ldb,
					 bool *exhausted,
					 uint64_t *_alloc_pool)
{
	struct ldb_dn *server_dn, *machine_dn, *rid_set_dn;
	TALLOC_CTX *tmp_ctx = talloc_new(ldb);
	uint64_t alloc_pool;
	uint64_t prev_pool;
	uint32_t prev_pool_lo, prev_pool_hi;
	uint32_t next_rid;
	static const char * const attrs[] = {
		"rIDAllocationPool",
		"rIDPreviousAllocationPool",
		"rIDNextRid",
		NULL
	};
	int ret;
	struct ldb_result *res;

	*exhausted = false;
	*_alloc_pool = UINT64_MAX;

	server_dn = ldb_dn_get_parent(tmp_ctx, samdb_ntds_settings_dn(ldb));
	if (!server_dn) {
		talloc_free(tmp_ctx);
		return ldb_operr(ldb);
	}

	ret = samdb_reference_dn(ldb, tmp_ctx, server_dn, "serverReference", &machine_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find serverReference in %s - %s",
			 ldb_dn_get_linearized(server_dn), ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = samdb_reference_dn(ldb, tmp_ctx, machine_dn, "rIDSetReferences", &rid_set_dn);
	if (ret == LDB_ERR_NO_SUCH_ATTRIBUTE) {
		*exhausted = true;
		*_alloc_pool = 0;
		talloc_free(tmp_ctx);
		return LDB_SUCCESS;
	}
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find rIDSetReferences in %s - %s",
			 ldb_dn_get_linearized(machine_dn), ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return ret;
	}

	ret = ldb_search(ldb, tmp_ctx, &res, rid_set_dn, LDB_SCOPE_BASE, attrs, NULL);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to load RID Set attrs from %s - %s",
			 ldb_dn_get_linearized(rid_set_dn), ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return ret;
	}

	alloc_pool = ldb_msg_find_attr_as_uint64(res->msgs[0], "rIDAllocationPool", 0);
	prev_pool = ldb_msg_find_attr_as_uint64(res->msgs[0], "rIDPreviousAllocationPool", 0);
	prev_pool_lo = prev_pool & 0xFFFFFFFF;
	prev_pool_hi = prev_pool >> 32;
	next_rid = ldb_msg_find_attr_as_uint(res->msgs[0], "rIDNextRid", 0);

	if (alloc_pool != prev_pool) {
		talloc_free(tmp_ctx);
		return LDB_SUCCESS;
	}

	if (next_rid < (prev_pool_hi + prev_pool_lo)/2) {
		talloc_free(tmp_ctx);
		return LDB_SUCCESS;
	}

	*exhausted = true;
	*_alloc_pool = alloc_pool;
	talloc_free(tmp_ctx);
	return LDB_SUCCESS;
}


/*
  see if we are low on RIDs in the RID Set rIDAllocationPool. If we
  are, then schedule a replication call with DRSUAPI_EXOP_FSMO_RID_ALLOC
  to the RID Manager
 */
WERROR dreplsrv_ridalloc_check_rid_pool(struct dreplsrv_service *service)
{
	struct ldb_dn *rid_manager_dn, *fsmo_role_dn;
	TALLOC_CTX *tmp_ctx = talloc_new(service);
	struct ldb_context *ldb = service->samdb;
	bool exhausted;
	WERROR werr;
	int ret;
	uint64_t alloc_pool;

	if (service->am_rodc) {
		talloc_free(tmp_ctx);
		return WERR_OK;
	}

	if (service->rid_alloc_in_progress) {
		talloc_free(tmp_ctx);
		return WERR_OK;
	}

	/*
	  steps:
	    - find who the RID Manager is
	    - if we are the RID Manager then nothing to do
	    - find our RID Set object
	    - load rIDAllocationPool and rIDPreviousAllocationPool
	    - if rIDAllocationPool != rIDPreviousAllocationPool then
	      nothing to do
	    - schedule a getncchanges with DRSUAPI_EXOP_FSMO_RID_ALLOC
	      to the RID Manager
	 */

	/* work out who is the RID Manager */
	ret = samdb_rid_manager_dn(ldb, tmp_ctx, &rid_manager_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0, (__location__ ": Failed to find RID Manager object - %s", ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	/* find the DN of the RID Manager */
	ret = samdb_reference_dn(ldb, tmp_ctx, rid_manager_dn, "fSMORoleOwner", &fsmo_role_dn);
	if (ret != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Failed to find fSMORoleOwner in RID Manager object - %s",
			 ldb_errstring(ldb)));
		talloc_free(tmp_ctx);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	if (ldb_dn_compare(samdb_ntds_settings_dn(ldb), fsmo_role_dn) == 0) {
		/* we are the RID Manager - no need to do a
		   DRSUAPI_EXOP_FSMO_RID_ALLOC */
		talloc_free(tmp_ctx);
		return WERR_OK;
	}

	ret = drepl_ridalloc_pool_exhausted(ldb, &exhausted, &alloc_pool);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	if (!exhausted) {
		/* don't need a new pool */
		talloc_free(tmp_ctx);
		return WERR_OK;
	}

	DEBUG(2,(__location__ ": Requesting more RIDs from RID Manager\n"));

	werr = drepl_request_new_rid_pool(service, rid_manager_dn, fsmo_role_dn, alloc_pool);
	talloc_free(tmp_ctx);
	return werr;
}

/* called by the samldb ldb module to tell us to ask for a new RID
   pool */
void dreplsrv_allocate_rid(struct messaging_context *msg, void *private_data,
			   uint32_t msg_type,
			   struct server_id server_id, DATA_BLOB *data)
{
	struct dreplsrv_service *service = talloc_get_type(private_data, struct dreplsrv_service);
	dreplsrv_ridalloc_check_rid_pool(service);
}
