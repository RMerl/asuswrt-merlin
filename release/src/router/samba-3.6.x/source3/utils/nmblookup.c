/*
   Unix SMB/CIFS implementation.
   NBT client - used to lookup netbios names
   Copyright (C) Andrew Tridgell 1994-1998
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
#include "popt_common.h"
#include "libsmb/nmblib.h"

static bool give_flags = false;
static bool use_bcast = true;
static bool got_bcast = false;
static struct sockaddr_storage bcast_addr;
static bool recursion_desired = false;
static bool translate_addresses = false;
static int ServerFD= -1;
static bool RootPort = false;
static bool find_status = false;

/****************************************************************************
 Open the socket communication.
**************************************************************************/

static bool open_sockets(void)
{
	struct sockaddr_storage ss;
	const char *sock_addr = lp_socket_address();

	if (!interpret_string_addr(&ss, sock_addr,
				AI_NUMERICHOST|AI_PASSIVE)) {
		DEBUG(0,("open_sockets: unable to get socket address "
					"from string %s", sock_addr));
		return false;
	}
	ServerFD = open_socket_in( SOCK_DGRAM,
				(RootPort ? 137 : 0),
				(RootPort ?   0 : 3),
				&ss, true );

	if (ServerFD == -1) {
		return false;
	}

	set_socket_options( ServerFD, "SO_BROADCAST" );

	DEBUG(3, ("Socket opened.\n"));
	return true;
}

/****************************************************************************
turn a node status flags field into a string
****************************************************************************/
static char *node_status_flags(unsigned char flags)
{
	static fstring ret;
	fstrcpy(ret,"");

	fstrcat(ret, (flags & 0x80) ? "<GROUP> " : "        ");
	if ((flags & 0x60) == 0x00) fstrcat(ret,"B ");
	if ((flags & 0x60) == 0x20) fstrcat(ret,"P ");
	if ((flags & 0x60) == 0x40) fstrcat(ret,"M ");
	if ((flags & 0x60) == 0x60) fstrcat(ret,"H ");
	if (flags & 0x10) fstrcat(ret,"<DEREGISTERING> ");
	if (flags & 0x08) fstrcat(ret,"<CONFLICT> ");
	if (flags & 0x04) fstrcat(ret,"<ACTIVE> ");
	if (flags & 0x02) fstrcat(ret,"<PERMANENT> ");

	return ret;
}

/****************************************************************************
 Turn the NMB Query flags into a string.
****************************************************************************/

static char *query_flags(int flags)
{
	static fstring ret1;
	fstrcpy(ret1, "");

	if (flags & NM_FLAGS_RS) fstrcat(ret1, "Response ");
	if (flags & NM_FLAGS_AA) fstrcat(ret1, "Authoritative ");
	if (flags & NM_FLAGS_TC) fstrcat(ret1, "Truncated ");
	if (flags & NM_FLAGS_RD) fstrcat(ret1, "Recursion_Desired ");
	if (flags & NM_FLAGS_RA) fstrcat(ret1, "Recursion_Available ");
	if (flags & NM_FLAGS_B)  fstrcat(ret1, "Broadcast ");

	return ret1;
}

/****************************************************************************
 Do a node status query.
****************************************************************************/

static void do_node_status(const char *name,
		int type,
		struct sockaddr_storage *pss)
{
	struct nmb_name nname;
	int count, i, j;
	struct node_status *addrs;
	struct node_status_extra extra;
	fstring cleanname;
	char addr[INET6_ADDRSTRLEN];
	NTSTATUS status;

