/* 
   Unix SMB/CIFS mplementation.
   DSDB schema header
   
   Copyright (C) Stefan Metzmacher <metze@samba.org> 2006-2007
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006-2008

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
#include "lib/ldb/include/ldb_errors.h"
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "param/param.h"
#include "lib/ldb/include/ldb_module.h"

struct dsdb_schema *dsdb_new_schema(TALLOC_CTX *mem_ctx, struct smb_iconv_convenience *iconv_convenience)
{
	struct dsdb_schema *schema = talloc_zero(mem_ctx, struct dsdb_schema);
	if (!schema) {
		return NULL;
	}

	schema->iconv_convenience = iconv_convenience;
	return schema;
}


WERROR dsdb_load_oid_mappings_drsuapi(struct dsdb_schema *schema, const struct drsuapi_DsReplicaOIDMapping_Ctr *ctr)
{
	uint32_t i,j;

	schema->prefixes = talloc_array(schema, struct dsdb_schema_oid_prefix, ctr->num_mappings);
	W_ERROR_HAVE_NO_MEMORY(schema->prefixes);

	for (i=0, j=0; i < ctr->num_mappings; i++) {
		if (ctr->mappings[i].oid.oid == NULL) {
			return WERR_INVALID_PARAM;
		}

		if (strncasecmp(ctr->mappings[i].oid.oid, "ff", 2) == 0) {
			if (ctr->mappings[i].id_prefix != 0) {
				return WERR_INVALID_PARAM;
			}

			/* the magic value should be in the last array member */
			if (i != (ctr->num_mappings - 1)) {
				return WERR_INVALID_PARAM;
			}

			if (ctr->mappings[i].oid.__ndr_size != 21) {
				return WERR_INVALID_PARAM;
			}

			schema->schema_info = talloc_strdup(schema, ctr->mappings[i].oid.oid);
			W_ERROR_HAVE_NO_MEMORY(schema->schema_info);
		} else {
			/* the last array member should contain the magic value not a oid */
			if (i == (ctr->num_mappings - 1)) {
				return WERR_INVALID_PARAM;
			}

			schema->prefixes[j].id	= ctr->mappings[i].id_prefix<<16;
			schema->prefixes[j].oid	= talloc_asprintf(schema->prefixes, "%s.",
								  ctr->mappings[i].oid.oid);
			W_ERROR_HAVE_NO_MEMORY(schema->prefixes[j].oid);
			schema->prefixes[j].oid_len = strlen(schema->prefixes[j].oid);
			j++;
		}
	}

	schema->num_prefixes = j;
	return WERR_OK;
}

WERROR dsdb_load_oid_mappings_ldb(struct dsdb_schema *schema,
				  const struct ldb_val *prefixMap,
				  const struct ldb_val *schemaInfo)
{
	WERROR status;
	enum ndr_err_code ndr_err;
	struct prefixMapBlob pfm;
	char *schema_info;

	TALLOC_CTX *mem_ctx = talloc_new(schema);
	W_ERROR_HAVE_NO_MEMORY(mem_ctx);
	
	ndr_err = ndr_pull_struct_blob(prefixMap, mem_ctx, schema->iconv_convenience, &pfm, (ndr_pull_flags_fn_t)ndr_pull_prefixMapBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		talloc_free(mem_ctx);
		return ntstatus_to_werror(nt_status);
	}

	if (pfm.version != PREFIX_MAP_VERSION_DSDB) {
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}

	if (schemaInfo->length != 21 && schemaInfo->data[0] == 0xFF) {
		talloc_free(mem_ctx);
		return WERR_FOOBAR;
	}

	/* append the schema info as last element */
	pfm.ctr.dsdb.num_mappings++;
	pfm.ctr.dsdb.mappings = talloc_realloc(mem_ctx, pfm.ctr.dsdb.mappings,
					       struct drsuapi_DsReplicaOIDMapping,
					       pfm.ctr.dsdb.num_mappings);
	W_ERROR_HAVE_NO_MEMORY(pfm.ctr.dsdb.mappings);

	schema_info = data_blob_hex_string(pfm.ctr.dsdb.mappings, schemaInfo);
	W_ERROR_HAVE_NO_MEMORY(schema_info);

	pfm.ctr.dsdb.mappings[pfm.ctr.dsdb.num_mappings - 1].id_prefix		= 0;	
	pfm.ctr.dsdb.mappings[pfm.ctr.dsdb.num_mappings - 1].oid.__ndr_size	= schemaInfo->length;
	pfm.ctr.dsdb.mappings[pfm.ctr.dsdb.num_mappings - 1].oid.oid		= schema_info;

	/* call the drsuapi version */
	status = dsdb_load_oid_mappings_drsuapi(schema, &pfm.ctr.dsdb);
	talloc_free(mem_ctx);

	W_ERROR_NOT_OK_RETURN(status);

	return WERR_OK;
}

WERROR dsdb_get_oid_mappings_drsuapi(const struct dsdb_schema *schema,
				     bool include_schema_info,
				     TALLOC_CTX *mem_ctx,
				     struct drsuapi_DsReplicaOIDMapping_Ctr **_ctr)
{
	struct drsuapi_DsReplicaOIDMapping_Ctr *ctr;
	uint32_t i;

	ctr = talloc(mem_ctx, struct drsuapi_DsReplicaOIDMapping_Ctr);
	W_ERROR_HAVE_NO_MEMORY(ctr);

	ctr->num_mappings	= schema->num_prefixes;
	if (include_schema_info) ctr->num_mappings++;
	ctr->mappings = talloc_array(schema, struct drsuapi_DsReplicaOIDMapping, ctr->num_mappings);
	W_ERROR_HAVE_NO_MEMORY(ctr->mappings);

	for (i=0; i < schema->num_prefixes; i++) {
		ctr->mappings[i].id_prefix	= schema->prefixes[i].id>>16;
		ctr->mappings[i].oid.oid	= talloc_strndup(ctr->mappings,
								 schema->prefixes[i].oid,
								 schema->prefixes[i].oid_len - 1);
		W_ERROR_HAVE_NO_MEMORY(ctr->mappings[i].oid.oid);
	}

	if (include_schema_info) {
		ctr->mappings[i].id_prefix	= 0;
		ctr->mappings[i].oid.oid	= talloc_strdup(ctr->mappings,
								schema->schema_info);
		W_ERROR_HAVE_NO_MEMORY(ctr->mappings[i].oid.oid);
	}

	*_ctr = ctr;
	return WERR_OK;
}

WERROR dsdb_get_oid_mappings_ldb(const struct dsdb_schema *schema,
				 TALLOC_CTX *mem_ctx,
				 struct ldb_val *prefixMap,
				 struct ldb_val *schemaInfo)
{
	WERROR status;
	enum ndr_err_code ndr_err;
	struct drsuapi_DsReplicaOIDMapping_Ctr *ctr;
	struct prefixMapBlob pfm;

	status = dsdb_get_oid_mappings_drsuapi(schema, false, mem_ctx, &ctr);
	W_ERROR_NOT_OK_RETURN(status);

	pfm.version	= PREFIX_MAP_VERSION_DSDB;
	pfm.reserved	= 0;
	pfm.ctr.dsdb	= *ctr;

	ndr_err = ndr_push_struct_blob(prefixMap, mem_ctx, schema->iconv_convenience, &pfm, (ndr_push_flags_fn_t)ndr_push_prefixMapBlob);
	talloc_free(ctr);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		return ntstatus_to_werror(nt_status);
	}

	*schemaInfo = strhex_to_data_blob(mem_ctx, schema->schema_info);
	W_ERROR_HAVE_NO_MEMORY(schemaInfo->data);

	return WERR_OK;
}

