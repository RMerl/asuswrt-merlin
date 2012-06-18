/* 
   Unix SMB/CIFS implementation.

   delete on close testing

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
#include "system/filesys.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"

#include "torture/raw/proto.h"

static bool check_delete_on_close(struct torture_context *tctx, 
								  struct smbcli_state *cli, int fnum,
								  const char *fname, bool expect_it, 
								  const char *where)
{
	union smb_search_data data;
	NTSTATUS status;

	time_t c_time, a_time, m_time;
	size_t size;
	uint16_t mode;

	status = torture_single_search(cli, tctx,
				       fname,
				       RAW_SEARCH_TRANS2,
				       RAW_SEARCH_DATA_FULL_DIRECTORY_INFO,
				       FILE_ATTRIBUTE_DIRECTORY,
				       &data);
	torture_assert_ntstatus_ok(tctx, status, 
		talloc_asprintf(tctx, "single_search failed (%s)", where));

	if (fnum != -1) {
		union smb_fileinfo io;
		int nlink = expect_it ? 0 : 1;

		io.all_info.level = RAW_FILEINFO_ALL_INFO;
		io.all_info.in.file.fnum = fnum;

		status = smb_raw_fileinfo(cli->tree, tctx, &io);
		torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, 
					"qfileinfo failed (%s)", where));

		torture_assert(tctx, expect_it == io.all_info.out.delete_pending, 
			talloc_asprintf(tctx, 
			"%s - Expected del_on_close flag %d, qfileinfo/all_info gave %d",
			       where, expect_it, io.all_info.out.delete_pending));

		torture_assert(tctx, nlink == io.all_info.out.nlink, 
			talloc_asprintf(tctx, 
				"%s - Expected nlink %d, qfileinfo/all_info gave %d",
			       where, nlink, io.all_info.out.nlink));

		io.standard_info.level = RAW_FILEINFO_STANDARD_INFO;
		io.standard_info.in.file.fnum = fnum;

		status = smb_raw_fileinfo(cli->tree, tctx, &io);
		torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, "qpathinfo failed (%s)", where));

		torture_assert(tctx, expect_it == io.standard_info.out.delete_pending,
			talloc_asprintf(tctx, "%s - Expected del_on_close flag %d, qfileinfo/standard_info gave %d\n",
			       where, expect_it, io.standard_info.out.delete_pending));

		torture_assert(tctx, nlink == io.standard_info.out.nlink,
			talloc_asprintf(tctx, "%s - Expected nlink %d, qfileinfo/standard_info gave %d",
			       where, nlink, io.all_info.out.nlink));
	}

	status = smbcli_qpathinfo(cli->tree, fname,
				  &c_time, &a_time, &m_time,
				  &size, &mode);

	if (expect_it) {
		torture_assert_ntstatus_equal(tctx, status, NT_STATUS_DELETE_PENDING,
			"qpathinfo did not give correct error code");
	} else {
		torture_assert_ntstatus_ok(tctx, status, 
			talloc_asprintf(tctx, "qpathinfo failed (%s)", where));
	}

	return true;
}

#define CHECK_STATUS(_cli, _expected) \
	torture_assert_ntstatus_equal(tctx, _cli->tree->session->transport->error.e.nt_status, _expected, \
		 "Incorrect status")

static const char *fname = "\\delete.file";
static const char *fname_new = "\\delete.new";
static const char *dname = "\\delete.dir";

static void del_clean_area(struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);

	smbcli_deltree(cli1->tree, dname);
	smbcli_setatr(cli1->tree, fname, 0, 0);
	smbcli_unlink(cli1->tree, fname);
	smbcli_setatr(cli1->tree, fname_new, 0, 0);
	smbcli_unlink(cli1->tree, fname_new);
}

/* Test 1 - this should delete the file on close. */

static bool deltest1(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;

	del_clean_area(cli1, cli2);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_DELETE, NTCREATEX_DISP_OVERWRITE_IF, 
				      NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));
	
	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1), 
		talloc_asprintf(tctx, "close failed (%s)", smbcli_errstr(cli1->tree)));

	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum1 == -1, talloc_asprintf(tctx, "open of %s succeeded (should fail)", 
		       fname));

	return true;
}

/* Test 2 - this should delete the file on close. */
static bool deltest2(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;

	del_clean_area(cli1, cli2);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL, NTCREATEX_SHARE_ACCESS_NONE, 
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
	
	torture_assert(tctx, fnum1 != -1, 
		talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));
	
	torture_assert_ntstatus_ok(tctx, smbcli_nt_delete_on_close(cli1->tree, fnum1, true), 
		talloc_asprintf(tctx, "setting delete_on_close failed (%s)", 
		       smbcli_errstr(cli1->tree)));
	
	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1), 
		talloc_asprintf(tctx, "close failed (%s)", 
		       smbcli_errstr(cli1->tree)));
	
	fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY, DENY_NONE);
	if (fnum1 != -1) {
		printf("(%s) open of %s succeeded should have been deleted on close !\n", 
		       __location__, fname);
		if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
			printf("(%s) close failed (%s)\n", 
			       __location__, smbcli_errstr(cli1->tree));
			return false;
		}
		smbcli_unlink(cli1->tree, fname);
	}
	return true;
}

