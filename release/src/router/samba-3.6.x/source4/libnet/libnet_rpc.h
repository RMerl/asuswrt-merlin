/* 
   Unix SMB/CIFS implementation.
   
   Copyright (C) Stefan Metzmacher	2004
   
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


#include "librpc/rpc/dcerpc.h"

/*
 * struct definition for connecting to a dcerpc inferface
 */

enum libnet_RpcConnect_level {
	LIBNET_RPC_CONNECT_SERVER,          /* connect to a standalone rpc server */
	LIBNET_RPC_CONNECT_SERVER_ADDRESS,  /* connect to a standalone rpc server, 
					       knowing both name and address */
	LIBNET_RPC_CONNECT_PDC,             /* connect to a domain pdc (resolves domain
					       name to a pdc address before connecting) */
	LIBNET_RPC_CONNECT_DC,              /* connect to any DC (resolves domain
					       name to a DC address before connecting) */
	LIBNET_RPC_CONNECT_BINDING,         /* specified binding string */
	LIBNET_RPC_CONNECT_DC_INFO          /* connect to a DC and provide basic domain
					       information (name, realm, sid, guid) */
};

struct libnet_RpcConnect {
	enum libnet_RpcConnect_level level;

	struct {
		const char *name;
		const char *address;
		const char *binding;
		const struct ndr_interface_table *dcerpc_iface;
		int dcerpc_flags;
	} in;
	struct {
		struct dcerpc_pipe *dcerpc_pipe;
		
		/* parameters provided in LIBNET_RPC_CONNECT_DC_INFO level, null otherwise */
		const char *domain_name;
		struct dom_sid *domain_sid;
		const char *realm;           /* these parameters are only present if */
		struct GUID *guid;           /* the remote server is known to be AD */

		const char *error_string;
	} out;
};


/*
 * Monitor messages sent from libnet_rpc.c functions
 */

struct msg_net_rpc_connect {
	const char *host;
	const char *domain_name;
	const char *endpoint;
	enum dcerpc_transport_t transport;
};
