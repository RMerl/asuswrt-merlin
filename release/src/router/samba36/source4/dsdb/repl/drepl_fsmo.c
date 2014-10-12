/*
   Unix SMB/CIFS mplementation.

   DSDB replication service - FSMO role change

   Copyright (C) Nadezhda Ivanova 2010
   Copyright (C) Andrew Tridgell 2010
   Copyright (C) Andrew Bartlett 2010
   Copyright (C) Anatoliy Atanasov 2010

   based on drepl_ridalloc.c

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
#include "smbd/service.h"
#include "dsdb/repl/drepl_service.h"
#include "param/param.h"

static void drepl_role_callback(struct dreplsrv_service *service,
				WERROR werr,
				enum drsuapi_DsExtendedError ext_err,
				void *cb_data)
{
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,(__location__ ": Failed role transfer - %s - extended_ret[0x%X]\n",
			 win_errstr(werr), ext_err));
	} else {
		DEBUG(0,(__location__ ": Successful role transfer\n"));
	}
}

static bool fsmo_master_cmp(struct ldb_dn *ntds_dn, struct ldb_dn *role_owner_dn)
{
	if (ldb_dn_compare(ntds_dn, role_owner_dn) == 0) {
		DEBUG(0,("\nWe are the FSMO master.\n"));
		return true;
	}
	return false;
}

/*
  see which role is we are asked to assume, initialize data and send request
 */
WERROR dreplsrv_fsmo_role_check(struct dreplsrv_service *service,
				enum drepl_role_master role)
{
	struct ldb_dn *role_owner_dn, *fsmo_role_dn, *ntds_dn;
	TALLOC_CTX *tmp_ctx = talloc_new(service);
	uint64_t fsmo_info = 0;
	enum drsuapi_DsExtendedOperation extended_op = DRSUAPI_EXOP_NONE;
	WERROR werr;

	ntds_dn = samdb_ntds_settings_dn(service->samdb);
	if (!ntds_dn) {
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	werr = dsdb_get_fsmo_role_info(tmp_ctx, service->samdb, role,
				       &fsmo_role_dn, &role_owner_dn);
	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	switch (role) {
	case DREPL_NAMING_MASTER:
	case DREPL_INFRASTRUCTURE_MASTER:
	case DREPL_SCHEMA_MASTER:
		extended_op = DRSUAPI_EXOP_FSMO_REQ_ROLE;
		break;
	case DREPL_RID_MASTER:
		extended_op = DRSUAPI_EXOP_FSMO_RID_REQ_ROLE;
		break;
	case DREPL_PDC_MASTER:
		extended_op = DRSUAPI_EXOP_FSMO_REQ_PDC;
		break;
	default:
		return WERR_DS_DRA_INTERNAL_ERROR;
	}

	if (fsmo_master_cmp(ntds_dn, role_owner_dn) ||
	    (extended_op == DRSUAPI_EXOP_NONE)) {
		DEBUG(0,("FSMO role check failed for DN %s and owner %s ",
			 ldb_dn_get_linearized(fsmo_role_dn),
			 ldb_dn_get_linearized(role_owner_dn)));
		return WERR_OK;
	}

	werr = drepl_request_extended_op(service,
					 fsmo_role_dn,
					 role_owner_dn,
					 extended_op,
					 fsmo_info,
					 0,
					 drepl_role_callback,
					 NULL);
	if (W_ERROR_IS_OK(werr)) {
		dreplsrv_run_pending_ops(service);
	} else {
		DEBUG(0,("%s: drepl_request_extended_op() failed with %s",
			 __FUNCTION__, win_errstr(werr)));
	}
	return werr;
}
