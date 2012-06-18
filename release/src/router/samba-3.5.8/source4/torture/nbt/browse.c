/* 
   Unix SMB/CIFS implementation.

   Browse service

   (C) Jelmer Vernooij 2005
   
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
#include "librpc/gen_ndr/nbt.h"
#include "libcli/resolve/resolve.h"
#include "torture/torture.h"

/*
  test nbt dgram operations
*/
bool torture_nbt_browse(struct torture_context *torture)
{
	const char *address;
	struct nbt_name name;
	TALLOC_CTX *mem_ctx = talloc_new(NULL);
	NTSTATUS status;
	bool ret = true;
	
	name.name = lp_workgroup();
	name.type = NBT_NAME_BROWSER;
	name.scope = NULL;

	/* do an initial name resolution to find its IP */
	status = resolve_name(&name, mem_ctx, &address, torture->ev);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to resolve %s - %s\n",
		       name.name, nt_errstr(status));
		talloc_free(mem_ctx);
		return false;
	}

	talloc_free(mem_ctx);

	return ret;
}
