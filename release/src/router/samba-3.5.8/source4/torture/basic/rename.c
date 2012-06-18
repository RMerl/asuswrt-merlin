/* 
   Unix SMB/CIFS implementation.

   rename testing

   Copyright (C) Andrew Tridgell 2003
   
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
#include "libcli/libcli.h"
#include "torture/torture.h"
#include "torture/util.h"

/*
  Test rename on files open with share delete and no share delete.
 */
bool torture_test_rename(struct torture_context *tctx, 
			 struct smbcli_state *cli1)
{
	const char *fname = "\\test.txt";
	const char *fname1 = "\\test1.txt";
	int fnum1;

	smbcli_unlink(cli1->tree, fname);
	smbcli_unlink(cli1->tree, fname1);
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_READ, 
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ, 
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "First open failed - %s", 
		       smbcli_errstr(cli1->tree)));

	torture_assert(tctx, NT_STATUS_IS_ERR(smbcli_rename(cli1->tree, fname, fname1)), 
					"First rename succeeded - this should have failed !");

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close - 1 failed (%s)", smbcli_errstr(cli1->tree)));

	smbcli_unlink(cli1->tree, fname);
	smbcli_unlink(cli1->tree, fname1);
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_READ, 
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_DELETE|NTCREATEX_SHARE_ACCESS_READ, 
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, 
				   "Second open failed - %s", smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_rename(cli1->tree, fname, fname1),
		talloc_asprintf(tctx, 
				"Second rename failed - this should have succeeded - %s", 
		       smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, 
		"close - 2 failed (%s)", smbcli_errstr(cli1->tree)));

	smbcli_unlink(cli1->tree, fname);
	smbcli_unlink(cli1->tree, fname1);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_STD_READ_CONTROL, 
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_NONE, 
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "Third open failed - %s", 
			smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_rename(cli1->tree, fname, fname1),
		talloc_asprintf(tctx, "Third rename failed - this should have succeeded - %s", 
		       smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1), 
		talloc_asprintf(tctx, "close - 3 failed (%s)", smbcli_errstr(cli1->tree)));

	smbcli_unlink(cli1->tree, fname);
	smbcli_unlink(cli1->tree, fname1);

	return true;
}

