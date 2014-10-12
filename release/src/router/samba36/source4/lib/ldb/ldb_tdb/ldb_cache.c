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

#include "ldb_tdb.h"
#include "ldb_private.h"

#define LTDB_FLAG_CASE_INSENSITIVE (1<<0)
#define LTDB_FLAG_INTEGER          (1<<1)
#define LTDB_FLAG_HIDDEN           (1<<2)

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
	struct ldb_context *ldb;
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	struct ldb_message *msg;
	unsigned int i;

	ldb = ldb_module_get_ctx(module);

	if (ltdb->cache->attributes == NULL) {
		/* no previously loaded attributes */
		return;
	}

	msg = ltdb->cache->attributes;
	for (i=0;i<msg->num_elements;i++) {
		ldb_schema_attribute_remove(ldb, msg->elements[i].name);
	}

	talloc_free(ltdb->cache->attributes);
	ltdb->cache->attributes = NULL;
}

/*
  add up the attrib flags for a @ATTRIBUTES element
*/
static int ltdb_attributes_flags(struct ldb_message_element *el, unsigned *v)
{
	unsigned int i;
	unsigned value = 0;
	for (i=0;i<el->num_values;i++) {
		unsigned int j;
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
	struct ldb_context *ldb;
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	struct ldb_message *msg = ltdb->cache->attributes;
	struct ldb_dn *dn;
	unsigned int i;
	int r;

	ldb = ldb_module_get_ctx(module);

	if (ldb->schema.attribute_handler_override) {
		/* we skip loading the @ATTRIBUTES record when a module is supplying
		   its own attribute handling */
		return 0;
	}

	dn = ldb_dn_new(module, ldb, LTDB_ATTRIBUTES);
	if (dn == NULL) goto failed;

	r = ltdb_search_dn1(module, dn, msg);
	talloc_free(dn);
	if (r != LDB_SUCCESS && r != LDB_ERR_NO_SUCH_OBJECT) {
		goto failed;
	}
	if (r == LDB_ERR_NO_SUCH_OBJECT) {
		return 0;
	}
	/* mapping these flags onto ldap 'syntaxes' isn't strictly correct,
	   but its close enough for now */
	for (i=0;i<msg->num_elements;i++) {
		unsigned flags;
		const char *syntax;
		const struct ldb_schema_syntax *s;

		if (ltdb_attributes_flags(&msg->elements[i], &flags) != 0) {
			ldb_debug(ldb, LDB_DEBUG_ERROR, "Invalid @ATTRIBUTES element for '%s'", msg->elements[i].name);
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
			ldb_debug(ldb, LDB_DEBUG_ERROR, 
				  "Invalid flag combination 0x%x for '%s' in @ATTRIBUTES",
				  flags, msg->elements[i].name);
			goto failed;
		}

		s = ldb_standard_syntax_by_name(ldb, syntax);
		if (s == NULL) {
			ldb_debug(ldb, LDB_DEBUG_ERROR, 
				  "Invalid attribute syntax '%s' for '%s' in @ATTRIBUTES",
				  syntax, msg->elements[i].name);
			goto failed;
		}

		flags |= LDB_ATTR_FLAG_ALLOCATED;
		if (ldb_schema_attribute_add_with_syntax(ldb, msg->elements[i].name, flags, s) != 0) {
			goto failed;
		}
	}

	return 0;
failed:
	return -1;
}


/*
  initialise the baseinfo record
*/
static int ltdb_baseinfo_init(struct ldb_module *module)
{
	struct ldb_context *ldb;
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	struct ldb_message *msg;
	struct ldb_message_element el;
	struct ldb_val val;
	int ret;
	/* the initial sequence number must be different from the one
	   set in ltdb_cache_free(). Thanks to Jon for pointing this
	   out. */
	const char *initial_sequence_number = "1";

	ldb = ldb_module_get_ctx(module);

	ltdb->sequence_number = atof(initial_sequence_number);

	msg = ldb_msg_new(ltdb);
	if (msg == NULL) {
		goto failed;
	}

	msg->num_elements = 1;
	msg->elements = &el;
	msg->dn = ldb_dn_new(msg, ldb, LTDB_BASEINFO);
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
	return LDB_ERR_OPERATIONS_ERROR;
}

/*
  free any cache records
 */
static void ltdb_cache_free(struct ldb_module *module)
{
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);

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
	ltdb_cache_free(module);
	return ltdb_cache_load(module);
}

