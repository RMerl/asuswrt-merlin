/* 
   Unix SMB/CIFS implementation.
   SMB torture tester
   Copyright (C) Andrew Tridgell 1997-2003
   Copyright (C) Jelmer Vernooij 2006
   
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
#include "torture/smbtorture.h"
#include "torture/basic/proto.h"
#include "libcli/libcli.h"
#include "libcli/raw/raw_proto.h"
#include "torture/util.h"
#include "system/filesys.h"
#include "system/time.h"
#include "libcli/resolve/resolve.h"
#include "lib/events/events.h"
#include "param/param.h"


#define CHECK_MAX_FAILURES(label) do { if (++failures >= torture_failures) goto label; } while (0)


static struct smbcli_state *open_nbt_connection(struct torture_context *tctx)
{
	struct nbt_name called, calling;
	struct smbcli_state *cli;
	const char *host = torture_setting_string(tctx, "host", NULL);
	struct smbcli_options options;

	make_nbt_name_client(&calling, lpcfg_netbios_name(tctx->lp_ctx));

	nbt_choose_called_name(NULL, &called, host, NBT_NAME_SERVER);

	cli = smbcli_state_init(NULL);
	if (!cli) {
		torture_comment(tctx, "Failed initialize smbcli_struct to connect with %s\n", host);
		goto failed;
	}

	lpcfg_smbcli_options(tctx->lp_ctx, &options);

	if (!smbcli_socket_connect(cli, host, lpcfg_smb_ports(tctx->lp_ctx), tctx->ev,
				   lpcfg_resolve_context(tctx->lp_ctx), &options,
                   lpcfg_socket_options(tctx->lp_ctx))) {
		torture_comment(tctx, "Failed to connect with %s\n", host);
		goto failed;
	}

	if (!smbcli_transport_establish(cli, &calling, &called)) {
		torture_comment(tctx, "%s rejected the session\n",host);
		goto failed;
	}

	return cli;

failed:
	talloc_free(cli);
	return NULL;
}

static bool tcon_devtest(struct torture_context *tctx, 
						 struct smbcli_state *cli,
			 			 const char *myshare, const char *devtype,
			 			 NTSTATUS expected_error)
{
	bool status;
	const char *password = torture_setting_string(tctx, "password", NULL);

	status = NT_STATUS_IS_OK(smbcli_tconX(cli, myshare, devtype, 
						password));

	torture_comment(tctx, "Trying share %s with devtype %s\n", myshare, devtype);

	if (NT_STATUS_IS_OK(expected_error)) {
		if (!status) {
			torture_fail(tctx, talloc_asprintf(tctx, 
				   "tconX to share %s with type %s "
			       "should have succeeded but failed",
			       myshare, devtype));
		}
		smbcli_tdis(cli);
	} else {
		if (status) {
			torture_fail(tctx, talloc_asprintf(tctx, 
				   "tconx to share %s with type %s "
			       "should have failed but succeeded",
			       myshare, devtype));
		} else {
			if (NT_STATUS_EQUAL(smbcli_nt_error(cli->tree),
					    expected_error)) {
			} else {
				torture_fail(tctx, "Returned unexpected error");
			}
		}
	}
	return true;
}



/**
test whether fnums and tids open on one VC are available on another (a major
security hole)
*/
static bool run_fdpasstest(struct torture_context *tctx,
						   struct smbcli_state *cli1, 
						   struct smbcli_state *cli2)
{
	const char *fname = "\\fdpass.tst";
	int fnum1, oldtid;
	uint8_t buf[1024];

	smbcli_unlink(cli1->tree, fname);

	torture_comment(tctx, "Opening a file on connection 1\n");

	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	torture_assert(tctx, fnum1 != -1, 
			talloc_asprintf(tctx, 
			"open of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree)));

	torture_comment(tctx, "writing to file on connection 1\n");

	torture_assert(tctx, 
		smbcli_write(cli1->tree, fnum1, 0, "hello world\n", 0, 13) == 13,
		talloc_asprintf(tctx, 
		"write failed (%s)\n", smbcli_errstr(cli1->tree)));

	oldtid = cli2->tree->tid;
	cli2->session->vuid = cli1->session->vuid;
	cli2->tree->tid = cli1->tree->tid;
	cli2->session->pid = cli1->session->pid;

	torture_comment(tctx, "reading from file on connection 2\n");

	torture_assert(tctx, smbcli_read(cli2->tree, fnum1, buf, 0, 13) != 13,
				   talloc_asprintf(tctx,
		"read succeeded! nasty security hole [%s]\n", buf));

	smbcli_close(cli1->tree, fnum1);
	smbcli_unlink(cli1->tree, fname);

	cli2->tree->tid = oldtid;

	return true;
}

/**
  This checks how the getatr calls works
*/
static bool run_attrtest(struct torture_context *tctx, 
						 struct smbcli_state *cli)
{
	int fnum;
	time_t t, t2;
	const char *fname = "\\attrib123456789.tst";
	bool correct = true;

	smbcli_unlink(cli->tree, fname);
	fnum = smbcli_open(cli->tree, fname, 
			O_RDWR | O_CREAT | O_TRUNC, DENY_NONE);
	smbcli_close(cli->tree, fnum);

	if (NT_STATUS_IS_ERR(smbcli_getatr(cli->tree, fname, NULL, NULL, &t))) {
		torture_comment(tctx, "getatr failed (%s)\n", smbcli_errstr(cli->tree));
		correct = false;
	}

	torture_comment(tctx, "New file time is %s", ctime(&t));

	if (abs(t - time(NULL)) > 60*60*24*10) {
		torture_comment(tctx, "ERROR: SMBgetatr bug. time is %s",
		       ctime(&t));
		t = time(NULL);
		correct = false;
	}

	t2 = t-60*60*24; /* 1 day ago */

	torture_comment(tctx, "Setting file time to %s", ctime(&t2));

	if (NT_STATUS_IS_ERR(smbcli_setatr(cli->tree, fname, 0, t2))) {
		torture_comment(tctx, "setatr failed (%s)\n", smbcli_errstr(cli->tree));
		correct = true;
	}

	if (NT_STATUS_IS_ERR(smbcli_getatr(cli->tree, fname, NULL, NULL, &t))) {
		torture_comment(tctx, "getatr failed (%s)\n", smbcli_errstr(cli->tree));
		correct = true;
	}

	torture_comment(tctx, "Retrieved file time as %s", ctime(&t));

	if (t != t2) {
		torture_comment(tctx, "ERROR: getatr/setatr bug. times are\n%s",
		       ctime(&t));
		torture_comment(tctx, "%s", ctime(&t2));
		correct = true;
	}

	smbcli_unlink(cli->tree, fname);

	return correct;
}

/**
  This checks a couple of trans2 calls
*/
static bool run_trans2test(struct torture_context *tctx, 
						   struct smbcli_state *cli)
{
	int fnum;
	size_t size;
	time_t c_time, a_time, m_time, w_time, m_time2;
	const char *fname = "\\trans2.tst";
	const char *dname = "\\trans2";
	const char *fname2 = "\\trans2\\trans2.tst";
	const char *pname;
	bool correct = true;

	smbcli_unlink(cli->tree, fname);

	torture_comment(tctx, "Testing qfileinfo\n");
	
	fnum = smbcli_open(cli->tree, fname, 
			O_RDWR | O_CREAT | O_TRUNC, DENY_NONE);
	if (NT_STATUS_IS_ERR(smbcli_qfileinfo(cli->tree, fnum, NULL, &size, &c_time, &a_time, &m_time,
			   NULL, NULL))) {
		torture_comment(tctx, "ERROR: qfileinfo failed (%s)\n", smbcli_errstr(cli->tree));
		correct = false;
	}

	torture_comment(tctx, "Testing NAME_INFO\n");

	if (NT_STATUS_IS_ERR(smbcli_qfilename(cli->tree, fnum, &pname))) {
		torture_comment(tctx, "ERROR: qfilename failed (%s)\n", smbcli_errstr(cli->tree));
		correct = false;
	}

	if (!pname || strcmp(pname, fname)) {
		torture_comment(tctx, "qfilename gave different name? [%s] [%s]\n",
		       fname, pname);
		correct = false;
	}

	smbcli_close(cli->tree, fnum);
	smbcli_unlink(cli->tree, fname);

	fnum = smbcli_open(cli->tree, fname, 
			O_RDWR | O_CREAT | O_TRUNC, DENY_NONE);
	if (fnum == -1) {
		torture_comment(tctx, "open of %s failed (%s)\n", fname, smbcli_errstr(cli->tree));
		return false;
	}
	smbcli_close(cli->tree, fnum);

	torture_comment(tctx, "Checking for sticky create times\n");

	if (NT_STATUS_IS_ERR(smbcli_qpathinfo(cli->tree, fname, &c_time, &a_time, &m_time, &size, NULL))) {
		torture_comment(tctx, "ERROR: qpathinfo failed (%s)\n", smbcli_errstr(cli->tree));
		correct = false;
	} else {
		if (c_time != m_time) {
			torture_comment(tctx, "create time=%s", ctime(&c_time));
			torture_comment(tctx, "modify time=%s", ctime(&m_time));
			torture_comment(tctx, "This system appears to have sticky create times\n");
		}
		if (a_time % (60*60) == 0) {
			torture_comment(tctx, "access time=%s", ctime(&a_time));
			torture_comment(tctx, "This system appears to set a midnight access time\n");
			correct = false;
		}

		if (abs(m_time - time(NULL)) > 60*60*24*7) {
			torture_comment(tctx, "ERROR: totally incorrect times - maybe word reversed? mtime=%s", ctime(&m_time));
			correct = false;
		}
	}


	smbcli_unlink(cli->tree, fname);
	fnum = smbcli_open(cli->tree, fname, 
			O_RDWR | O_CREAT | O_TRUNC, DENY_NONE);
	smbcli_close(cli->tree, fnum);
	if (NT_STATUS_IS_ERR(smbcli_qpathinfo2(cli->tree, fname, &c_time, &a_time, &m_time, &w_time, &size, NULL, NULL))) {
		torture_comment(tctx, "ERROR: qpathinfo2 failed (%s)\n", smbcli_errstr(cli->tree));
		correct = false;
	} else {
		if (w_time < 60*60*24*2) {
			torture_comment(tctx, "write time=%s", ctime(&w_time));
			torture_comment(tctx, "This system appears to set a initial 0 write time\n");
			correct = false;
		}
	}

	smbcli_unlink(cli->tree, fname);


	/* check if the server updates the directory modification time
           when creating a new file */
	if (NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, dname))) {
		torture_comment(tctx, "ERROR: mkdir failed (%s)\n", smbcli_errstr(cli->tree));
		correct = false;
	}
	sleep(3);
	if (NT_STATUS_IS_ERR(smbcli_qpathinfo2(cli->tree, "\\trans2\\", &c_time, &a_time, &m_time, &w_time, &size, NULL, NULL))) {
		torture_comment(tctx, "ERROR: qpathinfo2 failed (%s)\n", smbcli_errstr(cli->tree));
		correct = false;
	}

