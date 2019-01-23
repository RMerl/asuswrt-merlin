/*
   ldb database mapping module

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Martin Kuehl <mkhl@samba.org> 2006
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006
   Copyright (C) Simo Sorce <idra@samba.org> 2008

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

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "ldb_map.h"
#include "ldb_map_private.h"


/* Mapping attributes
 * ================== */

/* Select attributes that stay in the local partition. */
static const char **map_attrs_select_local(struct ldb_module *module, void *mem_ctx, const char * const *attrs)
{
	const struct ldb_map_context *data = map_get_context(module);
	const char **result;
	unsigned int i, last;

	if (attrs == NULL)
		return NULL;

	last = 0;
	result = talloc_array(mem_ctx, const char *, 1);
	if (result == NULL) {
		goto failed;
	}
	result[0] = NULL;

	for (i = 0; attrs[i]; i++) {
		/* Wildcards and ignored attributes are kept locally */
		if ((ldb_attr_cmp(attrs[i], "*") == 0) ||
		    (!map_attr_check_remote(data, attrs[i]))) {
			result = talloc_realloc(mem_ctx, result, const char *, last+2);
			if (result == NULL) {
				goto failed;
			}

			result[last] = talloc_strdup(result, attrs[i]);
			result[last+1] = NULL;
			last++;
		}
	}

	return result;

failed:
	talloc_free(result);
	map_oom(module);
	return NULL;
}

/* Collect attributes that are mapped into the remote partition. */
static const char **map_attrs_collect_remote(struct ldb_module *module, void *mem_ctx, 
					     const char * const *attrs)
{
	const struct ldb_map_context *data = map_get_context(module);
	const char **result;
	const struct ldb_map_attribute *map;
	const char *name=NULL;
	unsigned int i, j, last;
	int ret;

	last = 0;
	result = talloc_array(mem_ctx, const char *, 1);
	if (result == NULL) {
		goto failed;
	}
	result[0] = NULL;

	for (i = 0; attrs[i]; i++) {
		/* Wildcards are kept remotely, too */
		if (ldb_attr_cmp(attrs[i], "*") == 0) {
			const char **new_attrs = NULL;
			ret = map_attrs_merge(module, mem_ctx, &new_attrs, attrs);
			if (ret != LDB_SUCCESS) {
				goto failed;
			}
			ret = map_attrs_merge(module, mem_ctx, &new_attrs, data->wildcard_attributes);
			if (ret != LDB_SUCCESS) {
				goto failed;
			}

			attrs = new_attrs;
			break;
		}
	}

	for (i = 0; attrs[i]; i++) {
		/* Wildcards are kept remotely, too */
		if (ldb_attr_cmp(attrs[i], "*") == 0) {
			/* Add all 'include in wildcard' attributes */
			name = attrs[i];
			goto named;
		}

		/* Add remote names of mapped attrs */
		map = map_attr_find_local(data, attrs[i]);
		if (map == NULL) {
			continue;
		}

		switch (map->type) {
		case LDB_MAP_IGNORE:
			continue;

		case LDB_MAP_KEEP:
			name = attrs[i];
			goto named;

		case LDB_MAP_RENAME:
		case LDB_MAP_CONVERT:
			name = map->u.rename.remote_name;
			goto named;

		case LDB_MAP_GENERATE:
			/* Add all remote names of "generate" attrs */
			for (j = 0; map->u.generate.remote_names[j]; j++) {
				result = talloc_realloc(mem_ctx, result, const char *, last+2);
				if (result == NULL) {
					goto failed;
				}

				result[last] = talloc_strdup(result, map->u.generate.remote_names[j]);
				result[last+1] = NULL;
				last++;
			}
			continue;
		}

	named:	/* We found a single remote name, add that */
		result = talloc_realloc(mem_ctx, result, const char *, last+2);
		if (result == NULL) {
			goto failed;
		}

		result[last] = talloc_strdup(result, name);
		result[last+1] = NULL;
		last++;
	}

	return result;

failed:
	talloc_free(result);
	map_oom(module);
	return NULL;
}

/* Split attributes that stay in the local partition from those that
 * are mapped into the remote partition. */
static int map_attrs_partition(struct ldb_module *module, void *mem_ctx, const char ***local_attrs, const char ***remote_attrs, const char * const *attrs)
{
	*local_attrs = map_attrs_select_local(module, mem_ctx, attrs);
	*remote_attrs = map_attrs_collect_remote(module, mem_ctx, attrs);

	return 0;
}

/* Mapping message elements
 * ======================== */

/* Add an element to a message, overwriting any old identically named elements. */
static int ldb_msg_replace(struct ldb_message *msg, const struct ldb_message_element *el)
{
	struct ldb_message_element *old;

	old = ldb_msg_find_element(msg, el->name);

	/* no local result, add as new element */
	if (old == NULL) {
		if (ldb_msg_add_empty(msg, el->name, 0, &old) != 0) {
			return -1;
		}
		talloc_free(discard_const_p(char, old->name));
	}

	/* copy new element */
	*old = *el;

	/* and make sure we reference the contents */
	if (!talloc_reference(msg->elements, el->name)) {
		return -1;
	}
	if (!talloc_reference(msg->elements, el->values)) {
		return -1;
	}

	return 0;
}

