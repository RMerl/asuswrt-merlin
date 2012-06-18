/*
   Unix SMB/CIFS implementation.

   test suite for SMB2 oplocks

   Copyright (C) Stefan Metzmacher 2008

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

#define CHECK_VAL(v, correct) do { \
	if ((v) != (correct)) { \
		torture_result(tctx, TORTURE_FAIL, "(%s): wrong value for %s got 0x%x - should be 0x%x\n", \
				__location__, #v, (int)v, (int)correct); \
		ret = false; \
	}} while (0)

#define CHECK_STATUS(status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, __location__": Incorrect status %s - should be %s", \
		       nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)

static struct {
	struct smb2_handle handle;
	uint8_t level;
	struct smb2_break br;
	int count;
	int failures;
} break_info;

static void torture_oplock_break_callback(struct smb2_request *req)
{
	NTSTATUS status;
	struct smb2_break br;

	ZERO_STRUCT(br);
	status = smb2_break_recv(req, &break_info.br);
	if (!NT_STATUS_IS_OK(status)) {
		break_info.failures++;
	}

	return;
}

/* a oplock break request handler */
static bool torture_oplock_handler(struct smb2_transport *transport,
				   const struct smb2_handle *handle,
				   uint8_t level, void *private_data)
{
	struct smb2_tree *tree = private_data;
	const char *name;
	struct smb2_request *req;

	break_info.handle	= *handle;
	break_info.level	= level;
	break_info.count++;

	switch (level) {
	case SMB2_OPLOCK_LEVEL_II:
		name = "level II";
		break;
	case SMB2_OPLOCK_LEVEL_NONE:
		name = "none";
		break;
	default:
		name = "unknown";
		break_info.failures++;
	}
	printf("Acking to %s [0x%02X] in oplock handler\n",
		name, level);

	ZERO_STRUCT(break_info.br);
	break_info.br.in.file.handle	= *handle;
	break_info.br.in.oplock_level	= level;
	break_info.br.in.reserved	= 0;
	break_info.br.in.reserved2	= 0;

	req = smb2_break_send(tree, &break_info.br);
	req->async.fn = torture_oplock_break_callback;
	req->async.private_data = NULL;

	return true;
}

bool torture_smb2_oplock_batch1(struct torture_context *tctx,
				struct smb2_tree *tree)
{
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	struct smb2_handle h1, h2;
	struct smb2_create io;
	NTSTATUS status;
	const char *fname = "oplock.dat";
	bool ret = true;

	tree->session->transport->oplock.handler	= torture_oplock_handler;
	tree->session->transport->oplock.private_data	= tree;

	smb2_util_unlink(tree, fname);

	ZERO_STRUCT(break_info);

	ZERO_STRUCT(io);
	io.in.security_flags		= 0x00;
	io.in.oplock_level		= SMB2_OPLOCK_LEVEL_BATCH;
	io.in.impersonation_level	= NTCREATEX_IMPERSONATION_IMPERSONATION;
	io.in.create_flags		= 0x00000000;
	io.in.reserved			= 0x00000000;
	io.in.desired_access		= SEC_RIGHTS_FILE_ALL;
	io.in.file_attributes		= FILE_ATTRIBUTE_NORMAL;
	io.in.share_access		= NTCREATEX_SHARE_ACCESS_READ |
					  NTCREATEX_SHARE_ACCESS_WRITE |
					  NTCREATEX_SHARE_ACCESS_DELETE;
	io.in.create_disposition	= NTCREATEX_DISP_OPEN_IF;
	io.in.create_options		= NTCREATEX_OPTIONS_SEQUENTIAL_ONLY |
					  NTCREATEX_OPTIONS_ASYNC_ALERT	|
					  NTCREATEX_OPTIONS_NON_DIRECTORY_FILE |
					  0x00200000;
	io.in.fname			= fname;

	status = smb2_create(tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(io.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);
	/*CHECK_VAL(io.out.reserved, 0);*/
	CHECK_VAL(io.out.create_action, NTCREATEX_ACTION_CREATED);
	CHECK_VAL(io.out.alloc_size, 0);
	CHECK_VAL(io.out.size, 0);
	CHECK_VAL(io.out.file_attr, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_VAL(io.out.reserved2, 0);
	CHECK_VAL(break_info.count, 0);

	h1 = io.out.file.handle;

	ZERO_STRUCT(io.in.blobs);
	status = smb2_create(tree, mem_ctx, &io);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VAL(io.out.oplock_level, SMB2_OPLOCK_LEVEL_II);
	/*CHECK_VAL(io.out.reserved, 0);*/
	CHECK_VAL(io.out.create_action, NTCREATEX_ACTION_EXISTED);
	CHECK_VAL(io.out.alloc_size, 0);
	CHECK_VAL(io.out.size, 0);
	CHECK_VAL(io.out.file_attr, FILE_ATTRIBUTE_ARCHIVE);
	CHECK_VAL(io.out.reserved2, 0);

	h2 = io.out.file.handle;

done:
	talloc_free(mem_ctx);

	smb2_util_close(tree, h1);
	smb2_util_close(tree, h2);
	smb2_util_unlink(tree, fname);
	return ret;
}
