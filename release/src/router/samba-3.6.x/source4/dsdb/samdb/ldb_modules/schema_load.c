/* 
   Unix SMB/CIFS mplementation.

   The module that handles the Schema FSMO Role Owner
   checkings, it also loads the dsdb_schema.
   
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2007
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2009-2010

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
#include "param/param.h"
#include "dsdb/samdb/ldb_modules/util.h"

struct schema_load_private_data {
	bool in_transaction;
};

static int dsdb_schema_from_db(struct ldb_module *module, struct ldb_dn *schema_dn, uint64_t current_usn,
			       struct dsdb_schema **schema);

struct dsdb_schema *dsdb_schema_refresh(struct ldb_module *module, struct dsdb_schema *schema, bool is_global_schema)
{
	uint64_t current_usn;
	int ret;
	struct ldb_result *res;
	struct ldb_request *treq;
	struct ldb_seqnum_request *tseq;
	struct ldb_seqnum_result *tseqr;
	struct dsdb_control_current_partition *ctrl;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct dsdb_schema *new_schema;
	
	struct schema_load_private_data *private_data = talloc_get_type(ldb_module_get_private(module), struct schema_load_private_data);
	if (!private_data) {
		/* We can't refresh until the init function has run */
		return schema;
	}

	/* We don't allow a schema reload during a transaction - nobody else can modify our schema behind our backs */
	if (private_data->in_transaction) {
		return schema;
	}

	res = talloc_zero(schema, struct ldb_result);
	if (res == NULL) {
		return NULL;
	}
	tseq = talloc_zero(res, struct ldb_seqnum_request);
	if (tseq == NULL) {
		talloc_free(res);
		return NULL;
	}
	tseq->type = LDB_SEQ_HIGHEST_SEQ;
	
	ret = ldb_build_extended_req(&treq, ldb_module_get_ctx(module), res,
				     LDB_EXTENDED_SEQUENCE_NUMBER,
				     tseq,
				     NULL,
				     res,
				     ldb_extended_default_callback,
				     NULL);
	LDB_REQ_SET_LOCATION(treq);
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		return NULL;
	}
	
	ctrl = talloc(treq, struct dsdb_control_current_partition);
	if (!ctrl) {
		talloc_free(res);
		return NULL;
	}
	ctrl->version = DSDB_CONTROL_CURRENT_PARTITION_VERSION;
	ctrl->dn = schema->base_dn;
	
	ret = ldb_request_add_control(treq,
				      DSDB_CONTROL_CURRENT_PARTITION_OID,
				      false, ctrl);
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		return NULL;
	}
	
	ret = ldb_next_request(module, treq);
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		return NULL;
	}
	ret = ldb_wait(treq->handle, LDB_WAIT_ALL);
	if (ret != LDB_SUCCESS) {
		talloc_free(res);
		return NULL;
	}
	tseqr = talloc_get_type(res->extended->data,
				struct ldb_seqnum_result);
	if (tseqr->seq_num == schema->reload_seq_number) {
		talloc_free(res);
		return schema;
	}

	schema->reload_seq_number = tseqr->seq_num;
	talloc_free(res);
		
	ret = dsdb_module_load_partition_usn(module, schema->base_dn, &current_usn, NULL, NULL);
	if (ret != LDB_SUCCESS || current_usn == schema->loaded_usn) {
		return schema;
	}

	ret = dsdb_schema_from_db(module, schema->base_dn, current_usn, &new_schema);
	if (ret != LDB_SUCCESS) {
		return schema;
	}
	
	if (is_global_schema) {
		dsdb_make_schema_global(ldb, new_schema);
	}
	return new_schema;
}


/*
  Given an LDB module (pointing at the schema DB), and the DN, set the populated schema
*/

