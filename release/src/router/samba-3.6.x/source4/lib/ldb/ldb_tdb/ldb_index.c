/*
   ldb database library

   Copyright (C) Andrew Tridgell  2004-2009

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

#include "ldb_tdb.h"

struct dn_list {
	unsigned int count;
	struct ldb_val *dn;
};

struct ltdb_idxptr {
	struct tdb_context *itdb;
	int error;
};

/* we put a @IDXVERSION attribute on index entries. This
   allows us to tell if it was written by an older version
*/
#define LTDB_INDEXING_VERSION 2

/* enable the idxptr mode when transactions start */
int ltdb_index_transaction_start(struct ldb_module *module)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	ltdb->idxptr = talloc_zero(ltdb, struct ltdb_idxptr);
	return LDB_SUCCESS;
}

/* compare two DN entries in a dn_list. Take account of possible
 * differences in string termination */
static int dn_list_cmp(const struct ldb_val *v1, const struct ldb_val *v2)
{
	if (v1->length > v2->length && v1->data[v2->length] != 0) {
		return -1;
	}
	if (v1->length < v2->length && v2->data[v1->length] != 0) {
		return 1;
	}
	return strncmp((char *)v1->data, (char *)v2->data, v1->length);
}


/*
  find a entry in a dn_list, using a ldb_val. Uses a case sensitive
  comparison with the dn returns -1 if not found
 */
static int ltdb_dn_list_find_val(const struct dn_list *list, const struct ldb_val *v)
{
	unsigned int i;
	for (i=0; i<list->count; i++) {
		if (dn_list_cmp(&list->dn[i], v) == 0) return i;
	}
	return -1;
}

/*
  find a entry in a dn_list. Uses a case sensitive comparison with the dn
  returns -1 if not found
 */
static int ltdb_dn_list_find_str(struct dn_list *list, const char *dn)
{
	struct ldb_val v;
	v.data = discard_const_p(unsigned char, dn);
	v.length = strlen(dn);
	return ltdb_dn_list_find_val(list, &v);
}

/*
  this is effectively a cast function, but with lots of paranoia
  checks and also copes with CPUs that are fussy about pointer
  alignment
 */
static struct dn_list *ltdb_index_idxptr(struct ldb_module *module, TDB_DATA rec, bool check_parent)
{
	struct dn_list *list;
	if (rec.dsize != sizeof(void *)) {
		ldb_asprintf_errstring(ldb_module_get_ctx(module), 
				       "Bad data size for idxptr %u", (unsigned)rec.dsize);
		return NULL;
	}
	/* note that we can't just use a cast here, as rec.dptr may
	   not be aligned sufficiently for a pointer. A cast would cause
	   platforms like some ARM CPUs to crash */
	memcpy(&list, rec.dptr, sizeof(void *));
	list = talloc_get_type(list, struct dn_list);
	if (list == NULL) {
		ldb_asprintf_errstring(ldb_module_get_ctx(module), 
				       "Bad type '%s' for idxptr", 
				       talloc_get_name(list));
		return NULL;
	}
	if (check_parent && list->dn && talloc_parent(list->dn) != list) {
		ldb_asprintf_errstring(ldb_module_get_ctx(module), 
				       "Bad parent '%s' for idxptr", 
				       talloc_get_name(talloc_parent(list->dn)));
		return NULL;
	}
	return list;
}

/*
  return the @IDX list in an index entry for a dn as a 
  struct dn_list
 */
static int ltdb_dn_list_load(struct ldb_module *module,
			     struct ldb_dn *dn, struct dn_list *list)
{
	struct ldb_message *msg;
	int ret;
	struct ldb_message_element *el;
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	TDB_DATA rec;
	struct dn_list *list2;
	TDB_DATA key;

	list->dn = NULL;
	list->count = 0;

	/* see if we have any in-memory index entries */
	if (ltdb->idxptr == NULL ||
	    ltdb->idxptr->itdb == NULL) {
		goto normal_index;
	}

	key.dptr = discard_const_p(unsigned char, ldb_dn_get_linearized(dn));
	key.dsize = strlen((char *)key.dptr);

	rec = tdb_fetch(ltdb->idxptr->itdb, key);
	if (rec.dptr == NULL) {
		goto normal_index;
	}

	/* we've found an in-memory index entry */
	list2 = ltdb_index_idxptr(module, rec, true);
	if (list2 == NULL) {
		free(rec.dptr);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	free(rec.dptr);

	*list = *list2;
	return LDB_SUCCESS;

normal_index:
	msg = ldb_msg_new(list);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ltdb_search_dn1(module, dn, msg);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return ret;
	}

	/* TODO: check indexing version number */

	el = ldb_msg_find_element(msg, LTDB_IDX);
	if (!el) {
		talloc_free(msg);
		return LDB_SUCCESS;
	}

	/* we avoid copying the strings by stealing the list */
	list->dn = talloc_steal(list, el->values);
	list->count = el->num_values;

	return LDB_SUCCESS;
}


/*
  save a dn_list into a full @IDX style record
 */
static int ltdb_dn_list_store_full(struct ldb_module *module, struct ldb_dn *dn, 
				   struct dn_list *list)
{
	struct ldb_message *msg;
	int ret;

