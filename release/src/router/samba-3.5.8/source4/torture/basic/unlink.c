/* 
   Unix SMB/CIFS implementation.

   unlink tester

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
#include "libcli/libcli.h"
#include "torture/util.h"

#define BASEDIR "\\unlinktest"

/*
  This test checks that 

  1) the server does not allow an unlink on a file that is open
*/
bool torture_unlinktest(struct torture_context *tctx, struct smbcli_state *cli)
{
	const char *fname = BASEDIR "\\unlink.tst";
	int fnum;
	bool correct = true;
	union smb_open io;
	NTSTATUS status;

	torture_assert(tctx, torture_setup_dir(cli, BASEDIR), 
				   talloc_asprintf(tctx, "Failed setting up %s", BASEDIR));

	cli->session->pid = 1;

	torture_comment(tctx, "Opening a file\n");

	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	torture_assert(tctx, fnum != -1, talloc_asprintf(tctx, "open of %s failed (%s)", fname, smbcli_errstr(cli->tree)));

	torture_comment(tctx, "Unlinking a open file\n");

	torture_assert(tctx, !NT_STATUS_IS_OK(smbcli_unlink(cli->tree, fname)),
		"server allowed unlink on an open file");
	
	correct = check_error(__location__, cli, ERRDOS, ERRbadshare, 
				      NT_STATUS_SHARING_VIOLATION);

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	torture_comment(tctx, "testing unlink after ntcreatex with DELETE access\n");

	io.ntcreatex.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_NON_DIRECTORY_FILE;
	io.ntcreatex.in.file_attr = 0;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_IMPERSONATION;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.access_mask  = SEC_RIGHTS_FILE_ALL;

	status = smb_raw_open(cli->tree, cli, &io);
	torture_assert_ntstatus_ok(tctx, status, talloc_asprintf(tctx, "failed to open %s", fname));

	torture_assert(tctx, !NT_STATUS_IS_OK(smbcli_unlink(cli->tree, fname)),
		"server allowed unlink on an open file");

	correct = check_error(__location__, cli, ERRDOS, ERRbadshare, 
				      NT_STATUS_SHARING_VIOLATION);

	return correct;
}


