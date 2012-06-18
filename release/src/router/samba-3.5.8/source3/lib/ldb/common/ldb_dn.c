/* 
   ldb database library

   Copyright (C) Simo Sorce 2005

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
 *  Name: ldb
 *
 *  Component: ldb dn explode and utility functions
 *
 *  Description: - explode a dn into its own basic elements
 *                 and put them in a structure
 *               - manipulate ldb_dn structures
 *
 *  Author: Simo Sorce
 */

#include "includes.h"
#include "ldb/include/includes.h"

#define LDB_DN_NULL_FAILED(x) if (!(x)) goto failed

#define LDB_SPECIAL "@SPECIAL"

/**
   internal ldb exploded dn structures
*/
struct ldb_dn_component {
	char *name;  
	struct ldb_val value;
};

struct ldb_dn {
	int comp_num;
	struct ldb_dn_component *components;
};

int ldb_dn_is_special(const struct ldb_dn *dn)
{
	if (dn == NULL || dn->comp_num != 1) return 0;

	return ! strcmp(dn->components[0].name, LDB_SPECIAL);
}

int ldb_dn_check_special(const struct ldb_dn *dn, const char *check)
{
	if (dn == NULL || dn->comp_num != 1) return 0;

	return ! strcmp((char *)dn->components[0].value.data, check);
}

char *ldb_dn_escape_value(void *mem_ctx, struct ldb_val value)
{
	const char *p, *s, *src;
	char *d, *dst;
	int len;

	if (!value.length)
		return NULL;

	p = s = src = (const char *)value.data;
	len = value.length;

	/* allocate destination string, it will be at most 3 times the source */
	dst = d = talloc_array(mem_ctx, char, len * 3 + 1);
	LDB_DN_NULL_FAILED(dst);

	while (p - src < len) {

		p += strcspn(p, ",=\n+<>#;\\\"");

		if (p - src == len) /* found no escapable chars */
			break;

		memcpy(d, s, p - s); /* copy the part of the string before the stop */
		d += (p - s); /* move to current position */

		if (*p) { /* it is a normal escapable character */
			*d++ = '\\';
			*d++ = *p++;
		} else { /* we have a zero byte in the string */
			strncpy(d, "\00", 3); /* escape the zero */
			d = d + 3;
			p++; /* skip the zero */
		}
		s = p; /* move forward */
	}

	/* copy the last part (with zero) and return */
	memcpy(d, s, &src[len] - s + 1);

	return dst;

failed:
	talloc_free(dst);
	return NULL;
}

static struct ldb_val ldb_dn_unescape_value(void *mem_ctx, const char *src)
{
	struct ldb_val value;
	unsigned x;
	char *p, *dst = NULL, *end;

	memset(&value, 0, sizeof(value));

	LDB_DN_NULL_FAILED(src);

	dst = p = (char *)talloc_memdup(mem_ctx, src, strlen(src) + 1);
	LDB_DN_NULL_FAILED(dst);

	end = &dst[strlen(dst)];

	while (*p) {
		p += strcspn(p, ",=\n+<>#;\\\"");

		if (*p == '\\') {
			if (strchr(",=\n+<>#;\\\"", p[1])) {
				memmove(p, p + 1, end - (p + 1) + 1);
				end--;
				p++;
				continue;
			}

			if (sscanf(p + 1, "%02x", &x) == 1) {
				*p = (unsigned char)x;
				memmove(p + 1, p + 3, end - (p + 3) + 1);
				end -= 2;
				p++;
				continue;
			}
		}

		/* a string with not escaped specials is invalid (tested) */
		if (*p != '\0') {
			goto failed;
		}
	}

	value.length = end - dst;
	value.data = (uint8_t *)dst;
	return value;

failed:
	talloc_free(dst);
	return value;
}

/* check if the string contains quotes
 * skips leading and trailing spaces
 * - returns 0 if no quotes found
 * - returns 1 if quotes are found and put their position
 *   in *quote_start and *quote_end parameters
 * - return -1 if there are open quotes
 */

