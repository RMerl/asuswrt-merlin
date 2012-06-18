/* 
   Unix SMB/CIFS implementation.

   open benchmark

   Copyright (C) Andrew Tridgell 2007
   
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
#include "system/time.h"
#include "system/filesys.h"
#include "libcli/libcli.h"
#include "torture/util.h"
#include "lib/events/events.h"
#include "lib/cmdline/popt_common.h"
#include "libcli/composite/composite.h"
#include "libcli/smb_composite/smb_composite.h"
#include "libcli/resolve/resolve.h"
#include "param/param.h"

#define BASEDIR "\\benchopen"

static int nprocs;
static int open_failed;
static int close_failed;
static char **fnames;
static int num_connected;
static struct tevent_timer *report_te;

struct benchopen_state {
	struct torture_context *tctx;
	TALLOC_CTX *mem_ctx;
	struct tevent_context *ev;
	struct smbcli_state *cli;
	struct smbcli_tree *tree;
	int client_num;
	int close_fnum;
	int open_fnum;
	int close_file_num;
	int open_file_num;
	int pending_file_num;
	int next_file_num;
	int count;
	int lastcount;
	union smb_open open_parms;
	int open_retries;
	union smb_close close_parms;
	struct smbcli_request *req_open;
	struct smbcli_request *req_close;
	struct smb_composite_connect reconnect;
	struct tevent_timer *te;

	/* these are used for reconnections */
	const char **dest_ports;
	const char *dest_host;
	const char *called_name;
	const char *service_type;
};

static void next_open(struct benchopen_state *state);
static void reopen_connection(struct tevent_context *ev, struct tevent_timer *te, 
			      struct timeval t, void *private_data);


/*
  complete an async reconnect
 */
static void reopen_connection_complete(struct composite_context *ctx)
{
	struct benchopen_state *state = (struct benchopen_state *)ctx->async.private_data;
	NTSTATUS status;
	struct smb_composite_connect *io = &state->reconnect;

	status = smb_composite_connect_recv(ctx, state->mem_ctx);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(state->te);
		state->te = event_add_timed(state->ev, state->mem_ctx, 
					    timeval_current_ofs(1,0), 
					    reopen_connection, state);
		return;
	}

	state->tree = io->out.tree;

	num_connected++;

	DEBUG(0,("[%u] reconnect to %s finished (%u connected)\n",
		 state->client_num, state->dest_host, num_connected));

	state->open_fnum = -1;
	state->close_fnum = -1;
	next_open(state);
}

	

/*
  reopen a connection
 */
static void reopen_connection(struct tevent_context *ev, struct tevent_timer *te, 
			      struct timeval t, void *private_data)
{
	struct benchopen_state *state = (struct benchopen_state *)private_data;
	struct composite_context *ctx;
	struct smb_composite_connect *io = &state->reconnect;
	char *host, *share;

	state->te = NULL;

	if (!torture_get_conn_index(state->client_num, state->mem_ctx, state->tctx, &host, &share)) {
		DEBUG(0,("Can't find host/share for reconnect?!\n"));
		exit(1);
	}

	io->in.dest_host    = state->dest_host;
	io->in.dest_ports   = state->dest_ports;
	io->in.socket_options = lp_socket_options(state->tctx->lp_ctx);
	io->in.called_name  = state->called_name;
	io->in.service      = share;
	io->in.service_type = state->service_type;
	io->in.credentials  = cmdline_credentials;
	io->in.fallback_to_anonymous = false;
	io->in.workgroup    = lp_workgroup(state->tctx->lp_ctx);
	io->in.gensec_settings = lp_gensec_settings(state->mem_ctx, state->tctx->lp_ctx);
	lp_smbcli_options(state->tctx->lp_ctx, &io->in.options);
	lp_smbcli_session_options(state->tctx->lp_ctx, &io->in.session_options);

	/* kill off the remnants of the old connection */
	talloc_free(state->tree);
	state->tree = NULL;
	state->open_fnum = -1;
	state->close_fnum = -1;

	ctx = smb_composite_connect_send(io, state->mem_ctx, 
					 lp_resolve_context(state->tctx->lp_ctx), 
					 state->ev);
	if (ctx == NULL) {
		DEBUG(0,("Failed to setup async reconnect\n"));
		exit(1);
	}

	ctx->async.fn = reopen_connection_complete;
	ctx->async.private_data = state;
}

static void open_completed(struct smbcli_request *req);
static void close_completed(struct smbcli_request *req);


