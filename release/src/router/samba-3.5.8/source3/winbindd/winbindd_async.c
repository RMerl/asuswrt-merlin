/* 
   Unix SMB/CIFS implementation.

   Async helpers for blocking functions

   Copyright (C) Volker Lendecke 2005
   Copyright (C) Gerald Carter 2006

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
#include "winbindd.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_WINBIND

bool print_sidlist(TALLOC_CTX *mem_ctx, const DOM_SID *sids,
		   size_t num_sids, char **result, ssize_t *len)
{
	size_t i;
	size_t buflen = 0;

	*len = 0;
	*result = NULL;
	for (i=0; i<num_sids; i++) {
		fstring tmp;
		sprintf_append(mem_ctx, result, len, &buflen,
			       "%s\n", sid_to_fstring(tmp, &sids[i]));
	}

	if ((num_sids != 0) && (*result == NULL)) {
		return False;
	}

	return True;
}

bool parse_sidlist(TALLOC_CTX *mem_ctx, const char *sidstr,
		   DOM_SID **sids, size_t *num_sids)
{
	const char *p, *q;

	p = sidstr;
	if (p == NULL)
		return False;

	while (p[0] != '\0') {
		fstring tmp;
		size_t sidlen;
		DOM_SID sid;
		q = strchr(p, '\n');
		if (q == NULL) {
			DEBUG(0, ("Got invalid sidstr: %s\n", p));
			return False;
		}
		sidlen = PTR_DIFF(q, p);
		if (sidlen >= sizeof(tmp)-1) {
			return false;
		}
		memcpy(tmp, p, sidlen);
		tmp[sidlen] = '\0';
		q += 1;
		if (!string_to_sid(&sid, tmp)) {
			DEBUG(0, ("Could not parse sid %s\n", p));
			return False;
		}
		if (!NT_STATUS_IS_OK(add_sid_to_array(mem_ctx, &sid, sids,
						      num_sids)))
		{
			return False;
		}
		p = q;
	}
	return True;
}

enum winbindd_result winbindd_dual_ping(struct winbindd_domain *domain,
					struct winbindd_cli_state *state)
{
	return WINBINDD_OK;
}
