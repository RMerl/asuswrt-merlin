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
#include "libcli/raw/libcliraw.h"
#include "libcli/raw/raw_proto.h"
#include "system/time.h"
#include "system/wait.h"
#include "system/filesys.h"
#include "libcli/raw/ioctl.h"
#include "libcli/libcli.h"
#include "lib/events/events.h"
#include "libcli/resolve/resolve.h"
#include "torture/smbtorture.h"
#include "torture/util.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/composite/composite.h"
#include "param/param.h"

extern struct cli_credentials *cmdline_credentials;
	
static bool wait_lock(struct smbcli_state *c, int fnum, uint32_t offset, uint32_t len)
{
	while (NT_STATUS_IS_ERR(smbcli_lock(c->tree, fnum, offset, len, -1, WRITE_LOCK))) {
		if (!check_error(__location__, c, ERRDOS, ERRlock, NT_STATUS_LOCK_NOT_GRANTED)) return false;
	}
	return true;
}


static bool rw_torture(struct torture_context *tctx, struct smbcli_state *c)
{
	const char *lockfname = "\\torture.lck";
	char *fname;
	int fnum;
	int fnum2;
	pid_t pid2, pid = getpid();
	int i, j;
	uint8_t buf[1024];
	bool correct = true;

	fnum2 = smbcli_open(c->tree, lockfname, O_RDWR | O_CREAT | O_EXCL, 
			 DENY_NONE);
	if (fnum2 == -1)
		fnum2 = smbcli_open(c->tree, lockfname, O_RDWR, DENY_NONE);
	if (fnum2 == -1) {
		torture_comment(tctx, "open of %s failed (%s)\n", lockfname, smbcli_errstr(c->tree));
		return false;
	}

	generate_random_buffer(buf, sizeof(buf));

	for (i=0;i<torture_numops;i++) {
		unsigned int n = (unsigned int)random()%10;
		if (i % 10 == 0) {
			if (torture_setting_bool(tctx, "progress", true)) {
				torture_comment(tctx, "%d\r", i);
				fflush(stdout);
			}
		}
		asprintf(&fname, "\\torture.%u", n);

		if (!wait_lock(c, fnum2, n*sizeof(int), sizeof(int))) {
			return false;
		}

		fnum = smbcli_open(c->tree, fname, O_RDWR | O_CREAT | O_TRUNC, DENY_ALL);
		if (fnum == -1) {
			torture_comment(tctx, "open failed (%s)\n", smbcli_errstr(c->tree));
			correct = false;
			break;
		}

		if (smbcli_write(c->tree, fnum, 0, &pid, 0, sizeof(pid)) != sizeof(pid)) {
			torture_comment(tctx, "write failed (%s)\n", smbcli_errstr(c->tree));
			correct = false;
		}

		for (j=0;j<50;j++) {
			if (smbcli_write(c->tree, fnum, 0, buf, 
				      sizeof(pid)+(j*sizeof(buf)), 
				      sizeof(buf)) != sizeof(buf)) {
				torture_comment(tctx, "write failed (%s)\n", smbcli_errstr(c->tree));
				correct = false;
			}
		}

		pid2 = 0;

		if (smbcli_read(c->tree, fnum, &pid2, 0, sizeof(pid)) != sizeof(pid)) {
			torture_comment(tctx, "read failed (%s)\n", smbcli_errstr(c->tree));
			correct = false;
		}

		if (pid2 != pid) {
			torture_comment(tctx, "data corruption!\n");
			correct = false;
		}

		if (NT_STATUS_IS_ERR(smbcli_close(c->tree, fnum))) {
			torture_comment(tctx, "close failed (%s)\n", smbcli_errstr(c->tree));
			correct = false;
		}

		if (NT_STATUS_IS_ERR(smbcli_unlink(c->tree, fname))) {
			torture_comment(tctx, "unlink failed (%s)\n", smbcli_errstr(c->tree));
			correct = false;
		}

		if (NT_STATUS_IS_ERR(smbcli_unlock(c->tree, fnum2, n*sizeof(int), sizeof(int)))) {
			torture_comment(tctx, "unlock failed (%s)\n", smbcli_errstr(c->tree));
			correct = false;
		}
		free(fname);
	}

	smbcli_close(c->tree, fnum2);
	smbcli_unlink(c->tree, lockfname);

	torture_comment(tctx, "%d\n", i);

	return correct;
}

