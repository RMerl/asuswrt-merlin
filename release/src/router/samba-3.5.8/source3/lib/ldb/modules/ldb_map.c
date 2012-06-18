/*
   ldb database mapping module

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Martin Kuehl <mkhl@samba.org> 2006

   * NOTICE: this module is NOT released under the GNU LGPL license as
   * other ldb code. This module is release under the GNU GPL v2 or
   * later license.

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
 *  Component: ldb ldb_map module
 *
 *  Description: Map portions of data into a different format on a
 *  remote partition.
 *
 *  Author: Jelmer Vernooij, Martin Kuehl
 */

#include "includes.h"
#include "ldb/include/includes.h"

#include "ldb/modules/ldb_map.h"
#include "ldb/modules/ldb_map_private.h"

/* Description of the provided ldb requests:
 - special attribute 'isMapped'

 - search:
     - if parse tree can be split
         - search remote records w/ remote attrs and parse tree
     - otherwise
         - enumerate all remote records
     - for each remote result
         - map remote result to local message
         - search local result
         - is present
             - merge local into remote result
             - run callback on merged result
         - otherwise
             - run callback on remote result

 - add:
     - split message into local and remote part
     - if local message is not empty
         - add isMapped to local message
         - add local message
     - add remote message

 - modify:
     - split message into local and remote part
     - if local message is not empty
         - add isMapped to local message
         - search for local record
         - if present
             - modify local record
         - otherwise
             - add local message
     - modify remote record

 - delete:
     - search for local record
     - if present
         - delete local record
     - delete remote record

 - rename:
     - search for local record
     - if present
         - rename local record
         - modify local isMapped
     - rename remote record
*/



/* Private data structures
 * ======================= */

/* Global private data */
/* Extract mappings from private data. */
const struct ldb_map_context *map_get_context(struct ldb_module *module)
{
	const struct map_private *data = talloc_get_type(module->private_data, struct map_private);
	return data->context;
}

/* Create a generic request context. */
static struct map_context *map_init_context(struct ldb_handle *h, struct ldb_request *req)
{
	struct map_context *ac;

	ac = talloc_zero(h, struct map_context);
	if (ac == NULL) {
		map_oom(h->module);
		return NULL;
	}

	ac->module = h->module;
	ac->orig_req = req;

	return ac;
}

/* Create a search request context. */
struct map_search_context *map_init_search_context(struct map_context *ac, struct ldb_reply *ares)
{
	struct map_search_context *sc;

	sc = talloc_zero(ac, struct map_search_context);
	if (sc == NULL) {
		map_oom(ac->module);
		return NULL;
	}

	sc->ac = ac;
	sc->local_res = NULL;
	sc->remote_res = ares;

	return sc;
}

/* Create a request context and handle. */
struct ldb_handle *map_init_handle(struct ldb_request *req, struct ldb_module *module)
{
	struct map_context *ac;
	struct ldb_handle *h;

	h = talloc_zero(req, struct ldb_handle);
	if (h == NULL) {
		map_oom(module);
		return NULL;
	}

	h->module = module;

	ac = map_init_context(h, req);
	if (ac == NULL) {
		talloc_free(h);
		return NULL;
	}

	h->private_data = (void *)ac;

	h->state = LDB_ASYNC_INIT;
	h->status = LDB_SUCCESS;

	return h;
}


/* Dealing with DNs for different partitions
 * ========================================= */

/* Check whether any data should be stored in the local partition. */
BOOL map_check_local_db(struct ldb_module *module)
{
	const struct ldb_map_context *data = map_get_context(module);

	if (!data->remote_base_dn || !data->local_base_dn) {
		return False;
	}

	return True;
}

/* Copy a DN with the base DN of the local partition. */
static struct ldb_dn *ldb_dn_rebase_local(void *mem_ctx, const struct ldb_map_context *data, const struct ldb_dn *dn)
{
	return ldb_dn_copy_rebase(mem_ctx, dn, data->remote_base_dn, data->local_base_dn);
}

/* Copy a DN with the base DN of the remote partition. */
static struct ldb_dn *ldb_dn_rebase_remote(void *mem_ctx, const struct ldb_map_context *data, const struct ldb_dn *dn)
{
	return ldb_dn_copy_rebase(mem_ctx, dn, data->local_base_dn, data->remote_base_dn);
}

