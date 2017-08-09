/* 
   Unix SMB/CIFS implementation.
   basic raw test suite for oplocks
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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "lib/events/events.h"
#include "param/param.h"
#include "lib/cmdline/popt_common.h"
#include "libcli/resolve/resolve.h"

#define CHECK_VAL(v, correct) do { \
	if ((v) != (correct)) { \
		torture_result(tctx, TORTURE_FAIL, "(%s): wrong value for %s got 0x%x - should be 0x%x\n", \
				__location__, #v, (int)v, (int)correct); \
		ret = false; \
	}} while (0)

#define CHECK_RANGE(v, min, max) do {					\
	if ((v) < (min) || (v) > (max)) {				\
		torture_warning(tctx, "(%s): wrong value for %s got "	\
		    "%d - should be between %d and %d\n",		\
		    __location__, #v, (int)v, (int)min, (int)max);	\
	}} while (0)

#define CHECK_STRMATCH(v, correct) do { \
	if (!v || strstr((v),(correct)) == NULL) { \
		torture_result(tctx, TORTURE_FAIL,  "(%s): wrong value for %s got '%s' - should be '%s'\n", \
				__location__, #v, v?v:"NULL", correct); \
		ret = false; \
	} \
} while (0)

#define CHECK_STATUS(tctx, status, correct) do { \
	if (!NT_STATUS_EQUAL(status, correct)) { \
		torture_result(tctx, TORTURE_FAIL, __location__": Incorrect status %s - should be %s", \
		       nt_errstr(status), nt_errstr(correct)); \
		ret = false; \
		goto done; \
	}} while (0)


static struct {
	int fnum;
	uint8_t level;
	int count;
	int failures;
} break_info;

#define BASEDIR "\\test_oplock"

/*
  a handler function for oplock break requests. Ack it as a break to level II if possible
*/
static bool oplock_handler_ack_to_given(struct smbcli_transport *transport,
					uint16_t tid, uint16_t fnum,
					uint8_t level, void *private_data)
{
	struct smbcli_tree *tree = (struct smbcli_tree *)private_data;
	const char *name;

	break_info.fnum = fnum;
	break_info.level = level;
	break_info.count++;

	switch (level) {
	case OPLOCK_BREAK_TO_LEVEL_II:
		name = "level II";
		break;
	case OPLOCK_BREAK_TO_NONE:
		name = "none";
		break;
	default:
		name = "unknown";
		break_info.failures++;
	}
	printf("Acking to %s [0x%02X] in oplock handler\n",
		name, level);

	return smbcli_oplock_ack(tree, fnum, level);
}

/*
  a handler function for oplock break requests. Ack it as a break to none
*/
static bool oplock_handler_ack_to_none(struct smbcli_transport *transport, 
				       uint16_t tid, uint16_t fnum, 
				       uint8_t level, void *private_data)
{
	struct smbcli_tree *tree = (struct smbcli_tree *)private_data;
	break_info.fnum = fnum;
	break_info.level = level;
	break_info.count++;

	printf("Acking to none in oplock handler\n");

	return smbcli_oplock_ack(tree, fnum, OPLOCK_BREAK_TO_NONE);
}

/*
  a handler function for oplock break requests. Let it timeout
*/
static bool oplock_handler_timeout(struct smbcli_transport *transport,
				   uint16_t tid, uint16_t fnum,
				   uint8_t level, void *private_data)
{
	break_info.fnum = fnum;
	break_info.level = level;
	break_info.count++;

	printf("Let oplock break timeout\n");
	return true;
}

static void oplock_handler_close_recv(struct smbcli_request *req)
{
	NTSTATUS status;
	status = smbcli_request_simple_recv(req);
	if (!NT_STATUS_IS_OK(status)) {
		printf("close failed in oplock_handler_close\n");
		break_info.failures++;
	}
}

/*
  a handler function for oplock break requests - close the file
*/
static bool oplock_handler_close(struct smbcli_transport *transport, uint16_t tid, 
				 uint16_t fnum, uint8_t level, void *private_data)
{
	union smb_close io;
	struct smbcli_tree *tree = (struct smbcli_tree *)private_data;
	struct smbcli_request *req;

	break_info.fnum = fnum;
	break_info.level = level;
	break_info.count++;

	io.close.level = RAW_CLOSE_CLOSE;
	io.close.in.file.fnum = fnum;
	io.close.in.write_time = 0;
	req = smb_raw_close_send(tree, &io);
	if (req == NULL) {
		printf("failed to send close in oplock_handler_close\n");
		return false;
	}

	req->async.fn = oplock_handler_close_recv;
	req->async.private_data = NULL;

	return true;
}

static bool open_connection_no_level2_oplocks(struct torture_context *tctx,
					      struct smbcli_state **c)
{
	NTSTATUS status;
	struct smbcli_options options;
	struct smbcli_session_options session_options;

	lpcfg_smbcli_options(tctx->lp_ctx, &options);
	lpcfg_smbcli_session_options(tctx->lp_ctx, &session_options);

	options.use_level2_oplocks = false;

	status = smbcli_full_connection(tctx, c,
					torture_setting_string(tctx, "host", NULL),
					lpcfg_smb_ports(tctx->lp_ctx),
					torture_setting_string(tctx, "share", NULL),
					NULL, lpcfg_socket_options(tctx->lp_ctx), cmdline_credentials,
					lpcfg_resolve_context(tctx->lp_ctx),
					tctx->ev, &options, &session_options,
					lpcfg_gensec_settings(tctx, tctx->lp_ctx));
	if (!NT_STATUS_IS_OK(status)) {
		torture_comment(tctx, "Failed to open connection - %s\n",
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

static uint8_t get_break_level1_to_none_count(struct torture_context *tctx)
{
	return torture_setting_bool(tctx, "2_step_break_to_none", false) ?
	    2 : 1;
}

static bool test_raw_oplock_exclusive1(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_exclusive1.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_unlink unl;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE1: open a file with an exclusive oplock (share mode: none)\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | NTCREATEX_FLAGS_REQUEST_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	torture_comment(tctx, "a 2nd open should not cause a break\n");
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	torture_comment(tctx, "unlink it - should also be no break\n");
	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli2->tree, &unl);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_exclusive2(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_exclusive2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_unlink unl;
	uint16_t fnum=0, fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE2: open a file with an exclusive oplock (share mode: all)\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | NTCREATEX_FLAGS_REQUEST_OPLOCK;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	torture_comment(tctx, "a 2nd open should cause a break to level 2\n");
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);
	ZERO_STRUCT(break_info);

	/* now we have 2 level II oplocks... */
	torture_comment(tctx, "try to unlink it - should not cause a break, but a sharing violation\n");
	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli2->tree, &unl);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	torture_comment(tctx, "close 1st handle\n");
	smbcli_close(cli1->tree, fnum);

	torture_comment(tctx, "try to unlink it - should not cause a break, but a sharing violation\n");
	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli2->tree, &unl);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	torture_comment(tctx, "close 2nd handle\n");
	smbcli_close(cli2->tree, fnum2);

