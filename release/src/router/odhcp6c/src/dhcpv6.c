/**
 * Copyright (C) 2012-2014 Steven Barth <steven@midlink.org>
 * Copyright (C) 2017 Hans Dedecker <dedeckeh@gmail.com>
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
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <signal.h>
#include <limits.h>
#include <resolv.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <net/if.h>
#include <net/ethernet.h>

#include "odhcp6c.h"
#ifdef USE_LIBUBOX
#include <libubox/md5.h>
#else
#include "md5.h"
#endif


#define ALL_DHCPV6_RELAYS {{{0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,\
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02}}}
#define DHCPV6_CLIENT_PORT 546
#define DHCPV6_SERVER_PORT 547
#define DHCPV6_DUID_LLADDR 3
#define DHCPV6_REQ_DELAY 1

#define DHCPV6_SOL_MAX_RT_MIN 60
#define DHCPV6_SOL_MAX_RT_MAX 86400
#define DHCPV6_INF_MAX_RT_MIN 60
#define DHCPV6_INF_MAX_RT_MAX 86400

static bool dhcpv6_response_is_valid(const void *buf, ssize_t len,
		const uint8_t transaction[3], enum dhcpv6_msg type,
		const struct in6_addr *daddr);

static int dhcpv6_parse_ia(void *opt, void *end);

static int dhcpv6_calc_refresh_timers(void);
static void dhcpv6_handle_status_code(_unused const enum dhcpv6_msg orig,
		const uint16_t code, const void *status_msg, const int len,
		int *ret);
static void dhcpv6_handle_ia_status_code(const enum dhcpv6_msg orig,
		const struct dhcpv6_ia_hdr *ia_hdr, const uint16_t code,
		const void *status_msg, const int len,
		bool handled_status_codes[_DHCPV6_Status_Max],
		int *ret);
static void dhcpv6_add_server_cand(const struct dhcpv6_server_cand *cand);
static void dhcpv6_clear_all_server_cand(void);

static reply_handler dhcpv6_handle_reply;
static reply_handler dhcpv6_handle_advert;
static reply_handler dhcpv6_handle_rebind_reply;
static reply_handler dhcpv6_handle_reconfigure;
static int dhcpv6_commit_advert(void);

// RFC 3315 - 5.5 Timeout and Delay values
static struct dhcpv6_retx dhcpv6_retx[_DHCPV6_MSG_MAX] = {
	[DHCPV6_MSG_UNKNOWN] = {false, 1, 120, 0, "<POLL>",
			dhcpv6_handle_reconfigure, NULL},
	[DHCPV6_MSG_SOLICIT] = {true, 1, DHCPV6_SOL_MAX_RT, 0, "SOLICIT",
			dhcpv6_handle_advert, dhcpv6_commit_advert},
	[DHCPV6_MSG_REQUEST] = {true, 1, DHCPV6_REQ_MAX_RT, 10, "REQUEST",
			dhcpv6_handle_reply, NULL},
	[DHCPV6_MSG_RENEW] = {false, 10, DHCPV6_REN_MAX_RT, 0, "RENEW",
			dhcpv6_handle_reply, NULL},
	[DHCPV6_MSG_REBIND] = {false, 10, DHCPV6_REB_MAX_RT, 0, "REBIND",
			dhcpv6_handle_rebind_reply, NULL},
	[DHCPV6_MSG_RELEASE] = {false, 1, 0, 5, "RELEASE", NULL, NULL},
	[DHCPV6_MSG_DECLINE] = {false, 1, 0, 5, "DECLINE", NULL, NULL},
	[DHCPV6_MSG_INFO_REQ] = {true, 1, DHCPV6_INF_MAX_RT, 0, "INFOREQ",
			dhcpv6_handle_reply, NULL},
};

// Sockets
static int sock = -1;
static int ifindex = -1;
static int64_t t1 = 0, t2 = 0, t3 = 0;

// IA states
static int request_prefix = -1;
static enum odhcp6c_ia_mode na_mode = IA_MODE_NONE, pd_mode = IA_MODE_NONE;
static bool accept_reconfig = false;
// Server unicast address
static struct in6_addr server_addr = IN6ADDR_ANY_INIT;

// Reconfigure key
static uint8_t reconf_key[16];

// client options
static unsigned int client_options = 0;

static uint32_t ntohl_unaligned(const uint8_t *data)
{
	uint32_t buf;

	memcpy(&buf, data, sizeof(buf));
	return ntohl(buf);
}

int init_dhcpv6(const char *ifname, unsigned int options, int sol_timeout)
{
	client_options = options;
	dhcpv6_retx[DHCPV6_MSG_SOLICIT].max_timeo = sol_timeout;

#ifdef SOCK_CLOEXEC
	sock = socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
#else
	sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	sock = fflags(sock, O_CLOEXEC);
#endif
	if (sock < 0)
		return -1;

	// Detect interface
	struct ifreq ifr;
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0)
		return -1;

	ifindex = ifr.ifr_ifindex;

	// Create client DUID
	size_t client_id_len;
	odhcp6c_get_state(STATE_CLIENT_ID, &client_id_len);
	if (client_id_len == 0) {
		uint8_t duid[14] = {0, DHCPV6_OPT_CLIENTID, 0, 10, 0,
				DHCPV6_DUID_LLADDR, 0, 1};

		if (ioctl(sock, SIOCGIFHWADDR, &ifr) >= 0)
			memcpy(&duid[8], ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);

		uint8_t zero[ETHER_ADDR_LEN] = {0, 0, 0, 0, 0, 0};
		struct ifreq ifs[100], *ifp, *ifend;
		struct ifconf ifc;
		ifc.ifc_req = ifs;
		ifc.ifc_len = sizeof(ifs);

		if (!memcmp(&duid[8], zero, ETHER_ADDR_LEN) &&
				ioctl(sock, SIOCGIFCONF, &ifc) >= 0) {
			// If our interface doesn't have an address...
			ifend = ifs + (ifc.ifc_len / sizeof(struct ifreq));
			for (ifp = ifc.ifc_req; ifp < ifend &&
					!memcmp(&duid[8], zero, ETHER_ADDR_LEN); ifp++) {
				memcpy(ifr.ifr_name, ifp->ifr_name,
						sizeof(ifr.ifr_name));
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
					continue;

				memcpy(&duid[8], ifr.ifr_hwaddr.sa_data,
						ETHER_ADDR_LEN);
			}
		}

		odhcp6c_add_state(STATE_CLIENT_ID, duid, sizeof(duid));
	}

	// Create ORO
	if (!(client_options & DHCPV6_STRICT_OPTIONS)) {
		uint16_t oro[] = {
			htons(DHCPV6_OPT_SIP_SERVER_D),
			htons(DHCPV6_OPT_SIP_SERVER_A),
			htons(DHCPV6_OPT_DNS_SERVERS),
			htons(DHCPV6_OPT_DNS_DOMAIN),
			htons(DHCPV6_OPT_UNICAST),
			htons(DHCPV6_OPT_SNTP_SERVERS),
			htons(DHCPV6_OPT_NTP_SERVER),
			htons(DHCPV6_OPT_AFTR_NAME),
			htons(DHCPV6_OPT_PD_EXCLUDE),
			htons(DHCPV6_OPT_SOL_MAX_RT),
			htons(DHCPV6_OPT_INF_MAX_RT),
#ifdef EXT_CER_ID
			htons(DHCPV6_OPT_CER_ID),
#endif
			htons(DHCPV6_OPT_S46_CONT_MAPE),
			htons(DHCPV6_OPT_S46_CONT_MAPT),
			htons(DHCPV6_OPT_S46_CONT_LW),
		};
		odhcp6c_add_state(STATE_ORO, oro, sizeof(oro));
	}

	// Configure IPv6-options
	int val = 1;
	setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &val, sizeof(val));
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &val, sizeof(val));
	setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, ifname, strlen(ifname));

	struct sockaddr_in6 client_addr = { .sin6_family = AF_INET6,
		.sin6_port = htons(DHCPV6_CLIENT_PORT), .sin6_flowinfo = 0 };

	if (bind(sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0)
		return -1;

	return 0;
}

enum {
	IOV_HDR=0,
	IOV_ORO,
	IOV_ORO_REFRESH,
	IOV_CL_ID,
	IOV_SRV_ID,
	IOV_VENDOR_CLASS_HDR,
	IOV_VENDOR_CLASS,
	IOV_USER_CLASS_HDR,
	IOV_USER_CLASS,
	IOV_RECONF_ACCEPT,
	IOV_FQDN,
	IOV_HDR_IA_NA,
	IOV_IA_NA,
	IOV_IA_PD,
	IOV_TOTAL
};

int dhcpv6_set_ia_mode(enum odhcp6c_ia_mode na, enum odhcp6c_ia_mode pd)
{
	int mode = DHCPV6_UNKNOWN;

	na_mode = na;
	pd_mode = pd;

	if (na_mode == IA_MODE_NONE && pd_mode == IA_MODE_NONE)
		mode = DHCPV6_STATELESS;
	else if (na_mode == IA_MODE_FORCE || pd_mode == IA_MODE_FORCE)
		mode = DHCPV6_STATEFUL;

	return mode;
}

static void dhcpv6_send(enum dhcpv6_msg type, uint8_t trid[3], uint32_t ecs)
{
	// Build FQDN
	char fqdn_buf[256];
	gethostname(fqdn_buf, sizeof(fqdn_buf));
	struct {
		uint16_t type;
		uint16_t len;
		uint8_t flags;
		uint8_t data[256];
	} fqdn;
	size_t fqdn_len = 5 + dn_comp(fqdn_buf, fqdn.data,
			sizeof(fqdn.data), NULL, NULL);
	fqdn.type = htons(DHCPV6_OPT_FQDN);
	fqdn.len = htons(fqdn_len - 4);
	fqdn.flags = 0;

	// Build Client ID
	size_t cl_id_len;
	void *cl_id = odhcp6c_get_state(STATE_CLIENT_ID, &cl_id_len);

	// Get Server ID
	size_t srv_id_len;
	void *srv_id = odhcp6c_get_state(STATE_SERVER_ID, &srv_id_len);

	// Build IA_PDs
	size_t ia_pd_entries = 0, ia_pd_len = 0;
	uint8_t *ia_pd;

	if (type == DHCPV6_MSG_SOLICIT) {
		odhcp6c_clear_state(STATE_IA_PD);
		size_t n_prefixes;
		struct odhcp6c_request_prefix *request_prefixes = odhcp6c_get_state(STATE_IA_PD_INIT, &n_prefixes);
		n_prefixes /= sizeof(struct odhcp6c_request_prefix);

		ia_pd = alloca(n_prefixes * (sizeof(struct dhcpv6_ia_hdr) + sizeof(struct dhcpv6_ia_prefix)));

		for (size_t i = 0; i < n_prefixes; i++) {
			struct dhcpv6_ia_hdr hdr_ia_pd = {
				htons(DHCPV6_OPT_IA_PD),
				htons(sizeof(hdr_ia_pd) - 4 +
				      sizeof(struct dhcpv6_ia_prefix) * !!request_prefixes[i].length),
				request_prefixes[i].iaid, 0, 0
			};
			struct dhcpv6_ia_prefix pref = {
				.type = htons(DHCPV6_OPT_IA_PREFIX),
				.len = htons(sizeof(pref) - 4),
				.prefix = request_prefixes[i].length
			};
			memcpy(ia_pd + ia_pd_len, &hdr_ia_pd, sizeof(hdr_ia_pd));
			ia_pd_len += sizeof(hdr_ia_pd);
			if (request_prefixes[i].length) {
				memcpy(ia_pd + ia_pd_len, &pref, sizeof(pref));
				ia_pd_len += sizeof(pref);
			}
		}
	} else {
		struct odhcp6c_entry *e = odhcp6c_get_state(STATE_IA_PD, &ia_pd_entries);
		ia_pd_entries /= sizeof(*e);

		// we're too lazy to count our distinct IAIDs,
		// so just allocate maximally needed space
		ia_pd = alloca(ia_pd_entries * (sizeof(struct dhcpv6_ia_prefix) + 10 +
					sizeof(struct dhcpv6_ia_hdr)));

		for (size_t i = 0; i < ia_pd_entries; ++i) {
			uint32_t iaid = e[i].iaid;

			// check if this is an unprocessed IAID and skip if not.
			int new_iaid = 1;
			for (int j = i-1; j >= 0; j--) {
				if (e[j].iaid == iaid) {
					new_iaid = 0;
					break;
				}
			}

			if (!new_iaid)
				continue;

			// construct header
			struct dhcpv6_ia_hdr hdr_ia_pd = {
				htons(DHCPV6_OPT_IA_PD),
				htons(sizeof(hdr_ia_pd) - 4),
				iaid, 0, 0
			};

			memcpy(ia_pd + ia_pd_len, &hdr_ia_pd, sizeof(hdr_ia_pd));
			struct dhcpv6_ia_hdr *hdr = (struct dhcpv6_ia_hdr *) (ia_pd + ia_pd_len);
			ia_pd_len += sizeof(hdr_ia_pd);

			for (size_t j = i; j < ia_pd_entries; j++) {
				if (e[j].iaid != iaid)
					continue;

				uint8_t ex_len = 0;
				if (e[j].priority > 0)
					ex_len = ((e[j].priority - e[j].length - 1) / 8) + 6;

				struct dhcpv6_ia_prefix p = {
					.type = htons(DHCPV6_OPT_IA_PREFIX),
					.len = htons(sizeof(p) - 4U + ex_len),
					.prefix = e[j].length,
					.addr = e[j].target
				};

				if (type == DHCPV6_MSG_REQUEST) {
					p.preferred = htonl(e[j].preferred);
					p.valid = htonl(e[j].valid);
				}

				memcpy(ia_pd + ia_pd_len, &p, sizeof(p));
				ia_pd_len += sizeof(p);

				if (ex_len) {
					ia_pd[ia_pd_len++] = 0;
					ia_pd[ia_pd_len++] = DHCPV6_OPT_PD_EXCLUDE;
					ia_pd[ia_pd_len++] = 0;
					ia_pd[ia_pd_len++] = ex_len - 4;
					ia_pd[ia_pd_len++] = e[j].priority;

					uint32_t excl = ntohl(e[j].router.s6_addr32[1]);
					excl >>= (64 - e[j].priority);
					excl <<= 8 - ((e[j].priority - e[j].length) % 8);

					for (size_t i = ex_len - 5; i > 0; --i, excl >>= 8)
						ia_pd[ia_pd_len + i] = excl & 0xff;
					ia_pd_len += ex_len - 5;
				}

				hdr->len = htons(ntohs(hdr->len) + ntohs(p.len) + 4U);
			}
		}
	}

	if (ia_pd_entries > 0)
		request_prefix = 1;

	// Build IA_NAs
	size_t ia_na_entries, ia_na_len = 0;
	void *ia_na = NULL;
	struct odhcp6c_entry *e = odhcp6c_get_state(STATE_IA_NA, &ia_na_entries);
	ia_na_entries /= sizeof(*e);

	struct dhcpv6_ia_hdr hdr_ia_na = {
		htons(DHCPV6_OPT_IA_NA),
		htons(sizeof(hdr_ia_na) - 4),
		htonl(1), 0, 0
	};

	struct dhcpv6_ia_addr pa[ia_na_entries];
	for (size_t i = 0; i < ia_na_entries; ++i) {
		pa[i].type = htons(DHCPV6_OPT_IA_ADDR);
		pa[i].len = htons(sizeof(pa[i]) - 4U);
		pa[i].addr = e[i].target;

		if (type == DHCPV6_MSG_REQUEST) {
			pa[i].preferred = htonl(e[i].preferred);
			pa[i].valid = htonl(e[i].valid);
		} else {
			pa[i].preferred = 0;
			pa[i].valid = 0;
		}
	}

	ia_na = pa;
	ia_na_len = sizeof(pa);
	hdr_ia_na.len = htons(ntohs(hdr_ia_na.len) + ia_na_len);

	// Reconfigure Accept
	struct {
		uint16_t type;
		uint16_t length;
	} reconf_accept = {htons(DHCPV6_OPT_RECONF_ACCEPT), 0};

	// Request Information Refresh
	uint16_t oro_refresh = htons(DHCPV6_OPT_INFO_REFRESH);

	// Build vendor-class option
	size_t vendor_class_len, user_class_len;
	struct dhcpv6_vendorclass *vendor_class = odhcp6c_get_state(STATE_VENDORCLASS, &vendor_class_len);
	void *user_class = odhcp6c_get_state(STATE_USERCLASS, &user_class_len);

	struct {
		uint16_t type;
		uint16_t length;
	} vendor_class_hdr = {htons(DHCPV6_OPT_VENDOR_CLASS), htons(vendor_class_len)};

	struct {
		uint16_t type;
		uint16_t length;
	} user_class_hdr = {htons(DHCPV6_OPT_USER_CLASS), htons(user_class_len)};

	// Prepare Header
	size_t oro_len;
	void *oro = odhcp6c_get_state(STATE_ORO, &oro_len);
	struct {
		uint8_t type;
		uint8_t trid[3];
		uint16_t elapsed_type;
		uint16_t elapsed_len;
		uint16_t elapsed_value;
		uint16_t oro_type;
		uint16_t oro_len;
	} hdr = {
		type, {trid[0], trid[1], trid[2]},
		htons(DHCPV6_OPT_ELAPSED), htons(2),
			htons((ecs > 0xffff) ? 0xffff : ecs),
		htons(DHCPV6_OPT_ORO), htons(oro_len),
	};

	struct iovec iov[IOV_TOTAL] = {
		[IOV_HDR] = {&hdr, sizeof(hdr)},
		[IOV_ORO] = {oro, oro_len},
		[IOV_ORO_REFRESH] = {&oro_refresh, 0},
		[IOV_CL_ID] = {cl_id, cl_id_len},
		[IOV_SRV_ID] = {srv_id, srv_id_len},
		[IOV_VENDOR_CLASS_HDR] = {&vendor_class_hdr, vendor_class_len ? sizeof(vendor_class_hdr) : 0},
		[IOV_VENDOR_CLASS] = {vendor_class, vendor_class_len},
		[IOV_USER_CLASS_HDR] = {&user_class_hdr, user_class_len ? sizeof(user_class_hdr) : 0},
		[IOV_USER_CLASS] = {user_class, user_class_len},
		[IOV_RECONF_ACCEPT] = {&reconf_accept, sizeof(reconf_accept)},
		[IOV_FQDN] = {&fqdn, fqdn_len},
		[IOV_HDR_IA_NA] = {&hdr_ia_na, sizeof(hdr_ia_na)},
		[IOV_IA_NA] = {ia_na, ia_na_len},
		[IOV_IA_PD] = {ia_pd, ia_pd_len},
	};

	size_t cnt = IOV_TOTAL;
	if (type == DHCPV6_MSG_INFO_REQ) {
		cnt = 9;
		iov[IOV_ORO_REFRESH].iov_len = sizeof(oro_refresh);
		hdr.oro_len = htons(oro_len + sizeof(oro_refresh));
	} else if (!request_prefix)
		cnt = 13;

	// Disable IAs if not used
	if (type != DHCPV6_MSG_SOLICIT && ia_na_len == 0)
		iov[IOV_HDR_IA_NA].iov_len = 0;

	if (na_mode == IA_MODE_NONE)
		iov[IOV_HDR_IA_NA].iov_len = 0;

	if ((type != DHCPV6_MSG_SOLICIT && type != DHCPV6_MSG_REQUEST) ||
			!(client_options & DHCPV6_ACCEPT_RECONFIGURE))
		iov[IOV_RECONF_ACCEPT].iov_len = 0;

	if (!(client_options & DHCPV6_CLIENT_FQDN))
		iov[IOV_FQDN].iov_len = 0;

	struct sockaddr_in6 srv = {AF_INET6, htons(DHCPV6_SERVER_PORT),
		0, ALL_DHCPV6_RELAYS, ifindex};
	struct msghdr msg = {.msg_name = &srv, .msg_namelen = sizeof(srv),
			.msg_iov = iov, .msg_iovlen = cnt};

	switch (type) {
	case DHCPV6_MSG_REQUEST:
	case DHCPV6_MSG_RENEW:
	case DHCPV6_MSG_RELEASE:
	case DHCPV6_MSG_DECLINE:
		if (!IN6_IS_ADDR_UNSPECIFIED(&server_addr) &&
			odhcp6c_addr_in_scope(&server_addr)) {
			srv.sin6_addr = server_addr;
			if (!IN6_IS_ADDR_LINKLOCAL(&server_addr))
				srv.sin6_scope_id = 0;
		}
		break;
	default:
		break;
	}

	if (sendmsg(sock, &msg, 0) < 0) {
		char in6_str[INET6_ADDRSTRLEN];

		syslog(LOG_ERR, "Failed to send DHCPV6 message to %s (%s)",
			inet_ntop(AF_INET6, (const void *)&srv.sin6_addr,
				in6_str, sizeof(in6_str)), strerror(errno));
	}
}

static int64_t dhcpv6_rand_delay(int64_t time)
{
	int random;
	odhcp6c_random(&random, sizeof(random));

	return (time * ((int64_t)random % 1000LL)) / 10000LL;
}

int dhcpv6_request(enum dhcpv6_msg type)
{
	uint8_t rc = 0;
	uint64_t timeout = UINT32_MAX;
	struct dhcpv6_retx *retx = &dhcpv6_retx[type];

	if (retx->delay) {
		struct timespec ts = {0, 0};
		ts.tv_nsec = (dhcpv6_rand_delay((10000 * DHCPV6_REQ_DELAY) / 2) + (1000 * DHCPV6_REQ_DELAY) / 2) * 1000000;

		while (nanosleep(&ts, &ts) < 0 && errno == EINTR);
	}

	if (type == DHCPV6_MSG_UNKNOWN)
		timeout = t1;
	else if (type == DHCPV6_MSG_RENEW)
		timeout = (t2 > t1) ? t2 - t1 : ((t1 == UINT32_MAX) ? UINT32_MAX : 0);
	else if (type == DHCPV6_MSG_REBIND)
		timeout = (t3 > t2) ? t3 - t2 : ((t2 == UINT32_MAX) ? UINT32_MAX : 0);

	if (timeout == 0)
		return -1;

	syslog(LOG_NOTICE, "Starting %s transaction (timeout %"PRIu64"s, max rc %d)",
			retx->name, timeout, retx->max_rc);

	uint64_t start = odhcp6c_get_milli_time(), round_start = start, elapsed;

	// Generate transaction ID
	uint8_t trid[3] = {0, 0, 0};
	if (type != DHCPV6_MSG_UNKNOWN)
		odhcp6c_random(trid, sizeof(trid));

	ssize_t len = -1;
	int64_t rto = 0;

	do {
		if (rto == 0) {
			int64_t delay = dhcpv6_rand_delay(retx->init_timeo * 1000);

			// First RT MUST be strictly greater than IRT for solicit messages (RFC3313 17.1.2)
			while (type == DHCPV6_MSG_SOLICIT && delay <= 0)
				delay = dhcpv6_rand_delay(retx->init_timeo * 1000);

			rto = (retx->init_timeo * 1000 + delay);
		} else
			rto = (2 * rto + dhcpv6_rand_delay(rto));

		if (retx->max_timeo && (rto >= retx->max_timeo * 1000))
			rto = retx->max_timeo * 1000 +
				dhcpv6_rand_delay(retx->max_timeo * 1000);

		// Calculate end for this round and elapsed time
		uint64_t round_end = round_start + rto;
		elapsed = round_start - start;

		// Don't wait too long if timeout differs from infinite
		if ((timeout != UINT32_MAX) && (round_end - start > timeout * 1000))
			round_end = timeout * 1000 + start;

		// Built and send package
		switch (type) {
		case DHCPV6_MSG_UNKNOWN:
			break;
		default:
			syslog(LOG_NOTICE, "Send %s message (elapsed %"PRIu64"ms, rc %d)",
					retx->name, elapsed, rc);
			// Fall through
		case DHCPV6_MSG_SOLICIT:
		case DHCPV6_MSG_INFO_REQ:
			dhcpv6_send(type, trid, elapsed / 10);
			rc++;
		}

		// Receive rounds
		for (; len < 0 && (round_start < round_end);
				round_start = odhcp6c_get_milli_time()) {
			uint8_t buf[1536];
			union {
				struct cmsghdr hdr;
				uint8_t buf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
			} cmsg_buf;
			struct iovec iov = {buf, sizeof(buf)};
			struct sockaddr_in6 addr;
			struct msghdr msg = {.msg_name = &addr, .msg_namelen = sizeof(addr),
					.msg_iov = &iov, .msg_iovlen = 1, .msg_control = cmsg_buf.buf,
					.msg_controllen = sizeof(cmsg_buf)};
			struct in6_pktinfo *pktinfo = NULL;

			// Check for pending signal
			if (odhcp6c_signal_process())
				return -1;

			// Set timeout for receiving
			uint64_t t = round_end - round_start;
			struct timeval tv = {t / 1000, (t % 1000) * 1000};
			setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
					&tv, sizeof(tv));

			// Receive cycle
			len = recvmsg(sock, &msg, 0);
			if (len < 0)
				continue;

			for (struct cmsghdr *ch = CMSG_FIRSTHDR(&msg); ch != NULL;
				ch = CMSG_NXTHDR(&msg, ch)) {
				if (ch->cmsg_level == SOL_IPV6 &&
					ch->cmsg_type == IPV6_PKTINFO) {
					pktinfo = (struct in6_pktinfo *)CMSG_DATA(ch);
					break;
				}
			}

			if (pktinfo == NULL) {
				len = -1;
				continue;
			}

			if (!dhcpv6_response_is_valid(buf, len, trid,
							type, &pktinfo->ipi6_addr)) {
				len = -1;
				continue;
			}

			uint8_t *opt = &buf[4];
			uint8_t *opt_end = opt + len - 4;

			round_start = odhcp6c_get_milli_time();
			elapsed = round_start - start;
			syslog(LOG_NOTICE, "Got a valid reply after %"PRIu64"ms",
					elapsed);

			if (retx->handler_reply)
				len = retx->handler_reply(type, rc, opt, opt_end, &addr);

			if (len > 0 && round_end - round_start > 1000)
				round_end = 1000 + round_start;
		}

		// Allow
		if (retx->handler_finish)
			len = retx->handler_finish();
	} while (len < 0 && ((timeout == UINT32_MAX) || (elapsed / 1000 < timeout)) &&
			(!retx->max_rc || rc < retx->max_rc));
	return len;
}

// Message validation checks according to RFC3315 chapter 15
static bool dhcpv6_response_is_valid(const void *buf, ssize_t len,
		const uint8_t transaction[3], enum dhcpv6_msg type,
		const struct in6_addr *daddr)
{
	const struct dhcpv6_header *rep = buf;
	if (len < (ssize_t)sizeof(*rep) || memcmp(rep->tr_id,
			transaction, sizeof(rep->tr_id)))
		return false; // Invalid reply

	if (type == DHCPV6_MSG_SOLICIT) {
		if (rep->msg_type != DHCPV6_MSG_ADVERT &&
				rep->msg_type != DHCPV6_MSG_REPLY)
			return false;

	} else if (type == DHCPV6_MSG_UNKNOWN) {
		if (!accept_reconfig || rep->msg_type != DHCPV6_MSG_RECONF)
			return false;

	} else if (rep->msg_type != DHCPV6_MSG_REPLY)
		return false;

	uint8_t *end = ((uint8_t*)buf) + len, *odata = NULL,
		rcmsg = DHCPV6_MSG_UNKNOWN;
	uint16_t otype, olen = UINT16_MAX;
	bool clientid_ok = false, serverid_ok = false, rcauth_ok = false,
		ia_present = false, options_valid = true;

	size_t client_id_len, server_id_len;
	void *client_id = odhcp6c_get_state(STATE_CLIENT_ID, &client_id_len);
	void *server_id = odhcp6c_get_state(STATE_SERVER_ID, &server_id_len);

	dhcpv6_for_each_option(&rep[1], end, otype, olen, odata) {
		if (otype == DHCPV6_OPT_CLIENTID) {
			clientid_ok = (olen + 4U == client_id_len) && !memcmp(
					&odata[-4], client_id, client_id_len);
		} else if (otype == DHCPV6_OPT_SERVERID) {
			if (server_id_len)
				serverid_ok = (olen + 4U == server_id_len) && !memcmp(
						&odata[-4], server_id, server_id_len);
			else
				serverid_ok = true;
		} else if (otype == DHCPV6_OPT_AUTH && olen == -4 +
				sizeof(struct dhcpv6_auth_reconfigure)) {
			struct dhcpv6_auth_reconfigure *r = (void*)&odata[-4];
			if (r->protocol != 3 || r->algorithm != 1 || r->reconf_type != 2)
				continue;

			md5_ctx_t md5;
			uint8_t serverhash[16], secretbytes[64];
			uint32_t hash[4];
			memcpy(serverhash, r->key, sizeof(serverhash));
			memset(r->key, 0, sizeof(r->key));

			memset(secretbytes, 0, sizeof(secretbytes));
			memcpy(secretbytes, reconf_key, sizeof(reconf_key));

			for (size_t i = 0; i < sizeof(secretbytes); ++i)
				secretbytes[i] ^= 0x36;

			md5_begin(&md5);
			md5_hash(secretbytes, sizeof(secretbytes), &md5);
			md5_hash(buf, len, &md5);
			md5_end(hash, &md5);

			for (size_t i = 0; i < sizeof(secretbytes); ++i) {
				secretbytes[i] ^= 0x36;
				secretbytes[i] ^= 0x5c;
			}

			md5_begin(&md5);
			md5_hash(secretbytes, sizeof(secretbytes), &md5);
			md5_hash(hash, 16, &md5);
			md5_end(hash, &md5);

			rcauth_ok = !memcmp(hash, serverhash, sizeof(hash));
		} else if (otype == DHCPV6_OPT_RECONF_MESSAGE && olen == 1) {
			rcmsg = odata[0];
		} else if ((otype == DHCPV6_OPT_IA_PD || otype == DHCPV6_OPT_IA_NA)) {
			ia_present = true;
			if (olen < -4 + sizeof(struct dhcpv6_ia_hdr))
				options_valid = false;
		} else if ((otype == DHCPV6_OPT_IA_ADDR) || (otype == DHCPV6_OPT_IA_PREFIX) ||
				(otype == DHCPV6_OPT_PD_EXCLUDE))
			// Options are not allowed on global level
			options_valid = false;
	}

	if (!options_valid || ((odata + olen) > end))
		return false;

	if (type == DHCPV6_MSG_INFO_REQ && ia_present)
		return false;

	if (rep->msg_type == DHCPV6_MSG_RECONF) {
		if ((rcmsg != DHCPV6_MSG_RENEW && rcmsg != DHCPV6_MSG_REBIND && rcmsg != DHCPV6_MSG_INFO_REQ) ||
			(rcmsg == DHCPV6_MSG_INFO_REQ && ia_present) ||
			!rcauth_ok || IN6_IS_ADDR_MULTICAST(daddr))
			return false;
	}

	return clientid_ok && serverid_ok;
}

int dhcpv6_poll_reconfigure(void)
{
	int ret = dhcpv6_request(DHCPV6_MSG_UNKNOWN);

	if (ret != -1)
		ret = dhcpv6_request(ret);

	return ret;
}

static int dhcpv6_handle_reconfigure(enum dhcpv6_msg orig, const int rc,
		const void *opt, const void *end, _unused const struct sockaddr_in6 *from)
{
	uint16_t otype, olen;
	uint8_t *odata;
	int msg = -1;

	dhcpv6_for_each_option(opt, end, otype, olen, odata) {
		if (otype == DHCPV6_OPT_RECONF_MESSAGE && olen == 1) {
			switch (odata[0]) {
			case DHCPV6_MSG_REBIND:
				if (t2 != UINT32_MAX)
					t2 = 0;
			// Fall through
			case DHCPV6_MSG_RENEW:
				if (t1 != UINT32_MAX)
					t1 = 0;
			// Fall through
			case DHCPV6_MSG_INFO_REQ:
				msg = odata[0];
				break;

			default:
				break;
			}
		}
	}

	dhcpv6_handle_reply(orig, rc, NULL, NULL, NULL);

	return msg;
}

// Collect all advertised servers
static int dhcpv6_handle_advert(enum dhcpv6_msg orig, const int rc,
		const void *opt, const void *end, _unused const struct sockaddr_in6 *from)
{
	uint16_t olen, otype;
	uint8_t *odata, pref = 0;
	uint16_t code = DHCPV6_Success;
	struct dhcpv6_server_cand cand = {false, false, 0, 0, {0},
					IN6ADDR_ANY_INIT, DHCPV6_SOL_MAX_RT,
					DHCPV6_INF_MAX_RT, NULL, NULL, 0, 0};
	bool allowed = true;
	bool have_na = false;
	int have_pd = 0;

	dhcpv6_for_each_option(opt, end, otype, olen, odata) {
		if (orig == DHCPV6_MSG_SOLICIT &&
				(otype == DHCPV6_OPT_IA_PD || otype == DHCPV6_OPT_IA_NA) &&
				olen > -4 + sizeof(struct dhcpv6_ia_hdr)) {
			struct dhcpv6_ia_hdr *ia_hdr = (void*)(&odata[-4]);
			dhcpv6_parse_ia(ia_hdr, odata + olen + sizeof(*ia_hdr));
		}

		if (otype == DHCPV6_OPT_SERVERID && olen <= 130) {
			memcpy(cand.duid, odata, olen);
			cand.duid_len = olen;
		} else if (otype == DHCPV6_OPT_PREF && olen >= 1 &&
				cand.preference >= 0) {
			cand.preference = pref = odata[0];
		} else if (otype == DHCPV6_OPT_UNICAST && olen == sizeof(cand.server_addr)) {
			cand.server_addr = *(struct in6_addr *)odata;
		} else if (otype == DHCPV6_OPT_RECONF_ACCEPT) {
			cand.wants_reconfigure = true;
		} else if (otype == DHCPV6_OPT_SOL_MAX_RT && olen == 4) {
			uint32_t sol_max_rt = ntohl_unaligned(odata);
			if (sol_max_rt >= DHCPV6_SOL_MAX_RT_MIN &&
					sol_max_rt <= DHCPV6_SOL_MAX_RT_MAX)
				cand.sol_max_rt = sol_max_rt;

		} else if (otype == DHCPV6_OPT_INF_MAX_RT && olen == 4) {
			uint32_t inf_max_rt = ntohl_unaligned(odata);
			if (inf_max_rt >= DHCPV6_INF_MAX_RT_MIN &&
					inf_max_rt <= DHCPV6_INF_MAX_RT_MAX)
				cand.inf_max_rt = inf_max_rt;

		} else if (otype == DHCPV6_OPT_IA_PD && request_prefix &&
					olen >= -4 + sizeof(struct dhcpv6_ia_hdr)) {
			struct dhcpv6_ia_hdr *h = (struct dhcpv6_ia_hdr*)&odata[-4];
			uint8_t *oend = odata + olen, *d;
			dhcpv6_for_each_option(&h[1], oend, otype, olen, d) {
				if (otype == DHCPV6_OPT_IA_PREFIX &&
						olen >= -4 + sizeof(struct dhcpv6_ia_prefix)) {
					struct dhcpv6_ia_prefix *p = (struct dhcpv6_ia_prefix*)&d[-4];
					have_pd = p->prefix;
				}
			}
		} else if (otype == DHCPV6_OPT_IA_NA &&
					olen >= -4 + sizeof(struct dhcpv6_ia_hdr)) {
			struct dhcpv6_ia_hdr *h = (struct dhcpv6_ia_hdr*)&odata[-4];
			uint8_t *oend = odata + olen, *d;

			dhcpv6_for_each_option(&h[1], oend, otype, olen, d) {
				if (otype == DHCPV6_OPT_IA_ADDR &&
						olen >= -4 + sizeof(struct dhcpv6_ia_addr))
					have_na = true;
			}
		} else if (otype == DHCPV6_OPT_STATUS && olen >= 2)
			code = ((int)odata[0]) << 8 | ((int)odata[1]);
	}

	if (!have_na && na_mode == IA_MODE_FORCE)
		allowed = false;
	else if (!have_pd && pd_mode == IA_MODE_FORCE)
		allowed = !have_na && na_mode == IA_MODE_TRY && code == DHCPV6_NoAddrsAvail;
	if (!allowed) {
		/*
		 * RFC7083 states to process the SOL_MAX_RT and
		 * INF_MAX_RT options even if the DHCPv6 server
		 * did not propose any IA_NA and/or IA_PD
		 */
		dhcpv6_retx[DHCPV6_MSG_SOLICIT].max_timeo = cand.sol_max_rt;
		dhcpv6_retx[DHCPV6_MSG_INFO_REQ].max_timeo = cand.inf_max_rt;
		return -1;
	}

	if (na_mode != IA_MODE_NONE && !have_na) {
		cand.has_noaddravail = true;
		cand.preference -= 1000;
	}

	if (pd_mode != IA_MODE_NONE) {
		if (have_pd)
			cand.preference += 2000 + (128 - have_pd);
		else
			cand.preference -= 2000;
	}

	if (cand.duid_len > 0) {
		cand.ia_na = odhcp6c_move_state(STATE_IA_NA, &cand.ia_na_len);
		cand.ia_pd = odhcp6c_move_state(STATE_IA_PD, &cand.ia_pd_len);
		dhcpv6_add_server_cand(&cand);
	}

	return (rc > 1 || (pref == 255 && cand.preference > 0)) ? 1 : -1;
}