WERROR dsdb_verify_oid_mappings_drsuapi(const struct dsdb_schema *schema, const struct drsuapi_DsReplicaOIDMapping_Ctr *ctr)
{
	uint32_t i,j;

	for (i=0; i < ctr->num_mappings; i++) {
		if (ctr->mappings[i].oid.oid == NULL) {
			return WERR_INVALID_PARAM;
		}

		if (strncasecmp(ctr->mappings[i].oid.oid, "ff", 2) == 0) {
			if (ctr->mappings[i].id_prefix != 0) {
				return WERR_INVALID_PARAM;
			}

			/* the magic value should be in the last array member */
			if (i != (ctr->num_mappings - 1)) {
				return WERR_INVALID_PARAM;
			}

			if (ctr->mappings[i].oid.__ndr_size != 21) {
				return WERR_INVALID_PARAM;
			}

			if (strcasecmp(schema->schema_info, ctr->mappings[i].oid.oid) != 0) {
				return WERR_DS_DRA_SCHEMA_MISMATCH;
			}
		} else {
			/* the last array member should contain the magic value not a oid */
			if (i == (ctr->num_mappings - 1)) {
				return WERR_INVALID_PARAM;
			}

			for (j=0; j < schema->num_prefixes; j++) {
				size_t oid_len;
				if (schema->prefixes[j].id != (ctr->mappings[i].id_prefix<<16)) {
					continue;
				}

				oid_len = strlen(ctr->mappings[i].oid.oid);

				if (oid_len != (schema->prefixes[j].oid_len - 1)) {
					return WERR_DS_DRA_SCHEMA_MISMATCH;
				}

				if (strncmp(ctr->mappings[i].oid.oid, schema->prefixes[j].oid, oid_len) != 0) {
					return WERR_DS_DRA_SCHEMA_MISMATCH;				
				}

				break;
			}

			if (j == schema->num_prefixes) {
				return WERR_DS_DRA_SCHEMA_MISMATCH;				
			}
		}
	}

	return WERR_OK;
}

WERROR dsdb_map_oid2int(const struct dsdb_schema *schema, const char *in, uint32_t *out)
{
	return dsdb_find_prefix_for_oid(schema->num_prefixes, schema->prefixes, in, out);
}


WERROR dsdb_map_int2oid(const struct dsdb_schema *schema, uint32_t in, TALLOC_CTX *mem_ctx, const char **out)
{
	uint32_t i;

	for (i=0; i < schema->num_prefixes; i++) {
		const char *val;
		if (schema->prefixes[i].id != (in & 0xFFFF0000)) {
			continue;
		}

		val = talloc_asprintf(mem_ctx, "%s%u",
				      schema->prefixes[i].oid,
				      in & 0xFFFF);
		W_ERROR_HAVE_NO_MEMORY(val);

		*out = val;
		return WERR_OK;
	}

	return WERR_DS_NO_MSDS_INTID;
}

/*
 * this function is called from within a ldb transaction from the schema_fsmo module
 */
WERROR dsdb_create_prefix_mapping(struct ldb_context *ldb, struct dsdb_schema *schema, const char *full_oid)
{
	WERROR status;
	uint32_t num_prefixes;
	struct dsdb_schema_oid_prefix *prefixes;
	TALLOC_CTX *mem_ctx;
	uint32_t out;

	mem_ctx = talloc_new(ldb);
	W_ERROR_HAVE_NO_MEMORY(mem_ctx);

	/* Read prefixes from disk*/
	status = dsdb_read_prefixes_from_ldb( mem_ctx, ldb, &num_prefixes, &prefixes ); 
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("dsdb_create_prefix_mapping: dsdb_read_prefixes_from_ldb: %s\n",
			win_errstr(status)));
		talloc_free(mem_ctx);
		return status;
	}

	/* Check if there is a prefix for the oid in the prefixes array*/
	status = dsdb_find_prefix_for_oid( num_prefixes, prefixes, full_oid, &out ); 
	if (W_ERROR_IS_OK(status)) {
		/* prefix found*/
		talloc_free(mem_ctx);
		return status;
	} else if (!W_ERROR_EQUAL(WERR_DS_NO_MSDS_INTID, status)) {
		/* error */
		DEBUG(0,("dsdb_create_prefix_mapping: dsdb_find_prefix_for_oid: %s\n",
			win_errstr(status)));
		talloc_free(mem_ctx);
		return status;
	}

	/* Create the new mapping for the prefix of full_oid */
	status = dsdb_prefix_map_update(mem_ctx, &num_prefixes, &prefixes, full_oid);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("dsdb_create_prefix_mapping: dsdb_prefix_map_update: %s\n",
			win_errstr(status)));
		talloc_free(mem_ctx);
		return status;
	}

	talloc_free(schema->prefixes);
	schema->prefixes = talloc_steal(schema, prefixes);
	schema->num_prefixes = num_prefixes;

	/* Update prefixMap in ldb*/
	status = dsdb_write_prefixes_from_schema_to_ldb(mem_ctx, ldb, schema);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("dsdb_create_prefix_mapping: dsdb_write_prefixes_to_ldb: %s\n",
			win_errstr(status)));
		talloc_free(mem_ctx);
		return status;
	}

	DEBUG(2,(__location__ " Added prefixMap %s - now have %u prefixes\n",
		 full_oid, num_prefixes));

	talloc_free(mem_ctx);
	return status;
}

WERROR dsdb_prefix_map_update(TALLOC_CTX *mem_ctx, uint32_t *num_prefixes, struct dsdb_schema_oid_prefix **prefixes, const char *oid)
{
	uint32_t new_num_prefixes, index_new_prefix, new_entry_id;
	const char* lastDotOffset;
	size_t size;
	
	new_num_prefixes = *num_prefixes + 1;
	index_new_prefix = *num_prefixes;

	/*
	 * this is the algorithm we use to create new mappings for now
	 *
	 * TODO: find what algorithm windows use
	 */
	new_entry_id = (*num_prefixes)<<16;

	/* Extract the prefix from the oid*/
	lastDotOffset = strrchr(oid, '.');
	if (lastDotOffset == NULL) {
		DEBUG(0,("dsdb_prefix_map_update: failed to find the last dot\n"));
		return WERR_NOT_FOUND;
	}

	/* Calculate the size of the remainig string that should be the prefix of it */
	size = strlen(oid) - strlen(lastDotOffset);
	if (size <= 0) {
		DEBUG(0,("dsdb_prefix_map_update: size of the remaining string invalid\n"));
		return WERR_FOOBAR;
	}
	/* Add one because we need to copy the dot */
	size += 1;

	/* Create a spot in the prefixMap for one more prefix*/
	(*prefixes) = talloc_realloc(mem_ctx, *prefixes, struct dsdb_schema_oid_prefix, new_num_prefixes);
	W_ERROR_HAVE_NO_MEMORY(*prefixes);

	/* Add the new prefix entry*/
	(*prefixes)[index_new_prefix].id = new_entry_id;
	(*prefixes)[index_new_prefix].oid = talloc_strndup(mem_ctx, oid, size);
	(*prefixes)[index_new_prefix].oid_len = strlen((*prefixes)[index_new_prefix].oid);

	/* Increase num_prefixes because new prefix has been added */
	++(*num_prefixes);

	return WERR_OK;
}

WERROR dsdb_find_prefix_for_oid(uint32_t num_prefixes, const struct dsdb_schema_oid_prefix *prefixes, const char *in, uint32_t *out)
{
	uint32_t i;

	for (i=0; i < num_prefixes; i++) {
		const char *val_str;
		char *end_str;
		unsigned val;

		if (strncmp(prefixes[i].oid, in, prefixes[i].oid_len) != 0) {
			continue;
		}

		val_str = in + prefixes[i].oid_len;
		end_str = NULL;
		errno = 0;

		if (val_str[0] == '\0') {
			return WERR_INVALID_PARAM;
		}

		/* two '.' chars are invalid */
		if (val_str[0] == '.') {
			return WERR_INVALID_PARAM;
		}

		val = strtoul(val_str, &end_str, 10);
		if (end_str[0] == '.' && end_str[1] != '\0') {
			/*
			 * if it's a '.' and not the last char
			 * then maybe an other mapping apply
			 */
			continue;
		} else if (end_str[0] != '\0') {
			return WERR_INVALID_PARAM;
		} else if (val > 0xFFFF) {
			return WERR_INVALID_PARAM;
		}

		*out = prefixes[i].id | val;
		return WERR_OK;
	}

	DEBUG(5,(__location__ " Failed to find oid %s - have %u prefixes\n", in, num_prefixes));

	return WERR_DS_NO_MSDS_INTID;
}

