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
 *  Component: ldb tdb cache functions
 *
 *  Description: cache special records in a ldb/tdb
 *
 *  Author: Andrew Tridgell
 */

#include "includes.h"
#include "ldb/include/includes.h"

#include "ldb/ldb_tdb/ldb_tdb.h"

#define LTDB_FLAG_CASE_INSENSITIVE (1<<0)
#define LTDB_FLAG_INTEGER          (1<<1)
#define LTDB_FLAG_HIDDEN           (1<<2)
#define LTDB_FLAG_OBJECTCLASS      (1<<3)

int ltdb_attribute_flags(struct ldb_module *module, const char *attr_name);

/* valid attribute flags */
static const struct {
	const char *name;
	int value;
} ltdb_valid_attr_flags[] = {
	{ "CASE_INSENSITIVE", LTDB_FLAG_CASE_INSENSITIVE },
	{ "INTEGER", LTDB_FLAG_INTEGER },
	{ "HIDDEN", LTDB_FLAG_HIDDEN },
	{ "NONE", 0 },
	{ NULL, 0 }
};


/*
  de-register any special handlers for @ATTRIBUTES
*/
static void ltdb_attributes_unload(struct ldb_module *module)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	struct ldb_message *msg;
	int i;

	if (ltdb->cache->attributes == NULL) {
		/* no previously loaded attributes */
		return;
	}

	msg = ltdb->cache->attributes;
	for (i=0;i<msg->num_elements;i++) {
		ldb_remove_attrib_handler(module->ldb, msg->elements[i].name);
	}

	talloc_free(ltdb->cache->attributes);
	ltdb->cache->attributes = NULL;
}

/*
  add up the attrib flags for a @ATTRIBUTES element
*/
static int ltdb_attributes_flags(struct ldb_message_element *el, unsigned *v)
{
	int i;
	unsigned value = 0;
	for (i=0;i<el->num_values;i++) {
		int j;
		for (j=0;ltdb_valid_attr_flags[j].name;j++) {
			if (strcmp(ltdb_valid_attr_flags[j].name, 
				   (char *)el->values[i].data) == 0) {
				value |= ltdb_valid_attr_flags[j].value;
				break;
			}
		}
		if (ltdb_valid_attr_flags[j].name == NULL) {
			return -1;
		}
	}
	*v = value;
	return 0;
}

/*
  register any special handlers from @ATTRIBUTES
*/
static int ltdb_attributes_load(struct ldb_module *module)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	struct ldb_message *msg = ltdb->cache->attributes;
	struct ldb_dn *dn;
	int i;

	dn = ldb_dn_explode(module->ldb, LTDB_ATTRIBUTES);
	if (dn == NULL) goto failed;

	if (ltdb_search_dn1(module, dn, msg) == -1) {
		talloc_free(dn);
		goto failed;
	}
	talloc_free(dn);
	/* mapping these flags onto ldap 'syntaxes' isn't strictly correct,
	   but its close enough for now */
	for (i=0;i<msg->num_elements;i++) {
		unsigned flags;
		const char *syntax;
		const struct ldb_attrib_handler *h;
		struct ldb_attrib_handler h2;

		if (ltdb_attributes_flags(&msg->elements[i], &flags) != 0) {
			ldb_debug(module->ldb, LDB_DEBUG_ERROR, "Invalid @ATTRIBUTES element for '%s'\n", msg->elements[i].name);
			goto failed;
		}
		switch (flags & ~LTDB_FLAG_HIDDEN) {
		case 0:
			syntax = LDB_SYNTAX_OCTET_STRING;
			break;
		case LTDB_FLAG_CASE_INSENSITIVE:
			syntax = LDB_SYNTAX_DIRECTORY_STRING;
			break;
		case LTDB_FLAG_INTEGER:
			syntax = LDB_SYNTAX_INTEGER;
			break;
		default:
			ldb_debug(module->ldb, LDB_DEBUG_ERROR, 
				  "Invalid flag combination 0x%x for '%s' in @ATTRIBUTES\n",
				  flags, msg->elements[i].name);
			goto failed;
		}

		h = ldb_attrib_handler_syntax(module->ldb, syntax);
		if (h == NULL) {
			ldb_debug(module->ldb, LDB_DEBUG_ERROR, 
				  "Invalid attribute syntax '%s' for '%s' in @ATTRIBUTES\n",
				  syntax, msg->elements[i].name);
			goto failed;
		}
		h2 = *h;
		h2.attr = msg->elements[i].name;
		h2.flags |= LDB_ATTR_FLAG_ALLOCATED;
		if (ldb_set_attrib_handlers(module->ldb, &h2, 1) != 0) {
			goto failed;
		}
	}

	return 0;
