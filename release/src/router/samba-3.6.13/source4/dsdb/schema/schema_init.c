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
#include "dsdb/common/util.h"
#include <ldb_errors.h>
#include "../lib/util/dlinklist.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "param/param.h"
#include <ldb_module.h>
#include "../lib/util/asn1.h"


struct dsdb_schema *dsdb_new_schema(TALLOC_CTX *mem_ctx)
{
	struct dsdb_schema *schema = talloc_zero(mem_ctx, struct dsdb_schema);
	if (!schema) {
		return NULL;
	}

	return schema;
}

struct dsdb_schema *dsdb_schema_copy_shallow(TALLOC_CTX *mem_ctx,
					     struct ldb_context *ldb,
					     const struct dsdb_schema *schema)
{
	int ret;
	struct dsdb_class *cls;
	struct dsdb_attribute *attr;
	struct dsdb_schema *schema_copy;

	schema_copy = dsdb_new_schema(mem_ctx);
	if (!schema_copy) {
		return NULL;
	}

	/* schema base_dn */
	schema_copy->base_dn = ldb_dn_copy(schema_copy, schema->base_dn);
	if (!schema_copy->base_dn) {
		goto failed;
	}

	/* copy prexiMap & schemaInfo */
	schema_copy->prefixmap = dsdb_schema_pfm_copy_shallow(schema_copy,
							      schema->prefixmap);
	if (!schema_copy->prefixmap) {
		goto failed;
	}

	schema_copy->schema_info = talloc_strdup(schema_copy, schema->schema_info);

	/* copy classes and attributes*/
	for (cls = schema->classes; cls; cls = cls->next) {
		struct dsdb_class *class_copy = talloc_memdup(schema_copy,
							      cls, sizeof(*cls));
		if (!class_copy) {
			goto failed;
		}
		DLIST_ADD(schema_copy->classes, class_copy);
	}
	schema_copy->num_classes = schema->num_classes;

	for (attr = schema->attributes; attr; attr = attr->next) {
		struct dsdb_attribute *a_copy = talloc_memdup(schema_copy,
							      attr, sizeof(*attr));
		if (!a_copy) {
			goto failed;
		}
		DLIST_ADD(schema_copy->attributes, a_copy);
	}
	schema_copy->num_attributes = schema->num_attributes;

	/* rebuild indexes */
	ret = dsdb_setup_sorted_accessors(ldb, schema_copy);
	if (ret != LDB_SUCCESS) {
		goto failed;
	}

	/* leave reload_seq_number = 0 so it will be refresh ASAP */
	schema_copy->refresh_fn = schema->refresh_fn;
	schema_copy->loaded_from_module = schema->loaded_from_module;

	return schema_copy;

failed:
	talloc_free(schema_copy);
	return NULL;
}


WERROR dsdb_load_prefixmap_from_drsuapi(struct dsdb_schema *schema,
					const struct drsuapi_DsReplicaOIDMapping_Ctr *ctr)
{
	WERROR werr;
	const char *schema_info;
	struct dsdb_schema_prefixmap *pfm;

	werr = dsdb_schema_pfm_from_drsuapi_pfm(ctr, true, schema, &pfm, &schema_info);
	W_ERROR_NOT_OK_RETURN(werr);

	/* set loaded prefixMap */
	talloc_free(schema->prefixmap);
	schema->prefixmap = pfm;

	talloc_free(discard_const(schema->schema_info));
	schema->schema_info = schema_info;

	return WERR_OK;
}

static WERROR _dsdb_prefixmap_from_ldb_val(const struct ldb_val *pfm_ldb_val,
					   TALLOC_CTX *mem_ctx,
					   struct dsdb_schema_prefixmap **_pfm)
{
	WERROR werr;
	enum ndr_err_code ndr_err;
	struct prefixMapBlob pfm_blob;