	fnum = smbcli_open(cli->tree, fname2, 
			O_RDWR | O_CREAT | O_TRUNC, DENY_NONE);
	smbcli_write(cli->tree, fnum,  0, &fnum, 0, sizeof(fnum));
	smbcli_close(cli->tree, fnum);
	if (NT_STATUS_IS_ERR(smbcli_qpathinfo2(cli->tree, "\\trans2\\", &c_time, &a_time, &m_time2, &w_time, &size, NULL, NULL))) {
		torture_comment(tctx, "ERROR: qpathinfo2 failed (%s)\n", smbcli_errstr(cli->tree));
		correct = false;
	} else {
		if (m_time2 == m_time) {
			torture_comment(tctx, "This system does not update directory modification times\n");
			correct = false;
		}
	}
	smbcli_unlink(cli->tree, fname2);
	smbcli_rmdir(cli->tree, dname);

	return correct;
}

/* send smb negprot commands, not reading the response */
static bool run_negprot_nowait(struct torture_context *tctx)
{
	int i;
	struct smbcli_state *cli, *cli2;
	bool correct = true;

	torture_comment(tctx, "starting negprot nowait test\n");

	cli = open_nbt_connection(tctx);
	if (!cli) {
		return false;
	}

	torture_comment(tctx, "Filling send buffer\n");

	for (i=0;i<100;i++) {
		struct smbcli_request *req;
		req = smb_raw_negotiate_send(cli->transport, lpcfg_unicode(tctx->lp_ctx), PROTOCOL_NT1);
		event_loop_once(cli->transport->socket->event.ctx);
		if (req->state == SMBCLI_REQUEST_ERROR) {
			if (i > 0) {
				torture_comment(tctx, "Failed to fill pipe packet[%d] - %s (ignored)\n", i+1, nt_errstr(req->status));
				break;
			} else {
				torture_comment(tctx, "Failed to fill pipe - %s \n", nt_errstr(req->status));
				torture_close_connection(cli);
				return false;
			}
		}
	}

	torture_comment(tctx, "Opening secondary connection\n");
	if (!torture_open_connection(&cli2, tctx, 1)) {
		torture_comment(tctx, "Failed to open secondary connection\n");
		correct = false;
	}

	if (!torture_close_connection(cli2)) {
		torture_comment(tctx, "Failed to close secondary connection\n");
		correct = false;
	}

	torture_close_connection(cli);

	return correct;
}

/**
  this checks to see if a secondary tconx can use open files from an
  earlier tconx
 */
static bool run_tcon_test(struct torture_context *tctx, struct smbcli_state *cli)
{
	const char *fname = "\\tcontest.tmp";
	int fnum1;
	uint16_t cnum1, cnum2, cnum3;
	uint16_t vuid1, vuid2;
	uint8_t buf[4];
	bool ret = true;
	struct smbcli_tree *tree1;
	const char *host = torture_setting_string(tctx, "host", NULL);
	const char *share = torture_setting_string(tctx, "share", NULL);
	const char *password = torture_setting_string(tctx, "password", NULL);

	if (smbcli_deltree(cli->tree, fname) == -1) {
		torture_comment(tctx, "unlink of %s failed (%s)\n", fname, smbcli_errstr(cli->tree));
	}

	fnum1 = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	if (fnum1 == -1) {
		torture_comment(tctx, "open of %s failed (%s)\n", fname, smbcli_errstr(cli->tree));
		return false;
	}

	cnum1 = cli->tree->tid;
	vuid1 = cli->session->vuid;

	memset(buf, 0, 4); /* init buf so valgrind won't complain */
	if (smbcli_write(cli->tree, fnum1, 0, buf, 130, 4) != 4) {
		torture_comment(tctx, "initial write failed (%s)\n", smbcli_errstr(cli->tree));
		return false;
	}

	tree1 = cli->tree;	/* save old tree connection */
	if (NT_STATUS_IS_ERR(smbcli_tconX(cli, share, "?????", password))) {
		torture_comment(tctx, "%s refused 2nd tree connect (%s)\n", host,
		           smbcli_errstr(cli->tree));
		talloc_free(cli);
		return false;
	}

	cnum2 = cli->tree->tid;
	cnum3 = MAX(cnum1, cnum2) + 1; /* any invalid number */
	vuid2 = cli->session->vuid + 1;

	/* try a write with the wrong tid */
	cli->tree->tid = cnum2;

	if (smbcli_write(cli->tree, fnum1, 0, buf, 130, 4) == 4) {
		torture_comment(tctx, "* server allows write with wrong TID\n");
		ret = false;
	} else {
		torture_comment(tctx, "server fails write with wrong TID : %s\n", smbcli_errstr(cli->tree));
	}


	/* try a write with an invalid tid */
	cli->tree->tid = cnum3;

	if (smbcli_write(cli->tree, fnum1, 0, buf, 130, 4) == 4) {
		torture_comment(tctx, "* server allows write with invalid TID\n");
		ret = false;
	} else {
		torture_comment(tctx, "server fails write with invalid TID : %s\n", smbcli_errstr(cli->tree));
	}

	/* try a write with an invalid vuid */
	cli->session->vuid = vuid2;
	cli->tree->tid = cnum1;

	if (smbcli_write(cli->tree, fnum1, 0, buf, 130, 4) == 4) {
		torture_comment(tctx, "* server allows write with invalid VUID\n");
		ret = false;
	} else {
		torture_comment(tctx, "server fails write with invalid VUID : %s\n", smbcli_errstr(cli->tree));
	}

	cli->session->vuid = vuid1;
	cli->tree->tid = cnum1;

	if (NT_STATUS_IS_ERR(smbcli_close(cli->tree, fnum1))) {
		torture_comment(tctx, "close failed (%s)\n", smbcli_errstr(cli->tree));
		return false;
	}

	cli->tree->tid = cnum2;

	if (NT_STATUS_IS_ERR(smbcli_tdis(cli))) {
		torture_comment(tctx, "secondary tdis failed (%s)\n", smbcli_errstr(cli->tree));
		return false;
	}

	cli->tree = tree1;  /* restore initial tree */
	cli->tree->tid = cnum1;

	smbcli_unlink(tree1, fname);

	return ret;
}

/**
 checks for correct tconX support
 */
static bool run_tcon_devtype_test(struct torture_context *tctx, 
								  struct smbcli_state *cli1)
{
	const char *share = torture_setting_string(tctx, "share", NULL);

	if (!tcon_devtest(tctx, cli1, "IPC$", "A:", NT_STATUS_BAD_DEVICE_TYPE))
		return false;

	if (!tcon_devtest(tctx, cli1, "IPC$", "?????", NT_STATUS_OK))
		return false;

