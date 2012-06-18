/* 
   Unix SMB/CIFS implementation.

   NBT client - used to lookup netbios names

   Copyright (C) Andrew Tridgell 1994-2005
   Copyright (C) Jelmer Vernooij 2003 (Conversion to popt)
   
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
#include "lib/cmdline/popt_common.h"
#include "lib/socket/socket.h"
#include "lib/events/events.h"
#include "system/network.h"
#include "system/locale.h"
#include "lib/socket/netif.h"
#include "librpc/gen_ndr/nbt.h"
#include "../libcli/nbt/libnbt.h"
#include "param/param.h"

/* command line options */
static struct {
	const char *broadcast_address;
	const char *unicast_address;
	bool find_master;
	bool wins_lookup;
	bool node_status;
	bool root_port;
	bool lookup_by_ip;
	bool case_sensitive;
} options;

/*
  clean any binary from a node name
*/
static const char *clean_name(TALLOC_CTX *mem_ctx, const char *name)
{
	char *ret = talloc_strdup(mem_ctx, name);
	int i;
	for (i=0;ret[i];i++) {
		if (!isprint((unsigned char)ret[i])) ret[i] = '.';
	}
	return ret;
}

/*
  turn a node status flags field into a string
*/
static char *node_status_flags(TALLOC_CTX *mem_ctx, uint16_t flags)
{
	char *ret;
	const char *group = "       ";
	const char *type = "B";

	if (flags & NBT_NM_GROUP) {
		group = "<GROUP>";
	}

	switch (flags & NBT_NM_OWNER_TYPE) {
	case NBT_NODE_B: 
		type = "B";
		break;
	case NBT_NODE_P: 
		type = "P";
		break;
	case NBT_NODE_M: 
		type = "M";
		break;
	case NBT_NODE_H: 
		type = "H";
		break;
	}

	ret = talloc_asprintf(mem_ctx, "%s %s", group, type);

	if (flags & NBT_NM_DEREGISTER) {
		ret = talloc_asprintf_append_buffer(ret, " <DEREGISTERING>");
	}
	if (flags & NBT_NM_CONFLICT) {
		ret = talloc_asprintf_append_buffer(ret, " <CONFLICT>");
	}
	if (flags & NBT_NM_ACTIVE) {
		ret = talloc_asprintf_append_buffer(ret, " <ACTIVE>");
	}
	if (flags & NBT_NM_PERMANENT) {
		ret = talloc_asprintf_append_buffer(ret, " <PERMANENT>");
	}
	
	return ret;
}

/* do a single node status */
static bool do_node_status(struct nbt_name_socket *nbtsock,
			   const char *addr, uint16_t port)
{
	struct nbt_name_status io;
	NTSTATUS status;

	io.in.name.name = "*";
	io.in.name.type = NBT_NAME_CLIENT;
	io.in.name.scope = NULL;
	io.in.dest_addr = addr;
	io.in.dest_port = port;
	io.in.timeout = 1;
	io.in.retries = 2;

	status = nbt_name_status(nbtsock, nbtsock, &io);
	if (NT_STATUS_IS_OK(status)) {
		int i;
		printf("Node status reply from %s\n",
		       io.out.reply_from);
		for (i=0;i<io.out.status.num_names;i++) {
			d_printf("\t%-16s <%02x>  %s\n", 
				 clean_name(nbtsock, io.out.status.names[i].name),
				 io.out.status.names[i].type,
				 node_status_flags(nbtsock, io.out.status.names[i].nb_flags));
		}
		printf("\n\tMAC Address = %02X-%02X-%02X-%02X-%02X-%02X\n",
		       io.out.status.statistics.unit_id[0],
		       io.out.status.statistics.unit_id[1],
		       io.out.status.statistics.unit_id[2],
		       io.out.status.statistics.unit_id[3],
		       io.out.status.statistics.unit_id[4],
		       io.out.status.statistics.unit_id[5]);
		return true;
	}

	return false;
}

