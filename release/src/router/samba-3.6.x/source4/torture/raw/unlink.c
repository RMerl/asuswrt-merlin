/* 
   Unix SMB/CIFS implementation.
   unlink test suite
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
#include "torture/torture.h"
#include "system/filesys.h"
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"
#include "torture/util.h"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

#define BASEDIR "\\testunlink"

/*
  test unlink ops
*/
static bool test_unlink(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_unlink io;
	NTSTATUS status;
	bool ret = true;
	const char *fname = BASEDIR "\\test.txt";

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("Trying non-existant file\n");
	io.unlink.in.pattern = fname;
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	smbcli_close(cli->tree, smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE));

	io.unlink.in.pattern = fname;
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("Trying a hidden file\n");
	smbcli_close(cli->tree, smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE));
	torture_set_file_attribute(cli->tree, fname, FILE_ATTRIBUTE_HIDDEN);

	io.unlink.in.pattern = fname;
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);

	io.unlink.in.pattern = fname;
	io.unlink.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.unlink.in.pattern = fname;
	io.unlink.in.attrib = FILE_ATTRIBUTE_HIDDEN;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	printf("Trying a directory\n");
	io.unlink.in.pattern = BASEDIR;
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_IS_A_DIRECTORY);

	io.unlink.in.pattern = BASEDIR;
	io.unlink.in.attrib = FILE_ATTRIBUTE_DIRECTORY;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_IS_A_DIRECTORY);

	printf("Trying a bad path\n");
	io.unlink.in.pattern = "..";
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_PATH_SYNTAX_BAD);

	io.unlink.in.pattern = "\\..";
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_PATH_SYNTAX_BAD);

	io.unlink.in.pattern = BASEDIR "\\..\\..";
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_PATH_SYNTAX_BAD);

	io.unlink.in.pattern = BASEDIR "\\..";
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_IS_A_DIRECTORY);

	printf("Trying wildcards\n");
	smbcli_close(cli->tree, smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE));
	io.unlink.in.pattern = BASEDIR "\\t*.t";
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);

	io.unlink.in.pattern = BASEDIR "\\z*";
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);

	io.unlink.in.pattern = BASEDIR "\\z*";
	io.unlink.in.attrib = FILE_ATTRIBUTE_DIRECTORY;
	status = smb_raw_unlink(cli->tree, &io);

	if (torture_setting_bool(tctx, "samba3", false)) {
		/*
		 * In Samba3 we gave up upon getting the error codes in
		 * wildcard unlink correct. Trying gentest showed that this is
		 * irregular beyond our capabilities. So for
		 * FILE_ATTRIBUTE_DIRECTORY we always return NAME_INVALID.
		 * Tried by jra and vl. If others feel like solving this
		 * puzzle, please tell us :-)
		 */
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);
	}
	else {
		CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);
	}

	io.unlink.in.pattern = BASEDIR "\\*";
	io.unlink.in.attrib = FILE_ATTRIBUTE_DIRECTORY;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);

	io.unlink.in.pattern = BASEDIR "\\?";
	io.unlink.in.attrib = FILE_ATTRIBUTE_DIRECTORY;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);

	io.unlink.in.pattern = BASEDIR "\\t*";
	io.unlink.in.attrib = FILE_ATTRIBUTE_DIRECTORY;
	status = smb_raw_unlink(cli->tree, &io);
	if (torture_setting_bool(tctx, "samba3", false)) {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);
	}
	else {
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	smbcli_close(cli->tree, smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE));

	io.unlink.in.pattern = BASEDIR "\\*.dat";
	io.unlink.in.attrib = FILE_ATTRIBUTE_DIRECTORY;
	status = smb_raw_unlink(cli->tree, &io);
	if (torture_setting_bool(tctx, "samba3", false)) {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_INVALID);
	}
	else {
		CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);
	}

	io.unlink.in.pattern = BASEDIR "\\*.tx?";
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	if (torture_setting_bool(tctx, "samba3", false)) {
		CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);
	}
	else {
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_NO_SUCH_FILE);


done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test delete on close 
*/
static bool test_delete_on_close(struct torture_context *tctx, 
								 struct smbcli_state *cli)
{
	union smb_open op;
	union smb_unlink io;
	struct smb_rmdir dio;
	NTSTATUS status;
	bool ret = true;
	int fnum, fnum2;
	const char *fname = BASEDIR "\\test.txt";
	const char *dname = BASEDIR "\\test.dir";
	const char *inside = BASEDIR "\\test.dir\\test.txt";
	union smb_setfileinfo sfinfo;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	dio.in.path = dname;