failed:
	return -1;
}


/*
  register any subclasses from @SUBCLASSES
*/
static int ltdb_subclasses_load(struct ldb_module *module)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	struct ldb_message *msg = ltdb->cache->subclasses;
	struct ldb_dn *dn;
	int i, j;

	dn = ldb_dn_explode(module->ldb, LTDB_SUBCLASSES);
	if (dn == NULL) goto failed;

	if (ltdb_search_dn1(module, dn, msg) == -1) {
		talloc_free(dn);
		goto failed;
	}
	talloc_free(dn);

	for (i=0;i<msg->num_elements;i++) {
		struct ldb_message_element *el = &msg->elements[i];
		for (j=0;j<el->num_values;j++) {
			if (ldb_subclass_add(module->ldb, el->name, 
					     (char *)el->values[j].data) != 0) {
				goto failed;
			}
		}
	}

	return 0;
failed:
	return -1;
}


/*
  de-register any @SUBCLASSES
*/
static void ltdb_subclasses_unload(struct ldb_module *module)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	struct ldb_message *msg;
	int i;

	if (ltdb->cache->subclasses == NULL) {
		/* no previously loaded subclasses */
		return;
	}

	msg = ltdb->cache->subclasses;
	for (i=0;i<msg->num_elements;i++) {
		ldb_subclass_remove(module->ldb, msg->elements[i].name);
	}

	talloc_free(ltdb->cache->subclasses);
	ltdb->cache->subclasses = NULL;
}


/*
  initialise the baseinfo record
*/
static int ltdb_baseinfo_init(struct ldb_module *module)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	struct ldb_message *msg;
	struct ldb_message_element el;
	struct ldb_val val;
	int ret;
	/* the initial sequence number must be different from the one
	   set in ltdb_cache_free(). Thanks to Jon for pointing this
	   out. */
	const char *initial_sequence_number = "1";

	ltdb->sequence_number = atof(initial_sequence_number);

	msg = talloc(ltdb, struct ldb_message);
	if (msg == NULL) {
		goto failed;
	}

	msg->num_elements = 1;
	msg->elements = &el;
	msg->dn = ldb_dn_explode(msg, LTDB_BASEINFO);
	if (!msg->dn) {
		goto failed;
	}
	el.name = talloc_strdup(msg, LTDB_SEQUENCE_NUMBER);
	if (!el.name) {
		goto failed;
	}
	el.values = &val;
	el.num_values = 1;
	el.flags = 0;
	val.data = (uint8_t *)talloc_strdup(msg, initial_sequence_number);
	if (!val.data) {
		goto failed;
	}
	val.length = 1;
	
	ret = ltdb_store(module, msg, TDB_INSERT);

	talloc_free(msg);

	return ret;

failed:
	talloc_free(msg);
	errno = ENOMEM;
	return -1;
}

/*
  free any cache records
 */
static void ltdb_cache_free(struct ldb_module *module)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;

	ltdb->sequence_number = 0;
	talloc_free(ltdb->cache);
	ltdb->cache = NULL;
}

/*
  force a cache reload
*/
int ltdb_cache_reload(struct ldb_module *module)
{
	ltdb_attributes_unload(module);
	ltdb_subclasses_unload(module);
	ltdb_cache_free(module);
	return ltdb_cache_load(module);
}

