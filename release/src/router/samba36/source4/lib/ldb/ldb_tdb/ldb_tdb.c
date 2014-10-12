/*
   ldb database library

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Stefan Metzmacher 2004
   Copyright (C) Simo Sorce 2006-2008
   Copyright (C) Matthias Dieter Wallnöfer 2009-2010

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
 *  Name: ldb_tdb
 *
 *  Component: ldb tdb backend
 *
 *  Description: core functions for tdb backend
 *
 *  Author: Andrew Tridgell
 *  Author: Stefan Metzmacher
 *
 *  Modifications:
 *
 *  - description: make the module use asynchronous calls
 *    date: Feb 2006
 *    Author: Simo Sorce
 *
 *  - description: make it possible to use event contexts
 *    date: Jan 2008
 *    Author: Simo Sorce
 *
 *  - description: fix up memory leaks and small bugs
 *    date: Oct 2009
 *    Author: Matthias Dieter Wallnöfer
 */

#include "ldb_tdb.h"


/*
  map a tdb error code to a ldb error code
*/
int ltdb_err_map(enum TDB_ERROR tdb_code)
{
	switch (tdb_code) {
	case TDB_SUCCESS:
		return LDB_SUCCESS;
	case TDB_ERR_CORRUPT:
	case TDB_ERR_OOM:
	case TDB_ERR_EINVAL:
		return LDB_ERR_OPERATIONS_ERROR;
	case TDB_ERR_IO:
		return LDB_ERR_PROTOCOL_ERROR;
	case TDB_ERR_LOCK:
	case TDB_ERR_NOLOCK:
		return LDB_ERR_BUSY;
	case TDB_ERR_LOCK_TIMEOUT:
		return LDB_ERR_TIME_LIMIT_EXCEEDED;
	case TDB_ERR_EXISTS:
		return LDB_ERR_ENTRY_ALREADY_EXISTS;
	case TDB_ERR_NOEXIST:
		return LDB_ERR_NO_SUCH_OBJECT;
	case TDB_ERR_RDONLY:
		return LDB_ERR_INSUFFICIENT_ACCESS_RIGHTS;
	default:
		break;
	}
	return LDB_ERR_OTHER;
}

/*
  lock the database for read - use by ltdb_search and ltdb_sequence_number
*/
int ltdb_lock_read(struct ldb_module *module)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	int ret = 0;

	if (ltdb->in_transaction == 0 &&
	    ltdb->read_lock_count == 0) {
		ret = tdb_lockall_read(ltdb->tdb);
	}
	if (ret == 0) {
		ltdb->read_lock_count++;
	}
	return ret;
}

/*
  unlock the database after a ltdb_lock_read()
*/
int ltdb_unlock_read(struct ldb_module *module)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	if (ltdb->in_transaction == 0 && ltdb->read_lock_count == 1) {
		return tdb_unlockall_read(ltdb->tdb);
	}
	ltdb->read_lock_count--;
	return 0;
}


/*
  form a TDB_DATA for a record key
  caller frees

  note that the key for a record can depend on whether the
  dn refers to a case sensitive index record or not
*/
struct TDB_DATA ltdb_key(struct ldb_module *module, struct ldb_dn *dn)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	TDB_DATA key;
	char *key_str = NULL;
	const char *dn_folded = NULL;

	/*
	  most DNs are case insensitive. The exception is index DNs for
	  case sensitive attributes

	  there are 3 cases dealt with in this code:

	  1) if the dn doesn't start with @ then uppercase the attribute
             names and the attributes values of case insensitive attributes
	  2) if the dn starts with @ then leave it alone -
	     the indexing code handles the rest
	*/

	dn_folded = ldb_dn_get_casefold(dn);
	if (!dn_folded) {
		goto failed;
	}

	key_str = talloc_strdup(ldb, "DN=");
	if (!key_str) {
		goto failed;
	}

	key_str = talloc_strdup_append_buffer(key_str, dn_folded);
	if (!key_str) {
		goto failed;
	}

	key.dptr = (uint8_t *)key_str;
	key.dsize = strlen(key_str) + 1;

	return key;

failed:
	errno = ENOMEM;
	key.dptr = NULL;
	key.dsize = 0;
	return key;
}

/*
  check special dn's have valid attributes
  currently only @ATTRIBUTES is checked
*/
static int ltdb_check_special_dn(struct ldb_module *module,
				 const struct ldb_message *msg)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	unsigned int i, j;

	if (! ldb_dn_is_special(msg->dn) ||
	    ! ldb_dn_check_special(msg->dn, LTDB_ATTRIBUTES)) {
		return LDB_SUCCESS;
	}

	/* we have @ATTRIBUTES, let's check attributes are fine */
	/* should we check that we deny multivalued attributes ? */
	for (i = 0; i < msg->num_elements; i++) {
		if (ldb_attr_cmp(msg->elements[i].name, "distinguishedName") == 0) continue;

		for (j = 0; j < msg->elements[i].num_values; j++) {
			if (ltdb_check_at_attributes_values(&msg->elements[i].values[j]) != 0) {
				ldb_set_errstring(ldb, "Invalid attribute value in an @ATTRIBUTES entry");
				return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
			}
		}
	}

	return LDB_SUCCESS;
}