bool run_torture(struct torture_context *tctx, struct smbcli_state *cli, int dummy)
{
	return rw_torture(tctx, cli);
}


/*
  see how many RPC pipes we can open at once
*/
bool run_pipe_number(struct torture_context *tctx, 
					 struct smbcli_state *cli1)
{
	const char *pipe_name = "\\WKSSVC";
	int fnum;
	int num_pipes = 0;

	while(1) {
		fnum = smbcli_nt_create_full(cli1->tree, pipe_name, 0, SEC_FILE_READ_DATA, FILE_ATTRIBUTE_NORMAL,
				   NTCREATEX_SHARE_ACCESS_READ|NTCREATEX_SHARE_ACCESS_WRITE, NTCREATEX_DISP_OPEN_IF, 0, 0);

		if (fnum == -1) {
			torture_comment(tctx, "Open of pipe %s failed with error (%s)\n", pipe_name, smbcli_errstr(cli1->tree));
			break;
		}
		num_pipes++;
		if (torture_setting_bool(tctx, "progress", true)) {
			torture_comment(tctx, "%d\r", num_pipes);
			fflush(stdout);
		}
	}

	torture_comment(tctx, "pipe_number test - we can open %d %s pipes.\n", num_pipes, pipe_name );
	return true;
}




/*
  open N connections to the server and just hold them open
  used for testing performance when there are N idle users
  already connected
 */
bool torture_holdcon(struct torture_context *tctx)
{
	int i;
	struct smbcli_state **cli;
	int num_dead = 0;

	torture_comment(tctx, "Opening %d connections\n", torture_numops);
	
	cli = malloc_array_p(struct smbcli_state *, torture_numops);

	for (i=0;i<torture_numops;i++) {
		if (!torture_open_connection(&cli[i], tctx, i)) {
			return false;
		}
		if (torture_setting_bool(tctx, "progress", true)) {
			torture_comment(tctx, "opened %d connections\r", i);
			fflush(stdout);
		}
	}

	torture_comment(tctx, "\nStarting pings\n");

	while (1) {
		for (i=0;i<torture_numops;i++) {
			NTSTATUS status;
			if (cli[i]) {
				status = smbcli_chkpath(cli[i]->tree, "\\");
				if (!NT_STATUS_IS_OK(status)) {
					torture_comment(tctx, "Connection %d is dead\n", i);
					cli[i] = NULL;
					num_dead++;
				}
				usleep(100);
			}
		}

		if (num_dead == torture_numops) {
			torture_comment(tctx, "All connections dead - finishing\n");
			break;
		}

		torture_comment(tctx, ".");
		fflush(stdout);
	}

	return true;
}

/*
  open a file N times on the server and just hold them open
  used for testing performance when there are N file handles
  alopenn
 */
bool torture_holdopen(struct torture_context *tctx,
		      struct smbcli_state *cli)
{
	int i, fnum;
	const char *fname = "\\holdopen.dat";
	NTSTATUS status;

	smbcli_unlink(cli->tree, fname);

	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	if (fnum == -1) {
		torture_comment(tctx, "open of %s failed (%s)\n", fname, smbcli_errstr(cli->tree));
		return false;
	}

	smbcli_close(cli->tree, fnum);

	for (i=0;i<torture_numops;i++) {
		union smb_open op;

		op.generic.level = RAW_OPEN_NTCREATEX;
		op.ntcreatex.in.root_fid.fnum = 0;
		op.ntcreatex.in.flags = 0;
		op.ntcreatex.in.access_mask = SEC_FILE_WRITE_DATA;
		op.ntcreatex.in.create_options = 0;
		op.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
		op.ntcreatex.in.share_access = NTCREATEX_SHARE_ACCESS_MASK;
		op.ntcreatex.in.alloc_size = 0;
		op.ntcreatex.in.open_disposition = NTCREATEX_DISP_OPEN;
		op.ntcreatex.in.impersonation = NTCREATEX_IMPERSONATION_ANONYMOUS;
		op.ntcreatex.in.security_flags = 0;
		op.ntcreatex.in.fname = fname;
		status = smb_raw_open(cli->tree, tctx, &op);
		if (!NT_STATUS_IS_OK(status)) {
			torture_warning(tctx, "open %d failed\n", i);
			continue;
		}

		if (torture_setting_bool(tctx, "progress", true)) {
			torture_comment(tctx, "opened %d file\r", i);
			fflush(stdout);
		}
	}