/*
  load the cache records
*/
int ltdb_cache_load(struct ldb_module *module)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	struct ldb_dn *baseinfo_dn = NULL;
	struct ldb_dn *indexlist_dn = NULL;
	uint64_t seq;
	struct ldb_message *baseinfo = NULL;

	/* a very fast check to avoid extra database reads */
	if (ltdb->cache != NULL && 
	    tdb_get_seqnum(ltdb->tdb) == ltdb->tdb_seqnum) {
		return 0;
	}

	if (ltdb->cache == NULL) {
		ltdb->cache = talloc_zero(ltdb, struct ltdb_cache);
		if (ltdb->cache == NULL) goto failed;
		ltdb->cache->indexlist = talloc_zero(ltdb->cache, struct ldb_message);
		ltdb->cache->subclasses = talloc_zero(ltdb->cache, struct ldb_message);
		ltdb->cache->attributes = talloc_zero(ltdb->cache, struct ldb_message);
		if (ltdb->cache->indexlist == NULL ||
		    ltdb->cache->subclasses == NULL ||
		    ltdb->cache->attributes == NULL) {
			goto failed;
		}
	}

	baseinfo = talloc(ltdb->cache, struct ldb_message);
	if (baseinfo == NULL) goto failed;

	baseinfo_dn = ldb_dn_explode(module->ldb, LTDB_BASEINFO);
	if (baseinfo_dn == NULL) goto failed;

	if (ltdb_search_dn1(module, baseinfo_dn, baseinfo) == -1) {
		goto failed;
	}
	
	/* possibly initialise the baseinfo */
	if (!baseinfo->dn) {
		if (ltdb_baseinfo_init(module) != 0) {
			goto failed;
		}
		if (ltdb_search_dn1(module, baseinfo_dn, baseinfo) != 1) {
			goto failed;
		}
	}

	ltdb->tdb_seqnum = tdb_get_seqnum(ltdb->tdb);

	/* if the current internal sequence number is the same as the one
	   in the database then assume the rest of the cache is OK */
	seq = ldb_msg_find_attr_as_uint64(baseinfo, LTDB_SEQUENCE_NUMBER, 0);
	if (seq == ltdb->sequence_number) {
		goto done;
	}
	ltdb->sequence_number = seq;

	talloc_free(ltdb->cache->last_attribute.name);
	memset(&ltdb->cache->last_attribute, 0, sizeof(ltdb->cache->last_attribute));

	ltdb_attributes_unload(module);
	ltdb_subclasses_unload(module);

	talloc_free(ltdb->cache->indexlist);
	talloc_free(ltdb->cache->subclasses);

	ltdb->cache->indexlist = talloc_zero(ltdb->cache, struct ldb_message);
	ltdb->cache->subclasses = talloc_zero(ltdb->cache, struct ldb_message);
	ltdb->cache->attributes = talloc_zero(ltdb->cache, struct ldb_message);
	if (ltdb->cache->indexlist == NULL ||
	    ltdb->cache->subclasses == NULL ||
	    ltdb->cache->attributes == NULL) {
		goto failed;
	}
	    
	indexlist_dn = ldb_dn_explode(module->ldb, LTDB_INDEXLIST);
	if (indexlist_dn == NULL) goto failed;

	if (ltdb_search_dn1(module, indexlist_dn, ltdb->cache->indexlist) == -1) {
		goto failed;
	}

	if (ltdb_attributes_load(module) == -1) {
		goto failed;
	}
	if (ltdb_subclasses_load(module) == -1) {
		goto failed;
	}

done:
	talloc_free(baseinfo);
	talloc_free(baseinfo_dn);
	talloc_free(indexlist_dn);
	return 0;

failed:
	talloc_free(baseinfo);
	talloc_free(baseinfo_dn);
	talloc_free(indexlist_dn);
	return -1;
}


