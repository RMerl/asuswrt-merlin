/* 
   ldb database library - ldif handlers for Samba

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Andrew Bartlett 2006-2007
   Copyright (C) Matthias Dieter Wallnöfer 2009
     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "lib/ldb/include/ldb.h"
#include "lib/ldb/include/ldb_module.h"
#include "ldb_handlers.h"
#include "dsdb/samdb/samdb.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include "librpc/gen_ndr/ndr_drsblobs.h"
#include "librpc/ndr/libndr.h"
#include "libcli/security/security.h"
#include "param/param.h"

/*
  use ndr_print_* to convert a NDR formatted blob to a ldif formatted blob
*/
static int ldif_write_NDR(struct ldb_context *ldb, void *mem_ctx,
			  const struct ldb_val *in, struct ldb_val *out,
			  size_t struct_size,
			  ndr_pull_flags_fn_t pull_fn,
			  ndr_print_fn_t print_fn)
{
	uint8_t *p;
	enum ndr_err_code err;
	if (!(ldb_get_flags(ldb) & LDB_FLG_SHOW_BINARY)) {
		return ldb_handler_copy(ldb, mem_ctx, in, out);
	}
	p = talloc_size(mem_ctx, struct_size);
	err = ndr_pull_struct_blob(in, mem_ctx, 
				   lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
				   p, pull_fn);
	if (err != NDR_ERR_SUCCESS) {
		talloc_free(p);
		out->data = (uint8_t *)talloc_strdup(mem_ctx, "<Unable to decode binary data>");
		out->length = strlen((const char *)out->data);
		return 0;
	}
	out->data = (uint8_t *)ndr_print_struct_string(mem_ctx, print_fn, "NDR", p);
	talloc_free(p);
	if (out->data == NULL) {
		return ldb_handler_copy(ldb, mem_ctx, in, out);		
	}
	out->length = strlen((char *)out->data);
	return 0;
}

/*
  convert a ldif formatted objectSid to a NDR formatted blob
*/
static int ldif_read_objectSid(struct ldb_context *ldb, void *mem_ctx,
			       const struct ldb_val *in, struct ldb_val *out)
{
	enum ndr_err_code ndr_err;
	struct dom_sid *sid;
	sid = dom_sid_parse_length(mem_ctx, in);
	if (sid == NULL) {
		return -1;
	}
	ndr_err = ndr_push_struct_blob(out, mem_ctx, NULL, sid,
				       (ndr_push_flags_fn_t)ndr_push_dom_sid);
	talloc_free(sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return -1;
	}
	return 0;
}

