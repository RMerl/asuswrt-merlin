/* 
   Unix SMB/CIFS implementation.

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

/*
  test offline files
 */

#include "includes.h"
#include "system/time.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "lib/events/events.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"

#define BASEDIR "\\testoffline"

static int nconnections;
static int numstates;
static int num_connected;
static int test_failed;
extern int torture_numops;
extern int torture_entries;
static bool test_finished;

enum offline_op {OP_LOADFILE, OP_SAVEFILE, OP_SETOFFLINE, OP_GETOFFLINE, OP_ENDOFLIST};

static double latencies[OP_ENDOFLIST];
static double worst_latencies[OP_ENDOFLIST];

#define FILE_SIZE 8192


struct offline_state {
	struct torture_context *tctx;
	struct tevent_context *ev;
	struct smbcli_tree *tree;
	TALLOC_CTX *mem_ctx;
	int client;
	int fnum;
	uint32_t count;
	uint32_t lastcount;
	uint32_t fnumber;
	uint32_t offline_count;
	uint32_t online_count;
	char *fname;
	struct smb_composite_loadfile *loadfile;
	struct smb_composite_savefile *savefile;
	struct smbcli_request *req;
	enum offline_op op;
	struct timeval tv_start;
};

static void test_offline(struct offline_state *state);


static char *filename(TALLOC_CTX *ctx, int i)
{
	char *s = talloc_asprintf(ctx, BASEDIR "\\file%u.dat", i);
	return s;
}


/*
  called when a loadfile completes
 */
static void loadfile_callback(struct composite_context *ctx) 
{
	struct offline_state *state = ctx->async.private_data;
	NTSTATUS status;
	int i;

	status = smb_composite_loadfile_recv(ctx, state->mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to read file '%s' - %s\n", 
		       state->loadfile->in.fname, nt_errstr(status));
		test_failed++;
	}

	/* check the data is correct */
	if (state->loadfile->out.size != FILE_SIZE) {
		printf("Wrong file size %u - expected %u\n", 
		       state->loadfile->out.size, FILE_SIZE);
		test_failed++;
		return;
	}

	for (i=0;i<FILE_SIZE;i++) {
		if (state->loadfile->out.data[i] != 1+(state->fnumber % 255)) {
			printf("Bad data in file %u (got %u expected %u)\n", 
			       state->fnumber, 
			       state->loadfile->out.data[i],
			       1+(state->fnumber % 255));
			test_failed++;
			return;
		}
	}
	
	talloc_steal(state->loadfile, state->loadfile->out.data);

	state->count++;
	talloc_free(state->loadfile);
	state->loadfile = NULL;

	if (!test_finished) {
		test_offline(state);
	}
}


/*
  called when a savefile completes
 */
static void savefile_callback(struct composite_context *ctx) 
{
	struct offline_state *state = ctx->async.private_data;
	NTSTATUS status;

	status = smb_composite_savefile_recv(ctx);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to save file '%s' - %s\n", 
		       state->savefile->in.fname, nt_errstr(status));
		test_failed++;
	}

	state->count++;
	talloc_free(state->savefile);
	state->savefile = NULL;

	if (!test_finished) {
		test_offline(state);
	}
}


/*
  called when a setoffline completes
 */
static void setoffline_callback(struct smbcli_request *req) 
{
	struct offline_state *state = req->async.private_data;
	NTSTATUS status;

	status = smbcli_request_simple_recv(req);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to set offline file '%s' - %s\n", 
		       state->fname, nt_errstr(status));
		test_failed++;
	}

	state->req = NULL;
	state->count++;

	if (!test_finished) {
		test_offline(state);
	}
}


/*
  called when a getoffline completes
 */
static void getoffline_callback(struct smbcli_request *req) 
{
	struct offline_state *state = req->async.private_data;
	NTSTATUS status;
	union smb_fileinfo io;

	io.getattr.level = RAW_FILEINFO_GETATTR;
	
	status = smb_raw_pathinfo_recv(req, state->mem_ctx, &io);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Failed to get offline file '%s' - %s\n", 
		       state->fname, nt_errstr(status));
		test_failed++;
	}

	if (io.getattr.out.attrib & FILE_ATTRIBUTE_OFFLINE) {
		state->offline_count++;
	} else {
		state->online_count++;
	}

	state->req = NULL;
	state->count++;

	if (!test_finished) {
		test_offline(state);
	}
}


