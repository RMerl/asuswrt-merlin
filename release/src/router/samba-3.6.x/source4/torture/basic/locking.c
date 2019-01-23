/* 
   Unix SMB/CIFS implementation.

   basic locking tests

   Copyright (C) Andrew Tridgell 2000-2004
   Copyright (C) Jeremy Allison 2000-2004
   
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
#include "torture/util.h"
#include "system/time.h"
#include "system/filesys.h"

#define BASEDIR "\\locktest"

/*
  This test checks for two things:

  1) correct support for retaining locks over a close (ie. the server
     must not use posix semantics)
  2) support for lock timeouts
 */
static bool torture_locktest1(struct torture_context *tctx, 
			      struct smbcli_state *cli1,
			      struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\lockt1.lck";
	int fnum1, fnum2, fnum3;
	time_t t1, t2;
	unsigned int lock_timeout;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	torture_assert(tctx, fnum1 != -1,
				   talloc_asprintf(tctx, 
		"open of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree)));
	fnum2 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, 
		"open2 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree)));
	fnum3 = smbcli_open(cli2->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum3 != -1, talloc_asprintf(tctx, 
		"open3 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree)));

	torture_assert_ntstatus_ok(tctx, 
		smbcli_lock(cli1->tree, fnum1, 0, 4, 0, WRITE_LOCK),
		talloc_asprintf(tctx, "lock1 failed (%s)", smbcli_errstr(cli1->tree)));

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum3, 0, 4, 0, WRITE_LOCK)),
		"lock2 succeeded! This is a locking bug\n");

	if (!check_error(__location__, cli2, ERRDOS, ERRlock, 
			 NT_STATUS_LOCK_NOT_GRANTED)) return false;

	torture_assert(tctx,
		!NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum3, 0, 4, 0, WRITE_LOCK)),
		"lock2 succeeded! This is a locking bug\n");

	if (!check_error(__location__, cli2, ERRDOS, ERRlock, 
				 NT_STATUS_FILE_LOCK_CONFLICT)) return false;

	torture_assert_ntstatus_ok(tctx, 
		smbcli_lock(cli1->tree, fnum1, 5, 9, 0, WRITE_LOCK),
		talloc_asprintf(tctx, 
		"lock1 failed (%s)", smbcli_errstr(cli1->tree)));

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum3, 5, 9, 0, WRITE_LOCK)),
		"lock2 succeeded! This is a locking bug");
	
	if (!check_error(__location__, cli2, ERRDOS, ERRlock, 
				 NT_STATUS_LOCK_NOT_GRANTED)) return false;

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum3, 0, 4, 0, WRITE_LOCK)),
		"lock2 succeeded! This is a locking bug");
	
	if (!check_error(__location__, cli2, ERRDOS, ERRlock, 
				 NT_STATUS_LOCK_NOT_GRANTED)) return false;

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum3, 0, 4, 0, WRITE_LOCK)),
		"lock2 succeeded! This is a locking bug");

	if (!check_error(__location__, cli2, ERRDOS, ERRlock, 
			 NT_STATUS_FILE_LOCK_CONFLICT)) return false;

	lock_timeout = (6 + (random() % 20));
	torture_comment(tctx, "Testing lock timeout with timeout=%u\n", 
					lock_timeout);
	t1 = time_mono(NULL);
	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum3, 0, 4, lock_timeout * 1000, WRITE_LOCK)),
		"lock3 succeeded! This is a locking bug\n");

	if (!check_error(__location__, cli2, ERRDOS, ERRlock, 
				 NT_STATUS_FILE_LOCK_CONFLICT)) return false;
	t2 = time_mono(NULL);

	if (t2 - t1 < 5) {
		torture_fail(tctx, 
			"error: This server appears not to support timed lock requests");
	}
	torture_comment(tctx, "server slept for %u seconds for a %u second timeout\n",
	       (unsigned int)(t2-t1), lock_timeout);

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum2),
		talloc_asprintf(tctx, "close1 failed (%s)", smbcli_errstr(cli1->tree)));

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum3, 0, 4, 0, WRITE_LOCK)),
		"lock4 succeeded! This is a locking bug");
		
	if (!check_error(__location__, cli2, ERRDOS, ERRlock, 
			 NT_STATUS_FILE_LOCK_CONFLICT)) return false;

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close2 failed (%s)", smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli2->tree, fnum3),
		talloc_asprintf(tctx, "close3 failed (%s)", smbcli_errstr(cli2->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_unlink(cli1->tree, fname),
		talloc_asprintf(tctx, "unlink failed (%s)", smbcli_errstr(cli1->tree)));

	return true;
}


