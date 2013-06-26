/* 
   Unix SMB/CIFS implementation.
   test suite for various lock operations
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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "system/time.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "lib/cmdline/popt_common.h"
#include "param/param.h"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s\n", \
			__location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_STATUS_CONT(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s\n", \
			__location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
	}} while (0)

#define CHECK_STATUS_OR(status, correct1, correct2) do { \
	if ((!NT_STATUS_EQUAL(status, correct1)) && \
	    (!NT_STATUS_EQUAL(status, correct2))) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s or %s\n", \
			__location__, nt_errstr(status), nt_errstr(correct1), \
			nt_errstr(correct2)); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_STATUS_OR_CONT(status, correct1, correct2) do { \
	if ((!NT_STATUS_EQUAL(status, correct1)) && \
	    (!NT_STATUS_EQUAL(status, correct2))) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s or %s\n", \
			__location__, nt_errstr(status), nt_errstr(correct1), \
			nt_errstr(correct2)); \
		ret = false; \
	}} while (0)
#define BASEDIR "\\testlock"

#define TARGET_IS_W2K8(_tctx) (torture_setting_bool(_tctx, "w2k8", false))
#define TARGET_IS_WIN7(_tctx) (torture_setting_bool(_tctx, "win7", false))
#define TARGET_IS_WINDOWS(_tctx) \
	((torture_setting_bool(_tctx, "w2k3", false)) || \
	 (torture_setting_bool(_tctx, "w2k8", false)) || \
	 (torture_setting_bool(_tctx, "win7", false)))
#define TARGET_IS_SAMBA3(_tctx) (torture_setting_bool(_tctx, "samba3", false))
#define TARGET_IS_SAMBA4(_tctx) (torture_setting_bool(_tctx, "samba4", false))

#define TARGET_SUPPORTS_INVALID_LOCK_RANGE(_tctx) \
	(torture_setting_bool(_tctx, "invalid_lock_range_support", true))
#define TARGET_SUPPORTS_SMBEXIT(_tctx) \
    (torture_setting_bool(_tctx, "smbexit_pdu_support", true))
#define TARGET_SUPPORTS_SMBLOCK(_tctx) \
    (torture_setting_bool(_tctx, "smblock_pdu_support", true))
#define TARGET_SUPPORTS_OPENX_DENY_DOS(_tctx) \
    (torture_setting_bool(_tctx, "openx_deny_dos_support", true))
#define TARGET_RETURNS_RANGE_NOT_LOCKED(_tctx) \
    (torture_setting_bool(_tctx, "range_not_locked_on_file_close", true))
/*
  test SMBlock and SMBunlock ops
*/
static bool test_lock(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_lock io;
	NTSTATUS status;
	bool ret = true;
	int fnum;
	const char *fname = BASEDIR "\\test.txt";

	if (!TARGET_SUPPORTS_SMBLOCK(tctx))
		torture_skip(tctx, "Target does not support the SMBlock PDU");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Testing RAW_LOCK_LOCK\n");
	io.generic.level = RAW_LOCK_LOCK;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	torture_comment(tctx, "Trying 0/0 lock\n");
	io.lock.level = RAW_LOCK_LOCK;
	io.lock.in.file.fnum = fnum;
	io.lock.in.count = 0;
	io.lock.in.offset = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli->session->pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli->session->pid--;
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Trying 0/1 lock\n");
	io.lock.level = RAW_LOCK_LOCK;
	io.lock.in.file.fnum = fnum;
	io.lock.in.count = 1;
	io.lock.in.offset = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli->session->pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	cli->session->pid--;
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying 0xEEFFFFFF lock\n");
	io.lock.level = RAW_LOCK_LOCK;
	io.lock.in.file.fnum = fnum;
	io.lock.in.count = 4000;
	io.lock.in.offset = 0xEEFFFFFF;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli->session->pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	cli->session->pid--;
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying 0xEF000000 lock\n");
	io.lock.level = RAW_LOCK_LOCK;
	io.lock.in.file.fnum = fnum;
	io.lock.in.count = 4000;
	io.lock.in.offset = 0xEEFFFFFF;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli->session->pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);
	cli->session->pid--;
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying max lock\n");
	io.lock.level = RAW_LOCK_LOCK;
	io.lock.in.file.fnum = fnum;
	io.lock.in.count = 4000;
	io.lock.in.offset = 0xEF000000;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli->session->pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);
	cli->session->pid--;
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying wrong pid unlock\n");
	io.lock.level = RAW_LOCK_LOCK;
	io.lock.in.file.fnum = fnum;
	io.lock.in.count = 4002;
	io.lock.in.offset = 10001;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	cli->session->pid++;
	io.lock.level = RAW_LOCK_UNLOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	cli->session->pid--;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test locking&X ops
