/*
   Unix SMB/CIFS implementation.

   test suite for SMB2 oplocks

   Copyright (C) Andrew Tridgell 2003
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

#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/resolve/resolve.h"

#include "lib/cmdline/popt_common.h"
#include "lib/events/events.h"

#include "param/param.h"
#include "system/filesys.h"

#include "torture/torture.h"
#include "torture/smb2/proto.h"

#define CHECK_RANGE(v, min, max) do { \
	if ((v) < (min) || (v) > (max)) { \
		torture_result(tctx, TORTURE_FAIL, "(%s): wrong value for %s " \
			       "got %d - should be between %d and %d\n", \
				__location__, #v, (int)v, (int)min, (int)max); \
		ret = false; \
	}} while (0)

#define CHECK_STRMATCH(v, correct) do { \
	if (!v || strstr((v),(correct)) == NULL) { \
		torture_result(tctx, TORTURE_FAIL,  "(%s): wrong value for %s "\
			       "got '%s' - should be '%s'\n", \
				__location__, #v, v?v:"NULL", correct); \
		ret = false; \
	}} while (0)

#define CHECK_VAL(v, correct) do { \
	if ((v) != (correct)) { \
		torture_result(tctx, TORTURE_FAIL, "(%s): wrong value for %s " \
			       "got 0x%x - should be 0x%x\n", \
				__location__, #v, (int)v, (int)correct); \
		ret = false; \
	}} while (0)

#define BASEDIR "oplock_test"

static struct {
	struct smb2_handle handle;
	uint8_t level;
	struct smb2_break br;
	int count;
	int failures;
	NTSTATUS failure_status;
} break_info;

static void torture_oplock_break_callback(struct smb2_request *req)
{
	NTSTATUS status;
	struct smb2_break br;

	ZERO_STRUCT(br);
	status = smb2_break_recv(req, &break_info.br);
	if (!NT_STATUS_IS_OK(status)) {
		break_info.failures++;
		break_info.failure_status = status;
	}

	return;
}

/* A general oplock break notification handler.  This should be used when a
 * test expects to break from batch or exclusive to a lower level. */
static bool torture_oplock_handler(struct smb2_transport *transport,
				   const struct smb2_handle *handle,
				   uint8_t level,
				   void *private_data)
{
	struct smb2_tree *tree = private_data;
	const char *name;
	struct smb2_request *req;
	ZERO_STRUCT(break_info.br);

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
	printf("Acking to %s [0x%02X] in oplock handler\n", name, level);

	break_info.br.in.file.handle	= *handle;
	break_info.br.in.oplock_level	= level;
	break_info.br.in.reserved	= 0;
	break_info.br.in.reserved2	= 0;

	req = smb2_break_send(tree, &break_info.br);
	req->async.fn = torture_oplock_break_callback;
	req->async.private_data = NULL;
	return true;
}

/*
  A handler function for oplock break notifications. Send a break to none
  request.
*/
static bool torture_oplock_handler_ack_to_none(struct smb2_transport *transport,
					       const struct smb2_handle *handle,
					       uint8_t level,
					       void *private_data)
{
	struct smb2_tree *tree = private_data;
	struct smb2_request *req;

	break_info.handle = *handle;
	break_info.level = level;
	break_info.count++;

	printf("Acking to none in oplock handler\n");

	ZERO_STRUCT(break_info.br);
	break_info.br.in.file.handle    = *handle;
	break_info.br.in.oplock_level   = SMB2_OPLOCK_LEVEL_NONE;
	break_info.br.in.reserved       = 0;
	break_info.br.in.reserved2      = 0;

	req = smb2_break_send(tree, &break_info.br);
	req->async.fn = torture_oplock_break_callback;
	req->async.private_data = NULL;

	return true;
}

/*
  A handler function for oplock break notifications. Break from level II to
  none.  SMB2 requires that the client does not send an oplock break request to
  the server in this case.
*/
static bool torture_oplock_handler_level2_to_none(
					       struct smb2_transport *transport,
					       const struct smb2_handle *handle,
					       uint8_t level,
					       void *private_data)
{
	break_info.handle = *handle;
	break_info.level = level;
	break_info.count++;

	printf("Break from level II to none in oplock handler\n");

	return true;
}

/* A handler function for oplock break notifications.  This should be used when
 * test expects two break notifications, first to level II, then to none. */
static bool torture_oplock_handler_two_notifications(
					struct smb2_transport *transport,
					const struct smb2_handle *handle,
					uint8_t level,
					void *private_data)
{
	struct smb2_tree *tree = private_data;
	const char *name;
	struct smb2_request *req;
	ZERO_STRUCT(break_info.br);

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
	printf("Breaking to %s [0x%02X] in oplock handler\n", name, level);

	if (level == SMB2_OPLOCK_LEVEL_NONE)
		return true;

	break_info.br.in.file.handle	= *handle;
	break_info.br.in.oplock_level	= level;
	break_info.br.in.reserved	= 0;
	break_info.br.in.reserved2	= 0;

	req = smb2_break_send(tree, &break_info.br);
	req->async.fn = torture_oplock_break_callback;
	req->async.private_data = NULL;
	return true;
}
static void torture_oplock_handler_close_recv(struct smb2_request *req)
{
	if (!smb2_request_receive(req)) {
		printf("close failed in oplock_handler_close\n");
		break_info.failures++;
	}
}

/*
  a handler function for oplock break requests - close the file
*/
static bool torture_oplock_handler_close(struct smb2_transport *transport,
					 const struct smb2_handle *handle,
					 uint8_t level,
					 void *private_data)
{
	struct smb2_close io;
	struct smb2_tree *tree = private_data;
	struct smb2_request *req;

	break_info.handle = *handle;
	break_info.level = level;
	break_info.count++;

	ZERO_STRUCT(io);
	io.in.file.handle       = *handle;
	io.in.flags	     = RAW_CLOSE_SMB2;
	req = smb2_close_send(tree, &io);
	if (req == NULL) {
		printf("failed to send close in oplock_handler_close\n");
		return false;
	}

	req->async.fn = torture_oplock_handler_close_recv;
	req->async.private_data = NULL;

	return true;
}

/*
  a handler function for oplock break requests. Let it timeout
*/
static bool torture_oplock_handler_timeout(struct smb2_transport *transport,
					   const struct smb2_handle *handle,
					   uint8_t level,
					   void *private_data)
{
	break_info.handle = *handle;
	break_info.level = level;
	break_info.count++;

	printf("Let oplock break timeout\n");
	return true;
}

static bool open_smb2_connection_no_level2_oplocks(struct torture_context *tctx,
						   struct smb2_tree **tree)
{
	NTSTATUS status;
	const char *host = torture_setting_string(tctx, "host", NULL);
	const char *share = torture_setting_string(tctx, "share", NULL);
	struct cli_credentials *credentials = cmdline_credentials;
	struct smbcli_options options;

	lpcfg_smbcli_options(tctx->lp_ctx, &options);
	options.use_level2_oplocks = false;

