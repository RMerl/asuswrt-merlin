/*
   Samba trivial allocation library - compat functions

   Copyright (C) Stefan Metzmacher 2009

     ** NOTE! The following LGPL license applies to the talloc
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
 * This file contains only function to build a
 * compat talloc.so.1 library on top of talloc.so.2
 */

#include "replace.h"
#include "talloc.h"

void *_talloc_reference(const void *context, const void *ptr);
void *_talloc_reference(const void *context, const void *ptr) {
	return _talloc_reference_loc(context, ptr,
				     "Called from talloc compat1 "
				     "_talloc_reference");
}

void *_talloc_steal(const void *new_ctx, const void *ptr);
void *_talloc_steal(const void *new_ctx, const void *ptr)
{
	return talloc_reparent(talloc_parent(ptr), new_ctx, ptr);
}

#undef talloc_free
int talloc_free(void *ptr);
int talloc_free(void *ptr)
{
	return talloc_unlink(talloc_parent(ptr), ptr);
}

