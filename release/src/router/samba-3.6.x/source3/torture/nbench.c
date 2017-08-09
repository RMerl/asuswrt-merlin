/*
   Unix SMB/CIFS implementation.
   In-memory cache
   Copyright (C) Volker Lendecke 2007

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
#include "torture/proto.h"
#include "libsmb/libsmb.h"
#include "libsmb/clirap.h"
#include "../lib/util/tevent_ntstatus.h"

static long long int ival(const char *str)
{
	return strtoll(str, NULL, 0);
}

struct nbench_state {
	struct tevent_context *ev;
	struct cli_state *cli;
	const char *cliname;
	FILE *loadfile;
	struct ftable *ftable;
	void (*bw_report)(size_t nread,
			  size_t nwritten,
			  void *private_data);
	void *bw_report_private;
};

struct lock_info {
	struct lock_info *next, *prev;
	off_t offset;
	int size;
};

struct createx_params {
	char *fname;
	unsigned int cr_options;
	unsigned int cr_disposition;
	int handle;
};

struct ftable {
	struct ftable *next, *prev;
	struct createx_params cp;
	struct lock_info *locks;
	uint16_t fnum; /* the fd that we got back from the server */
};

enum nbench_cmd {
	NBENCH_CMD_NTCREATEX,
	NBENCH_CMD_CLOSE,
	NBENCH_CMD_RENAME,
	NBENCH_CMD_UNLINK,
	NBENCH_CMD_DELTREE,
	NBENCH_CMD_RMDIR,
	NBENCH_CMD_MKDIR,
	NBENCH_CMD_QUERY_PATH_INFORMATION,
	NBENCH_CMD_QUERY_FILE_INFORMATION,
	NBENCH_CMD_QUERY_FS_INFORMATION,
	NBENCH_CMD_SET_FILE_INFORMATION,
	NBENCH_CMD_FIND_FIRST,
	NBENCH_CMD_WRITEX,
	NBENCH_CMD_WRITE,
	NBENCH_CMD_LOCKX,
	NBENCH_CMD_UNLOCKX,
	NBENCH_CMD_READX,
	NBENCH_CMD_FLUSH,
	NBENCH_CMD_SLEEP,
};

struct nbench_cmd_struct {
	char **params;
	int num_params;
	NTSTATUS status;
	enum nbench_cmd cmd;
};

static struct nbench_cmd_struct *nbench_parse(TALLOC_CTX *mem_ctx,
					      const char *line)
{
	struct nbench_cmd_struct *result;
	char *cmd;
	char *status;

	result = TALLOC_P(mem_ctx, struct nbench_cmd_struct);
	if (result == NULL) {
		return NULL;
	}
	result->params = str_list_make_shell(mem_ctx, line, " ");
	if (result->params == NULL) {
		goto fail;
	}
	result->num_params = talloc_array_length(result->params) - 1;
	if (result->num_params < 2) {
		goto fail;
	}
	status = result->params[result->num_params-1];
	if (strncmp(status, "NT_STATUS_", 10) != 0 &&
	    strncmp(status, "0x", 2) != 0) {
		goto fail;
	}
	/* accept numeric or string status codes */
	if (strncmp(status, "0x", 2) == 0) {
		result->status = NT_STATUS(strtoul(status, NULL, 16));
	} else {
		result->status = nt_status_string_to_code(status);
	}

	cmd = result->params[0];

	if (!strcmp(cmd, "NTCreateX")) {
		result->cmd = NBENCH_CMD_NTCREATEX;
	} else if (!strcmp(cmd, "Close")) {
		result->cmd = NBENCH_CMD_CLOSE;
	} else if (!strcmp(cmd, "Rename")) {
		result->cmd = NBENCH_CMD_RENAME;
	} else if (!strcmp(cmd, "Unlink")) {
		result->cmd = NBENCH_CMD_UNLINK;
	} else if (!strcmp(cmd, "Deltree")) {
		result->cmd = NBENCH_CMD_DELTREE;
	} else if (!strcmp(cmd, "Rmdir")) {
		result->cmd = NBENCH_CMD_RMDIR;
	} else if (!strcmp(cmd, "Mkdir")) {
		result->cmd = NBENCH_CMD_MKDIR;
	} else if (!strcmp(cmd, "QUERY_PATH_INFORMATION")) {
		result->cmd = NBENCH_CMD_QUERY_PATH_INFORMATION;
	} else if (!strcmp(cmd, "QUERY_FILE_INFORMATION")) {
		result->cmd = NBENCH_CMD_QUERY_FILE_INFORMATION;
	} else if (!strcmp(cmd, "QUERY_FS_INFORMATION")) {
		result->cmd = NBENCH_CMD_QUERY_FS_INFORMATION;
	} else if (!strcmp(cmd, "SET_FILE_INFORMATION")) {
		result->cmd = NBENCH_CMD_SET_FILE_INFORMATION;
	} else if (!strcmp(cmd, "FIND_FIRST")) {
		result->cmd = NBENCH_CMD_FIND_FIRST;
	} else if (!strcmp(cmd, "WriteX")) {
		result->cmd = NBENCH_CMD_WRITEX;
	} else if (!strcmp(cmd, "Write")) {
		result->cmd = NBENCH_CMD_WRITE;
	} else if (!strcmp(cmd, "LockX")) {
		result->cmd = NBENCH_CMD_LOCKX;
	} else if (!strcmp(cmd, "UnlockX")) {
		result->cmd = NBENCH_CMD_UNLOCKX;
	} else if (!strcmp(cmd, "ReadX")) {
		result->cmd = NBENCH_CMD_READX;
	} else if (!strcmp(cmd, "Flush")) {
		result->cmd = NBENCH_CMD_FLUSH;
	} else if (!strcmp(cmd, "Sleep")) {
		result->cmd = NBENCH_CMD_SLEEP;
	} else {
		goto fail;
	}
	return result;
fail:
	TALLOC_FREE(result);
	return NULL;
}