static int dhcpv6_commit_advert(void)
{
	return dhcpv6_promote_server_cand();
}

static int dhcpv6_handle_rebind_reply(enum dhcpv6_msg orig, const int rc,
		const void *opt, const void *end, const struct sockaddr_in6 *from)
{
	dhcpv6_handle_advert(orig, rc, opt, end, from);
	if (dhcpv6_commit_advert() < 0)
		return -1;

	return dhcpv6_handle_reply(orig, rc, opt, end, from);
}

static int dhcpv6_handle_reply(enum dhcpv6_msg orig, _unused const int rc,
		const void *opt, const void *end, const struct sockaddr_in6 *from)
{
	uint8_t *odata;
	uint16_t otype, olen;
	uint32_t refresh = 86400;
	int ret = 1;
	bool handled_status_codes[_DHCPV6_Status_Max] = { false, };

	odhcp6c_expire();

	if (orig == DHCPV6_MSG_UNKNOWN) {
		static time_t last_update = 0;
		time_t now = odhcp6c_get_milli_time() / 1000;

		uint32_t elapsed = (last_update > 0) ? now - last_update : 0;
		last_update = now;

		if (t1 != UINT32_MAX)
			t1 -= elapsed;

		if (t2 != UINT32_MAX)
			t2 -= elapsed;

		if (t3 != UINT32_MAX)
			t3 -= elapsed;

		if (t1 < 0)
			t1 = 0;

		if (t2 < 0)
			t2 = 0;

		if (t3 < 0)
			t3 = 0;
	}

	if (orig == DHCPV6_MSG_REQUEST && !odhcp6c_is_bound()) {
		// Delete NA and PD we have in the state from the Advert
		odhcp6c_clear_state(STATE_IA_NA);
		odhcp6c_clear_state(STATE_IA_PD);
	}

	if (opt) {
		odhcp6c_clear_state(STATE_DNS);
		odhcp6c_clear_state(STATE_SEARCH);
		odhcp6c_clear_state(STATE_SNTP_IP);
		odhcp6c_clear_state(STATE_NTP_IP);
		odhcp6c_clear_state(STATE_NTP_FQDN);
		odhcp6c_clear_state(STATE_SIP_IP);
		odhcp6c_clear_state(STATE_SIP_FQDN);
		odhcp6c_clear_state(STATE_AFTR_NAME);
		odhcp6c_clear_state(STATE_CER);
		odhcp6c_clear_state(STATE_S46_MAPT);
		odhcp6c_clear_state(STATE_S46_MAPE);
		odhcp6c_clear_state(STATE_S46_LW);
		odhcp6c_clear_state(STATE_PASSTHRU);
		odhcp6c_clear_state(STATE_CUSTOM_OPTS);

		// Parse and find all matching IAs
		dhcpv6_for_each_option(opt, end, otype, olen, odata) {
			bool passthru = true;

			if ((otype == DHCPV6_OPT_IA_PD || otype == DHCPV6_OPT_IA_NA)
					&& olen > -4 + sizeof(struct dhcpv6_ia_hdr)) {
				struct dhcpv6_ia_hdr *ia_hdr = (void*)(&odata[-4]);

				if ((na_mode == IA_MODE_NONE && otype == DHCPV6_OPT_IA_NA) ||
					(pd_mode == IA_MODE_NONE && otype == DHCPV6_OPT_IA_PD))
					continue;

				// Test ID
				if (ia_hdr->iaid != htonl(1) && otype == DHCPV6_OPT_IA_NA)
					continue;

				uint16_t code = DHCPV6_Success;
				uint16_t stype, slen;
				uint8_t *sdata;
				// Get and handle status code
				dhcpv6_for_each_option(&ia_hdr[1], odata + olen,
						stype, slen, sdata) {
					if (stype == DHCPV6_OPT_STATUS && slen >= 2) {
						uint8_t *mdata = (slen > 2) ? &sdata[2] : NULL;
						uint16_t mlen = (slen > 2) ? slen - 2 : 0;

						code = ((int)sdata[0]) << 8 | ((int)sdata[1]);

						if (code == DHCPV6_Success)
							continue;

						dhcpv6_handle_ia_status_code(orig, ia_hdr,
							code, mdata, mlen, handled_status_codes, &ret);

						if (ret > 0)
							return ret;

						break;
					}
				}

				if (code != DHCPV6_Success)
					continue;

				dhcpv6_parse_ia(ia_hdr, odata + olen);
				passthru = false;
			} else if (otype == DHCPV6_OPT_UNICAST && olen == sizeof(server_addr)) {
				server_addr = *(struct in6_addr *)odata;
				passthru = false;
			} else if (otype == DHCPV6_OPT_STATUS && olen >= 2) {
				uint8_t *mdata = (olen > 2) ? &odata[2] : NULL;
				uint16_t mlen = (olen > 2) ? olen - 2 : 0;
				uint16_t code = ((int)odata[0]) << 8 | ((int)odata[1]);

				dhcpv6_handle_status_code(orig, code, mdata, mlen, &ret);
				passthru = false;
			} else if (otype == DHCPV6_OPT_DNS_SERVERS) {
				if (olen % 16 == 0)
					odhcp6c_add_state(STATE_DNS, odata, olen);
			} else if (otype == DHCPV6_OPT_DNS_DOMAIN)
				odhcp6c_add_state(STATE_SEARCH, odata, olen);
			else if (otype == DHCPV6_OPT_SNTP_SERVERS) {
				if (olen % 16 == 0)
					odhcp6c_add_state(STATE_SNTP_IP, odata, olen);
			} else if (otype == DHCPV6_OPT_NTP_SERVER) {
				uint16_t stype, slen;
				uint8_t *sdata;
				// Test status and bail if error
				dhcpv6_for_each_option(odata, odata + olen,
						stype, slen, sdata) {
					if (slen == 16 && (stype == NTP_MC_ADDR ||
							stype == NTP_SRV_ADDR))
						odhcp6c_add_state(STATE_NTP_IP,
								sdata, slen);
					else if (slen > 0 && stype == NTP_SRV_FQDN)
						odhcp6c_add_state(STATE_NTP_FQDN,
								sdata, slen);
				}
			} else if (otype == DHCPV6_OPT_SIP_SERVER_A) {
				if (olen == 16)
					odhcp6c_add_state(STATE_SIP_IP, odata, olen);
			} else if (otype == DHCPV6_OPT_SIP_SERVER_D)
				odhcp6c_add_state(STATE_SIP_FQDN, odata, olen);
			else if (otype == DHCPV6_OPT_INFO_REFRESH && olen >= 4) {
				refresh = ntohl_unaligned(odata);
				passthru = false;
			} else if (otype == DHCPV6_OPT_AUTH) {
				if (olen == -4 + sizeof(struct dhcpv6_auth_reconfigure)) {
					struct dhcpv6_auth_reconfigure *r = (void*)&odata[-4];
					if (r->protocol == 3 && r->algorithm == 1 &&
							r->reconf_type == 1)
						memcpy(reconf_key, r->key, sizeof(r->key));
				}
				passthru = false;
			} else if (otype == DHCPV6_OPT_AFTR_NAME && olen > 3) {
				size_t cur_len;
				odhcp6c_get_state(STATE_AFTR_NAME, &cur_len);
				if (cur_len == 0)
					odhcp6c_add_state(STATE_AFTR_NAME, odata, olen);
				passthru = false;
			} else if (otype == DHCPV6_OPT_SOL_MAX_RT && olen == 4) {
				uint32_t sol_max_rt = ntohl_unaligned(odata);
				if (sol_max_rt >= DHCPV6_SOL_MAX_RT_MIN &&
						sol_max_rt <= DHCPV6_SOL_MAX_RT_MAX)
					dhcpv6_retx[DHCPV6_MSG_SOLICIT].max_timeo = sol_max_rt;
				passthru = false;
			} else if (otype == DHCPV6_OPT_INF_MAX_RT && olen == 4) {
				uint32_t inf_max_rt = ntohl_unaligned(odata);
				if (inf_max_rt >= DHCPV6_INF_MAX_RT_MIN &&
						inf_max_rt <= DHCPV6_INF_MAX_RT_MAX)
					dhcpv6_retx[DHCPV6_MSG_INFO_REQ].max_timeo = inf_max_rt;
				passthru = false;
	#ifdef EXT_CER_ID
			} else if (otype == DHCPV6_OPT_CER_ID && olen == -4 +
					sizeof(struct dhcpv6_cer_id)) {
				struct dhcpv6_cer_id *cer_id = (void*)&odata[-4];
				struct in6_addr any = IN6ADDR_ANY_INIT;
				if (memcmp(&cer_id->addr, &any, sizeof(any)))
					odhcp6c_add_state(STATE_CER, &cer_id->addr, sizeof(any));
				passthru = false;
	#endif
			} else if (otype == DHCPV6_OPT_S46_CONT_MAPT) {
				odhcp6c_add_state(STATE_S46_MAPT, odata, olen);
				passthru = false;
			} else if (otype == DHCPV6_OPT_S46_CONT_MAPE) {
				size_t mape_len;
				odhcp6c_get_state(STATE_S46_MAPE, &mape_len);
				if (mape_len == 0)
					odhcp6c_add_state(STATE_S46_MAPE, odata, olen);
				passthru = false;
			} else if (otype == DHCPV6_OPT_S46_CONT_LW) {
				odhcp6c_add_state(STATE_S46_LW, odata, olen);
				passthru = false;
			} else if (otype == DHCPV6_OPT_CLIENTID ||
					otype == DHCPV6_OPT_SERVERID ||
					otype == DHCPV6_OPT_IA_TA ||
					otype == DHCPV6_OPT_PREF ||
					otype == DHCPV6_OPT_UNICAST ||
					otype == DHCPV6_OPT_FQDN ||
					otype == DHCPV6_OPT_RECONF_ACCEPT)
				passthru = false;
			else
				odhcp6c_add_state(STATE_CUSTOM_OPTS, &odata[-4], olen + 4);

			if (passthru)
				odhcp6c_add_state(STATE_PASSTHRU, &odata[-4], olen + 4);
		}
	}

	switch (orig) {
	case DHCPV6_MSG_REQUEST:
	case DHCPV6_MSG_REBIND:
	case DHCPV6_MSG_RENEW:
		// Update refresh timers if no fatal status code was received
		if ((ret > 0) && (ret = dhcpv6_calc_refresh_timers())) {
			if (orig == DHCPV6_MSG_REQUEST) {
				// All server candidates can be cleared if not yet bound
				if (!odhcp6c_is_bound())
					dhcpv6_clear_all_server_cand();

				odhcp6c_clear_state(STATE_SERVER_ADDR);
				odhcp6c_add_state(STATE_SERVER_ADDR, &from->sin6_addr, 16);
			} else if (orig == DHCPV6_MSG_RENEW) {
				// Send further renews if T1 is not set
				if (!t1)
					ret = -1;

			} else if (orig == DHCPV6_MSG_REBIND) {
				// Send further rebinds if T1 and T2 is not set
				if (!t1 && !t2)
					ret = -1;

				odhcp6c_clear_state(STATE_SERVER_ADDR);
				odhcp6c_add_state(STATE_SERVER_ADDR, &from->sin6_addr, 16);
			}
		}
		break;

	case DHCPV6_MSG_INFO_REQ:
		if (ret > 0) {
			// All server candidates can be cleared if not yet bound
			if (!odhcp6c_is_bound())
				dhcpv6_clear_all_server_cand();

			t1 = refresh;
		}
		break;

	default:
		break;
	}

	return ret;
}