/* Run a request and make sure it targets the remote partition. */
/* TODO: free old DNs and messages? */
int ldb_next_remote_request(struct ldb_module *module, struct ldb_request *request)
{
	const struct ldb_map_context *data = map_get_context(module);
	struct ldb_message *msg;

	switch (request->operation) {
	case LDB_SEARCH:
		if (request->op.search.base) {
			request->op.search.base = ldb_dn_rebase_remote(request, data, request->op.search.base);
		} else {
			request->op.search.base = data->remote_base_dn;
			/* TODO: adjust scope? */
		}
		break;

	case LDB_ADD:
		msg = ldb_msg_copy_shallow(request, request->op.add.message);
		msg->dn = ldb_dn_rebase_remote(msg, data, msg->dn);
		request->op.add.message = msg;
		break;

	case LDB_MODIFY:
		msg = ldb_msg_copy_shallow(request, request->op.mod.message);
		msg->dn = ldb_dn_rebase_remote(msg, data, msg->dn);
		request->op.mod.message = msg;
		break;

	case LDB_DELETE:
		request->op.del.dn = ldb_dn_rebase_remote(request, data, request->op.del.dn);
		break;

	case LDB_RENAME:
		request->op.rename.olddn = ldb_dn_rebase_remote(request, data, request->op.rename.olddn);
		request->op.rename.newdn = ldb_dn_rebase_remote(request, data, request->op.rename.newdn);
		break;

	default:
		ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
			  "Invalid remote request!\n");
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_request(module, request);
}


/* Finding mappings for attributes and objectClasses
 * ================================================= */

/* Find an objectClass mapping by the local name. */
static const struct ldb_map_objectclass *map_objectclass_find_local(const struct ldb_map_context *data, const char *name)
{
	int i;

	for (i = 0; data->objectclass_maps && data->objectclass_maps[i].local_name; i++) {
		if (ldb_attr_cmp(data->objectclass_maps[i].local_name, name) == 0) {
			return &data->objectclass_maps[i];
		}
	}

	return NULL;
}

/* Find an objectClass mapping by the remote name. */
static const struct ldb_map_objectclass *map_objectclass_find_remote(const struct ldb_map_context *data, const char *name)
{
	int i;

	for (i = 0; data->objectclass_maps && data->objectclass_maps[i].remote_name; i++) {
		if (ldb_attr_cmp(data->objectclass_maps[i].remote_name, name) == 0) {
			return &data->objectclass_maps[i];
		}
	}

	return NULL;
}

/* Find an attribute mapping by the local name. */
const struct ldb_map_attribute *map_attr_find_local(const struct ldb_map_context *data, const char *name)
{
	int i;

	for (i = 0; data->attribute_maps[i].local_name; i++) {
		if (ldb_attr_cmp(data->attribute_maps[i].local_name, name) == 0) {
			return &data->attribute_maps[i];
		}
	}
	for (i = 0; data->attribute_maps[i].local_name; i++) {
		if (ldb_attr_cmp(data->attribute_maps[i].local_name, "*") == 0) {
			return &data->attribute_maps[i];
		}
	}

	return NULL;
}

/* Find an attribute mapping by the remote name. */
const struct ldb_map_attribute *map_attr_find_remote(const struct ldb_map_context *data, const char *name)
{
	const struct ldb_map_attribute *map;
	const struct ldb_map_attribute *wildcard = NULL;
	int i, j;

	for (i = 0; data->attribute_maps[i].local_name; i++) {
		map = &data->attribute_maps[i];
		if (ldb_attr_cmp(map->local_name, "*") == 0) {
			wildcard = &data->attribute_maps[i];
		}

		switch (map->type) {
		case MAP_IGNORE:
			break;

		case MAP_KEEP:
			if (ldb_attr_cmp(map->local_name, name) == 0) {
				return map;
			}
			break;

		case MAP_RENAME:
		case MAP_CONVERT:
			if (ldb_attr_cmp(map->u.rename.remote_name, name) == 0) {
				return map;
			}
			break;

		case MAP_GENERATE:
			for (j = 0; map->u.generate.remote_names && map->u.generate.remote_names[j]; j++) {
				if (ldb_attr_cmp(map->u.generate.remote_names[j], name) == 0) {
					return map;
				}
			}
			break;
		}
	}

	/* We didn't find it, so return the wildcard record if one was configured */
	return wildcard;
}


/* Mapping attributes
 * ================== */

/* Check whether an attribute will be mapped into the remote partition. */
BOOL map_attr_check_remote(const struct ldb_map_context *data, const char *attr)
{
	const struct ldb_map_attribute *map = map_attr_find_local(data, attr);

	if (map == NULL) {
		return False;
	}
	if (map->type == MAP_IGNORE) {
		return False;
	}

	return True;
}

/* Map an attribute name into the remote partition. */
const char *map_attr_map_local(void *mem_ctx, const struct ldb_map_attribute *map, const char *attr)
{
	if (map == NULL) {
		return talloc_strdup(mem_ctx, attr);
	}

	switch (map->type) {
	case MAP_KEEP:
		return talloc_strdup(mem_ctx, attr);

	case MAP_RENAME:
	case MAP_CONVERT:
		return talloc_strdup(mem_ctx, map->u.rename.remote_name);

	default:
		return NULL;
	}
}

