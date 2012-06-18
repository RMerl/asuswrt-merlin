/* 
   Unix SMB/CIFS implementation.
   basic raw test suite for multiplexing
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

#define BASEDIR "\\test_mux"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)


/*
  test the delayed reply to a open that leads to a sharing violation
*/
static bool test_mux_open(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_open io;
	NTSTATUS status;
	int fnum1, fnum2;
	bool ret = true;
	struct smbcli_request *req1, *req2;
	struct timeval tv;
	double d;

	printf("testing multiplexed open/open/close\n");

	printf("send first open\n");
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_DATA;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR "\\open.dat";
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum1 = io.ntcreatex.out.file.fnum;

	printf("send 2nd open, non-conflicting\n");
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;

	tv = timeval_current();

	printf("send 3rd open, conflicting\n");
	io.ntcreatex.in.share_access = 0;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);

	d = timeval_elapsed(&tv);
	if (d < 0.5 || d > 1.5) {
		printf("bad timeout for conflict - %.2f should be 1.0\n", d);
	} else {
		printf("open delay %.2f\n", d);
	}

	printf("send async open, conflicting\n");
	tv = timeval_current();
	req1 = smb_raw_open_send(cli->tree, &io);

	printf("send 2nd async open, conflicting\n");
	tv = timeval_current();
	req2 = smb_raw_open_send(cli->tree, &io);
	
	printf("close first sync open\n");
	smbcli_close(cli->tree, fnum1);

	printf("cancel 2nd async open (should be ignored)\n");
	smb_raw_ntcancel(req2);

	d = timeval_elapsed(&tv);
	if (d > 0.25) {
		printf("bad timeout after cancel - %.2f should be <0.25\n", d);
		ret = false;
	}

	printf("close the 2nd sync open\n");
	smbcli_close(cli->tree, fnum2);

	printf("see if the 1st async open now succeeded\n");
	status = smb_raw_open_recv(req1, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	d = timeval_elapsed(&tv);
	if (d > 0.25) {
		printf("bad timeout for async conflict - %.2f should be <0.25\n", d);
		ret = false;
	} else {
		printf("async open delay %.2f\n", d);
	}

	printf("2nd async open should have timed out\n");
	status = smb_raw_open_recv(req2, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_SHARING_VIOLATION);
	d = timeval_elapsed(&tv);
	if (d < 0.8) {
		printf("bad timeout for async conflict - %.2f should be 1.0\n", d);
	}

	printf("close the 1st async open\n");
	smbcli_close(cli->tree, io.ntcreatex.out.file.fnum);

done:
	return ret;
}


/*
  test a write that hits a byte range lock and send the close after the write
*/
static bool test_mux_write(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_write io;
	NTSTATUS status;
	int fnum;
	bool ret = true;
	struct smbcli_request *req;

	printf("testing multiplexed lock/write/close\n");

	fnum = smbcli_open(cli->tree, BASEDIR "\\write.dat", O_RDWR | O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("open failed in mux_write - %s\n", smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	cli->session->pid = 1;

	/* lock a range */
	if (NT_STATUS_IS_ERR(smbcli_lock(cli->tree, fnum, 0, 4, 0, WRITE_LOCK))) {
		printf("lock failed in mux_write - %s\n", smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	cli->session->pid = 2;

	/* send an async write */
	io.generic.level = RAW_WRITE_WRITEX;
	io.writex.in.file.fnum = fnum;
	io.writex.in.offset = 0;
	io.writex.in.wmode = 0;
	io.writex.in.remaining = 0;
	io.writex.in.count = 4;
	io.writex.in.data = (const uint8_t *)&fnum;	
	req = smb_raw_write_send(cli->tree, &io);

	/* unlock the range */
	cli->session->pid = 1;
	smbcli_unlock(cli->tree, fnum, 0, 4);

	/* and recv the async write reply */
	status = smb_raw_write_recv(req, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	smbcli_close(cli->tree, fnum);

done:
	return ret;
}


/*
  test a lock that conflicts with an existing lock
*/
static bool test_mux_lock(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	union smb_lock io;
	NTSTATUS status;
	int fnum;
	bool ret = true;
	struct smbcli_request *req;
	struct smb_lock_entry lock[1];
	struct timeval t;

	printf("TESTING MULTIPLEXED LOCK/LOCK/UNLOCK\n");

	fnum = smbcli_open(cli->tree, BASEDIR "\\write.dat", O_RDWR | O_CREAT, DENY_NONE);
	if (fnum == -1) {
		printf("open failed in mux_write - %s\n", smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	printf("establishing a lock\n");
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

	printf("the second lock will conflict with the first\n");
	lock[0].pid = 2;
	io.lockx.in.timeout = 1000;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	printf("this will too, but we'll unlock while waiting\n");
	t = timeval_current();
	req = smb_raw_lock_send(cli->tree, &io);

	printf("unlock the first range\n");
	lock[0].pid = 1;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.timeout = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("recv the async reply\n");
	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_OK);	

	printf("async lock took %.2f msec\n", timeval_elapsed(&t) * 1000);
	if (timeval_elapsed(&t) > 0.1) {
		printf("failed to trigger early lock retry\n");
		return false;		
	}

	printf("reopening with an exit\n");
	smb_raw_exit(cli->session);
	fnum = smbcli_open(cli->tree, BASEDIR "\\write.dat", O_RDWR | O_CREAT, DENY_NONE);

	printf("Now trying with a cancel\n");

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
	io.lockx.in.timeout = 1000;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	req = smb_raw_lock_send(cli->tree, &io);

	/* cancel the blocking lock */
	smb_raw_ntcancel(req);

	printf("sending 2nd cancel\n");
	/* the 2nd cancel is totally harmless, but tests the server trying to 
	   cancel an already cancelled request */
	smb_raw_ntcancel(req);

	printf("sent 2nd cancel\n");

	lock[0].pid = 1;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.timeout = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);	

	printf("cancel a lock using exit to close file\n");
	lock[0].pid = 1;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.timeout = 1000;

	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	t = timeval_current();
	lock[0].pid = 2;
	req = smb_raw_lock_send(cli->tree, &io);

	smb_raw_exit(cli->session);
	smb_raw_exit(cli->session);
	smb_raw_exit(cli->session);
	smb_raw_exit(cli->session);

	printf("recv the async reply\n");
	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	printf("async lock exit took %.2f msec\n", timeval_elapsed(&t) * 1000);
	if (timeval_elapsed(&t) > 0.1) {
		printf("failed to trigger early lock failure\n");
		return false;		
	}

done:
	return ret;
}



/* 
   basic testing of multiplexing notify
*/
bool torture_raw_mux(struct torture_context *torture, struct smbcli_state *cli)
{
	bool ret = true;
		
	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	ret &= test_mux_open(cli, torture);
	ret &= test_mux_write(cli, torture);
	ret &= test_mux_lock(cli, torture);

	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}