/* Test 3 - ... */
static bool deltest3(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;

	del_clean_area(cli1, cli2);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE, 
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		        fname, smbcli_errstr(cli1->tree)));

	/* This should fail with a sharing violation - open for delete is only compatible
	   with SHARE_DELETE. */

	fnum2 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_READ, 
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE, 
				      NTCREATEX_DISP_OPEN, 0, 0);

	torture_assert(tctx, fnum2 == -1, 
		talloc_asprintf(tctx, "open  - 2 of %s succeeded - should have failed.", 
		       fname));

	/* This should succeed. */

	fnum2 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_READ, 
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE|NTCREATEX_SHARE_ACCESS_DELETE, 
				      NTCREATEX_DISP_OPEN, 0, 0);

	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open  - 2 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, 
							   smbcli_nt_delete_on_close(cli1->tree, fnum1, true),
							   talloc_asprintf(tctx, "setting delete_on_close failed (%s)", 
		       				   smbcli_errstr(cli1->tree)));
	
	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close 1 failed (%s)", 
		       smbcli_errstr(cli1->tree)));
	
	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum2),
		talloc_asprintf(tctx, "close 2 failed (%s)", 
		       smbcli_errstr(cli1->tree)));
	
	/* This should fail - file should no longer be there. */

	fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY, DENY_NONE);
	if (fnum1 != -1) {
		printf("(%s) open of %s succeeded should have been deleted on close !\n", 
		       __location__, fname);
		if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
			printf("(%s) close failed (%s)\n", 
			       __location__, smbcli_errstr(cli1->tree));
		}
		smbcli_unlink(cli1->tree, fname);
		return false;
	}
	return true;
}

/* Test 4 ... */
static bool deltest4(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_FILE_READ_DATA  | 
				      SEC_FILE_WRITE_DATA |
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE, 
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
								
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	/* This should succeed. */
	fnum2 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_READ,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ  | 
				      NTCREATEX_SHARE_ACCESS_WRITE |
				      NTCREATEX_SHARE_ACCESS_DELETE, 
				      NTCREATEX_DISP_OPEN, 0, 0);
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open  - 2 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));
	
	torture_assert_ntstatus_ok(tctx, 
					smbcli_close(cli1->tree, fnum2),
		 			talloc_asprintf(tctx, "close - 1 failed (%s)", 
					smbcli_errstr(cli1->tree)));
	
	torture_assert_ntstatus_ok(tctx, 
				smbcli_nt_delete_on_close(cli1->tree, fnum1, true), 
		 		talloc_asprintf(tctx, "setting delete_on_close failed (%s)", 
		        smbcli_errstr(cli1->tree)));

	/* This should fail - no more opens once delete on close set. */
	fnum2 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_READ,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE|NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN, 0, 0);
	torture_assert(tctx, fnum2 == -1, 
		 talloc_asprintf(tctx, "open  - 3 of %s succeeded ! Should have failed.",
		       fname ));

	CHECK_STATUS(cli1, NT_STATUS_DELETE_PENDING);

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1), 
		 talloc_asprintf(tctx, "close - 2 failed (%s)", 
		       smbcli_errstr(cli1->tree)));
	
	return correct;
}

/* Test 5 ... */
static bool deltest5(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;

	del_clean_area(cli1, cli2);

	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));
	
	/* This should fail - only allowed on NT opens with DELETE access. */

	torture_assert(tctx, !NT_STATUS_IS_OK(smbcli_nt_delete_on_close(cli1->tree, fnum1, true)),
		 "setting delete_on_close on OpenX file succeeded - should fail !");

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close - 2 failed (%s)", smbcli_errstr(cli1->tree)));

	return true;
}

/* Test 6 ... */
static bool deltest6(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;

	del_clean_area(cli1, cli2);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				   SEC_FILE_READ_DATA | SEC_FILE_WRITE_DATA,
				   FILE_ATTRIBUTE_NORMAL, 
				   NTCREATEX_SHARE_ACCESS_READ  |
				   NTCREATEX_SHARE_ACCESS_WRITE |
				   NTCREATEX_SHARE_ACCESS_DELETE,
				   NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
	
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));
	
	/* This should fail - only allowed on NT opens with DELETE access. */
	
	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_nt_delete_on_close(cli1->tree, fnum1, true)),
		"setting delete_on_close on file with no delete access succeeded - should fail !");

	torture_assert_ntstatus_ok(tctx, 
							   smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close - 2 failed (%s)", 
		       smbcli_errstr(cli1->tree)));

	return true;
}

