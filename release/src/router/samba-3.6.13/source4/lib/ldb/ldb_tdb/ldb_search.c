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
 *  Component: ldb search functions
 *
 *  Description: functions to search ldb+tdb databases
 *
 *  Author: Andrew Tridgell
 */

#include "ldb_tdb.h"

/*
  add one element to a message
*/
static int msg_add_element(struct ldb_message *ret, 
			   const struct ldb_message_element *el,
			   int check_duplicates)
{
	unsigned int i;
	struct ldb_message_element *e2, *elnew;

	if (check_duplicates && ldb_msg_find_element(ret, el->name)) {
		/* its already there */
		return 0;
	}

	e2 = talloc_realloc(ret, ret->elements, struct ldb_message_element, ret->num_elements+1);
	if (!e2) {
		return -1;
	}
	ret->elements = e2;
	
	elnew = &e2[ret->num_elements];

	elnew->name = talloc_strdup(ret->elements, el->name);
	if (!elnew->name) {
		return -1;
	}

	if (el->num_values) {
		elnew->values = talloc_array(ret->elements, struct ldb_val, el->num_values);
		if (!elnew->values) {
			return -1;
		}
	} else {
		elnew->values = NULL;
	}

	for (i=0;i<el->num_values;i++) {
		elnew->values[i] = ldb_val_dup(elnew->values, &el->values[i]);
		if (elnew->values[i].length != el->values[i].length) {
			return -1;
		}
	}

	elnew->num_values = el->num_values;
	elnew->flags = el->flags;

	ret->num_elements++;

	return 0;
}

/*
  add the special distinguishedName element
*/
static int msg_add_distinguished_name(struct ldb_message *msg)
{
	struct ldb_message_element el;
	struct ldb_val val;
	int ret;

	el.flags = 0;
	el.name = "distinguishedName";
	el.num_values = 1;
	el.values = &val;
	el.flags = 0;
	val.data = (uint8_t *)ldb_dn_alloc_linearized(msg, msg->dn);
	val.length = strlen((char *)val.data);
	
	ret = msg_add_element(msg, &el, 1);
	return ret;
}

/*
  add all elements from one message into another
 */
static int msg_add_all_elements(struct ldb_module *module, struct ldb_message *ret,
				const struct ldb_message *msg)
{
	struct ldb_context *ldb;
	unsigned int i;
	int check_duplicates = (ret->num_elements != 0);

	ldb = ldb_module_get_ctx(module);

	if (msg_add_distinguished_name(ret) != 0) {
		return -1;
	}

	for (i=0;i<msg->num_elements;i++) {
		const struct ldb_schema_attribute *a;
		a = ldb_schema_attribute_by_name(ldb, msg->elements[i].name);
		if (a->flags & LDB_ATTR_FLAG_HIDDEN) {
			continue;
		}
		if (msg_add_element(ret, &msg->elements[i],
				    check_duplicates) != 0) {
			return -1;
		}
	}

	return 0;
}


/*
  pull the specified list of attributes from a message
 */
static struct ldb_message *ltdb_pull_attrs(struct ldb_module *module, 
					   TALLOC_CTX *mem_ctx, 
					   const struct ldb_message *msg, 
					   const char * const *attrs)
{
	struct ldb_message *ret;
	unsigned int i;

	ret = talloc(mem_ctx, struct ldb_message);
	if (!ret) {
		return NULL;
	}

	ret->dn = ldb_dn_copy(ret, msg->dn);
	if (!ret->dn) {
		talloc_free(ret);
		return NULL;
	}

	ret->num_elements = 0;
	ret->elements = NULL;

	if (!attrs) {
		if (msg_add_all_elements(module, ret, msg) != 0) {
			talloc_free(ret);
			return NULL;
		}
		return ret;
	}

	for (i=0;attrs[i];i++) {
		struct ldb_message_element *el;

		if (strcmp(attrs[i], "*") == 0) {
			if (msg_add_all_elements(module, ret, msg) != 0) {
				talloc_free(ret);
				return NULL;
			}
			continue;
		}

		if (ldb_attr_cmp(attrs[i], "distinguishedName") == 0) {
			if (msg_add_distinguished_name(ret) != 0) {
				return NULL;
			}
			continue;
		}

		el = ldb_msg_find_element(msg, attrs[i]);
		if (!el) {
			continue;
		}
		if (msg_add_element(ret, el, 1) != 0) {
			talloc_free(ret);
			return NULL;				
		}
	}

	return ret;
}

/*
  search the database for a single simple dn.
  return LDB_ERR_NO_SUCH_OBJECT on record-not-found
  and LDB_SUCCESS on success
*/
static int ltdb_search_base(struct ldb_module *module, struct ldb_dn *dn)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	TDB_DATA tdb_key, tdb_data;

	if (ldb_dn_is_null(dn)) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}

	/* form the key */
	tdb_key = ltdb_key(module, dn);
	if (!tdb_key.dptr) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	tdb_data = tdb_fetch(ltdb->tdb, tdb_key);
	talloc_free(tdb_key.dptr);
	if (!tdb_data.dptr) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	
	free(tdb_data.dptr);
	return LDB_SUCCESS;
}

