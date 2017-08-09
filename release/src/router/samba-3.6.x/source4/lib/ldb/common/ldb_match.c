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

#include "ldb_private.h"

/*
  check if the scope matches in a search result
*/
static int ldb_match_scope(struct ldb_context *ldb,
			   struct ldb_dn *base,
			   struct ldb_dn *dn,
			   enum ldb_scope scope)
{
	int ret = 0;

	if (base == NULL || dn == NULL) {
		return 1;
	}

	switch (scope) {
	case LDB_SCOPE_BASE:
		if (ldb_dn_compare(base, dn) == 0) {
			ret = 1;
		}
		break;

	case LDB_SCOPE_ONELEVEL:
		if (ldb_dn_get_comp_num(dn) == (ldb_dn_get_comp_num(base) + 1)) {
			if (ldb_dn_compare_base(base, dn) == 0) {
				ret = 1;
			}
		}
		break;
		
	case LDB_SCOPE_SUBTREE:
	default:
		if (ldb_dn_compare_base(base, dn) == 0) {
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
			     enum ldb_scope scope, bool *matched)
{
	const struct ldb_schema_attribute *a;
	struct ldb_message_element *el;

	if (ldb_attr_dn(tree->u.present.attr) == 0) {
		*matched = true;
		return LDB_SUCCESS;
	}

	el = ldb_msg_find_element(msg, tree->u.present.attr);
	if (el == NULL) {
		*matched = false;
		return LDB_SUCCESS;
	}

	a = ldb_schema_attribute_by_name(ldb, el->name);
	if (!a) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}

	if (a->syntax->operator_fn) {
		unsigned int i;
		for (i = 0; i < el->num_values; i++) {
			int ret = a->syntax->operator_fn(ldb, LDB_OP_PRESENT, a, &el->values[i], NULL, matched);
			if (ret != LDB_SUCCESS) return ret;
			if (*matched) return LDB_SUCCESS;
		}
		*matched = false;
		return LDB_SUCCESS;
	}

	*matched = true;
	return LDB_SUCCESS;
}

static int ldb_match_comparison(struct ldb_context *ldb, 
				const struct ldb_message *msg,
				const struct ldb_parse_tree *tree,
				enum ldb_scope scope,
				enum ldb_parse_op comp_op, bool *matched)
{
	unsigned int i;
	struct ldb_message_element *el;
	const struct ldb_schema_attribute *a;

	/* FIXME: APPROX comparison not handled yet */
	if (comp_op == LDB_OP_APPROX) {
		return LDB_ERR_INAPPROPRIATE_MATCHING;
	}

	el = ldb_msg_find_element(msg, tree->u.comparison.attr);
	if (el == NULL) {
		*matched = false;
		return LDB_SUCCESS;
	}

	a = ldb_schema_attribute_by_name(ldb, el->name);
	if (!a) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}

	for (i = 0; i < el->num_values; i++) {
		if (a->syntax->operator_fn) {
			int ret;
			ret = a->syntax->operator_fn(ldb, comp_op, a, &el->values[i], &tree->u.comparison.value, matched);
			if (ret != LDB_SUCCESS) return ret;
			if (*matched) return LDB_SUCCESS;
		} else {
			int ret = a->syntax->comparison_fn(ldb, ldb, &el->values[i], &tree->u.comparison.value);

			if (ret == 0) {
				*matched = true;
				return LDB_SUCCESS;
			}
			if (ret > 0 && comp_op == LDB_OP_GREATER) {
				*matched = true;
				return LDB_SUCCESS;
			}
			if (ret < 0 && comp_op == LDB_OP_LESS) {
				*matched = true;
				return LDB_SUCCESS;
			}
		}
	}

	*matched = false;
	return LDB_SUCCESS;
}