	status = smb2_connect(tctx, host,
			      lpcfg_smb_ports(tctx->lp_ctx), share,
			      lpcfg_resolve_context(tctx->lp_ctx),
			      credentials, tree, tctx->ev, &options,
			      lpcfg_socket_options(tctx->lp_ctx),
			      lpcfg_gensec_settings(tctx, tctx->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "Failed to connect to SMB2 share "
				"\\\\%s\\%s - %s\n", host, share,
				nt_errstr(status));
		return false;
	}
	return true;
}

/*
   Timer handler function notifies the registering function that time is up
*/
static void timeout_cb(struct tevent_context *ev,
		       struct tevent_timer *te,
		       struct timeval current_time,
		       void *private_data)
{
	bool *timesup = (bool *)private_data;
	*timesup = true;
	return;
}

/*
   Wait a short period of time to receive a single oplock break request
*/
static void torture_wait_for_oplock_break(struct torture_context *tctx)
{
	TALLOC_CTX *tmp_ctx = talloc_new(NULL);
	struct tevent_timer *te = NULL;
	struct timeval ne;
	bool timesup = false;
	int old_count = break_info.count;

	/* Wait .1 seconds for an oplock break */
	ne = tevent_timeval_current_ofs(0, 100000);

	if ((te = event_add_timed(tctx->ev, tmp_ctx, ne, timeout_cb, &timesup))
	    == NULL)
	{
		torture_comment(tctx, "Failed to wait for an oplock break. "
				      "test results may not be accurate.");
		goto done;
	}

	while (!timesup && break_info.count < old_count + 1) {
		if (event_loop_once(tctx->ev) != 0) {
			torture_comment(tctx, "Failed to wait for an oplock "
					      "break. test results may not be "
					      "accurate.");
			goto done;
		}
	}

done:
	/* We don't know if the timed event fired and was freed, we received
	 * our oplock break, or some other event triggered the loop.  Thus,
	 * we create a tmp_ctx to be able to safely free/remove the timed
	 * event in all 3 cases. */
	talloc_free(tmp_ctx);

	return;
}

static bool test_smb2_oplock_exclusive1(struct torture_context *tctx,
					struct smb2_tree *tree1,
					struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_exclusive1.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_unlink unl;
	struct smb2_handle h1;
	struct smb2_handle h;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE1: open a file with an exclusive "
			"oplock (share mode: none)\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;

	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_EXCLUSIVE);

	torture_comment(tctx, "a 2nd open should not cause a break\n");
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	torture_comment(tctx, "unlink it - should also be no break\n");
	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	status = smb2_util_unlink(tree2, fname);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_exclusive2(struct torture_context *tctx,
					struct smb2_tree *tree1,
					struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_exclusive2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_unlink unl;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE2: open a file with an exclusive "
			"oplock (share mode: all)\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;

	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_EXCLUSIVE);

	torture_comment(tctx, "a 2nd open should cause a break to level 2\n");
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h2 = io.smb2.out.file.handle;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);
	ZERO_STRUCT(break_info);

	/* now we have 2 level II oplocks... */
	torture_comment(tctx, "try to unlink it - should cause a break\n");
	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	status = smb2_util_unlink(tree2, fname);
	torture_assert_ntstatus_ok(tctx, status, "Error unlinking the file");
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	torture_comment(tctx, "close both handles\n");
	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_exclusive3(struct torture_context *tctx,
					struct smb2_tree *tree1,
					struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_exclusive3.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfi;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE3: open a file with an exclusive "
			"oplock (share mode: none)\n");

	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;

	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_EXCLUSIVE);

	torture_comment(tctx, "setpathinfo EOF should trigger a break to "
			"none\n");
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFORMATION;
	sfi.generic.in.file.path = fname;
	sfi.end_of_file_info.in.size = 100;

	status = smb2_composite_setpathinfo(tree2, &sfi);

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_NONE);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_exclusive4(struct torture_context *tctx,
					struct smb2_tree *tree1,
					struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_exclusive4.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE4: open with exclusive oplock\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_EXCLUSIVE);

	ZERO_STRUCT(break_info);
	torture_comment(tctx, "second open with attributes only shouldn't "
			"cause oplock break\n");

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.desired_access = SEC_FILE_READ_ATTRIBUTE |
				SEC_FILE_WRITE_ATTRIBUTE |
				SEC_STD_SYNCHRONIZE;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, NO_OPLOCK_RETURN);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_exclusive5(struct torture_context *tctx,
					struct smb2_tree *tree1,
					struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_exclusive5.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	tree2->session->transport->oplock.handler = torture_oplock_handler;
	tree2->session->transport->oplock.private_data = tree2;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE5: open with exclusive oplock\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_EXCLUSIVE);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "second open with attributes only and "
			"NTCREATEX_DISP_OVERWRITE_IF dispostion causes "
			"oplock break\n");

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.desired_access = SEC_FILE_READ_ATTRIBUTE |
				SEC_FILE_WRITE_ATTRIBUTE |
				SEC_STD_SYNCHRONIZE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_II;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_exclusive6(struct torture_context *tctx,
					struct smb2_tree *tree1,
					struct smb2_tree *tree2)
{
	const char *fname1 = BASEDIR "\\test_exclusive6_1.dat";
	const char *fname2 = BASEDIR "\\test_exclusive6_2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sinfo;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname1);
	smb2_util_unlink(tree2, fname2);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname1;

	torture_comment(tctx, "EXCLUSIVE6: open a file with an exclusive "
			"oplock (share mode: none)\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;

	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_EXCLUSIVE);

	torture_comment(tctx, "rename should not generate a break but get "
			"a sharing violation\n");
	ZERO_STRUCT(sinfo);
	sinfo.rename_information.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sinfo.rename_information.in.file.handle = h1;
	sinfo.rename_information.in.overwrite = true;
	sinfo.rename_information.in.new_name = fname2;
	status = smb2_setinfo_file(tree1, &sinfo);

