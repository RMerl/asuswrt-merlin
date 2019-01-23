/*
   Unix SMB/CIFS implementation.
   Run the async echo responder
   Copyright (C) Volker Lendecke 2010

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
#include "rpc_client/cli_pipe.h"
#include "librpc/gen_ndr/ndr_echo_c.h"

static void rpccli_sleep_done(struct tevent_req *req)
{
	int *done = (int *)tevent_req_callback_data_void(req);
	NTSTATUS status;
	uint32_t result = UINT32_MAX;

	status = dcerpc_echo_TestSleep_recv(req, talloc_tos(), &result);
	TALLOC_FREE(req);
	printf("sleep returned %s, %d\n", nt_errstr(status), (int)result);
	*done -= 1;
}

static void cli_echo_done(struct tevent_req *req)
{
	int *done = (int *)tevent_req_callback_data_void(req);
	NTSTATUS status;

	status = cli_echo_recv(req);
	TALLOC_FREE(req);
	printf("echo returned %s\n", nt_errstr(status));
	*done -= 1;
}

static void cli_close_done(struct tevent_req *req)
{
	int *done = (int *)tevent_req_callback_data_void(req);
	NTSTATUS status;
	size_t written;

	status = cli_write_andx_recv(req, &written);
	TALLOC_FREE(req);
	printf("close returned %s\n", nt_errstr(status));
	*done -= 1;
}

bool run_async_echo(int dummy)
{
	struct cli_state *cli = NULL;
	struct rpc_pipe_client *p;
	struct dcerpc_binding_handle *b;
	struct tevent_context *ev;
	struct tevent_req *req;
	NTSTATUS status;
	bool ret = false;
	int i, num_reqs;
	uint8_t buf[32768];

	printf("Starting ASYNC_ECHO\n");

	ev = tevent_context_init(talloc_tos());
	if (ev == NULL) {
		printf("tevent_context_init failed\n");
		goto fail;
	}

	if (!torture_open_connection(&cli, 0)) {
		printf("torture_open_connection failed\n");
		goto fail;
	}
	status = cli_rpc_pipe_open_noauth(cli, &ndr_table_rpcecho.syntax_id,
					  &p);
	if (!NT_STATUS_IS_OK(status)) {
		printf("Could not open echo pipe: %s\n", nt_errstr(status));
		goto fail;
	}
	b = p->binding_handle;

	num_reqs = 0;

	req = dcerpc_echo_TestSleep_send(ev, ev, b, 15);
	if (req == NULL) {
		printf("rpccli_echo_TestSleep_send failed\n");
		goto fail;
	}
	tevent_req_set_callback(req, rpccli_sleep_done, &num_reqs);
	num_reqs += 1;

	req = cli_echo_send(ev, ev, cli, 1, data_blob_const("hello", 5));
	if (req == NULL) {
		printf("cli_echo_send failed\n");
		goto fail;
	}
	tevent_req_set_callback(req, cli_echo_done, &num_reqs);
	num_reqs += 1;

	memset(buf, 0, sizeof(buf));

	for (i=0; i<10; i++) {
		req = cli_write_andx_send(ev, ev, cli, 4711, 0, buf, 0, sizeof(buf));
		if (req == NULL) {
			printf("cli_close_send failed\n");
			goto fail;
		}
		tevent_req_set_callback(req, cli_close_done, &num_reqs);
		num_reqs += 1;

		req = cli_echo_send(ev, ev, cli, 1, data_blob_const("hello", 5));
		if (req == NULL) {
			printf("cli_echo_send failed\n");
			goto fail;
		}
		tevent_req_set_callback(req, cli_echo_done, &num_reqs);
		num_reqs += 1;
	}

	while (num_reqs > 0) {
		if (tevent_loop_once(ev) != 0) {
			printf("tevent_loop_once failed\n");
			goto fail;
		}
	}

	TALLOC_FREE(p);

	ret = true;
fail:
	if (cli != NULL) {
		torture_close_connection(cli);
	}
	return ret;
}