static void next_open(struct benchopen_state *state)
{
	state->count++;

	state->pending_file_num = state->next_file_num;
	state->next_file_num = (state->next_file_num+1) % (3*nprocs);

	DEBUG(2,("[%d] opening %u\n", state->client_num, state->pending_file_num));
	state->open_parms.ntcreatex.level = RAW_OPEN_NTCREATEX;
	state->open_parms.ntcreatex.in.flags = 0;
	state->open_parms.ntcreatex.in.root_fid = 0;
	state->open_parms.ntcreatex.in.access_mask = SEC_RIGHTS_FILE_ALL;
	state->open_parms.ntcreatex.in.file_attr = FILE_ATTRIBUTE_NORMAL;
	state->open_parms.ntcreatex.in.alloc_size = 0;
	state->open_parms.ntcreatex.in.share_access = 0;
	state->open_parms.ntcreatex.in.open_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	state->open_parms.ntcreatex.in.create_options = 0;
	state->open_parms.ntcreatex.in.impersonation = 0;
	state->open_parms.ntcreatex.in.security_flags = 0;
	state->open_parms.ntcreatex.in.fname = fnames[state->pending_file_num];

	state->req_open = smb_raw_open_send(state->tree, &state->open_parms);
	state->req_open->async.fn = open_completed;
	state->req_open->async.private_data = state;
}


static void next_close(struct benchopen_state *state)
{
	if (state->close_fnum == -1) {
		return;
	}
	DEBUG(2,("[%d] closing %d (fnum[%d])\n",
		 state->client_num, state->close_file_num, state->close_fnum));
	state->close_parms.close.level = RAW_CLOSE_CLOSE;
	state->close_parms.close.in.file.fnum = state->close_fnum;
	state->close_parms.close.in.write_time = 0;

	state->req_close = smb_raw_close_send(state->tree, &state->close_parms);
	state->req_close->async.fn = close_completed;
	state->req_close->async.private_data = state;
}

/*
  called when a open completes
*/
static void open_completed(struct smbcli_request *req)
{
	struct benchopen_state *state = (struct benchopen_state *)req->async.private_data;
	TALLOC_CTX *tmp_ctx = talloc_new(state->mem_ctx);
	NTSTATUS status;

	status = smb_raw_open_recv(req, tmp_ctx, &state->open_parms);

	talloc_free(tmp_ctx);

	state->req_open = NULL;

	if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_LOCAL_DISCONNECT)) {
		talloc_free(state->tree);
		talloc_free(state->cli);
		state->tree = NULL;
		state->cli = NULL;
		num_connected--;	
		DEBUG(0,("[%u] reopening connection to %s\n",
			 state->client_num, state->dest_host));
		talloc_free(state->te);
		state->te = event_add_timed(state->ev, state->mem_ctx, 
					    timeval_current_ofs(1,0), 
					    reopen_connection, state);
		return;
	}

	if (NT_STATUS_EQUAL(status, NT_STATUS_SHARING_VIOLATION)) {
		DEBUG(2,("[%d] retrying open %d\n",
			 state->client_num, state->pending_file_num));
		state->open_retries++;
		state->req_open = smb_raw_open_send(state->tree, &state->open_parms);
		state->req_open->async.fn = open_completed;
		state->req_open->async.private_data = state;
		return;
	}

	if (!NT_STATUS_IS_OK(status)) {
		open_failed++;
		DEBUG(0,("[%u] open failed %d - %s\n",
			 state->client_num, state->pending_file_num,
			 nt_errstr(status)));
		return;
	}

	state->close_file_num = state->open_file_num;
	state->close_fnum = state->open_fnum;
	state->open_file_num = state->pending_file_num;
	state->open_fnum = state->open_parms.ntcreatex.out.file.fnum;

	DEBUG(2,("[%d] open completed %d (fnum[%d])\n",
		 state->client_num, state->open_file_num, state->open_fnum));

	if (state->close_fnum != -1) {
		next_close(state);
	}

	next_open(state);
}	

/*
  called when a close completes
*/
static void close_completed(struct smbcli_request *req)
{
	struct benchopen_state *state = (struct benchopen_state *)req->async.private_data;
	NTSTATUS status = smbcli_request_simple_recv(req);

	state->req_close = NULL;

	if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_LOCAL_DISCONNECT)) {
		talloc_free(state->tree);
		talloc_free(state->cli);
		state->tree = NULL;
		state->cli = NULL;
		num_connected--;	
		DEBUG(0,("[%u] reopening connection to %s\n",
			 state->client_num, state->dest_host));
		talloc_free(state->te);
		state->te = event_add_timed(state->ev, state->mem_ctx, 
					    timeval_current_ofs(1,0), 
					    reopen_connection, state);
		return;
	}

	if (!NT_STATUS_IS_OK(status)) {
		close_failed++;
		DEBUG(0,("[%u] close failed %d (fnum[%d]) - %s\n",
			 state->client_num, state->close_file_num,
			 state->close_fnum,
			 nt_errstr(status)));
		return;
	}

	DEBUG(2,("[%d] close completed %d (fnum[%d])\n",
		 state->client_num, state->close_file_num,
		 state->close_fnum));
}	

static void echo_completion(struct smbcli_request *req)
{
	struct benchopen_state *state = (struct benchopen_state *)req->async.private_data;
	NTSTATUS status = smbcli_request_simple_recv(req);
	if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE) ||
	    NT_STATUS_EQUAL(status, NT_STATUS_LOCAL_DISCONNECT)) {
		talloc_free(state->tree);
		state->tree = NULL;
		num_connected--;	
		DEBUG(0,("[%u] reopening connection to %s\n",
			 state->client_num, state->dest_host));
		talloc_free(state->te);
		state->te = event_add_timed(state->ev, state->mem_ctx, 
					    timeval_current_ofs(1,0), 
					    reopen_connection, state);
	}
}

