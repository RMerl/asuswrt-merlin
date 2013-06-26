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

/*
 *  Name: ldb
 *
 *  Component: ldb expression parsing
 *
 *  Description: parse LDAP-like search expressions
 *
 *  Author: Andrew Tridgell
 */

/*
  TODO:
      - add RFC2254 binary string handling
      - possibly add ~=, <= and >= handling
      - expand the test suite
      - add better parse error handling

*/

#include "ldb_private.h"
#include "system/locale.h"

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
a filter is defined by:
               <filter> ::= '(' <filtercomp> ')'
               <filtercomp> ::= <and> | <or> | <not> | <simple>
               <and> ::= '&' <filterlist>
               <or> ::= '|' <filterlist>
               <not> ::= '!' <filter>
               <filterlist> ::= <filter> | <filter> <filterlist>
               <simple> ::= <attributetype> <filtertype> <attributevalue>
               <filtertype> ::= '=' | '~=' | '<=' | '>='
*/

/*
   decode a RFC2254 binary string representation of a buffer.
   Used in LDAP filters.
*/
struct ldb_val ldb_binary_decode(TALLOC_CTX *mem_ctx, const char *str)
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
char *ldb_binary_encode(TALLOC_CTX *mem_ctx, struct ldb_val val)
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

/*
   encode a string as a RFC2254 binary string, escaping any
   non-printable or '\' characters.  This routine is suitable for use
   in escaping user data in ldap filters.
*/
char *ldb_binary_encode_string(TALLOC_CTX *mem_ctx, const char *string)
{
	struct ldb_val val;
	if (string == NULL) {
		return NULL;
	}
	val.data = discard_const_p(uint8_t, string);
	val.length = strlen(string);
	return ldb_binary_encode(mem_ctx, val);
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
static struct ldb_val **ldb_wildcard_decode(TALLOC_CTX *mem_ctx, const char *string)
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

static struct ldb_parse_tree *ldb_parse_filter(TALLOC_CTX *mem_ctx, const char **s);


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

static enum ldb_parse_op ldb_parse_filtertype(TALLOC_CTX *mem_ctx, char **type, char **value, const char **s)
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

/*
  <simple> ::= <attributetype> <filtertype> <attributevalue>
*/
static struct ldb_parse_tree *ldb_parse_simple(TALLOC_CTX *mem_ctx, const char **s)
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
static struct ldb_parse_tree *ldb_parse_filterlist(TALLOC_CTX *mem_ctx, const char **s)
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
static struct ldb_parse_tree *ldb_parse_not(TALLOC_CTX *mem_ctx, const char **s)
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
static struct ldb_parse_tree *ldb_parse_filtercomp(TALLOC_CTX *mem_ctx, const char **s)
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
static struct ldb_parse_tree *ldb_parse_filter(TALLOC_CTX *mem_ctx, const char **s)
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
struct ldb_parse_tree *ldb_parse_tree(TALLOC_CTX *mem_ctx, const char *s)
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


/*
  construct a ldap parse filter given a parse tree
*/
char *ldb_filter_from_tree(TALLOC_CTX *mem_ctx, const struct ldb_parse_tree *tree)
{
	char *s, *s2, *ret;
	unsigned int i;

	if (tree == NULL) {
		return NULL;
	}

	switch (tree->operation) {
	case LDB_OP_AND:
	case LDB_OP_OR:
		ret = talloc_asprintf(mem_ctx, "(%c", tree->operation==LDB_OP_AND?'&':'|');
		if (ret == NULL) return NULL;
		for (i=0;i<tree->u.list.num_elements;i++) {
			s = ldb_filter_from_tree(mem_ctx, tree->u.list.elements[i]);
			if (s == NULL) {
				talloc_free(ret);
				return NULL;
			}
			s2 = talloc_asprintf_append(ret, "%s", s);
			talloc_free(s);
			if (s2 == NULL) {
				talloc_free(ret);
				return NULL;
			}
			ret = s2;
		}
		s = talloc_asprintf_append(ret, ")");
		if (s == NULL) {
			talloc_free(ret);
			return NULL;
		}
		return s;
	case LDB_OP_NOT:
		s = ldb_filter_from_tree(mem_ctx, tree->u.isnot.child);
		if (s == NULL) return NULL;

		ret = talloc_asprintf(mem_ctx, "(!%s)", s);
		talloc_free(s);
		return ret;
	case LDB_OP_EQUALITY:
		s = ldb_binary_encode(mem_ctx, tree->u.equality.value);
		if (s == NULL) return NULL;
		ret = talloc_asprintf(mem_ctx, "(%s=%s)", 
				      tree->u.equality.attr, s);
		talloc_free(s);
		return ret;
	case LDB_OP_SUBSTRING:
		ret = talloc_asprintf(mem_ctx, "(%s=%s", tree->u.substring.attr,
				      tree->u.substring.start_with_wildcard?"*":"");
		if (ret == NULL) return NULL;
		for (i = 0; tree->u.substring.chunks[i]; i++) {
			s2 = ldb_binary_encode(mem_ctx, *(tree->u.substring.chunks[i]));
			if (s2 == NULL) {
				talloc_free(ret);
				return NULL;
			}
			if (tree->u.substring.chunks[i+1] ||
			    tree->u.substring.end_with_wildcard) {
				s = talloc_asprintf_append(ret, "%s*", s2);
			} else {
				s = talloc_asprintf_append(ret, "%s", s2);
			}
			if (s == NULL) {
				talloc_free(ret);
				return NULL;
			}
			ret = s;
		}
		s = talloc_asprintf_append(ret, ")");
		if (s == NULL) {
			talloc_free(ret);
			return NULL;
		}
		ret = s;
		return ret;
	case LDB_OP_GREATER:
		s = ldb_binary_encode(mem_ctx, tree->u.equality.value);
		if (s == NULL) return NULL;
		ret = talloc_asprintf(mem_ctx, "(%s>=%s)", 
				      tree->u.equality.attr, s);
		talloc_free(s);
		return ret;
	case LDB_OP_LESS:
		s = ldb_binary_encode(mem_ctx, tree->u.equality.value);
		if (s == NULL) return NULL;
		ret = talloc_asprintf(mem_ctx, "(%s<=%s)", 
				      tree->u.equality.attr, s);
		talloc_free(s);
		return ret;
	case LDB_OP_PRESENT:
		ret = talloc_asprintf(mem_ctx, "(%s=*)", tree->u.present.attr);
		return ret;
	case LDB_OP_APPROX:
		s = ldb_binary_encode(mem_ctx, tree->u.equality.value);
		if (s == NULL) return NULL;
		ret = talloc_asprintf(mem_ctx, "(%s~=%s)", 
				      tree->u.equality.attr, s);
		talloc_free(s);
		return ret;
	case LDB_OP_EXTENDED:
		s = ldb_binary_encode(mem_ctx, tree->u.extended.value);
		if (s == NULL) return NULL;
		ret = talloc_asprintf(mem_ctx, "(%s%s%s%s:=%s)", 
				      tree->u.extended.attr?tree->u.extended.attr:"", 
				      tree->u.extended.dnAttributes?":dn":"",
				      tree->u.extended.rule_id?":":"", 
				      tree->u.extended.rule_id?tree->u.extended.rule_id:"", 
				      s);
		talloc_free(s);
		return ret;
	}
	
	return NULL;
}


/*
  replace any occurrences of an attribute name in the parse tree with a
  new name
*/
void ldb_parse_tree_attr_replace(struct ldb_parse_tree *tree, 
				 const char *attr, 
				 const char *replace)
{
	unsigned int i;
	switch (tree->operation) {
	case LDB_OP_AND:
	case LDB_OP_OR:
		for (i=0;i<tree->u.list.num_elements;i++) {
			ldb_parse_tree_attr_replace(tree->u.list.elements[i],
						    attr, replace);
		}
		break;
	case LDB_OP_NOT:
		ldb_parse_tree_attr_replace(tree->u.isnot.child, attr, replace);
		break;
	case LDB_OP_EQUALITY:
	case LDB_OP_GREATER:
	case LDB_OP_LESS:
	case LDB_OP_APPROX:
		if (ldb_attr_cmp(tree->u.equality.attr, attr) == 0) {
			tree->u.equality.attr = replace;
		}
		break;
	case LDB_OP_SUBSTRING:
		if (ldb_attr_cmp(tree->u.substring.attr, attr) == 0) {
			tree->u.substring.attr = replace;
		}
		break;
	case LDB_OP_PRESENT:
		if (ldb_attr_cmp(tree->u.present.attr, attr) == 0) {
			tree->u.present.attr = replace;
		}
		break;
	case LDB_OP_EXTENDED:
		if (tree->u.extended.attr &&
		    ldb_attr_cmp(tree->u.extended.attr, attr) == 0) {
			tree->u.extended.attr = replace;
		}
		break;
	}
}

/*
  shallow copy a tree - copying only the elements array so that the caller
  can safely add new elements without changing the message
*/
struct ldb_parse_tree *ldb_parse_tree_copy_shallow(TALLOC_CTX *mem_ctx,
						   const struct ldb_parse_tree *ot)
{
	unsigned int i;
	struct ldb_parse_tree *nt;

	nt = talloc(mem_ctx, struct ldb_parse_tree);
	if (!nt) {
		return NULL;
	}

	*nt = *ot;

	switch (ot->operation) {
	case LDB_OP_AND:
	case LDB_OP_OR:
		nt->u.list.elements = talloc_array(nt, struct ldb_parse_tree *,
						   ot->u.list.num_elements);
		if (!nt->u.list.elements) {
			talloc_free(nt);
			return NULL;
		}

		for (i=0;i<ot->u.list.num_elements;i++) {
			nt->u.list.elements[i] =
				ldb_parse_tree_copy_shallow(nt->u.list.elements,
						ot->u.list.elements[i]);
			if (!nt->u.list.elements[i]) {
				talloc_free(nt);
				return NULL;
			}
		}
		break;
	case LDB_OP_NOT:
		nt->u.isnot.child = ldb_parse_tree_copy_shallow(nt,
							ot->u.isnot.child);
		if (!nt->u.isnot.child) {
			talloc_free(nt);
			return NULL;
		}
		break;
	case LDB_OP_EQUALITY:
	case LDB_OP_GREATER:
	case LDB_OP_LESS:
	case LDB_OP_APPROX:
	case LDB_OP_SUBSTRING:
	case LDB_OP_PRESENT:
	case LDB_OP_EXTENDED:
		break;
	}

	return nt;
}