	if (list->count == 0) {
		ret = ltdb_delete_noindex(module, dn);
		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			return LDB_SUCCESS;
		}
		return ret;
	}

	msg = ldb_msg_new(module);
	if (!msg) {
		return ldb_module_oom(module);
	}

	ret = ldb_msg_add_fmt(msg, LTDB_IDXVERSION, "%u", LTDB_INDEXING_VERSION);
	if (ret != LDB_SUCCESS) {
		talloc_free(msg);
		return ldb_module_oom(module);
	}

	msg->dn = dn;
	if (list->count > 0) {
		struct ldb_message_element *el;

		ret = ldb_msg_add_empty(msg, LTDB_IDX, LDB_FLAG_MOD_ADD, &el);
		if (ret != LDB_SUCCESS) {
			talloc_free(msg);
			return ldb_module_oom(module);
		}
		el->values = list->dn;
		el->num_values = list->count;
	}

	ret = ltdb_store(module, msg, TDB_REPLACE);
	talloc_free(msg);
	return ret;
}

/*
  save a dn_list into the database, in either @IDX or internal format
 */
static int ltdb_dn_list_store(struct ldb_module *module, struct ldb_dn *dn, 
			      struct dn_list *list)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	TDB_DATA rec, key;
	int ret;
	struct dn_list *list2;

	if (ltdb->idxptr == NULL) {
		return ltdb_dn_list_store_full(module, dn, list);
	}

	if (ltdb->idxptr->itdb == NULL) {
		ltdb->idxptr->itdb = tdb_open(NULL, 1000, TDB_INTERNAL, O_RDWR, 0);
		if (ltdb->idxptr->itdb == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	key.dptr = discard_const_p(unsigned char, ldb_dn_get_linearized(dn));
	key.dsize = strlen((char *)key.dptr);

	rec = tdb_fetch(ltdb->idxptr->itdb, key);
	if (rec.dptr != NULL) {
		list2 = ltdb_index_idxptr(module, rec, false);
		if (list2 == NULL) {
			free(rec.dptr);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		free(rec.dptr);
		list2->dn = talloc_steal(list2, list->dn);
		list2->count = list->count;
		return LDB_SUCCESS;
	}

	list2 = talloc(ltdb->idxptr, struct dn_list);
	if (list2 == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	list2->dn = talloc_steal(list2, list->dn);
	list2->count = list->count;

	rec.dptr = (uint8_t *)&list2;
	rec.dsize = sizeof(void *);

	ret = tdb_store(ltdb->idxptr->itdb, key, rec, TDB_INSERT);
	if (ret == -1) {
		return ltdb_err_map(tdb_error(ltdb->idxptr->itdb));
	}
	return LDB_SUCCESS;
}

/*
  traverse function for storing the in-memory index entries on disk
 */
static int ltdb_index_traverse_store(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data, void *state)
{
	struct ldb_module *module = state;
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	struct ldb_dn *dn;
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	struct ldb_val v;
	struct dn_list *list;

	list = ltdb_index_idxptr(module, data, true);
	if (list == NULL) {
		ltdb->idxptr->error = LDB_ERR_OPERATIONS_ERROR;
		return -1;
	}

	v.data = key.dptr;
	v.length = strnlen((char *)key.dptr, key.dsize);

	dn = ldb_dn_from_ldb_val(module, ldb, &v);
	if (dn == NULL) {
		ldb_asprintf_errstring(ldb, "Failed to parse index key %*.*s as an LDB DN", (int)v.length, (int)v.length, (const char *)v.data);
		ltdb->idxptr->error = LDB_ERR_OPERATIONS_ERROR;
		return -1;
	}

	ltdb->idxptr->error = ltdb_dn_list_store_full(module, dn, list);
	talloc_free(dn);
	if (ltdb->idxptr->error != 0) {
		return -1;
	}
	return 0;
}

/* cleanup the idxptr mode when transaction commits */
int ltdb_index_transaction_commit(struct ldb_module *module)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	int ret;

	struct ldb_context *ldb = ldb_module_get_ctx(module);

	ldb_reset_err_string(ldb);

	if (ltdb->idxptr->itdb) {
		tdb_traverse(ltdb->idxptr->itdb, ltdb_index_traverse_store, module);
		tdb_close(ltdb->idxptr->itdb);
	}

	ret = ltdb->idxptr->error;
	if (ret != LDB_SUCCESS) {
		if (!ldb_errstring(ldb)) {
			ldb_set_errstring(ldb, ldb_strerror(ret));
		}
		ldb_asprintf_errstring(ldb, "Failed to store index records in transaction commit: %s", ldb_errstring(ldb));
	}

	talloc_free(ltdb->idxptr);
	ltdb->idxptr = NULL;
	return ret;
}

/* cleanup the idxptr mode when transaction cancels */
int ltdb_index_transaction_cancel(struct ldb_module *module)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	if (ltdb->idxptr && ltdb->idxptr->itdb) {
		tdb_close(ltdb->idxptr->itdb);
	}
	talloc_free(ltdb->idxptr);
	ltdb->idxptr = NULL;
	return LDB_SUCCESS;
}


/*
  return the dn key to be used for an index
  the caller is responsible for freeing
*/
static struct ldb_dn *ltdb_index_key(struct ldb_context *ldb,
				     const char *attr, const struct ldb_val *value,
				     const struct ldb_schema_attribute **ap)
{
	struct ldb_dn *ret;
	struct ldb_val v;
	const struct ldb_schema_attribute *a;
	char *attr_folded;
	int r;

	attr_folded = ldb_attr_casefold(ldb, attr);
	if (!attr_folded) {
		return NULL;
	}

	a = ldb_schema_attribute_by_name(ldb, attr);
	if (ap) {
		*ap = a;
	}
	r = a->syntax->canonicalise_fn(ldb, ldb, value, &v);
	if (r != LDB_SUCCESS) {
		const char *errstr = ldb_errstring(ldb);
		/* canonicalisation can be refused. For example, 
		   a attribute that takes wildcards will refuse to canonicalise
		   if the value contains a wildcard */
		ldb_asprintf_errstring(ldb, "Failed to create index key for attribute '%s':%s%s%s",
				       attr, ldb_strerror(r), (errstr?":":""), (errstr?errstr:""));
		talloc_free(attr_folded);
		return NULL;
	}
	if (ldb_should_b64_encode(ldb, &v)) {
		char *vstr = ldb_base64_encode(ldb, (char *)v.data, v.length);
		if (!vstr) {
			talloc_free(attr_folded);
			return NULL;
		}
		ret = ldb_dn_new_fmt(ldb, ldb, "%s:%s::%s", LTDB_INDEX, attr_folded, vstr);
		talloc_free(vstr);
	} else {
		ret = ldb_dn_new_fmt(ldb, ldb, "%s:%s:%.*s", LTDB_INDEX, attr_folded, (int)v.length, (char *)v.data);
	}

	if (v.data != value->data) {
		talloc_free(v.data);
	}
	talloc_free(attr_folded);

	return ret;
}

/*
  see if a attribute value is in the list of indexed attributes
*/
static bool ltdb_is_indexed(const struct ldb_message *index_list, const char *attr)
{
	unsigned int i;
	struct ldb_message_element *el;

	el = ldb_msg_find_element(index_list, LTDB_IDXATTR);
	if (el == NULL) {
		return false;
	}

	/* TODO: this is too expensive! At least use a binary search */
	for (i=0; i<el->num_values; i++) {
		if (ldb_attr_cmp((char *)el->values[i].data, attr) == 0) {
			return true;
		}
	}
	return false;
}

/*
  in the following logic functions, the return value is treated as
  follows:

     LDB_SUCCESS: we found some matching index values

     LDB_ERR_NO_SUCH_OBJECT: we know for sure that no object matches

     LDB_ERR_OPERATIONS_ERROR: indexing could not answer the call,
                               we'll need a full search
 */

/*
  return a list of dn's that might match a simple indexed search (an
  equality search only)
 */
static int ltdb_index_dn_simple(struct ldb_module *module,
				const struct ldb_parse_tree *tree,
				const struct ldb_message *index_list,
				struct dn_list *list)
{
	struct ldb_context *ldb;
	struct ldb_dn *dn;
	int ret;

	ldb = ldb_module_get_ctx(module);

	list->count = 0;
	list->dn = NULL;

	/* if the attribute isn't in the list of indexed attributes then
	   this node needs a full search */
	if (!ltdb_is_indexed(index_list, tree->u.equality.attr)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* the attribute is indexed. Pull the list of DNs that match the 
	   search criterion */
	dn = ltdb_index_key(ldb, tree->u.equality.attr, &tree->u.equality.value, NULL);
	if (!dn) return LDB_ERR_OPERATIONS_ERROR;

	ret = ltdb_dn_list_load(module, dn, list);
	talloc_free(dn);
	return ret;
}


static bool list_union(struct ldb_context *, struct dn_list *, const struct dn_list *);

/*
  return a list of dn's that might match a leaf indexed search
 */
static int ltdb_index_dn_leaf(struct ldb_module *module,
			      const struct ldb_parse_tree *tree,
			      const struct ldb_message *index_list,
			      struct dn_list *list)
{
	if (ldb_attr_dn(tree->u.equality.attr) == 0) {
		list->dn = talloc_array(list, struct ldb_val, 1);
		if (list->dn == NULL) {
			ldb_module_oom(module);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		list->dn[0] = tree->u.equality.value;
		list->count = 1;
		return LDB_SUCCESS;
	}
	return ltdb_index_dn_simple(module, tree, index_list, list);
}


/*
  list intersection
  list = list & list2
*/
static bool list_intersect(struct ldb_context *ldb,
			   struct dn_list *list, const struct dn_list *list2)
{
	struct dn_list *list3;
	unsigned int i;

	if (list->count == 0) {
		/* 0 & X == 0 */
		return true;
	}
	if (list2->count == 0) {
		/* X & 0 == 0 */
		list->count = 0;
		list->dn = NULL;
		return true;
	}

	/* the indexing code is allowed to return a longer list than
	   what really matches, as all results are filtered by the
	   full expression at the end - this shortcut avoids a lot of
	   work in some cases */
	if (list->count < 2 && list2->count > 10) {
		return true;
	}
	if (list2->count < 2 && list->count > 10) {
		list->count = list2->count;
		list->dn = list2->dn;
		/* note that list2 may not be the parent of list2->dn,
		   as list2->dn may be owned by ltdb->idxptr. In that
		   case we expect this reparent call to fail, which is
		   OK */
		talloc_reparent(list2, list, list2->dn);
		return true;
	}

	list3 = talloc_zero(list, struct dn_list);
	if (list3 == NULL) {
		return false;
	}

	list3->dn = talloc_array(list3, struct ldb_val, list->count);
	if (!list3->dn) {
		talloc_free(list3);
		return false;
	}
	list3->count = 0;

	for (i=0;i<list->count;i++) {
		if (ltdb_dn_list_find_val(list2, &list->dn[i]) != -1) {
			list3->dn[list3->count] = list->dn[i];
			list3->count++;
		}
	}

	list->dn = talloc_steal(list, list3->dn);
	list->count = list3->count;
	talloc_free(list3);

	return true;
}


/*
  list union
  list = list | list2
*/
static bool list_union(struct ldb_context *ldb,
		       struct dn_list *list, const struct dn_list *list2)
{
	struct ldb_val *dn3;

	if (list2->count == 0) {
		/* X | 0 == X */
		return true;
	}

	if (list->count == 0) {
		/* 0 | X == X */
		list->count = list2->count;
		list->dn = list2->dn;
		/* note that list2 may not be the parent of list2->dn,
		   as list2->dn may be owned by ltdb->idxptr. In that
		   case we expect this reparent call to fail, which is
		   OK */
		talloc_reparent(list2, list, list2->dn);
		return true;
	}

	dn3 = talloc_array(list, struct ldb_val, list->count + list2->count);
	if (!dn3) {
		ldb_oom(ldb);
		return false;
	}

	/* we allow for duplicates here, and get rid of them later */
	memcpy(dn3, list->dn, sizeof(list->dn[0])*list->count);
	memcpy(dn3+list->count, list2->dn, sizeof(list2->dn[0])*list2->count);

	list->dn = dn3;
	list->count += list2->count;

	return true;
}

static int ltdb_index_dn(struct ldb_module *module,
			 const struct ldb_parse_tree *tree,
			 const struct ldb_message *index_list,
			 struct dn_list *list);


/*
  process an OR list (a union)
 */
static int ltdb_index_dn_or(struct ldb_module *module,
			    const struct ldb_parse_tree *tree,
			    const struct ldb_message *index_list,
			    struct dn_list *list)
{
	struct ldb_context *ldb;
	unsigned int i;

	ldb = ldb_module_get_ctx(module);

	list->dn = NULL;
	list->count = 0;

	for (i=0; i<tree->u.list.num_elements; i++) {
		struct dn_list *list2;
		int ret;

		list2 = talloc_zero(list, struct dn_list);
		if (list2 == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ltdb_index_dn(module, tree->u.list.elements[i], index_list, list2);

		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			/* X || 0 == X */
			talloc_free(list2);
			continue;
		}

		if (ret != LDB_SUCCESS) {
			/* X || * == * */
			talloc_free(list2);
			return ret;
		}

		if (!list_union(ldb, list, list2)) {
			talloc_free(list2);
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	if (list->count == 0) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}

	return LDB_SUCCESS;
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
	return LDB_ERR_OPERATIONS_ERROR;
}


static bool ltdb_index_unique(struct ldb_context *ldb,
			      const char *attr)
{
	const struct ldb_schema_attribute *a;
	a = ldb_schema_attribute_by_name(ldb, attr);
	if (a->flags & LDB_ATTR_FLAG_UNIQUE_INDEX) {
		return true;
	}
	return false;
}

/*
  process an AND expression (intersection)
 */
static int ltdb_index_dn_and(struct ldb_module *module,
			     const struct ldb_parse_tree *tree,
			     const struct ldb_message *index_list,
			     struct dn_list *list)
{
	struct ldb_context *ldb;
	unsigned int i;
	bool found;

	ldb = ldb_module_get_ctx(module);

	list->dn = NULL;
	list->count = 0;

	/* in the first pass we only look for unique simple
	   equality tests, in the hope of avoiding having to look
	   at any others */
	for (i=0; i<tree->u.list.num_elements; i++) {
		const struct ldb_parse_tree *subtree = tree->u.list.elements[i];
		int ret;

		if (subtree->operation != LDB_OP_EQUALITY ||
		    !ltdb_index_unique(ldb, subtree->u.equality.attr)) {
			continue;
		}
		
		ret = ltdb_index_dn(module, subtree, index_list, list);
		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			/* 0 && X == 0 */
			return LDB_ERR_NO_SUCH_OBJECT;
		}
		if (ret == LDB_SUCCESS) {
			/* a unique index match means we can
			 * stop. Note that we don't care if we return
			 * a few too many objects, due to later
			 * filtering */
			return LDB_SUCCESS;
		}
	}	

	/* now do a full intersection */
	found = false;

	for (i=0; i<tree->u.list.num_elements; i++) {
		const struct ldb_parse_tree *subtree = tree->u.list.elements[i];
		struct dn_list *list2;
		int ret;

		list2 = talloc_zero(list, struct dn_list);
		if (list2 == NULL) {
			return ldb_module_oom(module);
		}
			
		ret = ltdb_index_dn(module, subtree, index_list, list2);

		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			/* X && 0 == 0 */
			list->dn = NULL;
			list->count = 0;
			talloc_free(list2);
			return LDB_ERR_NO_SUCH_OBJECT;
		}
		
		if (ret != LDB_SUCCESS) {
			/* this didn't adding anything */
			talloc_free(list2);
			continue;
		}

		if (!found) {
			talloc_reparent(list2, list, list->dn);
			list->dn = list2->dn;
			list->count = list2->count;
			found = true;
		} else if (!list_intersect(ldb, list, list2)) {
			talloc_free(list2);
			return LDB_ERR_OPERATIONS_ERROR;
		}
			
		if (list->count == 0) {
			list->dn = NULL;
			return LDB_ERR_NO_SUCH_OBJECT;
		}
			
		if (list->count < 2) {
			/* it isn't worth loading the next part of the tree */
			return LDB_SUCCESS;
		}
	}	

	if (!found) {
		/* none of the attributes were indexed */
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return LDB_SUCCESS;
}
	
/*
  return a list of matching objects using a one-level index
 */
static int ltdb_index_dn_one(struct ldb_module *module,
			     struct ldb_dn *parent_dn,
			     struct dn_list *list)
{
	struct ldb_context *ldb;
	struct ldb_dn *key;
	struct ldb_val val;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* work out the index key from the parent DN */
	val.data = (uint8_t *)((uintptr_t)ldb_dn_get_casefold(parent_dn));
	val.length = strlen((char *)val.data);
	key = ltdb_index_key(ldb, LTDB_IDXONE, &val, NULL);
	if (!key) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ltdb_dn_list_load(module, key, list);
	talloc_free(key);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (list->count == 0) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}

	return LDB_SUCCESS;
}

/*
  return a list of dn's that might match a indexed search or
  an error. return LDB_ERR_NO_SUCH_OBJECT for no matches, or LDB_SUCCESS for matches
 */
static int ltdb_index_dn(struct ldb_module *module,
			 const struct ldb_parse_tree *tree,
			 const struct ldb_message *index_list,
			 struct dn_list *list)
{
	int ret = LDB_ERR_OPERATIONS_ERROR;

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
		ret = LDB_ERR_OPERATIONS_ERROR;
		break;
	}

	return ret;
}

/*
  filter a candidate dn_list from an indexed search into a set of results
  extracting just the given attributes
*/
static int ltdb_index_filter(const struct dn_list *dn_list,
			     struct ltdb_context *ac, 
			     uint32_t *match_count)
{
	struct ldb_context *ldb;
	struct ldb_message *msg;
	unsigned int i;

	ldb = ldb_module_get_ctx(ac->module);

	for (i = 0; i < dn_list->count; i++) {
		struct ldb_dn *dn;
		int ret;
		bool matched;

		msg = ldb_msg_new(ac);
		if (!msg) {
			return LDB_ERR_OPERATIONS_ERROR;
		}

		dn = ldb_dn_from_ldb_val(msg, ldb, &dn_list->dn[i]);
		if (dn == NULL) {
			talloc_free(msg);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ltdb_search_dn1(ac->module, dn, msg);
		talloc_free(dn);
		if (ret == LDB_ERR_NO_SUCH_OBJECT) {
			/* the record has disappeared? yes, this can happen */
			talloc_free(msg);
			continue;
		}

		if (ret != LDB_SUCCESS && ret != LDB_ERR_NO_SUCH_OBJECT) {
			/* an internal error */
			talloc_free(msg);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ldb_match_msg_error(ldb, msg,
					  ac->tree, ac->base, ac->scope, &matched);
		if (ret != LDB_SUCCESS) {
			talloc_free(msg);
			return ret;
		}
		if (!matched) {
			talloc_free(msg);
			continue;
		}

		/* filter the attributes that the user wants */
		ret = ltdb_filter_attrs(msg, ac->attrs);

		if (ret == -1) {
			talloc_free(msg);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		ret = ldb_module_send_entry(ac->req, msg, NULL);
		if (ret != LDB_SUCCESS) {
			/* Regardless of success or failure, the msg
			 * is the callbacks responsiblity, and should
			 * not be talloc_free()'ed */
			ac->request_terminated = true;
			return ret;
		}

		(*match_count)++;
	}

	return LDB_SUCCESS;
}

/*
  remove any duplicated entries in a indexed result
 */
static void ltdb_dn_list_remove_duplicates(struct dn_list *list)
{
	unsigned int i, new_count;

	if (list->count < 2) {
		return;
	}

	TYPESAFE_QSORT(list->dn, list->count, dn_list_cmp);

	new_count = 1;
	for (i=1; i<list->count; i++) {
		if (dn_list_cmp(&list->dn[i], &list->dn[new_count-1]) != 0) {
			if (new_count != i) {
				list->dn[new_count] = list->dn[i];
			}
			new_count++;
		}
	}
	
	list->count = new_count;
}

/*
  search the database with a LDAP-like expression using indexes
  returns -1 if an indexed search is not possible, in which
  case the caller should call ltdb_search_full()
*/
int ltdb_search_indexed(struct ltdb_context *ac, uint32_t *match_count)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(ac->module), struct ltdb_private);
	struct dn_list *dn_list;
	int ret;

	/* see if indexing is enabled */
	if (!ltdb->cache->attribute_indexes && 
	    !ltdb->cache->one_level_indexes &&
	    ac->scope != LDB_SCOPE_BASE) {
		/* fallback to a full search */
		return LDB_ERR_OPERATIONS_ERROR;
	}

	dn_list = talloc_zero(ac, struct dn_list);
	if (dn_list == NULL) {
		return ldb_module_oom(ac->module);
	}

	switch (ac->scope) {
	case LDB_SCOPE_BASE:
		dn_list->dn = talloc_array(dn_list, struct ldb_val, 1);
		if (dn_list->dn == NULL) {
			talloc_free(dn_list);
			return ldb_module_oom(ac->module);
		}
		dn_list->dn[0].data = discard_const_p(unsigned char, ldb_dn_get_linearized(ac->base));
		if (dn_list->dn[0].data == NULL) {
			talloc_free(dn_list);
			return ldb_module_oom(ac->module);
		}
		dn_list->dn[0].length = strlen((char *)dn_list->dn[0].data);
		dn_list->count = 1;
		break;		

	case LDB_SCOPE_ONELEVEL:
		if (!ltdb->cache->one_level_indexes) {
			talloc_free(dn_list);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		ret = ltdb_index_dn_one(ac->module, ac->base, dn_list);
		if (ret != LDB_SUCCESS) {
			talloc_free(dn_list);
			return ret;
		}
		break;

	case LDB_SCOPE_SUBTREE:
	case LDB_SCOPE_DEFAULT:
		if (!ltdb->cache->attribute_indexes) {
			talloc_free(dn_list);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		ret = ltdb_index_dn(ac->module, ac->tree, ltdb->cache->indexlist, dn_list);
		if (ret != LDB_SUCCESS) {
			talloc_free(dn_list);
			return ret;
		}
		ltdb_dn_list_remove_duplicates(dn_list);
		break;
	}

	ret = ltdb_index_filter(dn_list, ac, match_count);
	talloc_free(dn_list);
	return ret;
}

/*
  add an index entry for one message element
*/
static int ltdb_index_add1(struct ldb_module *module, const char *dn,
			   struct ldb_message_element *el, int v_idx)
{
	struct ldb_context *ldb;
	struct ldb_dn *dn_key;
	int ret;
	const struct ldb_schema_attribute *a;
	struct dn_list *list;
	unsigned alloc_len;

	ldb = ldb_module_get_ctx(module);

	list = talloc_zero(module, struct dn_list);
	if (list == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	dn_key = ltdb_index_key(ldb, el->name, &el->values[v_idx], &a);
	if (!dn_key) {
		talloc_free(list);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	talloc_steal(list, dn_key);

	ret = ltdb_dn_list_load(module, dn_key, list);
	if (ret != LDB_SUCCESS && ret != LDB_ERR_NO_SUCH_OBJECT) {
		talloc_free(list);
		return ret;
	}

	if (ltdb_dn_list_find_str(list, dn) != -1) {
		talloc_free(list);
		return LDB_SUCCESS;
	}

	if (list->count > 0 &&
	    a->flags & LDB_ATTR_FLAG_UNIQUE_INDEX) {
		talloc_free(list);
		ldb_asprintf_errstring(ldb, __location__ ": unique index violation on %s in %s",
				       el->name, dn);
		return LDB_ERR_ENTRY_ALREADY_EXISTS;		
	}

	/* overallocate the list a bit, to reduce the number of
	 * realloc trigered copies */	 
	alloc_len = ((list->count+1)+7) & ~7;
	list->dn = talloc_realloc(list, list->dn, struct ldb_val, alloc_len);
	if (list->dn == NULL) {
		talloc_free(list);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	list->dn[list->count].data = (uint8_t *)talloc_strdup(list->dn, dn);
	list->dn[list->count].length = strlen(dn);
	list->count++;

	ret = ltdb_dn_list_store(module, dn_key, list);

	talloc_free(list);

	return ret;
}

/*
  add index entries for one elements in a message
 */
static int ltdb_index_add_el(struct ldb_module *module, const char *dn,
			     struct ldb_message_element *el)
{
	unsigned int i;
	for (i = 0; i < el->num_values; i++) {
		int ret = ltdb_index_add1(module, dn, el, i);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}

/*
  add index entries for all elements in a message
 */
static int ltdb_index_add_all(struct ldb_module *module, const char *dn,
			      struct ldb_message_element *elements, int num_el)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	unsigned int i;

	if (dn[0] == '@') {
		return LDB_SUCCESS;
	}

	if (ltdb->cache->indexlist->num_elements == 0) {
		/* no indexed fields */
		return LDB_SUCCESS;
	}

	for (i = 0; i < num_el; i++) {
		int ret;
		if (!ltdb_is_indexed(ltdb->cache->indexlist, elements[i].name)) {
			continue;
		}
		ret = ltdb_index_add_el(module, dn, &elements[i]);
		if (ret != LDB_SUCCESS) {
			struct ldb_context *ldb = ldb_module_get_ctx(module);
			ldb_asprintf_errstring(ldb,
					       __location__ ": Failed to re-index %s in %s - %s",
					       elements[i].name, dn, ldb_errstring(ldb));
			return ret;
		}
	}

	return LDB_SUCCESS;
}


/*
  insert a one level index for a message
*/
static int ltdb_index_onelevel(struct ldb_module *module, const struct ldb_message *msg, int add)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	struct ldb_message_element el;
	struct ldb_val val;
	struct ldb_dn *pdn;
	const char *dn;
	int ret;

	/* We index for ONE Level only if requested */
	if (!ltdb->cache->one_level_indexes) {
		return LDB_SUCCESS;
	}

	pdn = ldb_dn_get_parent(module, msg->dn);
	if (pdn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	dn = ldb_dn_get_linearized(msg->dn);
	if (dn == NULL) {
		talloc_free(pdn);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	val.data = (uint8_t *)((uintptr_t)ldb_dn_get_casefold(pdn));
	if (val.data == NULL) {
		talloc_free(pdn);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	val.length = strlen((char *)val.data);
	el.name = LTDB_IDXONE;
	el.values = &val;
	el.num_values = 1;

	if (add) {
		ret = ltdb_index_add1(module, dn, &el, 0);
	} else { /* delete */
		ret = ltdb_index_del_value(module, msg->dn, &el, 0);
	}

	talloc_free(pdn);

	return ret;
}

/*
  add the index entries for a new element in a record
  The caller guarantees that these element values are not yet indexed
*/
int ltdb_index_add_element(struct ldb_module *module, struct ldb_dn *dn, 
			   struct ldb_message_element *el)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	if (ldb_dn_is_special(dn)) {
		return LDB_SUCCESS;
	}
	if (!ltdb_is_indexed(ltdb->cache->indexlist, el->name)) {
		return LDB_SUCCESS;
	}
	return ltdb_index_add_el(module, ldb_dn_get_linearized(dn), el);
}

/*
  add the index entries for a new record
*/
int ltdb_index_add_new(struct ldb_module *module, const struct ldb_message *msg)
{
	const char *dn;
	int ret;

	if (ldb_dn_is_special(msg->dn)) {
		return LDB_SUCCESS;
	}

	dn = ldb_dn_get_linearized(msg->dn);
	if (dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ltdb_index_add_all(module, dn, msg->elements, msg->num_elements);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	return ltdb_index_onelevel(module, msg, 1);
}


/*
  delete an index entry for one message element
*/
int ltdb_index_del_value(struct ldb_module *module, struct ldb_dn *dn,
			 struct ldb_message_element *el, unsigned int v_idx)
{
	struct ldb_context *ldb;
	struct ldb_dn *dn_key;
	const char *dn_str;
	int ret, i;
	unsigned int j;
	struct dn_list *list;

	ldb = ldb_module_get_ctx(module);

	dn_str = ldb_dn_get_linearized(dn);
	if (dn_str == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (dn_str[0] == '@') {
		return LDB_SUCCESS;
	}

	dn_key = ltdb_index_key(ldb, el->name, &el->values[v_idx], NULL);
	if (!dn_key) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	list = talloc_zero(dn_key, struct dn_list);
	if (list == NULL) {
		talloc_free(dn_key);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ltdb_dn_list_load(module, dn_key, list);
	if (ret == LDB_ERR_NO_SUCH_OBJECT) {
		/* it wasn't indexed. Did we have an earlier error? If we did then
		   its gone now */
		talloc_free(dn_key);
		return LDB_SUCCESS;
	}

	if (ret != LDB_SUCCESS) {
		talloc_free(dn_key);
		return ret;
	}

	i = ltdb_dn_list_find_str(list, dn_str);
	if (i == -1) {
		/* nothing to delete */
		talloc_free(dn_key);
		return LDB_SUCCESS;		
	}

	j = (unsigned int) i;
	if (j != list->count - 1) {
		memmove(&list->dn[j], &list->dn[j+1], sizeof(list->dn[0])*(list->count - (j+1)));
	}
	list->count--;
	list->dn = talloc_realloc(list, list->dn, struct ldb_val, list->count);

	ret = ltdb_dn_list_store(module, dn_key, list);

	talloc_free(dn_key);

	return ret;
}

/*
  delete the index entries for a element
  return -1 on failure
*/
int ltdb_index_del_element(struct ldb_module *module, struct ldb_dn *dn,
			   struct ldb_message_element *el)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	const char *dn_str;
	int ret;
	unsigned int i;

	if (!ltdb->cache->attribute_indexes) {
		/* no indexed fields */
		return LDB_SUCCESS;
	}

	dn_str = ldb_dn_get_linearized(dn);
	if (dn_str == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (dn_str[0] == '@') {
		return LDB_SUCCESS;
	}

	if (!ltdb_is_indexed(ltdb->cache->indexlist, el->name)) {
		return LDB_SUCCESS;
	}
	for (i = 0; i < el->num_values; i++) {
		ret = ltdb_index_del_value(module, dn, el, i);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}

/*
  delete the index entries for a record
  return -1 on failure
*/
int ltdb_index_delete(struct ldb_module *module, const struct ldb_message *msg)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	int ret;
	unsigned int i;

	if (ldb_dn_is_special(msg->dn)) {
		return LDB_SUCCESS;
	}

	ret = ltdb_index_onelevel(module, msg, 0);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (!ltdb->cache->attribute_indexes) {
		/* no indexed fields */
		return LDB_SUCCESS;
	}

	for (i = 0; i < msg->num_elements; i++) {
		ret = ltdb_index_del_element(module, msg->dn, &msg->elements[i]);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}


/*
  traversal function that deletes all @INDEX records
*/
static int delete_index(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data, void *state)
{
	struct ldb_module *module = state;
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	const char *dnstr = "DN=" LTDB_INDEX ":";
	struct dn_list list;
	struct ldb_dn *dn;
	struct ldb_val v;
	int ret;

	if (strncmp((char *)key.dptr, dnstr, strlen(dnstr)) != 0) {
		return 0;
	}
	/* we need to put a empty list in the internal tdb for this
	 * index entry */
	list.dn = NULL;
	list.count = 0;

	/* the offset of 3 is to remove the DN= prefix. */
	v.data = key.dptr + 3;
	v.length = strnlen((char *)key.dptr, key.dsize) - 3;

	dn = ldb_dn_from_ldb_val(ltdb, ldb_module_get_ctx(module), &v);
	ret = ltdb_dn_list_store(module, dn, &list);
	if (ret != LDB_SUCCESS) {
		ldb_asprintf_errstring(ldb_module_get_ctx(module), 
				       "Unable to store null index for %s\n",
						ldb_dn_get_linearized(dn));
		talloc_free(dn);
		return -1;
	}
	talloc_free(dn);
	return 0;
}

struct ltdb_reindex_context {
	struct ldb_module *module;
	int error;
};

/*
  traversal function that adds @INDEX records during a re index
*/
static int re_index(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data, void *state)
{
	struct ldb_context *ldb;
	struct ltdb_reindex_context *ctx = (struct ltdb_reindex_context *)state;
	struct ldb_module *module = ctx->module;
	struct ldb_message *msg;
	const char *dn = NULL;
	int ret;
	TDB_DATA key2;

	ldb = ldb_module_get_ctx(module);

	if (strncmp((char *)key.dptr, "DN=@", 4) == 0 ||
	    strncmp((char *)key.dptr, "DN=", 3) != 0) {
		return 0;
	}

	msg = ldb_msg_new(module);
	if (msg == NULL) {
		return -1;
	}

	ret = ltdb_unpack_data(module, &data, msg);
	if (ret != 0) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, "Invalid data for index %s\n",
						ldb_dn_get_linearized(msg->dn));
		talloc_free(msg);
		return -1;
	}

	/* check if the DN key has changed, perhaps due to the
	   case insensitivity of an element changing */
	key2 = ltdb_key(module, msg->dn);
	if (key2.dptr == NULL) {
		/* probably a corrupt record ... darn */
		ldb_debug(ldb, LDB_DEBUG_ERROR, "Invalid DN in re_index: %s",
						ldb_dn_get_linearized(msg->dn));
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
		dn = ldb_dn_get_linearized(msg->dn);
	}

	ret = ltdb_index_onelevel(module, msg, 1);
	if (ret != LDB_SUCCESS) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			  "Adding special ONE LEVEL index failed (%s)!",
						ldb_dn_get_linearized(msg->dn));
		talloc_free(msg);
		return -1;
	}

	ret = ltdb_index_add_all(module, dn, msg->elements, msg->num_elements);

	if (ret != LDB_SUCCESS) {
		ctx->error = ret;
		talloc_free(msg);
		return -1;
	}

	talloc_free(msg);

	return 0;
}

/*
  force a complete reindex of the database
*/
int ltdb_reindex(struct ldb_module *module)
{
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);
	int ret;
	struct ltdb_reindex_context ctx;

	if (ltdb_cache_reload(module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* first traverse the database deleting any @INDEX records by
	 * putting NULL entries in the in-memory tdb
	 */
	ret = tdb_traverse(ltdb->tdb, delete_index, module);
	if (ret == -1) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* if we don't have indexes we have nothing todo */
	if (ltdb->cache->indexlist->num_elements == 0) {
		return LDB_SUCCESS;
	}

	ctx.module = module;
	ctx.error = 0;

	/* now traverse adding any indexes for normal LDB records */
	ret = tdb_traverse(ltdb->tdb, re_index, &ctx);
	if (ret == -1) {
		struct ldb_context *ldb = ldb_module_get_ctx(module);
		ldb_asprintf_errstring(ldb, "reindexing traverse failed: %s", ldb_errstring(ldb));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (ctx.error != LDB_SUCCESS) {
		struct ldb_context *ldb = ldb_module_get_ctx(module);
		ldb_asprintf_errstring(ldb, "reindexing failed: %s", ldb_errstring(ldb));
		return ctx.error;
	}

	return LDB_SUCCESS;
}