/* Map an attribute name back into the local partition. */
const char *map_attr_map_remote(void *mem_ctx, const struct ldb_map_attribute *map, const char *attr)
{
	if (map == NULL) {
		return talloc_strdup(mem_ctx, attr);
	}

	if (map->type == MAP_KEEP) {
		return talloc_strdup(mem_ctx, attr);
	}

	return talloc_strdup(mem_ctx, map->local_name);
}


/* Merge two lists of attributes into a single one. */
int map_attrs_merge(struct ldb_module *module, void *mem_ctx, 
		    const char ***attrs, const char * const *more_attrs)
{
	int i, j, k;

	for (i = 0; *attrs && (*attrs)[i]; i++) /* noop */ ;
	for (j = 0; more_attrs && more_attrs[j]; j++) /* noop */ ;
	
	*attrs = talloc_realloc(mem_ctx, *attrs, const char *, i+j+1);
	if (*attrs == NULL) {
		map_oom(module);
		return -1;
	}

	for (k = 0; k < j; k++) {
		(*attrs)[i + k] = more_attrs[k];
	}

	(*attrs)[i+k] = NULL;

	return 0;
}

/* Mapping ldb values
 * ================== */

/* Map an ldb value into the remote partition. */
struct ldb_val ldb_val_map_local(struct ldb_module *module, void *mem_ctx, 
				 const struct ldb_map_attribute *map, const struct ldb_val *val)
{
	if (map && (map->type == MAP_CONVERT) && (map->u.convert.convert_local)) {
		return map->u.convert.convert_local(module, mem_ctx, val);
	}

	return ldb_val_dup(mem_ctx, val);
}

/* Map an ldb value back into the local partition. */
struct ldb_val ldb_val_map_remote(struct ldb_module *module, void *mem_ctx, 
				  const struct ldb_map_attribute *map, const struct ldb_val *val)
{
	if (map && (map->type == MAP_CONVERT) && (map->u.convert.convert_remote)) {
		return map->u.convert.convert_remote(module, mem_ctx, val);
	}

	return ldb_val_dup(mem_ctx, val);
}


/* Mapping DNs
 * =========== */

/* Check whether a DN is below the local baseDN. */
BOOL ldb_dn_check_local(struct ldb_module *module, const struct ldb_dn *dn)
{
	const struct ldb_map_context *data = map_get_context(module);

	if (!data->local_base_dn) {
		return True;
	}

	return ldb_dn_compare_base(module->ldb, data->local_base_dn, dn) == 0;
}

/* Map a DN into the remote partition. */
struct ldb_dn *ldb_dn_map_local(struct ldb_module *module, void *mem_ctx, const struct ldb_dn *dn)
{
	const struct ldb_map_context *data = map_get_context(module);
	struct ldb_dn *newdn;
	const struct ldb_map_attribute *map;
	enum ldb_map_attr_type map_type;
	const char *name;
	struct ldb_val value;
	int i, ret;

	if (dn == NULL) {
		return NULL;
	}

	newdn = ldb_dn_copy(mem_ctx, dn);
	if (newdn == NULL) {
		map_oom(module);
		return NULL;
	}

	/* For each RDN, map the component name and possibly the value */
	for (i = 0; i < ldb_dn_get_comp_num(newdn); i++) {
		map = map_attr_find_local(data, ldb_dn_get_component_name(dn, i));

		/* Unknown attribute - leave this RDN as is and hope the best... */
		if (map == NULL) {
			map_type = MAP_KEEP;
		} else {
			map_type = map->type;
		}

		switch (map_type) {
		case MAP_IGNORE:
		case MAP_GENERATE:
			ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
				  "MAP_IGNORE/MAP_GENERATE attribute '%s' "
				  "used in DN!\n", ldb_dn_get_component_name(dn, i));
			goto failed;

		case MAP_CONVERT:
			if (map->u.convert.convert_local == NULL) {
				ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
					  "'convert_local' not set for attribute '%s' "
					  "used in DN!\n", ldb_dn_get_component_name(dn, i));
				goto failed;
			}
			/* fall through */
		case MAP_KEEP:
		case MAP_RENAME:
			name = map_attr_map_local(newdn, map, ldb_dn_get_component_name(dn, i));
			if (name == NULL) goto failed;

			value = ldb_val_map_local(module, newdn, map, ldb_dn_get_component_val(dn, i));
			if (value.data == NULL) goto failed;

			ret = ldb_dn_set_component(newdn, i, name, value);
			if (ret != LDB_SUCCESS) {
				goto failed;
			}

			break;
		}
	}

	return newdn;

failed:
	talloc_free(newdn);
	return NULL;
}

/* Map a DN into the local partition. */
struct ldb_dn *ldb_dn_map_remote(struct ldb_module *module, void *mem_ctx, const struct ldb_dn *dn)
{
	const struct ldb_map_context *data = map_get_context(module);
	struct ldb_dn *newdn;
	const struct ldb_map_attribute *map;
	enum ldb_map_attr_type map_type;
	const char *name;
	struct ldb_val value;
	int i, ret;