/*
  match a simple leaf node
*/
static int ldb_match_equality(struct ldb_context *ldb, 
			      const struct ldb_message *msg,
			      const struct ldb_parse_tree *tree,
			      enum ldb_scope scope,
			      bool *matched)
{
	unsigned int i;
	struct ldb_message_element *el;
	const struct ldb_schema_attribute *a;
	struct ldb_dn *valuedn;
	int ret;

	if (ldb_attr_dn(tree->u.equality.attr) == 0) {
		valuedn = ldb_dn_from_ldb_val(ldb, ldb, &tree->u.equality.value);
		if (valuedn == NULL) {
			return LDB_ERR_INVALID_DN_SYNTAX;
		}

		ret = ldb_dn_compare(msg->dn, valuedn);

		talloc_free(valuedn);

		*matched = (ret == 0);
		return LDB_SUCCESS;
	}

	/* TODO: handle the "*" case derived from an extended search
	   operation without the attibute type defined */
	el = ldb_msg_find_element(msg, tree->u.equality.attr);
	if (el == NULL) {
		*matched = false;
		return LDB_SUCCESS;
	}

	a = ldb_schema_attribute_by_name(ldb, el->name);
	if (a == NULL) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}

	for (i=0;i<el->num_values;i++) {
		if (a->syntax->operator_fn) {
			ret = a->syntax->operator_fn(ldb, LDB_OP_EQUALITY, a,
						     &tree->u.equality.value, &el->values[i], matched);
			if (ret != LDB_SUCCESS) return ret;
			if (*matched) return LDB_SUCCESS;
		} else {
			if (a->syntax->comparison_fn(ldb, ldb, &tree->u.equality.value,
						     &el->values[i]) == 0) {
				*matched = true;
				return LDB_SUCCESS;
			}
		}
	}

	*matched = false;
	return LDB_SUCCESS;
}

static int ldb_wildcard_compare(struct ldb_context *ldb,
				const struct ldb_parse_tree *tree,
				const struct ldb_val value, bool *matched)
{
	const struct ldb_schema_attribute *a;
	struct ldb_val val;
	struct ldb_val cnk;
	struct ldb_val *chunk;
	char *p, *g;
	uint8_t *save_p = NULL;
	unsigned int c = 0;

	a = ldb_schema_attribute_by_name(ldb, tree->u.substring.attr);
	if (!a) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}

	if (a->syntax->canonicalise_fn(ldb, ldb, &value, &val) != 0) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}

	save_p = val.data;
	cnk.data = NULL;

	if ( ! tree->u.substring.start_with_wildcard ) {

		chunk = tree->u.substring.chunks[c];
		if (a->syntax->canonicalise_fn(ldb, ldb, chunk, &cnk) != 0) goto mismatch;

		/* This deals with wildcard prefix searches on binary attributes (eg objectGUID) */
		if (cnk.length > val.length) {
			goto mismatch;
		}
		if (memcmp((char *)val.data, (char *)cnk.data, cnk.length) != 0) goto mismatch;
		val.length -= cnk.length;
		val.data += cnk.length;
		c++;
		talloc_free(cnk.data);
		cnk.data = NULL;
	}

	while (tree->u.substring.chunks[c]) {

		chunk = tree->u.substring.chunks[c];
		if(a->syntax->canonicalise_fn(ldb, ldb, chunk, &cnk) != 0) goto mismatch;

		/* FIXME: case of embedded nulls */
		p = strstr((char *)val.data, (char *)cnk.data);
		if (p == NULL) goto mismatch;
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

	/* last chunk may not have reached end of string */
	if ( (! tree->u.substring.end_with_wildcard) && (*(val.data) != 0) ) goto mismatch;
	talloc_free(save_p);
	*matched = true;
	return LDB_SUCCESS;

mismatch:
	*matched = false;
	talloc_free(save_p);
	talloc_free(cnk.data);
	return LDB_SUCCESS;
}

/*
  match a simple leaf node
*/
static int ldb_match_substring(struct ldb_context *ldb, 
			       const struct ldb_message *msg,
			       const struct ldb_parse_tree *tree,
			       enum ldb_scope scope, bool *matched)
{
	unsigned int i;
	struct ldb_message_element *el;