/*
  we've made a modification to a dn - possibly reindex and
  update sequence number
*/
static int ltdb_modified(struct ldb_module *module, struct ldb_dn *dn)
{
	int ret = LDB_SUCCESS;
	struct ltdb_private *ltdb = talloc_get_type(ldb_module_get_private(module), struct ltdb_private);

	/* only allow modifies inside a transaction, otherwise the
	 * ldb is unsafe */
	if (ltdb->in_transaction == 0) {
		ldb_set_errstring(ldb_module_get_ctx(module), "ltdb modify without transaction");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (ldb_dn_is_special(dn) &&
	    (ldb_dn_check_special(dn, LTDB_INDEXLIST) ||
	     ldb_dn_check_special(dn, LTDB_ATTRIBUTES)) ) {
		ret = ltdb_reindex(module);
	}

	/* If the modify was to a normal record, or any special except @BASEINFO, update the seq number */
	if (ret == LDB_SUCCESS &&
	    !(ldb_dn_is_special(dn) &&
	      ldb_dn_check_special(dn, LTDB_BASEINFO)) ) {
		ret = ltdb_increase_sequence_number(module);
	}

	/* If the modify was to @OPTIONS, reload the cache */
	if (ret == LDB_SUCCESS &&
	    ldb_dn_is_special(dn) &&
	    (ldb_dn_check_special(dn, LTDB_OPTIONS)) ) {
		ret = ltdb_cache_reload(module);
	}

	return ret;
}

/*
  store a record into the db
*/
int ltdb_store(struct ldb_module *module, const struct ldb_message *msg, int flgs)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	TDB_DATA tdb_key, tdb_data;
	int ret = LDB_SUCCESS;

	tdb_key = ltdb_key(module, msg->dn);
	if (tdb_key.dptr == NULL) {
		return LDB_ERR_OTHER;
	}

	ret = ltdb_pack_data(module, msg, &tdb_data);
	if (ret == -1) {
		talloc_free(tdb_key.dptr);
		return LDB_ERR_OTHER;
	}

	ret = tdb_store(ltdb->tdb, tdb_key, tdb_data, flgs);
	if (ret == -1) {
		ret = ltdb_err_map(tdb_error(ltdb->tdb));
		goto done;
	}

done:
	talloc_free(tdb_key.dptr);
	talloc_free(tdb_data.dptr);

	return ret;
}


/*
  check if a attribute is a single valued, for a given element
 */
static bool ldb_tdb_single_valued(const struct ldb_schema_attribute *a,
				  struct ldb_message_element *el)
{
	if (!a) return false;
	if (el != NULL) {
		if (el->flags & LDB_FLAG_INTERNAL_FORCE_SINGLE_VALUE_CHECK) {
			/* override from a ldb module, for example
			   used for the description field, which is
			   marked multi-valued in the schema but which
			   should not actually accept multiple
			   values */
			return true;
		}
		if (el->flags & LDB_FLAG_INTERNAL_DISABLE_SINGLE_VALUE_CHECK) {
			/* override from a ldb module, for example used for
			   deleted linked attribute entries */
			return false;
		}
	}
	if (a->flags & LDB_ATTR_FLAG_SINGLE_VALUE) {
		return true;
	}
	return false;
}