	torture_comment(tctx, "trying rename while first file open\n");
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch1(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch1.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1;
	char c = 0;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "BATCH1: open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	torture_comment(tctx, "unlink should generate a break\n");
	status = smb2_util_unlink(tree2, fname);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	torture_comment(tctx, "2nd unlink should not generate a break\n");
	ZERO_STRUCT(break_info);
	status = smb2_util_unlink(tree2, fname);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	torture_comment(tctx, "writing should generate a self break to none\n");
	tree1->session->transport->oplock.handler =
	    torture_oplock_handler_level2_to_none;
	smb2_util_write(tree1, h1, &c, 0, 1);

	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_NONE);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch2(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	char c = 0;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH2: open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	torture_comment(tctx, "unlink should generate a break, which we ack "
			"as break to none\n");
	tree1->session->transport->oplock.handler =
				torture_oplock_handler_ack_to_none;
	tree1->session->transport->oplock.private_data = tree1;
	status = smb2_util_unlink(tree2, fname);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				     "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	torture_comment(tctx, "2nd unlink should not generate a break\n");
	ZERO_STRUCT(break_info);
	status = smb2_util_unlink(tree2, fname);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	torture_comment(tctx, "writing should not generate a break\n");
	smb2_util_write(tree1, h1, &c, 0, 1);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch3(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch3.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);
	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH3: if we close on break then the unlink "
			"can succeed\n");
	ZERO_STRUCT(break_info);
	tree1->session->transport->oplock.handler =
					torture_oplock_handler_close;
	tree1->session->transport->oplock.private_data = tree1;

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);
	status = smb2_util_unlink(tree2, fname);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.level, 1);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch4(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch4.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_read r;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH4: a self read should not cause a break\n");
	ZERO_STRUCT(break_info);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(r);
	r.in.file.handle = h1;
	r.in.offset      = 0;

	status = smb2_read(tree1, tree1, &r);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch5(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch5.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH5: a 2nd open should give a break\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.level, 1);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch6(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch6.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;
	char c = 0;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH6: a 2nd open should give a break to "
			"level II if the first open allowed shared read\n");
	ZERO_STRUCT(break_info);
	tree2->session->transport->oplock.handler = torture_oplock_handler;
	tree2->session->transport->oplock.private_data = tree2;

	io.smb2.in.desired_access = SEC_RIGHTS_FILE_READ |
				SEC_RIGHTS_FILE_WRITE;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				NTCREATEX_SHARE_ACCESS_WRITE;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);

	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.level, 1);
	CHECK_VAL(break_info.failures, 0);
	ZERO_STRUCT(break_info);

	torture_comment(tctx, "write should trigger a break to none on both\n");
	tree1->session->transport->oplock.handler =
	    torture_oplock_handler_level2_to_none;
	tree2->session->transport->oplock.handler =
	    torture_oplock_handler_level2_to_none;
	smb2_util_write(tree1, h1, &c, 0, 1);

	/* We expect two breaks */
	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.count, 2);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch7(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch7.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH7: a 2nd open should get an oplock when "
			"we close instead of ack\n");
	ZERO_STRUCT(break_info);
	tree1->session->transport->oplock.handler =
			torture_oplock_handler_close;
	tree1->session->transport->oplock.private_data = tree1;

	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h2.data[0]);
	CHECK_VAL(break_info.level, 1);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree2, h1);
	smb2_util_close(tree2, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch8(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch8.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH8: open with batch oplock\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);
	torture_comment(tctx, "second open with attributes only shouldn't "
			"cause oplock break\n");

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.desired_access = SEC_FILE_READ_ATTRIBUTE |
				SEC_FILE_WRITE_ATTRIBUTE |
				SEC_STD_SYNCHRONIZE;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_NONE);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch9(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch9.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;
	char c = 0;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH9: open with attributes only can create "
			"file\n");

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_FILE_READ_ATTRIBUTE |
				SEC_FILE_WRITE_ATTRIBUTE |
				SEC_STD_SYNCHRONIZE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	torture_comment(tctx, "Subsequent normal open should break oplock on "
			"attribute only open to level II\n");

	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);
	smb2_util_close(tree2, h2);

	torture_comment(tctx, "third oplocked open should grant level2 without "
			"break\n");
	ZERO_STRUCT(break_info);

	tree2->session->transport->oplock.handler = torture_oplock_handler;
	tree2->session->transport->oplock.private_data = tree2;

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "write should trigger a break to none on both\n");
	tree1->session->transport->oplock.handler =
	    torture_oplock_handler_level2_to_none;
	tree2->session->transport->oplock.handler =
	    torture_oplock_handler_level2_to_none;
	smb2_util_write(tree2, h2, &c, 0, 1);

	/* We expect two breaks */
	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.count, 2);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch10(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch10.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH10: Open with oplock after a non-oplock "
			"open should grant level2\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.smb2.out.oplock_level, 0);

	tree2->session->transport->oplock.handler =
	    torture_oplock_handler_level2_to_none;
	tree2->session->transport->oplock.private_data = tree2;

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	torture_comment(tctx, "write should trigger a break to none\n");
	{
		struct smb2_write wr;
		DATA_BLOB data;
		data = data_blob_talloc(tree1, NULL, UINT16_MAX);
		data.data[0] = (const uint8_t)'x';
		ZERO_STRUCT(wr);
		wr.in.file.handle = h1;
		wr.in.offset      = 0;
		wr.in.data        = data;
		status = smb2_write(tree1, &wr);
		torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	}

	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h2.data[0]);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch11(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch11.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfi;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler =
	    torture_oplock_handler_two_notifications;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/* Test if a set-eof on pathname breaks an exclusive oplock. */
	torture_comment(tctx, "BATCH11: Test if setpathinfo set EOF breaks "
			"oplocks.\n");

	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
				NTCREATEX_SHARE_ACCESS_WRITE|
				NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h1 = io.smb2.out.file.handle;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFORMATION;
	sfi.generic.in.file.path = fname;
	sfi.end_of_file_info.in.size = 100;

	status = smb2_composite_setpathinfo(tree2, &sfi);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");

	/* We expect two breaks */
	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.count, 2);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch12(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch12.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfi;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler =
	    torture_oplock_handler_two_notifications;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/* Test if a set-allocation size on pathname breaks an exclusive
	 * oplock. */
	torture_comment(tctx, "BATCH12: Test if setpathinfo allocation size "
			"breaks oplocks.\n");

	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
				NTCREATEX_SHARE_ACCESS_WRITE|
				NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h1 = io.smb2.out.file.handle;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_ALLOCATION_INFORMATION;
	sfi.generic.in.file.path = fname;
	sfi.allocation_info.in.alloc_size = 65536 * 8;

	status = smb2_composite_setpathinfo(tree2, &sfi);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");

	/* We expect two breaks */
	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.count, 2);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch13(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch13.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	tree2->session->transport->oplock.handler = torture_oplock_handler;
	tree2->session->transport->oplock.private_data = tree2;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH13: open with batch oplock\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "second open with attributes only and "
			"NTCREATEX_DISP_OVERWRITE dispostion causes "
			"oplock break\n");

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_FILE_READ_ATTRIBUTE |
				SEC_FILE_WRITE_ATTRIBUTE |
				SEC_STD_SYNCHRONIZE;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
				NTCREATEX_SHARE_ACCESS_WRITE|
				NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OVERWRITE;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);

	return ret;
}

