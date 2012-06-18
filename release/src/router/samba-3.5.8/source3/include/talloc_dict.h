/*
   Unix SMB/CIFS implementation.
   Little dictionary style data structure based on dbwrap_rbt
   Copyright (C) Volker Lendecke 2009

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

#ifndef __TALLOC_DICT_H__
#define __TALLOC_DICT_H__

#include "includes.h"

struct talloc_dict;

/*
 * Create a talloc_dict structure.
 */

struct talloc_dict *talloc_dict_init(TALLOC_CTX *mem_ctx);

/*
 * Add a talloced object to the dict. Nulls out the pointer to indicate that
 * the talloc ownership has been taken. If an object for "key" already exists,
 * the existing object is talloc_free()ed and overwritten by the new
 * object. If "data" is NULL, object for key "key" is deleted. Return false
 * for "no memory".
 */

bool talloc_dict_set(struct talloc_dict *dict, DATA_BLOB key, void *data);

/*
 * Fetch a talloced object. If "mem_ctx!=NULL", talloc_move the object there
 * and delete it from the dict.
 */

void *talloc_dict_fetch(struct talloc_dict *dict, DATA_BLOB key,
			TALLOC_CTX *mem_ctx);

/*
 * Traverse a talloc_dict. If "fn" returns non-null, quit the traverse
 */

int talloc_dict_traverse(struct talloc_dict *dict,
			 int (*fn)(DATA_BLOB key, void *data,
				   void *private_data),
			 void *private_data);

#endif