/*
  increase the sequence number to indicate a database change
*/
int ltdb_increase_sequence_number(struct ldb_module *module)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	struct ldb_message *msg;
	struct ldb_message_element el[2];
	struct ldb_val val;
	struct ldb_val val_time;
	time_t t = time(NULL);
	char *s = NULL;
	int ret;

	msg = talloc(ltdb, struct ldb_message);
	if (msg == NULL) {
		errno = ENOMEM;
		return -1;
	}

	s = talloc_asprintf(msg, "%llu", ltdb->sequence_number+1);
	if (!s) {
		errno = ENOMEM;
		return -1;
	}

	msg->num_elements = ARRAY_SIZE(el);
	msg->elements = el;
	msg->dn = ldb_dn_explode(msg, LTDB_BASEINFO);
	if (msg->dn == NULL) {
		talloc_free(msg);
		errno = ENOMEM;
		return -1;
	}
	el[0].name = talloc_strdup(msg, LTDB_SEQUENCE_NUMBER);
	if (el[0].name == NULL) {
		talloc_free(msg);
		errno = ENOMEM;
		return -1;
	}
	el[0].values = &val;
	el[0].num_values = 1;
	el[0].flags = LDB_FLAG_MOD_REPLACE;
	val.data = (uint8_t *)s;
	val.length = strlen(s);

	el[1].name = talloc_strdup(msg, LTDB_MOD_TIMESTAMP);
	if (el[1].name == NULL) {
		talloc_free(msg);
		errno = ENOMEM;
		return -1;
	}
	el[1].values = &val_time;
	el[1].num_values = 1;
	el[1].flags = LDB_FLAG_MOD_REPLACE;

	s = ldb_timestring(msg, t);
	if (s == NULL) {
		return -1;
	}

	val_time.data = (uint8_t *)s;
	val_time.length = strlen(s);

	ret = ltdb_modify_internal(module, msg);

	talloc_free(msg);

	if (ret == 0) {
		ltdb->sequence_number += 1;
	}

	return ret;
}


/*
  return the attribute flags from the @ATTRIBUTES record 
  for the given attribute
*/
int ltdb_attribute_flags(struct ldb_module *module, const char *attr_name)
{
	struct ltdb_private *ltdb =
		(struct ltdb_private *)module->private_data;
	const struct ldb_message_element *attr_el;
	int i, j, ret=0;

	if (ltdb->cache->last_attribute.name &&
	    ldb_attr_cmp(ltdb->cache->last_attribute.name, attr_name) == 0) {
		return ltdb->cache->last_attribute.flags;
	}

	/* objectclass is a special default case */
	if (ldb_attr_cmp(attr_name, LTDB_OBJECTCLASS) == 0) {
		ret = LTDB_FLAG_OBJECTCLASS | LTDB_FLAG_CASE_INSENSITIVE;
	}

	attr_el = ldb_msg_find_element(ltdb->cache->attributes, attr_name);

	if (!attr_el) {
		/* check if theres a wildcard attribute */
		attr_el = ldb_msg_find_element(ltdb->cache->attributes, "*");

		if (!attr_el) {
			return ret;
		}
	}

	for (i = 0; i < attr_el->num_values; i++) {
		for (j=0; ltdb_valid_attr_flags[j].name; j++) {
			if (strcmp(ltdb_valid_attr_flags[j].name, 
				   (char *)attr_el->values[i].data) == 0) {
				ret |= ltdb_valid_attr_flags[j].value;
			}
		}
	}

	talloc_free(ltdb->cache->last_attribute.name);

	ltdb->cache->last_attribute.name = talloc_strdup(ltdb->cache, attr_name);
	ltdb->cache->last_attribute.flags = ret;

	return ret;
}

int ltdb_check_at_attributes_values(const struct ldb_val *value)
{
	int i;

	for (i = 0; ltdb_valid_attr_flags[i].name != NULL; i++) {
		if ((strcmp(ltdb_valid_attr_flags[i].name, (char *)value->data) == 0)) {
			return 0;
		}
	}

	return -1;
}