/*
  convert a NDR formatted blob to a ldif formatted objectSid
*/
static int ldif_write_objectSid(struct ldb_context *ldb, void *mem_ctx,
				const struct ldb_val *in, struct ldb_val *out)
{
	struct dom_sid *sid;
	enum ndr_err_code ndr_err;

	sid = talloc(mem_ctx, struct dom_sid);
	if (sid == NULL) {
		return -1;
	}
	ndr_err = ndr_pull_struct_blob_all(in, sid, NULL, sid,
					   (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(sid);
		return -1;
	}
	*out = data_blob_string_const(dom_sid_string(mem_ctx, sid));
	talloc_free(sid);
	if (out->data == NULL) {
		return -1;
	}
	return 0;
}

static bool ldif_comparision_objectSid_isString(const struct ldb_val *v)
{
	if (v->length < 3) {
		return false;
	}

	if (strncmp("S-", (const char *)v->data, 2) != 0) return false;
	
	return true;
}

/*
  compare two objectSids
*/
static int ldif_comparison_objectSid(struct ldb_context *ldb, void *mem_ctx,
				    const struct ldb_val *v1, const struct ldb_val *v2)
{
	if (ldif_comparision_objectSid_isString(v1) && ldif_comparision_objectSid_isString(v2)) {
		return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
	} else if (ldif_comparision_objectSid_isString(v1)
		   && !ldif_comparision_objectSid_isString(v2)) {
		struct ldb_val v;
		int ret;
		if (ldif_read_objectSid(ldb, mem_ctx, v1, &v) != 0) {
			/* Perhaps not a string after all */
			return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
		}
		ret = ldb_comparison_binary(ldb, mem_ctx, &v, v2);
		talloc_free(v.data);
		return ret;
	} else if (!ldif_comparision_objectSid_isString(v1)
		   && ldif_comparision_objectSid_isString(v2)) {
		struct ldb_val v;
		int ret;
		if (ldif_read_objectSid(ldb, mem_ctx, v2, &v) != 0) {
			/* Perhaps not a string after all */
			return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
		}
		ret = ldb_comparison_binary(ldb, mem_ctx, v1, &v);
		talloc_free(v.data);
		return ret;
	}
	return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
}

/*
  canonicalise a objectSid
*/
static int ldif_canonicalise_objectSid(struct ldb_context *ldb, void *mem_ctx,
				      const struct ldb_val *in, struct ldb_val *out)
{
	if (ldif_comparision_objectSid_isString(in)) {
		if (ldif_read_objectSid(ldb, mem_ctx, in, out) != 0) {
			/* Perhaps not a string after all */
			return ldb_handler_copy(ldb, mem_ctx, in, out);
		}
		return 0;
	}
	return ldb_handler_copy(ldb, mem_ctx, in, out);
}

static int extended_dn_read_SID(struct ldb_context *ldb, void *mem_ctx,
			      const struct ldb_val *in, struct ldb_val *out)
{
	struct dom_sid sid;
	enum ndr_err_code ndr_err;
	if (ldif_comparision_objectSid_isString(in)) {
		if (ldif_read_objectSid(ldb, mem_ctx, in, out) == 0) {
			return 0;
		}
	}
	
	/* Perhaps not a string after all */
	*out = data_blob_talloc(mem_ctx, NULL, in->length/2+1);

	if (!out->data) {
		return -1;
	}

	(*out).length = strhex_to_str((char *)out->data, out->length,
				     (const char *)in->data, in->length);

	/* Check it looks like a SID */
	ndr_err = ndr_pull_struct_blob_all(out, mem_ctx, NULL, &sid,
					   (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return -1;
	}
	return 0;
}

/*
  convert a ldif formatted objectGUID to a NDR formatted blob
*/
static int ldif_read_objectGUID(struct ldb_context *ldb, void *mem_ctx,
			        const struct ldb_val *in, struct ldb_val *out)
{
	struct GUID guid;
	NTSTATUS status;
	enum ndr_err_code ndr_err;

	status = GUID_from_data_blob(in, &guid);
	if (!NT_STATUS_IS_OK(status)) {
		return -1;
	}

	ndr_err = ndr_push_struct_blob(out, mem_ctx, NULL, &guid,
				       (ndr_push_flags_fn_t)ndr_push_GUID);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return -1;
	}
	return 0;
}

/*
  convert a NDR formatted blob to a ldif formatted objectGUID
*/
static int ldif_write_objectGUID(struct ldb_context *ldb, void *mem_ctx,
				 const struct ldb_val *in, struct ldb_val *out)
{
	struct GUID guid;
	enum ndr_err_code ndr_err;
	ndr_err = ndr_pull_struct_blob_all(in, mem_ctx, NULL, &guid,
					   (ndr_pull_flags_fn_t)ndr_pull_GUID);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return -1;
	}
	out->data = (uint8_t *)GUID_string(mem_ctx, &guid);
	if (out->data == NULL) {
		return -1;
	}
	out->length = strlen((const char *)out->data);
	return 0;
}

static bool ldif_comparision_objectGUID_isString(const struct ldb_val *v)
{
	if (v->length != 36 && v->length != 38) return false;

	/* Might be a GUID string, can't be a binary GUID (fixed 16 bytes) */
	return true;
}