/* Test 7 ... */
static bool deltest7(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_FILE_READ_DATA  | 
				      SEC_FILE_WRITE_DATA |
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_NORMAL, 0, 
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
								
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_nt_delete_on_close(cli1->tree, fnum1, true),
			"setting delete_on_close on file failed !");

	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, true, __location__);
	
	torture_assert_ntstatus_ok(tctx, 
					smbcli_nt_delete_on_close(cli1->tree, fnum1, false), 
		 			"unsetting delete_on_close on file failed !");

	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, false, __location__);
	
	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1), 
		talloc_asprintf(tctx, "close - 2 failed (%s)", smbcli_errstr(cli1->tree)));
	
	/* This next open should succeed - we reset the flag. */
	
	fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY, DENY_NONE);
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1), 
		 				       talloc_asprintf(tctx, "close - 2 failed (%s)", 
		       				   smbcli_errstr(cli1->tree)));

	return correct;
}

/* Test 8 ... */
static bool deltest8(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE|NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
	
	torture_assert(tctx, fnum1 != -1,
		talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, 
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE|NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN, 0, 0);
	
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, 
					smbcli_nt_delete_on_close(cli1->tree, fnum1, true),
		"setting delete_on_close on file failed !");

	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, true, __location__);
	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, true, __location__);

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close - 1 failed (%s)", 
		       smbcli_errstr(cli1->tree)));

	correct &= check_delete_on_close(tctx, cli1, -1, fname, true, __location__);
	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, true, __location__);
	
	torture_assert_ntstatus_ok(tctx, smbcli_close(cli2->tree, fnum2),
		talloc_asprintf(tctx, "close - 2 failed (%s)", smbcli_errstr(cli2->tree)));

	/* This should fail.. */
	fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY, DENY_NONE);
	torture_assert(tctx, fnum1 == -1,
		talloc_asprintf(tctx, "open of %s succeeded should have been deleted on close !\n", fname));

	return correct;
}

/* Test 9 ... */
static bool deltest9(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;

	del_clean_area(cli1, cli2);

	/* This should fail - we need to set DELETE_ACCESS. */
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0,
				      SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_NONE, 
				      NTCREATEX_DISP_OVERWRITE_IF, 
				      NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	
	torture_assert(tctx, fnum1 == -1, 
				   talloc_asprintf(tctx, "open of %s succeeded should have failed!", 
		       fname));

	return true;
}

/* Test 10 ... */
static bool deltest10(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;

	del_clean_area(cli1, cli2);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_NONE, 
				      NTCREATEX_DISP_OVERWRITE_IF, 
				      NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	torture_assert(tctx, fnum1 != -1, 
		talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	/* This should delete the file. */
	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close failed (%s)", 
		       smbcli_errstr(cli1->tree)));

	/* This should fail.. */
	fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY, DENY_NONE);
	torture_assert(tctx, fnum1 == -1, 
				talloc_asprintf(tctx, "open of %s succeeded should have been deleted on close !",
		       fname));
	return true;
}

/* Test 11 ... */
static bool deltest11(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	NTSTATUS status;

	del_clean_area(cli1, cli2);

	/* test 11 - does having read only attribute still allow delete on close. */

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_READONLY, 
				      NTCREATEX_SHARE_ACCESS_NONE, 
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
	
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	status = smbcli_nt_delete_on_close(cli1->tree, fnum1, true);

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_CANNOT_DELETE, 
		talloc_asprintf(tctx, "setting delete_on_close should fail with NT_STATUS_CANNOT_DELETE. Got %s instead)", smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close failed (%s)", 
		       smbcli_errstr(cli1->tree)));

	return true;
}

/* Test 12 ... */
static bool deltest12(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	NTSTATUS status;

	del_clean_area(cli1, cli2);

	/* test 12 - does having read only attribute still allow delete on
	 * close at time of open. */

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_READONLY,
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OVERWRITE_IF, 
				      NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	
	torture_assert(tctx, fnum1 == -1, 
		 talloc_asprintf(tctx, "open of %s succeeded. Should fail with "
		       "NT_STATUS_CANNOT_DELETE.\n", fname));

	status = smbcli_nt_error(cli1->tree);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_CANNOT_DELETE, 
			 talloc_asprintf(tctx, "setting delete_on_close on open should "
			       "fail with NT_STATUS_CANNOT_DELETE. Got %s "
			       "instead)", 
			       smbcli_errstr(cli1->tree)));
	
	return true;
}

