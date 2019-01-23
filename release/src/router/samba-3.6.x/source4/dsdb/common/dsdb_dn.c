/* 
   Unix SMB/CIFS implementation.
   Samba utility functions

   Copyright (C) Andrew Tridgell 2009
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

#include "includes.h"
#include "dsdb/samdb/samdb.h"
#include <ldb_module.h>
#include "librpc/ndr/libndr.h"
#include "libcli/security/dom_sid.h"

enum dsdb_dn_format dsdb_dn_oid_to_format(const char *oid) 
{
	if (strcmp(oid, LDB_SYNTAX_DN) == 0) {
		return DSDB_NORMAL_DN;
	} else if (strcmp(oid, DSDB_SYNTAX_BINARY_DN) == 0) {
		return DSDB_BINARY_DN;
	} else if (strcmp(oid, DSDB_SYNTAX_STRING_DN) == 0) {
		return DSDB_STRING_DN;
	} else if (strcmp(oid, DSDB_SYNTAX_OR_NAME) == 0) {
		return DSDB_NORMAL_DN;
	} else {
		return DSDB_INVALID_DN;
	}
}

static struct dsdb_dn *dsdb_dn_construct_internal(TALLOC_CTX *mem_ctx, 
						  struct ldb_dn *dn, 
						  DATA_BLOB extra_part, 
						  enum dsdb_dn_format dn_format, 
						  const char *oid) 
{
	struct dsdb_dn *dsdb_dn = talloc(mem_ctx, struct dsdb_dn);
	if (!dsdb_dn) {
		return NULL;
	}
	dsdb_dn->dn = talloc_steal(dsdb_dn, dn);
	dsdb_dn->extra_part = extra_part;
	dsdb_dn->dn_format = dn_format;
	/* Look to see if this attributeSyntax is a DN */
	if (dsdb_dn->dn_format == DSDB_INVALID_DN) {
		talloc_free(dsdb_dn);
		return NULL;
	}

	dsdb_dn->oid = oid;
	talloc_steal(dsdb_dn, extra_part.data);
	return dsdb_dn;
}

struct dsdb_dn *dsdb_dn_construct(TALLOC_CTX *mem_ctx, struct ldb_dn *dn, DATA_BLOB extra_part, 
				  const char *oid) 
{
	enum dsdb_dn_format dn_format = dsdb_dn_oid_to_format(oid);
	return dsdb_dn_construct_internal(mem_ctx, dn, extra_part, dn_format, oid);
}