	torture_comment(tctx, "\nStarting pings\n");

	while (1) {
		struct smb_echo ec;

		status = smb_raw_echo(cli->transport, &ec);
		torture_comment(tctx, ".");
		fflush(stdout);
		sleep(15);
	}
}

/*
test how many open files this server supports on the one socket
*/
bool run_maxfidtest(struct torture_context *tctx, struct smbcli_state *cli, int dummy)
{
#define MAXFID_TEMPLATE "\\maxfid\\fid%d\\maxfid.%d.%d"
	char *fname;
	int fnums[0x11000], i;
	int retries=4, maxfid;
	bool correct = true;

	if (retries <= 0) {
		torture_comment(tctx, "failed to connect\n");
		return false;
	}

	if (smbcli_deltree(cli->tree, "\\maxfid") == -1) {
		torture_comment(tctx, "Failed to deltree \\maxfid - %s\n",
		       smbcli_errstr(cli->tree));
		return false;
	}
	if (NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, "\\maxfid"))) {
		torture_comment(tctx, "Failed to mkdir \\maxfid, error=%s\n", 
		       smbcli_errstr(cli->tree));
		return false;
	}

	torture_comment(tctx, "Testing maximum number of open files\n");

	for (i=0; i<0x11000; i++) {
		if (i % 1000 == 0) {
			asprintf(&fname, "\\maxfid\\fid%d", i/1000);
			if (NT_STATUS_IS_ERR(smbcli_mkdir(cli->tree, fname))) {
				torture_comment(tctx, "Failed to mkdir %s, error=%s\n", 
				       fname, smbcli_errstr(cli->tree));
				return false;
			}
			free(fname);
		}
		asprintf(&fname, MAXFID_TEMPLATE, i/1000, i,(int)getpid());
		if ((fnums[i] = smbcli_open(cli->tree, fname, 
					O_RDWR|O_CREAT|O_TRUNC, DENY_NONE)) ==
		    -1) {
			torture_comment(tctx, "open of %s failed (%s)\n", 
			       fname, smbcli_errstr(cli->tree));
			torture_comment(tctx, "maximum fnum is %d\n", i);
			break;
		}
		free(fname);
		if (torture_setting_bool(tctx, "progress", true)) {
			torture_comment(tctx, "%6d\r", i);
			fflush(stdout);
		}
	}
	torture_comment(tctx, "%6d\n", i);
	i--;

	maxfid = i;

	torture_comment(tctx, "cleaning up\n");
	for (i=0;i<maxfid/2;i++) {
		asprintf(&fname, MAXFID_TEMPLATE, i/1000, i,(int)getpid());
		if (NT_STATUS_IS_ERR(smbcli_close(cli->tree, fnums[i]))) {
			torture_comment(tctx, "Close of fnum %d failed - %s\n", fnums[i], smbcli_errstr(cli->tree));
		}
		if (NT_STATUS_IS_ERR(smbcli_unlink(cli->tree, fname))) {
			torture_comment(tctx, "unlink of %s failed (%s)\n", 
			       fname, smbcli_errstr(cli->tree));
			correct = false;
		}
		free(fname);

		asprintf(&fname, MAXFID_TEMPLATE, (maxfid-i)/1000, maxfid-i,(int)getpid());
		if (NT_STATUS_IS_ERR(smbcli_close(cli->tree, fnums[maxfid-i]))) {
			torture_comment(tctx, "Close of fnum %d failed - %s\n", fnums[maxfid-i], smbcli_errstr(cli->tree));
		}
		if (NT_STATUS_IS_ERR(smbcli_unlink(cli->tree, fname))) {
			torture_comment(tctx, "unlink of %s failed (%s)\n", 
			       fname, smbcli_errstr(cli->tree));
			correct = false;
		}
		free(fname);

		if (torture_setting_bool(tctx, "progress", true)) {
			torture_comment(tctx, "%6d %6d\r", i, maxfid-i);
			fflush(stdout);
		}
	}
	torture_comment(tctx, "%6d\n", 0);

	if (smbcli_deltree(cli->tree, "\\maxfid") == -1) {
		torture_comment(tctx, "Failed to deltree \\maxfid - %s\n",
		       smbcli_errstr(cli->tree));
		return false;
	}

	torture_comment(tctx, "maxfid test finished\n");
	if (!torture_close_connection(cli)) {
		correct = false;
	}
	return correct;