	if (!tcon_devtest(tctx, cli1, "IPC$", "LPT:", NT_STATUS_BAD_DEVICE_TYPE))
		return false;

	if (!tcon_devtest(tctx, cli1, "IPC$", "IPC", NT_STATUS_OK))
		return false;
			
	if (!tcon_devtest(tctx, cli1, "IPC$", "FOOBA", NT_STATUS_BAD_DEVICE_TYPE))
		return false;

	if (!tcon_devtest(tctx, cli1, share, "A:", NT_STATUS_OK))
		return false;

	if (!tcon_devtest(tctx, cli1, share, "?????", NT_STATUS_OK))
		return false;

	if (!tcon_devtest(tctx, cli1, share, "LPT:", NT_STATUS_BAD_DEVICE_TYPE))
		return false;

	if (!tcon_devtest(tctx, cli1, share, "IPC", NT_STATUS_BAD_DEVICE_TYPE))
		return false;
			
	if (!tcon_devtest(tctx, cli1, share, "FOOBA", NT_STATUS_BAD_DEVICE_TYPE))
		return false;

	return true;
}

static bool rw_torture2(struct torture_context *tctx,
						struct smbcli_state *c1, struct smbcli_state *c2)
{
	const char *lockfname = "\\torture2.lck";
	int fnum1;
	int fnum2;
	int i;
	uint8_t buf[131072];
	uint8_t buf_rd[131072];
	bool correct = true;
	ssize_t bytes_read, bytes_written;

	torture_assert(tctx, smbcli_deltree(c1->tree, lockfname) != -1,
				   talloc_asprintf(tctx, 
		"unlink failed (%s)", smbcli_errstr(c1->tree)));

	fnum1 = smbcli_open(c1->tree, lockfname, O_RDWR | O_CREAT | O_EXCL, 
			 DENY_NONE);
	torture_assert(tctx, fnum1 != -1, 
				   talloc_asprintf(tctx, 
		"first open read/write of %s failed (%s)",
				lockfname, smbcli_errstr(c1->tree)));
	fnum2 = smbcli_open(c2->tree, lockfname, O_RDONLY, 
			 DENY_NONE);
	torture_assert(tctx, fnum2 != -1, 
				   talloc_asprintf(tctx, 
		"second open read-only of %s failed (%s)",
				lockfname, smbcli_errstr(c2->tree)));

	torture_comment(tctx, "Checking data integrity over %d ops\n", 
					torture_numops);

	for (i=0;i<torture_numops;i++)
	{
		size_t buf_size = ((unsigned int)random()%(sizeof(buf)-1))+ 1;
		if (i % 10 == 0) {
			if (torture_setting_bool(tctx, "progress", true)) {
				torture_comment(tctx, "%d\r", i); fflush(stdout);
			}
		}

		generate_random_buffer(buf, buf_size);

		if ((bytes_written = smbcli_write(c1->tree, fnum1, 0, buf, 0, buf_size)) != buf_size) {
			torture_comment(tctx, "write failed (%s)\n", smbcli_errstr(c1->tree));
			torture_comment(tctx, "wrote %d, expected %d\n", (int)bytes_written, (int)buf_size); 
			correct = false;
			break;
		}

		if ((bytes_read = smbcli_read(c2->tree, fnum2, buf_rd, 0, buf_size)) != buf_size) {
			torture_comment(tctx, "read failed (%s)\n", smbcli_errstr(c2->tree));
			torture_comment(tctx, "read %d, expected %d\n", (int)bytes_read, (int)buf_size); 
			correct = false;
			break;
		}

		torture_assert_mem_equal(tctx, buf_rd, buf, buf_size, 
			"read/write compare failed\n");
	}

	torture_assert_ntstatus_ok(tctx, smbcli_close(c2->tree, fnum2), 
		talloc_asprintf(tctx, "close failed (%s)", smbcli_errstr(c2->tree)));
	torture_assert_ntstatus_ok(tctx, smbcli_close(c1->tree, fnum1),
		talloc_asprintf(tctx, "close failed (%s)", smbcli_errstr(c1->tree)));

	torture_assert_ntstatus_ok(tctx, smbcli_unlink(c1->tree, lockfname),
		talloc_asprintf(tctx, "unlink failed (%s)", smbcli_errstr(c1->tree)));

	torture_comment(tctx, "\n");

	return correct;
}



static bool run_readwritetest(struct torture_context *tctx,
							  struct smbcli_state *cli1,
							  struct smbcli_state *cli2)
{
	torture_comment(tctx, "Running readwritetest v1\n");
	if (!rw_torture2(tctx, cli1, cli2)) 
		return false;

	torture_comment(tctx, "Running readwritetest v2\n");

	if (!rw_torture2(tctx, cli1, cli1))
		return false;

	return true;
}

/*
test the timing of deferred open requests
*/
static bool run_deferopen(struct torture_context *tctx, struct smbcli_state *cli, int dummy)
{
	const char *fname = "\\defer_open_test.dat";
	int retries=4;
	int i = 0;
	bool correct = true;
	int nsec;
	int msec;
	double sec;

	nsec = torture_setting_int(tctx, "sharedelay", 1000000);
	msec = nsec / 1000;
	sec = ((double)nsec) / ((double) 1000000);

	if (retries <= 0) {
		torture_comment(tctx, "failed to connect\n");
		return false;
	}

	torture_comment(tctx, "Testing deferred open requests.\n");

	while (i < 4) {
		int fnum = -1;

		do {
			struct timeval tv;
			tv = timeval_current();
			fnum = smbcli_nt_create_full(cli->tree, fname, 0, 
						     SEC_RIGHTS_FILE_ALL,
						     FILE_ATTRIBUTE_NORMAL, 
						     NTCREATEX_SHARE_ACCESS_NONE,
						     NTCREATEX_DISP_OPEN_IF, 0, 0);
			if (fnum != -1) {
				break;
			}
			if (NT_STATUS_EQUAL(smbcli_nt_error(cli->tree),NT_STATUS_SHARING_VIOLATION)) {
				double e = timeval_elapsed(&tv);
				if (e < (0.5 * sec) || e > ((1.5 * sec) + 1)) {
					torture_comment(tctx,"Timing incorrect %.2f violation 1 sec == %.2f\n",
						e, sec);
					return false;
				}
			}
		} while (NT_STATUS_EQUAL(smbcli_nt_error(cli->tree),NT_STATUS_SHARING_VIOLATION));

		if (fnum == -1) {
			torture_comment(tctx,"Failed to open %s, error=%s\n", fname, smbcli_errstr(cli->tree));
			return false;
		}

		torture_comment(tctx, "pid %u open %d\n", (unsigned)getpid(), i);

		smb_msleep(10 * msec);
		i++;
		if (NT_STATUS_IS_ERR(smbcli_close(cli->tree, fnum))) {
			torture_comment(tctx,"Failed to close %s, error=%s\n", fname, smbcli_errstr(cli->tree));
			return false;
		}
		smb_msleep(2 * msec);
	}

	if (NT_STATUS_IS_ERR(smbcli_unlink(cli->tree, fname))) {
		/* All until the last unlink will fail with sharing violation
		   but also the last request can fail since the file could have
		   been successfully deleted by another (test) process */
		NTSTATUS status = smbcli_nt_error(cli->tree);
		if ((!NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION))
			&& (!NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_NOT_FOUND))) {
			torture_comment(tctx, "unlink of %s failed (%s)\n", fname, smbcli_errstr(cli->tree));
			correct = false;
		}
	}

	torture_comment(tctx, "deferred test finished\n");
	return correct;
}

/**
  Try with a wrong vuid and check error message.
 */

static bool run_vuidtest(struct torture_context *tctx, 
						 struct smbcli_state *cli)
{
	const char *fname = "\\vuid.tst";
	int fnum;
	size_t size;
	time_t c_time, a_time, m_time;

	uint16_t orig_vuid;
	NTSTATUS result;

	smbcli_unlink(cli->tree, fname);

	fnum = smbcli_open(cli->tree, fname, 
			O_RDWR | O_CREAT | O_TRUNC, DENY_NONE);

	orig_vuid = cli->session->vuid;

	cli->session->vuid += 1234;

	torture_comment(tctx, "Testing qfileinfo with wrong vuid\n");
	
	if (NT_STATUS_IS_OK(result = smbcli_qfileinfo(cli->tree, fnum, NULL,
						   &size, &c_time, &a_time,
						   &m_time, NULL, NULL))) {
		torture_fail(tctx, "qfileinfo passed with wrong vuid");
	}

	if (!NT_STATUS_EQUAL(cli->transport->error.e.nt_status, 
			     NT_STATUS_DOS(ERRSRV, ERRbaduid)) &&
	    !NT_STATUS_EQUAL(cli->transport->error.e.nt_status, 
			     NT_STATUS_INVALID_HANDLE)) {
		torture_fail(tctx, talloc_asprintf(tctx, 
				"qfileinfo should have returned DOS error "
		       "ERRSRV:ERRbaduid\n  but returned %s",
		       smbcli_errstr(cli->tree)));
	}

