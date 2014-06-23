/*
 * Network configuration layer (Linux)
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: netconf_linux.c 358181 2012-09-21 13:59:23Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>

#include <typedefs.h>
#include <proto/ethernet.h>
#include <netconf.h>
#include <netconf_linux.h>

#ifndef LINUX_2_6_36
typedef struct ipt_time_info		time_info_t;
#define TIME_INFO_EXTRA_BYTES		8
#else
typedef struct xt_time_info		time_info_t;
#define TIME_INFO_EXTRA_BYTES		0
#define IPT_ALIGN			XT_ALIGN
#endif

/* Loops over each match in the ipt_entry */
#define for_each_ipt_match(match, entry) \
	for ((match) = (struct ipt_entry_match *) &(entry)->elems[0]; \
	     (int) (match) < (int) (entry) + (entry)->target_offset; \
	     (match) = (struct ipt_entry_match *) ((int) (match) + (match)->u.match_size))

/* Supported ipt table names */
static const char *ipt_table_names[] = { "filter", "nat", NULL };

/* ipt table name appropriate for target (indexed by netconf_fw_t.target) */
static const char * ipt_table_name[] = {
	"filter", "filter", "filter", "filter",
	"nat", "nat", "nat", "nat"
};

/* ipt target name (indexed by netconf_fw_t.target) */
static const char * ipt_target_name[] = {
	"DROP", "ACCEPT", "logdrop", "logaccept",
	"SNAT", "DNAT", "MASQUERADE", "autofw"
};

/* ipt target data size (indexed by netconf_fw_t.target) */
#ifndef LINUX_2_6_36
static const size_t ipt_target_size[] = {
	sizeof(int), sizeof(int), sizeof(int), sizeof(int),
	sizeof(struct ip_nat_multi_range), sizeof(struct ip_nat_multi_range), sizeof(struct ip_nat_multi_range), sizeof(struct ip_autofw_info)
};
#else /* linux-2.6.36 */
/* ipt target data size (indexed by netconf_fw_t.target) */
static const size_t ipt_target_size[] = {
	sizeof(int), sizeof(int), sizeof(int), sizeof(int),
	sizeof(struct nf_nat_multi_range), sizeof(struct nf_nat_multi_range), sizeof(struct nf_nat_multi_range), sizeof(struct ip_autofw_info)
};
#endif /* linux-2.6.36 */

/* ipt filter chain name appropriate for direction (indexed by netconf_filter_t.dir) */
static const char * ipt_filter_chain_name[] = { 
	"INPUT", "FORWARD", "OUTPUT"
};

/* ipt nat chain name appropriate for target (indexed by netconf_nat_t.target) */
static const char * ipt_nat_chain_name[] = { 
	NULL, NULL, NULL, NULL,
	"POSTROUTING", "PREROUTING", "POSTROUTING"
};