/*
  load the cache records
*/
int ltdb_cache_load(struct ldb_module *module)
{
	struct ldb_context *ldb;
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	struct ldb_dn *baseinfo_dn = NULL, *options_dn = NULL;
	struct ldb_dn *indexlist_dn = NULL;
	uint64_t seq;
	struct ldb_message *baseinfo = NULL, *options = NULL;
	int r;

	ldb = ldb_module_get_ctx(module);

	/* a very fast check to avoid extra database reads */
	if (ltdb->cache != NULL && 
	    tdb_get_seqnum(ltdb->tdb) == ltdb->tdb_seqnum) {
		return 0;
	}

	if (ltdb->cache == NULL) {
		ltdb->cache = talloc_zero(ltdb, struct ltdb_cache);
		if (ltdb->cache == NULL) goto failed;
		ltdb->cache->indexlist = ldb_msg_new(ltdb->cache);
		ltdb->cache->attributes = ldb_msg_new(ltdb->cache);
		if (ltdb->cache->indexlist == NULL ||
		    ltdb->cache->attributes == NULL) {
			goto failed;
		}
	}

	baseinfo = ldb_msg_new(ltdb->cache);
	if (baseinfo == NULL) goto failed;

	baseinfo_dn = ldb_dn_new(baseinfo, ldb, LTDB_BASEINFO);
	if (baseinfo_dn == NULL) goto failed;

	r= ltdb_search_dn1(module, baseinfo_dn, baseinfo);
	if (r != LDB_SUCCESS && r != LDB_ERR_NO_SUCH_OBJECT) {
		goto failed;
	}
	
	/* possibly initialise the baseinfo */
	if (r == LDB_ERR_NO_SUCH_OBJECT) {
		if (ltdb_baseinfo_init(module) != LDB_SUCCESS) {
			goto failed;
		}
		if (ltdb_search_dn1(module, baseinfo_dn, baseinfo) != LDB_SUCCESS) {
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

	/* Read an interpret database options */
	options = ldb_msg_new(ltdb->cache);
	if (options == NULL) goto failed;

	options_dn = ldb_dn_new(options, ldb, LTDB_OPTIONS);
	if (options_dn == NULL) goto failed;

	r= ltdb_search_dn1(module, options_dn, options);
	if (r != LDB_SUCCESS && r != LDB_ERR_NO_SUCH_OBJECT) {
		goto failed;
	}
	
	/* set flag for checking base DN on searches */
	if (r == LDB_SUCCESS) {
		ltdb->check_base = ldb_msg_find_attr_as_bool(options, LTDB_CHECK_BASE, false);
	} else {
		ltdb->check_base = false;
	}

	talloc_free(ltdb->cache->last_attribute.name);
	memset(&ltdb->cache->last_attribute, 0, sizeof(ltdb->cache->last_attribute));

	talloc_free(ltdb->cache->indexlist);
	ltdb_attributes_unload(module); /* calls internally "talloc_free" */

	ltdb->cache->indexlist = ldb_msg_new(ltdb->cache);
	ltdb->cache->attributes = ldb_msg_new(ltdb->cache);
	if (ltdb->cache->indexlist == NULL ||
	    ltdb->cache->attributes == NULL) {
		goto failed;
	}
	ltdb->cache->one_level_indexes = false;
	ltdb->cache->attribute_indexes = false;
	    
	indexlist_dn = ldb_dn_new(module, ldb, LTDB_INDEXLIST);
	if (indexlist_dn == NULL) goto failed;

	r = ltdb_search_dn1(module, indexlist_dn, ltdb->cache->indexlist);
	if (r != LDB_SUCCESS && r != LDB_ERR_NO_SUCH_OBJECT) {
		goto failed;
	}

	if (ldb_msg_find_element(ltdb->cache->indexlist, LTDB_IDXONE) != NULL) {
		ltdb->cache->one_level_indexes = true;
	}
	if (ldb_msg_find_element(ltdb->cache->indexlist, LTDB_IDXATTR) != NULL) {
		ltdb->cache->attribute_indexes = true;
	}

	if (ltdb_attributes_load(module) == -1) {
		goto failed;
	}

done:
	talloc_free(options);
	talloc_free(baseinfo);
	talloc_free(indexlist_dn);
	return 0;

failed:
	talloc_free(options);
	talloc_free(baseinfo);
	talloc_free(indexlist_dn);
	return -1;
}


/*
  increase the sequence number to indicate a database change
*/
int ltdb_increase_sequence_number(struct ldb_module *module)
{
	struct ldb_context *ldb;
	void *data = ldb_module_get_private(module);
	struct ltdb_private *ltdb = talloc_get_type(data, struct ltdb_private);
	struct ldb_message *msg;
	struct ldb_message_element el[2];
	struct ldb_val val;
	struct ldb_val val_time;
	time_t t = time(NULL);
	char *s = NULL;
	int ret;

	ldb = ldb_module_get_ctx(module);

	msg = ldb_msg_new(ltdb);
	if (msg == NULL) {
		errno = ENOMEM;
		return LDB_ERR_OPERATIONS_ERROR;
	}

	s = talloc_asprintf(msg, "%llu", ltdb->sequence_number+1);
	if (!s) {
		talloc_free(msg);
		errno = ENOMEM;
		return LDB_ERR_OPERATIONS_ERROR;
	}

	msg->num_elements = ARRAY_SIZE(el);
	msg->elements = el;
	msg->dn = ldb_dn_new(msg, ldb, LTDB_BASEINFO);
	if (msg->dn == NULL) {
		talloc_free(msg);
		errno = ENOMEM;
		return LDB_ERR_OPERATIONS_ERROR;
	}
	el[0].name = talloc_strdup(msg, LTDB_SEQUENCE_NUMBER);
	if (el[0].name == NULL) {
		talloc_free(msg);
		errno = ENOMEM;
		return LDB_ERR_OPERATIONS_ERROR;
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
		return LDB_ERR_OPERATIONS_ERROR;
	}
	el[1].values = &val_time;
	el[1].num_values = 1;
	el[1].flags = LDB_FLAG_MOD_REPLACE;

	s = ldb_timestring(msg, t);
	if (s == NULL) {
		talloc_free(msg);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	val_time.data = (uint8_t *)s;
	val_time.length = strlen(s);

	ret = ltdb_modify_internal(module, msg, NULL);

	talloc_free(msg);

	if (ret == LDB_SUCCESS) {
		ltdb->sequence_number += 1;
	}

	/* updating the tdb_seqnum here avoids us reloading the cache
	   records due to our own modification */
	ltdb->tdb_seqnum = tdb_get_seqnum(ltdb->tdb);

	return ret;
}

int ltdb_check_at_attributes_values(const struct ldb_val *value)
{
	unsigned int i;

	for (i = 0; ltdb_valid_attr_flags[i].name != NULL; i++) {
		if ((strcmp(ltdb_valid_attr_flags[i].name, (char *)value->data) == 0)) {
			return 0;
		}
	}

	return -1;
}