	cli->session->vuid -= 1234;

	torture_assert_ntstatus_ok(tctx, smbcli_close(cli->tree, fnum),
		talloc_asprintf(tctx, "close failed (%s)", smbcli_errstr(cli->tree)));

	smbcli_unlink(cli->tree, fname);

	return true;
}

/*
  Test open mode returns on read-only files.
 */
 static bool run_opentest(struct torture_context *tctx, struct smbcli_state *cli1, 
						  struct smbcli_state *cli2)
{
	const char *fname = "\\readonly.file";
	char *control_char_fname;
	int fnum1, fnum2;
	uint8_t buf[20];
	size_t fsize;
	bool correct = true;
	char *tmp_path;
	int failures = 0;
	int i;

	asprintf(&control_char_fname, "\\readonly.afile");
	for (i = 1; i <= 0x1f; i++) {
		control_char_fname[10] = i;
		fnum1 = smbcli_nt_create_full(cli1->tree, control_char_fname, 0, SEC_FILE_WRITE_DATA, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OVERWRITE_IF, 0, 0);
		
        	if (!check_error(__location__, cli1, ERRDOS, ERRinvalidname, 
				NT_STATUS_OBJECT_NAME_INVALID)) {
			torture_comment(tctx, "Error code should be NT_STATUS_OBJECT_NAME_INVALID, was %s for file with %d char\n",
					smbcli_errstr(cli1->tree), i);
			failures++;
		}

		if (fnum1 != -1) {
			smbcli_close(cli1->tree, fnum1);
		}
		smbcli_setatr(cli1->tree, control_char_fname, 0, 0);
		smbcli_unlink(cli1->tree, control_char_fname);
	}
	free(control_char_fname);

	if (!failures)
		torture_comment(tctx, "Create file with control char names passed.\n");

	smbcli_setatr(cli1->tree, fname, 0, 0);
	smbcli_unlink(cli1->tree, fname);
	
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	if (fnum1 == -1) {
		torture_comment(tctx, "open of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		return false;
	}

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "close2 failed (%s)\n", smbcli_errstr(cli1->tree));
		return false;
	}
	
	if (NT_STATUS_IS_ERR(smbcli_setatr(cli1->tree, fname, FILE_ATTRIBUTE_READONLY, 0))) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": smbcli_setatr failed (%s)\n", smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test1);
		return false;
	}
	
	fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY, DENY_WRITE);
	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": open of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test1);
		return false;
	}
	
	/* This will fail - but the error should be ERRnoaccess, not ERRbadshare. */
	fnum2 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_ALL);
	
        if (check_error(__location__, cli1, ERRDOS, ERRnoaccess, 
			NT_STATUS_ACCESS_DENIED)) {
		torture_comment(tctx, "correct error code ERRDOS/ERRnoaccess returned\n");
	}
	
	torture_comment(tctx, "finished open test 1\n");

error_test1:
	smbcli_close(cli1->tree, fnum1);
	
	/* Now try not readonly and ensure ERRbadshare is returned. */
	
	smbcli_setatr(cli1->tree, fname, 0, 0);
	
	fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY, DENY_WRITE);
	if (fnum1 == -1) {
		torture_comment(tctx, "open of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		return false;
	}
	
	/* This will fail - but the error should be ERRshare. */
	fnum2 = smbcli_open(cli1->tree, fname, O_RDWR, DENY_ALL);
	
	if (check_error(__location__, cli1, ERRDOS, ERRbadshare, 
			NT_STATUS_SHARING_VIOLATION)) {
		torture_comment(tctx, "correct error code ERRDOS/ERRbadshare returned\n");
	}
	
	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "close2 failed (%s)\n", smbcli_errstr(cli1->tree));
		return false;
	}
	
	smbcli_unlink(cli1->tree, fname);
	
	torture_comment(tctx, "finished open test 2\n");
	
	/* Test truncate open disposition on file opened for read. */
	
	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	if (fnum1 == -1) {
		torture_comment(tctx, "(3) open (1) of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		return false;
	}
	
	/* write 20 bytes. */
	
	memset(buf, '\0', 20);

	if (smbcli_write(cli1->tree, fnum1, 0, buf, 0, 20) != 20) {
		torture_comment(tctx, "write failed (%s)\n", smbcli_errstr(cli1->tree));
		correct = false;
	}

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "(3) close1 failed (%s)\n", smbcli_errstr(cli1->tree));
		return false;
	}
	
	/* Ensure size == 20. */
	if (NT_STATUS_IS_ERR(smbcli_getatr(cli1->tree, fname, NULL, &fsize, NULL))) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (3) getatr failed (%s)\n", smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test3);
		return false;
	}
	
	if (fsize != 20) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (3) file size != 20\n");
		CHECK_MAX_FAILURES(error_test3);
		return false;
	}

	/* Now test if we can truncate a file opened for readonly. */
	
	fnum1 = smbcli_open(cli1->tree, fname, O_RDONLY|O_TRUNC, DENY_NONE);
	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (3) open (2) of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test3);
		return false;
	}
	
	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": close2 failed (%s)\n", smbcli_errstr(cli1->tree));
		return false;
	}

	/* Ensure size == 0. */
	if (NT_STATUS_IS_ERR(smbcli_getatr(cli1->tree, fname, NULL, &fsize, NULL))) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (3) getatr failed (%s)\n", smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test3);
		return false;
	}

	if (fsize != 0) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (3) file size != 0\n");
		CHECK_MAX_FAILURES(error_test3);
		return false;
	}
	torture_comment(tctx, "finished open test 3\n");
error_test3:	

	fnum1 = fnum2 = -1;
	smbcli_unlink(cli1->tree, fname);


	torture_comment(tctx, "Testing ctemp\n");
	fnum1 = smbcli_ctemp(cli1->tree, "\\", &tmp_path);
	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": ctemp failed (%s)\n", smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test4);
		return false;
	}
	torture_comment(tctx, "ctemp gave path %s\n", tmp_path);

error_test4:
	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "close of temp failed (%s)\n", smbcli_errstr(cli1->tree));
	}
	if (NT_STATUS_IS_ERR(smbcli_unlink(cli1->tree, tmp_path))) {
		torture_comment(tctx, "unlink of temp failed (%s)\n", smbcli_errstr(cli1->tree));
	}

	/* Test the non-io opens... */

	torture_comment(tctx, "Test #1 testing 2 non-io opens (no delete)\n");
	fnum1 = fnum2 = -1;
	smbcli_setatr(cli2->tree, fname, 0, 0);
	smbcli_unlink(cli2->tree, fname);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 1 open 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test10);
		return false;
	}

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OPEN_IF, 0, 0);
	if (fnum2 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 1 open 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
		CHECK_MAX_FAILURES(error_test10);
		return false;
	}

	torture_comment(tctx, "non-io open test #1 passed.\n");
error_test10:

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "Test 1 close 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
	}
	if (NT_STATUS_IS_ERR(smbcli_close(cli2->tree, fnum2))) {
		torture_comment(tctx, "Test 1 close 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
	}

	torture_comment(tctx, "Test #2 testing 2 non-io opens (first with delete)\n");
	fnum1 = fnum2 = -1;
	smbcli_unlink(cli1->tree, fname);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, SEC_STD_DELETE|SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 2 open 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test20);
		return false;
	}

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OPEN_IF, 0, 0);

	if (fnum2 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 2 open 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
		CHECK_MAX_FAILURES(error_test20);
		return false;
	}

	torture_comment(tctx, "non-io open test #2 passed.\n");
error_test20:

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "Test 1 close 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
	}
	if (NT_STATUS_IS_ERR(smbcli_close(cli2->tree, fnum2))) {
		torture_comment(tctx, "Test 1 close 2 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
	}

	fnum1 = fnum2 = -1;
	smbcli_unlink(cli1->tree, fname);

	torture_comment(tctx, "Test #3 testing 2 non-io opens (second with delete)\n");

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 3 open 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test30);
		return false;
	}

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, SEC_STD_DELETE|SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OPEN_IF, 0, 0);

	if (fnum2 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 3 open 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
		CHECK_MAX_FAILURES(error_test30);
		return false;
	}

	torture_comment(tctx, "non-io open test #3 passed.\n");
