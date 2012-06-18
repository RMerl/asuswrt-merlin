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
 *  Component: ldb tdb backend - indexing
 *
 *  Description: indexing routines for ldb tdb backend
 *
 *  Author: Andrew Tridgell
 */

#include "includes.h"
#include "ldb/include/includes.h"

#include "ldb/ldb_tdb/ldb_tdb.h"

/*
  find an element in a list, using the given comparison function and
  assuming that the list is already sorted using comp_fn

  return -1 if not found, or the index of the first occurance of needle if found
*/
static int ldb_list_find(const void *needle, 
			 const void *base, size_t nmemb, size_t size, 
			 comparison_fn_t comp_fn)
{
	const char *base_p = (const char *)base;
	size_t min_i, max_i, test_i;

	if (nmemb == 0) {
		return -1;
	}

	min_i = 0;
	max_i = nmemb-1;

	while (min_i < max_i) {
		int r;

		test_i = (min_i + max_i) / 2;
		/* the following cast looks strange, but is
		 correct. The key to understanding it is that base_p
		 is a pointer to an array of pointers, so we have to
		 dereference it after casting to void **. The strange
		 const in the middle gives us the right type of pointer
		 after the dereference  (tridge) */
		r = comp_fn(needle, *(void * const *)(base_p + (size * test_i)));
		if (r == 0) {
			/* scan back for first element */
			while (test_i > 0 &&
			       comp_fn(needle, *(void * const *)(base_p + (size * (test_i-1)))) == 0) {
				test_i--;
			}
			return test_i;
		}
		if (r < 0) {
			if (test_i == 0) {
				return -1;
			}
			max_i = test_i - 1;
		}
		if (r > 0) {
			min_i = test_i + 1;
		}
	}

	if (comp_fn(needle, *(void * const *)(base_p + (size * min_i))) == 0) {
		return min_i;
	}

	return -1;
}

struct dn_list {
	unsigned int count;
	char **dn;
};

/*
  return the dn key to be used for an index
  caller frees
*/
static struct ldb_dn *ldb_dn_key(struct ldb_context *ldb,
			const char *attr, const struct ldb_val *value)
{
	struct ldb_dn *ret;
	char *dn;
	struct ldb_val v;
	const struct ldb_attrib_handler *h;
	char *attr_folded;

	attr_folded = ldb_attr_casefold(ldb, attr);
	if (!attr_folded) {
		return NULL;
	}

	h = ldb_attrib_handler(ldb, attr);
	if (h->canonicalise_fn(ldb, ldb, value, &v) != 0) {
		/* canonicalisation can be refused. For example, 
		   a attribute that takes wildcards will refuse to canonicalise
		   if the value contains a wildcard */
		talloc_free(attr_folded);
		return NULL;
	}
	if (ldb_should_b64_encode(&v)) {
		char *vstr = ldb_base64_encode(ldb, (char *)v.data, v.length);
		if (!vstr) return NULL;
		dn = talloc_asprintf(ldb, "%s:%s::%s", LTDB_INDEX, attr_folded, vstr);
		talloc_free(vstr);
		if (v.data != value->data) {
			talloc_free(v.data);
		}
		talloc_free(attr_folded);
		if (dn == NULL) return NULL;
		goto done;
	}

	dn = talloc_asprintf(ldb, "%s:%s:%.*s", 
			      LTDB_INDEX, attr_folded, (int)v.length, (char *)v.data);

	if (v.data != value->data) {
		talloc_free(v.data);
	}
	talloc_free(attr_folded);

done:
	ret = ldb_dn_explode(ldb, dn);
	talloc_free(dn);
	return ret;
}

/*
  see if a attribute value is in the list of indexed attributes
*/
static int ldb_msg_find_idx(const struct ldb_message *msg, const char *attr,
			    unsigned int *v_idx, const char *key)
{
	unsigned int i, j;
	for (i=0;i<msg->num_elements;i++) {
		if (ldb_attr_cmp(msg->elements[i].name, key) == 0) {
			const struct ldb_message_element *el = 
				&msg->elements[i];
			for (j=0;j<el->num_values;j++) {
				if (ldb_attr_cmp((char *)el->values[j].data, attr) == 0) {
					if (v_idx) {
						*v_idx = j;
					}
					return i;
				}
			}
		}
	}
	return -1;
}

/* used in sorting dn lists */
static int list_cmp(const char **s1, const char **s2)
{
	return strcmp(*s1, *s2);
}