static int extended_dn_read_GUID(struct ldb_context *ldb, void *mem_ctx,
			      const struct ldb_val *in, struct ldb_val *out)
{
	struct GUID guid;
	enum ndr_err_code ndr_err;
	if (in->length == 36 && ldif_read_objectGUID(ldb, mem_ctx, in, out) == 0) {
		return 0;
	}

	/* Try as 'hex' form */
	if (in->length != 32) {
		return -1;
	}
		
	*out = data_blob_talloc(mem_ctx, NULL, in->length/2+1);
	
	if (!out->data) {
		return -1;
	}
	
	(*out).length = strhex_to_str((char *)out->data, out->length,
				      (const char *)in->data, in->length);
	
	/* Check it looks like a GUID */
	ndr_err = ndr_pull_struct_blob_all(out, mem_ctx, NULL, &guid,
					   (ndr_pull_flags_fn_t)ndr_pull_GUID);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return -1;
	}
	return 0;
}

/*
  compare two objectGUIDs
*/
static int ldif_comparison_objectGUID(struct ldb_context *ldb, void *mem_ctx,
				     const struct ldb_val *v1, const struct ldb_val *v2)
{
	if (ldif_comparision_objectGUID_isString(v1) && ldif_comparision_objectGUID_isString(v2)) {
		return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
	} else if (ldif_comparision_objectGUID_isString(v1)
		   && !ldif_comparision_objectGUID_isString(v2)) {
		struct ldb_val v;
		int ret;
		if (ldif_read_objectGUID(ldb, mem_ctx, v1, &v) != 0) {
			/* Perhaps it wasn't a valid string after all */
			return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
		}
		ret = ldb_comparison_binary(ldb, mem_ctx, &v, v2);
		talloc_free(v.data);
		return ret;
	} else if (!ldif_comparision_objectGUID_isString(v1)
		   && ldif_comparision_objectGUID_isString(v2)) {
		struct ldb_val v;
		int ret;
		if (ldif_read_objectGUID(ldb, mem_ctx, v2, &v) != 0) {
			/* Perhaps it wasn't a valid string after all */
			return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
		}
		ret = ldb_comparison_binary(ldb, mem_ctx, v1, &v);
		talloc_free(v.data);
		return ret;
	}
	return ldb_comparison_binary(ldb, mem_ctx, v1, v2);
}

/*
  canonicalise a objectGUID
*/
static int ldif_canonicalise_objectGUID(struct ldb_context *ldb, void *mem_ctx,
				       const struct ldb_val *in, struct ldb_val *out)
{
	if (ldif_comparision_objectGUID_isString(in)) {
		if (ldif_read_objectGUID(ldb, mem_ctx, in, out) != 0) {
			/* Perhaps it wasn't a valid string after all */
			return ldb_handler_copy(ldb, mem_ctx, in, out);
		}
		return 0;
	}
	return ldb_handler_copy(ldb, mem_ctx, in, out);
}


/*
  convert a ldif (SDDL) formatted ntSecurityDescriptor to a NDR formatted blob
*/
static int ldif_read_ntSecurityDescriptor(struct ldb_context *ldb, void *mem_ctx,
					  const struct ldb_val *in, struct ldb_val *out)
{
	struct security_descriptor *sd;
	enum ndr_err_code ndr_err;

	sd = talloc(mem_ctx, struct security_descriptor);
	if (sd == NULL) {
		return -1;
	}

	ndr_err = ndr_pull_struct_blob(in, sd, NULL, sd,
				       (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		/* If this does not parse, then it is probably SDDL, and we should try it that way */
		
		const struct dom_sid *sid = samdb_domain_sid(ldb);
		talloc_free(sd);
		sd = sddl_decode(mem_ctx, (const char *)in->data, sid);
		if (sd == NULL) {
			return -1;
		}
	}

	ndr_err = ndr_push_struct_blob(out, mem_ctx, NULL, sd,
				       (ndr_push_flags_fn_t)ndr_push_security_descriptor);
	talloc_free(sd);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return -1;
	}

	return 0;
}