	TALLOC_CTX *temp_ctx = talloc_new(mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(temp_ctx);

	ndr_err = ndr_pull_struct_blob(pfm_ldb_val, temp_ctx,
				&pfm_blob,
				(ndr_pull_flags_fn_t)ndr_pull_prefixMapBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(0,("_dsdb_prefixmap_from_ldb_val: Failed to parse prefixmap of length %u: %s\n",
			 (unsigned int)pfm_ldb_val->length, ndr_map_error2string(ndr_err)));
		talloc_free(temp_ctx);
		return ntstatus_to_werror(nt_status);
	}

	if (pfm_blob.version != PREFIX_MAP_VERSION_DSDB) {
		DEBUG(0,("_dsdb_prefixmap_from_ldb_val: pfm_blob->version %u incorrect\n", (unsigned int)pfm_blob.version));
		talloc_free(temp_ctx);
		return WERR_VERSION_PARSE_ERROR;
	}

	/* call the drsuapi version */
	werr = dsdb_schema_pfm_from_drsuapi_pfm(&pfm_blob.ctr.dsdb, false, mem_ctx, _pfm, NULL);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, (__location__ " dsdb_schema_pfm_from_drsuapi_pfm failed: %s\n", win_errstr(werr)));
		talloc_free(temp_ctx);
		return werr;
	}

	talloc_free(temp_ctx);

	return werr;
}

WERROR dsdb_load_oid_mappings_ldb(struct dsdb_schema *schema,
				  const struct ldb_val *prefixMap,
				  const struct ldb_val *schemaInfo)
{
	WERROR werr;
	const char *schema_info;
	struct dsdb_schema_prefixmap *pfm;
	TALLOC_CTX *mem_ctx;

	/* verify schemaInfo blob is valid one */
	if (!dsdb_schema_info_blob_is_valid(schemaInfo)) {
		DEBUG(0,(__location__": dsdb_schema_info_blob_is_valid() failed.\n"));
	        return WERR_INVALID_PARAMETER;
	}

	mem_ctx = talloc_new(schema);
	W_ERROR_HAVE_NO_MEMORY(mem_ctx);

	/* fetch prefixMap */
	werr = _dsdb_prefixmap_from_ldb_val(prefixMap,
					    mem_ctx, &pfm);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0, (__location__ " _dsdb_prefixmap_from_ldb_val failed: %s\n", win_errstr(werr)));
		talloc_free(mem_ctx);
		return werr;
	}

	/* decode schema_info */
	schema_info = hex_encode_talloc(mem_ctx,
					schemaInfo->data,
					schemaInfo->length);
	if (!schema_info) {
		talloc_free(mem_ctx);
		return WERR_NOMEM;
	}

	/* store prefixMap and schema_info into cached Schema */
	talloc_free(schema->prefixmap);
	schema->prefixmap = talloc_steal(schema, pfm);

	talloc_free(discard_const(schema->schema_info));
	schema->schema_info = talloc_steal(schema, schema_info);

	/* clean up locally allocated mem */
	talloc_free(mem_ctx);

	return WERR_OK;
}

WERROR dsdb_get_oid_mappings_drsuapi(const struct dsdb_schema *schema,
				     bool include_schema_info,
				     TALLOC_CTX *mem_ctx,
				     struct drsuapi_DsReplicaOIDMapping_Ctr **_ctr)
{
	return dsdb_drsuapi_pfm_from_schema_pfm(schema->prefixmap,
						include_schema_info ? schema->schema_info : NULL,
						mem_ctx, _ctr);
}

WERROR dsdb_get_drsuapi_prefixmap_as_blob(const struct drsuapi_DsReplicaOIDMapping_Ctr *ctr,
					  TALLOC_CTX *mem_ctx,
					  struct ldb_val *prefixMap)
{
	struct prefixMapBlob pfm;
	enum ndr_err_code ndr_err;
	pfm.version	= PREFIX_MAP_VERSION_DSDB;
	pfm.reserved	= 0;
	pfm.ctr.dsdb	= *ctr;

	ndr_err = ndr_push_struct_blob(prefixMap, mem_ctx, &pfm,
					(ndr_push_flags_fn_t)ndr_push_prefixMapBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		NTSTATUS nt_status = ndr_map_error2ntstatus(ndr_err);
		return ntstatus_to_werror(nt_status);
	}
	return WERR_OK;
}