/*
  This test checks that 

  1) the server supports multiple locking contexts on the one SMB
  connection, distinguished by PID.  

  2) the server correctly fails overlapping locks made by the same PID (this
     goes against POSIX behaviour, which is why it is tricky to implement)

  3) the server denies unlock requests by an incorrect client PID
*/
static bool torture_locktest2(struct torture_context *tctx,
			      struct smbcli_state *cli)
{
	const char *fname = BASEDIR "\\lockt2.lck";
	int fnum1, fnum2, fnum3;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	torture_comment(tctx, "Testing pid context\n");
	
	cli->session->pid = 1;

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	torture_assert(tctx, fnum1 != -1, 
		talloc_asprintf(tctx, 
		"open of %s failed (%s)", fname, smbcli_errstr(cli->tree)));

	fnum2 = smbcli_open(cli->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum2 != -1,
		talloc_asprintf(tctx, "open2 of %s failed (%s)", 
		       fname, smbcli_errstr(cli->tree)));

	cli->session->pid = 2;

	fnum3 = smbcli_open(cli->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum3 != -1,
		talloc_asprintf(tctx, 
		"open3 of %s failed (%s)\n", fname, smbcli_errstr(cli->tree)));

	cli->session->pid = 1;

	torture_assert_ntstatus_ok(tctx, 
		smbcli_lock(cli->tree, fnum1, 0, 4, 0, WRITE_LOCK),
		talloc_asprintf(tctx, 
		"lock1 failed (%s)", smbcli_errstr(cli->tree)));

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli->tree, fnum1, 0, 4, 0, WRITE_LOCK)),
		"WRITE lock1 succeeded! This is a locking bug");
		
	if (!check_error(__location__, cli, ERRDOS, ERRlock, 
			 NT_STATUS_LOCK_NOT_GRANTED)) return false;

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli->tree, fnum2, 0, 4, 0, WRITE_LOCK)),
		"WRITE lock2 succeeded! This is a locking bug");

	if (!check_error(__location__, cli, ERRDOS, ERRlock, 
			 NT_STATUS_LOCK_NOT_GRANTED)) return false;

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli->tree, fnum2, 0, 4, 0, READ_LOCK)),
		"READ lock2 succeeded! This is a locking bug");

	if (!check_error(__location__, cli, ERRDOS, ERRlock, 
			 NT_STATUS_FILE_LOCK_CONFLICT)) return false;

	torture_assert_ntstatus_ok(tctx, 
		smbcli_lock(cli->tree, fnum1, 100, 4, 0, WRITE_LOCK),
		talloc_asprintf(tctx, 
		"lock at 100 failed (%s)", smbcli_errstr(cli->tree)));

	cli->session->pid = 2;

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_unlock(cli->tree, fnum1, 100, 4)),
		"unlock at 100 succeeded! This is a locking bug");

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_unlock(cli->tree, fnum1, 0, 4)),
		"unlock1 succeeded! This is a locking bug");

	if (!check_error(__location__, cli, 
				 ERRDOS, ERRnotlocked, 
				 NT_STATUS_RANGE_NOT_LOCKED)) return false;

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_unlock(cli->tree, fnum1, 0, 8)),
		"unlock2 succeeded! This is a locking bug");

	if (!check_error(__location__, cli, 
			 ERRDOS, ERRnotlocked, 
			 NT_STATUS_RANGE_NOT_LOCKED)) return false;

	torture_assert(tctx, 
		!NT_STATUS_IS_OK(smbcli_lock(cli->tree, fnum3, 0, 4, 0, WRITE_LOCK)),
		"lock3 succeeded! This is a locking bug");

	if (!check_error(__location__, cli, ERRDOS, ERRlock, NT_STATUS_LOCK_NOT_GRANTED)) return false;

	cli->session->pid = 1;

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli->tree, fnum1), 
		talloc_asprintf(tctx, "close1 failed (%s)", smbcli_errstr(cli->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli->tree, fnum2),
		talloc_asprintf(tctx, "close2 failed (%s)", smbcli_errstr(cli->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli->tree, fnum3),
		talloc_asprintf(tctx, "close3 failed (%s)", smbcli_errstr(cli->tree)));

	return true;
}