static int dhcpv6_parse_ia(void *opt, void *end)
{
	struct dhcpv6_ia_hdr *ia_hdr = (struct dhcpv6_ia_hdr *)opt;
	int parsed_ia = 0;
	uint32_t t1, t2;
	uint16_t otype, olen;
	uint8_t *odata;

	t1 = ntohl(ia_hdr->t1);
	t2 = ntohl(ia_hdr->t2);

	if (t1 > t2)
		return 0;

	// Update address IA
	dhcpv6_for_each_option(&ia_hdr[1], end, otype, olen, odata) {
		struct odhcp6c_entry entry = {IN6ADDR_ANY_INIT, 0, 0, 0,
				IN6ADDR_ANY_INIT, 0, 0, 0, 0, 0};

		entry.iaid = ia_hdr->iaid;

		if (otype == DHCPV6_OPT_IA_PREFIX) {
			struct dhcpv6_ia_prefix *prefix = (void*)&odata[-4];
			if (olen + 4U < sizeof(*prefix))
				continue;

			entry.valid = ntohl(prefix->valid);
			entry.preferred = ntohl(prefix->preferred);

			if (entry.preferred > entry.valid)
				continue;

			entry.t1 = (t1 ? t1 : (entry.preferred != UINT32_MAX ? 0.5 * entry.preferred : UINT32_MAX));
			entry.t2 = (t2 ? t2 : (entry.preferred != UINT32_MAX ? 0.8 * entry.preferred : UINT32_MAX));
			if (entry.t1 > entry.t2)
				entry.t1 = entry.t2;

			entry.length = prefix->prefix;
			entry.target = prefix->addr;
			uint16_t stype, slen;
			uint8_t *sdata;

			// Parse PD-exclude
			bool ok = true;
			dhcpv6_for_each_option(odata + sizeof(*prefix) - 4U,
					odata + olen, stype, slen, sdata) {
				if (stype != DHCPV6_OPT_PD_EXCLUDE || slen < 2)
					continue;

				uint8_t elen = sdata[0];
				if (elen > 64)
					elen = 64;

				if (entry.length < 32 || elen <= entry.length) {
					ok = false;
					continue;
				}

				uint8_t bytes = ((elen - entry.length - 1) / 8) + 1;
				if (slen <= bytes) {
					ok = false;
					continue;
				}

				uint32_t exclude = 0;
				do {
					exclude = exclude << 8 | sdata[bytes];
				} while (--bytes);

				exclude >>= 8 - ((elen - entry.length) % 8);
				exclude <<= 64 - elen;

				// Abusing router & priority fields for exclusion
				entry.router = entry.target;
				entry.router.s6_addr32[1] |= htonl(exclude);
				entry.priority = elen;
			}

			if (ok) {
				odhcp6c_update_entry(STATE_IA_PD, &entry, 0, 0);
				parsed_ia++;
			}

			entry.priority = 0;
			memset(&entry.router, 0, sizeof(entry.router));
		} else if (otype == DHCPV6_OPT_IA_ADDR) {
			struct dhcpv6_ia_addr *addr = (void*)&odata[-4];
			if (olen + 4U < sizeof(*addr))
				continue;

			entry.preferred = ntohl(addr->preferred);
			entry.valid = ntohl(addr->valid);

			if (entry.preferred > entry.valid)
				continue;

			entry.t1 = (t1 ? t1 : (entry.preferred != UINT32_MAX ? 0.5 * entry.preferred : UINT32_MAX));
			entry.t2 = (t2 ? t2 : (entry.preferred != UINT32_MAX ? 0.8 * entry.preferred : UINT32_MAX));
			if (entry.t1 > entry.t2)
				entry.t1 = entry.t2;

			entry.length = 128;
			entry.target = addr->addr;

			odhcp6c_update_entry(STATE_IA_NA, &entry, 0, 0);
			parsed_ia++;
		}
	}
	return parsed_ia;
}

