/* 
   Unix SMB/CIFS implementation.

   local testing of torture

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
#include "torture/torture.h"
#include "system/wait.h"
#include "lib/events/events.h"
#include "libcli/raw/libcliraw.h"
#include "torture/util.h"
#include "param/provision.h"

static bool test_tempdir(struct torture_context *tctx)
{
	char *location = NULL;
	TALLOC_CTX *mem_ctx = tctx;
	
	torture_assert_ntstatus_ok(tctx, torture_temp_dir(mem_ctx, "tempdir", &location), 
								"torture_temp_dir should return NT_STATUS_OK" );

	torture_assert(tctx, directory_exist(location), 
				   "created dir doesn't exist");
	return true;
}

static bool test_provision(struct torture_context *tctx)
{
	NTSTATUS status;
	struct provision_settings *settings = talloc_zero(tctx, struct provision_settings);
	struct provision_result result;
	char *targetdir = NULL;

	torture_assert_ntstatus_ok(tctx, torture_temp_dir(tctx, "torture_provision", &targetdir), 
				   "torture_temp_dir should return NT_STATUS_OK" );
	settings->targetdir = talloc_steal(settings, targetdir);

	settings->site_name = "SOME-SITE-NAME";
	settings->root_dn_str = "DC=EXAMPLE,DC=COM";
	settings->domain_dn_str = "DC=EXAMPLE,DC=COM";
	settings->config_dn_str = NULL;
	settings->schema_dn_str = NULL;
	settings->invocation_id = NULL;
	settings->netbios_name = "FOO";
	settings->realm = "EXAMPLE.COM";
	settings->domain = "EXAMPLE";
	settings->netbios_name = "torture";
	settings->ntds_dn_str = NULL;
	settings->machine_password = "geheim";

	status = provision_bare(settings, tctx->lp_ctx, settings, &result);
			
	torture_assert_ntstatus_ok(tctx, status, "provision");

	torture_assert_str_equal(tctx, result.domaindn, "DC=EXAMPLE,DC=COM", 
				 "domaindn incorrect");

	return true;
}

struct torture_suite *torture_local_torture(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, 
													   "TORTURE");

	torture_suite_add_simple_test(suite, "tempdir", test_tempdir);
	torture_suite_add_simple_test(suite, "provision", test_provision);

	return suite;
}