#undef MAXFID_TEMPLATE
}



/*
  sees what IOCTLs are supported
 */
bool torture_ioctl_test(struct torture_context *tctx, 
						struct smbcli_state *cli)
{
	uint16_t device, function;
	int fnum;
	const char *fname = "\\ioctl.dat";
	NTSTATUS status;
	union smb_ioctl parms;
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_named_const(tctx, 0, "ioctl_test");

	smbcli_unlink(cli->tree, fname);

	fnum = smbcli_open(cli->tree, fname, O_RDWR|O_CREAT|O_EXCL, DENY_NONE);
	if (fnum == -1) {
		torture_comment(tctx, "open of %s failed (%s)\n", fname, smbcli_errstr(cli->tree));
		return false;
	}

	parms.ioctl.level = RAW_IOCTL_IOCTL;
	parms.ioctl.in.file.fnum = fnum;
	parms.ioctl.in.request = IOCTL_QUERY_JOB_INFO;
	status = smb_raw_ioctl(cli->tree, mem_ctx, &parms);
	torture_comment(tctx, "ioctl job info: %s\n", smbcli_errstr(cli->tree));

	for (device=0;device<0x100;device++) {
		torture_comment(tctx, "Testing device=0x%x\n", device);
		for (function=0;function<0x100;function++) {
			parms.ioctl.in.request = (device << 16) | function;
			status = smb_raw_ioctl(cli->tree, mem_ctx, &parms);

			if (NT_STATUS_IS_OK(status)) {
				torture_comment(tctx, "ioctl device=0x%x function=0x%x OK : %d bytes\n", 
					device, function, (int)parms.ioctl.out.blob.length);
			}
		}
	}

	return true;
}

static void benchrw_callback(struct smbcli_request *req);
enum benchrw_stage {
	START,
	OPEN_CONNECTION,
	CLEANUP_TESTDIR,
	MK_TESTDIR,
	OPEN_FILE,
	INITIAL_WRITE,
	READ_WRITE_DATA,
	MAX_OPS_REACHED,
	ERROR,
	CLOSE_FILE,
	CLEANUP,
	FINISHED
};

struct benchrw_state {
	struct torture_context *tctx;
	char *dname;
	char *fname;
	uint16_t fnum;
	int nr;
	struct smbcli_tree	*cli;		
	uint8_t *buffer;
	int writecnt;
	int readcnt;
	int completed;
	int num_parallel_requests;
	void *req_params;
	enum benchrw_stage mode;
	struct params{
		struct unclist{
			const char *host;
			const char *share;
		} **unc;
		const char *workgroup;
		int retry;
		unsigned int writeblocks;
		unsigned int blocksize;
		unsigned int writeratio;
		int num_parallel_requests;
	} *lpcfg_params;
};

/* 
	init params using lpcfg_parm_xxx
 	return number of unclist entries
*/
static int init_benchrw_params(struct torture_context *tctx,
			       struct params *lpar)
{
	char **unc_list = NULL;
	int num_unc_names = 0, conn_index=0, empty_lines=0;
	const char *p;
	lpar->retry = torture_setting_int(tctx, "retry",3);
	lpar->blocksize = torture_setting_int(tctx, "blocksize",65535);
	lpar->writeblocks = torture_setting_int(tctx, "writeblocks",15);
	lpar->writeratio = torture_setting_int(tctx, "writeratio",5);
	lpar->num_parallel_requests = torture_setting_int(
		tctx, "parallel_requests", 5);
	lpar->workgroup = lpcfg_workgroup(tctx->lp_ctx);
	