static int dhcpv6_calc_refresh_timers(void)
{
	struct odhcp6c_entry *e;
	size_t ia_na_entries, ia_pd_entries, i;
	int64_t l_t1 = UINT32_MAX, l_t2 = UINT32_MAX, l_t3 = 0;

	e = odhcp6c_get_state(STATE_IA_NA, &ia_na_entries);
	ia_na_entries /= sizeof(*e);

	for (i = 0; i < ia_na_entries; i++) {
		if (e[i].t1 < l_t1)
			l_t1 = e[i].t1;

		if (e[i].t2 < l_t2)
			l_t2 = e[i].t2;

		if (e[i].valid > l_t3)
			l_t3 = e[i].valid;
	}

	e = odhcp6c_get_state(STATE_IA_PD, &ia_pd_entries);
	ia_pd_entries /= sizeof(*e);

	for (i = 0; i < ia_pd_entries; i++) {
		if (e[i].t1 < l_t1)
			l_t1 = e[i].t1;

		if (e[i].t2 < l_t2)
			l_t2 = e[i].t2;

		if (e[i].valid > l_t3)
			l_t3 = e[i].valid;
	}

	if (ia_pd_entries || ia_na_entries) {
		t1 = l_t1;
		t2 = l_t2;
		t3 = l_t3;
	}

	return (int)(ia_pd_entries + ia_na_entries);
}

