/*
   Unix SMB/CIFS implementation.

   dsdb module schema utility functions

   Copyright (C) Kamen Mazdrashki <kamenim@samba.org> 2010
   Copyright (C) Andrew Tridgell 2010
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2010

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
#include "dsdb/common/util.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include <ldb_module.h>
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "param/param.h"

/**
 * Reads schema_info structure from schemaInfo
 * attribute on SCHEMA partition
 *
 * @param dsdb_flags 	DSDB_FLAG_... flag of 0
 */
int dsdb_module_schema_info_blob_read(struct ldb_module *ldb_module,
				      uint32_t dsdb_flags,
				      TALLOC_CTX *mem_ctx,
				      struct ldb_val *schema_info_blob,
				      struct ldb_request *parent)
{
	int ldb_err;
	const struct ldb_val *blob_val;
	struct ldb_dn *schema_dn;
	struct ldb_result *schema_res = NULL;
	static const char *schema_attrs[] = {
		"schemaInfo",
		NULL
	};

	schema_dn = ldb_get_schema_basedn(ldb_module_get_ctx(ldb_module));
	if (!schema_dn) {
		DEBUG(0,("dsdb_module_schema_info_blob_read: no schema dn present!\n"));
		return ldb_operr(ldb_module_get_ctx(ldb_module));
	}

	ldb_err = dsdb_module_search(ldb_module, mem_ctx, &schema_res, schema_dn,
	                             LDB_SCOPE_BASE, schema_attrs, dsdb_flags, parent,
				     NULL);
	if (ldb_err == LDB_ERR_NO_SUCH_OBJECT) {
		DEBUG(0,("dsdb_module_schema_info_blob_read: Schema DN not found!\n"));
		talloc_free(schema_res);
		return ldb_err;
	} else if (ldb_err != LDB_SUCCESS) {
		DEBUG(0,("dsdb_module_schema_info_blob_read: failed to find schemaInfo attribute\n"));
		talloc_free(schema_res);
		return ldb_err;
	}

	blob_val = ldb_msg_find_ldb_val(schema_res->msgs[0], "schemaInfo");
	if (!blob_val) {
		ldb_asprintf_errstring(ldb_module_get_ctx(ldb_module),
				       "dsdb_module_schema_info_blob_read: no schemaInfo attribute found");
		talloc_free(schema_res);
		return LDB_ERR_NO_SUCH_ATTRIBUTE;
	}

	/* transfer .data ownership to mem_ctx */
	schema_info_blob->length = blob_val->length;
	schema_info_blob->data = talloc_steal(mem_ctx, blob_val->data);

	talloc_free(schema_res);

	return LDB_SUCCESS;
}

/**
 * Prepares ldb_msg to be used for updating schemaInfo value in DB
 */
static int dsdb_schema_info_write_prepare(struct ldb_context *ldb,
					  struct ldb_val *schema_info_blob,
					  TALLOC_CTX *mem_ctx,
					  struct ldb_message **_msg)
{
	int ldb_err;
	struct ldb_message *msg;
	struct ldb_dn *schema_dn;
	struct ldb_message_element *return_el;

	schema_dn = ldb_get_schema_basedn(ldb);
	if (!schema_dn) {
		DEBUG(0,("dsdb_schema_info_write_prepare: no schema dn present\n"));
		return ldb_operr(ldb);
	}

	/* prepare ldb_msg to update schemaInfo */
	msg = ldb_msg_new(mem_ctx);
	if (msg == NULL) {
		return ldb_oom(ldb);
	}

	msg->dn = schema_dn;
	ldb_err = ldb_msg_add_value(msg, "schemaInfo", schema_info_blob, &return_el);
	if (ldb_err != 0) {
		ldb_asprintf_errstring(ldb, "dsdb_schema_info_write_prepare: ldb_msg_add_value failed - %s\n",
				       ldb_strerror(ldb_err));
		talloc_free(msg);
		return ldb_err;
	}