WERROR dsdb_write_prefixes_from_schema_to_ldb(TALLOC_CTX *mem_ctx, struct ldb_context *ldb,
						     const struct dsdb_schema *schema)
{
	struct ldb_message *msg = ldb_msg_new(mem_ctx);
	struct ldb_dn *schema_dn;
	struct prefixMapBlob pm;
	struct ldb_val ndr_blob;
	enum ndr_err_code ndr_err;
	uint32_t i;
	int ret;

	if (!msg) {
		return WERR_NOMEM;
	}
	
	schema_dn = samdb_schema_dn(ldb);
	if (!schema_dn) {
		DEBUG(0,("dsdb_write_prefixes_from_schema_to_ldb: no schema dn present\n"));	
		return WERR_FOOBAR;
	}

	pm.version			= PREFIX_MAP_VERSION_DSDB;
	pm.ctr.dsdb.num_mappings	= schema->num_prefixes;
	pm.ctr.dsdb.mappings		= talloc_array(msg,
						struct drsuapi_DsReplicaOIDMapping,
						pm.ctr.dsdb.num_mappings);
	if (!pm.ctr.dsdb.mappings) {
		talloc_free(msg);
		return WERR_NOMEM;
	}

	for (i=0; i < schema->num_prefixes; i++) {
		pm.ctr.dsdb.mappings[i].id_prefix = schema->prefixes[i].id>>16;
		pm.ctr.dsdb.mappings[i].oid.oid = talloc_strdup(pm.ctr.dsdb.mappings, schema->prefixes[i].oid);
	}

	ndr_err = ndr_push_struct_blob(&ndr_blob, msg,
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
				       &pm,
				       (ndr_push_flags_fn_t)ndr_push_prefixMapBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(msg);
		return WERR_FOOBAR;
	}
 
	msg->dn = schema_dn;
	ret = ldb_msg_add_value(msg, "prefixMap", &ndr_blob, NULL);
	if (ret != 0) {
		talloc_free(msg);
		DEBUG(0,("dsdb_write_prefixes_from_schema_to_ldb: ldb_msg_add_value failed\n"));	
		return WERR_NOMEM;
 	}
 
	ret = samdb_replace( ldb, msg, msg );
	talloc_free(msg);

	if (ret != 0) {
		DEBUG(0,("dsdb_write_prefixes_from_schema_to_ldb: samdb_replace failed\n"));	
		return WERR_FOOBAR;
 	}
 
	return WERR_OK;
}

WERROR dsdb_read_prefixes_from_ldb(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, uint32_t* num_prefixes, struct dsdb_schema_oid_prefix **prefixes)
{
	struct prefixMapBlob *blob;
	enum ndr_err_code ndr_err;
	uint32_t i;
	const struct ldb_val *prefix_val;
	struct ldb_dn *schema_dn;
	struct ldb_result *schema_res;
	int ret;    
	static const char *schema_attrs[] = {
		"prefixMap",
		NULL
	};

	schema_dn = samdb_schema_dn(ldb);
	if (!schema_dn) {
		DEBUG(0,("dsdb_read_prefixes_from_ldb: no schema dn present\n"));
		return WERR_FOOBAR;
	}

	ret = ldb_search(ldb, mem_ctx, &schema_res, schema_dn, LDB_SCOPE_BASE, schema_attrs, NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		DEBUG(0,("dsdb_read_prefixes_from_ldb: no prefix map present\n"));
		talloc_free(schema_res);
		return WERR_FOOBAR;
	} else if (ret != LDB_SUCCESS) {
		DEBUG(0,("dsdb_read_prefixes_from_ldb: failed to search the schema head\n"));
		talloc_free(schema_res);
		return WERR_FOOBAR;
	}

	prefix_val = ldb_msg_find_ldb_val(schema_res->msgs[0], "prefixMap");
	if (!prefix_val) {
		DEBUG(0,("dsdb_read_prefixes_from_ldb: no prefixMap attribute found\n"));
		talloc_free(schema_res);
		return WERR_FOOBAR;
	}

	blob = talloc(mem_ctx, struct prefixMapBlob);
	W_ERROR_HAVE_NO_MEMORY(blob);

	ndr_err = ndr_pull_struct_blob(prefix_val, blob, 
					   lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
					   blob,
					   (ndr_pull_flags_fn_t)ndr_pull_prefixMapBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		DEBUG(0,("dsdb_read_prefixes_from_ldb: ndr_pull_struct_blob failed\n"));
		talloc_free(blob);
		talloc_free(schema_res);
		return WERR_FOOBAR;
	}

	talloc_free(schema_res);

	if (blob->version != PREFIX_MAP_VERSION_DSDB) {
		DEBUG(0,("dsdb_read_prefixes_from_ldb: blob->version incorect\n"));
		talloc_free(blob);
		return WERR_FOOBAR;
	}
	
	*num_prefixes = blob->ctr.dsdb.num_mappings;
	*prefixes = talloc_array(mem_ctx, struct dsdb_schema_oid_prefix, *num_prefixes);
	if(!(*prefixes)) {
		talloc_free(blob);
		return WERR_NOMEM;
	}
	for (i=0; i < blob->ctr.dsdb.num_mappings; i++) {
		char *oid;
		(*prefixes)[i].id = blob->ctr.dsdb.mappings[i].id_prefix<<16;
		oid = talloc_strdup(mem_ctx, blob->ctr.dsdb.mappings[i].oid.oid);
		(*prefixes)[i].oid = talloc_asprintf_append(oid, "."); 
		(*prefixes)[i].oid_len = strlen((*prefixes)[i].oid);
	}

	talloc_free(blob);
	return WERR_OK;
}

/*
  this will be replaced with something that looks at the right part of
  the schema once we know where unique indexing information is hidden
 */
static bool dsdb_schema_unique_attribute(const char *attr)
{
	const char *attrs[] = { "objectGUID", "objectSID" , NULL };
	int i;
	for (i=0;attrs[i];i++) {
		if (strcasecmp(attr, attrs[i]) == 0) {
			return true;
		}
	}
	return false;
}


/*
  setup the ldb_schema_attribute field for a dsdb_attribute
 */
static int dsdb_schema_setup_ldb_schema_attribute(struct ldb_context *ldb, 
						  struct dsdb_attribute *attr)
{
	const char *syntax = attr->syntax->ldb_syntax;
	const struct ldb_schema_syntax *s;
	struct ldb_schema_attribute *a;

	if (!syntax) {
		syntax = attr->syntax->ldap_oid;
	}

	s = ldb_samba_syntax_by_lDAPDisplayName(ldb, attr->lDAPDisplayName);
	if (s == NULL) {
		s = ldb_samba_syntax_by_name(ldb, syntax);
	}
	if (s == NULL) {
		s = ldb_standard_syntax_by_name(ldb, syntax);
	}

	if (s == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;		
	}