static void dhcpv6_log_status_code(const uint16_t code, const char *scope,
		const void *status_msg, int len)
{
	const char *src = status_msg;
	char buf[len + 3];
	char *dst = buf;

	if (len) {
		*dst++ = '(';
		while (len--) {
			*dst = isprint((unsigned char)*src) ? *src : '?';
			src++;
			dst++;
		}
		*dst++ = ')';
	}

	*dst = 0;

	syslog(LOG_WARNING, "Server returned %s status %i %s",
		scope, code, buf);
}

static void dhcpv6_handle_status_code(const enum dhcpv6_msg orig,
		const uint16_t code, const void *status_msg, const int len,
		int *ret)
{
	dhcpv6_log_status_code(code, "message", status_msg, len);

	switch (code) {
	case DHCPV6_UnspecFail:
		// Generic failure
		*ret = 0;
		break;

	case DHCPV6_UseMulticast:
		switch(orig) {
		case DHCPV6_MSG_REQUEST:
		case DHCPV6_MSG_RENEW:
		case DHCPV6_MSG_RELEASE:
		case DHCPV6_MSG_DECLINE:
			// Message needs to be retransmitted according to RFC3315 chapter 18.1.8
			server_addr = in6addr_any;
			*ret = 0;
			break;
		default:
			break;
		}
		break;

	case DHCPV6_NoAddrsAvail:
	case DHCPV6_NoPrefixAvail:
		if (orig == DHCPV6_MSG_REQUEST)
			*ret = 0; // Failure
		break;

	default:
		break;
	}
}

