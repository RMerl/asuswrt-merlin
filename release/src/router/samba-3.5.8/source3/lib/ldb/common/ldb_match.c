/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004-2005
   Copyright (C) Simo Sorce            2005

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
 *  Component: ldb expression matching
 *
 *  Description: ldb expression matching 
 *
 *  Author: Andrew Tridgell
 */

#include "includes.h"
#include "ldb/include/includes.h"

/*
  check if the scope matches in a search result
*/
static int ldb_match_scope(struct ldb_context *ldb,
			   const struct ldb_dn *base,
			   const struct ldb_dn *dn,
			   enum ldb_scope scope)
{
	int ret = 0;

	if (base == NULL || dn == NULL) {
		return 1;
	}

	switch (scope) {
	case LDB_SCOPE_BASE:
		if (ldb_dn_compare(ldb, base, dn) == 0) {
			ret = 1;
		}
		break;

	case LDB_SCOPE_ONELEVEL:
		if (ldb_dn_get_comp_num(dn) == (ldb_dn_get_comp_num(base) + 1)) {
			if (ldb_dn_compare_base(ldb, base, dn) == 0) {
				ret = 1;
			}
		}
		break;
		
	case LDB_SCOPE_SUBTREE:
	default:
		if (ldb_dn_compare_base(ldb, base, dn) == 0) {
			ret = 1;
		}
		break;
	}

	return ret;
}


/*
  match if node is present
*/
static int ldb_match_present(struct ldb_context *ldb, 
			     const struct ldb_message *msg,
			     const struct ldb_parse_tree *tree,
			     enum ldb_scope scope)
{
	if (ldb_attr_dn(tree->u.present.attr) == 0) {
		return 1;
	}

	if (ldb_msg_find_element(msg, tree->u.present.attr)) {
		return 1;
	}

	return 0;
}

static int ldb_match_comparison(struct ldb_context *ldb, 
				const struct ldb_message *msg,
				const struct ldb_parse_tree *tree,
				enum ldb_scope scope,
				enum ldb_parse_op comp_op)
{
	unsigned int i;
	struct ldb_message_element *el;
	const struct ldb_attrib_handler *h;
	int ret;

	/* FIXME: APPROX comparison not handled yet */
	if (comp_op == LDB_OP_APPROX) return 0;

	el = ldb_msg_find_element(msg, tree->u.comparison.attr);
	if (el == NULL) {
		return 0;
	}

	h = ldb_attrib_handler(ldb, el->name);

	for (i = 0; i < el->num_values; i++) {
		ret = h->comparison_fn(ldb, ldb, &el->values[i], &tree->u.comparison.value);

		if (ret == 0) {
			return 1;
		}
		if (ret > 0 && comp_op == LDB_OP_GREATER) {
			return 1;
		}
		if (ret < 0 && comp_op == LDB_OP_LESS) {
			return 1;
		}
	}

	return 0;
}

/*
  match a simple leaf node
*/
static int ldb_match_equality(struct ldb_context *ldb, 
			      const struct ldb_message *msg,
			      const struct ldb_parse_tree *tree,
			      enum ldb_scope scope)
{
	unsigned int i;
	struct ldb_message_element *el;
	const struct ldb_attrib_handler *h;
	struct ldb_dn *valuedn;
	int ret;

	if (ldb_attr_dn(tree->u.equality.attr) == 0) {
		valuedn = ldb_dn_explode_casefold(ldb, ldb,
						  (char *)tree->u.equality.value.data);
		if (valuedn == NULL) {
			return 0;
		}

		ret = ldb_dn_compare(ldb, msg->dn, valuedn);

		talloc_free(valuedn);

		if (ret == 0) return 1;
		return 0;
	}

	/* TODO: handle the "*" case derived from an extended search
	   operation without the attibute type defined */
	el = ldb_msg_find_element(msg, tree->u.equality.attr);
	if (el == NULL) {
		return 0;
	}