	attr->ldb_schema_attribute = a = talloc(attr, struct ldb_schema_attribute);
	if (attr->ldb_schema_attribute == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	a->name = attr->lDAPDisplayName;
	a->flags = 0;
	a->syntax = s;

	if (dsdb_schema_unique_attribute(a->name)) {
		a->flags |= LDB_ATTR_FLAG_UNIQUE_INDEX;
	}
	if (attr->isSingleValued) {
		a->flags |= LDB_ATTR_FLAG_SINGLE_VALUE;
	}
	
	
	return LDB_SUCCESS;
}


#define GET_STRING_LDB(msg, attr, mem_ctx, p, elem, strict) do { \
	const struct ldb_val *get_string_val = ldb_msg_find_ldb_val(msg, attr); \
	if (get_string_val == NULL) { \
		if (strict) {					  \
			d_printf("%s: %s == NULL\n", __location__, attr); \
			return WERR_INVALID_PARAM;			\
		} else {						\
			(p)->elem = NULL;				\
		}							\
	} else {							\
		(p)->elem = talloc_strndup(mem_ctx,			\
					   (const char *)get_string_val->data, \
					   get_string_val->length); \
		if (!(p)->elem) {					\
			d_printf("%s: talloc_strndup failed for %s\n", __location__, attr); \
			return WERR_NOMEM;				\
		}							\
	}								\
} while (0)

#define GET_STRING_LIST_LDB(msg, attr, mem_ctx, p, elem, strict) do {	\
	int get_string_list_counter;					\
	struct ldb_message_element *get_string_list_el = ldb_msg_find_element(msg, attr); \
	if (get_string_list_el == NULL) {				\
		if (strict) {						\
			d_printf("%s: %s == NULL\n", __location__, attr); \
			return WERR_INVALID_PARAM;			\
		} else {						\
			(p)->elem = NULL;				\
			break;						\
		}							\
	}								\
	(p)->elem = talloc_array(mem_ctx, const char *, get_string_list_el->num_values + 1); \
        for (get_string_list_counter=0;					\
	     get_string_list_counter < get_string_list_el->num_values;	\
	     get_string_list_counter++) {				\
		(p)->elem[get_string_list_counter] = talloc_strndup((p)->elem, \
								    (const char *)get_string_list_el->values[get_string_list_counter].data, \
								    get_string_list_el->values[get_string_list_counter].length); \
		if (!(p)->elem[get_string_list_counter]) {		\
			d_printf("%s: talloc_strndup failed for %s\n", __location__, attr); \
			return WERR_NOMEM;				\
		}							\
		(p)->elem[get_string_list_counter+1] = NULL;		\
	}								\
	talloc_steal(mem_ctx, (p)->elem);				\
} while (0)

#define GET_BOOL_LDB(msg, attr, p, elem, strict) do { \
	const char *str; \
	str = samdb_result_string(msg, attr, NULL);\
	if (str == NULL) { \
		if (strict) { \
			d_printf("%s: %s == NULL\n", __location__, attr); \
			return WERR_INVALID_PARAM; \
		} else { \
			(p)->elem = false; \
		} \
	} else if (strcasecmp("TRUE", str) == 0) { \
		(p)->elem = true; \
	} else if (strcasecmp("FALSE", str) == 0) { \
		(p)->elem = false; \
	} else { \
		d_printf("%s: %s == %s\n", __location__, attr, str); \
		return WERR_INVALID_PARAM; \
	} \
} while (0)

#define GET_UINT32_LDB(msg, attr, p, elem) do { \
	(p)->elem = samdb_result_uint(msg, attr, 0);\
} while (0)

#define GET_UINT32_PTR_LDB(msg, attr, p, elem) do { \
	uint64_t _v = samdb_result_uint64(msg, attr, UINT64_MAX);\
	if (_v == UINT64_MAX) { \
		(p)->elem = NULL; \
	} else if (_v > UINT32_MAX) { \
		d_printf("%s: %s == 0x%llX\n", __location__, \
			 attr, (unsigned long long)_v); \
		return WERR_INVALID_PARAM; \
	} else { \
		(p)->elem = talloc(mem_ctx, uint32_t); \
		if (!(p)->elem) { \
			d_printf("%s: talloc failed for %s\n", __location__, attr); \
			return WERR_NOMEM; \
		} \
		*(p)->elem = (uint32_t)_v; \
	} \
} while (0)

#define GET_GUID_LDB(msg, attr, p, elem) do { \
	(p)->elem = samdb_result_guid(msg, attr);\
} while (0)

#define GET_BLOB_LDB(msg, attr, mem_ctx, p, elem) do { \
	const struct ldb_val *_val;\
	_val = ldb_msg_find_ldb_val(msg, attr);\
	if (_val) {\
		(p)->elem = *_val;\
		talloc_steal(mem_ctx, (p)->elem.data);\
	} else {\
		ZERO_STRUCT((p)->elem);\
	}\
} while (0)

WERROR dsdb_attribute_from_ldb(struct ldb_context *ldb,
			       const struct dsdb_schema *schema,
			       struct ldb_message *msg,
			       TALLOC_CTX *mem_ctx,
			       struct dsdb_attribute *attr)
{
	WERROR status;

	GET_STRING_LDB(msg, "cn", mem_ctx, attr, cn, false);
	GET_STRING_LDB(msg, "lDAPDisplayName", mem_ctx, attr, lDAPDisplayName, true);
	GET_STRING_LDB(msg, "attributeID", mem_ctx, attr, attributeID_oid, true);
	if (schema->num_prefixes == 0) {
		/* set an invalid value */
		attr->attributeID_id = 0xFFFFFFFF;
	} else {
		status = dsdb_map_oid2int(schema, attr->attributeID_oid, &attr->attributeID_id);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0,("%s: '%s': unable to map attributeID %s: %s\n",
				__location__, attr->lDAPDisplayName, attr->attributeID_oid,
				win_errstr(status)));
			return status;
		}
	}
	GET_GUID_LDB(msg, "schemaIDGUID", attr, schemaIDGUID);
	GET_UINT32_LDB(msg, "mAPIID", attr, mAPIID);

	GET_GUID_LDB(msg, "attributeSecurityGUID", attr, attributeSecurityGUID);

	GET_UINT32_LDB(msg, "searchFlags", attr, searchFlags);
	GET_UINT32_LDB(msg, "systemFlags", attr, systemFlags);
	GET_BOOL_LDB(msg, "isMemberOfPartialAttributeSet", attr, isMemberOfPartialAttributeSet, false);
	GET_UINT32_LDB(msg, "linkID", attr, linkID);

	GET_STRING_LDB(msg, "attributeSyntax", mem_ctx, attr, attributeSyntax_oid, true);
	if (schema->num_prefixes == 0) {
		/* set an invalid value */
		attr->attributeSyntax_id = 0xFFFFFFFF;
	} else {
		status = dsdb_map_oid2int(schema, attr->attributeSyntax_oid, &attr->attributeSyntax_id);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0,("%s: '%s': unable to map attributeSyntax_ %s: %s\n",
				__location__, attr->lDAPDisplayName, attr->attributeSyntax_oid,
				win_errstr(status)));
			return status;
		}
	}
	GET_UINT32_LDB(msg, "oMSyntax", attr, oMSyntax);
	GET_BLOB_LDB(msg, "oMObjectClass", mem_ctx, attr, oMObjectClass);

	GET_BOOL_LDB(msg, "isSingleValued", attr, isSingleValued, true);
	GET_UINT32_PTR_LDB(msg, "rangeLower", attr, rangeLower);
	GET_UINT32_PTR_LDB(msg, "rangeUpper", attr, rangeUpper);
	GET_BOOL_LDB(msg, "extendedCharsAllowed", attr, extendedCharsAllowed, false);

	GET_UINT32_LDB(msg, "schemaFlagsEx", attr, schemaFlagsEx);
	GET_BLOB_LDB(msg, "msDs-Schema-Extensions", mem_ctx, attr, msDs_Schema_Extensions);

	GET_BOOL_LDB(msg, "showInAdvancedViewOnly", attr, showInAdvancedViewOnly, false);
	GET_STRING_LDB(msg, "adminDisplayName", mem_ctx, attr, adminDisplayName, false);
	GET_STRING_LDB(msg, "adminDescription", mem_ctx, attr, adminDescription, false);
	GET_STRING_LDB(msg, "classDisplayName", mem_ctx, attr, classDisplayName, false);
	GET_BOOL_LDB(msg, "isEphemeral", attr, isEphemeral, false);
	GET_BOOL_LDB(msg, "isDefunct", attr, isDefunct, false);
	GET_BOOL_LDB(msg, "systemOnly", attr, systemOnly, false);

	attr->syntax = dsdb_syntax_for_attribute(attr);
	if (!attr->syntax) {
		return WERR_DS_ATT_SCHEMA_REQ_SYNTAX;
	}

	if (dsdb_schema_setup_ldb_schema_attribute(ldb, attr) != LDB_SUCCESS) {
		return WERR_DS_ATT_SCHEMA_REQ_SYNTAX;
	}

	return WERR_OK;
}

