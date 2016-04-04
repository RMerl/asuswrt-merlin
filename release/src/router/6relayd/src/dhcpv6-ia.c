/**
 * Copyright (C) 2013 Steven Barth <steven@midlink.org>
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

#include "list.h"
#include "6relayd.h"
#include "dhcpv6.h"
#include "md5.h"

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <alloca.h>
#include <resolv.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>


struct assignment {
	struct list_head head;
	struct sockaddr_in6 peer;
	time_t valid_until;
	time_t reconf_sent;
	int reconf_cnt;
	char *hostname;
	uint8_t key[16];
	uint32_t assigned;
	uint32_t iaid;
	uint8_t length; // length == 128 -> IA_NA, length <= 64 -> IA_PD
	bool accept_reconf;
	uint8_t clid_len;
	uint8_t clid_data[];
};


static const struct relayd_config *config = NULL;
static void update(struct relayd_interface *iface);
static void reconf_timer(struct relayd_event *event);
static struct relayd_event reconf_event = {-1, reconf_timer, NULL};
static int socket_fd = -1;
static uint32_t serial = 0;



int dhcpv6_init_ia(const struct relayd_config *relayd_config, int dhcpv6_socket)
{
	config = relayd_config;
	socket_fd = dhcpv6_socket;

#if defined(TFD_CLOEXEC) && defined(TFD_NONBLOCK)
	reconf_event.socket = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
#else
	reconf_event.socket = timerfd_create(CLOCK_MONOTONIC, 0);
	reconf_event.socket = fflags(reconf_event.socket, O_CLOEXEC | O_NONBLOCK);
#endif
	if (reconf_event.socket < 0) {
		syslog(LOG_ERR, "Failed to create timer: %s", strerror(errno));
		return -1;
	}

	relayd_register_event(&reconf_event);

	struct itimerspec its = {{2, 0}, {2, 0}};
	timerfd_settime(reconf_event.socket, 0, &its, NULL);

	for (size_t i = 0; i < config->slavecount; ++i) {
		struct relayd_interface *iface = &config->slaves[i];

		INIT_LIST_HEAD(&iface->pd_assignments);
		struct assignment *border = calloc(1, sizeof(*border));
		border->length = 64;
		list_add(&border->head, &iface->pd_assignments);
	}

	for (size_t i = 0; i < config->slavecount; ++i)
		update(&config->slaves[i]);

	// Parse static entries
	for (size_t i = 0; i < config->dhcpv6_lease_len; ++i) {
		char *saveptr;
		char *duid = strtok_r(config->dhcpv6_lease[i], ":", &saveptr), *assign;
		size_t duidlen = (duid) ? strlen(duid) : 0;
		if (!duidlen || duidlen % 2 || !(assign = strtok_r(NULL, ":", &saveptr))) {
			syslog(LOG_ERR, "Invalid static lease %s", config->dhcpv6_lease[i]);
			return -1;
		}
		duidlen /= 2;

		// Construct entry
		struct assignment *a = calloc(1, sizeof(*a) + duidlen);
		a->clid_len = duidlen;
		a->length = 128;
		a->assigned = strtol(assign, NULL, 16);
		relayd_urandom(a->key, sizeof(a->key));

		for (size_t j = 0; j < duidlen; ++j) {
			char hexnum[3] = {duid[j * 2], duid[j * 2 + 1], 0};
			a->clid_data[j] = strtol(hexnum, NULL, 16);
		}

		// Assign to all interfaces
		struct assignment *c;
		for (size_t j = 0; j < config->slavecount; ++j) {
			struct relayd_interface *iface = &config->slaves[j];
			list_for_each_entry(c, &iface->pd_assignments, head) {
				if (c->length != 128 || c->assigned > a->assigned) {
					struct assignment *n = malloc(sizeof(*a) + duidlen);
					memcpy(n, a, sizeof(*a) + duidlen);
					list_add_tail(&n->head, &c->head);
				} else if (c->assigned == a->assigned) {
					// Already an assignment with that number
					break;
				}
			}
		}


		free(a);
	}

	return 0;
}


static time_t monotonic_time(void)
{
	struct timespec ts;
	syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts);
	return ts.tv_sec;
}


static int send_reconf(struct relayd_interface *iface, struct assignment *assign)
{
	struct {
		struct dhcpv6_client_header hdr;
		uint16_t srvid_type;
		uint16_t srvid_len;
		uint16_t duid_type;
		uint16_t hardware_type;
		uint8_t mac[6];
		uint16_t msg_type;
		uint16_t msg_len;
		uint8_t msg_id;
		struct dhcpv6_auth_reconfigure auth;
		uint16_t clid_type;
		uint16_t clid_len;
		uint8_t clid_data[128];
	} __attribute__((packed)) reconf_msg = {
		.hdr = {DHCPV6_MSG_RECONFIGURE, {0, 0, 0}},
		.srvid_type = htons(DHCPV6_OPT_SERVERID),
		.srvid_len = htons(10),
		.duid_type = htons(3),
		.hardware_type = htons(1),
		.mac = {iface->mac[0], iface->mac[1], iface->mac[2],
				iface->mac[3], iface->mac[4], iface->mac[5]},
		.msg_type = htons(DHCPV6_OPT_RECONF_MSG),
		.msg_len = htons(1),
		.msg_id = DHCPV6_MSG_RENEW,
		.auth = {htons(DHCPV6_OPT_AUTH),
				htons(sizeof(reconf_msg.auth) - 4), 3, 1, 0,
				{htonl(time(NULL)), htonl(++serial)}, 2, {0}},
		.clid_type = htons(DHCPV6_OPT_CLIENTID),
		.clid_len = htons(assign->clid_len),
		.clid_data = {0},
	};

	memcpy(reconf_msg.clid_data, assign->clid_data, assign->clid_len);
	struct iovec iov = {&reconf_msg, sizeof(reconf_msg) - 128 + assign->clid_len};

	md5_state_t md5;
	uint8_t secretbytes[16];
	memcpy(secretbytes, assign->key, sizeof(secretbytes));

	for (size_t i = 0; i < sizeof(secretbytes); ++i)
		secretbytes[i] ^= 0x36;

	md5_init(&md5);
	md5_append(&md5, secretbytes, sizeof(secretbytes));
	md5_append(&md5, iov.iov_base, iov.iov_len);
	md5_finish(&md5, reconf_msg.auth.key);

	for (size_t i = 0; i < sizeof(secretbytes); ++i) {
		secretbytes[i] ^= 0x36;
		secretbytes[i] ^= 0x5c;
	}

	md5_init(&md5);
	md5_append(&md5, secretbytes, sizeof(secretbytes));
	md5_append(&md5, reconf_msg.auth.key, 16);
	md5_finish(&md5, reconf_msg.auth.key);

	return relayd_forward_packet(socket_fd, &assign->peer, &iov, 1, iface);
}


static void write_statefile(void)
{
	if (config->dhcpv6_statefile) {
		time_t now = monotonic_time(), wall_time = time(NULL);
		int fd = open(config->dhcpv6_statefile, O_CREAT | O_WRONLY | O_CLOEXEC, 0644);
		if (fd < 0) {
			return;
		}
		lockf(fd, F_LOCK, 0);
		ftruncate(fd, 0);

		FILE *fp = fdopen(fd, "w");
		if (!fp) {
			close(fd);
			return;
		}

		for (size_t i = 0; i < config->slavecount; ++i) {
			struct relayd_interface *iface = &config->slaves[i];

			struct assignment *c;
			list_for_each_entry(c, &iface->pd_assignments, head) {
				if (c->clid_len == 0)
					continue;

				char ipbuf[INET6_ADDRSTRLEN];
				char leasebuf[512];
				char duidbuf[264];
				const char hex[] = "0123456789abcdef";

				for (size_t i = 0; i < c->clid_len; ++i) {
					duidbuf[2 * i] = hex[(c->clid_data[i] >> 4) & 0x0f];
					duidbuf[2 * i + 1] = hex[c->clid_data[i] & 0x0f];
				}
				duidbuf[c->clid_len * 2] = 0;

				// iface DUID iaid hostname lifetime assigned length [addrs...]
				int l = snprintf(leasebuf, sizeof(leasebuf), "# %s %s %x %s %u %x %u ",
						iface->ifname, duidbuf, ntohl(c->iaid),
						(c->hostname ? c->hostname : "-"),
						(unsigned)(c->valid_until > now ?
								(c->valid_until - now + wall_time) : 0),
						c->assigned, (unsigned)c->length);

				struct in6_addr addr;
				for (size_t i = 0; i < iface->pd_addr_len; ++i) {
					if (iface->pd_addr[i].prefix > 64)
						continue;

					addr = iface->pd_addr[i].addr;
					if (c->length == 128)
						addr.s6_addr32[3] = htonl(c->assigned);
					else
						addr.s6_addr32[1] |= htonl(c->assigned);
					inet_ntop(AF_INET6, &addr, ipbuf, sizeof(ipbuf) - 1);

					if (c->length == 128 && c->hostname && i == 0)
						fprintf(fp, "%s\t%s\n", ipbuf, c->hostname);

					l += snprintf(leasebuf + l, sizeof(leasebuf) - l, "%s/%hhu ", ipbuf, c->length);
				}
				leasebuf[l - 1] = '\n';
				fwrite(leasebuf, 1, l, fp);
			}
		}

		fclose(fp);
	}

	if (config->dhcpv6_cb) {
		char *argv[2] = {config->dhcpv6_cb, NULL};
		if (!vfork()) {
			execv(argv[0], argv);
			_exit(128);
		}
	}
}


static void apply_lease(struct relayd_interface *iface, struct assignment *a, bool add)
{
	if (a->length > 64)
		return;

	for (size_t i = 0; i < iface->pd_addr_len; ++i) {
		struct in6_addr prefix = iface->pd_addr[i].addr;
		prefix.s6_addr32[1] |= htonl(a->assigned);
		relayd_setup_route(&prefix, a->length, iface, &a->peer.sin6_addr, add);
	}
}


static bool assign_pd(struct relayd_interface *iface, struct assignment *assign)
{
	struct assignment *c;
	if (iface->pd_addr_len < 1)
		return false;

	// Try honoring the hint first
	uint32_t current = 1, asize = (1 << (64 - assign->length)) - 1;
	if (assign->assigned) {
		list_for_each_entry(c, &iface->pd_assignments, head) {
			if (c->length == 128)
				continue;

			if (assign->assigned >= current && assign->assigned + asize < c->assigned) {
				list_add_tail(&assign->head, &c->head);
				apply_lease(iface, assign, true);
				return true;
			}

			if (c->assigned != 0)
				current = (c->assigned + (1 << (64 - c->length)));
		}
	}

	// Fallback to a variable assignment
	current = 1;
	list_for_each_entry(c, &iface->pd_assignments, head) {
		if (c->length == 128)
			continue;

		current = (current + asize) & (~asize);
		if (current + asize < c->assigned) {
			assign->assigned = current;
			list_add_tail(&assign->head, &c->head);
			apply_lease(iface, assign, true);
			return true;
		}

		if (c->assigned != 0)
			current = (c->assigned + (1 << (64 - c->length)));
	}

	return false;
}


static bool assign_na(struct relayd_interface *iface, struct assignment *assign)
{
	if (iface->pd_addr_len < 1)
		return false;

	// Seed RNG with checksum of DUID
	uint32_t seed = 0;
	for (size_t i = 0; i < assign->clid_len; ++i)
		seed += assign->clid_data[i];
	srand(seed);

	// Try to assign up to 100x
	for (size_t i = 0; i < 100; ++i) {
		uint32_t try;
		do try = ((uint32_t)rand()) % 0x0fff; while (try < 0x100);

		struct assignment *c;
		list_for_each_entry(c, &iface->pd_assignments, head) {
			if (c->assigned > try || c->length != 128) {
				assign->assigned = try;
				list_add_tail(&assign->head, &c->head);
				return true;
			} else if (c->assigned == try) {
				break;
			}
		}
	}

	return false;
}


static int prefixcmp(const void *va, const void *vb)
{
	const struct relayd_ipaddr *a = va, *b = vb;
	uint32_t a_pref = ((a->addr.s6_addr[0] & 0xfe) != 0xfc) ? a->preferred : 1;
	uint32_t b_pref = ((b->addr.s6_addr[0] & 0xfe) != 0xfc) ? b->preferred : 1;
	return (a_pref < b_pref) ? 1 : (a_pref > b_pref) ? -1 : 0;
}


static void update(struct relayd_interface *iface)
{
	struct relayd_ipaddr addr[8];
	memset(addr, 0, sizeof(addr));
	int len = relayd_get_interface_addresses(iface->ifindex, addr, 8);

	if (len < 0)
		return;

	qsort(addr, len, sizeof(*addr), prefixcmp);

	time_t now = monotonic_time();
	int minprefix = -1;

	for (int i = 0; i < len; ++i) {
		if (addr[i].prefix > minprefix)
			minprefix = addr[i].prefix;

		addr[i].addr.s6_addr32[2] = 0;
		addr[i].addr.s6_addr32[3] = 0;

		if (addr[i].preferred < UINT32_MAX - now)
			addr[i].preferred += now;

		if (addr[i].valid < UINT32_MAX - now)
			addr[i].valid += now;
	}

	struct assignment *border = list_last_entry(&iface->pd_assignments, struct assignment, head);
	border->assigned = 1 << (64 - minprefix);

	bool change = len != (int)iface->pd_addr_len;
	for (int i = 0; !change && i < len; ++i)
		if (addr[i].addr.s6_addr32[0] != iface->pd_addr[i].addr.s6_addr32[0] ||
				addr[i].addr.s6_addr32[1] != iface->pd_addr[i].addr.s6_addr32[1] ||
				(addr[i].preferred > 0) != (iface->pd_addr[i].preferred > 0) ||
				(addr[i].valid > (uint32_t)now + 7200) !=
						(iface->pd_addr[i].valid > (uint32_t)now + 7200))
			change = true;

	if (change) {
		struct assignment *c;
		list_for_each_entry(c, &iface->pd_assignments, head)
			if (c != border)
				apply_lease(iface, c, false);
	}

	memcpy(iface->pd_addr, addr, len * sizeof(*addr));
	iface->pd_addr_len = len;

	if (change) { // Addresses / prefixes have changed
		struct list_head reassign = LIST_HEAD_INIT(reassign);
		struct assignment *c, *d;
		list_for_each_entry_safe(c, d, &iface->pd_assignments, head) {
			if (c->clid_len == 0 || c->valid_until < now)
				continue;

			if (c->length < 128 && c->assigned >= border->assigned && c != border)
				list_move(&c->head, &reassign);
			else if (c != border)
				apply_lease(iface, c, true);

			if (c->accept_reconf && c->reconf_cnt == 0) {
				c->reconf_cnt = 1;
				c->reconf_sent = now;
				send_reconf(iface, c);

				// Leave all other assignments of that client alone
				struct assignment *a;
				list_for_each_entry(a, &iface->pd_assignments, head)
					if (a != c && a->clid_len == c->clid_len &&
							!memcmp(a->clid_data, c->clid_data, a->clid_len))
						c->reconf_cnt = INT_MAX;
			}
		}

		while (!list_empty(&reassign)) {
			c = list_first_entry(&reassign, struct assignment, head);
			list_del(&c->head);
			if (!assign_pd(iface, c)) {
				c->assigned = 0;
				list_add(&c->head, &iface->pd_assignments);
			}
		}

		write_statefile();
	}
}


static void reconf_timer(struct relayd_event *event)
{
	uint64_t cnt;
	if (read(event->socket, &cnt, sizeof(cnt))) {
		// Avoid compiler warning
	}

	time_t now = monotonic_time();
	for (size_t i = 0; i < config->slavecount; ++i) {
		struct relayd_interface *iface = &config->slaves[i];
		if (iface->pd_assignments.next == NULL)
			return;

		struct assignment *a, *n;
		list_for_each_entry_safe(a, n, &iface->pd_assignments, head) {
			if (a->valid_until < now) {
				if ((a->length < 128 && a->clid_len > 0) ||
						(a->length == 128 && a->clid_len == 0)) {
					list_del(&a->head);
					free(a->hostname);
					free(a);
				}
			} else if (a->reconf_cnt > 0 && a->reconf_cnt < 8 &&
					now > a->reconf_sent + (1 << a->reconf_cnt)) {
				++a->reconf_cnt;
				a->reconf_sent = now;
				send_reconf(iface, a);
			}
		}

		if (iface->pd_reconf) {
			update(iface);
			iface->pd_reconf = false;
		}
	}
}


static size_t append_reply(uint8_t *buf, size_t buflen, uint16_t status,
		const struct dhcpv6_ia_hdr *ia, struct assignment *a,
		struct relayd_interface *iface, bool request)
{
	if (buflen < sizeof(*ia) + sizeof(struct dhcpv6_ia_prefix))
		return 0;

	struct dhcpv6_ia_hdr out = {ia->type, 0, ia->iaid, 0, 0};
	size_t datalen = sizeof(out);
	time_t now = monotonic_time();

	if (status) {
		struct __attribute__((packed)) {
			uint16_t type;
			uint16_t len;
			uint16_t value;
		} stat = {htons(DHCPV6_OPT_STATUS), htons(sizeof(stat) - 4),
				htons(status)};

		memcpy(buf + datalen, &stat, sizeof(stat));
		datalen += sizeof(stat);
	} else {
		if (a) {
			uint32_t pref = 3600;
			uint32_t valid = 3600;
			bool have_non_ula = false;
			for (size_t i = 0; i < iface->pd_addr_len; ++i)
				if ((iface->pd_addr[i].addr.s6_addr[0] & 0xfe) != 0xfc)
					have_non_ula = true;

			for (size_t i = 0; i < iface->pd_addr_len; ++i) {
				uint32_t prefix_pref = iface->pd_addr[i].preferred - now;
				uint32_t prefix_valid = iface->pd_addr[i].valid - now;

				if (iface->pd_addr[i].prefix > 64 ||
						iface->pd_addr[i].preferred <= (uint32_t)now)
					continue;

				// ULA-deprecation compatibility workaround
				if ((iface->pd_addr[i].addr.s6_addr[0] & 0xfe) == 0xfc &&
						a->length == 128 && have_non_ula &&
						config->deprecate_ula_if_public_avail)
					continue;

				if (prefix_pref > 86400)
					prefix_pref = 86400;

				if (prefix_valid > 86400)
					prefix_valid = 86400;

				if (a->length < 128) {
					struct dhcpv6_ia_prefix p = {
						.type = htons(DHCPV6_OPT_IA_PREFIX),
						.len = htons(sizeof(p) - 4),
						.preferred = htonl(prefix_pref),
						.valid = htonl(prefix_valid),
						.prefix = a->length,
						.addr = iface->pd_addr[i].addr
					};
					p.addr.s6_addr32[1] |= htonl(a->assigned);

					if (datalen + sizeof(p) > buflen || a->assigned == 0)
						continue;

					memcpy(buf + datalen, &p, sizeof(p));
					datalen += sizeof(p);
				} else {
					struct dhcpv6_ia_addr n = {
						.type = htons(DHCPV6_OPT_IA_ADDR),
						.len = htons(sizeof(n) - 4),
						.addr = iface->pd_addr[i].addr,
						.preferred = htonl(prefix_pref),
						.valid = htonl(prefix_valid)
					};
					n.addr.s6_addr32[3] = htonl(a->assigned);

					if (datalen + sizeof(n) > buflen || a->assigned == 0)
						continue;

					memcpy(buf + datalen, &n, sizeof(n));
					datalen += sizeof(n);
				}

				// Calculate T1 / T2 based on non-deprecated addresses
				if (prefix_pref > 0) {
					if (prefix_pref < pref)
						pref = prefix_pref;

					if (prefix_valid < valid)
						valid = prefix_valid;
				}
			}

			a->valid_until = valid + now;
			out.t1 = htonl(pref * 5 / 10);
			out.t2 = htonl(pref * 8 / 10);

			if (!out.t1)
				out.t1 = htonl(1);

			if (!out.t2)
				out.t2 = htonl(1);
		}

		if (!request) {
			uint8_t *odata, *end = ((uint8_t*)ia) + htons(ia->len) + 4;
			uint16_t otype, olen;
			dhcpv6_for_each_option((uint8_t*)&ia[1], end, otype, olen, odata) {
				struct dhcpv6_ia_prefix *p = (struct dhcpv6_ia_prefix*)&odata[-4];
				struct dhcpv6_ia_addr *n = (struct dhcpv6_ia_addr*)&odata[-4];
				if ((otype != DHCPV6_OPT_IA_PREFIX || olen < sizeof(*p) - 4) &&
						(otype != DHCPV6_OPT_IA_ADDR || olen < sizeof(*n) - 4))
					continue;

				bool found = false;
				if (a) {
					for (size_t i = 0; i < iface->pd_addr_len; ++i) {
						if (iface->pd_addr[i].prefix > 64 ||
								iface->pd_addr[i].preferred <= (uint32_t)now)
							continue;

						struct in6_addr addr = iface->pd_addr[i].addr;
						if (ia->type == htons(DHCPV6_OPT_IA_PD)) {
							addr.s6_addr32[1] |= htonl(a->assigned);

							if (IN6_ARE_ADDR_EQUAL(&p->addr, &addr) &&
									p->prefix == a->length)
								found = true;
						} else {
							addr.s6_addr32[3] = htonl(a->assigned);

							if (IN6_ARE_ADDR_EQUAL(&n->addr, &addr))
								found = true;
						}
					}
				}

				if (!found) {
					if (otype == DHCPV6_OPT_IA_PREFIX) {
						struct dhcpv6_ia_prefix inv = {
							.type = htons(DHCPV6_OPT_IA_PREFIX),
							.len = htons(sizeof(inv) - 4),
							.preferred = 0,
							.valid = 0,
							.prefix = p->prefix,
							.addr = p->addr
						};

						if (datalen + sizeof(inv) > buflen)
							continue;

						memcpy(buf + datalen, &inv, sizeof(inv));
						datalen += sizeof(inv);
					} else {
						struct dhcpv6_ia_addr inv = {
							.type = htons(DHCPV6_OPT_IA_ADDR),
							.len = htons(sizeof(inv) - 4),
							.addr = n->addr,
							.preferred = 0,
							.valid = 0
						};

						if (datalen + sizeof(inv) > buflen)
							continue;

						memcpy(buf + datalen, &inv, sizeof(inv));
						datalen += sizeof(inv);
					}
				}
			}
		}
	}

	out.len = htons(datalen - 4);
	memcpy(buf, &out, sizeof(out));
	return datalen;
}


size_t dhcpv6_handle_ia(uint8_t *buf, size_t buflen, struct relayd_interface *iface,
		const struct sockaddr_in6 *addr, const void *data, const uint8_t *end)
{
	time_t now = monotonic_time();
	size_t response_len = 0;
	const struct dhcpv6_client_header *hdr = data;
	uint8_t *start = (uint8_t*)&hdr[1], *odata;
	uint16_t otype, olen;

	// Find and parse client-id and hostname
	bool accept_reconf = false;
	uint8_t *clid_data = NULL, clid_len = 0;
	char hostname[256];
	size_t hostname_len = 0;
	dhcpv6_for_each_option(start, end, otype, olen, odata) {
		if (otype == DHCPV6_OPT_CLIENTID) {
			clid_data = odata;
			clid_len = olen;
		} else if (otype == DHCPV6_OPT_FQDN && olen >= 2 && olen <= 255) {
			uint8_t fqdn_buf[256];
			memcpy(fqdn_buf, odata, olen);
			fqdn_buf[olen++] = 0;

			if (dn_expand(&fqdn_buf[1], &fqdn_buf[olen], &fqdn_buf[1], hostname, sizeof(hostname)) > 0)
				hostname_len = strcspn(hostname, ".");
		} else if (otype == DHCPV6_OPT_RECONF_ACCEPT) {
			accept_reconf = true;
		}
	}

	if (!clid_data || !clid_len || clid_len > 130)
		goto out;

	update(iface);
	bool update_state = false;

	struct assignment *first = NULL;
	dhcpv6_for_each_option(start, end, otype, olen, odata) {
		bool is_pd = (otype == DHCPV6_OPT_IA_PD);
		bool is_na = (otype == DHCPV6_OPT_IA_NA);
		if (!is_pd && !is_na)
			continue;

		struct dhcpv6_ia_hdr *ia = (struct dhcpv6_ia_hdr*)&odata[-4];
		size_t ia_response_len = 0;
		uint8_t reqlen = (is_pd) ? 62 : 128;
		uint32_t reqhint = 0;

		// Parse request hint for IA-PD
		if (is_pd) {
			uint8_t *sdata;
			uint16_t stype, slen;
			dhcpv6_for_each_option(&ia[1], odata + olen, stype, slen, sdata) {
				if (stype == DHCPV6_OPT_IA_PREFIX && slen >= sizeof(struct dhcpv6_ia_prefix) - 4) {
					struct dhcpv6_ia_prefix *p = (struct dhcpv6_ia_prefix*)&sdata[-4];
					if (p->prefix) {
						reqlen = p->prefix;
						reqhint = ntohl(p->addr.s6_addr32[1]);
						if (reqlen > 32 && reqlen <= 64)
							reqhint &= (1U << (64 - reqlen)) - 1;
					}
					break;
				}
			}

			if (reqlen > 64)
				reqlen = 64;
		}

		// Find assignment
		struct assignment *c, *a = NULL;
		list_for_each_entry(c, &iface->pd_assignments, head) {
			if (c->clid_len == clid_len && !memcmp(c->clid_data, clid_data, clid_len) &&
					(c->iaid == ia->iaid || c->valid_until < now) &&
					((is_pd && c->length <= 64) || (is_na && c->length == 128))) {
				a = c;

				// Reset state
				apply_lease(iface, a, false);
				a->iaid = ia->iaid;
				a->peer = *addr;
				a->reconf_cnt = 0;
				a->reconf_sent = 0;
				break;
			}
		}

		// Generic message handling
		uint16_t status = DHCPV6_STATUS_OK;
		if (hdr->msg_type == DHCPV6_MSG_SOLICIT || hdr->msg_type == DHCPV6_MSG_REQUEST) {
			bool assigned = !!a;

			if (!a) { // Create new binding
				a = calloc(1, sizeof(*a) + clid_len);
				a->clid_len = clid_len;
				a->iaid = ia->iaid;
				a->length = reqlen;
				a->peer = *addr;
				a->assigned = reqhint;
				if (first)
					memcpy(a->key, first->key, sizeof(a->key));
				else
					relayd_urandom(a->key, sizeof(a->key));
				memcpy(a->clid_data, clid_data, clid_len);

				if (is_pd)
					while (!(assigned = assign_pd(iface, a)) && ++a->length <= 64);
				else
					assigned = assign_na(iface, a);
			}

			if (!assigned || iface->pd_addr_len == 0) { // Set error status
				status = (is_pd) ? DHCPV6_STATUS_NOPREFIXAVAIL : DHCPV6_STATUS_NOADDRSAVAIL;
			} else if (assigned && !first) { //
				size_t handshake_len = 4;
				buf[0] = 0;
				buf[1] = DHCPV6_OPT_RECONF_ACCEPT;
				buf[2] = 0;
				buf[3] = 0;

				if (hdr->msg_type == DHCPV6_MSG_REQUEST) {
					struct dhcpv6_auth_reconfigure auth = {
						htons(DHCPV6_OPT_AUTH),
						htons(sizeof(auth) - 4),
						3, 1, 0,
						{htonl(time(NULL)), htonl(++serial)},
						1,
						{0}
					};
					memcpy(auth.key, a->key, sizeof(a->key));
					memcpy(buf + handshake_len, &auth, sizeof(auth));
					handshake_len += sizeof(auth);
				}

				buf += handshake_len;
				buflen -= handshake_len;
				response_len += handshake_len;

				first = a;
			}

			ia_response_len = append_reply(buf, buflen, status, ia, a, iface, true);

			// Was only a solicitation: mark binding for removal
			if (assigned && hdr->msg_type == DHCPV6_MSG_SOLICIT) {
				a->valid_until = 0;
			} else if (assigned && hdr->msg_type == DHCPV6_MSG_REQUEST) {
				if (hostname_len > 0) {
					a->hostname = realloc(a->hostname, hostname_len + 1);
					memcpy(a->hostname, hostname, hostname_len);
					a->hostname[hostname_len] = 0;
				}
				a->accept_reconf = accept_reconf;
				apply_lease(iface, a, true);
				update_state = true;
			} else if (!assigned && a) { // Cleanup failed assignment
				free(a->hostname);
				free(a);
			}
		} else if (hdr->msg_type == DHCPV6_MSG_RENEW ||
				hdr->msg_type == DHCPV6_MSG_RELEASE ||
				hdr->msg_type == DHCPV6_MSG_REBIND ||
				hdr->msg_type == DHCPV6_MSG_DECLINE) {
			if (!a && hdr->msg_type != DHCPV6_MSG_REBIND) {
				status = DHCPV6_STATUS_NOBINDING;
				ia_response_len = append_reply(buf, buflen, status, ia, a, iface, false);
			} else if (hdr->msg_type == DHCPV6_MSG_RENEW ||
					hdr->msg_type == DHCPV6_MSG_REBIND) {
				ia_response_len = append_reply(buf, buflen, status, ia, a, iface, false);
				if (a)
					apply_lease(iface, a, true);
			} else if (hdr->msg_type == DHCPV6_MSG_RELEASE) {
				a->valid_until = 0;
				apply_lease(iface, a, false);
				update_state = true;
			} else if (hdr->msg_type == DHCPV6_MSG_DECLINE && a->length == 128) {
				a->clid_len = 0;
				a->valid_until = now + 3600; // Block address for 1h
				update_state = true;
			}
		} else if (hdr->msg_type == DHCPV6_MSG_CONFIRM) {
			// Always send NOTONLINK for CONFIRM so that clients restart connection
			status = DHCPV6_STATUS_NOTONLINK;
			ia_response_len = append_reply(buf, buflen, status, ia, a, iface, true);
		}

		buf += ia_response_len;
		buflen -= ia_response_len;
		response_len += ia_response_len;
	}

	if (hdr->msg_type == DHCPV6_MSG_RELEASE && response_len + 6 < buflen) {
		buf[0] = 0;
		buf[1] = DHCPV6_OPT_STATUS;
		buf[2] = 0;
		buf[3] = 2;
		buf[4] = 0;
		buf[5] = DHCPV6_STATUS_OK;
		response_len += 6;
	}

	if (update_state)
		write_statefile();

out:
	return response_len;
}