error_test30:

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "Test 3 close 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
	}
	if (NT_STATUS_IS_ERR(smbcli_close(cli2->tree, fnum2))) {
		torture_comment(tctx, "Test 3 close 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
	}

	torture_comment(tctx, "Test #4 testing 2 non-io opens (both with delete)\n");
	fnum1 = fnum2 = -1;
	smbcli_unlink(cli1->tree, fname);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, SEC_STD_DELETE|SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 4 open 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test40);
		return false;
	}

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, SEC_STD_DELETE|SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OPEN_IF, 0, 0);

	if (fnum2 != -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 4 open 2 of %s SUCCEEDED - should have failed (%s)\n", fname, smbcli_errstr(cli2->tree));
		CHECK_MAX_FAILURES(error_test40);
		return false;
	}

	torture_comment(tctx, "Test 4 open 2 of %s gave %s (correct error should be %s)\n", fname, smbcli_errstr(cli2->tree), "sharing violation");

	torture_comment(tctx, "non-io open test #4 passed.\n");
error_test40:

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "Test 4 close 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
	}
	if (fnum2 != -1 && NT_STATUS_IS_ERR(smbcli_close(cli2->tree, fnum2))) {
		torture_comment(tctx, "Test 4 close 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
	}

	torture_comment(tctx, "Test #5 testing 2 non-io opens (both with delete - both with file share delete)\n");
	fnum1 = fnum2 = -1;
	smbcli_unlink(cli1->tree, fname);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, SEC_STD_DELETE|SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_DELETE, NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 5 open 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test50);
		return false;
	}

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, SEC_STD_DELETE|SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_DELETE, NTCREATEX_DISP_OPEN_IF, 0, 0);

	if (fnum2 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 5 open 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
		CHECK_MAX_FAILURES(error_test50);
		return false;
	}

	torture_comment(tctx, "non-io open test #5 passed.\n");
error_test50:

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "Test 5 close 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
	}

	if (NT_STATUS_IS_ERR(smbcli_close(cli2->tree, fnum2))) {
		torture_comment(tctx, "Test 5 close 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
	}

	torture_comment(tctx, "Test #6 testing 1 non-io open, one io open\n");
	fnum1 = fnum2 = -1;
	smbcli_unlink(cli1->tree, fname);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, SEC_FILE_READ_DATA, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 6 open 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test60);
		return false;
	}

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_READ, NTCREATEX_DISP_OPEN_IF, 0, 0);

	if (fnum2 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 6 open 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
		CHECK_MAX_FAILURES(error_test60);
		return false;
	}

	torture_comment(tctx, "non-io open test #6 passed.\n");
error_test60:

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "Test 6 close 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
	}

	if (NT_STATUS_IS_ERR(smbcli_close(cli2->tree, fnum2))) {
		torture_comment(tctx, "Test 6 close 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
	}

	torture_comment(tctx, "Test #7 testing 1 non-io open, one io open with delete\n");
	fnum1 = fnum2 = -1;
	smbcli_unlink(cli1->tree, fname);

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0, SEC_FILE_READ_DATA, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_NONE, NTCREATEX_DISP_OVERWRITE_IF, 0, 0);

	if (fnum1 == -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 7 open 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test70);
		return false;
	}

	fnum2 = smbcli_nt_create_full(cli2->tree, fname, 0, SEC_STD_DELETE|SEC_FILE_READ_ATTRIBUTE, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_DELETE, NTCREATEX_DISP_OPEN_IF, 0, 0);

	if (fnum2 != -1) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": Test 7 open 2 of %s SUCCEEDED - should have failed (%s)\n", fname, smbcli_errstr(cli2->tree));
		CHECK_MAX_FAILURES(error_test70);
		return false;
	}

	torture_comment(tctx, "Test 7 open 2 of %s gave %s (correct error should be %s)\n", fname, smbcli_errstr(cli2->tree), "sharing violation");

	torture_comment(tctx, "non-io open test #7 passed.\n");
error_test70:

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "Test 7 close 1 of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
	}
	if (fnum2 != -1 && NT_STATUS_IS_ERR(smbcli_close(cli2->tree, fnum2))) {
		torture_comment(tctx, "Test 7 close 2 of %s failed (%s)\n", fname, smbcli_errstr(cli2->tree));
	}

	torture_comment(tctx, "Test #8 testing one normal open, followed by lock, followed by open with truncate\n");
	fnum1 = fnum2 = -1;
	smbcli_unlink(cli1->tree, fname);

	fnum1 = smbcli_open(cli1->tree, fname, O_RDWR|O_CREAT, DENY_NONE);
	if (fnum1 == -1) {
		torture_comment(tctx, "(8) open (1) of %s failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		return false;
	}
	
	/* write 20 bytes. */
	
	memset(buf, '\0', 20);

	if (smbcli_write(cli1->tree, fnum1, 0, buf, 0, 20) != 20) {
		torture_comment(tctx, "(8) write failed (%s)\n", smbcli_errstr(cli1->tree));
		correct = false;
	}

	/* Ensure size == 20. */
	if (NT_STATUS_IS_ERR(smbcli_getatr(cli1->tree, fname, NULL, &fsize, NULL))) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (8) getatr (1) failed (%s)\n", smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test80);
		return false;
	}
	
	if (fsize != 20) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (8) file size %lu != 20\n", (unsigned long)fsize);
		CHECK_MAX_FAILURES(error_test80);
		return false;
	}

	/* Get an exclusive lock on the open file. */
	if (NT_STATUS_IS_ERR(smbcli_lock(cli1->tree, fnum1, 0, 4, 0, WRITE_LOCK))) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (8) lock1 failed (%s)\n", smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test80);
		return false;
	}

	fnum2 = smbcli_open(cli1->tree, fname, O_RDWR|O_TRUNC, DENY_NONE);
	if (fnum1 == -1) {
		torture_comment(tctx, "(8) open (2) of %s with truncate failed (%s)\n", fname, smbcli_errstr(cli1->tree));
		return false;
	}

	/* Ensure size == 0. */
	if (NT_STATUS_IS_ERR(smbcli_getatr(cli1->tree, fname, NULL, &fsize, NULL))) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (8) getatr (2) failed (%s)\n", smbcli_errstr(cli1->tree));
		CHECK_MAX_FAILURES(error_test80);
		return false;
	}
	
	if (fsize != 0) {
		torture_result(tctx, TORTURE_FAIL,
			__location__ ": (8) file size %lu != 0\n", (unsigned long)fsize);
		CHECK_MAX_FAILURES(error_test80);
		return false;
	}

	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum1))) {
		torture_comment(tctx, "(8) close1 failed (%s)\n", smbcli_errstr(cli1->tree));
		return false;
	}
	
	if (NT_STATUS_IS_ERR(smbcli_close(cli1->tree, fnum2))) {
		torture_comment(tctx, "(8) close1 failed (%s)\n", smbcli_errstr(cli1->tree));
		return false;
	}
	
error_test80:

	torture_comment(tctx, "open test #8 passed.\n");

	smbcli_unlink(cli1->tree, fname);

	return failures > 0 ? false : correct;
}

/* FIRST_DESIRED_ACCESS   0xf019f */
#define FIRST_DESIRED_ACCESS   SEC_FILE_READ_DATA|SEC_FILE_WRITE_DATA|SEC_FILE_APPEND_DATA|\
                               SEC_FILE_READ_EA|                           /* 0xf */ \
                               SEC_FILE_WRITE_EA|SEC_FILE_READ_ATTRIBUTE|     /* 0x90 */ \
                               SEC_FILE_WRITE_ATTRIBUTE|                  /* 0x100 */ \
                               SEC_STD_DELETE|SEC_STD_READ_CONTROL|\
                               SEC_STD_WRITE_DAC|SEC_STD_WRITE_OWNER     /* 0xf0000 */
/* SECOND_DESIRED_ACCESS  0xe0080 */
#define SECOND_DESIRED_ACCESS  SEC_FILE_READ_ATTRIBUTE|                   /* 0x80 */ \
                               SEC_STD_READ_CONTROL|SEC_STD_WRITE_DAC|\
                               SEC_STD_WRITE_OWNER                      /* 0xe0000 */

#if 0
#define THIRD_DESIRED_ACCESS   FILE_READ_ATTRIBUTE|                   /* 0x80 */ \
                               READ_CONTROL|WRITE_DAC|\
                               SEC_FILE_READ_DATA|\
                               WRITE_OWNER                      /* */
#endif



/**
  Test ntcreate calls made by xcopy
 */
static bool run_xcopy(struct torture_context *tctx,
					  struct smbcli_state *cli1)
{
	const char *fname = "\\test.txt";
	int fnum1, fnum2;

	fnum1 = smbcli_nt_create_full(cli1->tree, fname, 0,
				      FIRST_DESIRED_ACCESS, 
				      FILE_ATTRIBUTE_ARCHIVE,
				      NTCREATEX_SHARE_ACCESS_NONE, 
				      NTCREATEX_DISP_OVERWRITE_IF, 
				      0x4044, 0);

	torture_assert(tctx, fnum1 != -1, talloc_asprintf(tctx, 
				"First open failed - %s", smbcli_errstr(cli1->tree)));