*/
static bool test_lockx(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_lock io;
	struct smb_lock_entry lock[1];
	NTSTATUS status;
	bool ret = true;
	int fnum;
	const char *fname = BASEDIR "\\test.txt";

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Testing RAW_LOCK_LOCKX\n");
	io.generic.level = RAW_LOCK_LOCKX;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 10;
	lock[0].count = 1;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);


	torture_comment(tctx, "Trying 0xEEFFFFFF lock\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].count = 4000;
	lock[0].offset = 0xEEFFFFFF;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	lock[0].pid--;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying 0xEF000000 lock\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].count = 4000;
	lock[0].offset = 0xEF000000;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);
	lock[0].pid--;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying zero lock\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].count = 0;
	lock[0].offset = ~0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid--;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying max lock\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].count = 0;
	lock[0].offset = ~0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid--;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying 2^63\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].count = 1;
	lock[0].offset = 1;
	lock[0].offset <<= 63;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	lock[0].pid--;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying 2^63 - 1\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].count = 1;
	lock[0].offset = 1;
	lock[0].offset <<= 63;
	lock[0].offset--;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid++;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);
	lock[0].pid--;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(tctx, "Trying max lock 2\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].count = 1;
	lock[0].offset = ~0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid++;
	lock[0].count = 2;
	status = smb_raw_lock(cli->tree, &io);
	if (TARGET_SUPPORTS_INVALID_LOCK_RANGE(tctx))
		CHECK_STATUS(status, NT_STATUS_INVALID_LOCK_RANGE);
	else
		CHECK_STATUS(status, NT_STATUS_OK);
	lock[0].pid--;
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	lock[0].count = 1;
	status = smb_raw_lock(cli->tree, &io);

	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/*
  test high pid
*/
static bool test_pidhigh(struct torture_context *tctx, 
						 struct smbcli_state *cli)
{
	union smb_lock io;
	struct smb_lock_entry lock[1];
	NTSTATUS status;
	bool ret = true;
	int fnum;
	const char *fname = BASEDIR "\\test.txt";
	uint8_t c = 1;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Testing high pid\n");
	io.generic.level = RAW_LOCK_LOCKX;

	cli->session->pid = 1;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	if (smbcli_write(cli->tree, fnum, 0, &c, 0, 1) != 1) {
		torture_result(tctx, TORTURE_FAIL,
			"Failed to write 1 byte - %s\n",
			smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 0;
	lock[0].count = 0xFFFFFFFF;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (smbcli_read(cli->tree, fnum, &c, 0, 1) != 1) {
		torture_result(tctx, TORTURE_FAIL,
			"Failed to read 1 byte - %s\n",
			smbcli_errstr(cli->tree));
		ret = false;
		goto done;
	}

	cli->session->pid = 2;

	if (smbcli_read(cli->tree, fnum, &c, 0, 1) == 1) {
		torture_result(tctx, TORTURE_FAIL,
			"pid is incorrect handled for read with lock!\n");
		ret = false;
		goto done;
	}

	cli->session->pid = 0x10001;

	if (smbcli_read(cli->tree, fnum, &c, 0, 1) != 1) {
		torture_result(tctx, TORTURE_FAIL,
			"High pid is used on this server!\n");
		ret = false;
	} else {
		torture_warning(tctx, "High pid is not used on this server (correct)\n");
	}

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test locking&X async operation
*/
static bool test_async(struct torture_context *tctx, 
					   struct smbcli_state *cli)
{
	struct smbcli_session *session;
	struct smb_composite_sesssetup setup;
	struct smbcli_tree *tree;
	union smb_tcon tcon;
	const char *host, *share;
	union smb_lock io;
	struct smb_lock_entry lock[2];
	NTSTATUS status;
	bool ret = true;
	int fnum;
	const char *fname = BASEDIR "\\test.txt";
	time_t t;
	struct smbcli_request *req, *req2;
	struct smbcli_session_options options;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	lpcfg_smbcli_session_options(tctx->lp_ctx, &options);

	torture_comment(tctx, "Testing LOCKING_ANDX_CANCEL_LOCK\n");
	io.generic.level = RAW_LOCK_LOCKX;

	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 100;
	lock[0].count = 10;
	lock[1].pid = cli->session->pid;
	lock[1].offset = 110;
	lock[1].count = 10;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	t = time_mono(NULL);

	torture_comment(tctx, "Testing cancel by CANCEL_LOCK\n");

	/* setup a timed lock */
	io.lockx.in.timeout = 10000;
	req = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));

	/* cancel the wrong range */
	lock[0].offset = 0;
	io.lockx.in.timeout = 0;
	io.lockx.in.mode = LOCKING_ANDX_CANCEL_LOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_DOS(ERRDOS, ERRcancelviolation));

	/* cancel with the wrong bits set */
	lock[0].offset = 100;
	io.lockx.in.timeout = 0;
	io.lockx.in.mode = LOCKING_ANDX_CANCEL_LOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_DOS(ERRDOS, ERRcancelviolation));

	/* cancel the right range */
	lock[0].offset = 100;
	io.lockx.in.timeout = 0;
	io.lockx.in.mode = LOCKING_ANDX_CANCEL_LOCK | LOCKING_ANDX_LARGE_FILES;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* receive the failed lock request */
	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	torture_assert(tctx,!(time_mono(NULL) > t+2), talloc_asprintf(tctx,
		       "lock cancel was not immediate (%s)\n", __location__));

	/* MS-CIFS (2.2.4.32.1) states that a cancel is honored if and only
	 * if the lock vector contains one entry. When given mutliple cancel
	 * requests in a single PDU we expect the server to return an
	 * error. Samba4 handles this correctly. Windows servers seem to
	 * accept the request but only cancel the first lock.  Samba3
	 * now does what Windows does (JRA).
	 */
	torture_comment(tctx, "Testing multiple cancel\n");

	/* acquire second lock */
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = &lock[1];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* setup 2 timed locks */
	t = time_mono(NULL);
	io.lockx.in.timeout = 10000;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lock[0];
	req = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));
	io.lockx.in.locks = &lock[1];
	req2 = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req2 != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));

	/* try to cancel both locks in the same packet */
	io.lockx.in.timeout = 0;
	io.lockx.in.lock_cnt = 2;
	io.lockx.in.mode = LOCKING_ANDX_CANCEL_LOCK | LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = lock;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_warning(tctx, "Target server accepted a lock cancel "
			      "request with multiple locks. This violates "
			      "MS-CIFS 2.2.4.32.1.\n");

	/* receive the failed lock requests */
	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	torture_assert(tctx,!(time_mono(NULL) > t+2), talloc_asprintf(tctx,
		       "first lock was not cancelled immediately (%s)\n",
		       __location__));

	/* send cancel to second lock */
	io.lockx.in.timeout = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_CANCEL_LOCK |
			   LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = &lock[1];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smbcli_request_simple_recv(req2);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	torture_assert(tctx,!(time_mono(NULL) > t+2), talloc_asprintf(tctx,
		       "second lock was not cancelled immediately (%s)\n",
		       __location__));

	/* cleanup the second lock */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = &lock[1];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* If a lock request contained multiple ranges and we are cancelling
	 * one while it's still pending, what happens? */
	torture_comment(tctx, "Testing cancel 1/2 lock request\n");

	/* Send request with two ranges */
	io.lockx.in.timeout = -1;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 2;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = lock;
	req = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup pending lock (%s)\n", __location__));

	/* Try to cancel the first lock range */
	io.lockx.in.timeout = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_CANCEL_LOCK | LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Locking request should've failed and second range should be
	 * unlocked */
	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = &lock[1];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Cleanup both locks */
	io.lockx.in.ulock_cnt = 2;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = lock;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing cancel 2/2 lock request\n");

	/* Lock second range so it contends */
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = &lock[1];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Send request with two ranges */
	io.lockx.in.timeout = -1;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 2;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = lock;
	req = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup pending lock (%s)\n", __location__));

	/* Try to cancel the second lock range */
	io.lockx.in.timeout = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_CANCEL_LOCK | LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = &lock[1];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Locking request should've failed and first range should be
	 * unlocked */
	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Cleanup both locks */
	io.lockx.in.ulock_cnt = 2;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.locks = lock;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing cancel by unlock\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.lockx.in.timeout = 5000;
	req = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));

	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	t = time_mono(NULL);
	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_assert(tctx,!(time_mono(NULL) > t+2), talloc_asprintf(tctx,
		       "lock cancel by unlock was not immediate (%s) - took %d secs\n",
		       __location__, (int)(time_mono(NULL)-t)));

	torture_comment(tctx, "Testing cancel by close\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	t = time_mono(NULL);
	io.lockx.in.timeout = 10000;
	req = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));

	status = smbcli_close(cli->tree, fnum);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smbcli_request_simple_recv(req);
	if (TARGET_RETURNS_RANGE_NOT_LOCKED(tctx))
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	else
		CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	torture_assert(tctx,!(time_mono(NULL) > t+2), talloc_asprintf(tctx,
		       "lock cancel by close was not immediate (%s)\n", __location__));

	torture_comment(tctx, "create a new sessions\n");
	session = smbcli_session_init(cli->transport, tctx, false, options);
	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities;
	setup.in.workgroup = lpcfg_workgroup(tctx->lp_ctx);
	setup.in.credentials = cmdline_credentials;
	setup.in.gensec_settings = lpcfg_gensec_settings(tctx, tctx->lp_ctx);
	status = smb_composite_sesssetup(session, &setup);
	CHECK_STATUS(status, NT_STATUS_OK);
	session->vuid = setup.out.vuid;

	torture_comment(tctx, "create new tree context\n");
	share = torture_setting_string(tctx, "share", NULL);
	host  = torture_setting_string(tctx, "host", NULL);
	tree = smbcli_tree_init(session, tctx, false);
	tcon.generic.level = RAW_TCON_TCONX;
	tcon.tconx.in.flags = 0;
	tcon.tconx.in.password = data_blob(NULL, 0);
	tcon.tconx.in.path = talloc_asprintf(tctx, "\\\\%s\\%s", host, share);
	tcon.tconx.in.device = "A:";
	status = smb_raw_tcon(tree, tctx, &tcon);
	CHECK_STATUS(status, NT_STATUS_OK);
	tree->tid = tcon.tconx.out.tid;

	torture_comment(tctx, "Testing cancel by exit\n");
	if (TARGET_SUPPORTS_SMBEXIT(tctx)) {
		fname = BASEDIR "\\test_exit.txt";
		fnum = smbcli_open(tree, fname, O_RDWR|O_CREAT, DENY_NONE);
		torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
			       "Failed to reopen %s - %s\n",
			       fname, smbcli_errstr(tree)));

		io.lockx.level = RAW_LOCK_LOCKX;
		io.lockx.in.file.fnum = fnum;
		io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
		io.lockx.in.timeout = 0;
		io.lockx.in.ulock_cnt = 0;
		io.lockx.in.lock_cnt = 1;
		lock[0].pid = session->pid;
		lock[0].offset = 100;
		lock[0].count = 10;
		io.lockx.in.locks = &lock[0];
		status = smb_raw_lock(tree, &io);
		CHECK_STATUS(status, NT_STATUS_OK);

		io.lockx.in.ulock_cnt = 0;
		io.lockx.in.lock_cnt = 1;
		io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
		io.lockx.in.timeout = 0;
		status = smb_raw_lock(tree, &io);
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

		io.lockx.in.timeout = 10000;
		t = time_mono(NULL);
		req = smb_raw_lock_send(tree, &io);
		torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
			       "Failed to setup timed lock (%s)\n",
			       __location__));

		status = smb_raw_exit(session);
		CHECK_STATUS(status, NT_STATUS_OK);

		status = smbcli_request_simple_recv(req);
		if (TARGET_RETURNS_RANGE_NOT_LOCKED(tctx))
			CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
		else
			CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

		torture_assert(tctx,!(time_mono(NULL) > t+2), talloc_asprintf(tctx,
			       "lock cancel by exit was not immediate (%s)\n",
			       __location__));
	}
	else {
		torture_comment(tctx,
				"  skipping test, SMBExit not supported\n");
	}

	torture_comment(tctx, "Testing cancel by ulogoff\n");
	fname = BASEDIR "\\test_ulogoff.txt";
	fnum = smbcli_open(tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to reopen %s - %s\n",
		       fname, smbcli_errstr(tree)));

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = session->pid;
	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	status = smb_raw_lock(tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	io.lockx.in.timeout = 10000;
	t = time_mono(NULL);
	req = smb_raw_lock_send(tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));

	status = smb_raw_ulogoff(session);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smbcli_request_simple_recv(req);
	if (TARGET_RETURNS_RANGE_NOT_LOCKED(tctx)) {
		if (NT_STATUS_EQUAL(NT_STATUS_FILE_LOCK_CONFLICT, status)) {
			torture_result(tctx, TORTURE_FAIL,
				"lock not canceled by ulogoff - %s "
				"(ignored because of vfs_vifs fails it)\n",
				nt_errstr(status));
			smb_tree_disconnect(tree);
			smb_raw_exit(session);
			goto done;
		}
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	} else {
		CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);
	}

	torture_assert(tctx,!(time_mono(NULL) > t+2), talloc_asprintf(tctx,
		       "lock cancel by ulogoff was not immediate (%s)\n", __location__));

	torture_comment(tctx, "Testing cancel by tdis\n");
	tree->session = cli->session;

	fname = BASEDIR "\\test_tdis.txt";
	fnum = smbcli_open(tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to reopen %s - %s\n",
		       fname, smbcli_errstr(tree)));

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb_raw_lock(tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	io.lockx.in.timeout = 10000;
	t = time_mono(NULL);
	req = smb_raw_lock_send(tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));

	status = smb_tree_disconnect(tree);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smbcli_request_simple_recv(req);
	if (TARGET_RETURNS_RANGE_NOT_LOCKED(tctx))
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	else
		CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	torture_assert(tctx,!(time_mono(NULL) > t+2), talloc_asprintf(tctx,
		       "lock cancel by tdis was not immediate (%s)\n", __location__));