	torture_comment(tctx, "unlink it\n");
	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli2->tree, &unl);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_exclusive3(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_exclusive3.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfi;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE3: open a file with an exclusive oplock (share mode: none)\n");

	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | NTCREATEX_FLAGS_REQUEST_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	torture_comment(tctx, "setpathinfo EOF should trigger a break to none\n");
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFORMATION;
	sfi.generic.in.file.path = fname;
	sfi.end_of_file_info.in.size = 100;

	status = smb_raw_setpathinfo(cli2->tree, &sfi);

	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, get_break_level1_to_none_count(tctx));
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_NONE);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_exclusive4(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_exclusive4.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE4: open with exclusive oplock\n");
	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | NTCREATEX_FLAGS_REQUEST_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);
	torture_comment(tctx, "second open with attributes only shouldn't cause oplock break\n");

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | NTCREATEX_FLAGS_REQUEST_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE|SEC_FILE_WRITE_ATTRIBUTE|SEC_STD_SYNCHRONIZE;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, NO_OPLOCK_RETURN);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_exclusive5(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_exclusive5.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	smbcli_oplock_handler(cli2->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "EXCLUSIVE5: open with exclusive oplock\n");
	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);


	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | NTCREATEX_FLAGS_REQUEST_OPLOCK;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "second open with attributes only and NTCREATEX_DISP_OVERWRITE_IF dispostion causes oplock break\n");

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | NTCREATEX_FLAGS_REQUEST_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE|SEC_FILE_WRITE_ATTRIBUTE|SEC_STD_SYNCHRONIZE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, get_break_level1_to_none_count(tctx));
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_exclusive6(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname1 = BASEDIR "\\test_exclusive6_1.dat";
	const char *fname2 = BASEDIR "\\test_exclusive6_2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_rename rn;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname1);
	smbcli_unlink(cli1->tree, fname2);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;

	torture_comment(tctx, "EXCLUSIVE6: open a file with an exclusive "
			"oplock (share mode: none)\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | NTCREATEX_FLAGS_REQUEST_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	torture_comment(tctx, "rename should not generate a break but get a "
			"sharing violation\n");
	ZERO_STRUCT(rn);
	rn.generic.level = RAW_RENAME_RENAME;
	rn.rename.in.pattern1 = fname1;
	rn.rename.in.pattern2 = fname2;
	rn.rename.in.attrib = 0;

	torture_comment(tctx, "trying rename while first file open\n");
	status = smb_raw_rename(cli2->tree, &rn);

	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

/**
 * Exclusive version of batch19
 */
static bool test_raw_oplock_exclusive7(struct torture_context *tctx,
    struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname1 = BASEDIR "\\test_exclusiv6_1.dat";
	const char *fname2 = BASEDIR "\\test_exclusiv6_2.dat";
	const char *fname3 = BASEDIR "\\test_exclusiv6_3.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo qfi;
	union smb_setfileinfo sfi;
	uint16_t fnum=0;
	uint16_t fnum2 = 0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname1);
	smbcli_unlink(cli1->tree, fname2);
	smbcli_unlink(cli1->tree, fname3);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given,
	    cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
	    NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;

	torture_comment(tctx, "open a file with an exclusive oplock (share "
	    "mode: none)\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	torture_comment(tctx, "setpathinfo rename info should trigger a break "
	    "to none\n");
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.generic.in.file.path = fname1;
	sfi.rename_information.in.overwrite	= 0;
	sfi.rename_information.in.root_fid	= 0;
	sfi.rename_information.in.new_name	= fname2+strlen(BASEDIR)+1;

        status = smb_raw_setpathinfo(cli2->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.failures, 0);

	if (TARGET_IS_WINXP(tctx)) {
		/* XP incorrectly breaks to level2. */
		CHECK_VAL(break_info.count, 1);
		CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	} else {
		/* Exclusive oplocks should not be broken on rename. */
		CHECK_VAL(break_info.failures, 0);
		CHECK_VAL(break_info.count, 0);
	}

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli1->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname2);

	/* Try breaking to level2 and then see if rename breaks the level2.*/
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.fname = fname2;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.failures, 0);

	if (TARGET_IS_WINXP(tctx)) {
		/* XP already broke to level2. */
		CHECK_VAL(break_info.failures, 0);
		CHECK_VAL(break_info.count, 0);
	} else {
		/* Break to level 2 expected. */
		CHECK_VAL(break_info.count, 1);
		CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	}

	ZERO_STRUCT(break_info);
	sfi.generic.in.file.path = fname2;
	sfi.rename_information.in.overwrite	= 0;
	sfi.rename_information.in.root_fid	= 0;
	sfi.rename_information.in.new_name	= fname1+strlen(BASEDIR)+1;

	status = smb_raw_setpathinfo(cli2->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	/* Level2 oplocks are not broken on rename. */
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.count, 0);

	/* Close and re-open file with oplock. */
	smbcli_close(cli1->tree, fnum);
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	torture_comment(tctx, "setfileinfo rename info on a client's own fid "
	    "should not trigger a break nor a violation\n");
	ZERO_STRUCT(break_info);
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.generic.in.file.fnum = fnum;
	sfi.rename_information.in.overwrite	= 0;
	sfi.rename_information.in.root_fid	= 0;
	sfi.rename_information.in.new_name	= fname3+strlen(BASEDIR)+1;

	status = smb_raw_setfileinfo(cli1->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	if (TARGET_IS_WINXP(tctx)) {
		/* XP incorrectly breaks to level2. */
		CHECK_VAL(break_info.count, 1);
		CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	} else {
		CHECK_VAL(break_info.count, 0);
	}

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli1->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname3);

