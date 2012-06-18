/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2005

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

#include "includes.h"
#include "ldb/include/includes.h"
#include "system/locale.h"

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
static int ldb_handler_fold(struct ldb_context *ldb, void *mem_ctx,
			    const struct ldb_val *in, struct ldb_val *out)
{
	char *s, *t;
	int l;
	if (!in || !out || !(in->data)) {
		return -1;
	}

	out->data = (uint8_t *)ldb_casefold(ldb, mem_ctx, (const char *)(in->data));
	if (out->data == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb_handler_fold: unable to casefold string [%s]", in->data);
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



/*
  canonicalise a ldap Integer
  rfc2252 specifies it should be in decimal form
*/
static int ldb_canonicalise_Integer(struct ldb_context *ldb, void *mem_ctx,
				    const struct ldb_val *in, struct ldb_val *out)
{
	char *end;
	long long i = strtoll((char *)in->data, &end, 0);
	if (*end != 0) {
		return -1;
	}
	out->data = (uint8_t *)talloc_asprintf(mem_ctx, "%lld", i);
	if (out->data == NULL) {
		return -1;
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
	return strtoll((char *)v1->data, NULL, 0) - strtoll((char *)v2->data, NULL, 0);
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
static int ldb_comparison_fold(struct ldb_context *ldb, void *mem_ctx,
			       const struct ldb_val *v1, const struct ldb_val *v2)
{
	const char *s1=(const char *)v1->data, *s2=(const char *)v2->data;
	const char *u1, *u2;
	char *b1, *b2;
	int ret;
	while (*s1 == ' ') s1++;
	while (*s2 == ' ') s2++;
	/* TODO: make utf8 safe, possibly with helper function from application */
	while (*s1 && *s2) {
		/* the first 127 (0x7F) chars are ascii and utf8 guarantes they
		 * never appear in multibyte sequences */
		if (((unsigned char)s1[0]) & 0x80) goto utf8str;
		if (((unsigned char)s2[0]) & 0x80) goto utf8str;
		if (toupper((unsigned char)*s1) != toupper((unsigned char)*s2))
			break;
		if (*s1 == ' ') {
			while (s1[0] == s1[1]) s1++;
			while (s2[0] == s2[1]) s2++;
		}
		s1++; s2++;
	}
	if (! (*s1 && *s2)) {
		/* check for trailing spaces only if one of the pointers
		 * has reached the end of the strings otherwise we
		 * can mistakenly match.
		 * ex. "domain users" <-> "domainUpdates"
		 */
		while (*s1 == ' ') s1++;
		while (*s2 == ' ') s2++;
	}
	return (int)(toupper(*s1)) - (int)(toupper(*s2));

utf8str:
	/* no need to recheck from the start, just from the first utf8 char found */
	b1 = ldb_casefold(ldb, mem_ctx, s1);
	b2 = ldb_casefold(ldb, mem_ctx, s2);

	if (b1 && b2) {
		/* Both strings converted correctly */

		u1 = b1;
		u2 = b2;
	} else {
		/* One of the strings was not UTF8, so we have no options but to do a binary compare */

		u1 = s1;
		u2 = s2;
	}

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

	dn = ldb_dn_explode_casefold(ldb, mem_ctx, (char *)in->data);
	if (dn == NULL) {
		return -1;
	}

	out->data = (uint8_t *)ldb_dn_linearize(mem_ctx, dn);
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

	dn1 = ldb_dn_explode_casefold(ldb, mem_ctx, (char *)v1->data);
	if (dn1 == NULL) return -1;

	dn2 = ldb_dn_explode_casefold(ldb, mem_ctx, (char *)v2->data);
	if (dn2 == NULL) {
		talloc_free(dn1);
		return -1;
	} 

	ret = ldb_dn_compare(ldb, dn1, dn2);

	talloc_free(dn1);
	talloc_free(dn2);
	return ret;
}

/*
  compare two objectclasses, looking at subclasses
*/
static int ldb_comparison_objectclass(struct ldb_context *ldb, void *mem_ctx,
				      const struct ldb_val *v1, const struct ldb_val *v2)
{
	int ret, i;
	const char **subclasses;
	ret = ldb_comparison_fold(ldb, mem_ctx, v1, v2);
	if (ret == 0) {
		return 0;
	}
	subclasses = ldb_subclass_list(ldb, (char *)v1->data);
	if (subclasses == NULL) {
		return ret;
	}
	for (i=0;subclasses[i];i++) {
		struct ldb_val vs;
		vs.data = discard_const_p(uint8_t, subclasses[i]);
		vs.length = strlen(subclasses[i]);
		if (ldb_comparison_objectclass(ldb, mem_ctx, &vs, v2) == 0) {
			return 0;
		}
	}
	return ret;
}

/*
  compare two utc time values. 1 second resolution
*/
static int ldb_comparison_utctime(struct ldb_context *ldb, void *mem_ctx,
				  const struct ldb_val *v1, const struct ldb_val *v2)
{
	time_t t1, t2;
	t1 = ldb_string_to_time((char *)v1->data);
	t2 = ldb_string_to_time((char *)v2->data);
	return (int)t2 - (int)t1;
}

/*
  canonicalise a utc time
*/
static int ldb_canonicalise_utctime(struct ldb_context *ldb, void *mem_ctx,
				    const struct ldb_val *in, struct ldb_val *out)
{
	time_t t = ldb_string_to_time((char *)in->data);
	out->data = (uint8_t *)ldb_timestring(mem_ctx, t);
	if (out->data == NULL) {
		return -1;
	}
	out->length = strlen((char *)out->data);
	return 0;
}

/*
  table of standard attribute handlers
*/
static const struct ldb_attrib_handler ldb_standard_attribs[] = {
	{ 
		.attr            = LDB_SYNTAX_INTEGER,
		.flags           = 0,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_canonicalise_Integer,
		.comparison_fn   = ldb_comparison_Integer
	},
	{ 
		.attr            = LDB_SYNTAX_OCTET_STRING,
		.flags           = 0,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_handler_copy,
		.comparison_fn   = ldb_comparison_binary
	},
	{ 
		.attr            = LDB_SYNTAX_DIRECTORY_STRING,
		.flags           = 0,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_handler_fold,
		.comparison_fn   = ldb_comparison_fold
	},
	{ 
		.attr            = LDB_SYNTAX_DN,
		.flags           = 0,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_canonicalise_dn,
		.comparison_fn   = ldb_comparison_dn
	},
	{ 
		.attr            = LDB_SYNTAX_OBJECTCLASS,
		.flags           = 0,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_handler_fold,
		.comparison_fn   = ldb_comparison_objectclass
	},
	{ 
		.attr            = LDB_SYNTAX_UTC_TIME,
		.flags           = 0,
		.ldif_read_fn    = ldb_handler_copy,
		.ldif_write_fn   = ldb_handler_copy,
		.canonicalise_fn = ldb_canonicalise_utctime,
		.comparison_fn   = ldb_comparison_utctime
	}
};


/*
  return the attribute handlers for a given syntax name
*/
const struct ldb_attrib_handler *ldb_attrib_handler_syntax(struct ldb_context *ldb,
							   const char *syntax)
{
	int i;
	unsigned num_handlers = sizeof(ldb_standard_attribs)/sizeof(ldb_standard_attribs[0]);
	/* TODO: should be replaced with a binary search */
	for (i=0;i<num_handlers;i++) {
		if (strcmp(ldb_standard_attribs[i].attr, syntax) == 0) {
			return &ldb_standard_attribs[i];
		}
	}
	return NULL;
}

