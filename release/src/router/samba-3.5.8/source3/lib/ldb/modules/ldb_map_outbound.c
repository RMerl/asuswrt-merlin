/*
   ldb database mapping module

   Copyright (C) Jelmer Vernooij 2005
   Copyright (C) Martin Kuehl <mkhl@samba.org> 2006
   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2006

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

#include "includes.h"
#include "ldb/include/includes.h"

#include "ldb/modules/ldb_map.h"
#include "ldb/modules/ldb_map_private.h"


/* Mapping attributes
 * ================== */

/* Select attributes that stay in the local partition. */
static const char **map_attrs_select_local(struct ldb_module *module, void *mem_ctx, const char * const *attrs)
{
	const struct ldb_map_context *data = map_get_context(module);
	const char **result;
	int i, last;

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
	int i, j, last;
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
		case MAP_IGNORE:
			continue;

		case MAP_KEEP:
			name = attrs[i];
			goto named;

		case MAP_RENAME:
		case MAP_CONVERT:
			name = map->u.rename.remote_name;
			goto named;

		case MAP_GENERATE:
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
		talloc_free(old->name);
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
	struct ldb_message_element *el;
	int i;

	el = talloc_zero(mem_ctx, struct ldb_message_element);
	if (el == NULL) {
		map_oom(module);
		return NULL;
	}

	el->num_values = old->num_values;
	el->values = talloc_array(el, struct ldb_val, el->num_values);
	if (el->values == NULL) {
		talloc_free(el);
		map_oom(module);
		return NULL;
	}

	el->name = talloc_strdup(el, attr_name);
	if (el->name == NULL) {
		talloc_free(el);
		map_oom(module);
		return NULL;
	}

	for (i = 0; i < el->num_values; i++) {
		el->values[i] = ldb_val_map_remote(module, el->values, map, &old->values[i]);
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

	/* We handle wildcards in ldb_msg_el_merge_wildcard */
	if (ldb_attr_cmp(attr_name, "*") == 0) {
		return 0;
	}

	map = map_attr_find_local(data, attr_name);

	/* Unknown attribute in remote message:
	 * skip, attribute was probably auto-generated */
	if (map == NULL) {
		return 0;
	}

	switch (map->type) {
	case MAP_IGNORE:
		break;
	case MAP_CONVERT:
		remote_name = map->u.convert.remote_name;
		break;
	case MAP_KEEP:
		remote_name = attr_name;
		break;
	case MAP_RENAME:
		remote_name = map->u.rename.remote_name;
		break;
	case MAP_GENERATE:
		break;
	}

	switch (map->type) {
	case MAP_IGNORE:
		return 0;

	case MAP_CONVERT:
		if (map->u.convert.convert_remote == NULL) {
			ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
				  "Skipping attribute '%s': "
				  "'convert_remote' not set\n",
				  attr_name);
			return 0;
		}
		/* fall through */
	case MAP_KEEP:
	case MAP_RENAME:
		old = ldb_msg_find_element(remote, remote_name);
		if (old) {
			el = ldb_msg_el_map_remote(module, local, map, attr_name, old);
		} else {
			return LDB_ERR_NO_SUCH_ATTRIBUTE;
		}
		break;

	case MAP_GENERATE:
		if (map->u.generate.generate_local == NULL) {
			ldb_debug(module->ldb, LDB_DEBUG_ERROR, "ldb_map: "
				  "Skipping attribute '%s': "
				  "'generate_local' not set\n",
				  attr_name);
			return 0;
		}

		el = map->u.generate.generate_local(module, local, attr_name, remote);
		if (!el) {
			/* Generation failure is probably due to lack of source attributes */
			return LDB_ERR_NO_SUCH_ATTRIBUTE;
		}
		break;
	}

	if (el == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
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
	int i, ret;

	/* Perhaps we have a mapping for "*" */
	if (map && map->type == MAP_KEEP) {
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

	return 0;
}

/* Mapping messages
 * ================ */

/* Merge two local messages into a single one. */
static int ldb_msg_merge_local(struct ldb_module *module, struct ldb_message *msg1, struct ldb_message *msg2)
{
	int i, ret;

	for (i = 0; i < msg2->num_elements; i++) {
		ret = ldb_msg_replace(msg1, &msg2->elements[i]);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

/* Merge a local and a remote message into a single local one. */
static int ldb_msg_merge_remote(struct map_context *ac, struct ldb_message *local, 
				struct ldb_message *remote)
{
	int i, ret;
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

	return 0;
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
static BOOL ldb_parse_tree_check_splittable(const struct ldb_parse_tree *tree)
{
	const struct ldb_parse_tree *subtree = tree;
	BOOL negate = False;

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
			return True;	/* simple parse tree */
		}
	}

	return True;			/* no parse tree */
}

/* Collect a list of attributes required to match a given parse tree. */
static int ldb_parse_tree_collect_attrs(struct ldb_module *module, void *mem_ctx, const char ***attrs, const struct ldb_parse_tree *tree)
{
	const char **new_attrs;
	int i, ret;

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

	return -1;
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
	int i, j, ret=0;

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
	int i, j, ret=0;

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
	
	if (map->type == MAP_KEEP) {
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

	if (map->type == MAP_RENAME) {
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

	if (map->type == MAP_GENERATE) {
		ldb_debug(module->ldb, LDB_DEBUG_WARNING, "ldb_map: "
			  "Skipping attribute '%s': "
			  "'convert_operator' not set\n",
			  tree->u.equality.attr);
		*new = NULL;
		return 0;
	}

	return map_subtree_collect_remote_simple(module, mem_ctx, new, tree, map);
}

/* Split subtrees that query attributes in the local partition from
 * those that query the remote partition. */
static int ldb_parse_tree_partition(struct ldb_module *module, void *local_ctx, void *remote_ctx, struct ldb_parse_tree **local_tree, struct ldb_parse_tree **remote_tree, const struct ldb_parse_tree *tree)
{
	int ret;

	*local_tree = NULL;
	*remote_tree = NULL;

	/* No original tree */
	if (tree == NULL) {
		return 0;
	}

	/* Generate local tree */
	ret = map_subtree_select_local(module, local_ctx, local_tree, tree);
	if (ret) {
		return ret;
	}

	/* Generate remote tree */
	ret = map_subtree_collect_remote(module, remote_ctx, remote_tree, tree);
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

	/* Clear initial lists of partitioned attributes */

	/* Clear initial lists of partitioned attributes */

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

/* Pass a merged search result up the callback chain. */
int map_up_callback(struct ldb_context *ldb, const struct ldb_request *req, struct ldb_reply *ares)
{
	int i;

	/* No callback registered, stop */
	if (req->callback == NULL) {
		return LDB_SUCCESS;
	}

	/* Only records need special treatment */
	if (ares->type != LDB_REPLY_ENTRY) {
		return req->callback(ldb, req->context, ares);
	}

	/* Merged result doesn't match original query, skip */
	if (!ldb_match_msg(ldb, ares->message, req->op.search.tree, req->op.search.base, req->op.search.scope)) {
		ldb_debug(ldb, LDB_DEBUG_TRACE, "ldb_map: "
			  "Skipping record '%s': "
			  "doesn't match original search\n",
			  ldb_dn_linearize(ldb, ares->message->dn));
		return LDB_SUCCESS;
	}

	/* Limit result to requested attrs */
	if ((req->op.search.attrs) && (!ldb_attr_in_list(req->op.search.attrs, "*"))) {
		for (i = 0; i < ares->message->num_elements; ) {
			struct ldb_message_element *el = &ares->message->elements[i];
			if (!ldb_attr_in_list(req->op.search.attrs, el->name)) {
				ldb_msg_remove_element(ares->message, el);
			} else {
				i++;
			}
		}
	}

	return req->callback(ldb, req->context, ares);
}

/* Merge the remote and local parts of a search result. */
int map_local_merge_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct map_search_context *sc;
	int ret;

	if (context == NULL || ares == NULL) {
		ldb_set_errstring(ldb, talloc_asprintf(ldb, "ldb_map: "
						       "NULL Context or Result in `map_local_merge_callback`"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	sc = talloc_get_type(context, struct map_search_context);

	switch (ares->type) {
	case LDB_REPLY_ENTRY:
		/* We have already found a local record */
		if (sc->local_res) {
			ldb_set_errstring(ldb, talloc_asprintf(ldb, "ldb_map: "
							       "Too many results to base search for local entry"));
			talloc_free(ares);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		/* Store local result */
		sc->local_res = ares;

		/* Merge remote into local message */
		ret = ldb_msg_merge_local(sc->ac->module, ares->message, sc->remote_res->message);
		if (ret) {
			talloc_free(ares);
			return LDB_ERR_OPERATIONS_ERROR;
		}

		return map_up_callback(ldb, sc->ac->orig_req, ares);

	case LDB_REPLY_DONE:
		/* No local record found, continue with remote record */
		if (sc->local_res == NULL) {
			return map_up_callback(ldb, sc->ac->orig_req, sc->remote_res);
		}
		return LDB_SUCCESS;

	default:
		ldb_set_errstring(ldb, talloc_asprintf(ldb, "ldb_map: "
						       "Unexpected result type in base search for local entry"));
		talloc_free(ares);
		return LDB_ERR_OPERATIONS_ERROR;
	}
}

/* Search the local part of a remote search result. */
int map_remote_search_callback(struct ldb_context *ldb, void *context, struct ldb_reply *ares)
{
	struct map_context *ac;
	struct map_search_context *sc;
	struct ldb_request *req;
	int ret;

	if (context == NULL || ares == NULL) {
		ldb_set_errstring(ldb, talloc_asprintf(ldb, "ldb_map: "
						       "NULL Context or Result in `map_remote_search_callback`"));
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac = talloc_get_type(context, struct map_context);

	/* It's not a record, stop searching */
	if (ares->type != LDB_REPLY_ENTRY) {
		return map_up_callback(ldb, ac->orig_req, ares);
	}

	/* Map result record into a local message */
	ret = map_reply_remote(ac, ares);
	if (ret) {
		talloc_free(ares);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* There is no local db, stop searching */
	if (!map_check_local_db(ac->module)) {
		return map_up_callback(ldb, ac->orig_req, ares);
	}

	/* Prepare local search context */
	sc = map_init_search_context(ac, ares);
	if (sc == NULL) {
		talloc_free(ares);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	/* Prepare local search request */
	/* TODO: use GUIDs here instead? */

	ac->search_reqs = talloc_realloc(ac, ac->search_reqs, struct ldb_request *, ac->num_searches + 2);
	if (ac->search_reqs == NULL) {
		talloc_free(ares);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ac->search_reqs[ac->num_searches]
		= req = map_search_base_req(ac, ares->message->dn, 
					    NULL, NULL, sc, map_local_merge_callback);
	if (req == NULL) {
		talloc_free(sc);
		talloc_free(ares);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->num_searches++;
	ac->search_reqs[ac->num_searches] = NULL;

	return ldb_next_request(ac->module, req);
}

/* Search a record. */
int map_search(struct ldb_module *module, struct ldb_request *req)
{
	struct ldb_handle *h;
	struct map_context *ac;
	struct ldb_parse_tree *local_tree, *remote_tree;
	int ret;

	const char *wildcard[] = { "*", NULL };
	const char * const *attrs;

	/* Do not manipulate our control entries */
	if (ldb_dn_is_special(req->op.search.base))
		return ldb_next_request(module, req);

	/* No mapping requested, skip to next module */
	if ((req->op.search.base) && (!ldb_dn_check_local(module, req->op.search.base))) {
		return ldb_next_request(module, req);
	}

	/* TODO: How can we be sure about which partition we are
	 *	 targetting when there is no search base? */

	/* Prepare context and handle */
	h = map_init_handle(req, module);
	if (h == NULL) {
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac = talloc_get_type(h->private_data, struct map_context);

	ac->search_reqs = talloc_array(ac, struct ldb_request *, 2);
	if (ac->search_reqs == NULL) {
		talloc_free(h);
		return LDB_ERR_OPERATIONS_ERROR;
	}
	ac->num_searches = 1;
	ac->search_reqs[1] = NULL;

	/* Prepare the remote operation */
	ac->search_reqs[0] = talloc(ac, struct ldb_request);
	if (ac->search_reqs[0] == NULL) {
		goto oom;
	}

	*(ac->search_reqs[0]) = *req;	/* copy the request */

	ac->search_reqs[0]->handle = h;	/* return our own handle to deal with this call */

	ac->search_reqs[0]->context = ac;
	ac->search_reqs[0]->callback = map_remote_search_callback;

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
		goto failed;
	}

	ac->search_reqs[0]->op.search.attrs = ac->remote_attrs;

	/* Split local from remote tree */
	ret = ldb_parse_tree_partition(module, ac, ac->search_reqs[0], 
				       &local_tree, &remote_tree, 
				       req->op.search.tree);
	if (ret) {
		goto failed;
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
			goto failed;
		}

		local_tree->operation = LDB_OP_PRESENT;
		local_tree->u.present.attr = talloc_strdup(local_tree, IS_MAPPED);
	}
	if (remote_tree == NULL) {
		/* Construct default remote parse tree */
		remote_tree = ldb_parse_tree(ac->search_reqs[0], NULL);
		if (remote_tree == NULL) {
			goto failed;
		}
	}

	ac->local_tree = local_tree;
	ac->search_reqs[0]->op.search.tree = remote_tree;

	ldb_set_timeout_from_prev_req(module->ldb, req, ac->search_reqs[0]);

	h->state = LDB_ASYNC_INIT;
	h->status = LDB_SUCCESS;

	ac->step = MAP_SEARCH_REMOTE;

	ret = ldb_next_remote_request(module, ac->search_reqs[0]);
	if (ret == LDB_SUCCESS) {
		req->handle = h;
	}
	return ret;

oom:
	map_oom(module);
failed:
	talloc_free(h);
	return LDB_ERR_OPERATIONS_ERROR;
}