/*
  convert a NDR formatted blob to a ldif formatted ntSecurityDescriptor (SDDL format)
*/
static int ldif_write_ntSecurityDescriptor(struct ldb_context *ldb, void *mem_ctx,
					   const struct ldb_val *in, struct ldb_val *out)
{
	struct security_descriptor *sd;
	enum ndr_err_code ndr_err;

	if (ldb_get_flags(ldb) & LDB_FLG_SHOW_BINARY) {
		return ldif_write_NDR(ldb, mem_ctx, in, out, 
				      sizeof(struct security_descriptor),
				      (ndr_pull_flags_fn_t)ndr_pull_security_descriptor,
				      (ndr_print_fn_t)ndr_print_security_descriptor);
				      
	}

	sd = talloc(mem_ctx, struct security_descriptor);
	if (sd == NULL) {
		return -1;
	}
	/* We can't use ndr_pull_struct_blob_all because this contains relative pointers */
	ndr_err = ndr_pull_struct_blob(in, sd, NULL, sd,
					   (ndr_pull_flags_fn_t)ndr_pull_security_descriptor);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(sd);
		return -1;
	}
	out->data = (uint8_t *)sddl_encode(mem_ctx, sd, NULL);
	talloc_free(sd);
	if (out->data == NULL) {
		return -1;
	}
	out->length = strlen((const char *)out->data);
	return 0;
}

/* 
   canonicalise an objectCategory.  We use the short form as the cannoical form:
   cn=Person,cn=Schema,cn=Configuration,<basedn> becomes 'person'
*/

static int ldif_canonicalise_objectCategory(struct ldb_context *ldb, void *mem_ctx,
					    const struct ldb_val *in, struct ldb_val *out)
{
	struct ldb_dn *dn1 = NULL;
	const struct dsdb_schema *schema = dsdb_get_schema(ldb);
	const struct dsdb_class *sclass;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!tmp_ctx) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (!schema) {
		talloc_free(tmp_ctx);
		*out = data_blob_talloc(mem_ctx, in->data, in->length);
		if (in->data && !out->data) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
		return LDB_SUCCESS;
	}
	dn1 = ldb_dn_from_ldb_val(tmp_ctx, ldb, in);
	if ( ! ldb_dn_validate(dn1)) {
		const char *lDAPDisplayName = talloc_strndup(tmp_ctx, (char *)in->data, in->length);
		sclass = dsdb_class_by_lDAPDisplayName(schema, lDAPDisplayName);
		if (sclass) {
			struct ldb_dn *dn = ldb_dn_new(mem_ctx, ldb,  
						       sclass->defaultObjectCategory);
			*out = data_blob_string_const(ldb_dn_alloc_casefold(mem_ctx, dn));
			talloc_free(tmp_ctx);

			if (!out->data) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
			return LDB_SUCCESS;
		} else {
			*out = data_blob_talloc(mem_ctx, in->data, in->length);
			talloc_free(tmp_ctx);

			if (in->data && !out->data) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
			return LDB_SUCCESS;
		}
	}
	*out = data_blob_string_const(ldb_dn_alloc_casefold(mem_ctx, dn1));
	talloc_free(tmp_ctx);

	if (!out->data) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	return LDB_SUCCESS;
}

static int ldif_comparison_objectCategory(struct ldb_context *ldb, void *mem_ctx,
					  const struct ldb_val *v1,
					  const struct ldb_val *v2)
{

	int ret, ret1, ret2;
	struct ldb_val v1_canon, v2_canon;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	/* I could try and bail if tmp_ctx was NULL, but what return
	 * value would I use?
	 *
	 * It seems easier to continue on the NULL context 
	 */
	ret1 = ldif_canonicalise_objectCategory(ldb, tmp_ctx, v1, &v1_canon);
	ret2 = ldif_canonicalise_objectCategory(ldb, tmp_ctx, v2, &v2_canon);

	if (ret1 == LDB_SUCCESS && ret2 == LDB_SUCCESS) {
		ret = data_blob_cmp(&v1_canon, &v2_canon);
	} else {
		ret = data_blob_cmp(v1, v2);
	}
	talloc_free(tmp_ctx);
	return ret;
}

