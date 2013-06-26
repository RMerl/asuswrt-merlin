/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2005
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006-2009

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
/*
  attribute handlers for well known attribute types, selected by syntax OID
  see rfc2252
*/

#include "ldb_private.h"
#include "system/locale.h"
#include "ldb_handlers.h"

/*
  default handler that just copies a ldb_val.
*/
int ldb_handler_copy(struct ldb_context *ldb, void *mem_ctx,
		     const struct ldb_val *in, struct ldb_val *out)
{
	*out = ldb_val_dup(mem_ctx, in);
	if (in->length > 0 && out->data == NULL) {
		ldb_oom(ldb);
		return -1;
	}
	return 0;
}

/*
  a case folding copy handler, removing leading and trailing spaces and
  multiple internal spaces

  We exploit the fact that utf8 never uses the space octet except for
  the space itself
*/
int ldb_handler_fold(struct ldb_context *ldb, void *mem_ctx,
			    const struct ldb_val *in, struct ldb_val *out)
{
	char *s, *t;
	size_t l;

	if (!in || !out || !(in->data)) {
		return -1;
	}

	out->data = (uint8_t *)ldb_casefold(ldb, mem_ctx, (const char *)(in->data), in->length);
	if (out->data == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb_handler_fold: unable to casefold string [%.*s]", (int)in->length, (const char *)in->data);
		return -1;
	}

	s = (char *)(out->data);
	
	/* remove trailing spaces if any */
	l = strlen(s);
	while (l > 0 && s[l - 1] == ' ') l--;
	s[l] = '\0';
	
	/* remove leading spaces if any */
	if (*s == ' ') {
		for (t = s; *s == ' '; s++) ;

		/* remove leading spaces by moving down the string */
		memmove(t, s, l);

		s = t;
	}

	/* check middle spaces */
	while ((t = strchr(s, ' ')) != NULL) {
		for (s = t; *s == ' '; s++) ;

		if ((s - t) > 1) {
			l = strlen(s);

			/* remove all spaces but one by moving down the string */
			memmove(t + 1, s, l);
		}
	}

	out->length = strlen((char *)out->data);
	return 0;
}

/* length limited conversion of a ldb_val to a int32_t */
static int val_to_int64(const struct ldb_val *in, int64_t *v)
{
	char *end;
	char buf[64];

	/* make sure we don't read past the end of the data */
	if (in->length > sizeof(buf)-1) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}
	strncpy(buf, (char *)in->data, in->length);
	buf[in->length] = 0;

	/* We've to use "strtoll" here to have the intended overflows.
	 * Otherwise we may get "LONG_MAX" and the conversion is wrong. */
	*v = (int64_t) strtoll(buf, &end, 0);
	if (*end != 0) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}
	return LDB_SUCCESS;
}