/*
  send the next offline file fetch request
*/
static void test_offline(struct offline_state *state)
{
	struct composite_context *ctx;
	double lat;

	lat = timeval_elapsed(&state->tv_start);
	if (latencies[state->op] < lat) {
		latencies[state->op] = lat;
	}

	state->op = (enum offline_op) (random() % OP_ENDOFLIST);
	
	state->fnumber = random() % torture_numops;
	talloc_free(state->fname);
	state->fname = filename(state->mem_ctx, state->fnumber);

	state->tv_start = timeval_current();

	switch (state->op) {
	case OP_LOADFILE:
		state->loadfile = talloc_zero(state->mem_ctx, struct smb_composite_loadfile);
		state->loadfile->in.fname = state->fname;
	
		ctx = smb_composite_loadfile_send(state->tree, state->loadfile);
		if (ctx == NULL) {
			printf("Failed to setup loadfile for %s\n", state->fname);
			test_failed = true;
		}

		talloc_steal(state->loadfile, ctx);

		ctx->async.fn = loadfile_callback;
		ctx->async.private_data = state;
		break;

	case OP_SAVEFILE:
		state->savefile = talloc_zero(state->mem_ctx, struct smb_composite_savefile);

		state->savefile->in.fname = state->fname;
		state->savefile->in.data  = talloc_size(state->savefile, FILE_SIZE);
		state->savefile->in.size  = FILE_SIZE;
		memset(state->savefile->in.data, 1+(state->fnumber%255), FILE_SIZE);
	
		ctx = smb_composite_savefile_send(state->tree, state->savefile);
		if (ctx == NULL) {
			printf("Failed to setup savefile for %s\n", state->fname);
			test_failed = true;
		}

		talloc_steal(state->savefile, ctx);

		ctx->async.fn = savefile_callback;
		ctx->async.private_data = state;
		break;

	case OP_SETOFFLINE: {
		union smb_setfileinfo io;
		ZERO_STRUCT(io);
		io.setattr.level = RAW_SFILEINFO_SETATTR;
		io.setattr.in.attrib = FILE_ATTRIBUTE_OFFLINE;
		io.setattr.in.file.path = state->fname;
		/* make the file 1 hour old, to get past mininum age restrictions 
		   for HSM systems */
		io.setattr.in.write_time = time(NULL) - 60*60;

		state->req = smb_raw_setpathinfo_send(state->tree, &io);
		if (state->req == NULL) {
			printf("Failed to setup setoffline for %s\n", state->fname);
			test_failed = true;
		}
		
		state->req->async.fn = setoffline_callback;
		state->req->async.private_data = state;
		break;
	}

	case OP_GETOFFLINE: {
		union smb_fileinfo io;
		ZERO_STRUCT(io);
		io.getattr.level = RAW_FILEINFO_GETATTR;
		io.getattr.in.file.path = state->fname;

		state->req = smb_raw_pathinfo_send(state->tree, &io);
		if (state->req == NULL) {
			printf("Failed to setup getoffline for %s\n", state->fname);
			test_failed = true;
		}
		
		state->req->async.fn = getoffline_callback;
		state->req->async.private_data = state;
		break;
	}

	default:
		printf("bad operation??\n");
		break;
	}
}




static void echo_completion(struct smbcli_request *req)
{
	struct offline_state *state = (struct offline_state *)req->async.private_data;
	NTSTATUS status = smbcli_request_simple_recv(req);
	if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_LOCAL_DISCONNECT) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_CONNECTION_RESET)) {
		talloc_free(state->tree);
		state->tree = NULL;
		num_connected--;	
		DEBUG(0,("lost connection\n"));
		test_failed++;
	}
}

static void report_rate(struct tevent_context *ev, struct tevent_timer *te, 
			struct timeval t, void *private_data)
{
	struct offline_state *state = talloc_get_type(private_data, 
							struct offline_state);
	int i;
	uint32_t total=0, total_offline=0, total_online=0;
	for (i=0;i<numstates;i++) {
		total += state[i].count - state[i].lastcount;
		if (timeval_elapsed(&state[i].tv_start) > latencies[state[i].op]) {
			latencies[state[i].op] = timeval_elapsed(&state[i].tv_start);
		}
		state[i].lastcount = state[i].count;		
		total_online += state[i].online_count;
		total_offline += state[i].offline_count;
	}
	printf("ops/s=%4u  offline=%5u online=%4u  set_lat=%.1f/%.1f get_lat=%.1f/%.1f save_lat=%.1f/%.1f load_lat=%.1f/%.1f\n",
	       total, total_offline, total_online,
	       latencies[OP_SETOFFLINE],
	       worst_latencies[OP_SETOFFLINE],
	       latencies[OP_GETOFFLINE],
	       worst_latencies[OP_GETOFFLINE],
	       latencies[OP_SAVEFILE],
	       worst_latencies[OP_SAVEFILE],
	       latencies[OP_LOADFILE],
	       worst_latencies[OP_LOADFILE]);
	fflush(stdout);
	event_add_timed(ev, state, timeval_current_ofs(1, 0), report_rate, state);