/*
  convert a ldif formatted prefixMap to a NDR formatted blob
*/
static int ldif_read_prefixMap(struct ldb_context *ldb, void *mem_ctx,
			       const struct ldb_val *in, struct ldb_val *out)
{
	struct prefixMapBlob *blob;
	enum ndr_err_code ndr_err;
	char *string, *line, *p, *oid;

	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	if (tmp_ctx == NULL) {
		return -1;
	}

	blob = talloc_zero(tmp_ctx, struct prefixMapBlob);
	if (blob == NULL) {
		talloc_free(blob);
		return -1;
	}

	blob->version = PREFIX_MAP_VERSION_DSDB;
	
	string = talloc_strndup(mem_ctx, (const char *)in->data, in->length);
	if (string == NULL) {
		talloc_free(blob);
		return -1;
	}

	line = string;
	while (line && line[0]) {
		p=strchr(line, ';');
		if (p) {
			p[0] = '\0';
		} else {
			p=strchr(line, '\n');
			if (p) {
				p[0] = '\0';
			}
		}
		/* allow a traling seperator */
		if (line == p) {
			break;
		}
		
		blob->ctr.dsdb.mappings = talloc_realloc(blob, 
							 blob->ctr.dsdb.mappings, 
							 struct drsuapi_DsReplicaOIDMapping,
							 blob->ctr.dsdb.num_mappings+1);
		if (!blob->ctr.dsdb.mappings) {
			talloc_free(tmp_ctx);
			return -1;
		}

		blob->ctr.dsdb.mappings[blob->ctr.dsdb.num_mappings].id_prefix = strtoul(line, &oid, 10);

		if (oid[0] != ':') {
			talloc_free(tmp_ctx);
			return -1;
		}

		/* we know there must be at least ":" */
		oid++;

		blob->ctr.dsdb.mappings[blob->ctr.dsdb.num_mappings].oid.oid
			= talloc_strdup(blob->ctr.dsdb.mappings, oid);

		blob->ctr.dsdb.num_mappings++;

		/* Now look past the terminator we added above */
		if (p) {
			line = p + 1;
		} else {
			line = NULL;
		}
	}

	ndr_err = ndr_push_struct_blob(out, mem_ctx, 
				       lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
				       blob,
				       (ndr_push_flags_fn_t)ndr_push_prefixMapBlob);
	talloc_free(tmp_ctx);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		return -1;
	}
	return 0;
}

/*
  convert a NDR formatted blob to a ldif formatted prefixMap
*/
static int ldif_write_prefixMap(struct ldb_context *ldb, void *mem_ctx,
				const struct ldb_val *in, struct ldb_val *out)
{
	struct prefixMapBlob *blob;
	enum ndr_err_code ndr_err;
	char *string;
	uint32_t i;

	if (ldb_get_flags(ldb) & LDB_FLG_SHOW_BINARY) {
		return ldif_write_NDR(ldb, mem_ctx, in, out, 
				      sizeof(struct prefixMapBlob),
				      (ndr_pull_flags_fn_t)ndr_pull_prefixMapBlob,
				      (ndr_print_fn_t)ndr_print_prefixMapBlob);
				      
	}

	blob = talloc(mem_ctx, struct prefixMapBlob);
	if (blob == NULL) {
		return -1;
	}
	ndr_err = ndr_pull_struct_blob_all(in, blob, 
					   lp_iconv_convenience(ldb_get_opaque(ldb, "loadparm")), 
					   blob,
					   (ndr_pull_flags_fn_t)ndr_pull_prefixMapBlob);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(blob);
		return -1;
	}
	if (blob->version != PREFIX_MAP_VERSION_DSDB) {
		return -1;
	}
	string = talloc_strdup(mem_ctx, "");
	if (string == NULL) {
		return -1;
	}

	for (i=0; i < blob->ctr.dsdb.num_mappings; i++) {
		if (i > 0) {
			string = talloc_asprintf_append(string, ";"); 
		}
		string = talloc_asprintf_append(string, "%u:%s", 
						   blob->ctr.dsdb.mappings[i].id_prefix,
						   blob->ctr.dsdb.mappings[i].oid.oid);
		if (string == NULL) {
			return -1;
		}
	}

	talloc_free(blob);
	*out = data_blob_string_const(string);
	return 0;
}

static bool ldif_comparision_prefixMap_isString(const struct ldb_val *v)
{
	if (v->length < 4) {
		return true;
	}

	if (IVAL(v->data, 0) == PREFIX_MAP_VERSION_DSDB) {
		return false;
	}
	
	return true;
}

