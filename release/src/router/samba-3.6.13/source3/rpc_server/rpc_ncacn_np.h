/*
   Unix SMB/Netbios implementation.
   RPC Server Headers
   Copyright (C) Simo Sorce 2010

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

#ifndef _RPC_NCACN_NP_H_
#define _RPC_NCACN_NP_H_

struct dcerpc_binding_handle;
struct ndr_interface_table;
struct tsocket_address;

struct np_proxy_state {
	uint16_t file_type;
	uint16_t device_state;
	uint64_t allocation_size;
	struct tstream_context *npipe;
	struct tevent_queue *read_queue;
	struct tevent_queue *write_queue;
};

struct pipes_struct *make_internal_rpc_pipe_p(TALLOC_CTX *mem_ctx,
					      const struct ndr_syntax_id *syntax,
					      struct client_address *client_id,
					      const struct auth_serversupplied_info *session_info,
					      struct messaging_context *msg_ctx);
struct np_proxy_state *make_external_rpc_pipe_p(TALLOC_CTX *mem_ctx,
				const char *pipe_name,
				const struct tsocket_address *local_address,
				const struct tsocket_address *remote_address,
				const struct auth_serversupplied_info *session_info);
NTSTATUS rpcint_binding_handle(TALLOC_CTX *mem_ctx,
			       const struct ndr_interface_table *ndr_table,
			       struct client_address *client_id,
			       const struct auth_serversupplied_info *session_info,
			       struct messaging_context *msg_ctx,
			       struct dcerpc_binding_handle **binding_handle);
NTSTATUS rpc_pipe_open_interface(TALLOC_CTX *mem_ctx,
				 const struct ndr_syntax_id *syntax,
				 const struct auth_serversupplied_info *session_info,
				 struct client_address *client_id,
				 struct messaging_context *msg_ctx,
				 struct rpc_pipe_client **cli_pipe);

struct pipes_struct *get_first_internal_pipe(void);
struct pipes_struct *get_next_internal_pipe(struct pipes_struct *p);
bool check_open_pipes(void);
int close_internal_rpc_pipe_hnd(struct pipes_struct *p);

#endif /* _RPC_NCACN_NP_H_ */
