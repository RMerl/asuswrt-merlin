/* 
   Unix SMB/CIFS implementation.

   SMB2 create test suite

   Copyright (C) Andrew Tridgell 2008
   
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
#include "torture/util.h"
#include "torture/smb2/proto.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "libcli/security/security.h"

#include "system/filesys.h"
#include "auth/credentials/credentials.h"
#include "lib/cmdline/popt_common.h"
#include "librpc/gen_ndr/security.h"
#include "lib/events/events.h"

#define FNAME "test_create.dat"
#define DNAME "smb2_open"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect status %s - should be %s\n", \
			 __location__, nt_errstr(status), nt_errstr(correct)); \
		return false; \
	}} while (0)

#define CHECK_EQUAL(v, correct) do { \
	if (v != correct) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) Incorrect value for %s 0x%08llx - " \
		        "should be 0x%08llx\n", \
			 __location__, #v, \
		        (unsigned long long)v, \
		        (unsigned long long)correct); \
		return false;					\
	}} while (0)

#define CHECK_TIME(t, field) do { \
	time_t t1, t2; \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFORMATION; \
	finfo.all_info.in.file.handle = h1; \
	status = smb2_getinfo_file(tree, tctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	t1 = t & ~1; \
	t2 = nt_time_to_unix(finfo.all_info.out.field) & ~1; \
	if (abs(t1-t2) > 2) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) wrong time for field %s  %s - %s\n", \
			__location__, #field, \
			timestring(tctx, t1), \
			timestring(tctx, t2)); \
		dump_all_info(tctx, &finfo); \
		ret = false; \
	}} while (0)

#define CHECK_NTTIME(t, field) do { \
	NTTIME t2; \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFORMATION; \
	finfo.all_info.in.file.handle = h1; \
	status = smb2_getinfo_file(tree, tctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	t2 = finfo.all_info.out.field; \
	if (t != t2) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) wrong time for field %s  %s - %s\n", \
		       __location__, #field, \
		       nt_time_string(tctx, t), \
		       nt_time_string(tctx, t2)); \
		dump_all_info(tctx, &finfo); \
		ret = false; \
	}} while (0)

#define CHECK_ALL_INFO(v, field) do { \
	finfo.all_info.level = RAW_FILEINFO_ALL_INFORMATION; \
	finfo.all_info.in.file.handle = h1; \
	status = smb2_getinfo_file(tree, tctx, &finfo); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	if ((v) != (finfo.all_info.out.field)) { \
	       torture_result(tctx, TORTURE_FAIL, \
			"(%s) wrong value for field %s  0x%x - 0x%x\n", \
			__location__, #field, (int)v,\
			(int)(finfo.all_info.out.field)); \
		dump_all_info(tctx, &finfo); \
		ret = false; \
	}} while (0)

#define CHECK_VAL(v, correct) do { \
	if ((v) != (correct)) { \
		torture_result(tctx, TORTURE_FAIL, \
			"(%s) wrong value for %s  0x%x - should be 0x%x\n", \
		       __location__, #v, (int)(v), (int)correct); \
		ret = false; \
	}} while (0)

#define SET_ATTRIB(sattrib) do { \
	union smb_setfileinfo sfinfo; \
	ZERO_STRUCT(sfinfo.basic_info.in); \
	sfinfo.basic_info.level = RAW_SFILEINFO_BASIC_INFORMATION; \
	sfinfo.basic_info.in.file.handle = h1; \
	sfinfo.basic_info.in.attrib = sattrib; \
	status = smb2_setinfo_file(tree, &sfinfo); \
	if (!NT_STATUS_IS_OK(status)) { \
		torture_comment(tctx, \
		    "(%s) Failed to set attrib 0x%x on %s\n", \
		       __location__, sattrib, fname); \
	}} while (0)

/*
  test some interesting combinations found by gentest
 */
