/*
   Samba4 module loading module

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
 *  Component: Samba4 module loading module
 *
 *  Description: Implement a single 'module' in the ldb database,
 *  which loads the remaining modules based on 'choice of configuration' attributes
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
#include "librpc/ndr/libndr.h"

static int read_at_rootdse_record(struct ldb_context *ldb, struct ldb_module *module, TALLOC_CTX *mem_ctx,
				  struct ldb_message **msg, struct ldb_request *parent)
{
	int ret;
	static const char *rootdse_attrs[] = { "defaultNamingContext", "configurationNamingContext", "schemaNamingContext", NULL };
	struct ldb_result *rootdse_res;
	struct ldb_dn *rootdse_dn;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return ldb_oom(ldb);
	}

	rootdse_dn = ldb_dn_new(tmp_ctx, ldb, "@ROOTDSE");
	if (!rootdse_dn) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}

	ret = dsdb_module_search_dn(module, tmp_ctx, &rootdse_res, rootdse_dn,
	                            rootdse_attrs, DSDB_FLAG_NEXT_MODULE, parent);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	talloc_steal(mem_ctx, rootdse_res->msgs);
	*msg = rootdse_res->msgs[0];

	talloc_free(tmp_ctx);

	return ret;
}

static int prepare_modules_line(struct ldb_context *ldb,
				TALLOC_CTX *mem_ctx,
				const struct ldb_message *rootdse_msg,
				struct ldb_message *msg, const char *backend_attr,
				const char *backend_mod, const char **backend_mod_list)
{
	int ret;
	const char **backend_full_list;
	const char *backend_dn;
	char *mod_list_string;
	char *full_string;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return ldb_oom(ldb);
	}

	if (backend_attr) {
		backend_dn = ldb_msg_find_attr_as_string(rootdse_msg, backend_attr, NULL);
		if (!backend_dn) {
			ldb_asprintf_errstring(ldb,
					       "samba_dsdb_init: "
					       "unable to read %s from %s:%s",
					       backend_attr, ldb_dn_get_linearized(rootdse_msg->dn),
					       ldb_errstring(ldb));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
	} else {
		backend_dn = "*";
	}

	if (backend_mod) {
		backend_full_list = (const char **)str_list_make_single(tmp_ctx, backend_mod);
	} else {
		backend_full_list = (const char **)str_list_make_empty(tmp_ctx);
	}
	if (!backend_full_list) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}

	backend_full_list = str_list_append_const(backend_full_list, backend_mod_list);
	if (!backend_full_list) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}

	mod_list_string = str_list_join(tmp_ctx, backend_full_list, ',');
	if (!mod_list_string) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}

	full_string = talloc_asprintf(tmp_ctx, "%s:%s", backend_dn, mod_list_string);
	ret = ldb_msg_add_steal_string(msg, "modules", full_string);
	talloc_free(tmp_ctx);
	return ret;
}



static int samba_dsdb_init(struct ldb_module *module)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret, len, i;
	TALLOC_CTX *tmp_ctx = talloc_new(module);
	struct ldb_result *res;
	struct ldb_message *rootdse_msg, *partition_msg;
	struct ldb_dn *samba_dsdb_dn;
	struct ldb_module *backend_module, *module_chain;
	const char **final_module_list, **reverse_module_list;
	/*
	  Add modules to the list to activate them by default
	  beware often order is important

	  Some Known ordering constraints:
	  - rootdse must be first, as it makes redirects from "" -> cn=rootdse
	  - extended_dn_in must be before objectclass.c, as it resolves the DN
	  - objectclass must be before password_hash and samldb since these LDB
	    modules require the expanded "objectClass" list
	  - objectclass_attrs must be behind operational in order to see all
	    attributes (the operational module protects and therefore
	    suppresses per default some important ones)
	  - partition must be last
	  - each partition has its own module list then

	  The list is presented here as a set of declarations to show the
	  stack visually - the code below then handles the creation of the list
	  based on the parameters loaded from the database.
	*/
	static const char *modules_list[] = {"resolve_oids",
					     "rootdse",
					     "lazy_commit",
					     "paged_results",
					     "ranged_results",
					     "anr",
					     "server_sort",
					     "asq",
					     "extended_dn_store",
					     "extended_dn_in",
					     "objectclass",
					     "descriptor",
					     "acl",
					     "aclread",
					     "samldb",
					     "password_hash",
					     "operational",
					     "schema_load",
					     "instancetype",
					     "objectclass_attrs",
					     NULL };

	const char **link_modules;
	static const char *fedora_ds_modules[] = {
		"rdn_name", NULL };
	static const char *openldap_modules[] = {
		NULL };
	static const char *tdb_modules_list[] = {
		"rdn_name",
		"subtree_delete",
		"repl_meta_data",
		"subtree_rename",
		"linked_attributes",
		NULL};

	const char *extended_dn_module;
	const char *extended_dn_module_ldb = "extended_dn_out_ldb";
	const char *extended_dn_module_fds = "extended_dn_out_fds";
	const char *extended_dn_module_openldap = "extended_dn_out_openldap";

	static const char *modules_list2[] = {"show_deleted",
					      "new_partition",
					      "partition",
					      NULL };

	const char **backend_modules;
	static const char *fedora_ds_backend_modules[] = {
		"nsuniqueid", "paged_searches", "simple_dn", NULL };
	static const char *openldap_backend_modules[] = {
		"entryuuid", "paged_searches", "simple_dn", NULL };

	static const char *samba_dsdb_attrs[] = { "backendType", "serverRole", NULL };
	const char *backendType, *serverRole;

	if (!tmp_ctx) {
		return ldb_oom(ldb);
	}

	ret = ldb_register_samba_handlers(ldb);
	if (ret != LDB_SUCCESS) {
		talloc_free(tmp_ctx);
		return ret;
	}

	samba_dsdb_dn = ldb_dn_new(tmp_ctx, ldb, "@SAMBA_DSDB");
	if (!samba_dsdb_dn) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}

