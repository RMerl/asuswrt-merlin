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
 *  Component: ldb message component utility functions
 *
 *  Description: functions for manipulating ldb_message structures
 *
 *  Author: Andrew Tridgell
 */

#include "includes.h"
#include "ldb/include/includes.h"

void ldb_dump_results(struct ldb_context *ldb, struct ldb_result *result, FILE *f);
int ldb_msg_element_compare_name(struct ldb_message_element *el1, 
				 struct ldb_message_element *el2);

/*
  create a new ldb_message in a given memory context (NULL for top level)
*/
struct ldb_message *ldb_msg_new(void *mem_ctx)
{
	return talloc_zero(mem_ctx, struct ldb_message);
}

/*
  find an element in a message by attribute name
*/
struct ldb_message_element *ldb_msg_find_element(const struct ldb_message *msg, 
						 const char *attr_name)
{
	unsigned int i;
	for (i=0;i<msg->num_elements;i++) {
		if (ldb_attr_cmp(msg->elements[i].name, attr_name) == 0) {
			return &msg->elements[i];
		}
	}
	return NULL;
}

/*
  see if two ldb_val structures contain exactly the same data
  return 1 for a match, 0 for a mis-match
*/
int ldb_val_equal_exact(const struct ldb_val *v1, const struct ldb_val *v2)
{
	if (v1->length != v2->length) return 0;

	if (v1->length == 0) return 1;

	if (memcmp(v1->data, v2->data, v1->length) == 0) {
		return 1;
	}

	return 0;
}

/*
  find a value in an element
  assumes case sensitive comparison
*/
struct ldb_val *ldb_msg_find_val(const struct ldb_message_element *el, 
				 struct ldb_val *val)
{
	unsigned int i;
	for (i=0;i<el->num_values;i++) {
		if (ldb_val_equal_exact(val, &el->values[i])) {
			return &el->values[i];
		}
	}
	return NULL;
}

/*
  duplicate a ldb_val structure
*/
struct ldb_val ldb_val_dup(void *mem_ctx, const struct ldb_val *v)
{
	struct ldb_val v2;
	v2.length = v->length;
	if (v->data == NULL) {
		v2.data = NULL;
		return v2;
	}

	/* the +1 is to cope with buggy C library routines like strndup
	   that look one byte beyond */
	v2.data = talloc_array(mem_ctx, uint8_t, v->length+1);
	if (!v2.data) {
		v2.length = 0;
		return v2;
	}

	memcpy(v2.data, v->data, v->length);
	((char *)v2.data)[v->length] = 0;
	return v2;
}

/*
  add an empty element to a message
*/
int ldb_msg_add_empty(	struct ldb_message *msg,
			const char *attr_name,
			int flags,
			struct ldb_message_element **return_el)
{
	struct ldb_message_element *els;

	if (! ldb_valid_attr_name(attr_name)) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	els = talloc_realloc(msg, msg->elements, 
			     struct ldb_message_element, msg->num_elements+1);
	if (!els) {
		errno = ENOMEM;
		return LDB_ERR_OPERATIONS_ERROR;
	}

