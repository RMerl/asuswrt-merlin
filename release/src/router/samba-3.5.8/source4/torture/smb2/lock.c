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

#include "librpc/gen_ndr/ndr_security.h"

#define TARGET_IS_WINDOWS(_tctx) (torture_setting_bool(_tctx, "win7", false) || torture_setting_bool(torture, "windows", false))
#define TARGET_IS_WIN7(_tctx) (torture_setting_bool(_tctx, "win7", false))

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		printf("(%s) Incorrect status %s - should be %s\n", \
		       __location__, nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

#define CHECK_VALUE(v, correct) do { \
	if ((v) != (correct)) { \
		printf("(%s) Incorrect value %s=%d - should be %d\n", \
		       __location__, #v, v, correct); \
		ret = false; \
		goto done; \
	}} while (0)

static bool test_valid_request(struct torture_context *torture, struct smb2_tree *tree)
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

	lck.in.lock_count	= 0x0000;
	lck.in.reserved		= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0x0000000000000000;
	el[0].length		= 0x0000000000000000;
	el[0].reserved		= 0x0000000000000000;
	el[0].flags		= 0x00000000;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	lck.in.lock_count	= 0x0001;
	lck.in.reserved		= 0x00000000;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 0;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_NONE;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.file.handle.data[0] +=1;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	lck.in.file.handle.data[0] -=1;

	lck.in.lock_count	= 0x0001;
	lck.in.reserved		= 0x123ab1;
	lck.in.file.handle	= h;
	el[0].offset		= UINT64_MAX;
	el[0].length		= UINT64_MAX;
	el[0].reserved		= 0x00000000;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE|SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_WIN7(torture)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_LOCK_RANGE);
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
	}
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.reserved		= 0x123ab2;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_WIN7(torture)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_LOCK_RANGE);
	} else if (TARGET_IS_WINDOWS(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.reserved		= 0x123ab3;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_WIN7(torture)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_LOCK_RANGE);
	} else if (TARGET_IS_WINDOWS(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.reserved		= 0x123ab4;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_WIN7(torture)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_LOCK_RANGE);
	} else if (TARGET_IS_WINDOWS(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.reserved		= 0x123ab5;
	status = smb2_lock(tree, &lck);
	if (TARGET_IS_WIN7(torture)) {
		CHECK_STATUS(status, NT_STATUS_INVALID_LOCK_RANGE);
	} else if (TARGET_IS_WINDOWS(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.lock_count	= 0x0001;
	lck.in.reserved		= 0x12345678;
	lck.in.file.handle	= h;
	el[0].offset		= UINT32_MAX;
	el[0].length		= UINT32_MAX;
	el[0].reserved		= 0x87654321;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE|SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	status = smb2_lock(tree, &lck);
	if (TARGET_IS_WINDOWS(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}
	CHECK_VALUE(lck.out.reserved, 0);

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	status = smb2_lock(tree, &lck);
	if (TARGET_IS_WINDOWS(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}
	CHECK_VALUE(lck.out.reserved, 0);

	el[0].flags		= 0x00000000;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	el[0].flags		= 0x00000001;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.lock_count	= 0x0001;
	lck.in.reserved		= 0x87654321;
	lck.in.file.handle	= h;
	el[0].offset		= 0x00000000FFFFFFFF;
	el[0].length		= 0x00000000FFFFFFFF;
	el[0].reserved		= 0x1234567;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.lock_count	= 0x0001;
	lck.in.reserved		= 0x1234567;
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
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	lck.in.lock_count	= 0x0001;
	lck.in.reserved		= 0;
	lck.in.file.handle	= h;
	el[0].offset		= 1;
	el[0].length		= 1;
	el[0].reserved		= 0x00000000;
	el[0].flags		= ~SMB2_LOCK_FLAG_ALL_MASK;

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK | SMB2_LOCK_FLAG_EXCLUSIVE;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK | SMB2_LOCK_FLAG_SHARED;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK | SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_RANGE_NOT_LOCKED);

	lck.in.lock_count	= 2;
	lck.in.reserved		= 0;
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
	CHECK_STATUS(status, NT_STATUS_OK);

	lck.in.lock_count	= 2;
	el[0].flags		= SMB2_LOCK_FLAG_UNLOCK;
	el[1].flags		= 0;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

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
	el[0].flags		= 0;
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
	lck.in.reserved		= 0x00000000;
	lck.in.file.handle	= h1;
	el[0].offset		= 0;
	el[0].length		= ARRAY_SIZE(buf)/2;
	el[0].reserved		= 0x00000000;
	el[0].flags		= s->lock_flags;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(lck.out.reserved, 0);

	lck.in.lock_count	= 0x0001;
	lck.in.reserved		= 0x00000000;
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
	lck.in.reserved		= 0x00000000;
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

static bool test_lock_rw_none(struct torture_context *torture, struct smb2_tree *tree)
{
	struct test_lock_read_write_state s = {
		.fname			= "lock_rw_none.dat",
		.lock_flags		= SMB2_LOCK_FLAG_NONE,
		.write_h1_status	= NT_STATUS_FILE_LOCK_CONFLICT,
		.read_h1_status		= NT_STATUS_OK,
		.write_h2_status	= NT_STATUS_FILE_LOCK_CONFLICT,
		.read_h2_status		= NT_STATUS_OK,
	};

	return test_lock_read_write(torture, tree, &s);
}

static bool test_lock_rw_shared(struct torture_context *torture, struct smb2_tree *tree)
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

static bool test_lock_rw_exclusiv(struct torture_context *torture, struct smb2_tree *tree)
{
	struct test_lock_read_write_state s = {
		.fname			= "lock_rw_exclusiv.dat",
		.lock_flags		= SMB2_LOCK_FLAG_EXCLUSIVE,
		.write_h1_status	= NT_STATUS_OK,
		.read_h1_status		= NT_STATUS_OK,
		.write_h2_status	= NT_STATUS_FILE_LOCK_CONFLICT,
		.read_h2_status		= NT_STATUS_FILE_LOCK_CONFLICT,
	};

	return test_lock_read_write(torture, tree, &s);
}

static bool test_lock_auto_unlock(struct torture_context *torture, struct smb2_tree *tree)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle h;
	uint8_t buf[200];
	struct smb2_lock lck;
	struct smb2_lock_element el[2];

	ZERO_STRUCT(buf);

	status = torture_smb2_testfile(tree, "autounlock.txt", &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(lck);
	lck.in.locks		= el;
	lck.in.lock_count	= 0x0001;
	lck.in.file.handle	= h;
	el[0].offset		= 0;
	el[0].length		= 1;
	el[0].flags		= SMB2_LOCK_FLAG_EXCLUSIVE | SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

	status = smb2_lock(tree, &lck);
	if (TARGET_IS_WINDOWS(torture)) {
		CHECK_STATUS(status, NT_STATUS_OK);
	} else {
		CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);
	}

	status = smb2_lock(tree, &lck);
	CHECK_STATUS(status, NT_STATUS_LOCK_NOT_GRANTED);

done:
	return ret;
}


/* basic testing of SMB2 locking
*/
struct torture_suite *torture_smb2_lock_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "LOCK");

	torture_suite_add_1smb2_test(suite, "VALID-REQUEST", test_valid_request);
	torture_suite_add_1smb2_test(suite, "RW-NONE", test_lock_rw_none);
	torture_suite_add_1smb2_test(suite, "RW-SHARED", test_lock_rw_shared);
	torture_suite_add_1smb2_test(suite, "RW-EXCLUSIV", test_lock_rw_exclusiv);
	torture_suite_add_1smb2_test(suite, "AUTO-UNLOCK", test_lock_auto_unlock);

	suite->description = talloc_strdup(suite, "SMB2-LOCK tests");

	return suite;
}

