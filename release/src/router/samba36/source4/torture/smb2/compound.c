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
	uint32_t saved_tid = tree->tid;
	uint64_t saved_uid = tree->session->uid;

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

	tree->tid = 0xFFFFFFFF;
	tree->session->uid = UINT64_MAX;

	req[1] = smb2_close_send(tree, &cl);

	status = smb2_create_recv(req[0], tree, &cr);
	CHECK_STATUS(status, NT_STATUS_OK);
	status = smb2_close_recv(req[1], &cl);
	CHECK_STATUS(status, NT_STATUS_OK);

	tree->tid = saved_tid;
	tree->session->uid = saved_uid;

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
	uint32_t saved_tid = tree->tid;
	uint64_t saved_uid = tree->session->uid;

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
	tree->tid = 0xFFFFFFFF;
	tree->session->uid = UINT64_MAX;

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
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[4], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);

	tree->tid = saved_tid;
	tree->session->uid = saved_uid;

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
	struct smb2_request *req[3];

	smb2_transport_credits_ask_num(tree->session->transport, 3);

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

	smb2_transport_compound_start(tree->session->transport, 3);

	/* passing the first request with the related flag is invalid */
	smb2_transport_compound_set_related(tree->session->transport, true);

	req[0] = smb2_create_send(tree, &cr);

	hd.data[0] = UINT64_MAX;
	hd.data[1] = UINT64_MAX;

	ZERO_STRUCT(cl);
	cl.in.file.handle = hd;
	req[1] = smb2_close_send(tree, &cl);

	smb2_transport_compound_set_related(tree->session->transport, false);
	req[2] = smb2_close_send(tree, &cl);

	status = smb2_create_recv(req[0], tree, &cr);
	/* TODO: check why this fails with --signing=required */
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	status = smb2_close_recv(req[1], &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);
	status = smb2_close_recv(req[2], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);

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
	uint32_t saved_tid = tree->tid;
	uint64_t saved_uid = tree->session->uid;

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
	tree->tid = 0xFFFFFFFF;
	tree->session->uid = UINT64_MAX;

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
	CHECK_STATUS(status, NT_STATUS_USER_SESSION_DELETED);
	status = smb2_close_recv(req[3], &cl);
	CHECK_STATUS(status, NT_STATUS_USER_SESSION_DELETED);
	status = smb2_close_recv(req[4], &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_PARAMETER);

	tree->tid = saved_tid;
	tree->session->uid = saved_uid;

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
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);
	status = smb2_close_recv(req[4], &cl);
	CHECK_STATUS(status, NT_STATUS_FILE_CLOSED);

	smb2_util_unlink(tree, fname);
done:
	return ret;
}

/* Send a compound request where we expect the last request (Create, Notify)
 * to go asynchronous. This works against a Win7 server and the reply is
 * sent in two different packets. */
static bool test_compound_interim1(struct torture_context *tctx,
				   struct smb2_tree *tree)
{
    struct smb2_handle hd;
    struct smb2_create cr;
    NTSTATUS status = NT_STATUS_OK;
    const char *dname = "compound_interim_dir";
    struct smb2_notify nt;
    bool ret = true;
    struct smb2_request *req[2];

    /* Win7 compound request implementation deviates substantially from the
     * SMB2 spec as noted in MS-SMB2 <159>, <162>.  This, test currently
     * verifies the Windows behavior, not the general spec behavior. */

    smb2_transport_credits_ask_num(tree->session->transport, 5);

    smb2_deltree(tree, dname);

    smb2_transport_credits_ask_num(tree->session->transport, 1);

    ZERO_STRUCT(cr);
    cr.in.desired_access	= SEC_RIGHTS_FILE_ALL;
    cr.in.create_options	= NTCREATEX_OPTIONS_DIRECTORY;
    cr.in.file_attributes	= FILE_ATTRIBUTE_DIRECTORY;
    cr.in.share_access		= NTCREATEX_SHARE_ACCESS_READ |
				  NTCREATEX_SHARE_ACCESS_WRITE |
				  NTCREATEX_SHARE_ACCESS_DELETE;
    cr.in.create_disposition	= NTCREATEX_DISP_CREATE;
    cr.in.fname			= dname;

    smb2_transport_compound_start(tree->session->transport, 2);

    req[0] = smb2_create_send(tree, &cr);

    smb2_transport_compound_set_related(tree->session->transport, true);

    hd.data[0] = UINT64_MAX;
    hd.data[1] = UINT64_MAX;

    ZERO_STRUCT(nt);
    nt.in.recursive          = true;
    nt.in.buffer_size        = 0x1000;
    nt.in.file.handle        = hd;
    nt.in.completion_filter  = FILE_NOTIFY_CHANGE_NAME;
    nt.in.unknown            = 0x00000000;