static int get_quotes_position(const char *source, int *quote_start, int *quote_end)
{
	const char *p;

	if (source == NULL || quote_start == NULL || quote_end == NULL) return -1;

	p = source;

	/* check if there are quotes surrounding the value */
	p += strspn(p, " \n"); /* skip white spaces */

	if (*p == '\"') {
		*quote_start = p - source;

		p++;
		while (*p != '\"') {
			p = strchr(p, '\"');
			LDB_DN_NULL_FAILED(p);

			if (*(p - 1) == '\\')
				p++;
		}

		*quote_end = p - source;
		return 1;
	}

	return 0;

failed:
	return -1;
}

static char *seek_to_separator(char *string, const char *separators)
{
	char *p, *q;
	int ret, qs, qe, escaped;

	if (string == NULL || separators == NULL) return NULL;

	p = strchr(string, '=');
	LDB_DN_NULL_FAILED(p);

	p++;

	/* check if there are quotes surrounding the value */

	ret = get_quotes_position(p, &qs, &qe);
	if (ret == -1)
		return NULL;

	if (ret == 1) { /* quotes found */

		p += qe; /* positioning after quotes */
		p += strspn(p, " \n"); /* skip white spaces after the quote */

		if (strcspn(p, separators) != 0) /* if there are characters between quotes */
			return NULL;	    /* and separators, the dn is invalid */

		return p; /* return on the separator */
	}

	/* no quotes found seek to separators */
	q = p;
	do {
		escaped = 0;
		ret = strcspn(q, separators);
		
		if (q[ret - 1] == '\\') {
			escaped = 1;
			q = q + ret + 1;
		}
	} while (escaped);

	if (ret == 0 && p == q) /* no separators ?! bail out */
		return NULL;

	return q + ret;

failed:
	return NULL;
}

static char *ldb_dn_trim_string(char *string, const char *edge)
{
	char *s, *p;

	/* seek out edge from start of string */
	s = string + strspn(string, edge);

	/* backwards skip from end of string */
	p = &s[strlen(s) - 1];
	while (p > s && strchr(edge, *p)) {
		*p = '\0';
		p--;
	}

	return s;
}

/* we choosed to not support multpile valued components */
static struct ldb_dn_component ldb_dn_explode_component(void *mem_ctx, char *raw_component)
{
	struct ldb_dn_component dc;
	char *p;
	int ret, qs, qe;

	memset(&dc, 0, sizeof(dc));

	if (raw_component == NULL) {
		return dc;
	}

	/* find attribute type/value separator */
	p = strchr(raw_component, '=');
	LDB_DN_NULL_FAILED(p);

	*p++ = '\0'; /* terminate name and point to value */

	/* copy and trim name in the component */
	dc.name = talloc_strdup(mem_ctx, ldb_dn_trim_string(raw_component, " \n"));
	if (!dc.name)
		return dc;

	if (! ldb_valid_attr_name(dc.name)) {
		goto failed;
	}

	ret = get_quotes_position(p, &qs, &qe);

	switch (ret) {
	case 0: /* no quotes trim the string */
		p = ldb_dn_trim_string(p, " \n");
		dc.value = ldb_dn_unescape_value(mem_ctx, p);
		break;

	case 1: /* quotes found get the unquoted string */
		p[qe] = '\0';
		p = p + qs + 1;
		dc.value.length = strlen(p);
		dc.value.data = (uint8_t *)talloc_memdup(mem_ctx, p,
							 dc.value.length + 1);
		break;

	default: /* mismatched quotes ot other error, bail out */
		goto failed;
	}

	if (dc.value.length == 0) {
		goto failed;
	}

	return dc;

failed:
	talloc_free(dc.name);
	dc.name = NULL;
	return dc;
}

struct ldb_dn *ldb_dn_new(void *mem_ctx, struct ldb_context *ldb, const char *text)
{
	struct ldb_dn *edn;

	if (text == NULL) {
		edn = talloc_zero(mem_ctx, struct ldb_dn);
	} else {
		edn = ldb_dn_explode(mem_ctx, text);
	}

	return edn;
}

bool ldb_dn_validate(struct ldb_dn *dn)
{
	/* This implementation does not do "lazy" evaluation of DN's, so 
	 * if a DN can be created it will be valid. */
	return true;
}

struct ldb_dn *ldb_dn_new_fmt(void *mem_ctx, struct ldb_context *ldb, const char *new_fmt, ...)
{
	char *strdn;
	va_list ap;
	struct ldb_dn *dn;