done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/*
  test NT_STATUS_LOCK_NOT_GRANTED vs. NT_STATUS_FILE_LOCK_CONFLICT
*/
static bool test_errorcode(struct torture_context *tctx, 
						   struct smbcli_state *cli)
{
	union smb_lock io;
	union smb_open op;
	struct smb_lock_entry lock[2];
	NTSTATUS status;
	bool ret = true;
	int fnum, fnum2;
	const char *fname;
	struct smbcli_request *req;
	time_t start;
	int t;
	int delay;
	uint16_t deny_mode = 0;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Testing LOCK_NOT_GRANTED vs. FILE_LOCK_CONFLICT\n");

	torture_comment(tctx, "Testing with timeout = 0\n");
	fname = BASEDIR "\\test0.txt";
	t = 0;

	/*
	 * the first run is with t = 0,
	 * the second with t > 0 (=1)
	 */
next_run:
	/*
	 * use the DENY_DOS mode, that creates two fnum's of one low-level
	 * file handle, this demonstrates that the cache is per fnum, not
	 * per file handle
	 */
	if (TARGET_SUPPORTS_OPENX_DENY_DOS(tctx))
	    deny_mode = OPENX_MODE_DENY_DOS;
	else
	    deny_mode = OPENX_MODE_DENY_NONE;

