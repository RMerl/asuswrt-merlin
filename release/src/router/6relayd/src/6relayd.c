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
 */

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip6.h>
#include <netpacket/packet.h>
#include <linux/rtnetlink.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <sys/syscall.h>

#include "6relayd.h"


static struct relayd_config config;

static int epoll, ioctl_sock;
static size_t epoll_registered = 0;
static volatile bool do_stop = false;

static int rtnl_socket = -1;
static int rtnl_seq = 0;
static int urandom_fd = -1;

static int print_usage(const char *name);
static void set_stop(_unused int signal);
static void wait_child(_unused int signal);
static int open_interface(struct relayd_interface *iface,
		const char *ifname, bool external);
static void relayd_receive_packets(struct relayd_event *event);


int main(int argc, char* const argv[])
{
	memset(&config, 0, sizeof(config));

	const char *pidfile = "/var/run/6relayd.pid";
	bool daemonize = false;
	int verbosity = 0;
	int c;
	while ((c = getopt(argc, argv, "ASR:D:Nsucn::l:a:rt:m:oi:p:dvh")) != -1) {
		switch (c) {
		case 'A':
			config.enable_router_discovery_relay = true;
			config.enable_dhcpv6_relay = true;
			config.enable_ndp_relay = true;
			config.send_router_solicitation = true;
			config.enable_route_learning = true;
			break;

		case 'S':
			config.enable_router_discovery_relay = true;
			config.enable_router_discovery_server = true;
			config.enable_dhcpv6_relay = true;
			config.enable_dhcpv6_server = true;
			break;

		case 'R':
			config.enable_router_discovery_relay = true;
			if (!strcmp(optarg, "server"))
				config.enable_router_discovery_server = true;
			else if (strcmp(optarg, "relay"))
				return print_usage(argv[0]);
			break;

		case 'D':
			config.enable_dhcpv6_relay = true;
			if (!strcmp(optarg, "server"))
				config.enable_dhcpv6_server = true;
			else if (strcmp(optarg, "relay"))
				return print_usage(argv[0]);
			break;

		case 'N':
			config.enable_ndp_relay = true;
			break;

		case 's':
			config.send_router_solicitation = true;
			break;

		case 'u':
			config.always_announce_default_router = true;
			break;

		case 'c':
			config.deprecate_ula_if_public_avail = true;
			break;

		case 'n':
			config.always_rewrite_dns = true;
			if (optarg)
				inet_pton(AF_INET6, optarg, &config.dnsaddr);
			break;

		case 'l':
			config.dhcpv6_statefile = strtok(optarg, ",");
			if (config.dhcpv6_statefile)
				config.dhcpv6_cb = strtok(NULL, ",");
			break;

		case 'a':
			config.dhcpv6_lease = realloc(config.dhcpv6_lease,
					sizeof(char*) * ++config.dhcpv6_lease_len);
			config.dhcpv6_lease[config.dhcpv6_lease_len - 1] = optarg;
			break;

		case 'r':
			config.enable_route_learning = true;
			break;

		case 't':
			config.static_ndp = realloc(config.static_ndp,
					sizeof(char*) * ++config.static_ndp_len);
			config.static_ndp[config.static_ndp_len - 1] = optarg;
			break;

		case 'm':
			config.ra_managed_mode = atoi(optarg);
			break;

		case 'o':
			config.ra_not_onlink = true;
			break;

		case 'i':
			if (!strcmp(optarg, "low"))
				config.ra_preference = -1;
			else if (!strcmp(optarg, "high"))
				config.ra_preference = 1;
			break;

		case 'p':
			pidfile = optarg;
			break;

		case 'd':
			daemonize = true;
			break;

		case 'v':
			verbosity++;
			break;

		default:
			return print_usage(argv[0]);
		}
	}

	openlog("6relayd", LOG_PERROR | LOG_PID, LOG_DAEMON);
	if (verbosity == 0)
		setlogmask(LOG_UPTO(LOG_WARNING));
	else if (verbosity == 1)
		setlogmask(LOG_UPTO(LOG_INFO));

	if (argc - optind < 1)
		return print_usage(argv[0]);

	if (getuid() != 0) {
		syslog(LOG_ERR, "Must be run as root. stopped.");
		return 2;
	}

#if defined(__NR_epoll_create1) && defined(EPOLL_CLOEXEC)
	epoll = epoll_create1(EPOLL_CLOEXEC);
#else
	epoll = epoll_create(32);
	epoll = fflags(epoll, O_CLOEXEC);
#endif
	if (epoll < 0) {
		syslog(LOG_ERR, "Unable to open epoll: %s", strerror(errno));
		return 2;
	}

#ifdef SOCK_CLOEXEC
	ioctl_sock = socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, 0);
