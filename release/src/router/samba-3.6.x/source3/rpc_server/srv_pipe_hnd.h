/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-1998,
 *  Largely re-written : 2005
 *  Copyright (C) Jeremy Allison		1998 - 2005
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RPC_SERVER_SRV_PIPE_HND_H_
#define _RPC_SERVER_SRV_PIPE_HND_H_

struct tsocket_address;
struct pipes_struct;

/* The following definitions come from rpc_server/srv_pipe_hnd.c  */

bool fsp_is_np(struct files_struct *fsp);
NTSTATUS np_open(TALLOC_CTX *mem_ctx, const char *name,
		 const struct tsocket_address *local_address,
		 const struct tsocket_address *remote_address,
		 struct client_address *client_id,
		 struct auth_serversupplied_info *session_info,
		 struct messaging_context *msg_ctx,
		 struct fake_file_handle **phandle);
bool np_read_in_progress(struct fake_file_handle *handle);
struct tevent_req *np_write_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				 struct fake_file_handle *handle,
				 const uint8_t *data, size_t len);
NTSTATUS np_write_recv(struct tevent_req *req, ssize_t *pnwritten);
struct tevent_req *np_read_send(TALLOC_CTX *mem_ctx, struct event_context *ev,
				struct fake_file_handle *handle,
				uint8_t *data, size_t len);
NTSTATUS np_read_recv(struct tevent_req *req, ssize_t *nread,
		      bool *is_data_outstanding);

ssize_t process_incoming_data(struct pipes_struct *p, char *data, size_t n);

#endif /* _RPC_SERVER_SRV_PIPE_HND_H_ */