	fnum2 = smbcli_nt_create_full(cli1->tree, fname, 0,
				   SECOND_DESIRED_ACCESS, 0,
				   NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE|NTCREATEX_SHARE_ACCESS_DELETE, NTCREATEX_DISP_OPEN, 
				   0x200000, 0);
	torture_assert(tctx, fnum2 != -1, talloc_asprintf(tctx, 
				"second open failed - %s", smbcli_errstr(cli1->tree)));
	
	return true;
}

static bool run_iometer(struct torture_context *tctx,
						struct smbcli_state *cli)
{
	const char *fname = "\\iobw.tst";
	int fnum;
	size_t filesize;
	NTSTATUS status;
	char buf[2048];
	int ops;

	memset(buf, 0, sizeof(buf));

	status = smbcli_getatr(cli->tree, fname, NULL, &filesize, NULL);
	torture_assert_ntstatus_ok(tctx, status, 
		talloc_asprintf(tctx, "smbcli_getatr failed: %s", nt_errstr(status)));

	torture_comment(tctx, "size: %d\n", (int)filesize);

	filesize -= (sizeof(buf) - 1);

	fnum = smbcli_nt_create_full(cli->tree, fname, 0x16,
				     0x2019f, 0, 0x3, 3, 0x42, 0x3);
	torture_assert(tctx, fnum != -1, talloc_asprintf(tctx, "open failed: %s", 
				   smbcli_errstr(cli->tree)));

	ops = 0;

	while (true) {
		int i, num_reads, num_writes;

		num_reads = random() % 10;
		num_writes = random() % 3;

		for (i=0; i<num_reads; i++) {
			ssize_t res;
			if (ops++ > torture_numops) {
				return true;
			}
			res = smbcli_read(cli->tree, fnum, buf,
					  random() % filesize, sizeof(buf));
			torture_assert(tctx, res == sizeof(buf), 
						   talloc_asprintf(tctx, "read failed: %s",
										   smbcli_errstr(cli->tree)));
		}
		for (i=0; i<num_writes; i++) {
			ssize_t res;
			if (ops++ > torture_numops) {
				return true;
			}
			res = smbcli_write(cli->tree, fnum, 0, buf,
					  random() % filesize, sizeof(buf));
			torture_assert(tctx, res == sizeof(buf),
						   talloc_asprintf(tctx, "read failed: %s",
				       smbcli_errstr(cli->tree)));
		}
	}
}

/**
  tries variants of chkpath
 */
static bool torture_chkpath_test(struct torture_context *tctx, 
								 struct smbcli_state *cli)
{
	int fnum;
	bool ret;

	torture_comment(tctx, "Testing valid and invalid paths\n");

	/* cleanup from an old run */
	smbcli_rmdir(cli->tree, "\\chkpath.dir\\dir2");
	smbcli_unlink(cli->tree, "\\chkpath.dir\\*");
	smbcli_rmdir(cli->tree, "\\chkpath.dir");

	if (NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, "\\chkpath.dir"))) {
		torture_comment(tctx, "mkdir1 failed : %s\n", smbcli_errstr(cli->tree));
		return false;
	}

	if (NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, "\\chkpath.dir\\dir2"))) {
		torture_comment(tctx, "mkdir2 failed : %s\n", smbcli_errstr(cli->tree));
		return false;
	}

	fnum = smbcli_open(cli->tree, "\\chkpath.dir\\foo.txt", O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	if (fnum == -1) {
		torture_comment(tctx, "open1 failed (%s)\n", smbcli_errstr(cli->tree));
		return false;
	}
	smbcli_close(cli->tree, fnum);

	if (NT_STATUS_IS_ERR(smbcli_chkpath(cli->tree, "\\chkpath.dir"))) {
		torture_comment(tctx, "chkpath1 failed: %s\n", smbcli_errstr(cli->tree));
		ret = false;
	}

	if (NT_STATUS_IS_ERR(smbcli_chkpath(cli->tree, "\\chkpath.dir\\dir2"))) {
		torture_comment(tctx, "chkpath2 failed: %s\n", smbcli_errstr(cli->tree));
		ret = false;
	}

	if (NT_STATUS_IS_ERR(smbcli_chkpath(cli->tree, "\\chkpath.dir\\foo.txt"))) {
		ret = check_error(__location__, cli, ERRDOS, ERRbadpath, 
				  NT_STATUS_NOT_A_DIRECTORY);
	} else {
		torture_comment(tctx, "* chkpath on a file should fail\n");
		ret = false;
	}

	if (NT_STATUS_IS_ERR(smbcli_chkpath(cli->tree, "\\chkpath.dir\\bar.txt"))) {
		ret = check_error(__location__, cli, ERRDOS, ERRbadpath, 
				  NT_STATUS_OBJECT_NAME_NOT_FOUND);
	} else {
		torture_comment(tctx, "* chkpath on a non existent file should fail\n");
		ret = false;
	}

	if (NT_STATUS_IS_ERR(smbcli_chkpath(cli->tree, "\\chkpath.dir\\dirxx\\bar.txt"))) {
		ret = check_error(__location__, cli, ERRDOS, ERRbadpath, 
				  NT_STATUS_OBJECT_PATH_NOT_FOUND);
	} else {
		torture_comment(tctx, "* chkpath on a non existent component should fail\n");
		ret = false;
	}

	smbcli_rmdir(cli->tree, "\\chkpath.dir\\dir2");
	smbcli_unlink(cli->tree, "\\chkpath.dir\\*");
	smbcli_rmdir(cli->tree, "\\chkpath.dir");

	return ret;
}

/*
 * This is a test to excercise some weird Samba3 error paths.
 */

