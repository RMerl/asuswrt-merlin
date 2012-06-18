/*
   Unix SMB/CIFS implementation.

   test suite for SMB2 compounded requests

   Copyright (C) Stefan Metzmacher 2009

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
#include "librpc/gen_ndr/security.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "torture/torture.h"
#include "torture/smb2/proto.h"

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, __location__": Incorrect status %s - should be %s", \
		       nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

static bool test_compound_related1(struct torture_context *tctx,
				   struct smb2_tree *tree)
{
	struct smb2_handle hd;
	struct smb2_create cr;
	NTSTATUS status;
	const char *fname = "compound_related1.dat";
	struct smb2_close cl;
	bool ret = true;
	struct smb2_request *req[2];
	DATA_BLOB data;

	smb2_transport_credits_ask_num(tree->session->transport, 2);

	smb2_util_unlink(tree, fname);

	smb2_transport_credits_ask_num(tree->session->transport, 1);

	ZERO_STRUCT(cr);
	cr.in.security_flags		= 0x00;
	cr.in.oplock_level		= 0;
	cr.in.impersonation_level	= NTCREATEX_IMPERSONATION_IMPERSONATION;
	cr.in.create_flags		= 0x00000000;
	cr.in.reserved			= 0x00000000;
	cr.in.desired_access		= SEC_RIGHTS_FILE_ALL;
	cr.in.file_attributes		= FILE_ATTRIBUTE_NORMAL;
	cr.in.share_access		= NTCREATEX_SHARE_ACCESS_READ |
					  NTCREATEX_SHARE_ACCESS_WRITE |
					  NTCREATEX_SHARE_ACCESS_DELETE;
	cr.in.create_disposition	= NTCREATEX_DISP_OPEN_IF;
	cr.in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;
	cr.in.fname			= fname;

	smb2_transport_compound_start(tree->session->transport, 2);

	req[0] = smb2_create_send(tree, &cr);

	smb2_transport_compound_set_related(tree->session->transport, true);

	hd.data[0] = UINT64_MAX;
	hd.data[1] = UINT64_MAX;

	ZERO_STRUCT(cl);
	cl.in.file.handle = hd;
	req[1] = smb2_close_send(tree, &cl);

	status = smb2_create_recv(req[0], tree, &cr);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_close_recv(req[1], &cl);
	CHECK_STATUS(status, NT_STATUS_OK);

	smb2_util_unlink(tree, fname);
done:
	return ret;
}

static bool test_compound_related2(struct torture_context *tctx,
				   struct smb2_tree *tree)
{
	struct smb2_handle hd;
	struct smb2_create cr;
	NTSTATUS status;
	const char *fname = "compound_related2.dat";
	struct smb2_close cl;
	bool ret = true;
	struct smb2_request *req[5];

	smb2_transport_credits_ask_num(tree->session->transport, 5);

	smb2_util_unlink(tree, fname);

	smb2_transport_credits_ask_num(tree->session->transport, 1);

	ZERO_STRUCT(cr);
	cr.in.security_flags		= 0x00;
	cr.in.oplock_level		= 0;
	cr.in.impersonation_level	= NTCREATEX_IMPERSONATION_IMPERSONATION;
	cr.in.create_flags		= 0x00000000;
	cr.in.reserved			= 0x00000000;
	cr.in.desired_access		= SEC_RIGHTS_FILE_ALL;
	cr.in.file_attributes		= FILE_ATTRIBUTE_NORMAL;
	cr.in.share_access		= NTCREATEX_SHARE_ACCESS_READ |
					  NTCREATEX_SHARE_ACCESS_WRITE |
					  NTCREATEX_SHARE_ACCESS_DELETE;
	cr.in.create_disposition	= NTCREATEX_DISP_OPEN_IF;
	cr.in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;
	cr.in.fname			= fname;

	smb2_transport_compound_start(tree->session->transport, 5);

	req[0] = smb2_create_send(tree, &cr);

	hd.data[0] = UINT64_MAX;
	hd.data[1] = UINT64_MAX;

	smb2_transport_compound_set_related(tree->session->transport, true);

	ZERO_STRUCT(cl);
	cl.in.file.handle = hd;
	req[1] = smb2_close_send(tree, &cl);
	req[2] = smb2_close_send(tree, &cl);
	req[3] = smb2_close_send(tree, &cl);
	req[4] = smb2_close_send(tree, &cl);

	status = smb2_create_recv(req[0], tree, &cr);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_close_recv(req[1], &cl);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_close_recv(req[2], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[3], &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	status = smb2_close_recv(req[4], &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	smb2_util_unlink(tree, fname);
done:
	return ret;
}

static bool test_compound_unrelated1(struct torture_context *tctx,
				     struct smb2_tree *tree)
{
	struct smb2_handle hd;
	struct smb2_create cr;
	NTSTATUS status;
	const char *fname = "compound_unrelated1.dat";
	struct smb2_close cl;
	bool ret = true;
	struct smb2_request *req[5];
	uint64_t uid;
	uint32_t tid;

	smb2_transport_credits_ask_num(tree->session->transport, 5);

	smb2_util_unlink(tree, fname);

	smb2_transport_credits_ask_num(tree->session->transport, 1);

	ZERO_STRUCT(cr);
	cr.in.security_flags		= 0x00;
	cr.in.oplock_level		= 0;
	cr.in.impersonation_level	= NTCREATEX_IMPERSONATION_IMPERSONATION;
	cr.in.create_flags		= 0x00000000;
	cr.in.reserved			= 0x00000000;
	cr.in.desired_access		= SEC_RIGHTS_FILE_ALL;
	cr.in.file_attributes		= FILE_ATTRIBUTE_NORMAL;
	cr.in.share_access		= NTCREATEX_SHARE_ACCESS_READ |
					  NTCREATEX_SHARE_ACCESS_WRITE |
					  NTCREATEX_SHARE_ACCESS_DELETE;
	cr.in.create_disposition	= NTCREATEX_DISP_OPEN_IF;
	cr.in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;
	cr.in.fname			= fname;

	smb2_transport_compound_start(tree->session->transport, 5);

	req[0] = smb2_create_send(tree, &cr);

	hd.data[0] = UINT64_MAX;
	hd.data[1] = UINT64_MAX;

	ZERO_STRUCT(cl);
	cl.in.file.handle = hd;
	req[1] = smb2_close_send(tree, &cl);
	req[2] = smb2_close_send(tree, &cl);
	req[3] = smb2_close_send(tree, &cl);
	req[4] = smb2_close_send(tree, &cl);

	status = smb2_create_recv(req[0], tree, &cr);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_close_recv(req[1], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[2], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[3], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[4], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);

	smb2_util_unlink(tree, fname);
done:
	return ret;
}

static bool test_compound_invalid1(struct torture_context *tctx,
				   struct smb2_tree *tree)
{
	struct smb2_handle hd;
	struct smb2_create cr;
	NTSTATUS status;
	const char *fname = "compound_invalid1.dat";
	struct smb2_close cl;
	bool ret = true;
	struct smb2_request *req[2];
	DATA_BLOB data;

	smb2_transport_credits_ask_num(tree->session->transport, 2);

	smb2_util_unlink(tree, fname);

	smb2_transport_credits_ask_num(tree->session->transport, 1);

	ZERO_STRUCT(cr);
	cr.in.security_flags		= 0x00;
	cr.in.oplock_level		= 0;
	cr.in.impersonation_level	= NTCREATEX_IMPERSONATION_IMPERSONATION;
	cr.in.create_flags		= 0x00000000;
	cr.in.reserved			= 0x00000000;
	cr.in.desired_access		= SEC_RIGHTS_FILE_ALL;
	cr.in.file_attributes		= FILE_ATTRIBUTE_NORMAL;
	cr.in.share_access		= NTCREATEX_SHARE_ACCESS_READ |
					  NTCREATEX_SHARE_ACCESS_WRITE |
					  NTCREATEX_SHARE_ACCESS_DELETE;
	cr.in.create_disposition	= NTCREATEX_DISP_OPEN_IF;
	cr.in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;
	cr.in.fname			= fname;

	smb2_transport_compound_start(tree->session->transport, 2);

	/* passing the first request with the related flag is invalid */
	smb2_transport_compound_set_related(tree->session->transport, true);

	req[0] = smb2_create_send(tree, &cr);

	hd.data[0] = UINT64_MAX;
	hd.data[1] = UINT64_MAX;

	ZERO_STRUCT(cl);
	cl.in.file.handle = hd;
	req[1] = smb2_close_send(tree, &cl);

	status = smb2_create_recv(req[0], tree, &cr);
	/* TODO: check why this fails with --signing=required */
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	status = smb2_close_recv(req[1], &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	smb2_util_unlink(tree, fname);
done:
	return ret;
}

