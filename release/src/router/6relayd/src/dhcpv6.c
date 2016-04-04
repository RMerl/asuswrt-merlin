/**
 * Copyright (C) 2012-2013 Steven Barth <steven@midlink.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License v2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <resolv.h>
#include <sys/timerfd.h>

#include "6relayd.h"
#include "dhcpv6.h"


static void relay_client_request(struct sockaddr_in6 *source,
		const void *data, size_t len, struct relayd_interface *iface);
static void relay_server_response(uint8_t *data, size_t len);

static int create_socket(uint16_t port);

static void handle_dhcpv6(void *addr, void *data, size_t len,
		struct relayd_interface *iface);
static void handle_client_request(void *addr, void *data, size_t len,
		struct relayd_interface *iface);

static struct relayd_event dhcpv6_event = {-1, NULL, handle_dhcpv6};

static const struct relayd_config *config = NULL;



// Create socket and register events
int init_dhcpv6_relay(const struct relayd_config *relayd_config)
{
	config = relayd_config;

	if (!config->enable_dhcpv6_relay || config->slavecount < 1)
		return 0;

	if ((dhcpv6_event.socket = create_socket(DHCPV6_SERVER_PORT)) < 0) {
		syslog(LOG_ERR, "Failed to open DHCPv6 server socket: %s",
				strerror(errno));
		return -1;
	}

	dhcpv6_init_ia(relayd_config, dhcpv6_event.socket);


	// Configure multicast settings
	struct ipv6_mreq mreq = {ALL_DHCPV6_RELAYS, 0};
	struct ipv6_mreq mreq2 = {ALL_DHCPV6_SERVERS, 0};
	for (size_t i = 0; i < config->slavecount; ++i) {
		mreq.ipv6mr_interface = config->slaves[i].ifindex;
		setsockopt(dhcpv6_event.socket, IPPROTO_IPV6,
				IPV6_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

		if (config->enable_dhcpv6_server) {
			mreq2.ipv6mr_interface = config->slaves[i].ifindex;
			setsockopt(dhcpv6_event.socket, IPPROTO_IPV6,
					IPV6_ADD_MEMBERSHIP, &mreq2, sizeof(mreq2));
		}
	}

	if (config->enable_dhcpv6_server)
		dhcpv6_event.handle_dgram = handle_client_request;
	relayd_register_event(&dhcpv6_event);

	return 0;
}


// Create server socket
static int create_socket(uint16_t port)
{
#ifdef SOCK_CLOEXEC
	int sock = socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
#else
	int sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	sock = fflags(sock, O_CLOEXEC);
#endif
	if (sock < 0)
		return -1;

	// Basic IPv6 configuration
	int val = 1;
	setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val, sizeof(val));

	val = DHCPV6_HOP_COUNT_LIMIT;
	setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, sizeof(val));

	val = 0;
	setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val, sizeof(val));

	struct sockaddr_in6 bind_addr = {AF_INET6, htons(port),
				0, IN6ADDR_ANY_INIT, 0};
	if (bind(sock, (struct sockaddr*)&bind_addr, sizeof(bind_addr))) {
		close(sock);
		return -1;
	}

	return sock;
}


static void handle_nested_message(uint8_t *data, size_t len,
		uint8_t **opts, uint8_t **end, struct iovec iov[5])
{
	struct dhcpv6_relay_header *hdr = (struct dhcpv6_relay_header*)data;
	if (iov[0].iov_base == NULL) {
		iov[0].iov_base = data;
		iov[0].iov_len = len;
	}

	if (len < sizeof(struct dhcpv6_client_header))
		return;

	if (hdr->msg_type != DHCPV6_MSG_RELAY_FORW) {
		iov[0].iov_len = data - (uint8_t*)iov[0].iov_base;
		struct dhcpv6_client_header *hdr = (void*)data;
		*opts = (uint8_t*)&hdr[1];
		*end = data + len;
		return;
	}

	uint16_t otype, olen;
	uint8_t *odata;
	dhcpv6_for_each_option(hdr->options, data + len, otype, olen, odata) {
		if (otype == DHCPV6_OPT_RELAY_MSG) {
			iov[5].iov_base = odata + olen;
			iov[5].iov_len = (((uint8_t*)iov[0].iov_base) + iov[0].iov_len)
					- (odata + olen);
			handle_nested_message(odata, olen, opts, end, iov);
			return;
		}
	}
}


static void update_nested_message(uint8_t *data, size_t len, ssize_t pdiff)
{
	struct dhcpv6_relay_header *hdr = (struct dhcpv6_relay_header*)data;
	if (hdr->msg_type != DHCPV6_MSG_RELAY_FORW)
		return;

	hdr->msg_type = DHCPV6_MSG_RELAY_REPL;

	uint16_t otype, olen;
	uint8_t *odata;
	dhcpv6_for_each_option(hdr->options, data + len, otype, olen, odata) {
		if (otype == DHCPV6_OPT_RELAY_MSG) {
			olen += pdiff;
			odata[-2] = (olen >> 8) & 0xff;
			odata[-1] = olen & 0xff;
			update_nested_message(odata, olen - pdiff, pdiff);
			return;
		}
	}
}


// Simple DHCPv6-server for information requests
static void handle_client_request(void *addr, void *data, size_t len,
		struct relayd_interface *iface)
{
	struct dhcpv6_client_header *hdr = data;
	if (len < sizeof(*hdr))
		return;

	syslog(LOG_NOTICE, "Got DHCPv6 request");

	// Construct reply message
	struct __attribute__((packed)) {
		uint8_t msg_type;
		uint8_t tr_id[3];
		uint16_t serverid_type;
		uint16_t serverid_length;
		uint16_t duid_type;
		uint16_t hardware_type;
		uint8_t mac[6];
		uint16_t clientid_type;
		uint16_t clientid_length;
		uint8_t clientid_buf[130];
	} dest = {
		.msg_type = DHCPV6_MSG_REPLY,
		.serverid_type = htons(DHCPV6_OPT_SERVERID),
		.serverid_length = htons(10),
		.duid_type = htons(3),
		.hardware_type = htons(1),
		.clientid_type = htons(DHCPV6_OPT_CLIENTID),
		.clientid_buf = {0}
	};
	memcpy(dest.mac, iface->mac, sizeof(dest.mac));

	struct __attribute__((packed)) {
		uint16_t type;
		uint16_t len;
		uint16_t value;
	} stat = {htons(DHCPV6_OPT_STATUS), htons(sizeof(stat) - 4),
			htons(DHCPV6_STATUS_NOADDRSAVAIL)};

	struct __attribute__((packed)) {
		uint16_t type;
		uint16_t len;
		uint32_t value;
	} refresh = {htons(DHCPV6_OPT_INFO_REFRESH), htons(sizeof(uint32_t)),
			htonl(600)};

	struct __attribute__((packed)) {
		uint16_t type;
		uint16_t len;
		uint8_t name[255];
	} domain = {htons(DHCPV6_OPT_DNS_DOMAIN), 0, {0}};
	size_t domain_len = 0;

	struct __attribute__((packed)) {
		uint16_t dns_type;
		uint16_t dns_length;
		struct in6_addr addr;
	} dnsaddr = {htons(DHCPV6_OPT_DNS_SERVERS), htons(sizeof(struct in6_addr)), IN6ADDR_ANY_INIT};

	res_init();
	const char *search = _res.dnsrch[0];
	if (search && search[0]) {
		int len = dn_comp(search, domain.name,
				sizeof(domain.name), NULL, NULL);
		if (len > 0) {
			domain.len = htons(len);
			domain_len = len + 4;
		}

	}

	uint8_t pdbuf[512];
	struct iovec iov[] = {{NULL, 0}, {&dest, (uint8_t*)&dest.clientid_type
			- (uint8_t*)&dest}, {&dnsaddr, 0}, {&domain, domain_len},
			{pdbuf, 0}, {NULL, 0}};

	uint8_t *opts = (uint8_t*)&hdr[1], *opts_end = (uint8_t*)data + len;
	if (hdr->msg_type == DHCPV6_MSG_RELAY_FORW)
		handle_nested_message(data, len, &opts, &opts_end, iov);

	memcpy(dest.tr_id, &opts[-3], sizeof(dest.tr_id));

	if (opts[-4] == DHCPV6_MSG_ADVERTISE || opts[-4] == DHCPV6_MSG_REPLY || opts[-4] == DHCPV6_MSG_RELAY_REPL)
		return;

	if (opts[-4] == DHCPV6_MSG_SOLICIT) {
		dest.msg_type = DHCPV6_MSG_ADVERTISE;
	} else if (opts[-4] == DHCPV6_MSG_INFORMATION_REQUEST) {
		iov[4].iov_base = &refresh;
		iov[4].iov_len = sizeof(refresh);
	}

	// Go through options and find what we need
	uint16_t otype, olen;
	uint8_t *odata;
	dhcpv6_for_each_option(opts, opts_end, otype, olen, odata) {
		if (otype == DHCPV6_OPT_CLIENTID && olen <= 130) {
			dest.clientid_length = htons(olen);
			memcpy(dest.clientid_buf, odata, olen);
			iov[1].iov_len += 4 + olen;
		} else if (otype == DHCPV6_OPT_SERVERID) {
			if (olen != ntohs(dest.serverid_length) ||
					memcmp(odata, &dest.duid_type, olen))
				return; // Not for us
		}
	}

	if (opts[-4] != DHCPV6_MSG_INFORMATION_REQUEST) {
		iov[4].iov_len = dhcpv6_handle_ia(pdbuf, sizeof(pdbuf), iface, addr, &opts[-4], opts_end);
		if (iov[4].iov_len == 0 && opts[-4] == DHCPV6_MSG_REBIND)
			return;
	}

	if (!IN6_IS_ADDR_UNSPECIFIED(&config->dnsaddr)) {
		dnsaddr.addr = config->dnsaddr;
		iov[2].iov_len = sizeof(dnsaddr);
	} else {
		struct relayd_ipaddr ipaddr;
		if (relayd_get_interface_addresses(iface->ifindex, &ipaddr, 1) == 1) {
			dnsaddr.addr = ipaddr.addr;
			iov[2].iov_len = sizeof(dnsaddr);
		}
	}

	if (iov[0].iov_len > 0) // Update length
		update_nested_message(data, len, iov[1].iov_len + iov[2].iov_len +
				iov[3].iov_len + iov[4].iov_len - (4 + opts_end - opts));

	relayd_forward_packet(dhcpv6_event.socket, addr, iov, 5, iface);
}


// Central DHCPv6-relay handler
static void handle_dhcpv6(void *addr, void *data, size_t len,
		struct relayd_interface *iface)
{
	if (iface == &config->master)
		relay_server_response(data, len);
	else
		relay_client_request(addr, data, len, iface);
}


// Relay server response (regular relay server handling)
static void relay_server_response(uint8_t *data, size_t len)
{
	// Information we need to gather
	uint8_t *payload_data = NULL;
	size_t payload_len = 0;
	int32_t ifaceidx = 0;
	struct sockaddr_in6 target = {AF_INET6, htons(DHCPV6_CLIENT_PORT),
		0, IN6ADDR_ANY_INIT, 0};

	syslog(LOG_NOTICE, "Got a DHCPv6-reply");

	int otype, olen;
	uint8_t *odata, *end = data + len;

	// Relay DHCPv6 reply from server to client
	struct dhcpv6_relay_header *h = (void*)data;
	if (len < sizeof(*h) || h->msg_type != DHCPV6_MSG_RELAY_REPL)
		return;

	memcpy(&target.sin6_addr, &h->peer_address,
			sizeof(struct in6_addr));

	// Go through options and find what we need
	dhcpv6_for_each_option(h->options, end, otype, olen, odata) {
		if (otype == DHCPV6_OPT_INTERFACE_ID
				&& olen == sizeof(ifaceidx)) {
			memcpy(&ifaceidx, odata, sizeof(ifaceidx));
		} else if (otype == DHCPV6_OPT_RELAY_MSG) {
			payload_data = odata;
			payload_len = olen;
		}
	}

	// Invalid interface-id or basic payload
	struct relayd_interface *iface = relayd_get_interface_by_index(ifaceidx);
	if (!iface || iface == &config->master || !payload_data || payload_len < 4)
		return;

	bool is_authenticated = false;
	struct in6_addr *dns_ptr = NULL;
	size_t dns_count = 0;

	// If the payload is relay-reply we have to send to the server port
	if (payload_data[0] == DHCPV6_MSG_RELAY_REPL) {
		target.sin6_port = htons(DHCPV6_SERVER_PORT);
	} else { // Go through the payload data
		struct dhcpv6_client_header *h = (void*)payload_data;
		end = payload_data + payload_len;

		dhcpv6_for_each_option(&h[1], end, otype, olen, odata) {
			if (otype == DHCPV6_OPT_DNS_SERVERS && olen >= 16) {
				dns_ptr = (struct in6_addr*)odata;
				dns_count = olen / 16;
			} else if (otype == DHCPV6_OPT_AUTH) {
				is_authenticated = true;
			}
		}
	}

	// Rewrite DNS servers if requested
	if (config->always_rewrite_dns && dns_ptr && dns_count > 0) {
		if (is_authenticated)
			return; // Impossible to rewrite

		struct relayd_ipaddr ip;
		const struct in6_addr *rewrite;

		if (!IN6_IS_ADDR_UNSPECIFIED(&config->dnsaddr)) {
			rewrite = &config->dnsaddr;
		} else {
			if (relayd_get_interface_addresses(iface->ifindex, &ip, 1) < 1)
				return; // Unable to get interface address
			rewrite = &ip.addr;
		}

		// Copy over any other addresses
		for (size_t i = 0; i < dns_count; ++i)
			memcpy(&dns_ptr[i], rewrite, sizeof(*rewrite));
	}

	struct iovec iov = {payload_data, payload_len};
	relayd_forward_packet(dhcpv6_event.socket, &target, &iov, 1, iface);
}


// Relay client request (regular DHCPv6-relay)
static void relay_client_request(struct sockaddr_in6 *source,
		const void *data, size_t len, struct relayd_interface *iface)
{
	const struct dhcpv6_relay_header *h = data;
	if (h->msg_type == DHCPV6_MSG_RELAY_REPL ||
			h->msg_type == DHCPV6_MSG_RECONFIGURE ||
			h->msg_type == DHCPV6_MSG_REPLY ||
			h->msg_type == DHCPV6_MSG_ADVERTISE)
		return; // Invalid message types for client

	syslog(LOG_NOTICE, "Got a DHCPv6-request");

	// Construct our forwarding envelope
	struct dhcpv6_relay_forward_envelope hdr = {
		.msg_type = DHCPV6_MSG_RELAY_FORW,
		.hop_count = 0,
		.interface_id_type = htons(DHCPV6_OPT_INTERFACE_ID),
		.interface_id_len = htons(sizeof(uint32_t)),
		.relay_message_type = htons(DHCPV6_OPT_RELAY_MSG),
		.relay_message_len = htons(len),
	};

	if (h->msg_type == DHCPV6_MSG_RELAY_FORW) { // handle relay-forward
		if (h->hop_count >= DHCPV6_HOP_COUNT_LIMIT)
			return; // Invalid hop count
		else
			hdr.hop_count = h->hop_count + 1;
	}

	// use memcpy here as the destination fields are unaligned
	uint32_t ifindex = iface->ifindex;
	memcpy(&hdr.peer_address, &source->sin6_addr, sizeof(struct in6_addr));
	memcpy(&hdr.interface_id_data, &ifindex, sizeof(ifindex));

	// Detect public IP of slave interface to use as link-address
	struct relayd_ipaddr ip;
	if (relayd_get_interface_addresses(iface->ifindex, &ip, 1) < 1) {
		// No suitable address! Is the slave not configured yet?
		// Detect public IP of master interface and use it instead
		// This is WRONG and probably violates the RFC. However
		// otherwise we have a hen and egg problem because the
		// slave-interface cannot be auto-configured.
		if (relayd_get_interface_addresses(config->master.ifindex,
				&ip, 1) < 1)
			return; // Could not obtain a suitable address
	}
	memcpy(&hdr.link_address, &ip.addr, sizeof(hdr.link_address));

	struct sockaddr_in6 dhcpv6_servers = {AF_INET6,
			htons(DHCPV6_SERVER_PORT), 0, ALL_DHCPV6_SERVERS, 0};
	struct iovec iov[2] = {{&hdr, sizeof(hdr)}, {(void*)data, len}};
	relayd_forward_packet(dhcpv6_event.socket, &dhcpv6_servers,
			iov, 2, &config->master);
}