	if ( (! mem_ctx) || (! ldb)) return NULL;

	va_start(ap, new_fmt);
	strdn = talloc_vasprintf(mem_ctx, new_fmt, ap);
	va_end(ap);
	if (strdn == NULL)
		return NULL;

	dn = ldb_dn_explode(mem_ctx, strdn);

	talloc_free(strdn);
	return dn;
}

/*
  explode a DN string into a ldb_dn structure
*/
struct ldb_dn *ldb_dn_explode(void *mem_ctx, const char *dn)
{
	struct ldb_dn *edn; /* the exploded dn */
	char *pdn, *p;

	if (dn == NULL) return NULL;

	/* Allocate a structure to hold the exploded DN */
	edn = talloc_zero(mem_ctx, struct ldb_dn);
	if (edn == NULL) {
		return NULL;
	}

	pdn = NULL;

	/* Empty DNs */
	if (dn[0] == '\0') {
		return edn;
	}

	/* Special DNs case */
	if (dn[0] == '@') {
		edn->comp_num = 1;
		edn->components = talloc(edn, struct ldb_dn_component);
		if (edn->components == NULL) goto failed;
		edn->components[0].name = talloc_strdup(edn->components, LDB_SPECIAL);
		if (edn->components[0].name == NULL) goto failed;
		edn->components[0].value.data = (uint8_t *)talloc_strdup(edn->components, dn);
		if (edn->components[0].value.data== NULL) goto failed;
		edn->components[0].value.length = strlen(dn);
		return edn;
	}

	pdn = p = talloc_strdup(edn, dn);
	LDB_DN_NULL_FAILED(pdn);

	/* get the components */
	do {
		char *t;

		/* terminate the current component and return pointer to the next one */
		t = seek_to_separator(p, ",;");
		LDB_DN_NULL_FAILED(t);

		if (*t) { /* here there is a separator */
			*t = '\0'; /*terminate */
			t++; /* a separtor means another component follows */
		}

		/* allocate space to hold the dn component */
		edn->components = talloc_realloc(edn, edn->components,
						 struct ldb_dn_component,
						 edn->comp_num + 1);
		if (edn->components == NULL)
			goto failed;

		/* store the exploded component in the main structure */
		edn->components[edn->comp_num] = ldb_dn_explode_component(edn, p);
		LDB_DN_NULL_FAILED(edn->components[edn->comp_num].name);

		edn->comp_num++;

		/* jump to the next component if any */
		p = t;

	} while(*p);

	talloc_free(pdn);
	return edn;

failed:
	talloc_free(pdn);
	talloc_free(edn);
	return NULL;
}

struct ldb_dn *ldb_dn_explode_or_special(void *mem_ctx, const char *dn)
{
	struct ldb_dn *edn; /* the exploded dn */

	if (dn == NULL) return NULL;

	if (strncasecmp(dn, "<GUID=", 6) == 0) {
		/* this is special DN returned when the
		 * exploded_dn control is used
		 */

		/* Allocate a structure to hold the exploded DN */
		if (!(edn = talloc_zero(mem_ctx, struct ldb_dn))) {
			return NULL;
		}

		edn->comp_num = 1;
		edn->components = talloc(edn, struct ldb_dn_component);
		if (edn->components == NULL) goto failed;
		edn->components[0].name = talloc_strdup(edn->components, LDB_SPECIAL);
		if (edn->components[0].name == NULL) goto failed;
		edn->components[0].value.data = (uint8_t *)talloc_strdup(edn->components, dn);
		if (edn->components[0].value.data== NULL) goto failed;
		edn->components[0].value.length = strlen(dn);
		return edn;

	}
	
	return ldb_dn_explode(mem_ctx, dn);

failed:
	talloc_free(edn);
	return NULL;
}

