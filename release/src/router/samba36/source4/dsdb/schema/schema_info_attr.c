/*
   Unix SMB/CIFS implementation.

   SCHEMA::schemaInfo implementation

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
#include "dsdb/common/util.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/samdb/ldb_modules/util.h"
#include <ldb_module.h>
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "param/param.h"


/**
 * Creates and initializes new dsdb_schema_info value.
 * Initial schemaInfo values is with:
 *   revision = 0
 *   invocationId = GUID_ZERO
 */
WERROR dsdb_schema_info_new(TALLOC_CTX *mem_ctx, struct dsdb_schema_info **_schema_info)
{
	struct dsdb_schema_info *schema_info;

	schema_info = talloc_zero(mem_ctx, struct dsdb_schema_info);
	W_ERROR_HAVE_NO_MEMORY(schema_info);

	*_schema_info = schema_info;

	return WERR_OK;
}

/**
 * Creates and initializes new dsdb_schema_info blob value.
 * Initial schemaInfo values is with:
 *   revision = 0
 *   invocationId = GUID_ZERO
 */
WERROR dsdb_schema_info_blob_new(TALLOC_CTX *mem_ctx, DATA_BLOB *_schema_info_blob)
{
	DATA_BLOB blob;

	blob = data_blob_talloc_zero(mem_ctx, 21);
	W_ERROR_HAVE_NO_MEMORY(blob.data);

	/* Set the schemaInfo marker to 0xFF */
	blob.data[0] = 0xFF;

	*_schema_info_blob = blob;

	return WERR_OK;
}


/**
 * Verify the 'blob' is a valid schemaInfo blob
 */
bool dsdb_schema_info_blob_is_valid(const DATA_BLOB *blob)
{
	if (!blob || !blob->data) {
		return false;
	}

	/* schemaInfo blob must be 21 bytes long */
	if (blob->length != 21) {
		return false;
	}

	/* schemaInfo blob should start with 0xFF */
	if (blob->data[0] != 0xFF) {
		return false;
	}

	return true;
}

/**
 * Parse schemaInfo structure from a data_blob
 * (DATA_BLOB or ldb_val).
 * Suitable for parsing blobs that comes from
 * DRS interface of from LDB database
 */
WERROR dsdb_schema_info_from_blob(const DATA_BLOB *blob,
				  TALLOC_CTX *mem_ctx, struct dsdb_schema_info **_schema_info)
{
	TALLOC_CTX *temp_ctx;
	enum ndr_err_code ndr_err;
	struct dsdb_schema_info *schema_info;
	struct schemaInfoBlob schema_info_blob;

	/* verify schemaInfo blob is valid */
	if (!dsdb_schema_info_blob_is_valid(blob)) {
		return WERR_INVALID_PARAMETER;
	}

	temp_ctx = talloc_new(mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(temp_ctx);

	ndr_err = ndr_pull_struct_blob_all(blob, temp_ctx,
	                                   &schema_info_blob,
	                                   (ndr_pull_flags_fn_t)ndr_pull_schemaInfoBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		talloc_free(temp_ctx);
		return ntstatus_to_werror(nt_status);
	}

	schema_info = talloc(mem_ctx, struct dsdb_schema_info);
	if (!schema_info) {
		talloc_free(temp_ctx);
		return WERR_NOMEM;
	}

	/* note that we accept revision numbers of zero now - w2k8r2
	   sends a revision of zero on initial vampire */
	schema_info->revision      = schema_info_blob.revision;
	schema_info->invocation_id = schema_info_blob.invocation_id;
	*_schema_info = schema_info;

	talloc_free(temp_ctx);
	return WERR_OK;
}

/**
 * Creates a blob from schemaInfo structure
 * Suitable for packing schemaInfo into a blob
 * which is to be used in DRS interface of LDB database
 */
WERROR dsdb_blob_from_schema_info(const struct dsdb_schema_info *schema_info,
				  TALLOC_CTX *mem_ctx, DATA_BLOB *blob)
{
	enum ndr_err_code ndr_err;
	struct schemaInfoBlob schema_info_blob;

	schema_info_blob.marker		= 0xFF;
	schema_info_blob.revision	= schema_info->revision;
	schema_info_blob.invocation_id  = schema_info->invocation_id;

	ndr_err = ndr_push_struct_blob(blob, mem_ctx,
	                               &schema_info_blob,
	                               (ndr_push_flags_fn_t)ndr_push_schemaInfoBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		return ntstatus_to_werror(nt_status);
	}

	return WERR_OK;
}

/**
 * Compares schemaInfo signatures in dsdb_schema and prefixMap.
 * NOTE: At present function compares schemaInfo values
 * as string without taking into account schemVersion field
 *
 * @return WERR_OK if schemaInfos are equal
 * 	   WERR_DS_DRA_SCHEMA_MISMATCH if schemaInfos are different
 */
WERROR dsdb_schema_info_cmp(const struct dsdb_schema *schema,
			    const struct drsuapi_DsReplicaOIDMapping_Ctr *ctr)
{
	bool bres;
	DATA_BLOB blob;
	char *schema_info_str;
	struct drsuapi_DsReplicaOIDMapping *mapping;

	/* we should have at least schemaInfo element */
	if (ctr->num_mappings < 1) {
		return WERR_INVALID_PARAMETER;
	}

	/* verify schemaInfo element is valid */
	mapping = &ctr->mappings[ctr->num_mappings - 1];
	if (mapping->id_prefix != 0) {
		return WERR_INVALID_PARAMETER;
	}

	blob = data_blob_const(mapping->oid.binary_oid, mapping->oid.length);
	if (!dsdb_schema_info_blob_is_valid(&blob)) {
		return WERR_INVALID_PARAMETER;
	}

	schema_info_str = hex_encode_talloc(NULL, blob.data, blob.length);
	W_ERROR_HAVE_NO_MEMORY(schema_info_str);

	bres = strequal(schema->schema_info, schema_info_str);
	talloc_free(schema_info_str);

	return bres ? WERR_OK : WERR_DS_DRA_SCHEMA_MISMATCH;
}


