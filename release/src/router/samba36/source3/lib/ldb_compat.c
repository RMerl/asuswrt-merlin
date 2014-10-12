/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004

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
#include "lib/ldb_compat.h"

static struct ldb_parse_tree *ldb_parse_filter(void *mem_ctx, const char **s);

static int ldb_parse_hex2char(const char *x)
{
	if (isxdigit(x[0]) && isxdigit(x[1])) {
		const char h1 = x[0], h2 = x[1];
		int c = 0;

		if (h1 >= 'a') c = h1 - (int)'a' + 10;
		else if (h1 >= 'A') c = h1 - (int)'A' + 10;
		else if (h1 >= '0') c = h1 - (int)'0';
		c = c << 4;
		if (h2 >= 'a') c += h2 - (int)'a' + 10;
		else if (h2 >= 'A') c += h2 - (int)'A' + 10;
		else if (h2 >= '0') c += h2 - (int)'0';

		return c;
	}

	return -1;
}



/*
   decode a RFC2254 binary string representation of a buffer.
   Used in LDAP filters.
*/
static struct ldb_val ldb_binary_decode(void *mem_ctx, const char *str)
{
	size_t i, j;
	struct ldb_val ret;
	size_t slen = str?strlen(str):0;

	ret.data = (uint8_t *)talloc_size(mem_ctx, slen+1);
	ret.length = 0;
	if (ret.data == NULL) return ret;

	for (i=j=0;i<slen;i++) {
		if (str[i] == '\\') {
			int c;

			c = ldb_parse_hex2char(&str[i+1]);
			if (c == -1) {
				talloc_free(ret.data);
				memset(&ret, 0, sizeof(ret));
				return ret;
			}
			((uint8_t *)ret.data)[j++] = c;
			i += 2;
		} else {
			((uint8_t *)ret.data)[j++] = str[i];
		}
	}
	ret.length = j;
	((uint8_t *)ret.data)[j] = 0;

	return ret;
}

static bool need_encode(unsigned char cval)
{
	if (cval < 0x20 || cval > 0x7E || strchr(" *()\\&|!\"", cval)) {
		return true;
	}
	return false;
}

/*
   encode a blob as a RFC2254 binary string, escaping any
   non-printable or '\' characters
*/
char *ldb_binary_encode(void *mem_ctx, struct ldb_val val)
{
	size_t i;
	char *ret;
	size_t len = val.length;
	unsigned char *buf = val.data;

	for (i=0;i<val.length;i++) {
		if (need_encode(buf[i])) {
			len += 2;
		}
	}
	ret = talloc_array(mem_ctx, char, len+1);
	if (ret == NULL) return NULL;

	len = 0;
	for (i=0;i<val.length;i++) {
		if (need_encode(buf[i])) {
			snprintf(ret+len, 4, "\\%02X", buf[i]);
			len += 3;
		} else {
			ret[len++] = buf[i];
		}
	}

	ret[len] = 0;

	return ret;	
}



static enum ldb_parse_op ldb_parse_filtertype(void *mem_ctx, char **type, char **value, const char **s)
{
	enum ldb_parse_op filter = 0;
	char *name, *val, *k;
	const char *p = *s;
	const char *t, *t1;

	/* retrieve attributetype name */
	t = p;

	if (*p == '@') { /* for internal attributes the first char can be @ */
		p++;
	}

	while ((isascii(*p) && isalnum((unsigned char)*p)) || (*p == '-') || (*p == '.')) { 
		/* attribute names can only be alphanums */
		p++;
	}

	if (*p == ':') { /* but extended searches have : and . chars too */
		p = strstr(p, ":=");
		if (p == NULL) { /* malformed attribute name */
			return 0;
		}
	}

	t1 = p;

	while (isspace((unsigned char)*p)) p++;

	if (!strchr("=<>~:", *p)) {
		return 0;
	}

	/* save name */
	name = (char *)talloc_memdup(mem_ctx, t, t1 - t + 1);
	if (name == NULL) return 0;
	name[t1 - t] = '\0';

