/* 
   ldb database library

   Copyright (C) Simo Sorce  2005

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
 *  Name: ldb_controls.c
 *
 *  Component: ldb controls utility functions
 *
 *  Description: helper functions for control modules
 *
 *  Author: Simo Sorce
 */

#include "includes.h"
#include "ldb/include/includes.h"

/* check if a control with the specified "oid" exist and return it */
/* returns NULL if not found */
struct ldb_control *get_control_from_list(struct ldb_control **controls, const char *oid)
{
	int i;

	/* check if there's a paged request control */
	if (controls != NULL) {
		for (i = 0; controls[i]; i++) {
			if (strcmp(oid, controls[i]->oid) == 0) {
				break;
			}
		}

		return controls[i];
	}

	return NULL;
}

/* saves the current controls list into the "saver" and replace the one in req with a new one excluding
the "exclude" control */
/* returns False on error */
int save_controls(struct ldb_control *exclude, struct ldb_request *req, struct ldb_control ***saver)
{
	struct ldb_control **lcs;
	int i, j;

	*saver = req->controls;
	for (i = 0; req->controls[i]; i++);
	if (i == 1) {
		req->controls = NULL;
		return 1;
	}

	lcs = talloc_array(req, struct ldb_control *, i);
	if (!lcs) {
		return 0;
	}

	for (i = 0, j = 0; (*saver)[i]; i++) {
		if (exclude == (*saver)[i]) continue;
		lcs[j] = (*saver)[i];
		j++;
	}
	lcs[j] = NULL;

	req->controls = lcs;
	return 1;
}

/* check if there's any control marked as critical in the list */
/* return True if any, False if none */
int check_critical_controls(struct ldb_control **controls)
{
	int i;

	if (controls == NULL) {
		return 0;
	}

	for (i = 0; controls[i]; i++) {
		if (controls[i]->critical) {
			return 1;
		}
	}

	return 0;
}