static bool test_smb2_oplock_batch14(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch14.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH14: open with batch oplock\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "second open with attributes only and "
			"NTCREATEX_DISP_SUPERSEDE dispostion causes "
			"oplock break\n");

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_FILE_READ_ATTRIBUTE |
				SEC_FILE_WRITE_ATTRIBUTE |
				SEC_STD_SYNCHRONIZE;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
				NTCREATEX_SHARE_ACCESS_WRITE|
				NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OVERWRITE;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch15(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch15.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo qfi;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/* Test if a qpathinfo all info on pathname breaks a batch oplock. */
	torture_comment(tctx, "BATCH15: Test if qpathinfo all info breaks "
			"a batch oplock (should not).\n");

	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
				NTCREATEX_SHARE_ACCESS_WRITE|
				NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_SMB2_ALL_INFORMATION;
	qfi.generic.in.file.handle = h1;
	status = smb2_getinfo_file(tree2, tctx, &qfi);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch16(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch16.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	tree2->session->transport->oplock.handler = torture_oplock_handler;
	tree2->session->transport->oplock.private_data = tree2;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH16: open with batch oplock\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "second open with attributes only and "
			"NTCREATEX_DISP_OVERWRITE_IF dispostion causes "
			"oplock break\n");

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.desired_access = SEC_FILE_READ_ATTRIBUTE |
				SEC_FILE_WRITE_ATTRIBUTE |
				SEC_STD_SYNCHRONIZE;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
				NTCREATEX_SHARE_ACCESS_WRITE|
				NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

/* This function is a placeholder for the SMB1 RAW-OPLOCK-BATCH17 test.  Since
 * SMB2 doesn't have a RENAME command this test isn't applicable.  However,
 * it's much less confusing, when comparing test, to keep the SMB1 and SMB2
 * test numbers in sync. */
#if 0
static bool test_raw_oplock_batch17(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	return true;
}
#endif

/* This function is a placeholder for the SMB1 RAW-OPLOCK-BATCH18 test.  Since
 * SMB2 doesn't have an NTRENAME command this test isn't applicable.  However,
 * it's much less confusing, when comparing tests, to keep the SMB1 and SMB2
 * test numbers in sync. */
#if 0
static bool test_raw_oplock_batch18(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	return true;
}
#endif

static bool test_smb2_oplock_batch19(struct torture_context *tctx,
				     struct smb2_tree *tree1)
{
	const char *fname1 = BASEDIR "\\test_batch19_1.dat";
	const char *fname2 = BASEDIR "\\test_batch19_2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo qfi;
	union smb_setfileinfo sfi;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname1);
	smb2_util_unlink(tree1, fname2);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname1;

	torture_comment(tctx, "BATCH19: open a file with an batch oplock "
			"(share mode: none)\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	torture_comment(tctx, "setfileinfo rename info should not trigger "
			"a break but should cause a sharing violation\n");
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.generic.in.file.path = fname1;
	sfi.rename_information.in.file.handle   = h1;
	sfi.rename_information.in.overwrite     = 0;
	sfi.rename_information.in.root_fid      = 0;
	sfi.rename_information.in.new_name      = fname2;

	status = smb2_setinfo_file(tree1, &sfi);

	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_SMB2_ALL_INFORMATION;
	qfi.generic.in.file.handle = h1;

	status = smb2_getinfo_file(tree1, tctx, &qfi);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	CHECK_STRMATCH(qfi.all_info2.out.fname.s, fname1);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, fname1);
	smb2_deltree(tree1, fname2);
	return ret;
}

static bool test_smb2_oplock_batch20(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname1 = BASEDIR "\\test_batch20_1.dat";
	const char *fname2 = BASEDIR "\\test_batch20_2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo qfi;
	union smb_setfileinfo sfi;
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname1);
	smb2_util_unlink(tree1, fname2);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname1;

	torture_comment(tctx, "BATCH20: open a file with an batch oplock "
			"(share mode: all)\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
				NTCREATEX_SHARE_ACCESS_WRITE|
				NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	torture_comment(tctx, "setfileinfo rename info should not trigger "
			"a break but should cause a sharing violation\n");
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.rename_information.in.file.handle	= h1;
	sfi.rename_information.in.overwrite     = 0;
	sfi.rename_information.in.new_name      = fname2;

	status = smb2_setinfo_file(tree1, &sfi);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_SMB2_ALL_INFORMATION;
	qfi.generic.in.file.handle = h1;

	status = smb2_getinfo_file(tree1, tctx, &qfi);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	CHECK_STRMATCH(qfi.all_info2.out.fname.s, fname1);

	torture_comment(tctx, "open the file a second time requesting batch "
			"(share mode: all)\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
				NTCREATEX_SHARE_ACCESS_WRITE|
				NTCREATEX_SHARE_ACCESS_DELETE;
	io.smb2.in.fname = fname1;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);

	torture_comment(tctx, "setfileinfo rename info should not trigger "
			"a break but should cause a sharing violation\n");
	ZERO_STRUCT(break_info);
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.rename_information.in.file.handle	= h2;
	sfi.rename_information.in.overwrite     = 0;
	sfi.rename_information.in.new_name      = fname2;

	status = smb2_setinfo_file(tree2, &sfi);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_SMB2_ALL_INFORMATION;
	qfi.generic.in.file.handle = h1;

	status = smb2_getinfo_file(tree1, tctx, &qfi);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	CHECK_STRMATCH(qfi.all_info2.out.fname.s, fname1);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_SMB2_ALL_INFORMATION;
	qfi.generic.in.file.handle = h2;

	status = smb2_getinfo_file(tree2, tctx, &qfi);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	CHECK_STRMATCH(qfi.all_info2.out.fname.s, fname1);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, fname1);
	return ret;
}

static bool test_smb2_oplock_batch21(struct torture_context *tctx,
				     struct smb2_tree *tree1)
{
	const char *fname = BASEDIR "\\test_batch21.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1;
	char c = 0;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "BATCH21: open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	torture_comment(tctx, "writing should not generate a break\n");
	status = smb2_util_write(tree1, h1, &c, 0, 1);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch22(struct torture_context *tctx,
				     struct smb2_tree *tree1)
{
	const char *fname = BASEDIR "\\test_batch22.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;
	struct timeval tv;
	int timeout = torture_setting_int(tctx, "oplocktimeout", 30);
	int te;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;
	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "BATCH22: open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	torture_comment(tctx, "a 2nd open should succeed after the oplock "
			"break timeout\n");
	tv = timeval_current();
	tree1->session->transport->oplock.handler =
				torture_oplock_handler_timeout;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	torture_wait_for_oplock_break(tctx);
	te = (int)timeval_elapsed(&tv);
	CHECK_RANGE(te, timeout - 1, timeout + 15);
	torture_comment(tctx, "waited %d seconds for oplock timeout\n", te);

	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h2);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch23(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch23.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2, h3;
	struct smb2_tree *tree3 = NULL;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	ret = open_smb2_connection_no_level2_oplocks(tctx, &tree3);
	CHECK_VAL(ret, true);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	tree2->session->transport->oplock.handler = torture_oplock_handler;
	tree2->session->transport->oplock.private_data = tree2;

	tree3->session->transport->oplock.handler = torture_oplock_handler;
	tree3->session->transport->oplock.private_data = tree3;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH23: an open and ask for a batch oplock\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.desired_access = SEC_RIGHTS_FILE_READ |
				SEC_RIGHTS_FILE_WRITE;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				NTCREATEX_SHARE_ACCESS_WRITE;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a 2nd open without level2 oplock support "
			"should generate a break to level2\n");
	status = smb2_create(tree3, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h3 = io.smb2.out.file.handle;

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a 3rd open with level2 oplock support should "
			"not generate a break\n");
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree3, h3);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch24(struct torture_context *tctx,
				     struct smb2_tree *tree1,
				     struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch24.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1, h2;
	struct smb2_tree *tree3 = NULL;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	ret = open_smb2_connection_no_level2_oplocks(tctx, &tree3);
	CHECK_VAL(ret, true);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	tree2->session->transport->oplock.handler = torture_oplock_handler;
	tree2->session->transport->oplock.private_data = tree2;

	tree3->session->transport->oplock.handler = torture_oplock_handler;
	tree3->session->transport->oplock.private_data = tree3;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH24: a open without level support and "
			"ask for a batch oplock\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.desired_access = SEC_RIGHTS_FILE_READ |
				SEC_RIGHTS_FILE_WRITE;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				NTCREATEX_SHARE_ACCESS_WRITE;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;

	status = smb2_create(tree3, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h2 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a 2nd open with level2 oplock support should "
			"generate a break\n");
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.handle.data[0], h2.data[0]);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree3, h2);
	smb2_util_close(tree2, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_batch25(struct torture_context *tctx,
			             struct smb2_tree *tree1)
{
	const char *fname = BASEDIR "\\test_batch25.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "BATCH25: open a file with an batch oplock "
			"(share mode: none)\n");

	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;

	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	torture_comment(tctx, "changing the file attribute info should trigger "
			"a break and a violation\n");

	status = smb2_util_setatr(tree1, fname, FILE_ATTRIBUTE_HIDDEN);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_SHARING_VIOLATION,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, fname);
	return ret;
}