	if (dn == NULL) {
		return NULL;
	}

	newdn = ldb_dn_copy(mem_ctx, dn);
	if (newdn == NULL) {
		map_oom(module);
		return NULL;
	}

	/* For each RDN, map the component name and possibly the value */
	for (i = 0; i < ldb_dn_get_comp_num(newdn); i++) {
		map = map_attr_find_remote(data, ldb_dn_get_component_name(dn, i));

		/* Unknown attribute - leave this RDN as is and hope the best... */
		if (map == NULL) {
			map_type = MAP_KEEP;
		} else {
			map_type = map->type;
		}

		switch (map_type) {
		case MAP_IGNORE:
		case MAP_GENERATE:
			ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
				  "MAP_IGNORE/MAP_GENERATE attribute '%s' "
				  "used in DN!\n", ldb_dn_get_component_name(dn, i));
			goto failed;

		case MAP_CONVERT:
			if (map->u.convert.convert_remote == NULL) {
				ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
					  "'convert_remote' not set for attribute '%s' "
					  "used in DN!\n", ldb_dn_get_component_name(dn, i));
				goto failed;
			}
			/* fall through */
		case MAP_KEEP:
		case MAP_RENAME:
			name = map_attr_map_remote(newdn, map, ldb_dn_get_component_name(dn, i));
			if (name == NULL) goto failed;

			value = ldb_val_map_remote(module, newdn, map, ldb_dn_get_component_val(dn, i));
			if (value.data == NULL) goto failed;

			ret = ldb_dn_set_component(newdn, i, name, value);
			if (ret != LDB_SUCCESS) {
				goto failed;
			}

			break;
		}
	}

	return newdn;

failed:
	talloc_free(newdn);
	return NULL;
}

/* Map a DN and its base into the local partition. */
/* TODO: This should not be required with GUIDs. */
struct ldb_dn *ldb_dn_map_rebase_remote(struct ldb_module *module, void *mem_ctx, const struct ldb_dn *dn)
{
	const struct ldb_map_context *data = map_get_context(module);
	struct ldb_dn *dn1, *dn2;

	dn1 = ldb_dn_rebase_local(mem_ctx, data, dn);
	dn2 = ldb_dn_map_remote(module, mem_ctx, dn1);

	talloc_free(dn1);
	return dn2;
}


/* Converting DNs and objectClasses (as ldb values)
 * ================================================ */

/* Map a DN contained in an ldb value into the remote partition. */
static struct ldb_val ldb_dn_convert_local(struct ldb_module *module, void *mem_ctx, const struct ldb_val *val)
{
	struct ldb_dn *dn, *newdn;
	struct ldb_val newval;

	dn = ldb_dn_explode(mem_ctx, (char *)val->data);
	newdn = ldb_dn_map_local(module, mem_ctx, dn);
	talloc_free(dn);

	newval.length = 0;
	newval.data = (uint8_t *)ldb_dn_linearize(mem_ctx, newdn);
	if (newval.data) {
		newval.length = strlen((char *)newval.data);
	}
	talloc_free(newdn);

	return newval;
}

/* Map a DN contained in an ldb value into the local partition. */
static struct ldb_val ldb_dn_convert_remote(struct ldb_module *module, void *mem_ctx, const struct ldb_val *val)
{
	struct ldb_dn *dn, *newdn;
	struct ldb_val newval;

	dn = ldb_dn_explode(mem_ctx, (char *)val->data);
	newdn = ldb_dn_map_remote(module, mem_ctx, dn);
	talloc_free(dn);

	newval.length = 0;
	newval.data = (uint8_t *)ldb_dn_linearize(mem_ctx, newdn);
	if (newval.data) {
		newval.length = strlen((char *)newval.data);
	}
	talloc_free(newdn);

	return newval;
}

/* Map an objectClass into the remote partition. */
static struct ldb_val map_objectclass_convert_local(struct ldb_module *module, void *mem_ctx, const struct ldb_val *val)
{
	const struct ldb_map_context *data = map_get_context(module);
	const char *name = (char *)val->data;
	const struct ldb_map_objectclass *map = map_objectclass_find_local(data, name);
	struct ldb_val newval;

	if (map) {
		newval.data = (uint8_t*)talloc_strdup(mem_ctx, map->remote_name);
		newval.length = strlen((char *)newval.data);
		return newval;
	}

	return ldb_val_dup(mem_ctx, val);
}

