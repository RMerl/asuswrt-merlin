/*
   Unix SMB/CIFS implementation.

   security access checking routines

   Copyright (C) Nadezhda Ivanova 2009

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
 *  Description: Contains data handler functions for
 *               the object tree that must be constructed to perform access checks.
 *               The object tree is an unbalanced tree of depth 3, indexed by
 *               object type guid. Perhaps a different data structure
 *               should be concidered later to improve performance
 *
 *  Author: Nadezhda Ivanova
 */
#include "includes.h"
#include "libcli/security/security.h"
#include "lib/util/dlinklist.h"
#include "librpc/ndr/libndr.h"

/* Adds a new node to the object tree. If attributeSecurityGUID is not zero and
 * has already been added to the tree, the new node is added as a child of that node
 * In all other cases as a child of the root
 */

struct object_tree * insert_in_object_tree(TALLOC_CTX *mem_ctx,
					   const struct GUID *schemaGUIDID,
					   const struct GUID *attributeSecurityGUID,
					   uint32_t init_access,
					   struct object_tree *root)
{
	struct object_tree * parent = NULL;
	struct object_tree * new_node;

	new_node = talloc(mem_ctx, struct object_tree);
	if (!new_node)
		return NULL;
	memset(new_node, 0, sizeof(struct object_tree));
	new_node->remaining_access = init_access;

	if (!root){
		memcpy(&new_node->guid, schemaGUIDID, sizeof(struct GUID));
		return new_node;
	}

	if (attributeSecurityGUID && !GUID_all_zero(attributeSecurityGUID)){
		parent = get_object_tree_by_GUID(root, attributeSecurityGUID);
		memcpy(&new_node->guid, attributeSecurityGUID, sizeof(struct GUID));
	}
	else
		memcpy(&new_node->guid, schemaGUIDID, sizeof(struct GUID));

	if (!parent)
		parent = root;

	new_node->remaining_access = init_access;
	DLIST_ADD(parent, new_node);
	return new_node;
}

/* search by GUID */
struct object_tree * get_object_tree_by_GUID(struct object_tree *root,
					     const struct GUID *guid)
{
	struct object_tree *p;
	struct object_tree *result = NULL;

	if (!root || GUID_equal(&root->guid, guid))
		result = root;
	else{
	for (p = root->children; p != NULL; p = p->next)
		if ((result = get_object_tree_by_GUID(p, guid)))
			break;
	}

	return result;
}

/* Change the granted access per each ACE */

void object_tree_modify_access(struct object_tree *root,
			       uint32_t access)
{
	struct object_tree *p;
	if (root){
		root->remaining_access &= ~access;
	}

	for (p = root->children; p != NULL; p = p->next)
		object_tree_modify_access(p, access);
}