done:
	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch1(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch1.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_unlink unl;
	uint16_t fnum=0;
	char c = 0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "BATCH1: open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "unlink should generate a break\n");
	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli2->tree, &unl);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	torture_comment(tctx, "2nd unlink should not generate a break\n");
	ZERO_STRUCT(break_info);
	status = smb_raw_unlink(cli2->tree, &unl);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	torture_comment(tctx, "writing should generate a self break to none\n");
	smbcli_write(cli1->tree, fnum, 0, &c, 0, 1);

	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_NONE);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch2(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_unlink unl;
	uint16_t fnum=0;
	char c = 0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH2: open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "unlink should generate a break, which we ack as break to none\n");
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_none, cli1->tree);
	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	status = smb_raw_unlink(cli2->tree, &unl);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	torture_comment(tctx, "2nd unlink should not generate a break\n");
	ZERO_STRUCT(break_info);
	status = smb_raw_unlink(cli2->tree, &unl);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	torture_comment(tctx, "writing should not generate a break\n");
	smbcli_write(cli1->tree, fnum, 0, &c, 0, 1);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch3(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch3.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_unlink unl;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH3: if we close on break then the unlink can succeed\n");
	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_close, cli1->tree);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	unl.unlink.in.pattern = fname;
	unl.unlink.in.attrib = 0;
	ZERO_STRUCT(break_info);
	status = smb_raw_unlink(cli2->tree, &unl);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.level, 1);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch4(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch4.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_read rd;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH4: a self read should not cause a break\n");
	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	rd.readx.level = RAW_READ_READX;
	rd.readx.in.file.fnum = fnum;
	rd.readx.in.mincnt = 1;
	rd.readx.in.maxcnt = 1;
	rd.readx.in.offset = 0;
	rd.readx.in.remaining = 0;
	rd.readx.in.read_for_execute = false;
	status = smb_raw_read(cli1->tree, &rd);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch5(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch5.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH5: a 2nd open should give a break\n");
	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.level, 1);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch6(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch6.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;
	char c = 0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	smbcli_oplock_handler(cli2->transport, oplock_handler_ack_to_given, cli2->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH6: a 2nd open should give a break to level II if the first open allowed shared read\n");
	ZERO_STRUCT(break_info);

	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_READ | SEC_RIGHTS_FILE_WRITE;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);

	//torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.level, 1);
	CHECK_VAL(break_info.failures, 0);
	ZERO_STRUCT(break_info);

	torture_comment(tctx, "write should trigger a break to none on both\n");
	smbcli_write(cli1->tree, fnum, 0, &c, 0, 1);

	/* We expect two breaks */
	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.count, 2);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch7(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch7.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH7: a 2nd open should get an oplock when we close instead of ack\n");
	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_close, cli1->tree);

	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum2);
	CHECK_VAL(break_info.level, 1);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli2->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch8(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch8.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH8: open with batch oplock\n");
	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);
	torture_comment(tctx, "second open with attributes only shouldn't cause oplock break\n");

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE|SEC_FILE_WRITE_ATTRIBUTE|SEC_STD_SYNCHRONIZE;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, NO_OPLOCK_RETURN);
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch9(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch9.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;
	char c = 0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH9: open with attributes only can create file\n");

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE|SEC_FILE_WRITE_ATTRIBUTE|SEC_STD_SYNCHRONIZE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "Subsequent normal open should break oplock on attribute only open to level II\n");

	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);
	smbcli_close(cli2->tree, fnum2);

	torture_comment(tctx, "third oplocked open should grant level2 without break\n");
	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	smbcli_oplock_handler(cli2->transport, oplock_handler_ack_to_given, cli2->tree);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "write should trigger a break to none on both\n");
	smbcli_write(cli2->tree, fnum2, 0, &c, 0, 1);

	/* We expect two breaks */
	torture_wait_for_oplock_break(tctx);
	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.count, 2);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch10(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch10.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH10: Open with oplock after a non-oplock open should grant level2\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.ntcreatex.out.oplock_level, 0);

	smbcli_oplock_handler(cli2->transport, oplock_handler_ack_to_given, cli2->tree);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);

	torture_comment(tctx, "write should trigger a break to none\n");
	{
		union smb_write wr;
		wr.write.level = RAW_WRITE_WRITE;
		wr.write.in.file.fnum = fnum;
		wr.write.in.count = 1;
		wr.write.in.offset = 0;
		wr.write.in.remaining = 0;
		wr.write.in.data = (const uint8_t *)"x";
		status = smb_raw_write(cli1->tree, &wr);
		CHECK_STATUS(tctx, status, NT_STATUS_OK);
	}

	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum2);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch11(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch11.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfi;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/* Test if a set-eof on pathname breaks an exclusive oplock. */
	torture_comment(tctx, "BATCH11: Test if setpathinfo set EOF breaks oplocks.\n");

	ZERO_STRUCT(break_info);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_END_OF_FILE_INFORMATION;
	sfi.generic.in.file.path = fname;
	sfi.end_of_file_info.in.size = 100;

        status = smb_raw_setpathinfo(cli2->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, get_break_level1_to_none_count(tctx));
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch12(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch12.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfi;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/* Test if a set-allocation size on pathname breaks an exclusive oplock. */
	torture_comment(tctx, "BATCH12: Test if setpathinfo allocation size breaks oplocks.\n");

	ZERO_STRUCT(break_info);
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(sfi);
	sfi.generic.level = SMB_SFILEINFO_ALLOCATION_INFORMATION;
	sfi.generic.in.file.path = fname;
	sfi.allocation_info.in.alloc_size = 65536 * 8;

        status = smb_raw_setpathinfo(cli2->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, get_break_level1_to_none_count(tctx));
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch13(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch13.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	smbcli_oplock_handler(cli2->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH13: open with batch oplock\n");
	ZERO_STRUCT(break_info);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "second open with attributes only and NTCREATEX_DISP_OVERWRITE dispostion causes oplock break\n");

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE|SEC_FILE_WRITE_ATTRIBUTE|SEC_STD_SYNCHRONIZE;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);
	CHECK_VAL(break_info.count, get_break_level1_to_none_count(tctx));
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch14(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch14.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH14: open with batch oplock\n");
	ZERO_STRUCT(break_info);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "second open with attributes only and NTCREATEX_DISP_SUPERSEDE dispostion causes oplock break\n");

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE|SEC_FILE_WRITE_ATTRIBUTE|SEC_STD_SYNCHRONIZE;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, get_break_level1_to_none_count(tctx));
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);
done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch15(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch15.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo qfi;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/* Test if a qpathinfo all info on pathname breaks a batch oplock. */
	torture_comment(tctx, "BATCH15: Test if qpathinfo all info breaks a batch oplock (should not).\n");

	ZERO_STRUCT(break_info);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.path = fname;

	status = smb_raw_pathinfo(cli2->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch16(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch16.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	smbcli_oplock_handler(cli2->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH16: open with batch oplock\n");
	ZERO_STRUCT(break_info);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "second open with attributes only and NTCREATEX_DISP_OVERWRITE_IF dispostion causes oplock break\n");

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.access_mask = SEC_FILE_READ_ATTRIBUTE|SEC_FILE_WRITE_ATTRIBUTE|SEC_STD_SYNCHRONIZE;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, get_break_level1_to_none_count(tctx));
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch17(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname1 = BASEDIR "\\test_batch17_1.dat";
	const char *fname2 = BASEDIR "\\test_batch17_2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_rename rn;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname1);
	smbcli_unlink(cli1->tree, fname2);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;

	torture_comment(tctx, "BATCH17: open a file with an batch oplock (share mode: none)\n");

	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "rename should trigger a break\n");
	ZERO_STRUCT(rn);
	rn.generic.level = RAW_RENAME_RENAME;
	rn.rename.in.pattern1 = fname1;
	rn.rename.in.pattern2 = fname2;
	rn.rename.in.attrib = 0;

	torture_comment(tctx, "trying rename while first file open\n");
	status = smb_raw_rename(cli2->tree, &rn);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch18(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname1 = BASEDIR "\\test_batch18_1.dat";
	const char *fname2 = BASEDIR "\\test_batch18_2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_rename rn;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname1);
	smbcli_unlink(cli1->tree, fname2);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;

	torture_comment(tctx, "BATCH18: open a file with an batch oplock (share mode: none)\n");

	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "ntrename should trigger a break\n");
	ZERO_STRUCT(rn);
	rn.generic.level = RAW_RENAME_NTRENAME;
	rn.ntrename.in.attrib	= 0;
	rn.ntrename.in.flags	= RENAME_FLAG_RENAME;
	rn.ntrename.in.old_name = fname1;
	rn.ntrename.in.new_name = fname2;
	torture_comment(tctx, "trying rename while first file open\n");
	status = smb_raw_rename(cli2->tree, &rn);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch19(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname1 = BASEDIR "\\test_batch19_1.dat";
	const char *fname2 = BASEDIR "\\test_batch19_2.dat";
	const char *fname3 = BASEDIR "\\test_batch19_3.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo qfi;
	union smb_setfileinfo sfi;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname1);
	smbcli_unlink(cli1->tree, fname2);
	smbcli_unlink(cli1->tree, fname3);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
	    NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;

	torture_comment(tctx, "BATCH19: open a file with an batch oplock (share mode: none)\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "setpathinfo rename info should trigger a break "
	    "to none\n");
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.generic.in.file.path = fname1;
	sfi.rename_information.in.overwrite	= 0;
	sfi.rename_information.in.root_fid	= 0;
	sfi.rename_information.in.new_name	= fname2+strlen(BASEDIR)+1;

        status = smb_raw_setpathinfo(cli2->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);

	CHECK_VAL(break_info.failures, 0);

	if (TARGET_IS_WINXP(tctx)) {
		/* Win XP breaks to level2. */
		CHECK_VAL(break_info.count, 1);
		CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	} else if (TARGET_IS_W2K3(tctx) || TARGET_IS_W2K8(tctx) ||
	    TARGET_IS_SAMBA3(tctx) || TARGET_IS_SAMBA4(tctx)) {
		/* Win2K3/2k8 incorrectly doesn't break at all. */
		CHECK_VAL(break_info.count, 0);
	} else {
		/* win7/2k8r2 break to none. */
		CHECK_VAL(break_info.count, 1);
		CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_NONE);
	}

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli1->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname2);

	/* Close and re-open file with oplock. */
	smbcli_close(cli1->tree, fnum);
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "setfileinfo rename info on a client's own fid "
	    "should not trigger a break nor a violation\n");
	ZERO_STRUCT(break_info);
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.generic.in.file.fnum = fnum;
	sfi.rename_information.in.overwrite	= 0;
	sfi.rename_information.in.root_fid	= 0;
	sfi.rename_information.in.new_name	= fname3+strlen(BASEDIR)+1;

	status = smb_raw_setfileinfo(cli1->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	if (TARGET_IS_WINXP(tctx)) {
		/* XP incorrectly breaks to level2. */
		CHECK_VAL(break_info.count, 1);
		CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	} else {
		CHECK_VAL(break_info.count, 0);
	}

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli1->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname3);

