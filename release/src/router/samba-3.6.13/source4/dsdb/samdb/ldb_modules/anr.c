/* 
   ldb database library

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2007
   Copyright (C) Simo Sorce <idra@samba.org> 2008
   Copyright (C) Andrew Tridgell  2004
    
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

/*
 *  Name: ldb
 *
 *  Component: ldb anr module
 *
 *  Description: module to implement 'ambiguous name resolution'
 *
 *  Author: Andrew Bartlett
 */

#include "includes.h"
#include "ldb_module.h"
#include "dsdb/samdb/samdb.h"
#include "dsdb/samdb/ldb_modules/util.h"

/**
 * Make a and 'and' or 'or' tree from the two supplied elements 
 */
static struct ldb_parse_tree *make_parse_list(struct ldb_module *module,
				       TALLOC_CTX *mem_ctx, enum ldb_parse_op op, 
				       struct ldb_parse_tree *first_arm, struct ldb_parse_tree *second_arm)
{
	struct ldb_context *ldb;
	struct ldb_parse_tree *list;

	ldb = ldb_module_get_ctx(module);

	list = talloc(mem_ctx, struct ldb_parse_tree);
	if (list == NULL){
		ldb_oom(ldb);
		return NULL;
	}
	list->operation = op;
	
	list->u.list.num_elements = 2;
	list->u.list.elements = talloc_array(list, struct ldb_parse_tree *, 2);
	if (!list->u.list.elements) {
		ldb_oom(ldb);
		return NULL;
	}
	list->u.list.elements[0] = talloc_steal(list, first_arm);
	list->u.list.elements[1] = talloc_steal(list, second_arm);
	return list;
}

/**
 * Make an equality or prefix match tree, from the attribute, operation and matching value supplied
 */
static struct ldb_parse_tree *make_match_tree(struct ldb_module *module,
					      TALLOC_CTX *mem_ctx,
					      enum ldb_parse_op op,
					      const char *attr,
					      struct ldb_val *match)
{
	struct ldb_context *ldb;
	struct ldb_parse_tree *match_tree;

	ldb = ldb_module_get_ctx(module);

	match_tree = talloc(mem_ctx, struct ldb_parse_tree);
	
	/* Depending on what type of match was selected, fill in the right part of the union */
	 
	match_tree->operation = op;
	switch (op) {
	case LDB_OP_SUBSTRING:
		match_tree->u.substring.attr = attr;
		
		match_tree->u.substring.start_with_wildcard = 0;
		match_tree->u.substring.end_with_wildcard = 1;
		match_tree->u.substring.chunks = talloc_array(match_tree, struct ldb_val *, 2);
		
		if (match_tree->u.substring.chunks == NULL){
			talloc_free(match_tree);
			ldb_oom(ldb);
			return NULL;
		}
		match_tree->u.substring.chunks[0] = match;
		match_tree->u.substring.chunks[1] = NULL;
		break;
	case LDB_OP_EQUALITY:
		match_tree->u.equality.attr = attr;
		match_tree->u.equality.value = *match;
		break;
	default:
		talloc_free(match_tree);
		return NULL;
	}
	return match_tree;
}

struct anr_context {
	bool found_anr;
	struct ldb_module *module;
	struct ldb_request *req;
};

/**
 * Given the match for an 'ambigious name resolution' query, create a
 * parse tree with an 'or' of all the anr attributes in the schema.  
 */

/**
 * Callback function to do the heavy lifting for the parse tree walker
 */
static int anr_replace_value(struct anr_context *ac,
			     TALLOC_CTX *mem_ctx,
			     struct ldb_val *match,
			     struct ldb_parse_tree **ntree)
{
	struct ldb_parse_tree *tree = NULL;
	struct ldb_module *module = ac->module;
	struct ldb_parse_tree *match_tree;
	struct dsdb_attribute *cur;
	const struct dsdb_schema *schema;
	struct ldb_context *ldb;
	uint8_t *p;
	enum ldb_parse_op op;

	ldb = ldb_module_get_ctx(module);

