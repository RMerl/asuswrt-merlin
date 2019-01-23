/*
   Unix SMB/CIFS mplementation.
   DSDB schema syntaxes

   Copyright (C) Stefan Metzmacher <metze@samba.org> 2006
   Copyright (C) Simo Sorce 2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2008

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
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/gen_ndr/ndr_misc.h"
#include <ldb.h>
#include <ldb_errors.h>
#include "system/time.h"
#include "../lib/util/charset/charset.h"
#include "librpc/ndr/libndr.h"
#include "../lib/util/asn1.h"

/**
 * Initialize dsdb_syntax_ctx with default values
 * for common cases.
 */
void dsdb_syntax_ctx_init(struct dsdb_syntax_ctx *ctx,
			  struct ldb_context *ldb,
			  const struct dsdb_schema *schema)
{
	ctx->ldb 	= ldb;
	ctx->schema 	= schema;

	/*
	 * 'true' will keep current behavior,
	 * i.e. attributeID_id will be returned by default
	 */
	ctx->is_schema_nc = true;

	ctx->pfm_remote = NULL;
}


/**
 * Returns ATTID for DRS attribute.
 *
 * ATTID depends on whether we are replicating
 * Schema NC or msDs-IntId is set for schemaAttribute
 * for the attribute.
 */
uint32_t dsdb_attribute_get_attid(const struct dsdb_attribute *attr,
				  bool for_schema_nc)
{
	if (!for_schema_nc && attr->msDS_IntId) {
		return attr->msDS_IntId;
	}

	return attr->attributeID_id;
}

/**
 * Map an ATTID from remote DC to a local ATTID
 * using remote prefixMap
 */
static bool dsdb_syntax_attid_from_remote_attid(const struct dsdb_syntax_ctx *ctx,
						TALLOC_CTX *mem_ctx,
						uint32_t id_remote,
						uint32_t *id_local)
{
	WERROR werr;
	const char *oid;

	/*
	 * map remote ATTID to local directly in case
	 * of no remote prefixMap (during provision for instance)
	 */
	if (!ctx->pfm_remote) {
		*id_local = id_remote;
		return true;
	}

	werr = dsdb_schema_pfm_oid_from_attid(ctx->pfm_remote, id_remote, mem_ctx, &oid);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("ATTID->OID failed (%s) for: 0x%08X\n", win_errstr(werr), id_remote));
		return false;
	}

	werr = dsdb_schema_pfm_make_attid(ctx->schema->prefixmap, oid, id_local);
	if (!W_ERROR_IS_OK(werr)) {
		DEBUG(0,("OID->ATTID failed (%s) for: %s\n", win_errstr(werr), oid));
		return false;
	}

	return true;
}

static WERROR dsdb_syntax_FOOBAR_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						const struct dsdb_attribute *attr,
						const struct drsuapi_DsReplicaAttribute *in,
						TALLOC_CTX *mem_ctx,
						struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		str = talloc_asprintf(out->values, "%s: not implemented",
				      attr->syntax->name);
		W_ERROR_HAVE_NO_MEMORY(str);

		out->values[i] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_FOOBAR_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						const struct dsdb_attribute *attr,
						const struct ldb_message_element *in,
						TALLOC_CTX *mem_ctx,
						struct drsuapi_DsReplicaAttribute *out)
{
	return WERR_FOOBAR;
}

static WERROR dsdb_syntax_FOOBAR_validate_ldb(const struct dsdb_syntax_ctx *ctx,
					      const struct dsdb_attribute *attr,
					      const struct ldb_message_element *in)
{
	return WERR_FOOBAR;
}