/* Test 13 ... */
static bool deltest13(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	/* Test 13: Does resetting the delete on close flag affect a second
	 * fd? */

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OVERWRITE_IF,
				      0, 0);
	
	torture_assert(tctx, fnum1 != -1, 
		talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, 
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN, 0, 0);
	
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, 
				"open of %s failed (%s)", 
		       fname, smbcli_errstr(cli2->tree)));

	torture_assert_ntstatus_ok(tctx, 
						smbcli_nt_delete_on_close(cli1->tree, fnum1,
						       true), 
		 "setting delete_on_close on file failed !");

	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, true, __location__);
	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, true, __location__);

	torture_assert_ntstatus_ok(tctx, smbcli_nt_delete_on_close(cli2->tree, fnum2,
						       false), 
		 "setting delete_on_close on file failed !");

	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, false, __location__);
	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, false, __location__);
	
	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1), 
		talloc_asprintf(tctx, "close - 1 failed (%s)", 
		       smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli2->tree, fnum2),
			talloc_asprintf(tctx, "close - 2 failed (%s)", 
		       smbcli_errstr(cli2->tree)));

	fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY, DENY_NONE);

	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open of %s failed!", 
		       fname));

	smbcli_close(cli1->tree, fnum1);

	return correct;
}

/* Test 14 ... */
static bool deltest14(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int dnum1 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	/* Test 14 -- directory */

	dnum1 = smbcli_nt_create_full(cli1->tree, dname, 0,
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_DIRECTORY, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_CREATE, 0, 0);
	torture_assert(tctx, dnum1 != -1, talloc_asprintf(tctx, "open of %s failed: %s!", 
		       dname, smbcli_errstr(cli1->tree)));

	correct &= check_delete_on_close(tctx, cli1, dnum1, dname, false, __location__);
	torture_assert_ntstatus_ok(tctx, smbcli_nt_delete_on_close(cli1->tree, dnum1, true),
			"setting delete_on_close on file failed !");
	correct &= check_delete_on_close(tctx, cli1, dnum1, dname, true, __location__);
	smbcli_close(cli1->tree, dnum1);

	/* Now it should be gone... */

	dnum1 = smbcli_nt_create_full(cli1->tree, dname, 0,
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_DIRECTORY, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN, 0, 0);
	torture_assert(tctx, dnum1 == -1, "setting delete_on_close on file succeeded !");

	return correct;
}

/* Test 15 ... */
static bool deltest15(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	bool correct = true;
	int fnum2 = -1;
	NTSTATUS status;

	del_clean_area(cli1, cli2);

	/* Test 15: delete on close under rename */

	smbcli_setatr(cli1->tree, fname, 0, 0);
	smbcli_unlink(cli1->tree, fname);
	smbcli_unlink(cli1->tree, fname_new);
	
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_FILE_READ_DATA,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OVERWRITE_IF,
				      0, 0);

	torture_assert(tctx, fnum1 != -1, 
		talloc_asprintf(tctx, "open - 1 of %s failed (%s)", fname, smbcli_errstr(cli1->tree)));

	status = smbcli_rename(cli2->tree, fname, fname_new);

	torture_assert_ntstatus_ok(tctx, status, "renaming failed!");

	fnum2 = smbcli_nt_create_full(cli2->tree, fname_new, 0, 
				      SEC_GENERIC_ALL,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OVERWRITE_IF,
				      0, 0);

	torture_assert(tctx, fnum2 != -1, 
		talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       fname_new, smbcli_errstr(cli1->tree)));

	status = smbcli_nt_delete_on_close(cli2->tree, fnum2, true);

	torture_assert_ntstatus_ok(tctx, status, 
		"setting delete_on_close on file failed !");

	smbcli_close(cli2->tree, fnum2);

	/* The file should be around under the new name, there's a second
	 * handle open */

	correct &= check_delete_on_close(tctx, cli1, fnum1, fname_new, true, __location__);

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, 
				      SEC_GENERIC_ALL,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OVERWRITE_IF,
				      0, 0);

	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, false, __location__);

	smbcli_close(cli2->tree, fnum2);
	smbcli_close(cli1->tree, fnum1);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_FILE_READ_EA,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      0, 0);

	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	smbcli_close(cli1->tree, fnum1);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname_new, 0, 
				      SEC_FILE_READ_EA,
				      FILE_ATTRIBUTE_NORMAL, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      0, 0);

	torture_assert(tctx, fnum1 == -1, 
		"smbcli_open succeeded, should have "
		       "failed");

	return correct;
}

/* Test 16 ... */
static bool deltest16(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	/* Test 16. */

	/* Ensure the file doesn't already exist. */
	smbcli_close(cli1->tree, fnum1);
	smbcli_close(cli1->tree, fnum2);
	smbcli_setatr(cli1->tree, fname, 0, 0);
	smbcli_unlink(cli1->tree, fname);

	/* Firstly create with all access, but delete on close. */
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_CREATE,
				      NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	
	torture_assert (tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", fname, smbcli_errstr(cli1->tree)));

	/* The delete on close bit is *not* reported as being set. */
	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, false, __location__);

	/* The delete on close bit is *not* reported as being set. */
	correct &= check_delete_on_close(tctx, cli1, -1, fname, false, __location__);
	correct &= check_delete_on_close(tctx, cli2, -1, fname, false, __location__);

	/* Now try opening again for read-only. */
	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, 
				      SEC_RIGHTS_FILE_READ,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      0, 0);
	
	/* Should work. */
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		      fname, smbcli_errstr(cli1->tree)));

	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, false, __location__);
	correct &= check_delete_on_close(tctx, cli1, -1, fname, false, __location__);
	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, false, __location__);
	correct &= check_delete_on_close(tctx, cli2, -1, fname, false, __location__);

	smbcli_close(cli1->tree, fnum1);

	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, true, __location__);
	correct &= check_delete_on_close(tctx, cli2, -1, fname, true, __location__);

	smbcli_close(cli2->tree, fnum2);

	/* And the file should be deleted ! */
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum1 == -1, talloc_asprintf(tctx, "open of %s succeeded (should fail)", 
		       fname));

	return correct;
}