char *ldb_dn_linearize(void *mem_ctx, const struct ldb_dn *edn)
{
	char *dn, *value;
	int i;

	if (edn == NULL) return NULL;

	/* Special DNs */
	if (ldb_dn_is_special(edn)) {
		dn = talloc_strdup(mem_ctx, (char *)edn->components[0].value.data);
		return dn;
	}

	dn = talloc_strdup(mem_ctx, "");
	LDB_DN_NULL_FAILED(dn);

	for (i = 0; i < edn->comp_num; i++) {
		value = ldb_dn_escape_value(dn, edn->components[i].value);
		LDB_DN_NULL_FAILED(value);

		if (i == 0) {
			dn = talloc_asprintf_append(dn, "%s=%s", edn->components[i].name, value);
		} else {
			dn = talloc_asprintf_append(dn, ",%s=%s", edn->components[i].name, value);
		}
		LDB_DN_NULL_FAILED(dn);

		talloc_free(value);
	}

	return dn;

failed:
	talloc_free(dn);
	return NULL;
}

/* Determine if dn is below base, in the ldap tree.  Used for
 * evaluating a subtree search.
 * 0 if they match, otherwise non-zero
 */

int ldb_dn_compare_base(struct ldb_context *ldb,
			const struct ldb_dn *base,
			const struct ldb_dn *dn)
{
	int ret;
	int n0, n1;

	if (base == NULL || base->comp_num == 0) return 0;
	if (dn == NULL || dn->comp_num == 0) return -1;

	/* if the base has more componts than the dn, then they differ */
	if (base->comp_num > dn->comp_num) {
		return (dn->comp_num - base->comp_num);
	}

	n0 = base->comp_num - 1;
	n1 = dn->comp_num - 1;
	while (n0 >= 0 && n1 >= 0) {
		const struct ldb_attrib_handler *h;

		/* compare names (attribute names are guaranteed to be ASCII only) */
		ret = ldb_attr_cmp(base->components[n0].name,
				   dn->components[n1].name);
		if (ret) {
			return ret;
		}

		/* names match, compare values */
		h = ldb_attrib_handler(ldb, base->components[n0].name);
		ret = h->comparison_fn(ldb, ldb, &(base->components[n0].value),
						  &(dn->components[n1].value));
		if (ret) {
			return ret;
		}
		n1--;
		n0--;
	}

	return 0;
}

/* compare DNs using casefolding compare functions.  

   If they match, then return 0
 */

int ldb_dn_compare(struct ldb_context *ldb,
		   const struct ldb_dn *edn0,
		   const struct ldb_dn *edn1)
{
	if (edn0 == NULL || edn1 == NULL) return edn1 - edn0;

	if (edn0->comp_num != edn1->comp_num)
		return (edn1->comp_num - edn0->comp_num);

	return ldb_dn_compare_base(ldb, edn0, edn1);
}

int ldb_dn_cmp(struct ldb_context *ldb, const char *dn0, const char *dn1)
{
	struct ldb_dn *edn0;
	struct ldb_dn *edn1;
	int ret;

	if (dn0 == NULL || dn1 == NULL) return dn1 - dn0;

	edn0 = ldb_dn_explode_casefold(ldb, ldb, dn0);
	if (edn0 == NULL) return 1;

	edn1 = ldb_dn_explode_casefold(ldb, ldb, dn1);
	if (edn1 == NULL) {
		talloc_free(edn0);
		return -1;
	}

	ret = ldb_dn_compare(ldb, edn0, edn1);

	talloc_free(edn0);
	talloc_free(edn1);

	return ret;
}

/*
  casefold a dn. We need to casefold the attribute names, and canonicalize 
  attribute values of case insensitive attributes.
*/
struct ldb_dn *ldb_dn_casefold(struct ldb_context *ldb, void *mem_ctx, const struct ldb_dn *edn)
{
	struct ldb_dn *cedn;
	int i, ret;

	if (edn == NULL) return NULL;

	cedn = talloc_zero(mem_ctx, struct ldb_dn);
	if (!cedn) {
		return NULL;
	}

	cedn->comp_num = edn->comp_num;
	cedn->components = talloc_array(cedn, struct ldb_dn_component, edn->comp_num);
	if (!cedn->components) {
		talloc_free(cedn);
		return NULL;
	}

	for (i = 0; i < edn->comp_num; i++) {
		struct ldb_dn_component dc;
		const struct ldb_attrib_handler *h;

		memset(&dc, 0, sizeof(dc));
		dc.name = ldb_attr_casefold(cedn->components, edn->components[i].name);
		if (!dc.name) {
			talloc_free(cedn);
			return NULL;
		}

		h = ldb_attrib_handler(ldb, dc.name);
		ret = h->canonicalise_fn(ldb, cedn->components,
					 &(edn->components[i].value),
					 &(dc.value));
		if (ret != 0) {
			talloc_free(cedn);
			return NULL;
		}

		cedn->components[i] = dc;
	}