/* Map a message element back into the local partition. */
static struct ldb_message_element *ldb_msg_el_map_remote(struct ldb_module *module, 
							 void *mem_ctx, 
							 const struct ldb_map_attribute *map, 
							 const char *attr_name,
							 const struct ldb_message_element *old)
{
	const struct ldb_map_context *data = map_get_context(module);
	const char *local_attr_name = attr_name;
	struct ldb_message_element *el;
	unsigned int i;

	el = talloc_zero(mem_ctx, struct ldb_message_element);
	if (el == NULL) {
		map_oom(module);
		return NULL;
	}

	el->values = talloc_array(el, struct ldb_val, old->num_values);
	if (el->values == NULL) {
		talloc_free(el);
		map_oom(module);
		return NULL;
	}

	for (i = 0; data->attribute_maps[i].local_name; i++) {
		struct ldb_map_attribute *am = &data->attribute_maps[i];
		if ((am->type == LDB_MAP_RENAME &&
			!strcmp(am->u.rename.remote_name, attr_name))
		    || (am->type == LDB_MAP_CONVERT &&
			!strcmp(am->u.convert.remote_name, attr_name))) {

			local_attr_name = am->local_name;
			break;
		}
	}

	el->name = talloc_strdup(el, local_attr_name);
	if (el->name == NULL) {
		talloc_free(el);
		map_oom(module);
		return NULL;
	}

	for (i = 0; i < old->num_values; i++) {
		el->values[i] = ldb_val_map_remote(module, el->values, map, &old->values[i]);
		/* Conversions might fail, in which case bail */
		if (!el->values[i].data) {
			talloc_free(el);
			return NULL;
		}
		el->num_values++;
	}

	return el;
}

/* Merge a remote message element into a local message. */
static int ldb_msg_el_merge(struct ldb_module *module, struct ldb_message *local, 
			    struct ldb_message *remote, const char *attr_name)
{
	const struct ldb_map_context *data = map_get_context(module);
	const struct ldb_map_attribute *map;
	struct ldb_message_element *old, *el=NULL;
	const char *remote_name = NULL;
	struct ldb_context *ldb;

	ldb = ldb_module_get_ctx(module);

	/* We handle wildcards in ldb_msg_el_merge_wildcard */
	if (ldb_attr_cmp(attr_name, "*") == 0) {
		return LDB_SUCCESS;
	}

	map = map_attr_find_local(data, attr_name);

	/* Unknown attribute in remote message:
	 * skip, attribute was probably auto-generated */
	if (map == NULL) {
		return LDB_SUCCESS;
	}

	switch (map->type) {
	case LDB_MAP_IGNORE:
		break;
	case LDB_MAP_CONVERT:
		remote_name = map->u.convert.remote_name;
		break;
	case LDB_MAP_KEEP:
		remote_name = attr_name;
		break;
	case LDB_MAP_RENAME:
		remote_name = map->u.rename.remote_name;
		break;
	case LDB_MAP_GENERATE:
		break;
	}

	switch (map->type) {
	case LDB_MAP_IGNORE:
		return LDB_SUCCESS;

	case LDB_MAP_CONVERT:
		if (map->u.convert.convert_remote == NULL) {
			ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb_map: "
				  "Skipping attribute '%s': "
				  "'convert_remote' not set",
				  attr_name);
			return LDB_SUCCESS;
		}
		/* fall through */
	case LDB_MAP_KEEP:
	case LDB_MAP_RENAME:
		old = ldb_msg_find_element(remote, remote_name);
		if (old) {
			el = ldb_msg_el_map_remote(module, local, map, attr_name, old);
		} else {
			return LDB_ERR_NO_SUCH_ATTRIBUTE;
		}
		break;

	case LDB_MAP_GENERATE:
		if (map->u.generate.generate_local == NULL) {
			ldb_debug(ldb, LDB_DEBUG_ERROR, "ldb_map: "
				  "Skipping attribute '%s': "
				  "'generate_local' not set",
				  attr_name);
			return LDB_SUCCESS;
		}

		el = map->u.generate.generate_local(module, local, attr_name, remote);
		if (!el) {
			/* Generation failure is probably due to lack of source attributes */
			return LDB_ERR_NO_SUCH_ATTRIBUTE;
		}
		break;
	}

	if (el == NULL) {
		return LDB_ERR_NO_SUCH_ATTRIBUTE;
	}

	return ldb_msg_replace(local, el);
}