/* Generate a remote message with a mapped objectClass. */
static void map_objectclass_generate_remote(struct ldb_module *module, const char *local_attr, const struct ldb_message *old, struct ldb_message *remote, struct ldb_message *local)
{
	struct ldb_message_element *el, *oc;
	struct ldb_val val;
	BOOL found_extensibleObject = False;
	int i;

	/* Find old local objectClass */
	oc = ldb_msg_find_element(old, "objectClass");
	if (oc == NULL) {
		return;
	}

	/* Prepare new element */
	el = talloc_zero(remote, struct ldb_message_element);
	if (el == NULL) {
		ldb_oom(module->ldb);
		return;			/* TODO: fail? */
	}

	/* Copy local objectClass element, reverse space for an extra value */
	el->num_values = oc->num_values + 1;
	el->values = talloc_array(el, struct ldb_val, el->num_values);
	if (el->values == NULL) {
		talloc_free(el);
		ldb_oom(module->ldb);
		return;			/* TODO: fail? */
	}

	/* Copy local element name "objectClass" */
	el->name = talloc_strdup(el, local_attr);

	/* Convert all local objectClasses */
	for (i = 0; i < el->num_values - 1; i++) {
		el->values[i] = map_objectclass_convert_local(module, el->values, &oc->values[i]);
		if (ldb_attr_cmp((char *)el->values[i].data, "extensibleObject") == 0) {
			found_extensibleObject = True;
		}
	}

	if (!found_extensibleObject) {
		val.data = (uint8_t *)talloc_strdup(el->values, "extensibleObject");
		val.length = strlen((char *)val.data);

		/* Append additional objectClass "extensibleObject" */
		el->values[i] = val;
	} else {
		el->num_values--;
	}

	/* Add new objectClass to remote message */
	ldb_msg_add(remote, el, 0);
}

/* Map an objectClass into the local partition. */
static struct ldb_val map_objectclass_convert_remote(struct ldb_module *module, void *mem_ctx, const struct ldb_val *val)
{
	const struct ldb_map_context *data = map_get_context(module);
	const char *name = (char *)val->data;
	const struct ldb_map_objectclass *map = map_objectclass_find_remote(data, name);
	struct ldb_val newval;

	if (map) {
		newval.data = (uint8_t*)talloc_strdup(mem_ctx, map->local_name);
		newval.length = strlen((char *)newval.data);
		return newval;
	}

	return ldb_val_dup(mem_ctx, val);
}

/* Generate a local message with a mapped objectClass. */
static struct ldb_message_element *map_objectclass_generate_local(struct ldb_module *module, void *mem_ctx, const char *local_attr, const struct ldb_message *remote)
{
	struct ldb_message_element *el, *oc;
	struct ldb_val val;
	int i;

	/* Find old remote objectClass */
	oc = ldb_msg_find_element(remote, "objectClass");
	if (oc == NULL) {
		return NULL;
	}

	/* Prepare new element */
	el = talloc_zero(mem_ctx, struct ldb_message_element);
	if (el == NULL) {
		ldb_oom(module->ldb);
		return NULL;
	}

	/* Copy remote objectClass element */
	el->num_values = oc->num_values;
	el->values = talloc_array(el, struct ldb_val, el->num_values);
	if (el->values == NULL) {
		talloc_free(el);
		ldb_oom(module->ldb);
		return NULL;
	}

	/* Copy remote element name "objectClass" */
	el->name = talloc_strdup(el, local_attr);

	/* Convert all remote objectClasses */
	for (i = 0; i < el->num_values; i++) {
		el->values[i] = map_objectclass_convert_remote(module, el->values, &oc->values[i]);
	}

	val.data = (uint8_t *)talloc_strdup(el->values, "extensibleObject");
	val.length = strlen((char *)val.data);

	/* Remove last value if it was "extensibleObject" */
	if (ldb_val_equal_exact(&val, &el->values[i-1])) {
		el->num_values--;
		el->values = talloc_realloc(el, el->values, struct ldb_val, el->num_values);
		if (el->values == NULL) {
			talloc_free(el);
			ldb_oom(module->ldb);
			return NULL;
		}
	}

	return el;
}

/* Mappings for searches on objectClass= assuming a one-to-one
 * mapping.  Needed because this is a generate operator for the
 * add/modify code */
static int map_objectclass_convert_operator(struct ldb_module *module, void *mem_ctx, 
					    struct ldb_parse_tree **new, const struct ldb_parse_tree *tree) 
{
	
	static const struct ldb_map_attribute objectclass_map = {
		.local_name = "objectClass",
		.type = MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "objectClass",
				 .convert_local = map_objectclass_convert_local,
				 .convert_remote = map_objectclass_convert_remote,
			 },
		},
	};

	return map_subtree_collect_remote_simple(module, mem_ctx, new, tree, &objectclass_map);
}

/* Auxiliary request construction
 * ============================== */