static WERROR dsdb_syntax_BOOL_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
					      const struct dsdb_attribute *attr,
					      const struct drsuapi_DsReplicaAttribute *in,
					      TALLOC_CTX *mem_ctx,
					      struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		uint32_t v;
		char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length != 4) {
			return WERR_FOOBAR;
		}

		v = IVAL(in->value_ctr.values[i].blob->data, 0);

		if (v != 0) {
			str = talloc_strdup(out->values, "TRUE");
			W_ERROR_HAVE_NO_MEMORY(str);
		} else {
			str = talloc_strdup(out->values, "FALSE");
			W_ERROR_HAVE_NO_MEMORY(str);
		}

		out->values[i] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_BOOL_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
					      const struct dsdb_attribute *attr,
					      const struct ldb_message_element *in,
					      TALLOC_CTX *mem_ctx,
					      struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		out->value_ctr.values[i].blob	= &blobs[i];

		blobs[i] = data_blob_talloc(blobs, NULL, 4);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		if (strcmp("TRUE", (const char *)in->values[i].data) == 0) {
			SIVAL(blobs[i].data, 0, 0x00000001);
		} else if (strcmp("FALSE", (const char *)in->values[i].data) == 0) {
			SIVAL(blobs[i].data, 0, 0x00000000);
		} else {
			return WERR_FOOBAR;
		}
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_BOOL_validate_ldb(const struct dsdb_syntax_ctx *ctx,
					    const struct dsdb_attribute *attr,
					    const struct ldb_message_element *in)
{
	unsigned int i;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		int t, f;

		t = strncmp("TRUE",
			    (const char *)in->values[i].data,
			    in->values[i].length);
		f = strncmp("FALSE",
			    (const char *)in->values[i].data,
			    in->values[i].length);

		if (t != 0 && f != 0) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_INT32_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
					       const struct dsdb_attribute *attr,
					       const struct drsuapi_DsReplicaAttribute *in,
					       TALLOC_CTX *mem_ctx,
					       struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		int32_t v;
		char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length != 4) {
			return WERR_FOOBAR;
		}

		v = IVALS(in->value_ctr.values[i].blob->data, 0);

		str = talloc_asprintf(out->values, "%d", v);
		W_ERROR_HAVE_NO_MEMORY(str);

		out->values[i] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_INT32_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
					       const struct dsdb_attribute *attr,
					       const struct ldb_message_element *in,
					       TALLOC_CTX *mem_ctx,
					       struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		int32_t v;

		out->value_ctr.values[i].blob	= &blobs[i];

		blobs[i] = data_blob_talloc(blobs, NULL, 4);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		/* We've to use "strtoll" here to have the intended overflows.
		 * Otherwise we may get "LONG_MAX" and the conversion is wrong. */
		v = (int32_t) strtoll((char *)in->values[i].data, NULL, 0);

		SIVALS(blobs[i].data, 0, v);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_INT32_validate_ldb(const struct dsdb_syntax_ctx *ctx,
					     const struct dsdb_attribute *attr,
					     const struct ldb_message_element *in)
{
	unsigned int i;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		long v;
		char buf[sizeof("-2147483648")];
		char *end = NULL;

		ZERO_STRUCT(buf);
		if (in->values[i].length >= sizeof(buf)) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		memcpy(buf, in->values[i].data, in->values[i].length);
		errno = 0;
		v = strtol(buf, &end, 10);
		if (errno != 0) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
		if (end && end[0] != '\0') {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		if (attr->rangeLower) {
			if ((int32_t)v < (int32_t)*attr->rangeLower) {
				return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
			}
		}

		if (attr->rangeUpper) {
			if ((int32_t)v > (int32_t)*attr->rangeUpper) {
				return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
			}
		}
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_INT64_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
					       const struct dsdb_attribute *attr,
					       const struct drsuapi_DsReplicaAttribute *in,
					       TALLOC_CTX *mem_ctx,
					       struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		int64_t v;
		char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length != 8) {
			return WERR_FOOBAR;
		}

		v = BVALS(in->value_ctr.values[i].blob->data, 0);

		str = talloc_asprintf(out->values, "%lld", (long long int)v);
		W_ERROR_HAVE_NO_MEMORY(str);

		out->values[i] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_INT64_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
					       const struct dsdb_attribute *attr,
					       const struct ldb_message_element *in,
					       TALLOC_CTX *mem_ctx,
					       struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		int64_t v;

		out->value_ctr.values[i].blob	= &blobs[i];

		blobs[i] = data_blob_talloc(blobs, NULL, 8);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		v = strtoll((const char *)in->values[i].data, NULL, 10);

		SBVALS(blobs[i].data, 0, v);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_INT64_validate_ldb(const struct dsdb_syntax_ctx *ctx,
					     const struct dsdb_attribute *attr,
					     const struct ldb_message_element *in)
{
	unsigned int i;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		long long v;
		char buf[sizeof("-9223372036854775808")];
		char *end = NULL;

		ZERO_STRUCT(buf);
		if (in->values[i].length >= sizeof(buf)) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
		memcpy(buf, in->values[i].data, in->values[i].length);

		errno = 0;
		v = strtoll(buf, &end, 10);
		if (errno != 0) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
		if (end && end[0] != '\0') {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		if (attr->rangeLower) {
			if ((int64_t)v < (int64_t)*attr->rangeLower) {
				return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
			}
		}

		if (attr->rangeUpper) {
			if ((int64_t)v > (int64_t)*attr->rangeUpper) {
				return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
			}
		}
	}

	return WERR_OK;
}
static WERROR dsdb_syntax_NTTIME_UTC_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						    const struct dsdb_attribute *attr,
						    const struct drsuapi_DsReplicaAttribute *in,
						    TALLOC_CTX *mem_ctx,
						    struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		NTTIME v;
		time_t t;
		char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length != 8) {
			return WERR_FOOBAR;
		}

		v = BVAL(in->value_ctr.values[i].blob->data, 0);
		v *= 10000000;
		t = nt_time_to_unix(v);

		/*
		 * NOTE: On a w2k3 server you can set a GeneralizedTime string
		 *       via LDAP, but you get back an UTCTime string,
		 *       but via DRSUAPI you get back the NTTIME_1sec value
		 *       that represents the GeneralizedTime value!
		 *
		 *       So if we store the UTCTime string in our ldb
		 *       we'll loose information!
		 */
		str = ldb_timestring_utc(out->values, t);
		W_ERROR_HAVE_NO_MEMORY(str);
		out->values[i] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_NTTIME_UTC_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						    const struct dsdb_attribute *attr,
						    const struct ldb_message_element *in,
						    TALLOC_CTX *mem_ctx,
						    struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		NTTIME v;
		time_t t;

		out->value_ctr.values[i].blob	= &blobs[i];

		blobs[i] = data_blob_talloc(blobs, NULL, 8);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		t = ldb_string_utc_to_time((const char *)in->values[i].data);
		unix_to_nt_time(&v, t);
		v /= 10000000;

		SBVAL(blobs[i].data, 0, v);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_NTTIME_UTC_validate_ldb(const struct dsdb_syntax_ctx *ctx,
						  const struct dsdb_attribute *attr,
						  const struct ldb_message_element *in)
{
	unsigned int i;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		time_t t;
		char buf[sizeof("090826075717Z")];

		ZERO_STRUCT(buf);
		if (in->values[i].length >= sizeof(buf)) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
		memcpy(buf, in->values[i].data, in->values[i].length);

		errno = 0;
		t = ldb_string_utc_to_time(buf);
		if (errno != 0) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		if (attr->rangeLower) {
			if ((int32_t)t < (int32_t)*attr->rangeLower) {
				return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
			}
		}

		if (attr->rangeUpper) {
			if ((int32_t)t > (int32_t)*attr->rangeLower) {
				return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
			}
		}

		/*
		 * TODO: verify the comment in the
		 * dsdb_syntax_NTTIME_UTC_drsuapi_to_ldb() function!
		 */
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_NTTIME_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						const struct dsdb_attribute *attr,
						const struct drsuapi_DsReplicaAttribute *in,
						TALLOC_CTX *mem_ctx,
						struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		NTTIME v;
		time_t t;
		char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length != 8) {
			return WERR_FOOBAR;
		}

		v = BVAL(in->value_ctr.values[i].blob->data, 0);
		v *= 10000000;
		t = nt_time_to_unix(v);

		str = ldb_timestring(out->values, t);
		W_ERROR_HAVE_NO_MEMORY(str);

		out->values[i] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_NTTIME_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						const struct dsdb_attribute *attr,
						const struct ldb_message_element *in,
						TALLOC_CTX *mem_ctx,
						struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		NTTIME v;
		time_t t;
		int ret;

		out->value_ctr.values[i].blob	= &blobs[i];

		blobs[i] = data_blob_talloc(blobs, NULL, 8);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		ret = ldb_val_to_time(&in->values[i], &t);
		if (ret != LDB_SUCCESS) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
		unix_to_nt_time(&v, t);
		v /= 10000000;

		SBVAL(blobs[i].data, 0, v);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_NTTIME_validate_ldb(const struct dsdb_syntax_ctx *ctx,
					      const struct dsdb_attribute *attr,
					      const struct ldb_message_element *in)
{
	unsigned int i;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		time_t t;
		int ret;

		ret = ldb_val_to_time(&in->values[i], &t);
		if (ret != LDB_SUCCESS) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		if (attr->rangeLower) {
			if ((int32_t)t < (int32_t)*attr->rangeLower) {
				return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
			}
		}

		if (attr->rangeUpper) {
			if ((int32_t)t > (int32_t)*attr->rangeLower) {
				return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
			}
		}
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DATA_BLOB_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct drsuapi_DsReplicaAttribute *in,
						   TALLOC_CTX *mem_ctx,
						   struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length == 0) {
			return WERR_FOOBAR;
		}

		out->values[i] = data_blob_dup_talloc(out->values,
						      in->value_ctr.values[i].blob);
		W_ERROR_HAVE_NO_MEMORY(out->values[i].data);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DATA_BLOB_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct ldb_message_element *in,
						   TALLOC_CTX *mem_ctx,
						   struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		out->value_ctr.values[i].blob	= &blobs[i];

		blobs[i] = data_blob_dup_talloc(blobs, &in->values[i]);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DATA_BLOB_validate_one_val(const struct dsdb_syntax_ctx *ctx,
						     const struct dsdb_attribute *attr,
						     const struct ldb_val *val)
{
	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	if (attr->rangeLower) {
		if ((uint32_t)val->length < (uint32_t)*attr->rangeLower) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
	}

	if (attr->rangeUpper) {
		if ((uint32_t)val->length > (uint32_t)*attr->rangeUpper) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DATA_BLOB_validate_ldb(const struct dsdb_syntax_ctx *ctx,
						 const struct dsdb_attribute *attr,
						 const struct ldb_message_element *in)
{
	unsigned int i;
	WERROR status;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		if (in->values[i].length == 0) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		status = dsdb_syntax_DATA_BLOB_validate_one_val(ctx,
								attr,
								&in->values[i]);
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}
	}

	return WERR_OK;
}

static WERROR _dsdb_syntax_auto_OID_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct drsuapi_DsReplicaAttribute *in,
						   TALLOC_CTX *mem_ctx,
						   struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		uint32_t v;
		const struct dsdb_class *c;
		const struct dsdb_attribute *a;
		const char *str = NULL;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length != 4) {
			return WERR_FOOBAR;
		}

		v = IVAL(in->value_ctr.values[i].blob->data, 0);

		if ((c = dsdb_class_by_governsID_id(ctx->schema, v))) {
			str = talloc_strdup(out->values, c->lDAPDisplayName);
		} else if ((a = dsdb_attribute_by_attributeID_id(ctx->schema, v))) {
			str = talloc_strdup(out->values, a->lDAPDisplayName);
		} else {
			WERROR werr;
			SMB_ASSERT(ctx->pfm_remote);
			werr = dsdb_schema_pfm_oid_from_attid(ctx->pfm_remote, v,
							      out->values, &str);
			W_ERROR_NOT_OK_RETURN(werr);
		}
		W_ERROR_HAVE_NO_MEMORY(str);

		/* the values need to be reversed */
		out->values[out->num_values - (i + 1)] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR _dsdb_syntax_OID_obj_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						  const struct dsdb_attribute *attr,
						  const struct drsuapi_DsReplicaAttribute *in,
						  TALLOC_CTX *mem_ctx,
						  struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		uint32_t v;
		const struct dsdb_class *c;
		const char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length != 4) {
			return WERR_FOOBAR;
		}

		v = IVAL(in->value_ctr.values[i].blob->data, 0);

		/* convert remote ATTID to local ATTID */
		if (!dsdb_syntax_attid_from_remote_attid(ctx, mem_ctx, v, &v)) {
			DEBUG(1,(__location__ ": Failed to map remote ATTID to local ATTID!\n"));
			return WERR_FOOBAR;
		}

		c = dsdb_class_by_governsID_id(ctx->schema, v);
		if (!c) {
			DEBUG(1,(__location__ ": Unknown governsID 0x%08X\n", v));
			return WERR_FOOBAR;
		}

		str = talloc_strdup(out->values, c->lDAPDisplayName);
		W_ERROR_HAVE_NO_MEMORY(str);

		/* the values need to be reversed */
		out->values[out->num_values - (i + 1)] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR _dsdb_syntax_OID_attr_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct drsuapi_DsReplicaAttribute *in,
						   TALLOC_CTX *mem_ctx,
						   struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		uint32_t v;
		const struct dsdb_attribute *a;
		const char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length != 4) {
			return WERR_FOOBAR;
		}

		v = IVAL(in->value_ctr.values[i].blob->data, 0);

		/* convert remote ATTID to local ATTID */
		if (!dsdb_syntax_attid_from_remote_attid(ctx, mem_ctx, v, &v)) {
			DEBUG(1,(__location__ ": Failed to map remote ATTID to local ATTID!\n"));
			return WERR_FOOBAR;
		}

		a = dsdb_attribute_by_attributeID_id(ctx->schema, v);
		if (!a) {
			DEBUG(1,(__location__ ": Unknown attributeID_id 0x%08X\n", v));
			return WERR_FOOBAR;
		}

		str = talloc_strdup(out->values, a->lDAPDisplayName);
		W_ERROR_HAVE_NO_MEMORY(str);

		/* the values need to be reversed */
		out->values[out->num_values - (i + 1)] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR _dsdb_syntax_OID_oid_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						  const struct dsdb_attribute *attr,
						  const struct drsuapi_DsReplicaAttribute *in,
						  TALLOC_CTX *mem_ctx,
						  struct ldb_message_element *out)
{
	unsigned int i;