/* Test 17 ... */
static bool deltest17(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	/* Test 17. */

	/* Ensure the file doesn't already exist. */
	smbcli_close(cli1->tree, fnum1);
	smbcli_close(cli1->tree, fnum2);
	smbcli_setatr(cli1->tree, fname, 0, 0);
	smbcli_unlink(cli1->tree, fname);

	/* Firstly open and create with all access */
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_CREATE, 
				      0, 0);
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	/* And close - just to create the file. */
	smbcli_close(cli1->tree, fnum1);
	
	/* Next open with all access, but add delete on close. */
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	/* The delete on close bit is *not* reported as being set. */
	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, false, __location__);

	/* Now try opening again for read-only. */
	fnum2 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_READ|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      0, 0);
	
	/* Should work. */
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	/* still not reported as being set on either */
	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, false, __location__);
	correct &= check_delete_on_close(tctx, cli1, fnum2, fname, false, __location__);

	smbcli_close(cli1->tree, fnum1);

	/* After the first close, the files has the delete on close bit set. */
	correct &= check_delete_on_close(tctx, cli1, fnum2, fname, true, __location__);

	smbcli_close(cli1->tree, fnum2);

	/* Make sure the file has been deleted */
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum1 == -1, talloc_asprintf(tctx, "open of %s failed (should succeed) - %s",
		       fname, smbcli_errstr(cli1->tree)));

	CHECK_STATUS(cli1, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	return correct;
}

/* Test 18 ... */
static bool deltest18(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	/* Test 18. With directories. */

	/* Ensure the file doesn't already exist. */
	smbcli_close(cli1->tree, fnum1);
	smbcli_close(cli1->tree, fnum2);

	smbcli_deltree(cli1->tree, dname);

	/* Firstly create with all access, but delete on close. */
	fnum1 = smbcli_nt_create_full(cli1->tree, dname, 0, 
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_DIRECTORY,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_CREATE,
				      NTCREATEX_OPTIONS_DIRECTORY|NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       dname, smbcli_errstr(cli1->tree)));

	/*
	 * The delete on close bit is *not* reported as being set.
	 * Win2k3/win2k8 should pass this check, but WinXPsp2 reports delete on
	 * close as being set.  This causes the subsequent create to fail with
	 * NT_STATUS_DELETE_PENDING.
	 */
	correct &= check_delete_on_close(tctx, cli1, fnum1, dname, false, __location__);

	/* Now try opening again for read-only. */
	fnum2 = smbcli_nt_create_full(cli1->tree, dname, 0, 
				      SEC_RIGHTS_FILE_READ,
				      FILE_ATTRIBUTE_DIRECTORY,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      NTCREATEX_OPTIONS_DIRECTORY, 0);
	

	/* Should work. */
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       dname, smbcli_errstr(cli1->tree)));

	correct &= check_delete_on_close(tctx, cli1, fnum1, dname, false, __location__);
	correct &= check_delete_on_close(tctx, cli1, fnum2, dname, false, __location__);

	smbcli_close(cli1->tree, fnum1);

	correct &= check_delete_on_close(tctx, cli1, fnum2, dname, true, __location__);

	smbcli_close(cli1->tree, fnum2);

	/* And the directory should be deleted ! */
	fnum1 = smbcli_nt_create_full(cli1->tree, dname, 0, 
				      SEC_RIGHTS_FILE_READ,
				      FILE_ATTRIBUTE_DIRECTORY,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      NTCREATEX_OPTIONS_DIRECTORY, 0);
	torture_assert(tctx, fnum1 == -1, talloc_asprintf(tctx, "open of %s succeeded (should fail)", 
		       dname));

	return correct;
}

