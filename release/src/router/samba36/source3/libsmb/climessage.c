/*
   Unix SMB/CIFS implementation.
   client message handling routines
   Copyright (C) Andrew Tridgell 1994-1998

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

struct cli_message_start_state {
	uint16_t grp;
};

static void cli_message_start_done(struct tevent_req *subreq);

static struct tevent_req *cli_message_start_send(TALLOC_CTX *mem_ctx,
						 struct tevent_context *ev,
						 struct cli_state *cli,
						 const char *host,
						 const char *username)
{
	struct tevent_req *req, *subreq;
	struct cli_message_start_state *state;
	char *htmp = NULL;
	char *utmp = NULL;
	size_t hlen, ulen;
	uint8_t *bytes, *p;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_message_start_state);
	if (req == NULL) {
		return NULL;
	}

	if (!convert_string_talloc(talloc_tos(), CH_UNIX, CH_DOS,
				   username, strlen(username)+1,
				   &utmp, &ulen, true)) {
		goto fail;
	}
	if (!convert_string_talloc(talloc_tos(), CH_UNIX, CH_DOS,
				   host, strlen(host)+1,
				   &htmp, &hlen, true)) {
		goto fail;
	}

	bytes = talloc_array(state, uint8_t, ulen+hlen+2);
	if (bytes == NULL) {
		goto fail;
	}
	p = bytes;

	*p++ = 4;
	memcpy(p, utmp, ulen);
	p += ulen;
	*p++ = 4;
	memcpy(p, htmp, hlen);
	p += hlen;
	TALLOC_FREE(htmp);
	TALLOC_FREE(utmp);

	subreq = cli_smb_send(state, ev, cli, SMBsendstrt, 0, 0, NULL,
			      talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_message_start_done, req);
	return req;
fail:
	TALLOC_FREE(htmp);
	TALLOC_FREE(utmp);
	tevent_req_nterror(req, NT_STATUS_NO_MEMORY);
	return tevent_req_post(req, ev);
}

static void cli_message_start_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_message_start_state *state = tevent_req_data(
		req, struct cli_message_start_state);
	NTSTATUS status;
	uint8_t wct;
	uint16_t *vwv;
	uint8_t *inbuf;

	status = cli_smb_recv(subreq, state, &inbuf, 0, &wct, &vwv,
			      NULL, NULL);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		TALLOC_FREE(subreq);
		tevent_req_nterror(req, status);
		return;
	}
	if (wct >= 1) {
		state->grp = SVAL(vwv+0, 0);
	} else {
		state->grp = 0;
	}
	tevent_req_done(req);
}

static NTSTATUS cli_message_start_recv(struct tevent_req *req,
				       uint16_t *pgrp)
{
	struct cli_message_start_state *state = tevent_req_data(
		req, struct cli_message_start_state);
	NTSTATUS status;

	if (tevent_req_is_nterror(req, &status)) {
		return status;
	}
	*pgrp = state->grp;
	return NT_STATUS_OK;
}

struct cli_message_text_state {
	uint16_t vwv;
};

static void cli_message_text_done(struct tevent_req *subreq);

static struct tevent_req *cli_message_text_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct cli_state *cli,
						uint16_t grp,
						const char *msg,
						int msglen)
{
	struct tevent_req *req, *subreq;
	struct cli_message_text_state *state;
	char *tmp;
	size_t tmplen;
	uint8_t *bytes;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_message_text_state);
	if (req == NULL) {
		return NULL;
	}

	SSVAL(&state->vwv, 0, grp);

	if (convert_string_talloc(talloc_tos(), CH_UNIX, CH_DOS, msg, msglen,
				  &tmp, &tmplen, true)) {
		msg = tmp;
		msglen = tmplen;
	} else {
		DEBUG(3, ("Conversion failed, sending message in UNIX "
			  "charset\n"));
		tmp = NULL;
	}

	bytes = talloc_array(state, uint8_t, msglen+3);
	if (tevent_req_nomem(bytes, req)) {
		TALLOC_FREE(tmp);
		return tevent_req_post(req, ev);
	}
	SCVAL(bytes, 0, 1);	/* pad */
	SSVAL(bytes+1, 0, msglen);
	memcpy(bytes+3, msg, msglen);
	TALLOC_FREE(tmp);

	subreq = cli_smb_send(state, ev, cli, SMBsendtxt, 0, 1, &state->vwv,
			      talloc_get_size(bytes), bytes);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_message_text_done, req);
	return req;
}