/*
  This test checks that 

  1) the server supports the full offset range in lock requests
*/
static bool torture_locktest3(struct torture_context *tctx, 
			      struct smbcli_state *cli1,
			      struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\lockt3.lck";
	int fnum1, fnum2, i;
	uint32_t offset;
	extern int torture_numops;

#define NEXT_OFFSET offset += (~(uint32_t)0) / torture_numops

	torture_comment(tctx, "Testing 32 bit offset ranges");

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	torture_assert(tctx, fnum1 != -1, 
		talloc_asprintf(tctx, "open of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree)));
	fnum2 = smbcli_open(cli2->tree, fname, O_RDWR, DENY_NONE);
	torture_assert(tctx, fnum2 != -1,
		talloc_asprintf(tctx, "open2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree)));

	torture_comment(tctx, "Establishing %d locks\n", torture_numops);

	for (offset=i=0;i<torture_numops;i++) {
		NEXT_OFFSET;
		torture_assert_ntstatus_ok(tctx, 
			smbcli_lock(cli1->tree, fnum1, offset-1, 1, 0, WRITE_LOCK),
			talloc_asprintf(tctx, "lock1 %d failed (%s)", i, smbcli_errstr(cli1->tree)));

		torture_assert_ntstatus_ok(tctx, 
			smbcli_lock(cli2->tree, fnum2, offset-2, 1, 0, WRITE_LOCK),
			talloc_asprintf(tctx, "lock2 %d failed (%s)", 
			       i, smbcli_errstr(cli1->tree)));
	}

	torture_comment(tctx, "Testing %d locks\n", torture_numops);

	for (offset=i=0;i<torture_numops;i++) {
		NEXT_OFFSET;

		torture_assert(tctx, 
			!NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, offset-2, 1, 0, WRITE_LOCK)),
			talloc_asprintf(tctx, "error: lock1 %d succeeded!", i));

		torture_assert(tctx, 
			!NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum2, offset-1, 1, 0, WRITE_LOCK)),
			talloc_asprintf(tctx, "error: lock2 %d succeeded!", i));

		torture_assert(tctx, 
			!NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, offset-1, 1, 0, WRITE_LOCK)),
			talloc_asprintf(tctx, "error: lock3 %d succeeded!", i));

		torture_assert(tctx, 
			!NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum2, offset-2, 1, 0, WRITE_LOCK)),
			talloc_asprintf(tctx, "error: lock4 %d succeeded!", i));
	}

	torture_comment(tctx, "Removing %d locks\n", torture_numops);

	for (offset=i=0;i<torture_numops;i++) {
		NEXT_OFFSET;

		torture_assert_ntstatus_ok(tctx, 
					smbcli_unlock(cli1->tree, fnum1, offset-1, 1),
					talloc_asprintf(tctx, "unlock1 %d failed (%s)", 
			       i,
			       smbcli_errstr(cli1->tree)));

		torture_assert_ntstatus_ok(tctx, 
			smbcli_unlock(cli2->tree, fnum2, offset-2, 1),
			talloc_asprintf(tctx, "unlock2 %d failed (%s)", 
			       i,
			       smbcli_errstr(cli1->tree)));
	}

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close1 failed (%s)", smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli2->tree, fnum2),
		talloc_asprintf(tctx, "close2 failed (%s)", smbcli_errstr(cli2->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_unlink(cli1->tree, fname),
		talloc_asprintf(tctx, "unlink failed (%s)", smbcli_errstr(cli1->tree)));

	return true;
}