/* Handle wildcard parts of merging a remote message element into a local message. */
static int ldb_msg_el_merge_wildcard(struct ldb_module *module, struct ldb_message *local, 
				     struct ldb_message *remote)
{
	const struct ldb_map_context *data = map_get_context(module);
	const struct ldb_map_attribute *map = map_attr_find_local(data, "*");
	struct ldb_message_element *el=NULL;
	unsigned int i;
	int ret;

	/* Perhaps we have a mapping for "*" */
	if (map && map->type == LDB_MAP_KEEP) {
		/* We copy everything over, and hope that anything with a 
		   more specific rule is overwritten */
		for (i = 0; i < remote->num_elements; i++) {
			el = ldb_msg_el_map_remote(module, local, map, remote->elements[i].name,
						   &remote->elements[i]);
			if (el == NULL) {
				return LDB_ERR_OPERATIONS_ERROR;
			}
			
			ret = ldb_msg_replace(local, el);
			if (ret) {
				return ret;
			}
		}
	}
	
	/* Now walk the list of possible mappings, and apply each */
	for (i = 0; data->attribute_maps[i].local_name; i++) {
		ret = ldb_msg_el_merge(module, local, remote, 
				       data->attribute_maps[i].local_name);
		if (ret == LDB_ERR_NO_SUCH_ATTRIBUTE) {
			continue;
		} else if (ret) {
			return ret;
		} else {
			continue;
		}
	}

	return LDB_SUCCESS;
}

/* Mapping messages
 * ================ */