static struct ftable *ft_find(struct ftable *ftlist, int handle)
{
	while (ftlist != NULL) {
		if (ftlist->cp.handle == handle) {
			return ftlist;
		}
		ftlist = ftlist->next;
	}
	return NULL;
}

struct nbench_cmd_state {
	struct tevent_context *ev;
	struct nbench_state *state;
	struct nbench_cmd_struct *cmd;
	struct ftable *ft;
	bool eof;
};

static void nbench_cmd_done(struct tevent_req *subreq);

static struct tevent_req *nbench_cmd_send(TALLOC_CTX *mem_ctx,
					  struct tevent_context *ev,
					  struct nbench_state *nb_state)
{
	struct tevent_req *req, *subreq;
	struct nbench_cmd_state *state;
	char line[1024];
	size_t len;

	req = tevent_req_create(mem_ctx, &state, struct nbench_cmd_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->state = nb_state;

	if (fgets(line, sizeof(line), nb_state->loadfile) == NULL) {
		tevent_req_nterror(req, NT_STATUS_END_OF_FILE);
		return tevent_req_post(req, ev);
	}
	len = strlen(line);
	if (len == 0) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}
	if (line[len-1] == '\n') {
		line[len-1] = '\0';
	}

	state->cmd = nbench_parse(state, line);
	if (state->cmd == NULL) {
		tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
		return tevent_req_post(req, ev);
	}

	switch (state->cmd->cmd) {
	case NBENCH_CMD_NTCREATEX: {
		uint32_t desired_access;
		uint32_t share_mode;
		unsigned int flags = 0;

		state->ft = talloc(state, struct ftable);
		if (tevent_req_nomem(state->ft, req)) {
			return tevent_req_post(req, ev);
		}

		state->ft->cp.fname = talloc_all_string_sub(
			state->ft, state->cmd->params[1], "client1",
			nb_state->cliname);
		if (tevent_req_nomem(state->ft->cp.fname, req)) {
			return tevent_req_post(req, ev);
		}
		state->ft->cp.cr_options = ival(state->cmd->params[2]);
		state->ft->cp.cr_disposition = ival(state->cmd->params[3]);
		state->ft->cp.handle = ival(state->cmd->params[4]);

		if (state->ft->cp.cr_options & FILE_DIRECTORY_FILE) {
			desired_access = SEC_FILE_READ_DATA;
		} else {
			desired_access =
				SEC_FILE_READ_DATA |
				SEC_FILE_WRITE_DATA |
				SEC_FILE_READ_ATTRIBUTE |
				SEC_FILE_WRITE_ATTRIBUTE;
			flags = EXTENDED_RESPONSE_REQUIRED
				| REQUEST_OPLOCK | REQUEST_BATCH_OPLOCK;
		}
		share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;

		subreq = cli_ntcreate_send(
			state, ev, nb_state->cli, state->ft->cp.fname, flags,
			desired_access, 0, share_mode,
			state->ft->cp.cr_disposition,
			state->ft->cp.cr_options, 0);
		break;
	}
	case NBENCH_CMD_CLOSE: {
		state->ft = ft_find(state->state->ftable,
				    ival(state->cmd->params[1]));
		if (state->ft == NULL) {
			tevent_req_nterror(req, NT_STATUS_INVALID_PARAMETER);
			return tevent_req_post(req, ev);
		}
		subreq = cli_close_send(
			state, ev, nb_state->cli, state->ft->fnum);
		break;
	}
	case NBENCH_CMD_MKDIR: {
		char *fname;
		fname = talloc_all_string_sub(
			state, state->cmd->params[1], "client1",
			nb_state->cliname);
		if (tevent_req_nomem(state->ft->cp.fname, req)) {
			return tevent_req_post(req, ev);
		}
		subreq = cli_mkdir_send(state, ev, nb_state->cli, fname);
		break;
	}
	case NBENCH_CMD_QUERY_PATH_INFORMATION: {
		char *fname;
		fname = talloc_all_string_sub(
			state, state->cmd->params[1], "client1",
			nb_state->cliname);
		if (tevent_req_nomem(state->ft->cp.fname, req)) {
			return tevent_req_post(req, ev);
		}
		subreq = cli_qpathinfo_send(state, ev, nb_state->cli, fname,
					    ival(state->cmd->params[2]),
					    0, nb_state->cli->max_xmit);
		break;
	}
	default:
		tevent_req_nterror(req, NT_STATUS_NOT_IMPLEMENTED);
		return tevent_req_post(req, ev);
	}

	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, nbench_cmd_done, req);
	return req;
}

