/*
   Unix SMB/CIFS implementation.

   SMB2 lock test suite

   Copyright (C) Stefan Metzmacher 2006

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
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"

#include "torture/torture.h"
#include "torture/smb2/proto.h"
#include "torture/util.h"

#include "lib/events/events.h"
#include "param/param.h"

#define CHECK_STATUS(status, correct) do { \
	const char *_cmt = "(" __location__ ")"; \
	torture_assert_ntstatus_equal_goto(torture,status,correct, \
					   ret,done,_cmt); \
	} while (0)

#define CHECK_STATUS_CMT(status, correct, cmt) do { \
	torture_assert_ntstatus_equal_goto(torture,status,correct, \
					   ret,done,cmt); \
	} while (0)

#define CHECK_STATUS_CONT(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(torture, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s\n", \
			__location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
	}} while (0)

#define CHECK_VALUE(v, correct) do { \
	const char *_cmt = "(" __location__ ")"; \
	torture_assert_int_equal_goto(torture,v,correct,ret,done,_cmt); \
	} while (0)

#define BASEDIR "testlock"

#define TARGET_SUPPORTS_INVALID_LOCK_RANGE(_tctx) \
    (torture_setting_bool(_tctx, "invalid_lock_range_support", true))
#define TARGET_IS_W2K8(_tctx) (torture_setting_bool(_tctx, "w2k8", false))

#define WAIT_FOR_ASYNC_RESPONSE(req) \
	while (!req->cancel.can_cancel && req->state <= SMB2_REQUEST_RECV) { \
		if (event_loop_once(req->transport->socket->event.ctx) != 0) { \
			break; \
		} \
	}

static bool test_valid_request(struct torture_context *torture,
			       struct smb2_tree *tree)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle h;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[2];

	ZERO_STRUCT(buf);

	status = torture_smb2_testfile(tree, "lock1.txt", &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.locks		= el;

	torture_comment(torture, "Test request with 0 locks.\n");

	lck.in.lock_count	= 0x0000;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0x0000000000000000;
	el[0].length		= 0x0000000000000000;
	el[0].reserved		= 0x0000000000000000;
	el[0].flags		= 0x00000000;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	lck.in.lock_count	= 0x0000;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 0;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_SHARED;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 0;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_NONE;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_W2K8(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
		torture_warning(torture, "Target has bug validating lock flags "
					 "parameter.\n");
	} else {
		CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	}

	torture_comment(torture, "Test >63-bit lock requests.\n");

	lck.in.file.handle.data[0] +=1;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	lck.in.file.handle.data[0] -=1;

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x123ab1;
	lck.in.file.handle	= h;
	el[0].offset		= UINT64_MAX;
	el[0].length		= UINT64_MAX;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	if (TARGET_SUPPORTS_INVALID_LOCK_RANGE(torture)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_LOCK_RANGE);
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_VALUE(lck.out.reserved, 0);
	}

	lck.in.lock_sequence	= 0x123ab2;
	status = smb2_lock(tree, &lck);
	if (TARGET_SUPPORTS_INVALID_LOCK_RANGE(torture)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_LOCK_RANGE);
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}

	torture_comment(torture, "Test basic lock stacking.\n");

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x12345678;
	lck.in.file.handle	= h;
	el[0].offset		= UINT32_MAX;
	el[0].length		= UINT32_MAX;
	el[0].reserved		= 0x87654321;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	el[0].flags		= SMB2_LOCK_FLAG_SHARED;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x87654321;
	lck.in.file.handle	= h;
	el[0].offset		= 0x00000000FFFFFFFF;
	el[0].length		= 0x00000000FFFFFFFF;
	el[0].reserved		= 0x1234567;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x1234567;
	lck.in.file.handle	= h;
	el[0].offset		= 0x00000000FFFFFFFF;
	el[0].length		= 0x00000000FFFFFFFF;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "Test flags field permutations.\n");

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0;
	lck.in.file.handle	= h;
	el[0].offset		= 1;
	el[0].length		= 1;
	el[0].reserved		= 0x00000000;
	el[0].flags		= ~SMB2_LOCK_FLAG_ALL_MASK;

	status = smb2_lock(tree, &lck);
	if (TARGET_IS_W2K8(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
		torture_warning(torture, "Target has bug validating lock flags "
					 "parameter.\n");
	} else {
		CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	}

	if (TARGET_IS_W2K8(torture)) {
		el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK |
				  SMB2_LOCK_FLAG_EXCLUSIVE;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK |
				  SMB2_LOCK_FLAG_SHARED;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_W2K8(torture)) {
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
		torture_warning(torture, "Target has bug validating lock flags "
					 "parameter.\n");
	} else {
		CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	}

	torture_comment(torture, "Test return error when 2 locks are "
				 "requested\n");

	lck.in.lock_count	= 2;
	lck.in.lock_sequence	= 0;
	lck.in.file.handle	= h;
	el[0].offset		= 9999;
	el[0].length		= 1;
	el[0].reserved		= 0x00000000;
	el[1].offset		= 9999;
	el[1].length		= 1;
	el[1].reserved		= 0x00000000;

	lck.in.lock_count	= 2;
	el[0].flags		= 0;
	el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	lck.in.lock_count	= 2;
	el[0].flags		= 0;
	el[1].flags		= 0;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_W2K8(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
		torture_warning(torture, "Target has bug validating lock flags "
					 "parameter.\n");
	} else {
		CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	}

	lck.in.lock_count	= 2;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	el[1].flags		= 0;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_W2K8(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
		torture_warning(torture, "Target has bug validating lock flags "
					 "parameter.\n");
	} else {
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	}

	lck.in.lock_count	= 1;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	lck.in.lock_count	= 1;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	lck.in.lock_count	= 1;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	lck.in.lock_count	= 1;
	el[0].flags		= SMB2_LOCK_FLAG_SHARED;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.lock_count	= 2;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.lock_count	= 1;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

done:
	return ret;
}

struct test_lock_read_write_state {
	const char *fname;
	uint32_t lock_flags;
	NTSTATUS write_h1_status;
	NTSTATUS read_h1_status;
	NTSTATUS write_h2_status;
	NTSTATUS read_h2_status;
};

static bool test_lock_read_write(struct torture_context *torture,
				 struct smb2_tree *tree,
				 struct test_lock_read_write_state *s)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle h1, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_create cr;
	struct smb2_write wr;
	struct smb2_read rd;
	struct smb2_lock_element el[1];

	lck.in.locks		= el;

	ZERO_STRUCT(buf);

	status = torture_smb2_testfile(tree, s->fname, &h1);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_write(tree, h1, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h1;
	el[0].offset		= 0;
	el[0].length		= ARRAY_SIZE(buf)/2;
	el[0].reserved		= 0x00000000;
	el[0].flags		= s->lock_flags;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h1;
	el[0].offset		= ARRAY_SIZE(buf)/2;
	el[0].length		= ARRAY_SIZE(buf)/2;
	el[0].reserved		= 0x00000000;
	el[0].flags		= s->lock_flags;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	ZERO_STRUCT(cr);
	cr.in.oplock_level = 0;
	cr.in.desired_access = SEC_RIGHTS_FILE_ALL;
	cr.in.file_attributes   = FILE_ATTRIBUTE_NORMAL;
	cr.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	cr.in.share_access =
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	cr.in.create_options = 0;
	cr.in.fname = s->fname;

	status = smb2_create(tree, tree, &cr);
	CHECK_STATUS(status, NT_STATUS_OK);

	h2 = cr.out.file.handle;

	ZERO_STRUCT(wr);
	wr.in.file.handle = h1;
	wr.in.offset      = ARRAY_SIZE(buf)/2;
	wr.in.data        = data_blob_const(buf, ARRAY_SIZE(buf)/2);

	status = smb2_write(tree, &wr);
	CHECK_STATUS(status, s->write_h1_status);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h1;
	rd.in.offset      = ARRAY_SIZE(buf)/2;
	rd.in.length      = ARRAY_SIZE(buf)/2;

	status = smb2_read(tree, tree, &rd);
	CHECK_STATUS(status, s->read_h1_status);

	ZERO_STRUCT(wr);
	wr.in.file.handle = h2;
	wr.in.offset      = ARRAY_SIZE(buf)/2;
	wr.in.data        = data_blob_const(buf, ARRAY_SIZE(buf)/2);

	status = smb2_write(tree, &wr);
	CHECK_STATUS(status, s->write_h2_status);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h2;
	rd.in.offset      = ARRAY_SIZE(buf)/2;
	rd.in.length      = ARRAY_SIZE(buf)/2;

	status = smb2_read(tree, tree, &rd);
	CHECK_STATUS(status, s->read_h2_status);

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h1;
	el[0].offset		= ARRAY_SIZE(buf)/2;
	el[0].length		= ARRAY_SIZE(buf)/2;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	ZERO_STRUCT(wr);
	wr.in.file.handle = h2;
	wr.in.offset      = ARRAY_SIZE(buf)/2;
	wr.in.data        = data_blob_const(buf, ARRAY_SIZE(buf)/2);

	status = smb2_write(tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h2;
	rd.in.offset      = ARRAY_SIZE(buf)/2;
	rd.in.length      = ARRAY_SIZE(buf)/2;

	status = smb2_read(tree, tree, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	return ret;
}

static bool test_lock_rw_none(struct torture_context *torture,
			      struct smb2_tree *tree)
{
	struct test_lock_read_write_state s = {
		.fname			= "lock_rw_none.dat",
		.lock_flags		= SMB2_LOCK_FLAG_NONE,
		.write_h1_status	= NT_STATUS_FILE_LOCK_CONFLICT,
		.read_h1_status		= NT_STATUS_OK,
		.write_h2_status	= NT_STATUS_FILE_LOCK_CONFLICT,
		.read_h2_status		= NT_STATUS_OK,
	};

	if (!TARGET_IS_W2K8(torture)) {
		torture_skip(torture, "RW-NONE tests the behavior of a "
			     "NONE-type lock, which is the same as a SHARED "
			     "lock but is granted due to a bug in W2K8.  If "
			     "target is not W2K8 we skip this test.\n");
	}

	return test_lock_read_write(torture, tree, &s);
}

static bool test_lock_rw_shared(struct torture_context *torture,
				struct smb2_tree *tree)
{
	struct test_lock_read_write_state s = {
		.fname			= "lock_rw_shared.dat",
		.lock_flags		= SMB2_LOCK_FLAG_SHARED,
		.write_h1_status	= NT_STATUS_FILE_LOCK_CONFLICT,
		.read_h1_status		= NT_STATUS_OK,
		.write_h2_status	= NT_STATUS_FILE_LOCK_CONFLICT,
		.read_h2_status		= NT_STATUS_OK,
	};

	return test_lock_read_write(torture, tree, &s);
}

static bool test_lock_rw_exclusive(struct torture_context *torture,
				   struct smb2_tree *tree)
{
	struct test_lock_read_write_state s = {
		.fname			= "lock_rw_exclusive.dat",
		.lock_flags		= SMB2_LOCK_FLAG_EXCLUSIVE,
		.write_h1_status	= NT_STATUS_OK,
		.read_h1_status		= NT_STATUS_OK,
		.write_h2_status	= NT_STATUS_FILE_LOCK_CONFLICT,
		.read_h2_status		= NT_STATUS_FILE_LOCK_CONFLICT,
	};

	return test_lock_read_write(torture, tree, &s);
}

static bool test_lock_auto_unlock(struct torture_context *torture,
				  struct smb2_tree *tree)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle h;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[1];

	ZERO_STRUCT(buf);

	status = torture_smb2_testfile(tree, "autounlock.txt", &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(lck);
	ZERO_STRUCT(el[0]);
	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 1;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	status = smb2_lock(tree, &lck);
	if (TARGET_IS_W2K8(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
		torture_warning(torture, "Target has \"pretty please\" bug. "
				"A contending lock request on the same handle "
				"unlocks the lock.\n");
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

done:
	return ret;
}

/*
  test different lock ranges and see if different handles conflict
*/
static bool test_lock(struct torture_context *torture,
		      struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[2];

	const char *fname = BASEDIR "\\async.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.locks		= el;

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;

	torture_comment(torture, "Trying 0/0 lock\n");
	el[0].offset		= 0x0000000000000000;
	el[0].length		= 0x0000000000000000;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle	= h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "Trying 0/1 lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[0].offset		= 0x0000000000000000;
	el[0].length		= 0x0000000000000001;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle      = h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	lck.in.file.handle      = h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "Trying 0xEEFFFFF lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[0].offset		= 0xEEFFFFFF;
	el[0].length		= 4000;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle      = h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	lck.in.file.handle      = h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "Trying 0xEF00000 lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[0].offset		= 0xEF000000;
	el[0].length		= 4000;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle      = h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	lck.in.file.handle      = h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "Trying (2^63 - 1)/1\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[0].offset		= 1;
	el[0].offset	      <<= 63;
	el[0].offset--;
	el[0].length		= 1;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle      = h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	lck.in.file.handle      = h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "Trying 2^63/1\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[0].offset		= 1;
	el[0].offset	      <<= 63;
	el[0].length		= 1;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle      = h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	lck.in.file.handle      = h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "Trying max/0 lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[0].offset		= ~0;
	el[0].length		= 0;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle      = h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle      = h;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "Trying max/1 lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[0].offset		= ~0;
	el[0].length		= 1;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle      = h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	lck.in.file.handle      = h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "Trying max/2 lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[0].offset		= ~0;
	el[0].length		= 2;
	status = smb2_lock(tree, &lck);
	if (TARGET_SUPPORTS_INVALID_LOCK_RANGE(torture)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_LOCK_RANGE);
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
		el[0].flags	= SMB2_LOCK_FLAG_UNLOCK;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	torture_comment(torture, "Trying wrong handle unlock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[0].offset		= 10001;
	el[0].length		= 40002;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	lck.in.file.handle	= h2;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	lck.in.file.handle	= h;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/*
  test SMB2 LOCK async operation
*/
static bool test_async(struct torture_context *torture,
		       struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[2];
	struct smb2_request *req = NULL;

	const char *fname = BASEDIR "\\async.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.locks		= el;

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 100;
	el[0].length		= 50;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;

	torture_comment(torture, "  Acquire first lock\n");
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Second lock should pend on first\n");
	lck.in.file.handle	= h2;
	req = smb2_lock_send(tree, &lck);
	WAIT_FOR_ASYNC_RESPONSE(req);

	torture_comment(torture, "  Unlock first lock\n");
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Second lock should now succeed\n");
	lck.in.file.handle	= h2;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;
	status = smb2_lock_recv(req, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/*
  test SMB2 LOCK Cancel operation
*/
static bool test_cancel(struct torture_context *torture,
			struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[2];
	struct smb2_request *req = NULL;

	const char *fname = BASEDIR "\\cancel.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.locks		= el;

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 100;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;

	torture_comment(torture, "Testing basic cancel\n");

	torture_comment(torture, "  Acquire first lock\n");
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Second lock should pend on first\n");
	lck.in.file.handle	= h2;
	req = smb2_lock_send(tree, &lck);
	WAIT_FOR_ASYNC_RESPONSE(req);

	torture_comment(torture, "  Cancel the second lock\n");
	smb2_cancel(req);
	lck.in.file.handle	= h2;
	status = smb2_lock_recv(req, &lck);
	CHECK_STATUS(status, NT_STATUS_CANCELLED);

	torture_comment(torture, "  Unlock first lock\n");
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);


	torture_comment(torture, "Testing cancel by unlock\n");

	torture_comment(torture, "  Acquire first lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Second lock should pend on first\n");
	lck.in.file.handle	= h2;
	req = smb2_lock_send(tree, &lck);
	WAIT_FOR_ASYNC_RESPONSE(req);

	torture_comment(torture, "  Attempt to unlock pending second lock\n");
	lck.in.file.handle	= h2;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "  Now cancel the second lock\n");
	smb2_cancel(req);
	lck.in.file.handle	= h2;
	status = smb2_lock_recv(req, &lck);
	CHECK_STATUS(status, NT_STATUS_CANCELLED);

	torture_comment(torture, "  Unlock first lock\n");
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);


	torture_comment(torture, "Testing cancel by close\n");

	torture_comment(torture, "  Acquire first lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Second lock should pend on first\n");
	lck.in.file.handle	= h2;
	req = smb2_lock_send(tree, &lck);
	WAIT_FOR_ASYNC_RESPONSE(req);

	torture_comment(torture, "  Close the second lock handle\n");
	smb2_util_close(tree, h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Check pending lock reply\n");
	status = smb2_lock_recv(req, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	torture_comment(torture, "  Unlock first lock\n");
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/*
  test SMB2 LOCK Cancel by tree disconnect
*/
static bool test_cancel_tdis(struct torture_context *torture,
			     struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[2];
	struct smb2_request *req = NULL;

	const char *fname = BASEDIR "\\cancel_tdis.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.locks		= el;

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 100;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;

	torture_comment(torture, "Testing cancel by tree disconnect\n");

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Acquire first lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Second lock should pend on first\n");
	lck.in.file.handle	= h2;
	req = smb2_lock_send(tree, &lck);
	WAIT_FOR_ASYNC_RESPONSE(req);

	torture_comment(torture, "  Disconnect the tree\n");
	smb2_tdis(tree);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Check pending lock reply\n");
	status = smb2_lock_recv(req, &lck);
	if (torture_setting_bool(torture, "samba4", false)) {
		/* saying that this lock succeeded is nonsense - the
		 * tree is gone!! */
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	torture_comment(torture, "  Attempt to unlock first lock\n");
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	if (torture_setting_bool(torture, "samba4", false)) {
		/* checking if the tcon supplied are still valid
		 * should happen before you validate a file handle,
		 * so we should return USER_SESSION_DELETED */
		CHECK_STATUS(status, NT_STATUS_NETWORK_NAME_DELETED);
	} else {
		CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	}

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/*
  test SMB2 LOCK Cancel by user logoff
*/
static bool test_cancel_logoff(struct torture_context *torture,
			       struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[2];
	struct smb2_request *req = NULL;

	const char *fname = BASEDIR "\\cancel_tdis.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.locks		= el;

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 100;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;

	torture_comment(torture, "Testing cancel by ulogoff\n");

	torture_comment(torture, "  Acquire first lock\n");
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  Second lock should pend on first\n");
	lck.in.file.handle	= h2;
	req = smb2_lock_send(tree, &lck);
	WAIT_FOR_ASYNC_RESPONSE(req);

	torture_comment(torture, "  Logoff user\n");
	smb2_logoff(tree->session);

	torture_comment(torture, "  Check pending lock reply\n");
	status = smb2_lock_recv(req, &lck);
	if (torture_setting_bool(torture, "samba4", false)) {
		/* another bogus 'success' code from windows. The lock
		 * cannot have succeeded, as we are now logged off */
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	torture_comment(torture, "  Attempt to unlock first lock\n");
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	if (torture_setting_bool(torture, "samba4", false)) {
		/* checking if the credential supplied are still valid
		 * should happen before you validate a file handle,
		 * so we should return USER_SESSION_DELETED */
		CHECK_STATUS(status, NT_STATUS_USER_SESSION_DELETED);
	} else {
		CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	}

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/*
 * Test NT_STATUS_LOCK_NOT_GRANTED vs. NT_STATUS_FILE_LOCK_CONFLICT
 *
 * The SMBv1 protocol returns a different error code on lock acquisition
 * failure depending on a number of parameters, including what error code
 * was returned to the previous failure.
 *
 * SMBv2 has cleaned up these semantics and should always return
 * NT_STATUS_LOCK_NOT_GRANTED to failed lock requests, and
 * NT_STATUS_FILE_LOCK_CONFLICT to failed read/write requests due to a lock
 * being held on that range.
*/
static bool test_errorcode(struct torture_context *torture,
			   struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[2];

	const char *fname = BASEDIR "\\errorcode.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.locks		= el;

	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 100;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;

	torture_comment(torture, "Testing LOCK_NOT_GRANTED vs. "
				 "FILE_LOCK_CONFLICT\n");

	if (TARGET_IS_W2K8(torture)) {
		torture_result(torture, TORTURE_SKIP,
		    "Target has \"pretty please\" bug. A contending lock "
		    "request on the same handle unlocks the lock.");
		goto done;
	}

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Demonstrate that the first conflicting lock on each handle gives
	 * LOCK_NOT_GRANTED. */
	lck.in.file.handle	= h;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	lck.in.file.handle	= h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* Demonstrate that each following conflict also gives
	 * LOCK_NOT_GRANTED */
	lck.in.file.handle	= h;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	lck.in.file.handle	= h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	lck.in.file.handle	= h;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	lck.in.file.handle	= h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* Demonstrate that the smbpid doesn't matter */
	tree->session->pid++;
	lck.in.file.handle	= h;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	lck.in.file.handle	= h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	tree->session->pid--;

	/* Demonstrate that a 0-byte lock inside the locked range still
	 * gives the same error. */

	el[0].offset		= 102;
	el[0].length		= 0;
	lck.in.file.handle	= h;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	lck.in.file.handle	= h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* Demonstrate that a lock inside the locked range still gives the
	 * same error. */

	el[0].offset		= 102;
	el[0].length		= 5;
	lck.in.file.handle	= h;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	lck.in.file.handle	= h2;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/**
 * Tests zero byte locks.
 */

struct double_lock_test {
	struct smb2_lock_element lock1;
	struct smb2_lock_element lock2;
	NTSTATUS status;
};

static struct double_lock_test zero_byte_tests[] = {
	/* {offset, count, reserved, flags},
	 * {offset, count, reserved, flags},
	 * status */

	/** First, takes a zero byte lock at offset 10. Then:
	*   - Taking 0 byte lock at 10 should succeed.
	*   - Taking 1 byte locks at 9,10,11 should succeed.
	*   - Taking 2 byte lock at 9 should fail.
	*   - Taking 2 byte lock at 10 should succeed.
	*   - Taking 3 byte lock at 9 should fail.
	*/
	{{10, 0, 0, 0}, {10, 0, 0, 0}, NT_STATUS_OK},
	{{10, 0, 0, 0}, {9, 1, 0, 0},  NT_STATUS_OK},
	{{10, 0, 0, 0}, {10, 1, 0, 0}, NT_STATUS_OK},
	{{10, 0, 0, 0}, {11, 1, 0, 0}, NT_STATUS_OK},
	{{10, 0, 0, 0}, {9, 2, 0, 0},  NT_STATUS_LOCK_NOT_GRANTED},
	{{10, 0, 0, 0}, {10, 2, 0, 0}, NT_STATUS_OK},
	{{10, 0, 0, 0}, {9, 3, 0, 0},  NT_STATUS_LOCK_NOT_GRANTED},

	/** Same, but opposite order. */
	{{10, 0, 0, 0}, {10, 0, 0, 0}, NT_STATUS_OK},
	{{9, 1, 0, 0},  {10, 0, 0, 0}, NT_STATUS_OK},
	{{10, 1, 0, 0}, {10, 0, 0, 0}, NT_STATUS_OK},
	{{11, 1, 0, 0}, {10, 0, 0, 0}, NT_STATUS_OK},
	{{9, 2, 0, 0},  {10, 0, 0, 0}, NT_STATUS_LOCK_NOT_GRANTED},
	{{10, 2, 0, 0}, {10, 0, 0, 0}, NT_STATUS_OK},
	{{9, 3, 0, 0},  {10, 0, 0, 0}, NT_STATUS_LOCK_NOT_GRANTED},

	/** Zero zero case. */
	{{0, 0, 0, 0},  {0, 0, 0, 0},  NT_STATUS_OK},
};

static bool test_zerobytelength(struct torture_context *torture,
			        struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	int i;

	const char *fname = BASEDIR "\\zero.txt";

	torture_comment(torture, "Testing zero length byte range locks:\n");

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Setup initial parameters */
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;

	/* Try every combination of locks in zero_byte_tests, using the same
	 * handle for both locks. The first lock is assumed to succeed. The
	 * second lock may contend, depending on the expected status. */
	for (i = 0; i < ARRAY_SIZE(zero_byte_tests); i++) {
		torture_comment(torture,
		    "  ... {%llu, %llu} + {%llu, %llu} = %s\n",
		    (unsigned long long) zero_byte_tests[i].lock1.offset,
		    (unsigned long long) zero_byte_tests[i].lock1.length,
		    (unsigned long long) zero_byte_tests[i].lock2.offset,
		    (unsigned long long) zero_byte_tests[i].lock2.length,
		    nt_errstr(zero_byte_tests[i].status));

		/* Lock both locks. */
		lck.in.locks		= &zero_byte_tests[i].lock1;
		lck.in.locks[0].flags	= SMB2_LOCK_FLAG_EXCLUSIVE |
					  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);

		lck.in.locks		= &zero_byte_tests[i].lock2;
		lck.in.locks[0].flags	= SMB2_LOCK_FLAG_EXCLUSIVE |
					  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CONT(status, zero_byte_tests[i].status);

		/* Unlock both locks in reverse order. */
		lck.in.locks[0].flags	= SMB2_LOCK_FLAG_UNLOCK;
		if (NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
			status = smb2_lock(tree, &lck);
			CHECK_STATUS(status, NT_STATUS_OK);
		}

		lck.in.locks		= &zero_byte_tests[i].lock1;
		lck.in.locks[0].flags	= SMB2_LOCK_FLAG_UNLOCK;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	/* Try every combination of locks in zero_byte_tests, using two
	 * different handles. */
	for (i = 0; i < ARRAY_SIZE(zero_byte_tests); i++) {
		torture_comment(torture,
		    "  ... {%llu, %llu} + {%llu, %llu} = %s\n",
		    (unsigned long long) zero_byte_tests[i].lock1.offset,
		    (unsigned long long) zero_byte_tests[i].lock1.length,
		    (unsigned long long) zero_byte_tests[i].lock2.offset,
		    (unsigned long long) zero_byte_tests[i].lock2.length,
		    nt_errstr(zero_byte_tests[i].status));

		/* Lock both locks. */
		lck.in.file.handle	= h;
		lck.in.locks		= &zero_byte_tests[i].lock1;
		lck.in.locks[0].flags	= SMB2_LOCK_FLAG_EXCLUSIVE |
					  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);

		lck.in.file.handle	= h2;
		lck.in.locks		= &zero_byte_tests[i].lock2;
		lck.in.locks[0].flags	= SMB2_LOCK_FLAG_EXCLUSIVE |
					  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CONT(status, zero_byte_tests[i].status);

		/* Unlock both locks in reverse order. */
		lck.in.file.handle	= h2;
		lck.in.locks[0].flags	= SMB2_LOCK_FLAG_UNLOCK;
		if (NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
			status = smb2_lock(tree, &lck);
			CHECK_STATUS(status, NT_STATUS_OK);
		}

		lck.in.file.handle	= h;
		lck.in.locks		= &zero_byte_tests[i].lock1;
		lck.in.locks[0].flags	= SMB2_LOCK_FLAG_UNLOCK;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

static bool test_zerobyteread(struct torture_context *torture,
			      struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[1];
	struct smb2_read rd;

	const char *fname = BASEDIR "\\zerobyteread.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Setup initial parameters */
	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;

	ZERO_STRUCT(rd);
	rd.in.file.handle = h2;

	torture_comment(torture, "Testing zero byte read on lock range:\n");

	/* Take an exclusive lock */
	torture_comment(torture, "  taking exclusive lock.\n");
	el[0].offset		= 0;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	/* Try a zero byte read */
	torture_comment(torture, "  reading 0 bytes.\n");
	rd.in.offset      = 5;
	rd.in.length      = 0;
	status = smb2_read(tree, tree, &rd);
	torture_assert_int_equal_goto(torture, rd.out.data.length, 0, ret, done,
				      "zero byte read did not return 0 bytes");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Unlock lock */
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	torture_comment(torture, "Testing zero byte read on zero byte lock "
				 "range:\n");

	/* Take an exclusive lock */
	torture_comment(torture, "  taking exclusive 0-byte lock.\n");
	el[0].offset		= 5;
	el[0].length		= 0;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	/* Try a zero byte read before the lock */
	torture_comment(torture, "  reading 0 bytes before the lock.\n");
	rd.in.offset      = 4;
	rd.in.length      = 0;
	status = smb2_read(tree, tree, &rd);
	torture_assert_int_equal_goto(torture, rd.out.data.length, 0, ret, done,
				      "zero byte read did not return 0 bytes");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try a zero byte read on the lock */
	torture_comment(torture, "  reading 0 bytes on the lock.\n");
	rd.in.offset      = 5;
	rd.in.length      = 0;
	status = smb2_read(tree, tree, &rd);
	torture_assert_int_equal_goto(torture, rd.out.data.length, 0, ret, done,
				      "zero byte read did not return 0 bytes");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try a zero byte read after the lock */
	torture_comment(torture, "  reading 0 bytes after the lock.\n");
	rd.in.offset      = 6;
	rd.in.length      = 0;
	status = smb2_read(tree, tree, &rd);
	torture_assert_int_equal_goto(torture, rd.out.data.length, 0, ret, done,
				      "zero byte read did not return 0 bytes");
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Unlock lock */
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

static bool test_unlock(struct torture_context *torture,
		        struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el1[1];
	struct smb2_lock_element el2[1];

	const char *fname = BASEDIR "\\unlock.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Setup initial parameters */
	lck.in.locks		= el1;
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	el1[0].offset		= 0;
	el1[0].length		= 10;
	el1[0].reserved		= 0x00000000;

	/* Take exclusive lock, then unlock it with a shared-unlock call. */

	torture_comment(torture, "Testing unlock exclusive with shared\n");

	torture_comment(torture, "  taking exclusive lock.\n");
	lck.in.file.handle	= h;
	el1[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  try to unlock the exclusive with a shared "
				 "unlock call.\n");
	el1[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	torture_comment(torture, "  try shared lock on h2, to test the "
				 "unlock.\n");
	lck.in.file.handle	= h2;
	el1[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	torture_comment(torture, "  unlock the exclusive lock\n");
	lck.in.file.handle	= h;
	el1[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Unlock a shared lock with an exclusive-unlock call. */

	torture_comment(torture, "Testing unlock shared with exclusive\n");

	torture_comment(torture, "  taking shared lock.\n");
	lck.in.file.handle	= h;
	el1[0].flags		= SMB2_LOCK_FLAG_SHARED;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "  try to unlock the shared with an exclusive "
				 "unlock call.\n");
	el1[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	torture_comment(torture, "  try exclusive lock on h2, to test the "
				 "unlock.\n");
	lck.in.file.handle	= h2;
	el1[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	torture_comment(torture, "  unlock the exclusive lock\n");
	lck.in.file.handle	= h;
	el1[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Test unlocking of stacked 0-byte locks. SMB2 0-byte lock behavior
	 * should be the same as >0-byte behavior.  Exclusive locks should be
	 * unlocked before shared. */

	torture_comment(torture, "Test unlocking stacked 0-byte locks\n");

	lck.in.locks		= el1;
	lck.in.file.handle	= h;
	el1[0].offset		= 10;
	el1[0].length		= 0;
	el1[0].reserved		= 0x00000000;
	el2[0].offset		= 5;
	el2[0].length		= 10;
	el2[0].reserved		= 0x00000000;

	/* lock 0-byte exclusive */
	el1[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* lock 0-byte shared */
	el1[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* test contention */
	lck.in.locks		= el2;
	lck.in.file.handle	= h2;
	el2[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	lck.in.locks		= el2;
	lck.in.file.handle	= h2;
	el2[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* unlock */
	lck.in.locks		= el1;
	lck.in.file.handle	= h;
	el1[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* test - can we take a shared lock? */
	lck.in.locks		= el2;
	lck.in.file.handle	= h2;
	el2[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el2[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	lck.in.locks		= el1;
	lck.in.file.handle	= h;
	el1[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Test unlocking of stacked exclusive, shared locks. Exclusive
	 * should be unlocked before any shared. */

	torture_comment(torture, "Test unlocking stacked exclusive/shared "
				 "locks\n");

	lck.in.locks		= el1;
	lck.in.file.handle	= h;
	el1[0].offset		= 10;
	el1[0].length		= 10;
	el1[0].reserved		= 0x00000000;
	el2[0].offset		= 5;
	el2[0].length		= 10;
	el2[0].reserved		= 0x00000000;

	/* lock exclusive */
	el1[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* lock shared */
	el1[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* test contention */
	lck.in.locks		= el2;
	lck.in.file.handle	= h2;
	el2[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	lck.in.locks		= el2;
	lck.in.file.handle	= h2;
	el2[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* unlock */
	lck.in.locks		= el1;
	lck.in.file.handle	= h;
	el1[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* test - can we take a shared lock? */
	lck.in.locks		= el2;
	lck.in.file.handle	= h2;
	el2[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el2[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	lck.in.locks		= el1;
	lck.in.file.handle	= h;
	el1[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

static bool test_multiple_unlock(struct torture_context *torture,
				 struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[2];

	const char *fname = BASEDIR "\\unlock_multiple.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "Testing multiple unlocks:\n");

	/* Setup initial parameters */
	lck.in.lock_count	= 0x0002;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;
	el[1].offset		= 10;
	el[1].length		= 10;
	el[1].reserved		= 0x00000000;

	/* Test1: Acquire second lock, but not first. */
	torture_comment(torture, "  unlock 2 locks, first one not locked. "
				 "Expect no locks unlocked. \n");

	lck.in.lock_count	= 0x0001;
	el[1].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[1];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try to unlock both locks */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	/* Second lock should not be unlocked. */
	lck.in.lock_count	= 0x0001;
	el[1].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[1];
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_W2K8(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
		torture_warning(torture, "Target has \"pretty please\" bug. "
				"A contending lock request on the same handle "
				"unlocks the lock.\n");
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}

	/* cleanup */
	lck.in.lock_count	= 0x0001;
	el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
	lck.in.locks		= &el[1];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Test2: Acquire first lock, but not second. */
	torture_comment(torture, "  unlock 2 locks, second one not locked. "
				 "Expect first lock unlocked.\n");

	lck.in.lock_count	= 0x0001;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[0];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try to unlock both locks */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	/* First lock should be unlocked. */
	lck.in.lock_count	= 0x0001;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[0];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	lck.in.lock_count	= 0x0001;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	lck.in.locks		= &el[0];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Test3: Request 2 locks, second will contend.  What happens to the
	 * first? */
	torture_comment(torture, "  request 2 locks, second one will contend. "
				 "Expect both to fail.\n");

	/* Lock the second range */
	lck.in.lock_count	= 0x0001;
	el[1].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[1];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Request both locks */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* First lock should be unlocked. */
	lck.in.lock_count	= 0x0001;
	lck.in.locks		= &el[0];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	if (TARGET_IS_W2K8(torture)) {
		lck.in.lock_count	= 0x0001;
		el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
		lck.in.locks		= &el[0];
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);
		torture_warning(torture, "Target has \"pretty please\" bug. "
				"A contending lock request on the same handle "
				"unlocks the lock.\n");
	} else {
		lck.in.lock_count	= 0x0002;
		el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
		el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
		lck.in.locks		= el;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	/* Test4: Request unlock and lock.  The lock contends, is the unlock
	 * then relocked?  SMB2 doesn't like the lock and unlock requests in the
	 * same packet. The unlock will succeed, but the lock will return
	 * INVALID_PARAMETER.  This behavior is described in MS-SMB2
	 * 3.3.5.14.1 */
	torture_comment(torture, "  request unlock and lock, second one will "
				 "error. Expect the unlock to succeed.\n");

	/* Lock both ranges */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[1].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Attempt to unlock the first range and lock the second. The lock
	 * request will error. */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	el[1].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	/* The first lock should've been unlocked */
	lck.in.lock_count	= 0x0001;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[0];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Test10: SMB2 only test. Request unlock and lock in same packet.
	 * Neither contend. SMB2 doesn't like lock and unlock requests in the
	 * same packet.  The unlock will succeed, but the lock will return
	 * INVALID_PARAMETER. */
	torture_comment(torture, "  request unlock and lock.  Unlock will "
				 "succeed, but lock will fail.\n");

	/* Lock first range */
	lck.in.lock_count	= 0x0001;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[0];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Attempt to unlock the first range and lock the second */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	el[1].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	/* Neither lock should still be locked */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[1].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Test11: SMB2 only test. Request lock and unlock in same packet.
	 * Neither contend. SMB2 doesn't like lock and unlock requests in the
	 * same packet.  The lock will succeed, the unlock will fail with
	 * INVALID_PARAMETER, and the lock will be unlocked before return. */
	torture_comment(torture, "  request lock and unlock.  Both will "
				 "fail.\n");

	/* Lock second range */
	lck.in.lock_count	= 0x0001;
	el[1].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[1];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Attempt to lock the first range and unlock the second */
	lck.in.lock_count	= 0x0002;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
	lck.in.locks		= el;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	/* First range should be unlocked, second locked. */
	lck.in.lock_count	= 0x0001;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[0];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.lock_count	= 0x0001;
	el[1].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	lck.in.locks		= &el[1];
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* cleanup */
	if (TARGET_IS_W2K8(torture)) {
		lck.in.lock_count	= 0x0001;
		el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
		lck.in.locks		= &el[0];
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);
		torture_warning(torture, "Target has \"pretty please\" bug. "
				"A contending lock request on the same handle "
				"unlocks the lock.\n");
	} else {
		lck.in.lock_count	= 0x0002;
		el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
		el[1].flags		= SMB2_LOCK_FLAG_UNLOCK;
		lck.in.locks		= el;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

done:
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/**
 * Test lock stacking
 *  - some tests ported from BASE-LOCK-LOCK5
 */
static bool test_stacking(struct torture_context *torture,
			  struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[1];

	const char *fname = BASEDIR "\\stacking.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "Testing lock stacking:\n");

	/* Setup initial parameters */
	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;

	/* Try to take a shared lock, then a shared lock on same handle */
	torture_comment(torture, "  stacking a shared on top of a shared"
				 "lock succeeds.\n");

	el[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* cleanup */
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);


	/* Try to take an exclusive lock, then a shared lock on same handle */
	torture_comment(torture, "  stacking a shared on top of an exclusive "
				 "lock succeeds.\n");

	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* stacking a shared from a different handle should fail */
	lck.in.file.handle	= h2;
	el[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* cleanup */
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* ensure the 4th unlock fails */
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	/* ensure a second handle can now take an exclusive lock */
	lck.in.file.handle	= h2;
	el[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try to take an exclusive lock, then a shared lock on a
	 * different handle */
	torture_comment(torture, "  stacking a shared on top of an exclusive "
				 "lock with different handles fails.\n");

	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.file.handle	= h2;
	el[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* cleanup */
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Try to take a shared lock, then stack an exclusive with same
	 * handle.  */
	torture_comment(torture, "  stacking an exclusive on top of a shared "
				 "lock fails.\n");

	el[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* cleanup */
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_W2K8(torture)) {
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
		torture_warning(torture, "Target has \"pretty please\" bug. "
				"A contending lock request on the same handle "
				"unlocks the lock.\n");
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	/* Prove that two exclusive locks do not stack on the same handle. */
	torture_comment(torture, "  two exclusive locks do not stack.\n");

	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* cleanup */
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_W2K8(torture)) {
		CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);
		torture_warning(torture, "Target has \"pretty please\" bug. "
				"A contending lock request on the same handle "
				"unlocks the lock.\n");
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
	}

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/**
 * Test lock contention
 * - shared lock should contend with exclusive lock on different handle
 */
static bool test_contend(struct torture_context *torture,
			 struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[1];

	const char *fname = BASEDIR "\\contend.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "Testing lock contention:\n");

	/* Setup initial parameters */
	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;

	/* Take an exclusive lock, then a shared lock on different handle */
	torture_comment(torture, "  shared should contend on exclusive on "
				 "different handle.\n");

	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.file.handle	= h2;
	el[0].flags		= SMB2_LOCK_FLAG_SHARED |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	/* cleanup */
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/**
 * Test locker context
 * - test that pid does not affect the locker context
 */
static bool test_context(struct torture_context *torture,
			 struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[1];

	const char *fname = BASEDIR "\\context.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "Testing locker context:\n");

	/* Setup initial parameters */
	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;

	/* Take an exclusive lock, then try to unlock with a different pid,
	 * same handle.  This shows that the pid doesn't affect the locker
	 * context in SMB2. */
	torture_comment(torture, "  pid shouldn't affect locker context\n");

	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	tree->session->pid++;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	tree->session->pid--;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/**
 * Test as much of the potential lock range as possible
 *  - test ported from BASE-LOCK-LOCK3
 */
static bool test_range(struct torture_context *torture,
		       struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[1];
	uint64_t offset, i;
	extern int torture_numops;

	const char *fname = BASEDIR "\\range.txt";

#define NEXT_OFFSET offset += (~(uint64_t)0) / torture_numops

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "Testing locks spread across the 64-bit "
				 "offset range\n");

	if (TARGET_IS_W2K8(torture)) {
		torture_result(torture, TORTURE_SKIP,
		    "Target has \"pretty please\" bug. A contending lock "
		    "request on the same handle unlocks the lock.");
		goto done;
	}

	/* Setup initial parameters */
	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 1;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;

	torture_comment(torture, "  establishing %d locks\n", torture_numops);

	for (offset=i=0; i<torture_numops; i++) {
		NEXT_OFFSET;

		lck.in.file.handle	= h;
		el[0].offset		= offset - 1;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CMT(status, NT_STATUS_OK,
				 talloc_asprintf(torture,
				     "lock h failed at offset %#llx ",
				     (unsigned long long) el[0].offset));

		lck.in.file.handle	= h2;
		el[0].offset		= offset - 2;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CMT(status, NT_STATUS_OK,
				 talloc_asprintf(torture,
				     "lock h2 failed at offset %#llx ",
				     (unsigned long long) el[0].offset));
	}

	torture_comment(torture, "  testing %d locks\n", torture_numops);

	for (offset=i=0; i<torture_numops; i++) {
		NEXT_OFFSET;

		lck.in.file.handle	= h;
		el[0].offset		= offset - 1;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CMT(status, NT_STATUS_LOCK_NOT_GRANTED,
				 talloc_asprintf(torture,
				     "lock h at offset %#llx should not have "
				     "succeeded ",
				     (unsigned long long) el[0].offset));

		lck.in.file.handle	= h;
		el[0].offset		= offset - 2;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CMT(status, NT_STATUS_LOCK_NOT_GRANTED,
				 talloc_asprintf(torture,
				     "lock h2 at offset %#llx should not have "
				     "succeeded ",
				     (unsigned long long) el[0].offset));

		lck.in.file.handle	= h2;
		el[0].offset		= offset - 1;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CMT(status, NT_STATUS_LOCK_NOT_GRANTED,
				 talloc_asprintf(torture,
				     "lock h at offset %#llx should not have "
				     "succeeded ",
				     (unsigned long long) el[0].offset));

		lck.in.file.handle	= h2;
		el[0].offset		= offset - 2;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CMT(status, NT_STATUS_LOCK_NOT_GRANTED,
				 talloc_asprintf(torture,
				     "lock h2 at offset %#llx should not have "
				     "succeeded ",
				     (unsigned long long) el[0].offset));
	}

	torture_comment(torture, "  removing %d locks\n", torture_numops);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;

	for (offset=i=0; i<torture_numops; i++) {
		NEXT_OFFSET;

		lck.in.file.handle	= h;
		el[0].offset		= offset - 1;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CMT(status, NT_STATUS_OK,
				 talloc_asprintf(torture,
				     "unlock from h failed at offset %#llx ",
				     (unsigned long long) el[0].offset));

		lck.in.file.handle	= h2;
		el[0].offset		= offset - 2;
		status = smb2_lock(tree, &lck);
		CHECK_STATUS_CMT(status, NT_STATUS_OK,
				 talloc_asprintf(torture,
				     "unlock from h2 failed at offset %#llx ",
				     (unsigned long long) el[0].offset));
	}

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

static NTSTATUS smb2cli_lock(struct smb2_tree *tree, struct smb2_handle h,
			     uint64_t offset, uint64_t length, bool exclusive)
{
	struct smb2_lock lck;
	struct smb2_lock_element el[1];
	NTSTATUS status;

	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= offset;
	el[0].length		= length;
	el[0].reserved		= 0x00000000;
	el[0].flags		= (exclusive ?
				  SMB2_LOCK_FLAG_EXCLUSIVE :
				  SMB2_LOCK_FLAG_SHARED) |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;

	status = smb2_lock(tree, &lck);

	return status;
}

static NTSTATUS smb2cli_unlock(struct smb2_tree *tree, struct smb2_handle h,
			       uint64_t offset, uint64_t length)
{
	struct smb2_lock lck;
	struct smb2_lock_element el[1];
	NTSTATUS status;

	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= offset;
	el[0].length		= length;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;

	status = smb2_lock(tree, &lck);

	return status;
}

#define EXPECTED(ret, v) if ((ret) != (v)) { \
	torture_result(torture, TORTURE_FAIL, __location__": subtest failed");\
        torture_comment(torture, "** "); correct = false; \
        }

/**
 * Test overlapping lock ranges from various lockers
 *  - some tests ported from BASE-LOCK-LOCK4
 */
static bool test_overlap(struct torture_context *torture,
			 struct smb2_tree *tree,
			 struct smb2_tree *tree2)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2, h3;
	uint8_t buf[200];
	bool correct = true;

	const char *fname = BASEDIR "\\overlap.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = torture_smb2_testfile(tree2, fname, &h3);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "Testing overlapping locks:\n");

	ret = NT_STATUS_IS_OK(smb2cli_lock(tree, h, 0, 4, true)) &&
	      NT_STATUS_IS_OK(smb2cli_lock(tree, h, 2, 4, true));
	EXPECTED(ret, false);
	torture_comment(torture, "the same session/handle %s set overlapping "
				 "exclusive locks\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smb2cli_lock(tree, h, 10, 4, false)) &&
	      NT_STATUS_IS_OK(smb2cli_lock(tree, h, 12, 4, false));
	EXPECTED(ret, true);
	torture_comment(torture, "the same session/handle %s set overlapping "
				 "shared locks\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smb2cli_lock(tree, h, 20, 4, true)) &&
	      NT_STATUS_IS_OK(smb2cli_lock(tree2, h3, 22, 4, true));
	EXPECTED(ret, false);
	torture_comment(torture, "a different session %s set overlapping "
				 "exclusive locks\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smb2cli_lock(tree, h, 30, 4, false)) &&
	      NT_STATUS_IS_OK(smb2cli_lock(tree2, h3, 32, 4, false));
	EXPECTED(ret, true);
	torture_comment(torture, "a different session %s set overlapping "
				 "shared locks\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smb2cli_lock(tree, h, 40, 4, true)) &&
	      NT_STATUS_IS_OK(smb2cli_lock(tree, h2, 42, 4, true));
	EXPECTED(ret, false);
	torture_comment(torture, "a different handle %s set overlapping "
				 "exclusive locks\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smb2cli_lock(tree, h, 50, 4, false)) &&
	      NT_STATUS_IS_OK(smb2cli_lock(tree, h2, 52, 4, false));
	EXPECTED(ret, true);
	torture_comment(torture, "a different handle %s set overlapping "
				 "shared locks\n", ret?"can":"cannot");

	ret = NT_STATUS_IS_OK(smb2cli_lock(tree, h, 110, 4, false)) &&
	      NT_STATUS_IS_OK(smb2cli_lock(tree, h, 112, 4, false)) &&
	      NT_STATUS_IS_OK(smb2cli_unlock(tree, h, 110, 6));
	EXPECTED(ret, false);
	torture_comment(torture, "the same handle %s coalesce read locks\n",
				 ret?"can":"cannot");

	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = torture_smb2_testfile(tree, fname, &h2);
	CHECK_STATUS(status, NT_STATUS_OK);
	ret = NT_STATUS_IS_OK(smb2cli_lock(tree, h, 0, 8, false)) &&
	      NT_STATUS_IS_OK(smb2cli_lock(tree, h2, 0, 1, false)) &&
	      NT_STATUS_IS_OK(smb2_util_close(tree, h)) &&
	      NT_STATUS_IS_OK(torture_smb2_testfile(tree, fname, &h)) &&
	      NT_STATUS_IS_OK(smb2cli_lock(tree, h, 7, 1, true));
	EXPECTED(ret, true);
	torture_comment(torture, "the server %s have the NT byte range lock "
				 "bug\n", !ret?"does":"doesn't");

done:
	smb2_util_close(tree2, h3);
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return correct;
}

/**
 * Test truncation of locked file
 *  - some tests ported from BASE-LOCK-LOCK7
 */
static bool test_truncate(struct torture_context *torture,
			  struct smb2_tree *tree)
{
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h, h2;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[1];
	struct smb2_create io;

	const char *fname = BASEDIR "\\truncate.txt";

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	CHECK_STATUS(status, NT_STATUS_OK);
	smb2_util_close(tree, h);

	status = torture_smb2_testfile(tree, fname, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(buf);
	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(torture, "Testing truncation of locked file:\n");

	/* Setup initial parameters */
	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.lock_sequence	= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 10;
	el[0].reserved		= 0x00000000;

	ZERO_STRUCT(io);
	io.in.oplock_level = 0;
	io.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.in.file_attributes   = FILE_ATTRIBUTE_NORMAL;
	io.in.create_disposition = NTCREATEX_DISP_OVERWRITE;
	io.in.share_access = NTCREATEX_SHARE_ACCESS_DELETE |
			     NTCREATEX_SHARE_ACCESS_READ |
			     NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options = 0;
	io.in.fname = fname;

	/* Take an exclusive lock */
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE |
				  SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* On second handle open the file with OVERWRITE disposition */
	torture_comment(torture, "  overwrite disposition is allowed on a "
				 "locked file.\n");

	io.in.create_disposition = NTCREATEX_DISP_OVERWRITE;
	status = smb2_create(tree, tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	h2 = io.out.file.handle;
	smb2_util_close(tree, h2);

	/* On second handle open the file with SUPERSEDE disposition */
	torture_comment(torture, "  supersede disposition is allowed on a "
				 "locked file.\n");

	io.in.create_disposition = NTCREATEX_DISP_SUPERSEDE;
	status = smb2_create(tree, tree, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	h2 = io.out.file.handle;
	smb2_util_close(tree, h2);

	/* cleanup */
	lck.in.file.handle	= h;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	smb2_util_close(tree, h2);
	smb2_util_close(tree, h);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/* basic testing of SMB2 locking
*/
struct torture_suite *torture_smb2_lock_init(void)
{
	struct torture_suite *suite =
	    torture_suite_create(talloc_autofree_context(), "lock");

	torture_suite_add_1smb2_test(suite, "valid-request",
	    test_valid_request);
	torture_suite_add_1smb2_test(suite, "rw-none", test_lock_rw_none);
	torture_suite_add_1smb2_test(suite, "rw-shared", test_lock_rw_shared);
	torture_suite_add_1smb2_test(suite, "rw-exclusive",
	    test_lock_rw_exclusive);
	torture_suite_add_1smb2_test(suite, "auto-unlock",
	    test_lock_auto_unlock);
	torture_suite_add_1smb2_test(suite, "lock", test_lock);
	torture_suite_add_1smb2_test(suite, "async", test_async);
	torture_suite_add_1smb2_test(suite, "cancel", test_cancel);
	torture_suite_add_1smb2_test(suite, "cancel-tdis", test_cancel_tdis);
	torture_suite_add_1smb2_test(suite, "cancel-logoff",
	    test_cancel_logoff);
	torture_suite_add_1smb2_test(suite, "errorcode", test_errorcode);
	torture_suite_add_1smb2_test(suite, "zerobytelength",
	    test_zerobytelength);
	torture_suite_add_1smb2_test(suite, "zerobyteread",
	    test_zerobyteread);
	torture_suite_add_1smb2_test(suite, "unlock", test_unlock);
	torture_suite_add_1smb2_test(suite, "multiple-unlock",
	    test_multiple_unlock);
	torture_suite_add_1smb2_test(suite, "stacking", test_stacking);
	torture_suite_add_1smb2_test(suite, "contend", test_contend);
	torture_suite_add_1smb2_test(suite, "context", test_context);
	torture_suite_add_1smb2_test(suite, "range", test_range);
	torture_suite_add_2smb2_test(suite, "overlap", test_overlap);
	torture_suite_add_1smb2_test(suite, "truncate", test_truncate);

	suite->description = talloc_strdup(suite, "SMB2-LOCK tests");

	return suite;
}
