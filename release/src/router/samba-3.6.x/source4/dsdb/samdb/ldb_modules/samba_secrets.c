/* 
   Samba4 module loading module (for secrets)

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009

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

/*
 *  Name: ldb
 *
 *  Component: Samba4 module loading module (for secrets.ldb)
 *
 *  Description: Implement a single 'module' in the secrets.ldb database
 *
 *  This is to avoid forcing a reprovision of the ldb databases when we change the internal structure of the code
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include <ldb.h>
#include <ldb_errors.h>
#include <ldb_module.h>
#include "dsdb/samdb/ldb_modules/util.h"
#include "dsdb/samdb/samdb.h"


static int samba_secrets_init(struct ldb_module *module)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret, len, i;
	TALLOC_CTX *tmp_ctx = talloc_new(module);
	struct ldb_module *backend_module, *module_chain;
	const char **reverse_module_list;
	/*
	  Add modules to the list to activate them by default
	  beware often order is important
	  
	  The list is presented here as a set of declarations to show the
	  stack visually
	*/
	static const char *modules_list[] = {"update_keytab",
					     "objectguid",
					     "rdn_name",
					     NULL };

	if (!tmp_ctx) {
		return ldb_oom(ldb);
	}

	/* Now prepare the module chain.  Oddly, we must give it to ldb_load_modules_list in REVERSE */
	for (len = 0; modules_list[len]; len++) { /* noop */};

	reverse_module_list = talloc_array(tmp_ctx, const char *, len+1);
	if (!reverse_module_list) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}
	for (i=0; i < len; i++) {
		reverse_module_list[i] = modules_list[(len - 1) - i];
	}
	reverse_module_list[i] = NULL;

	/* The backend (at least until the partitions module
	 * reconfigures things) is the next module in the currently
	 * loaded chain */
	backend_module = ldb_module_next(module);
	ret = ldb_module_load_list(ldb, reverse_module_list, backend_module, &module_chain);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	talloc_free(tmp_ctx);
	/* Set this as the 'next' module, so that we effectivly append it to module chain */
	ldb_module_set_next(module, module_chain);

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_samba_secrets_module_ops = {
	.name		   = "samba_secrets",
	.init_context	   = samba_secrets_init,
};

int ldb_samba_secrets_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_samba_secrets_module_ops);
}
