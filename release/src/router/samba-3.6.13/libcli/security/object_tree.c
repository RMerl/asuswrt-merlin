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
#include "librpc/ndr/libndr.h"

/* Adds a new node to the object tree. If attributeSecurityGUID is not zero and
 * has already been added to the tree, the new node is added as a child of that node
 * In all other cases as a child of the root
 */

bool insert_in_object_tree(TALLOC_CTX *mem_ctx,
			  const struct GUID *guid,
			  uint32_t init_access,
			  struct object_tree **root,
			  struct object_tree **new_node)
{
	if (!guid || GUID_all_zero(guid)){
		return true;
	}

	if (!*root){
		*root = talloc_zero(mem_ctx, struct object_tree);
		if (!*root) {
			return false;
		}
		(*root)->guid = *guid;
		*new_node = *root;
		return true;
	}

	if (!(*root)->children) {
		(*root)->children = talloc_array(mem_ctx, struct object_tree, 1);
		(*root)->children[0].guid = *guid;
		(*root)->children[0].num_of_children = 0;
		(*root)->children[0].children = NULL;
		(*root)->num_of_children++;
		(*root)->children[0].remaining_access = init_access;
		*new_node = &((*root)->children[0]);
		return true;
	}
	else {
		int i;
		for (i = 0; i < (*root)->num_of_children; i++) {
			if (GUID_equal(&((*root)->children[i].guid), guid)) {
				*new_node = &((*root)->children[i]);
				return true;
			}
		}
		(*root)->children = talloc_realloc(mem_ctx, (*root)->children, struct object_tree,
						   (*root)->num_of_children +1);
		(*root)->children[(*root)->num_of_children].guid = *guid;
		(*root)->children[(*root)->num_of_children].remaining_access = init_access;
		*new_node = &((*root)->children[(*root)->num_of_children]);
		(*root)->num_of_children++;
		return true;
	}
}

/* search by GUID */
struct object_tree *get_object_tree_by_GUID(struct object_tree *root,
					     const struct GUID *guid)
{
	struct object_tree *result = NULL;
	int i;

	if (!root || GUID_equal(&root->guid, guid)) {
		result = root;
		return result;
	}
	else if (root->num_of_children > 0) {
		for (i = 0; i < root->num_of_children; i++) {
		if ((result = get_object_tree_by_GUID(&root->children[i], guid)))
			break;
		}
	}
	return result;
}

/* Change the granted access per each ACE */

void object_tree_modify_access(struct object_tree *root,
			       uint32_t access_mask)
{
	root->remaining_access &= ~access_mask;
	if (root->num_of_children > 0) {
		int i;
		for (i = 0; i < root->num_of_children; i++) {
			object_tree_modify_access(&root->children[i], access_mask);
		}
	}
}