	op.openx.level = RAW_OPEN_OPENX;
	op.openx.in.fname = fname;
	op.openx.in.flags = OPENX_FLAGS_ADDITIONAL_INFO;
	op.openx.in.open_mode = OPENX_MODE_ACCESS_RDWR | deny_mode;
	op.openx.in.open_func = OPENX_OPEN_FUNC_OPEN | OPENX_OPEN_FUNC_CREATE;
	op.openx.in.search_attrs = 0;
	op.openx.in.file_attrs = 0;
	op.openx.in.write_time = 0;
	op.openx.in.size = 0;
	op.openx.in.timeout = 0;

	status = smb_raw_open(cli->tree, tctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = op.openx.out.file.fnum;

	status = smb_raw_open(cli->tree, tctx, &op);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = op.openx.out.file.fnum;

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = t;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/*
	 * demonstrate that the first conflicting lock on each handle give LOCK_NOT_GRANTED
	 * this also demonstrates that the error code cache is per file handle
	 * (LOCK_NOT_GRANTED is only be used when timeout is 0!)
	 */
	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));

	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));

	/* demonstrate that each following conflict gives FILE_LOCK_CONFLICT */
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	/* demonstrate that the smbpid doesn't matter */
	lock[0].pid++;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);
	lock[0].pid--;

	/* 
	 * demonstrate the a successful lock with count = 0 and the same offset,
	 * doesn't reset the error cache
	 */
	lock[0].offset = 100;
	lock[0].count = 0;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	/* 
	 * demonstrate the a successful lock with count = 0 and outside the locked range,
	 * doesn't reset the error cache
	 */
	lock[0].offset = 110;
	lock[0].count = 0;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	lock[0].offset = 99;
	lock[0].count = 0;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	/* demonstrate that a changing count doesn't reset the error cache */
	lock[0].offset = 100;
	lock[0].count = 5;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	lock[0].offset = 100;
	lock[0].count = 15;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	/* 
	 * demonstrate the a lock with count = 0 and inside the locked range,
	 * fails and resets the error cache
	 */
	lock[0].offset = 101;
	lock[0].count = 0;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	/* demonstrate the a changing offset, resets the error cache */
	lock[0].offset = 105;
	lock[0].count = 10;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	lock[0].offset = 95;
	lock[0].count = 9;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	/* 
	 * demonstrate the a successful lock in a different range, 
	 * doesn't reset the cache, the failing lock on the 2nd handle
	 * resets the cache
	 */
	lock[0].offset = 120;
	lock[0].count = 15;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));

	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.file.fnum = fnum;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, (t?NT_STATUS_FILE_LOCK_CONFLICT:NT_STATUS_LOCK_NOT_GRANTED));
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	/* end of the loop */
	if (t == 0) {
		smb_raw_exit(cli->session);
		t = 1;
		torture_comment(tctx, "Testing with timeout > 0 (=%d)\n",
				t);
		fname = BASEDIR "\\test1.txt";
		goto next_run;
	}

	t = 4000;
	torture_comment(tctx, "Testing special cases with timeout > 0 (=%d)\n",
			t);

	/*
	 * the following 3 test sections demonstrate that
	 * the cache is only set when the error is reported
	 * to the client (after the timeout went by)
	 */
	smb_raw_exit(cli->session);
	torture_comment(tctx, "Testing a conflict while a lock is pending\n");
	fname = BASEDIR "\\test2.txt";
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to reopen %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	start = time_mono(NULL);
	io.lockx.in.timeout = t;
	req = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));

	io.lockx.in.timeout = 0;
	lock[0].offset = 105;
	lock[0].count = 10;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	delay = t / 1000;
	if (TARGET_IS_W2K8(tctx) || TARGET_IS_WIN7(tctx)) {
		delay /= 2;
	}

	torture_assert(tctx,!(time_mono(NULL) < start+delay), talloc_asprintf(tctx,
		       "lock comes back to early timeout[%d] delay[%d]"
		       "(%s)\n", t, delay, __location__));

	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	smbcli_close(cli->tree, fnum);
	fname = BASEDIR "\\test3.txt";
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to reopen %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	start = time_mono(NULL);
	io.lockx.in.timeout = t;
	req = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));

	io.lockx.in.timeout = 0;
	lock[0].offset = 105;
	lock[0].count = 10;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	delay = t / 1000;
	if (TARGET_IS_W2K8(tctx) || TARGET_IS_WIN7(tctx)) {
		delay /= 2;
	}

	torture_assert(tctx,!(time_mono(NULL) < start+delay), talloc_asprintf(tctx,
		       "lock comes back to early timeout[%d] delay[%d]"
		       "(%s)\n", t, delay, __location__));

	lock[0].offset = 100;
	lock[0].count = 10;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	smbcli_close(cli->tree, fnum);
	fname = BASEDIR "\\test4.txt";
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to reopen %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	start = time_mono(NULL);
	io.lockx.in.timeout = t;
	req = smb_raw_lock_send(cli->tree, &io);
	torture_assert(tctx,(req != NULL), talloc_asprintf(tctx,
		       "Failed to setup timed lock (%s)\n", __location__));

	io.lockx.in.timeout = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	status = smbcli_request_simple_recv(req);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	delay = t / 1000;
	if (TARGET_IS_W2K8(tctx) || TARGET_IS_WIN7(tctx)) {
		delay /= 2;
	}

	torture_assert(tctx,!(time_mono(NULL) < start+delay), talloc_asprintf(tctx,
		       "lock comes back to early timeout[%d] delay[%d]"
		       "(%s)\n", t, delay, __location__));

	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}