#define EXPECTED(ret, v) if ((ret) != (v)) { \
        torture_comment(tctx, "** "); correct = false; \
        }

/*
  looks at overlapping locks
*/
static bool torture_locktest4(struct torture_context *tctx, 
			      struct smbcli_state *cli1,
			      struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\lockt4.lck";
	int fnum1, fnum2, f;
	bool ret;
	uint8_t buf[1000];
	bool correct = true;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	fnum2 = smbcli_open(cli2->tree, fname, O_RDWR, DENY_NONE);

	memset(buf, 0, sizeof(buf));

	if (smbcli_write(cli1->tree, fnum1, 0, buf, 0, sizeof(buf)) != sizeof(buf)) {
		torture_comment(tctx, "Failed to create file\n");
		correct = false;
		goto fail;
	}

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 0, 4, 0, WRITE_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 2, 4, 0, WRITE_LOCK));
	EXPECTED(ret, false);
	torture_comment(tctx, "the same process %s set overlapping write locks\n", ret?"can":"cannot");
	    
	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 10, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 12, 4, 0, READ_LOCK));
	EXPECTED(ret, true);
	torture_comment(tctx, "the same process %s set overlapping read locks\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 20, 4, 0, WRITE_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum2, 22, 4, 0, WRITE_LOCK));
	EXPECTED(ret, false);
	torture_comment(tctx, "a different connection %s set overlapping write locks\n", ret?"can":"cannot");
	    
	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 30, 4, 0, READ_LOCK)) &&
		NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum2, 32, 4, 0, READ_LOCK));
	EXPECTED(ret, true);
	torture_comment(tctx, "a different connection %s set overlapping read locks\n", ret?"can":"cannot");
	
	ret = NT_STATUS_IS_OK((cli1->session->pid = 1, smbcli_lock(cli1->tree, fnum1, 40, 4, 0, WRITE_LOCK))) &&
	      NT_STATUS_IS_OK((cli1->session->pid = 2, smbcli_lock(cli1->tree, fnum1, 42, 4, 0, WRITE_LOCK)));
	EXPECTED(ret, false);
	torture_comment(tctx, "a different pid %s set overlapping write locks\n", ret?"can":"cannot");
	    
	ret = NT_STATUS_IS_OK((cli1->session->pid = 1, smbcli_lock(cli1->tree, fnum1, 50, 4, 0, READ_LOCK))) &&
	      NT_STATUS_IS_OK((cli1->session->pid = 2, smbcli_lock(cli1->tree, fnum1, 52, 4, 0, READ_LOCK)));
	EXPECTED(ret, true);
	torture_comment(tctx, "a different pid %s set overlapping read locks\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 60, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 60, 4, 0, READ_LOCK));
	EXPECTED(ret, true);
	torture_comment(tctx, "the same process %s set the same read lock twice\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 70, 4, 0, WRITE_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 70, 4, 0, WRITE_LOCK));
	EXPECTED(ret, false);
	torture_comment(tctx, "the same process %s set the same write lock twice\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 80, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 80, 4, 0, WRITE_LOCK));
	EXPECTED(ret, false);
	torture_comment(tctx, "the same process %s overlay a read lock with a write lock\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 90, 4, 0, WRITE_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 90, 4, 0, READ_LOCK));
	EXPECTED(ret, true);
	torture_comment(tctx, "the same process %s overlay a write lock with a read lock\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK((cli1->session->pid = 1, smbcli_lock(cli1->tree, fnum1, 100, 4, 0, WRITE_LOCK))) &&
	      NT_STATUS_IS_OK((cli1->session->pid = 2, smbcli_lock(cli1->tree, fnum1, 100, 4, 0, READ_LOCK)));
	EXPECTED(ret, false);
	torture_comment(tctx, "a different pid %s overlay a write lock with a read lock\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 110, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 112, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 110, 6));
	EXPECTED(ret, false);
	torture_comment(tctx, "the same process %s coalesce read locks\n", ret?"can":"cannot");


	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 120, 4, 0, WRITE_LOCK)) &&
	      (smbcli_read(cli2->tree, fnum2, buf, 120, 4) == 4);
	EXPECTED(ret, false);
	torture_comment(tctx, "this server %s strict write locking\n", ret?"doesn't do":"does");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 130, 4, 0, READ_LOCK)) &&
	      (smbcli_write(cli2->tree, fnum2, 0, buf, 130, 4) == 4);
	EXPECTED(ret, false);
	torture_comment(tctx, "this server %s strict read locking\n", ret?"doesn't do":"does");


	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 140, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 140, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 140, 4)) &&
	      NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 140, 4));
	EXPECTED(ret, true);
	torture_comment(tctx, "this server %s do recursive read locking\n", ret?"does":"doesn't");


	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 150, 4, 0, WRITE_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 150, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 150, 4)) &&
	      (smbcli_read(cli2->tree, fnum2, buf, 150, 4) == 4) &&
	      !(smbcli_write(cli2->tree, fnum2, 0, buf, 150, 4) == 4) &&
	      NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 150, 4));
	EXPECTED(ret, true);
	torture_comment(tctx, "this server %s do recursive lock overlays\n", ret?"does":"doesn't");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 160, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 160, 4)) &&
	      (smbcli_write(cli2->tree, fnum2, 0, buf, 160, 4) == 4) &&		
	      (smbcli_read(cli2->tree, fnum2, buf, 160, 4) == 4);		
	EXPECTED(ret, true);
	torture_comment(tctx, "the same process %s remove a read lock using write locking\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 170, 4, 0, WRITE_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 170, 4)) &&
	      (smbcli_write(cli2->tree, fnum2, 0, buf, 170, 4) == 4) &&		
	      (smbcli_read(cli2->tree, fnum2, buf, 170, 4) == 4);		
	EXPECTED(ret, true);
	torture_comment(tctx, "the same process %s remove a write lock using read locking\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 190, 4, 0, WRITE_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 190, 4, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 190, 4)) &&
	      !(smbcli_write(cli2->tree, fnum2, 0, buf, 190, 4) == 4) &&		
	      (smbcli_read(cli2->tree, fnum2, buf, 190, 4) == 4);		
	EXPECTED(ret, true);
	torture_comment(tctx, "the same process %s remove the first lock first\n", ret?"does":"doesn't");

	smbcli_close(cli1->tree, fnum1);
	smbcli_close(cli2->tree, fnum2);
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	f = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 0, 8, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, f, 0, 1, 0, READ_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_close(cli1->tree, fnum1)) &&
	      ((fnum1 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE)) != -1) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 7, 1, 0, WRITE_LOCK));
        smbcli_close(cli1->tree, f);
	smbcli_close(cli1->tree, fnum1);
	EXPECTED(ret, true);
	torture_comment(tctx, "the server %s have the NT byte range lock bug\n", !ret?"does":"doesn't");

 fail:
	smbcli_close(cli1->tree, fnum1);
	smbcli_close(cli2->tree, fnum2);
	smbcli_unlink(cli1->tree, fname);

	return correct;
}