/* do a single node query */
static NTSTATUS do_node_query(struct nbt_name_socket *nbtsock,
			      const char *addr, 
			      uint16_t port,
			      const char *node_name, 
			      enum nbt_name_type node_type,
			      bool broadcast)
{
	struct nbt_name_query io;
	NTSTATUS status;
	int i;

	io.in.name.name = node_name;
	io.in.name.type = node_type;
	io.in.name.scope = NULL;
	io.in.dest_addr = addr;
	io.in.dest_port = port;
	io.in.broadcast = broadcast;
	io.in.wins_lookup = options.wins_lookup;
	io.in.timeout = 1;
	io.in.retries = 2;

	status = nbt_name_query(nbtsock, nbtsock, &io);
	NT_STATUS_NOT_OK_RETURN(status);

	for (i=0;i<io.out.num_addrs;i++) {
		printf("%s %s<%02x>\n",
		       io.out.reply_addrs[i],
		       io.out.name.name,
		       io.out.name.type);
	}
	if (options.node_status && io.out.num_addrs > 0) {
		do_node_status(nbtsock, io.out.reply_addrs[0], port);
	}

	return status;
}


static bool process_one(struct loadparm_context *lp_ctx, struct tevent_context *ev,
			struct interface *ifaces, const char *name, int nbt_port)
{
	TALLOC_CTX *tmp_ctx = talloc_new(NULL);
	enum nbt_name_type node_type = NBT_NAME_CLIENT;
	char *node_name, *p;
	struct socket_address *all_zero_addr;
	struct nbt_name_socket *nbtsock;
	NTSTATUS status = NT_STATUS_OK;
	bool ret = true;

	if (!options.case_sensitive) {
		name = strupper_talloc(tmp_ctx, name);
	}
	
	if (options.find_master) {
		node_type = NBT_NAME_MASTER;
		if (*name == '-' || *name == '_') {
			name = "\01\02__MSBROWSE__\02";
			node_type = NBT_NAME_MS;
		}
	}

	p = strchr(name, '#');
	if (p) {
		node_name = talloc_strndup(tmp_ctx, name, PTR_DIFF(p,name));
		node_type = (enum nbt_name_type)strtol(p+1, NULL, 16);
	} else {
		node_name = talloc_strdup(tmp_ctx, name);
	}

	nbtsock = nbt_name_socket_init(tmp_ctx, ev, lp_iconv_convenience(lp_ctx));
	
	if (options.root_port) {
		all_zero_addr = socket_address_from_strings(tmp_ctx, nbtsock->sock->backend_name, 
							    "0.0.0.0", NBT_NAME_SERVICE_PORT);
		
		if (!all_zero_addr) {
			talloc_free(tmp_ctx);
			return false;
		}

		status = socket_listen(nbtsock->sock, all_zero_addr, 0, 0);
		if (!NT_STATUS_IS_OK(status)) {
			printf("Failed to bind to local port 137 - %s\n", nt_errstr(status));
			talloc_free(tmp_ctx);
			return false;
		}
	}

	if (options.lookup_by_ip) {
		ret = do_node_status(nbtsock, name, nbt_port);
		talloc_free(tmp_ctx);
		return ret;
	}