	SMB_ASSERT(ctx->pfm_remote);

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		uint32_t attid;
		WERROR status;
		const char *oid;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length != 4) {
			return WERR_FOOBAR;
		}

		attid = IVAL(in->value_ctr.values[i].blob->data, 0);

		status = dsdb_schema_pfm_oid_from_attid(ctx->pfm_remote, attid,
							out->values, &oid);
		if (!W_ERROR_IS_OK(status)) {
			DEBUG(0,(__location__ ": Error: Unknown ATTID 0x%08X\n",
				 attid));
			return status;
		}

		out->values[i] = data_blob_string_const(oid);
	}

	return WERR_OK;
}

static WERROR _dsdb_syntax_auto_OID_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct ldb_message_element *in,
						   TALLOC_CTX *mem_ctx,
						   struct drsuapi_DsReplicaAttribute *out)
{
        unsigned int i;
        DATA_BLOB *blobs;

        out->attid= dsdb_attribute_get_attid(attr,
					     ctx->is_schema_nc);
        out->value_ctr.num_values= in->num_values;
        out->value_ctr.values= talloc_array(mem_ctx,
                                            struct drsuapi_DsAttributeValue,
                                            in->num_values);
        W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

        blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
        W_ERROR_HAVE_NO_MEMORY(blobs);

        for (i=0; i < in->num_values; i++) {
		const struct dsdb_class *obj_class;
		const struct dsdb_attribute *obj_attr;
		struct ldb_val *v;

		out->value_ctr.values[i].blob= &blobs[i];

		blobs[i] = data_blob_talloc(blobs, NULL, 4);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		/* in DRS windows puts the classes in the opposite
		   order to the order used in ldap */
		v = &in->values[(in->num_values-1)-i];

		if ((obj_class = dsdb_class_by_lDAPDisplayName_ldb_val(ctx->schema, v))) {
			SIVAL(blobs[i].data, 0, obj_class->governsID_id);
		} else if ((obj_attr = dsdb_attribute_by_lDAPDisplayName_ldb_val(ctx->schema, v))) {
			SIVAL(blobs[i].data, 0, obj_attr->attributeID_id);
		} else {
			uint32_t attid;
			WERROR werr;
			werr = dsdb_schema_pfm_attid_from_oid(ctx->schema->prefixmap,
							      (const char *)v->data,
							      &attid);
			W_ERROR_NOT_OK_RETURN(werr);
			SIVAL(blobs[i].data, 0, attid);
		}

        }


        return WERR_OK;
}

static WERROR _dsdb_syntax_OID_obj_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						  const struct dsdb_attribute *attr,
						  const struct ldb_message_element *in,
						  TALLOC_CTX *mem_ctx,
						  struct drsuapi_DsReplicaAttribute *out)
{
        unsigned int i;
        DATA_BLOB *blobs;

        out->attid= dsdb_attribute_get_attid(attr,
					     ctx->is_schema_nc);
        out->value_ctr.num_values= in->num_values;
        out->value_ctr.values= talloc_array(mem_ctx,
                                            struct drsuapi_DsAttributeValue,
                                            in->num_values);
        W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

        blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
        W_ERROR_HAVE_NO_MEMORY(blobs);

        for (i=0; i < in->num_values; i++) {
		const struct dsdb_class *obj_class;

		out->value_ctr.values[i].blob= &blobs[i];

		blobs[i] = data_blob_talloc(blobs, NULL, 4);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		/* in DRS windows puts the classes in the opposite
		   order to the order used in ldap */
		obj_class = dsdb_class_by_lDAPDisplayName(ctx->schema,
							  (const char *)in->values[(in->num_values-1)-i].data);
		if (!obj_class) {
			return WERR_FOOBAR;
		}
		SIVAL(blobs[i].data, 0, obj_class->governsID_id);
        }


        return WERR_OK;
}

static WERROR _dsdb_syntax_OID_attr_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct ldb_message_element *in,
						   TALLOC_CTX *mem_ctx,
						   struct drsuapi_DsReplicaAttribute *out)
{
        unsigned int i;
        DATA_BLOB *blobs;

        out->attid= dsdb_attribute_get_attid(attr,
					     ctx->is_schema_nc);
        out->value_ctr.num_values= in->num_values;
        out->value_ctr.values= talloc_array(mem_ctx,
                                            struct drsuapi_DsAttributeValue,
                                            in->num_values);
        W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

        blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
        W_ERROR_HAVE_NO_MEMORY(blobs);

        for (i=0; i < in->num_values; i++) {
		const struct dsdb_attribute *obj_attr;

		out->value_ctr.values[i].blob= &blobs[i];

		blobs[i] = data_blob_talloc(blobs, NULL, 4);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		obj_attr = dsdb_attribute_by_lDAPDisplayName(ctx->schema, (const char *)in->values[i].data);
		if (!obj_attr) {
			return WERR_FOOBAR;
		}
		SIVAL(blobs[i].data, 0, obj_attr->attributeID_id);
        }


        return WERR_OK;
}

static WERROR _dsdb_syntax_OID_oid_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						  const struct dsdb_attribute *attr,
						  const struct ldb_message_element *in,
						  TALLOC_CTX *mem_ctx,
						  struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	out->attid= dsdb_attribute_get_attid(attr,
					     ctx->is_schema_nc);
	out->value_ctr.num_values= in->num_values;
	out->value_ctr.values= talloc_array(mem_ctx,
					    struct drsuapi_DsAttributeValue,
					    in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		uint32_t attid;
		WERROR status;

		out->value_ctr.values[i].blob= &blobs[i];

		blobs[i] = data_blob_talloc(blobs, NULL, 4);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		status = dsdb_schema_pfm_attid_from_oid(ctx->schema->prefixmap,
						        (const char *)in->values[i].data,
						        &attid);
		W_ERROR_NOT_OK_RETURN(status);

		SIVAL(blobs[i].data, 0, attid);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_OID_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
					     const struct dsdb_attribute *attr,
					     const struct drsuapi_DsReplicaAttribute *in,
					     TALLOC_CTX *mem_ctx,
					     struct ldb_message_element *out)
{
	WERROR werr;

	switch (attr->attributeID_id) {
	case DRSUAPI_ATTID_objectClass:
	case DRSUAPI_ATTID_subClassOf:
	case DRSUAPI_ATTID_auxiliaryClass:
	case DRSUAPI_ATTID_systemAuxiliaryClass:
	case DRSUAPI_ATTID_systemPossSuperiors:
	case DRSUAPI_ATTID_possSuperiors:
		werr = _dsdb_syntax_OID_obj_drsuapi_to_ldb(ctx, attr, in, mem_ctx, out);
		break;
	case DRSUAPI_ATTID_systemMustContain:
	case DRSUAPI_ATTID_systemMayContain:
	case DRSUAPI_ATTID_mustContain:
	case DRSUAPI_ATTID_rDNAttId:
	case DRSUAPI_ATTID_transportAddressAttribute:
	case DRSUAPI_ATTID_mayContain:
		werr = _dsdb_syntax_OID_attr_drsuapi_to_ldb(ctx, attr, in, mem_ctx, out);
		break;
	case DRSUAPI_ATTID_governsID:
	case DRSUAPI_ATTID_attributeID:
	case DRSUAPI_ATTID_attributeSyntax:
		werr = _dsdb_syntax_OID_oid_drsuapi_to_ldb(ctx, attr, in, mem_ctx, out);
		break;
	default:
		DEBUG(0,(__location__ ": Unknown handling for attributeID_id for %s\n",
			 attr->lDAPDisplayName));
		return _dsdb_syntax_auto_OID_drsuapi_to_ldb(ctx, attr, in, mem_ctx, out);
	}

