/*
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Volker Lendecke 2010

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
#include "../librpc/gen_ndr/netlogon.h"
#include "../libcli/security/security.h"
#include "rpc_client/util_netlogon.h"

#define COPY_LSA_STRING(mem_ctx, in, out, name) do { \
	if (in->name.string) { \
		out->name.string = talloc_strdup(mem_ctx, in->name.string); \
		NT_STATUS_HAVE_NO_MEMORY(out->name.string); \
	} \
} while (0)

NTSTATUS copy_netr_SamBaseInfo(TALLOC_CTX *mem_ctx,
			       const struct netr_SamBaseInfo *in,
			       struct netr_SamBaseInfo *out)
{
	/* first copy all, then realloc pointers */
	*out = *in;

	COPY_LSA_STRING(mem_ctx, in, out, account_name);
	COPY_LSA_STRING(mem_ctx, in, out, full_name);
	COPY_LSA_STRING(mem_ctx, in, out, logon_script);
	COPY_LSA_STRING(mem_ctx, in, out, profile_path);
	COPY_LSA_STRING(mem_ctx, in, out, home_directory);
	COPY_LSA_STRING(mem_ctx, in, out, home_drive);

	if (in->groups.count) {
		out->groups.rids = (struct samr_RidWithAttribute *)
			talloc_memdup(mem_ctx, in->groups.rids,
				(sizeof(struct samr_RidWithAttribute) *
					in->groups.count));
		NT_STATUS_HAVE_NO_MEMORY(out->groups.rids);
	}

	COPY_LSA_STRING(mem_ctx, in, out, logon_server);
	COPY_LSA_STRING(mem_ctx, in, out, domain);

	if (in->domain_sid) {
		out->domain_sid = dom_sid_dup(mem_ctx, in->domain_sid);
		NT_STATUS_HAVE_NO_MEMORY(out->domain_sid);
	}

	return NT_STATUS_OK;
}
