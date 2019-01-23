/* 
   Unix SMB/CIFS implementation.
   RPC pipe client

   Copyright (C) Gerald Carter 2002
   Copyright (C) Guenther Deschner 2008

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
#include "rpcclient.h"
#include "../librpc/gen_ndr/ndr_dssetup_c.h"

/* Look up domain related information on a remote host */

static WERROR cmd_ds_dsrole_getprimarydominfo(struct rpc_pipe_client *cli,
					      TALLOC_CTX *mem_ctx, int argc,
					      const char **argv)
{
	struct dcerpc_binding_handle *b = cli->binding_handle;
	NTSTATUS status;
	WERROR werr;
	union dssetup_DsRoleInfo info;

	status = dcerpc_dssetup_DsRoleGetPrimaryDomainInformation(b, mem_ctx,
								  DS_ROLE_BASIC_INFORMATION,
								  &info,
								  &werr);
	if (!NT_STATUS_IS_OK(status)) {
		return ntstatus_to_werror(status);
	}

	if (!W_ERROR_IS_OK(werr)) {
		return werr;
	}

	printf ("Machine Role = [%d]\n", info.basic.role);

	if (info.basic.flags & DS_ROLE_PRIMARY_DS_RUNNING) {
		printf("Directory Service is running.\n");
		printf("Domain is in %s mode.\n",
			(info.basic.flags & DS_ROLE_PRIMARY_DS_MIXED_MODE) ? "mixed" : "native" );
	} else {
		printf("Directory Service not running on server\n");
	}

	return werr;
}

/* List of commands exported by this module */

struct cmd_set ds_commands[] = {

	{ "LSARPC-DS" },

	{ "dsroledominfo",   RPC_RTYPE_WERROR, NULL, cmd_ds_dsrole_getprimarydominfo, &ndr_table_dssetup.syntax_id, NULL, "Get Primary Domain Information", "" },

{ NULL }
};