/*
  return a list of dn's that might match a simple indexed search or
 */
static int ltdb_index_dn_simple(struct ldb_module *module, 
				const struct ldb_parse_tree *tree,
				const struct ldb_message *index_list,
				struct dn_list *list)
{
	struct ldb_context *ldb = module->ldb;
	struct ldb_dn *dn;
	int ret;
	unsigned int i, j;
	struct ldb_message *msg;

	list->count = 0;
	list->dn = NULL;

	/* if the attribute isn't in the list of indexed attributes then
	   this node needs a full search */
	if (ldb_msg_find_idx(index_list, tree->u.equality.attr, NULL, LTDB_IDXATTR) == -1) {
		return -1;
	}

	/* the attribute is indexed. Pull the list of DNs that match the 
	   search criterion */
	dn = ldb_dn_key(ldb, tree->u.equality.attr, &tree->u.equality.value);
	if (!dn) return -1;

	msg = talloc(list, struct ldb_message);
	if (msg == NULL) {
		return -1;
	}

	ret = ltdb_search_dn1(module, dn, msg);
	talloc_free(dn);
	if (ret == 0 || ret == -1) {
		return ret;
	}

	for (i=0;i<msg->num_elements;i++) {
		struct ldb_message_element *el;

		if (strcmp(msg->elements[i].name, LTDB_IDX) != 0) {
			continue;
		}

		el = &msg->elements[i];

		list->dn = talloc_array(list, char *, el->num_values);
		if (!list->dn) {
			talloc_free(msg);
			return -1;
		}

		for (j=0;j<el->num_values;j++) {
			list->dn[list->count] = 
				talloc_strdup(list->dn, (char *)el->values[j].data);
			if (!list->dn[list->count]) {
				talloc_free(msg);
				return -1;
			}
			list->count++;
		}
	}

	talloc_free(msg);

	if (list->count > 1) {
		qsort(list->dn, list->count, sizeof(char *), (comparison_fn_t) list_cmp);
	}

	return 1;
}


static int list_union(struct ldb_context *, struct dn_list *, const struct dn_list *);

/*
  return a list of dn's that might match a simple indexed search on
  the special objectclass attribute
 */
static int ltdb_index_dn_objectclass(struct ldb_module *module, 
				     const struct ldb_parse_tree *tree,
				     const struct ldb_message *index_list,
				     struct dn_list *list)
{
	struct ldb_context *ldb = module->ldb;
	unsigned int i;
	int ret;
	const char *target = (const char *)tree->u.equality.value.data;
	const char **subclasses;

	list->count = 0;
	list->dn = NULL;

	ret = ltdb_index_dn_simple(module, tree, index_list, list);

	subclasses = ldb_subclass_list(module->ldb, target);

	if (subclasses == NULL) {
		return ret;
	}

	for (i=0;subclasses[i];i++) {
		struct ldb_parse_tree tree2;
		struct dn_list *list2;
		tree2.operation = LDB_OP_EQUALITY;
		tree2.u.equality.attr = LTDB_OBJECTCLASS;
		if (!tree2.u.equality.attr) {
			return -1;
		}
		tree2.u.equality.value.data = 
			(uint8_t *)talloc_strdup(list, subclasses[i]);
		if (tree2.u.equality.value.data == NULL) {
			return -1;			
		}
		tree2.u.equality.value.length = strlen(subclasses[i]);
		list2 = talloc(list, struct dn_list);
		if (list2 == NULL) {
			talloc_free(tree2.u.equality.value.data);
			return -1;
		}
		if (ltdb_index_dn_objectclass(module, &tree2, 
					      index_list, list2) == 1) {
			if (list->count == 0) {
				*list = *list2;
				ret = 1;
			} else {
				list_union(ldb, list, list2);
				talloc_free(list2);
			}
		}
		talloc_free(tree2.u.equality.value.data);
	}

	return ret;
}

/*
  return a list of dn's that might match a leaf indexed search
 */
