/* 
   Unix SMB/CIFS implementation.
   NBT netbios routines and daemon - version 2
   Copyright (C) Andrew Tridgell 1994-1998
   Copyright (C) Luke Kenneth Casson Leighton 1994-1998
   Copyright (C) Jeremy Allison 1994-1998
   
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
   
   Revision History:

*/

#include "includes.h"
#include "nmbd/nmbd.h"

extern int global_nmb_port;

/* This is the broadcast subnets database. */
struct subnet_record *subnetlist = NULL;

/* Extra subnets - keep these separate so enumeration code doesn't
   run onto it by mistake. */

struct subnet_record *unicast_subnet = NULL;
struct subnet_record *remote_broadcast_subnet = NULL;
struct subnet_record *wins_server_subnet = NULL;

extern uint16 samba_nb_type; /* Samba's NetBIOS name type. */

/****************************************************************************
  Add a subnet into the list.
  **************************************************************************/

static void add_subnet(struct subnet_record *subrec)
{
	DLIST_ADD(subnetlist, subrec);
}

/****************************************************************************
stop listening on a subnet
we don't free the record as we don't have proper reference counting for it
yet and it may be in use by a response record
  ****************************************************************************/

void close_subnet(struct subnet_record *subrec)
{
	if (subrec->nmb_sock != -1) {
		close(subrec->nmb_sock);
		subrec->nmb_sock = -1;
	}
	if (subrec->nmb_bcast != -1) {
		close(subrec->nmb_bcast);
		subrec->nmb_bcast = -1;
	}
	if (subrec->dgram_sock != -1) {
		close(subrec->dgram_sock);
		subrec->dgram_sock = -1;
	}
	if (subrec->dgram_bcast != -1) {
		close(subrec->dgram_bcast);
		subrec->dgram_bcast = -1;
	}

	DLIST_REMOVE(subnetlist, subrec);
}

/****************************************************************************
  Create a subnet entry.
  ****************************************************************************/

static struct subnet_record *make_subnet(const char *name, enum subnet_type type,
					 struct in_addr myip, struct in_addr bcast_ip, 
					 struct in_addr mask_ip)
{
	struct subnet_record *subrec = NULL;
	int nmb_sock = -1;
	int dgram_sock = -1;
	int nmb_bcast = -1;
	int dgram_bcast = -1;
	bool bind_bcast = lp_nmbd_bind_explicit_broadcast();

	/* Check if we are creating a non broadcast subnet - if so don't create
		sockets.  */

	if (type == NORMAL_SUBNET) {
		struct sockaddr_storage ss;
		struct sockaddr_storage ss_bcast;

		in_addr_to_sockaddr_storage(&ss, myip);
		in_addr_to_sockaddr_storage(&ss_bcast, bcast_ip);

		/*
		 * Attempt to open the sockets on port 137/138 for this interface
		 * and bind them.
		 * Fail the subnet creation if this fails.
		 */

		nmb_sock = open_socket_in(SOCK_DGRAM, global_nmb_port,
					  0, &ss, true);
		if (nmb_sock == -1) {
			DEBUG(0,   ("nmbd_subnetdb:make_subnet()\n"));
			DEBUGADD(0,("  Failed to open nmb socket on interface %s ",
				    inet_ntoa(myip)));
			DEBUGADD(0,("for port %d.  ", global_nmb_port));
			DEBUGADD(0,("Error was %s\n", strerror(errno)));
			goto failed;
		}
		set_socket_options(nmb_sock,"SO_BROADCAST");
		set_blocking(nmb_sock, false);

		if (bind_bcast) {
			nmb_bcast = open_socket_in(SOCK_DGRAM, global_nmb_port,
						   0, &ss_bcast, true);
			if (nmb_bcast == -1) {
				DEBUG(0,   ("nmbd_subnetdb:make_subnet()\n"));
				DEBUGADD(0,("  Failed to open nmb bcast socket on interface %s ",
					    inet_ntoa(bcast_ip)));
				DEBUGADD(0,("for port %d.  ", global_nmb_port));
				DEBUGADD(0,("Error was %s\n", strerror(errno)));
				goto failed;
			}
			set_socket_options(nmb_bcast, "SO_BROADCAST");
			set_blocking(nmb_bcast, false);
		}

		dgram_sock = open_socket_in(SOCK_DGRAM, DGRAM_PORT,
					    3, &ss, true);
		if (dgram_sock == -1) {
			DEBUG(0,   ("nmbd_subnetdb:make_subnet()\n"));
			DEBUGADD(0,("  Failed to open dgram socket on interface %s ",
				    inet_ntoa(myip)));
			DEBUGADD(0,("for port %d.  ", DGRAM_PORT));
			DEBUGADD(0,("Error was %s\n", strerror(errno)));
			goto failed;
		}
		set_socket_options(dgram_sock, "SO_BROADCAST");
		set_blocking(dgram_sock, false);

		if (bind_bcast) {
			dgram_bcast = open_socket_in(SOCK_DGRAM, DGRAM_PORT,
						     3, &ss_bcast, true);
			if (dgram_bcast == -1) {
				DEBUG(0,   ("nmbd_subnetdb:make_subnet()\n"));
				DEBUGADD(0,("  Failed to open dgram bcast socket on interface %s ",
					    inet_ntoa(bcast_ip)));
				DEBUGADD(0,("for port %d.  ", DGRAM_PORT));
				DEBUGADD(0,("Error was %s\n", strerror(errno)));
				goto failed;
			}
			set_socket_options(dgram_bcast, "SO_BROADCAST");
			set_blocking(dgram_bcast, false);
		}
	}

