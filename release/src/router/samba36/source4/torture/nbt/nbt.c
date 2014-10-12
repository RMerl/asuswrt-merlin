/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Jelmer Vernooij 2006
   
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
#include "../libcli/nbt/libnbt.h"
#include "torture/torture.h"
#include "torture/nbt/proto.h"
#include "torture/smbtorture.h"
#include "libcli/resolve/resolve.h"
#include "param/param.h"

struct nbt_name_socket *torture_init_nbt_socket(struct torture_context *tctx)
{
	return nbt_name_socket_init(tctx, tctx->ev);
}

bool torture_nbt_get_name(struct torture_context *tctx, 
			  struct nbt_name *name, 
			  const char **address)
{
	make_nbt_name_server(name, strupper_talloc(tctx, 
			     torture_setting_string(tctx, "host", NULL)));

	/* do an initial name resolution to find its IP */
	torture_assert_ntstatus_ok(tctx, 
				   resolve_name(lpcfg_resolve_context(tctx->lp_ctx), name, tctx, address, tctx->ev),
				   talloc_asprintf(tctx, 
						   "Failed to resolve %s", name->name));
	
	return true;
}

NTSTATUS torture_nbt_init(void)
{
	struct torture_suite *suite = torture_suite_create(
		talloc_autofree_context(), "nbt");
	/* nbt tests */
	torture_suite_add_suite(suite, torture_nbt_register(suite));
	torture_suite_add_suite(suite, torture_nbt_wins(suite));
	torture_suite_add_suite(suite, torture_nbt_dgram(suite));
	torture_suite_add_suite(suite, torture_nbt_winsreplication(suite));
	torture_suite_add_suite(suite, torture_bench_nbt(suite));
	torture_suite_add_suite(suite, torture_bench_wins(suite));

	suite->description = talloc_strdup(suite, 
					 "NetBIOS over TCP/IP and WINS tests");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
