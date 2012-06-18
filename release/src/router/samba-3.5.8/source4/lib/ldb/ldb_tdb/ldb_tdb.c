/*
   ldb database library

   Copyright (C) Andrew Tridgell  2004
   Copyright (C) Stefan Metzmacher  2004
   Copyright (C) Simo Sorce       2006-2008


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
 *  - description: make the module use asyncronous calls
 *    date: Feb 2006
 *    Author: Simo Sorce
 *
 *  - description: make it possible to use event contexts
 *    date: Jan 2008
 *    Author: Simo Sorce
 */

#include "ldb_tdb.h"


/*
  map a tdb error code to a ldb error code
*/
static int ltdb_err_map(enum TDB_ERROR tdb_code)
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
	if (ltdb->in_transaction == 0) {
		return tdb_lockall_read(ltdb->tdb);
	}
	return 0;
}

/*
  unlock the database after a ltdb_lock_read()
*/
int ltdb_unlock_read(struct ldb_module *module)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	if (ltdb->in_transaction == 0) {
		return tdb_unlockall_read(ltdb->tdb);
	}
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
	int i, j;

	if (! ldb_dn_is_special(msg->dn) ||
	    ! ldb_dn_check_special(msg->dn, LTDB_ATTRIBUTES)) {
		return 0;
	}

	/* we have @ATTRIBUTES, let's check attributes are fine */
	/* should we check that we deny multivalued attributes ? */
	for (i = 0; i < msg->num_elements; i++) {
		for (j = 0; j < msg->elements[i].num_values; j++) {
			if (ltdb_check_at_attributes_values(&msg->elements[i].values[j]) != 0) {
				ldb_set_errstring(ldb, "Invalid attribute value in an @ATTRIBUTES entry");
				return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
			}
		}
	}

	return 0;
}