/*
  canonicalise a prefixMap
*/
static int ldif_canonicalise_prefixMap(struct ldb_context *ldb, void *mem_ctx,
				       const struct ldb_val *in, struct ldb_val *out)
{
	if (ldif_comparision_prefixMap_isString(in)) {
		return ldif_read_prefixMap(ldb, mem_ctx, in, out);
	}
	return ldb_handler_copy(ldb, mem_ctx, in, out);
}

static int ldif_comparison_prefixMap(struct ldb_context *ldb, void *mem_ctx,
				     const struct ldb_val *v1,
				     const struct ldb_val *v2)
{

	int ret, ret1, ret2;
	struct ldb_val v1_canon, v2_canon;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);

	/* I could try and bail if tmp_ctx was NULL, but what return
	 * value would I use?
	 *
	 * It seems easier to continue on the NULL context 
	 */
	ret1 = ldif_canonicalise_prefixMap(ldb, tmp_ctx, v1, &v1_canon);
	ret2 = ldif_canonicalise_prefixMap(ldb, tmp_ctx, v2, &v2_canon);

	if (ret1 == LDB_SUCCESS && ret2 == LDB_SUCCESS) {
		ret = data_blob_cmp(&v1_canon, &v2_canon);
	} else {
		ret = data_blob_cmp(v1, v2);
	}
	talloc_free(tmp_ctx);
	return ret;
}

/* Canonicalisation of two 32-bit integers */
static int ldif_canonicalise_int32(struct ldb_context *ldb, void *mem_ctx,
			const struct ldb_val *in, struct ldb_val *out)
{
	char *end;
	/* We've to use "strtoll" here to have the intended overflows.
	 * Otherwise we may get "LONG_MAX" and the conversion is wrong. */
	int32_t i = (int32_t) strtoll((char *)in->data, &end, 0);
	if (*end != 0) {
		return -1;
	}
	out->data = (uint8_t *) talloc_asprintf(mem_ctx, "%d", i);
	if (out->data == NULL) {
		return -1;
	}
	out->length = strlen((char *)out->data);
	return 0;
}

/* Comparison of two 32-bit integers */
static int ldif_comparison_int32(struct ldb_context *ldb, void *mem_ctx,
			const struct ldb_val *v1, const struct ldb_val *v2)
{
	/* We've to use "strtoll" here to have the intended overflows.
	 * Otherwise we may get "LONG_MAX" and the conversion is wrong. */
	return (int32_t) strtoll((char *)v1->data, NULL, 0)
	 - (int32_t) strtoll((char *)v2->data, NULL, 0);
}

/*
  convert a NDR formatted blob to a ldif formatted repsFromTo
*/
static int ldif_write_repsFromTo(struct ldb_context *ldb, void *mem_ctx,
				 const struct ldb_val *in, struct ldb_val *out)
{
	return ldif_write_NDR(ldb, mem_ctx, in, out, 
			      sizeof(struct repsFromToBlob),
			      (ndr_pull_flags_fn_t)ndr_pull_repsFromToBlob,
			      (ndr_print_fn_t)ndr_print_repsFromToBlob);
}

/*
  convert a NDR formatted blob to a ldif formatted replPropertyMetaData
*/
static int ldif_write_replPropertyMetaData(struct ldb_context *ldb, void *mem_ctx,
					   const struct ldb_val *in, struct ldb_val *out)
{
	return ldif_write_NDR(ldb, mem_ctx, in, out, 
			      sizeof(struct replPropertyMetaDataBlob),
			      (ndr_pull_flags_fn_t)ndr_pull_replPropertyMetaDataBlob,
			      (ndr_print_fn_t)ndr_print_replPropertyMetaDataBlob);
}

/*
  convert a NDR formatted blob to a ldif formatted replUpToDateVector
*/
static int ldif_write_replUpToDateVector(struct ldb_context *ldb, void *mem_ctx,
					 const struct ldb_val *in, struct ldb_val *out)
{
	return ldif_write_NDR(ldb, mem_ctx, in, out, 
			      sizeof(struct replUpToDateVectorBlob),
			      (ndr_pull_flags_fn_t)ndr_pull_replUpToDateVectorBlob,
			      (ndr_print_fn_t)ndr_print_replPropertyMetaDataBlob);
}