static int dsdb_schema_from_db(struct ldb_module *module, struct ldb_dn *schema_dn, uint64_t current_usn,
			       struct dsdb_schema **schema)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	TALLOC_CTX *tmp_ctx;
	char *error_string;
	int ret;
	struct ldb_result *schema_res;
	struct ldb_result *a_res;
	struct ldb_result *c_res;
	static const char *schema_attrs[] = {
		"prefixMap",
		"schemaInfo",
		"fSMORoleOwner",
		NULL
	};
	unsigned flags;

	tmp_ctx = talloc_new(module);
	if (!tmp_ctx) {
		return ldb_oom(ldb);
	}

	/* we don't want to trace the schema load */
	flags = ldb_get_flags(ldb);
	ldb_set_flags(ldb, flags & ~LDB_FLG_ENABLE_TRACING);

	/*
	 * setup the prefix mappings and schema info
	 */
	ret = dsdb_module_search_dn(module, tmp_ctx, &schema_res,
				    schema_dn, schema_attrs,
				    DSDB_FLAG_NEXT_MODULE, NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		ldb_reset_err_string(ldb);
		ldb_debug(ldb, LDB_DEBUG_WARNING,
			  "schema_load_init: no schema head present: (skip schema loading)\n");
		goto failed;
	} else if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb, 
				       "dsdb_schema: failed to search the schema head: %s",
				       ldb_errstring(ldb));
		goto failed;
	}

	/*
	 * load the attribute definitions
	 */
	ret = dsdb_module_search(module, tmp_ctx, &a_res,
				 schema_dn, LDB_SCOPE_ONELEVEL, NULL,
				 DSDB_FLAG_NEXT_MODULE,
				 NULL,
				 "(objectClass=attributeSchema)");
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb, 
				       "dsdb_schema: failed to search attributeSchema objects: %s",
				       ldb_errstring(ldb));
		goto failed;
	}

	/*
	 * load the objectClass definitions
	 */
	ret = dsdb_module_search(module, tmp_ctx, &c_res,
				 schema_dn, LDB_SCOPE_ONELEVEL, NULL,
				 DSDB_FLAG_NEXT_MODULE |
				 DSDB_SEARCH_SHOW_DN_IN_STORAGE_FORMAT,
				 NULL,
				 "(objectClass=classSchema)");
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb, 
				       "dsdb_schema: failed to search classSchema objects: %s",
				       ldb_errstring(ldb));
		goto failed;
	}

	ret = dsdb_schema_from_ldb_results(tmp_ctx, ldb,
					   schema_res, a_res, c_res, schema, &error_string);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb, 
				       "dsdb_schema load failed: %s",
				       error_string);
		goto failed;
	}

	(*schema)->refresh_in_progress = true;

	/* If we have the readOnlySchema opaque, then don't check for
	 * runtime schema updates, as they are not permitted (we would
	 * have to update the backend server schema too */
	if (!ldb_get_opaque(ldb, "readOnlySchema")) {
		(*schema)->refresh_fn = dsdb_schema_refresh;
		(*schema)->loaded_from_module = module;
		(*schema)->loaded_usn = current_usn;
	}

	/* "dsdb_set_schema()" steals schema into the ldb_context */
	ret = dsdb_set_schema(ldb, (*schema));

	(*schema)->refresh_in_progress = false;

	if (ret != LDB_SUCCESS) {
		ldb_debug_set(ldb, LDB_DEBUG_FATAL,
			      "schema_load_init: dsdb_set_schema() failed: %d:%s: %s",
			      ret, ldb_strerror(ret), ldb_errstring(ldb));
		goto failed;
	}

	/* Ensure this module won't go away before the callback */
	if (talloc_reference(*schema, ldb) == NULL) {
		ldb_oom(ldb);
		ret = LDB_ERR_OPERATIONS_ERROR;
	}

failed:
	if (flags & LDB_FLG_ENABLE_TRACING) {
		flags = ldb_get_flags(ldb);
		ldb_set_flags(ldb, flags | LDB_FLG_ENABLE_TRACING);
	}
	talloc_free(tmp_ctx);
	return ret;
}	


static int schema_load_init(struct ldb_module *module)
{
	struct schema_load_private_data *private_data;
	struct dsdb_schema *schema;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret;
	uint64_t current_usn;
	struct ldb_dn *schema_dn;

	private_data = talloc_zero(module, struct schema_load_private_data);
	if (private_data == NULL) {
		return ldb_oom(ldb);
	}

	ldb_module_set_private(module, private_data);

	ret = ldb_next_init(module);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (dsdb_get_schema(ldb, NULL)) {
		return LDB_SUCCESS;
	}

	schema_dn = ldb_get_schema_basedn(ldb);
	if (!schema_dn) {
		ldb_reset_err_string(ldb);
		ldb_debug(ldb, LDB_DEBUG_WARNING,
			  "schema_load_init: no schema dn present: (skip schema loading)\n");
		return LDB_SUCCESS;
	}

	ret = dsdb_module_load_partition_usn(module, schema_dn, &current_usn, NULL, NULL);
	if (ret != LDB_SUCCESS) {
		/* Ignore the error and just reload the DB more often */
		current_usn = 0;
	}

	return dsdb_schema_from_db(module, schema_dn, current_usn, &schema);
}

static int schema_load_start_transaction(struct ldb_module *module)
{
	struct schema_load_private_data *private_data =
		talloc_get_type(ldb_module_get_private(module), struct schema_load_private_data);

	private_data->in_transaction = true;

	return ldb_next_start_trans(module);
}

static int schema_load_end_transaction(struct ldb_module *module)
{
	struct schema_load_private_data *private_data =
		talloc_get_type(ldb_module_get_private(module), struct schema_load_private_data);

	private_data->in_transaction = false;

	return ldb_next_end_trans(module);
}

static int schema_load_del_transaction(struct ldb_module *module)
{
	struct schema_load_private_data *private_data =
		talloc_get_type(ldb_module_get_private(module), struct schema_load_private_data);

	private_data->in_transaction = false;

	return ldb_next_del_trans(module);
}

static int schema_load_extended(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;

	ldb = ldb_module_get_ctx(module);

	if (strcmp(req->op.extended.oid, DSDB_EXTENDED_SCHEMA_UPDATE_NOW_OID) != 0) {
		return ldb_next_request(module, req);
	}

	/* This is a no-op.  We reload as soon as we can */
	return ldb_module_done(req, NULL, NULL, LDB_SUCCESS);
}


static const struct ldb_module_ops ldb_schema_load_module_ops = {
	.name		= "schema_load",
	.init_context	= schema_load_init,
	.extended	= schema_load_extended,
	.start_transaction = schema_load_start_transaction,
	.end_transaction   = schema_load_end_transaction,
	.del_transaction   = schema_load_del_transaction,
};

int ldb_schema_load_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_schema_load_module_ops);
}