#define CHECK_LDB_RET(check_ret)				\
	do {							\
		if (check_ret != LDB_SUCCESS) {			\
			talloc_free(tmp_ctx);			\
			return check_ret;			\
		}						\
	} while (0)

	ret = dsdb_module_search_dn(module, tmp_ctx, &res, samba_dsdb_dn,
	                            samba_dsdb_attrs, DSDB_FLAG_NEXT_MODULE, NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		backendType = "ldb";
		serverRole = "domain controller";
	} else if (ret == LDB_SUCCESS) {
		backendType = ldb_msg_find_attr_as_string(res->msgs[0], "backendType", "ldb");
		serverRole = ldb_msg_find_attr_as_string(res->msgs[0], "serverRole", "domain controller");
	} else {
		talloc_free(tmp_ctx);
		return ret;
	}

	backend_modules = NULL;
	if (strcasecmp(backendType, "ldb") == 0) {
		extended_dn_module = extended_dn_module_ldb;
		link_modules = tdb_modules_list;
	} else {
		if (strcasecmp(backendType, "fedora-ds") == 0) {
			link_modules = fedora_ds_modules;
			backend_modules = fedora_ds_backend_modules;
			extended_dn_module = extended_dn_module_fds;
		} else if (strcasecmp(backendType, "openldap") == 0) {
			link_modules = openldap_modules;
			backend_modules = openldap_backend_modules;
			extended_dn_module = extended_dn_module_openldap;
		} else {
			return ldb_error(ldb, LDB_ERR_OPERATIONS_ERROR, "invalid backend type");
		}
		ret = ldb_set_opaque(ldb, "readOnlySchema", (void*)1);
		if (ret != LDB_SUCCESS) {
			ldb_set_errstring(ldb, "Failed to set readOnlySchema opaque");
		}
	}

#define CHECK_MODULE_LIST \
	do {							\
		if (!final_module_list) {			\
			talloc_free(tmp_ctx);			\
			return ldb_oom(ldb);			\
		}						\
	} while (0)

	final_module_list = str_list_copy_const(tmp_ctx, modules_list);
	CHECK_MODULE_LIST;

	final_module_list = str_list_append_const(final_module_list, link_modules);
	CHECK_MODULE_LIST;

	final_module_list = str_list_add_const(final_module_list, extended_dn_module);
	CHECK_MODULE_LIST;

	final_module_list = str_list_append_const(final_module_list, modules_list2);
	CHECK_MODULE_LIST;


	ret = read_at_rootdse_record(ldb, module, tmp_ctx, &rootdse_msg, NULL);
	CHECK_LDB_RET(ret);

	partition_msg = ldb_msg_new(tmp_ctx);
	partition_msg->dn = ldb_dn_new(partition_msg, ldb, "@" DSDB_OPAQUE_PARTITION_MODULE_MSG_OPAQUE_NAME);

	ret = prepare_modules_line(ldb, tmp_ctx,
				   rootdse_msg,
				   partition_msg, "defaultNamingContext",
				   "pdc_fsmo", backend_modules);
	CHECK_LDB_RET(ret);

	ret = prepare_modules_line(ldb, tmp_ctx,
				   rootdse_msg,
				   partition_msg, "configurationNamingContext",
				   "naming_fsmo", backend_modules);
	CHECK_LDB_RET(ret);

	ret = prepare_modules_line(ldb, tmp_ctx,
				   rootdse_msg,
				   partition_msg, "schemaNamingContext",
				   "schema_data", backend_modules);
	CHECK_LDB_RET(ret);

	ret = prepare_modules_line(ldb, tmp_ctx,
				   rootdse_msg,
				   partition_msg, NULL,
				   NULL, backend_modules);
	CHECK_LDB_RET(ret);

	ret = ldb_set_opaque(ldb, DSDB_OPAQUE_PARTITION_MODULE_MSG_OPAQUE_NAME, partition_msg);
	CHECK_LDB_RET(ret);

	talloc_steal(ldb, partition_msg);

	/* Now prepare the module chain.  Oddly, we must give it to ldb_load_modules_list in REVERSE */
	for (len = 0; final_module_list[len]; len++) { /* noop */};

	reverse_module_list = talloc_array(tmp_ctx, const char *, len+1);
	if (!reverse_module_list) {
		talloc_free(tmp_ctx);
		return ldb_oom(ldb);
	}
	for (i=0; i < len; i++) {
		reverse_module_list[i] = final_module_list[(len - 1) - i];
	}
	reverse_module_list[i] = NULL;

	/* The backend (at least until the partitions module
	 * reconfigures things) is the next module in the currently
	 * loaded chain */
	backend_module = ldb_module_next(module);
	ret = ldb_module_load_list(ldb, reverse_module_list, backend_module, &module_chain);
	CHECK_LDB_RET(ret);

	talloc_free(tmp_ctx);
	/* Set this as the 'next' module, so that we effectivly append it to module chain */
	ldb_module_set_next(module, module_chain);

	return ldb_next_init(module);
}

static const struct ldb_module_ops ldb_samba_dsdb_module_ops = {
	.name		   = "samba_dsdb",
	.init_context	   = samba_dsdb_init,
};

int ldb_samba_dsdb_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_samba_dsdb_module_ops);
}