	print_sockaddr(addr, sizeof(addr), pss);
	d_printf("Looking up status of %s\n",addr);
	make_nmb_name(&nname, name, type);
	status = node_status_query(talloc_tos(), &nname, pss,
				   &addrs, &count, &extra);
	if (NT_STATUS_IS_OK(status)) {
		for (i=0;i<count;i++) {
			pull_ascii_fstring(cleanname, addrs[i].name);
			for (j=0;cleanname[j];j++) {
				if (!isprint((int)cleanname[j])) {
					cleanname[j] = '.';
				}
			}
			d_printf("\t%-15s <%02x> - %s\n",
			       cleanname,addrs[i].type,
			       node_status_flags(addrs[i].flags));
		}
		d_printf("\n\tMAC Address = %02X-%02X-%02X-%02X-%02X-%02X\n",
				extra.mac_addr[0], extra.mac_addr[1],
				extra.mac_addr[2], extra.mac_addr[3],
				extra.mac_addr[4], extra.mac_addr[5]);
		d_printf("\n");
		TALLOC_FREE(addrs);
	} else {
		d_printf("No reply from %s\n\n",addr);
	}
}


/****************************************************************************
 Send out one query.
****************************************************************************/

static bool query_one(const char *lookup, unsigned int lookup_type)
{
	int j, count;
	uint8_t flags;
	struct sockaddr_storage *ip_list=NULL;
	NTSTATUS status = NT_STATUS_NOT_FOUND;

	if (got_bcast) {
		char addr[INET6_ADDRSTRLEN];
		print_sockaddr(addr, sizeof(addr), &bcast_addr);
		d_printf("querying %s on %s\n", lookup, addr);
		status = name_query(lookup,lookup_type,use_bcast,
				    use_bcast?true:recursion_desired,
				    &bcast_addr, talloc_tos(),
				    &ip_list, &count, &flags);
	} else {
		const struct in_addr *bcast;
		for (j=iface_count() - 1;
		     !ip_list && j >= 0;
		     j--) {
			char addr[INET6_ADDRSTRLEN];
			struct sockaddr_storage bcast_ss;

			bcast = iface_n_bcast_v4(j);
			if (!bcast) {
				continue;
			}
			in_addr_to_sockaddr_storage(&bcast_ss, *bcast);
			print_sockaddr(addr, sizeof(addr), &bcast_ss);
			d_printf("querying %s on %s\n",
			       lookup, addr);
			status = name_query(lookup,lookup_type,
					    use_bcast,
					    use_bcast?True:recursion_desired,
					    &bcast_ss, talloc_tos(),
					    &ip_list, &count, &flags);
		}
	}

	if (!NT_STATUS_IS_OK(status)) {
		return false;
	}

	if (give_flags) {
		d_printf("Flags: %s\n", query_flags(flags));
	}

	for (j=0;j<count;j++) {
		char addr[INET6_ADDRSTRLEN];
		if (translate_addresses) {
			char h_name[MAX_DNS_NAME_LENGTH];
			h_name[0] = '\0';
			if (sys_getnameinfo((const struct sockaddr *)&ip_list[j],
					sizeof(struct sockaddr_storage),
					h_name, sizeof(h_name),
					NULL, 0,
					NI_NAMEREQD)) {
				continue;
			}
			d_printf("%s, ", h_name);
		}
		print_sockaddr(addr, sizeof(addr), &ip_list[j]);
		d_printf("%s %s<%02x>\n", addr,lookup, lookup_type);
		/* We can only do find_status if the ip address returned
		   was valid - ie. name_query returned true.
		 */
		if (find_status) {
			do_node_status(lookup, lookup_type, &ip_list[j]);
		}
	}

	TALLOC_FREE(ip_list);

	return NT_STATUS_IS_OK(status);
}