/* Test 19 ... */
static bool deltest19(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	/* Test 19. */

	smbcli_deltree(cli1->tree, dname);

	/* Firstly open and create with all access */
	fnum1 = smbcli_nt_create_full(cli1->tree, dname, 0, 
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_DIRECTORY,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_CREATE,
				      NTCREATEX_OPTIONS_DIRECTORY, 0);
	
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       dname, smbcli_errstr(cli1->tree)));

	/* And close - just to create the directory. */
	smbcli_close(cli1->tree, fnum1);
	
	/* Next open with all access, but add delete on close. */
	fnum1 = smbcli_nt_create_full(cli1->tree, dname, 0, 
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_DIRECTORY,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      NTCREATEX_OPTIONS_DIRECTORY|NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	
	torture_assert(tctx, fnum1 != -1, 
		talloc_asprintf(tctx, "open - 1 of %s failed (%s)", fname, smbcli_errstr(cli1->tree)));

	/*
	 * The delete on close bit is *not* reported as being set.
	 * Win2k3/win2k8 should pass this check, but WinXPsp2 reports delete on
	 * close as being set.  This causes the subsequent create to fail with
	 * NT_STATUS_DELETE_PENDING.
	 */
	correct &= check_delete_on_close(tctx, cli1, fnum1, dname, false, __location__);

	/* Now try opening again for read-only. */
	fnum2 = smbcli_nt_create_full(cli1->tree, dname, 0, 
				      SEC_RIGHTS_FILE_READ,
				      FILE_ATTRIBUTE_DIRECTORY,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      NTCREATEX_OPTIONS_DIRECTORY, 0);
	
	/* Should work. */
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       dname, smbcli_errstr(cli1->tree)));

	smbcli_close(cli1->tree, fnum1);

	correct &= check_delete_on_close(tctx, cli1, fnum2, dname, true, __location__);

	smbcli_close(cli1->tree, fnum2);

	/* See if the file is deleted - for a directory this seems to be true ! */
	fnum1 = smbcli_nt_create_full(cli1->tree, dname, 0, 
				      SEC_RIGHTS_FILE_READ,
				      FILE_ATTRIBUTE_DIRECTORY,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      NTCREATEX_OPTIONS_DIRECTORY, 0);

	CHECK_STATUS(cli1, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	torture_assert(tctx, fnum1 == -1, 
		talloc_asprintf(tctx, "open of %s succeeded (should fail)", dname));

	return correct;
}

/* Test 20 ... */
static bool deltest20(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int dnum1 = -1;
	bool correct = true;
	NTSTATUS status;

	del_clean_area(cli1, cli2);

	/* Test 20 -- non-empty directory hardest to get right... */

	if (torture_setting_bool(tctx, "samba3", false)) {
		return true;
	}

	smbcli_deltree(cli1->tree, dname);

	dnum1 = smbcli_nt_create_full(cli1->tree, dname, 0,
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_DIRECTORY, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_CREATE, 
				      NTCREATEX_OPTIONS_DIRECTORY, 0);
	torture_assert(tctx, dnum1 != -1, talloc_asprintf(tctx, "open of %s failed: %s!", 
		       dname, smbcli_errstr(cli1->tree)));

	correct &= check_delete_on_close(tctx, cli1, dnum1, dname, false, __location__);
	status = smbcli_nt_delete_on_close(cli1->tree, dnum1, true);

	{
		char *fullname;
		asprintf(&fullname, "\\%s%s", dname, fname);
		fnum1 = smbcli_open(cli1->tree, fullname, O_CREAT|O_RDWR,
				    DENY_NONE);
		torture_assert(tctx, fnum1 == -1, 
				"smbcli_open succeeded, should have "
			       "failed with NT_STATUS_DELETE_PENDING"
			       );

		torture_assert_ntstatus_equal(tctx, 
					 smbcli_nt_error(cli1->tree),
				     NT_STATUS_DELETE_PENDING, 
					"smbcli_open failed");
	}

	status = smbcli_nt_delete_on_close(cli1->tree, dnum1, false);
	torture_assert_ntstatus_ok(tctx, status, 
					"setting delete_on_close on file failed !");
		
	{
		char *fullname;
		asprintf(&fullname, "\\%s%s", dname, fname);
		fnum1 = smbcli_open(cli1->tree, fullname, O_CREAT|O_RDWR,
				    DENY_NONE);
		torture_assert(tctx, fnum1 != -1, 
				talloc_asprintf(tctx, "smbcli_open failed: %s\n",
			       smbcli_errstr(cli1->tree)));
		smbcli_close(cli1->tree, fnum1);
	}

	status = smbcli_nt_delete_on_close(cli1->tree, dnum1, true);

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_DIRECTORY_NOT_EMPTY,
		 "setting delete_on_close failed");
	smbcli_close(cli1->tree, dnum1);

	return correct;
}

/* Test 20a ... */
static bool deltest20a(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	/* Test 20a. */

	/* Ensure the file doesn't already exist. */
	smbcli_close(cli1->tree, fnum1);
	smbcli_close(cli1->tree, fnum2);
	smbcli_setatr(cli1->tree, fname, 0, 0);
	smbcli_unlink(cli1->tree, fname);

	/* Firstly open and create with all access */
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_CREATE, 
				      0, 0);
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	/* Next open with all access, but add delete on close. */
	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open - 2 of %s failed (%s)", 
		       fname, smbcli_errstr(cli2->tree)));

	/* The delete on close bit is *not* reported as being set. */
	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, false, __location__);
	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, false, __location__);

	smbcli_close(cli1->tree, fnum1);

	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, false, __location__);

	smbcli_close(cli2->tree, fnum2);

	/* See if the file is deleted - should be.... */
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum1 == -1, talloc_asprintf(tctx, "open of %s succeeded (should fail) - %s", 
		       fname, smbcli_errstr(cli1->tree)));

	return correct;
}

