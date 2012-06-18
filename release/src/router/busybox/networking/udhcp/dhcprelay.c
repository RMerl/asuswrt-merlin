/* vi: set sw=4 ts=4: */
/* Port to Busybox Copyright (C) 2006 Jesse Dutton <jessedutton@gmail.com>
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
 *
 * DHCP Relay for 'DHCPv4 Configuration of IPSec Tunnel Mode' support
 * Copyright (C) 2002 Mario Strasser <mast@gmx.net>,
 *                   Zuercher Hochschule Winterthur,
 *                   Netbeat AG
 * Upstream has GPL v2 or later
 */
#include "common.h"

#define SERVER_PORT      67
#define SELECT_TIMEOUT    5 /* select timeout in sec. */
#define MAX_LIFETIME   2*60 /* lifetime of an xid entry in sec. */

/* This list holds information about clients. The xid_* functions manipulate this list. */
struct xid_item {
	unsigned timestamp;
	int client;
	uint32_t xid;
	struct sockaddr_in ip;
	struct xid_item *next;
};

#define dhcprelay_xid_list (*(struct xid_item*)&bb_common_bufsiz1)

static struct xid_item *xid_add(uint32_t xid, struct sockaddr_in *ip, int client)
{
	struct xid_item *item;

	/* create new xid entry */
	item = xmalloc(sizeof(struct xid_item));

	/* add xid entry */
	item->ip = *ip;
	item->xid = xid;
	item->client = client;
	item->timestamp = monotonic_sec();
	item->next = dhcprelay_xid_list.next;
	dhcprelay_xid_list.next = item;

	return item;
}

static void xid_expire(void)
{
	struct xid_item *item = dhcprelay_xid_list.next;
	struct xid_item *last = &dhcprelay_xid_list;
	unsigned current_time = monotonic_sec();

	while (item != NULL) {
		if ((current_time - item->timestamp) > MAX_LIFETIME) {
			last->next = item->next;
			free(item);
			item = last->next;
		} else {
			last = item;
			item = item->next;
		}
	}
}

static struct xid_item *xid_find(uint32_t xid)
{
	struct xid_item *item = dhcprelay_xid_list.next;
	while (item != NULL) {
		if (item->xid == xid) {
			return item;
		}
		item = item->next;
	}
	return NULL;
}

static void xid_del(uint32_t xid)
{
	struct xid_item *item = dhcprelay_xid_list.next;
	struct xid_item *last = &dhcprelay_xid_list;
	while (item != NULL) {
		if (item->xid == xid) {
			last->next = item->next;
			free(item);
			item = last->next;
		} else {
			last = item;
			item = item->next;
		}
	}
}

/**
 * get_dhcp_packet_type - gets the message type of a dhcp packet
 * p - pointer to the dhcp packet
 * returns the message type on success, -1 otherwise
 */
static int get_dhcp_packet_type(struct dhcp_packet *p)
{
	uint8_t *op;

	/* it must be either a BOOTREQUEST or a BOOTREPLY */
	if (p->op != BOOTREQUEST && p->op != BOOTREPLY)
		return -1;
	/* get message type option */
	op = udhcp_get_option(p, DHCP_MESSAGE_TYPE);
	if (op != NULL)
		return op[0];
	return -1;
}

/**
 * get_client_devices - parses the devices list
 * dev_list - comma separated list of devices
 * returns array
 */
static char **get_client_devices(char *dev_list, int *client_number)
{
	char *s, **client_dev;
	int i, cn;

	/* copy list */
	dev_list = xstrdup(dev_list);

	/* get number of items, replace ',' with NULs */
	s = dev_list;
	cn = 1;
	while (*s) {
		if (*s == ',') {
			*s = '\0';
			cn++;
		}
		s++;
	}
	*client_number = cn;

	/* create vector of pointers */
	client_dev = xzalloc(cn * sizeof(*client_dev));
	client_dev[0] = dev_list;
	i = 1;
	while (i != cn) {
		client_dev[i] = client_dev[i - 1] + strlen(client_dev[i - 1]) + 1;
		i++;
	}
	return client_dev;
}

/* Creates listen sockets (in fds) bound to client and server ifaces,
 * and returns numerically max fd.
 */
static int init_sockets(char **client_ifaces, int num_clients,
			char *server_iface, int *fds)
{
	int i, n;

	/* talk to real server on bootps */
	fds[0] = udhcp_listen_socket(/*INADDR_ANY,*/ SERVER_PORT, server_iface);
	n = fds[0];

	for (i = 1; i < num_clients; i++) {
		/* listen for clients on bootps */
		fds[i] = udhcp_listen_socket(/*INADDR_ANY,*/ SERVER_PORT, client_ifaces[i-1]);
		if (fds[i] > n)
			n = fds[i];
	}
	return n;
}