	/* retrieve filtertype */

	if (*p == '=') {
		filter = LDB_OP_EQUALITY;
	} else if (*(p + 1) == '=') {
		switch (*p) {
		case '<':
			filter = LDB_OP_LESS;
			p++;
			break;
		case '>':
			filter = LDB_OP_GREATER;
			p++;
			break;
		case '~':
			filter = LDB_OP_APPROX;
			p++;
			break;
		case ':':
			filter = LDB_OP_EXTENDED;
			p++;
			break;
		}
	}
	if (!filter) {
		talloc_free(name);
		return filter;
	}
	p++;

	while (isspace((unsigned char)*p)) p++;

	/* retrieve value */
	t = p;

	while (*p && ((*p != ')') || ((*p == ')') && (*(p - 1) == '\\')))) p++;

	val = (char *)talloc_memdup(mem_ctx, t, p - t + 1);
	if (val == NULL) {
		talloc_free(name);
		return 0;
	}
	val[p - t] = '\0';

	k = &(val[p - t]);

	/* remove trailing spaces from value */
	while ((k > val) && (isspace((unsigned char)*(k - 1)))) k--;
	*k = '\0';

	*type = name;
	*value = val;
	*s = p;
	return filter;
}

/* find the first matching wildcard */
static char *ldb_parse_find_wildcard(char *value)
{
	while (*value) {
		value = strpbrk(value, "\\*");
		if (value == NULL) return NULL;

		if (value[0] == '\\') {
			if (value[1] == '\0') return NULL;
			value += 2;
			continue;
		}

		if (value[0] == '*') return value;
	}

	return NULL;
}

/* return a NULL terminated list of binary strings representing the value
   chunks separated by wildcards that makes the value portion of the filter
*/
static struct ldb_val **ldb_wildcard_decode(void *mem_ctx, const char *string)
{
	struct ldb_val **ret = NULL;
	unsigned int val = 0;
	char *wc, *str;

	wc = talloc_strdup(mem_ctx, string);
	if (wc == NULL) return NULL;

	while (wc && *wc) {
		str = wc;
		wc = ldb_parse_find_wildcard(str);
		if (wc && *wc) {
			if (wc == str) {
				wc++;
				continue;
			}
			*wc = 0;
			wc++;
		}

		ret = talloc_realloc(mem_ctx, ret, struct ldb_val *, val + 2);
		if (ret == NULL) return NULL;

		ret[val] = talloc(mem_ctx, struct ldb_val);
		if (ret[val] == NULL) return NULL;

		*(ret[val]) = ldb_binary_decode(mem_ctx, str);
		if ((ret[val])->data == NULL) return NULL;

		val++;
	}

	if (ret != NULL) {
		ret[val] = NULL;
	}

	return ret;
}

/*
  parse an extended match

  possible forms:
        (attr:oid:=value)
        (attr:dn:oid:=value)
        (attr:dn:=value)
        (:dn:oid:=value)

  the ':dn' part sets the dnAttributes boolean if present
  the oid sets the rule_id string
  
*/
static struct ldb_parse_tree *ldb_parse_extended(struct ldb_parse_tree *ret, 
						 char *attr, char *value)
{
	char *p1, *p2;

	ret->operation = LDB_OP_EXTENDED;
	ret->u.extended.value = ldb_binary_decode(ret, value);
	if (ret->u.extended.value.data == NULL) goto failed;

	p1 = strchr(attr, ':');
	if (p1 == NULL) goto failed;
	p2 = strchr(p1+1, ':');

	*p1 = 0;
	if (p2) *p2 = 0;

	ret->u.extended.attr = attr;
	if (strcmp(p1+1, "dn") == 0) {
		ret->u.extended.dnAttributes = 1;
		if (p2) {
			ret->u.extended.rule_id = talloc_strdup(ret, p2+1);
			if (ret->u.extended.rule_id == NULL) goto failed;
		} else {
			ret->u.extended.rule_id = NULL;
		}
	} else {
		ret->u.extended.dnAttributes = 0;
		ret->u.extended.rule_id = talloc_strdup(ret, p1+1);
		if (ret->u.extended.rule_id == NULL) goto failed;
	}

	return ret;

failed:
	talloc_free(ret);
	return NULL;
}