/*
  looks at lock upgrade/downgrade.
*/
static bool torture_locktest5(struct torture_context *tctx, struct smbcli_state *cli1, 
			      struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\lockt5.lck";
	int fnum1, fnum2, fnum3;
	bool ret;
	uint8_t buf[1000];
	bool correct = true;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	fnum2 = smbcli_open(cli2->tree, fname, O_RDWR, DENY_NONE);
	fnum3 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);

	memset(buf, 0, sizeof(buf));

	torture_assert(tctx, smbcli_write(cli1->tree, fnum1, 0, buf, 0, sizeof(buf)) == sizeof(buf),
		"Failed to create file");

	/* Check for NT bug... */
	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 0, 8, 0, READ_LOCK)) &&
		  NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum3, 0, 1, 0, READ_LOCK));
	smbcli_close(cli1->tree, fnum1);
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 7, 1, 0, WRITE_LOCK));
	EXPECTED(ret, true);
	torture_comment(tctx, "this server %s the NT locking bug\n", ret ? "doesn't have" : "has");
	smbcli_close(cli1->tree, fnum1);
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_NONE);
	smbcli_unlock(cli1->tree, fnum3, 0, 1);

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 0, 4, 0, WRITE_LOCK)) &&
	      NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 1, 1, 0, READ_LOCK));
	EXPECTED(ret, true);
	torture_comment(tctx, "the same process %s overlay a write with a read lock\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum2, 0, 4, 0, READ_LOCK));
	EXPECTED(ret, false);

	torture_comment(tctx, "a different processs %s get a read lock on the first process lock stack\n", ret?"can":"cannot");

	/* Unlock the process 2 lock. */
	smbcli_unlock(cli2->tree, fnum2, 0, 4);

	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum3, 0, 4, 0, READ_LOCK));
	EXPECTED(ret, false);

	torture_comment(tctx, "the same processs on a different fnum %s get a read lock\n", ret?"can":"cannot");

	/* Unlock the process 1 fnum3 lock. */
	smbcli_unlock(cli1->tree, fnum3, 0, 4);

	/* Stack 2 more locks here. */
	ret = NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 0, 4, 0, READ_LOCK)) &&
		  NT_STATUS_IS_OK(smbcli_lock(cli1->tree, fnum1, 0, 4, 0, READ_LOCK));

	EXPECTED(ret, true);
	torture_comment(tctx, "the same process %s stack read locks\n", ret?"can":"cannot");

	/* Unlock the first process lock, then check this was the WRITE lock that was
		removed. */

