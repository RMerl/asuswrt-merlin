/* 
   Unix SMB/CIFS implementation.

   NBT server structures

   Copyright (C) Andrew Tridgell	2005
   
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

#include "../libcli/nbt/libnbt.h"
#include "libcli/wrepl/winsrepl.h"
#include "libcli/dgram/libdgram.h"
#include "librpc/gen_ndr/irpc.h"
#include "lib/messaging/irpc.h"

/* 
   a list of our registered names on each interface
*/
struct nbtd_iface_name {
	struct nbtd_iface_name *next, *prev;
	struct nbtd_interface *iface;
	struct nbt_name name;
	uint16_t nb_flags;
	struct timeval registration_time;
	uint32_t ttl;

	/* if registered with a wins server, then this lists the server being
	   used */
	const char *wins_server;
};

struct nbtd_wins_wack_state;

/* a list of network interfaces we are listening on */
struct nbtd_interface {
	struct nbtd_interface *next, *prev;
	struct nbtd_server *nbtsrv;
	const char *ip_address;
	const char *bcast_address;
	const char *netmask;
	struct nbt_name_socket *nbtsock;
	struct nbt_dgram_socket *dgmsock;
	struct nbtd_iface_name *names;
	struct nbtd_wins_wack_state *wack_queue;
};


/*
  top level context structure for the nbt server
*/
struct nbtd_server {
	struct task_server *task;

	/* the list of local network interfaces */
	struct nbtd_interface *interfaces;

	/* broadcast interface used for receiving packets only */
	struct nbtd_interface *bcast_interface;

	/* wins client interface - used for registering and refreshing
	   our names with a WINS server */
	struct nbtd_interface *wins_interface;

	struct wins_server *winssrv;

	struct nbtd_statistics stats;

	struct ldb_context *sam_ctx;
};



/* check a condition on an incoming packet */
#define NBTD_ASSERT_PACKET(packet, src, test) do { \
	if (!(test)) { \
		nbtd_bad_packet(packet, src, #test); \
		return; \
	} \
} while (0)

struct interface;
#include "nbt_server/nbt_server_proto.h"