done:
	smbcli_close(cli1->tree, fnum);
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

/****************************************************
 Called from raw-rename - we need oplock handling for
 this test so this is why it's in oplock.c, not rename.c
****************************************************/

bool test_trans2rename(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname1 = BASEDIR "\\test_trans2rename_1.dat";
	const char *fname2 = BASEDIR "\\test_trans2rename_2.dat";
	const char *fname3 = BASEDIR "\\test_trans2rename_3.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo qfi;
	union smb_setfileinfo sfi;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname1);
	smbcli_unlink(cli1->tree, fname2);
	smbcli_unlink(cli1->tree, fname3);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
	    NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;

	torture_comment(tctx, "open a file with an exclusive oplock (share mode: none)\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	torture_comment(tctx, "setpathinfo rename info should not trigger a break nor a violation\n");
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.generic.in.file.path = fname1;
	sfi.rename_information.in.overwrite	= 0;
	sfi.rename_information.in.root_fid	= 0;
	sfi.rename_information.in.new_name	= fname2+strlen(BASEDIR)+1;

        status = smb_raw_setpathinfo(cli2->tree, &sfi);

	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli1->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname2);

	torture_comment(tctx, "setfileinfo rename info should not trigger a break nor a violation\n");
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.generic.in.file.fnum = fnum;
	sfi.rename_information.in.overwrite	= 0;
	sfi.rename_information.in.root_fid	= 0;
	sfi.rename_information.in.new_name	= fname3+strlen(BASEDIR)+1;

	status = smb_raw_setfileinfo(cli1->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli1->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname3);

done:
	smbcli_close(cli1->tree, fnum);
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

/****************************************************
 Called from raw-rename - we need oplock handling for
 this test so this is why it's in oplock.c, not rename.c
****************************************************/

