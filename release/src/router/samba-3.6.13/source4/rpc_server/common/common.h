/* 
   Unix SMB/CIFS implementation.

   common macros for the dcerpc server interfaces

   Copyright (C) Stefan (metze) Metzmacher 2004
   Copyright (C) Andrew Tridgell 2004
   
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

#ifndef _DCERPC_SERVER_COMMON_H_
#define _DCERPC_SERVER_COMMON_H_

struct share_config;
struct dcesrv_connection;
struct dcesrv_context;
struct dcesrv_context;
struct dcesrv_call_state;
struct ndr_interface_table;
struct ncacn_packet;

struct dcerpc_server_info { 
	const char *domain_name;
	uint32_t version_major;
	uint32_t version_minor;
	uint32_t version_build;
};

#include "rpc_server/common/proto.h"

#endif /* _DCERPC_SERVER_COMMON_H_ */