static bool test_create_gentest(struct torture_context *tctx, struct smb2_tree *tree)
{
	struct smb2_create io;
	NTSTATUS status;
	uint32_t access_mask, file_attributes_set;
	uint32_t ok_mask, not_supported_mask, invalid_parameter_mask;
	uint32_t not_a_directory_mask, unexpected_mask;
	union smb_fileinfo q;

	ZERO_STRUCT(io);
	io.in.desired_access     = SEC_FLAG_MAXIMUM_ALLOWED;
	io.in.file_attributes    = FILE_ATTRIBUTE_NORMAL;
	io.in.create_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options = 0;
	io.in.fname = FNAME;

	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.in.create_options = 0xF0000000;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	io.in.create_options = 0;

	io.in.file_attributes = FILE_ATTRIBUTE_DEVICE;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	io.in.file_attributes = FILE_ATTRIBUTE_VOLUME;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	io.in.create_disposition = NTCREATEX_DISP_OPEN;
	io.in.file_attributes = FILE_ATTRIBUTE_VOLUME;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	
	io.in.create_disposition = NTCREATEX_DISP_CREATE;
	io.in.desired_access = 0x08000000;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	io.in.desired_access = 0x04000000;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	io.in.file_attributes = 0;
	io.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.in.desired_access     = SEC_FLAG_MAXIMUM_ALLOWED;
	ok_mask = 0;
	not_supported_mask = 0;
	invalid_parameter_mask = 0;
	not_a_directory_mask = 0;
	unexpected_mask = 0;
	{
		int i;
		for (i=0;i<32;i++) {
			io.in.create_options = 1<<i;
			if (io.in.create_options & NTCREATEX_OPTIONS_DELETE_ON_CLOSE) {
				continue;
			}
			status = smb2_create(tree, tctx, &io);
			if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_SUPPORTED)) {
				not_supported_mask |= 1<<i;
			} else if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER)) {
				invalid_parameter_mask |= 1<<i;
			} else if (NT_STATUS_EQUAL(status, NT_STATUS_NOT_A_DIRECTORY)) {
				not_a_directory_mask |= 1<<i;
			} else if (NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
				ok_mask |= 1<<i;
				status = smb2_util_close(tree, io.out.file.handle);
				CHECK_STATUS(status, NT_STATUS_OK);
			} else {
				unexpected_mask |= 1<<i;
				torture_comment(tctx,
				    "create option 0x%08x returned %s\n",
				    1<<i, nt_errstr(status));
			}
		}
	}
	io.in.create_options = 0;

	CHECK_EQUAL(ok_mask,                0x00efcf7e);
	CHECK_EQUAL(not_a_directory_mask,   0x00000001);
	CHECK_EQUAL(not_supported_mask,     0x00102080);
	CHECK_EQUAL(invalid_parameter_mask, 0xff000000);
	CHECK_EQUAL(unexpected_mask,        0x00000000);

	io.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.in.file_attributes = 0;
	access_mask = 0;
	{
		int i;
		for (i=0;i<32;i++) {
			io.in.desired_access = 1<<i;
			status = smb2_create(tree, tctx, &io);
			if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED) ||
			    NT_STATUS_EQUAL(status, NT_STATUS_PRIVILEGE_NOT_HELD)) {
				access_mask |= io.in.desired_access;
			} else {
				CHECK_STATUS(status, NT_STATUS_OK);
				status = smb2_util_close(tree, io.out.file.handle);
				CHECK_STATUS(status, NT_STATUS_OK);
			}
		}
	}

	if (TARGET_IS_WIN7(tctx)) {
		CHECK_EQUAL(access_mask, 0x0de0fe00);
	} else if (torture_setting_bool(tctx, "samba4", false)) {
		CHECK_EQUAL(access_mask, 0x0cf0fe00);
	} else {
		CHECK_EQUAL(access_mask, 0x0df0fe00);
	}

	io.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.in.desired_access = SEC_FLAG_MAXIMUM_ALLOWED;
	io.in.file_attributes = 0;
	ok_mask = 0;
	invalid_parameter_mask = 0;
	unexpected_mask = 0;
	file_attributes_set = 0;
	{
		int i;
		for (i=0;i<32;i++) {
			io.in.file_attributes = 1<<i;
			if (io.in.file_attributes & FILE_ATTRIBUTE_ENCRYPTED) {
				continue;
			}
			smb2_deltree(tree, FNAME);
			status = smb2_create(tree, tctx, &io);
			if (NT_STATUS_EQUAL(status, NT_STATUS_INVALID_PARAMETER)) {
				invalid_parameter_mask |= 1<<i;
			} else if (NT_STATUS_IS_OK(status)) {
				uint32_t expected;
				ok_mask |= 1<<i;

				expected = (io.in.file_attributes | FILE_ATTRIBUTE_ARCHIVE) & 0x00005127;
				CHECK_EQUAL(io.out.file_attr, expected);
				file_attributes_set |= io.out.file_attr;

				status = smb2_util_close(tree, io.out.file.handle);
				CHECK_STATUS(status, NT_STATUS_OK);
			} else {
				unexpected_mask |= 1<<i;
				torture_comment(tctx,
				    "file attribute 0x%08x returned %s\n",
				    1<<i, nt_errstr(status));
			}
		}
	}

	CHECK_EQUAL(ok_mask,                0x00003fb7);
	CHECK_EQUAL(invalid_parameter_mask, 0xffff8048);
	CHECK_EQUAL(unexpected_mask,        0x00000000);
	CHECK_EQUAL(file_attributes_set,    0x00001127);

	smb2_deltree(tree, FNAME);

	/*
	 * Standalone servers doesn't support encryption
	 */
	io.in.file_attributes = FILE_ATTRIBUTE_ENCRYPTED;
	status = smb2_create(tree, tctx, &io);
	if (NT_STATUS_EQUAL(status, NT_STATUS_ACCESS_DENIED)) {
		torture_comment(tctx,
		    "FILE_ATTRIBUTE_ENCRYPTED returned %s\n",
		    nt_errstr(status));
	} else {
		CHECK_STATUS(status, NT_STATUS_OK);
		CHECK_EQUAL(io.out.file_attr, (FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_ARCHIVE));
		status = smb2_util_close(tree, io.out.file.handle);
		CHECK_STATUS(status, NT_STATUS_OK);
	}

	smb2_deltree(tree, FNAME);

	ZERO_STRUCT(io);
	io.in.desired_access     = SEC_FLAG_MAXIMUM_ALLOWED;
	io.in.file_attributes    = 0;
	io.in.create_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options = 0;
	io.in.fname = FNAME ":stream1";
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);

	io.in.fname = FNAME;
	io.in.file_attributes = 0x8040;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_READ;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	io.in.fname = FNAME;
	io.in.file_attributes = 0;
	io.in.desired_access  = SEC_FILE_READ_DATA | SEC_FILE_WRITE_DATA | SEC_FILE_APPEND_DATA;
	io.in.query_maximal_access = true;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_EQUAL(io.out.maximal_access, 0x001f01ff);

	q.access_information.level = RAW_FILEINFO_ACCESS_INFORMATION;
	q.access_information.in.file.handle = io.out.file.handle;
	status = smb2_getinfo_file(tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_EQUAL(q.access_information.out.access_flags, io.in.desired_access);

	io.in.file_attributes = 0;
	io.in.desired_access  = 0;
	io.in.query_maximal_access = false;
	io.in.share_access = 0;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	
	smb2_deltree(tree, FNAME);

	return true;
}