bool test_nttransrename(struct torture_context *tctx, struct smbcli_state *cli1)
{
	const char *fname1 = BASEDIR "\\test_nttransrename_1.dat";
	const char *fname2 = BASEDIR "\\test_nttransrename_2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo qfi, qpi;
	union smb_rename rn;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname1);
	smbcli_unlink(cli1->tree, fname2);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;

	torture_comment(tctx, "nttrans_rename: open a file with an exclusive oplock (share mode: none)\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	torture_comment(tctx, "nttrans_rename: should not trigger a break nor a share mode violation\n");
	ZERO_STRUCT(rn);
	rn.generic.level = RAW_RENAME_NTTRANS;
	rn.nttrans.in.file.fnum = fnum;
	rn.nttrans.in.flags	= 0;
	rn.nttrans.in.new_name	= fname2+strlen(BASEDIR)+1;

        status = smb_raw_rename(cli1->tree, &rn);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	/* w2k3 does nothing, it doesn't rename the file */
	torture_comment(tctx, "nttrans_rename: the server should have done nothing\n");
	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli1->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname1);

	ZERO_STRUCT(qpi);
	qpi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qpi.generic.in.file.path = fname1;

	status = smb_raw_pathinfo(cli1->tree, tctx, &qpi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qpi.all_info.out.fname.s, fname1);

	ZERO_STRUCT(qpi);
	qpi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qpi.generic.in.file.path = fname2;

	status = smb_raw_pathinfo(cli1->tree, tctx, &qpi);
	CHECK_STATUS(tctx, status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	torture_comment(tctx, "nttrans_rename: after closing the file the file is still not renamed\n");
	status = smbcli_close(cli1->tree, fnum);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	ZERO_STRUCT(qpi);
	qpi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qpi.generic.in.file.path = fname1;

	status = smb_raw_pathinfo(cli1->tree, tctx, &qpi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qpi.all_info.out.fname.s, fname1);

	ZERO_STRUCT(qpi);
	qpi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qpi.generic.in.file.path = fname2;

	status = smb_raw_pathinfo(cli1->tree, tctx, &qpi);
	CHECK_STATUS(tctx, status, NT_STATUS_OBJECT_NAME_NOT_FOUND);

	torture_comment(tctx, "nttrans_rename: rename with an invalid handle gives NT_STATUS_INVALID_HANDLE\n");
	ZERO_STRUCT(rn);
	rn.generic.level = RAW_RENAME_NTTRANS;
	rn.nttrans.in.file.fnum = fnum+1;
	rn.nttrans.in.flags	= 0;
	rn.nttrans.in.new_name	= fname2+strlen(BASEDIR)+1;

	status = smb_raw_rename(cli1->tree, &rn);

	CHECK_STATUS(tctx, status, NT_STATUS_INVALID_HANDLE);

done:
	smb_raw_exit(cli1->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}


static bool test_raw_oplock_batch20(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname1 = BASEDIR "\\test_batch20_1.dat";
	const char *fname2 = BASEDIR "\\test_batch20_2.dat";
	const char *fname3 = BASEDIR "\\test_batch20_3.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_fileinfo qfi;
	union smb_setfileinfo sfi;
	uint16_t fnum=0,fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname1);
	smbcli_unlink(cli1->tree, fname2);
	smbcli_unlink(cli1->tree, fname3);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;

	torture_comment(tctx, "BATCH20: open a file with an batch oplock (share mode: all)\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.generic.in.file.path = fname1;
	sfi.rename_information.in.overwrite	= 0;
	sfi.rename_information.in.root_fid	= 0;
	sfi.rename_information.in.new_name	= fname2+strlen(BASEDIR)+1;

	status = smb_raw_setpathinfo(cli2->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.failures, 0);

	if (TARGET_IS_WINXP(tctx)) {
		/* Win XP breaks to level2. */
		CHECK_VAL(break_info.count, 1);
		CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	} else if (TARGET_IS_W2K3(tctx) || TARGET_IS_W2K8(tctx) ||
	    TARGET_IS_SAMBA3(tctx) || TARGET_IS_SAMBA4(tctx)) {
		/* Win2K3/2k8 incorrectly doesn't break at all. */
		CHECK_VAL(break_info.count, 0);
	} else {
		/* win7/2k8r2 break to none. */
		CHECK_VAL(break_info.count, 1);
		CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_NONE);
	}

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli1->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname2);

	torture_comment(tctx, "open a file with the new name an batch oplock (share mode: all)\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.fname = fname2;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);

	torture_wait_for_oplock_break(tctx);

	if (TARGET_IS_WINXP(tctx)) {
		/* XP broke to level2, and doesn't break again. */
		CHECK_VAL(break_info.count, 0);
	} else if (TARGET_IS_W2K3(tctx) || TARGET_IS_W2K8(tctx) ||
	    TARGET_IS_SAMBA3(tctx) || TARGET_IS_SAMBA4(tctx)) {
		/* Win2K3 incorrectly didn't break before so break now. */
		CHECK_VAL(break_info.count, 1);
		CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	} else {
		/* win7/2k8r2 broke to none, and doesn't break again. */
		CHECK_VAL(break_info.count, 0);
	}

	ZERO_STRUCT(break_info);

	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_RENAME_INFORMATION;
	sfi.generic.in.file.fnum = fnum;
	sfi.rename_information.in.overwrite	= 0;
	sfi.rename_information.in.root_fid	= 0;
	sfi.rename_information.in.new_name	= fname3+strlen(BASEDIR)+1;

	status = smb_raw_setfileinfo(cli1->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum;

	status = smb_raw_fileinfo(cli1->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname3);

	ZERO_STRUCT(qfi);
	qfi.generic.level = RAW_FILEINFO_ALL_INFORMATION;
	qfi.generic.in.file.fnum = fnum2;

	status = smb_raw_fileinfo(cli2->tree, tctx, &qfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_STRMATCH(qfi.all_info.out.fname.s, fname3);


done:
	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch21(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch21.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	struct smb_echo e;
	uint16_t fnum=0;
	char c = 0;
	ssize_t wr;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "BATCH21: open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "writing should not generate a break\n");
	wr = smbcli_write(cli1->tree, fnum, 0, &c, 0, 1);
	CHECK_VAL(wr, 1);
	CHECK_STATUS(tctx, smbcli_nt_error(cli1->tree), NT_STATUS_OK);

	ZERO_STRUCT(e);
	e.in.repeat_count = 1;
	status = smb_raw_echo(cli1->transport, &e);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch22(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch22.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum = 0, fnum2 = 0, fnum3 = 0;
	struct timeval tv;
	int timeout = torture_setting_int(tctx, "oplocktimeout", 30);
	int te;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "BATCH22: open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE|
		NTCREATEX_SHARE_ACCESS_DELETE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "a 2nd open should not succeed after the oplock "
			"break timeout\n");
	tv = timeval_current();
	smbcli_oplock_handler(cli1->transport, oplock_handler_timeout, cli1->tree);
	status = smb_raw_open(cli1->tree, tctx, &io);

	if (TARGET_IS_W2K3(tctx)) {
		/* 2k3 has an issue here. xp/win7 are ok. */
		CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);
	} else {
		CHECK_STATUS(tctx, status, NT_STATUS_OK);
	}

	fnum2 = io.ntcreatex.out.file.fnum;

	torture_wait_for_oplock_break(tctx);
	te = (int)timeval_elapsed(&tv);

	/*
	 * Some servers detect clients that let oplocks timeout, so this check
	 * only shows a warning message instead failing the test to eliminate
	 * failures from repeated runs of the test.  This isn't ideal, but
	 * it's better than not running the test at all.
	 */
	CHECK_RANGE(te, timeout - 1, timeout + 15);

	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);
	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a 2nd open should succeed after the oplock "
			"release without break\n");
	tv = timeval_current();
	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
#if 0
	/* Samba 3.6.0 and above behave as Windows. */
	if (TARGET_IS_SAMBA3(tctx)) {
		/* samba3 doesn't grant additional oplocks to bad clients. */
		CHECK_VAL(io.ntcreatex.out.oplock_level, NO_OPLOCK_RETURN);
	} else {
		CHECK_VAL(io.ntcreatex.out.oplock_level,
			LEVEL_II_OPLOCK_RETURN);
	}
#else
	CHECK_VAL(io.ntcreatex.out.oplock_level,
		  LEVEL_II_OPLOCK_RETURN);
#endif
	torture_wait_for_oplock_break(tctx);
	te = (int)timeval_elapsed(&tv);
	/* it should come in without delay */
	CHECK_RANGE(te+1, 0, timeout);
	fnum3 = io.ntcreatex.out.file.fnum;

	CHECK_VAL(break_info.count, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli1->tree, fnum2);
	smbcli_close(cli1->tree, fnum3);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch23(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch23.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0, fnum2=0,fnum3=0;
	struct smbcli_state *cli3 = NULL;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	ret = open_connection_no_level2_oplocks(tctx, &cli3);
	CHECK_VAL(ret, true);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	smbcli_oplock_handler(cli2->transport, oplock_handler_ack_to_given, cli2->tree);
	smbcli_oplock_handler(cli3->transport, oplock_handler_ack_to_given, cli3->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH23: a open and ask for a batch oplock\n");
	ZERO_STRUCT(break_info);

	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_READ | SEC_RIGHTS_FILE_WRITE;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a 2nd open without level2 oplock support should generate a break to level2\n");
	status = smb_raw_open(cli3->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum3 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, NO_OPLOCK_RETURN);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a 3rd open with level2 oplock support should not generate a break\n");
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);
	smbcli_close(cli3->tree, fnum3);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smb_raw_exit(cli3->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch24(struct torture_context *tctx, struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch24.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum2=0,fnum3=0;
	struct smbcli_state *cli3 = NULL;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	ret = open_connection_no_level2_oplocks(tctx, &cli3);
	CHECK_VAL(ret, true);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	smbcli_oplock_handler(cli2->transport, oplock_handler_ack_to_given, cli2->tree);
	smbcli_oplock_handler(cli3->transport, oplock_handler_ack_to_given, cli3->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH24: a open without level support and ask for a batch oplock\n");
	ZERO_STRUCT(break_info);

	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_READ | SEC_RIGHTS_FILE_WRITE;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli3->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum3 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a 2nd open with level2 oplock support should generate a break to none\n");
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, LEVEL_II_OPLOCK_RETURN);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.fnum, fnum3);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_NONE);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli3->tree, fnum3);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smb_raw_exit(cli3->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_batch25(struct torture_context *tctx,
				    struct smbcli_state *cli1,
				    struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch25.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_setfileinfo sfi;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "BATCH25: open a file with an batch oplock "
			"(share mode: none)\n");

	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "setpathinfo attribute info should not trigger "
			"a break nor a violation\n");
	ZERO_STRUCT(sfi);
	sfi.generic.level = RAW_SFILEINFO_SETATTR;
	sfi.generic.in.file.path	= fname;
	sfi.setattr.in.attrib		= FILE_ATTRIBUTE_HIDDEN;
	sfi.setattr.in.write_time	= 0;

        status = smb_raw_setpathinfo(cli2->tree, &sfi);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

/**
 * Similar to batch17/18, but test with open share mode rather than
 * share_none.
 */
static bool test_raw_oplock_batch26(struct torture_context *tctx,
    struct smbcli_state *cli1, struct smbcli_state *cli2)
{
	const char *fname1 = BASEDIR "\\test_batch26_1.dat";
	const char *fname2 = BASEDIR "\\test_batch26_2.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_rename rn;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname1);
	smbcli_unlink(cli1->tree, fname2);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given,
	    cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
	    NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_DELETE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;

	torture_comment(tctx, "BATCH26: open a file with an batch oplock "
	    "(share mode: none)\n");

	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;


	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	torture_comment(tctx, "rename should trigger a break\n");
	ZERO_STRUCT(rn);
	rn.generic.level = RAW_RENAME_RENAME;
	rn.rename.in.pattern1 = fname1;
	rn.rename.in.pattern2 = fname2;
	rn.rename.in.attrib = 0;

	torture_comment(tctx, "trying rename while first file open\n");
	status = smb_raw_rename(cli2->tree, &rn);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);

	/* Close and reopen with batch again. */
	smbcli_close(cli1->tree, fnum);
	ZERO_STRUCT(break_info);

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	/* Now try ntrename. */
	torture_comment(tctx, "ntrename should trigger a break\n");
	ZERO_STRUCT(rn);
	rn.generic.level = RAW_RENAME_NTRENAME;
	rn.ntrename.in.attrib	= 0;
	rn.ntrename.in.flags	= RENAME_FLAG_RENAME;
	rn.ntrename.in.old_name = fname1;
	rn.ntrename.in.new_name = fname2;
	torture_comment(tctx, "trying rename while first file open\n");
	status = smb_raw_rename(cli2->tree, &rn);
	CHECK_STATUS(tctx, status, NT_STATUS_SHARING_VIOLATION);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

/* Test how oplocks work on streams. */
static bool test_raw_oplock_stream1(struct torture_context *tctx,
				    struct smbcli_state *cli1,
				    struct smbcli_state *cli2)
{
	NTSTATUS status;
	union smb_open io;
	const char *fname_base = BASEDIR "\\test_stream1.txt";
	const char *stream = "Stream One:$DATA";
	const char *fname_stream, *fname_default_stream;
	const char *default_stream = "::$DATA";
	bool ret = true;
	int fnum = -1;
	int i;
	int stream_fnum = -1;
	uint32_t batch_req = NTCREATEX_FLAGS_REQUEST_OPLOCK |
	    NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK | NTCREATEX_FLAGS_EXTENDED;
	uint32_t exclusive_req = NTCREATEX_FLAGS_REQUEST_OPLOCK |
	    NTCREATEX_FLAGS_EXTENDED;

#define NSTREAM_OPLOCK_RESULTS 8
	struct {
		const char **fname;
		bool open_base_file;
		uint32_t oplock_req;
		uint32_t oplock_granted;
	} stream_oplock_results[NSTREAM_OPLOCK_RESULTS] = {
		/* Request oplock on stream without the base file open. */
		{&fname_stream, false, batch_req, NO_OPLOCK_RETURN},
		{&fname_default_stream, false, batch_req, NO_OPLOCK_RETURN},
		{&fname_stream, false, exclusive_req, EXCLUSIVE_OPLOCK_RETURN},
		{&fname_default_stream, false,  exclusive_req, EXCLUSIVE_OPLOCK_RETURN},

		/* Request oplock on stream with the base file open. */
		{&fname_stream, true, batch_req, NO_OPLOCK_RETURN},
		{&fname_default_stream, true, batch_req, NO_OPLOCK_RETURN},
		{&fname_stream, true, exclusive_req, EXCLUSIVE_OPLOCK_RETURN},
		{&fname_default_stream, true,  exclusive_req, LEVEL_II_OPLOCK_RETURN},

	};


	/* Only passes against windows at the moment. */
	if (torture_setting_bool(tctx, "samba3", false) ||
	    torture_setting_bool(tctx, "samba4", false)) {
		torture_skip(tctx, "STREAM1 disabled against samba3+4\n");
	}

	fname_stream = talloc_asprintf(tctx, "%s:%s", fname_base, stream);
	fname_default_stream = talloc_asprintf(tctx, "%s%s", fname_base,
					       default_stream);

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}
	smbcli_unlink(cli1->tree, fname_base);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given, cli1->tree);
	smbcli_oplock_handler(cli2->transport, oplock_handler_ack_to_given, cli2->tree);

	/* Setup generic open parameters. */
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = (SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA|
	    SEC_FILE_APPEND_DATA|SEC_STD_READ_CONTROL);
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
	    NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;

	/* Create the file with a stream */
	io.ntcreatex.in.fname = fname_stream;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	smbcli_close(cli1->tree, io.ntcreatex.out.file.fnum);

	/* Change the disposition to open now that the file has been created. */
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;

	/* Try some permutations of taking oplocks on streams. */
	for (i = 0; i < NSTREAM_OPLOCK_RESULTS; i++) {
		const char *fname = *stream_oplock_results[i].fname;
		bool open_base_file = stream_oplock_results[i].open_base_file;
		uint32_t oplock_req = stream_oplock_results[i].oplock_req;
		uint32_t oplock_granted =
		    stream_oplock_results[i].oplock_granted;
		int base_fnum = -1;

		if (open_base_file) {
			torture_comment(tctx, "Opening base file: %s with "
			    "%d\n", fname_base, batch_req);
			io.ntcreatex.in.fname = fname_base;
			io.ntcreatex.in.flags = batch_req;
			status = smb_raw_open(cli2->tree, tctx, &io);
			CHECK_STATUS(tctx, status, NT_STATUS_OK);
			CHECK_VAL(io.ntcreatex.out.oplock_level,
			    BATCH_OPLOCK_RETURN);
			base_fnum = io.ntcreatex.out.file.fnum;
		}

		torture_comment(tctx, "%d: Opening stream: %s with %d\n", i,
		    fname, oplock_req);
		io.ntcreatex.in.fname = fname;
		io.ntcreatex.in.flags = oplock_req;

		/* Do the open with the desired oplock on the stream. */
		status = smb_raw_open(cli1->tree, tctx, &io);
		CHECK_STATUS(tctx, status, NT_STATUS_OK);
		CHECK_VAL(io.ntcreatex.out.oplock_level, oplock_granted);
		smbcli_close(cli1->tree, io.ntcreatex.out.file.fnum);

		/* Cleanup the base file if it was opened. */
		if (base_fnum != -1) {
			smbcli_close(cli2->tree, base_fnum);
		}
	}

	/* Open the stream with an exclusive oplock. */
	torture_comment(tctx, "Opening stream: %s with %d\n",
	    fname_stream, exclusive_req);
	io.ntcreatex.in.fname = fname_stream;
	io.ntcreatex.in.flags = exclusive_req;
	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);
	stream_fnum = io.ntcreatex.out.file.fnum;

	/* Open the base file and see if it contends. */
	ZERO_STRUCT(break_info);
	torture_comment(tctx, "Opening base file: %s with "
	    "%d\n", fname_base, batch_req);
	io.ntcreatex.in.fname = fname_base;
	io.ntcreatex.in.flags = batch_req;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_VAL(io.ntcreatex.out.oplock_level,
	    BATCH_OPLOCK_RETURN);
	smbcli_close(cli2->tree, io.ntcreatex.out.file.fnum);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.failures, 0);

	/* Open the stream again to see if it contends. */
	ZERO_STRUCT(break_info);
	torture_comment(tctx, "Opening stream again: %s with "
	    "%d\n", fname_base, batch_req);
	io.ntcreatex.in.fname = fname_stream;
	io.ntcreatex.in.flags = exclusive_req;
	status = smb_raw_open(cli2->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_VAL(io.ntcreatex.out.oplock_level,
	    LEVEL_II_OPLOCK_RETURN);
	smbcli_close(cli2->tree, io.ntcreatex.out.file.fnum);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);

	/* Close the stream. */
	if (stream_fnum != -1) {
		smbcli_close(cli1->tree, stream_fnum);
	}

 done:
	smbcli_close(cli1->tree, fnum);
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