/*
  we've made a modification to a dn - possibly reindex and
  update sequence number
*/
static int ltdb_modified(struct ldb_module *module, struct ldb_dn *dn)
{
	int ret = LDB_SUCCESS;

	if (ldb_dn_is_special(dn) &&
	    (ldb_dn_check_special(dn, LTDB_INDEXLIST) ||
	     ldb_dn_check_special(dn, LTDB_ATTRIBUTES)) ) {
		ret = ltdb_reindex(module);
	}

	if (ret == LDB_SUCCESS &&
	    !(ldb_dn_is_special(dn) &&
	      ldb_dn_check_special(dn, LTDB_BASEINFO)) ) {
		ret = ltdb_increase_sequence_number(module);
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
	int ret;

	tdb_key = ltdb_key(module, msg->dn);
	if (!tdb_key.dptr) {
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

	ret = ltdb_index_add(module, msg);
	if (ret != LDB_SUCCESS) {
		tdb_delete(ltdb->tdb, tdb_key);
	}

done:
	talloc_free(tdb_key.dptr);
	talloc_free(tdb_data.dptr);

	return ret;
}


static int ltdb_add_internal(struct ldb_module *module,
			     const struct ldb_message *msg)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	int ret, i;

	ret = ltdb_check_special_dn(module, msg);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (ltdb_cache_load(module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	for (i=0;i<msg->num_elements;i++) {
		struct ldb_message_element *el = &msg->elements[i];
		const struct ldb_schema_attribute *a = ldb_schema_attribute_by_name(ldb, el->name);

		if (el->num_values == 0) {
			ldb_asprintf_errstring(ldb, "attribute %s on %s specified, but with 0 values (illegal)", 
					       el->name, ldb_dn_get_linearized(msg->dn));
			return LDB_ERR_CONSTRAINT_VIOLATION;
		}
		if (a && a->flags & LDB_ATTR_FLAG_SINGLE_VALUE) {
			if (el->num_values > 1) {
				ldb_asprintf_errstring(ldb, "SINGLE-VALUE attribute %s on %s speicified more than once", 
						       el->name, ldb_dn_get_linearized(msg->dn));
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
		}
	}

	ret = ltdb_store(module, msg, TDB_INSERT);

	if (ret == LDB_ERR_ENTRY_ALREADY_EXISTS) {
		ldb_asprintf_errstring(ldb,
					"Entry %s already exists",
					ldb_dn_get_linearized(msg->dn));
		return ret;
	}

	if (ret == LDB_SUCCESS) {
		ret = ltdb_index_one(module, msg, 1);
		if (ret != LDB_SUCCESS) {
			return ret;
		}

		ret = ltdb_modified(module, msg->dn);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return ret;
}

/*
  add a record to the database
*/
static int ltdb_add(struct ltdb_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	int tret;

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	tret = ltdb_add_internal(module, req->op.add.message);
	if (tret != LDB_SUCCESS) {
		return tret;
	}

	return LDB_SUCCESS;
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
	int ret;

	msg = talloc(module, struct ldb_message);
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

	/* remove one level attribute */
	ret = ltdb_index_one(module, msg, 0);
	if (ret != LDB_SUCCESS) {
		goto done;
	}

	/* remove any indexed attributes */
	ret = ltdb_index_del(module, msg);
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
	int tret;

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	if (ltdb_cache_load(module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	tret = ltdb_delete_internal(module, req->op.del.dn);
	if (tret != LDB_SUCCESS) {
		return tret;
	}

	return LDB_SUCCESS;
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
static int msg_add_element(struct ldb_context *ldb,
			   struct ldb_message *msg,
			   struct ldb_message_element *el)
{
	struct ldb_message_element *e2;
	unsigned int i;

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
	e2->values = NULL;
	if (el->num_values != 0) {
		e2->values = talloc_array(msg->elements,
					  struct ldb_val, el->num_values);
		if (!e2->values) {
			errno = ENOMEM;
			return -1;
		}
	}
	for (i=0;i<el->num_values;i++) {
		e2->values[i] = el->values[i];
	}
	e2->num_values = el->num_values;

	msg->num_elements++;

	return 0;
}

/*
  delete all elements having a specified attribute name
*/
static int msg_delete_attribute(struct ldb_module *module,
				struct ldb_context *ldb,
				struct ldb_message *msg, const char *name)
{
	const char *dn;
	unsigned int i, j;

	dn = ldb_dn_get_linearized(msg->dn);
	if (dn == NULL) {
		return -1;
	}

	for (i=0;i<msg->num_elements;i++) {
		if (ldb_attr_cmp(msg->elements[i].name, name) == 0) {
			for (j=0;j<msg->elements[i].num_values;j++) {
				ltdb_index_del_value(module, dn,
						     &msg->elements[i], j);
			}
			talloc_free(msg->elements[i].values);
			if (msg->num_elements > (i+1)) {
				memmove(&msg->elements[i],
					&msg->elements[i+1],
					sizeof(struct ldb_message_element)*
					(msg->num_elements - (i+1)));
			}
			msg->num_elements--;
			i--;
			msg->elements = talloc_realloc(msg, msg->elements,
							struct ldb_message_element,
							msg->num_elements);
		}
	}

	return 0;
}

/*
  delete all elements matching an attribute name/value

  return 0 on success, -1 on failure
*/
static int msg_delete_element(struct ldb_module *module,
			      struct ldb_message *msg,
			      const char *name,
			      const struct ldb_val *val)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	unsigned int i;
	int found;
	struct ldb_message_element *el;
	const struct ldb_schema_attribute *a;

	found = find_element(msg, name);
	if (found == -1) {
		return -1;
	}

	el = &msg->elements[found];

	a = ldb_schema_attribute_by_name(ldb, el->name);

	for (i=0;i<el->num_values;i++) {
		if (a->syntax->comparison_fn(ldb, ldb,
						&el->values[i], val) == 0) {
			if (i<el->num_values-1) {
				memmove(&el->values[i], &el->values[i+1],
					sizeof(el->values[i])*
						(el->num_values-(i+1)));
			}
			el->num_values--;
			if (el->num_values == 0) {
				return msg_delete_attribute(module, ldb,
							    msg, name);
			}
			return 0;
		}
	}

	return -1;
}


/*
  modify a record - internal interface

  yuck - this is O(n^2). Luckily n is usually small so we probably
  get away with it, but if we ever have really large attribute lists
  then we'll need to look at this again
*/
int ltdb_modify_internal(struct ldb_module *module,
			 const struct ldb_message *msg)
{
	struct ldb_context *ldb = ldb_module_get_ctx(module);
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	TDB_DATA tdb_key, tdb_data;
	struct ldb_message *msg2;
	unsigned i, j;
	int ret, idx;

	tdb_key = ltdb_key(module, msg->dn);
	if (!tdb_key.dptr) {
		return LDB_ERR_OTHER;
	}

	tdb_data = tdb_fetch(ltdb->tdb, tdb_key);
	if (!tdb_data.dptr) {
		talloc_free(tdb_key.dptr);
		return ltdb_err_map(tdb_error(ltdb->tdb));
	}

	msg2 = talloc(tdb_key.dptr, struct ldb_message);
	if (msg2 == NULL) {
		talloc_free(tdb_key.dptr);
		return LDB_ERR_OTHER;
	}

	ret = ltdb_unpack_data(module, &tdb_data, msg2);
	if (ret == -1) {
		ret = LDB_ERR_OTHER;
		goto failed;
	}

	if (!msg2->dn) {
		msg2->dn = msg->dn;
	}

	for (i=0;i<msg->num_elements;i++) {
		struct ldb_message_element *el = &msg->elements[i];
		struct ldb_message_element *el2;
		struct ldb_val *vals;
		const char *dn;
		const struct ldb_schema_attribute *a = ldb_schema_attribute_by_name(ldb, el->name);
		switch (msg->elements[i].flags & LDB_FLAG_MOD_MASK) {

		case LDB_FLAG_MOD_ADD:
			
			/* add this element to the message. fail if it
			   already exists */
			idx = find_element(msg2, el->name);

			if (el->num_values == 0) {
				ldb_asprintf_errstring(ldb, "attribute %s on %s speicified, but with 0 values (illigal)", 
						  el->name, ldb_dn_get_linearized(msg->dn));
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}
			if (idx == -1) {
				if (a && a->flags & LDB_ATTR_FLAG_SINGLE_VALUE) {
					if (el->num_values > 1) {
						ldb_asprintf_errstring(ldb, "SINGLE-VALUE attribute %s on %s speicified more than once", 
							       el->name, ldb_dn_get_linearized(msg->dn));
						return LDB_ERR_CONSTRAINT_VIOLATION;
					}
				}
				if (msg_add_element(ldb, msg2, el) != 0) {
					ret = LDB_ERR_OTHER;
					goto failed;
				}
				continue;
			}

			/* If this is an add, then if it already
			 * exists in the object, then we violoate the
			 * single-value rule */
			if (a && a->flags & LDB_ATTR_FLAG_SINGLE_VALUE) {
				return LDB_ERR_CONSTRAINT_VIOLATION;
			}

			el2 = &msg2->elements[idx];

			/* An attribute with this name already exists,
			 * add all values if they don't already exist
			 * (check both the other elements to be added,
			 * and those already in the db). */

			for (j=0;j<el->num_values;j++) {
				if (ldb_msg_find_val(el2, &el->values[j])) {
					ldb_asprintf_errstring(ldb, "%s: value #%d already exists", el->name, j);
					ret = LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
					goto failed;
				}
				if (ldb_msg_find_val(el, &el->values[j]) != &el->values[j]) {
					ldb_asprintf_errstring(ldb, "%s: value #%d provided more than once", el->name, j);
					ret = LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
					goto failed;
				}
			}

		        vals = talloc_realloc(msg2->elements, el2->values, struct ldb_val,
						el2->num_values + el->num_values);

			if (vals == NULL) {
				ret = LDB_ERR_OTHER;
				goto failed;
			}

			for (j=0;j<el->num_values;j++) {
				vals[el2->num_values + j] =
					ldb_val_dup(vals, &el->values[j]);
			}

			el2->values = vals;
			el2->num_values += el->num_values;

			break;

		case LDB_FLAG_MOD_REPLACE:
			if (a && a->flags & LDB_ATTR_FLAG_SINGLE_VALUE) {
				if (el->num_values > 1) {
					ldb_asprintf_errstring(ldb, "SINGLE-VALUE attribute %s on %s speicified more than once", 
							       el->name, ldb_dn_get_linearized(msg->dn));
					return LDB_ERR_CONSTRAINT_VIOLATION;
				}
			}
			/* replace all elements of this attribute name with the elements
			   listed. The attribute not existing is not an error */
			msg_delete_attribute(module, ldb, msg2, el->name);

			for (j=0;j<el->num_values;j++) {
				if (ldb_msg_find_val(el, &el->values[j]) != &el->values[j]) {
					ldb_asprintf_errstring(ldb, "%s: value #%d provided more than once", el->name, j);
					ret = LDB_ERR_ATTRIBUTE_OR_VALUE_EXISTS;
					goto failed;
				}
			}

			/* add the replacement element, if not empty */
			if (el->num_values != 0 &&
			    msg_add_element(ldb, msg2, el) != 0) {
				ret = LDB_ERR_OTHER;
				goto failed;
			}
			break;

		case LDB_FLAG_MOD_DELETE:

			dn = ldb_dn_get_linearized(msg->dn);
			if (dn == NULL) {
				ret = LDB_ERR_OTHER;
				goto failed;
			}

			/* we could be being asked to delete all
			   values or just some values */
			if (msg->elements[i].num_values == 0) {
				if (msg_delete_attribute(module, ldb, msg2, 
							 msg->elements[i].name) != 0) {
					ldb_asprintf_errstring(ldb, "No such attribute: %s for delete on %s", msg->elements[i].name, dn);
					ret = LDB_ERR_NO_SUCH_ATTRIBUTE;
					goto failed;
				}
				break;
			}
			for (j=0;j<msg->elements[i].num_values;j++) {
				if (msg_delete_element(module,
						       msg2, 
						       msg->elements[i].name,
						       &msg->elements[i].values[j]) != 0) {
					ldb_asprintf_errstring(ldb, "No matching attribute value when deleting attribute: %s on %s", msg->elements[i].name, dn);
					ret = LDB_ERR_NO_SUCH_ATTRIBUTE;
					goto failed;
				}
				ret = ltdb_index_del_value(module, dn, &msg->elements[i], j);
				if (ret != LDB_SUCCESS) {
					goto failed;
				}
			}
			break;
		default:
			ldb_asprintf_errstring(ldb,
				"Invalid ldb_modify flags on %s: 0x%x",
				msg->elements[i].name,
				msg->elements[i].flags & LDB_FLAG_MOD_MASK);
			ret = LDB_ERR_PROTOCOL_ERROR;
			goto failed;
		}
	}

	/* we've made all the mods
	 * save the modified record back into the database */
	ret = ltdb_store(module, msg2, TDB_MODIFY);
	if (ret != LDB_SUCCESS) {
		goto failed;
	}

	ret = ltdb_modified(module, msg->dn);
	if (ret != LDB_SUCCESS) {
		goto failed;
	}

	talloc_free(tdb_key.dptr);
	free(tdb_data.dptr);
	return ret;

failed:
	talloc_free(tdb_key.dptr);
	free(tdb_data.dptr);
	return ret;
}

/*
  modify a record
*/
static int ltdb_modify(struct ltdb_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	int tret;

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	tret = ltdb_check_special_dn(module, req->op.mod.message);
	if (tret != LDB_SUCCESS) {
		return tret;
	}

	if (ltdb_cache_load(module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	tret = ltdb_modify_internal(module, req->op.mod.message);
	if (tret != LDB_SUCCESS) {
		return tret;
	}

	return LDB_SUCCESS;
}

/*
  rename a record
*/
static int ltdb_rename(struct ltdb_context *ctx)
{
	struct ldb_module *module = ctx->module;
	struct ldb_request *req = ctx->req;
	struct ldb_message *msg;
	int tret;

	ldb_request_set_state(req, LDB_ASYNC_PENDING);

	if (ltdb_cache_load(ctx->module) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg = talloc(ctx, struct ldb_message);
	if (msg == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* in case any attribute of the message was indexed, we need
	   to fetch the old record */
	tret = ltdb_search_dn1(module, req->op.rename.olddn, msg);
	if (tret != LDB_SUCCESS) {
		/* not finding the old record is an error */
		return tret;
	}

	msg->dn = ldb_dn_copy(msg, req->op.rename.newdn);
	if (!msg->dn) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Always delete first then add, to avoid conflicts with
	 * unique indexes. We rely on the transaction to make this
	 * atomic
	 */
	tret = ltdb_delete_internal(module, req->op.rename.olddn);
	if (tret != LDB_SUCCESS) {
		return tret;
	}

	tret = ltdb_add_internal(module, msg);
	if (tret != LDB_SUCCESS) {
		return tret;
	}

	return LDB_SUCCESS;
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
	TALLOC_CTX *tmp_ctx;
	struct ldb_seqnum_request *seq;
	struct ldb_seqnum_result *res;
	struct ldb_message *msg = NULL;
	struct ldb_dn *dn;
	const char *date;
	int ret;

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

	msg = talloc(tmp_ctx, struct ldb_message);
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

	ret = LDB_SUCCESS;

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
		ret = LDB_ERR_UNWILLING_TO_PERFORM;
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
	struct ldb_context *ldb;
	struct tevent_context *ev;
	struct ltdb_context *ac;
	struct tevent_timer *te;
	struct timeval tv;

	if (check_critical_controls(req->controls)) {
		return LDB_ERR_UNSUPPORTED_CRITICAL_EXTENSION;
	}

	ldb = ldb_module_get_ctx(module);

	if (req->starttime == 0 || req->timeout == 0) {
		ldb_set_errstring(ldb, "Invalid timeout settings");
		return LDB_ERR_TIME_LIMIT_EXCEEDED;
	}

	ev = ldb_get_event_context(ldb);

	ac = talloc_zero(ldb, struct ltdb_context);
	if (ac == NULL) {
		ldb_set_errstring(ldb, "Out of Memory");
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

static const struct ldb_module_ops ltdb_ops = {
	.name              = "tdb",
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
			return -1;
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
		return -1;
	}

	/* note that we use quite a large default hash size */
	ltdb->tdb = ltdb_wrap_open(ltdb, path, 10000,
				   tdb_flags, open_flags,
				   ldb_get_create_perms(ldb), ldb);
	if (!ltdb->tdb) {
		ldb_debug(ldb, LDB_DEBUG_ERROR,
			  "Unable to open tdb '%s'", path);
		talloc_free(ltdb);
		return -1;
	}

	ltdb->sequence_number = 0;

	module = ldb_module_new(ldb, ldb, "ldb_tdb backend", &ltdb_ops);
	if (!module) {
		talloc_free(ltdb);
		return -1;
	}
	ldb_module_set_private(module, ltdb);

	if (ltdb_cache_load(module) != 0) {
		talloc_free(module);
		talloc_free(ltdb);
		return -1;
	}

	*_module = module;
	return 0;
}

const struct ldb_backend_ops ldb_tdb_backend_ops = {
	.name = "tdb",
	.connect_fn = ltdb_connect
};