	/* mark schemaInfo element for replacement */
	return_el->flags = LDB_FLAG_MOD_REPLACE;

	*_msg = msg;

	return LDB_SUCCESS;
}



/**
 * Writes schema_info structure into schemaInfo
 * attribute on SCHEMA partition
 *
 * @param dsdb_flags 	DSDB_FLAG_... flag of 0
 */
int dsdb_module_schema_info_blob_write(struct ldb_module *ldb_module,
				       uint32_t dsdb_flags,
				       struct ldb_val *schema_info_blob,
				       struct ldb_request *parent)
{
	int ldb_err;
	struct ldb_message *msg;
	TALLOC_CTX *temp_ctx;

	temp_ctx = talloc_new(ldb_module);
	if (temp_ctx == NULL) {
		return ldb_module_oom(ldb_module);
	}

	/* write serialized schemaInfo into LDB */
	ldb_err = dsdb_schema_info_write_prepare(ldb_module_get_ctx(ldb_module),
						 schema_info_blob,
						 temp_ctx, &msg);
	if (ldb_err != LDB_SUCCESS) {
		talloc_free(temp_ctx);
		return ldb_err;
	}


	ldb_err = dsdb_module_modify(ldb_module, msg, dsdb_flags, parent);

	talloc_free(temp_ctx);

	if (ldb_err != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb_module_get_ctx(ldb_module),
				       "dsdb_module_schema_info_blob_write: dsdb_replace failed: %s (%s)\n",
				       ldb_strerror(ldb_err),
				       ldb_errstring(ldb_module_get_ctx(ldb_module)));
		return ldb_err;
	}

	return LDB_SUCCESS;
}


/**
 * Reads schema_info structure from schemaInfo
 * attribute on SCHEMA partition
 */
static int dsdb_module_schema_info_read(struct ldb_module *ldb_module,
					uint32_t dsdb_flags,
					TALLOC_CTX *mem_ctx,
					struct dsdb_schema_info **_schema_info,
					struct ldb_request *parent)
{
	int ret;
	DATA_BLOB ndr_blob;
	TALLOC_CTX *temp_ctx;
	WERROR werr;

	temp_ctx = talloc_new(mem_ctx);
	if (temp_ctx == NULL) {
		return ldb_module_oom(ldb_module);
	}

	/* read serialized schemaInfo from LDB  */
	ret = dsdb_module_schema_info_blob_read(ldb_module, dsdb_flags, temp_ctx, &ndr_blob, parent);
	if (ret != LDB_SUCCESS) {
		talloc_free(temp_ctx);
		return ret;
	}

	/* convert NDR blob to dsdb_schema_info object */
	werr = dsdb_schema_info_from_blob(&ndr_blob,
					  mem_ctx,
					  _schema_info);
	talloc_free(temp_ctx);

	if (W_ERROR_EQUAL(werr, WERR_DS_NO_ATTRIBUTE_OR_VALUE)) {
		return LDB_ERR_NO_SUCH_ATTRIBUTE;
	}

	if (!W_ERROR_IS_OK(werr)) {
		ldb_asprintf_errstring(ldb_module_get_ctx(ldb_module), __location__ ": failed to get schema_info");
		return ldb_operr(ldb_module_get_ctx(ldb_module));
	}

	return LDB_SUCCESS;
}

/**
 * Writes schema_info structure into schemaInfo
 * attribute on SCHEMA partition
 *
 * @param dsdb_flags 	DSDB_FLAG_... flag of 0
 */
static int dsdb_module_schema_info_write(struct ldb_module *ldb_module,
					 uint32_t dsdb_flags,
					 const struct dsdb_schema_info *schema_info,
					 struct ldb_request *parent)
{
	WERROR werr;
	int ret;
	DATA_BLOB ndr_blob;
	TALLOC_CTX *temp_ctx;