struct dsdb_dn *dsdb_dn_parse(TALLOC_CTX *mem_ctx, struct ldb_context *ldb, 
			      const struct ldb_val *dn_blob, const char *dn_oid)
{
	struct dsdb_dn *dsdb_dn;
	struct ldb_dn *dn;
	const char *data;
	size_t len;
	TALLOC_CTX *tmp_ctx;
	char *p1;
	char *p2;
	uint32_t blen;
	struct ldb_val bval;
	struct ldb_val dval;
	char *dn_str;

	enum dsdb_dn_format dn_format = dsdb_dn_oid_to_format(dn_oid);
	switch (dn_format) {
	case DSDB_INVALID_DN:
		return NULL;
	case DSDB_NORMAL_DN:
	{
		dn = ldb_dn_from_ldb_val(mem_ctx, ldb, dn_blob);
		if (!dn || !ldb_dn_validate(dn)) {
			talloc_free(dn);
			return NULL;
		}
		return dsdb_dn_construct_internal(mem_ctx, dn, data_blob_null, dn_format, dn_oid);
	}
	case DSDB_BINARY_DN:
		if (dn_blob->length < 2 || dn_blob->data[0] != 'B' || dn_blob->data[1] != ':') {
			return NULL;
		}
		break;
	case DSDB_STRING_DN:
		if (dn_blob->length < 2 || dn_blob->data[0] != 'S' || dn_blob->data[1] != ':') {
			return NULL;
		}
		break;
	default:
		return NULL;
	}

	if (dn_blob && dn_blob->data
	    && (strlen((const char*)dn_blob->data) != dn_blob->length)) {
		/* The RDN must not contain a character with value 0x0 */
		return NULL;
	}
		
	if (!dn_blob->data || dn_blob->length == 0) {
		return NULL;
	}
		
	tmp_ctx = talloc_new(mem_ctx);
	if (tmp_ctx == NULL) {
		return NULL;
	}
		
	data = (const char *)dn_blob->data;

	len = dn_blob->length - 2;
	p1 = talloc_strndup(tmp_ctx, (const char *)dn_blob->data + 2, len);
	if (!p1) {
		goto failed;
	}

	errno = 0;
	blen = strtoul(p1, &p2, 10);
	if (errno != 0) {
		DEBUG(10, (__location__ ": failed\n"));
		goto failed;
	}
	if (p2 == NULL) {
		DEBUG(10, (__location__ ": failed\n"));
		goto failed;
	}
	if (p2[0] != ':') {
		DEBUG(10, (__location__ ": failed\n"));
		goto failed;
	}
	len -= PTR_DIFF(p2,p1);//???
	p1 = p2+1;
	len--;
		
	if (blen >= len) {
		DEBUG(10, (__location__ ": blen=%u len=%u\n", (unsigned)blen, (unsigned)len));
		goto failed;
	}
		
	p2 = p1 + blen;
	if (p2[0] != ':') {
		DEBUG(10, (__location__ ": %s", p2));
		goto failed;
	}
	dn_str = p2+1;
		
		
	switch (dn_format) {
	case DSDB_BINARY_DN:
		if ((blen % 2 != 0)) {
			DEBUG(10, (__location__ ": blen=%u - not an even number\n", (unsigned)blen));
			goto failed;
		}
		
		if (blen >= 2) {
			bval.length = (blen/2)+1;
			bval.data = talloc_size(tmp_ctx, bval.length);
			if (bval.data == NULL) {
				DEBUG(10, (__location__ ": err\n"));
				goto failed;
			}
			bval.data[bval.length-1] = 0;
		
			bval.length = strhex_to_str((char *)bval.data, bval.length,
						    p1, blen);
			if (bval.length != (blen / 2)) {
				DEBUG(10, (__location__ ": non hexidecimal characters found in binary prefix\n"));
				goto failed;
			}
		} else {
			bval = data_blob_null;
		}

		break;
	case DSDB_STRING_DN:
		bval = data_blob(p1, blen);
		break;
	default:
		/* never reached */
		return NULL;
	}
	

	dval.data = (uint8_t *)dn_str;
	dval.length = strlen(dn_str);
		
	dn = ldb_dn_from_ldb_val(tmp_ctx, ldb, &dval);
	if (!dn || !ldb_dn_validate(dn)) {
		DEBUG(10, (__location__ ": err\n"));
		goto failed;
	}
		
	dsdb_dn = dsdb_dn_construct(mem_ctx, dn, bval, dn_oid);
		
	return dsdb_dn;

failed:
	talloc_free(tmp_ctx);
	return NULL;
}


static char *dsdb_dn_get_with_postfix(TALLOC_CTX *mem_ctx, 
				     struct dsdb_dn *dsdb_dn,
				     const char *postfix)
{
	if (!postfix) {
		return NULL;
	}

	switch (dsdb_dn->dn_format) {
	case DSDB_NORMAL_DN:
	{
		return talloc_strdup(mem_ctx, postfix);
	}
	case DSDB_BINARY_DN:
	{
		char *hexstr = data_blob_hex_string_upper(mem_ctx, &dsdb_dn->extra_part);
	
		char *p = talloc_asprintf(mem_ctx, "B:%u:%s:%s", (unsigned)(dsdb_dn->extra_part.length*2), hexstr, 
					  postfix);
		talloc_free(hexstr);
		return p;
	}
	case DSDB_STRING_DN:
	{
		return talloc_asprintf(mem_ctx, "S:%u:%*.*s:%s", 
				    (unsigned)(dsdb_dn->extra_part.length), 
				    (int)(dsdb_dn->extra_part.length), 
				    (int)(dsdb_dn->extra_part.length), 
				    (const char *)dsdb_dn->extra_part.data, 
				    postfix);
	}
	default:
		return NULL;
	}
}

char *dsdb_dn_get_linearized(TALLOC_CTX *mem_ctx, 
			      struct dsdb_dn *dsdb_dn)
{
	const char *postfix = ldb_dn_get_linearized(dsdb_dn->dn);
	return dsdb_dn_get_with_postfix(mem_ctx, dsdb_dn, postfix);
}