/**
 * pass_to_server() - forwards dhcp packets from client to server
 * p - packet to send
 * client - number of the client
 */
static void pass_to_server(struct dhcp_packet *p, int packet_len, int client, int *fds,
			struct sockaddr_in *client_addr, struct sockaddr_in *server_addr)
{
	int res, type;

	/* check packet_type */
	type = get_dhcp_packet_type(p);
	if (type != DHCPDISCOVER && type != DHCPREQUEST
	 && type != DHCPDECLINE && type != DHCPRELEASE
	 && type != DHCPINFORM
	) {
		return;
	}

	/* create new xid entry */
	xid_add(p->xid, client_addr, client);

	/* forward request to LAN (server) */
	errno = 0;
	res = sendto(fds[0], p, packet_len, 0, (struct sockaddr*)server_addr,
			sizeof(struct sockaddr_in));
	if (res != packet_len) {
		bb_perror_msg("sendto");
	}
}

/**
 * pass_to_client() - forwards dhcp packets from server to client
 * p - packet to send
 */
static void pass_to_client(struct dhcp_packet *p, int packet_len, int *fds)
{
	int res, type;
	struct xid_item *item;

	/* check xid */
	item = xid_find(p->xid);
	if (!item) {
		return;
	}

	/* check packet type */
	type = get_dhcp_packet_type(p);
	if (type != DHCPOFFER && type != DHCPACK && type != DHCPNAK) {
		return;
	}

	if (item->ip.sin_addr.s_addr == htonl(INADDR_ANY))
		item->ip.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	errno = 0;
	res = sendto(fds[item->client], p, packet_len, 0, (struct sockaddr*) &(item->ip),
			sizeof(item->ip));
	if (res != packet_len) {
		bb_perror_msg("sendto");
		return;
	}

	/* remove xid entry */
	xid_del(p->xid);
}

int dhcprelay_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dhcprelay_main(int argc, char **argv)
{
	struct dhcp_packet dhcp_msg;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	fd_set rfds;
	char **client_ifaces;
	int *fds;
	int num_sockets, max_socket;
	uint32_t our_nip;

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);

	/* dhcprelay client_iface1,client_iface2,... server_iface [server_IP] */
	if (argc == 4) {
		if (!inet_aton(argv[3], &server_addr.sin_addr))
			bb_perror_msg_and_die("bad server IP");
	} else if (argc == 3) {
		server_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	} else {
		bb_show_usage();
	}

	/* Produce list of client ifaces */
	client_ifaces = get_client_devices(argv[1], &num_sockets);

	num_sockets++; /* for server socket at fds[0] */
	fds = xmalloc(num_sockets * sizeof(fds[0]));

	/* Create sockets and bind one to every iface */
	max_socket = init_sockets(client_ifaces, num_sockets, argv[2], fds);

	/* Get our IP on server_iface */
	if (udhcp_read_interface(argv[2], NULL, &our_nip, NULL, NULL))
		return 1;

	/* Main loop */
	while (1) {
//reinit stuff from time to time? go back to get_client_devices
//every N minutes?
		struct timeval tv;
		size_t packlen;
		socklen_t addr_size;
		int i;

		FD_ZERO(&rfds);
		for (i = 0; i < num_sockets; i++)
			FD_SET(fds[i], &rfds);
		tv.tv_sec = SELECT_TIMEOUT;
		tv.tv_usec = 0;
		if (select(max_socket + 1, &rfds, NULL, NULL, &tv) > 0) {
			/* server */
			if (FD_ISSET(fds[0], &rfds)) {
				packlen = udhcp_recv_kernel_packet(&dhcp_msg, fds[0]);
				if (packlen > 0) {
					pass_to_client(&dhcp_msg, packlen, fds);
				}
			}
			/* clients */
			for (i = 1; i < num_sockets; i++) {
				if (!FD_ISSET(fds[i], &rfds))
					continue;
				addr_size = sizeof(struct sockaddr_in);
				packlen = recvfrom(fds[i], &dhcp_msg, sizeof(dhcp_msg), 0,
						(struct sockaddr *)(&client_addr), &addr_size);
				if (packlen <= 0)
					continue;

				/* Get our IP on corresponding client_iface */
//why? what if server can't route such IP?
				if (udhcp_read_interface(client_ifaces[i-1], NULL, &dhcp_msg.gateway_nip, NULL, NULL)) {
					/* Fall back to our server_iface's IP */
//this makes more sense!
					dhcp_msg.gateway_nip = our_nip;
				}
//maybe set dhcp_msg.flags |= BROADCAST_FLAG too?
				pass_to_server(&dhcp_msg, packlen, i, fds, &client_addr, &server_addr);
			}
		}
		xid_expire();
	} /* while (1) */

	/* return 0; - not reached */
}
