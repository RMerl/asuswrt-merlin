/* 
   Unix SMB/CIFS implementation.
   test suite for session setup operations
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
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "lib/cmdline/popt_common.h"
#include "lib/events/events.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "auth/credentials/credentials.h"
#include "param/param.h"

#define BASEDIR "\\rawcontext"

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

#define CHECK_NOT_VALUE(v, correct) do { \
	if ((v) == (correct)) { \
		printf("(%s) Incorrect value %s=%d - should not be %d\n", \
		       __location__, #v, v, correct); \
		ret = false; \
		goto done; \
	}} while (0)


/*
  test session ops
*/
static bool test_session(struct smbcli_state *cli, struct torture_context *tctx)
{
	NTSTATUS status;
	bool ret = true;
	struct smbcli_session *session;
	struct smbcli_session *session2;
	struct smbcli_session *session3;
	struct smbcli_session *session4;
	struct cli_credentials *anon_creds;
	struct smbcli_session *sessions[15];
	struct composite_context *composite_contexts[15];
	struct smbcli_tree *tree;
	struct smb_composite_sesssetup setup;
	struct smb_composite_sesssetup setups[15];
	struct gensec_settings *gensec_settings;
	union smb_open io;
	union smb_write wr;
	union smb_close cl;
	int fnum;
	const char *fname = BASEDIR "\\test.txt";
	uint8_t c = 1;
	int i;
	struct smbcli_session_options options;

	printf("TESTING SESSION HANDLING\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	printf("create a second security context on the same transport\n");

	lp_smbcli_session_options(tctx->lp_ctx, &options);
	gensec_settings = lp_gensec_settings(tctx, tctx->lp_ctx);

	session = smbcli_session_init(cli->transport, tctx, false, options);

	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities; /* ignored in secondary session setup, except by our libs, which care about the extended security bit */
	setup.in.workgroup = lp_workgroup(tctx->lp_ctx);

	setup.in.credentials = cmdline_credentials;
	setup.in.gensec_settings = gensec_settings;

	status = smb_composite_sesssetup(session, &setup);
	CHECK_STATUS(status, NT_STATUS_OK);
	
	session->vuid = setup.out.vuid;

	printf("create a third security context on the same transport, with vuid set\n");
	session2 = smbcli_session_init(cli->transport, tctx, false, options);

	session2->vuid = session->vuid;
	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities; /* ignored in secondary session setup, except by our libs, which care about the extended security bit */
	setup.in.workgroup = lp_workgroup(tctx->lp_ctx);

	setup.in.credentials = cmdline_credentials;

	status = smb_composite_sesssetup(session2, &setup);
	CHECK_STATUS(status, NT_STATUS_OK);

	session2->vuid = setup.out.vuid;
	printf("vuid1=%d vuid2=%d vuid3=%d\n", cli->session->vuid, session->vuid, session2->vuid);
	
	if (cli->transport->negotiate.capabilities & CAP_EXTENDED_SECURITY) {
		/* Samba4 currently fails this - we need to determine if this insane behaviour is important */
		if (session2->vuid == session->vuid) {
			printf("server allows the user to re-use an existing vuid in session setup \n");
		}
	} else {
		CHECK_NOT_VALUE(session2->vuid, session->vuid);
	}
	talloc_free(session2);

	if (cli->transport->negotiate.capabilities & CAP_EXTENDED_SECURITY) {
		printf("create a fourth security context on the same transport, without extended security\n");
		session3 = smbcli_session_init(cli->transport, tctx, false, options);

		session3->vuid = session->vuid;
		setup.in.sesskey = cli->transport->negotiate.sesskey;
		setup.in.capabilities &= ~CAP_EXTENDED_SECURITY; /* force a non extended security login (should fail) */
		setup.in.workgroup = lp_workgroup(tctx->lp_ctx);
	
		setup.in.credentials = cmdline_credentials;

		status = smb_composite_sesssetup(session3, &setup);
		CHECK_STATUS(status, NT_STATUS_LOGON_FAILURE);

		printf("create a fouth anonymous security context on the same transport, without extended security\n");
		session4 = smbcli_session_init(cli->transport, tctx, false, options);

		session4->vuid = session->vuid;
		setup.in.sesskey = cli->transport->negotiate.sesskey;
		setup.in.capabilities &= ~CAP_EXTENDED_SECURITY; /* force a non extended security login (should fail) */
		setup.in.workgroup = lp_workgroup(tctx->lp_ctx);
		
		anon_creds = cli_credentials_init(tctx);
		cli_credentials_set_conf(anon_creds, tctx->lp_ctx);
		cli_credentials_set_anonymous(anon_creds);

		setup.in.credentials = anon_creds;
	
		status = smb_composite_sesssetup(session3, &setup);
		CHECK_STATUS(status, NT_STATUS_OK);

		talloc_free(session4);
	}
		
	printf("use the same tree as the existing connection\n");
	tree = smbcli_tree_init(session, tctx, false);
	tree->tid = cli->tree->tid;

	printf("create a file using the new vuid\n");
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("write using the old vuid\n");
	wr.generic.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum;
	wr.writex.in.offset = 0;
	wr.writex.in.wmode = 0;
	wr.writex.in.remaining = 0;
	wr.writex.in.count = 1;
	wr.writex.in.data = &c;

	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("write with the new vuid\n");
	status = smb_raw_write(tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("logoff the new vuid\n");
	status = smb_raw_ulogoff(session);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("the new vuid should not now be accessible\n");
	status = smb_raw_write(tree, &wr);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("second logoff for the new vuid should fail\n");
	status = smb_raw_ulogoff(session);
	CHECK_STATUS(status, NT_STATUS_DOS(ERRSRV, ERRbaduid));
	talloc_free(session);

	printf("the fnum should have been auto-closed\n");
	cl.close.level = RAW_CLOSE_CLOSE;
	cl.close.in.file.fnum = fnum;
	cl.close.in.write_time = 0;
	status = smb_raw_close(cli->tree, &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("create %d secondary security contexts on the same transport\n", 
	       (int)ARRAY_SIZE(sessions));
	for (i=0; i <ARRAY_SIZE(sessions); i++) {
		setups[i].in.sesskey = cli->transport->negotiate.sesskey;
		setups[i].in.capabilities = cli->transport->negotiate.capabilities; /* ignored in secondary session setup, except by our libs, which care about the extended security bit */
		setups[i].in.workgroup = lp_workgroup(tctx->lp_ctx);
		
		setups[i].in.credentials = cmdline_credentials;
		setups[i].in.gensec_settings = gensec_settings;

		sessions[i] = smbcli_session_init(cli->transport, tctx, false, options);
		composite_contexts[i] = smb_composite_sesssetup_send(sessions[i], &setups[i]);

	}


	printf("finishing %d secondary security contexts on the same transport\n", 
	       (int)ARRAY_SIZE(sessions));
	for (i=0; i< ARRAY_SIZE(sessions); i++) {
		status = smb_composite_sesssetup_recv(composite_contexts[i]);
		CHECK_STATUS(status, NT_STATUS_OK);
		sessions[i]->vuid = setups[i].out.vuid;
		printf("VUID: %d\n", sessions[i]->vuid);
		status = smb_raw_ulogoff(sessions[i]);
		CHECK_STATUS(status, NT_STATUS_OK);
	}


	talloc_free(tree);
	
done:
	return ret;
}


/*
  test tree ops
*/
static bool test_tree(struct smbcli_state *cli, struct torture_context *tctx)
{
	NTSTATUS status;
	bool ret = true;
	const char *share, *host;
	struct smbcli_tree *tree;
	union smb_tcon tcon;
	union smb_open io;
	union smb_write wr;
	union smb_close cl;
	int fnum;
	const char *fname = BASEDIR "\\test.txt";
	uint8_t c = 1;

	printf("TESTING TREE HANDLING\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	share = torture_setting_string(tctx, "share", NULL);
	host  = torture_setting_string(tctx, "host", NULL);
	
	printf("create a second tree context on the same session\n");
	tree = smbcli_tree_init(cli->session, tctx, false);

	tcon.generic.level = RAW_TCON_TCONX;
	tcon.tconx.in.flags = 0;
	tcon.tconx.in.password = data_blob(NULL, 0);
	tcon.tconx.in.path = talloc_asprintf(tctx, "\\\\%s\\%s", host, share);
	tcon.tconx.in.device = "A:";	
	status = smb_raw_tcon(tree, tctx, &tcon);
	CHECK_STATUS(status, NT_STATUS_OK);
	

	tree->tid = tcon.tconx.out.tid;
	printf("tid1=%d tid2=%d\n", cli->tree->tid, tree->tid);

	printf("try a tconx with a bad device type\n");
	tcon.tconx.in.device = "FOO";	
	status = smb_raw_tcon(tree, tctx, &tcon);
	CHECK_STATUS(status, NT_STATUS_BAD_DEVICE_TYPE);


	printf("create a file using the new tid\n");
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("write using the old tid\n");
	wr.generic.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum;
	wr.writex.in.offset = 0;
	wr.writex.in.wmode = 0;
	wr.writex.in.remaining = 0;
	wr.writex.in.count = 1;
	wr.writex.in.data = &c;

	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("write with the new tid\n");
	status = smb_raw_write(tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("disconnect the new tid\n");
	status = smb_tree_disconnect(tree);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("the new tid should not now be accessible\n");
	status = smb_raw_write(tree, &wr);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("the fnum should have been auto-closed\n");
	cl.close.level = RAW_CLOSE_CLOSE;
	cl.close.in.file.fnum = fnum;
	cl.close.in.write_time = 0;
	status = smb_raw_close(cli->tree, &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	/* close down the new tree */
	talloc_free(tree);
	
done:
	return ret;
}

/*
  test tree with ulogoff
  this demonstrates that a tcon isn't autoclosed by a ulogoff
  the tcon can be reused using any other valid session later
*/
static bool test_tree_ulogoff(struct smbcli_state *cli, struct torture_context *tctx)
{
	NTSTATUS status;
	bool ret = true;
	const char *share, *host;
	struct smbcli_session *session1;
	struct smbcli_session *session2;
	struct smb_composite_sesssetup setup;
	struct smbcli_tree *tree;
	union smb_tcon tcon;
	union smb_open io;
	union smb_write wr;
	int fnum1, fnum2;
	const char *fname1 = BASEDIR "\\test1.txt";
	const char *fname2 = BASEDIR "\\test2.txt";
	uint8_t c = 1;
	struct smbcli_session_options options;

	printf("TESTING TREE with ulogoff\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	share = torture_setting_string(tctx, "share", NULL);
	host  = torture_setting_string(tctx, "host", NULL);

	lp_smbcli_session_options(tctx->lp_ctx, &options);

	printf("create the first new sessions\n");
	session1 = smbcli_session_init(cli->transport, tctx, false, options);
	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities;
	setup.in.workgroup = lp_workgroup(tctx->lp_ctx);
	setup.in.credentials = cmdline_credentials;
	setup.in.gensec_settings = lp_gensec_settings(tctx, tctx->lp_ctx);
	status = smb_composite_sesssetup(session1, &setup);
	CHECK_STATUS(status, NT_STATUS_OK);
	session1->vuid = setup.out.vuid;
	printf("vuid1=%d\n", session1->vuid);

	printf("create a tree context on the with vuid1\n");
	tree = smbcli_tree_init(session1, tctx, false);
	tcon.generic.level = RAW_TCON_TCONX;
	tcon.tconx.in.flags = 0;
	tcon.tconx.in.password = data_blob(NULL, 0);
	tcon.tconx.in.path = talloc_asprintf(tctx, "\\\\%s\\%s", host, share);
	tcon.tconx.in.device = "A:";
	status = smb_raw_tcon(tree, tctx, &tcon);
	CHECK_STATUS(status, NT_STATUS_OK);
	tree->tid = tcon.tconx.out.tid;
	printf("tid=%d\n", tree->tid);

	printf("create a file using vuid1\n");
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;
	status = smb_raw_open(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum1 = io.ntcreatex.out.file.fnum;

	printf("write using vuid1\n");
	wr.generic.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum1;
	wr.writex.in.offset = 0;
	wr.writex.in.wmode = 0;
	wr.writex.in.remaining = 0;
	wr.writex.in.count = 1;
	wr.writex.in.data = &c;
	status = smb_raw_write(tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("ulogoff the vuid1\n");
	status = smb_raw_ulogoff(session1);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("create the second new sessions\n");
	session2 = smbcli_session_init(cli->transport, tctx, false, options);
	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities;
	setup.in.workgroup = lp_workgroup(tctx->lp_ctx);
	setup.in.credentials = cmdline_credentials;
	setup.in.gensec_settings = lp_gensec_settings(tctx, tctx->lp_ctx);
	status = smb_composite_sesssetup(session2, &setup);
	CHECK_STATUS(status, NT_STATUS_OK);
	session2->vuid = setup.out.vuid;
	printf("vuid2=%d\n", session2->vuid);

	printf("use the existing tree with vuid2\n");
	tree->session = session2;

	printf("create a file using vuid2\n");
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname2;
	status = smb_raw_open(tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;

	printf("write using vuid2\n");
	wr.generic.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum2;
	wr.writex.in.offset = 0;
	wr.writex.in.wmode = 0;
	wr.writex.in.remaining = 0;
	wr.writex.in.count = 1;
	wr.writex.in.data = &c;
	status = smb_raw_write(tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("ulogoff the vuid2\n");
	status = smb_raw_ulogoff(session2);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* this also demonstrates that SMBtdis doesn't need a valid vuid */
	printf("disconnect the existing tree connection\n");
	status = smb_tree_disconnect(tree);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("disconnect the existing tree connection\n");
	status = smb_tree_disconnect(tree);
	CHECK_STATUS(status, NT_STATUS_DOS(ERRSRV,ERRinvnid));

	/* close down the new tree */
	talloc_free(tree);
	
done:
	return ret;
}

/*
  test pid ops
  this test demonstrates that exit() only sees the PID
  used for the open() calls
*/
static bool test_pid_exit_only_sees_open(struct smbcli_state *cli, TALLOC_CTX *mem_ctx)
{
	NTSTATUS status;
	bool ret = true;
	union smb_open io;
	union smb_write wr;
	union smb_close cl;
	int fnum;
	const char *fname = BASEDIR "\\test.txt";
	uint8_t c = 1;
	uint16_t pid1, pid2;

	printf("TESTING PID HANDLING exit() only cares about open() PID\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	pid1 = cli->session->pid;
	pid2 = pid1 + 1;

	printf("pid1=%d pid2=%d\n", pid1, pid2);

	printf("create a file using pid1\n");
	cli->session->pid = pid1;
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, mem_ctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("write using pid2\n");
	cli->session->pid = pid2;
	wr.generic.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum;
	wr.writex.in.offset = 0;
	wr.writex.in.wmode = 0;
	wr.writex.in.remaining = 0;
	wr.writex.in.count = 1;
	wr.writex.in.data = &c;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("exit pid2\n");
	cli->session->pid = pid2;
	status = smb_raw_exit(cli->session);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("the fnum should still be accessible via pid2\n");
	cli->session->pid = pid2;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("exit pid2\n");
	cli->session->pid = pid2;
	status = smb_raw_exit(cli->session);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("the fnum should still be accessible via pid1 and pid2\n");
	cli->session->pid = pid1;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);
	cli->session->pid = pid2;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("exit pid1\n");
	cli->session->pid = pid1;
	status = smb_raw_exit(cli->session);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("the fnum should not now be accessible via pid1 or pid2\n");
	cli->session->pid = pid1;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);
	cli->session->pid = pid2;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("the fnum should have been auto-closed\n");
	cli->session->pid = pid1;
	cl.close.level = RAW_CLOSE_CLOSE;
	cl.close.in.file.fnum = fnum;
	cl.close.in.write_time = 0;
	status = smb_raw_close(cli->tree, &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

done:
	return ret;
}

/*
  test pid ops with 2 sessions
*/
static bool test_pid_2sess(struct smbcli_state *cli, struct torture_context *tctx)
{
	NTSTATUS status;
	bool ret = true;
	struct smbcli_session *session;
	struct smb_composite_sesssetup setup;
	union smb_open io;
	union smb_write wr;
	union smb_close cl;
	int fnum;
	const char *fname = BASEDIR "\\test.txt";
	uint8_t c = 1;
	uint16_t vuid1, vuid2;
	struct smbcli_session_options options;

	printf("TESTING PID HANDLING WITH 2 SESSIONS\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	lp_smbcli_session_options(tctx->lp_ctx, &options);

	printf("create a second security context on the same transport\n");
	session = smbcli_session_init(cli->transport, tctx, false, options);

	setup.in.sesskey = cli->transport->negotiate.sesskey;
	setup.in.capabilities = cli->transport->negotiate.capabilities; /* ignored in secondary session setup, except by our libs, which care about the extended security bit */
	setup.in.workgroup = lp_workgroup(tctx->lp_ctx);
	setup.in.credentials = cmdline_credentials;
	setup.in.gensec_settings = lp_gensec_settings(tctx, tctx->lp_ctx);

	status = smb_composite_sesssetup(session, &setup);
	CHECK_STATUS(status, NT_STATUS_OK);	
	session->vuid = setup.out.vuid;

	vuid1 = cli->session->vuid;
	vuid2 = session->vuid;

	printf("vuid1=%d vuid2=%d\n", vuid1, vuid2);

	printf("create a file using the vuid1\n");
	cli->session->vuid = vuid1;
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum = io.ntcreatex.out.file.fnum;

	printf("write using the vuid1 (fnum=%d)\n", fnum);
	cli->session->vuid = vuid1;
	wr.generic.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum;
	wr.writex.in.offset = 0;
	wr.writex.in.wmode = 0;
	wr.writex.in.remaining = 0;
	wr.writex.in.count = 1;
	wr.writex.in.data = &c;

	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("exit the pid with vuid2\n");
	cli->session->vuid = vuid2;
	status = smb_raw_exit(cli->session);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("the fnum should still be accessible\n");
	cli->session->vuid = vuid1;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("exit the pid with vuid1\n");
	cli->session->vuid = vuid1;
	status = smb_raw_exit(cli->session);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("the fnum should not now be accessible\n");
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("the fnum should have been auto-closed\n");
	cl.close.level = RAW_CLOSE_CLOSE;
	cl.close.in.file.fnum = fnum;
	cl.close.in.write_time = 0;
	status = smb_raw_close(cli->tree, &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

done:
	return ret;
}

/*
  test pid ops with 2 tcons
*/
static bool test_pid_2tcon(struct smbcli_state *cli, struct torture_context *tctx)
{
	NTSTATUS status;
	bool ret = true;
	const char *share, *host;
	struct smbcli_tree *tree;
	union smb_tcon tcon;
	union smb_open io;
	union smb_write wr;
	union smb_close cl;
	int fnum1, fnum2;
	const char *fname1 = BASEDIR "\\test1.txt";
	const char *fname2 = BASEDIR "\\test2.txt";
	uint8_t c = 1;
	uint16_t tid1, tid2;

	printf("TESTING PID HANDLING WITH 2 TCONS\n");

	if (!torture_setup_dir(cli, BASEDIR)) {
		return false;
	}

	share = torture_setting_string(tctx, "share", NULL);
	host  = torture_setting_string(tctx, "host", NULL);
	
	printf("create a second tree context on the same session\n");
	tree = smbcli_tree_init(cli->session, tctx, false);

	tcon.generic.level = RAW_TCON_TCONX;
	tcon.tconx.in.flags = 0;
	tcon.tconx.in.password = data_blob(NULL, 0);
	tcon.tconx.in.path = talloc_asprintf(tctx, "\\\\%s\\%s", host, share);
	tcon.tconx.in.device = "A:";	
	status = smb_raw_tcon(tree, tctx, &tcon);
	CHECK_STATUS(status, NT_STATUS_OK);	

	tree->tid = tcon.tconx.out.tid;

	tid1 = cli->tree->tid;
	tid2 = tree->tid;
	printf("tid1=%d tid2=%d\n", tid1, tid2);

	printf("create a file using the tid1\n");
	cli->tree->tid = tid1;
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname1;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum1 = io.ntcreatex.out.file.fnum;

	printf("write using the tid1\n");
	wr.generic.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum1;
	wr.writex.in.offset = 0;
	wr.writex.in.wmode = 0;
	wr.writex.in.remaining = 0;
	wr.writex.in.count = 1;
	wr.writex.in.data = &c;

	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("create a file using the tid2\n");
	cli->tree->tid = tid2;
	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.root_fid = 0;
	io.ntcreatex.in.flags = 0;
	io.ntcreatex.in.access_mask = SEC_FLAG_MAXIMUM_ALLOWED;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_WRITE;
	io.ntcreatex.in.alloc_size = 0;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = fname2;
	status = smb_raw_open(cli->tree, tctx, &io);
	CHECK_STATUS(status, NT_STATUS_OK);
	fnum2 = io.ntcreatex.out.file.fnum;

	printf("write using the tid2\n");
	wr.generic.level = RAW_WRITE_WRITEX;
	wr.writex.in.file.fnum = fnum2;
	wr.writex.in.offset = 0;
	wr.writex.in.wmode = 0;
	wr.writex.in.remaining = 0;
	wr.writex.in.count = 1;
	wr.writex.in.data = &c;

	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(wr.writex.out.nwritten, 1);

	printf("exit the pid\n");
	status = smb_raw_exit(cli->session);
	CHECK_STATUS(status, NT_STATUS_OK);

	printf("the fnum1 on tid1 should not be accessible\n");
	cli->tree->tid = tid1;
	wr.writex.in.file.fnum = fnum1;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("the fnum1 on tid1 should have been auto-closed\n");
	cl.close.level = RAW_CLOSE_CLOSE;
	cl.close.in.file.fnum = fnum1;
	cl.close.in.write_time = 0;
	status = smb_raw_close(cli->tree, &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("the fnum2 on tid2 should not be accessible\n");
	cli->tree->tid = tid2;
	wr.writex.in.file.fnum = fnum2;
	status = smb_raw_write(cli->tree, &wr);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

	printf("the fnum2 on tid2 should have been auto-closed\n");
	cl.close.level = RAW_CLOSE_CLOSE;
	cl.close.in.file.fnum = fnum2;
	cl.close.in.write_time = 0;
	status = smb_raw_close(cli->tree, &cl);
	CHECK_STATUS(status, NT_STATUS_INVALID_HANDLE);

done:
	return ret;
}


/* 
   basic testing of session/tree context calls
*/
static bool torture_raw_context_int(struct torture_context *tctx, 
									struct smbcli_state *cli)
{
	bool ret = true;

	ret &= test_session(cli, tctx);
	ret &= test_tree(cli, tctx);
	ret &= test_tree_ulogoff(cli, tctx);
	ret &= test_pid_exit_only_sees_open(cli, tctx);
	ret &= test_pid_2sess(cli, tctx);
	ret &= test_pid_2tcon(cli, tctx);

	smb_raw_exit(cli->session);
	smbcli_deltree(cli->tree, BASEDIR);

	return ret;
}
/* 
   basic testing of session/tree context calls
*/
bool torture_raw_context(struct torture_context *torture, 
			 struct smbcli_state *cli)
{
	bool ret = true;
	if (lp_use_spnego(torture->lp_ctx)) {
		ret &= torture_raw_context_int(torture, cli);
		lp_set_cmdline(torture->lp_ctx, "use spnego", "False");
	}

	ret &= torture_raw_context_int(torture, cli);

	return ret;
}