/****************************************************************************
  main program
****************************************************************************/
int main(int argc,char *argv[])
{
	int opt;
	unsigned int lookup_type = 0x0;
	fstring lookup;
	static bool find_master=False;
	static bool lookup_by_ip = False;
	poptContext pc;
	TALLOC_CTX *frame = talloc_stackframe();

	struct poptOption long_options[] = {
		POPT_AUTOHELP
		{ "broadcast", 'B', POPT_ARG_STRING, NULL, 'B', "Specify address to use for broadcasts", "BROADCAST-ADDRESS" },
		{ "flags", 'f', POPT_ARG_NONE, NULL, 'f', "List the NMB flags returned" },
		{ "unicast", 'U', POPT_ARG_STRING, NULL, 'U', "Specify address to use for unicast" },
		{ "master-browser", 'M', POPT_ARG_NONE, NULL, 'M', "Search for a master browser" },
		{ "recursion", 'R', POPT_ARG_NONE, NULL, 'R', "Set recursion desired in package" },
		{ "status", 'S', POPT_ARG_NONE, NULL, 'S', "Lookup node status as well" },
		{ "translate", 'T', POPT_ARG_NONE, NULL, 'T', "Translate IP addresses into names" },
		{ "root-port", 'r', POPT_ARG_NONE, NULL, 'r', "Use root port 137 (Win95 only replies to this)" },
		{ "lookup-by-ip", 'A', POPT_ARG_NONE, NULL, 'A', "Do a node status on <name> as an IP Address" },
		POPT_COMMON_SAMBA
		POPT_COMMON_CONNECTION
		{ 0, 0, 0, 0 }
	};

	*lookup = 0;

	load_case_tables();

	setup_logging(argv[0], DEBUG_STDOUT);

	pc = poptGetContext("nmblookup", argc, (const char **)argv,
			long_options, POPT_CONTEXT_KEEP_FIRST);

	poptSetOtherOptionHelp(pc, "<NODE> ...");

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case 'f':
			give_flags = true;
			break;
		case 'M':
			find_master = true;
			break;
		case 'R':
			recursion_desired = true;
			break;
		case 'S':
			find_status = true;
			break;
		case 'r':
			RootPort = true;
			break;
		case 'A':
			lookup_by_ip = true;
			break;
		case 'B':
			if (interpret_string_addr(&bcast_addr,
					poptGetOptArg(pc),
					NI_NUMERICHOST)) {
				got_bcast = True;
				use_bcast = True;
			}
			break;
		case 'U':
			if (interpret_string_addr(&bcast_addr,
					poptGetOptArg(pc),
					0)) {
				got_bcast = True;
				use_bcast = False;
			}
			break;
		case 'T':
			translate_addresses = !translate_addresses;
			break;
		}
	}

	poptGetArg(pc); /* Remove argv[0] */

	if(!poptPeekArg(pc)) {
		poptPrintUsage(pc, stderr, 0);
		exit(1);
	}

	if (!lp_load(get_dyn_CONFIGFILE(),True,False,False,True)) {
		fprintf(stderr, "Can't load %s - run testparm to debug it\n",
				get_dyn_CONFIGFILE());
	}

	load_interfaces();
	if (!open_sockets()) {
		return(1);
	}

	while(poptPeekArg(pc)) {
		char *p;
		struct in_addr ip;

		fstrcpy(lookup,poptGetArg(pc));

		if(lookup_by_ip) {
			struct sockaddr_storage ss;
			ip = interpret_addr2(lookup);
			in_addr_to_sockaddr_storage(&ss, ip);
			fstrcpy(lookup,"*");
			do_node_status(lookup, lookup_type, &ss);
			continue;
		}

		if (find_master) {
			if (*lookup == '-') {
				fstrcpy(lookup,"\01\02__MSBROWSE__\02");
				lookup_type = 1;
			} else {
				lookup_type = 0x1d;
			}
		}

		p = strchr_m(lookup,'#');
		if (p) {
			*p = '\0';
			sscanf(++p,"%x",&lookup_type);
		}

		if (!query_one(lookup, lookup_type)) {
			d_printf( "name_query failed to find name %s", lookup );
			if( 0 != lookup_type ) {
				d_printf( "#%02x", lookup_type );
			}
			d_printf( "\n" );
		}
	}

	poptFreeContext(pc);
	TALLOC_FREE(frame);
	return(0);
}
