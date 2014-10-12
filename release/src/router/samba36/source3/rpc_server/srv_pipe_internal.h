/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Almost completely rewritten by (C) Jeremy Allison 2005 - 2010
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

#ifndef _RPC_PIPE_INTERNAL_H_
#define _RPC_PIPE_INTERNAL_H_

#include "includes.h"

bool rpc_srv_pipe_exists_by_id(const struct ndr_syntax_id *id);

bool rpc_srv_pipe_exists_by_cli_name(const char *cli_name);

bool rpc_srv_pipe_exists_by_srv_name(const char *srv_name);

const char *rpc_srv_get_pipe_cli_name(const struct ndr_syntax_id *id);

const char *rpc_srv_get_pipe_srv_name(const struct ndr_syntax_id *id);

uint32_t rpc_srv_get_pipe_num_cmds(const struct ndr_syntax_id *id);

const struct api_struct *rpc_srv_get_pipe_cmds(const struct ndr_syntax_id *id);

bool rpc_srv_get_pipe_interface_by_cli_name(const char *cli_name,
					    struct ndr_syntax_id *id);

#endif /* _RPC_PIPE_INTERNAL_H_ */