#else
	ioctl_sock = socket(AF_INET6, SOCK_DGRAM, 0);
	ioctl_sock = fflags(ioctl_sock, O_CLOEXEC);
#endif

	if ((rtnl_socket = relayd_open_rtnl_socket()) < 0) {
		syslog(LOG_ERR, "Unable to open socket: %s", strerror(errno));
		return 2;
	}

	if (open_interface(&config.master, argv[optind++], false))
		return 3;

	config.slavecount = argc - optind;
	config.slaves = calloc(config.slavecount, sizeof(*config.slaves));

	for (size_t i = 0; i < config.slavecount; ++i) {
		const char *name = argv[optind + i];
		bool external = (name[0] == '~');
		if (external)
			++name;

		if (open_interface(&config.slaves[i], name, external))
			return 3;
	}

	if ((urandom_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC)) < 0)
		return 4;

	struct sigaction sa = {.sa_handler = SIG_IGN};
	sigaction(SIGUSR1, &sa, NULL);

	if (init_router_discovery_relay(&config))
		return 4;

	if (init_dhcpv6_relay(&config))
		return 4;

	if (init_ndp_proxy(&config))
		return 4;

	if (epoll_registered == 0) {
		syslog(LOG_WARNING, "No relays enabled or no slave "
				"interfaces specified. stopped.");
		return 5;
	}

	if (daemonize) {
		openlog("6relayd", LOG_PID, LOG_DAEMON); // Disable LOG_PERROR
		if (daemon(0, 0)) {
			syslog(LOG_ERR, "Failed to daemonize: %s",
					strerror(errno));
			return 6;
		}
		FILE *fp = fopen(pidfile, "w");
		if (fp) {
			fprintf(fp, "%i\n", getpid());
			fclose(fp);
		}
	}

	signal(SIGTERM, set_stop);
	signal(SIGHUP, set_stop);
	signal(SIGINT, set_stop);
	signal(SIGCHLD, wait_child);

	// Main loop
	while (!do_stop) {
		struct epoll_event ev[16];
		int len = epoll_wait(epoll, ev, 16, -1);
		for (int i = 0; i < len; ++i) {
			struct relayd_event *event = ev[i].data.ptr;
			if (event->handle_event)
				event->handle_event(event);
			else if (event->handle_dgram)
				relayd_receive_packets(event);
		}
	}

	syslog(LOG_WARNING, "Termination requested by signal.");

	deinit_ndp_proxy();
	deinit_router_discovery_relay();
	free(config.slaves);
	close(urandom_fd);
	return 0;
}


