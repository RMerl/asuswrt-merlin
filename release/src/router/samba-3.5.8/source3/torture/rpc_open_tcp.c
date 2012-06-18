/*
 * Unix SMB/CIFS implementation.
 * test program for rpc_pipe_open_tcp().
 *
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

#include "librpc/gen_ndr/ndr_dfs.h"
#include "librpc/gen_ndr/ndr_drsuapi.h"
#include "librpc/gen_ndr/ndr_dssetup.h"
#include "librpc/gen_ndr/ndr_echo.h"
#include "librpc/gen_ndr/ndr_epmapper.h"
#include "librpc/gen_ndr/ndr_eventlog.h"
#include "librpc/gen_ndr/ndr_initshutdown.h"
#include "librpc/gen_ndr/ndr_lsa.h"
#include "librpc/gen_ndr/ndr_netlogon.h"
#include "librpc/gen_ndr/ndr_ntsvcs.h"
#include "librpc/gen_ndr/ndr_samr.h"
#include "librpc/gen_ndr/ndr_srvsvc.h"
#include "librpc/gen_ndr/ndr_svcctl.h"
#include "librpc/gen_ndr/ndr_winreg.h"
#include "librpc/gen_ndr/ndr_wkssvc.h"

extern const struct ndr_interface_table ndr_table_netdfs;
extern const struct ndr_interface_table ndr_table_drsuapi;
extern const struct ndr_interface_table ndr_table_dssetup;
extern const struct ndr_interface_table ndr_table_rpcecho;
extern const struct ndr_interface_table ndr_table_epmapper;
extern const struct ndr_interface_table ndr_table_eventlog;
extern const struct ndr_interface_table ndr_table_initshutdown;
extern const struct ndr_interface_table ndr_table_lsarpc;
extern const struct ndr_interface_table ndr_table_netlogon;
extern const struct ndr_interface_table ndr_table_ntsvcs;
extern const struct ndr_interface_table ndr_table_samr;
extern const struct ndr_interface_table ndr_table_srvsvc;
extern const struct ndr_interface_table ndr_table_svcctl;
extern const struct ndr_interface_table ndr_table_winreg;
extern const struct ndr_interface_table ndr_table_wkssvc;

const struct ndr_interface_table *tables[] = {
	&ndr_table_netdfs,
	&ndr_table_drsuapi,
	&ndr_table_dssetup,
	&ndr_table_rpcecho,
	&ndr_table_epmapper,
	&ndr_table_eventlog,
	&ndr_table_initshutdown,
	&ndr_table_lsarpc,
	&ndr_table_netlogon,
	&ndr_table_ntsvcs,
	&ndr_table_samr,
	&ndr_table_srvsvc,
	&ndr_table_svcctl,
	&ndr_table_winreg,
	&ndr_table_wkssvc,
	NULL
};

int main(int argc, const char **argv)
{
	const struct ndr_interface_table **table;
	NTSTATUS status;
	struct rpc_pipe_client *rpc_pipe = NULL;
	TALLOC_CTX *mem_ctx = talloc_stackframe();

	if (argc != 3) {
		d_printf("USAGE: %s interface host\n", argv[0]);
		return -1;
	}

	for (table = tables; *table != NULL; table++) {
		if (strequal((*table)->name, argv[1])) {
			break;
		}
	}

	if (*table == NULL) {
		d_printf("ERROR: unknown interface '%s' given\n", argv[1]);
		return -1;
	}

	status = rpc_pipe_open_tcp(mem_ctx, argv[2], &((*table)->syntax_id),
				   &rpc_pipe);
	if (!NT_STATUS_IS_OK(status)) {
		d_printf("ERROR calling rpc_pipe_open_tcp(): %s\n",
			nt_errstr(status));
		return -1;
	}

	TALLOC_FREE(mem_ctx);

	return 0;
}