	h = ldb_attrib_handler(ldb, el->name);

	for (i=0;i<el->num_values;i++) {
		if (h->comparison_fn(ldb, ldb, &tree->u.equality.value, 
				     &el->values[i]) == 0) {
			return 1;
		}
	}

	return 0;
}

static int ldb_wildcard_compare(struct ldb_context *ldb,
				const struct ldb_parse_tree *tree,
				const struct ldb_val value)
{
	const struct ldb_attrib_handler *h;
	struct ldb_val val;
	struct ldb_val cnk;
	struct ldb_val *chunk;
	char *p, *g;
	uint8_t *save_p = NULL;
	int c = 0;

	h = ldb_attrib_handler(ldb, tree->u.substring.attr);

	if(h->canonicalise_fn(ldb, ldb, &value, &val) != 0)
		return -1;

	save_p = val.data;
	cnk.data = NULL;

	if ( ! tree->u.substring.start_with_wildcard ) {

		chunk = tree->u.substring.chunks[c];
		if(h->canonicalise_fn(ldb, ldb, chunk, &cnk) != 0) goto failed;

		/* This deals with wildcard prefix searches on binary attributes (eg objectGUID) */
		if (cnk.length > val.length) {
			goto failed;
		}
		if (memcmp((char *)val.data, (char *)cnk.data, cnk.length) != 0) goto failed;
		val.length -= cnk.length;
		val.data += cnk.length;
		c++;
		talloc_free(cnk.data);
		cnk.data = NULL;
	}

	while (tree->u.substring.chunks[c]) {

		chunk = tree->u.substring.chunks[c];
		if(h->canonicalise_fn(ldb, ldb, chunk, &cnk) != 0) goto failed;

		/* FIXME: case of embedded nulls */
		p = strstr((char *)val.data, (char *)cnk.data);
		if (p == NULL) goto failed;
		if ( (! tree->u.substring.chunks[c + 1]) && (! tree->u.substring.end_with_wildcard) ) {
			do { /* greedy */
				g = strstr((char *)p + cnk.length, (char *)cnk.data);
				if (g) p = g;
			} while(g);
		}
		val.length = val.length - (p - (char *)(val.data)) - cnk.length;
		val.data = (uint8_t *)(p + cnk.length);
		c++;
		talloc_free(cnk.data);
		cnk.data = NULL;
	}

	if ( (! tree->u.substring.end_with_wildcard) && (*(val.data) != 0) ) goto failed; /* last chunk have not reached end of string */
	talloc_free(save_p);
	return 1;

failed:
	talloc_free(save_p);
	talloc_free(cnk.data);
	return 0;
}

/*
  match a simple leaf node
*/
static int ldb_match_substring(struct ldb_context *ldb, 
			       const struct ldb_message *msg,
			       const struct ldb_parse_tree *tree,
			       enum ldb_scope scope)
{
	unsigned int i;
	struct ldb_message_element *el;

	el = ldb_msg_find_element(msg, tree->u.substring.attr);
	if (el == NULL) {
		return 0;
	}

	for (i = 0; i < el->num_values; i++) {
		if (ldb_wildcard_compare(ldb, tree, el->values[i]) == 1) {
			return 1;
		}
	}

	return 0;
}


/*
  bitwise-and comparator
*/
static int ldb_comparator_and(const struct ldb_val *v1, const struct ldb_val *v2)
{
	uint64_t i1, i2;
	i1 = strtoull((char *)v1->data, NULL, 0);
	i2 = strtoull((char *)v2->data, NULL, 0);
	return ((i1 & i2) == i2);
}

/*
  bitwise-or comparator
*/
static int ldb_comparator_or(const struct ldb_val *v1, const struct ldb_val *v2)
{
	uint64_t i1, i2;
	i1 = strtoull((char *)v1->data, NULL, 0);
	i2 = strtoull((char *)v2->data, NULL, 0);
	return ((i1 & i2) != 0);
}