	return cedn;
}

struct ldb_dn *ldb_dn_explode_casefold(struct ldb_context *ldb, void *mem_ctx, const char *dn)
{
	struct ldb_dn *edn, *cdn;

	if (dn == NULL) return NULL;

	edn = ldb_dn_explode(ldb, dn);
	if (edn == NULL) return NULL;

	cdn = ldb_dn_casefold(ldb, mem_ctx, edn);
	
	talloc_free(edn);
	return cdn;
}

char *ldb_dn_linearize_casefold(struct ldb_context *ldb, void *mem_ctx, const struct ldb_dn *edn)
{
	struct ldb_dn *cdn;
	char *dn;

	if (edn == NULL) return NULL;

	/* Special DNs */
	if (ldb_dn_is_special(edn)) {
		dn = talloc_strdup(mem_ctx, (char *)edn->components[0].value.data);
		return dn;
	}

	cdn = ldb_dn_casefold(ldb, mem_ctx, edn);
	if (cdn == NULL) return NULL;

	dn = ldb_dn_linearize(ldb, cdn);
	if (dn == NULL) {
		talloc_free(cdn);
		return NULL;
	}

	talloc_free(cdn);
	return dn;
}

static struct ldb_dn_component ldb_dn_copy_component(void *mem_ctx, struct ldb_dn_component *src)
{
	struct ldb_dn_component dst;

	memset(&dst, 0, sizeof(dst));

	if (src == NULL) {
		return dst;
	}

	dst.value = ldb_val_dup(mem_ctx, &(src->value));
	if (dst.value.data == NULL) {
		return dst;
	}

	dst.name = talloc_strdup(mem_ctx, src->name);
	if (dst.name == NULL) {
		talloc_free(dst.value.data);
		dst.value.data = NULL;
	}

	return dst;
}

/* Copy a DN but replace the old with the new base DN. */
struct ldb_dn *ldb_dn_copy_rebase(void *mem_ctx, const struct ldb_dn *old, const struct ldb_dn *old_base, const struct ldb_dn *new_base)
{
	struct ldb_dn *new_dn;
	int i, offset;

	/* Perhaps we don't need to rebase at all? */
	if (!old_base || !new_base) {
		return ldb_dn_copy(mem_ctx, old);
	}

	offset = old->comp_num - old_base->comp_num;
	if (!(new_dn = ldb_dn_copy_partial(mem_ctx, new_base,
					   offset + new_base->comp_num))) {
		return NULL;
	}
	for (i = 0; i < offset; i++) {
		new_dn->components[i] = ldb_dn_copy_component(new_dn->components, &(old->components[i]));
	}

	return new_dn;
}

/* copy specified number of elements of a dn into a new one
   element are copied from top level up to the unique rdn
   num_el may be greater than dn->comp_num (see ldb_dn_make_child)
*/
struct ldb_dn *ldb_dn_copy_partial(void *mem_ctx, const struct ldb_dn *dn, int num_el)
{
	struct ldb_dn *newdn;
	int i, n, e;

	if (dn == NULL) return NULL;
	if (num_el <= 0) return NULL;

	newdn = talloc_zero(mem_ctx, struct ldb_dn);
	LDB_DN_NULL_FAILED(newdn);

	newdn->comp_num = num_el;
	n = newdn->comp_num - 1;
	newdn->components = talloc_array(newdn, struct ldb_dn_component, newdn->comp_num);
	if (newdn->components == NULL) goto failed;

	if (dn->comp_num == 0) return newdn;
	e = dn->comp_num - 1;

	for (i = 0; i < newdn->comp_num; i++) {
		newdn->components[n - i] = ldb_dn_copy_component(newdn->components,
								&(dn->components[e - i]));
		if ((e - i) == 0) {
			return newdn;
		}
	}

	return newdn;

failed:
	talloc_free(newdn);
	return NULL;
}