	p = torture_setting_string(tctx, "unclist", NULL);
	if (p) {
		char *h, *s;
		unc_list = file_lines_load(p, &num_unc_names, 0, NULL);
		if (!unc_list || num_unc_names <= 0) {
			torture_comment(tctx, "Failed to load unc names list "
					"from '%s'\n", p);
			exit(1);
		}
		
		lpar->unc = talloc_array(tctx, struct unclist *,
					 (num_unc_names-empty_lines));
		for(conn_index = 0; conn_index < num_unc_names; conn_index++) {
			/* ignore empty lines */
			if(strlen(unc_list[conn_index % num_unc_names])==0){
				empty_lines++;
				continue;
			}
			if (!smbcli_parse_unc(
				    unc_list[conn_index % num_unc_names],
				    NULL, &h, &s)) {
				torture_comment(
					tctx, "Failed to parse UNC "
					"name %s\n",
					unc_list[conn_index % num_unc_names]);
				exit(1);
			}
			lpar->unc[conn_index-empty_lines] =
				talloc(tctx, struct unclist);
			lpar->unc[conn_index-empty_lines]->host = h;
			lpar->unc[conn_index-empty_lines]->share = s;	
		}
		return num_unc_names-empty_lines;
	}else{
		lpar->unc = talloc_array(tctx, struct unclist *, 1);
		lpar->unc[0] = talloc(tctx,struct unclist);
		lpar->unc[0]->host  = torture_setting_string(tctx, "host",
							     NULL);
		lpar->unc[0]->share = torture_setting_string(tctx, "share",
							     NULL);
		return 1;
	}
}

/*
 Called when the reads & writes are finished. closes the file.
*/
static NTSTATUS benchrw_close(struct torture_context *tctx,
			      struct smbcli_request *req,
			      struct benchrw_state *state)
{
	union smb_close close_parms;
	
	NT_STATUS_NOT_OK_RETURN(req->status);
	
	torture_comment(tctx, "Close file %d (%d)\n",state->nr,state->fnum);
	close_parms.close.level = RAW_CLOSE_CLOSE;
	close_parms.close.in.file.fnum = state->fnum ;
	close_parms.close.in.write_time = 0;
	state->mode=CLOSE_FILE;
	
	req = smb_raw_close_send(state->cli, &close_parms);
	NT_STATUS_HAVE_NO_MEMORY(req);
	/*register the callback function!*/
	req->async.fn = benchrw_callback;
	req->async.private_data = state;
	
	return NT_STATUS_OK;
}

static NTSTATUS benchrw_readwrite(struct torture_context *tctx,
				  struct benchrw_state *state);
static void benchrw_callback(struct smbcli_request *req);

static void benchrw_rw_callback(struct smbcli_request *req)
{
	struct benchrw_state *state = req->async.private_data;
	struct torture_context *tctx = state->tctx;

	if (!NT_STATUS_IS_OK(req->status)) {
		state->mode = ERROR;
		return;
	}

	state->completed++;
	state->num_parallel_requests--;

	if ((state->completed >= torture_numops)
	    && (state->num_parallel_requests == 0)) {
		benchrw_callback(req);
		talloc_free(req);
		return;
	}

	talloc_free(req);

	if (state->completed + state->num_parallel_requests
	    < torture_numops) {
		benchrw_readwrite(tctx, state);
	}
}

/*
 Called when the initial write is completed is done. write or read a file.
*/
static NTSTATUS benchrw_readwrite(struct torture_context *tctx,
				  struct benchrw_state *state)
{
	struct smbcli_request *req;
	union smb_read	rd;
	union smb_write	wr;
	
	/* randomize between writes and reads*/
	if (random() % state->lpcfg_params->writeratio == 0) {
		torture_comment(tctx, "Callback WRITE file:%d (%d/%d)\n",
				state->nr,state->completed,torture_numops);
		wr.generic.level = RAW_WRITE_WRITEX  ;
		wr.writex.in.file.fnum  = state->fnum ;
		wr.writex.in.offset     = 0;
		wr.writex.in.wmode	= 0		;
		wr.writex.in.remaining  = 0;
		wr.writex.in.count      = state->lpcfg_params->blocksize;
		wr.writex.in.data       = state->buffer;
		state->readcnt=0;
		req = smb_raw_write_send(state->cli,&wr);
	}
	else {
		torture_comment(tctx,
				"Callback READ file:%d (%d/%d) Offset:%d\n",
				state->nr,state->completed,torture_numops,
				(state->readcnt*state->lpcfg_params->blocksize));
		rd.generic.level = RAW_READ_READX;
		rd.readx.in.file.fnum	= state->fnum 	;
		rd.readx.in.offset	= state->readcnt*state->lpcfg_params->blocksize;
		rd.readx.in.mincnt	= state->lpcfg_params->blocksize;
		rd.readx.in.maxcnt	= rd.readx.in.mincnt;
		rd.readx.in.remaining	= 0	;
		rd.readx.out.data	= state->buffer;
		rd.readx.in.read_for_execute = false;
		if(state->readcnt < state->lpcfg_params->writeblocks){
			state->readcnt++;	
		}else{
			/*start reading from beginn of file*/
			state->readcnt=0;
		}
		req = smb_raw_read_send(state->cli,&rd);
	}
	state->num_parallel_requests += 1;
	NT_STATUS_HAVE_NO_MEMORY(req);
	/*register the callback function!*/
	req->async.fn = benchrw_rw_callback;
	req->async.private_data = state;
	