	subrec = SMB_MALLOC_P(struct subnet_record);
	if (!subrec) {
		DEBUG(0,("make_subnet: malloc fail !\n"));
		goto failed;
	}
  
	ZERO_STRUCTP(subrec);

	if((subrec->subnet_name = SMB_STRDUP(name)) == NULL) {
		DEBUG(0,("make_subnet: malloc fail for subnet name !\n"));
		goto failed;
	}

	DEBUG(2, ("making subnet name:%s ", name ));
	DEBUG(2, ("Broadcast address:%s ", inet_ntoa(bcast_ip)));
	DEBUG(2, ("Subnet mask:%s\n", inet_ntoa(mask_ip)));
 
	subrec->namelist_changed = False;
	subrec->work_changed = False;
 
	subrec->bcast_ip = bcast_ip;
	subrec->mask_ip  = mask_ip;
	subrec->myip = myip;
	subrec->type = type;
	subrec->nmb_sock = nmb_sock;
	subrec->nmb_bcast = nmb_bcast;
	subrec->dgram_sock = dgram_sock;
	subrec->dgram_bcast = dgram_bcast;

	return subrec;

failed:
	SAFE_FREE(subrec);
	if (nmb_sock != -1) {
		close(nmb_sock);
	}
	if (nmb_bcast != -1) {
		close(nmb_bcast);
	}
	if (dgram_sock != -1) {
		close(dgram_sock);
	}
	if (dgram_bcast != -1) {
		close(dgram_bcast);
	}
	return NULL;
}

/****************************************************************************
  Create a normal subnet
**************************************************************************/

struct subnet_record *make_normal_subnet(const struct interface *iface)
{

	struct subnet_record *subrec;
	const struct in_addr *pip = &((const struct sockaddr_in *)&iface->ip)->sin_addr;
	const struct in_addr *pbcast = &((const struct sockaddr_in *)&iface->bcast)->sin_addr;
	const struct in_addr *pnmask = &((const struct sockaddr_in *)&iface->netmask)->sin_addr;

	subrec = make_subnet(inet_ntoa(*pip), NORMAL_SUBNET,
			     *pip, *pbcast, *pnmask);
	if (subrec) {
		add_subnet(subrec);
	}
	return subrec;
}

/****************************************************************************
  Create subnet entries.
**************************************************************************/