WERROR dsdb_class_from_ldb(const struct dsdb_schema *schema,
			   struct ldb_message *msg,
			   TALLOC_CTX *mem_ctx,
			   struct dsdb_class *obj)
{
	WERROR status;

	GET_STRING_LDB(msg, "cn", mem_ctx, obj, cn, false);
	GET_STRING_LDB(msg, "lDAPDisplayName", mem_ctx, obj, lDAPDisplayName, true);
	GET_STRING_LDB(msg, "governsID", mem_ctx, obj, governsID_oid, true);
	if (schema->num_prefixes == 0) {
		/* set an invalid value */
		obj->governsID_id = 0xFFFFFFFF;
	} else {
		status = dsdb_map_oid2int(schema, obj->governsID_oid, &obj->governsID_id);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0,("%s: '%s': unable to map governsID %s: %s\n",
				__location__, obj->lDAPDisplayName, obj->governsID_oid,
				win_errstr(status)));
			return status;
		}
	}
	GET_GUID_LDB(msg, "schemaIDGUID", obj, schemaIDGUID);

	GET_UINT32_LDB(msg, "objectClassCategory", obj, objectClassCategory);
	GET_STRING_LDB(msg, "rDNAttID", mem_ctx, obj, rDNAttID, false);
	GET_STRING_LDB(msg, "defaultObjectCategory", mem_ctx, obj, defaultObjectCategory, true);
 
	GET_STRING_LDB(msg, "subClassOf", mem_ctx, obj, subClassOf, true);

	GET_STRING_LIST_LDB(msg, "systemAuxiliaryClass", mem_ctx, obj, systemAuxiliaryClass, false);
	GET_STRING_LIST_LDB(msg, "auxiliaryClass", mem_ctx, obj, auxiliaryClass, false);

	GET_STRING_LIST_LDB(msg, "systemMustContain", mem_ctx, obj, systemMustContain, false);
	GET_STRING_LIST_LDB(msg, "systemMayContain", mem_ctx, obj, systemMayContain, false);
	GET_STRING_LIST_LDB(msg, "mustContain", mem_ctx, obj, mustContain, false);
	GET_STRING_LIST_LDB(msg, "mayContain", mem_ctx, obj, mayContain, false);

	GET_STRING_LIST_LDB(msg, "systemPossSuperiors", mem_ctx, obj, systemPossSuperiors, false);
	GET_STRING_LIST_LDB(msg, "possSuperiors", mem_ctx, obj, possSuperiors, false);

	GET_STRING_LDB(msg, "defaultSecurityDescriptor", mem_ctx, obj, defaultSecurityDescriptor, false);

	GET_UINT32_LDB(msg, "schemaFlagsEx", obj, schemaFlagsEx);
	GET_BLOB_LDB(msg, "msDs-Schema-Extensions", mem_ctx, obj, msDs_Schema_Extensions);

	GET_BOOL_LDB(msg, "showInAdvancedViewOnly", obj, showInAdvancedViewOnly, false);
	GET_STRING_LDB(msg, "adminDisplayName", mem_ctx, obj, adminDisplayName, false);
	GET_STRING_LDB(msg, "adminDescription", mem_ctx, obj, adminDescription, false);
	GET_STRING_LDB(msg, "classDisplayName", mem_ctx, obj, classDisplayName, false);
	GET_BOOL_LDB(msg, "defaultHidingValue", obj, defaultHidingValue, false);
	GET_BOOL_LDB(msg, "isDefunct", obj, isDefunct, false);
	GET_BOOL_LDB(msg, "systemOnly", obj, systemOnly, false);

	return WERR_OK;
}

#define dsdb_oom(error_string, mem_ctx) *error_string = talloc_asprintf(mem_ctx, "dsdb out of memory at %s:%d\n", __FILE__, __LINE__)

/* 
 Create a DSDB schema from the ldb results provided.  This is called
 directly when the schema is provisioned from an on-disk LDIF file, or
 from dsdb_schema_from_schema_dn below
*/