static void dhcpv6_handle_ia_status_code(const enum dhcpv6_msg orig,
		const struct dhcpv6_ia_hdr *ia_hdr, const uint16_t code,
		const void *status_msg, const int len,
		bool handled_status_codes[_DHCPV6_Status_Max], int *ret)
{
	dhcpv6_log_status_code(code, ia_hdr->type == DHCPV6_OPT_IA_NA ?
		"IA_NA" : "IA_PD", status_msg, len);

	switch (code) {
	case DHCPV6_NoBinding:
		switch (orig) {
		case DHCPV6_MSG_RENEW:
		case DHCPV6_MSG_REBIND:
			if ((*ret > 0) && !handled_status_codes[code])
				*ret = dhcpv6_request(DHCPV6_MSG_REQUEST);
			break;

		default:
			break;
		}
		break;

	default:
		*ret = 0;
		break;
	}
}

// Note this always takes ownership of cand->ia_na and cand->ia_pd
static void dhcpv6_add_server_cand(const struct dhcpv6_server_cand *cand)
{
	size_t cand_len, i;
	struct dhcpv6_server_cand *c = odhcp6c_get_state(STATE_SERVER_CAND, &cand_len);

	// Remove identical duid server candidate
	for (i = 0; i < cand_len / sizeof(*c); ++i) {
		if (cand->duid_len == c[i].duid_len &&
				!memcmp(cand->duid, c[i].duid, cand->duid_len)) {
			free(c[i].ia_na);
			free(c[i].ia_pd);
			odhcp6c_remove_state(STATE_SERVER_CAND, i * sizeof(*c), sizeof(*c));
			break;
		}
	}

	for (i = 0, c = odhcp6c_get_state(STATE_SERVER_CAND, &cand_len);
		i < cand_len / sizeof(*c); ++i) {
		if (c[i].preference < cand->preference)
			break;
	}

	if (odhcp6c_insert_state(STATE_SERVER_CAND, i * sizeof(*c), cand, sizeof(*cand))) {
		free(cand->ia_na);
		free(cand->ia_pd);
	}
}