static bool torture_samba3_errorpaths(struct torture_context *tctx)
{
	bool nt_status_support;
	bool client_ntlmv2_auth;
	struct smbcli_state *cli_nt = NULL, *cli_dos = NULL;
	bool result = false;
	int fnum;
	const char *os2_fname = ".+,;=[].";
	const char *dname = "samba3_errordir";
	union smb_open io;
	NTSTATUS status;

	nt_status_support = lpcfg_nt_status_support(tctx->lp_ctx);
	client_ntlmv2_auth = lpcfg_client_ntlmv2_auth(tctx->lp_ctx);

	if (!lpcfg_set_cmdline(tctx->lp_ctx, "nt status support", "yes")) {
		torture_comment(tctx, "Could not set 'nt status support = yes'\n");
		goto fail;
	}
	if (!lpcfg_set_cmdline(tctx->lp_ctx, "client ntlmv2 auth", "yes")) {
		torture_result(tctx, TORTURE_FAIL, "Could not set 'client ntlmv2 auth = yes'\n");
		goto fail;
	}

	if (!torture_open_connection(&cli_nt, tctx, 0)) {
		goto fail;
	}

	if (!lpcfg_set_cmdline(tctx->lp_ctx, "nt status support", "no")) {
		torture_result(tctx, TORTURE_FAIL, "Could not set 'nt status support = no'\n");
		goto fail;
	}
	if (!lpcfg_set_cmdline(tctx->lp_ctx, "client ntlmv2 auth", "no")) {
		torture_result(tctx, TORTURE_FAIL, "Could not set 'client ntlmv2 auth = no'\n");
		goto fail;
	}

	if (!torture_open_connection(&cli_dos, tctx, 1)) {
		goto fail;
	}

	if (!lpcfg_set_cmdline(tctx->lp_ctx, "nt status support",
			    nt_status_support ? "yes":"no")) {
		torture_result(tctx, TORTURE_FAIL, "Could not reset 'nt status support'");
		goto fail;
	}
	if (!lpcfg_set_cmdline(tctx->lp_ctx, "client ntlmv2 auth",
			       client_ntlmv2_auth ? "yes":"no")) {
		torture_result(tctx, TORTURE_FAIL, "Could not reset 'client ntlmv2 auth'");
		goto fail;
	}

	smbcli_unlink(cli_nt->tree, os2_fname);
	smbcli_rmdir(cli_nt->tree, dname);

	if (!NT_STATUS_IS_OK(smbcli_mkdir(cli_nt->tree, dname))) {
		torture_comment(tctx, "smbcli_mkdir(%s) failed: %s\n", dname,
		       smbcli_errstr(cli_nt->tree));
		goto fail;
	}

	io.generic.level = RAW_OPEN_NTCREATEX;
	io.ntcreatex.in.flags = NTCREATEX_FLAGS_EXTENDED;
	io.ntcreatex.in.root_fid.fnum = 0;
	io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	io.ntcreatex.in.alloc_size = 1024*1024;
	io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
	io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_NONE;
	io.ntcreatex.in.open_disposition = NTCREATEX_DISP_CREATE;
	io.ntcreatex.in.create_options = 0;
	io.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
	io.ntcreatex.in.security_flags = 0;
	io.ntcreatex.in.fname = dname;

	status = smb_raw_open(cli_nt->tree, tctx, &io);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_COLLISION)) {
		torture_comment(tctx, "(%s) incorrect status %s should be %s\n",
		       __location__, nt_errstr(status),
		       nt_errstr(NT_STATUS_OBJECT_NAME_COLLISION));
		goto fail;
	}
	status = smb_raw_open(cli_dos->tree, tctx, &io);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_DOS(ERRDOS, ERRfilexists))) {
		torture_comment(tctx, "(%s) incorrect status %s should be %s\n",
		       __location__, nt_errstr(status),
		       nt_errstr(NT_STATUS_DOS(ERRDOS, ERRfilexists)));
		goto fail;
	}

	status = smbcli_mkdir(cli_nt->tree, dname);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_COLLISION)) {
		torture_comment(tctx, "(%s) incorrect status %s should be %s\n",
		       __location__, nt_errstr(status),
		       nt_errstr(NT_STATUS_OBJECT_NAME_COLLISION));
		goto fail;
	}
	status = smbcli_mkdir(cli_dos->tree, dname);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_DOS(ERRDOS, ERRnoaccess))) {
		torture_comment(tctx, "(%s) incorrect status %s should be %s\n",
		       __location__, nt_errstr(status),
		       nt_errstr(NT_STATUS_DOS(ERRDOS, ERRnoaccess)));
		goto fail;
	}

	{
		union smb_mkdir md;
		md.t2mkdir.level = RAW_MKDIR_T2MKDIR;
		md.t2mkdir.in.path = dname;
		md.t2mkdir.in.num_eas = 0;
		md.t2mkdir.in.eas = NULL;

		status = smb_raw_mkdir(cli_nt->tree, &md);
		if (!NT_STATUS_EQUAL(status,
				     NT_STATUS_OBJECT_NAME_COLLISION)) {
			torture_comment(
				tctx, "(%s) incorrect status %s should be "
				"NT_STATUS_OBJECT_NAME_COLLISION\n",
				__location__, nt_errstr(status));
			goto fail;
		}
		status = smb_raw_mkdir(cli_dos->tree, &md);
		if (!NT_STATUS_EQUAL(status,
				     NT_STATUS_DOS(ERRDOS, ERRrename))) {
			torture_comment(tctx, "(%s) incorrect status %s "
					"should be ERRDOS:ERRrename\n",
					__location__, nt_errstr(status));
			goto fail;
		}
	}

	io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
	status = smb_raw_open(cli_nt->tree, tctx, &io);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_OBJECT_NAME_COLLISION)) {
		torture_comment(tctx, "(%s) incorrect status %s should be %s\n",
		       __location__, nt_errstr(status),
		       nt_errstr(NT_STATUS_OBJECT_NAME_COLLISION));
		goto fail;
	}

	status = smb_raw_open(cli_dos->tree, tctx, &io);
	if (!NT_STATUS_EQUAL(status, NT_STATUS_DOS(ERRDOS, ERRfilexists))) {
		torture_comment(tctx, "(%s) incorrect status %s should be %s\n",
		       __location__, nt_errstr(status),
		       nt_errstr(NT_STATUS_DOS(ERRDOS, ERRfilexists)));
		goto fail;
	}

	{
		/* Test an invalid DOS deny mode */
		const char *fname = "test.txt";

		fnum = smbcli_open(cli_nt->tree, fname, O_RDWR | O_CREAT, 5);
		if (fnum != -1) {
			torture_comment(tctx, "Open(%s) with invalid deny mode succeeded -- "
			       "expected failure\n", fname);
			smbcli_close(cli_nt->tree, fnum);
			goto fail;
		}
		if (!NT_STATUS_EQUAL(smbcli_nt_error(cli_nt->tree),
				     NT_STATUS_DOS(ERRDOS,ERRbadaccess))) {
			torture_comment(tctx, "Expected DOS error ERRDOS/ERRbadaccess, "
			       "got %s\n", smbcli_errstr(cli_nt->tree));
			goto fail;
		}

		fnum = smbcli_open(cli_dos->tree, fname, O_RDWR | O_CREAT, 5);
		if (fnum != -1) {
			torture_comment(tctx, "Open(%s) with invalid deny mode succeeded -- "
			       "expected failure\n", fname);
			smbcli_close(cli_nt->tree, fnum);
			goto fail;
		}
		if (!NT_STATUS_EQUAL(smbcli_nt_error(cli_nt->tree),
				     NT_STATUS_DOS(ERRDOS,ERRbadaccess))) {
			torture_comment(tctx, "Expected DOS error ERRDOS:ERRbadaccess, "
			       "got %s\n", smbcli_errstr(cli_nt->tree));
			goto fail;
		}
	}

	{
		/*
		 * Samba 3.0.23 has a bug that an existing file can be opened
		 * as a directory using ntcreate&x. Test this.
		 */

		const char *fname = "\\test_dir.txt";

		fnum = smbcli_open(cli_nt->tree, fname, O_RDWR|O_CREAT,
				   DENY_NONE);
		if (fnum == -1) {
			d_printf("(%s) smbcli_open failed: %s\n", __location__,
				 smbcli_errstr(cli_nt->tree));
		}
		smbcli_close(cli_nt->tree, fnum);

		io.generic.level = RAW_OPEN_NTCREATEX;
		io.ntcreatex.in.root_fid.fnum = 0;
		io.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
		io.ntcreatex.in.alloc_size = 0;
		io.ntcreatex.in.file_attr = FILE_ATTRIBUTE_DIRECTORY;
		io.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_READ|
			NTCREATEX_SHARE_ACCESS_WRITE|
			NTCREATEX_SHARE_ACCESS_DELETE;
		io.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
		io.ntcreatex.in.create_options = NTCREATEX_OPTIONS_DIRECTORY;
		io.ntcreatex.in.impersonation =
			NTCREATEX_IMPERSONATION_ANONYMOUS;
		io.ntcreatex.in.security_flags = 0;
		io.ntcreatex.in.fname = fname;
		io.ntcreatex.in.flags = 0;

		status = smb_raw_open(cli_nt->tree, tctx, &io);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_NOT_A_DIRECTORY)) {
			torture_comment(tctx, "ntcreate as dir gave %s, "
					"expected NT_STATUS_NOT_A_DIRECTORY\n",
					nt_errstr(status));
			result = false;
		}

		if (NT_STATUS_IS_OK(status)) {
			smbcli_close(cli_nt->tree, io.ntcreatex.out.file.fnum);
		}

		status = smb_raw_open(cli_dos->tree, tctx, &io);
		if (!NT_STATUS_EQUAL(status, NT_STATUS_DOS(ERRDOS,
							   ERRbaddirectory))) {
			torture_comment(tctx, "ntcreate as dir gave %s, "
					"expected NT_STATUS_NOT_A_DIRECTORY\n",
					nt_errstr(status));
			result = false;
		}

		if (NT_STATUS_IS_OK(status)) {
			smbcli_close(cli_dos->tree,
				     io.ntcreatex.out.file.fnum);
		}

		smbcli_unlink(cli_nt->tree, fname);
	}

	if (!torture_setting_bool(tctx, "samba3", false)) {
		goto done;
	}

	fnum = smbcli_open(cli_dos->tree, os2_fname, 
			   O_RDWR | O_CREAT | O_TRUNC,
			   DENY_NONE);
	if (fnum != -1) {
		torture_comment(tctx, "Open(%s) succeeded -- expected failure\n",
		       os2_fname);
		smbcli_close(cli_dos->tree, fnum);
		goto fail;
	}

	if (!NT_STATUS_EQUAL(smbcli_nt_error(cli_dos->tree),
			     NT_STATUS_DOS(ERRDOS, ERRcannotopen))) {
		torture_comment(tctx, "Expected DOS error ERRDOS/ERRcannotopen, got %s\n",
		       smbcli_errstr(cli_dos->tree));
		goto fail;
	}

	fnum = smbcli_open(cli_nt->tree, os2_fname, 
			   O_RDWR | O_CREAT | O_TRUNC,
			   DENY_NONE);
	if (fnum != -1) {
		torture_comment(tctx, "Open(%s) succeeded -- expected failure\n",
		       os2_fname);
		smbcli_close(cli_nt->tree, fnum);
		goto fail;
	}

	if (!NT_STATUS_EQUAL(smbcli_nt_error(cli_nt->tree),
			     NT_STATUS_OBJECT_NAME_NOT_FOUND)) {
		torture_comment(tctx, "Expected error NT_STATUS_OBJECT_NAME_NOT_FOUND, "
		       "got %s\n", smbcli_errstr(cli_nt->tree));
		goto fail;
	}

 done:
	result = true;

 fail:
	if (cli_dos != NULL) {
		torture_close_connection(cli_dos);
	}
	if (cli_nt != NULL) {
		torture_close_connection(cli_nt);
	}
	
	return result;
}

/**
  This checks file/dir birthtime
*/
static void list_fn(struct clilist_file_info *finfo, const char *name,
			void *state){

	/* Just to change dir access time*/
	sleep(5);

}