static int print_usage(const char *name)
{
	fprintf(stderr,
	"Usage: %s [options] <master> [[~]<slave1> [[~]<slave2> [...]]]\n"
	"\nNote: to use server features only (no relaying) set master to '.'\n"
	"\nFeatures:\n"
	"	-A		Automatic relay (defaults: RrelayDrelayNsr)\n"
	"	-S		Automatic server (defaults: RserverDserver)\n"
	"	-R <mode>	Enable Router Discovery support (RD)\n"
	"	   relay	relay mode\n"
	"	   server	mini-server for Router Discovery on slaves\n"
	"	-D <mode>	Enable DHCPv6-support\n"
	"	   relay	standards-compliant relay\n"
	"	   server	server for DHCPv6 + PD on slaves\n"
	"	-N		Enable Neighbor Discovery Proxy (NDP)\n"
	"\nFeature options:\n"
	"	-s		Send initial RD-Solicitation to <master>\n"
	"	-u		RD: Assume default router even with ULA only\n"
	"	-c		RD: ULA-compatibility with broken devices\n"
	"	-m <mode>	RD: Address Management Level\n"
	"	   0 (default)	enable SLAAC and don't send Managed-Flag\n"
	"	   1		enable SLAAC and send Managed-Flag\n"
	"	   2		disable SLAAC and send Managed-Flag\n"
	"	-o		RD: Don't send on-link flag for prefixes\n"
	"	-i <preference>	RD: Route info and default preference\n"
	"	   medium	medium priority (default)\n"
	"	   low		low priority\n"
	"	   high		high priority\n"
	"	-n [server]	RD/DHCPv6: always rewrite name server\n"
	"	-l <file>,<cmd>	DHCPv6: IA lease-file and update callback\n"
	"	-a <duid>:<val>	DHCPv6: IA_NA static assignment\n"
	"	-r		NDP: learn routes to neighbors\n"
	"	-t <p>/<l>:<if>	NDP: define a static NDP-prefix on <if>\n"
	"	slave prefix ~	NDP: don't proxy NDP for hosts and only\n"
	"			serve NDP for DAD and traffic to router\n"
	"\nInvocation options:\n"
	"	-p <pidfile>	Set pidfile (/var/run/6relayd.pid)\n"
	"	-d		Daemonize\n"
	"	-v		Increase logging verbosity\n"
	"	-h		Show this help\n\n",
	name);
	return 1;
}


static void wait_child(_unused int signal)
{
	while (waitpid(-1, NULL, WNOHANG) > 0);
}


static void set_stop(_unused int signal)
{
	do_stop = true;
}


// Create an interface context
static int open_interface(struct relayd_interface *iface,
		const char *ifname, bool external)
{
	if (ifname[0] == '.' && iface == &config.master)
		return 0; // Skipped

	int status = 0;

	size_t ifname_len = strlen(ifname) + 1;
	if (ifname_len > IF_NAMESIZE)
		ifname_len = IF_NAMESIZE;

	struct ifreq ifr;
	memcpy(ifr.ifr_name, ifname, ifname_len);

	// Detect interface index
	if (ioctl(ioctl_sock, SIOCGIFINDEX, &ifr) < 0)
		goto err;

	iface->ifindex = ifr.ifr_ifindex;

	// Detect MAC-address of interface
	if (ioctl(ioctl_sock, SIOCGIFHWADDR, &ifr) < 0)
		goto err;

	// Fill interface structure
	memcpy(iface->mac, ifr.ifr_hwaddr.sa_data, sizeof(iface->mac));
	memcpy(iface->ifname, ifname, ifname_len);
	iface->external = external;

	goto out;

err:
	syslog(LOG_ERR, "Unable to open interface %s (%s)",
			ifname, strerror(errno));
	status = -1;
out:
	return status;
}


int relayd_open_rtnl_socket(void)
{
#ifdef SOCK_CLOEXEC
	int sock = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
#else
	int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	sock = fflags(sock, O_CLOEXEC);
#endif

	// Connect to the kernel netlink interface
	struct sockaddr_nl nl = {.nl_family = AF_NETLINK};
	if (connect(sock, (struct sockaddr*)&nl, sizeof(nl))) {
		syslog(LOG_ERR, "Failed to connect to kernel rtnetlink: %s",
				strerror(errno));
		return -1;
	}

	return sock;
}


// Read IPv6 MTU for interface
int relayd_get_interface_mtu(const char *ifname)
{
	char buf[64];
	const char *sysctl_pattern = "/proc/sys/net/ipv6/conf/%s/mtu";
	snprintf(buf, sizeof(buf), sysctl_pattern, ifname);

	int fd = open(buf, O_RDONLY);
	ssize_t len = read(fd, buf, sizeof(buf) - 1);
	close(fd);

	if (len < 0)
		return -1;


	buf[len] = 0;
	return atoi(buf);

}