	els[msg->num_elements].values = NULL;
	els[msg->num_elements].num_values = 0;
	els[msg->num_elements].flags = flags;
	els[msg->num_elements].name = talloc_strdup(els, attr_name);
	if (!els[msg->num_elements].name) {
		errno = ENOMEM;
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->elements = els;
	msg->num_elements++;

	if (return_el) {
		*return_el = &els[msg->num_elements-1];
	}

	return LDB_SUCCESS;
}

/*
  add an empty element to a message
*/
int ldb_msg_add(struct ldb_message *msg, 
		const struct ldb_message_element *el, 
		int flags)
{
	if (ldb_msg_add_empty(msg, el->name, flags, NULL) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->elements[msg->num_elements-1] = *el;
	msg->elements[msg->num_elements-1].flags = flags;

	return LDB_SUCCESS;
}

/*
  add a value to a message
*/
int ldb_msg_add_value(struct ldb_message *msg, 
		      const char *attr_name,
		      const struct ldb_val *val,
		      struct ldb_message_element **return_el)
{
	struct ldb_message_element *el;
	struct ldb_val *vals;
	int ret;

	el = ldb_msg_find_element(msg, attr_name);
	if (!el) {
		ret = ldb_msg_add_empty(msg, attr_name, 0, &el);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	vals = talloc_realloc(msg, el->values, struct ldb_val, el->num_values+1);
	if (!vals) {
		errno = ENOMEM;
		return LDB_ERR_OPERATIONS_ERROR;
	}
	el->values = vals;
	el->values[el->num_values] = *val;
	el->num_values++;

	if (return_el) {
		*return_el = el;
	}

	return LDB_SUCCESS;
}


/*
  add a value to a message, stealing it into the 'right' place
*/
int ldb_msg_add_steal_value(struct ldb_message *msg, 
			    const char *attr_name,
			    struct ldb_val *val)
{
	int ret;
	struct ldb_message_element *el;

	ret = ldb_msg_add_value(msg, attr_name, val, &el);
	if (ret == LDB_SUCCESS) {
		talloc_steal(el->values, val->data);
	}
	return ret;
}


/*
  add a string element to a message
*/
int ldb_msg_add_string(struct ldb_message *msg, 
		       const char *attr_name, const char *str)
{
	struct ldb_val val;

	val.data = discard_const_p(uint8_t, str);
	val.length = strlen(str);

	if (val.length == 0) {
		/* allow empty strings as non-existant attributes */
		return LDB_SUCCESS;
	}

	return ldb_msg_add_value(msg, attr_name, &val, NULL);
}

/*
  add a string element to a message, stealing it into the 'right' place
*/
int ldb_msg_add_steal_string(struct ldb_message *msg, 
			     const char *attr_name, char *str)
{
	struct ldb_val val;

	val.data = (uint8_t *)str;
	val.length = strlen(str);

	return ldb_msg_add_steal_value(msg, attr_name, &val);
}

/*
  add a printf formatted element to a message
*/
int ldb_msg_add_fmt(struct ldb_message *msg, 
		    const char *attr_name, const char *fmt, ...)
{
	struct ldb_val val;
	va_list ap;
	char *str;

	va_start(ap, fmt);
	str = talloc_vasprintf(msg, fmt, ap);
	va_end(ap);

	if (str == NULL) return LDB_ERR_OPERATIONS_ERROR;

	val.data   = (uint8_t *)str;
	val.length = strlen(str);

	return ldb_msg_add_steal_value(msg, attr_name, &val);
}

/*
  compare two ldb_message_element structures
  assumes case senistive comparison
*/
int ldb_msg_element_compare(struct ldb_message_element *el1, 
			    struct ldb_message_element *el2)
{
	unsigned int i;

	if (el1->num_values != el2->num_values) {
		return el1->num_values - el2->num_values;
	}

	for (i=0;i<el1->num_values;i++) {
		if (!ldb_msg_find_val(el2, &el1->values[i])) {
			return -1;
		}
	}

	return 0;
}

/*
  compare two ldb_message_element structures
  comparing by element name
*/
int ldb_msg_element_compare_name(struct ldb_message_element *el1, 
				 struct ldb_message_element *el2)
{
	return ldb_attr_cmp(el1->name, el2->name);
}

/*
  convenience functions to return common types from a message
  these return the first value if the attribute is multi-valued
*/
const struct ldb_val *ldb_msg_find_ldb_val(const struct ldb_message *msg, const char *attr_name)
{
	struct ldb_message_element *el = ldb_msg_find_element(msg, attr_name);
	if (!el || el->num_values == 0) {
		return NULL;
	}
	return &el->values[0];
}

int ldb_msg_find_attr_as_int(const struct ldb_message *msg, 
			     const char *attr_name,
			     int default_value)
{
	const struct ldb_val *v = ldb_msg_find_ldb_val(msg, attr_name);
	if (!v || !v->data) {
		return default_value;
	}
	return strtol((const char *)v->data, NULL, 0);
}

unsigned int ldb_msg_find_attr_as_uint(const struct ldb_message *msg, 
				       const char *attr_name,
				       unsigned int default_value)
{
	const struct ldb_val *v = ldb_msg_find_ldb_val(msg, attr_name);
	if (!v || !v->data) {
		return default_value;
	}
	return strtoul((const char *)v->data, NULL, 0);
}

int64_t ldb_msg_find_attr_as_int64(const struct ldb_message *msg, 
				   const char *attr_name,
				   int64_t default_value)
{
	const struct ldb_val *v = ldb_msg_find_ldb_val(msg, attr_name);
	if (!v || !v->data) {
		return default_value;
	}
	return strtoll((const char *)v->data, NULL, 0);
}

uint64_t ldb_msg_find_attr_as_uint64(const struct ldb_message *msg, 
				     const char *attr_name,
				     uint64_t default_value)
{
	const struct ldb_val *v = ldb_msg_find_ldb_val(msg, attr_name);
	if (!v || !v->data) {
		return default_value;
	}
	return strtoull((const char *)v->data, NULL, 0);
}

double ldb_msg_find_attr_as_double(const struct ldb_message *msg, 
				   const char *attr_name,
				   double default_value)
{
	const struct ldb_val *v = ldb_msg_find_ldb_val(msg, attr_name);
	if (!v || !v->data) {
		return default_value;
	}
	return strtod((const char *)v->data, NULL);
}

int ldb_msg_find_attr_as_bool(const struct ldb_message *msg, 
			      const char *attr_name,
			      int default_value)
{
	const struct ldb_val *v = ldb_msg_find_ldb_val(msg, attr_name);
	if (!v || !v->data) {
		return default_value;
	}
	if (strcasecmp((const char *)v->data, "FALSE") == 0) {
		return 0;
	}
	if (strcasecmp((const char *)v->data, "TRUE") == 0) {
		return 1;
	}
	return default_value;
}

const char *ldb_msg_find_attr_as_string(const struct ldb_message *msg, 
					const char *attr_name,
					const char *default_value)
{
	const struct ldb_val *v = ldb_msg_find_ldb_val(msg, attr_name);
	if (!v || !v->data) {
		return default_value;
	}
	return (const char *)v->data;
}

struct ldb_dn *ldb_msg_find_attr_as_dn(void *mem_ctx,
				       const struct ldb_message *msg,
				       const char *attr_name)
{
	const struct ldb_val *v;

	v = ldb_msg_find_ldb_val(msg, attr_name);
	if (!v || !v->data) {
		return NULL;
	}
	return ldb_dn_explode(mem_ctx, (const char *)v->data);
}

/*
  sort the elements of a message by name
*/
void ldb_msg_sort_elements(struct ldb_message *msg)
{
	qsort(msg->elements, msg->num_elements, sizeof(struct ldb_message_element), 
	      (comparison_fn_t)ldb_msg_element_compare_name);
}

/*
  shallow copy a message - copying only the elements array so that the caller
  can safely add new elements without changing the message
*/
struct ldb_message *ldb_msg_copy_shallow(TALLOC_CTX *mem_ctx, 
					 const struct ldb_message *msg)
{
	struct ldb_message *msg2;
	int i;

	msg2 = talloc(mem_ctx, struct ldb_message);
	if (msg2 == NULL) return NULL;

	*msg2 = *msg;
	msg2->private_data = NULL;

	msg2->elements = talloc_array(msg2, struct ldb_message_element, 
				      msg2->num_elements);
	if (msg2->elements == NULL) goto failed;

	for (i=0;i<msg2->num_elements;i++) {
		msg2->elements[i] = msg->elements[i];
	}

	return msg2;

failed:
	talloc_free(msg2);
	return NULL;
}


/*
  copy a message, allocating new memory for all parts
*/
struct ldb_message *ldb_msg_copy(TALLOC_CTX *mem_ctx, 
				 const struct ldb_message *msg)
{
	struct ldb_message *msg2;
	int i, j;

	msg2 = ldb_msg_copy_shallow(mem_ctx, msg);
	if (msg2 == NULL) return NULL;

	msg2->dn = ldb_dn_copy(msg2, msg2->dn);
	if (msg2->dn == NULL) goto failed;

	for (i=0;i<msg2->num_elements;i++) {
		struct ldb_message_element *el = &msg2->elements[i];
		struct ldb_val *values = el->values;
		el->name = talloc_strdup(msg2->elements, el->name);
		if (el->name == NULL) goto failed;
		el->values = talloc_array(msg2->elements, struct ldb_val, el->num_values);
		for (j=0;j<el->num_values;j++) {
			el->values[j] = ldb_val_dup(el->values, &values[j]);
			if (el->values[j].data == NULL && values[j].length != 0) {
				goto failed;
			}
		}
	}

	return msg2;

failed:
	talloc_free(msg2);
	return NULL;
}


/*
  canonicalise a message, merging elements of the same name
*/
struct ldb_message *ldb_msg_canonicalize(struct ldb_context *ldb, 
					 const struct ldb_message *msg)
{
	int i;
	struct ldb_message *msg2;

	msg2 = ldb_msg_copy(ldb, msg);
	if (msg2 == NULL) return NULL;

	ldb_msg_sort_elements(msg2);

	for (i=1;i<msg2->num_elements;i++) {
		struct ldb_message_element *el1 = &msg2->elements[i-1];
		struct ldb_message_element *el2 = &msg2->elements[i];
		if (ldb_msg_element_compare_name(el1, el2) == 0) {
			el1->values = talloc_realloc(msg2->elements, el1->values, struct ldb_val, 
						       el1->num_values + el2->num_values);
			if (el1->values == NULL) {
				return NULL;
			}
			memcpy(el1->values + el1->num_values,
			       el2->values,
			       sizeof(struct ldb_val) * el2->num_values);
			el1->num_values += el2->num_values;
			talloc_free(discard_const_p(char, el2->name));
			if (i+1<msg2->num_elements) {
				memmove(el2, el2+1, sizeof(struct ldb_message_element) * 
					(msg2->num_elements - (i+1)));
			}
			msg2->num_elements--;
			i--;
		}
	}

	return msg2;
}


/*
  return a ldb_message representing the differences between msg1 and msg2. If you
  then use this in a ldb_modify() call it can be used to save edits to a message
*/
struct ldb_message *ldb_msg_diff(struct ldb_context *ldb, 
				 struct ldb_message *msg1,
				 struct ldb_message *msg2)
{
	struct ldb_message *mod;
	struct ldb_message_element *el;
	unsigned int i;

	mod = ldb_msg_new(ldb);

	mod->dn = msg1->dn;
	mod->num_elements = 0;
	mod->elements = NULL;

	msg2 = ldb_msg_canonicalize(ldb, msg2);
	if (msg2 == NULL) {
		return NULL;
	}
	
	/* look in msg2 to find elements that need to be added
	   or modified */
	for (i=0;i<msg2->num_elements;i++) {
		el = ldb_msg_find_element(msg1, msg2->elements[i].name);

		if (el && ldb_msg_element_compare(el, &msg2->elements[i]) == 0) {
			continue;
		}

		if (ldb_msg_add(mod, 
				&msg2->elements[i],
				el?LDB_FLAG_MOD_REPLACE:LDB_FLAG_MOD_ADD) != 0) {
			return NULL;
		}
	}

	/* look in msg1 to find elements that need to be deleted */
	for (i=0;i<msg1->num_elements;i++) {
		el = ldb_msg_find_element(msg2, msg1->elements[i].name);
		if (!el) {
			if (ldb_msg_add_empty(mod, 
					      msg1->elements[i].name,
					      LDB_FLAG_MOD_DELETE, NULL) != 0) {
				return NULL;
			}
		}
	}

	return mod;
}

int ldb_msg_sanity_check(struct ldb_context *ldb, 
			 const struct ldb_message *msg)
{
	int i, j;

	/* basic check on DN */
	if (msg->dn == NULL) {
		/* TODO: return also an error string */
		ldb_set_errstring(ldb, "ldb message lacks a DN!");
		return LDB_ERR_INVALID_DN_SYNTAX;
	}

	/* basic syntax checks */
	for (i = 0; i < msg->num_elements; i++) {
		for (j = 0; j < msg->elements[i].num_values; j++) {
			if (msg->elements[i].values[j].length == 0) {
				TALLOC_CTX *mem_ctx = talloc_new(ldb);
				/* an attribute cannot be empty */
				/* TODO: return also an error string */
				ldb_asprintf_errstring(ldb, "Element %s has empty attribute in ldb message (%s)!",
							    msg->elements[i].name, 
							    ldb_dn_linearize(mem_ctx, msg->dn));
				talloc_free(mem_ctx);
				return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
			}
		}
	}

	return LDB_SUCCESS;
}




/*
  copy an attribute list. This only copies the array, not the elements
  (ie. the elements are left as the same pointers)
*/
const char **ldb_attr_list_copy(TALLOC_CTX *mem_ctx, const char * const *attrs)
{
	const char **ret;
	int i;
	for (i=0;attrs[i];i++) /* noop */ ;
	ret = talloc_array(mem_ctx, const char *, i+1);
	if (ret == NULL) {
		return NULL;
	}
	for (i=0;attrs[i];i++) {
		ret[i] = attrs[i];
	}
	ret[i] = attrs[i];
	return ret;
}


/*
  copy an attribute list. This only copies the array, not the elements
  (ie. the elements are left as the same pointers).  The new attribute is added to the list.
*/
const char **ldb_attr_list_copy_add(TALLOC_CTX *mem_ctx, const char * const *attrs, const char *new_attr)
{
	const char **ret;
	int i;
	for (i=0;attrs[i];i++) /* noop */ ;
	ret = talloc_array(mem_ctx, const char *, i+2);
	if (ret == NULL) {
		return NULL;
	}
	for (i=0;attrs[i];i++) {
		ret[i] = attrs[i];
	}
	ret[i] = new_attr;
	ret[i+1] = NULL;
	return ret;
}


/*
  return 1 if an attribute is in a list of attributes, or 0 otherwise
*/
int ldb_attr_in_list(const char * const *attrs, const char *attr)
{
	int i;
	for (i=0;attrs[i];i++) {
		if (ldb_attr_cmp(attrs[i], attr) == 0) {
			return 1;
		}
	}
	return 0;
}


/*
  rename the specified attribute in a search result
*/
int ldb_msg_rename_attr(struct ldb_message *msg, const char *attr, const char *replace)
{
	struct ldb_message_element *el = ldb_msg_find_element(msg, attr);
	if (el == NULL) {
		return LDB_SUCCESS;
	}
	el->name = talloc_strdup(msg->elements, replace);
	if (el->name == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	return LDB_SUCCESS;
}


/*
  copy the specified attribute in a search result to a new attribute
*/
int ldb_msg_copy_attr(struct ldb_message *msg, const char *attr, const char *replace)
{
	struct ldb_message_element *el = ldb_msg_find_element(msg, attr);
	if (el == NULL) {
		return LDB_SUCCESS;
	}
	if (ldb_msg_add(msg, el, 0) != 0) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	return ldb_msg_rename_attr(msg, attr, replace);
}


/*
  remove the specified attribute in a search result
*/
void ldb_msg_remove_attr(struct ldb_message *msg, const char *attr)
{
	struct ldb_message_element *el = ldb_msg_find_element(msg, attr);
	if (el) {
		int n = (el - msg->elements);
		if (n != msg->num_elements-1) {
			memmove(el, el+1, ((msg->num_elements-1) - n)*sizeof(*el));
		}
		msg->num_elements--;
	}
}

/*
  remove the specified element in a search result
*/
void ldb_msg_remove_element(struct ldb_message *msg, struct ldb_message_element *el)
{
	int n = (el - msg->elements);
	if (n != msg->num_elements-1) {
		memmove(el, el+1, ((msg->num_elements-1) - n)*sizeof(*el));
	}
	msg->num_elements--;
}

/*
  return a LDAP formatted time string
*/
char *ldb_timestring(TALLOC_CTX *mem_ctx, time_t t)
{
	struct tm *tm = gmtime(&t);

	if (!tm) {
		return NULL;
	}

	/* formatted like: 20040408072012.0Z */
	return talloc_asprintf(mem_ctx, 
			       "%04u%02u%02u%02u%02u%02u.0Z",
			       tm->tm_year+1900, tm->tm_mon+1,
			       tm->tm_mday, tm->tm_hour, tm->tm_min,
			       tm->tm_sec);
}


/*
  convert a LDAP time string to a time_t. Return 0 if unable to convert
*/
time_t ldb_string_to_time(const char *s)
{
	struct tm tm;
	
	if (s == NULL) return 0;
	
	memset(&tm, 0, sizeof(tm));
	if (sscanf(s, "%04u%02u%02u%02u%02u%02u", 
		   &tm.tm_year, &tm.tm_mon, &tm.tm_mday, 
		   &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
		return 0;
	}
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	
	return timegm(&tm);
}


/*
  dump a set of results to a file. Useful from within gdb
*/
void ldb_dump_results(struct ldb_context *ldb, struct ldb_result *result, FILE *f)
{
	int i;

	for (i = 0; i < result->count; i++) {
		struct ldb_ldif ldif;
		fprintf(f, "# record %d\n", i+1);
		ldif.changetype = LDB_CHANGETYPE_NONE;
		ldif.msg = result->msgs[i];
		ldb_ldif_write_file(ldb, f, &ldif);
	}
}

int ldb_msg_check_string_attribute(const struct ldb_message *msg, const char *name, const char *value)
{
	struct ldb_message_element *el;
	struct ldb_val val;
	
	el = ldb_msg_find_element(msg, name);
	if (el == NULL)
		return 0;

	val.data = discard_const_p(uint8_t, value);
	val.length = strlen(value);

	if (ldb_msg_find_val(el, &val))
		return 1;

	return 0;
}