/* Test how oplocks work on streams. */
static bool test_raw_oplock_stream1(struct torture_context *tctx,
				    struct smb2_tree *tree1,
				    struct smb2_tree *tree2)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname_base = BASEDIR "\\test_stream1.txt";
	const char *fname_stream, *fname_default_stream;
	const char *default_stream = "::$DATA";
	const char *stream = "Stream One:$DATA";
	bool ret = true;
	struct smb2_handle h, h_base, h_stream;
	int i;

#define NSTREAM_OPLOCK_RESULTS 8
	struct {
		const char **fname;
		bool open_base_file;
		uint32_t oplock_req;
		uint32_t oplock_granted;
	} stream_oplock_results[NSTREAM_OPLOCK_RESULTS] = {
		/* Request oplock on stream without the base file open. */
		{&fname_stream, false, SMB2_OPLOCK_LEVEL_BATCH, SMB2_OPLOCK_LEVEL_BATCH},
		{&fname_default_stream, false, SMB2_OPLOCK_LEVEL_BATCH, SMB2_OPLOCK_LEVEL_BATCH},
		{&fname_stream, false, SMB2_OPLOCK_LEVEL_EXCLUSIVE, SMB2_OPLOCK_LEVEL_EXCLUSIVE},
		{&fname_default_stream, false,  SMB2_OPLOCK_LEVEL_EXCLUSIVE, SMB2_OPLOCK_LEVEL_EXCLUSIVE},

		/* Request oplock on stream with the base file open. */
		{&fname_stream, true, SMB2_OPLOCK_LEVEL_BATCH, SMB2_OPLOCK_LEVEL_BATCH},
		{&fname_default_stream, true, SMB2_OPLOCK_LEVEL_BATCH, SMB2_OPLOCK_LEVEL_II},
		{&fname_stream, true, SMB2_OPLOCK_LEVEL_EXCLUSIVE, SMB2_OPLOCK_LEVEL_EXCLUSIVE},
		{&fname_default_stream, true,  SMB2_OPLOCK_LEVEL_EXCLUSIVE, SMB2_OPLOCK_LEVEL_II},
	};

	fname_stream = talloc_asprintf(tctx, "%s:%s", fname_base, stream);
	fname_default_stream = talloc_asprintf(tctx, "%s%s", fname_base,
					       default_stream);

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* Initialize handles to "closed".  Using -1 in the first 64-bytes
	 * as the sentry for this */
	h_stream.data[0] = -1;

	/* cleanup */
	smb2_util_unlink(tree1, fname_base);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	tree2->session->transport->oplock.handler = torture_oplock_handler;
	tree2->session->transport->oplock.private_data = tree2;

	/* Setup generic open parameters. */
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = (SEC_FILE_READ_DATA |
				     SEC_FILE_WRITE_DATA |
				     SEC_FILE_APPEND_DATA |
				     SEC_STD_READ_CONTROL);
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				  NTCREATEX_SHARE_ACCESS_WRITE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;

	/* Create the file with a stream */
	io.smb2.in.fname = fname_stream;
	io.smb2.in.create_flags = 0;
	io.smb2.in.create_disposition = NTCREATEX_DISP_CREATE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error creating file");
	smb2_util_close(tree1, io.smb2.out.file.handle);

	/* Change the disposition to open now that the file has been created. */
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN;

	/* Try some permutations of taking oplocks on streams. */
	for (i = 0; i < NSTREAM_OPLOCK_RESULTS; i++) {
		const char *fname = *stream_oplock_results[i].fname;
		bool open_base_file = stream_oplock_results[i].open_base_file;
		uint32_t oplock_req = stream_oplock_results[i].oplock_req;
		uint32_t oplock_granted =
		    stream_oplock_results[i].oplock_granted;

		if (open_base_file) {
			torture_comment(tctx, "Opening base file: %s with "
			    "%d\n", fname_base, SMB2_OPLOCK_LEVEL_BATCH);
			io.smb2.in.fname = fname_base;
			io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
			io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
			status = smb2_create(tree2, tctx, &(io.smb2));
			torture_assert_ntstatus_ok(tctx, status,
			    "Error opening file");
			CHECK_VAL(io.smb2.out.oplock_level,
			    SMB2_OPLOCK_LEVEL_BATCH);
			h_base = io.smb2.out.file.handle;
		}

		torture_comment(tctx, "%d: Opening stream: %s with %d\n", i,
		    fname, oplock_req);
		io.smb2.in.fname = fname;
		io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
		io.smb2.in.oplock_level = oplock_req;

		/* Do the open with the desired oplock on the stream. */
		status = smb2_create(tree1, tctx, &(io.smb2));
		torture_assert_ntstatus_ok(tctx, status, "Error opening file");
		CHECK_VAL(io.smb2.out.oplock_level, oplock_granted);
		smb2_util_close(tree1, io.smb2.out.file.handle);

		/* Cleanup the base file if it was opened. */
		if (open_base_file)
			smb2_util_close(tree2, h_base);
	}

	/* Open the stream with an exclusive oplock. */
	torture_comment(tctx, "Opening stream: %s with %d\n",
	    fname_stream, SMB2_OPLOCK_LEVEL_EXCLUSIVE);
	io.smb2.in.fname = fname_stream;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening file");
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_EXCLUSIVE);
	h_stream = io.smb2.out.file.handle;

	/* Open the base file and see if it contends. */
	ZERO_STRUCT(break_info);
	torture_comment(tctx, "Opening base file: %s with %d\n",
	    fname_base, SMB2_OPLOCK_LEVEL_BATCH);
	io.smb2.in.fname = fname_base;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening file");
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);
	smb2_util_close(tree2, io.smb2.out.file.handle);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	/* Open the stream again to see if it contends. */
	ZERO_STRUCT(break_info);
	torture_comment(tctx, "Opening stream again: %s with "
	    "%d\n", fname_base, SMB2_OPLOCK_LEVEL_BATCH);
	io.smb2.in.fname = fname_stream;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_EXCLUSIVE;
	status = smb2_create(tree2, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening file");
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);
	smb2_util_close(tree2, io.smb2.out.file.handle);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	/* Close the stream. */
	if (h_stream.data[0] != -1) {
		smb2_util_close(tree1, h_stream);
	}

	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