	return NT_STATUS_OK;
}

/*
 Called when the open is done. writes to the file.
*/
static NTSTATUS benchrw_open(struct torture_context *tctx,
			     struct smbcli_request *req,
			     struct benchrw_state *state)
{
	union smb_write	wr;
	if(state->mode == OPEN_FILE){
		NTSTATUS status;
		status = smb_raw_open_recv(req,tctx,(
					union smb_open*)state->req_params);
		NT_STATUS_NOT_OK_RETURN(status);
	
		state->fnum = ((union smb_open*)state->req_params)
						->openx.out.file.fnum;
		torture_comment(tctx, "File opened (%d)\n",state->fnum);
		state->mode=INITIAL_WRITE;
	}
		
	torture_comment(tctx, "Write initial test file:%d (%d/%d)\n",state->nr,
		(state->writecnt+1)*state->lpcfg_params->blocksize,
		(state->lpcfg_params->writeblocks*state->lpcfg_params->blocksize));
	wr.generic.level = RAW_WRITE_WRITEX  ;
	wr.writex.in.file.fnum  = state->fnum ;
	wr.writex.in.offset     = state->writecnt * 
					state->lpcfg_params->blocksize;
	wr.writex.in.wmode	= 0		;
	wr.writex.in.remaining  = (state->lpcfg_params->writeblocks *
						state->lpcfg_params->blocksize)-
						((state->writecnt+1)*state->
						lpcfg_params->blocksize);
	wr.writex.in.count      = state->lpcfg_params->blocksize;
	wr.writex.in.data       = state->buffer;
	state->writecnt++;
	if(state->writecnt == state->lpcfg_params->writeblocks){
		state->mode=READ_WRITE_DATA;
	}
	req = smb_raw_write_send(state->cli,&wr);
	NT_STATUS_HAVE_NO_MEMORY(req);
	
	/*register the callback function!*/
	req->async.fn = benchrw_callback;
	req->async.private_data = state;
	return NT_STATUS_OK;
} 

/*
 Called when the mkdir is done. Opens a file.
*/
static NTSTATUS benchrw_mkdir(struct torture_context *tctx,
			      struct smbcli_request *req,
			      struct benchrw_state *state)
{
	union smb_open *open_parms;	
	uint8_t *writedata;	
		
	NT_STATUS_NOT_OK_RETURN(req->status);
	
	/* open/create the files */
	torture_comment(tctx, "Open File %d/%d\n",state->nr+1,
			torture_setting_int(tctx, "nprocs", 4));
	open_parms=talloc_zero(tctx, union smb_open);
	NT_STATUS_HAVE_NO_MEMORY(open_parms);
	open_parms->openx.level = RAW_OPEN_OPENX;
	open_parms->openx.in.flags = 0;
	open_parms->openx.in.open_mode = OPENX_MODE_ACCESS_RDWR;
	open_parms->openx.in.search_attrs = 
			FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
	open_parms->openx.in.file_attrs = 0;
	open_parms->openx.in.write_time = 0;
	open_parms->openx.in.open_func = OPENX_OPEN_FUNC_CREATE;
	open_parms->openx.in.size = 0;
	open_parms->openx.in.timeout = 0;
	open_parms->openx.in.fname = state->fname;
		
	writedata = talloc_size(tctx,state->lpcfg_params->blocksize);
	NT_STATUS_HAVE_NO_MEMORY(writedata);
	generate_random_buffer(writedata,state->lpcfg_params->blocksize);
	state->buffer=writedata;
	state->writecnt=1;
	state->readcnt=0;
	state->req_params=open_parms;		
	state->mode=OPEN_FILE;	
			
	req = smb_raw_open_send(state->cli,open_parms);
	NT_STATUS_HAVE_NO_MEMORY(req);
	