	el = ldb_msg_find_element(msg, tree->u.substring.attr);
	if (el == NULL) {
		*matched = false;
		return LDB_SUCCESS;
	}

	for (i = 0; i < el->num_values; i++) {
		int ret;
		ret = ldb_wildcard_compare(ldb, tree, el->values[i], matched);
		if (ret != LDB_SUCCESS) return ret;
		if (*matched) return LDB_SUCCESS;
	}

	*matched = false;
	return LDB_SUCCESS;
}


/*
  bitwise-and comparator
*/
static int ldb_comparator_bitmask(const char *oid, const struct ldb_val *v1, const struct ldb_val *v2,
				  bool *matched)
{
	uint64_t i1, i2;
	char ibuf[100];
	char *endptr = NULL;

	if (v1->length >= sizeof(ibuf)-1) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}
	memcpy(ibuf, (char *)v1->data, v1->length);
	ibuf[v1->length] = 0;
	i1 = strtoull(ibuf, &endptr, 0);
	if (endptr != NULL) {
		if (endptr == ibuf || *endptr != 0) {
			return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
		}
	}

	if (v2->length >= sizeof(ibuf)-1) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}
	endptr = NULL;
	memcpy(ibuf, (char *)v2->data, v2->length);
	ibuf[v2->length] = 0;
	i2 = strtoull(ibuf, &endptr, 0);
	if (endptr != NULL) {
		if (endptr == ibuf || *endptr != 0) {
			return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
		}
	}
	if (strcmp(LDB_OID_COMPARATOR_AND, oid) == 0) {
		*matched = ((i1 & i2) == i2);
	} else if (strcmp(LDB_OID_COMPARATOR_OR, oid) == 0) {
		*matched = ((i1 & i2) != 0);
	} else {
		return LDB_ERR_INAPPROPRIATE_MATCHING;
	}
	return LDB_SUCCESS;
}


/*
  extended match, handles things like bitops
*/
static int ldb_match_extended(struct ldb_context *ldb, 
			      const struct ldb_message *msg,
			      const struct ldb_parse_tree *tree,
			      enum ldb_scope scope, bool *matched)
{
	unsigned int i;
	const struct {
		const char *oid;
		int (*comparator)(const char *, const struct ldb_val *, const struct ldb_val *, bool *);
	} rules[] = {
		{ LDB_OID_COMPARATOR_AND, ldb_comparator_bitmask},
		{ LDB_OID_COMPARATOR_OR, ldb_comparator_bitmask}
	};
	int (*comp)(const char *,const struct ldb_val *, const struct ldb_val *, bool *) = NULL;
	struct ldb_message_element *el;

	if (tree->u.extended.dnAttributes) {
		/* FIXME: We really need to find out what this ":dn" part in
		 * an extended match means and how to handle it. For now print
		 * only a warning to have s3 winbind and other tools working
		 * against us. - Matthias */
		ldb_debug(ldb, LDB_DEBUG_WARNING, "ldb: dnAttributes extended match not supported yet");
	}
	if (tree->u.extended.rule_id == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb: no-rule extended matches not supported yet");
		return LDB_ERR_INAPPROPRIATE_MATCHING;
	}
	if (tree->u.extended.attr == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb: no-attribute extended matches not supported yet");
		return LDB_ERR_INAPPROPRIATE_MATCHING;
	}

	for (i=0;i<ARRAY_SIZE(rules);i++) {
		if (strcmp(rules[i].oid, tree->u.extended.rule_id) == 0) {
			comp = rules[i].comparator;
			break;
		}
	}
	if (comp == NULL) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb: unknown extended rule_id %s",
			  tree->u.extended.rule_id);
		return LDB_ERR_INAPPROPRIATE_MATCHING;
	}

	/* find the message element */
	el = ldb_msg_find_element(msg, tree->u.extended.attr);
	if (el == NULL) {
		*matched = false;
		return LDB_SUCCESS;
	}

	for (i=0;i<el->num_values;i++) {
		int ret = comp(tree->u.extended.rule_id, &el->values[i], &tree->u.extended.value, matched);
		if (ret != LDB_SUCCESS) return ret;
		if (*matched) return LDB_SUCCESS;
	}

	*matched = false;
	return LDB_SUCCESS;
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
			     enum ldb_scope scope, bool *matched)
{
	unsigned int i;
	int ret;