	temp_ctx = talloc_new(ldb_module);
	if (temp_ctx == NULL) {
		return ldb_module_oom(temp_ctx);
	}

	/* convert schema_info to a blob */
	werr = dsdb_blob_from_schema_info(schema_info, temp_ctx, &ndr_blob);
	if (!W_ERROR_IS_OK(werr)) {
		talloc_free(temp_ctx);
		ldb_asprintf_errstring(ldb_module_get_ctx(ldb_module), __location__ ": failed to get schema_info");
		return ldb_operr(ldb_module_get_ctx(ldb_module));
	}

	/* write serialized schemaInfo into LDB */
	ret = dsdb_module_schema_info_blob_write(ldb_module, dsdb_flags, &ndr_blob, parent);

	talloc_free(temp_ctx);

	return ret;
}


/**
 * Increments schemaInfo revision and save it to DB
 * setting our invocationID in the process
 * NOTE: this function should be called in a transaction
 * much in the same way prefixMap update function is called
 *
 * @param ldb_module 	current module
 * @param schema 	schema cache
 * @param dsdb_flags 	DSDB_FLAG_... flag of 0
 */
int dsdb_module_schema_info_update(struct ldb_module *ldb_module,
				   struct dsdb_schema *schema,
				   int dsdb_flags, struct ldb_request *parent)
{
	int ret;
	const struct GUID *invocation_id;
	DATA_BLOB ndr_blob;
	struct dsdb_schema_info *schema_info;
	const char *schema_info_str;
	WERROR werr;
	TALLOC_CTX *temp_ctx = talloc_new(schema);
	if (temp_ctx == NULL) {
		return ldb_module_oom(ldb_module);
	}

	invocation_id = samdb_ntds_invocation_id(ldb_module_get_ctx(ldb_module));
	if (!invocation_id) {
		talloc_free(temp_ctx);
		return ldb_operr(ldb_module_get_ctx(ldb_module));
	}

	/* read serialized schemaInfo from LDB  */
	ret = dsdb_module_schema_info_read(ldb_module, dsdb_flags, temp_ctx, &schema_info, parent);
	if (ret == LDB_ERR_NO_SUCH_ATTRIBUTE) {
		/* make default value in case
		 * we have no schemaInfo value yet */
		werr = dsdb_schema_info_new(temp_ctx, &schema_info);
		if (!W_ERROR_IS_OK(werr)) {
			talloc_free(temp_ctx);
			return ldb_module_oom(ldb_module);
		}
		ret = LDB_SUCCESS;
	}
	if (ret != LDB_SUCCESS) {
		talloc_free(temp_ctx);
		return ret;
	}

	/* update schemaInfo */
	schema_info->revision++;
	schema_info->invocation_id = *invocation_id;

	ret = dsdb_module_schema_info_write(ldb_module, dsdb_flags, schema_info, parent);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb_module_get_ctx(ldb_module),
				       "dsdb_module_schema_info_update: failed to save schemaInfo - %s\n",
				       ldb_strerror(ret));
		talloc_free(temp_ctx);
		return ret;
	}

	/* finally, update schema_info in the cache */
	werr = dsdb_blob_from_schema_info(schema_info, temp_ctx, &ndr_blob);
	if (!W_ERROR_IS_OK(werr)) {
		ldb_asprintf_errstring(ldb_module_get_ctx(ldb_module), "Failed to get schema info");
		talloc_free(temp_ctx);
		return ldb_operr(ldb_module_get_ctx(ldb_module));
	}

	schema_info_str = hex_encode_talloc(schema, ndr_blob.data, ndr_blob.length);
	if (!schema_info_str) {
		talloc_free(temp_ctx);
		return ldb_module_oom(ldb_module);
	}

	talloc_unlink(schema, discard_const(schema->schema_info));
	schema->schema_info = schema_info_str;

	talloc_free(temp_ctx);
	return LDB_SUCCESS;
}