static int ltdb_index_dn_leaf(struct ldb_module *module, 
			      const struct ldb_parse_tree *tree,
			      const struct ldb_message *index_list,
			      struct dn_list *list)
{
	if (ldb_attr_cmp(tree->u.equality.attr, LTDB_OBJECTCLASS) == 0) {
		return ltdb_index_dn_objectclass(module, tree, index_list, list);
	}
	if (ldb_attr_dn(tree->u.equality.attr) == 0) {
		list->dn = talloc_array(list, char *, 1);
		if (list->dn == NULL) {
			ldb_oom(module->ldb);
			return -1;
		}
		list->dn[0] = talloc_strdup(list->dn, (char *)tree->u.equality.value.data);
		if (list->dn[0] == NULL) {
			ldb_oom(module->ldb);
			return -1;
		}
		list->count = 1;
		return 1;
	}
	return ltdb_index_dn_simple(module, tree, index_list, list);
}


/*
  list intersection
  list = list & list2
  relies on the lists being sorted
*/
static int list_intersect(struct ldb_context *ldb,
			  struct dn_list *list, const struct dn_list *list2)
{
	struct dn_list *list3;
	unsigned int i;

	if (list->count == 0 || list2->count == 0) {
		/* 0 & X == 0 */
		return 0;
	}

	list3 = talloc(ldb, struct dn_list);
	if (list3 == NULL) {
		return -1;
	}

	list3->dn = talloc_array(list3, char *, list->count);
	if (!list3->dn) {
		talloc_free(list3);
		return -1;
	}
	list3->count = 0;

	for (i=0;i<list->count;i++) {
		if (ldb_list_find(list->dn[i], list2->dn, list2->count, 
			      sizeof(char *), (comparison_fn_t)strcmp) != -1) {
			list3->dn[list3->count] = talloc_move(list3->dn, &list->dn[i]);
			list3->count++;
		} else {
			talloc_free(list->dn[i]);
		}		
	}

	talloc_free(list->dn);
	list->dn = talloc_move(list, &list3->dn);
	list->count = list3->count;
	talloc_free(list3);

	return 0;
}


/*
  list union
  list = list | list2
  relies on the lists being sorted
*/
static int list_union(struct ldb_context *ldb, 
		      struct dn_list *list, const struct dn_list *list2)
{
	unsigned int i;
	char **d;
	unsigned int count = list->count;

	if (list->count == 0 && list2->count == 0) {
		/* 0 | 0 == 0 */
		return 0;
	}

	d = talloc_realloc(list, list->dn, char *, list->count + list2->count);
	if (!d) {
		return -1;
	}
	list->dn = d;

	for (i=0;i<list2->count;i++) {
		if (ldb_list_find(list2->dn[i], list->dn, count, 
			      sizeof(char *), (comparison_fn_t)strcmp) == -1) {
			list->dn[list->count] = talloc_strdup(list->dn, list2->dn[i]);
			if (!list->dn[list->count]) {
				return -1;
			}
			list->count++;
		}		
	}

	if (list->count != count) {
		qsort(list->dn, list->count, sizeof(char *), (comparison_fn_t)list_cmp);
	}

	return 0;
}

static int ltdb_index_dn(struct ldb_module *module, 
			 const struct ldb_parse_tree *tree,
			 const struct ldb_message *index_list,
			 struct dn_list *list);


/*
  OR two index results
 */
static int ltdb_index_dn_or(struct ldb_module *module, 
			    const struct ldb_parse_tree *tree,
			    const struct ldb_message *index_list,
			    struct dn_list *list)
{
	struct ldb_context *ldb = module->ldb;
	unsigned int i;
	int ret;
	
	ret = -1;
	list->dn = NULL;
	list->count = 0;

	for (i=0;i<tree->u.list.num_elements;i++) {
		struct dn_list *list2;
		int v;

		list2 = talloc(module, struct dn_list);
		if (list2 == NULL) {
			return -1;
		}

		v = ltdb_index_dn(module, tree->u.list.elements[i], index_list, list2);

		if (v == 0) {
			/* 0 || X == X */
			if (ret == -1) {
				ret = 0;
			}
			talloc_free(list2);
			continue;
		}

		if (v == -1) {
			/* 1 || X == 1 */
			talloc_free(list->dn);
			talloc_free(list2);
			return -1;
		}

		if (ret == -1) {
			ret = 1;
			list->dn = talloc_move(list, &list2->dn);
			list->count = list2->count;
		} else {
			if (list_union(ldb, list, list2) == -1) {
				talloc_free(list2);
				return -1;
			}
			ret = 1;
		}
		talloc_free(list2);
	}

	if (list->count == 0) {
		return 0;
	}

	return ret;
}


/*
  NOT an index results
 */