WERROR dsdb_get_oid_mappings_ldb(const struct dsdb_schema *schema,
				 TALLOC_CTX *mem_ctx,
				 struct ldb_val *prefixMap,
				 struct ldb_val *schemaInfo)
{
	WERROR status;
	struct drsuapi_DsReplicaOIDMapping_Ctr *ctr;

	status = dsdb_get_oid_mappings_drsuapi(schema, false, mem_ctx, &ctr);
	W_ERROR_NOT_OK_RETURN(status);

	status = dsdb_get_drsuapi_prefixmap_as_blob(ctr, mem_ctx, prefixMap);
	talloc_free(ctr);
	W_ERROR_NOT_OK_RETURN(status);

	*schemaInfo = strhex_to_data_blob(mem_ctx, schema->schema_info);
	W_ERROR_HAVE_NO_MEMORY(schemaInfo->data);

	return WERR_OK;
}


/*
 * this function is called from within a ldb transaction from the schema_fsmo module
 */
WERROR dsdb_create_prefix_mapping(struct ldb_context *ldb, struct dsdb_schema *schema, const char *full_oid)
{
	WERROR status;
	uint32_t attid;
	TALLOC_CTX *mem_ctx;
	struct dsdb_schema_prefixmap *pfm;

	mem_ctx = talloc_new(ldb);
	W_ERROR_HAVE_NO_MEMORY(mem_ctx);

	/* Read prefixes from disk*/
	status = dsdb_read_prefixes_from_ldb(ldb, mem_ctx, &pfm);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("dsdb_create_prefix_mapping: dsdb_read_prefixes_from_ldb: %s\n",
			win_errstr(status)));
		talloc_free(mem_ctx);
		return status;
	}

	/* Check if there is a prefix for the oid in the prefixes array*/
	status = dsdb_schema_pfm_find_oid(pfm, full_oid, NULL);
	if (W_ERROR_IS_OK(status)) {
		/* prefix found*/
		talloc_free(mem_ctx);
		return status;
	} else if (!W_ERROR_EQUAL(status, WERR_NOT_FOUND)) {
		/* error */
		DEBUG(0,("dsdb_create_prefix_mapping: dsdb_find_prefix_for_oid: %s\n",
			win_errstr(status)));
		talloc_free(mem_ctx);
		return status;
	}

	/* Create the new mapping for the prefix of full_oid */
	status = dsdb_schema_pfm_make_attid(pfm, full_oid, &attid);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("dsdb_create_prefix_mapping: dsdb_schema_pfm_make_attid: %s\n",
			win_errstr(status)));
		talloc_free(mem_ctx);
		return status;
	}

	talloc_unlink(schema, schema->prefixmap);
	schema->prefixmap = talloc_steal(schema, pfm);

	/* Update prefixMap in ldb*/
	status = dsdb_write_prefixes_from_schema_to_ldb(mem_ctx, ldb, schema);
	if (!W_ERROR_IS_OK(status)) {
		DEBUG(0,("dsdb_create_prefix_mapping: dsdb_write_prefixes_to_ldb: %s\n",
			win_errstr(status)));
		talloc_free(mem_ctx);
		return status;
	}

	DEBUG(2,(__location__ " Added prefixMap %s - now have %u prefixes\n",
		 full_oid, schema->prefixmap->length));

	talloc_free(mem_ctx);
	return status;
}