static void report_rate(struct tevent_context *ev, struct tevent_timer *te, 
			struct timeval t, void *private_data)
{
	struct benchopen_state *state = talloc_get_type(private_data, 
							struct benchopen_state);
	int i;
	for (i=0;i<nprocs;i++) {
		printf("%5u ", (unsigned)(state[i].count - state[i].lastcount));
		state[i].lastcount = state[i].count;
	}
	printf("\r");
	fflush(stdout);
	report_te = event_add_timed(ev, state, timeval_current_ofs(1, 0), 
				    report_rate, state);

	/* send an echo on each interface to ensure it stays alive - this helps
	   with IP takeover */
	for (i=0;i<nprocs;i++) {
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
   benchmark open calls
*/
bool torture_bench_open(struct torture_context *torture)
{
	bool ret = true;
	TALLOC_CTX *mem_ctx = talloc_new(torture);
	int i;
	int timelimit = torture_setting_int(torture, "timelimit", 10);
	struct timeval tv;
	struct benchopen_state *state;
	int total = 0;
	int total_retries = 0;
	int minops = 0;
	bool progress=false;

	progress = torture_setting_bool(torture, "progress", true);
	
	nprocs = torture_setting_int(torture, "nprocs", 4);

	state = talloc_zero_array(mem_ctx, struct benchopen_state, nprocs);

	printf("Opening %d connections\n", nprocs);
	for (i=0;i<nprocs;i++) {
		state[i].tctx = torture;
		state[i].mem_ctx = talloc_new(state);
		state[i].client_num = i;
		state[i].ev = torture->ev;
		if (!torture_open_connection_ev(&state[i].cli, i, torture, torture->ev)) {
			return false;
		}
		talloc_steal(mem_ctx, state);
		state[i].tree = state[i].cli->tree;
		state[i].dest_host = talloc_strdup(state[i].mem_ctx, 
						   state[i].cli->tree->session->transport->socket->hostname);
		state[i].dest_ports = talloc_array(state[i].mem_ctx, 
						   const char *, 2);
		state[i].dest_ports[0] = talloc_asprintf(state[i].dest_ports, 
							 "%u", state[i].cli->tree->session->transport->socket->port);
		state[i].dest_ports[1] = NULL;
		state[i].called_name  = talloc_strdup(state[i].mem_ctx,
						      state[i].cli->tree->session->transport->called.name);
		state[i].service_type = talloc_strdup(state[i].mem_ctx,
						      state[i].cli->tree->device);
	}

	num_connected = i;

	if (!torture_setup_dir(state[0].cli, BASEDIR)) {
		goto failed;
	}

	fnames = talloc_array(mem_ctx, char *, 3*nprocs);
	for (i=0;i<3*nprocs;i++) {
		fnames[i] = talloc_asprintf(fnames, "%s\\file%d.dat", BASEDIR, i);
	}

	for (i=0;i<nprocs;i++) {
		/* all connections start with the same file */
		state[i].next_file_num = 0;
		state[i].open_fnum = -1;
		state[i].close_fnum = -1;
		next_open(&state[i]);
	}

	tv = timeval_current();	

	if (progress) {
		report_te = event_add_timed(torture->ev, state, timeval_current_ofs(1, 0), 
					    report_rate, state);
	}

	printf("Running for %d seconds\n", timelimit);
	while (timeval_elapsed(&tv) < timelimit) {
		event_loop_once(torture->ev);

		if (open_failed) {
			DEBUG(0,("open failed\n"));
			goto failed;
		}
		if (close_failed) {
			DEBUG(0,("open failed\n"));
			goto failed;
		}
	}

	talloc_free(report_te);
	if (progress) {
		for (i=0;i<nprocs;i++) {
			printf("      ");
		}
		printf("\r");
	}

	minops = state[0].count;
	for (i=0;i<nprocs;i++) {
		total += state[i].count;
		total_retries += state[i].open_retries;
		printf("[%d] %u ops (%u retries)\n",
		       i, state[i].count, state[i].open_retries);
		if (state[i].count < minops) minops = state[i].count;
	}
	printf("%.2f ops/second (%d retries)\n",
	       total/timeval_elapsed(&tv), total_retries);
	if (minops < 0.5*total/nprocs) {
		printf("Failed: unbalanced open\n");
		goto failed;
	}

	for (i=0;i<nprocs;i++) {
		talloc_free(state[i].req_open);
		talloc_free(state[i].req_close);
		smb_raw_exit(state[i].tree->session);
	}

	smbcli_deltree(state[0].tree, BASEDIR);
	talloc_free(mem_ctx);
	return ret;

failed:
	talloc_free(mem_ctx);
	return false;
}