int dsdb_schema_from_ldb_results(TALLOC_CTX *mem_ctx, struct ldb_context *ldb,
				 struct smb_iconv_convenience *iconv_convenience, 
				 struct ldb_result *schema_res,
				 struct ldb_result *attrs_res, struct ldb_result *objectclass_res, 
				 struct dsdb_schema **schema_out,
				 char **error_string)
{
	WERROR status;
	uint32_t i;
	const struct ldb_val *prefix_val;
	const struct ldb_val *info_val;
	struct ldb_val info_val_default;
	struct dsdb_schema *schema;

	schema = dsdb_new_schema(mem_ctx, iconv_convenience);
	if (!schema) {
		dsdb_oom(error_string, mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	prefix_val = ldb_msg_find_ldb_val(schema_res->msgs[0], "prefixMap");
	if (!prefix_val) {
		*error_string = talloc_asprintf(mem_ctx, 
						"schema_fsmo_init: no prefixMap attribute found");
		DEBUG(0,(__location__ ": %s\n", *error_string));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	info_val = ldb_msg_find_ldb_val(schema_res->msgs[0], "schemaInfo");
	if (!info_val) {
		info_val_default = strhex_to_data_blob(mem_ctx, "FF0000000000000000000000000000000000000000");
		if (!info_val_default.data) {
			dsdb_oom(error_string, mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		info_val = &info_val_default;
	}

	status = dsdb_load_oid_mappings_ldb(schema, prefix_val, info_val);
	if (!W_ERROR_IS_OK(status)) {
		*error_string = talloc_asprintf(mem_ctx, 
			      "schema_fsmo_init: failed to load oid mappings: %s",
			      win_errstr(status));
		DEBUG(0,(__location__ ": %s\n", *error_string));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	for (i=0; i < attrs_res->count; i++) {
		struct dsdb_attribute *sa;

		sa = talloc_zero(schema, struct dsdb_attribute);
		if (!sa) {
			dsdb_oom(error_string, mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		status = dsdb_attribute_from_ldb(ldb, schema, attrs_res->msgs[i], sa, sa);
		if (!W_ERROR_IS_OK(status)) {
			*error_string = talloc_asprintf(mem_ctx, 
				      "schema_fsmo_init: failed to load attribute definition: %s:%s",
				      ldb_dn_get_linearized(attrs_res->msgs[i]->dn),
				      win_errstr(status));
			DEBUG(0,(__location__ ": %s\n", *error_string));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}

		DLIST_ADD(schema->attributes, sa);
	}

	for (i=0; i < objectclass_res->count; i++) {
		struct dsdb_class *sc;

		sc = talloc_zero(schema, struct dsdb_class);
		if (!sc) {
			dsdb_oom(error_string, mem_ctx);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		status = dsdb_class_from_ldb(schema, objectclass_res->msgs[i], sc, sc);
		if (!W_ERROR_IS_OK(status)) {
			*error_string = talloc_asprintf(mem_ctx, 
				      "schema_fsmo_init: failed to load class definition: %s:%s",
				      ldb_dn_get_linearized(objectclass_res->msgs[i]->dn),
				      win_errstr(status));
			DEBUG(0,(__location__ ": %s\n", *error_string));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}

		DLIST_ADD(schema->classes, sc);
	}

	schema->fsmo.master_dn = ldb_msg_find_attr_as_dn(ldb, schema, schema_res->msgs[0], "fSMORoleOwner");
	if (ldb_dn_compare(samdb_ntds_settings_dn(ldb), schema->fsmo.master_dn) == 0) {
		schema->fsmo.we_are_master = true;
	} else {
		schema->fsmo.we_are_master = false;
	}

	DEBUG(5, ("schema_fsmo_init: we are master: %s\n",
		  (schema->fsmo.we_are_master?"yes":"no")));

	*schema_out = schema;
	return LDB_SUCCESS;
}

/*
  Given an LDB, and the DN, return a populated schema
*/

int dsdb_schema_from_schema_dn(TALLOC_CTX *mem_ctx, struct ldb_context *ldb,
			       struct smb_iconv_convenience *iconv_convenience, 
			       struct ldb_dn *schema_dn,
			       struct dsdb_schema **schema,
			       char **error_string_out) 
{
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

	tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		dsdb_oom(error_string_out, mem_ctx);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* we don't want to trace the schema load */
	flags = ldb_get_flags(ldb);
	ldb_set_flags(ldb, flags & ~LDB_FLG_ENABLE_TRACING);

	/*
	 * setup the prefix mappings and schema info
	 */
	ret = ldb_search(ldb, tmp_ctx, &schema_res,
			 schema_dn, LDB_SCOPE_BASE, schema_attrs, NULL);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		goto failed;
	} else if (ret != LDB_SUCCESS) {
		*error_string_out = talloc_asprintf(mem_ctx, 
				       "dsdb_schema: failed to search the schema head: %s",
				       ldb_errstring(ldb));
		goto failed;
	}
	if (schema_res->count != 1) {
		*error_string_out = talloc_asprintf(mem_ctx, 
			      "dsdb_schema: [%u] schema heads found on a base search",
			      schema_res->count);
		goto failed;
	}

	/*
	 * load the attribute definitions
	 */
	ret = ldb_search(ldb, tmp_ctx, &a_res,
			 schema_dn, LDB_SCOPE_ONELEVEL, NULL,
			 "(objectClass=attributeSchema)");
	if (ret != LDB_SUCCESS) {
		*error_string_out = talloc_asprintf(mem_ctx, 
				       "dsdb_schema: failed to search attributeSchema objects: %s",
				       ldb_errstring(ldb));
		goto failed;
	}

	/*
	 * load the objectClass definitions
	 */
	ret = ldb_search(ldb, tmp_ctx, &c_res,
			 schema_dn, LDB_SCOPE_ONELEVEL, NULL,
			 "(objectClass=classSchema)");
	if (ret != LDB_SUCCESS) {
		*error_string_out = talloc_asprintf(mem_ctx, 
				       "dsdb_schema: failed to search attributeSchema objects: %s",
				       ldb_errstring(ldb));
		goto failed;
	}

	ret = dsdb_schema_from_ldb_results(tmp_ctx, ldb,
					   lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")),
					   schema_res, a_res, c_res, schema, &error_string);
	if (ret != LDB_SUCCESS) {
		*error_string_out = talloc_asprintf(mem_ctx, 
						    "dsdb_schema load failed: %s",
						    error_string);
		goto failed;
	}
	talloc_steal(mem_ctx, *schema);
	talloc_free(tmp_ctx);

	if (flags & LDB_FLG_ENABLE_TRACING) {
		flags = ldb_get_flags(ldb);
		ldb_set_flags(ldb, flags | LDB_FLG_ENABLE_TRACING);
	}

	return LDB_SUCCESS;

failed:
	if (flags & LDB_FLG_ENABLE_TRACING) {
		flags = ldb_get_flags(ldb);
		ldb_set_flags(ldb, flags | LDB_FLG_ENABLE_TRACING);
	}
	talloc_free(tmp_ctx);
	return ret;
}	


static const struct {
	const char *name;
	const char *oid;
} name_mappings[] = {
	{ "cn",					"2.5.4.3" },
	{ "name",				"1.2.840.113556.1.4.1" },
	{ "lDAPDisplayName",			"1.2.840.113556.1.2.460" },
	{ "attributeID", 			"1.2.840.113556.1.2.30" },
	{ "schemaIDGUID", 			"1.2.840.113556.1.4.148" },
	{ "mAPIID", 				"1.2.840.113556.1.2.49" },
	{ "attributeSecurityGUID", 		"1.2.840.113556.1.4.149" },
	{ "searchFlags", 			"1.2.840.113556.1.2.334" },
	{ "systemFlags", 			"1.2.840.113556.1.4.375" },
	{ "isMemberOfPartialAttributeSet", 	"1.2.840.113556.1.4.639" },
	{ "linkID", 				"1.2.840.113556.1.2.50" },
	{ "attributeSyntax", 			"1.2.840.113556.1.2.32" },
	{ "oMSyntax", 				"1.2.840.113556.1.2.231" },
	{ "oMObjectClass", 			"1.2.840.113556.1.2.218" },
	{ "isSingleValued",			"1.2.840.113556.1.2.33" },
	{ "rangeLower", 			"1.2.840.113556.1.2.34" },
	{ "rangeUpper", 			"1.2.840.113556.1.2.35" },
	{ "extendedCharsAllowed", 		"1.2.840.113556.1.2.380" },
	{ "schemaFlagsEx", 			"1.2.840.113556.1.4.120" },
	{ "msDs-Schema-Extensions", 		"1.2.840.113556.1.4.1440" },
	{ "showInAdvancedViewOnly", 		"1.2.840.113556.1.2.169" },
	{ "adminDisplayName", 			"1.2.840.113556.1.2.194" },
	{ "adminDescription", 			"1.2.840.113556.1.2.226" },
	{ "classDisplayName", 			"1.2.840.113556.1.4.610" },
	{ "isEphemeral", 			"1.2.840.113556.1.4.1212" },
	{ "isDefunct", 				"1.2.840.113556.1.4.661" },
	{ "systemOnly", 			"1.2.840.113556.1.4.170" },
	{ "governsID",				"1.2.840.113556.1.2.22" },
	{ "objectClassCategory",		"1.2.840.113556.1.2.370" },
	{ "rDNAttID",				"1.2.840.113556.1.2.26" },
	{ "defaultObjectCategory",		"1.2.840.113556.1.4.783" },
	{ "subClassOf",				"1.2.840.113556.1.2.21" },
	{ "systemAuxiliaryClass",		"1.2.840.113556.1.4.198" },
	{ "systemPossSuperiors",		"1.2.840.113556.1.4.195" },
	{ "systemMustContain",			"1.2.840.113556.1.4.197" },
	{ "systemMayContain",			"1.2.840.113556.1.4.196" },
	{ "auxiliaryClass",			"1.2.840.113556.1.2.351" },
	{ "possSuperiors",			"1.2.840.113556.1.2.8" },
	{ "mustContain",			"1.2.840.113556.1.2.24" },
	{ "mayContain",				"1.2.840.113556.1.2.25" },
	{ "defaultSecurityDescriptor",		"1.2.840.113556.1.4.224" },
	{ "defaultHidingValue",			"1.2.840.113556.1.4.518" },
};

static struct drsuapi_DsReplicaAttribute *dsdb_find_object_attr_name(struct dsdb_schema *schema,
								     struct drsuapi_DsReplicaObject *obj,
								     const char *name,
								     uint32_t *idx)
{
	WERROR status;
	uint32_t i, id;
	const char *oid = NULL;

	for(i=0; i < ARRAY_SIZE(name_mappings); i++) {
		if (strcmp(name_mappings[i].name, name) != 0) continue;

		oid = name_mappings[i].oid;
		break;
	}

	if (!oid) {
		return NULL;
	}

	status = dsdb_map_oid2int(schema, oid, &id);
	if (!W_ERROR_IS_OK(status)) {
		return NULL;
	}

	for (i=0; i < obj->attribute_ctr.num_attributes; i++) {
		if (obj->attribute_ctr.attributes[i].attid != id) continue;

		if (idx) *idx = i;
		return &obj->attribute_ctr.attributes[i];
	}

	return NULL;
}

#define GET_STRING_DS(s, r, attr, mem_ctx, p, elem, strict) do { \
	struct drsuapi_DsReplicaAttribute *_a; \
	_a = dsdb_find_object_attr_name(s, r, attr, NULL); \
	if (strict && !_a) { \
		d_printf("%s: %s == NULL\n", __location__, attr); \
		return WERR_INVALID_PARAM; \
	} \
	if (strict && _a->value_ctr.num_values != 1) { \
		d_printf("%s: %s num_values == %u\n", __location__, attr, \
			_a->value_ctr.num_values); \
		return WERR_INVALID_PARAM; \
	} \
	if (_a && _a->value_ctr.num_values >= 1) { \
		size_t _ret; \
		if (!convert_string_talloc_convenience(mem_ctx, s->iconv_convenience, CH_UTF16, CH_UNIX, \
					     _a->value_ctr.values[0].blob->data, \
					     _a->value_ctr.values[0].blob->length, \
					     (void **)discard_const(&(p)->elem), &_ret, false)) { \
			DEBUG(0,("%s: invalid data!\n", attr)); \
			dump_data(0, \
				     _a->value_ctr.values[0].blob->data, \
				     _a->value_ctr.values[0].blob->length); \
			return WERR_FOOBAR; \
		} \
	} else { \
		(p)->elem = NULL; \
	} \
} while (0)

#define GET_UINT32_LIST_DS(s, r, attr, mem_ctx, p, elem) do { \
	int list_counter;					\
	struct drsuapi_DsReplicaAttribute *_a; \
	_a = dsdb_find_object_attr_name(s, r, attr, NULL); \
	(p)->elem = _a ? talloc_array(mem_ctx, uint32_t, _a->value_ctr.num_values + 1) : NULL; \
        for (list_counter=0;					\
	     _a && list_counter < _a->value_ctr.num_values;	\
	     list_counter++) {				\
		if (_a->value_ctr.values[list_counter].blob->length != 4) { \
			return WERR_INVALID_PARAM;			\
		}							\
		(p)->elem[list_counter] = IVAL(_a->value_ctr.values[list_counter].blob->data, 0); \
	}								\
	if (_a) (p)->elem[list_counter] = 0;				\
} while (0)

#define GET_DN_DS(s, r, attr, mem_ctx, p, elem, strict) do { \
	struct drsuapi_DsReplicaAttribute *_a; \
	_a = dsdb_find_object_attr_name(s, r, attr, NULL); \
	if (strict && !_a) { \
		d_printf("%s: %s == NULL\n", __location__, attr); \
		return WERR_INVALID_PARAM; \
	} \
	if (strict && _a->value_ctr.num_values != 1) { \
		d_printf("%s: %s num_values == %u\n", __location__, attr, \
			_a->value_ctr.num_values); \
		return WERR_INVALID_PARAM; \
	} \
	if (strict && !_a->value_ctr.values[0].blob) { \
		d_printf("%s: %s data == NULL\n", __location__, attr); \
		return WERR_INVALID_PARAM; \
	} \
	if (_a && _a->value_ctr.num_values >= 1 \
	    && _a->value_ctr.values[0].blob) { \
		struct drsuapi_DsReplicaObjectIdentifier3 _id3; \
		enum ndr_err_code _ndr_err; \
		_ndr_err = ndr_pull_struct_blob_all(_a->value_ctr.values[0].blob, \
						      mem_ctx, s->iconv_convenience, &_id3,\
						      (ndr_pull_flags_fn_t)ndr_pull_drsuapi_DsReplicaObjectIdentifier3);\
		if (!NDR_ERR_CODE_IS_SUCCESS(_ndr_err)) { \
			NTSTATUS _nt_status = ndr_map_error2ntstatus(_ndr_err); \
			return ntstatus_to_werror(_nt_status); \
		} \
		(p)->elem = _id3.dn; \
	} else { \
		(p)->elem = NULL; \
	} \
} while (0)

#define GET_BOOL_DS(s, r, attr, p, elem, strict) do { \
	struct drsuapi_DsReplicaAttribute *_a; \
	_a = dsdb_find_object_attr_name(s, r, attr, NULL); \
	if (strict && !_a) { \
		d_printf("%s: %s == NULL\n", __location__, attr); \
		return WERR_INVALID_PARAM; \
	} \
	if (strict && _a->value_ctr.num_values != 1) { \
		d_printf("%s: %s num_values == %u\n", __location__, attr, \
			 (unsigned int)_a->value_ctr.num_values);	\
		return WERR_INVALID_PARAM; \
	} \
	if (strict && !_a->value_ctr.values[0].blob) { \
		d_printf("%s: %s data == NULL\n", __location__, attr); \
		return WERR_INVALID_PARAM; \
	} \
	if (strict && _a->value_ctr.values[0].blob->length != 4) { \
		d_printf("%s: %s length == %u\n", __location__, attr, \
			 (unsigned int)_a->value_ctr.values[0].blob->length); \
		return WERR_INVALID_PARAM; \
	} \
	if (_a && _a->value_ctr.num_values >= 1 \
	    && _a->value_ctr.values[0].blob \
	    && _a->value_ctr.values[0].blob->length == 4) { \
		(p)->elem = (IVAL(_a->value_ctr.values[0].blob->data,0)?true:false);\
	} else { \
		(p)->elem = false; \
	} \
} while (0)

#define GET_UINT32_DS(s, r, attr, p, elem) do { \
	struct drsuapi_DsReplicaAttribute *_a; \
	_a = dsdb_find_object_attr_name(s, r, attr, NULL); \
	if (_a && _a->value_ctr.num_values >= 1 \
	    && _a->value_ctr.values[0].blob \
	    && _a->value_ctr.values[0].blob->length == 4) { \
		(p)->elem = IVAL(_a->value_ctr.values[0].blob->data,0);\
	} else { \
		(p)->elem = 0; \
	} \
} while (0)

#define GET_UINT32_PTR_DS(s, r, attr, p, elem) do { \
	struct drsuapi_DsReplicaAttribute *_a; \
	_a = dsdb_find_object_attr_name(s, r, attr, NULL); \
	if (_a && _a->value_ctr.num_values >= 1 \
	    && _a->value_ctr.values[0].blob \
	    && _a->value_ctr.values[0].blob->length == 4) { \
		(p)->elem = talloc(mem_ctx, uint32_t); \
		if (!(p)->elem) { \
			d_printf("%s: talloc failed for %s\n", __location__, attr); \
			return WERR_NOMEM; \
		} \
		*(p)->elem = IVAL(_a->value_ctr.values[0].blob->data,0);\
	} else { \
		(p)->elem = NULL; \
	} \
} while (0)

#define GET_GUID_DS(s, r, attr, mem_ctx, p, elem) do { \
	struct drsuapi_DsReplicaAttribute *_a; \
	_a = dsdb_find_object_attr_name(s, r, attr, NULL); \
	if (_a && _a->value_ctr.num_values >= 1 \
	    && _a->value_ctr.values[0].blob \
	    && _a->value_ctr.values[0].blob->length == 16) { \
		enum ndr_err_code _ndr_err; \
		_ndr_err = ndr_pull_struct_blob_all(_a->value_ctr.values[0].blob, \
						      mem_ctx, s->iconv_convenience, &(p)->elem, \
						      (ndr_pull_flags_fn_t)ndr_pull_GUID); \
		if (!NDR_ERR_CODE_IS_SUCCESS(_ndr_err)) { \
			NTSTATUS _nt_status = ndr_map_error2ntstatus(_ndr_err); \
			return ntstatus_to_werror(_nt_status); \
		} \
	} else { \
		ZERO_STRUCT((p)->elem);\
	} \
} while (0)

#define GET_BLOB_DS(s, r, attr, mem_ctx, p, elem) do { \
	struct drsuapi_DsReplicaAttribute *_a; \
	_a = dsdb_find_object_attr_name(s, r, attr, NULL); \
	if (_a && _a->value_ctr.num_values >= 1 \
	    && _a->value_ctr.values[0].blob) { \
		(p)->elem = *_a->value_ctr.values[0].blob;\
		talloc_steal(mem_ctx, (p)->elem.data); \
	} else { \
		ZERO_STRUCT((p)->elem);\
	}\
} while (0)