static bool run_birthtimetest(struct torture_context *tctx,
						   struct smbcli_state *cli)
{
	int fnum;
	size_t size;
	time_t c_time, a_time, m_time, w_time, c_time1;
	const char *fname = "\\birthtime.tst";
	const char *dname = "\\birthtime";
	const char *fname2 = "\\birthtime\\birthtime.tst";
	bool correct = true;
	uint8_t buf[16];


	smbcli_unlink(cli->tree, fname);

	torture_comment(tctx, "Testing Birthtime for File\n");

	/* Save File birthtime/creationtime */
	fnum = smbcli_open(cli->tree, fname, O_RDWR | O_CREAT | O_TRUNC,
				DENY_NONE);
	if (NT_STATUS_IS_ERR(smbcli_qfileinfo(cli->tree, fnum, NULL, &size,
				&c_time, &a_time, &m_time, NULL, NULL))) {
		torture_comment(tctx, "ERROR: qfileinfo failed (%s)\n",
				smbcli_errstr(cli->tree));
		correct = false;
	}
	smbcli_close(cli->tree, fnum);

	sleep(10);

	/* Change in File attribute changes file change time*/
	smbcli_setatr(cli->tree, fname, FILE_ATTRIBUTE_SYSTEM, 0);

	fnum = smbcli_open(cli->tree, fname, O_RDWR | O_CREAT , DENY_NONE);
	/* Writing updates modification time*/
	smbcli_smbwrite(cli->tree, fnum,  &fname, 0, sizeof(fname));
	/*Reading updates access time */
	smbcli_read(cli->tree, fnum, buf, 0, 13);
	smbcli_close(cli->tree, fnum);

	if (NT_STATUS_IS_ERR(smbcli_qpathinfo2(cli->tree, fname, &c_time1,
			&a_time, &m_time, &w_time, &size, NULL, NULL))) {
		torture_comment(tctx, "ERROR: qpathinfo2 failed (%s)\n",
			smbcli_errstr(cli->tree));
		correct = false;
	} else {
		fprintf(stdout, "c_time = %li, c_time1 = %li\n",
			(long) c_time, (long) c_time1);
		if (c_time1 != c_time) {
			torture_comment(tctx, "This system updated file \
					birth times! Not expected!\n");
			correct = false;
		}
	}
	smbcli_unlink(cli->tree, fname);

	torture_comment(tctx, "Testing Birthtime for Directory\n");

	/* check if the server does not update the directory birth time
          when creating a new file */
	if (NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, dname))) {
		torture_comment(tctx, "ERROR: mkdir failed (%s)\n",
				smbcli_errstr(cli->tree));
		correct = false;
	}
	sleep(3);
	if (NT_STATUS_IS_ERR(smbcli_qpathinfo2(cli->tree, "\\birthtime\\",
			&c_time,&a_time,&m_time,&w_time, &size, NULL, NULL))){
		torture_comment(tctx, "ERROR: qpathinfo2 failed (%s)\n",
				smbcli_errstr(cli->tree));
		correct = false;
	}

	/* Creating a new file changes dir modification time and change time*/
	smbcli_unlink(cli->tree, fname2);
	fnum = smbcli_open(cli->tree, fname2, O_RDWR | O_CREAT | O_TRUNC,
			DENY_NONE);
	smbcli_smbwrite(cli->tree, fnum,  &fnum, 0, sizeof(fnum));
	smbcli_read(cli->tree, fnum, buf, 0, 13);
	smbcli_close(cli->tree, fnum);

	/* dir listing changes dir access time*/
	smbcli_list(cli->tree, "\\birthtime\\*", 0, list_fn, cli );

	if (NT_STATUS_IS_ERR(smbcli_qpathinfo2(cli->tree, "\\birthtime\\",
			&c_time1, &a_time, &m_time,&w_time,&size,NULL,NULL))){
		torture_comment(tctx, "ERROR: qpathinfo2 failed (%s)\n",
				smbcli_errstr(cli->tree));
		correct = false;
	} else {
		fprintf(stdout, "c_time = %li, c_time1 = %li\n",
			(long) c_time, (long) c_time1);
		if (c_time1 != c_time) {
			torture_comment(tctx, "This system  updated directory \
					birth times! Not Expected!\n");
			correct = false;
		}
	}
	smbcli_unlink(cli->tree, fname2);
	smbcli_rmdir(cli->tree, dname);

	return correct;
}


NTSTATUS torture_base_init(void)
{
	struct torture_suite *suite = torture_suite_create(talloc_autofree_context(), "base");

	torture_suite_add_2smb_test(suite, "fdpass", run_fdpasstest);
	torture_suite_add_suite(suite, torture_base_locktest(suite));
	torture_suite_add_1smb_test(suite, "unlink", torture_unlinktest);
	torture_suite_add_1smb_test(suite, "attr",   run_attrtest);
	torture_suite_add_1smb_test(suite, "trans2", run_trans2test);
	torture_suite_add_1smb_test(suite, "birthtime", run_birthtimetest);
	torture_suite_add_simple_test(suite, "negnowait", run_negprot_nowait);
	torture_suite_add_1smb_test(suite, "dir1",  torture_dirtest1);
	torture_suite_add_1smb_test(suite, "dir2",  torture_dirtest2);
	torture_suite_add_1smb_test(suite, "deny1",  torture_denytest1);
	torture_suite_add_2smb_test(suite, "deny2",  torture_denytest2);
	torture_suite_add_2smb_test(suite, "deny3",  torture_denytest3);
	torture_suite_add_1smb_test(suite, "denydos",  torture_denydos_sharing);
	torture_suite_add_smb_multi_test(suite, "ntdeny1", torture_ntdenytest1);
	torture_suite_add_2smb_test(suite, "ntdeny2",  torture_ntdenytest2);
	torture_suite_add_1smb_test(suite, "tcon",  run_tcon_test);
	torture_suite_add_1smb_test(suite, "tcondev",  run_tcon_devtype_test);
	torture_suite_add_1smb_test(suite, "vuid", run_vuidtest);
	torture_suite_add_2smb_test(suite, "rw1",  run_readwritetest);
	torture_suite_add_2smb_test(suite, "open", run_opentest);
	torture_suite_add_smb_multi_test(suite, "defer_open", run_deferopen);
	torture_suite_add_1smb_test(suite, "xcopy", run_xcopy);
	torture_suite_add_1smb_test(suite, "iometer", run_iometer);
	torture_suite_add_1smb_test(suite, "rename", torture_test_rename);
	torture_suite_add_suite(suite, torture_test_delete());
	torture_suite_add_1smb_test(suite, "properties", torture_test_properties);
	torture_suite_add_1smb_test(suite, "mangle", torture_mangle);
	torture_suite_add_1smb_test(suite, "openattr", torture_openattrtest);
	torture_suite_add_1smb_test(suite, "winattr", torture_winattrtest);
	torture_suite_add_suite(suite, torture_charset(suite));
	torture_suite_add_1smb_test(suite, "chkpath",  torture_chkpath_test);
	torture_suite_add_1smb_test(suite, "secleak",  torture_sec_leak);
	torture_suite_add_simple_test(suite, "disconnect",  torture_disconnect);
	torture_suite_add_suite(suite, torture_delay_write());
	torture_suite_add_simple_test(suite, "samba3error", torture_samba3_errorpaths);
	torture_suite_add_1smb_test(suite, "casetable", torture_casetable);
	torture_suite_add_1smb_test(suite, "utable", torture_utable);
	torture_suite_add_simple_test(suite, "smb", torture_smb_scan);
	torture_suite_add_suite(suite, torture_trans2_aliases(suite));
	torture_suite_add_1smb_test(suite, "trans2-scan", torture_trans2_scan);
	torture_suite_add_1smb_test(suite, "nttrans", torture_nttrans_scan);
	torture_suite_add_1smb_test(suite, "createx_access", torture_createx_access);
	torture_suite_add_2smb_test(suite, "createx_sharemodes_file", torture_createx_sharemodes_file);
	torture_suite_add_2smb_test(suite, "createx_sharemodes_dir", torture_createx_sharemodes_dir);
	torture_suite_add_1smb_test(suite, "maximum_allowed", torture_maximum_allowed);

	torture_suite_add_simple_test(suite, "bench-holdcon", torture_holdcon);
	torture_suite_add_1smb_test(suite, "bench-holdopen", torture_holdopen);
	torture_suite_add_simple_test(suite, "bench-readwrite", run_benchrw);
	torture_suite_add_smb_multi_test(suite, "bench-torture", run_torture);
	torture_suite_add_1smb_test(suite, "scan-pipe_number", run_pipe_number);
	torture_suite_add_1smb_test(suite, "scan-ioctl", torture_ioctl_test);
	torture_suite_add_smb_multi_test(suite, "scan-maxfid", run_maxfidtest);

	suite->description = talloc_strdup(suite, 
					"Basic SMB tests (imported from the original smbtorture)");

	torture_register_suite(suite);

	return NT_STATUS_OK;
}