struct ldb_dn *ldb_dn_copy(void *mem_ctx, const struct ldb_dn *dn)
{
	if (dn == NULL) return NULL;
	return ldb_dn_copy_partial(mem_ctx, dn, dn->comp_num);
}

struct ldb_dn *ldb_dn_get_parent(void *mem_ctx, const struct ldb_dn *dn)
{
	if (dn == NULL) return NULL;
	return ldb_dn_copy_partial(mem_ctx, dn, dn->comp_num - 1);
}

struct ldb_dn_component *ldb_dn_build_component(void *mem_ctx, const char *attr,
						const char *val)
{
	struct ldb_dn_component *dc;

	if (attr == NULL || val == NULL) return NULL;

	dc = talloc(mem_ctx, struct ldb_dn_component);
	if (dc == NULL) return NULL;

	dc->name = talloc_strdup(dc, attr);
	if (dc->name ==  NULL) {
		talloc_free(dc);
		return NULL;
	}

	dc->value.data = (uint8_t *)talloc_strdup(dc, val);
	if (dc->value.data ==  NULL) {
		talloc_free(dc);
		return NULL;
	}

	dc->value.length = strlen(val);

	return dc;
}

struct ldb_dn *ldb_dn_build_child(void *mem_ctx, const char *attr,
						 const char * value,
						 const struct ldb_dn *base)
{
	struct ldb_dn *newdn;
	if (! ldb_valid_attr_name(attr)) return NULL;
	if (value == NULL || value == '\0') return NULL; 

	if (base != NULL) {
		newdn = ldb_dn_copy_partial(mem_ctx, base, base->comp_num + 1);
		LDB_DN_NULL_FAILED(newdn);
	} else {
		newdn = talloc_zero(mem_ctx, struct ldb_dn);
		LDB_DN_NULL_FAILED(newdn);

		newdn->comp_num = 1;
		newdn->components = talloc_array(newdn, struct ldb_dn_component, newdn->comp_num);
		LDB_DN_NULL_FAILED(newdn->components);
	}

	newdn->components[0].name = talloc_strdup(newdn->components, attr);
	LDB_DN_NULL_FAILED(newdn->components[0].name);

	newdn->components[0].value.data = (uint8_t *)talloc_strdup(newdn->components, value);
	LDB_DN_NULL_FAILED(newdn->components[0].value.data);
	newdn->components[0].value.length = strlen((char *)newdn->components[0].value.data);

	return newdn;

failed:
	talloc_free(newdn);
	return NULL;

}

struct ldb_dn *ldb_dn_compose(void *mem_ctx, const struct ldb_dn *dn1, const struct ldb_dn *dn2)
{
	int i;
	struct ldb_dn *newdn;

	if (dn2 == NULL && dn1 == NULL) {
		return NULL;
	}

	if (dn2 == NULL) {
		newdn = talloc_zero(mem_ctx, struct ldb_dn);
		LDB_DN_NULL_FAILED(newdn);

		newdn->comp_num = dn1->comp_num;
		newdn->components = talloc_array(newdn, struct ldb_dn_component, newdn->comp_num);
		LDB_DN_NULL_FAILED(newdn->components);
	} else {
		int comp_num = dn2->comp_num;
		if (dn1 != NULL) comp_num += dn1->comp_num;
		newdn = ldb_dn_copy_partial(mem_ctx, dn2, comp_num);
		LDB_DN_NULL_FAILED(newdn);
	}

	if (dn1 == NULL) {
		return newdn;
	}

	for (i = 0; i < dn1->comp_num; i++) {
		newdn->components[i] = ldb_dn_copy_component(newdn->components,
							   &(dn1->components[i]));
		if (newdn->components[i].value.data == NULL) {
			goto failed;
		}
	}

	return newdn;

failed:
	talloc_free(newdn);
	return NULL;
}

struct ldb_dn *ldb_dn_string_compose(void *mem_ctx, const struct ldb_dn *base, const char *child_fmt, ...)
{
	struct ldb_dn *dn, *dn1;
	char *child_str;
	va_list ap;
	
	if (child_fmt == NULL) return NULL;

	va_start(ap, child_fmt);
	child_str = talloc_vasprintf(mem_ctx, child_fmt, ap);
	va_end(ap);

	if (child_str == NULL) return NULL;