static int ltdb_index_dn_not(struct ldb_module *module, 
			     const struct ldb_parse_tree *tree,
			     const struct ldb_message *index_list,
			     struct dn_list *list)
{
	/* the only way to do an indexed not would be if we could
	   negate the not via another not or if we knew the total
	   number of database elements so we could know that the
	   existing expression covered the whole database. 
	   
	   instead, we just give up, and rely on a full index scan
	   (unless an outer & manages to reduce the list)
	*/
	return -1;
}

/*
  AND two index results
 */
static int ltdb_index_dn_and(struct ldb_module *module, 
			     const struct ldb_parse_tree *tree,
			     const struct ldb_message *index_list,
			     struct dn_list *list)
{
	struct ldb_context *ldb = module->ldb;
	unsigned int i;
	int ret;
	
	ret = -1;
	list->dn = NULL;
	list->count = 0;

	for (i=0;i<tree->u.list.num_elements;i++) {
		struct dn_list *list2;
		int v;

		list2 = talloc(module, struct dn_list);
		if (list2 == NULL) {
			return -1;
		}

		v = ltdb_index_dn(module, tree->u.list.elements[i], index_list, list2);

		if (v == 0) {
			/* 0 && X == 0 */
			talloc_free(list->dn);
			talloc_free(list2);
			return 0;
		}

		if (v == -1) {
			talloc_free(list2);
			continue;
		}

		if (ret == -1) {
			ret = 1;
			talloc_free(list->dn);
			list->dn = talloc_move(list, &list2->dn);
			list->count = list2->count;
		} else {
			if (list_intersect(ldb, list, list2) == -1) {
				talloc_free(list2);
				return -1;
			}
		}

		talloc_free(list2);

		if (list->count == 0) {
			talloc_free(list->dn);
			return 0;
		}
	}

	return ret;
}

/*
  return a list of dn's that might match a indexed search or
  -1 if an error. return 0 for no matches, or 1 for matches
 */
static int ltdb_index_dn(struct ldb_module *module, 
			 const struct ldb_parse_tree *tree,
			 const struct ldb_message *index_list,
			 struct dn_list *list)
{
	int ret = -1;

	switch (tree->operation) {
	case LDB_OP_AND:
		ret = ltdb_index_dn_and(module, tree, index_list, list);
		break;

	case LDB_OP_OR:
		ret = ltdb_index_dn_or(module, tree, index_list, list);
		break;

	case LDB_OP_NOT:
		ret = ltdb_index_dn_not(module, tree, index_list, list);
		break;

	case LDB_OP_EQUALITY:
		ret = ltdb_index_dn_leaf(module, tree, index_list, list);
		break;

	case LDB_OP_SUBSTRING:
	case LDB_OP_GREATER:
	case LDB_OP_LESS:
	case LDB_OP_PRESENT:
	case LDB_OP_APPROX:
	case LDB_OP_EXTENDED:
		/* we can't index with fancy bitops yet */
		ret = -1;
		break;
	}

	return ret;
}

/*
  filter a candidate dn_list from an indexed search into a set of results
  extracting just the given attributes
*/
static int ltdb_index_filter(const struct dn_list *dn_list, 
			     struct ldb_handle *handle)
{
	struct ltdb_context *ac = talloc_get_type(handle->private_data, struct ltdb_context);
	struct ldb_reply *ares = NULL;
	unsigned int i;