	/* When we are doing the vampire of a schema, we don't want
	 * the inability to reference an OID to get in the way.
	 * Otherwise, we won't get the new schema with which to
	 * understand this */
	if (!W_ERROR_IS_OK(werr) && ctx->schema->relax_OID_conversions) {
		return _dsdb_syntax_OID_oid_drsuapi_to_ldb(ctx, attr, in, mem_ctx, out);
	}
	return werr;
}

static WERROR dsdb_syntax_OID_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
					     const struct dsdb_attribute *attr,
					     const struct ldb_message_element *in,
					     TALLOC_CTX *mem_ctx,
					     struct drsuapi_DsReplicaAttribute *out)
{
	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	switch (attr->attributeID_id) {
	case DRSUAPI_ATTID_objectClass:
	case DRSUAPI_ATTID_subClassOf:
	case DRSUAPI_ATTID_auxiliaryClass:
	case DRSUAPI_ATTID_systemAuxiliaryClass:
	case DRSUAPI_ATTID_systemPossSuperiors:
	case DRSUAPI_ATTID_possSuperiors:
		return _dsdb_syntax_OID_obj_ldb_to_drsuapi(ctx, attr, in, mem_ctx, out);
	case DRSUAPI_ATTID_systemMustContain:
	case DRSUAPI_ATTID_systemMayContain:
	case DRSUAPI_ATTID_mustContain:
	case DRSUAPI_ATTID_rDNAttId:
	case DRSUAPI_ATTID_transportAddressAttribute:
	case DRSUAPI_ATTID_mayContain:
		return _dsdb_syntax_OID_attr_ldb_to_drsuapi(ctx, attr, in, mem_ctx, out);
	case DRSUAPI_ATTID_governsID:
	case DRSUAPI_ATTID_attributeID:
	case DRSUAPI_ATTID_attributeSyntax:
		return _dsdb_syntax_OID_oid_ldb_to_drsuapi(ctx, attr, in, mem_ctx, out);
	}

	DEBUG(0,(__location__ ": Unknown handling for attributeID_id for %s\n",
		 attr->lDAPDisplayName));

	return _dsdb_syntax_auto_OID_ldb_to_drsuapi(ctx, attr, in, mem_ctx, out);
}

static WERROR _dsdb_syntax_OID_validate_numericoid(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct ldb_message_element *in)
{
	unsigned int i;
	TALLOC_CTX *tmp_ctx;

	tmp_ctx = talloc_new(ctx->ldb);
	W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

	for (i=0; i < in->num_values; i++) {
		DATA_BLOB blob;
		char *oid_out;
		const char *oid = (const char*)in->values[i].data;

		if (!ber_write_OID_String(tmp_ctx, &blob, oid)) {
			DEBUG(0,("ber_write_OID_String() failed for %s\n", oid));
			talloc_free(tmp_ctx);
			return WERR_INVALID_PARAMETER;
		}

		if (!ber_read_OID_String(tmp_ctx, blob, &oid_out)) {
			DEBUG(0,("ber_read_OID_String() failed for %s\n",
				 hex_encode_talloc(tmp_ctx, blob.data, blob.length)));
			talloc_free(tmp_ctx);
			return WERR_INVALID_PARAMETER;
		}

		if (strcmp(oid, oid_out) != 0) {
			talloc_free(tmp_ctx);
			return WERR_INVALID_PARAMETER;
		}
	}

	talloc_free(tmp_ctx);
	return WERR_OK;
}

static WERROR dsdb_syntax_OID_validate_ldb(const struct dsdb_syntax_ctx *ctx,
					   const struct dsdb_attribute *attr,
					   const struct ldb_message_element *in)
{
	WERROR status;
	struct drsuapi_DsReplicaAttribute drs_tmp;
	struct ldb_message_element ldb_tmp;
	TALLOC_CTX *tmp_ctx;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	switch (attr->attributeID_id) {
	case DRSUAPI_ATTID_governsID:
	case DRSUAPI_ATTID_attributeID:
	case DRSUAPI_ATTID_attributeSyntax:
		return _dsdb_syntax_OID_validate_numericoid(ctx, attr, in);
	}

	/*
	 * TODO: optimize and verify this code
	 */

	tmp_ctx = talloc_new(ctx->ldb);
	W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

	status = dsdb_syntax_OID_ldb_to_drsuapi(ctx,
						attr,
						in,
						tmp_ctx,
						&drs_tmp);
	if (!W_ERROR_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return status;
	}

	status = dsdb_syntax_OID_drsuapi_to_ldb(ctx,
						attr,
						&drs_tmp,
						tmp_ctx,
						&ldb_tmp);
	if (!W_ERROR_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return status;
	}

	talloc_free(tmp_ctx);
	return WERR_OK;
}

static WERROR dsdb_syntax_UNICODE_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						 const struct dsdb_attribute *attr,
						 const struct drsuapi_DsReplicaAttribute *in,
						 TALLOC_CTX *mem_ctx,
						 struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length == 0) {
			return WERR_FOOBAR;
		}

		if (!convert_string_talloc(out->values,
					   CH_UTF16, CH_UNIX,
					   in->value_ctr.values[i].blob->data,
					   in->value_ctr.values[i].blob->length,
					   (void **)&str, NULL, false)) {
			return WERR_FOOBAR;
		}

		out->values[i] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_UNICODE_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						 const struct dsdb_attribute *attr,
						 const struct ldb_message_element *in,
						 TALLOC_CTX *mem_ctx,
						 struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		out->value_ctr.values[i].blob	= &blobs[i];

		if (!convert_string_talloc(blobs,
					   CH_UNIX, CH_UTF16,
					   in->values[i].data, in->values[i].length,
					   (void **)&blobs[i].data, &blobs[i].length, false)) {
			return WERR_FOOBAR;
		}
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_UNICODE_validate_one_val(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct ldb_val *val)
{
	void *dst = NULL;
	size_t size;
	bool ok;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	ok = convert_string_talloc(ctx->ldb,
				   CH_UNIX, CH_UTF16,
				   val->data,
				   val->length,
				   (void **)&dst,
				   &size, false);
	TALLOC_FREE(dst);
	if (!ok) {
		return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
	}

	if (attr->rangeLower) {
		if ((size/2) < *attr->rangeLower) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
	}

	if (attr->rangeUpper) {
		if ((size/2) > *attr->rangeUpper) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_UNICODE_validate_ldb(const struct dsdb_syntax_ctx *ctx,
					       const struct dsdb_attribute *attr,
					       const struct ldb_message_element *in)
{
	WERROR status;
	unsigned int i;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		if (in->values[i].length == 0) {
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		status = dsdb_syntax_UNICODE_validate_one_val(ctx,
							      attr,
							      &in->values[i]);
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_one_DN_drsuapi_to_ldb(TALLOC_CTX *mem_ctx, struct ldb_context *ldb,
						const struct dsdb_syntax *syntax,
						const DATA_BLOB *in, DATA_BLOB *out)
{
	struct drsuapi_DsReplicaObjectIdentifier3 id3;
	enum ndr_err_code ndr_err;
	DATA_BLOB guid_blob;
	struct ldb_dn *dn;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	int ret;
	NTSTATUS status;

	if (!tmp_ctx) {
		W_ERROR_HAVE_NO_MEMORY(tmp_ctx);
	}

	if (in == NULL) {
		talloc_free(tmp_ctx);
		return WERR_FOOBAR;
	}

	if (in->length == 0) {
		talloc_free(tmp_ctx);
		return WERR_FOOBAR;
	}


	/* windows sometimes sends an extra two pad bytes here */
	ndr_err = ndr_pull_struct_blob(in,
				       tmp_ctx, &id3,
				       (ndr_pull_flags_fn_t)ndr_pull_drsuapi_DsReplicaObjectIdentifier3);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		talloc_free(tmp_ctx);
		return ntstatus_to_werror(status);
	}

	dn = ldb_dn_new(tmp_ctx, ldb, id3.dn);
	if (!dn) {
		talloc_free(tmp_ctx);
		/* If this fails, it must be out of memory, as it does not do much parsing */
		W_ERROR_HAVE_NO_MEMORY(dn);
	}

	if (!GUID_all_zero(&id3.guid)) {
		status = GUID_to_ndr_blob(&id3.guid, tmp_ctx, &guid_blob);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(status);
		}

		ret = ldb_dn_set_extended_component(dn, "GUID", &guid_blob);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return WERR_FOOBAR;
		}
		talloc_free(guid_blob.data);
	}

	if (id3.__ndr_size_sid) {
		DATA_BLOB sid_blob;
		ndr_err = ndr_push_struct_blob(&sid_blob, tmp_ctx, &id3.sid,
					       (ndr_push_flags_fn_t)ndr_push_dom_sid);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(status);
		}

		ret = ldb_dn_set_extended_component(dn, "SID", &sid_blob);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return WERR_FOOBAR;
		}
	}

	*out = data_blob_string_const(ldb_dn_get_extended_linearized(mem_ctx, dn, 1));
	talloc_free(tmp_ctx);
	return WERR_OK;
}