static int extended_dn_write_hex(struct ldb_context *ldb, void *mem_ctx,
				 const struct ldb_val *in, struct ldb_val *out)
{
	*out = data_blob_string_const(data_blob_hex_string(mem_ctx, in));
	if (!out->data) {
		return -1;
	}
	return 0;
}

static const struct ldb_schema_syntax samba_syntaxes[] = {
	{
		.name		  = LDB_SYNTAX_SAMBA_SID,
		.ldif_read_fn	  = ldif_read_objectSid,
		.ldif_write_fn	  = ldif_write_objectSid,
		.canonicalise_fn  = ldif_canonicalise_objectSid,
		.comparison_fn	  = ldif_comparison_objectSid
	},{
		.name		  = LDB_SYNTAX_SAMBA_SECURITY_DESCRIPTOR,
		.ldif_read_fn	  = ldif_read_ntSecurityDescriptor,
		.ldif_write_fn	  = ldif_write_ntSecurityDescriptor,
		.canonicalise_fn  = ldb_handler_copy,
		.comparison_fn	  = ldb_comparison_binary
	},{
		.name		  = LDB_SYNTAX_SAMBA_GUID,
		.ldif_read_fn	  = ldif_read_objectGUID,
		.ldif_write_fn	  = ldif_write_objectGUID,
		.canonicalise_fn  = ldif_canonicalise_objectGUID,
		.comparison_fn	  = ldif_comparison_objectGUID
	},{
		.name		  = LDB_SYNTAX_SAMBA_OBJECT_CATEGORY,
		.ldif_read_fn	  = ldb_handler_copy,
		.ldif_write_fn	  = ldb_handler_copy,
		.canonicalise_fn  = ldif_canonicalise_objectCategory,
		.comparison_fn	  = ldif_comparison_objectCategory
	},{
		.name		  = LDB_SYNTAX_SAMBA_PREFIX_MAP,
		.ldif_read_fn	  = ldif_read_prefixMap,
		.ldif_write_fn	  = ldif_write_prefixMap,
		.canonicalise_fn  = ldif_canonicalise_prefixMap,
		.comparison_fn	  = ldif_comparison_prefixMap
	},{
		.name		  = LDB_SYNTAX_SAMBA_INT32,
		.ldif_read_fn	  = ldb_handler_copy,
		.ldif_write_fn	  = ldb_handler_copy,
		.canonicalise_fn  = ldif_canonicalise_int32,
		.comparison_fn	  = ldif_comparison_int32
	},{
		.name		  = LDB_SYNTAX_SAMBA_REPSFROMTO,
		.ldif_read_fn	  = ldb_handler_copy,
		.ldif_write_fn	  = ldif_write_repsFromTo,
		.canonicalise_fn  = ldb_handler_copy,
		.comparison_fn	  = ldb_comparison_binary
	},{
		.name		  = LDB_SYNTAX_SAMBA_REPLPROPERTYMETADATA,
		.ldif_read_fn	  = ldb_handler_copy,
		.ldif_write_fn	  = ldif_write_replPropertyMetaData,
		.canonicalise_fn  = ldb_handler_copy,
		.comparison_fn	  = ldb_comparison_binary
	},{
		.name		  = LDB_SYNTAX_SAMBA_REPLUPTODATEVECTOR,
		.ldif_read_fn	  = ldb_handler_copy,
		.ldif_write_fn	  = ldif_write_replUpToDateVector,
		.canonicalise_fn  = ldb_handler_copy,
		.comparison_fn	  = ldb_comparison_binary
	},
};

static const struct ldb_dn_extended_syntax samba_dn_syntax[] = {
	{
		.name		  = "SID",
		.read_fn          = extended_dn_read_SID,
		.write_clear_fn   = ldif_write_objectSid,
		.write_hex_fn     = extended_dn_write_hex
	},{
		.name		  = "GUID",
		.read_fn          = extended_dn_read_GUID,
		.write_clear_fn   = ldif_write_objectGUID,
		.write_hex_fn     = extended_dn_write_hex
	},{
		.name		  = "WKGUID",
		.read_fn          = ldb_handler_copy,
		.write_clear_fn   = ldb_handler_copy,
		.write_hex_fn     = ldb_handler_copy
	}
};