ret = NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 0, 4)) &&
	NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum2, 0, 4, 0, READ_LOCK));

	EXPECTED(ret, true);
	torture_comment(tctx, "the first unlock removes the %s lock\n", ret?"WRITE":"READ");

	/* Unlock the process 2 lock. */
	smbcli_unlock(cli2->tree, fnum2, 0, 4);

	/* We should have 3 stacked locks here. Ensure we need to do 3 unlocks. */

	ret = NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 1, 1)) &&
		  NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 0, 4)) &&
		  NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 0, 4));

	EXPECTED(ret, true);
	torture_comment(tctx, "the same process %s unlock the stack of 3 locks\n", ret?"can":"cannot");

	/* Ensure the next unlock fails. */
	ret = NT_STATUS_IS_OK(smbcli_unlock(cli1->tree, fnum1, 0, 4));
	EXPECTED(ret, false);
	torture_comment(tctx, "the same process %s count the lock stack\n", !ret?"can":"cannot"); 

	/* Ensure connection 2 can get a write lock. */
	ret = NT_STATUS_IS_OK(smbcli_lock(cli2->tree, fnum2, 0, 4, 0, WRITE_LOCK));
	EXPECTED(ret, true);

	torture_comment(tctx, "a different processs %s get a write lock on the unlocked stack\n", ret?"can":"cannot");


	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum1),
		talloc_asprintf(tctx, "close1 failed (%s)", smbcli_errstr(cli1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli2->tree, fnum2),
		talloc_asprintf(tctx, "close2 failed (%s)", smbcli_errstr(cli2->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli1->tree, fnum3),
		talloc_asprintf(tctx, "close2 failed (%s)", smbcli_errstr(cli2->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_unlink(cli1->tree, fname),
		talloc_asprintf(tctx, "unlink failed (%s)", smbcli_errstr(cli1->tree)));

	return correct;
}

/*
  tries the unusual lockingX locktype bits
*/
static bool torture_locktest6(struct torture_context *tctx, 
			      struct smbcli_state *cli)
{
	const char *fname[1] = { "\\lock6.txt" };
	int i;
	int fnum;
	NTSTATUS status;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	for (i=0;i<1;i++) {
		torture_comment(tctx, "Testing %s\n", fname[i]);

		smbcli_unlink(cli->tree, fname[i]);

		fnum = smbcli_open(cli->tree, fname[i], O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
		status = smbcli_locktype(cli->tree, fnum, 0, 8, 0, LOCKING_ANDX_CHANGE_LOCKTYPE);
		smbcli_close(cli->tree, fnum);
		torture_comment(tctx, "CHANGE_LOCKTYPE gave %s\n", nt_errstr(status));

		fnum = smbcli_open(cli->tree, fname[i], O_RDWR, DENY_NONE);
		status = smbcli_locktype(cli->tree, fnum, 0, 8, 0, LOCKING_ANDX_CANCEL_LOCK);
		smbcli_close(cli->tree, fnum);
		torture_comment(tctx, "CANCEL_LOCK gave %s\n", nt_errstr(status));

		smbcli_unlink(cli->tree, fname[i]);
	}

	return true;
}

static bool torture_locktest7(struct torture_context *tctx, 
			      struct smbcli_state *cli1)
{
	const char *fname = BASEDIR "\\lockt7.lck";
	int fnum1;
	int fnum2 = -1;
	size_t size;
	uint8_t buf[200];
	bool correct = false;

	torture_assert(tctx, torture_setup_dir(cli1, BASEDIR),
				   talloc_asprintf(tctx, "Unable to set up %s", BASEDIR));

	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);

	memset(buf, 0, sizeof(buf));

	torture_assert(tctx, smbcli_write(cli1->tree, fnum1, 0, buf, 0, sizeof(buf)) == sizeof(buf),
		"Failed to create file");

	cli1->session->pid = 1;

	torture_assert_ntstatus_ok(tctx, smbcli_lock(cli1->tree, fnum1, 130, 4, 0, READ_LOCK),
		talloc_asprintf(tctx, "Unable to apply read lock on range 130:4, error was %s", 
		       smbcli_errstr(cli1->tree)));

	torture_comment(tctx, "pid1 successfully locked range 130:4 for READ\n");

	torture_assert(tctx, smbcli_read(cli1->tree, fnum1, buf, 130, 4) == 4, 
		 	talloc_asprintf(tctx, "pid1 unable to read the range 130:4, error was %s)", 
		       smbcli_errstr(cli1->tree)));

	torture_comment(tctx, "pid1 successfully read the range 130:4\n");

	if (smbcli_write(cli1->tree, fnum1, 0, buf, 130, 4) != 4) {
		torture_comment(tctx, "pid1 unable to write to the range 130:4, error was %s\n", smbcli_errstr(cli1->tree));
		torture_assert_ntstatus_equal(tctx, smbcli_nt_error(cli1->tree), NT_STATUS_FILE_LOCK_CONFLICT,
			"Incorrect error (should be NT_STATUS_FILE_LOCK_CONFLICT)");
	} else {
		torture_fail(tctx, "pid1 successfully wrote to the range 130:4 (should be denied)");
	}

	cli1->session->pid = 2;

	if (smbcli_read(cli1->tree, fnum1, buf, 130, 4) != 4) {
		torture_comment(tctx, "pid2 unable to read the range 130:4, error was %s\n", smbcli_errstr(cli1->tree));
	} else {
		torture_comment(tctx, "pid2 successfully read the range 130:4\n");
	}

	if (smbcli_write(cli1->tree, fnum1, 0, buf, 130, 4) != 4) {
		torture_comment(tctx, "pid2 unable to write to the range 130:4, error was %s\n", smbcli_errstr(cli1->tree));
		torture_assert_ntstatus_equal(tctx, smbcli_nt_error(cli1->tree), NT_STATUS_FILE_LOCK_CONFLICT, 
			"Incorrect error (should be NT_STATUS_FILE_LOCK_CONFLICT)");
	} else {
		torture_fail(tctx, "pid2 successfully wrote to the range 130:4 (should be denied)"); 
	}

	cli1->session->pid = 1;
	smbcli_unlock(cli1->tree, fnum1, 130, 4);

	torture_assert_ntstatus_ok(tctx, smbcli_lock(cli1->tree, fnum1, 130, 4, 0, WRITE_LOCK),
		talloc_asprintf(tctx, "Unable to apply write lock on range 130:4, error was %s", 
		       smbcli_errstr(cli1->tree)));
	torture_comment(tctx, "pid1 successfully locked range 130:4 for WRITE\n");

	torture_assert(tctx, smbcli_read(cli1->tree, fnum1, buf, 130, 4) == 4, 
		talloc_asprintf(tctx, "pid1 unable to read the range 130:4, error was %s", 
		       smbcli_errstr(cli1->tree)));
	torture_comment(tctx, "pid1 successfully read the range 130:4\n");

	torture_assert(tctx, smbcli_write(cli1->tree, fnum1, 0, buf, 130, 4) == 4, 
		talloc_asprintf(tctx, "pid1 unable to write to the range 130:4, error was %s",
		       smbcli_errstr(cli1->tree)));
	torture_comment(tctx, "pid1 successfully wrote to the range 130:4\n");

	cli1->session->pid = 2;

	if (smbcli_read(cli1->tree, fnum1, buf, 130, 4) != 4) {
		torture_comment(tctx, "pid2 unable to read the range 130:4, error was %s\n", 
		       smbcli_errstr(cli1->tree));
		torture_assert_ntstatus_equal(tctx, smbcli_nt_error(cli1->tree), NT_STATUS_FILE_LOCK_CONFLICT, 
			"Incorrect error (should be NT_STATUS_FILE_LOCK_CONFLICT)");
	} else {
		torture_fail(tctx, "pid2 successfully read the range 130:4 (should be denied)");
	}

	if (smbcli_write(cli1->tree, fnum1, 0, buf, 130, 4) != 4) {
		torture_comment(tctx, "pid2 unable to write to the range 130:4, error was %s\n", 
		       smbcli_errstr(cli1->tree));
		if (!NT_STATUS_EQUAL(smbcli_nt_error(cli1->tree), NT_STATUS_FILE_LOCK_CONFLICT)) {
			torture_comment(tctx, "Incorrect error (should be NT_STATUS_FILE_LOCK_CONFLICT) (%s)\n",
			       __location__);
			goto fail;
		}
	} else {
		torture_comment(tctx, "pid2 successfully wrote to the range 130:4 (should be denied) (%s)\n", 
		       __location__);
		goto fail;
	}

	torture_comment(tctx, "Testing truncate of locked file.\n");

	fnum2 = smbcli_open(cli1->tree, fname, O_RDWR|O_TRUNC, DENY_NONE);

	torture_assert(tctx, fnum2 != -1, "Unable to truncate locked file");

	torture_comment(tctx, "Truncated locked file.\n");

	torture_assert_ntstatus_ok(tctx, smbcli_getatr(cli1->tree, fname, NULL, &size, NULL), 
		talloc_asprintf(tctx, "getatr failed (%s)", smbcli_errstr(cli1->tree)));

	torture_assert(tctx, size == 0, talloc_asprintf(tctx, "Unable to truncate locked file. Size was %u", (unsigned)size));

	cli1->session->pid = 1;

	smbcli_unlock(cli1->tree, fnum1, 130, 4);
	correct = true;

fail:
	smbcli_close(cli1->tree, fnum1);
	smbcli_close(cli1->tree, fnum2);
	smbcli_unlink(cli1->tree, fname);

	return correct;
}

struct torture_suite *torture_base_locktest(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "lock");
	torture_suite_add_2smb_test(suite, "LOCK1", torture_locktest1);
	torture_suite_add_1smb_test(suite, "LOCK2", torture_locktest2);
	torture_suite_add_2smb_test(suite, "LOCK3", torture_locktest3);
	torture_suite_add_2smb_test(suite, "LOCK4",  torture_locktest4);
	torture_suite_add_2smb_test(suite, "LOCK5",  torture_locktest5);
	torture_suite_add_1smb_test(suite, "LOCK6",  torture_locktest6);
	torture_suite_add_1smb_test(suite, "LOCK7",  torture_locktest7);

	return suite;
}