WERROR dsdb_attribute_from_drsuapi(struct ldb_context *ldb,
				   struct dsdb_schema *schema,
				   struct drsuapi_DsReplicaObject *r,
				   TALLOC_CTX *mem_ctx,
				   struct dsdb_attribute *attr)
{
	WERROR status;

	GET_STRING_DS(schema, r, "name", mem_ctx, attr, cn, true);
	GET_STRING_DS(schema, r, "lDAPDisplayName", mem_ctx, attr, lDAPDisplayName, true);
	GET_UINT32_DS(schema, r, "attributeID", attr, attributeID_id);
	status = dsdb_map_int2oid(schema, attr->attributeID_id, mem_ctx, &attr->attributeID_oid);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("%s: '%s': unable to map attributeID 0x%08X: %s\n",
			__location__, attr->lDAPDisplayName, attr->attributeID_id,
			win_errstr(status)));
		return status;
	}
	GET_GUID_DS(schema, r, "schemaIDGUID", mem_ctx, attr, schemaIDGUID);
	GET_UINT32_DS(schema, r, "mAPIID", attr, mAPIID);

	GET_GUID_DS(schema, r, "attributeSecurityGUID", mem_ctx, attr, attributeSecurityGUID);

	GET_UINT32_DS(schema, r, "searchFlags", attr, searchFlags);
	GET_UINT32_DS(schema, r, "systemFlags", attr, systemFlags);
	GET_BOOL_DS(schema, r, "isMemberOfPartialAttributeSet", attr, isMemberOfPartialAttributeSet, false);
	GET_UINT32_DS(schema, r, "linkID", attr, linkID);

	GET_UINT32_DS(schema, r, "attributeSyntax", attr, attributeSyntax_id);
	status = dsdb_map_int2oid(schema, attr->attributeSyntax_id, mem_ctx, &attr->attributeSyntax_oid);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("%s: '%s': unable to map attributeSyntax 0x%08X: %s\n",
			__location__, attr->lDAPDisplayName, attr->attributeSyntax_id,
			win_errstr(status)));
		return status;
	}
	GET_UINT32_DS(schema, r, "oMSyntax", attr, oMSyntax);
	GET_BLOB_DS(schema, r, "oMObjectClass", mem_ctx, attr, oMObjectClass);

	GET_BOOL_DS(schema, r, "isSingleValued", attr, isSingleValued, true);
	GET_UINT32_PTR_DS(schema, r, "rangeLower", attr, rangeLower);
	GET_UINT32_PTR_DS(schema, r, "rangeUpper", attr, rangeUpper);
	GET_BOOL_DS(schema, r, "extendedCharsAllowed", attr, extendedCharsAllowed, false);

	GET_UINT32_DS(schema, r, "schemaFlagsEx", attr, schemaFlagsEx);
	GET_BLOB_DS(schema, r, "msDs-Schema-Extensions", mem_ctx, attr, msDs_Schema_Extensions);

	GET_BOOL_DS(schema, r, "showInAdvancedViewOnly", attr, showInAdvancedViewOnly, false);
	GET_STRING_DS(schema, r, "adminDisplayName", mem_ctx, attr, adminDisplayName, false);
	GET_STRING_DS(schema, r, "adminDescription", mem_ctx, attr, adminDescription, false);
	GET_STRING_DS(schema, r, "classDisplayName", mem_ctx, attr, classDisplayName, false);
	GET_BOOL_DS(schema, r, "isEphemeral", attr, isEphemeral, false);
	GET_BOOL_DS(schema, r, "isDefunct", attr, isDefunct, false);
	GET_BOOL_DS(schema, r, "systemOnly", attr, systemOnly, false);

	attr->syntax = dsdb_syntax_for_attribute(attr);
	if (!attr->syntax) {
		return WERR_DS_ATT_SCHEMA_REQ_SYNTAX;
	}

	if (dsdb_schema_setup_ldb_schema_attribute(ldb, attr) != LDB_SUCCESS) {
		return WERR_DS_ATT_SCHEMA_REQ_SYNTAX;
	}

	return WERR_OK;
}

