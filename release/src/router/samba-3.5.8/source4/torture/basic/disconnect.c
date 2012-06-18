/* 
   Unix SMB/CIFS implementation.

   test server handling of unexpected client disconnects

   Copyright (C) Andrew Tridgell 2004
   
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

#define BASEDIR "\\test_disconnect"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		talloc_free(cli); \
		return false; \
	}} while (0)

/*
  test disconnect after async open
*/
static bool test_disconnect_open(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io;
	NTSTATUS status;
	struct smbcli_request *req1, *req2;

	printf("trying open/disconnect\n");

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR "\\open.dat";
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.ntcreatex.in.share_access = 0;
	req1 = smb_raw_open_send(cli->tree, &io);
	req2 = smb_raw_open_send(cli->tree, &io);

	status = smbcli_chkpath(cli->tree, "\\");
	CHECK_STATUS(status, NT_STATUS_OK);
	
	talloc_free(cli);

	return true;
}


/*
  test disconnect with timed lock
*/
static bool test_disconnect_lock(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_lock io;
	NTSTATUS status;
	int fnum;
	struct smbcli_request *req;
	struct smb_lock_entry lock[1];

	printf("trying disconnect with async lock\n");

	fnum = smbcli_open(cli->tree, BASEDIR "\\write.dat", 
			   O_RDWR | O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("open failed in mux_write - %s\n", smbcli_errstr(cli->tree));
		return false;
	}

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = 0;
	io.lockx.in.timeout = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.ulock_cnt = 0;
	lock[0].pid = 1;
	lock[0].offset = 0;
	lock[0].count = 4;
	io.lockx.in.locks = &lock[0];

	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	lock[0].pid = 2;
	io.lockx.in.timeout = 3000;
	req = smb_raw_lock_send(cli->tree, &io);

	status = smbcli_chkpath(cli->tree, "\\");
	CHECK_STATUS(status, NT_STATUS_OK);

	talloc_free(cli);

	return true;
}



/* 
   basic testing of disconnects
*/
bool torture_disconnect(struct torture_context *torture)
{
	bool ret = true;
	TALLOC_CTX *mem_ctx;
	int i;
	extern int torture_numops;
	struct smbcli_state *cli;

	mem_ctx = talloc_init("torture_raw_mux");

	if (!torture_open_connection(&cli, torture, 0)) {
		return false;
	}

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	for (i=0;i<torture_numops;i++) {
		ret &= test_disconnect_lock(cli, mem_ctx);
		if (!torture_open_connection(&cli, torture, 0)) {
			return false;
		}

		ret &= test_disconnect_open(cli, mem_ctx);
		if (!torture_open_connection(&cli, torture, 0)) {
			return false;
		}

		if (torture_setting_bool(torture, "samba3", false)) {
			/*
			 * In Samba3 it might happen that the old smbd from
			 * test_disconnect_lock is not scheduled before the
			 * new process comes in. Try to get rid of the random
			 * failures in the build farm.
			 */
			msleep(200);
		}
	}

	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	talloc_free(mem_ctx);
	return ret;
}