static bool test_smb2_oplock_doc(struct torture_context *tctx, struct smb2_tree *tree)
{
	const char *fname = BASEDIR "\\test_oplock_doc.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1;

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree, fname);
	tree->session->transport->oplock.handler = torture_oplock_handler;
	tree->session->transport->oplock.private_data = tree;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = NTCREATEX_OPTIONS_DELETE_ON_CLOSE;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "open a delete-on-close file with a batch "
			"oplock\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;

	status = smb2_create(tree, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	smb2_util_close(tree, h1);

	smb2_util_unlink(tree, fname);
	smb2_deltree(tree, BASEDIR);
	return ret;
}

/* Open a file with a batch oplock, then open it again from a second client
 * requesting no oplock. Having two open file handles should break our own
 * oplock during BRL acquisition.
 */
static bool test_smb2_oplock_brl1(struct torture_context *tctx,
				struct smb2_tree *tree1,
				struct smb2_tree *tree2)
{
	const char *fname = BASEDIR "\\test_batch_brl.dat";
	/*int fname, f;*/
	bool ret = true;
	uint8_t buf[1000];
	bool correct = true;
	union smb_open io;
	NTSTATUS status;
	struct smb2_lock lck;
	struct smb2_lock_element lock[1];
	struct smb2_handle h, h1, h2;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler =
	    torture_oplock_handler_two_notifications;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_READ |
				    SEC_RIGHTS_FILE_WRITE;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				  NTCREATEX_SHARE_ACCESS_WRITE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "open with batch oplock\n");
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;

	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	/* create a file with bogus data */
	memset(buf, 0, sizeof(buf));

	status = smb2_util_write(tree1, h1,buf, 0, sizeof(buf));
	if (!NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
		torture_comment(tctx, "Failed to create file\n");
		correct = false;
		goto done;
	}

	torture_comment(tctx, "a 2nd open should give a break\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = 0;
	status = smb2_create(tree2, tctx, &(io.smb2));
	h2 = io.smb2.out.file.handle;
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a self BRL acquisition should break to none\n");
	lock[0].offset = 0;
	lock[0].length = 4;
	lock[0].flags = SMB2_LOCK_FLAG_EXCLUSIVE |
			SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;

	ZERO_STRUCT(lck);
	lck.in.file.handle = h1;
	lck.in.locks = &lock[0];
	lck.in.lock_count = 1;
	status = smb2_lock(tree1, &lck);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_NONE);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.failures, 0);

	/* expect no oplock break */
	ZERO_STRUCT(break_info);
	lock[0].offset = 2;
	status = smb2_lock(tree1, &lck);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_LOCK_NOT_GRANTED,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree2, h2);
	smb2_util_close(tree1, h);

done:
	smb2_deltree(tree1, BASEDIR);
	return ret;

}

/* Open a file with a batch oplock on one tree and then acquire a brl.
 * We should not contend our own oplock.
 */
static bool test_smb2_oplock_brl2(struct torture_context *tctx, struct smb2_tree *tree1)
{
	const char *fname = BASEDIR "\\test_batch_brl.dat";
	/*int fname, f;*/
	bool ret = true;
	uint8_t buf[1000];
	bool correct = true;
	union smb_open io;
	NTSTATUS status;
	struct smb2_handle h, h1;
	struct smb2_lock lck;
	struct smb2_lock_element lock[1];

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_READ |
				    SEC_RIGHTS_FILE_WRITE;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				  NTCREATEX_SHARE_ACCESS_WRITE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;

	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	/* create a file with bogus data */
	memset(buf, 0, sizeof(buf));

	status = smb2_util_write(tree1, h1, buf, 0, sizeof(buf));
	if (!NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
		torture_comment(tctx, "Failed to create file\n");
		correct = false;
		goto done;
	}

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a self BRL acquisition should not break to "
			"none\n");

	lock[0].offset = 0;
	lock[0].length = 4;
	lock[0].flags = SMB2_LOCK_FLAG_EXCLUSIVE |
			SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;

	ZERO_STRUCT(lck);
	lck.in.file.handle = h1;
	lck.in.locks = &lock[0];
	lck.in.lock_count = 1;
	status = smb2_lock(tree1, &lck);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");

	lock[0].offset = 2;
	status = smb2_lock(tree1, &lck);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_LOCK_NOT_GRANTED,
				      "Incorrect status");

	/* With one file handle open a BRL should not contend our oplock.
	 * Thus, no oplock break will be received and the entire break_info
	 * struct will be 0 */
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

done:
	smb2_deltree(tree1, BASEDIR);
	return ret;
}

/* Open a file with a batch oplock twice from one tree and then acquire a
 * brl. BRL acquisition should break our own oplock.
 */
static bool test_smb2_oplock_brl3(struct torture_context *tctx, struct smb2_tree *tree1)
{
	const char *fname = BASEDIR "\\test_batch_brl.dat";
	bool ret = true;
	uint8_t buf[1000];
	bool correct = true;
	union smb_open io;
	NTSTATUS status;
	struct smb2_handle h, h1, h2;
	struct smb2_lock lck;
	struct smb2_lock_element lock[1];

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);
	tree1->session->transport->oplock.handler =
	    torture_oplock_handler_two_notifications;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_READ |
				    SEC_RIGHTS_FILE_WRITE;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				  NTCREATEX_SHARE_ACCESS_WRITE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "open with batch oplock\n");
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;

	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_BATCH);

	/* create a file with bogus data */
	memset(buf, 0, sizeof(buf));
	status = smb2_util_write(tree1, h1, buf, 0, sizeof(buf));

	if (!NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
		torture_comment(tctx, "Failed to create file\n");
		correct = false;
		goto done;
	}

	torture_comment(tctx, "a 2nd open should give a break\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = 0;
	status = smb2_create(tree1, tctx, &(io.smb2));
	h2 = io.smb2.out.file.handle;
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a self BRL acquisition should break to none\n");

	lock[0].offset = 0;
	lock[0].length = 4;
	lock[0].flags = SMB2_LOCK_FLAG_EXCLUSIVE |
			SMB2_LOCK_FLAG_FAIL_IMMEDIATELY;

	ZERO_STRUCT(lck);
	lck.in.file.handle = h1;
	lck.in.locks = &lock[0];
	lck.in.lock_count = 1;
	status = smb2_lock(tree1, &lck);
	torture_assert_ntstatus_ok(tctx, status, "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, SMB2_OPLOCK_LEVEL_NONE);
	CHECK_VAL(break_info.handle.data[0], h1.data[0]);
	CHECK_VAL(break_info.failures, 0);

	/* expect no oplock break */
	ZERO_STRUCT(break_info);
	lock[0].offset = 2;
	status = smb2_lock(tree1, &lck);
	torture_assert_ntstatus_equal(tctx, status, NT_STATUS_LOCK_NOT_GRANTED,
				      "Incorrect status");

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h2);
	smb2_util_close(tree1, h);

done:
	smb2_deltree(tree1, BASEDIR);
	return ret;

}