WERROR dsdb_write_prefixes_from_schema_to_ldb(TALLOC_CTX *mem_ctx, struct ldb_context *ldb,
					      const struct dsdb_schema *schema)
{
	WERROR status;
	int ldb_ret;
	struct ldb_message *msg;
	struct ldb_dn *schema_dn;
	struct prefixMapBlob pfm_blob;
	struct ldb_val ndr_blob;
	enum ndr_err_code ndr_err;
	TALLOC_CTX *temp_ctx;
	struct drsuapi_DsReplicaOIDMapping_Ctr *ctr;

	schema_dn = ldb_get_schema_basedn(ldb);
	if (!schema_dn) {
		DEBUG(0,("dsdb_write_prefixes_from_schema_to_ldb: no schema dn present\n"));
		return WERR_FOOBAR;
	}

	temp_ctx = talloc_new(mem_ctx);
	W_ERROR_HAVE_NO_MEMORY(temp_ctx);

	/* convert schema_prefixMap to prefixMap blob */
	status = dsdb_get_oid_mappings_drsuapi(schema, false, temp_ctx, &ctr);
	if (!W_ERROR_IS_OK(status)) {
		talloc_free(temp_ctx);
		return status;
	}

	pfm_blob.version	= PREFIX_MAP_VERSION_DSDB;
	pfm_blob.ctr.dsdb	= *ctr;

	ndr_err = ndr_push_struct_blob(&ndr_blob, temp_ctx,
				       &pfm_blob,
				       (ndr_push_flags_fn_t)ndr_push_prefixMapBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(temp_ctx);
		return WERR_FOOBAR;
	}
 
	/* write serialized prefixMap into LDB */
	msg = ldb_msg_new(temp_ctx);
	if (!msg) {
		talloc_free(temp_ctx);
		return WERR_NOMEM;
	}

	msg->dn = schema_dn;
	ldb_ret = ldb_msg_add_value(msg, "prefixMap", &ndr_blob, NULL);
	if (ldb_ret != 0) {
		talloc_free(temp_ctx);
		DEBUG(0,("dsdb_write_prefixes_from_schema_to_ldb: ldb_msg_add_value failed\n"));	
		return WERR_NOMEM;
 	}
 
	ldb_ret = dsdb_replace(ldb, msg, DSDB_FLAG_AS_SYSTEM);

	talloc_free(temp_ctx);

	if (ldb_ret != 0) {
		DEBUG(0,("dsdb_write_prefixes_from_schema_to_ldb: dsdb_replace failed\n"));
		return WERR_FOOBAR;
 	}
 
	return WERR_OK;
}