static bool status_wrong(struct tevent_req *req, NTSTATUS expected,
			 NTSTATUS status)
{
	if (NT_STATUS_EQUAL(expected, status)) {
		return false;
	}
	if (NT_STATUS_IS_OK(status)) {
		status = NT_STATUS_INVALID_NETWORK_RESPONSE;
	}
	tevent_req_nterror(req, status);
	return true;
}

static void nbench_cmd_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nbench_cmd_state *state = tevent_req_data(
		req, struct nbench_cmd_state);
	struct nbench_state *nbstate = state->state;
	NTSTATUS status;

	switch (state->cmd->cmd) {
	case NBENCH_CMD_NTCREATEX: {
		struct ftable *ft;
		status = cli_ntcreate_recv(subreq, &state->ft->fnum);
		TALLOC_FREE(subreq);
		if (status_wrong(req, state->cmd->status, status)) {
			return;
		}
		if (!NT_STATUS_IS_OK(status)) {
			tevent_req_done(req);
			return;
		}
		ft = talloc_move(nbstate, &state->ft);
		DLIST_ADD(nbstate->ftable, ft);
		break;
	}
	case NBENCH_CMD_CLOSE: {
		status = cli_close_recv(subreq);
		TALLOC_FREE(subreq);
		if (status_wrong(req, state->cmd->status, status)) {
			return;
		}
		DLIST_REMOVE(state->state->ftable, state->ft);
		TALLOC_FREE(state->ft);
		break;
	}
	case NBENCH_CMD_MKDIR: {
		status = cli_mkdir_recv(subreq);
		TALLOC_FREE(subreq);
		if (status_wrong(req, state->cmd->status, status)) {
			return;
		}
		break;
	}
	case NBENCH_CMD_QUERY_PATH_INFORMATION: {
		status = cli_qpathinfo_recv(subreq, NULL, NULL, NULL);
		TALLOC_FREE(subreq);
		if (status_wrong(req, state->cmd->status, status)) {
			return;
		}
		break;
	}
	default:
		break;
	}
	tevent_req_done(req);
}

static NTSTATUS nbench_cmd_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

static void nbench_done(struct tevent_req *subreq);

static struct tevent_req *nbench_send(
	TALLOC_CTX *mem_ctx, struct tevent_context *ev, struct cli_state *cli,
	const char *cliname, FILE *loadfile,
	void (*bw_report)(size_t nread, size_t nwritten, void *private_data),
	void *bw_report_private)
{
	struct tevent_req *req, *subreq;
	struct nbench_state *state;

	req = tevent_req_create(mem_ctx, &state, struct nbench_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->cliname = cliname;
	state->loadfile = loadfile;
	state->bw_report = bw_report;
	state->bw_report_private = bw_report_private;

	subreq = nbench_cmd_send(state, ev, state);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, nbench_done, req);
	return req;
}

static void nbench_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct nbench_state *state = tevent_req_data(
		req, struct nbench_state);
	NTSTATUS status;

	status = nbench_cmd_recv(subreq);
	TALLOC_FREE(subreq);

	if (NT_STATUS_EQUAL(status, NT_STATUS_END_OF_FILE)) {
		tevent_req_done(req);
		return;
	}
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	subreq = nbench_cmd_send(state, state->ev, state);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	tevent_req_set_callback(subreq, nbench_done, req);
}

static NTSTATUS nbench_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

bool run_nbench2(int dummy)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct tevent_context *ev;
	struct cli_state *cli = NULL;
	FILE *loadfile;
	bool ret = false;
	struct tevent_req *req;
	NTSTATUS status;

	loadfile = fopen("client.txt", "r");
	if (loadfile == NULL) {
		fprintf(stderr, "Could not open \"client.txt\": %s\n",
			strerror(errno));
		return false;
	}
	ev = tevent_context_init(talloc_tos());
	if (ev == NULL) {
		goto fail;
	}
	if (!torture_open_connection(&cli, 0)) {
		goto fail;
	}

	req = nbench_send(talloc_tos(), ev, cli, "client1", loadfile,
			  NULL, NULL);
	if (req == NULL) {
		goto fail;
	}
	if (!tevent_req_poll(req, ev)) {
		goto fail;
	}
	status = nbench_recv(req);
	TALLOC_FREE(req);
	printf("nbench returned %s\n", nt_errstr(status));

	ret = true;
fail:
	if (cli != NULL) {
		torture_close_connection(cli);
	}
	TALLOC_FREE(ev);
	if (loadfile != NULL) {
		fclose(loadfile);
		loadfile = NULL;
	}
	TALLOC_FREE(frame);
	return ret;
}