static bool test_raw_oplock_doc(struct torture_context *tctx,
				struct smbcli_state *cli)
{
	const char *fname = BASEDIR "\\test_oplock_doc.dat";
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli->tree, fname);

	smbcli_oplock_handler(cli->transport, oplock_handler_ack_to_given,
			      cli->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DELETE_ON_CLOSE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "open a delete-on-close file with a batch "
			"oplock\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
	    NTCREATEX_FLAGS_REQUEST_OPLOCK |
	    NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;

	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	smbcli_close(cli->tree, fnum);

done:
	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);
	return ret;
}

/* Open a file with a batch oplock, then open it again from a second client
 * requesting no oplock. Having two open file handles should break our own
 * oplock during BRL acquisition.
 */
static bool test_raw_oplock_brl1(struct torture_context *tctx,
				 struct smbcli_state *cli1,
				 struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch_brl.dat";
	/*int fname, f;*/
	bool ret = true;
	uint8_t buf[1000];
	bool correct = true;
	union smb_open io;
	NTSTATUS status;
	uint16_t fnum=0;
	uint16_t fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given,
			      cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_READ |
				      SEC_RIGHTS_FILE_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				       NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "open with batch oplock\n");
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);
	/* create a file with bogus data */
	memset(buf, 0, sizeof(buf));

	if (smbcli_write(cli1->tree, fnum, 0, buf, 0, sizeof(buf)) !=
			 sizeof(buf))
	{
		torture_comment(tctx, "Failed to create file\n");
		correct = false;
		goto done;
	}

	torture_comment(tctx, "a 2nd open should give a break\n");
	ZERO_STRUCT(break_info);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	status = smb_raw_open(cli2->tree, tctx, &io);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.fnum, fnum);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a self BRL acquisition should break to none\n");

	status = smbcli_lock(cli1->tree, fnum, 0, 4, 0, WRITE_LOCK);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_NONE);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.failures, 0);

	/* expect no oplock break */
	ZERO_STRUCT(break_info);
	status = smbcli_lock(cli1->tree, fnum, 2, 4, 0, WRITE_LOCK);
	CHECK_STATUS(tctx, status, NT_STATUS_LOCK_NOT_GRANTED);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.fnum, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;

}

/* Open a file with a batch oplock on one client and then acquire a brl.
 * We should not contend our own oplock.
 */
