/*
   Unix SMB/CIFS implementation.
   Infrastructure for async SMB client requests
   Copyright (C) Volker Lendecke 2008

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

#ifndef __ASYNC_SMB_H__
#define __ASYNC_SMB_H__

struct cli_state;

/*
 * Fetch an error out of a NBT packet
 */

NTSTATUS cli_pull_error(char *buf);

/*
 * Compatibility helper for the sync APIs: Fake NTSTATUS in cli->inbuf
 */

void cli_set_error(struct cli_state *cli, NTSTATUS status);

struct tevent_req *cli_smb_req_create(TALLOC_CTX *mem_ctx,
				      struct event_context *ev,
				      struct cli_state *cli,
				      uint8_t smb_command,
				      uint8_t additional_flags,
				      uint8_t wct, uint16_t *vwv,
				      int iov_count,
				      struct iovec *bytes_iov);
NTSTATUS cli_smb_req_send(struct tevent_req *req);
size_t cli_smb_wct_ofs(struct tevent_req **reqs, int num_reqs);
NTSTATUS cli_smb_chain_send(struct tevent_req **reqs, int num_reqs);
uint8_t *cli_smb_inbuf(struct tevent_req *req);
bool cli_has_async_calls(struct cli_state *cli);
void cli_smb_req_unset_pending(struct tevent_req *req);
bool cli_smb_req_set_pending(struct tevent_req *req);
uint16_t cli_smb_req_mid(struct tevent_req *req);
void cli_smb_req_set_mid(struct tevent_req *req, uint16_t mid);
uint32_t cli_smb_req_seqnum(struct tevent_req *req);
void cli_smb_req_set_seqnum(struct tevent_req *req, uint32_t seqnum);
struct tevent_req *cli_smb_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				struct cli_state *cli,
				uint8_t smb_command, uint8_t additional_flags,
				uint8_t wct, uint16_t *vwv,
				uint32_t num_bytes,
				const uint8_t *bytes);
NTSTATUS cli_smb_recv(struct tevent_req *req,
		      TALLOC_CTX *mem_ctx, uint8_t **pinbuf,
		      uint8_t min_wct, uint8_t *pwct, uint16_t **pvwv,
		      uint32_t *pnum_bytes, uint8_t **pbytes);

struct tevent_req *cli_smb_oplock_break_waiter_send(TALLOC_CTX *mem_ctx,
						    struct event_context *ev,
						    struct cli_state *cli);
NTSTATUS cli_smb_oplock_break_waiter_recv(struct tevent_req *req,
					  uint16_t *pfnum,
					  uint8_t *plevel);

struct tevent_req *cli_session_request_send(TALLOC_CTX *mem_ctx,
					    struct tevent_context *ev,
					    int sock,
					    const struct nmb_name *called,
					    const struct nmb_name *calling);
bool cli_session_request_recv(struct tevent_req *req, int *err, uint8_t *resp);

#endif