static bool test_compound_invalid2(struct torture_context *tctx,
				   struct smb2_tree *tree)
{
	struct smb2_handle hd;
	struct smb2_create cr;
	NTSTATUS status;
	const char *fname = "compound_invalid2.dat";
	struct smb2_close cl;
	bool ret = true;
	struct smb2_request *req[5];

	smb2_transport_credits_ask_num(tree->session->transport, 5);

	smb2_util_unlink(tree, fname);

	smb2_transport_credits_ask_num(tree->session->transport, 1);

	ZERO_STRUCT(cr);
	cr.in.security_flags		= 0x00;
	cr.in.oplock_level		= 0;
	cr.in.impersonation_level	= NTCREATEX_IMPERSONATION_IMPERSONATION;
	cr.in.create_flags		= 0x00000000;
	cr.in.reserved			= 0x00000000;
	cr.in.desired_access		= SEC_RIGHTS_FILE_ALL;
	cr.in.file_attributes		= FILE_ATTRIBUTE_NORMAL;
	cr.in.share_access		= NTCREATEX_SHARE_ACCESS_READ |
					  NTCREATEX_SHARE_ACCESS_WRITE |
					  NTCREATEX_SHARE_ACCESS_DELETE;
	cr.in.create_disposition	= NTCREATEX_DISP_OPEN_IF;
	cr.in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;
	cr.in.fname			= fname;