	if (!ac) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i = 0; i < dn_list->count; i++) {
		struct ldb_dn *dn;
		int ret;

		ares = talloc_zero(ac, struct ldb_reply);
		if (!ares) {
			handle->status = LDB_ERR_OPERATIONS_ERROR;
			handle->state = LDB_ASYNC_DONE;
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ares->message = ldb_msg_new(ares);
		if (!ares->message) {
			handle->status = LDB_ERR_OPERATIONS_ERROR;
			handle->state = LDB_ASYNC_DONE;
			talloc_free(ares);
			return LDB_ERR_OPERATIONS_ERROR;
		}


		dn = ldb_dn_explode(ares->message, dn_list->dn[i]);
		if (dn == NULL) {
			talloc_free(ares);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ltdb_search_dn1(ac->module, dn, ares->message);
		talloc_free(dn);
		if (ret == 0) {
			/* the record has disappeared? yes, this can happen */
			talloc_free(ares);
			continue;
		}

		if (ret == -1) {
			/* an internal error */
			talloc_free(ares);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		if (!ldb_match_msg(ac->module->ldb, ares->message, ac->tree, ac->base, ac->scope)) {
			talloc_free(ares);
			continue;
		}

		/* filter the attributes that the user wants */
		ret = ltdb_filter_attrs(ares->message, ac->attrs);

		if (ret == -1) {
			handle->status = LDB_ERR_OPERATIONS_ERROR;
			handle->state = LDB_ASYNC_DONE;
			talloc_free(ares);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ares->type = LDB_REPLY_ENTRY;
        	handle->state = LDB_ASYNC_PENDING;
		handle->status = ac->callback(ac->module->ldb, ac->context, ares);

		if (handle->status != LDB_SUCCESS) {
			handle->state = LDB_ASYNC_DONE;
			return handle->status;
		}
	}

	return LDB_SUCCESS;
}

/*
  search the database with a LDAP-like expression using indexes
  returns -1 if an indexed search is not possible, in which
  case the caller should call ltdb_search_full() 
*/
int ltdb_search_indexed(struct ldb_handle *handle)
{
	struct ltdb_context *ac;
	struct ltdb_private *ltdb;
	struct dn_list *dn_list;
	int ret;

	if (!(ac = talloc_get_type(handle->private_data,
				   struct ltdb_context)) ||
	    !(ltdb = talloc_get_type(ac->module->private_data,
				     struct ltdb_private))) {
		return -1;
	}

	if (ltdb->cache->indexlist->num_elements == 0 && 
	    ac->scope != LDB_SCOPE_BASE) {
		/* no index list? must do full search */
		return -1;
	}

	dn_list = talloc(handle, struct dn_list);
	if (dn_list == NULL) {
		return -1;
	}

	if (ac->scope == LDB_SCOPE_BASE) {
		/* with BASE searches only one DN can match */
		dn_list->dn = talloc_array(dn_list, char *, 1);
		if (dn_list->dn == NULL) {
			ldb_oom(ac->module->ldb);
			return -1;
		}
		dn_list->dn[0] = ldb_dn_linearize(dn_list, ac->base);
		if (dn_list->dn[0] == NULL) {
			ldb_oom(ac->module->ldb);
			return -1;
		}
		dn_list->count = 1;
		ret = 1;
	} else {
		ret = ltdb_index_dn(ac->module, ac->tree, ltdb->cache->indexlist, dn_list);
	}

	if (ret == 1) {
		/* we've got a candidate list - now filter by the full tree
		   and extract the needed attributes */
		ret = ltdb_index_filter(dn_list, handle);
		handle->status = ret;
		handle->state = LDB_ASYNC_DONE;
	}

	talloc_free(dn_list);

	return ret;
}

/*
  add a index element where this is the first indexed DN for this value
*/
static int ltdb_index_add1_new(struct ldb_context *ldb, 
			       struct ldb_message *msg,
			       struct ldb_message_element *el,
			       const char *dn)
{
	struct ldb_message_element *el2;

	/* add another entry */
	el2 = talloc_realloc(msg, msg->elements, 
			       struct ldb_message_element, msg->num_elements+1);
	if (!el2) {
		return -1;
	}

	msg->elements = el2;
	msg->elements[msg->num_elements].name = talloc_strdup(msg->elements, LTDB_IDX);
	if (!msg->elements[msg->num_elements].name) {
		return -1;
	}
	msg->elements[msg->num_elements].num_values = 0;
	msg->elements[msg->num_elements].values = talloc(msg->elements, struct ldb_val);
	if (!msg->elements[msg->num_elements].values) {
		return -1;
	}
	msg->elements[msg->num_elements].values[0].length = strlen(dn);
	msg->elements[msg->num_elements].values[0].data = discard_const_p(uint8_t, dn);
	msg->elements[msg->num_elements].num_values = 1;
	msg->num_elements++;

	return 0;
}


/*
  add a index element where this is not the first indexed DN for this
  value
*/
static int ltdb_index_add1_add(struct ldb_context *ldb, 
			       struct ldb_message *msg,
			       struct ldb_message_element *el,
			       int idx,
			       const char *dn)
{
	struct ldb_val *v2;
	unsigned int i;

	/* for multi-valued attributes we can end up with repeats */
	for (i=0;i<msg->elements[idx].num_values;i++) {
		if (strcmp(dn, (char *)msg->elements[idx].values[i].data) == 0) {
			return 0;
		}
	}

	v2 = talloc_realloc(msg->elements, msg->elements[idx].values,
			      struct ldb_val, 
			      msg->elements[idx].num_values+1);
	if (!v2) {
		return -1;
	}
	msg->elements[idx].values = v2;

	msg->elements[idx].values[msg->elements[idx].num_values].length = strlen(dn);
	msg->elements[idx].values[msg->elements[idx].num_values].data = discard_const_p(uint8_t, dn);
	msg->elements[idx].num_values++;

	return 0;
}

/*
  add an index entry for one message element
*/
static int ltdb_index_add1(struct ldb_module *module, const char *dn, 
			   struct ldb_message_element *el, int v_idx)
{
	struct ldb_context *ldb = module->ldb;
	struct ldb_message *msg;
	struct ldb_dn *dn_key;
	int ret;
	unsigned int i;

	msg = talloc(module, struct ldb_message);
	if (msg == NULL) {
		errno = ENOMEM;
		return -1;
	}

	dn_key = ldb_dn_key(ldb, el->name, &el->values[v_idx]);
	if (!dn_key) {
		talloc_free(msg);
		errno = ENOMEM;
		return -1;
	}
	talloc_steal(msg, dn_key);

	ret = ltdb_search_dn1(module, dn_key, msg);
	if (ret == -1) {
		talloc_free(msg);
		return -1;
	}

	if (ret == 0) {
		msg->dn = dn_key;
		msg->num_elements = 0;
		msg->elements = NULL;
	}

	for (i=0;i<msg->num_elements;i++) {
		if (strcmp(LTDB_IDX, msg->elements[i].name) == 0) {
			break;
		}
	}

	if (i == msg->num_elements) {
		ret = ltdb_index_add1_new(ldb, msg, el, dn);
	} else {
		ret = ltdb_index_add1_add(ldb, msg, el, i, dn);
	}

	if (ret == 0) {
		ret = ltdb_store(module, msg, TDB_REPLACE);
	}

	talloc_free(msg);

	return ret;
}

static int ltdb_index_add0(struct ldb_module *module, const char *dn,
			   struct ldb_message_element *elements, int num_el)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	int ret;
	unsigned int i, j;

	if (dn[0] == '@') {
		return 0;
	}

	if (ltdb->cache->indexlist->num_elements == 0) {
		/* no indexed fields */
		return 0;
	}

	for (i = 0; i < num_el; i++) {
		ret = ldb_msg_find_idx(ltdb->cache->indexlist, elements[i].name, 
				       NULL, LTDB_IDXATTR);
		if (ret == -1) {
			continue;
		}
		for (j = 0; j < elements[i].num_values; j++) {
			ret = ltdb_index_add1(module, dn, &elements[i], j);
			if (ret == -1) {
				return -1;
			}
		}
	}

	return 0;
}

/*
  add the index entries for a new record
  return -1 on failure
*/
int ltdb_index_add(struct ldb_module *module, const struct ldb_message *msg)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	char *dn;
	int ret;

	dn = ldb_dn_linearize(ltdb, msg->dn);
	if (dn == NULL) {
		return -1;
	}

	ret = ltdb_index_add0(module, dn, msg->elements, msg->num_elements);

	talloc_free(dn);

	return ret;
}


/*
  delete an index entry for one message element
*/
int ltdb_index_del_value(struct ldb_module *module, const char *dn, 
			 struct ldb_message_element *el, int v_idx)
{
	struct ldb_context *ldb = module->ldb;
	struct ldb_message *msg;
	struct ldb_dn *dn_key;
	int ret, i;
	unsigned int j;

	if (dn[0] == '@') {
		return 0;
	}

	dn_key = ldb_dn_key(ldb, el->name, &el->values[v_idx]);
	if (!dn_key) {
		return -1;
	}

	msg = talloc(dn_key, struct ldb_message);
	if (msg == NULL) {
		talloc_free(dn_key);
		return -1;
	}

	ret = ltdb_search_dn1(module, dn_key, msg);
	if (ret == -1) {
		talloc_free(dn_key);
		return -1;
	}

	if (ret == 0) {
		/* it wasn't indexed. Did we have an earlier error? If we did then
		   its gone now */
		talloc_free(dn_key);
		return 0;
	}

	i = ldb_msg_find_idx(msg, dn, &j, LTDB_IDX);
	if (i == -1) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
				"ERROR: dn %s not found in %s\n", dn,
				ldb_dn_linearize(dn_key, dn_key));
		/* it ain't there. hmmm */
		talloc_free(dn_key);
		return 0;
	}

	if (j != msg->elements[i].num_values - 1) {
		memmove(&msg->elements[i].values[j], 
			&msg->elements[i].values[j+1], 
			(msg->elements[i].num_values-(j+1)) * 
			sizeof(msg->elements[i].values[0]));
	}
	msg->elements[i].num_values--;

	if (msg->elements[i].num_values == 0) {
		ret = ltdb_delete_noindex(module, dn_key);
	} else {
		ret = ltdb_store(module, msg, TDB_REPLACE);
	}

	talloc_free(dn_key);

	return ret;
}