static WERROR dsdb_syntax_DN_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
					    const struct dsdb_attribute *attr,
					    const struct drsuapi_DsReplicaAttribute *in,
					    TALLOC_CTX *mem_ctx,
					    struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		WERROR status = dsdb_syntax_one_DN_drsuapi_to_ldb(out->values, ctx->ldb, attr->syntax,
								  in->value_ctr.values[i].blob,
								  &out->values[i]);
		if (!W_ERROR_IS_OK(status)) {
			return status;
		}

	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DN_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
					    const struct dsdb_attribute *attr,
					    const struct ldb_message_element *in,
					    TALLOC_CTX *mem_ctx,
					    struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		struct drsuapi_DsReplicaObjectIdentifier3 id3;
		enum ndr_err_code ndr_err;
		struct ldb_dn *dn;
		TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
		NTSTATUS status;

		W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

		out->value_ctr.values[i].blob	= &blobs[i];

		dn = ldb_dn_from_ldb_val(tmp_ctx, ctx->ldb, &in->values[i]);

		W_ERROR_HAVE_NO_MEMORY(dn);

		ZERO_STRUCT(id3);

		status = dsdb_get_extended_dn_guid(dn, &id3.guid, "GUID");
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(status);
		}

		status = dsdb_get_extended_dn_sid(dn, &id3.sid, "SID");
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(status);
		}

		id3.dn = ldb_dn_get_linearized(dn);

		ndr_err = ndr_push_struct_blob(&blobs[i], blobs, &id3, (ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(status);
		}
		talloc_free(tmp_ctx);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DN_validate_one_val(const struct dsdb_syntax_ctx *ctx,
					      const struct dsdb_attribute *attr,
					      const struct ldb_val *val,
					      TALLOC_CTX *mem_ctx,
					      struct dsdb_dn **_dsdb_dn)
{
	static const char * const extended_list[] = { "GUID", "SID", NULL };
	enum ndr_err_code ndr_err;
	struct GUID guid;
	struct dom_sid sid;
	const DATA_BLOB *sid_blob;
	struct dsdb_dn *dsdb_dn;
	struct ldb_dn *dn;
	char *dn_str;
	struct ldb_dn *dn2;
	char *dn2_str;
	int num_components;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	NTSTATUS status;

	W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	dsdb_dn = dsdb_dn_parse(tmp_ctx, ctx->ldb, val,
				attr->syntax->ldap_oid);
	if (!dsdb_dn) {
		talloc_free(tmp_ctx);
		return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
	}
	dn = dsdb_dn->dn;

	dn2 = ldb_dn_copy(tmp_ctx, dn);
	if (dn == NULL) {
		talloc_free(tmp_ctx);
		return WERR_NOMEM;
	}

	num_components = ldb_dn_get_comp_num(dn);

	status = dsdb_get_extended_dn_guid(dn, &guid, "GUID");
	if (NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		num_components++;
	} else if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
	}

	sid_blob = ldb_dn_get_extended_component(dn, "SID");
	if (sid_blob) {
		num_components++;
		ndr_err = ndr_pull_struct_blob_all(sid_blob,
						   tmp_ctx,
						   &sid,
						   (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			talloc_free(tmp_ctx);
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}
	}

	/* Do not allow links to the RootDSE */
	if (num_components == 0) {
		talloc_free(tmp_ctx);
		return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
	}

	/*
	 * We need to check that only "GUID" and "SID" are
	 * specified as extended components, we do that
	 * by comparing the dn's after removing all components
	 * from one dn and only the allowed subset from the other
	 * one.
	 */
	ldb_dn_extended_filter(dn, extended_list);

	dn_str = ldb_dn_get_extended_linearized(tmp_ctx, dn, 0);
	if (dn_str == NULL) {
		talloc_free(tmp_ctx);
		return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
	}
	dn2_str = ldb_dn_get_extended_linearized(tmp_ctx, dn2, 0);
	if (dn2_str == NULL) {
		talloc_free(tmp_ctx);
		return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
	}

	if (strcmp(dn_str, dn2_str) != 0) {
		talloc_free(tmp_ctx);
		return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
	}

	*_dsdb_dn = talloc_move(mem_ctx, &dsdb_dn);
	talloc_free(tmp_ctx);
	return WERR_OK;
}

static WERROR dsdb_syntax_DN_validate_ldb(const struct dsdb_syntax_ctx *ctx,
					  const struct dsdb_attribute *attr,
					  const struct ldb_message_element *in)
{
	unsigned int i;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		WERROR status;
		struct dsdb_dn *dsdb_dn;
		TALLOC_CTX *tmp_ctx = talloc_new(ctx->ldb);
		W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

		status = dsdb_syntax_DN_validate_one_val(ctx,
							 attr,
							 &in->values[i],
							 tmp_ctx, &dsdb_dn);
		if (!W_ERROR_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return status;
		}

		if (dsdb_dn->dn_format != DSDB_NORMAL_DN) {
			talloc_free(tmp_ctx);
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		talloc_free(tmp_ctx);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DN_BINARY_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct drsuapi_DsReplicaAttribute *in,
						   TALLOC_CTX *mem_ctx,
						   struct ldb_message_element *out)
{
	unsigned int i;
	int ret;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		struct drsuapi_DsReplicaObjectIdentifier3Binary id3;
		enum ndr_err_code ndr_err;
		DATA_BLOB guid_blob;
		struct ldb_dn *dn;
		struct dsdb_dn *dsdb_dn;
		NTSTATUS status;
		TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
		if (!tmp_ctx) {
			W_ERROR_HAVE_NO_MEMORY(tmp_ctx);
		}

		if (in->value_ctr.values[i].blob == NULL) {
			talloc_free(tmp_ctx);
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length == 0) {
			talloc_free(tmp_ctx);
			return WERR_FOOBAR;
		}


		/* windows sometimes sends an extra two pad bytes here */
		ndr_err = ndr_pull_struct_blob(in->value_ctr.values[i].blob,
					       tmp_ctx, &id3,
					       (ndr_pull_flags_fn_t)ndr_pull_drsuapi_DsReplicaObjectIdentifier3Binary);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(status);
		}

		dn = ldb_dn_new(tmp_ctx, ctx->ldb, id3.dn);
		if (!dn) {
			talloc_free(tmp_ctx);
			/* If this fails, it must be out of memory, as it does not do much parsing */
			W_ERROR_HAVE_NO_MEMORY(dn);
		}

		status = GUID_to_ndr_blob(&id3.guid, tmp_ctx, &guid_blob);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(status);
		}

		ret = ldb_dn_set_extended_component(dn, "GUID", &guid_blob);
		if (ret != LDB_SUCCESS) {
			talloc_free(tmp_ctx);
			return WERR_FOOBAR;
		}

		talloc_free(guid_blob.data);

		if (id3.__ndr_size_sid) {
			DATA_BLOB sid_blob;
			ndr_err = ndr_push_struct_blob(&sid_blob, tmp_ctx, &id3.sid,
						       (ndr_push_flags_fn_t)ndr_push_dom_sid);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				status = ndr_map_error2ntstatus(ndr_err);
				talloc_free(tmp_ctx);
				return ntstatus_to_werror(status);
			}

			ret = ldb_dn_set_extended_component(dn, "SID", &sid_blob);
			if (ret != LDB_SUCCESS) {
				talloc_free(tmp_ctx);
				return WERR_FOOBAR;
			}
		}

		/* set binary stuff */
		dsdb_dn = dsdb_dn_construct(tmp_ctx, dn, id3.binary, attr->syntax->ldap_oid);
		if (!dsdb_dn) {
			/* If this fails, it must be out of memory, we know the ldap_oid is valid */
			talloc_free(tmp_ctx);
			W_ERROR_HAVE_NO_MEMORY(dsdb_dn);
		}
		out->values[i] = data_blob_string_const(dsdb_dn_get_extended_linearized(out->values, dsdb_dn, 1));
		talloc_free(tmp_ctx);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DN_BINARY_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct ldb_message_element *in,
						   TALLOC_CTX *mem_ctx,
						   struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		struct drsuapi_DsReplicaObjectIdentifier3Binary id3;
		enum ndr_err_code ndr_err;
		const DATA_BLOB *sid_blob;
		struct dsdb_dn *dsdb_dn;
		TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
		NTSTATUS status;

		W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

		out->value_ctr.values[i].blob	= &blobs[i];

		dsdb_dn = dsdb_dn_parse(tmp_ctx, ctx->ldb, &in->values[i], attr->syntax->ldap_oid);

		if (!dsdb_dn) {
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(NT_STATUS_INVALID_PARAMETER);
		}

		ZERO_STRUCT(id3);

		status = dsdb_get_extended_dn_guid(dsdb_dn->dn, &id3.guid, "GUID");
		if (!NT_STATUS_IS_OK(status) &&
		    !NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(status);
		}

		sid_blob = ldb_dn_get_extended_component(dsdb_dn->dn, "SID");
		if (sid_blob) {

			ndr_err = ndr_pull_struct_blob_all(sid_blob,
							   tmp_ctx, &id3.sid,
							   (ndr_pull_flags_fn_t)ndr_pull_dom_sid);
			if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
				status = ndr_map_error2ntstatus(ndr_err);
				talloc_free(tmp_ctx);
				return ntstatus_to_werror(status);
			}
		}

		id3.dn = ldb_dn_get_linearized(dsdb_dn->dn);

		/* get binary stuff */
		id3.binary = dsdb_dn->extra_part;

		ndr_err = ndr_push_struct_blob(&blobs[i], blobs, &id3, (ndr_push_flags_fn_t)ndr_push_drsuapi_DsReplicaObjectIdentifier3Binary);
		if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
			status = ndr_map_error2ntstatus(ndr_err);
			talloc_free(tmp_ctx);
			return ntstatus_to_werror(status);
		}
		talloc_free(tmp_ctx);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DN_BINARY_validate_ldb(const struct dsdb_syntax_ctx *ctx,
						 const struct dsdb_attribute *attr,
						 const struct ldb_message_element *in)
{
	unsigned int i;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		WERROR status;
		struct dsdb_dn *dsdb_dn;
		TALLOC_CTX *tmp_ctx = talloc_new(ctx->ldb);
		W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

		status = dsdb_syntax_DN_validate_one_val(ctx,
							 attr,
							 &in->values[i],
							 tmp_ctx, &dsdb_dn);
		if (!W_ERROR_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return status;
		}

		if (dsdb_dn->dn_format != DSDB_BINARY_DN) {
			talloc_free(tmp_ctx);
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		status = dsdb_syntax_DATA_BLOB_validate_one_val(ctx,
								attr,
								&dsdb_dn->extra_part);
		if (!W_ERROR_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return status;
		}

		talloc_free(tmp_ctx);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_DN_STRING_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct drsuapi_DsReplicaAttribute *in,
						   TALLOC_CTX *mem_ctx,
						   struct ldb_message_element *out)
{
	return dsdb_syntax_DN_BINARY_drsuapi_to_ldb(ctx,
						    attr,
						    in,
						    mem_ctx,
						    out);
}

static WERROR dsdb_syntax_DN_STRING_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
						   const struct dsdb_attribute *attr,
						   const struct ldb_message_element *in,
						   TALLOC_CTX *mem_ctx,
						   struct drsuapi_DsReplicaAttribute *out)
{
	return dsdb_syntax_DN_BINARY_ldb_to_drsuapi(ctx,
						    attr,
						    in,
						    mem_ctx,
						    out);
}