static int ltdb_add_internal(struct ldb_module *module,
			     const struct ldb_message *msg)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret = LDB_SUCCESS;
	unsigned int i;

	for (i=0;i<msg->num_elements;i++) {
		struct ldb_message_element *el = &msg->elements[i];
		const struct ldb_schema_attribute *a = ldb_schema_attribute_by_name(ldb, el->name);

		if (el->num_values == 0) {
			ldb_asprintf_errstring(ldb, "attribute '%s' on '%s' specified, but with 0 values (illegal)",
					       el->name, ldb_dn_get_linearized(msg->dn));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
		if (el->num_values > 1 && ldb_tdb_single_valued(a, el)) {
			ldb_asprintf_errstring(ldb, "SINGLE-VALUE attribute %s on %s specified more than once",
					       el->name, ldb_dn_get_linearized(msg->dn));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
	}

	ret = ltdb_store(module, msg, TDB_INSERT);
	if (ret != LDB_SUCCESS) {
		if (ret == LDB_ERR_ENTRY_ALREADY_EXISTS) {
			ldb_asprintf_errstring(ldb,
					       "Entry %s already exists",
					       ldb_dn_get_linearized(msg->dn));
		}
		return ret;
	}

	ret = ltdb_index_add_new(module, msg);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ret = ltdb_modified(module, msg->dn);

	return ret;
}

/*
  add a record to the database
*/
static int ltdb_add(struct ltdb_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	int ret = LDB_SUCCESS;

	ret = ltdb_check_special_dn(module, req->op.add.message);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	if (ltdb_cache_load(module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ltdb_add_internal(module, req->op.add.message);

	return ret;
}

/*
  delete a record from the database, not updating indexes (used for deleting
  index records)
*/
int ltdb_delete_noindex(struct ldb_module *module, struct ldb_dn *dn)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	TDB_DATA tdb_key;
	int ret;

	tdb_key = ltdb_key(module, dn);
	if (!tdb_key.dptr) {
		return LDB_ERR_OTHER;
	}

	ret = tdb_delete(ltdb->tdb, tdb_key);
	talloc_free(tdb_key.dptr);

	if (ret != 0) {
		ret = ltdb_err_map(tdb_error(ltdb->tdb));
	}

	return ret;
}

static int ltdb_delete_internal(struct ldb_module *module, struct ldb_dn *dn)
{
	struct ldb_message *msg;
	int ret = LDB_SUCCESS;

	msg = ldb_msg_new(module);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* in case any attribute of the message was indexed, we need
	   to fetch the old record */
	ret = ltdb_search_dn1(module, dn, msg);
	if (ret != LDB_SUCCESS) {
		/* not finding the old record is an error */
		goto done;
	}

	ret = ltdb_delete_noindex(module, dn);
	if (ret != LDB_SUCCESS) {
		goto done;
	}

	/* remove any indexed attributes */
	ret = ltdb_index_delete(module, msg);
	if (ret != LDB_SUCCESS) {
		goto done;
	}

	ret = ltdb_modified(module, dn);
	if (ret != LDB_SUCCESS) {
		goto done;
	}

done:
	talloc_free(msg);
	return ret;
}

/*
  delete a record from the database
*/
static int ltdb_delete(struct ltdb_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	int ret = LDB_SUCCESS;

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	if (ltdb_cache_load(module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ltdb_delete_internal(module, req->op.del.dn);

	return ret;
}

/*
  find an element by attribute name. At the moment this does a linear search,
  it should be re-coded to use a binary search once all places that modify
  records guarantee sorted order

  return the index of the first matching element if found, otherwise -1
*/
static int find_element(const struct ldb_message *msg, const char *name)
{
	unsigned int i;
	for (i=0;i<msg->num_elements;i++) {
		if (ldb_attr_cmp(msg->elements[i].name, name) == 0) {
			return i;
		}
	}
	return -1;
}


/*
  add an element to an existing record. Assumes a elements array that we
  can call re-alloc on, and assumed that we can re-use the data pointers from
  the passed in additional values. Use with care!

  returns 0 on success, -1 on failure (and sets errno)
*/
static int ltdb_msg_add_element(struct ldb_context *ldb,
				struct ldb_message *msg,
				struct ldb_message_element *el)
{
	struct ldb_message_element *e2;
	unsigned int i;

	if (el->num_values == 0) {
		/* nothing to do here - we don't add empty elements */
		return 0;
	}

	e2 = talloc_realloc(msg, msg->elements, struct ldb_message_element,
			      msg->num_elements+1);
	if (!e2) {
		errno = ENOMEM;
		return -1;
	}

	msg->elements = e2;

	e2 = &msg->elements[msg->num_elements];

	e2->name = el->name;
	e2->flags = el->flags;
	e2->values = talloc_array(msg->elements,
				  struct ldb_val, el->num_values);
	if (!e2->values) {
		errno = ENOMEM;
		return -1;
	}
	for (i=0;i<el->num_values;i++) {
		e2->values[i] = el->values[i];
	}
	e2->num_values = el->num_values;

	++msg->num_elements;

	return 0;
}

/*
  delete all elements having a specified attribute name
*/
static int msg_delete_attribute(struct ldb_module *module,
				struct ldb_context *ldb,
				struct ldb_message *msg, const char *name)
{
	unsigned int i;
	int ret;
	struct ldb_message_element *el;

	el = ldb_msg_find_element(msg, name);
	if (el == NULL) {
		return LDB_ERR_NO_SUCH_ATTRIBUTE;
	}
	i = el - msg->elements;

	ret = ltdb_index_del_element(module, msg->dn, el);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	talloc_free(el->values);
	if (msg->num_elements > (i+1)) {
		memmove(el, el+1, sizeof(*el) * (msg->num_elements - (i+1)));
	}
	msg->num_elements--;
	msg->elements = talloc_realloc(msg, msg->elements,
				       struct ldb_message_element,
				       msg->num_elements);
	return LDB_SUCCESS;
}

/*
  delete all elements matching an attribute name/value

  return LDB Error on failure
*/
static int msg_delete_element(struct ldb_module *module,
			      struct ldb_message *msg,
			      const char *name,
			      const struct ldb_val *val)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	unsigned int i;
	int found, ret;
	struct ldb_message_element *el;
	const struct ldb_schema_attribute *a;

	found = find_element(msg, name);
	if (found == -1) {
		return LDB_ERR_NO_SUCH_ATTRIBUTE;
	}

	i = (unsigned int) found;
	el = &(msg->elements[i]);

	a = ldb_schema_attribute_by_name(ldb, el->name);

	for (i=0;i<el->num_values;i++) {
		bool matched;
		if (a->syntax->operator_fn) {
			ret = a->syntax->operator_fn(ldb, LDB_OP_EQUALITY, a,
						     &el->values[i], val, &matched);
			if (ret != LDB_SUCCESS) return ret;
		} else {
			matched = (a->syntax->comparison_fn(ldb, ldb,
							    &el->values[i], val) == 0);
		}
		if (matched) {
			if (el->num_values == 1) {
				return msg_delete_attribute(module, ldb, msg, name);
			}

			ret = ltdb_index_del_value(module, msg->dn, el, i);
			if (ret != LDB_SUCCESS) {
				return ret;
			}

			if (i<el->num_values-1) {
				memmove(&el->values[i], &el->values[i+1],
					sizeof(el->values[i])*
						(el->num_values-(i+1)));
			}
			el->num_values--;

			/* per definition we find in a canonicalised message an
			   attribute value only once. So we are finished here */
			return LDB_SUCCESS;
		}
	}

	/* Not found */
	return LDB_ERR_NO_SUCH_ATTRIBUTE;
}


/*
  modify a record - internal interface

  yuck - this is O(n^2). Luckily n is usually small so we probably
  get away with it, but if we ever have really large attribute lists
  then we'll need to look at this again

  'req' is optional, and is used to specify controls if supplied
*/
int ltdb_modify_internal(struct ldb_module *module,
			 const struct ldb_message *msg,
			 struct ldb_request *req)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	TDB_DATA tdb_key, tdb_data;
	struct ldb_message *msg2;
	unsigned int i, j, k;
	int ret = LDB_SUCCESS, idx;
	struct ldb_control *control_permissive = NULL;

	if (req) {
		control_permissive = ldb_request_get_control(req,
					LDB_CONTROL_PERMISSIVE_MODIFY_OID);
	}

	tdb_key = ltdb_key(module, msg->dn);
	if (!tdb_key.dptr) {
		return LDB_ERR_OTHER;
	}

	tdb_data = tdb_fetch(ltdb->tdb, tdb_key);
	if (!tdb_data.dptr) {
		talloc_free(tdb_key.dptr);
		return ltdb_err_map(tdb_error(ltdb->tdb));
	}

	msg2 = ldb_msg_new(tdb_key.dptr);
	if (msg2 == NULL) {
		free(tdb_data.dptr);
		ret = LDB_ERR_OTHER;
		goto done;
	}

	ret = ltdb_unpack_data(module, &tdb_data, msg2);
	free(tdb_data.dptr);
	if (ret == -1) {
		ret = LDB_ERR_OTHER;
		goto done;
	}

	if (!msg2->dn) {
		msg2->dn = msg->dn;
	}

	for (i=0; i<msg->num_elements; i++) {
		struct ldb_message_element *el = &msg->elements[i], *el2;
		struct ldb_val *vals;
		const struct ldb_schema_attribute *a = ldb_schema_attribute_by_name(ldb, el->name);
		const char *dn;

		switch (msg->elements[i].flags & LDB_FLAG_MOD_MASK) {
		case LDB_FLAG_MOD_ADD:

			if (el->num_values == 0) {
				ldb_asprintf_errstring(ldb,
						       "attribute '%s': attribute on '%s' specified, but with 0 values (illegal)",
						       el->name, ldb_dn_get_linearized(msg2->dn));
				ret = LDB_ERR_CONSTRAINT_VIOLATION;
				goto done;
			}

			/* make a copy of the array so that a permissive
			 * control can remove duplicates without changing the
			 * original values, but do not copy data as we do not
			 * need to keep it around once the operation is
			 * finished */
			if (control_permissive) {
				el = talloc(msg2, struct ldb_message_element);
				if (!el) {
					ret = LDB_ERR_OTHER;
					goto done;
				}
				*el = msg->elements[i];
				el->values = talloc_array(el, struct ldb_val, el->num_values);
				if (el->values == NULL) {
					ret = LDB_ERR_OTHER;
					goto done;
				}
				for (j = 0; j < el->num_values; j++) {
					el->values[j] = msg->elements[i].values[j];
				}
			}

			if (el->num_values > 1 && ldb_tdb_single_valued(a, el)) {
				ldb_asprintf_errstring(ldb, "SINGLE-VALUE attribute %s on %s specified more than once",
						       el->name, ldb_dn_get_linearized(msg2->dn));
				ret = LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
				goto done;
			}

			/* Checks if element already exists */
			idx = find_element(msg2, el->name);
			if (idx == -1) {
				if (ltdb_msg_add_element(ldb, msg2, el) != 0) {
					ret = LDB_ERR_OTHER;
					goto done;
				}
				ret = ltdb_index_add_element(module, msg2->dn,
							     el);
				if (ret != LDB_SUCCESS) {
					goto done;
				}
			} else {
				j = (unsigned int) idx;
				el2 = &(msg2->elements[j]);

				/* We cannot add another value on a existing one
				   if the attribute is single-valued */
				if (ldb_tdb_single_valued(a, el)) {
					ldb_asprintf_errstring(ldb, "SINGLE-VALUE attribute %s on %s specified more than once",
						               el->name, ldb_dn_get_linearized(msg2->dn));
					ret = LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
					goto done;
				}

				/* Check that values don't exist yet on multi-
				   valued attributes or aren't provided twice */
				for (j = 0; j < el->num_values; j++) {
					if (ldb_msg_find_val(el2, &el->values[j]) != NULL) {
						if (control_permissive) {
							/* remove this one as if it was never added */
							el->num_values--;
							for (k = j; k < el->num_values; k++) {
								el->values[k] = el->values[k + 1];
							}
							j--; /* rewind */

							continue;
						}

						ldb_asprintf_errstring(ldb,
								       "attribute '%s': value #%u on '%s' already exists",
								       el->name, j, ldb_dn_get_linearized(msg2->dn));
						ret = LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
						goto done;
					}
					if (ldb_msg_find_val(el, &el->values[j]) != &el->values[j]) {
						ldb_asprintf_errstring(ldb,
								       "attribute '%s': value #%u on '%s' provided more than once",
								       el->name, j, ldb_dn_get_linearized(msg2->dn));
						ret = LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
						goto done;
					}
				}

				/* Now combine existing and new values to a new
				   attribute record */
				vals = talloc_realloc(msg2->elements,
						      el2->values, struct ldb_val,
						      el2->num_values + el->num_values);
				if (vals == NULL) {
					ldb_oom(ldb);
					ret = LDB_ERR_OTHER;
					goto done;
				}

				for (j=0; j<el->num_values; j++) {
					vals[el2->num_values + j] =
						ldb_val_dup(vals, &el->values[j]);
				}

				el2->values = vals;
				el2->num_values += el->num_values;

				ret = ltdb_index_add_element(module, msg2->dn, el);
				if (ret != LDB_SUCCESS) {
					goto done;
				}
			}

			break;

		case LDB_FLAG_MOD_REPLACE:

			if (el->num_values > 1 && ldb_tdb_single_valued(a, el)) {
				ldb_asprintf_errstring(ldb, "SINGLE-VALUE attribute %s on %s specified more than once",
						       el->name, ldb_dn_get_linearized(msg2->dn));
				ret = LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
				goto done;
			}

			/* TODO: This is O(n^2) - replace with more efficient check */
			for (j=0; j<el->num_values; j++) {
				if (ldb_msg_find_val(el, &el->values[j]) != &el->values[j]) {
					ldb_asprintf_errstring(ldb,
							       "attribute '%s': value #%u on '%s' provided more than once",
							       el->name, j, ldb_dn_get_linearized(msg2->dn));
					ret = LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
					goto done;
				}
			}

			/* Checks if element already exists */
			idx = find_element(msg2, el->name);
			if (idx != -1) {
				j = (unsigned int) idx;
				el2 = &(msg2->elements[j]);
				if (ldb_msg_element_compare(el, el2) == 0) {
					/* we are replacing with the same values */
					continue;
				}
			
				/* Delete the attribute if it exists in the DB */
				if (msg_delete_attribute(module, ldb, msg2,
							 el->name) != 0) {
					ret = LDB_ERR_OTHER;
					goto done;
				}
			}

			/* Recreate it with the new values */
			if (ltdb_msg_add_element(ldb, msg2, el) != 0) {
				ret = LDB_ERR_OTHER;
				goto done;
			}

			ret = ltdb_index_add_element(module, msg2->dn, el);
			if (ret != LDB_SUCCESS) {
				goto done;
			}

			break;

		case LDB_FLAG_MOD_DELETE:
			dn = ldb_dn_get_linearized(msg2->dn);
			if (dn == NULL) {
				ret = LDB_ERR_OTHER;
				goto done;
			}

			if (msg->elements[i].num_values == 0) {
				/* Delete the whole attribute */
				ret = msg_delete_attribute(module, ldb, msg2,
							   msg->elements[i].name);
				if (ret == LDB_ERR_NO_SUCH_ATTRIBUTE &&
				    control_permissive) {
					ret = LDB_SUCCESS;
				} else {
					ldb_asprintf_errstring(ldb,
							       "attribute '%s': no such attribute for delete on '%s'",
							       msg->elements[i].name, dn);
				}
				if (ret != LDB_SUCCESS) {
					goto done;
				}
			} else {
				/* Delete specified values from an attribute */
				for (j=0; j < msg->elements[i].num_values; j++) {
					ret = msg_delete_element(module,
							         msg2,
							         msg->elements[i].name,
							         &msg->elements[i].values[j]);
					if (ret == LDB_ERR_NO_SUCH_ATTRIBUTE &&
					    control_permissive) {
						ret = LDB_SUCCESS;
					} else {
						ldb_asprintf_errstring(ldb,
								       "attribute '%s': no matching attribute value while deleting attribute on '%s'",
								       msg->elements[i].name, dn);
					}
					if (ret != LDB_SUCCESS) {
						goto done;
					}
				}
			}
			break;
		default:
			ldb_asprintf_errstring(ldb,
					       "attribute '%s': invalid modify flags on '%s': 0x%x",
					       msg->elements[i].name, ldb_dn_get_linearized(msg->dn),
					       msg->elements[i].flags & LDB_FLAG_MOD_MASK);
			ret = LDB_ERR_PROTOCOL_ERROR;
			goto done;
		}
	}

	ret = ltdb_store(module, msg2, TDB_MODIFY);
	if (ret != LDB_SUCCESS) {
		goto done;
	}

	ret = ltdb_modified(module, msg2->dn);
	if (ret != LDB_SUCCESS) {
		goto done;
	}

done:
	talloc_free(tdb_key.dptr);
	return ret;
}

/*
  modify a record
*/
static int ltdb_modify(struct ltdb_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	int ret = LDB_SUCCESS;

	ret = ltdb_check_special_dn(module, req->op.mod.message);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	if (ltdb_cache_load(module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ltdb_modify_internal(module, req->op.mod.message, req);

	return ret;
}

/*
  rename a record
*/
static int ltdb_rename(struct ltdb_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	struct ldb_message *msg;
	int ret = LDB_SUCCESS;

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	if (ltdb_cache_load(ctx->module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg = ldb_msg_new(ctx);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* in case any attribute of the message was indexed, we need
	   to fetch the old record */
	ret = ltdb_search_dn1(module, req->op.rename.olddn, msg);
	if (ret != LDB_SUCCESS) {
		/* not finding the old record is an error */
		return ret;
	}

	/* Always delete first then add, to avoid conflicts with
	 * unique indexes. We rely on the transaction to make this
	 * atomic
	 */
	ret = ltdb_delete_internal(module, msg->dn);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	msg->dn = ldb_dn_copy(msg, req->op.rename.newdn);
	if (msg->dn == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ltdb_add_internal(module, msg);

	return ret;
}

static int ltdb_start_trans(struct ldb_module *module)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);

	if (tdb_transaction_start(ltdb->tdb) != 0) {
		return ltdb_err_map(tdb_error(ltdb->tdb));
	}

	ltdb->in_transaction++;

	ltdb_index_transaction_start(module);

	return LDB_SUCCESS;
}

static int ltdb_prepare_commit(struct ldb_module *module)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);

	if (ltdb->in_transaction != 1) {
		return LDB_SUCCESS;
	}

	if (ltdb_index_transaction_commit(module) != 0) {
		tdb_transaction_cancel(ltdb->tdb);
		ltdb->in_transaction--;
		return ltdb_err_map(tdb_error(ltdb->tdb));
	}

	if (tdb_transaction_prepare_commit(ltdb->tdb) != 0) {
		ltdb->in_transaction--;
		return ltdb_err_map(tdb_error(ltdb->tdb));
	}

	ltdb->prepared_commit = true;

	return LDB_SUCCESS;
}

static int ltdb_end_trans(struct ldb_module *module)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);

	if (!ltdb->prepared_commit) {
		int ret = ltdb_prepare_commit(module);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	ltdb->in_transaction--;
	ltdb->prepared_commit = false;

	if (tdb_transaction_commit(ltdb->tdb) != 0) {
		return ltdb_err_map(tdb_error(ltdb->tdb));
	}

	return LDB_SUCCESS;
}

static int ltdb_del_trans(struct ldb_module *module)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);

	ltdb->in_transaction--;

	if (ltdb_index_transaction_cancel(module) != 0) {
		tdb_transaction_cancel(ltdb->tdb);
		return ltdb_err_map(tdb_error(ltdb->tdb));
	}

	if (tdb_transaction_cancel(ltdb->tdb) != 0) {
		return ltdb_err_map(tdb_error(ltdb->tdb));
	}

	return LDB_SUCCESS;
}