/*
  delete the index entries for a record
  return -1 on failure
*/
int ltdb_index_del(struct ldb_module *module, const struct ldb_message *msg)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	int ret;
	char *dn;
	unsigned int i, j;

	/* find the list of indexed fields */	
	if (ltdb->cache->indexlist->num_elements == 0) {
		/* no indexed fields */
		return 0;
	}

	if (ldb_dn_is_special(msg->dn)) {
		return 0;
	}

	dn = ldb_dn_linearize(ltdb, msg->dn);
	if (dn == NULL) {
		return -1;
	}

	for (i = 0; i < msg->num_elements; i++) {
		ret = ldb_msg_find_idx(ltdb->cache->indexlist, msg->elements[i].name, 
				       NULL, LTDB_IDXATTR);
		if (ret == -1) {
			continue;
		}
		for (j = 0; j < msg->elements[i].num_values; j++) {
			ret = ltdb_index_del_value(module, dn, &msg->elements[i], j);
			if (ret == -1) {
				talloc_free(dn);
				return -1;
			}
		}
	}

	talloc_free(dn);
	return 0;
}


/*
  traversal function that deletes all @INDEX records
*/
static int delete_index(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data, void *state)
{
	const char *dn = "DN=" LTDB_INDEX ":";
	if (strncmp((char *)key.dptr, dn, strlen(dn)) == 0) {
		return tdb_delete(tdb, key);
	}
	return 0;
}