	schema = dsdb_get_schema(ldb, ac);
	if (!schema) {
		ldb_asprintf_errstring(ldb, "no schema with which to construct anr filter");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->found_anr = true;

	if (match->length > 1 && match->data[0] == '=') {
		struct ldb_val *match2 = talloc(mem_ctx, struct ldb_val);
		if (match2 == NULL){
			return ldb_oom(ldb);
		}
		*match2 = data_blob_const(match->data+1, match->length - 1);
		match = match2;
		op = LDB_OP_EQUALITY;
	} else {
		op = LDB_OP_SUBSTRING;
	}
	for (cur = schema->attributes; cur; cur = cur->next) {
		if (!(cur->searchFlags & SEARCH_FLAG_ANR)) continue;
		match_tree = make_match_tree(module, mem_ctx, op, cur->lDAPDisplayName, match);

		if (tree) {
			/* Inject an 'or' with the current tree */
			tree = make_parse_list(module, mem_ctx,  LDB_OP_OR, tree, match_tree);
			if (tree == NULL) {
				return ldb_oom(ldb);
			}
		} else {
			tree = match_tree;
		}
	}

	
	/* If the search term has a space in it, 
	   split it up at the first space.  */
	
	p = memchr(match->data, ' ', match->length);

	if (p) {
		struct ldb_parse_tree *first_split_filter, *second_split_filter, *split_filters, *match_tree_1, *match_tree_2;
		struct ldb_val *first_match = talloc(tree, struct ldb_val);
		struct ldb_val *second_match = talloc(tree, struct ldb_val);
		if (!first_match || !second_match) {
			return ldb_oom(ldb);
		}
		*first_match = data_blob_const(match->data, p-match->data);
		*second_match = data_blob_const(p+1, match->length - (p-match->data) - 1);
		
		/* Add (|(&(givenname=first)(sn=second))(&(givenname=second)(sn=first))) */

		match_tree_1 = make_match_tree(module, mem_ctx, op, "givenName", first_match);
		match_tree_2 = make_match_tree(module, mem_ctx, op, "sn", second_match);

		first_split_filter = make_parse_list(module, ac,  LDB_OP_AND, match_tree_1, match_tree_2);
		if (first_split_filter == NULL){
			return ldb_oom(ldb);
		}
		
		match_tree_1 = make_match_tree(module, mem_ctx, op, "sn", first_match);
		match_tree_2 = make_match_tree(module, mem_ctx, op, "givenName", second_match);

		second_split_filter = make_parse_list(module, ac,  LDB_OP_AND, match_tree_1, match_tree_2);
		if (second_split_filter == NULL){
			return ldb_oom(ldb);
		}

		split_filters = make_parse_list(module, mem_ctx,  LDB_OP_OR, 
						first_split_filter, second_split_filter);
		if (split_filters == NULL) {
			return ldb_oom(ldb);
		}

		if (tree) {
			/* Inject an 'or' with the current tree */
			tree = make_parse_list(module, mem_ctx,  LDB_OP_OR, tree, split_filters);
		} else {
			tree = split_filters;
		}
	}
	*ntree = tree;
	return LDB_SUCCESS;
}

/*
  replace any occurances of an attribute with a new, generated attribute tree
*/
static int anr_replace_subtrees(struct anr_context *ac,
				struct ldb_parse_tree *tree,
				const char *attr,
				struct ldb_parse_tree **ntree)
{
	int ret;
	unsigned int i;

	switch (tree->operation) {
	case LDB_OP_AND:
	case LDB_OP_OR:
		for (i=0;i<tree->u.list.num_elements;i++) {
			ret = anr_replace_subtrees(ac, tree->u.list.elements[i],
						   attr, &tree->u.list.elements[i]);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
			*ntree = tree;
		}
		break;
	case LDB_OP_NOT:
		ret = anr_replace_subtrees(ac, tree->u.isnot.child, attr, &tree->u.isnot.child);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
		*ntree = tree;
		break;
	case LDB_OP_EQUALITY:
		if (ldb_attr_cmp(tree->u.equality.attr, attr) == 0) {
			ret = anr_replace_value(ac, tree, &tree->u.equality.value, ntree);
			if (ret != LDB_SUCCESS) {
				return ret;
			}
		}
		break;
	case LDB_OP_SUBSTRING:
		if (ldb_attr_cmp(tree->u.substring.attr, attr) == 0) {
			if (tree->u.substring.start_with_wildcard == 0 &&
			    tree->u.substring.end_with_wildcard == 1 && 
			    tree->u.substring.chunks[0] != NULL && 
			    tree->u.substring.chunks[1] == NULL) {
				ret = anr_replace_value(ac, tree, tree->u.substring.chunks[0], ntree);
				if (ret != LDB_SUCCESS) {
					return ret;
				}
			}
		}
		break;
	default:
		break;
	}

	return LDB_SUCCESS;
}

static int anr_search_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct anr_context *ac;

	ac = talloc_get_type(req->context, struct anr_context);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		return ldb_module_send_entry(ac->req, ares->message, ares->controls);

	case LDB_REPLY_REFERRAL:
		return ldb_module_send_referral(ac->req, ares->referral);

	case LDB_REPLY_DONE:
		return ldb_module_done(ac->req, ares->controls,
					ares->response, LDB_SUCCESS);

	}
	return LDB_SUCCESS;
}

/* search */
static int anr_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_context *ldb;
	struct ldb_parse_tree *anr_tree;
	struct ldb_request *down_req;
	struct anr_context *ac;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ac = talloc(req, struct anr_context);
	if (!ac) {
		return ldb_oom(ldb);
	}

	ac->module = module;
	ac->req = req;
	ac->found_anr = false;

#if 0
	printf("oldanr : %s\n", ldb_filter_from_tree (0, req->op.search.tree));
#endif

	ret = anr_replace_subtrees(ac, req->op.search.tree, "anr", &anr_tree);
	if (ret != LDB_SUCCESS) {
		return ldb_operr(ldb);
	}

	if (!ac->found_anr) {
		talloc_free(ac);
		return ldb_next_request(module, req);
	}

	ret = ldb_build_search_req_ex(&down_req,
					ldb, ac,
					req->op.search.base,
					req->op.search.scope,
					anr_tree,
					req->op.search.attrs,
					req->controls,
					ac, anr_search_callback,
					req);
	LDB_REQ_SET_LOCATION(down_req);
	if (ret != LDB_SUCCESS) {
		return ldb_operr(ldb);
	}
	talloc_steal(down_req, anr_tree);

	return ldb_next_request(module, down_req);
}

static const struct ldb_module_ops ldb_anr_module_ops = {
	.name		   = "anr",
	.search = anr_search
};

int ldb_anr_module_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_module(&ldb_anr_module_ops);
}