	for (i=0;i<OP_ENDOFLIST;i++) {
		if (latencies[i] > worst_latencies[i]) {
			worst_latencies[i] = latencies[i];
		}
		latencies[i] = 0;
	}

	/* send an echo on each interface to ensure it stays alive - this helps
	   with IP takeover */
	for (i=0;i<numstates;i++) {
		struct smb_echo p;
		struct smbcli_request *req;

		if (!state[i].tree) {
			continue;
		}

		p.in.repeat_count = 1;
		p.in.size = 0;
		p.in.data = NULL;
		req = smb_raw_echo_send(state[i].tree->session->transport, &p);
		req->async.private_data = &state[i];
		req->async.fn      = echo_completion;
	}
}

/* 
   test offline file handling
*/
bool torture_test_offline(struct torture_context *torture)
{
	bool ret = true;
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	int i;
	int timelimit = torture_setting_int(torture, "timelimit", 10);
	struct timeval tv;
	struct offline_state *state;
	struct smbcli_state *cli;
	bool progress;
	progress = torture_setting_bool(torture, "progress", true);

	nconnections = torture_setting_int(torture, "nprocs", 4);
	numstates = nconnections * torture_entries;

	state = talloc_zero_array(mem_ctx, struct offline_state, numstates);

	printf("Opening %d connections with %d simultaneous operations and %u files\n", nconnections, numstates, torture_numops);
	for (i=0;i<nconnections;i++) {
		state[i].tctx = torture;
		state[i].mem_ctx = talloc_new(state);
		state[i].ev = torture->ev;
		if (!torture_open_connection_ev(&cli, i, torture, torture->ev)) {
			return false;
		}
		state[i].tree = cli->tree;
		state[i].client = i;
		/* allow more time for offline files */
		state[i].tree->session->transport->options.request_timeout = 200;
	}

	/* the others are repeats on the earlier connections */
	for (i=nconnections;i<numstates;i++) {
		state[i].tctx = torture;
		state[i].mem_ctx = talloc_new(state);
		state[i].ev = torture->ev;
		state[i].tree = state[i % nconnections].tree;
		state[i].client = i;
	}

	num_connected = i;

	if (!torture_setup_dir(cli, BASEDIR)) {
		goto failed;
	}

	/* pre-create files */
	printf("Pre-creating %u files ....\n", torture_numops);
	for (i=0;i<torture_numops;i++) {
		int fnum;
		char *fname = filename(mem_ctx, i);
		char buf[FILE_SIZE];
		NTSTATUS status;

		memset(buf, 1+(i % 255), sizeof(buf));

		fnum = smbcli_open(state[0].tree, fname, O_RDWR|O_CREAT, DENY_NONE);
		if (fnum == -1) {
			printf("Failed to open %s on connection %d\n", fname, i);
			goto failed;
		}

		if (smbcli_write(state[0].tree, fnum, 0, buf, 0, sizeof(buf)) != sizeof(buf)) {
			printf("Failed to write file of size %u\n", FILE_SIZE);
			goto failed;
		}

		status = smbcli_close(state[0].tree, fnum);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Close failed - %s\n", nt_errstr(status));
			goto failed;
		}

		talloc_free(fname);
	}

	/* start the async ops */
	for (i=0;i<numstates;i++) {
		state[i].tv_start = timeval_current();
		test_offline(&state[i]);
	}

	tv = timeval_current();	

	if (progress) {
		event_add_timed(torture->ev, state, timeval_current_ofs(1, 0), report_rate, state);
	}

	printf("Running for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		event_loop_once(torture->ev);

		if (test_failed) {
			DEBUG(0,("test failed\n"));
			goto failed;
		}
	}

	printf("\nWaiting for completion\n");
	test_finished = true;
	for (i=0;i<numstates;i++) {
		while (state[i].loadfile || 
		       state[i].savefile ||
		       state[i].req) {
			event_loop_once(torture->ev);
		}
	}	

	printf("worst latencies: set_lat=%.1f get_lat=%.1f save_lat=%.1f load_lat=%.1f\n",
	       worst_latencies[OP_SETOFFLINE],
	       worst_latencies[OP_GETOFFLINE],
	       worst_latencies[OP_SAVEFILE],
	       worst_latencies[OP_LOADFILE]);

	smbcli_deltree(state[0].tree, BASEDIR);
	talloc_free(mem_ctx);
	printf("\n");
	return ret;

failed:
	talloc_free(mem_ctx);
	return false;
}