/*
  traversal function that adds @INDEX records during a re index
*/
static int re_index(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data, void *state)
{
	struct ldb_module *module = (struct ldb_module *)state;
	struct ldb_message *msg;
	char *dn = NULL;
	int ret;
	TDB_DATA key2;

	if (strncmp((char *)key.dptr, "DN=@", 4) == 0 ||
	    strncmp((char *)key.dptr, "DN=", 3) != 0) {
		return 0;
	}

	msg = talloc(module, struct ldb_message);
	if (msg == NULL) {
		return -1;
	}

	ret = ltdb_unpack_data(module, &data, msg);
	if (ret != 0) {
		talloc_free(msg);
		return -1;
	}

	/* check if the DN key has changed, perhaps due to the 
	   case insensitivity of an element changing */
	key2 = ltdb_key(module, msg->dn);
	if (key2.dptr == NULL) {
		/* probably a corrupt record ... darn */
		ldb_debug(module->ldb, LDB_DEBUG_ERROR, "Invalid DN in re_index: %s\n",
							ldb_dn_linearize(msg, msg->dn));
		talloc_free(msg);
		return 0;
	}
	if (strcmp((char *)key2.dptr, (char *)key.dptr) != 0) {
		tdb_delete(tdb, key);
		tdb_store(tdb, key2, data, 0);
	}
	talloc_free(key2.dptr);

	if (msg->dn == NULL) {
		dn = (char *)key.dptr + 3;
	} else {
		if (!(dn = ldb_dn_linearize(msg->dn, msg->dn))) {
			talloc_free(msg);
			return -1;
		}
	}

	ret = ltdb_index_add0(module, dn, msg->elements, msg->num_elements);

	talloc_free(msg);

	return ret;
}

/*
  force a complete reindex of the database
*/
int ltdb_reindex(struct ldb_module *module)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	int ret;

	if (ltdb_cache_reload(module) != 0) {
		return -1;
	}

	/* first traverse the database deleting any @INDEX records */
	ret = tdb_traverse(ltdb->tdb, delete_index, NULL);
	if (ret == -1) {
		return -1;
	}

	/* now traverse adding any indexes for normal LDB records */
	ret = tdb_traverse(ltdb->tdb, re_index, module);
	if (ret == -1) {
		return -1;
	}

	return 0;
}