	*matched = false;

	switch (tree->operation) {
	case LDB_OP_AND:
		for (i=0;i<tree->u.list.num_elements;i++) {
			ret = ldb_match_message(ldb, msg, tree->u.list.elements[i], scope, matched);
			if (ret != LDB_SUCCESS) return ret;
			if (!*matched) return LDB_SUCCESS;
		}
		*matched = true;
		return LDB_SUCCESS;

	case LDB_OP_OR:
		for (i=0;i<tree->u.list.num_elements;i++) {
			ret = ldb_match_message(ldb, msg, tree->u.list.elements[i], scope, matched);
			if (ret != LDB_SUCCESS) return ret;
			if (*matched) return LDB_SUCCESS;
		}
		*matched = false;
		return LDB_SUCCESS;

	case LDB_OP_NOT:
		ret = ldb_match_message(ldb, msg, tree->u.isnot.child, scope, matched);
		if (ret != LDB_SUCCESS) return ret;
		*matched = ! *matched;
		return LDB_SUCCESS;

	case LDB_OP_EQUALITY:
		return ldb_match_equality(ldb, msg, tree, scope, matched);

	case LDB_OP_SUBSTRING:
		return ldb_match_substring(ldb, msg, tree, scope, matched);

	case LDB_OP_GREATER:
		return ldb_match_comparison(ldb, msg, tree, scope, LDB_OP_GREATER, matched);

	case LDB_OP_LESS:
		return ldb_match_comparison(ldb, msg, tree, scope, LDB_OP_LESS, matched);

	case LDB_OP_PRESENT:
		return ldb_match_present(ldb, msg, tree, scope, matched);

	case LDB_OP_APPROX:
		return ldb_match_comparison(ldb, msg, tree, scope, LDB_OP_APPROX, matched);

	case LDB_OP_EXTENDED:
		return ldb_match_extended(ldb, msg, tree, scope, matched);
	}

	return LDB_ERR_INAPPROPRIATE_MATCHING;
}

int ldb_match_msg(struct ldb_context *ldb,
		  const struct ldb_message *msg,
		  const struct ldb_parse_tree *tree,
		  struct ldb_dn *base,
		  enum ldb_scope scope)
{
	bool matched;
	int ret;

	if ( ! ldb_match_scope(ldb, base, msg->dn, scope) ) {
		return 0;
	}

	ret = ldb_match_message(ldb, msg, tree, scope, &matched);
	if (ret != LDB_SUCCESS) {
		/* to match the old API, we need to consider this a
		   failure to match */
		return 0;
	}
	return matched?1:0;
}

int ldb_match_msg_error(struct ldb_context *ldb,
			const struct ldb_message *msg,
			const struct ldb_parse_tree *tree,
			struct ldb_dn *base,
			enum ldb_scope scope,
			bool *matched)
{
	if ( ! ldb_match_scope(ldb, base, msg->dn, scope) ) {
		*matched = false;
		return LDB_SUCCESS;
	}

	return ldb_match_message(ldb, msg, tree, scope, matched);
}

int ldb_match_msg_objectclass(const struct ldb_message *msg,
			      const char *objectclass)
{
	unsigned int i;
	struct ldb_message_element *el = ldb_msg_find_element(msg, "objectClass");
	if (!el) {
		return 0;
	}
	for (i=0; i < el->num_values; i++) {
		if (ldb_attr_cmp((const char *)el->values[i].data, objectclass) == 0) {
			return 1;
		}
	}
	return 0;
}


			      