// Read IPv6 MAC for interface
int relayd_get_interface_mac(const char *ifname, uint8_t mac[6])
{
	struct ifreq ifr;
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(ioctl_sock, SIOCGIFHWADDR, &ifr) < 0)
		return -1;
	memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
	return 0;
}


// Register events for the multiplexer
int relayd_register_event(struct relayd_event *event)
{
	struct epoll_event ev = {EPOLLIN | EPOLLET, {event}};
	if (!epoll_ctl(epoll, EPOLL_CTL_ADD, event->socket, &ev)) {
		++epoll_registered;
		return 0;
	} else {
		return -1;
	}
}


// Forwards a packet on a specific interface
ssize_t relayd_forward_packet(int socket, struct sockaddr_in6 *dest,
		struct iovec *iov, size_t iov_len,
		const struct relayd_interface *iface)
{
	// Construct headers
	uint8_t cmsg_buf[CMSG_SPACE(sizeof(struct in6_pktinfo))] = {0};
	struct msghdr msg = {(void*)dest, sizeof(*dest), iov, iov_len,
				cmsg_buf, sizeof(cmsg_buf), 0};

	// Set control data (define destination interface)
	struct cmsghdr *chdr = CMSG_FIRSTHDR(&msg);
	chdr->cmsg_level = IPPROTO_IPV6;
	chdr->cmsg_type = IPV6_PKTINFO;
	chdr->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	struct in6_pktinfo *pktinfo = (struct in6_pktinfo*)CMSG_DATA(chdr);
	pktinfo->ipi6_ifindex = iface->ifindex;

	// Also set scope ID if link-local
	if (IN6_IS_ADDR_LINKLOCAL(&dest->sin6_addr)
			|| IN6_IS_ADDR_MC_LINKLOCAL(&dest->sin6_addr))
		dest->sin6_scope_id = iface->ifindex;

	// IPV6_PKTINFO doesn't really work for IPv6-raw sockets (bug?)
	if (dest->sin6_port == 0) {
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
	}

	char ipbuf[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &dest->sin6_addr, ipbuf, sizeof(ipbuf));

	ssize_t sent = sendmsg(socket, &msg, MSG_DONTWAIT);
	if (sent < 0)
		syslog(LOG_WARNING, "Failed to relay to %s%%%s (%s)",
				ipbuf, iface->ifname, strerror(errno));
	else
		syslog(LOG_NOTICE, "Relayed %li bytes to %s%%%s",
				(long)sent, ipbuf, iface->ifname);
	return sent;
}


// Detect an IPV6-address currently assigned to the given interface
ssize_t relayd_get_interface_addresses(int ifindex,
		struct relayd_ipaddr *addrs, size_t cnt)
{
	struct {
		struct nlmsghdr nhm;
		struct ifaddrmsg ifa;
	} req = {{sizeof(req), RTM_GETADDR, NLM_F_REQUEST | NLM_F_DUMP,
			++rtnl_seq, 0}, {AF_INET6, 0, 0, 0, ifindex}};
	if (send(rtnl_socket, &req, sizeof(req), 0) < (ssize_t)sizeof(req))
		return 0;

	uint8_t buf[8192];
	ssize_t len = 0, ret = 0;

	for (struct nlmsghdr *nhm = NULL; ; nhm = NLMSG_NEXT(nhm, len)) {
		while (len < 0 || !NLMSG_OK(nhm, (size_t)len)) {
			len = recv(rtnl_socket, buf, sizeof(buf), 0);
			nhm = (struct nlmsghdr*)buf;
			if (len < 0 || !NLMSG_OK(nhm, (size_t)len)) {
				if (errno == EINTR)
					continue;
				else
					return ret;
			}
		}

		if (nhm->nlmsg_type != RTM_NEWADDR)
			break;

		// Skip address but keep clearing socket buffer
		if (ret >= (ssize_t)cnt)
			continue;

		struct ifaddrmsg *ifa = NLMSG_DATA(nhm);
		if (ifa->ifa_scope != RT_SCOPE_UNIVERSE ||
				ifa->ifa_index != (unsigned)ifindex)
			continue;

		struct rtattr *rta = (struct rtattr*)&ifa[1];
		size_t alen = NLMSG_PAYLOAD(nhm, sizeof(*ifa));
		memset(&addrs[ret], 0, sizeof(addrs[ret]));
		addrs[ret].prefix = ifa->ifa_prefixlen;

		while (RTA_OK(rta, alen)) {
			if (rta->rta_type == IFA_ADDRESS) {
				memcpy(&addrs[ret].addr, RTA_DATA(rta),
						sizeof(struct in6_addr));
			} else if (rta->rta_type == IFA_CACHEINFO) {
				struct ifa_cacheinfo *ifc = RTA_DATA(rta);
				addrs[ret].preferred = ifc->ifa_prefered;
				addrs[ret].valid = ifc->ifa_valid;
			}

			rta = RTA_NEXT(rta, alen);
		}

		if (ifa->ifa_flags & IFA_F_DEPRECATED)
			addrs[ret].preferred = 0;

		++ret;
	}