char *dsdb_dn_get_casefold(TALLOC_CTX *mem_ctx, 
			   struct dsdb_dn *dsdb_dn) 
{
	const char *postfix = ldb_dn_get_casefold(dsdb_dn->dn);
	return dsdb_dn_get_with_postfix(mem_ctx, dsdb_dn, postfix);
}

char *dsdb_dn_get_extended_linearized(TALLOC_CTX *mem_ctx, 
				      struct dsdb_dn *dsdb_dn,
				      int mode)
{
	char *postfix = ldb_dn_get_extended_linearized(mem_ctx, dsdb_dn->dn, mode);
	char *ret = dsdb_dn_get_with_postfix(mem_ctx, dsdb_dn, postfix);
	talloc_free(postfix);
	return ret;
}

int dsdb_dn_binary_canonicalise(struct ldb_context *ldb, void *mem_ctx,
				const struct ldb_val *in, struct ldb_val *out)
{
	struct dsdb_dn *dsdb_dn = dsdb_dn_parse(mem_ctx, ldb, in, DSDB_SYNTAX_BINARY_DN);
	
	if (!dsdb_dn) {
		return -1;
	}
	*out = data_blob_string_const(dsdb_dn_get_casefold(mem_ctx, dsdb_dn));
	talloc_free(dsdb_dn);
	if (!out->data) {
		return -1;
	}
	return 0;
}

int dsdb_dn_binary_comparison(struct ldb_context *ldb, void *mem_ctx,
				     const struct ldb_val *v1,
				     const struct ldb_val *v2)
{
	return ldb_any_comparison(ldb, mem_ctx, dsdb_dn_binary_canonicalise, v1, v2);
}

int dsdb_dn_string_canonicalise(struct ldb_context *ldb, void *mem_ctx,
				const struct ldb_val *in, struct ldb_val *out)
{
	struct dsdb_dn *dsdb_dn = dsdb_dn_parse(mem_ctx, ldb, in, DSDB_SYNTAX_STRING_DN);
	
	if (!dsdb_dn) {
		return -1;
	}
	*out = data_blob_string_const(dsdb_dn_get_casefold(mem_ctx, dsdb_dn));
	talloc_free(dsdb_dn);
	if (!out->data) {
		return -1;
	}
	return 0;
}

int dsdb_dn_string_comparison(struct ldb_context *ldb, void *mem_ctx,
				     const struct ldb_val *v1,
				     const struct ldb_val *v2)
{
	return ldb_any_comparison(ldb, mem_ctx, dsdb_dn_string_canonicalise, v1, v2);
}

/*
  format a drsuapi_DsReplicaObjectIdentifier naming context as a string
 */
char *drs_ObjectIdentifier_to_string(TALLOC_CTX *mem_ctx,
				     struct drsuapi_DsReplicaObjectIdentifier *nc)
{
	char *ret = NULL;
	TALLOC_CTX *tmp_ctx = talloc_new(mem_ctx);
	if (!GUID_all_zero(&nc->guid)) {
		char *guid = GUID_string(tmp_ctx, &nc->guid);
		if (guid) {
			ret = talloc_asprintf_append(ret, "<GUID=%s>;", guid);
		}
	}
	if (nc->__ndr_size_sid != 0 && nc->sid.sid_rev_num != 0) {
		const char *sid = dom_sid_string(tmp_ctx, &nc->sid);
		if (sid) {
			ret = talloc_asprintf_append(ret, "<SID=%s>;", sid);
		}
	}
	if (nc->__ndr_size_dn != 0 && nc->dn) {
		ret = talloc_asprintf_append(ret, "%s", nc->dn);
	}
	talloc_free(tmp_ctx);
	talloc_steal(mem_ctx, ret);
	return ret;
}

struct ldb_dn *drs_ObjectIdentifier_to_dn(TALLOC_CTX *mem_ctx,
					  struct ldb_context *ldb,
					  struct drsuapi_DsReplicaObjectIdentifier *nc)
{
	char *dn_string = drs_ObjectIdentifier_to_string(mem_ctx, nc);
	struct ldb_dn *new_dn;
	new_dn = ldb_dn_new(mem_ctx, ldb, dn_string);
	talloc_free(dn_string);
	return new_dn;
}