	dn1 = ldb_dn_explode(mem_ctx, child_str);
	dn = ldb_dn_compose(mem_ctx, dn1, base);

	talloc_free(child_str);
	talloc_free(dn1);

	return dn;
}

/* Create a 'canonical name' string from a DN:

   ie dc=samba,dc=org -> samba.org/
      uid=administrator,ou=users,dc=samba,dc=org = samba.org/users/administrator

   There are two formats, the EX format has the last / replaced with a newline (\n).

*/
static char *ldb_dn_canonical(void *mem_ctx, const struct ldb_dn *dn, int ex_format) {
	int i;
	char *cracked = NULL;

	/* Walk backwards down the DN, grabbing 'dc' components at first */
	for (i = dn->comp_num - 1 ; i >= 0; i--) {
		if (ldb_attr_cmp(dn->components[i].name, "dc") != 0) {
			break;
		}
		if (cracked) {
			cracked = talloc_asprintf(mem_ctx, "%s.%s",
						  ldb_dn_escape_value(mem_ctx, dn->components[i].value),
						  cracked);
		} else {
			cracked = ldb_dn_escape_value(mem_ctx, dn->components[i].value);
		}
		if (!cracked) {
			return NULL;
		}
	}

	/* Only domain components?  Finish here */
	if (i < 0) {
		if (ex_format) {
			cracked = talloc_asprintf(mem_ctx, "%s\n", cracked);
		} else {
			cracked = talloc_asprintf(mem_ctx, "%s/", cracked);
		}
		return cracked;
	}

	/* Now walk backwards appending remaining components */
	for (; i > 0; i--) {
		cracked = talloc_asprintf(mem_ctx, "%s/%s", cracked, 
					  ldb_dn_escape_value(mem_ctx, dn->components[i].value));
		if (!cracked) {
			return NULL;
		}
	}

	/* Last one, possibly a newline for the 'ex' format */
	if (ex_format) {
		cracked = talloc_asprintf(mem_ctx, "%s\n%s", cracked, 
					  ldb_dn_escape_value(mem_ctx, dn->components[i].value));
	} else {
		cracked = talloc_asprintf(mem_ctx, "%s/%s", cracked, 
					  ldb_dn_escape_value(mem_ctx, dn->components[i].value));
	}
	return cracked;
}

/* Wrapper functions for the above, for the two different string formats */
char *ldb_dn_canonical_string(void *mem_ctx, const struct ldb_dn *dn) {
	return ldb_dn_canonical(mem_ctx, dn, 0);

}

char *ldb_dn_canonical_ex_string(void *mem_ctx, const struct ldb_dn *dn) {
	return ldb_dn_canonical(mem_ctx, dn, 1);
}

int ldb_dn_get_comp_num(const struct ldb_dn *dn)
{
	return dn->comp_num;
}

const char *ldb_dn_get_component_name(const struct ldb_dn *dn, unsigned int num)
{
	if (num >= dn->comp_num) return NULL;
	return dn->components[num].name;
}

const struct ldb_val *ldb_dn_get_component_val(const struct ldb_dn *dn, unsigned int num)
{
	if (num >= dn->comp_num) return NULL;
	return &dn->components[num].value;
}

const char *ldb_dn_get_rdn_name(const struct ldb_dn *dn) {
	if (dn->comp_num == 0) return NULL;
	return dn->components[0].name;
}

const struct ldb_val *ldb_dn_get_rdn_val(const struct ldb_dn *dn) {
	if (dn->comp_num == 0) return NULL;
	return &dn->components[0].value;
}

int ldb_dn_set_component(struct ldb_dn *dn, int num, const char *name, const struct ldb_val val)
{
	char *n;
	struct ldb_val v;

	if (num >= dn->comp_num) {
		return LDB_ERR_OTHER;
	}

	n = talloc_strdup(dn, name);
	if ( ! n) {
		return LDB_ERR_OTHER;
	}

	v.length = val.length;
	v.data = (uint8_t *)talloc_memdup(dn, val.data, v.length+1);
	if ( ! v.data) {
		return LDB_ERR_OTHER;
	}

	talloc_free(dn->components[num].name);
	talloc_free(dn->components[num].value.data);
	dn->components[num].name = n;
	dn->components[num].value = v;

	return LDB_SUCCESS;
}