/*
  return sequenceNumber from @BASEINFO
*/
static int ltdb_sequence_number(struct ltdb_context *ctx,
				struct ldb_extended **ext)
{
	struct ldb_context *ldb;
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	TALLOC_CTX *tmp_ctx = NULL;
	struct ldb_seqnum_request *seq;
	struct ldb_seqnum_result *res;
	struct ldb_message *msg = NULL;
	struct ldb_dn *dn;
	const char *date;
	int ret = LDB_SUCCESS;

	ldb = ldb_module_get_ctx(module);

	seq = talloc_get_type(req->op.extended.data,
				struct ldb_seqnum_request);
	if (seq == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	if (ltdb_lock_read(module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	res = talloc_zero(req, struct ldb_seqnum_result);
	if (res == NULL) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}

	tmp_ctx = talloc_new(req);
	if (tmp_ctx == NULL) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}

	dn = ldb_dn_new(tmp_ctx, ldb, LTDB_BASEINFO);
	if (dn == NULL) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}

	msg = ldb_msg_new(tmp_ctx);
	if (msg == NULL) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}

	ret = ltdb_search_dn1(module, dn, msg);
	if (ret != LDB_SUCCESS) {
		goto done;
	}

	switch (seq->type) {
	case LDB_SEQ_HIGHEST_SEQ:
		res->seq_num = ldb_msg_find_attr_as_uint64(msg, LTDB_SEQUENCE_NUMBER, 0);
		break;
	case LDB_SEQ_NEXT:
		res->seq_num = ldb_msg_find_attr_as_uint64(msg, LTDB_SEQUENCE_NUMBER, 0);
		res->seq_num++;
		break;
	case LDB_SEQ_HIGHEST_TIMESTAMP:
		date = ldb_msg_find_attr_as_string(msg, LTDB_MOD_TIMESTAMP, NULL);
		if (date) {
			res->seq_num = ldb_string_to_time(date);
		} else {
			res->seq_num = 0;
			/* zero is as good as anything when we don't know */
		}
		break;
	}

	*ext = talloc_zero(req, struct ldb_extended);
	if (*ext == NULL) {
		ret = LDB_ERR_OPERATIONS_ERROR;
		goto done;
	}
	(*ext)->oid = LDB_EXTENDED_SEQUENCE_NUMBER;
	(*ext)->data = talloc_steal(*ext, res);

