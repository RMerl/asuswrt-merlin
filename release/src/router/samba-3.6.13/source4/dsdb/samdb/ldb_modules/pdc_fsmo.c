/* 
   Unix SMB/CIFS mplementation.

   The module that handles the PDC FSMO Role Owner checkings
   
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
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "../lib/util/dlinklist.h"
#include "dsdb/samdb/ldb_modules/util.h"

static int pdc_fsmo_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	TALLOC_CTX *mem_ctx;
	struct ldb_dn *pdc_dn;
	struct dsdb_pdc_fsmo *pdc_fsmo;
	struct ldb_result *pdc_res;
	int ret;
	static const char *pdc_attrs[] = {
		"fSMORoleOwner",
		NULL
	};

	ldb = ldb_module_get_ctx(module);

	mem_ctx = talloc_new(module);
	if (!mem_ctx) {
		return ldb_oom(ldb);
	}

	pdc_dn = ldb_get_default_basedn(ldb);
	if (!pdc_dn) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			  "pdc_fsmo_init: could not determine default basedn");
		talloc_free(mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	pdc_fsmo = talloc_zero(mem_ctx, struct dsdb_pdc_fsmo);
	if (!pdc_fsmo) {
		return ldb_oom(ldb);
	}
	ldb_module_set_private(module, pdc_fsmo);

	ret = dsdb_module_search_dn(module, mem_ctx, &pdc_res,
				    pdc_dn, 
				    pdc_attrs,
				    DSDB_FLAG_NEXT_MODULE, NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		ldb_debug(ldb, LDB_DEBUG_TRACE,
			  "pdc_fsmo_init: no domain object present: (skip loading of domain details)");
		talloc_free(mem_ctx);
		return ldb_next_init(module);
	} else if (ret != LDB_SUCCESS) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			      "pdc_fsmo_init: failed to search the domain object: %d:%s: %s",
			      ret, ldb_strerror(ret), ldb_errstring(ldb));
		talloc_free(mem_ctx);
		return ret;
	}

	pdc_fsmo->master_dn = ldb_msg_find_attr_as_dn(ldb, mem_ctx, pdc_res->msgs[0], "fSMORoleOwner");
	if (ldb_dn_compare(samdb_ntds_settings_dn(ldb), pdc_fsmo->master_dn) == 0) {
		pdc_fsmo->we_are_master = true;
	} else {
		pdc_fsmo->we_are_master = false;
	}

	if (ldb_set_opaque(ldb, "dsdb_pdc_fsmo", pdc_fsmo) != LDB_SUCCESS) {
		return ldb_oom(ldb);
	}

	talloc_steal(module, pdc_fsmo);

	ldb_debug(ldb, LDB_DEBUG_TRACE,
			  "pdc_fsmo_init: we are master: %s\n",
			  (pdc_fsmo->we_are_master?"yes":"no"));

	talloc_free(mem_ctx);
	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_pdc_fsmo_module_ops = {
	.name		= "pdc_fsmo",
	.init_context	= pdc_fsmo_init
};

int ldb_pdc_fsmo_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_pdc_fsmo_module_ops);
}
