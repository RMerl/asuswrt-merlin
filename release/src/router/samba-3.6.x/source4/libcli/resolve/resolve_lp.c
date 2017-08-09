/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Jelmer Vernooij <jelmer@samba.org> 2007
   
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
#include "libcli/resolve/resolve.h"
#include "param/param.h"

struct resolve_context *lpcfg_resolve_context(struct loadparm_context *lp_ctx)
{
	const char **methods = lpcfg_name_resolve_order(lp_ctx);
	int i;
	struct resolve_context *ret = resolve_context_init(lp_ctx);

	if (ret == NULL)
		return NULL;

	for (i = 0; methods != NULL && methods[i] != NULL; i++) {
		if (!strcmp(methods[i], "wins")) {
			resolve_context_add_wins_method_lp(ret, lp_ctx);
		} else if (!strcmp(methods[i], "bcast")) {
			resolve_context_add_bcast_method_lp(ret, lp_ctx);
		} else if (!strcmp(methods[i], "file")) {
			resolve_context_add_file_method_lp(ret, lp_ctx);
		} else if (!strcmp(methods[i], "host")) {
			resolve_context_add_host_method(ret);
		} else {
			DEBUG(0, ("Unknown resolve method '%s'\n", methods[i]));
		}
	}

	return ret;
}