static bool test_raw_oplock_brl2(struct torture_context *tctx, struct smbcli_state *cli1)
{
	const char *fname = BASEDIR "\\test_batch_brl.dat";
	/*int fname, f;*/
	bool ret = true;
	uint8_t buf[1000];
	bool correct = true;
	union smb_open io;
	NTSTATUS status;
	uint16_t fnum=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given,
			      cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_READ |
				      SEC_RIGHTS_FILE_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				       NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "open with batch oplock\n");
	ZERO_STRUCT(break_info);
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	/* create a file with bogus data */
	memset(buf, 0, sizeof(buf));

	if (smbcli_write(cli1->tree, fnum, 0, buf, 0, sizeof(buf)) !=
			 sizeof(buf))
	{
		torture_comment(tctx, "Failed to create file\n");
		correct = false;
		goto done;
	}

	torture_comment(tctx, "a self BRL acquisition should not break to "
			"none\n");

	status = smbcli_lock(cli1->tree, fnum, 0, 4, 0, WRITE_LOCK);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	status = smbcli_lock(cli1->tree, fnum, 2, 4, 0, WRITE_LOCK);
	CHECK_STATUS(tctx, status, NT_STATUS_LOCK_NOT_GRANTED);

	/* With one file handle open a BRL should not contend our oplock.
	 * Thus, no oplock break will be received and the entire break_info
	 * struct will be 0 */
	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.fnum, 0);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);

done:
	smb_raw_exit(cli1->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

/* Open a file with a batch oplock twice from one client and then acquire a
 * brl. BRL acquisition should break our own oplock.
 */
static bool test_raw_oplock_brl3(struct torture_context *tctx,
				 struct smbcli_state *cli1)
{
	const char *fname = BASEDIR "\\test_batch_brl.dat";
	bool ret = true;
	uint8_t buf[1000];
	bool correct = true;
	union smb_open io;
	NTSTATUS status;
	uint16_t fnum=0;
	uint16_t fnum2=0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given,
			      cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_READ |
				      SEC_RIGHTS_FILE_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				       NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	/*
	  with a batch oplock we get a break
	*/
	torture_comment(tctx, "open with batch oplock\n");
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK |
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, BATCH_OPLOCK_RETURN);

	/* create a file with bogus data */
	memset(buf, 0, sizeof(buf));

	if (smbcli_write(cli1->tree, fnum, 0, buf, 0, sizeof(buf)) !=
			 sizeof(buf))
	{
		torture_comment(tctx, "Failed to create file\n");
		correct = false;
		goto done;
	}

	torture_comment(tctx, "a 2nd open should give a break\n");
	ZERO_STRUCT(break_info);

	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	status = smb_raw_open(cli1->tree, tctx, &io);
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.fnum, fnum);

	ZERO_STRUCT(break_info);

	torture_comment(tctx, "a self BRL acquisition should break to none\n");

	status = smbcli_lock(cli1->tree, fnum, 0, 4, 0, WRITE_LOCK);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_NONE);
	CHECK_VAL(break_info.fnum, fnum);
	CHECK_VAL(break_info.failures, 0);

	/* expect no oplock break */
	ZERO_STRUCT(break_info);
	status = smbcli_lock(cli1->tree, fnum, 2, 4, 0, WRITE_LOCK);
	CHECK_STATUS(tctx, status, NT_STATUS_LOCK_NOT_GRANTED);

	torture_wait_for_oplock_break(tctx);
	CHECK_VAL(break_info.count, 0);
	CHECK_VAL(break_info.level, 0);
	CHECK_VAL(break_info.fnum, 0);
	CHECK_VAL(break_info.failures, 0);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli1->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

/*
 * Open a file with an exclusive oplock from the 1st client and acquire a
 * brl. Then open the same file from the 2nd client that should give oplock
 * break with level2 to the 1st and return no oplock to the 2nd.
 */
static bool test_raw_oplock_brl4(struct torture_context *tctx,
				 struct smbcli_state *cli1,
				 struct smbcli_state *cli2)
{
	const char *fname = BASEDIR "\\test_batch_brl.dat";
	bool ret = true;
	uint8_t buf[1000];
	bool correct = true;
	union smb_open io;
	NTSTATUS status;
	uint16_t fnum = 0;
	uint16_t fnum2 = 0;

	if (!torture_setup_dir(cli1, BASEDIR)) {
		return false;
	}

	/* cleanup */
	smbcli_unlink(cli1->tree, fname);

	smbcli_oplock_handler(cli1->transport, oplock_handler_ack_to_given,
			      cli1->tree);

	/*
	  base ntcreatex parms
	*/
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_READ |
				      SEC_RIGHTS_FILE_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ |
				       NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;

	torture_comment(tctx, "open with exclusive oplock\n");
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED |
		NTCREATEX_FLAGS_REQUEST_OPLOCK;

	status = smb_raw_open(cli1->tree, tctx, &io);

	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, EXCLUSIVE_OPLOCK_RETURN);

	/* create a file with bogus data */
	memset(buf, 0, sizeof(buf));

	if (smbcli_write(cli1->tree, fnum, 0, buf, 0, sizeof(buf)) !=
			 sizeof(buf))
	{
		torture_comment(tctx, "Failed to create file\n");
		correct = false;
		goto done;
	}

	status = smbcli_lock(cli1->tree, fnum, 0, 1, 0, WRITE_LOCK);
	CHECK_STATUS(tctx, status, NT_STATUS_OK);

	torture_comment(tctx, "a 2nd open should give a break to the 1st\n");
	ZERO_STRUCT(break_info);

	status = smb_raw_open(cli2->tree, tctx, &io);

	CHECK_STATUS(tctx, status, NT_STATUS_OK);
	CHECK_VAL(break_info.count, 1);
	CHECK_VAL(break_info.level, OPLOCK_BREAK_TO_LEVEL_II);
	CHECK_VAL(break_info.failures, 0);
	CHECK_VAL(break_info.fnum, fnum);

	torture_comment(tctx, "and return no oplock to the 2nd\n");
	fnum2 = io.ntcreatex.out.file.fnum;
	CHECK_VAL(io.ntcreatex.out.oplock_level, NO_OPLOCK_RETURN);

	smbcli_close(cli1->tree, fnum);
	smbcli_close(cli2->tree, fnum2);

done:
	smb_raw_exit(cli1->session);
	smb_raw_exit(cli2->session);
	smbcli_deltree(cli1->tree, BASEDIR);
	return ret;
}