/*
  search the database for a single simple dn, returning all attributes
  in a single message

  return LDB_ERR_NO_SUCH_OBJECT on record-not-found
  and LDB_SUCCESS on success
*/
int ltdb_search_dn1(struct ldb_module *module, struct ldb_dn *dn, struct ldb_message *msg)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	int ret;
	TDB_DATA tdb_key, tdb_data;

	memset(msg, 0, sizeof(*msg));

	/* form the key */
	tdb_key = ltdb_key(module, dn);
	if (!tdb_key.dptr) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	tdb_data = tdb_fetch(ltdb->tdb, tdb_key);
	talloc_free(tdb_key.dptr);
	if (!tdb_data.dptr) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}
	
	msg->num_elements = 0;
	msg->elements = NULL;

	ret = ltdb_unpack_data(module, &tdb_data, msg);
	free(tdb_data.dptr);
	if (ret == -1) {
		struct ldb_context *ldb = ldb_module_get_ctx(module);
		ldb_debug(ldb, LDB_DEBUG_ERROR, "Invalid data for index %s\n",
			  ldb_dn_get_linearized(msg->dn));
		return LDB_ERR_OPERATIONS_ERROR;		
	}

	if (!msg->dn) {
		msg->dn = ldb_dn_copy(msg, dn);
	}
	if (!msg->dn) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

/*
  add a set of attributes from a record to a set of results
  return 0 on success, -1 on failure
*/
int ltdb_add_attr_results(struct ldb_module *module, 
			  TALLOC_CTX *mem_ctx, 
			  struct ldb_message *msg,
			  const char * const attrs[], 
			  unsigned int *count, 
			  struct ldb_message ***res)
{
	struct ldb_message *msg2;
	struct ldb_message **res2;

	/* pull the attributes that the user wants */
	msg2 = ltdb_pull_attrs(module, mem_ctx, msg, attrs);
	if (!msg2) {
		return -1;
	}

	/* add to the results list */
	res2 = talloc_realloc(mem_ctx, *res, struct ldb_message *, (*count)+2);
	if (!res2) {
		talloc_free(msg2);
		return -1;
	}

	(*res) = res2;

	(*res)[*count] = talloc_move(*res, &msg2);
	(*res)[(*count)+1] = NULL;
	(*count)++;

	return 0;
}



/*
  filter the specified list of attributes from a message
  removing not requested attrs.
 */
int ltdb_filter_attrs(struct ldb_message *msg, const char * const *attrs)
{
	unsigned int i;
	int keep_all = 0;
	struct ldb_message_element *el2;
	uint32_t num_elements;

	if (attrs) {
		/* check for special attrs */
		for (i = 0; attrs[i]; i++) {
			if (strcmp(attrs[i], "*") == 0) {
				keep_all = 1;
				break;
			}

			if (ldb_attr_cmp(attrs[i], "distinguishedName") == 0) {
				if (msg_add_distinguished_name(msg) != 0) {
					return -1;
				}
			}
		}
	} else {
		keep_all = 1;
	}
	
	if (keep_all) {
		if (msg_add_distinguished_name(msg) != 0) {
			return -1;
		}
		return 0;
	}

	el2 = talloc_array(msg, struct ldb_message_element, msg->num_elements);
	if (el2 == NULL) {
		return -1;
	}
	num_elements = 0;

	for (i = 0; i < msg->num_elements; i++) {
		unsigned int j;
		int found = 0;
		
		for (j = 0; attrs[j]; j++) {
			if (ldb_attr_cmp(msg->elements[i].name, attrs[j]) == 0) {
				found = 1;
				break;
			}
		}

		if (found) {
			el2[num_elements] = msg->elements[i];
			talloc_steal(el2, el2[num_elements].name);
			talloc_steal(el2, el2[num_elements].values);
			num_elements++;
		}
	}

	talloc_free(msg->elements);
	msg->elements = talloc_realloc(msg, el2, struct ldb_message_element, msg->num_elements);
	if (msg->elements == NULL) {
		return -1;
	}
	msg->num_elements = num_elements;

	return 0;
}

/*
  search function for a non-indexed search
 */
static int search_func(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data, void *state)
{
	struct ldb_context *ldb;
	struct ltdb_context *ac;
	struct ldb_message *msg;
	int ret;
	bool matched;

	ac = talloc_get_type(state, struct ltdb_context);
	ldb = ldb_module_get_ctx(ac->module);

	if (key.dsize < 4 || 
	    strncmp((char *)key.dptr, "DN=", 3) != 0) {
		return 0;
	}

	msg = ldb_msg_new(ac);
	if (!msg) {
		return -1;
	}

	/* unpack the record */
	ret = ltdb_unpack_data(ac->module, &data, msg);
	if (ret == -1) {
		talloc_free(msg);
		return -1;
	}

	if (!msg->dn) {
		msg->dn = ldb_dn_new(msg, ldb,
				     (char *)key.dptr + 3);
		if (msg->dn == NULL) {
			talloc_free(msg);
			return -1;
		}
	}

	/* see if it matches the given expression */
	ret = ldb_match_msg_error(ldb, msg,
				  ac->tree, ac->base, ac->scope, &matched);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return -1;
	}
	if (!matched) {
		talloc_free(msg);
		return 0;
	}

	/* filter the attributes that the user wants */
	ret = ltdb_filter_attrs(msg, ac->attrs);

	if (ret == -1) {
		talloc_free(msg);
		return -1;
	}

	ret = ldb_module_send_entry(ac->req, msg, NULL);
	if (ret != LDB_SUCCESS) {
		ac->request_terminated = true;
		/* the callback failed, abort the operation */
		return -1;
	}

	return 0;
}