/* Starting the SMB2 specific oplock tests at 500 so we can keep the SMB1
 * tests in sync with an identically numbered SMB2 test */

/* Test whether the server correctly returns an error when we send
 * a response to a levelII to none oplock notification. */
static bool test_smb2_oplock_levelII500(struct torture_context *tctx,
				      struct smb2_tree *tree1)
{
	const char *fname = BASEDIR "\\test_levelII500.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb2_handle h, h1;
	char c = 0;

	status = torture_smb2_testdir(tree1, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	/* cleanup */
	smb2_util_unlink(tree1, fname);

	tree1->session->transport->oplock.handler = torture_oplock_handler;
	tree1->session->transport->oplock.private_data = tree1;

	/*
	  base ntcreatex parms
	*/
	ZERO_STRUCT(io.smb2);
	io.generic.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = fname;

	torture_comment(tctx, "LEVELII500: acknowledging a break from II to "
			"none should return an error\n");
	ZERO_STRUCT(break_info);

	io.smb2.in.desired_access = SEC_RIGHTS_FILE_READ |
				SEC_RIGHTS_FILE_WRITE;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				NTCREATEX_SHARE_ACCESS_WRITE;
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_II;
	status = smb2_create(tree1, tctx, &(io.smb2));
	torture_assert_ntstatus_ok(tctx, status, "Error opening the file");
	h1 = io.smb2.out.file.handle;
	CHECK_VAL(io.smb2.out.oplock_level, SMB2_OPLOCK_LEVEL_II);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "write should trigger a break to none and when "
			"we reply, an oplock break failure\n");
	smb2_util_write(tree1, h1, &c, 0, 1);

	/* Wait several times to receive both the break notification, and the
	 * NT_STATUS_INVALID_OPLOCK_PROTOCOL error in the break response */
	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);

	/* There appears to be a race condition in W2K8 and W2K8R2 where
	 * sometimes the server will happily reply to our break response with
	 * NT_STATUS_OK, and sometimes it will return the OPLOCK_PROTOCOL
	 * error.  As the MS-SMB2 doc states that a client should not reply to
	 * a level2 to none break notification, I'm leaving the protocol error
	 * as the expected behavior. */
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 1);
	torture_assert_ntstatus_equal(tctx, break_info.failure_status,
				      NT_STATUS_INVALID_OPLOCK_PROTOCOL,
				      "Incorrect status");

	smb2_util_close(tree1, h1);
	smb2_util_close(tree1, h);

	smb2_deltree(tree1, BASEDIR);
	return ret;
}

struct torture_suite *torture_smb2_oplocks_init(void)
{
	struct torture_suite *suite =
	    torture_suite_create(talloc_autofree_context(), "oplock");

	torture_suite_add_2smb2_test(suite, "exclusive1", test_smb2_oplock_exclusive1);
	torture_suite_add_2smb2_test(suite, "exclusive2", test_smb2_oplock_exclusive2);
	torture_suite_add_2smb2_test(suite, "exclusive3", test_smb2_oplock_exclusive3);
	torture_suite_add_2smb2_test(suite, "exclusive4", test_smb2_oplock_exclusive4);
	torture_suite_add_2smb2_test(suite, "exclusive5", test_smb2_oplock_exclusive5);
	torture_suite_add_2smb2_test(suite, "exclusive6", test_smb2_oplock_exclusive6);
	torture_suite_add_2smb2_test(suite, "batch1", test_smb2_oplock_batch1);
	torture_suite_add_2smb2_test(suite, "batch2", test_smb2_oplock_batch2);
	torture_suite_add_2smb2_test(suite, "batch3", test_smb2_oplock_batch3);
	torture_suite_add_2smb2_test(suite, "batch4", test_smb2_oplock_batch4);
	torture_suite_add_2smb2_test(suite, "batch5", test_smb2_oplock_batch5);
	torture_suite_add_2smb2_test(suite, "batch6", test_smb2_oplock_batch6);
	torture_suite_add_2smb2_test(suite, "batch7", test_smb2_oplock_batch7);
	torture_suite_add_2smb2_test(suite, "batch8", test_smb2_oplock_batch8);
	torture_suite_add_2smb2_test(suite, "batch9", test_smb2_oplock_batch9);
	torture_suite_add_2smb2_test(suite, "batch10", test_smb2_oplock_batch10);
	torture_suite_add_2smb2_test(suite, "batch11", test_smb2_oplock_batch11);
	torture_suite_add_2smb2_test(suite, "batch12", test_smb2_oplock_batch12);
	torture_suite_add_2smb2_test(suite, "batch13", test_smb2_oplock_batch13);
	torture_suite_add_2smb2_test(suite, "batch14", test_smb2_oplock_batch14);
	torture_suite_add_2smb2_test(suite, "batch15", test_smb2_oplock_batch15);
	torture_suite_add_2smb2_test(suite, "batch16", test_smb2_oplock_batch16);
	torture_suite_add_1smb2_test(suite, "batch19", test_smb2_oplock_batch19);
	torture_suite_add_2smb2_test(suite, "batch20", test_smb2_oplock_batch20);
	torture_suite_add_1smb2_test(suite, "batch21", test_smb2_oplock_batch21);
	torture_suite_add_1smb2_test(suite, "batch22", test_smb2_oplock_batch22);
	torture_suite_add_2smb2_test(suite, "batch23", test_smb2_oplock_batch23);
	torture_suite_add_2smb2_test(suite, "batch24", test_smb2_oplock_batch24);
	torture_suite_add_1smb2_test(suite, "batch25", test_smb2_oplock_batch25);
	torture_suite_add_2smb2_test(suite, "stream1", test_raw_oplock_stream1);
	torture_suite_add_1smb2_test(suite, "doc", test_smb2_oplock_doc);
	torture_suite_add_2smb2_test(suite, "brl1", test_smb2_oplock_brl1);
	torture_suite_add_1smb2_test(suite, "brl2", test_smb2_oplock_brl2);
	torture_suite_add_1smb2_test(suite, "brl3", test_smb2_oplock_brl3);
	torture_suite_add_1smb2_test(suite, "levelii500", test_smb2_oplock_levelII500);

	suite->description = talloc_strdup(suite, "SMB2-OPLOCK tests");

	return suite;
}

/*
   stress testing of oplocks
*/
bool test_smb2_bench_oplock(struct torture_context *tctx,
				   struct smb2_tree *tree)
{
	struct smb2_tree **trees;
	bool ret = true;
	NTSTATUS status;
	TALLOC_CTX *mem_ctx = talloc_new(tctx);
	int torture_nprocs = torture_setting_int(tctx, "nprocs", 4);
	int i, count=0;
	int timelimit = torture_setting_int(tctx, "timelimit", 10);
	union smb_open io;
	struct timeval tv;
	struct smb2_handle h;