/*
   basic testing of oplocks
*/
struct torture_suite *torture_raw_oplock(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "oplock");

	torture_suite_add_2smb_test(suite, "exclusive1", test_raw_oplock_exclusive1);
	torture_suite_add_2smb_test(suite, "exclusive2", test_raw_oplock_exclusive2);
	torture_suite_add_2smb_test(suite, "exclusive3", test_raw_oplock_exclusive3);
	torture_suite_add_2smb_test(suite, "exclusive4", test_raw_oplock_exclusive4);
	torture_suite_add_2smb_test(suite, "exclusive5", test_raw_oplock_exclusive5);
	torture_suite_add_2smb_test(suite, "exclusive6", test_raw_oplock_exclusive6);
	torture_suite_add_2smb_test(suite, "exclusive7", test_raw_oplock_exclusive7);
	torture_suite_add_2smb_test(suite, "batch1", test_raw_oplock_batch1);
	torture_suite_add_2smb_test(suite, "batch2", test_raw_oplock_batch2);
	torture_suite_add_2smb_test(suite, "batch3", test_raw_oplock_batch3);
	torture_suite_add_2smb_test(suite, "batch4", test_raw_oplock_batch4);
	torture_suite_add_2smb_test(suite, "batch5", test_raw_oplock_batch5);
	torture_suite_add_2smb_test(suite, "batch6", test_raw_oplock_batch6);
	torture_suite_add_2smb_test(suite, "batch7", test_raw_oplock_batch7);
	torture_suite_add_2smb_test(suite, "batch8", test_raw_oplock_batch8);
	torture_suite_add_2smb_test(suite, "batch9", test_raw_oplock_batch9);
	torture_suite_add_2smb_test(suite, "batch10", test_raw_oplock_batch10);
	torture_suite_add_2smb_test(suite, "batch11", test_raw_oplock_batch11);
	torture_suite_add_2smb_test(suite, "batch12", test_raw_oplock_batch12);
	torture_suite_add_2smb_test(suite, "batch13", test_raw_oplock_batch13);
	torture_suite_add_2smb_test(suite, "batch14", test_raw_oplock_batch14);
	torture_suite_add_2smb_test(suite, "batch15", test_raw_oplock_batch15);
	torture_suite_add_2smb_test(suite, "batch16", test_raw_oplock_batch16);
	torture_suite_add_2smb_test(suite, "batch17", test_raw_oplock_batch17);
	torture_suite_add_2smb_test(suite, "batch18", test_raw_oplock_batch18);
	torture_suite_add_2smb_test(suite, "batch19", test_raw_oplock_batch19);
	torture_suite_add_2smb_test(suite, "batch20", test_raw_oplock_batch20);
	torture_suite_add_2smb_test(suite, "batch21", test_raw_oplock_batch21);
	torture_suite_add_2smb_test(suite, "batch22", test_raw_oplock_batch22);
	torture_suite_add_2smb_test(suite, "batch23", test_raw_oplock_batch23);
	torture_suite_add_2smb_test(suite, "batch24", test_raw_oplock_batch24);
	torture_suite_add_2smb_test(suite, "batch25", test_raw_oplock_batch25);
	torture_suite_add_2smb_test(suite, "batch26", test_raw_oplock_batch26);
	torture_suite_add_2smb_test(suite, "stream1", test_raw_oplock_stream1);
	torture_suite_add_1smb_test(suite, "doc1", test_raw_oplock_doc);
	torture_suite_add_2smb_test(suite, "brl1", test_raw_oplock_brl1);
	torture_suite_add_1smb_test(suite, "brl2", test_raw_oplock_brl2);
	torture_suite_add_1smb_test(suite, "brl3", test_raw_oplock_brl3);
	torture_suite_add_2smb_test(suite, "brl4", test_raw_oplock_brl4);

	return suite;
}

/*
   stress testing of oplocks
*/
bool torture_bench_oplock(struct torture_context *torture)
{
	struct smbcli_state **cli;
	bool ret = true;
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	int torture_nprocs = torture_setting_int(torture, "nprocs", 4);
	int i, count=0;
	int timelimit = torture_setting_int(torture, "timelimit", 10);
	union smb_open io;
	struct timeval tv;

	cli = talloc_array(mem_ctx, struct smbcli_state *, torture_nprocs);

	torture_comment(torture, "Opening %d connections\n", torture_nprocs);
	for (i=0;i<torture_nprocs;i++) {
		if (!torture_open_connection_ev(&cli[i], i, torture, torture->ev)) {
			return false;
		}
		talloc_steal(mem_ctx, cli[i]);
		smbcli_oplock_handler(cli[i]->transport, oplock_handler_close, 
				      cli[i]->tree);
	}

	if (!torture_setup_dir(cli[0], BASEDIR)) {
		ret = false;
		goto done;
	}

	io.ntcreatex.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = BASEDIR "\\test.dat";
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
		NTCREATEX_FLAGS_REQUEST_OPLOCK | 
		NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;

	tv = timeval_current();	

	/*
	  we open the same file with SHARE_ACCESS_NONE from all the
	  connections in a round robin fashion. Each open causes an
	  oplock break on the previous connection, which is answered
	  by the oplock_handler_close() to close the file.

	  This measures how fast we can pass on oplocks, and stresses
	  the oplock handling code
	*/
	torture_comment(torture, "Running for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		for (i=0;i<torture_nprocs;i++) {
			NTSTATUS status;

			status = smb_raw_open(cli[i]->tree, mem_ctx, &io);
			CHECK_STATUS(torture, status, NT_STATUS_OK);
			count++;
		}

		if (torture_setting_bool(torture, "progress", true)) {
			torture_comment(torture, "%.2f ops/second\r", count/timeval_elapsed(&tv));
		}
	}

	torture_comment(torture, "%.2f ops/second\n", count/timeval_elapsed(&tv));

	smb_raw_exit(cli[torture_nprocs-1]->session);
	
done:
	smb_raw_exit(cli[0]->session);
	smbcli_deltree(cli[0]->tree, BASEDIR);
	talloc_free(mem_ctx);
	return ret;
}


static struct hold_oplock_info {
	const char *fname;
	bool close_on_break;
	uint32_t share_access;
	uint16_t fnum;
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

static bool oplock_handler_hold(struct smbcli_transport *transport, 
				uint16_t tid, uint16_t fnum, uint8_t level, 
				void *private_data)
{
	struct smbcli_tree *tree = (struct smbcli_tree *)private_data;
	struct hold_oplock_info *info;
	int i;

	for (i=0;i<ARRAY_SIZE(hold_info);i++) {
		if (hold_info[i].fnum == fnum) break;
	}

	if (i == ARRAY_SIZE(hold_info)) {
		printf("oplock break for unknown fnum %u\n", fnum);
		return false;
	}

	info = &hold_info[i];

	if (info->close_on_break) {
		printf("oplock break on %s - closing\n",
		       info->fname);
		oplock_handler_close(transport, tid, fnum, level, private_data);
		return true;
	}

	printf("oplock break on %s - acking break\n", info->fname);

	return smbcli_oplock_ack(tree, fnum, OPLOCK_BREAK_TO_NONE);
}


/* 
   used for manual testing of oplocks - especially interaction with
   other filesystems (such as NFS and local access)
*/
bool torture_hold_oplock(struct torture_context *torture, 
			 struct smbcli_state *cli)
{
	struct tevent_context *ev = 
		(struct tevent_context *)cli->transport->socket->event.ctx;
	int i;

	printf("Setting up open files with oplocks in %s\n", BASEDIR);

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	smbcli_oplock_handler(cli->transport, oplock_handler_hold, cli->tree);

	/* setup the files */
	for (i=0;i<ARRAY_SIZE(hold_info);i++) {
		union smb_open io;
		NTSTATUS status;
		char c = 1;

		io.generic.level = RAW_OPEN_NTCREATEX;
		io.ntcreatex.in.root_fid.fnum = 0;
		io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
		io.ntcreatex.in.alloc_size = 0;
		io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
		io.ntcreatex.in.share_access = hold_info[i].share_access;
		io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN_IF;
		io.ntcreatex.in.create_options = 0;
		io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
		io.ntcreatex.in.security_flags = 0;
		io.ntcreatex.in.fname = hold_info[i].fname;
		io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED | 
			NTCREATEX_FLAGS_REQUEST_OPLOCK |
			NTCREATEX_FLAGS_REQUEST_BATCH_OPLOCK;
		printf("opening %s\n", hold_info[i].fname);

		status = smb_raw_open(cli->tree, cli, &io);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Failed to open %s - %s\n", 
			       hold_info[i].fname, nt_errstr(status));
			return false;
		}

		if (io.ntcreatex.out.oplock_level != BATCH_OPLOCK_RETURN) {
			printf("Oplock not granted for %s - expected %d but got %d\n", 
			       hold_info[i].fname, BATCH_OPLOCK_RETURN, 
				io.ntcreatex.out.oplock_level);
			return false;
		}
		hold_info[i].fnum = io.ntcreatex.out.file.fnum;

		/* make the file non-zero size */
		if (smbcli_write(cli->tree, hold_info[i].fnum, 0, &c, 0, 1) != 1) {
			printf("Failed to write to file\n");
			return false;
		}
	}

	printf("Waiting for oplock events\n");
	event_loop_wait(ev);

	return true;
}