/*
  canonicalise a ldap Integer
  rfc2252 specifies it should be in decimal form
*/
static int ldb_canonicalise_Integer(struct ldb_context *ldb, void *mem_ctx,
				    const struct ldb_val *in, struct ldb_val *out)
{
	int64_t i;
	int ret;

	ret = val_to_int64(in, &i);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	out->data = (uint8_t *) talloc_asprintf(mem_ctx, "%lld", (long long)i);
	if (out->data == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	out->length = strlen((char *)out->data);
	return 0;
}

/*
  compare two Integers
*/
static int ldb_comparison_Integer(struct ldb_context *ldb, void *mem_ctx,
				  const struct ldb_val *v1, const struct ldb_val *v2)
{
	int64_t i1=0, i2=0;
	val_to_int64(v1, &i1);
	val_to_int64(v2, &i2);
	if (i1 == i2) return 0;
	return i1 > i2? 1 : -1;
}

/*
  canonicalise a ldap Boolean
  rfc2252 specifies it should be either "TRUE" or "FALSE"
*/
static int ldb_canonicalise_Boolean(struct ldb_context *ldb, void *mem_ctx,
			     const struct ldb_val *in, struct ldb_val *out)
{
	if (strncasecmp((char *)in->data, "TRUE", in->length) == 0) {
		out->data = (uint8_t *)talloc_strdup(mem_ctx, "TRUE");
		out->length = 4;
	} else if (strncasecmp((char *)in->data, "FALSE", in->length) == 0) {
		out->data = (uint8_t *)talloc_strdup(mem_ctx, "FALSE");
		out->length = 4;
	} else {
		return -1;
	}
	return 0;
}

/*
  compare two Booleans
*/
static int ldb_comparison_Boolean(struct ldb_context *ldb, void *mem_ctx,
			   const struct ldb_val *v1, const struct ldb_val *v2)
{
	if (v1->length != v2->length) {
		return v1->length - v2->length;
	}
	return strncasecmp((char *)v1->data, (char *)v2->data, v1->length);
}


/*
  compare two binary blobs
*/
int ldb_comparison_binary(struct ldb_context *ldb, void *mem_ctx,
			  const struct ldb_val *v1, const struct ldb_val *v2)
{
	if (v1->length != v2->length) {
		return v1->length - v2->length;
	}
	return memcmp(v1->data, v2->data, v1->length);
}

/*
  compare two case insensitive strings, ignoring multiple whitespaces
  and leading and trailing whitespaces
  see rfc2252 section 8.1
	
  try to optimize for the ascii case,
  but if we find out an utf8 codepoint revert to slower but correct function
*/
int ldb_comparison_fold(struct ldb_context *ldb, void *mem_ctx,
			       const struct ldb_val *v1, const struct ldb_val *v2)
{
	const char *s1=(const char *)v1->data, *s2=(const char *)v2->data;
	size_t n1 = v1->length, n2 = v2->length;
	char *b1, *b2;
	const char *u1, *u2;
	int ret;
	while (n1 && *s1 == ' ') { s1++; n1--; };
	while (n2 && *s2 == ' ') { s2++; n2--; };

	while (n1 && n2 && *s1 && *s2) {
		/* the first 127 (0x7F) chars are ascii and utf8 guarantes they
		 * never appear in multibyte sequences */
		if (((unsigned char)s1[0]) & 0x80) goto utf8str;
		if (((unsigned char)s2[0]) & 0x80) goto utf8str;
		if (toupper((unsigned char)*s1) != toupper((unsigned char)*s2))
			break;
		if (*s1 == ' ') {
			while (n1 && s1[0] == s1[1]) { s1++; n1--; }
			while (n2 && s2[0] == s2[1]) { s2++; n2--; }
		}
		s1++; s2++;
		n1--; n2--;
	}

	/* check for trailing spaces only if the other pointers has
	 * reached the end of the strings otherwise we can
	 * mistakenly match.  ex. "domain users" <->
	 * "domainUpdates"
	 */
	if (n1 && *s1 == ' ' && (!n2 || !*s2)) {
		while (n1 && *s1 == ' ') { s1++; n1--; }		
	}
	if (n2 && *s2 == ' ' && (!n1 || !*s1)) {
		while (n2 && *s2 == ' ') { s2++; n2--; }		
	}
	if (n1 == 0 && n2 != 0) {
		return -(int)toupper(*s2);
	}
	if (n2 == 0 && n1 != 0) {
		return (int)toupper(*s1);
	}
	if (n2 == 0 && n2 == 0) {
		return 0;
	}
	return (int)toupper(*s1) - (int)toupper(*s2);

utf8str:
	/* no need to recheck from the start, just from the first utf8 char found */
	b1 = ldb_casefold(ldb, mem_ctx, s1, n1);
	b2 = ldb_casefold(ldb, mem_ctx, s2, n2);

	if (!b1 || !b2) {
		/* One of the strings was not UTF8, so we have no
		 * options but to do a binary compare */
		talloc_free(b1);
		talloc_free(b2);
		ret = memcmp(s1, s2, MIN(n1, n2));
		if (ret == 0) {
			if (n1 == n2) return 0;
			if (n1 > n2) {
				return (int)toupper(s1[n2]);
			} else {
				return -(int)toupper(s2[n1]);
			}
		}
		return ret;
	}

	u1 = b1;
	u2 = b2;

	while (*u1 & *u2) {
		if (*u1 != *u2)
			break;
		if (*u1 == ' ') {
			while (u1[0] == u1[1]) u1++;
			while (u2[0] == u2[1]) u2++;
		}
		u1++; u2++;
	}
	if (! (*u1 && *u2)) {
		while (*u1 == ' ') u1++;
		while (*u2 == ' ') u2++;
	}
	ret = (int)(*u1 - *u2);

	talloc_free(b1);
	talloc_free(b2);
	
	return ret;
}


/*
  canonicalise a attribute in DN format
*/
static int ldb_canonicalise_dn(struct ldb_context *ldb, void *mem_ctx,
			       const struct ldb_val *in, struct ldb_val *out)
{
	struct ldb_dn *dn;
	int ret = -1;

	out->length = 0;
	out->data = NULL;

	dn = ldb_dn_from_ldb_val(mem_ctx, ldb, in);
	if ( ! ldb_dn_validate(dn)) {
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	out->data = (uint8_t *)ldb_dn_alloc_casefold(mem_ctx, dn);
	if (out->data == NULL) {
		goto done;
	}
	out->length = strlen((char *)out->data);

	ret = 0;

done:
	talloc_free(dn);

	return ret;
}

/*
  compare two dns
*/
static int ldb_comparison_dn(struct ldb_context *ldb, void *mem_ctx,
			     const struct ldb_val *v1, const struct ldb_val *v2)
{
	struct ldb_dn *dn1 = NULL, *dn2 = NULL;
	int ret;

	dn1 = ldb_dn_from_ldb_val(mem_ctx, ldb, v1);
	if ( ! ldb_dn_validate(dn1)) return -1;

	dn2 = ldb_dn_from_ldb_val(mem_ctx, ldb, v2);
	if ( ! ldb_dn_validate(dn2)) {
		talloc_free(dn1);
		return -1;
	} 

	ret = ldb_dn_compare(dn1, dn2);

	talloc_free(dn1);
	talloc_free(dn2);
	return ret;
}

/*
  compare two utc time values. 1 second resolution
*/
static int ldb_comparison_utctime(struct ldb_context *ldb, void *mem_ctx,
				  const struct ldb_val *v1, const struct ldb_val *v2)
{
	time_t t1=0, t2=0;
	ldb_val_to_time(v1, &t1);
	ldb_val_to_time(v2, &t2);
	if (t1 == t2) return 0;
	return t1 > t2? 1 : -1;
}

/*
  canonicalise a utc time
*/
static int ldb_canonicalise_utctime(struct ldb_context *ldb, void *mem_ctx,
				    const struct ldb_val *in, struct ldb_val *out)
{
	time_t t;
	int ret;
	ret = ldb_val_to_time(in, &t);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	out->data = (uint8_t *)ldb_timestring(mem_ctx, t);
	if (out->data == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	out->length = strlen((char *)out->data);
	return 0;
}

/*
  table of standard attribute handlers
*/
static const struct ldb_schema_syntax ldb_standard_syntaxes[] = {
	{ 
		.name            = LDB_SYNTAX_INTEGER,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_canonicalise_Integer,
		.comparison_fn   = ldb_comparison_Integer
	},
	{ 
		.name            = LDB_SYNTAX_OCTET_STRING,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_handler_copy,
		.comparison_fn   = ldb_comparison_binary
	},
	{ 
		.name            = LDB_SYNTAX_DIRECTORY_STRING,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_handler_fold,
		.comparison_fn   = ldb_comparison_fold
	},
	{ 
		.name            = LDB_SYNTAX_DN,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_canonicalise_dn,
		.comparison_fn   = ldb_comparison_dn
	},
	{ 
		.name            = LDB_SYNTAX_OBJECTCLASS,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_handler_fold,
		.comparison_fn   = ldb_comparison_fold
	},
	{ 
		.name            = LDB_SYNTAX_UTC_TIME,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_canonicalise_utctime,
		.comparison_fn   = ldb_comparison_utctime
	},
	{ 
		.name            = LDB_SYNTAX_BOOLEAN,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_canonicalise_Boolean,
		.comparison_fn   = ldb_comparison_Boolean
	},
};


/*
  return the attribute handlers for a given syntax name
*/
const struct ldb_schema_syntax *ldb_standard_syntax_by_name(struct ldb_context *ldb,
							    const char *syntax)
{
	unsigned int i;
	unsigned num_handlers = sizeof(ldb_standard_syntaxes)/sizeof(ldb_standard_syntaxes[0]);
	/* TODO: should be replaced with a binary search */
	for (i=0;i<num_handlers;i++) {
		if (strcmp(ldb_standard_syntaxes[i].name, syntax) == 0) {
			return &ldb_standard_syntaxes[i];
		}
	}
	return NULL;
}

int ldb_any_comparison(struct ldb_context *ldb, void *mem_ctx, 
		       ldb_attr_handler_t canonicalise_fn, 
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
	ret1 = canonicalise_fn(ldb, tmp_ctx, v1, &v1_canon);
	ret2 = canonicalise_fn(ldb, tmp_ctx, v2, &v2_canon);

	if (ret1 == LDB_SUCCESS && ret2 == LDB_SUCCESS) {
		ret = ldb_comparison_binary(ldb, mem_ctx, &v1_canon, &v2_canon);
	} else {
		ret = ldb_comparison_binary(ldb, mem_ctx, v1, v2);
	}
	talloc_free(tmp_ctx);
	return ret;
}