/* Test 20b ... */
/* This is the delete semantics that the cifsfs client depends on when
 * trying to delete an open file on a Windows server. It
 * opens a file with initial delete on close set, renames it then closes
 * all open handles. The file goes away on Windows.
 */

static bool deltest20b(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	int fnum1 = -1;
	int fnum2 = -1;
	bool correct = true;

	del_clean_area(cli1, cli2);

	/* Test 20b. */

	/* Ensure the file doesn't already exist. */
	smbcli_close(cli1->tree, fnum1);
	smbcli_close(cli1->tree, fnum2);
	smbcli_setatr(cli1->tree, fname, 0, 0);
	smbcli_unlink(cli1->tree, fname);
	smbcli_setatr(cli1->tree, fname_new, 0, 0);
	smbcli_unlink(cli1->tree, fname_new);

	/* Firstly open and create with all access */
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_CREATE, 
				      0, 0);
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	/* And close - just to create the file. */
	smbcli_close(cli1->tree, fnum1);
	
	/* Firstly open and create with all access */
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN, 
				      0, 0);
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open - 1 of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));

	/* Next open with all access, but add delete on close. */
	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      NTCREATEX_OPTIONS_DELETE_ON_CLOSE, 0);
	
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, "open - 2 of %s failed (%s)", 
		       fname, smbcli_errstr(cli2->tree)));

	/* The delete on close bit is *not* reported as being set. */
	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, false, __location__);
	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, false, __location__);

	smbcli_close(cli1->tree, fnum1);

	correct &= check_delete_on_close(tctx, cli2, fnum2, fname, false, __location__);

	/* Rename the file by handle. */

	{
		union smb_setfileinfo sfinfo;
		NTSTATUS status;

		memset(&sfinfo, '\0', sizeof(sfinfo));
		sfinfo.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
		sfinfo.generic.in.file.fnum = fnum2;
		sfinfo.rename_information.in.root_fid  = 0;
		/* Don't start the filename with '\\', we get NT_STATUS_NOT_SUPPORTED if so. */
		sfinfo.rename_information.in.new_name  = fname_new + 1;
		sfinfo.rename_information.in.overwrite = 1;

		status = smb_raw_setfileinfo(cli2->tree, &sfinfo);

		torture_assert_ntstatus_equal(tctx,status,NT_STATUS_OK,talloc_asprintf(tctx, "rename of %s to %s failed (%s)",
			fname, fname_new, smbcli_errstr(cli2->tree)));
	}

	correct &= check_delete_on_close(tctx, cli2, fnum2, fname_new, false, __location__);

	smbcli_close(cli2->tree, fnum2);

	/* See if the file is deleted - should be.... */
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum1 == -1, talloc_asprintf(tctx, "open of %s succeeded (should fail) - %s", 
		       fname, smbcli_errstr(cli1->tree)));
	fnum1 = smbcli_open(cli1->tree, fname_new, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum1 == -1, talloc_asprintf(tctx, "open of %s succeeded (should fail) - %s", 
		       fname_new, smbcli_errstr(cli1->tree)));

	return correct;
}


/* Test 21 ... */
static bool deltest21(struct torture_context *tctx)
{
	int fnum1 = -1;
	struct smbcli_state *cli1;
	struct smbcli_state *cli2;
	bool correct = true;

	if (!torture_open_connection(&cli1, tctx, 0))
		return false;

	if (!torture_open_connection(&cli2, tctx, 1))
		return false;

	del_clean_area(cli1, cli2);

	/* Test 21 -- Test removal of file after socket close. */

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_ALL,
				      FILE_ATTRIBUTE_NORMAL, NTCREATEX_SHARE_ACCESS_NONE, 
				      NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
	
	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, "open of %s failed (%s)", 
		       fname, smbcli_errstr(cli1->tree)));
	
	torture_assert_ntstatus_ok(tctx, 
				smbcli_nt_delete_on_close(cli1->tree, fnum1, true),
				talloc_asprintf(tctx, "setting delete_on_close failed (%s)", 
		       smbcli_errstr(cli1->tree)));
	
	/* Ensure delete on close is set. */
	correct &= check_delete_on_close(tctx, cli1, fnum1, fname, true, __location__);

	/* Now yank the rug from under cli1. */
	smbcli_transport_dead(cli1->transport, NT_STATUS_LOCAL_DISCONNECT);

	fnum1 = -1;

	if (!torture_open_connection(&cli1, tctx, 0)) {
		return false;
	}

	/* On slow build farm machines it might happen that they are not fast
	 * enogh to delete the file for this test */
	msleep(200);

	/* File should not be there. */
	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, 
				      SEC_RIGHTS_FILE_READ,
				      FILE_ATTRIBUTE_NORMAL,
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN,
				      0, 0);
	
	CHECK_STATUS(cli1, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	return correct;
}