/*
  search the database with a LDAP-like expression.
  this is the "full search" non-indexed variant
*/
static int ltdb_search_full(struct ltdb_context *ctx)
{
	void *data = ldb_module_get_private(ctx->module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	int ret;

	if (ltdb->in_transaction != 0) {
		ret = tdb_traverse(ltdb->tdb, search_func, ctx);
	} else {
		ret = tdb_traverse_read(ltdb->tdb, search_func, ctx);
	}

	if (ret == -1) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}

/*
  search the database with a LDAP-like expression.
  choses a search method
*/
int ltdb_search(struct ltdb_context *ctx)
{
	struct ldb_context *ldb;
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	int ret;

	ldb = ldb_module_get_ctx(module);

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	if (ltdb_lock_read(module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (ltdb_cache_load(module) != 0) {
		ltdb_unlock_read(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (req->op.search.tree == NULL) {
		ltdb_unlock_read(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if ((req->op.search.base == NULL) || (ldb_dn_is_null(req->op.search.base) == true)) {

		/* Check what we should do with a NULL dn */
		switch (req->op.search.scope) {
		case LDB_SCOPE_BASE:
			ldb_asprintf_errstring(ldb, 
					       "NULL Base DN invalid for a base search");
			ret = LDB_ERR_INVALID_DN_SYNTAX;
			break;
		case LDB_SCOPE_ONELEVEL:
			ldb_asprintf_errstring(ldb, 
					       "NULL Base DN invalid for a one-level search");
			ret = LDB_ERR_INVALID_DN_SYNTAX;	
			break;
		case LDB_SCOPE_SUBTREE:
		default:
			/* We accept subtree searches from a NULL base DN, ie over the whole DB */
			ret = LDB_SUCCESS;
		}
	} else if (ldb_dn_is_valid(req->op.search.base) == false) {

		/* We don't want invalid base DNs here */
		ldb_asprintf_errstring(ldb, 
				       "Invalid Base DN: %s", 
				       ldb_dn_get_linearized(req->op.search.base));
		ret = LDB_ERR_INVALID_DN_SYNTAX;

	} else if (ltdb->check_base) {
		/* This database has been marked as 'checkBaseOnSearch', so do a spot check of the base dn */
		ret = ltdb_search_base(module, req->op.search.base);
		
		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			ldb_asprintf_errstring(ldb, 
					       "No such Base DN: %s", 
					       ldb_dn_get_linearized(req->op.search.base));
		}
			
	} else {
		/* If we are not checking the base DN life is easy */
		ret = LDB_SUCCESS;
	}

	ctx->tree = req->op.search.tree;
	ctx->scope = req->op.search.scope;
	ctx->base = req->op.search.base;
	ctx->attrs = req->op.search.attrs;

	if (ret == LDB_SUCCESS) {
		uint32_t match_count = 0;

		ret = ltdb_search_indexed(ctx, &match_count);
		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			/* Not in the index, therefore OK! */
			ret = LDB_SUCCESS;
			
		}
		/* Check if we got just a normal error.
		 * In that case proceed to a full search unless we got a
		 * callback error */
		if ( ! ctx->request_terminated && ret != LDB_SUCCESS) {
			/* Not indexed, so we need to do a full scan */
			if (ltdb->warn_unindexed) {
				/* useful for debugging when slow performance
				 * is caused by unindexed searches */
				char *expression = ldb_filter_from_tree(ctx, ctx->tree);
				ldb_debug(ldb, LDB_DEBUG_WARNING, "ldb FULL SEARCH: %s SCOPE: %s DN: %s\n",
							expression,
							req->op.search.scope==LDB_SCOPE_BASE?"base":
							req->op.search.scope==LDB_SCOPE_ONELEVEL?"one":
							req->op.search.scope==LDB_SCOPE_SUBTREE?"sub":"UNKNOWN",
							ldb_dn_get_linearized(req->op.search.base));

				talloc_free(expression);
			}
			if (match_count != 0) {
				/* the indexing code gave an error
				 * after having returned at least one
				 * entry. This means the indexes are
				 * corrupt or a database record is
				 * corrupt. We cannot continue with a
				 * full search or we may return
				 * duplicate entries
				 */
				ltdb_unlock_read(module);
				return LDB_ERR_OPERATIONS_ERROR;
			}
			ret = ltdb_search_full(ctx);
			if (ret != LDB_SUCCESS) {
				ldb_set_errstring(ldb, "Indexed and full searches both failed!\n");
			}
		}
	}

	ltdb_unlock_read(module);

	return ret;
}