	/*register the callback function!*/
	req->async.fn = benchrw_callback;
	req->async.private_data = state;
		
	return NT_STATUS_OK;
}

/*
 handler for completion of a sub-request of the bench-rw test
*/
static void benchrw_callback(struct smbcli_request *req)
{
	struct benchrw_state *state = req->async.private_data;
	struct torture_context *tctx = state->tctx;
	
	/*dont send new requests when torture_numops is reached*/
	if ((state->mode == READ_WRITE_DATA)
	    && (state->completed >= torture_numops)) {
		state->mode=MAX_OPS_REACHED;
	}
	
	switch (state->mode) {
	
	case MK_TESTDIR:
		if (!NT_STATUS_IS_OK(benchrw_mkdir(tctx, req,state))) {
			torture_comment(tctx, "Failed to create the test "
					"directory - %s\n", 
					nt_errstr(req->status));
			state->mode=ERROR;
			return;
		}
		break;	
	case OPEN_FILE:
	case INITIAL_WRITE:
		if (!NT_STATUS_IS_OK(benchrw_open(tctx, req,state))){
			torture_comment(tctx, "Failed to open/write the "
					"file - %s\n", 
					nt_errstr(req->status));
			state->mode=ERROR;
			state->readcnt=0;
			return;
		}
		break;
	case READ_WRITE_DATA:
		while (state->num_parallel_requests
		       < state->lpcfg_params->num_parallel_requests) {
			NTSTATUS status;
			status = benchrw_readwrite(tctx,state);
			if (!NT_STATUS_IS_OK(status)){
				torture_comment(tctx, "Failed to read/write "
						"the file - %s\n", 
						nt_errstr(req->status));
				state->mode=ERROR;
				return;
			}
		}
		break;
	case MAX_OPS_REACHED:
		if (!NT_STATUS_IS_OK(benchrw_close(tctx,req,state))){
			torture_comment(tctx, "Failed to read/write/close "
					"the file - %s\n", 
					nt_errstr(req->status));
			state->mode=ERROR;
			return;
		}
		break;
	case CLOSE_FILE:
		torture_comment(tctx, "File %d closed\n",state->nr);
		if (!NT_STATUS_IS_OK(req->status)) {
			torture_comment(tctx, "Failed to close the "
					"file - %s\n",
					nt_errstr(req->status));
			state->mode=ERROR;
			return;
		}
		state->mode=CLEANUP;
		return;	
	default:
		break;
	}
	
}

/* open connection async callback function*/
static void async_open_callback(struct composite_context *con)
{
	struct benchrw_state *state = con->async.private_data;
	struct torture_context *tctx = state->tctx;
	int retry = state->lpcfg_params->retry;
	 	
	if (NT_STATUS_IS_OK(con->status)) {
		state->cli=((struct smb_composite_connect*)
					state->req_params)->out.tree;
		state->mode=CLEANUP_TESTDIR;
	}else{
		if(state->writecnt < retry){
			torture_comment(tctx, "Failed to open connection: "
					"%d, Retry (%d/%d)\n",
					state->nr,state->writecnt,retry);
			state->writecnt++;
			state->mode=START;
			usleep(1000);	
		}else{
			torture_comment(tctx, "Failed to open connection "
					"(%d) - %s\n",
					state->nr, nt_errstr(con->status));
			state->mode=ERROR;
		}
		return;
	}	
}

/*
 establishs a smbcli_tree from scratch (async)
*/
static struct composite_context *torture_connect_async(
				struct torture_context *tctx,
				struct smb_composite_connect *smb,
				TALLOC_CTX *mem_ctx,
				struct tevent_context *ev,
				const char *host,
				const char *share,
				const char *workgroup)
{
	torture_comment(tctx, "Open Connection to %s/%s\n",host,share);
	smb->in.dest_host=talloc_strdup(mem_ctx,host);
	smb->in.service=talloc_strdup(mem_ctx,share);
	smb->in.dest_ports=lpcfg_smb_ports(tctx->lp_ctx);
	smb->in.socket_options = lpcfg_socket_options(tctx->lp_ctx);
	smb->in.called_name = strupper_talloc(mem_ctx, host);
	smb->in.service_type=NULL;
	smb->in.credentials=cmdline_credentials;
	smb->in.fallback_to_anonymous=false;
	smb->in.gensec_settings = lpcfg_gensec_settings(mem_ctx, tctx->lp_ctx);
	smb->in.workgroup=workgroup;
	lpcfg_smbcli_options(tctx->lp_ctx, &smb->in.options);
	lpcfg_smbcli_session_options(tctx->lp_ctx, &smb->in.session_options);
	
	return smb_composite_connect_send(smb,mem_ctx,
					  lpcfg_resolve_context(tctx->lp_ctx),ev);
}