WERROR dsdb_class_from_drsuapi(struct dsdb_schema *schema,
			       struct drsuapi_DsReplicaObject *r,
			       TALLOC_CTX *mem_ctx,
			       struct dsdb_class *obj)
{
	WERROR status;

	GET_STRING_DS(schema, r, "name", mem_ctx, obj, cn, true);
	GET_STRING_DS(schema, r, "lDAPDisplayName", mem_ctx, obj, lDAPDisplayName, true);
	GET_UINT32_DS(schema, r, "governsID", obj, governsID_id);
	status = dsdb_map_int2oid(schema, obj->governsID_id, mem_ctx, &obj->governsID_oid);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("%s: '%s': unable to map governsID 0x%08X: %s\n",
			__location__, obj->lDAPDisplayName, obj->governsID_id,
			win_errstr(status)));
		return status;
	}
	GET_GUID_DS(schema, r, "schemaIDGUID", mem_ctx, obj, schemaIDGUID);

	GET_UINT32_DS(schema, r, "objectClassCategory", obj, objectClassCategory);
	GET_STRING_DS(schema, r, "rDNAttID", mem_ctx, obj, rDNAttID, false);
	GET_DN_DS(schema, r, "defaultObjectCategory", mem_ctx, obj, defaultObjectCategory, true);

	GET_UINT32_DS(schema, r, "subClassOf", obj, subClassOf_id);

	GET_UINT32_LIST_DS(schema, r, "systemAuxiliaryClass", mem_ctx, obj, systemAuxiliaryClass_ids);
	GET_UINT32_LIST_DS(schema, r, "auxiliaryClass", mem_ctx, obj, auxiliaryClass_ids);

	GET_UINT32_LIST_DS(schema, r, "systemMustContain", mem_ctx, obj, systemMustContain_ids);
	GET_UINT32_LIST_DS(schema, r, "systemMayContain", mem_ctx, obj, systemMayContain_ids);
	GET_UINT32_LIST_DS(schema, r, "mustContain", mem_ctx, obj, mustContain_ids);
	GET_UINT32_LIST_DS(schema, r, "mayContain", mem_ctx, obj, mayContain_ids);

	GET_UINT32_LIST_DS(schema, r, "systemPossSuperiors", mem_ctx, obj, systemPossSuperiors_ids);
	GET_UINT32_LIST_DS(schema, r, "possSuperiors", mem_ctx, obj, possSuperiors_ids);

	GET_STRING_DS(schema, r, "defaultSecurityDescriptor", mem_ctx, obj, defaultSecurityDescriptor, false);

	GET_UINT32_DS(schema, r, "schemaFlagsEx", obj, schemaFlagsEx);
	GET_BLOB_DS(schema, r, "msDs-Schema-Extensions", mem_ctx, obj, msDs_Schema_Extensions);

	GET_BOOL_DS(schema, r, "showInAdvancedViewOnly", obj, showInAdvancedViewOnly, false);
	GET_STRING_DS(schema, r, "adminDisplayName", mem_ctx, obj, adminDisplayName, false);
	GET_STRING_DS(schema, r, "adminDescription", mem_ctx, obj, adminDescription, false);
	GET_STRING_DS(schema, r, "classDisplayName", mem_ctx, obj, classDisplayName, false);
	GET_BOOL_DS(schema, r, "defaultHidingValue", obj, defaultHidingValue, false);
	GET_BOOL_DS(schema, r, "isDefunct", obj, isDefunct, false);
	GET_BOOL_DS(schema, r, "systemOnly", obj, systemOnly, false);

	return WERR_OK;
}