done:
	talloc_free(tmp_ctx);
	ltdb_unlock_read(module);
	return ret;
}

static void ltdb_request_done(struct ltdb_context *ctx, int error)
{
	struct ldb_context *ldb;
	struct ldb_request *req;
	struct ldb_reply *ares;

	ldb = ldb_module_get_ctx(ctx->module);
	req = ctx->req;

	/* if we already returned an error just return */
	if (ldb_request_get_status(req) != LDB_SUCCESS) {
		return;
	}

	ares = talloc_zero(req, struct ldb_reply);
	if (!ares) {
		ldb_oom(ldb);
		req->callback(req, NULL);
		return;
	}
	ares->type = LDB_REPLY_DONE;
	ares->error = error;

	req->callback(req, ares);
}

static void ltdb_timeout(struct tevent_context *ev,
			  struct tevent_timer *te,
			  struct timeval t,
			  void *private_data)
{
	struct ltdb_context *ctx;
	ctx = talloc_get_type(private_data, struct ltdb_context);

	if (!ctx->request_terminated) {
		/* request is done now */
		ltdb_request_done(ctx, LDB_ERR_TIME_LIMIT_EXCEEDED);
	}

	if (!ctx->request_terminated) {
		/* neutralize the spy */
		ctx->spy->ctx = NULL;
	}
	talloc_free(ctx);
}