/*
  test LOCKING_ANDX_CHANGE_LOCKTYPE
*/
static bool test_changetype(struct torture_context *tctx, 
							struct smbcli_state *cli)
{
	union smb_lock io;
	struct smb_lock_entry lock[2];
	NTSTATUS status;
	bool ret = true;
	int fnum;
	uint8_t c = 0;
	const char *fname = BASEDIR "\\test.txt";

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Testing LOCKING_ANDX_CHANGE_LOCKTYPE\n");
	io.generic.level = RAW_LOCK_LOCKX;
	
	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_SHARED_LOCK;
	io.lockx.in.timeout = 0;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	lock[0].pid = cli->session->pid;
	lock[0].offset = 100;
	lock[0].count = 10;
	io.lockx.in.locks = &lock[0];
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	if (smbcli_write(cli->tree, fnum, 0, &c, 100, 1) == 1) {
		torture_result(tctx, TORTURE_FAIL,
			"allowed write on read locked region (%s)\n", __location__);
		ret = false;
		goto done;
	}

	/* windows server don't seem to support this */
	io.lockx.in.mode = LOCKING_ANDX_CHANGE_LOCKTYPE;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_DOS(ERRDOS, ERRnoatomiclocks));

	if (smbcli_write(cli->tree, fnum, 0, &c, 100, 1) == 1) {
		torture_result(tctx, TORTURE_FAIL,
			"allowed write after lock change (%s)\n", __location__);
		ret = false;
		goto done;
	}

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

struct double_lock_test {
	struct smb_lock_entry lock1;
	struct smb_lock_entry lock2;
	NTSTATUS exp_status;
};

/**
 * Tests zero byte locks.
 */
static struct double_lock_test zero_byte_tests[] = {
	/* {pid, offset, count}, {pid, offset, count}, status */

	/** First, takes a zero byte lock at offset 10. Then:
	*   - Taking 0 byte lock at 10 should succeed.
	*   - Taking 1 byte locks at 9,10,11 should succeed.
	*   - Taking 2 byte lock at 9 should fail.
	*   - Taking 2 byte lock at 10 should succeed.
	*   - Taking 3 byte lock at 9 should fail.
	*/
	{{1000, 10, 0}, {1001, 10, 0}, NT_STATUS_OK},
	{{1000, 10, 0}, {1001, 9, 1},  NT_STATUS_OK},
	{{1000, 10, 0}, {1001, 10, 1}, NT_STATUS_OK},
	{{1000, 10, 0}, {1001, 11, 1}, NT_STATUS_OK},
	{{1000, 10, 0}, {1001, 9, 2},  NT_STATUS_LOCK_NOT_GRANTED},
	{{1000, 10, 0}, {1001, 10, 2}, NT_STATUS_OK},
	{{1000, 10, 0}, {1001, 9, 3},  NT_STATUS_LOCK_NOT_GRANTED},