bool create_subnets(void)
{
	/* We only count IPv4 interfaces whilst we're waiting. */
	int num_interfaces;
	int i;
	struct in_addr unicast_ip, ipzero;

  try_interfaces_again:

	/* Only count IPv4, non-loopback interfaces. */
	if (iface_count_v4_nl() == 0) {
		DEBUG(0,("create_subnets: No local IPv4 non-loopback interfaces !\n"));
		DEBUG(0,("create_subnets: Waiting for an interface to appear ...\n"));
	}

	/* We only count IPv4, non-loopback interfaces here. */
	while (iface_count_v4_nl() == 0) {
		void (*saved_handler)(int);

		/*
		 * Whilst we're waiting for an interface, allow SIGTERM to
		 * cause us to exit.
		 */

		saved_handler = CatchSignal(SIGTERM, SIG_DFL);

		sleep(5);
		load_interfaces();

		/*
		 * We got an interface, restore our normal term handler.
		 */

		CatchSignal(SIGTERM, saved_handler);
	}

	/*
	 * Here we count v4 and v6 - we know there's at least one
	 * IPv4 interface and we filter on it below.
	 */
	num_interfaces = iface_count();

	/*
	 * Create subnets from all the local interfaces and thread them onto
	 * the linked list.
	 */

	for (i = 0 ; i < num_interfaces; i++) {
		const struct interface *iface = get_interface(i);

		if (!iface) {
			DEBUG(2,("create_subnets: can't get interface %d.\n", i ));
			continue;
		}

		/* Ensure we're only dealing with IPv4 here. */
		if (iface->ip.ss_family != AF_INET) {
			DEBUG(2,("create_subnets: "
				"ignoring non IPv4 interface.\n"));
			continue;
		}

		/*
		 * We don't want to add a loopback interface, in case
		 * someone has added 127.0.0.1 for smbd, nmbd needs to
		 * ignore it here. JRA.
		 */

		if (is_loopback_addr((struct sockaddr *)&iface->ip)) {
			DEBUG(2,("create_subnets: Ignoring loopback interface.\n" ));
			continue;
		}

		if (!make_normal_subnet(iface))
			return False;
	}

        /* We must have at least one subnet. */
	if (subnetlist == NULL) {
		void (*saved_handler)(int);

		DEBUG(0,("create_subnets: Unable to create any subnet from "
				"given interfaces. Is your interface line in "
				"smb.conf correct ?\n"));

		saved_handler = CatchSignal(SIGTERM, SIG_DFL);

		sleep(5);
		load_interfaces();

		CatchSignal(SIGTERM, saved_handler);
		goto try_interfaces_again;
	}

	if (lp_we_are_a_wins_server()) {
		/* Pick the first interface IPv4 address as the WINS server
		 * ip. */
		const struct in_addr *nip = first_ipv4_iface();

		if (!nip) {
			return False;
		}

		unicast_ip = *nip;
	} else {
		/* note that we do not set the wins server IP here. We just
			set it at zero and let the wins registration code cope
			with getting the IPs right for each packet */
		zero_ip_v4(&unicast_ip);
	}

	/*
	 * Create the unicast and remote broadcast subnets.
	 * Don't put these onto the linked list.
	 * The ip address of the unicast subnet is set to be
	 * the WINS server address, if it exists, or ipzero if not.
	 */

	unicast_subnet = make_subnet( "UNICAST_SUBNET", UNICAST_SUBNET, 
				unicast_ip, unicast_ip, unicast_ip);

	zero_ip_v4(&ipzero);

	remote_broadcast_subnet = make_subnet( "REMOTE_BROADCAST_SUBNET",
				REMOTE_BROADCAST_SUBNET,
				ipzero, ipzero, ipzero);

	if((unicast_subnet == NULL) || (remote_broadcast_subnet == NULL))
		return False;

	/* 
	 * If we are WINS server, create the WINS_SERVER_SUBNET - don't put on
	 * the linked list.
	 */

	if (lp_we_are_a_wins_server()) {
		if( (wins_server_subnet = make_subnet( "WINS_SERVER_SUBNET",
						WINS_SERVER_SUBNET, 
						ipzero, ipzero, ipzero )) == NULL )
			return False;
	}

	return True;
}

/*******************************************************************
Function to tell us if we can use the unicast subnet.
******************************************************************/

bool we_are_a_wins_client(void)
{
	if (wins_srv_count() > 0) {
		return True;
	}

	return False;
}

/*******************************************************************
Access function used by NEXT_SUBNET_INCLUDING_UNICAST
******************************************************************/

struct subnet_record *get_next_subnet_maybe_unicast(struct subnet_record *subrec)
{
	if(subrec == unicast_subnet)
		return NULL;
	else if((subrec->next == NULL) && we_are_a_wins_client())
		return unicast_subnet;
	else
		return subrec->next;
}

/*******************************************************************
 Access function used by retransmit_or_expire_response_records() in
 nmbd_packets.c. Patch from Andrey Alekseyev <fetch@muffin.arcadia.spb.ru>
 Needed when we need to enumerate all the broadcast, unicast and
 WINS subnets.
******************************************************************/

struct subnet_record *get_next_subnet_maybe_unicast_or_wins_server(struct subnet_record *subrec)
{
	if(subrec == unicast_subnet) {
		if(wins_server_subnet)
			return wins_server_subnet;
		else
			return NULL;
	}

	if(wins_server_subnet && subrec == wins_server_subnet)
		return NULL;

	if((subrec->next == NULL) && we_are_a_wins_client())
		return unicast_subnet;
	else
		return subrec->next;
}
