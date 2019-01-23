/*
   ldb database library

   Copyright (C) Andrew Tridgell  2010

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
 *  Component: ldb options[] handling
 *
 *  Author: Andrew Tridgell
 */

#include "ldb_private.h"

/*
  find an option within an options array

  accepts the following forms:

     NAME
     NAME:value
     NAME=value

  returns a pointer into an element of the options[] array, or NULL is
  not found.

  For the NAME form, returns a pointer to an empty string (thus
  allowing for boolean options).
 */
const char *ldb_options_find(struct ldb_context *ldb, const char *options[],
				       const char *option_name)
{
	size_t len = strlen(option_name);
	int i;

	if (options == NULL) {
		return NULL;
	}

	for (i=0; options[i]; i++) {
		if (strncmp(option_name, options[i], len) != 0) {
			continue;
		}
		if (options[i][len] == ':' || options[i][len] == '=') {
			return &options[i][len+1];
		}
		if (options[i][len] == 0) {
			return &options[i][len];
		}
	}

	return NULL;
}
