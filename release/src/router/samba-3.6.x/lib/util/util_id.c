/*
   Unix SMB/CIFS implementation.

   Helper routines for uid and gid arrays

   Copyright (C) Guenther Deschner 2009

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

/****************************************************************************
 Add a gid to an array of gids if it's not already there.
****************************************************************************/

bool add_gid_to_array_unique(TALLOC_CTX *mem_ctx, gid_t gid,
			     gid_t **gids, uint32_t *num_gids)
{
	int i;

	if ((*num_gids != 0) && (*gids == NULL)) {
		/*
		 * A former call to this routine has failed to allocate memory
		 */
		return false;
	}

	for (i=0; i<*num_gids; i++) {
		if ((*gids)[i] == gid) {
			return true;
		}
	}

	*gids = talloc_realloc(mem_ctx, *gids, gid_t, *num_gids+1);
	if (*gids == NULL) {
		*num_gids = 0;
		return false;
	}

	(*gids)[*num_gids] = gid;
	*num_gids += 1;
	return true;
}

/****************************************************************************
 Add a uid to an array of uids if it's not already there.
****************************************************************************/

bool add_uid_to_array_unique(TALLOC_CTX *mem_ctx, uid_t uid,
			     uid_t **uids, uint32_t *num_uids)
{
	int i;

	if ((*num_uids != 0) && (*uids == NULL)) {
		/*
		 * A former call to this routine has failed to allocate memory
		 */
		return false;
	}

	for (i=0; i<*num_uids; i++) {
		if ((*uids)[i] == uid) {
			return true;
		}
	}

	*uids = talloc_realloc(mem_ctx, *uids, uid_t, *num_uids+1);
	if (*uids == NULL) {
		*num_uids = 0;
		return false;
	}

	(*uids)[*num_uids] = uid;
	*num_uids += 1;
	return true;
}