static WERROR dsdb_syntax_DN_STRING_validate_ldb(const struct dsdb_syntax_ctx *ctx,
						 const struct dsdb_attribute *attr,
						 const struct ldb_message_element *in)
{
	unsigned int i;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	for (i=0; i < in->num_values; i++) {
		WERROR status;
		struct dsdb_dn *dsdb_dn;
		TALLOC_CTX *tmp_ctx = talloc_new(ctx->ldb);
		W_ERROR_HAVE_NO_MEMORY(tmp_ctx);

		status = dsdb_syntax_DN_validate_one_val(ctx,
							 attr,
							 &in->values[i],
							 tmp_ctx, &dsdb_dn);
		if (!W_ERROR_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return status;
		}

		if (dsdb_dn->dn_format != DSDB_STRING_DN) {
			talloc_free(tmp_ctx);
			return WERR_DS_INVALID_ATTRIBUTE_SYNTAX;
		}

		status = dsdb_syntax_UNICODE_validate_one_val(ctx,
							      attr,
							      &dsdb_dn->extra_part);
		if (!W_ERROR_IS_OK(status)) {
			talloc_free(tmp_ctx);
			return status;
		}

		talloc_free(tmp_ctx);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_PRESENTATION_ADDRESS_drsuapi_to_ldb(const struct dsdb_syntax_ctx *ctx,
							      const struct dsdb_attribute *attr,
							      const struct drsuapi_DsReplicaAttribute *in,
							      TALLOC_CTX *mem_ctx,
							      struct ldb_message_element *out)
{
	unsigned int i;

	out->flags	= 0;
	out->name	= talloc_strdup(mem_ctx, attr->lDAPDisplayName);
	W_ERROR_HAVE_NO_MEMORY(out->name);

	out->num_values	= in->value_ctr.num_values;
	out->values	= talloc_array(mem_ctx, struct ldb_val, out->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->values);

	for (i=0; i < out->num_values; i++) {
		size_t len;
		char *str;

		if (in->value_ctr.values[i].blob == NULL) {
			return WERR_FOOBAR;
		}

		if (in->value_ctr.values[i].blob->length < 4) {
			return WERR_FOOBAR;
		}

		len = IVAL(in->value_ctr.values[i].blob->data, 0);

		if (len != in->value_ctr.values[i].blob->length) {
			return WERR_FOOBAR;
		}

		if (!convert_string_talloc(out->values, CH_UTF16, CH_UNIX,
					   in->value_ctr.values[i].blob->data+4,
					   in->value_ctr.values[i].blob->length-4,
					   (void **)&str, NULL, false)) {
			return WERR_FOOBAR;
		}

		out->values[i] = data_blob_string_const(str);
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_PRESENTATION_ADDRESS_ldb_to_drsuapi(const struct dsdb_syntax_ctx *ctx,
							      const struct dsdb_attribute *attr,
							      const struct ldb_message_element *in,
							      TALLOC_CTX *mem_ctx,
							      struct drsuapi_DsReplicaAttribute *out)
{
	unsigned int i;
	DATA_BLOB *blobs;

	if (attr->attributeID_id == DRSUAPI_ATTID_INVALID) {
		return WERR_FOOBAR;
	}

	out->attid			= dsdb_attribute_get_attid(attr,
								   ctx->is_schema_nc);
	out->value_ctr.num_values	= in->num_values;
	out->value_ctr.values		= talloc_array(mem_ctx,
						       struct drsuapi_DsAttributeValue,
						       in->num_values);
	W_ERROR_HAVE_NO_MEMORY(out->value_ctr.values);

	blobs = talloc_array(mem_ctx, DATA_BLOB, in->num_values);
	W_ERROR_HAVE_NO_MEMORY(blobs);

	for (i=0; i < in->num_values; i++) {
		uint8_t *data;
		size_t ret;

		out->value_ctr.values[i].blob	= &blobs[i];

		if (!convert_string_talloc(blobs, CH_UNIX, CH_UTF16,
					   in->values[i].data,
					   in->values[i].length,
					   (void **)&data, &ret, false)) {
			return WERR_FOOBAR;
		}

		blobs[i] = data_blob_talloc(blobs, NULL, 4 + ret);
		W_ERROR_HAVE_NO_MEMORY(blobs[i].data);

		SIVAL(blobs[i].data, 0, 4 + ret);

		if (ret > 0) {
			memcpy(blobs[i].data + 4, data, ret);
			talloc_free(data);
		}
	}

	return WERR_OK;
}

static WERROR dsdb_syntax_PRESENTATION_ADDRESS_validate_ldb(const struct dsdb_syntax_ctx *ctx,
							    const struct dsdb_attribute *attr,
							    const struct ldb_message_element *in)
{
	return dsdb_syntax_UNICODE_validate_ldb(ctx,
						attr,
						in);
}

#define OMOBJECTCLASS(val) { .length = sizeof(val) - 1, .data = discard_const_p(uint8_t, val) }

static const struct dsdb_syntax dsdb_syntaxes[] = {
	{
		.name			= "Boolean",
		.ldap_oid		= LDB_SYNTAX_BOOLEAN,
		.oMSyntax		= 1,
		.attributeSyntax_oid	= "2.5.5.8",
		.drsuapi_to_ldb		= dsdb_syntax_BOOL_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_BOOL_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_BOOL_validate_ldb,
		.equality               = "booleanMatch",
		.comment                = "Boolean"
	},{
		.name			= "Integer",
		.ldap_oid		= LDB_SYNTAX_INTEGER,
		.oMSyntax		= 2,
		.attributeSyntax_oid	= "2.5.5.9",
		.drsuapi_to_ldb		= dsdb_syntax_INT32_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_INT32_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_INT32_validate_ldb,
		.equality               = "integerMatch",
		.comment                = "Integer",
		.ldb_syntax		= LDB_SYNTAX_SAMBA_INT32
	},{
		.name			= "String(Octet)",
		.ldap_oid		= LDB_SYNTAX_OCTET_STRING,
		.oMSyntax		= 4,
		.attributeSyntax_oid	= "2.5.5.10",
		.drsuapi_to_ldb		= dsdb_syntax_DATA_BLOB_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DATA_BLOB_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DATA_BLOB_validate_ldb,
		.equality               = "octetStringMatch",
		.comment                = "Octet String",
	},{
		.name			= "String(Sid)",
		.ldap_oid		= LDB_SYNTAX_OCTET_STRING,
		.oMSyntax		= 4,
		.attributeSyntax_oid	= "2.5.5.17",
		.drsuapi_to_ldb		= dsdb_syntax_DATA_BLOB_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DATA_BLOB_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DATA_BLOB_validate_ldb,
		.equality               = "octetStringMatch",
		.comment                = "Octet String - Security Identifier (SID)",
		.ldb_syntax             = LDB_SYNTAX_SAMBA_SID
	},{
		.name			= "String(Object-Identifier)",
		.ldap_oid		= "1.3.6.1.4.1.1466.115.121.1.38",
		.oMSyntax		= 6,
		.attributeSyntax_oid	= "2.5.5.2",
		.drsuapi_to_ldb		= dsdb_syntax_OID_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_OID_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_OID_validate_ldb,
		.equality               = "caseIgnoreMatch", /* Would use "objectIdentifierMatch" but most are ldap attribute/class names */
		.comment                = "OID String",
		.ldb_syntax             = LDB_SYNTAX_DIRECTORY_STRING
	},{
		.name			= "Enumeration",
		.ldap_oid		= LDB_SYNTAX_INTEGER,
		.oMSyntax		= 10,
		.attributeSyntax_oid	= "2.5.5.9",
		.drsuapi_to_ldb		= dsdb_syntax_INT32_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_INT32_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_INT32_validate_ldb,
		.ldb_syntax		= LDB_SYNTAX_SAMBA_INT32
	},{
	/* not used in w2k3 forest */
		.name			= "String(Numeric)",
		.ldap_oid		= "1.3.6.1.4.1.1466.115.121.1.36",
		.oMSyntax		= 18,
		.attributeSyntax_oid	= "2.5.5.6",
		.drsuapi_to_ldb		= dsdb_syntax_DATA_BLOB_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DATA_BLOB_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DATA_BLOB_validate_ldb,
		.equality               = "numericStringMatch",
		.substring              = "numericStringSubstringsMatch",
		.comment                = "Numeric String",
		.ldb_syntax             = LDB_SYNTAX_DIRECTORY_STRING,
	},{
		.name			= "String(Printable)",
		.ldap_oid		= "1.3.6.1.4.1.1466.115.121.1.44",
		.oMSyntax		= 19,
		.attributeSyntax_oid	= "2.5.5.5",
		.drsuapi_to_ldb		= dsdb_syntax_DATA_BLOB_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DATA_BLOB_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DATA_BLOB_validate_ldb,
		.ldb_syntax		= LDB_SYNTAX_OCTET_STRING,
	},{
		.name			= "String(Teletex)",
		.ldap_oid		= "1.2.840.113556.1.4.905",
		.oMSyntax		= 20,
		.attributeSyntax_oid	= "2.5.5.4",
		.drsuapi_to_ldb		= dsdb_syntax_DATA_BLOB_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DATA_BLOB_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DATA_BLOB_validate_ldb,
		.equality               = "caseIgnoreMatch",
		.substring              = "caseIgnoreSubstringsMatch",
		.comment                = "Case Insensitive String",
		.ldb_syntax             = LDB_SYNTAX_DIRECTORY_STRING,
	},{
		.name			= "String(IA5)",
		.ldap_oid		= "1.3.6.1.4.1.1466.115.121.1.26",
		.oMSyntax		= 22,
		.attributeSyntax_oid	= "2.5.5.5",
		.drsuapi_to_ldb		= dsdb_syntax_DATA_BLOB_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DATA_BLOB_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DATA_BLOB_validate_ldb,
		.equality               = "caseExactIA5Match",
		.comment                = "Printable String",
		.ldb_syntax		= LDB_SYNTAX_OCTET_STRING,
	},{
		.name			= "String(UTC-Time)",
		.ldap_oid		= "1.3.6.1.4.1.1466.115.121.1.53",
		.oMSyntax		= 23,
		.attributeSyntax_oid	= "2.5.5.11",
		.drsuapi_to_ldb		= dsdb_syntax_NTTIME_UTC_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_NTTIME_UTC_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_NTTIME_UTC_validate_ldb,
		.equality               = "generalizedTimeMatch",
		.comment                = "UTC Time",
	},{
		.name			= "String(Generalized-Time)",
		.ldap_oid		= "1.3.6.1.4.1.1466.115.121.1.24",
		.oMSyntax		= 24,
		.attributeSyntax_oid	= "2.5.5.11",
		.drsuapi_to_ldb		= dsdb_syntax_NTTIME_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_NTTIME_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_NTTIME_validate_ldb,
		.equality               = "generalizedTimeMatch",
		.comment                = "Generalized Time",
		.ldb_syntax             = LDB_SYNTAX_UTC_TIME,
	},{
	/* not used in w2k3 schema */
		.name			= "String(Case Sensitive)",
		.ldap_oid		= "1.2.840.113556.1.4.1362",
		.oMSyntax		= 27,
		.attributeSyntax_oid	= "2.5.5.3",
		.drsuapi_to_ldb		= dsdb_syntax_DATA_BLOB_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DATA_BLOB_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DATA_BLOB_validate_ldb,
		.equality               = "caseExactMatch",
		.substring              = "caseExactSubstringsMatch",
		/* TODO (kim): according to LDAP rfc we should be using same comparison
		 * as Directory String (LDB_SYNTAX_DIRECTORY_STRING), but case sensitive.
		 * But according to ms docs binary compare should do the job:
		 * http://msdn.microsoft.com/en-us/library/cc223200(v=PROT.10).aspx */
		.ldb_syntax		= LDB_SYNTAX_OCTET_STRING,
	},{
		.name			= "String(Unicode)",
		.ldap_oid		= LDB_SYNTAX_DIRECTORY_STRING,
		.oMSyntax		= 64,
		.attributeSyntax_oid	= "2.5.5.12",
		.drsuapi_to_ldb		= dsdb_syntax_UNICODE_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_UNICODE_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_UNICODE_validate_ldb,
		.equality               = "caseIgnoreMatch",
		.substring              = "caseIgnoreSubstringsMatch",
		.comment                = "Directory String",
	},{
		.name			= "Interval/LargeInteger",
		.ldap_oid		= "1.2.840.113556.1.4.906",
		.oMSyntax		= 65,
		.attributeSyntax_oid	= "2.5.5.16",
		.drsuapi_to_ldb		= dsdb_syntax_INT64_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_INT64_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_INT64_validate_ldb,
		.equality               = "integerMatch",
		.comment                = "Large Integer",
		.ldb_syntax             = LDB_SYNTAX_INTEGER,
	},{
		.name			= "String(NT-Sec-Desc)",
		.ldap_oid		= LDB_SYNTAX_SAMBA_SECURITY_DESCRIPTOR,
		.oMSyntax		= 66,
		.attributeSyntax_oid	= "2.5.5.15",
		.drsuapi_to_ldb		= dsdb_syntax_DATA_BLOB_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DATA_BLOB_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DATA_BLOB_validate_ldb,
	},{
		.name			= "Object(DS-DN)",
		.ldap_oid		= LDB_SYNTAX_DN,
		.oMSyntax		= 127,
		.oMObjectClass		= OMOBJECTCLASS("\x2b\x0c\x02\x87\x73\x1c\x00\x85\x4a"),
		.attributeSyntax_oid	= "2.5.5.1",
		.drsuapi_to_ldb		= dsdb_syntax_DN_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DN_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DN_validate_ldb,
		.equality               = "distinguishedNameMatch",
		.comment                = "Object(DS-DN) == a DN",
	},{
		.name			= "Object(DN-Binary)",
		.ldap_oid		= DSDB_SYNTAX_BINARY_DN,
		.oMSyntax		= 127,
		.oMObjectClass		= OMOBJECTCLASS("\x2a\x86\x48\x86\xf7\x14\x01\x01\x01\x0b"),
		.attributeSyntax_oid	= "2.5.5.7",
		.drsuapi_to_ldb		= dsdb_syntax_DN_BINARY_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DN_BINARY_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DN_BINARY_validate_ldb,
		.equality               = "octetStringMatch",
		.comment                = "OctetString: Binary+DN",
	},{
	/* not used in w2k3 schema, but used in Exchange schema*/
		.name			= "Object(OR-Name)",
		.ldap_oid		= DSDB_SYNTAX_OR_NAME,
		.oMSyntax		= 127,
		.oMObjectClass		= OMOBJECTCLASS("\x56\x06\x01\x02\x05\x0b\x1D"),
		.attributeSyntax_oid	= "2.5.5.7",
		.drsuapi_to_ldb		= dsdb_syntax_DN_BINARY_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DN_BINARY_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DN_BINARY_validate_ldb,
		.equality		= "caseIgnoreMatch",
		.ldb_syntax		= LDB_SYNTAX_DN,
	},{
	/*
	 * TODO: verify if DATA_BLOB is correct here...!
	 *
	 *       repsFrom and repsTo are the only attributes using
	 *       this attribute syntax, but they're not replicated...
	 */
		.name			= "Object(Replica-Link)",
		.ldap_oid		= "1.3.6.1.4.1.1466.115.121.1.40",
		.oMSyntax		= 127,
		.oMObjectClass		= OMOBJECTCLASS("\x2a\x86\x48\x86\xf7\x14\x01\x01\x01\x06"),
		.attributeSyntax_oid	= "2.5.5.10",
		.drsuapi_to_ldb		= dsdb_syntax_DATA_BLOB_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DATA_BLOB_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DATA_BLOB_validate_ldb,
	},{
		.name			= "Object(Presentation-Address)",
		.ldap_oid		= "1.3.6.1.4.1.1466.115.121.1.43",
		.oMSyntax		= 127,
		.oMObjectClass		= OMOBJECTCLASS("\x2b\x0c\x02\x87\x73\x1c\x00\x85\x5c"),
		.attributeSyntax_oid	= "2.5.5.13",
		.drsuapi_to_ldb		= dsdb_syntax_PRESENTATION_ADDRESS_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_PRESENTATION_ADDRESS_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_PRESENTATION_ADDRESS_validate_ldb,
		.comment                = "Presentation Address",
		.ldb_syntax             = LDB_SYNTAX_DIRECTORY_STRING,
	},{
	/* not used in w2k3 schema */
		.name			= "Object(Access-Point)",
		.ldap_oid		= "1.3.6.1.4.1.1466.115.121.1.2",
		.oMSyntax		= 127,
		.oMObjectClass		= OMOBJECTCLASS("\x2b\x0c\x02\x87\x73\x1c\x00\x85\x3e"),
		.attributeSyntax_oid	= "2.5.5.14",
		.drsuapi_to_ldb		= dsdb_syntax_FOOBAR_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_FOOBAR_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_FOOBAR_validate_ldb,
		.ldb_syntax             = LDB_SYNTAX_DIRECTORY_STRING,
	},{
	/* not used in w2k3 schema */
		.name			= "Object(DN-String)",
		.ldap_oid		= DSDB_SYNTAX_STRING_DN,
		.oMSyntax		= 127,
		.oMObjectClass		= OMOBJECTCLASS("\x2a\x86\x48\x86\xf7\x14\x01\x01\x01\x0c"),
		.attributeSyntax_oid	= "2.5.5.14",
		.drsuapi_to_ldb		= dsdb_syntax_DN_STRING_drsuapi_to_ldb,
		.ldb_to_drsuapi		= dsdb_syntax_DN_STRING_ldb_to_drsuapi,
		.validate_ldb		= dsdb_syntax_DN_STRING_validate_ldb,
		.equality               = "octetStringMatch",
		.comment                = "OctetString: String+DN",
	}
};

const struct dsdb_syntax *find_syntax_map_by_ad_oid(const char *ad_oid)
{
	unsigned int i;
	for (i=0; dsdb_syntaxes[i].ldap_oid; i++) {
		if (strcasecmp(ad_oid, dsdb_syntaxes[i].attributeSyntax_oid) == 0) {
			return &dsdb_syntaxes[i];
		}
	}
	return NULL;
}

const struct dsdb_syntax *find_syntax_map_by_ad_syntax(int oMSyntax)
{
	unsigned int i;
	for (i=0; dsdb_syntaxes[i].ldap_oid; i++) {
		if (oMSyntax == dsdb_syntaxes[i].oMSyntax) {
			return &dsdb_syntaxes[i];
		}
	}
	return NULL;
}

const struct dsdb_syntax *find_syntax_map_by_standard_oid(const char *standard_oid)
{
	unsigned int i;
	for (i=0; dsdb_syntaxes[i].ldap_oid; i++) {
		if (strcasecmp(standard_oid, dsdb_syntaxes[i].ldap_oid) == 0) {
			return &dsdb_syntaxes[i];
		}
	}
	return NULL;
}

const struct dsdb_syntax *dsdb_syntax_for_attribute(const struct dsdb_attribute *attr)
{
	unsigned int i;

	for (i=0; i < ARRAY_SIZE(dsdb_syntaxes); i++) {
		if (attr->oMSyntax != dsdb_syntaxes[i].oMSyntax) continue;

		if (attr->oMObjectClass.length != dsdb_syntaxes[i].oMObjectClass.length) continue;

		if (attr->oMObjectClass.length) {
			int ret;
			ret = memcmp(attr->oMObjectClass.data,
				     dsdb_syntaxes[i].oMObjectClass.data,
				     attr->oMObjectClass.length);
			if (ret != 0) continue;
		}

		if (strcmp(attr->attributeSyntax_oid, dsdb_syntaxes[i].attributeSyntax_oid) != 0) continue;

		return &dsdb_syntaxes[i];
	}

	return NULL;
}

WERROR dsdb_attribute_drsuapi_to_ldb(struct ldb_context *ldb,
				     const struct dsdb_schema *schema,
				     const struct dsdb_schema_prefixmap *pfm_remote,
				     const struct drsuapi_DsReplicaAttribute *in,
				     TALLOC_CTX *mem_ctx,
				     struct ldb_message_element *out)
{
	const struct dsdb_attribute *sa;
	struct dsdb_syntax_ctx syntax_ctx;
	uint32_t attid_local;

	/* use default syntax conversion context */
	dsdb_syntax_ctx_init(&syntax_ctx, ldb, schema);
	syntax_ctx.pfm_remote = pfm_remote;

	switch (dsdb_pfm_get_attid_type(in->attid)) {
	case DSDB_ATTID_TYPE_PFM:
		/* map remote ATTID to local ATTID */
		if (!dsdb_syntax_attid_from_remote_attid(&syntax_ctx, mem_ctx, in->attid, &attid_local)) {
			DEBUG(0,(__location__ ": Can't find local ATTID for 0x%08X\n",
				 in->attid));
			return WERR_FOOBAR;
		}
		break;
	case DSDB_ATTID_TYPE_INTID:
		/* use IntId value directly */
		attid_local = in->attid;
		break;
	default:
		/* we should never get here */
		DEBUG(0,(__location__ ": Invalid ATTID type passed for conversion - 0x%08X\n",
			 in->attid));
		return WERR_INVALID_PARAMETER;
	}

	sa = dsdb_attribute_by_attributeID_id(schema, attid_local);
	if (!sa) {
		DEBUG(1,(__location__ ": Unknown attributeID_id 0x%08X\n", in->attid));
		return WERR_FOOBAR;
	}

	return sa->syntax->drsuapi_to_ldb(&syntax_ctx, sa, in, mem_ctx, out);
}

WERROR dsdb_attribute_ldb_to_drsuapi(struct ldb_context *ldb,
				     const struct dsdb_schema *schema,
				     const struct ldb_message_element *in,
				     TALLOC_CTX *mem_ctx,
				     struct drsuapi_DsReplicaAttribute *out)
{
	const struct dsdb_attribute *sa;
	struct dsdb_syntax_ctx syntax_ctx;

	sa = dsdb_attribute_by_lDAPDisplayName(schema, in->name);
	if (!sa) {
		return WERR_FOOBAR;
	}

	/* use default syntax conversion context */
	dsdb_syntax_ctx_init(&syntax_ctx, ldb, schema);

	return sa->syntax->ldb_to_drsuapi(&syntax_ctx, sa, in, mem_ctx, out);
}