	if (options.broadcast_address) {
		status = do_node_query(nbtsock, options.broadcast_address, nbt_port,
				       node_name, node_type, true);
	} else if (options.unicast_address) {
		status = do_node_query(nbtsock, options.unicast_address, 
				       nbt_port, node_name, node_type, false);
	} else {
		int i, num_interfaces;

		num_interfaces = iface_count(ifaces);
		for (i=0;i<num_interfaces;i++) {
			const char *bcast = iface_n_bcast(ifaces, i);
			if (bcast == NULL) continue;
			status = do_node_query(nbtsock, bcast, nbt_port, 
					       node_name, node_type, true);
			if (NT_STATUS_IS_OK(status)) break;
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		printf("Lookup failed - %s\n", nt_errstr(status));
		ret = false;
	}

	talloc_free(tmp_ctx);
	return ret;
}

/*
  main program
*/
int main(int argc, const char *argv[])
{
	bool ret = true;
	struct interface *ifaces;
	struct tevent_context *ev;
	poptContext pc;
	int opt;
	enum {
		OPT_BROADCAST_ADDRESS	= 1000,
		OPT_UNICAST_ADDRESS,
		OPT_FIND_MASTER,
		OPT_WINS_LOOKUP,
		OPT_NODE_STATUS,
		OPT_ROOT_PORT,
		OPT_LOOKUP_BY_IP,
		OPT_CASE_SENSITIVE
	};
	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "broadcast", 'B', POPT_ARG_STRING, NULL, OPT_BROADCAST_ADDRESS,
		  "Specify address to use for broadcasts", "BROADCAST-ADDRESS" },

		{ "unicast", 'U', POPT_ARG_STRING, NULL, OPT_UNICAST_ADDRESS,
		  "Specify address to use for unicast", NULL },

		{ "master-browser", 'M', POPT_ARG_NONE, NULL, OPT_FIND_MASTER,
		  "Search for a master browser", NULL },

		{ "wins", 'W', POPT_ARG_NONE, NULL, OPT_WINS_LOOKUP,
		  "Do a WINS lookup", NULL },

		{ "status", 'S', POPT_ARG_NONE, NULL, OPT_NODE_STATUS, 
		  "Lookup node status as well", NULL },

		{ "root-port", 'r', POPT_ARG_NONE, NULL, OPT_ROOT_PORT, 
		  "Use root port 137 (Win95 only replies to this)", NULL },

		{ "lookup-by-ip", 'A', POPT_ARG_NONE, NULL, OPT_LOOKUP_BY_IP, 
		  "Do a node status on <name> as an IP Address", NULL },

		{ "case-sensitive", 0, POPT_ARG_NONE, NULL, OPT_CASE_SENSITIVE, 
		  "Don't uppercase the name before sending", NULL },

		POPT_COMMON_SAMBA
		{ 0, 0, 0, 0 }
	};
	
	pc = poptGetContext("nmblookup", argc, argv, long_options, 
			    POPT_CONTEXT_KEEP_FIRST);

	poptSetOtherOptionHelp(pc, "<NODE> ...");

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch(opt) {
		case OPT_BROADCAST_ADDRESS:
			options.broadcast_address = poptGetOptArg(pc);
			break;
		case OPT_UNICAST_ADDRESS:
			options.unicast_address = poptGetOptArg(pc);
			break;
		case OPT_FIND_MASTER:
			options.find_master = true;
			break;
		case OPT_WINS_LOOKUP:
			options.wins_lookup = true;
			break;
		case OPT_NODE_STATUS:
			options.node_status = true;
			break;
		case OPT_ROOT_PORT:
			options.root_port = true;
			break;
		case OPT_LOOKUP_BY_IP:
			options.lookup_by_ip = true;
			break;
		case OPT_CASE_SENSITIVE:
			options.case_sensitive = true;
			break;
		}
	}

	/* swallow argv[0] */
	poptGetArg(pc);

	if(!poptPeekArg(pc)) { 
		poptPrintUsage(pc, stderr, 0);
		exit(1);
	}

	load_interfaces(NULL, lp_interfaces(cmdline_lp_ctx), &ifaces);

	ev = s4_event_context_init(talloc_autofree_context());

	while (poptPeekArg(pc)) {
		const char *name = poptGetArg(pc);

		ret &= process_one(cmdline_lp_ctx, ev, ifaces, name, lp_nbt_port(cmdline_lp_ctx));
	}

	talloc_free(ev);

	talloc_free(ifaces);

	poptFreeContext(pc);

	if (!ret) {
		return 1;
	}

	return 0;
}