bool run_benchrw(struct torture_context *tctx)
{
	struct smb_composite_connect *smb_con;
	const char *fname = "\\rwtest.dat";
	struct smbcli_request *req;
	struct benchrw_state **state;
	int i , num_unc_names;
	struct tevent_context 	*ev	;	
	struct composite_context *req1;
	struct params lpparams;
	union smb_mkdir parms;
	int finished = 0;
	bool success=true;
	int torture_nprocs = torture_setting_int(tctx, "nprocs", 4);
	
	torture_comment(tctx, "Start BENCH-READWRITE num_ops=%d "
			"num_nprocs=%d\n",
			torture_numops, torture_nprocs);

	/*init talloc context*/
	ev = tctx->ev;
	state = talloc_array(tctx, struct benchrw_state *, torture_nprocs);

	/* init params using lpcfg_parm_xxx */
	num_unc_names = init_benchrw_params(tctx,&lpparams);
	
	/* init private data structs*/
	for(i = 0; i<torture_nprocs;i++){
		state[i]=talloc(tctx,struct benchrw_state);
		state[i]->tctx = tctx;
		state[i]->completed=0;
		state[i]->num_parallel_requests=0;
		state[i]->lpcfg_params=&lpparams;
		state[i]->nr=i;
		state[i]->dname=talloc_asprintf(tctx,"benchrw%d",i);
		state[i]->fname=talloc_asprintf(tctx,"%s%s",
						state[i]->dname,fname);	
		state[i]->mode=START;
		state[i]->writecnt=0;
	}
	
	torture_comment(tctx, "Starting async requests\n");	
	while(finished != torture_nprocs){
		finished=0;
		for(i = 0; i<torture_nprocs;i++){
			switch (state[i]->mode){
			/*open multiple connections with the same userid */
			case START:
				smb_con = talloc(
					tctx,struct smb_composite_connect) ;
				state[i]->req_params=smb_con; 
				state[i]->mode=OPEN_CONNECTION;
				req1 = torture_connect_async(
					tctx, smb_con, tctx,ev,
					lpparams.unc[i % num_unc_names]->host,
					lpparams.unc[i % num_unc_names]->share,
					lpparams.workgroup);
				/* register callback fn + private data */
				req1->async.fn = async_open_callback;
				req1->async.private_data=state[i];
				break;
			/*setup test dirs (sync)*/
			case CLEANUP_TESTDIR:
				torture_comment(tctx, "Setup test dir %d\n",i);
				smb_raw_exit(state[i]->cli->session);
				if (smbcli_deltree(state[i]->cli, 
						state[i]->dname) == -1) {
					torture_comment(
						tctx,
						"Unable to delete %s - %s\n", 
						state[i]->dname,
						smbcli_errstr(state[i]->cli));
					state[i]->mode=ERROR;
					break;
				}
				state[i]->mode=MK_TESTDIR;
				parms.mkdir.level = RAW_MKDIR_MKDIR;
				parms.mkdir.in.path = state[i]->dname;
				req = smb_raw_mkdir_send(state[i]->cli,&parms);
				/* register callback fn + private data */
				req->async.fn = benchrw_callback;
				req->async.private_data=state[i];
				break;
			/* error occured , finish */
			case ERROR:
				finished++;
				success=false;
				break;
			/* cleanup , close connection */
			case CLEANUP:
				torture_comment(tctx, "Deleting test dir %s "
						"%d/%d\n",state[i]->dname,
						i+1,torture_nprocs);
				smbcli_deltree(state[i]->cli,state[i]->dname);
				if (NT_STATUS_IS_ERR(smb_tree_disconnect(
							     state[i]->cli))) {
					torture_comment(tctx, "ERROR: Tree "
							"disconnect failed");
					state[i]->mode=ERROR;
					break;
				}
				state[i]->mode=FINISHED;
			case FINISHED:
				finished++;
				break;
			default:
				event_loop_once(ev);
			}
		}
	}
				
	return success;	
}