WERROR dsdb_read_prefixes_from_ldb(struct ldb_context *ldb, TALLOC_CTX *mem_ctx, struct dsdb_schema_prefixmap **_pfm)
{
	WERROR werr;
	int ldb_ret;
	const struct ldb_val *prefix_val;
	struct ldb_dn *schema_dn;
	struct ldb_result *schema_res = NULL;
	static const char *schema_attrs[] = {
		"prefixMap",
		NULL
	};

	schema_dn = ldb_get_schema_basedn(ldb);
	if (!schema_dn) {
		DEBUG(0,("dsdb_read_prefixes_from_ldb: no schema dn present\n"));
		return WERR_FOOBAR;
	}

	ldb_ret = ldb_search(ldb, mem_ctx, &schema_res, schema_dn, LDB_SCOPE_BASE, schema_attrs, NULL);
	if (ldb_ret == LDB_ERR_NO_SUCH_OBJECT) {
		DEBUG(0,("dsdb_read_prefixes_from_ldb: no prefix map present\n"));
		talloc_free(schema_res);
		return WERR_FOOBAR;
	} else if (ldb_ret != LDB_SUCCESS) {
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

	werr = _dsdb_prefixmap_from_ldb_val(prefix_val,
					    mem_ctx,
					    _pfm);
	talloc_free(schema_res);
	W_ERROR_NOT_OK_RETURN(werr);

	return WERR_OK;
}

/*
  this will be replaced with something that looks at the right part of
  the schema once we know where unique indexing information is hidden
 */
static bool dsdb_schema_unique_attribute(const char *attr)
{
	const char *attrs[] = { "objectGUID", "objectSid" , NULL };
	unsigned int i;
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
		return ldb_operr(ldb);
	}

	attr->ldb_schema_attribute = a = talloc(attr, struct ldb_schema_attribute);
	if (attr->ldb_schema_attribute == NULL) {
		return ldb_oom(ldb);
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
			d_printf("%s: %s == NULL in %s\n", __location__, attr, ldb_dn_get_linearized(msg->dn)); \
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

#define GET_STRING_LIST_LDB(msg, attr, mem_ctx, p, elem) do {	\
	int get_string_list_counter;					\
	struct ldb_message_element *get_string_list_el = ldb_msg_find_element(msg, attr); \
	/* We may get empty attributes over the replication channel */	\
	if (get_string_list_el == NULL || get_string_list_el->num_values == 0) {				\
		(p)->elem = NULL;					\
		break;							\
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
	str = ldb_msg_find_attr_as_string(msg, attr, NULL);\
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
	(p)->elem = ldb_msg_find_attr_as_uint(msg, attr, 0);\
} while (0)

#define GET_UINT32_PTR_LDB(msg, attr, mem_ctx, p, elem) do {		\
	uint64_t _v = ldb_msg_find_attr_as_uint64(msg, attr, UINT64_MAX);\
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
			       struct dsdb_schema *schema,
			       struct ldb_message *msg)
{
	WERROR status;
	struct dsdb_attribute *attr = talloc_zero(schema, struct dsdb_attribute);
	if (!attr) {
		return WERR_NOMEM;
	}

	GET_STRING_LDB(msg, "cn", attr, attr, cn, false);
	GET_STRING_LDB(msg, "lDAPDisplayName", attr, attr, lDAPDisplayName, true);
	GET_STRING_LDB(msg, "attributeID", attr, attr, attributeID_oid, true);
	if (!schema->prefixmap || schema->prefixmap->length == 0) {
		/* set an invalid value */
		attr->attributeID_id = DRSUAPI_ATTID_INVALID;
	} else {
		status = dsdb_schema_pfm_make_attid(schema->prefixmap,
						    attr->attributeID_oid,
						    &attr->attributeID_id);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0,("%s: '%s': unable to map attributeID %s: %s\n",
				__location__, attr->lDAPDisplayName, attr->attributeID_oid,
				win_errstr(status)));
			return status;
		}
	}
	/* fetch msDS-IntId to be used in resolving ATTRTYP values */
	GET_UINT32_LDB(msg, "msDS-IntId", attr, msDS_IntId);

	GET_GUID_LDB(msg, "schemaIDGUID", attr, schemaIDGUID);
	GET_UINT32_LDB(msg, "mAPIID", attr, mAPIID);

	GET_GUID_LDB(msg, "attributeSecurityGUID", attr, attributeSecurityGUID);

	GET_GUID_LDB(msg, "objectGUID", attr, objectGUID);

	GET_UINT32_LDB(msg, "searchFlags", attr, searchFlags);
	GET_UINT32_LDB(msg, "systemFlags", attr, systemFlags);
	GET_BOOL_LDB(msg, "isMemberOfPartialAttributeSet", attr, isMemberOfPartialAttributeSet, false);
	GET_UINT32_LDB(msg, "linkID", attr, linkID);

	GET_STRING_LDB(msg, "attributeSyntax", attr, attr, attributeSyntax_oid, true);
	if (!schema->prefixmap || schema->prefixmap->length == 0) {
		/* set an invalid value */
		attr->attributeSyntax_id = DRSUAPI_ATTID_INVALID;
	} else {
		status = dsdb_schema_pfm_attid_from_oid(schema->prefixmap,
							attr->attributeSyntax_oid,
							&attr->attributeSyntax_id);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0,("%s: '%s': unable to map attributeSyntax_ %s: %s\n",
				__location__, attr->lDAPDisplayName, attr->attributeSyntax_oid,
				win_errstr(status)));
			return status;
		}
	}
	GET_UINT32_LDB(msg, "oMSyntax", attr, oMSyntax);
	GET_BLOB_LDB(msg, "oMObjectClass", attr, attr, oMObjectClass);

	GET_BOOL_LDB(msg, "isSingleValued", attr, isSingleValued, true);
	GET_UINT32_PTR_LDB(msg, "rangeLower", attr, attr, rangeLower);
	GET_UINT32_PTR_LDB(msg, "rangeUpper", attr, attr, rangeUpper);
	GET_BOOL_LDB(msg, "extendedCharsAllowed", attr, extendedCharsAllowed, false);

	GET_UINT32_LDB(msg, "schemaFlagsEx", attr, schemaFlagsEx);
	GET_BLOB_LDB(msg, "msDs-Schema-Extensions", attr, attr, msDs_Schema_Extensions);

	GET_BOOL_LDB(msg, "showInAdvancedViewOnly", attr, showInAdvancedViewOnly, false);
	GET_STRING_LDB(msg, "adminDisplayName", attr, attr, adminDisplayName, false);
	GET_STRING_LDB(msg, "adminDescription", attr, attr, adminDescription, false);
	GET_STRING_LDB(msg, "classDisplayName", attr, attr, classDisplayName, false);
	GET_BOOL_LDB(msg, "isEphemeral", attr, isEphemeral, false);
	GET_BOOL_LDB(msg, "isDefunct", attr, isDefunct, false);
	GET_BOOL_LDB(msg, "systemOnly", attr, systemOnly, false);

	attr->syntax = dsdb_syntax_for_attribute(attr);
	if (!attr->syntax) {
		DEBUG(0,(__location__ ": Unknown schema syntax for %s\n",
			 attr->lDAPDisplayName));
		return WERR_DS_ATT_SCHEMA_REQ_SYNTAX;
	}

	if (dsdb_schema_setup_ldb_schema_attribute(ldb, attr) != LDB_SUCCESS) {
		DEBUG(0,(__location__ ": Unknown schema syntax for %s - ldb_syntax: %s, ldap_oid: %s\n",
			 attr->lDAPDisplayName,
			 attr->syntax->ldb_syntax,
			 attr->syntax->ldap_oid));
		return WERR_DS_ATT_SCHEMA_REQ_SYNTAX;
	}

	DLIST_ADD(schema->attributes, attr);
	return WERR_OK;
}

