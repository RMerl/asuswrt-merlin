/* 
   Unix SMB/CIFS implementation.
   SMB client oplock functions
   Copyright (C) Andrew Tridgell 2001
   
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
#include "../lib/util/tevent_ntstatus.h"
#include "async_smb.h"
#include "libsmb/libsmb.h"

/****************************************************************************
send an ack for an oplock break request
****************************************************************************/

struct cli_oplock_ack_state {
	uint16_t vwv[8];
};

static void cli_oplock_ack_done(struct tevent_req *subreq);

struct tevent_req *cli_oplock_ack_send(TALLOC_CTX *mem_ctx,
				       struct tevent_context *ev,
				       struct cli_state *cli,
				       uint16_t fnum, uint8_t level)
{
	struct tevent_req *req, *subreq;
	struct cli_oplock_ack_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_oplock_ack_state);
	if (req == NULL) {
		return NULL;
	}
	SCVAL(state->vwv+0, 0, 0xff);
	SCVAL(state->vwv+0, 1, 0);
	SSVAL(state->vwv+1, 0, 0);
	SSVAL(state->vwv+2, 0, fnum);
	SCVAL(state->vwv+3, 0, LOCKING_ANDX_OPLOCK_RELEASE);
	SCVAL(state->vwv+3, 1, level);
	SIVAL(state->vwv+4, 0, 0); /* timeout */
	SSVAL(state->vwv+6, 0, 0); /* unlockcount */
	SSVAL(state->vwv+7, 0, 0); /* lockcount */

	subreq = cli_smb_send(state, ev, cli, SMBlockingX, 0, 8, state->vwv,
			      0, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_oplock_ack_done, req);
	return req;
}

static void cli_oplock_ack_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_smb_recv(subreq, NULL, NULL, 0, NULL, NULL, NULL, NULL);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_oplock_ack_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_oplock_ack(struct cli_state *cli, uint16_t fnum, unsigned char level)
{
	TALLOC_CTX *frame = talloc_stackframe();
	struct event_context *ev;
	struct tevent_req *req;
	NTSTATUS status = NT_STATUS_OK;

	if (cli_has_async_calls(cli)) {
		/*
		 * Can't use sync call while an async call is in flight
		 */
		status = NT_STATUS_INVALID_PARAMETER;
		goto fail;
	}

	ev = event_context_init(frame);
	if (ev == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	req = cli_oplock_ack_send(frame, ev, cli, fnum, level);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_oplock_ack_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
}

/****************************************************************************
set the oplock handler for a connection
****************************************************************************/

void cli_oplock_handler(struct cli_state *cli, 
			NTSTATUS (*handler)(struct cli_state *, uint16_t, unsigned char))
{
	cli->oplock_handler = handler;
}