	return ret;
}


struct relayd_interface* relayd_get_interface_by_index(int ifindex)
{
	if (config.master.ifindex == ifindex)
		return &config.master;

	for (size_t i = 0; i < config.slavecount; ++i)
		if (config.slaves[i].ifindex == ifindex)
			return &config.slaves[i];

	return NULL;
}


struct relayd_interface* relayd_get_interface_by_name(const char *name)
{
	if (!strcmp(config.master.ifname, name))
		return &config.master;

	for (size_t i = 0; i < config.slavecount; ++i)
		if (!strcmp(config.slaves[i].ifname, name))
			return &config.slaves[i];

	return NULL;
}


// Convenience function to receive and do basic validation of packets
static void relayd_receive_packets(struct relayd_event *event)
{
	uint8_t data_buf[RELAYD_BUFFER_SIZE], cmsg_buf[128];
	union {
		struct sockaddr_in6 in6;
		struct sockaddr_ll ll;
		struct sockaddr_nl nl;
	} addr;

	while (true) {
		struct iovec iov = {data_buf, sizeof(data_buf)};
		struct msghdr msg = {&addr, sizeof(addr), &iov, 1,
				cmsg_buf, sizeof(cmsg_buf), 0};

		ssize_t len = recvmsg(event->socket, &msg, MSG_DONTWAIT);
		if (len < 0) {
			if (errno == EAGAIN)
				break;
			else
				continue;
		}


		// Extract destination interface
		int destiface = 0;
		struct in6_pktinfo *pktinfo;
		for (struct cmsghdr *ch = CMSG_FIRSTHDR(&msg); ch != NULL &&
				destiface == 0; ch = CMSG_NXTHDR(&msg, ch)) {
			if (ch->cmsg_level == IPPROTO_IPV6 &&
					ch->cmsg_type == IPV6_PKTINFO) {
				pktinfo = (struct in6_pktinfo*)CMSG_DATA(ch);
				destiface = pktinfo->ipi6_ifindex;
			}
		}

		// Detect interface for packet sockets
		if (addr.ll.sll_family == AF_PACKET)
			destiface = addr.ll.sll_ifindex;

		struct relayd_interface *iface =
				relayd_get_interface_by_index(destiface);

		if (!iface && addr.nl.nl_family != AF_NETLINK)
			continue;

		char ipbuf[INET6_ADDRSTRLEN] = "kernel";
		if (addr.ll.sll_family == AF_PACKET &&
				len >= (ssize_t)sizeof(struct ip6_hdr))
			inet_ntop(AF_INET6, &data_buf[8], ipbuf, sizeof(ipbuf));
		else if (addr.in6.sin6_family == AF_INET6)
			inet_ntop(AF_INET6, &addr.in6.sin6_addr, ipbuf, sizeof(ipbuf));

		syslog(LOG_NOTICE, "--");
		syslog(LOG_NOTICE, "Received %li Bytes from %s%%%s", (long)len,
				ipbuf, (iface) ? iface->ifname : "netlink");

		event->handle_dgram(&addr, data_buf, len, iface);
	}
}


void relayd_urandom(void *data, size_t len)
{
	read(urandom_fd, data, len);
}
