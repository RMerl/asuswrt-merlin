/*
   Unix SMB/CIFS implementation.

   Utility functions that are not related with async operations.

   Copyright (C) Andrew Bartlett <abartlet@samba.org> 2005

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
#include "param/param.h"


/* Split a domain\\user string into it's parts, because the client supplies it
 * as one string.
 * TODO: We probably will need to handle other formats later. */

bool wb_samba3_split_username(TALLOC_CTX *mem_ctx, struct loadparm_context *lp_ctx,
			      const char *domuser,
				 char **domain, char **user)
{
	char *p = strchr(domuser, *lpcfg_winbind_separator(lp_ctx));

	if (p == NULL) {
		*domain = talloc_strdup(mem_ctx, lpcfg_workgroup(lp_ctx));
	} else {
		*domain = talloc_strndup(mem_ctx, domuser,
					 PTR_DIFF(p, domuser));
		domuser = p+1;
	}

	*user = talloc_strdup(mem_ctx, domuser);

	return ((*domain != NULL) && (*user != NULL));
}