WERROR dsdb_class_from_ldb(struct dsdb_schema *schema,
			   struct ldb_message *msg)
{
	WERROR status;
	struct dsdb_class *obj = talloc_zero(schema, struct dsdb_class);
	if (!obj) {
		return WERR_NOMEM;
	}
	GET_STRING_LDB(msg, "cn", obj, obj, cn, false);
	GET_STRING_LDB(msg, "lDAPDisplayName", obj, obj, lDAPDisplayName, true);
	GET_STRING_LDB(msg, "governsID", obj, obj, governsID_oid, true);
	if (!schema->prefixmap || schema->prefixmap->length == 0) {
		/* set an invalid value */
		obj->governsID_id = DRSUAPI_ATTID_INVALID;
	} else {
		status = dsdb_schema_pfm_make_attid(schema->prefixmap,
						    obj->governsID_oid,
						    &obj->governsID_id);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0,("%s: '%s': unable to map governsID %s: %s\n",
				__location__, obj->lDAPDisplayName, obj->governsID_oid,
				win_errstr(status)));
			return status;
		}
	}
	GET_GUID_LDB(msg, "schemaIDGUID", obj, schemaIDGUID);
	GET_GUID_LDB(msg, "objectGUID", obj, objectGUID);

	GET_UINT32_LDB(msg, "objectClassCategory", obj, objectClassCategory);
	GET_STRING_LDB(msg, "rDNAttID", obj, obj, rDNAttID, false);
	GET_STRING_LDB(msg, "defaultObjectCategory", obj, obj, defaultObjectCategory, true);
 
	GET_STRING_LDB(msg, "subClassOf", obj, obj, subClassOf, true);

	GET_STRING_LIST_LDB(msg, "systemAuxiliaryClass", obj, obj, systemAuxiliaryClass);
	GET_STRING_LIST_LDB(msg, "auxiliaryClass", obj, obj, auxiliaryClass);

	GET_STRING_LIST_LDB(msg, "systemMustContain", obj, obj, systemMustContain);
	GET_STRING_LIST_LDB(msg, "systemMayContain", obj, obj, systemMayContain);
	GET_STRING_LIST_LDB(msg, "mustContain", obj, obj, mustContain);
	GET_STRING_LIST_LDB(msg, "mayContain", obj, obj, mayContain);

	GET_STRING_LIST_LDB(msg, "systemPossSuperiors", obj, obj, systemPossSuperiors);
	GET_STRING_LIST_LDB(msg, "possSuperiors", obj, obj, possSuperiors);

	GET_STRING_LDB(msg, "defaultSecurityDescriptor", obj, obj, defaultSecurityDescriptor, false);

	GET_UINT32_LDB(msg, "schemaFlagsEx", obj, schemaFlagsEx);
	GET_BLOB_LDB(msg, "msDs-Schema-Extensions", obj, obj, msDs_Schema_Extensions);

	GET_BOOL_LDB(msg, "showInAdvancedViewOnly", obj, showInAdvancedViewOnly, false);
	GET_STRING_LDB(msg, "adminDisplayName", obj, obj, adminDisplayName, false);
	GET_STRING_LDB(msg, "adminDescription", obj, obj, adminDescription, false);
	GET_STRING_LDB(msg, "classDisplayName", obj, obj, classDisplayName, false);
	GET_BOOL_LDB(msg, "defaultHidingValue", obj, defaultHidingValue, false);
	GET_BOOL_LDB(msg, "isDefunct", obj, isDefunct, false);
	GET_BOOL_LDB(msg, "systemOnly", obj, systemOnly, false);

	DLIST_ADD(schema->classes, obj);
	return WERR_OK;
}

