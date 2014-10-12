/*
 *  RPC Server helper headers
 *  Almost completely rewritten by (C) Jeremy Allison 2005 - 2010
 *  Copyright (C) Simo Sorce <idra@samba.org> - 2010
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

#ifndef _RPC_SERVER_H_
#define _RPC_SERVER_H_

struct pipes_struct;

typedef bool (*dcerpc_ncacn_disconnect_fn)(struct pipes_struct *p);

void set_incoming_fault(struct pipes_struct *p);
void process_complete_pdu(struct pipes_struct *p);
bool setup_named_pipe_socket(const char *pipe_name,
			     struct tevent_context *ev_ctx);

uint16_t setup_dcerpc_ncacn_tcpip_socket(struct tevent_context *ev_ctx,
					 struct messaging_context *msg_ctx,
					 struct ndr_syntax_id syntax_id,
					 const struct sockaddr_storage *ifss,
					 uint16_t port);

bool setup_dcerpc_ncalrpc_socket(struct tevent_context *ev_ctx,
				 struct messaging_context *msg_ctx,
				 struct ndr_syntax_id syntax_id,
				 const char *name,
				 dcerpc_ncacn_disconnect_fn fn);

#endif /* _PRC_SERVER_H_ */
