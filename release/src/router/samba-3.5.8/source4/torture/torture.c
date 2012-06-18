/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-2003
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
#include "system/time.h"
#include "torture/torture.h"
#include "../lib/util/dlinklist.h"
#include "param/param.h"
#include "lib/cmdline/popt_common.h"
#include "torture/smbtorture.h"

_PUBLIC_ int torture_numops=10;
_PUBLIC_ int torture_entries=1000;
_PUBLIC_ int torture_failures=1;
_PUBLIC_ int torture_seed=0;
_PUBLIC_ int torture_numasync=100;

struct torture_suite *torture_root = NULL;

bool torture_register_suite(struct torture_suite *suite)
{
	if (!suite)
		return true;

	if (torture_root == NULL)
		torture_root = talloc_zero(talloc_autofree_context(), struct torture_suite);

	return torture_suite_add_suite(torture_root, suite);
}

#ifndef ENABLE_LIBNETAPI
NTSTATUS torture_libnetapi_init(void)
{
	return NT_STATUS_OK;
}
#endif

_PUBLIC_ int torture_init(void)
{
	extern NTSTATUS torture_base_init(void);
	extern NTSTATUS torture_ldap_init(void);
	extern NTSTATUS torture_local_init(void);
	extern NTSTATUS torture_nbt_init(void);
	extern NTSTATUS torture_nbench_init(void);
	extern NTSTATUS torture_rap_init(void);
 	extern NTSTATUS torture_rpc_init(void);
 	extern NTSTATUS torture_ntp_init(void);
	extern NTSTATUS torture_smb2_init(void);
	extern NTSTATUS torture_net_init(void);
	extern NTSTATUS torture_libnetapi_init(void);
	extern NTSTATUS torture_raw_init(void);
	extern NTSTATUS torture_unix_init(void);
	extern NTSTATUS torture_winbind_init(void);
	init_module_fn static_init[] = { STATIC_smbtorture_MODULES };
	init_module_fn *shared_init = load_samba_modules(NULL, cmdline_lp_ctx, "smbtorture");

	run_init_functions(static_init);
	run_init_functions(shared_init);

	talloc_free(shared_init);

	return 0;
}