	trees = talloc_array(mem_ctx, struct smb2_tree *, torture_nprocs);

	torture_comment(tctx, "Opening %d connections\n", torture_nprocs);
	for (i=0;i<torture_nprocs;i++) {
		if (!torture_smb2_connection(tctx, &trees[i])) {
			return false;
		}
		talloc_steal(mem_ctx, trees[i]);
		trees[i]->session->transport->oplock.handler =
					torture_oplock_handler_close;
		trees[i]->session->transport->oplock.private_data = trees[i];
	}

	status = torture_smb2_testdir(trees[0], BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	ZERO_STRUCT(io.smb2);
	io.smb2.level = RAW_OPEN_SMB2;
	io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
	io.smb2.in.alloc_size = 0;
	io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
	io.smb2.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
	io.smb2.in.create_options = 0;
	io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
	io.smb2.in.security_flags = 0;
	io.smb2.in.fname = BASEDIR "\\test.dat";
	io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
	io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;

	tv = timeval_current();

	/*
	  we open the same file with SHARE_ACCESS_NONE from all the
	  connections in a round robin fashion. Each open causes an
	  oplock break on the previous connection, which is answered
	  by the oplock_handler_close() to close the file.

	  This measures how fast we can pass on oplocks, and stresses
	  the oplock handling code
	*/
	torture_comment(tctx, "Running for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		for (i=0;i<torture_nprocs;i++) {
			status = smb2_create(trees[i], mem_ctx, &(io.smb2));
			torture_assert_ntstatus_ok(tctx, status, "Incorrect status");
			count++;
		}

		if (torture_setting_bool(tctx, "progress", true)) {
			torture_comment(tctx, "%.2f ops/second\r",
					count/timeval_elapsed(&tv));
		}
	}

	torture_comment(tctx, "%.2f ops/second\n", count/timeval_elapsed(&tv));
	smb2_util_close(trees[0], io.smb2.out.file.handle);
	smb2_util_unlink(trees[0], BASEDIR "\\test.dat");
	smb2_deltree(trees[0], BASEDIR);
	talloc_free(mem_ctx);
	return ret;
}

static struct hold_oplock_info {
	const char *fname;
	bool close_on_break;
	uint32_t share_access;
	struct smb2_handle handle;
} hold_info[] = {
	{ BASEDIR "\\notshared_close", true,
	  NTCREATEX_SHARE_ACCESS_NONE, },
	{ BASEDIR "\\notshared_noclose", false,
	  NTCREATEX_SHARE_ACCESS_NONE, },
	{ BASEDIR "\\shared_close", true,
	  NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE|NTCREATEX_SHARE_ACCESS_DELETE, },
	{ BASEDIR "\\shared_noclose", false,
	  NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE|NTCREATEX_SHARE_ACCESS_DELETE, },
};

static bool torture_oplock_handler_hold(struct smb2_transport *transport,
					const struct smb2_handle *handle,
					uint8_t level, void *private_data)
{
	struct hold_oplock_info *info;
	int i;

	for (i=0;i<ARRAY_SIZE(hold_info);i++) {
		if (smb2_util_handle_equal(hold_info[i].handle, *handle))
			break;
	}

	if (i == ARRAY_SIZE(hold_info)) {
		printf("oplock break for unknown handle 0x%llx%llx\n",
		       (unsigned long long) handle->data[0],
		       (unsigned long long) handle->data[1]);
		return false;
	}

	info = &hold_info[i];

	if (info->close_on_break) {
		printf("oplock break on %s - closing\n", info->fname);
		torture_oplock_handler_close(transport, handle,
					     level, private_data);
		return true;
	}

	printf("oplock break on %s - acking break\n", info->fname);
	printf("Acking to none in oplock handler\n");

	torture_oplock_handler_ack_to_none(transport, handle,
					   level, private_data);
	return true;
}

/*
   used for manual testing of oplocks - especially interaction with
   other filesystems (such as NFS and local access)
*/
bool test_smb2_hold_oplock(struct torture_context *tctx,
			   struct smb2_tree *tree)
{
	struct torture_context *mem_ctx = talloc_new(tctx);
	struct tevent_context *ev =
		(struct tevent_context *)tree->session->transport->socket->event.ctx;
	int i;
	struct smb2_handle h;
	NTSTATUS status;

	torture_comment(tctx, "Setting up open files with oplocks in %s\n",
			BASEDIR);

	status = torture_smb2_testdir(tree, BASEDIR, &h);
	torture_assert_ntstatus_ok(tctx, status, "Error creating directory");

	tree->session->transport->oplock.handler = torture_oplock_handler_hold;
	tree->session->transport->oplock.private_data = tree;

	/* setup the files */
	for (i=0;i<ARRAY_SIZE(hold_info);i++) {
		union smb_open io;
		char c = 1;

		ZERO_STRUCT(io.smb2);
		io.generic.level = RAW_OPEN_SMB2;
		io.smb2.in.desired_access = SEC_RIGHTS_FILE_ALL;
		io.smb2.in.alloc_size = 0;
		io.smb2.in.file_attributes = FILE_ATTRIBUTE_NORMAL;
		io.smb2.in.share_access = hold_info[i].share_access;
		io.smb2.in.create_disposition = NTCREATEX_DISP_OPEN_IF;
		io.smb2.in.create_options = 0;
		io.smb2.in.impersonation_level = SMB2_IMPERSONATION_ANONYMOUS;
		io.smb2.in.security_flags = 0;
		io.smb2.in.fname = hold_info[i].fname;
		io.smb2.in.create_flags = NTCREATEX_FLAGS_EXTENDED;
		io.smb2.in.oplock_level = SMB2_OPLOCK_LEVEL_BATCH;

		torture_comment(tctx, "opening %s\n", hold_info[i].fname);

		status = smb2_create(tree, mem_ctx, &(io.smb2));
		if (!NT_STATUS_IS_OK(status)) {
			torture_comment(tctx, "Failed to open %s - %s\n",
			       hold_info[i].fname, nt_errstr(status));
			return false;
		}

		if (io.smb2.out.oplock_level != SMB2_OPLOCK_LEVEL_BATCH) {
			torture_comment(tctx, "Oplock not granted for %s - "
					"expected %d but got %d\n",
					hold_info[i].fname,
					SMB2_OPLOCK_LEVEL_BATCH,
					io.smb2.out.oplock_level);
			return false;
		}
		hold_info[i].handle = io.smb2.out.file.handle;

		/* make the file non-zero size */
		status = smb2_util_write(tree, hold_info[i].handle, &c, 0, 1);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_OK)) {
			torture_comment(tctx, "Failed to write to file\n");
			return false;
		}
	}

	torture_comment(tctx, "Waiting for oplock events\n");
	event_loop_wait(ev);
	smb2_deltree(tree, BASEDIR);
	talloc_free(mem_ctx);
	return true;
}