/* Returns a netconf_dir index */
static int
filter_dir(const char *name)
{
	if (strncmp(name, "INPUT", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_IN;
	else if (strncmp(name, "FORWARD", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_FORWARD;
	else if (strncmp(name, "OUTPUT", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_OUT;
	else
		return -1;
}

/* Returns a netconf_target index */
static int
#ifndef LINUX_2_6_36
target_num(const struct ipt_entry *entry, iptc_handle_t *handle)
#else /* linux-2.6.36 */
target_num(const struct ipt_entry *entry, struct iptc_handle *handle)
#endif /* linux-2.6.36 */
{
	const char *name = iptc_get_target(entry, handle);

	if (!name)
		return -1;

	if (strncmp(name, "DROP", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_DROP;
	else if (strncmp(name, "ACCEPT", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_ACCEPT;
	else if (strncmp(name, "logdrop", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_LOG_DROP;
	else if (strncmp(name, "logaccept", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_LOG_ACCEPT;
	else if (strncmp(name, "SNAT", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_SNAT;
	else if (strncmp(name, "DNAT", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_DNAT;
	else if (strncmp(name, "MASQUERADE", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_MASQ;
	else if (strncmp(name, "autofw", IPT_FUNCTION_MAXNAMELEN) == 0)
		return NETCONF_APP;
	else
		return -1;
}

#ifdef LINUX_2_6_36
/* User: match.day, SUN/0~MON/1 ~ SAT/6 */
/* Kernel: MON/1 ~ SAT/6~SUN/7 */
/* Need special handle for Sunday */
/* Be aware: we steal the flags bit 1 ~ 7 to store the begin day */
static void
get_days(unsigned int *days, time_info_t *time)
{
	int i, j;
	char weekdays_map[7] = {0};

	/* Translate from Kernel to User */
	for (i = 1; i <= 7; i++) {
		if (time->weekdays_match & (1 << i)) {
			if (i == 7)
				weekdays_map[0] = 1;
			else
				weekdays_map[i] = 1;
		}
	}

	/* Begin day */
	for (i = 1; i <= 7; i++) {
		if (time->flags & (1 << i)) {
			if (i == 7)
				days[0] = 0;
			else
				days[0] = i;
			break;
		}
	}

	/* End day */
	for (i = days[0], j = 0; j < 7; i = (i + 1) % 7, j++) {
		if (weekdays_map[i])
			days[1] = i;
		else
			break;
	}
}

/* User: match.day, SUN/0~MON/1 ~ SAT/6 */
/* Kernel: MON/1 ~ SAT/6~SUN/7 */
/* Need special handle for Sunday */
/* Be aware: we steal the flags bit 1 ~ 7 to store the begin day */
static void
set_days(unsigned int *days, time_info_t *time)
{
	int i;

	for (i = days[0]; i != days[1]; i = (i + 1) % 7) {
		if (!i)
			time->weekdays_match |= (1 << 7);
		else
			time->weekdays_match |= (1 << i);
	}

	if (!days[1])
		time->weekdays_match |= (1 << 7);
	else
		time->weekdays_match |= (1 << days[1]);

	/* Use local time */
	time->flags = XT_TIME_LOCAL_TZ;

	/* Steal flags to store the begin day */
	if (days[0] == 0)
		time->flags |= (1 << 7);
	else
		time->flags |= (1 << days[0]);
}
#endif /* LINUX_2_6_36 */

/*
 * Get a list of the current firewall entries
 * @param	fw_list	list of firewall entries
 * @return	0 on success and errno on failure
 */
int
netconf_get_fw(netconf_fw_t *fw_list)
{
	const char **table;
	const char *chain;
	const struct ipt_entry *entry;
#ifndef LINUX_2_6_36
	iptc_handle_t handle = NULL;
#else /* linux-2.6.36 */
	struct iptc_handle *handle = NULL;
#endif /* linux-2.6.36 */

	/* Initialize list */
	netconf_list_init(fw_list);

	/* Search all default tables */
	for (table = &ipt_table_names[0]; *table; table++) {

		if (strcmp(*table, "filter") && strcmp(*table, "nat"))
			continue;		

		if (!(handle = iptc_init(*table))) {
			fprintf(stderr, "%s\n", iptc_strerror(errno));
			goto err;
		}

		/* Search all default chains */
#ifndef LINUX_2_6_36
		for (chain = iptc_first_chain(&handle); chain; chain = iptc_next_chain(&handle)) {
#else /* linux-2.6.36 */
		for (chain = iptc_first_chain(handle); chain; chain = iptc_next_chain(handle)) {
#endif /* linux-2.6.36 */
			if (strcmp(chain, "INPUT") && strcmp(chain, "FORWARD") && strcmp(chain, "OUTPUT") &&
			    strcmp(chain, "PREROUTING") && strcmp(chain, "POSTROUTING"))
				continue;		

			/* Search all entries */
#ifndef LINUX_2_6_36
			for (entry = iptc_first_rule(chain, &handle); entry; entry = iptc_next_rule(entry, &handle)) {
				int num = target_num(entry, &handle);
#else /* linux-2.6.36 */
			for (entry = iptc_first_rule(chain, handle); entry; entry = iptc_next_rule(entry, handle)) {
				int num = target_num(entry, handle);
#endif /* linux-2.6.36 */
				netconf_fw_t *fw = NULL;
				netconf_filter_t *filter = NULL;
				netconf_nat_t *nat = NULL;
				netconf_app_t *app = NULL;

				const struct ipt_entry_match *match;
				const struct ipt_entry_target *target;
				struct ipt_mac_info *mac = NULL;
				struct ipt_state_info *state = NULL;
				time_info_t *time = NULL;

				/* Only know about TCP/UDP */
				if (!netconf_valid_ipproto(entry->ip.proto))
					continue;

				/* Only know about target types in the specified tables */
				if (!netconf_valid_target(num) ||
				    strncmp(ipt_table_name[num], *table, IPT_FUNCTION_MAXNAMELEN) != 0)
					continue;

				/* Only know about specified target types */
				if (netconf_valid_filter(num)) {
					filter = calloc(1, sizeof(netconf_filter_t));
					fw = (netconf_fw_t *) filter;
				}
				else if (netconf_valid_nat(num)) {
					nat = calloc(1, sizeof(netconf_nat_t));
					fw = (netconf_fw_t *) nat;
				}
				else if (num == NETCONF_APP) {
					app = calloc(1, sizeof(netconf_app_t));
					fw = (netconf_fw_t *) app;
				}
				else
					continue;

				if (!fw) {
					perror("calloc");
					goto err;
				}
				netconf_list_add(fw, fw_list);

				/* Get IP addresses */
				fw->match.src.ipaddr.s_addr = entry->ip.src.s_addr;
				fw->match.src.netmask.s_addr = entry->ip.smsk.s_addr;
				fw->match.dst.ipaddr.s_addr = entry->ip.dst.s_addr;
				fw->match.dst.netmask.s_addr = entry->ip.dmsk.s_addr;
				fw->match.flags |= (entry->ip.invflags & IPT_INV_SRCIP) ? NETCONF_INV_SRCIP : 0;
				fw->match.flags |= (entry->ip.invflags & IPT_INV_DSTIP) ? NETCONF_INV_DSTIP : 0;

				/* Get interface names */
				strncpy(fw->match.in.name, entry->ip.iniface, IFNAMSIZ);
				strncpy(fw->match.out.name, entry->ip.outiface, IFNAMSIZ);
				fw->match.flags |= (entry->ip.invflags & IPT_INV_VIA_IN) ? NETCONF_INV_IN : 0;
				fw->match.flags |= (entry->ip.invflags & IPT_INV_VIA_OUT) ? NETCONF_INV_OUT : 0;

				/* Get TCP port(s) */
				if (entry->ip.proto == IPPROTO_TCP) {
					struct ipt_tcp *tcp = NULL;

					for_each_ipt_match(match, entry) {
						if (strncmp(match->u.user.name, "tcp", IPT_FUNCTION_MAXNAMELEN) != 0)
							continue;

						tcp = (struct ipt_tcp *) &match->data[0];
						break;
					}

					if (tcp) {
						/* Match ports stored in host order for some stupid reason */
						fw->match.ipproto = IPPROTO_TCP;
						fw->match.src.ports[0] = htons(tcp->spts[0]);
						fw->match.src.ports[1] = htons(tcp->spts[1]);
						fw->match.dst.ports[0] = htons(tcp->dpts[0]);
						fw->match.dst.ports[1] = htons(tcp->dpts[1]);
						fw->match.flags |= (tcp->invflags & IPT_TCP_INV_SRCPT) ? NETCONF_INV_SRCPT : 0;
						fw->match.flags |= (tcp->invflags & IPT_TCP_INV_DSTPT) ? NETCONF_INV_DSTPT : 0;
					}
				}

				/* Get UDP port(s) */
				else if (entry->ip.proto == IPPROTO_UDP) {
					struct ipt_udp *udp = NULL;

					for_each_ipt_match(match, entry) {
						if (strncmp(match->u.user.name, "udp", IPT_FUNCTION_MAXNAMELEN) != 0)
							continue;

						udp = (struct ipt_udp *) &match->data[0];
						break;
					}

					if (udp) {
						/* Match ports stored in host order for some stupid reason */
						fw->match.ipproto = IPPROTO_UDP;
						fw->match.src.ports[0] = htons(udp->spts[0]);
						fw->match.src.ports[1] = htons(udp->spts[1]);
						fw->match.dst.ports[0] = htons(udp->dpts[0]);
						fw->match.dst.ports[1] = htons(udp->dpts[1]);
						fw->match.flags |= (udp->invflags & IPT_UDP_INV_SRCPT) ? NETCONF_INV_SRCPT : 0;
						fw->match.flags |= (udp->invflags & IPT_UDP_INV_DSTPT) ? NETCONF_INV_DSTPT : 0;
					}
				}
				
				/* Get source MAC address */
				for_each_ipt_match(match, entry) {
					if (strncmp(match->u.user.name, "mac", IPT_FUNCTION_MAXNAMELEN) != 0)
						continue;
			
					mac = (struct ipt_mac_info *) &match->data[0];
					break;
				}
				if (mac) {
					memcpy(fw->match.mac.octet, mac->srcaddr, ETHER_ADDR_LEN);
					fw->match.flags |= mac->invert ? NETCONF_INV_MAC : 0;
				}

				/* Get packet state */
				for_each_ipt_match(match, entry) {
					if (strncmp(match->u.user.name, "state", IPT_FUNCTION_MAXNAMELEN) != 0)
						continue;
			
					state = (struct ipt_state_info *) &match->data[0];
					break;
				}
				if (state) {
					fw->match.state |= (state->statemask & IPT_STATE_INVALID) ? NETCONF_INVALID : 0;
					fw->match.state |= (state->statemask & IPT_STATE_BIT(IP_CT_ESTABLISHED)) ? NETCONF_ESTABLISHED : 0;
					fw->match.state |= (state->statemask & IPT_STATE_BIT(IP_CT_RELATED)) ? NETCONF_RELATED : 0;
					fw->match.state |= (state->statemask & IPT_STATE_BIT(IP_CT_NEW)) ? NETCONF_NEW : 0;
				}

				/* Get local time */
				for_each_ipt_match(match, entry) {
					if (strncmp(match->u.user.name, "time", IPT_FUNCTION_MAXNAMELEN) != 0)
						continue;

					/* We added 8 bytes of day range at the end */
					if (match->u.match_size < (IPT_ALIGN(sizeof(struct ipt_entry_match)) +
								   IPT_ALIGN(sizeof(time_info_t) + TIME_INFO_EXTRA_BYTES)))
						continue;

					time = (time_info_t *) &match->data[0];
					break;
				}
				if (time) {
#ifndef LINUX_2_6_36
					unsigned int *days = (unsigned int *) &time[1];

					fw->match.days[0] = days[0];
					fw->match.days[1] = days[1];
					fw->match.secs[0] = time->time_start;
					fw->match.secs[1] = time->time_stop;
#else
					/* Get days from weekdays_match and flags */
					get_days(fw->match.days, time);
					fw->match.secs[0] = time->daytime_start;
					fw->match.secs[1] = time->daytime_stop;
#endif
				}

				/* Set target type */
				fw->target = num;
				target = (struct ipt_entry_target *) ((int) entry + entry->target_offset);

				/* Get filter target information */
				if (filter) {
					if (!netconf_valid_dir(filter->dir = filter_dir(chain)))
						goto err;
				}

				/* Get NAT target information */
				else if (nat) {
#ifndef LINUX_2_6_36
					struct ip_nat_multi_range *mr = (struct ip_nat_multi_range *) &target->data[0];
					struct ip_nat_range *range = (struct ip_nat_range *) &mr->range[0];
#else /* linux-2.6.36 */
					struct nf_nat_multi_range *mr = (struct nf_nat_multi_range *) &target->data[0];
					struct nf_nat_range *range = (struct nf_nat_range *) &mr->range[0];

#endif /* linux-2.6.36 */
					/* Get mapped IP address */
					nat->ipaddr.s_addr = range->min_ip;
				
					/* Get mapped TCP port(s) */
					if (entry->ip.proto == IPPROTO_TCP) {
						nat->ports[0] = range->min.tcp.port;
						nat->ports[1] = range->max.tcp.port;
					}

					/* Get mapped UDP port(s) */
					else if (entry->ip.proto == IPPROTO_UDP) {
						nat->ports[0] = range->min.udp.port;
						nat->ports[1] = range->max.udp.port;
					}
				}

				/* Get application specific port forward information */
				else if (app) {
					struct ip_autofw_info *info = (struct ip_autofw_info *) &target->data[0];

					app->proto = info->proto;
					app->dport[0] = info->dport[0];
					app->dport[1] = info->dport[1];
					app->to[0] = info->to[0];
					app->to[1] = info->to[1];
				}
			}
		}

#ifndef LINUX_2_6_36
		if (!iptc_commit(&handle)) {
#else /* linux-2.6.36 */
		if (!iptc_commit(handle)) {
#endif /* linux-2.6.36 */
#ifdef LINUX_2_6_36
			iptc_free(handle);
#endif
			fprintf(stderr, "%s\n", iptc_strerror(errno));
			handle = NULL;
			goto err;
		}
	}

#ifdef LINUX_2_6_36
	iptc_free(handle);
#endif
	return 0;

 err:
	if (handle)
#ifndef LINUX_2_6_36
		iptc_commit(&handle);
#else /* linux-2.6.36 */
		iptc_commit(handle);
#endif /* linux-2.6.36 */
#ifdef LINUX_2_6_36
	if (handle)
		iptc_free(handle);
#endif
	netconf_list_free(fw_list);
	return errno;	
}

/* Logical XOR */
#define lxor(a, b) (((a) && !(b)) || (!(a) && (b)))

/*
 * Get the index of a firewall entry
 * @param	fw	firewall entry to look for
 * @return	index of firewall entry or <0 if not found or an error occurred
 */
static int
netconf_fw_index(const netconf_fw_t *fw)
{
	const netconf_filter_t *filter = NULL;
	const netconf_nat_t *nat = NULL;
	const netconf_app_t *app = NULL;
	const char **table;
	const char *chain;
	const struct ipt_entry *entry = NULL;
#ifndef LINUX_2_6_36
	iptc_handle_t handle = NULL;
#else /* linux-2.6.36 */
	struct iptc_handle *handle = NULL;
#endif /* linux-2.6.36 */

	int ret = 0;

	if (!netconf_valid_ipproto(fw->match.ipproto)) {
		fprintf(stderr, "invalid IP protocol %d\n", fw->match.ipproto);
		return -EINVAL;
	}

	/* Only know about specified target types */
	if (netconf_valid_filter(fw->target)) {
		filter = (netconf_filter_t *) fw;
		if (!netconf_valid_dir(filter->dir)) {
			fprintf(stderr, "invalid filter direction %d\n", filter->dir);
			return -EINVAL;
		}
	}
	else if (netconf_valid_nat(fw->target))
		nat = (netconf_nat_t *) fw;
	else if (fw->target == NETCONF_APP)
		app = (netconf_app_t *) fw;
	else {
		fprintf(stderr, "invalid target type %d\n", fw->target);
		return -EINVAL;
	}

	/* Search all default tables */
	for (table = &ipt_table_names[0]; *table; table++) {

		/* Only consider specified tables */
		if (strncmp(ipt_table_name[fw->target], *table, IPT_FUNCTION_MAXNAMELEN) != 0)
			continue;

		if (!(handle = iptc_init(*table))) {
			fprintf(stderr, "%s\n", iptc_strerror(errno));
			return -errno;
		}

		/* Search all default chains */
#ifndef LINUX_2_6_36
		for (chain = iptc_first_chain(&handle); chain; chain = iptc_next_chain(&handle)) {
#else /* linux-2.6.36 */
		for (chain = iptc_first_chain(handle); chain; chain = iptc_next_chain(handle)) {
#endif /* linux-2.6.36 */

			/* Only consider specified chains */
			if (filter && strncmp(chain, ipt_filter_chain_name[filter->dir], sizeof(ipt_chainlabel)) != 0)
				continue;
			else if (nat && strncmp(chain, ipt_nat_chain_name[nat->target], sizeof(ipt_chainlabel)) != 0)
				continue;
			else if (app && strncmp(chain, "PREROUTING", sizeof(ipt_chainlabel)) != 0)
				continue;

			/* Search all entries */
#ifndef LINUX_2_6_36
			for (ret = 0, entry = iptc_first_rule(chain, &handle); entry; ret++, entry = iptc_next_rule(entry, &handle)) {
#else /* linux-2.6.36 */
			for (ret = 0, entry = iptc_first_rule(chain, handle); entry; ret++, entry = iptc_next_rule(entry, handle)) {
#endif /* linux-2.6.36 */
				const struct ipt_entry_match *match;
				const struct ipt_entry_target *target;
				struct ipt_mac_info *mac = NULL;
				struct ipt_state_info *state = NULL;
				time_info_t *time = NULL;

				/* Only know about TCP/UDP */
				if (entry->ip.proto != fw->match.ipproto)
					continue;

				/* Compare IP address(es) */
				if (entry->ip.src.s_addr != fw->match.src.ipaddr.s_addr ||
				    entry->ip.smsk.s_addr != fw->match.src.netmask.s_addr ||
				    entry->ip.dst.s_addr != fw->match.dst.ipaddr.s_addr ||
				    entry->ip.dmsk.s_addr != fw->match.dst.netmask.s_addr)
					continue;

				if (lxor(entry->ip.invflags & IPT_INV_SRCIP, fw->match.flags & NETCONF_INV_SRCIP) ||
				    lxor(entry->ip.invflags & IPT_INV_DSTIP, fw->match.flags & NETCONF_INV_DSTIP))
					continue;

				/* Compare interface names */
				if (strncmp(fw->match.in.name, entry->ip.iniface, IFNAMSIZ) != 0 ||
				    strncmp(fw->match.out.name, entry->ip.outiface, IFNAMSIZ) != 0)
					continue;

				/* Compare TCP port(s) */
				if (fw->match.ipproto == IPPROTO_TCP) {
					struct ipt_tcp *tcp = NULL;

					for_each_ipt_match(match, entry) {
						if (strncmp(match->u.user.name, "tcp", IPT_FUNCTION_MAXNAMELEN) != 0)
							continue;

						tcp = (struct ipt_tcp *) &match->data[0];
						break;
					}
			
					/* Match ports stored in host order for some stupid reason */
					if (!tcp ||
					    tcp->spts[0] != ntohs(fw->match.src.ports[0]) ||
					    tcp->spts[1] != ntohs(fw->match.src.ports[1]) ||
					    tcp->dpts[0] != ntohs(fw->match.dst.ports[0]) ||
					    tcp->dpts[1] != ntohs(fw->match.dst.ports[1]))
						continue;

					if (lxor(tcp->invflags & IPT_TCP_INV_SRCPT, fw->match.flags & NETCONF_INV_SRCPT) ||
					    lxor(tcp->invflags & IPT_TCP_INV_DSTPT, fw->match.flags & NETCONF_INV_DSTPT))
						continue;
				}

				/* Compare UDP port(s) */
				else if (fw->match.ipproto == IPPROTO_UDP) {
					struct ipt_udp *udp = NULL;

					for_each_ipt_match(match, entry) {
						if (strncmp(match->u.user.name, "udp", IPT_FUNCTION_MAXNAMELEN) != 0)
							continue;

						udp = (struct ipt_udp *) &match->data[0];
						break;
					}

					/* Match ports stored in host order for some stupid reason */
					if (!udp ||
					    udp->spts[0] != ntohs(fw->match.src.ports[0]) ||
					    udp->spts[1] != ntohs(fw->match.src.ports[1]) ||
					    udp->dpts[0] != ntohs(fw->match.dst.ports[0]) ||
					    udp->dpts[1] != ntohs(fw->match.dst.ports[1]))
						continue;

					if (lxor(udp->invflags & IPT_UDP_INV_SRCPT, fw->match.flags & NETCONF_INV_SRCPT) ||
					    lxor(udp->invflags & IPT_UDP_INV_DSTPT, fw->match.flags & NETCONF_INV_DSTPT))
						continue;
				}

				/* Compare source MAC addresses */
				if (!ETHER_ISNULLADDR(fw->match.mac.octet)) {
					for_each_ipt_match(match, entry) {
						if (strncmp(match->u.user.name, "mac", IPT_FUNCTION_MAXNAMELEN) != 0)
							continue;
			
						mac = (struct ipt_mac_info *) &match->data[0];
						break;
					}
			
					if (!mac ||
					    memcmp(mac->srcaddr, fw->match.mac.octet, ETHER_ADDR_LEN) != 0 ||
					    ( mac->invert && !(fw->match.flags & NETCONF_INV_MAC)) ||
					    (!mac->invert &&  (fw->match.flags & NETCONF_INV_MAC)))
						continue;
				}

				/* Compare packet states */
				if (fw->match.state) {
					for_each_ipt_match(match, entry) {
						if (strncmp(match->u.user.name, "state", IPT_FUNCTION_MAXNAMELEN) != 0)
							continue;
			
						state = (struct ipt_state_info *) &match->data[0];
						break;
					}
			
					if (!state ||
					    lxor(state->statemask & IPT_STATE_INVALID, fw->match.state & NETCONF_INVALID) ||
					    lxor(state->statemask & IPT_STATE_BIT(IP_CT_ESTABLISHED), fw->match.state & NETCONF_ESTABLISHED) ||
					    lxor(state->statemask & IPT_STATE_BIT(IP_CT_RELATED), fw->match.state & NETCONF_RELATED) ||
					    lxor(state->statemask & IPT_STATE_BIT(IP_CT_NEW), fw->match.state & NETCONF_NEW))
						continue;
				}

				/* Compare local time */
				if (fw->match.secs[0] || fw->match.secs[1]) {
					for_each_ipt_match(match, entry) {
						if (strncmp(match->u.user.name, "time", IPT_FUNCTION_MAXNAMELEN) != 0)
							continue;

						/* We added 8 bytes of day range at the end */
						if (match->u.match_size < (IPT_ALIGN(sizeof(struct ipt_entry_match)) +
									   IPT_ALIGN(sizeof(time_info_t) + TIME_INFO_EXTRA_BYTES)))
							continue;

						time = (time_info_t *) &match->data[0];
						break;
					}

					if (!time)
						continue;
					else {
						unsigned int time_start, time_stop;
#ifndef LINUX_2_6_36
						unsigned int *days = (unsigned int *) &time[1];

						time_start = time->time_start;
						time_stop = time->time_stop;
#else
						unsigned int days[2];

						time_start = time->daytime_start;
						time_stop = time->daytime_stop;
						get_days(days, time);
#endif
						if (fw->match.days[0] != days[0] ||
						    fw->match.days[1] != days[1] ||
						    fw->match.secs[0] != time_start ||
						    fw->match.secs[1] != time_stop)
							continue;
					}
				}

				/* Compare target type */
#ifndef LINUX_2_6_36
				if (fw->target != target_num(entry, &handle))
#else
				if (fw->target != target_num(entry, handle))
#endif /* linux-2.6.36 */
					continue;
				target = (struct ipt_entry_target *) ((int) entry + entry->target_offset);

				/* Compare NAT target information */
				if (nat) {
#ifndef LINUX_2_6_36
					struct ip_nat_multi_range *mr = (struct ip_nat_multi_range *) &target->data[0];
					struct ip_nat_range *range = (struct ip_nat_range *) &mr->range[0];
#else
					struct nf_nat_multi_range *mr = (struct nf_nat_multi_range *) &target->data[0];
					struct nf_nat_range *range = (struct nf_nat_range *) &mr->range[0];
#endif /* linux-2.6.36 */
			
					/* Compare mapped IP address */
					if (range->min_ip != nat->ipaddr.s_addr)
						continue;
				
					/* Compare mapped TCP port(s) */
					if (fw->match.ipproto == IPPROTO_TCP) {
						if (range->min.tcp.port != nat->ports[0] ||
						    range->max.tcp.port != nat->ports[1])
							continue;
					}

					/* Compare mapped UDP port(s) */
					else if (fw->match.ipproto == IPPROTO_UDP) {
						if (range->min.udp.port != nat->ports[0] ||
						    range->max.udp.port != nat->ports[1])
							continue;
					}
				}

				/* Compare application specific port forward information */
				else if (app) {
					struct ip_autofw_info *info = (struct ip_autofw_info *) &target->data[0];

					if (app->proto != info->proto ||
					    app->dport[0] != info->dport[0] ||
					    app->dport[1] != info->dport[1] ||
					    app->to[0] != info->to[0] ||
					    app->to[1] != info->to[1])
						continue;
				}

				break;
			}

			if (entry)
				break;
		}

#ifndef LINUX_2_6_36
		if (!iptc_commit(&handle)) {
#else
		if (!iptc_commit(handle)) {
#endif /* linux-2.6.36 */
#ifdef LINUX_2_6_36
			iptc_free(handle);
#endif
			fprintf(stderr, "%s\n", iptc_strerror(errno));
			return -errno;
		}

#ifdef LINUX_2_6_36
		iptc_free(handle);
#endif
		if (entry)
			break;
	}

	return (entry ? ret : -ENOENT);
}

/*
 * See if a given firewall entry already exists
 * @param	nat	NAT entry to look for
 * @return	whether NAT entry exists
 */
int
netconf_fw_exists(netconf_fw_t *fw)
{
	return (netconf_fw_index(fw) >= 0);
}

/*
 * Allocate and append a match structure to an existing ipt_entry
 * @param	pentry			pointer to pointer to initialized ipt_entry
 * @param	name			name of match
 * @param	match_data_size		size of data portion of match structure
 * @return	pointer to newly created match header inside ipt_entry
 */
static struct ipt_entry_match *
netconf_append_match(struct ipt_entry **pentry, const char *name, size_t match_data_size)
{
	struct ipt_entry *entry;
	struct ipt_entry_match *match;
	size_t match_size = 0;

	match_size += IPT_ALIGN(sizeof(struct ipt_entry_match));
	match_size += IPT_ALIGN(match_data_size);

	if (!(entry = realloc(*pentry, (*pentry)->next_offset + match_size))) {
		perror("realloc");
		return NULL;
	}

	match = (struct ipt_entry_match *) ((int) entry + entry->next_offset);
	entry->next_offset += match_size;
	entry->target_offset += match_size;
	memset(match, 0, match_size);

	strncpy(match->u.user.name, name, IPT_FUNCTION_MAXNAMELEN);
	match->u.match_size = match_size;

	*pentry = entry;
	return match;
}

/*
 * Allocate and append a target structure to an existing ipt_entry
 * @param	pentry			pointer to pointer to initialized ipt_entry with matches
 * @param	name			name of target
 * @param	target_data_size	size of data portion of target structure
 * @return	pointer to newly created target header inside ipt_entry
 */
static struct ipt_entry_target *
netconf_append_target(struct ipt_entry **pentry, const char *name, size_t target_data_size)
{
	struct ipt_entry *entry;
	struct ipt_entry_target *target;
	size_t target_size = 0;

	target_size += IPT_ALIGN(sizeof(struct ipt_entry_target));
	target_size += IPT_ALIGN(target_data_size);

	if (!(entry = realloc(*pentry, (*pentry)->next_offset + target_size))) {
		perror("realloc");
		return NULL;
	}

	target = (struct ipt_entry_target *) ((int) entry + entry->next_offset);
	entry->next_offset += target_size;
	memset(target, 0, target_size);

	strncpy(target->u.user.name, name, IPT_FUNCTION_MAXNAMELEN);
	target->u.target_size = target_size;

	*pentry = entry;
	return target;
}

/*
 * Insert an entry into a reasonable location in the chain
 * @param	chain	chain name
 * @param	entry	iptables entry
 * @param	handle	table handle
 * @return	TRUE on success and 0 on failure
 */
static int
#ifdef LINUX26
#ifndef LINUX_2_6_36
insert_entry(const char *chain, const char *target_name, struct ipt_entry *entry, iptc_handle_t *handle)
#else
insert_entry(const char *chain, const char *target_name, struct ipt_entry *entry, struct iptc_handle *handle)
#endif /* linux-2.6.36 */
#else /* LINUX26 */
insert_entry(const char *chain, struct ipt_entry *entry, iptc_handle_t *handle)
#endif /* LINUX26 */
{
	int i;
	struct ipt_ip blank;
	const struct ipt_entry *rule;
	struct ipt_entry_target *target;

	target = (struct ipt_entry_target *) ((int) entry + entry->target_offset);
	memset(&blank, 0, sizeof(struct ipt_ip));

	/* If this is a default policy (no match) insert at the end of the chain */
	if (entry->target_offset == sizeof(struct ipt_entry) &&
	    !memcmp(&entry->ip, &blank, sizeof(struct ipt_ip)))
		return iptc_append_entry(chain, entry, handle);

	/* If dropping insert at the beginning of the chain */
#ifdef LINUX26
	if (!strcmp(target_name, "DROP") ||
	    !strcmp(target_name, "logdrop"))
		return iptc_insert_entry(chain, entry, 0, handle);
	/* If accepting insert after the last drop but before the first default policy */
	else if (!strcmp(target_name, "ACCEPT") ||
		 !strcmp(target_name, "logaccept")) {
#else /* LINUX26 */
	if (!strcmp(iptc_get_target(entry, handle), "DROP") ||
	    !strcmp(iptc_get_target(entry, handle), "logdrop"))
		return iptc_insert_entry(chain, entry, 0, handle);
	/* If accepting insert after the last drop but before the first default policy */
	else if (!strcmp(iptc_get_target(entry, handle), "ACCEPT") ||
		 !strcmp(iptc_get_target(entry, handle), "logaccept")) {
#endif /* LINUX26 */
		for (i = 0, rule = iptc_first_rule(chain, handle); rule;
		     i++, rule = iptc_next_rule(rule, handle)) {
			if ((strcmp(iptc_get_target(rule, handle), "DROP") &&
			     strcmp(iptc_get_target(rule, handle), "logdrop")) ||
			    (rule->target_offset == sizeof(struct ipt_entry) &&
			     !memcmp(&rule->ip, &blank, sizeof(struct ipt_ip))))
				break;
		}
		return iptc_insert_entry(chain, entry, i, handle);
	}

	/* Otherwise insert at the end of the chain */
	else
		return iptc_append_entry(chain, entry, handle);
}

/*
 * Add a firewall entry
 * @param	fw	firewall entry
 * @return	0 on success and errno on failure
 */
int
netconf_add_fw(netconf_fw_t *fw)
{
	netconf_filter_t *filter = NULL;
	netconf_nat_t *nat = NULL;
	netconf_app_t *app = NULL;

	struct ipt_entry *entry;
	struct ipt_entry_match *match;
	struct ipt_entry_target *target;
#ifndef LINUX_2_6_36
	iptc_handle_t handle = NULL;
#else
	struct iptc_handle *handle = NULL;
#endif

	if (!netconf_valid_ipproto(fw->match.ipproto)) {
		fprintf(stderr, "invalid IP protocol %d\n", fw->match.ipproto);
		return -EINVAL;
	}

	if (!netconf_valid_target(fw->target)) {
		fprintf(stderr, "invalid target type %d\n", fw->target);
		return EINVAL;
	}

	/* Only know about specified target types */
	if (netconf_valid_filter(fw->target))
		filter = (netconf_filter_t *) fw;
	else if (netconf_valid_nat(fw->target))
		nat = (netconf_nat_t *) fw;
	else if (fw->target == NETCONF_APP)
		app = (netconf_app_t *) fw;
	else
		return EINVAL;

	/* Allocate entry */
	if (!(entry = calloc(1, sizeof(struct ipt_entry)))) {
		perror("calloc");
		return errno;
	}

	/* Initialize entry parameters */
	entry->nfcache |= NFC_UNKNOWN;
	entry->next_offset = entry->target_offset = sizeof(struct ipt_entry);

	if (nat && (nat->type == NETCONF_CONE_NAT))
		entry->nfcache |= NETCONF_CONE_NAT;

	/* Match by IP address(es) */
	if (fw->match.src.ipaddr.s_addr & fw->match.src.netmask.s_addr) {
		entry->ip.src.s_addr = fw->match.src.ipaddr.s_addr;
		entry->ip.smsk.s_addr = fw->match.src.netmask.s_addr;
		entry->nfcache |= NFC_IP_SRC;
		entry->ip.invflags |= (fw->match.flags & NETCONF_INV_SRCIP) ? IPT_INV_SRCIP : 0;
	}
	if (fw->match.dst.ipaddr.s_addr & fw->match.dst.netmask.s_addr) {
		entry->ip.dst.s_addr = fw->match.dst.ipaddr.s_addr;
		entry->ip.dmsk.s_addr = fw->match.dst.netmask.s_addr;
		entry->nfcache |= NFC_IP_DST;
		entry->ip.invflags |= (fw->match.flags & NETCONF_INV_DSTIP) ? IPT_INV_DSTIP : 0;
	}

	/* Match by inbound or outbound interface name */
	if (strlen(fw->match.in.name) > 0) {
		strncpy(entry->ip.iniface, fw->match.in.name, IFNAMSIZ);
		memset(&entry->ip.iniface_mask, 0, IFNAMSIZ);
		memset(&entry->ip.iniface_mask, 0xff, strlen(fw->match.in.name) + 1);
		entry->ip.invflags |= (fw->match.flags & NETCONF_INV_IN) ? IPT_INV_VIA_IN : 0;
		entry->nfcache |= NFC_IP_IF_IN;
	}
	if (strlen(fw->match.out.name) > 0) {
		strncpy(entry->ip.outiface, fw->match.out.name, IFNAMSIZ);
		memset(&entry->ip.outiface_mask, 0, IFNAMSIZ);
		memset(&entry->ip.outiface_mask, 0xff, strlen(fw->match.in.name) + 1);
		entry->ip.invflags |= (fw->match.flags & NETCONF_INV_IN) ? IPT_INV_VIA_OUT : 0;
		entry->nfcache |= NFC_IP_IF_OUT;
	}

	/* Match by TCP port(s) */
	if (fw->match.ipproto == IPPROTO_TCP) {
		struct ipt_tcp *tcp;

		if (!(match = netconf_append_match(&entry, "tcp", sizeof(struct ipt_tcp))))
			goto err;
		tcp = (struct ipt_tcp *) &match->data[0];

		entry->ip.proto = IPPROTO_TCP;
		entry->nfcache |= NFC_IP_PROTO;

		/* Match ports stored in host order for some stupid reason */
		tcp->spts[0] = ntohs(fw->match.src.ports[0]);
		tcp->spts[1] = ntohs(fw->match.src.ports[1]);
		tcp->invflags |= (fw->match.flags & NETCONF_INV_SRCPT) ? IPT_TCP_INV_SRCPT : 0;
		entry->nfcache |= (tcp->spts[0] != 0 || tcp->spts[1] != 0xffff) ? NFC_IP_SRC_PT : 0;
		
		/* Match ports stored in host order for some stupid reason */
		tcp->dpts[0] = ntohs(fw->match.dst.ports[0]);
		tcp->dpts[1] = ntohs(fw->match.dst.ports[1]);
		tcp->invflags |= (fw->match.flags & NETCONF_INV_DSTPT) ? IPT_TCP_INV_DSTPT : 0;
		entry->nfcache |= (tcp->dpts[0] != 0 || tcp->dpts[1] != 0xffff) ? NFC_IP_DST_PT : 0;
	}

	/* Match by UDP port(s) */
	else if (fw->match.ipproto == IPPROTO_UDP) {
		struct ipt_udp *udp;

		if (!(match = netconf_append_match(&entry, "udp", sizeof(struct ipt_udp))))
			goto err;
		udp = (struct ipt_udp *) &match->data[0];

		entry->ip.proto = IPPROTO_UDP;
		entry->nfcache |= NFC_IP_PROTO;

		/* Match ports stored in host order for some stupid reason */
		udp->spts[0] = ntohs(fw->match.src.ports[0]);
		udp->spts[1] = ntohs(fw->match.src.ports[1]);
		udp->invflags |= (fw->match.flags & NETCONF_INV_SRCPT) ? IPT_UDP_INV_SRCPT : 0;
		entry->nfcache |= (udp->spts[0] != 0 || udp->spts[1] != 0xffff) ? NFC_IP_SRC_PT : 0;
		
		/* Match ports stored in host order for some stupid reason */
		udp->dpts[0] = ntohs(fw->match.dst.ports[0]);
		udp->dpts[1] = ntohs(fw->match.dst.ports[1]);
		udp->invflags |= (fw->match.flags & NETCONF_INV_DSTPT) ? IPT_UDP_INV_DSTPT : 0;
		entry->nfcache |= (udp->dpts[0] != 0 || udp->dpts[1] != 0xffff) ? NFC_IP_DST_PT : 0;
	}
	
	/* Match by source MAC address */
	if (!ETHER_ISNULLADDR(fw->match.mac.octet)) {
		struct ipt_mac_info *mac;

		if (!(match = netconf_append_match(&entry, "mac", sizeof(struct ipt_mac_info))))
			goto err;
		mac = (struct ipt_mac_info *) &match->data[0];

		memcpy(mac->srcaddr, fw->match.mac.octet, ETHER_ADDR_LEN);
		mac->invert = (fw->match.flags & NETCONF_INV_MAC) ? 1 : 0;
	}

	/* Match by packet state */
	if (fw->match.state) {
		struct ipt_state_info *state;

		if (!(match = netconf_append_match(&entry, "state", sizeof(struct ipt_state_info))))
			goto err;
		state = (struct ipt_state_info *) &match->data[0];

		state->statemask |= (fw->match.state & NETCONF_INVALID) ? IPT_STATE_INVALID : 0;
		state->statemask |= (fw->match.state & NETCONF_ESTABLISHED) ? IPT_STATE_BIT(IP_CT_ESTABLISHED) : 0;
		state->statemask |= (fw->match.state & NETCONF_RELATED) ? IPT_STATE_BIT(IP_CT_RELATED) : 0;
		state->statemask |= (fw->match.state & NETCONF_NEW) ? IPT_STATE_BIT(IP_CT_NEW) : 0;
	}		

	/* Match by local time */
	if (fw->match.secs[0] || fw->match.secs[1]) {
		time_info_t *time;
#ifndef LINUX_2_6_36
		unsigned int *days;
		int i;
#endif
		if (fw->match.secs[0] >= (24 * 60 * 60) || fw->match.secs[1] >= (24 * 60 * 60) ||
		    fw->match.days[0] >= 7 || fw->match.days[1] >= 7) {
			fprintf(stderr, "invalid time %d-%d:%d-%d\n",
				fw->match.days[0], fw->match.days[1],
				fw->match.secs[0], fw->match.secs[1]);
			goto err;
		}

		if (!(match = netconf_append_match(&entry, "time", sizeof(time_info_t) + TIME_INFO_EXTRA_BYTES)))
			goto err;
		time = (time_info_t *) &match->data[0];

#ifndef LINUX_2_6_36
		days = (unsigned int *) &time[1];
		days[0] = fw->match.days[0];
		days[1] = fw->match.days[1];

		for (i = fw->match.days[0]; i != fw->match.days[1]; i = (i + 1) % 7)
			time->days_match |= (1 << i);
		time->days_match |= (1 << fw->match.days[1]);
		time->time_start = fw->match.secs[0];
		time->time_stop = fw->match.secs[1];
#else
		/* We don't need absolute date match */
		time->date_start = 0;
		time->date_stop = ~0U;
		/* Seconds per day */
		time->daytime_start = fw->match.secs[0];
		time->daytime_stop = fw->match.secs[1];
		/* We don't need month days match */
		time->monthdays_match = XT_TIME_ALL_MONTHDAYS;
		/* Week days match */
		set_days(fw->match.days, time);
#endif
	}

	/* Allocate target */
	if (!(target = netconf_append_target(&entry, ipt_target_name[fw->target], ipt_target_size[fw->target])))
		goto err;

	if (!(handle = iptc_init(ipt_table_name[fw->target]))) {
		fprintf(stderr, "%s\n", iptc_strerror(errno));
		goto err;
	}

	/* Set filter target information */
	if (filter) {
		if (!netconf_valid_dir(filter->dir)) {
			fprintf(stderr, "invalid filter direction %d\n", filter->dir);
			goto err;
		}

#ifdef LINUX26
#ifndef LINUX_2_6_36
		if (!insert_entry(ipt_filter_chain_name[filter->dir], ipt_target_name[fw->target], entry, &handle)) {
#else
		if (!insert_entry(ipt_filter_chain_name[filter->dir], ipt_target_name[fw->target], entry, handle)) {
#endif
#else /* LINUX26 */
		if (!insert_entry(ipt_filter_chain_name[filter->dir], entry, &handle)) {
#endif /* LINUX26 */
			fprintf(stderr, "%s\n", iptc_strerror(errno));
			goto err;
		}
	}

	/* Set NAT target information */
	else if (nat) {
#ifndef LINUX_2_6_36
		struct ip_nat_multi_range *mr = (struct ip_nat_multi_range *) &target->data[0];
		struct ip_nat_range *range = (struct ip_nat_range *) &mr->range[0];
#else
		struct nf_nat_multi_range *mr = (struct nf_nat_multi_range *) &target->data[0];
		struct nf_nat_range *range = (struct nf_nat_range *) &mr->range[0];
#endif /* linux-2.6.36 */
		
		mr->rangesize = 1;

		/* Map to IP address */
		if (nat->ipaddr.s_addr) {
			range->min_ip = range->max_ip = nat->ipaddr.s_addr;
			range->flags |= IP_NAT_RANGE_MAP_IPS;
		}

		/* Map to TCP port(s) */
		if (nat->match.ipproto == IPPROTO_TCP) {
			range->min.tcp.port = nat->ports[0];
			range->max.tcp.port = nat->ports[1];
			range->flags |= IP_NAT_RANGE_PROTO_SPECIFIED;
		}

		/* Map to UDP port(s) */
		else if (nat->match.ipproto == IPPROTO_UDP) {
			range->min.udp.port = nat->ports[0];
			range->max.udp.port = nat->ports[1];
			range->flags |= IP_NAT_RANGE_PROTO_SPECIFIED;
		}

#ifdef LINUX26
#ifndef LINUX_2_6_36
		if (!insert_entry(ipt_nat_chain_name[fw->target], ipt_target_name[fw->target], entry, &handle)) {
#else
		if (!insert_entry(ipt_nat_chain_name[fw->target], ipt_target_name[fw->target], entry, handle)) {
#endif /* linux-2.6.36 */
#else /* LINUX26 */
		if (!insert_entry(ipt_nat_chain_name[fw->target], entry, &handle)) {
#endif /* LINUX26 */
			fprintf(stderr, "%s\n", iptc_strerror(errno));
			goto err;
		}
	}

	else if (app) {
		struct ip_autofw_info *info = (struct ip_autofw_info *) &target->data[0];

		info->proto = app->proto;
		info->dport[0] = app->dport[0];
		info->dport[1] = app->dport[1];
		info->to[0] = app->to[0];
		info->to[1] = app->to[1];

#ifdef LINUX26
#ifndef LINUX_2_6_36
		if (!insert_entry("PREROUTING", ipt_target_name[fw->target], entry, &handle)) {
#else
		if (!insert_entry("PREROUTING", ipt_target_name[fw->target], entry, handle)) {
#endif /* linux-2.6.36 */
#else /* LINUX26 */
		if (!insert_entry("PREROUTING", entry, &handle)) {
#endif /* LINUX26 */
			fprintf(stderr, "%s\n", iptc_strerror(errno));
			goto err;
		}
	}

#ifndef LINUX_2_6_36
	if (!iptc_commit(&handle)) {
#else
	if (!iptc_commit(handle)) {
#endif /* linux-2.6.36 */
		fprintf(stderr, "%s\n", iptc_strerror(errno));
		goto err;
	}

#ifdef LINUX_2_6_36
	iptc_free(handle);
#endif
	free(entry);
	return 0;

 err:
	if (handle)
#ifndef LINUX_2_6_36
		iptc_commit(&handle);
#else
		iptc_commit(handle);
#endif /* linux-2.6.36 */
#ifdef LINUX_2_6_36
	if (handle)
		iptc_free(handle);
#endif
	free(entry);
	return errno;
}

/*
 * Delete a firewall entry
 * @param	fw	firewall entry
 * @return	0 on success and errno on failure
 */
int
netconf_del_fw(netconf_fw_t *fw)
{
	int num;
	const char *chain;
#ifndef LINUX_2_6_36
	iptc_handle_t handle;
#else
	struct iptc_handle *handle;
#endif /* linux-2.6.36 */

	/* netconf_fw_index() sanity checks fw */
	if ((num = netconf_fw_index(fw)) < 0)
		return -num;

	/* Pick the right chain name */
	if (netconf_valid_filter(fw->target))
		chain = ipt_filter_chain_name[((netconf_filter_t *) fw)->dir];
	else if (netconf_valid_nat(fw->target))
		chain = ipt_nat_chain_name[fw->target];
	else if (fw->target == NETCONF_APP)
		chain = "PREROUTING";
	else
		return EINVAL;
		
	/* Commit changes */
	if (!(handle = iptc_init(ipt_table_name[fw->target])) ||
#ifndef LINUX_2_6_36
	    !iptc_delete_num_entry(chain, num, &handle) ||
	    !iptc_commit(&handle)) {
#else
	    !iptc_delete_num_entry(chain, num, handle) ||
	    !iptc_commit(handle)) {
#endif /* linux-2.6.36 */
#ifdef LINUX_2_6_36
		if (handle)
			iptc_free(handle);
#endif
		fprintf(stderr, "%s\n", iptc_strerror(errno));
		return errno;
	}

#ifdef LINUX_2_6_36
	iptc_free(handle);
#endif
	return 0;
}

/*
 * Add or delete a firewall entry or list of firewall entries
 * @param	fw_list	firewall entry or list of firewall entries
 * @bool	del	whether to delete or add
 * @return	0 on success and errno on failure
 */
static int
netconf_manip_fw(netconf_fw_t *fw_list, bool del)
{
	netconf_fw_t *fw;
	int ret;

	/* Single firewall entry */
	if (netconf_list_empty(fw_list) || !fw_list->next)
		return (del ? netconf_del_fw(fw_list) : netconf_add_fw(fw_list));

	/* List of firewall entries */
	netconf_list_for_each(fw, fw_list) {
		if ((ret = del ? netconf_del_fw(fw) : netconf_add_fw(fw)))
			return ret;
	}
	
	return 0;
}

/*
 * Add a NAT entry or list of NAT entries
 * @param	nat_list	NAT entry or list of NAT entries
 * @return	0 on success and errno on failure
 */
int
netconf_add_nat(netconf_nat_t *nat_list)
{
	return netconf_manip_fw((netconf_fw_t *) nat_list, 0);
}

/*
 * Delete a NAT entry or list of NAT entries
 * @param	nat_list	NAT entry or list of NAT entries
 * @return	0 on success and errno on failure
 */
int
netconf_del_nat(netconf_nat_t *nat_list)
{
	return netconf_manip_fw((netconf_fw_t *) nat_list, 1);
}

/*
 * Get an array of the current NAT entries
 * @param	nat_array	array of NAT entries
 * @param	space		Pointer to size of nat_array in bytes
 * @return 0 on success and errno on failure
 */
int
netconf_get_nat(netconf_nat_t *nat_array, int *space)
{
	netconf_fw_t *fw, fw_list;
	int ret;
	int found = 0;
	
	if ((ret = netconf_get_fw(&fw_list)))
		return ret;
		
	netconf_list_for_each(fw, &fw_list) {
		if (netconf_valid_nat(fw->target)) {
			found++;
			if (*space && *space >= (found * sizeof(netconf_nat_t)))
				memcpy(&nat_array[found - 1], (netconf_nat_t *) fw, sizeof(netconf_nat_t));
		}
	}

	if (!*space)
		*space = found * sizeof(netconf_nat_t);

	netconf_list_free(&fw_list);
	return 0;
}			

/*
 * Add a filter entry or list of filter entries
 * @param	filter_list	filter entry or list of filter entries
 * @return	0 on success and errno on failure
 */
int
netconf_add_filter(netconf_filter_t *filter_list)
{
	return netconf_manip_fw((netconf_fw_t *) filter_list, 0);
}

/*
 * Delete a filter entry or list of filter entries
 * @param	filter_list	filter entry or list of filter entries
 * @return	0 on success and errno on failure
 */
int
netconf_del_filter(netconf_filter_t *filter_list)
{
	return netconf_manip_fw((netconf_fw_t *) filter_list, 1);
}

/*
 * Get an array of the current filter entries
 * @param	filter_array	array of filter entries
 * @param	space		Pointer to size of filter_array in bytes
 * @return 0 on success and errno on failure
 */
int
netconf_get_filter(netconf_filter_t *filter_array, int *space)
{
	netconf_fw_t *fw, fw_list;
	int ret;
	int found = 0;
	
	if ((ret = netconf_get_fw(&fw_list)))
		return ret;
		
	netconf_list_for_each(fw, &fw_list) {
		if (netconf_valid_filter(fw->target)) {
			found++;
			if (*space && *space >= (found * sizeof(netconf_filter_t)))
				memcpy(&filter_array[found - 1], (netconf_filter_t *) fw, sizeof(netconf_filter_t));
		}
	}

	if (!*space)
		*space = found * sizeof(netconf_filter_t);

	netconf_list_free(&fw_list);
	return 0;
}			

/*
 * Generates an ipt_entry with an optional match and one target
 * @param match_name		match name
 * @param match_data		match data
 * @param match_data_size	match data size
 * @param target_name		target name
 * @param target_data		target data
 * @param target_data_size	target data size
 * @return newly allocated and initialized ipt_entry
 */
static struct ipt_entry *
netconf_generate_entry(const char *match_name, const void *match_data, size_t match_data_size,
		       const char *target_name, const void *target_data, size_t target_data_size)
{
	struct ipt_entry *entry;
	struct ipt_entry_match *match;
	struct ipt_entry_target *target;

	/* Allocate entry */
	if (!(entry = calloc(1, sizeof(struct ipt_entry)))) {
		perror("calloc");
		return NULL;
	}

	/* Initialize entry parameters */
	entry->next_offset = entry->target_offset = sizeof(struct ipt_entry);

	/* Allocate space for and copy match data */
	if (match_data) {
		if (!(match = netconf_append_match(&entry, match_name, match_data_size)))
			goto err;
		memcpy(&match->data[0], match_data, match_data_size);
	}

	/* Allocate space for and copy target data */
	if (!(target = netconf_append_target(&entry, target_name, target_data_size)))
		goto err;
	memcpy(&target->data[0], target_data, target_data_size);

	return entry;

 err:
	free(entry);
	return NULL;
}

static int
netconf_reset_chain(char *table, char *chain)
{
#ifndef LINUX_2_6_36
	iptc_handle_t handle = NULL;
#else
	struct iptc_handle *handle = NULL;	
#endif /* linux-2.6.36 */

	/* Get handle to table */
	if (!(handle = iptc_init(table)))
		goto err;

	/* Create chain if necessary */
	if (!iptc_is_chain(chain, handle))
#ifndef LINUX_2_6_36
		if (!iptc_create_chain(chain, &handle))
#else
		if (!iptc_create_chain(chain, handle))
#endif /* linux-2.6.36 */
			goto err;

	/* Flush entries and commit */
#ifndef LINUX_2_6_36
	if (!iptc_flush_entries(chain, &handle) ||
	    !iptc_commit(&handle))
#else
	if (!iptc_flush_entries(chain, handle) ||
	    !iptc_commit(handle))
#endif /* linux-2.6.36 */
		goto err;

#ifdef LINUX_2_6_36
	iptc_free(handle);
#endif
	return 0;

 err:
	if (handle)
#ifndef LINUX_2_6_36
		iptc_commit(&handle);
#else
		iptc_commit(handle);
#endif /* linux-2.6.36 */
#ifdef LINUX_2_6_36
	if (handle)
		iptc_free(handle);
#endif
	fprintf(stderr, "%s\n", iptc_strerror(errno));
	return errno;
}

/*
 * Reset the firewall to a sane state
 * @return	0 on success and errno on failure
 */
int
netconf_reset_fw(void)
{
#ifndef LINUX_2_6_36
	iptc_handle_t handle = NULL;
#else
	struct iptc_handle *handle = NULL;	
#endif /* linux-2.6.36 */
	struct ipt_entry *entry = NULL;
	struct ipt_state_info state;
	struct ipt_log_info log;
	int ret, unused;

	/* Reset default chains */
	if ((ret = netconf_reset_chain("filter", "INPUT")) ||
	    (ret = netconf_reset_chain("filter", "FORWARD")) ||
	    (ret = netconf_reset_chain("filter", "OUTPUT")) ||
	    (ret = netconf_reset_chain("nat", "PREROUTING")) ||
	    (ret = netconf_reset_chain("nat", "POSTROUTING")) ||
	    (ret = netconf_reset_chain("nat", "OUTPUT")))
		return ret;

	/* Reset custom chains */
	if ((ret = netconf_reset_chain("filter", "logdrop")) ||
	    (ret = netconf_reset_chain("filter", "logaccept")))
		goto err;

	/* Log only when a connection is attempted */
	memset(&state, 0, sizeof(state));
	state.statemask = IPT_STATE_BIT(IP_CT_NEW);

	/* Set miscellaneous log parameters */
	memset(&log, 0, sizeof(log));
	log.level = LOG_WARNING;
	log.logflags = 0xf;

	/* Log packet */
	strncpy(log.prefix, "DROP ", sizeof(log.prefix));
	if (!(entry = netconf_generate_entry("state", &state, sizeof(state), "LOG", &log, sizeof(log))))
		return ENOMEM;
	entry->nfcache |= NFC_UNKNOWN;
	if (!(handle = iptc_init("filter")) ||
#ifndef LINUX_2_6_36
	    !iptc_insert_entry("logdrop", entry, 0, &handle) ||
	    !iptc_commit(&handle))
#else
	    !iptc_insert_entry("logdrop", entry, 0, handle) ||
	    !iptc_commit(handle))
#endif /* linux-2.6.36 */
		goto err;
#ifdef LINUX_2_6_36
	iptc_free(handle);
#endif
	free(entry);

	/* Drop packet */
	if (!(entry = netconf_generate_entry(NULL, NULL, 0, "DROP", &unused, sizeof(unused))))
		return ENOMEM;
	entry->nfcache |= NFC_UNKNOWN;
	if (!(handle = iptc_init("filter")) ||
#ifndef LINUX_2_6_36
	    !iptc_insert_entry("logdrop", entry, 1, &handle) ||
	    !iptc_commit(&handle))
#else
	    !iptc_insert_entry("logdrop", entry, 1, handle) ||
	    !iptc_commit(handle))
#endif /* linux-2.6.36 */
		goto err;
#ifdef LINUX_2_6_36
	iptc_free(handle);
#endif
	free(entry);

	/* Log packet */
	strncpy(log.prefix, "ACCEPT ", sizeof(log.prefix));
	if (!(entry = netconf_generate_entry("state", &state, sizeof(state), "LOG", &log, sizeof(log))))
		return ENOMEM;
	entry->nfcache |= NFC_UNKNOWN;
	if (!(handle = iptc_init("filter")) ||
#ifndef LINUX_2_6_36
	    !iptc_insert_entry("logaccept", entry, 0, &handle) ||
	    !iptc_commit(&handle))
#else
	    !iptc_insert_entry("logaccept", entry, 0, handle) ||
	    !iptc_commit(handle))
#endif /* linux-2.6.36 */
		goto err;
#ifdef LINUX_2_6_36
	iptc_free(handle);
#endif
	free(entry);

	/* Accept packet */
	if (!(entry = netconf_generate_entry(NULL, NULL, 0, "ACCEPT", &unused, sizeof(unused))))
		return ENOMEM;
	entry->nfcache |= NFC_UNKNOWN;
	if (!(handle = iptc_init("filter")) ||
#ifndef LINUX_2_6_36
	    !iptc_insert_entry("logaccept", entry, 1, &handle) ||
	    !iptc_commit(&handle))
#else
	    !iptc_insert_entry("logaccept", entry, 1, handle) ||
	    !iptc_commit(handle))
#endif /* linux-2.6.36 */
		goto err;
#ifdef LINUX_2_6_36
	iptc_free(handle);
#endif
	free(entry);

	return 0;

 err:
#ifdef LINUX_2_6_36
	if (handle)
		iptc_free(handle);
#endif
	if (entry)
		free(entry);
	fprintf(stderr, "%s\n", iptc_strerror(errno));
	return errno;
}

/* 
 * Below are miscellaneous functions that do not fit into the grand
 * scheme of netconf
 */

/*
 * Clamp TCP MSS value to PMTU of interface (for masquerading through PPPoE)
 * @return	0 on success and errno on failure
 */
int
netconf_clamp_mss_to_pmtu(void)
{
	struct ipt_entry *entry;
#ifndef LINUX_2_6_36
	iptc_handle_t handle;
#else
	struct iptc_handle *handle = NULL;
#endif /* linux-2.6.36 */
	struct ipt_tcp tcp;
	struct ipt_tcpmss_info tcpmss;

	/* Match on SYN=1 RST=0 */
	memset(&tcp, 0, sizeof(tcp));
	tcp.spts[1] = tcp.dpts[1] = 0xffff;
	tcp.flg_mask = TH_SYN | TH_RST;
	tcp.flg_cmp = TH_SYN;

	/* Clamp TCP MSS to PMTU */
	memset(&tcpmss, 0, sizeof(tcpmss));
	tcpmss.mss = IPT_TCPMSS_CLAMP_PMTU;

	/* Generate and complete the entry */
	if (!(entry = netconf_generate_entry("tcp", &tcp, sizeof(tcp), "TCPMSS", &tcpmss, sizeof(tcpmss))))
		return ENOMEM;
	entry->ip.proto = IPPROTO_TCP;
	entry->nfcache |= NFC_IP_PROTO | NFC_IP_TCPFLAGS;

	/* Do it */
	if (!(handle = iptc_init("filter")) ||
#ifndef LINUX_2_6_36
	    !iptc_insert_entry("FORWARD", entry, 0, &handle) ||
	    !iptc_commit(&handle)) {
#else
	    !iptc_insert_entry("FORWARD", entry, 0, handle) ||
	    !iptc_commit(handle)) {
#endif /* linux-2.6.36 */
#ifdef LINUX_2_6_36
		if (handle)
			iptc_free(handle);
#endif
		fprintf(stderr, "%s\n", iptc_strerror(errno));
		free(entry);
		return errno;
	}

#ifdef LINUX_2_6_36
	if (handle)
		iptc_free(handle);
#endif
	free(entry);
	return 0;
}	