/*
  <simple> ::= <attributetype> <filtertype> <attributevalue>
*/
static struct ldb_parse_tree *ldb_parse_simple(void *mem_ctx, const char **s)
{
	char *attr, *value;
	struct ldb_parse_tree *ret;
	enum ldb_parse_op filtertype;

	ret = talloc(mem_ctx, struct ldb_parse_tree);
	if (!ret) {
		errno = ENOMEM;
		return NULL;
	}

	filtertype = ldb_parse_filtertype(ret, &attr, &value, s);
	if (!filtertype) {
		talloc_free(ret);
		return NULL;
	}

	switch (filtertype) {

		case LDB_OP_PRESENT:
			ret->operation = LDB_OP_PRESENT;
			ret->u.present.attr = attr;
			break;

		case LDB_OP_EQUALITY:

			if (strcmp(value, "*") == 0) {
				ret->operation = LDB_OP_PRESENT;
				ret->u.present.attr = attr;
				break;
			}

			if (ldb_parse_find_wildcard(value) != NULL) {
				ret->operation = LDB_OP_SUBSTRING;
				ret->u.substring.attr = attr;
				ret->u.substring.start_with_wildcard = 0;
				ret->u.substring.end_with_wildcard = 0;
				ret->u.substring.chunks = ldb_wildcard_decode(ret, value);
				if (ret->u.substring.chunks == NULL){
					talloc_free(ret);
					return NULL;
				}
				if (value[0] == '*')
					ret->u.substring.start_with_wildcard = 1;
				if (value[strlen(value) - 1] == '*')
					ret->u.substring.end_with_wildcard = 1;
				talloc_free(value);

				break;
			}

			ret->operation = LDB_OP_EQUALITY;
			ret->u.equality.attr = attr;
			ret->u.equality.value = ldb_binary_decode(ret, value);
			if (ret->u.equality.value.data == NULL) {
				talloc_free(ret);
				return NULL;
			}
			talloc_free(value);
			break;

		case LDB_OP_GREATER:
			ret->operation = LDB_OP_GREATER;
			ret->u.comparison.attr = attr;
			ret->u.comparison.value = ldb_binary_decode(ret, value);
			if (ret->u.comparison.value.data == NULL) {
				talloc_free(ret);
				return NULL;
			}
			talloc_free(value);
			break;

		case LDB_OP_LESS:
			ret->operation = LDB_OP_LESS;
			ret->u.comparison.attr = attr;
			ret->u.comparison.value = ldb_binary_decode(ret, value);
			if (ret->u.comparison.value.data == NULL) {
				talloc_free(ret);
				return NULL;
			}
			talloc_free(value);
			break;

		case LDB_OP_APPROX:
			ret->operation = LDB_OP_APPROX;
			ret->u.comparison.attr = attr;
			ret->u.comparison.value = ldb_binary_decode(ret, value);
			if (ret->u.comparison.value.data == NULL) {
				talloc_free(ret);
				return NULL;
			}
			talloc_free(value);
			break;

		case LDB_OP_EXTENDED:

			ret = ldb_parse_extended(ret, attr, value);
			break;

		default:
			talloc_free(ret);
			return NULL;
	}

	return ret;
}

/*
  parse a filterlist
  <and> ::= '&' <filterlist>
  <or> ::= '|' <filterlist>
  <filterlist> ::= <filter> | <filter> <filterlist>
*/
static struct ldb_parse_tree *ldb_parse_filterlist(void *mem_ctx, const char **s)
{
	struct ldb_parse_tree *ret, *next;
	enum ldb_parse_op op;
	const char *p = *s;

	switch (*p) {
		case '&':
			op = LDB_OP_AND;
			break;
		case '|':
			op = LDB_OP_OR;
			break;
		default:
			return NULL;
	}
	p++;