/*
  try the various request blobs
 */
static bool test_create_blob(struct torture_context *tctx, struct smb2_tree *tree)
{
	struct smb2_create io;
	NTSTATUS status;

	smb2_deltree(tree, FNAME);

	ZERO_STRUCT(io);
	io.in.desired_access     = SEC_FLAG_MAXIMUM_ALLOWED;
	io.in.file_attributes    = FILE_ATTRIBUTE_NORMAL;
	io.in.create_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;
	io.in.fname = FNAME;

	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing alloc size\n");
	io.in.alloc_size = 4096;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_EQUAL(io.out.alloc_size, io.in.alloc_size);

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing durable open\n");
	io.in.durable_open = true;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing query maximal access\n");
	io.in.query_maximal_access = true;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_EQUAL(io.out.maximal_access, 0x001f01ff);

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing timewarp\n");
	io.in.timewarp = 10000;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	io.in.timewarp = 0;

	torture_comment(tctx, "Testing query_on_disk\n");
	io.in.query_on_disk_id = true;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing unknown tag\n");
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "FooO", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing bad tag length 0\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "x", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	torture_comment(tctx, "Testing bad tag length 1\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "x", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	torture_comment(tctx, "Testing bad tag length 2\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xx", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	torture_comment(tctx, "Testing bad tag length 3\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xxx", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	torture_comment(tctx, "Testing tag length 4\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xxxx", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing tag length 5\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xxxxx", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing tag length 6\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xxxxxx", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing tag length 7\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xxxxxxx", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing tag length 8\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xxxxxxxx", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing tag length 16\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xxxxxxxxxxxxxxxx", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing tag length 17\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xxxxxxxxxxxxxxxxx", data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "Testing tag length 34\n");
	ZERO_STRUCT(io.in.blobs);
	status = smb2_create_blob_add(tctx, &io.in.blobs,
				      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
				      data_blob(NULL, 0));
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	smb2_deltree(tree, FNAME);
	
	return true;
}

#define FAIL_UNLESS(__cond)					\
	do {							\
		if (__cond) {} else {				\
			torture_result(tctx, TORTURE_FAIL, "%s) condition violated: %s\n",	\
			       __location__, #__cond);		\
			ret = false; goto done;                 \
		}						\
	} while(0)

/*
  try creating with acls
 */
static bool test_create_acl_ext(struct torture_context *tctx, struct smb2_tree *tree, bool test_dir)
{
	bool ret = true;
	struct smb2_create io;
	NTSTATUS status;
	struct security_ace ace;
	struct security_descriptor *sd;
	struct dom_sid *test_sid;
	union smb_fileinfo q = {};
	uint32_t attrib =
	    FILE_ATTRIBUTE_HIDDEN |
	    FILE_ATTRIBUTE_SYSTEM |
	    (test_dir ? FILE_ATTRIBUTE_DIRECTORY : 0);
	NTSTATUS (*delete_func)(struct smb2_tree *, const char *) =
	    test_dir ? smb2_util_rmdir : smb2_util_unlink;

	smb2_deltree(tree, FNAME);

	ZERO_STRUCT(io);
	io.in.desired_access     = SEC_FLAG_MAXIMUM_ALLOWED;
	io.in.file_attributes    = FILE_ATTRIBUTE_NORMAL;
	io.in.create_disposition = NTCREATEX_DISP_CREATE;
	io.in.share_access = 
		NTCREATEX_SHARE_ACCESS_DELETE |
		NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options = NTCREATEX_OPTIONS_ASYNC_ALERT | 0x00200000 |
	    (test_dir ?  NTCREATEX_OPTIONS_DIRECTORY :
		(NTCREATEX_OPTIONS_NON_DIRECTORY_FILE));

	io.in.fname = FNAME;

	torture_comment(tctx, "basic create\n");

	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.handle = io.out.file.handle;
	q.query_secdesc.in.secinfo_flags = 
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb2_getinfo_file(tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);
	sd = q.query_secdesc.out.sd;

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = delete_func(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "adding a new ACE\n");
	test_sid = dom_sid_parse_talloc(tctx, SID_NT_AUTHENTICATED_USERS);

	ace.type = SEC_ACE_TYPE_ACCESS_ALLOWED;
	ace.flags = 0;
	ace.access_mask = SEC_STD_ALL;
	ace.trustee = *test_sid;

	status = security_descriptor_dacl_add(sd, &ace);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "creating a file with an initial ACL\n");

	io.in.sec_desc = sd;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	FAIL_UNLESS(smb2_util_verify_sd(tctx, tree, io.out.file.handle, sd));

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = delete_func(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "creating with attributes\n");

	io.in.sec_desc = NULL;
	io.in.file_attributes = attrib;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	FAIL_UNLESS(smb2_util_verify_attrib(tctx, tree, io.out.file.handle, attrib));

	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = delete_func(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "creating with attributes and ACL\n");

	io.in.sec_desc = sd;
	io.in.file_attributes = attrib;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	FAIL_UNLESS(smb2_util_verify_sd(tctx, tree, io.out.file.handle, sd));
	FAIL_UNLESS(smb2_util_verify_attrib(tctx, tree, io.out.file.handle, attrib));
	
	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = delete_func(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "creating with attributes, ACL and owner\n");
	sd = security_descriptor_dacl_create(tctx,
					0, SID_WORLD, SID_BUILTIN_USERS,
					SID_WORLD,
					SEC_ACE_TYPE_ACCESS_ALLOWED,
					SEC_RIGHTS_FILE_READ | SEC_STD_ALL,
					0,
					NULL);

	io.in.sec_desc = sd;
	io.in.file_attributes = attrib;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	FAIL_UNLESS(smb2_util_verify_sd(tctx, tree, io.out.file.handle, sd));
	FAIL_UNLESS(smb2_util_verify_attrib(tctx, tree, io.out.file.handle, attrib));

 done:
	status = smb2_util_close(tree, io.out.file.handle);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = delete_func(tree, FNAME);
	CHECK_STATUS(status, NT_STATUS_OK);

	return ret;
}

/*
  test SMB2 open
*/
static bool test_smb2_open(struct torture_context *tctx,
			   struct smb2_tree *tree)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = DNAME "\\torture_ntcreatex.txt";
	const char *dname = DNAME "\\torture_ntcreatex.dir";
	NTSTATUS status;
	struct smb2_handle h, h1;
	bool ret = true;
	int i;
	struct {
		uint32_t create_disp;
		bool with_file;
		NTSTATUS correct_status;
	} open_funcs[] = {
		{ NTCREATEX_DISP_SUPERSEDE,     true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_SUPERSEDE,     false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN,          true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN,          false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_CREATE,        true,  NT_STATUS_OBJECT_NAME_COLLISION },
		{ NTCREATEX_DISP_CREATE,        false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF,       true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OPEN_IF,       false, NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE,     true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE,     false, NT_STATUS_OBJECT_NAME_NOT_FOUND },
		{ NTCREATEX_DISP_OVERWRITE_IF,  true,  NT_STATUS_OK },
		{ NTCREATEX_DISP_OVERWRITE_IF,  false, NT_STATUS_OK },
		{ 6,                            true,  NT_STATUS_INVALID_PARAMETER },
		{ 6,                            false, NT_STATUS_INVALID_PARAMETER },
	};

	torture_comment(tctx, "Checking SMB2 Open\n");

	smb2_util_unlink(tree, fname);
	smb2_util_rmdir(tree, dname);

	status = torture_smb2_testdir(tree, DNAME, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(io.smb2);
	/* reasonable default parameters */
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 1024*1024;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/* test the create disposition */
	for (i=0; i<ARRAY_SIZE(open_funcs); i++) {
		if (open_funcs[i].with_file) {
			io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
			status= smb2_create(tree, tctx, &(io.smb2));
			if (!NT_STATUS_IS_OK(status)) {
				torture_comment(tctx,
				    "Failed to create file %s status %s %d\n",
				    fname, nt_errstr(status), i);

				ret = false;
				goto done;
			}
			smb2_util_close(tree, io.smb2.out.file.handle);
		}
		io.smb2.in.create_disposition = open_funcs[i].create_disp;
		status = smb2_create(tree, tctx, &(io.smb2));
		if (!NT_STATUS_EQUAL(status, open_funcs[i].correct_status)) {
			torture_comment(tctx,
			    "(%s) incorrect status %s should be %s (i=%d "
			    "with_file=%d open_disp=%d)\n",
			 __location__, nt_errstr(status),
			nt_errstr(open_funcs[i].correct_status),
			i, (int)open_funcs[i].with_file,
			(int)open_funcs[i].create_disp);

			ret = false;
			goto done;
		}
		if (NT_STATUS_IS_OK(status) || open_funcs[i].with_file) {
			smb2_util_close(tree, io.smb2.out.file.handle);
			smb2_util_unlink(tree, fname);
		}
	}

	/* basic field testing */
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;

	status = smb2_create(tree, tctx, &(io.smb2));
	CHECK_STATUS(status, NT_STATUS_OK);
	h1 = io.smb2.out.file.handle;

	CHECK_VAL(io.smb2.out.oplock_level, 0);
	CHECK_VAL(io.smb2.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_NTTIME(io.smb2.out.create_time, create_time);
	CHECK_NTTIME(io.smb2.out.access_time, access_time);
	CHECK_NTTIME(io.smb2.out.write_time, write_time);
	CHECK_NTTIME(io.smb2.out.change_time, change_time);
	CHECK_ALL_INFO(io.smb2.out.file_attr, attrib);
	CHECK_ALL_INFO(io.smb2.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.smb2.out.size, size);

	/* check fields when the file already existed */
	smb2_util_close(tree, h1);
	smb2_util_unlink(tree, fname);

	status = smb2_create_complex_file(tree, fname, &h1);
	CHECK_STATUS(status, NT_STATUS_OK);

	smb2_util_close(tree, h1);

	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN;
	status = smb2_create(tree, tctx, &(io.smb2));
	CHECK_STATUS(status, NT_STATUS_OK);
	h1 = io.smb2.out.file.handle;

	CHECK_VAL(io.smb2.out.oplock_level, 0);
	CHECK_VAL(io.smb2.out.create_action, NTCREATEX_ACTION_EXISTED);
	CHECK_NTTIME(io.smb2.out.create_time, create_time);
	CHECK_NTTIME(io.smb2.out.access_time, access_time);
	CHECK_NTTIME(io.smb2.out.write_time, write_time);
	CHECK_NTTIME(io.smb2.out.change_time, change_time);
	CHECK_ALL_INFO(io.smb2.out.file_attr, attrib);
	CHECK_ALL_INFO(io.smb2.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.smb2.out.size, size);
	smb2_util_close(tree, h1);
	smb2_util_unlink(tree, fname);

	/* create a directory */
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_DIRECTORY;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_options = 0;
	io.smb2.in.fname = dname;
	fname = dname;

	smb2_util_rmdir(tree, fname);
	smb2_util_unlink(tree, fname);

	io.smb2.in.desired_access = SEC_FLAG_MAXIMUM_ALLOWED;
	io.smb2.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				NTCREATEX_SHARE_ACCESS_WRITE;
	status = smb2_create(tree, tctx, &(io.smb2));
	CHECK_STATUS(status, NT_STATUS_OK);
	h1 = io.smb2.out.file.handle;

	CHECK_VAL(io.smb2.out.oplock_level, 0);
	CHECK_VAL(io.smb2.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_NTTIME(io.smb2.out.create_time, create_time);
	CHECK_NTTIME(io.smb2.out.access_time, access_time);
	CHECK_NTTIME(io.smb2.out.write_time, write_time);
	CHECK_NTTIME(io.smb2.out.change_time, change_time);
	CHECK_ALL_INFO(io.smb2.out.file_attr, attrib);
	CHECK_VAL(io.smb2.out.file_attr & ~FILE_ATTRIBUTE_NONINDEXED,
		  FILE_ATTRIBUTE_DIRECTORY);
	CHECK_ALL_INFO(io.smb2.out.alloc_size, alloc_size);
	CHECK_ALL_INFO(io.smb2.out.size, size);
	CHECK_VAL(io.smb2.out.size, 0);
	CHECK_VAL(io.smb2.out.alloc_size, 0);
	smb2_util_unlink(tree, fname);

done:
	smb2_util_close(tree, h1);
	smb2_util_unlink(tree, fname);
	smb2_deltree(tree, DNAME);
	return ret;
}

/*
  test with an already opened and byte range locked file
*/

static bool test_smb2_open_brlocked(struct torture_context *tctx,
				    struct smb2_tree *tree)
{
	union smb_open io, io1;
	union smb_lock io2;
	struct smb2_lock_element lock[1];
	const char *fname = DNAME "\\torture_ntcreatex.txt";
	NTSTATUS status;
	bool ret = true;
	struct smb2_handle h;
	char b = 42;

	torture_comment(tctx,
		"Testing SMB2 open with a byte range locked file\n");

	smb2_util_unlink(tree, fname);

	status = torture_smb2_testdir(tree, DNAME, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.desired_access = 0x2019f;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	io.smb2.in.create_options = NTCREATEX_OPTIONS_NON_DIRECTORY_FILE;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
	io.smb2.in.security_flags = SMB2_SECURITY_DYNAMIC_TRACKING;
	io.smb2.in.fname = fname;

	status = smb2_create(tree, tctx, &(io.smb2));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_write(tree, io.smb2.out.file.handle, &b, 0, 1);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(io2.smb2);
	io2.smb2.level = RAW_LOCK_SMB2;
	io2.smb2.in.file.handle = io.smb2.out.file.handle;
	io2.smb2.in.lock_count = 1;

	ZERO_STRUCT(lock);
	lock[0].offset = 0;
	lock[0].length = 1;
	lock[0].flags = SMB2_LOCK_FLAG_EXCLUSIVE |
			SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;
	io2.smb2.in.locks = &lock[0];
	status = smb2_lock(tree, &(io2.smb2));
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(io1.smb2);
	io1.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io1.smb2.in.desired_access = 0x20196;
	io1.smb2.in.alloc_size = 0;
	io1.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io1.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
		NTCREATEX_SHARE_ACCESS_WRITE;
	io1.smb2.in.create_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io1.smb2.in.create_options = 0;
	io1.smb2.in.impersonation_level = SMB2_IMPERSONATION_IMPERSONATION;
	io1.smb2.in.security_flags = SMB2_SECURITY_DYNAMIC_TRACKING;
	io1.smb2.in.fname = fname;

	status = smb2_create(tree, tctx, &(io1.smb2));
	CHECK_STATUS(status, NT_STATUS_OK);

	smb2_util_close(tree, io.smb2.out.file.handle);
	smb2_util_close(tree, io1.smb2.out.file.handle);
	smb2_util_unlink(tree, fname);
	smb2_deltree(tree, DNAME);

	return ret;
}

/* A little torture test to expose a race condition in Samba 3.0.20 ... :-) */

static bool test_smb2_open_multi(struct torture_context *tctx,
				struct smb2_tree *tree)
{
	const char *fname = "test_oplock.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_tree **trees;
	struct smb2_request **requests;
	union smb_open *ios;
	int i, num_files = 3;
	int num_ok = 0;
	int num_collision = 0;

	torture_comment(tctx,
		"Testing SMB2 Open with multiple connections\n");
	trees = talloc_array(tctx, struct smb2_tree *, num_files);
	requests = talloc_array(tctx, struct smb2_request *, num_files);
	ios = talloc_array(tctx, union smb_open, num_files);
	if ((tctx->ev == NULL) || (trees == NULL) || (requests == NULL) ||
	    (ios == NULL)) {
		torture_comment(tctx, ("talloc failed\n"));
		ret = false;
		goto done;
	}

	tree->session->transport->options.request_timeout = 60;

	for (i=0; i<num_files; i++) {
		if (!torture_smb2_connection(tctx, &(trees[i]))) {
			torture_comment(tctx,
				"Could not open %d'th connection\n", i);
			ret = false;
			goto done;
		}
		trees[i]->session->transport->options.request_timeout = 60;
	}

	/* cleanup */
	smb2_util_unlink(tree, fname);

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;
	io.smb2.in.create_flags = 0;

	for (i=0; i<num_files; i++) {
		ios[i] = io;
		requests[i] = smb2_create_send(trees[i], &(ios[i].smb2));
		if (requests[i] == NULL) {
			torture_comment(tctx,
				"could not send %d'th request\n", i);
			ret = false;
			goto done;
		}
	}

	torture_comment(tctx, "waiting for replies\n");
	while (1) {
		bool unreplied = false;
		for (i=0; i<num_files; i++) {
			if (requests[i] == NULL) {
				continue;
			}
			if (requests[i]->state < SMB2_REQUEST_DONE) {
				unreplied = true;
				break;
			}
			status = smb2_create_recv(requests[i], tctx,
						  &(ios[i].smb2));

			torture_comment(tctx,
				"File %d returned status %s\n", i,
				nt_errstr(status));

			if (NT_STATUS_IS_OK(status)) {
				num_ok += 1;
			}

			if (NT_STATUS_EQUAL(status,
					    NT_STATUS_OBJECT_NAME_COLLISION)) {
				num_collision += 1;
			}

			requests[i] = NULL;
		}
		if (!unreplied) {
			break;
		}

		if (event_loop_once(tctx->ev) != 0) {
			torture_comment(tctx, "event_loop_once failed\n");
			ret = false;
			goto done;
		}
	}

	if ((num_ok != 1) || (num_ok + num_collision != num_files)) {
		ret = false;
	}
done:
	smb2_deltree(tree, fname);

	return ret;
}

/*
  test opening for delete on a read-only attribute file.
*/

static bool test_smb2_open_for_delete(struct torture_context *tctx,
				      struct smb2_tree *tree)
{
	union smb_open io;
	union smb_fileinfo finfo;
	const char *fname = DNAME "\\torture_open_for_delete.txt";
	NTSTATUS status;
	struct smb2_handle h, h1;
	bool ret = true;

	torture_comment(tctx,
		"Checking SMB2_OPEN for delete on a readonly file.\n");
	smb2_util_unlink(tree, fname);
	smb2_deltree(tree, fname);

	status = torture_smb2_testdir(tree, DNAME, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* reasonable default parameters */
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_READONLY;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/* Create the readonly file. */

	status = smb2_create(tree, tctx, &(io.smb2));
	CHECK_STATUS(status, NT_STATUS_OK);
	h1 = io.smb2.out.file.handle;

	CHECK_VAL(io.smb2.out.oplock_level, 0);
	io.smb2.in.create_options = 0;
	CHECK_VAL(io.smb2.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_ALL_INFO(io.smb2.out.file_attr, attrib);
	smb2_util_close(tree, h1);

	/* Now try and open for delete only - should succeed. */
	io.smb2.in.desired_access = SEC_STD_DELETE;
	io.smb2.in.file_attributes = 0;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				NTCREATEX_SHARE_ACCESS_WRITE |
				NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN;
	status = smb2_create(tree, tctx, &(io.smb2));
	CHECK_STATUS(status, NT_STATUS_OK);

	smb2_util_unlink(tree, fname);

	smb2_util_close(tree, h1);
	smb2_util_unlink(tree, fname);
	smb2_deltree(tree, DNAME);

	return ret;
}

/*
  test SMB2 open with a leading slash on the path.
  Trying to create a directory with a leading slash
  should give NT_STATUS_INVALID_PARAMETER error
*/
static bool test_smb2_leading_slash(struct torture_context *tctx,
				    struct smb2_tree *tree)
{
	union smb_open io;
	const char *dnameslash = "\\"DNAME;
	NTSTATUS status;
	bool ret = true;

	torture_comment(tctx,
		"Trying to create a directory with leading slash on path\n");
	smb2_deltree(tree, dnameslash);

	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.oplock_level = 0;
	io.smb2.in.desired_access = SEC_RIGHTS_DIR_ALL;
	io.smb2.in.file_attributes   = FILE_ATTRIBUTE_DIRECTORY;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				NTCREATEX_SHARE_ACCESS_WRITE |
				NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	io.smb2.in.fname = dnameslash;

	status = smb2_create(tree, tree, &(io.smb2));
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	smb2_deltree(tree, dnameslash);
	return ret;
}


static bool test_create_acl_file(struct torture_context *tctx,
    struct smb2_tree *tree)
{
	torture_comment(tctx, "Testing nttrans create with sec_desc on files\n");

	return test_create_acl_ext(tctx, tree, false);
}

static bool test_create_acl_dir(struct torture_context *tctx,
    struct smb2_tree *tree)
{
	torture_comment(tctx, "Testing nttrans create with sec_desc on directories\n");

	return test_create_acl_ext(tctx, tree, true);
}

#define CHECK_ACCESS_FLAGS(_fh, flags) do { \
	union smb_fileinfo _q; \
	_q.access_information.level = RAW_FILEINFO_ACCESS_INFORMATION; \
	_q.access_information.in.file.handle = (_fh); \
	status = smb2_getinfo_file(tree, tctx, &_q); \
	CHECK_STATUS(status, NT_STATUS_OK); \
	if (_q.access_information.out.access_flags != (flags)) { \
		torture_result(tctx, TORTURE_FAIL, "(%s) Incorrect access_flags 0x%08x - should be 0x%08x\n", \
		       __location__, _q.access_information.out.access_flags, (flags)); \
		ret = false; \
		goto done; \
	} \
} while (0)

/*
 * Test creating a file with a NULL DACL.
 */
static bool test_create_null_dacl(struct torture_context *tctx,
    struct smb2_tree *tree)
{
	NTSTATUS status;
	struct smb2_create io;
	const char *fname = "nulldacl.txt";
	bool ret = true;
	struct smb2_handle handle;
	union smb_fileinfo q;
	union smb_setfileinfo s;
	struct security_descriptor *sd = security_descriptor_initialise(tctx);
	struct security_acl dacl;

	torture_comment(tctx, "TESTING SEC_DESC WITH A NULL DACL\n");

	smb2_util_unlink(tree, fname);

	ZERO_STRUCT(io);
	io.level = RAW_OPEN_SMB2;
	io.in.create_flags = 0;
	io.in.desired_access = SEC_STD_READ_CONTROL | SEC_STD_WRITE_DAC
		| SEC_STD_WRITE_OWNER;
	io.in.create_options = 0;
	io.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.in.share_access =
		NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.alloc_size = 0;
	io.in.create_disposition = NTCREATEX_DISP_CREATE;
	io.in.impersonation_level = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.in.security_flags = 0;
	io.in.fname = fname;
	io.in.sec_desc = sd;
	/* XXX create_options ? */
	io.in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;

	torture_comment(tctx, "creating a file with a empty sd\n");
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	handle = io.out.file.handle;

	torture_comment(tctx, "get the original sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.handle = handle;
	q.query_secdesc.in.secinfo_flags =
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb2_getinfo_file(tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	/*
	 * Testing the created DACL,
	 * the server should add the inherited DACL
	 * when SEC_DESC_DACL_PRESENT isn't specified
	 */
	if (!(q.query_secdesc.out.sd->type & SEC_DESC_DACL_PRESENT)) {
		ret = false;
		torture_fail_goto(tctx, done, "DACL_PRESENT flag not set by the server!\n");
	}
	if (q.query_secdesc.out.sd->dacl == NULL) {
		ret = false;
		torture_fail_goto(tctx, done, "no DACL has been created on the server!\n");
	}

	torture_comment(tctx, "set NULL DACL\n");
	sd->type |= SEC_DESC_DACL_PRESENT;

	s.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	s.set_secdesc.in.file.handle = handle;
	s.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	s.set_secdesc.in.sd = sd;
	status = smb2_setinfo_file(tree, &s);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "get the sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.handle = handle;
	q.query_secdesc.in.secinfo_flags =
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb2_getinfo_file(tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Testing the modified DACL */
	if (!(q.query_secdesc.out.sd->type & SEC_DESC_DACL_PRESENT)) {
		ret = false;
		torture_fail_goto(tctx, done, "DACL_PRESENT flag not set by the server!\n");
	}
	if (q.query_secdesc.out.sd->dacl != NULL) {
		ret = false;
		torture_fail_goto(tctx, done, "DACL has been created on the server!\n");
	}

	io.in.create_disposition = NTCREATEX_DISP_OPEN;

	torture_comment(tctx, "try open for read control\n");
	io.in.desired_access = SEC_STD_READ_CONTROL;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.out.file.handle,
		SEC_STD_READ_CONTROL);
	smb2_util_close(tree, io.out.file.handle);

	torture_comment(tctx, "try open for write\n");
	io.in.desired_access = SEC_FILE_WRITE_DATA;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.out.file.handle,
		SEC_FILE_WRITE_DATA);
	smb2_util_close(tree, io.out.file.handle);

	torture_comment(tctx, "try open for read\n");
	io.in.desired_access = SEC_FILE_READ_DATA;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.out.file.handle,
		SEC_FILE_READ_DATA);
	smb2_util_close(tree, io.out.file.handle);

	torture_comment(tctx, "try open for generic write\n");
	io.in.desired_access = SEC_GENERIC_WRITE;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.out.file.handle,
		SEC_RIGHTS_FILE_WRITE);
	smb2_util_close(tree, io.out.file.handle);

	torture_comment(tctx, "try open for generic read\n");
	io.in.desired_access = SEC_GENERIC_READ;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.out.file.handle,
		SEC_RIGHTS_FILE_READ);
	smb2_util_close(tree, io.out.file.handle);

	torture_comment(tctx, "set DACL with 0 aces\n");
	ZERO_STRUCT(dacl);
	dacl.revision = SECURITY_ACL_REVISION_NT4;
	dacl.num_aces = 0;
	sd->dacl = &dacl;

	s.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	s.set_secdesc.in.file.handle = handle;
	s.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	s.set_secdesc.in.sd = sd;
	status = smb2_setinfo_file(tree, &s);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "get the sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.handle = handle;
	q.query_secdesc.in.secinfo_flags =
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb2_getinfo_file(tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Testing the modified DACL */
	if (!(q.query_secdesc.out.sd->type & SEC_DESC_DACL_PRESENT)) {
		ret = false;
		torture_fail_goto(tctx, done, "DACL_PRESENT flag not set by the server!\n");
	}
	if (q.query_secdesc.out.sd->dacl == NULL) {
		ret = false;
		torture_fail_goto(tctx, done, "no DACL has been created on the server!\n");
	}
	if (q.query_secdesc.out.sd->dacl->num_aces != 0) {
		torture_result(tctx, TORTURE_FAIL, "DACL has %u aces!\n",
		       q.query_secdesc.out.sd->dacl->num_aces);
		ret = false;
		goto done;
	}

	torture_comment(tctx, "try open for read control\n");
	io.in.desired_access = SEC_STD_READ_CONTROL;
	status = smb2_create(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_ACCESS_FLAGS(io.out.file.handle,
		SEC_STD_READ_CONTROL);
	smb2_util_close(tree, io.out.file.handle);

	torture_comment(tctx, "try open for write => access_denied\n");
	io.in.desired_access = SEC_FILE_WRITE_DATA;
	status = smb2_create(tree, tctx, &io);
	if (torture_setting_bool(tctx, "hide_on_access_denied", false)) {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	torture_comment(tctx, "try open for read => access_denied\n");
	io.in.desired_access = SEC_FILE_READ_DATA;
	status = smb2_create(tree, tctx, &io);
	if (torture_setting_bool(tctx, "hide_on_access_denied", false)) {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	torture_comment(tctx, "try open for generic write => access_denied\n");
	io.in.desired_access = SEC_GENERIC_WRITE;
	status = smb2_create(tree, tctx, &io);
	if (torture_setting_bool(tctx, "hide_on_access_denied", false)) {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	torture_comment(tctx, "try open for generic read => access_denied\n");
	io.in.desired_access = SEC_GENERIC_READ;
	status = smb2_create(tree, tctx, &io);
	if (torture_setting_bool(tctx, "hide_on_access_denied", false)) {
		CHECK_STATUS(status, NT_STATUS_OBJECT_NAME_NOT_FOUND);
	} else {
		CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);
	}

	torture_comment(tctx, "set empty sd\n");
	sd->type &= ~SEC_DESC_DACL_PRESENT;
	sd->dacl = NULL;

	s.set_secdesc.level = RAW_SFILEINFO_SEC_DESC;
	s.set_secdesc.in.file.handle = handle;
	s.set_secdesc.in.secinfo_flags = SECINFO_DACL;
	s.set_secdesc.in.sd = sd;
	status = smb2_setinfo_file(tree, &s);
	CHECK_STATUS(status, NT_STATUS_OK);

	torture_comment(tctx, "get the sd\n");
	q.query_secdesc.level = RAW_FILEINFO_SEC_DESC;
	q.query_secdesc.in.file.handle = handle;
	q.query_secdesc.in.secinfo_flags =
		SECINFO_OWNER |
		SECINFO_GROUP |
		SECINFO_DACL;
	status = smb2_getinfo_file(tree, tctx, &q);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* Testing the modified DACL */
	if (!(q.query_secdesc.out.sd->type & SEC_DESC_DACL_PRESENT)) {
		ret = false;
		torture_fail_goto(tctx, done, "DACL_PRESENT flag not set by the server!\n");
	}
	if (q.query_secdesc.out.sd->dacl != NULL) {
		ret = false;
		torture_fail_goto(tctx, done, "DACL has been created on the server!\n");
	}
done:
	smb2_util_close(tree, handle);
	smb2_util_unlink(tree, fname);
	smb2_tdis(tree);
	smb2_logoff(tree->session);
	return ret;
}

/*
   basic testing of SMB2 read
*/
struct torture_suite *torture_smb2_create_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "create");

	torture_suite_add_1smb2_test(suite, "gentest", test_create_gentest);
	torture_suite_add_1smb2_test(suite, "blob", test_create_blob);
	torture_suite_add_1smb2_test(suite, "open", test_smb2_open);
	torture_suite_add_1smb2_test(suite, "brlocked", test_smb2_open_brlocked);
	torture_suite_add_1smb2_test(suite, "multi", test_smb2_open_multi);
	torture_suite_add_1smb2_test(suite, "delete", test_smb2_open_for_delete);
	torture_suite_add_1smb2_test(suite, "leading-slash", test_smb2_leading_slash);
	torture_suite_add_1smb2_test(suite, "aclfile", test_create_acl_file);
	torture_suite_add_1smb2_test(suite, "acldir", test_create_acl_dir);
	torture_suite_add_1smb2_test(suite, "nulldacl", test_create_null_dacl);

	suite->description = talloc_strdup(suite, "SMB2-CREATE tests");

	return suite;
}