static void ltdb_request_extended_done(struct ltdb_context *ctx,
					struct ldb_extended *ext,
					int error)
{
	struct ldb_context *ldb;
	struct ldb_request *req;
	struct ldb_reply *ares;

	ldb = ldb_module_get_ctx(ctx->module);
	req = ctx->req;

	/* if we already returned an error just return */
	if (ldb_request_get_status(req) != LDB_SUCCESS) {
		return;
	}

	ares = talloc_zero(req, struct ldb_reply);
	if (!ares) {
		ldb_oom(ldb);
		req->callback(req, NULL);
		return;
	}
	ares->type = LDB_REPLY_DONE;
	ares->response = ext;
	ares->error = error;

	req->callback(req, ares);
}

static void ltdb_handle_extended(struct ltdb_context *ctx)
{
	struct ldb_extended *ext = NULL;
	int ret;

	if (strcmp(ctx->req->op.extended.oid,
		   LDB_EXTENDED_SEQUENCE_NUMBER) == 0) {
		/* get sequence number */
		ret = ltdb_sequence_number(ctx, &ext);
	} else {
		/* not recognized */
		ret = LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION;
	}

	ltdb_request_extended_done(ctx, ext, ret);
}

static void ltdb_callback(struct tevent_context *ev,
			  struct tevent_timer *te,
			  struct timeval t,
			  void *private_data)
{
	struct ltdb_context *ctx;
	int ret;

	ctx = talloc_get_type(private_data, struct ltdb_context);

	if (ctx->request_terminated) {
		goto done;
	}

	switch (ctx->req->operation) {
	case LDB_SEARCH:
		ret = ltdb_search(ctx);
		break;
	case LDB_ADD:
		ret = ltdb_add(ctx);
		break;
	case LDB_MODIFY:
		ret = ltdb_modify(ctx);
		break;
	case LDB_DELETE:
		ret = ltdb_delete(ctx);
		break;
	case LDB_RENAME:
		ret = ltdb_rename(ctx);
		break;
	case LDB_EXTENDED:
		ltdb_handle_extended(ctx);
		goto done;
	default:
		/* no other op supported */
		ret = LDB_ERR_PROTOCOL_ERROR;
	}

	if (!ctx->request_terminated) {
		/* request is done now */
		ltdb_request_done(ctx, ret);
	}

done:
	if (!ctx->request_terminated) {
		/* neutralize the spy */
		ctx->spy->ctx = NULL;
	}
	talloc_free(ctx);
}