/*
  extended match, handles things like bitops
*/
static int ldb_match_extended(struct ldb_context *ldb, 
			      const struct ldb_message *msg,
			      const struct ldb_parse_tree *tree,
			      enum ldb_scope scope)
{
	int i;
	const struct {
		const char *oid;
		int (*comparator)(const struct ldb_val *, const struct ldb_val *);
	} rules[] = {
		{ LDB_OID_COMPARATOR_AND, ldb_comparator_and},
		{ LDB_OID_COMPARATOR_OR, ldb_comparator_or}
	};
	int (*comp)(const struct ldb_val *, const struct ldb_val *) = NULL;
	struct ldb_message_element *el;

	if (tree->u.extended.dnAttributes) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb: dnAttributes extended match not supported yet");
		return -1;
	}
	if (tree->u.extended.rule_id == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb: no-rule extended matches not supported yet");
		return -1;
	}
	if (tree->u.extended.attr == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb: no-attribute extended matches not supported yet");
		return -1;
	}

	for (i=0;i<ARRAY_SIZE(rules);i++) {
		if (strcmp(rules[i].oid, tree->u.extended.rule_id) == 0) {
			comp = rules[i].comparator;
			break;
		}
	}
	if (comp == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb: unknown extended rule_id %s\n",
			  tree->u.extended.rule_id);
		return -1;
	}

	/* find the message element */
	el = ldb_msg_find_element(msg, tree->u.extended.attr);
	if (el == NULL) {
		return 0;
	}

	for (i=0;i<el->num_values;i++) {
		int ret = comp(&el->values[i], &tree->u.extended.value);
		if (ret == -1 || ret == 1) return ret;
	}

	return 0;
}

/*
  return 0 if the given parse tree matches the given message. Assumes
  the message is in sorted order

  return 1 if it matches, and 0 if it doesn't match

  this is a recursive function, and does short-circuit evaluation
 */
static int ldb_match_message(struct ldb_context *ldb, 
			     const struct ldb_message *msg,
			     const struct ldb_parse_tree *tree,
			     enum ldb_scope scope)
{
	unsigned int i;
	int v;

	switch (tree->operation) {
	case LDB_OP_AND:
		for (i=0;i<tree->u.list.num_elements;i++) {
			v = ldb_match_message(ldb, msg, tree->u.list.elements[i], scope);
			if (!v) return 0;
		}
		return 1;

	case LDB_OP_OR:
		for (i=0;i<tree->u.list.num_elements;i++) {
			v = ldb_match_message(ldb, msg, tree->u.list.elements[i], scope);
			if (v) return 1;
		}
		return 0;

	case LDB_OP_NOT:
		return ! ldb_match_message(ldb, msg, tree->u.isnot.child, scope);

	case LDB_OP_EQUALITY:
		return ldb_match_equality(ldb, msg, tree, scope);

	case LDB_OP_SUBSTRING:
		return ldb_match_substring(ldb, msg, tree, scope);

	case LDB_OP_GREATER:
		return ldb_match_comparison(ldb, msg, tree, scope, LDB_OP_GREATER);

	case LDB_OP_LESS:
		return ldb_match_comparison(ldb, msg, tree, scope, LDB_OP_LESS);

	case LDB_OP_PRESENT:
		return ldb_match_present(ldb, msg, tree, scope);

	case LDB_OP_APPROX:
		return ldb_match_comparison(ldb, msg, tree, scope, LDB_OP_APPROX);

	case LDB_OP_EXTENDED:
		return ldb_match_extended(ldb, msg, tree, scope);

	}

	return 0;
}

int ldb_match_msg(struct ldb_context *ldb,
		  const struct ldb_message *msg,
		  const struct ldb_parse_tree *tree,
		  const struct ldb_dn *base,
		  enum ldb_scope scope)
{
	if ( ! ldb_match_scope(ldb, base, msg->dn, scope) ) {
		return 0;
	}

	return ldb_match_message(ldb, msg, tree, scope);
}