static void cli_message_text_done(struct tevent_req *subreq)
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

static NTSTATUS cli_message_text_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

struct cli_message_end_state {
	uint16_t vwv;
};

static void cli_message_end_done(struct tevent_req *subreq);

static struct tevent_req *cli_message_end_send(TALLOC_CTX *mem_ctx,
						struct tevent_context *ev,
						struct cli_state *cli,
						uint16_t grp)
{
	struct tevent_req *req, *subreq;
	struct cli_message_end_state *state;

	req = tevent_req_create(mem_ctx, &state,
				struct cli_message_end_state);
	if (req == NULL) {
		return NULL;
	}

	SSVAL(&state->vwv, 0, grp);

	subreq = cli_smb_send(state, ev, cli, SMBsendend, 0, 1, &state->vwv,
			      0, NULL);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_message_end_done, req);
	return req;
}

static void cli_message_end_done(struct tevent_req *subreq)
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

static NTSTATUS cli_message_end_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

struct cli_message_state {
	struct tevent_context *ev;
	struct cli_state *cli;
	size_t sent;
	const char *message;
	uint16_t grp;
};

static void cli_message_started(struct tevent_req *subreq);
static void cli_message_sent(struct tevent_req *subreq);
static void cli_message_done(struct tevent_req *subreq);

struct tevent_req *cli_message_send(TALLOC_CTX *mem_ctx,
				    struct tevent_context *ev,
				    struct cli_state *cli,
				    const char *host, const char *username,
				    const char *message)
{
	struct tevent_req *req, *subreq;
	struct cli_message_state *state;

	req = tevent_req_create(mem_ctx, &state, struct cli_message_state);
	if (req == NULL) {
		return NULL;
	}
	state->ev = ev;
	state->cli = cli;
	state->sent = 0;
	state->message = message;

	subreq = cli_message_start_send(state, ev, cli, host, username);
	if (tevent_req_nomem(subreq, req)) {
		return tevent_req_post(req, ev);
	}
	tevent_req_set_callback(subreq, cli_message_started, req);
	return req;
}

static void cli_message_started(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_message_state *state = tevent_req_data(
		req, struct cli_message_state);
	NTSTATUS status;
	size_t thistime;

	status = cli_message_start_recv(subreq, &state->grp);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	thistime = MIN(127, strlen(state->message));

	subreq = cli_message_text_send(state, state->ev, state->cli,
				       state->grp, state->message, thistime);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	state->sent += thistime;
	tevent_req_set_callback(subreq, cli_message_sent, req);
}

static void cli_message_sent(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	struct cli_message_state *state = tevent_req_data(
		req, struct cli_message_state);
	NTSTATUS status;
	size_t left, thistime;

	status = cli_message_text_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}

	if (state->sent >= strlen(state->message)) {
		subreq = cli_message_end_send(state, state->ev, state->cli,
					      state->grp);
		if (tevent_req_nomem(subreq, req)) {
			return;
		}
		tevent_req_set_callback(subreq, cli_message_done, req);
		return;
	}

	left = strlen(state->message) - state->sent;
	thistime = MIN(127, left);

	subreq = cli_message_text_send(state, state->ev, state->cli,
				       state->grp,
				       state->message + state->sent,
				       thistime);
	if (tevent_req_nomem(subreq, req)) {
		return;
	}
	state->sent += thistime;
	tevent_req_set_callback(subreq, cli_message_sent, req);
}

static void cli_message_done(struct tevent_req *subreq)
{
	struct tevent_req *req = tevent_req_callback_data(
		subreq, struct tevent_req);
	NTSTATUS status;

	status = cli_message_end_recv(subreq);
	TALLOC_FREE(subreq);
	if (!NT_STATUS_IS_OK(status)) {
		tevent_req_nterror(req, status);
		return;
	}
	tevent_req_done(req);
}

NTSTATUS cli_message_recv(struct tevent_req *req)
{
	return tevent_req_simple_recv_ntstatus(req);
}

NTSTATUS cli_message(struct cli_state *cli, const char *host,
		     const char *username, const char *message)
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

	req = cli_message_send(frame, ev, cli, host, username, message);
	if (req == NULL) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	if (!tevent_req_poll(req, ev)) {
		status = map_nt_error_from_unix(errno);
		goto fail;
	}

	status = cli_message_recv(req);
 fail:
	TALLOC_FREE(frame);
	return status;
}