    req[1] = smb2_notify_send(tree, &nt);

    status = smb2_create_recv(req[0], tree, &cr);
    CHECK_STATUS(status, NT_STATUS_OK);

    smb2_cancel(req[1]);
    status = smb2_notify_recv(req[1], tree, &nt);
    CHECK_STATUS(status, NT_STATUS_CANCELLED);

    smb2_util_close(tree, cr.out.file.handle);

    smb2_deltree(tree, dname);
done:
    return ret;
}

/* Send a compound request where we expect the middle request (Create, Notify,
 * GetInfo) to go asynchronous. Against Win7 the sync request succeed while
 * the async fails. All are returned in the same compound response. */
static bool test_compound_interim2(struct torture_context *tctx,
				   struct smb2_tree *tree)
{
    struct smb2_handle hd;
    struct smb2_create cr;
    NTSTATUS status = NT_STATUS_OK;
    const char *dname = "compound_interim_dir";
    struct smb2_getinfo gf;
    struct smb2_notify  nt;
    bool ret = true;
    struct smb2_request *req[3];

    /* Win7 compound request implementation deviates substantially from the
     * SMB2 spec as noted in MS-SMB2 <159>, <162>.  This, test currently
     * verifies the Windows behavior, not the general spec behavior. */

    smb2_transport_credits_ask_num(tree->session->transport, 5);

    smb2_deltree(tree, dname);

    smb2_transport_credits_ask_num(tree->session->transport, 1);

    ZERO_STRUCT(cr);
    cr.in.desired_access        = SEC_RIGHTS_FILE_ALL;
    cr.in.create_options        = NTCREATEX_OPTIONS_DIRECTORY;
    cr.in.file_attributes       = FILE_ATTRIBUTE_DIRECTORY;
    cr.in.share_access      = NTCREATEX_SHARE_ACCESS_READ |
                      NTCREATEX_SHARE_ACCESS_WRITE |
                      NTCREATEX_SHARE_ACCESS_DELETE;
    cr.in.create_disposition    = NTCREATEX_DISP_CREATE;
    cr.in.fname         = dname;

    smb2_transport_compound_start(tree->session->transport, 3);

    req[0] = smb2_create_send(tree, &cr);

    smb2_transport_compound_set_related(tree->session->transport, true);

    hd.data[0] = UINT64_MAX;
    hd.data[1] = UINT64_MAX;

    ZERO_STRUCT(nt);
    nt.in.recursive          = true;
    nt.in.buffer_size        = 0x1000;
    nt.in.file.handle        = hd;
    nt.in.completion_filter  = FILE_NOTIFY_CHANGE_NAME;
    nt.in.unknown            = 0x00000000;

    req[1] = smb2_notify_send(tree, &nt);

    ZERO_STRUCT(gf);
    gf.in.file.handle = hd;
    gf.in.info_type   = SMB2_GETINFO_FILE;
    gf.in.info_class  = 0x04; // FILE_BASIC_INFORMATION
    gf.in.output_buffer_length = 0x1000;
    gf.in.input_buffer_length = 0;

    req[2] = smb2_getinfo_send(tree, &gf);

    status = smb2_create_recv(req[0], tree, &cr);
    CHECK_STATUS(status, NT_STATUS_OK);

    status = smb2_notify_recv(req[1], tree, &nt);
    CHECK_STATUS(status, NT_STATUS_INTERNAL_ERROR);

    status = smb2_getinfo_recv(req[2], tree, &gf);
    CHECK_STATUS(status, NT_STATUS_OK);

    smb2_util_close(tree, cr.out.file.handle);

    smb2_deltree(tree, dname);
done:
    return ret;
}

struct torture_suite *torture_smb2_compound_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "compound");

	torture_suite_add_1smb2_test(suite, "related1", test_compound_related1);
	torture_suite_add_1smb2_test(suite, "related2", test_compound_related2);
	torture_suite_add_1smb2_test(suite, "unrelated1", test_compound_unrelated1);
	torture_suite_add_1smb2_test(suite, "invalid1", test_compound_invalid1);
	torture_suite_add_1smb2_test(suite, "invalid2", test_compound_invalid2);
	torture_suite_add_1smb2_test(suite, "invalid3", test_compound_invalid3);
	torture_suite_add_1smb2_test(suite, "interim1",  test_compound_interim1);
	torture_suite_add_1smb2_test(suite, "interim2",  test_compound_interim2);

	suite->description = talloc_strdup(suite, "SMB2-COMPOUND tests");

	return suite;
}