#define dsdb_oom(error_string, mem_ctx) *error_string = talloc_asprintf(mem_ctx, "dsdb out of memory at %s:%d\n", __FILE__, __LINE__)

/* 
 Create a DSDB schema from the ldb results provided.  This is called
 directly when the schema is provisioned from an on-disk LDIF file, or
 from dsdb_schema_from_schema_dn in schema_fsmo
*/

int dsdb_schema_from_ldb_results(TALLOC_CTX *mem_ctx, struct ldb_context *ldb,
				 struct ldb_result *schema_res,
				 struct ldb_result *attrs_res, struct ldb_result *objectclass_res, 
				 struct dsdb_schema **schema_out,
				 char **error_string)
{
	WERROR status;
	unsigned int i;
	const struct ldb_val *prefix_val;
	const struct ldb_val *info_val;
	struct ldb_val info_val_default;
	struct dsdb_schema *schema;

	schema = dsdb_new_schema(mem_ctx);
	if (!schema) {
		dsdb_oom(error_string, mem_ctx);
		return ldb_operr(ldb);
	}

	schema->base_dn = talloc_steal(schema, schema_res->msgs[0]->dn);

	prefix_val = ldb_msg_find_ldb_val(schema_res->msgs[0], "prefixMap");
	if (!prefix_val) {
		*error_string = talloc_asprintf(mem_ctx, 
						"schema_fsmo_init: no prefixMap attribute found");
		DEBUG(0,(__location__ ": %s\n", *error_string));
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	info_val = ldb_msg_find_ldb_val(schema_res->msgs[0], "schemaInfo");
	if (!info_val) {
		status = dsdb_schema_info_blob_new(mem_ctx, &info_val_default);
		if (!W_ERROR_IS_OK(status)) {
			*error_string = talloc_asprintf(mem_ctx,
			                                "schema_fsmo_init: dsdb_schema_info_blob_new() failed - %s",
			                                win_errstr(status));
			DEBUG(0,(__location__ ": %s\n", *error_string));
			return ldb_operr(ldb);
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
		status = dsdb_attribute_from_ldb(ldb, schema, attrs_res->msgs[i]);
		if (!W_ERROR_IS_OK(status)) {
			*error_string = talloc_asprintf(mem_ctx, 
				      "schema_fsmo_init: failed to load attribute definition: %s:%s",
				      ldb_dn_get_linearized(attrs_res->msgs[i]->dn),
				      win_errstr(status));
			DEBUG(0,(__location__ ": %s\n", *error_string));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
	}

	for (i=0; i < objectclass_res->count; i++) {
		status = dsdb_class_from_ldb(schema, objectclass_res->msgs[i]);
		if (!W_ERROR_IS_OK(status)) {
			*error_string = talloc_asprintf(mem_ctx, 
				      "schema_fsmo_init: failed to load class definition: %s:%s",
				      ldb_dn_get_linearized(objectclass_res->msgs[i]->dn),
				      win_errstr(status));
			DEBUG(0,(__location__ ": %s\n", *error_string));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
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
