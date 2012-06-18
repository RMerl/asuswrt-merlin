/* 
   Unix SMB/CIFS mplementation.

   The module that handles the Domain Naming FSMO Role Owner
   checkings
   
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

static int naming_fsmo_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	TALLOC_CTX *mem_ctx;
	struct ldb_dn *naming_dn;
	struct dsdb_naming_fsmo *naming_fsmo;
	struct ldb_result *naming_res;
	int ret;
	static const char *naming_attrs[] = {
		"fSMORoleOwner",
		NULL
	};

	ldb = ldb_module_get_ctx(module);

	mem_ctx = talloc_new(module);
	if (!mem_ctx) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	naming_dn = samdb_partitions_dn(ldb, mem_ctx);
	if (!naming_dn) {
		ldb_debug(ldb, LDB_DEBUG_WARNING,
			  "naming_fsmo_init: no partitions dn present: (skip loading of naming contexts details)\n");
		talloc_free(mem_ctx);
		return ldb_next_init(module);
	}

	naming_fsmo = talloc_zero(mem_ctx, struct dsdb_naming_fsmo);
	if (!naming_fsmo) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ldb_module_set_private(module, naming_fsmo);

	ret = ldb_search(ldb, mem_ctx, &naming_res,
			 naming_dn, LDB_SCOPE_BASE,
			 naming_attrs, NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		ldb_debug(ldb, LDB_DEBUG_WARNING,
			  "naming_fsmo_init: no partitions dn present: (skip loading of naming contexts details)\n");
		talloc_free(mem_ctx);
		return ldb_next_init(module);
	}
	if (ret != LDB_SUCCESS) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			      "naming_fsmo_init: failed to search the cross-ref container: %s: %s",
			      ldb_strerror(ret), ldb_errstring(ldb));
		talloc_free(mem_ctx);
		return ret;
	}
	if (naming_res->count == 0) {
		ldb_debug(ldb, LDB_DEBUG_WARNING,
			  "naming_fsmo_init: no cross-ref container present: (skip loading of naming contexts details)\n");
		talloc_free(mem_ctx);
		return ldb_next_init(module);
	} else if (naming_res->count > 1) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			      "naming_fsmo_init: [%u] cross-ref containers found on a base search",
			      naming_res->count);
		talloc_free(mem_ctx);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	naming_fsmo->master_dn = ldb_msg_find_attr_as_dn(ldb, naming_fsmo, naming_res->msgs[0], "fSMORoleOwner");
	if (ldb_dn_compare(samdb_ntds_settings_dn(ldb), naming_fsmo->master_dn) == 0) {
		naming_fsmo->we_are_master = true;
	} else {
		naming_fsmo->we_are_master = false;
	}

	if (ldb_set_opaque(ldb, "dsdb_naming_fsmo", naming_fsmo) != LDB_SUCCESS) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	talloc_steal(module, naming_fsmo);

	ldb_debug(ldb, LDB_DEBUG_TRACE,
			  "naming_fsmo_init: we are master: %s\n",
			  (naming_fsmo->we_are_master?"yes":"no"));

	talloc_free(mem_ctx);
	return ldb_next_init(module);
}

_PUBLIC_ const struct ldb_module_ops ldb_naming_fsmo_module_ops = {
	.name		= "naming_fsmo",
	.init_context	= naming_fsmo_init
};