	/** Same, but opposite order. */
	{{1001, 10, 0}, {1000, 10, 0}, NT_STATUS_OK},
	{{1001, 9, 1},  {1000, 10, 0}, NT_STATUS_OK},
	{{1001, 10, 1}, {1000, 10, 0}, NT_STATUS_OK},
	{{1001, 11, 1}, {1000, 10, 0}, NT_STATUS_OK},
	{{1001, 9, 2},  {1000, 10, 0}, NT_STATUS_LOCK_NOT_GRANTED},
	{{1001, 10, 2}, {1000, 10, 0}, NT_STATUS_OK},
	{{1001, 9, 3},  {1000, 10, 0}, NT_STATUS_LOCK_NOT_GRANTED},

	/** Zero zero case. */
	{{1000, 0, 0},  {1001, 0, 0},  NT_STATUS_OK},
};

static bool test_zerobytelocks(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_lock io;
	NTSTATUS status;
	bool ret = true;
	int fnum, i;
	const char *fname = BASEDIR "\\zero.txt";

	torture_comment(tctx, "Testing zero length byte range locks:\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	io.generic.level = RAW_LOCK_LOCKX;

	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	/* Setup initial parameters */
	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.file.fnum = fnum;
	io.lockx.in.mode = LOCKING_ANDX_LARGE_FILES; /* Exclusive */
	io.lockx.in.timeout = 0;

	/* Try every combination of locks in zero_byte_tests. The first lock is
	 * assumed to succeed. The second lock may contend, depending on the
	 * expected status. */
	for (i = 0;
	     i < ARRAY_SIZE(zero_byte_tests);
	     i++) {
		torture_comment(tctx, "  ... {%d, %llu, %llu} + {%d, %llu, %llu} = %s\n",
		    zero_byte_tests[i].lock1.pid,
		    (unsigned long long) zero_byte_tests[i].lock1.offset,
		    (unsigned long long) zero_byte_tests[i].lock1.count,
		    zero_byte_tests[i].lock2.pid,
		    (unsigned long long) zero_byte_tests[i].lock2.offset,
		    (unsigned long long) zero_byte_tests[i].lock2.count,
		    nt_errstr(zero_byte_tests[i].exp_status));

		/* Lock both locks. */
		io.lockx.in.ulock_cnt = 0;
		io.lockx.in.lock_cnt = 1;

		io.lockx.in.locks = discard_const_p(struct smb_lock_entry,
						    &zero_byte_tests[i].lock1);
		status = smb_raw_lock(cli->tree, &io);
		CHECK_STATUS(status, NT_STATUS_OK);

		io.lockx.in.locks = discard_const_p(struct smb_lock_entry,
						    &zero_byte_tests[i].lock2);
		status = smb_raw_lock(cli->tree, &io);

		if (NT_STATUS_EQUAL(zero_byte_tests[i].exp_status,
			NT_STATUS_LOCK_NOT_GRANTED)) {
			/* Allow either of the failure messages and keep going
			 * if we see the wrong status. */
			CHECK_STATUS_OR_CONT(status,
			    NT_STATUS_LOCK_NOT_GRANTED,
			    NT_STATUS_FILE_LOCK_CONFLICT);

		} else {
			CHECK_STATUS_CONT(status,
			    zero_byte_tests[i].exp_status);
		}

		/* Unlock both locks. */
		io.lockx.in.ulock_cnt = 1;
		io.lockx.in.lock_cnt = 0;

		if (NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
			status = smb_raw_lock(cli->tree, &io);
			CHECK_STATUS(status, NT_STATUS_OK);
		}

		io.lockx.in.locks = discard_const_p(struct smb_lock_entry,
						    &zero_byte_tests[i].lock1);
		status = smb_raw_lock(cli->tree, &io);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

done:
	smbcli_close(cli->tree, fnum);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

static bool test_unlock(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_lock io;
	NTSTATUS status;
	bool ret = true;
	int fnum1, fnum2;
	const char *fname = BASEDIR "\\unlock.txt";
	struct smb_lock_entry lock1;
	struct smb_lock_entry lock2;

	torture_comment(tctx, "Testing LOCKX unlock:\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum1 != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	fnum2 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum2 != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	/* Setup initial parameters */
	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.timeout = 0;

	lock1.pid = cli->session->pid;
	lock1.offset = 0;
	lock1.count = 10;
	lock2.pid = cli->session->pid - 1;
	lock2.offset = 0;
	lock2.count = 10;

	/**
	 * Take exclusive lock, then unlock it with a shared-unlock call.
	 */
	torture_comment(tctx, "  taking exclusive lock.\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = 0;
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "  unlock the exclusive with a shared unlock call.\n");
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.mode = LOCKING_ANDX_SHARED_LOCK;
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "  try shared lock on pid2/fnum2, testing the unlock.\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_SHARED_LOCK;
	io.lockx.in.file.fnum = fnum2;
	io.lockx.in.locks = &lock2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/**
	 * Unlock a shared lock with an exclusive-unlock call.
	 */
	torture_comment(tctx, "  unlock new shared lock with exclusive unlock call.\n");
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.mode = 0;
	io.lockx.in.file.fnum = fnum2;
	io.lockx.in.locks = &lock2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "  try exclusive lock on pid1, testing the unlock.\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = 0;
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/**
	 * Test unlocking of 0-byte locks.
	 */

	torture_comment(tctx, "  lock shared and exclusive 0-byte locks, testing that Windows "
	    "always unlocks the exclusive first.\n");
	lock1.pid = cli->session->pid;
	lock1.offset = 10;
	lock1.count = 0;
	lock2.pid = cli->session->pid;
	lock2.offset = 5;
	lock2.count = 10;
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;

	/* lock 0-byte shared
	 * Note: Order of the shared/exclusive locks doesn't matter. */
	io.lockx.in.mode = LOCKING_ANDX_SHARED_LOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* lock 0-byte exclusive */
	io.lockx.in.mode = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* test contention */
	io.lockx.in.mode = LOCKING_ANDX_SHARED_LOCK;
	io.lockx.in.locks = &lock2;
	io.lockx.in.file.fnum = fnum2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS_OR(status, NT_STATUS_LOCK_NOT_GRANTED,
	    NT_STATUS_FILE_LOCK_CONFLICT);

	/* unlock */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* test - can we take a shared lock? */
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_SHARED_LOCK;
	io.lockx.in.file.fnum = fnum2;
	io.lockx.in.locks = &lock2;
	status = smb_raw_lock(cli->tree, &io);

	/* XXX Samba 3 will fail this test. This is temporary(because this isn't
	 * new to Win7, it succeeds in WinXP too), until I can come to a
	 * resolution as to whether Samba should support this or not. There is
	 * code to preference unlocking exclusive locks before shared locks,
	 * but its wrapped with "#ifdef ZERO_ZERO". -zkirsch */
	if (TARGET_IS_SAMBA3(tctx)) {
		CHECK_STATUS_OR(status, NT_STATUS_LOCK_NOT_GRANTED,
		    NT_STATUS_FILE_LOCK_CONFLICT);
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	/* cleanup */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);

	/* XXX Same as above. */
	if (TARGET_IS_SAMBA3(tctx)) {
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smbcli_close(cli->tree, fnum1);
	smbcli_close(cli->tree, fnum2);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

static bool test_multiple_unlock(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_lock io;
	NTSTATUS status;
	bool ret = true;
	int fnum1;
	const char *fname = BASEDIR "\\unlock_multiple.txt";
	struct smb_lock_entry lock1;
	struct smb_lock_entry lock2;
	struct smb_lock_entry locks[2];

	torture_comment(tctx, "Testing LOCKX multiple unlock:\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum1 != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	/* Setup initial parameters */
	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.timeout = 0;

	lock1.pid = cli->session->pid;
	lock1.offset = 0;
	lock1.count = 10;
	lock2.pid = cli->session->pid;
	lock2.offset = 10;
	lock2.count = 10;

	locks[0] = lock1;
	locks[1] = lock2;

	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.mode = 0; /* exclusive */

	/** Test1: Take second lock, but not first. */
	torture_comment(tctx, "  unlock 2 locks, first one not locked. Expect no locks "
	    "unlocked. \n");

	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lock2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try to unlock both locks. */
	io.lockx.in.ulock_cnt = 2;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.locks = locks;

	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	/* Second lock should not be unlocked. */
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lock2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* cleanup */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.locks = &lock2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/** Test2: Take first lock, but not second. */
	torture_comment(tctx, "  unlock 2 locks, second one not locked. Expect first lock "
	    "unlocked.\n");

	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try to unlock both locks. */
	io.lockx.in.ulock_cnt = 2;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.locks = locks;

	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	/* First lock should be unlocked. */
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Test3: Request 2 locks, second will contend.  What happens to the
	 * first? */
	torture_comment(tctx, "  request 2 locks, second one will contend. "
	   "Expect both to fail.\n");

	/* Lock the second range */
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lock2;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Request both locks */
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 2;
	io.lockx.in.locks = locks;

	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	/* First lock should be unlocked. */
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	io.lockx.in.ulock_cnt = 2;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.locks = locks;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Test4: Request unlock and lock. The lock contends, is the unlock
	 * then re-locked? */
	torture_comment(tctx, "  request unlock and lock, second one will "
	   "contend. Expect the unlock to succeed.\n");

	/* Lock both ranges */
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 2;
	io.lockx.in.locks = locks;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Attempt to unlock the first range and lock the second */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = locks;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_FILE_LOCK_CONFLICT);

	/* The first lock should've been unlocked */
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	io.lockx.in.ulock_cnt = 2;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.locks = locks;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smbcli_close(cli->tree, fnum1);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/**
 * torture_locktest5 covers stacking pretty well, but its missing two tests:
 * - stacking an exclusive on top of shared fails
 * - stacking two exclusives fail
 */
static bool test_stacking(struct torture_context *tctx, struct smbcli_state *cli)
{
	union smb_lock io;
	NTSTATUS status;
	bool ret = true;
	int fnum1;
	const char *fname = BASEDIR "\\stacking.txt";
	struct smb_lock_entry lock1;
	struct smb_lock_entry lock2;

	torture_comment(tctx, "Testing stacking:\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	io.generic.level = RAW_LOCK_LOCKX;

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum1 != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	/* Setup initial parameters */
	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.timeout = 0;

	lock1.pid = cli->session->pid;
	lock1.offset = 0;
	lock1.count = 10;
	lock2.pid = cli->session->pid - 1;
	lock2.offset = 0;
	lock2.count = 10;

	/**
	 * Try to take a shared lock, then stack an exclusive.
	 */
	torture_comment(tctx, "  stacking an exclusive on top of a shared lock fails.\n");
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;

	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_SHARED_LOCK;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS_OR(status, NT_STATUS_LOCK_NOT_GRANTED,
	    NT_STATUS_FILE_LOCK_CONFLICT);

	/* cleanup */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/**
	 * Prove that two exclusive locks do not stack.
	 */
	torture_comment(tctx, "  two exclusive locks do not stack.\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS_OR(status, NT_STATUS_LOCK_NOT_GRANTED,
	    NT_STATUS_FILE_LOCK_CONFLICT);

	/* cleanup */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smbcli_close(cli->tree, fnum1);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/**
 * Test how 0-byte read requests contend with byte range locks
 */
static bool test_zerobyteread(struct torture_context *tctx,
			      struct smbcli_state *cli)
{
	union smb_lock io;
	union smb_read rd;
	NTSTATUS status;
	bool ret = true;
	int fnum1, fnum2;
	const char *fname = BASEDIR "\\zerobyteread.txt";
	struct smb_lock_entry lock1;
	uint8_t c = 1;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	io.generic.level = RAW_LOCK_LOCKX;

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum1 != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	fnum2 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	torture_assert(tctx,(fnum2 != -1), talloc_asprintf(tctx,
		       "Failed to create %s - %s\n",
		       fname, smbcli_errstr(cli->tree)));

	/* Setup initial parameters */
	io.lockx.level = RAW_LOCK_LOCKX;
	io.lockx.in.timeout = 0;

	lock1.pid = cli->session->pid;
	lock1.offset = 0;
	lock1.count = 10;

	ZERO_STRUCT(rd);
	rd.readx.level = RAW_READ_READX;

	torture_comment(tctx, "Testing zero byte read on lock range:\n");

	/* Take an exclusive lock */
	torture_comment(tctx, "  taking exclusive lock.\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_EXCLUSIVE_LOCK;
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try a zero byte read */
	torture_comment(tctx, "  reading 0 bytes.\n");
	rd.readx.in.file.fnum = fnum2;
	rd.readx.in.offset    = 5;
	rd.readx.in.mincnt    = 0;
	rd.readx.in.maxcnt    = 0;
	rd.readx.in.remaining = 0;
	rd.readx.in.read_for_execute = false;
	rd.readx.out.data     = &c;
	status = smb_raw_read(cli->tree, &rd);
	torture_assert_int_equal_goto(tctx, rd.readx.out.nread, 0, ret, done,
				      "zero byte read did not return 0 bytes");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Unlock lock */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.mode = LOCKING_ANDX_EXCLUSIVE_LOCK;
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing zero byte read on zero byte lock "
			      "range:\n");

	/* Take an exclusive lock */
	torture_comment(tctx, "  taking exclusive 0-byte lock.\n");
	io.lockx.in.ulock_cnt = 0;
	io.lockx.in.lock_cnt = 1;
	io.lockx.in.mode = LOCKING_ANDX_EXCLUSIVE_LOCK;
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;
	lock1.offset = 5;
	lock1.count = 0;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try a zero byte read before the lock */
	torture_comment(tctx, "  reading 0 bytes before the lock.\n");
	rd.readx.in.file.fnum = fnum2;
	rd.readx.in.offset    = 4;
	rd.readx.in.mincnt    = 0;
	rd.readx.in.maxcnt    = 0;
	rd.readx.in.remaining = 0;
	rd.readx.in.read_for_execute = false;
	rd.readx.out.data     = &c;
	status = smb_raw_read(cli->tree, &rd);
	torture_assert_int_equal_goto(tctx, rd.readx.out.nread, 0, ret, done,
				      "zero byte read did not return 0 bytes");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try a zero byte read on the lock */
	torture_comment(tctx, "  reading 0 bytes on the lock.\n");
	rd.readx.in.file.fnum = fnum2;
	rd.readx.in.offset    = 5;
	rd.readx.in.mincnt    = 0;
	rd.readx.in.maxcnt    = 0;
	rd.readx.in.remaining = 0;
	rd.readx.in.read_for_execute = false;
	rd.readx.out.data     = &c;
	status = smb_raw_read(cli->tree, &rd);
	torture_assert_int_equal_goto(tctx, rd.readx.out.nread, 0, ret, done,
				      "zero byte read did not return 0 bytes");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try a zero byte read after the lock */
	torture_comment(tctx, "  reading 0 bytes after the lock.\n");
	rd.readx.in.file.fnum = fnum2;
	rd.readx.in.offset    = 6;
	rd.readx.in.mincnt    = 0;
	rd.readx.in.maxcnt    = 0;
	rd.readx.in.remaining = 0;
	rd.readx.in.read_for_execute = false;
	rd.readx.out.data     = &c;
	status = smb_raw_read(cli->tree, &rd);
	torture_assert_int_equal_goto(tctx, rd.readx.out.nread, 0, ret, done,
				      "zero byte read did not return 0 bytes");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Unlock lock */
	io.lockx.in.ulock_cnt = 1;
	io.lockx.in.lock_cnt = 0;
	io.lockx.in.mode = LOCKING_ANDX_EXCLUSIVE_LOCK;
	io.lockx.in.file.fnum = fnum1;
	io.lockx.in.locks = &lock1;
	status = smb_raw_lock(cli->tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smbcli_close(cli->tree, fnum1);
	smbcli_close(cli->tree, fnum2);
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/*
   basic testing of lock calls
*/
struct torture_suite *torture_raw_lock(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "lock");

	torture_suite_add_1smb_test(suite, "lockx", test_lockx);
	torture_suite_add_1smb_test(suite, "lock", test_lock);
	torture_suite_add_1smb_test(suite, "pidhigh", test_pidhigh);
	torture_suite_add_1smb_test(suite, "async", test_async);
	torture_suite_add_1smb_test(suite, "errorcode", test_errorcode);
	torture_suite_add_1smb_test(suite, "changetype", test_changetype);

	torture_suite_add_1smb_test(suite, "stacking", test_stacking);
	torture_suite_add_1smb_test(suite, "unlock", test_unlock);
	torture_suite_add_1smb_test(suite, "multiple_unlock",
	    test_multiple_unlock);
	torture_suite_add_1smb_test(suite, "zerobytelocks", test_zerobytelocks);
	torture_suite_add_1smb_test(suite, "zerobyteread", test_zerobyteread);

	return suite;
}