	io.unlink.in.pattern = fname;
	io.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	printf("Testing with delete_on_close 0\n");
	fnum = create_complex_file(cli, tctx, fname);

	sfinfo.disposition_info.level = RAW_SFILEINFO_DISPOSITION_INFO;
	sfinfo.disposition_info.in.file.fnum = fnum;
	sfinfo.disposition_info.in.delete_on_close = 0;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);

	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("Testing with delete_on_close 1\n");
	fnum = create_complex_file(cli, tctx, fname);
	sfinfo.disposition_info.in.file.fnum = fnum;
	sfinfo.disposition_info.in.delete_on_close = 1;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);

	status = smb_raw_unlink(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);


	printf("Testing with directory and delete_on_close 0\n");
	status = create_directory_handle(cli->tree, dname, &fnum);
	CHECK_STATUS(status, NT_STATUS_OK);

	sfinfo.disposition_info.level = RAW_SFILEINFO_DISPOSITION_INFO;
	sfinfo.disposition_info.in.file.fnum = fnum;
	sfinfo.disposition_info.in.delete_on_close = 0;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);

	status = smb_raw_rmdir(cli->tree, &dio);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("Testing with directory delete_on_close 1\n");
	status = create_directory_handle(cli->tree, dname, &fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	sfinfo.disposition_info.in.file.fnum = fnum;
	sfinfo.disposition_info.in.delete_on_close = 1;
	status = smb_raw_setfileinfo(cli->tree, &sfinfo);
	CHECK_STATUS(status, NT_STATUS_OK);

	smbcli_close(cli->tree, fnum);

	status = smb_raw_rmdir(cli->tree, &dio);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);


	if (!torture_setting_bool(tctx, "samba3", false)) {

		/*
		 * Known deficiency, also skipped in base-delete.
		 */

		printf("Testing with non-empty directory delete_on_close\n");
		status = create_directory_handle(cli->tree, dname, &fnum);
		CHECK_STATUS(status, NT_STATUS_OK);

		fnum2 = create_complex_file(cli, tctx, inside);

		sfinfo.disposition_info.in.file.fnum = fnum;
		sfinfo.disposition_info.in.delete_on_close = 1;
		status = smb_raw_setfileinfo(cli->tree, &sfinfo);
		CHECK_STATUS(status, NT_STATUS_DIRECTORY_NOT_EMPTY);

		sfinfo.disposition_info.in.file.fnum = fnum2;
		status = smb_raw_setfileinfo(cli->tree, &sfinfo);
		CHECK_STATUS(status, NT_STATUS_OK);

		sfinfo.disposition_info.in.file.fnum = fnum;
		status = smb_raw_setfileinfo(cli->tree, &sfinfo);
		CHECK_STATUS(status, NT_STATUS_DIRECTORY_NOT_EMPTY);

		smbcli_close(cli->tree, fnum2);

		status = smb_raw_setfileinfo(cli->tree, &sfinfo);
		CHECK_STATUS(status, NT_STATUS_OK);

		smbcli_close(cli->tree, fnum);

		status = smb_raw_rmdir(cli->tree, &dio);
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	}

	printf("Testing open dir with delete_on_close\n");
	status = create_directory_handle(cli->tree, dname, &fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	smbcli_close(cli->tree, fnum);
	fnum2 = create_complex_file(cli, tctx, inside);
	smbcli_close(cli->tree, fnum2);

	op.generic.level = RAW_OPEN_NTCREATEX;
	op.ntcreatex.in.root_fid.fnum = 0;
	op.ntcreatex.in.flags = 0;
	op.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	op.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY |NTCREATEX_OPTIONS_DELETE_ON_CLOSE;
	op.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	op.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	op.ntcreatex.in.alloc_size = 0;
	op.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	op.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	op.ntcreatex.in.security_flags = 0;
	op.ntcreatex.in.fname = dname;

	status = smb_raw_open(cli->tree, tctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = op.ntcreatex.out.file.fnum;

	smbcli_close(cli->tree, fnum);

	status = smb_raw_rmdir(cli->tree, &dio);
	CHECK_STATUS(status, NT_STATUS_DIRECTORY_NOT_EMPTY);

	smbcli_deltree(cli->tree, dname);

	printf("Testing double open dir with second delete_on_close\n");
	status = create_directory_handle(cli->tree, dname, &fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	smbcli_close(cli->tree, fnum);
	
	fnum2 = create_complex_file(cli, tctx, inside);
	smbcli_close(cli->tree, fnum2);

	op.generic.level = RAW_OPEN_NTCREATEX;
	op.ntcreatex.in.root_fid.fnum = 0;
	op.ntcreatex.in.flags = 0;
	op.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	op.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY |NTCREATEX_OPTIONS_DELETE_ON_CLOSE;
	op.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	op.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	op.ntcreatex.in.alloc_size = 0;
	op.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	op.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	op.ntcreatex.in.security_flags = 0;
	op.ntcreatex.in.fname = dname;

	status = smb_raw_open(cli->tree, tctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = op.ntcreatex.out.file.fnum;

	smbcli_close(cli->tree, fnum2);

	status = smb_raw_rmdir(cli->tree, &dio);
	CHECK_STATUS(status, NT_STATUS_DIRECTORY_NOT_EMPTY);

	smbcli_deltree(cli->tree, dname);

	printf("Testing pre-existing open dir with second delete_on_close\n");
	status = create_directory_handle(cli->tree, dname, &fnum);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	smbcli_close(cli->tree, fnum);

	fnum = create_complex_file(cli, tctx, inside);
	smbcli_close(cli->tree, fnum);

	/* we have a dir with a file in it, no handles open */

	op.generic.level = RAW_OPEN_NTCREATEX;
	op.ntcreatex.in.root_fid.fnum = 0;
	op.ntcreatex.in.flags = 0;
	op.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	op.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY |NTCREATEX_OPTIONS_DELETE_ON_CLOSE;
	op.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	op.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
	op.ntcreatex.in.alloc_size = 0;
	op.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	op.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	op.ntcreatex.in.security_flags = 0;
	op.ntcreatex.in.fname = dname;

	status = smb_raw_open(cli->tree, tctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = op.ntcreatex.out.file.fnum;

	/* open without delete on close */
	op.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	status = smb_raw_open(cli->tree, tctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = op.ntcreatex.out.file.fnum;

	/* close 2nd file handle */
	smbcli_close(cli->tree, fnum2);

	status = smb_raw_rmdir(cli->tree, &dio);
	CHECK_STATUS(status, NT_STATUS_DIRECTORY_NOT_EMPTY);
	

	smbcli_close(cli->tree, fnum);

	status = smb_raw_rmdir(cli->tree, &dio);
	CHECK_STATUS(status, NT_STATUS_DIRECTORY_NOT_EMPTY);
	
done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


struct unlink_defer_cli_state {
	struct torture_context *tctx;
	struct smbcli_state *cli1;
};

/*
 * A handler function for oplock break requests. Ack it as a break to none
 */
static bool oplock_handler_ack_to_none(struct smbcli_transport *transport,
				       uint16_t tid, uint16_t fnum,
				       uint8_t level, void *private_data)
{
	struct unlink_defer_cli_state *ud_cli_state =
	    (struct unlink_defer_cli_state *)private_data;
	union smb_setfileinfo sfinfo;
	bool ret;
	struct smbcli_request *req = NULL;

	torture_comment(ud_cli_state->tctx, "delete the file before sending "
			"the ack.");

	/* cli1: set delete on close */
	sfinfo.disposition_info.level = RAW_SFILEINFO_DISPOSITION_INFO;
	sfinfo.disposition_info.in.file.fnum = fnum;
	sfinfo.disposition_info.in.delete_on_close = 1;
	req = smb_raw_setfileinfo_send(ud_cli_state->cli1->tree, &sfinfo);

	smbcli_close(ud_cli_state->cli1->tree, fnum);

	torture_comment(ud_cli_state->tctx, "Acking the oplock to NONE\n");

	ret = smbcli_oplock_ack(ud_cli_state->cli1->tree, fnum,
				 OPLOCK_BREAK_TO_NONE);

	return ret;
}

static bool test_unlink_defer(struct torture_context *tctx,
			      struct smbcli_state *cli1,
			      struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_unlink_defer.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_unlink unl;
	uint16_t fnum=0;
	struct unlink_defer_cli_state ud_cli_state = {};

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	ud_cli_state.tctx = tctx;
	ud_cli_state.cli1 = cli1;

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_none,
			      &ud_cli_state);

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
	    NTCREATEX_SHARE_ACCESS_WRITE |
	    NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/* cli1: open file with a batch oplock. */
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
	    NTCREATEX_FLAGS_REQUEST_OPLOCK |
	    NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	/* cli2: Try to unlink it, but block on the oplock */
	torture_comment(tctx, "Try an unlink (should defer the open\n");
	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli2->tree, &unl);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

/* 
   basic testing of unlink calls
*/
struct torture_suite *torture_raw_unlink(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "unlink");

	torture_suite_add_1smb_test(suite, "unlink", test_unlink);
	torture_suite_add_1smb_test(suite, "delete_on_close", test_delete_on_close);
	torture_suite_add_2smb_test(suite, "unlink-defer", test_unlink_defer);

	return suite;
}