/* TODO: Should be dynamic at some point */
static const struct {
	const char *name;
	const char *syntax;
} samba_attributes[] = {
	{ "objectSid",			LDB_SYNTAX_SAMBA_SID },
	{ "securityIdentifier", 	LDB_SYNTAX_SAMBA_SID },
	{ "ntSecurityDescriptor",	LDB_SYNTAX_SAMBA_SECURITY_DESCRIPTOR },
	{ "objectGUID",			LDB_SYNTAX_SAMBA_GUID },
	{ "invocationId",		LDB_SYNTAX_SAMBA_GUID },
	{ "schemaIDGUID",		LDB_SYNTAX_SAMBA_GUID },
	{ "attributeSecurityGUID",	LDB_SYNTAX_SAMBA_GUID },
	{ "parentGUID",			LDB_SYNTAX_SAMBA_GUID },
	{ "siteGUID",			LDB_SYNTAX_SAMBA_GUID },
	{ "pKTGUID",			LDB_SYNTAX_SAMBA_GUID },
	{ "fRSVersionGUID",		LDB_SYNTAX_SAMBA_GUID },
	{ "fRSReplicaSetGUID",		LDB_SYNTAX_SAMBA_GUID },
	{ "netbootGUID",		LDB_SYNTAX_SAMBA_GUID },
	{ "objectCategory",		LDB_SYNTAX_SAMBA_OBJECT_CATEGORY },
	{ "prefixMap",                  LDB_SYNTAX_SAMBA_PREFIX_MAP },
	{ "repsFrom",                   LDB_SYNTAX_SAMBA_REPSFROMTO },
	{ "repsTo",                     LDB_SYNTAX_SAMBA_REPSFROMTO },
	{ "replPropertyMetaData",       LDB_SYNTAX_SAMBA_REPLPROPERTYMETADATA },
	{ "replUpToDateVector",         LDB_SYNTAX_SAMBA_REPLUPTODATEVECTOR },
};

const struct ldb_schema_syntax *ldb_samba_syntax_by_name(struct ldb_context *ldb, const char *name)
{
	uint32_t j;
	const struct ldb_schema_syntax *s = NULL;
	
	for (j=0; j < ARRAY_SIZE(samba_syntaxes); j++) {
		if (strcmp(name, samba_syntaxes[j].name) == 0) {
			s = &samba_syntaxes[j];
			break;
		}
	}
	return s;
}

const struct ldb_schema_syntax *ldb_samba_syntax_by_lDAPDisplayName(struct ldb_context *ldb, const char *name)
{
	uint32_t j;
	const struct ldb_schema_syntax *s = NULL;

	for (j=0; j < ARRAY_SIZE(samba_attributes); j++) {
		if (strcmp(samba_attributes[j].name, name) == 0) {
			s = ldb_samba_syntax_by_name(ldb, samba_attributes[j].syntax);
			break;
		}
	}
	
	return s;
}

/*
  register the samba ldif handlers
*/
int ldb_register_samba_handlers(struct ldb_context *ldb)
{
	uint32_t i;

	for (i=0; i < ARRAY_SIZE(samba_attributes); i++) {
		int ret;
		const struct ldb_schema_syntax *s = NULL;

		s = ldb_samba_syntax_by_name(ldb, samba_attributes[i].syntax);

		if (!s) {
			s = ldb_standard_syntax_by_name(ldb, samba_attributes[i].syntax);
		}

		if (!s) {
			return -1;
		}

		ret = ldb_schema_attribute_add_with_syntax(ldb, samba_attributes[i].name, LDB_ATTR_FLAG_FIXED, s);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	for (i=0; i < ARRAY_SIZE(samba_dn_syntax); i++) {
		int ret;
		ret = ldb_dn_extended_add_syntax(ldb, LDB_ATTR_FLAG_FIXED, &samba_dn_syntax[i]);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		
	}

	return LDB_SUCCESS;
}