static int ltdb_request_destructor(void *ptr)
{
	struct ltdb_req_spy *spy = talloc_get_type(ptr, struct ltdb_req_spy);

	if (spy->ctx != NULL) {
		spy->ctx->request_terminated = true;
	}

	return 0;
}

static int ltdb_handle_request(struct ldb_module *module,
			       struct ldb_request *req)
{
	struct ldb_control *control_permissive;
	struct ldb_context *ldb;
	struct tevent_context *ev;
	struct ltdb_context *ac;
	struct tevent_timer *te;
	struct timeval tv;
	unsigned int i;

	ldb = ldb_module_get_ctx(module);

	control_permissive = ldb_request_get_control(req,
					LDB_CONTROL_PERMISSIVE_MODIFY_OID);

	for (i = 0; req->controls && req->controls[i]; i++) {
		if (req->controls[i]->critical &&
		    req->controls[i] != control_permissive) {
			ldb_asprintf_errstring(ldb, "Unsupported critical extension %s",
					       req->controls[i]->oid);
			return LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION;
		}
	}

	if (req->starttime == 0 || req->timeout == 0) {
		ldb_set_errstring(ldb, "Invalid timeout settings");
		return LDB_ERR_TIME_LIMIT_EXCEEDED;
	}

	ev = ldb_get_event_context(ldb);