	while (isspace((unsigned char)*p)) p++;

	ret = talloc(mem_ctx, struct ldb_parse_tree);
	if (!ret) {
		errno = ENOMEM;
		return NULL;
	}

	ret->operation = op;
	ret->u.list.num_elements = 1;
	ret->u.list.elements = talloc(ret, struct ldb_parse_tree *);
	if (!ret->u.list.elements) {
		errno = ENOMEM;
		talloc_free(ret);
		return NULL;
	}

	ret->u.list.elements[0] = ldb_parse_filter(ret->u.list.elements, &p);
	if (!ret->u.list.elements[0]) {
		talloc_free(ret);
		return NULL;
	}

	while (isspace((unsigned char)*p)) p++;

	while (*p && (next = ldb_parse_filter(ret->u.list.elements, &p))) {
		struct ldb_parse_tree **e;
		e = talloc_realloc(ret, ret->u.list.elements, 
				     struct ldb_parse_tree *, 
				     ret->u.list.num_elements + 1);
		if (!e) {
			errno = ENOMEM;
			talloc_free(ret);
			return NULL;
		}
		ret->u.list.elements = e;
		ret->u.list.elements[ret->u.list.num_elements] = next;
		ret->u.list.num_elements++;
		while (isspace((unsigned char)*p)) p++;
	}

	*s = p;

	return ret;
}

/*
  <not> ::= '!' <filter>
*/
static struct ldb_parse_tree *ldb_parse_not(void *mem_ctx, const char **s)
{
	struct ldb_parse_tree *ret;
	const char *p = *s;

	if (*p != '!') {
		return NULL;
	}
	p++;

	ret = talloc(mem_ctx, struct ldb_parse_tree);
	if (!ret) {
		errno = ENOMEM;
		return NULL;
	}

	ret->operation = LDB_OP_NOT;
	ret->u.isnot.child = ldb_parse_filter(ret, &p);
	if (!ret->u.isnot.child) {
		talloc_free(ret);
		return NULL;
	}

	*s = p;

	return ret;
}



/*
  parse a filtercomp
  <filtercomp> ::= <and> | <or> | <not> | <simple>
*/
static struct ldb_parse_tree *ldb_parse_filtercomp(void *mem_ctx, const char **s)
{
	struct ldb_parse_tree *ret;
	const char *p = *s;

	while (isspace((unsigned char)*p)) p++;

	switch (*p) {
	case '&':
		ret = ldb_parse_filterlist(mem_ctx, &p);
		break;

	case '|':
		ret = ldb_parse_filterlist(mem_ctx, &p);
		break;

	case '!':
		ret = ldb_parse_not(mem_ctx, &p);
		break;

	case '(':
	case ')':
		return NULL;

	default:
		ret = ldb_parse_simple(mem_ctx, &p);

	}

	*s = p;
	return ret;
}



/*
  <filter> ::= '(' <filtercomp> ')'
*/
static struct ldb_parse_tree *ldb_parse_filter(void *mem_ctx, const char **s)
{
	struct ldb_parse_tree *ret;
	const char *p = *s;

	if (*p != '(') {
		return NULL;
	}
	p++;

	ret = ldb_parse_filtercomp(mem_ctx, &p);

	if (*p != ')') {
		return NULL;
	}
	p++;

	while (isspace((unsigned char)*p)) {
		p++;
	}

	*s = p;

	return ret;
}



/*
  main parser entry point. Takes a search string and returns a parse tree

  expression ::= <simple> | <filter>
*/
struct ldb_parse_tree *ldb_parse_tree(void *mem_ctx, const char *s)
{
	if (s == NULL || *s == 0) {
		s = "(|(objectClass=*)(distinguishedName=*))";
	}

	while (isspace((unsigned char)*s)) s++;

	if (*s == '(') {
		return ldb_parse_filter(mem_ctx, &s);
	}

	return ldb_parse_simple(mem_ctx, &s);
}