static void dhcpv6_clear_all_server_cand(void)
{
	size_t cand_len, i;
	struct dhcpv6_server_cand *c = odhcp6c_get_state(STATE_SERVER_CAND, &cand_len);

	// Server candidates need deep delete for IA_NA/IA_PD
	for (i = 0; i < cand_len / sizeof(*c); ++i) {
		free(c[i].ia_na);
		free(c[i].ia_pd);
	}
	odhcp6c_clear_state(STATE_SERVER_CAND);
}

int dhcpv6_promote_server_cand(void)
{
	size_t cand_len;
	struct dhcpv6_server_cand *cand = odhcp6c_get_state(STATE_SERVER_CAND, &cand_len);
	uint16_t hdr[2];
	int ret = DHCPV6_STATELESS;

	// Clear lingering candidate state info
	odhcp6c_clear_state(STATE_SERVER_ID);
	odhcp6c_clear_state(STATE_IA_NA);
	odhcp6c_clear_state(STATE_IA_PD);

	if (!cand_len)
		return -1;

	if (cand->has_noaddravail && na_mode == IA_MODE_TRY) {
		na_mode = IA_MODE_NONE;

		dhcpv6_retx[DHCPV6_MSG_SOLICIT].max_timeo = cand->sol_max_rt;
		dhcpv6_retx[DHCPV6_MSG_INFO_REQ].max_timeo = cand->inf_max_rt;

		return dhcpv6_request(DHCPV6_MSG_SOLICIT);
	}

	hdr[0] = htons(DHCPV6_OPT_SERVERID);
	hdr[1] = htons(cand->duid_len);
	odhcp6c_add_state(STATE_SERVER_ID, hdr, sizeof(hdr));
	odhcp6c_add_state(STATE_SERVER_ID, cand->duid, cand->duid_len);
	accept_reconfig = cand->wants_reconfigure;

	if (cand->ia_na_len) {
		odhcp6c_add_state(STATE_IA_NA, cand->ia_na, cand->ia_na_len);
		free(cand->ia_na);
		if (na_mode != IA_MODE_NONE)
			ret = DHCPV6_STATEFUL;
	}

	if (cand->ia_pd_len) {
		odhcp6c_add_state(STATE_IA_PD, cand->ia_pd, cand->ia_pd_len);
		free(cand->ia_pd);
		if (request_prefix)
			ret = DHCPV6_STATEFUL;
	}

	dhcpv6_retx[DHCPV6_MSG_SOLICIT].max_timeo = cand->sol_max_rt;
	dhcpv6_retx[DHCPV6_MSG_INFO_REQ].max_timeo = cand->inf_max_rt;

	odhcp6c_remove_state(STATE_SERVER_CAND, 0, sizeof(*cand));

	return ret;
}