	ac = talloc_zero(ldb, struct ltdb_context);
	if (ac == NULL) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->module = module;
	ac->req = req;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	te = tevent_add_timer(ev, ac, tv, ltdb_callback, ac);
	if (NULL == te) {
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	tv.tv_sec = req->starttime + req->timeout;
	ac->timeout_event = tevent_add_timer(ev, ac, tv, ltdb_timeout, ac);
	if (NULL == ac->timeout_event) {
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* set a spy so that we do not try to use the request context
	 * if it is freed before ltdb_callback fires */
	ac->spy = talloc(req, struct ltdb_req_spy);
	if (NULL == ac->spy) {
		talloc_free(ac);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->spy->ctx = ac;

	talloc_set_destructor((TALLOC_CTX *)ac->spy, ltdb_request_destructor);

	return LDB_SUCCESS;
}

static int ltdb_init_rootdse(struct ldb_module *module)
{
	struct ldb_context *ldb;
	int ret;

	ldb = ldb_module_get_ctx(module);

	ret = ldb_mod_register_control(module,
				       LDB_CONTROL_PERMISSIVE_MODIFY_OID);
	/* ignore errors on this - we expect it for non-sam databases */

	/* there can be no module beyond the backend, just return */
	return LDB_SUCCESS;
}

static const struct ldb_module_ops ltdb_ops = {
	.name              = "tdb",
	.init_context      = ltdb_init_rootdse,
	.search            = ltdb_handle_request,
	.add               = ltdb_handle_request,
	.modify            = ltdb_handle_request,
	.del               = ltdb_handle_request,
	.rename            = ltdb_handle_request,
	.extended          = ltdb_handle_request,
	.start_transaction = ltdb_start_trans,
	.end_transaction   = ltdb_end_trans,
	.prepare_commit    = ltdb_prepare_commit,
	.del_transaction   = ltdb_del_trans,
};

/*
  connect to the database
*/
static int ltdb_connect(struct ldb_context *ldb, const char *url,
			unsigned int flags, const char *options[],
			struct ldb_module **_module)
{
	struct ldb_module *module;
	const char *path;
	int tdb_flags, open_flags;
	struct ltdb_private *ltdb;

	/* parse the url */
	if (strchr(url, ':')) {
		if (strncmp(url, "tdb://", 6) != 0) {
			ldb_debug(ldb, LDB_DEBUG_ERROR,
				  "Invalid tdb URL '%s'", url);
			return LDB_ERR_OPERATIONS_ERROR;
		}
		path = url+6;
	} else {
		path = url;
	}

	tdb_flags = TDB_DEFAULT | TDB_SEQNUM;

	/* check for the 'nosync' option */
	if (flags & LDB_FLG_NOSYNC) {
		tdb_flags |= TDB_NOSYNC;
	}

	/* and nommap option */
	if (flags & LDB_FLG_NOMMAP) {
		tdb_flags |= TDB_NOMMAP;
	}

	if (flags & LDB_FLG_RDONLY) {
		open_flags = O_RDONLY;
	} else {
		open_flags = O_CREAT | O_RDWR;
	}

	ltdb = talloc_zero(ldb, struct ltdb_private);
	if (!ltdb) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* note that we use quite a large default hash size */
	ltdb->tdb = ltdb_wrap_open(ltdb, path, 10000,
				   tdb_flags, open_flags,
				   ldb_get_create_perms(ldb), ldb);
	if (!ltdb->tdb) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			  "Unable to open tdb '%s'", path);
		talloc_free(ltdb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (getenv("LDB_WARN_UNINDEXED")) {
		ltdb->warn_unindexed = true;
	}

	ltdb->sequence_number = 0;

	module = ldb_module_new(ldb, ldb, "ldb_tdb backend", &ltdb_ops);
	if (!module) {
		talloc_free(ltdb);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ldb_module_set_private(module, ltdb);
	talloc_steal(module, ltdb);

	if (ltdb_cache_load(module) != 0) {
		talloc_free(module);
		talloc_free(ltdb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	*_module = module;
	return LDB_SUCCESS;
}

int ldb_tdb_init(const char *version)
{
	LDB_MODULE_CHECK_VERSION(version);
	return ldb_register_backend("tdb", ltdb_connect, false);
}
