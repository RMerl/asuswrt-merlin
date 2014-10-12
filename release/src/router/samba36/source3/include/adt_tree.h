/* 
 *  Unix SMB/CIFS implementation.
 *  Generic Abstract Data Types
 *  Copyright (C) Gerald Carter                     2002-2005.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ADT_TREE_H
#define ADT_TREE_H

struct sorted_tree;

/* 
 * API
 */

/* create a new tree, talloc_free() to throw it away */

struct sorted_tree *pathtree_init(void *data_p);

/* add a new path component */

WERROR pathtree_add(struct sorted_tree *tree, const char *path, void *data_p );

/* search path */

void *pathtree_find(struct sorted_tree *tree, char *key );

/* debug (print) functions */

void pathtree_print_keys(struct sorted_tree *tree, int debug );


#endif