	smb2_transport_compound_start(tree->session->transport, 5);

	req[0] = smb2_create_send(tree, &cr);

	hd.data[0] = UINT64_MAX;
	hd.data[1] = UINT64_MAX;

	smb2_transport_compound_set_related(tree->session->transport, true);

	ZERO_STRUCT(cl);
	cl.in.file.handle = hd;
	req[1] = smb2_close_send(tree, &cl);
	/* strange that this is not generating invalid parameter */
	smb2_transport_compound_set_related(tree->session->transport, false);
	req[2] = smb2_close_send(tree, &cl);
	req[3] = smb2_close_send(tree, &cl);
	smb2_transport_compound_set_related(tree->session->transport, true);
	req[4] = smb2_close_send(tree, &cl);

	status = smb2_create_recv(req[0], tree, &cr);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_close_recv(req[1], &cl);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_close_recv(req[2], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[3], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[4], &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	smb2_util_unlink(tree, fname);
done:
	return ret;
}

static bool test_compound_invalid3(struct torture_context *tctx,
				   struct smb2_tree *tree)
{
	struct smb2_handle hd;
	struct smb2_create cr;
	NTSTATUS status;
	const char *fname = "compound_invalid3.dat";
	struct smb2_close cl;
	bool ret = true;
	struct smb2_request *req[5];

	smb2_transport_credits_ask_num(tree->session->transport, 5);

	smb2_util_unlink(tree, fname);

	smb2_transport_credits_ask_num(tree->session->transport, 1);

	ZERO_STRUCT(cr);
	cr.in.security_flags		= 0x00;
	cr.in.oplock_level		= 0;
	cr.in.impersonation_level	= NTCREATEX_IMPERSONATION_IMPERSONATION;
	cr.in.create_flags		= 0x00000000;
	cr.in.reserved			= 0x00000000;
	cr.in.desired_access		= SEC_RIGHTS_FILE_ALL;
	cr.in.file_attributes		= FILE_ATTRIBUTE_NORMAL;
	cr.in.share_access		= NTCREATEX_SHARE_ACCESS_READ |
					  NTCREATEX_SHARE_ACCESS_WRITE |
					  NTCREATEX_SHARE_ACCESS_DELETE;
	cr.in.create_disposition	= NTCREATEX_DISP_OPEN_IF;
	cr.in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;
	cr.in.fname			= fname;

	smb2_transport_compound_start(tree->session->transport, 5);

	req[0] = smb2_create_send(tree, &cr);

	hd.data[0] = UINT64_MAX;
	hd.data[1] = UINT64_MAX;

	ZERO_STRUCT(cl);
	cl.in.file.handle = hd;
	req[1] = smb2_close_send(tree, &cl);
	req[2] = smb2_close_send(tree, &cl);
	/* flipping the related flag is invalid */
	smb2_transport_compound_set_related(tree->session->transport, true);
	req[3] = smb2_close_send(tree, &cl);
	req[4] = smb2_close_send(tree, &cl);

	status = smb2_create_recv(req[0], tree, &cr);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_close_recv(req[1], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[2], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[3], &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	status = smb2_close_recv(req[4], &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	smb2_util_unlink(tree, fname);
done:
	return ret;
}

struct torture_suite *torture_smb2_compound_init(void)
{
	struct torture_suite *suite =
	    torture_suite_create(talloc_autofree_context(), "COMPOUND");

	torture_suite_add_1smb2_test(suite, "RELATED1", test_compound_related1);
	torture_suite_add_1smb2_test(suite, "RELATED2", test_compound_related2);
	torture_suite_add_1smb2_test(suite, "UNRELATED1", test_compound_unrelated1);
	torture_suite_add_1smb2_test(suite, "INVALID1", test_compound_invalid1);
	torture_suite_add_1smb2_test(suite, "INVALID2", test_compound_invalid2);
	torture_suite_add_1smb2_test(suite, "INVALID3", test_compound_invalid3);

	suite->description = talloc_strdup(suite, "SMB2-COMPOUND tests");

	return suite;
}
