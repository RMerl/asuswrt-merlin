/*
   Unix SMB/CIFS implementation.

   DRSUAPI utility functions to be used in torture tests

   Copyright (C) Kamen Mazdrashki <kamen.mazdrashki@postpath.com> 2009

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
#include "torture/smbtorture.h"
#include "torture/rpc/drsuapi.h"
#include "dsdb/samdb/samdb.h"
#include "torture/drs/proto.h"

/**
 * DRSUAPI tests to be executed remotely
 */
static struct torture_suite * torture_drs_rpc_suite(TALLOC_CTX *mem_ctx,
                                                    const char *suite_name)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, suite_name);

	torture_drs_rpc_dssync_tcase(suite);
	torture_drs_rpc_dsintid_tcase(suite);

	suite->description = talloc_strdup(suite,
	                                   "DRSUAPI RPC Tests Suite");

	return suite;
}

/**
 * DRSUAPI tests to be executed remotely
 */
static struct torture_suite * torture_drs_unit_suite(TALLOC_CTX *mem_ctx,
                                                     const char *suite_name)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, suite_name);

	torture_drs_unit_prefixmap(suite);
	torture_drs_unit_schemainfo(suite);

	suite->description = talloc_strdup(suite,
	                                   "DRSUAPI Unit Tests Suite");

	return suite;
}

/**
 * DRSUAPI torture module initialization
 */
NTSTATUS torture_drs_init(void)
{
	struct torture_suite *suite;
	TALLOC_CTX *mem_ctx = talloc_autofree_context();

	/* register RPC related test cases */
	suite = torture_drs_rpc_suite(mem_ctx, "drs.rpc");
	if (!suite) return NT_STATUS_NO_MEMORY;
	torture_register_suite(suite);

	/* register DRS Unit test cases */
	suite = torture_drs_unit_suite(mem_ctx, "drs.unit");
	if (!suite) return NT_STATUS_NO_MEMORY;
	torture_register_suite(suite);

	return NT_STATUS_OK;
}