/* Store the DN of a single search result in context. */
static int map_search_self_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct map_context *ac;

	if (context == NULL || ares == NULL) {
		ldb_set_errstring(ldb, talloc_asprintf(ldb, "NULL Context or Result in callback"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac = talloc_get_type(context, struct map_context);

	/* We are interested only in the single reply */
	if (ares->type != LDB_REPLY_ENTRY) {
		talloc_free(ares);
		return LDB_SUCCESS;
	}

	/* We have already found a remote DN */
	if (ac->local_dn) {
		ldb_set_errstring(ldb, talloc_asprintf(ldb, "Too many results to base search"));
		talloc_free(ares);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Store local DN */
	ac->local_dn = ares->message->dn;

	return LDB_SUCCESS;
}

/* Build a request to search a record by its DN. */
struct ldb_request *map_search_base_req(struct map_context *ac, const struct ldb_dn *dn, const char * const *attrs, const struct ldb_parse_tree *tree, void *context, ldb_search_callback callback)
{
	struct ldb_request *req;

	req = talloc_zero(ac, struct ldb_request);
	if (req == NULL) {
		map_oom(ac->module);
		return NULL;
	}

	req->operation = LDB_SEARCH;
	req->op.search.base = dn;
	req->op.search.scope = LDB_SCOPE_BASE;
	req->op.search.attrs = attrs;

	if (tree) {
		req->op.search.tree = tree;
	} else {
		req->op.search.tree = ldb_parse_tree(req, NULL);
		if (req->op.search.tree == NULL) {
			talloc_free(req);
			return NULL;
		}
	}

	req->controls = NULL;
	req->context = context;
	req->callback = callback;
	ldb_set_timeout_from_prev_req(ac->module->ldb, ac->orig_req, req);

	return req;
}

/* Build a request to search the local record by its DN. */
struct ldb_request *map_search_self_req(struct map_context *ac, const struct ldb_dn *dn)
{
	/* attrs[] is returned from this function in
	 * ac->search_req->op.search.attrs, so it must be static, as
	 * otherwise the compiler can put it on the stack */
	static const char * const attrs[] = { IS_MAPPED, NULL };
	struct ldb_parse_tree *tree;

	/* Limit search to records with 'IS_MAPPED' present */
	/* TODO: `tree = ldb_parse_tree(ac, IS_MAPPED);' won't do. */
	tree = talloc_zero(ac, struct ldb_parse_tree);
	if (tree == NULL) {
		map_oom(ac->module);
		return NULL;
	}

	tree->operation = LDB_OP_PRESENT;
	tree->u.present.attr = talloc_strdup(tree, IS_MAPPED);

	return map_search_base_req(ac, dn, attrs, tree, ac, map_search_self_callback);
}

/* Build a request to update the 'IS_MAPPED' attribute */
struct ldb_request *map_build_fixup_req(struct map_context *ac, const struct ldb_dn *olddn, const struct ldb_dn *newdn)
{
	struct ldb_request *req;
	struct ldb_message *msg;
	const char *dn;

	/* Prepare request */
	req = talloc_zero(ac, struct ldb_request);
	if (req == NULL) {
		map_oom(ac->module);
		return NULL;
	}

	/* Prepare message */
	msg = ldb_msg_new(req);
	if (msg == NULL) {
		map_oom(ac->module);
		goto failed;
	}

	/* Update local 'IS_MAPPED' to the new remote DN */
	msg->dn = discard_const_p(struct ldb_dn, olddn);
	dn = ldb_dn_linearize(msg, newdn);
	if (dn == NULL) {
		goto failed;
	}
	if (ldb_msg_add_empty(msg, IS_MAPPED, LDB_FLAG_MOD_REPLACE, NULL) != 0) {
		goto failed;
	}
	if (ldb_msg_add_string(msg, IS_MAPPED, dn) != 0) {
		goto failed;
	}

	req->operation = LDB_MODIFY;
	req->op.mod.message = msg;
	req->controls = NULL;
	req->handle = NULL;
	req->context = NULL;
	req->callback = NULL;

	return req;

failed:
	talloc_free(req);
	return NULL;
}


/* Asynchronous call structure
 * =========================== */

/* Figure out which request is currently pending. */
static struct ldb_request *map_get_req(struct map_context *ac)
{
	switch (ac->step) {
	case MAP_SEARCH_SELF_MODIFY:
	case MAP_SEARCH_SELF_DELETE:
	case MAP_SEARCH_SELF_RENAME:
		return ac->search_req;

	case MAP_ADD_REMOTE:
	case MAP_MODIFY_REMOTE:
	case MAP_DELETE_REMOTE:
	case MAP_RENAME_REMOTE:
		return ac->remote_req;

	case MAP_RENAME_FIXUP:
		return ac->down_req;

	case MAP_ADD_LOCAL:
	case MAP_MODIFY_LOCAL:
	case MAP_DELETE_LOCAL:
	case MAP_RENAME_LOCAL:
		return ac->local_req;

	case MAP_SEARCH_REMOTE:
		/* Can't happen */
		break;
	}

	return NULL;		/* unreachable; silences a warning */
}

typedef int (*map_next_function)(struct ldb_handle *handle);

/* Figure out the next request to run. */
static map_next_function map_get_next(struct map_context *ac)
{
	switch (ac->step) {
	case MAP_SEARCH_REMOTE:
		return NULL;

	case MAP_ADD_LOCAL:
		return map_add_do_remote;
	case MAP_ADD_REMOTE:
		return NULL;

	case MAP_SEARCH_SELF_MODIFY:
		return map_modify_do_local;
	case MAP_MODIFY_LOCAL:
		return map_modify_do_remote;
	case MAP_MODIFY_REMOTE:
		return NULL;

	case MAP_SEARCH_SELF_DELETE:
		return map_delete_do_local;
	case MAP_DELETE_LOCAL:
		return map_delete_do_remote;
	case MAP_DELETE_REMOTE:
		return NULL;

	case MAP_SEARCH_SELF_RENAME:
		return map_rename_do_local;
	case MAP_RENAME_LOCAL:
		return map_rename_do_fixup;
	case MAP_RENAME_FIXUP:
		return map_rename_do_remote;
	case MAP_RENAME_REMOTE:
		return NULL;
	}

	return NULL;		/* unreachable; silences a warning */
}

/* Wait for the current pending request to finish and continue with the next. */
static int map_wait_next(struct ldb_handle *handle)
{
	struct map_context *ac;
	struct ldb_request *req;
	map_next_function next;
	int ret;

	if (handle == NULL || handle->private_data == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (handle->state == LDB_ASYNC_DONE) {
		return handle->status;
	}

	handle->state = LDB_ASYNC_PENDING;
	handle->status = LDB_SUCCESS;

	ac = talloc_get_type(handle->private_data, struct map_context);

	if (ac->step == MAP_SEARCH_REMOTE) {
		int i;
		for (i = 0; i < ac->num_searches; i++) {
			req = ac->search_reqs[i];
			ret = ldb_wait(req->handle, LDB_WAIT_NONE);

			if (ret != LDB_SUCCESS) {
				handle->status = ret;
				goto done;
			}
			if (req->handle->status != LDB_SUCCESS) {
				handle->status = req->handle->status;
				goto done;
			}
			if (req->handle->state != LDB_ASYNC_DONE) {
				return LDB_SUCCESS;
			}
		}
	} else {

		req = map_get_req(ac);

		ret = ldb_wait(req->handle, LDB_WAIT_NONE);

		if (ret != LDB_SUCCESS) {
			handle->status = ret;
			goto done;
		}
		if (req->handle->status != LDB_SUCCESS) {
			handle->status = req->handle->status;
			goto done;
		}
		if (req->handle->state != LDB_ASYNC_DONE) {
			return LDB_SUCCESS;
		}

		next = map_get_next(ac);
		if (next) {
			return next(handle);
		}
	}

	ret = LDB_SUCCESS;

done:
	handle->state = LDB_ASYNC_DONE;
	return ret;
}

/* Wait for all current pending requests to finish. */
static int map_wait_all(struct ldb_handle *handle)
{
	int ret;

	while (handle->state != LDB_ASYNC_DONE) {
		ret = map_wait_next(handle);
		if (ret != LDB_SUCCESS) {
			return ret;
		}
	}

	return handle->status;
}

/* Wait for pending requests to finish. */
static int map_wait(struct ldb_handle *handle, enum ldb_wait_type type)
{
	if (type == LDB_WAIT_ALL) {
		return map_wait_all(handle);
	} else {
		return map_wait_next(handle);
	}
}


/* Module initialization
 * ===================== */

/* Provided module operations */
static const struct ldb_module_ops map_ops = {
	.name		= "ldb_map",
	.add		= map_add,
	.modify		= map_modify,
	.del		= map_delete,
	.rename		= map_rename,
	.search		= map_search,
	.wait		= map_wait,
};

/* Builtin mappings for DNs and objectClasses */
static const struct ldb_map_attribute builtin_attribute_maps[] = {
	{
		.local_name = "dn",
		.type = MAP_CONVERT,
		.u = {
			.convert = {
				 .remote_name = "dn",
				 .convert_local = ldb_dn_convert_local,
				 .convert_remote = ldb_dn_convert_remote,
			 },
		},
	},
	{
		.local_name = "objectClass",
		.type = MAP_GENERATE,
		.convert_operator = map_objectclass_convert_operator,
		.u = {
			.generate = {
				 .remote_names = { "objectClass", NULL },
				 .generate_local = map_objectclass_generate_local,
				 .generate_remote = map_objectclass_generate_remote,
			 },
		},
	},
	{
		.local_name = NULL,
	}
};

/* Find the special 'MAP_DN_NAME' record and store local and remote
 * base DNs in private data. */
static int map_init_dns(struct ldb_module *module, struct ldb_map_context *data, const char *name)
{
	static const char * const attrs[] = { MAP_DN_FROM, MAP_DN_TO, NULL };
	struct ldb_dn *dn;
	struct ldb_message *msg;
	struct ldb_result *res;
	int ret;

	if (!name) {
		data->local_base_dn = NULL;
		data->remote_base_dn = NULL;
		return LDB_SUCCESS;
	}

	dn = ldb_dn_string_compose(data, NULL, "%s=%s", MAP_DN_NAME, name);
	if (dn == NULL) {
		ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
			  "Failed to construct '%s' DN!\n", MAP_DN_NAME);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_search(module->ldb, module->ldb, &res, dn, LDB_SCOPE_BASE, attrs, NULL);
	talloc_free(dn);
	if (ret != LDB_SUCCESS) {
		return ret;
	}
	if (res->count == 0) {
		ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
			  "No results for '%s=%s'!\n", MAP_DN_NAME, name);
		talloc_free(res);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}
	if (res->count > 1) {
		ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
			  "Too many results for '%s=%s'!\n", MAP_DN_NAME, name);
		talloc_free(res);
		return LDB_ERR_CONSTRAINT_VIOLATION;
	}

	msg = res->msgs[0];
	data->local_base_dn = ldb_msg_find_attr_as_dn(data, msg, MAP_DN_FROM);
	data->remote_base_dn = ldb_msg_find_attr_as_dn(data, msg, MAP_DN_TO);
	talloc_free(res);

	return LDB_SUCCESS;
}

/* Store attribute maps and objectClass maps in private data. */
static int map_init_maps(struct ldb_module *module, struct ldb_map_context *data, 
			 const struct ldb_map_attribute *attrs, 
			 const struct ldb_map_objectclass *ocls, 
			 const char * const *wildcard_attributes)
{
	int i, j, last;
	last = 0;

	/* Count specified attribute maps */
	for (i = 0; attrs[i].local_name; i++) /* noop */ ;
	/* Count built-in attribute maps */
	for (j = 0; builtin_attribute_maps[j].local_name; j++) /* noop */ ;

	/* Store list of attribute maps */
	data->attribute_maps = talloc_array(data, struct ldb_map_attribute, i+j+1);
	if (data->attribute_maps == NULL) {
		map_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Specified ones go first */
	for (i = 0; attrs[i].local_name; i++) {
		data->attribute_maps[last] = attrs[i];
		last++;
	}

	/* Built-in ones go last */
	for (i = 0; builtin_attribute_maps[i].local_name; i++) {
		data->attribute_maps[last] = builtin_attribute_maps[i];
		last++;
	}

	/* Ensure 'local_name == NULL' for the last entry */
	memset(&data->attribute_maps[last], 0, sizeof(struct ldb_map_attribute));

	/* Store list of objectClass maps */
	data->objectclass_maps = ocls;

	data->wildcard_attributes = wildcard_attributes;

	return LDB_SUCCESS;
}

/* Copy the list of provided module operations. */
_PUBLIC_ struct ldb_module_ops ldb_map_get_ops(void)
{
	return map_ops;
}

/* Initialize global private data. */
_PUBLIC_ int ldb_map_init(struct ldb_module *module, const struct ldb_map_attribute *attrs, 
		 const struct ldb_map_objectclass *ocls,
		 const char * const *wildcard_attributes,
		 const char *name)
{
	struct map_private *data;
	int ret;

	/* Prepare private data */
	data = talloc_zero(module, struct map_private);
	if (data == NULL) {
		map_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	module->private_data = data;

	data->context = talloc_zero(data, struct ldb_map_context);
	if (!data->context) {
		map_oom(module);
		return LDB_ERR_OPERATIONS_ERROR;		
	}

	/* Store local and remote baseDNs */
	ret = map_init_dns(module, data->context, name);
	if (ret != LDB_SUCCESS) {
		talloc_free(data);
		return ret;
	}

	/* Store list of attribute and objectClass maps */
	ret = map_init_maps(module, data->context, attrs, ocls, wildcard_attributes);
	if (ret != LDB_SUCCESS) {
		talloc_free(data);
		return ret;
	}

	return LDB_SUCCESS;
}

/* Usage note for initialization of this module:
 *
 * ldb_map is meant to be used from a different module that sets up
 * the mappings and gets registered in ldb.
 *
 * 'ldb_map_init' initializes the private data of this module and
 * stores the attribute and objectClass maps in there.	It also looks
 * up the '@MAP' special DN so requests can be redirected to the
 * remote partition.
 *
 * This function should be called from the 'init_context' op of the
 * module using ldb_map.
 *
 * 'ldb_map_get_ops' returns a copy of ldb_maps module operations.
 *
 * It should be called from the initialize function of the using
 * module, which should then override the 'init_context' op with a
 * function making the appropriate calls to 'ldb_map_init'.
 */