/* Test 22 ... */

/*
 * Test whether a second *directory* handle inhibits delete if the first has
 * del-on-close set and is closed
 */
static bool deltest22(struct torture_context *tctx)
{
	int dnum1 = -1;
	int dnum2 = -1;
	struct smbcli_state *cli1;
	bool correct = true;

	if (!torture_open_connection(&cli1, tctx, 0))
		return false;

	smbcli_deltree(cli1->tree, dname);

	torture_assert_ntstatus_ok(
		tctx, smbcli_mkdir(cli1->tree, dname),
		talloc_asprintf(tctx, "smbcli_mdir failed: (%s)\n",
				smbcli_errstr(cli1->tree)));

	dnum1 = smbcli_nt_create_full(cli1->tree, dname, 0,
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_DIRECTORY, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN, 
				      NTCREATEX_OPTIONS_DIRECTORY, 0);

	torture_assert(tctx, dnum1 != -1,
		       talloc_asprintf(tctx, "open of %s failed: %s!", 
				       dname, smbcli_errstr(cli1->tree)));

	dnum2 = smbcli_nt_create_full(cli1->tree, dname, 0,
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA,
				      FILE_ATTRIBUTE_DIRECTORY, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN, 
				      NTCREATEX_OPTIONS_DIRECTORY, 0);

	torture_assert(tctx, dnum2 != -1,
		       talloc_asprintf(tctx, "open of %s failed: %s!", 
				       dname, smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(
		tctx, smbcli_nt_delete_on_close(cli1->tree, dnum1, true), 
		talloc_asprintf(tctx, "setting delete_on_close failed (%s)", 
				smbcli_errstr(cli1->tree)));

	smbcli_close(cli1->tree, dnum1);

	dnum1 = smbcli_nt_create_full(cli1->tree, dname, 0,
				      SEC_FILE_READ_DATA|
				      SEC_FILE_WRITE_DATA|
				      SEC_STD_DELETE,
				      FILE_ATTRIBUTE_DIRECTORY, 
				      NTCREATEX_SHARE_ACCESS_READ|
				      NTCREATEX_SHARE_ACCESS_WRITE|
				      NTCREATEX_SHARE_ACCESS_DELETE,
				      NTCREATEX_DISP_OPEN, 
				      NTCREATEX_OPTIONS_DIRECTORY, 0);

	torture_assert(tctx, dnum1 == -1,
		       talloc_asprintf(tctx, "open of %s succeeded!\n",
				       dname));

	CHECK_STATUS(cli1, NT_STATUS_DELETE_PENDING);

	return correct;
}

/*
  Test delete on close semantics.
 */
struct torture_suite *torture_test_delete(void)
{
	struct torture_suite *suite = torture_suite_create(
		talloc_autofree_context(),
		"DELETE");

	torture_suite_add_2smb_test(suite, "deltest1", deltest1);
	torture_suite_add_2smb_test(suite, "deltest2", deltest2);
	torture_suite_add_2smb_test(suite, "deltest3", deltest3);
	torture_suite_add_2smb_test(suite, "deltest4", deltest4);
	torture_suite_add_2smb_test(suite, "deltest5", deltest5);
	torture_suite_add_2smb_test(suite, "deltest6", deltest6);
	torture_suite_add_2smb_test(suite, "deltest7", deltest7);
	torture_suite_add_2smb_test(suite, "deltest8", deltest8);
	torture_suite_add_2smb_test(suite, "deltest9", deltest9);
	torture_suite_add_2smb_test(suite, "deltest10", deltest10);
	torture_suite_add_2smb_test(suite, "deltest11", deltest11);
	torture_suite_add_2smb_test(suite, "deltest12", deltest12);
	torture_suite_add_2smb_test(suite, "deltest13", deltest13);
	torture_suite_add_2smb_test(suite, "deltest14", deltest14);
	torture_suite_add_2smb_test(suite, "deltest15", deltest15);
	torture_suite_add_2smb_test(suite, "deltest16", deltest16);
	torture_suite_add_2smb_test(suite, "deltest17", deltest17);
	torture_suite_add_2smb_test(suite, "deltest18", deltest18);
	torture_suite_add_2smb_test(suite, "deltest19", deltest19);
	torture_suite_add_2smb_test(suite, "deltest20", deltest20);
	torture_suite_add_2smb_test(suite, "deltest20a", deltest20a);
	torture_suite_add_2smb_test(suite, "deltest20b", deltest20b);
	torture_suite_add_simple_test(suite, "deltest21", deltest21);
	torture_suite_add_simple_test(suite, "deltest22", deltest22);

	return suite;
}