/* Merge two local messages into a single one. */
static int ldb_msg_merge_local(struct ldb_module *module, struct ldb_message *msg1, struct ldb_message *msg2)
{
	unsigned int i;
	int ret;

	for (i = 0; i < msg2->num_elements; i++) {
		ret = ldb_msg_replace(msg1, &msg2->elements[i]);
		if (ret) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}

/* Merge a local and a remote message into a single local one. */
static int ldb_msg_merge_remote(struct map_context *ac, struct ldb_message *local, 
				struct ldb_message *remote)
{
	unsigned int i;
	int ret;
	const char * const *attrs = ac->all_attrs;
	if (!attrs) {
		ret = ldb_msg_el_merge_wildcard(ac->module, local, remote);
		if (ret) {
			return ret;
		}
	}

	for (i = 0; attrs && attrs[i]; i++) {
		if (ldb_attr_cmp(attrs[i], "*") == 0) {
			ret = ldb_msg_el_merge_wildcard(ac->module, local, remote);
			if (ret) {
				return ret;
			}
			break;
		}
	}

	/* Try to map each attribute back;
	 * Add to local message is possible,
	 * Overwrite old local attribute if necessary */
	for (i = 0; attrs && attrs[i]; i++) {
		ret = ldb_msg_el_merge(ac->module, local, remote, 
				       attrs[i]);
		if (ret == LDB_ERR_NO_SUCH_ATTRIBUTE) {
		} else if (ret) {
			return ret;
		}
	}

	return LDB_SUCCESS;
}

/* Mapping search results
 * ====================== */

/* Map a search result back into the local partition. */
static int map_reply_remote(struct map_context *ac, struct ldb_reply *ares)
{
	struct ldb_message *msg;
	struct ldb_dn *dn;
	int ret;

	/* There is no result message, skip */
	if (ares->type != LDB_REPLY_ENTRY) {
		return 0;
	}

	/* Create a new result message */
	msg = ldb_msg_new(ares);
	if (msg == NULL) {
		map_oom(ac->module);
		return -1;
	}

	/* Merge remote message into new message */
	ret = ldb_msg_merge_remote(ac, msg, ares->message);
	if (ret) {
		talloc_free(msg);
		return ret;
	}

	/* Create corresponding local DN */
	dn = ldb_dn_map_rebase_remote(ac->module, msg, ares->message->dn);
	if (dn == NULL) {
		talloc_free(msg);
		return -1;
	}
	msg->dn = dn;

	/* Store new message with new DN as the result */
	talloc_free(ares->message);
	ares->message = msg;

	return 0;
}

/* Mapping parse trees
 * =================== */

/* Check whether a parse tree can safely be split in two. */
static bool ldb_parse_tree_check_splittable(const struct ldb_parse_tree *tree)
{
	const struct ldb_parse_tree *subtree = tree;
	bool negate = false;

	while (subtree) {
		switch (subtree->operation) {
		case LDB_OP_NOT:
			negate = !negate;
			subtree = subtree->u.isnot.child;
			continue;

		case LDB_OP_AND:
			return !negate;	/* if negate: False */

		case LDB_OP_OR:
			return negate;	/* if negate: True */

		default:
			return true;	/* simple parse tree */
		}
	}

	return true;			/* no parse tree */
}

/* Collect a list of attributes required to match a given parse tree. */
static int ldb_parse_tree_collect_attrs(struct ldb_module *module, void *mem_ctx, const char ***attrs, const struct ldb_parse_tree *tree)
{
	const char **new_attrs;
	unsigned int i;
	int ret;

	if (tree == NULL) {
		return 0;
	}

	switch (tree->operation) {
	case LDB_OP_OR:
	case LDB_OP_AND:		/* attributes stored in list of subtrees */
		for (i = 0; i < tree->u.list.num_elements; i++) {
			ret = ldb_parse_tree_collect_attrs(module, mem_ctx, 
							   attrs, tree->u.list.elements[i]);
			if (ret) {
				return ret;
			}
		}
		return 0;

	case LDB_OP_NOT:		/* attributes stored in single subtree */
		return ldb_parse_tree_collect_attrs(module, mem_ctx, attrs, tree->u.isnot.child);

	default:			/* single attribute in tree */
		new_attrs = ldb_attr_list_copy_add(mem_ctx, *attrs, tree->u.equality.attr);
		talloc_free(*attrs);
		*attrs = new_attrs;
		return 0;
	}
}

static int map_subtree_select_local(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree);

/* Select a negated subtree that queries attributes in the local partition */
static int map_subtree_select_local_not(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree)
{
	struct ldb_parse_tree *child;
	int ret;

	/* Prepare new tree */
	*new = talloc_memdup(mem_ctx, tree, sizeof(struct ldb_parse_tree));
	if (*new == NULL) {
		map_oom(module);
		return -1;
	}

	/* Generate new subtree */
	ret = map_subtree_select_local(module, *new, &child, tree->u.isnot.child);
	if (ret) {
		talloc_free(*new);
		return ret;
	}

	/* Prune tree without subtree */
	if (child == NULL) {
		talloc_free(*new);
		*new = NULL;
		return 0;
	}

	(*new)->u.isnot.child = child;

	return ret;
}

/* Select a list of subtrees that query attributes in the local partition */
static int map_subtree_select_local_list(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree)
{
	unsigned int i, j;
	int ret=0;

	/* Prepare new tree */
	*new = talloc_memdup(mem_ctx, tree, sizeof(struct ldb_parse_tree));
	if (*new == NULL) {
		map_oom(module);
		return -1;
	}

	/* Prepare list of subtrees */
	(*new)->u.list.num_elements = 0;
	(*new)->u.list.elements = talloc_array(*new, struct ldb_parse_tree *, tree->u.list.num_elements);
	if ((*new)->u.list.elements == NULL) {
		map_oom(module);
		talloc_free(*new);
		return -1;
	}

	/* Generate new list of subtrees */
	j = 0;
	for (i = 0; i < tree->u.list.num_elements; i++) {
		struct ldb_parse_tree *child;
		ret = map_subtree_select_local(module, *new, &child, tree->u.list.elements[i]);
		if (ret) {
			talloc_free(*new);
			return ret;
		}

		if (child) {
			(*new)->u.list.elements[j] = child;
			j++;
		}
	}

	/* Prune tree without subtrees */
	if (j == 0) {
		talloc_free(*new);
		*new = NULL;
		return 0;
	}

	/* Fix subtree list size */
	(*new)->u.list.num_elements = j;
	(*new)->u.list.elements = talloc_realloc(*new, (*new)->u.list.elements, struct ldb_parse_tree *, (*new)->u.list.num_elements);

	return ret;
}

/* Select a simple subtree that queries attributes in the local partition */
static int map_subtree_select_local_simple(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree)
{
	/* Prepare new tree */
	*new = talloc_memdup(mem_ctx, tree, sizeof(struct ldb_parse_tree));
	if (*new == NULL) {
		map_oom(module);
		return -1;
	}

	return 0;
}

/* Select subtrees that query attributes in the local partition */
static int map_subtree_select_local(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree)
{
	const struct ldb_map_context *data = map_get_context(module);

	if (tree == NULL) {
		return 0;
	}

	if (tree->operation == LDB_OP_NOT) {
		return map_subtree_select_local_not(module, mem_ctx, new, tree);
	}

	if (tree->operation == LDB_OP_AND || tree->operation == LDB_OP_OR) {
		return map_subtree_select_local_list(module, mem_ctx, new, tree);
	}

	if (map_attr_check_remote(data, tree->u.equality.attr)) {
		*new = NULL;
		return 0;
	}

	return map_subtree_select_local_simple(module, mem_ctx, new, tree);
}

static int map_subtree_collect_remote(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree);

/* Collect a negated subtree that queries attributes in the remote partition */
static int map_subtree_collect_remote_not(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree)
{
	struct ldb_parse_tree *child;
	int ret;

	/* Prepare new tree */
	*new = talloc_memdup(mem_ctx, tree, sizeof(struct ldb_parse_tree));
	if (*new == NULL) {
		map_oom(module);
		return -1;
	}

	/* Generate new subtree */
	ret = map_subtree_collect_remote(module, *new, &child, tree->u.isnot.child);
	if (ret) {
		talloc_free(*new);
		return ret;
	}

	/* Prune tree without subtree */
	if (child == NULL) {
		talloc_free(*new);
		*new = NULL;
		return 0;
	}

	(*new)->u.isnot.child = child;

	return ret;
}

/* Collect a list of subtrees that query attributes in the remote partition */
static int map_subtree_collect_remote_list(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree)
{
	unsigned int i, j;
	int ret=0;

	/* Prepare new tree */
	*new = talloc_memdup(mem_ctx, tree, sizeof(struct ldb_parse_tree));
	if (*new == NULL) {
		map_oom(module);
		return -1;
	}

	/* Prepare list of subtrees */
	(*new)->u.list.num_elements = 0;
	(*new)->u.list.elements = talloc_array(*new, struct ldb_parse_tree *, tree->u.list.num_elements);
	if ((*new)->u.list.elements == NULL) {
		map_oom(module);
		talloc_free(*new);
		return -1;
	}

	/* Generate new list of subtrees */
	j = 0;
	for (i = 0; i < tree->u.list.num_elements; i++) {
		struct ldb_parse_tree *child;
		ret = map_subtree_collect_remote(module, *new, &child, tree->u.list.elements[i]);
		if (ret) {
			talloc_free(*new);
			return ret;
		}

		if (child) {
			(*new)->u.list.elements[j] = child;
			j++;
		}
	}

	/* Prune tree without subtrees */
	if (j == 0) {
		talloc_free(*new);
		*new = NULL;
		return 0;
	}

	/* Fix subtree list size */
	(*new)->u.list.num_elements = j;
	(*new)->u.list.elements = talloc_realloc(*new, (*new)->u.list.elements, struct ldb_parse_tree *, (*new)->u.list.num_elements);

	return ret;
}

/* Collect a simple subtree that queries attributes in the remote partition */
int map_subtree_collect_remote_simple(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree, const struct ldb_map_attribute *map)
{
	const char *attr;

	/* Prepare new tree */
	*new = talloc(mem_ctx, struct ldb_parse_tree);
	if (*new == NULL) {
		map_oom(module);
		return -1;
	}
	**new = *tree;
	
	if (map->type == LDB_MAP_KEEP) {
		/* Nothing to do here */
		return 0;
	}

	/* Store attribute and value in new tree */
	switch (tree->operation) {
	case LDB_OP_PRESENT:
		attr = map_attr_map_local(*new, map, tree->u.present.attr);
		(*new)->u.present.attr = attr;
		break;
	case LDB_OP_SUBSTRING:
	{
		attr = map_attr_map_local(*new, map, tree->u.substring.attr);
		(*new)->u.substring.attr = attr;
		break;
	}
	case LDB_OP_EQUALITY:
		attr = map_attr_map_local(*new, map, tree->u.equality.attr);
		(*new)->u.equality.attr = attr;
		break;
	case LDB_OP_LESS:
	case LDB_OP_GREATER:
	case LDB_OP_APPROX:
		attr = map_attr_map_local(*new, map, tree->u.comparison.attr);
		(*new)->u.comparison.attr = attr;
		break;
	case LDB_OP_EXTENDED:
		attr = map_attr_map_local(*new, map, tree->u.extended.attr);
		(*new)->u.extended.attr = attr;
		break;
	default:			/* unknown kind of simple subtree */
		talloc_free(*new);
		return -1;
	}

	if (attr == NULL) {
		talloc_free(*new);
		*new = NULL;
		return 0;
	}

	if (map->type == LDB_MAP_RENAME) {
		/* Nothing more to do here, the attribute has been renamed */
		return 0;
	}

	/* Store attribute and value in new tree */
	switch (tree->operation) {
	case LDB_OP_PRESENT:
		break;
	case LDB_OP_SUBSTRING:
	{
		int i;
		/* Map value */
		(*new)->u.substring.chunks = NULL;
		for (i=0; tree->u.substring.chunks[i]; i++) {
			(*new)->u.substring.chunks = talloc_realloc(*new, (*new)->u.substring.chunks, struct ldb_val *, i+2);
			if (!(*new)->u.substring.chunks) {
				talloc_free(*new);
				*new = NULL;
				return 0;
			}
			(*new)->u.substring.chunks[i] = talloc(*new, struct ldb_val);
			if (!(*new)->u.substring.chunks[i]) {
				talloc_free(*new);
				*new = NULL;
				return 0;
			}
			*(*new)->u.substring.chunks[i] = ldb_val_map_local(module, *new, map, tree->u.substring.chunks[i]);
			(*new)->u.substring.chunks[i+1] = NULL;
		}
		break;
	}
	case LDB_OP_EQUALITY:
		(*new)->u.equality.value = ldb_val_map_local(module, *new, map, &tree->u.equality.value);
		break;
	case LDB_OP_LESS:
	case LDB_OP_GREATER:
	case LDB_OP_APPROX:
		(*new)->u.comparison.value = ldb_val_map_local(module, *new, map, &tree->u.comparison.value);
		break;
	case LDB_OP_EXTENDED:
		(*new)->u.extended.value = ldb_val_map_local(module, *new, map, &tree->u.extended.value);
		(*new)->u.extended.rule_id = talloc_strdup(*new, tree->u.extended.rule_id);
		break;
	default:			/* unknown kind of simple subtree */
		talloc_free(*new);
		return -1;
	}

	return 0;
}

/* Collect subtrees that query attributes in the remote partition */
static int map_subtree_collect_remote(struct ldb_module *module, void *mem_ctx, struct ldb_parse_tree **new, const struct ldb_parse_tree *tree)
{
	const struct ldb_map_context *data = map_get_context(module);
	const struct ldb_map_attribute *map;
	struct ldb_context *ldb;

	ldb = ldb_module_get_ctx(module);

	if (tree == NULL) {
		return 0;
	}

	if (tree->operation == LDB_OP_NOT) {
		return map_subtree_collect_remote_not(module, mem_ctx, new, tree);
	}

	if ((tree->operation == LDB_OP_AND) || (tree->operation == LDB_OP_OR)) {
		return map_subtree_collect_remote_list(module, mem_ctx, new, tree);
	}

	if (!map_attr_check_remote(data, tree->u.equality.attr)) {
		*new = NULL;
		return 0;
	}

	map = map_attr_find_local(data, tree->u.equality.attr);
	if (map->convert_operator) {
		return map->convert_operator(module, mem_ctx, new, tree);
	}

	if (map->type == LDB_MAP_GENERATE) {
		ldb_debug(ldb, LDB_DEBUG_WARNING, "ldb_map: "
			  "Skipping attribute '%s': "
			  "'convert_operator' not set",
			  tree->u.equality.attr);
		*new = NULL;
		return 0;
	}

	return map_subtree_collect_remote_simple(module, mem_ctx, new, tree, map);
}

/* Split subtrees that query attributes in the local partition from
 * those that query the remote partition. */
static int ldb_parse_tree_partition(struct ldb_module *module,
					void *mem_ctx,
					struct ldb_parse_tree **local_tree,
					struct ldb_parse_tree **remote_tree,
					const struct ldb_parse_tree *tree)
{
	int ret;

	*local_tree = NULL;
	*remote_tree = NULL;

	/* No original tree */
	if (tree == NULL) {
		return 0;
	}

	/* Generate local tree */
	ret = map_subtree_select_local(module, mem_ctx, local_tree, tree);
	if (ret) {
		return ret;
	}

	/* Generate remote tree */
	ret = map_subtree_collect_remote(module, mem_ctx, remote_tree, tree);
	if (ret) {
		talloc_free(*local_tree);
		return ret;
	}

	return 0;
}

/* Collect a list of attributes required either explicitly from a
 * given list or implicitly  from a given parse tree; split the
 * collected list into local and remote parts. */
static int map_attrs_collect_and_partition(struct ldb_module *module, struct map_context *ac,
					   const char * const *search_attrs, 
					   const struct ldb_parse_tree *tree)
{
	void *tmp_ctx;
	const char **tree_attrs;
	const char **remote_attrs;
	const char **local_attrs;
	int ret;

	/* There is no tree, just partition the searched attributes */
	if (tree == NULL) {
		ret = map_attrs_partition(module, ac, 
					  &local_attrs, &remote_attrs, search_attrs);
		if (ret == 0) {
			ac->local_attrs = local_attrs;
			ac->remote_attrs = remote_attrs;
			ac->all_attrs = search_attrs;
		}
		return ret; 
	}

	/* Create context for temporary memory */
	tmp_ctx = talloc_new(ac);
	if (tmp_ctx == NULL) {
		goto oom;
	}

	/* Prepare list of attributes from tree */
	tree_attrs = talloc_array(tmp_ctx, const char *, 1);
	if (tree_attrs == NULL) {
		talloc_free(tmp_ctx);
		goto oom;
	}
	tree_attrs[0] = NULL;

	/* Collect attributes from tree */
	ret = ldb_parse_tree_collect_attrs(module, tmp_ctx, &tree_attrs, tree);
	if (ret) {
		goto done;
	}

	/* Merge attributes from search operation */
	ret = map_attrs_merge(module, tmp_ctx, &tree_attrs, search_attrs);
	if (ret) {
		goto done;
	}

	/* Split local from remote attributes */
	ret = map_attrs_partition(module, ac, &local_attrs, 
				  &remote_attrs, tree_attrs);
	
	if (ret == 0) {
		ac->local_attrs = local_attrs;
		ac->remote_attrs = remote_attrs;
		talloc_steal(ac, tree_attrs);
		ac->all_attrs = tree_attrs;
	}
done:
	/* Free temporary memory */
	talloc_free(tmp_ctx);
	return ret;

oom:
	map_oom(module);
	return -1;
}


/* Outbound requests: search
 * ========================= */

static int map_remote_search_callback(struct ldb_request *req,
					struct ldb_reply *ares);
static int map_local_merge_callback(struct ldb_request *req,
					struct ldb_reply *ares);
static int map_search_local(struct map_context *ac);

static int map_save_entry(struct map_context *ac, struct ldb_reply *ares)
{
	struct map_reply *mr;

	mr = talloc_zero(ac, struct map_reply);
	if (mr == NULL) {
		map_oom(ac->module);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	mr->remote = talloc_steal(mr, ares);
	if (ac->r_current) {
		ac->r_current->next = mr;
	} else {
		/* first entry */
		ac->r_list = mr;
	}
	ac->r_current = mr;

	return LDB_SUCCESS;
}

/* Pass a merged search result up the callback chain. */
int map_return_entry(struct map_context *ac, struct ldb_reply *ares)
{
	struct ldb_message_element *el;
	const char * const *attrs;
	struct ldb_context *ldb;
	unsigned int i;
	int ret;
	bool matched;

	ldb = ldb_module_get_ctx(ac->module);

	/* Merged result doesn't match original query, skip */
	ret = ldb_match_msg_error(ldb, ares->message,
				  ac->req->op.search.tree,
				  ac->req->op.search.base,
				  ac->req->op.search.scope,
				  &matched);
	if (ret != LDB_SUCCESS) return ret;
	if (!matched) {
		ldb_debug(ldb, LDB_DEBUG_TRACE, "ldb_map: "
			  "Skipping record '%s': "
			  "doesn't match original search",
			  ldb_dn_get_linearized(ares->message->dn));
		return LDB_SUCCESS;
	}

	/* Limit result to requested attrs */
	if (ac->req->op.search.attrs &&
	    (! ldb_attr_in_list(ac->req->op.search.attrs, "*"))) {

		attrs = ac->req->op.search.attrs;
		i = 0;

		while (i < ares->message->num_elements) {

			el = &ares->message->elements[i];
			if ( ! ldb_attr_in_list(attrs, el->name)) {
				ldb_msg_remove_element(ares->message, el);
			} else {
				i++;
			}
		}
	}

	return ldb_module_send_entry(ac->req, ares->message, ares->controls);
}

/* Search a record. */
int ldb_map_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_parse_tree *remote_tree;
	struct ldb_parse_tree *local_tree;
	struct ldb_request *remote_req;
	struct ldb_context *ldb;
	struct map_context *ac;
	int ret;

	const char *wildcard[] = { "*", NULL };
	const char * const *attrs;

	ldb = ldb_module_get_ctx(module);

	/* if we're not yet initialized, go to the next module */
	if (!ldb_module_get_private(module))
		return ldb_next_request(module, req);

	/* Do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.search.base)) {
		return ldb_next_request(module, req);
	}

	/* No mapping requested, skip to next module */
	if ((req->op.search.base) && (!ldb_dn_check_local(module, req->op.search.base))) {
		return ldb_next_request(module, req);
	}

	/* TODO: How can we be sure about which partition we are
	 *	 targetting when there is no search base? */

	/* Prepare context and handle */
	ac = map_init_context(module, req);
	if (ac == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* It is easier to deal with the two different ways of
	 * expressing the wildcard in the same codepath */
	attrs = req->op.search.attrs;
	if (attrs == NULL) {
		attrs = wildcard;
	}

	/* Split local from remote attrs */
	ret = map_attrs_collect_and_partition(module, ac, 
					      attrs, req->op.search.tree);
	if (ret) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Split local from remote tree */
	ret = ldb_parse_tree_partition(module, ac,
				       &local_tree, &remote_tree,
				       req->op.search.tree);
	if (ret) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	if (((local_tree != NULL) && (remote_tree != NULL)) &&
	    (!ldb_parse_tree_check_splittable(req->op.search.tree))) {
		/* The query can't safely be split, enumerate the remote partition */
		local_tree = NULL;
		remote_tree = NULL;
	}

	if (local_tree == NULL) {
		/* Construct default local parse tree */
		local_tree = talloc_zero(ac, struct ldb_parse_tree);
		if (local_tree == NULL) {
			map_oom(ac->module);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		local_tree->operation = LDB_OP_PRESENT;
		local_tree->u.present.attr = talloc_strdup(local_tree, IS_MAPPED);
	}
	if (remote_tree == NULL) {
		/* Construct default remote parse tree */
		remote_tree = ldb_parse_tree(ac, NULL);
		if (remote_tree == NULL) {
			return LDB_ERR_OPERATIONS_ERROR;
		}
	}

	ac->local_tree = local_tree;

	/* Prepare the remote operation */
	ret = ldb_build_search_req_ex(&remote_req, ldb, ac,
				      req->op.search.base,
				      req->op.search.scope,
				      remote_tree,
				      ac->remote_attrs,
				      req->controls,
				      ac, map_remote_search_callback,
				      req);
	LDB_REQ_SET_LOCATION(remote_req);
	if (ret != LDB_SUCCESS) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_remote_request(module, remote_req);
}

/* Now, search the local part of a remote search result. */
static int map_remote_search_callback(struct ldb_request *req,
					struct ldb_reply *ares)
{
	struct map_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct map_context);

	if (!ares) {
		return ldb_module_done(ac->req, NULL, NULL,
					LDB_ERR_OPERATIONS_ERROR);
	}
	if (ares->error != LDB_SUCCESS) {
		return ldb_module_done(ac->req, ares->controls,
					ares->response, ares->error);
	}

	switch (ares->type) {
	case LDB_REPLY_REFERRAL:

		/* ignore referrals */
		talloc_free(ares);
		return LDB_SUCCESS;

	case LDB_REPLY_ENTRY:

		/* Map result record into a local message */
		ret = map_reply_remote(ac, ares);
		if (ret) {
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		/* if we have no local db, then we can just return the reply to
		 * the upper layer, otherwise we must save it and process it
		 * when all replies ahve been gathered */
		if ( ! map_check_local_db(ac->module)) {
			ret = map_return_entry(ac, ares);
		} else {
			ret = map_save_entry(ac,ares);
		}

		if (ret != LDB_SUCCESS) {
			talloc_free(ares);
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}
		break;

	case LDB_REPLY_DONE:

		if ( ! map_check_local_db(ac->module)) {
			return ldb_module_done(ac->req, ares->controls,
						ares->response, LDB_SUCCESS);
		}

		/* reset the pointer to the start of the list */
		ac->r_current = ac->r_list;

		/* no entry just return */
		if (ac->r_current == NULL) {
			ret = ldb_module_done(ac->req, ares->controls,
						ares->response, LDB_SUCCESS);
			talloc_free(ares);
			return ret;
		}

		ac->remote_done_ares = talloc_steal(ac, ares);

		ret = map_search_local(ac);
		if (ret != LDB_SUCCESS) {
			return ldb_module_done(ac->req, NULL, NULL, ret);
		}
	}

	return LDB_SUCCESS;
}

static int map_search_local(struct map_context *ac)
{
	struct ldb_request *search_req;

	if (ac->r_current == NULL || ac->r_current->remote == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Prepare local search request */
	/* TODO: use GUIDs here instead? */
	search_req = map_search_base_req(ac,
					 ac->r_current->remote->message->dn,
					 NULL, NULL,
					 ac, map_local_merge_callback);
	if (search_req == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}

	return ldb_next_request(ac->module, search_req);
}

/* Merge the remote and local parts of a search result. */
int map_local_merge_callback(struct ldb_request *req, struct ldb_reply *ares)
{
	struct ldb_context *ldb;
	struct map_context *ac;
	int ret;

	ac = talloc_get_type(req->context, struct map_context);
	ldb = ldb_module_get_ctx(ac->module);

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
		/* We have already found a local record */
		if (ac->r_current->local) {
			talloc_free(ares);
			ldb_set_errstring(ldb, "ldb_map: Too many results!");
			return ldb_module_done(ac->req, NULL, NULL,
						LDB_ERR_OPERATIONS_ERROR);
		}

		/* Store local result */
		ac->r_current->local = talloc_steal(ac->r_current, ares);

		break;

	case LDB_REPLY_REFERRAL:
		/* ignore referrals */
		talloc_free(ares);
		break;

	case LDB_REPLY_DONE:
		/* We don't need the local 'ares', but we will use the remote one from below */
		talloc_free(ares);

		/* No local record found, map and send remote record */
		if (ac->r_current->local != NULL) {
			/* Merge remote into local message */
			ret = ldb_msg_merge_local(ac->module,
						  ac->r_current->local->message,
						  ac->r_current->remote->message);
			if (ret == LDB_SUCCESS) {
				ret = map_return_entry(ac, ac->r_current->local);
			}
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req, NULL, NULL,
							LDB_ERR_OPERATIONS_ERROR);
			}
		} else {
			ret = map_return_entry(ac, ac->r_current->remote);
			if (ret != LDB_SUCCESS) {
				return ldb_module_done(ac->req,
							NULL, NULL, ret);
			}
		}

		if (ac->r_current->next != NULL) {
			ac->r_current = ac->r_current->next;
			if (ac->r_current->remote->type == LDB_REPLY_ENTRY) {
				ret = map_search_local(ac);
				if (ret != LDB_SUCCESS) {
					return ldb_module_done(ac->req,
							       NULL, NULL, ret);
				}
				break;
			}
		}

		/* ok we are done with all search, finally it is time to
		 * finish operations for this module */
		return ldb_module_done(ac->req,
					ac->remote_done_ares->controls,
					ac->remote_done_ares->response,
					ac->remote_done_ares->error);
	}

	return LDB_SUCCESS;
}
