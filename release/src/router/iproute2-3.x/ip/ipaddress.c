/*
 * ipaddress.c		"ip address".
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fnmatch.h>

#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>

#include "rt_names.h"
#include "utils.h"
#include "ll_map.h"
#include "ip_common.h"

enum {
	IPADD_LIST,
	IPADD_FLUSH,
	IPADD_SAVE,
};

static struct
{
	int ifindex;
	int family;
	int oneline;
	int showqueue;
	inet_prefix pfx;
	int scope, scopemask;
	int flags, flagmask;
	int up;
	char *label;
	int flushed;
	char *flushb;
	int flushp;
	int flushe;
	int group;
	int master;
	char *kind;
} filter;

static int do_link;

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	if (do_link) {
		iplink_usage();
	}
	fprintf(stderr, "Usage: ip addr {add|change|replace} IFADDR dev STRING [ LIFETIME ]\n");
	fprintf(stderr, "                                                      [ CONFFLAG-LIST ]\n");
	fprintf(stderr, "       ip addr del IFADDR dev STRING [mngtmpaddr]\n");
	fprintf(stderr, "       ip addr {show|save|flush} [ dev STRING ] [ scope SCOPE-ID ]\n");
	fprintf(stderr, "                            [ to PREFIX ] [ FLAG-LIST ] [ label PATTERN ] [up]\n");
	fprintf(stderr, "       ip addr {showdump|restore}\n");
	fprintf(stderr, "IFADDR := PREFIX | ADDR peer PREFIX\n");
	fprintf(stderr, "          [ broadcast ADDR ] [ anycast ADDR ]\n");
	fprintf(stderr, "          [ label STRING ] [ scope SCOPE-ID ]\n");
	fprintf(stderr, "SCOPE-ID := [ host | link | global | NUMBER ]\n");
	fprintf(stderr, "FLAG-LIST := [ FLAG-LIST ] FLAG\n");
	fprintf(stderr, "FLAG  := [ permanent | dynamic | secondary | primary |\n");
	fprintf(stderr, "           [-]tentative | [-]deprecated | [-]dadfailed | temporary |\n");
	fprintf(stderr, "           CONFFLAG-LIST ]\n");
	fprintf(stderr, "CONFFLAG-LIST := [ CONFFLAG-LIST ] CONFFLAG\n");
	fprintf(stderr, "CONFFLAG  := [ home | nodad | mngtmpaddr | noprefixroute ]\n");
	fprintf(stderr, "LIFETIME := [ valid_lft LFT ] [ preferred_lft LFT ]\n");
	fprintf(stderr, "LFT := forever | SECONDS\n");

	exit(-1);
}

static void print_link_flags(FILE *fp, unsigned flags, unsigned mdown)
{
	fprintf(fp, "<");
	if (flags & IFF_UP && !(flags & IFF_RUNNING))
		fprintf(fp, "NO-CARRIER%s", flags ? "," : "");
	flags &= ~IFF_RUNNING;
#define _PF(f) if (flags&IFF_##f) { \
		  flags &= ~IFF_##f ; \
		  fprintf(fp, #f "%s", flags ? "," : ""); }
	_PF(LOOPBACK);
	_PF(BROADCAST);
	_PF(POINTOPOINT);
	_PF(MULTICAST);
	_PF(NOARP);
	_PF(ALLMULTI);
	_PF(PROMISC);
	_PF(MASTER);
	_PF(SLAVE);
	_PF(DEBUG);
	_PF(DYNAMIC);
	_PF(AUTOMEDIA);
	_PF(PORTSEL);
	_PF(NOTRAILERS);
	_PF(UP);
	_PF(LOWER_UP);
	_PF(DORMANT);
	_PF(ECHO);
#undef _PF
	if (flags)
		fprintf(fp, "%x", flags);
	if (mdown)
		fprintf(fp, ",M-DOWN");
	fprintf(fp, "> ");
}

static const char *oper_states[] = {
	"UNKNOWN", "NOTPRESENT", "DOWN", "LOWERLAYERDOWN",
	"TESTING", "DORMANT",	 "UP"
};

static void print_operstate(FILE *f, __u8 state)
{
	if (state >= sizeof(oper_states)/sizeof(oper_states[0]))
		fprintf(f, "state %#x ", state);
	else
		fprintf(f, "state %s ", oper_states[state]);
}

int get_operstate(const char *name)
{
	int i;

	for (i = 0; i < sizeof(oper_states)/sizeof(oper_states[0]); i++)
		if (strcasecmp(name, oper_states[i]) == 0)
			return i;
	return -1;
}

static void print_queuelen(FILE *f, struct rtattr *tb[IFLA_MAX + 1])
{
	int qlen;

	if (tb[IFLA_TXQLEN])
		qlen = *(int *)RTA_DATA(tb[IFLA_TXQLEN]);
	else {
		struct ifreq ifr;
		int s = socket(AF_INET, SOCK_STREAM, 0);

		if (s < 0)
			return;

		memset(&ifr, 0, sizeof(ifr));
		strcpy(ifr.ifr_name, rta_getattr_str(tb[IFLA_IFNAME]));
		if (ioctl(s, SIOCGIFTXQLEN, &ifr) < 0) {
			fprintf(f, "ioctl(SIOCGIFTXQLEN) failed: %s\n", strerror(errno));
			close(s);
			return;
		}
		close(s);
		qlen = ifr.ifr_qlen;
	}
	if (qlen)
		fprintf(f, "qlen %d", qlen);
}

static const char *link_modes[] = {
	"DEFAULT", "DORMANT"
};

static void print_linkmode(FILE *f, struct rtattr *tb)
{
	unsigned int mode = rta_getattr_u8(tb);

	if (mode >= sizeof(link_modes) / sizeof(link_modes[0]))
		fprintf(f, "mode %d ", mode);
	else
		fprintf(f, "mode %s ", link_modes[mode]);
}

static char *parse_link_kind(struct rtattr *tb)
{
	struct rtattr *linkinfo[IFLA_INFO_MAX+1];

	parse_rtattr_nested(linkinfo, IFLA_INFO_MAX, tb);

	if (linkinfo[IFLA_INFO_KIND])
		return RTA_DATA(linkinfo[IFLA_INFO_KIND]);

	return "";
}

static void print_linktype(FILE *fp, struct rtattr *tb)
{
	struct rtattr *linkinfo[IFLA_INFO_MAX+1];
	struct link_util *lu;
	struct link_util *slave_lu;
	char *kind;
	char *slave_kind;

	parse_rtattr_nested(linkinfo, IFLA_INFO_MAX, tb);

	if (linkinfo[IFLA_INFO_KIND]) {
		kind = RTA_DATA(linkinfo[IFLA_INFO_KIND]);

		fprintf(fp, "%s", _SL_);
		fprintf(fp, "    %s ", kind);

		lu = get_link_kind(kind);
		if (lu && lu->print_opt) {
			struct rtattr *attr[lu->maxattr+1], **data = NULL;

			if (linkinfo[IFLA_INFO_DATA]) {
				parse_rtattr_nested(attr, lu->maxattr,
						    linkinfo[IFLA_INFO_DATA]);
				data = attr;
			}
			lu->print_opt(lu, fp, data);

			if (linkinfo[IFLA_INFO_XSTATS] && show_stats &&
			    lu->print_xstats)
				lu->print_xstats(lu, fp, linkinfo[IFLA_INFO_XSTATS]);
		}
	}

	if (linkinfo[IFLA_INFO_SLAVE_KIND]) {
		slave_kind = RTA_DATA(linkinfo[IFLA_INFO_SLAVE_KIND]);

		fprintf(fp, "%s", _SL_);
		fprintf(fp, "    %s_slave ", slave_kind);

		slave_lu = get_link_slave_kind(slave_kind);
		if (slave_lu && slave_lu->print_opt) {
			struct rtattr *attr[slave_lu->maxattr+1], **data = NULL;

			if (linkinfo[IFLA_INFO_SLAVE_DATA]) {
				parse_rtattr_nested(attr, slave_lu->maxattr,
						    linkinfo[IFLA_INFO_SLAVE_DATA]);
				data = attr;
			}
			slave_lu->print_opt(slave_lu, fp, data);
		}
	}
}

static void print_af_spec(FILE *fp, struct rtattr *af_spec_attr)
{
	struct rtattr *inet6_attr;
	struct rtattr *tb[IFLA_INET6_MAX + 1];

	inet6_attr = parse_rtattr_one_nested(AF_INET6, af_spec_attr);
	if (!inet6_attr)
		return;

	parse_rtattr_nested(tb, IFLA_INET6_MAX, inet6_attr);

	if (tb[IFLA_INET6_ADDR_GEN_MODE]) {
		switch (rta_getattr_u8(tb[IFLA_INET6_ADDR_GEN_MODE])) {
		case IN6_ADDR_GEN_MODE_EUI64:
			fprintf(fp, "addrgenmode eui64 ");
			break;
		case IN6_ADDR_GEN_MODE_NONE:
			fprintf(fp, "addrgenmode none ");
			break;
		}
	}
}

static void print_vfinfo(FILE *fp, struct rtattr *vfinfo)
{
	struct ifla_vf_mac *vf_mac;
	struct ifla_vf_vlan *vf_vlan;
	struct ifla_vf_tx_rate *vf_tx_rate;
	struct ifla_vf_spoofchk *vf_spoofchk;
	struct ifla_vf_link_state *vf_linkstate;
	struct rtattr *vf[IFLA_VF_MAX + 1] = {};
	struct rtattr *tmp;
	SPRINT_BUF(b1);

	if (vfinfo->rta_type != IFLA_VF_INFO) {
		fprintf(stderr, "BUG: rta type is %d\n", vfinfo->rta_type);
		return;
	}

	parse_rtattr_nested(vf, IFLA_VF_MAX, vfinfo);

	vf_mac = RTA_DATA(vf[IFLA_VF_MAC]);
	vf_vlan = RTA_DATA(vf[IFLA_VF_VLAN]);
	vf_tx_rate = RTA_DATA(vf[IFLA_VF_TX_RATE]);

	/* Check if the spoof checking vf info type is supported by
	 * this kernel.
	 */
	tmp = (struct rtattr *)((char *)vf[IFLA_VF_TX_RATE] +
			vf[IFLA_VF_TX_RATE]->rta_len);

	if (tmp->rta_type != IFLA_VF_SPOOFCHK)
		vf_spoofchk = NULL;
	else
		vf_spoofchk = RTA_DATA(vf[IFLA_VF_SPOOFCHK]);

	if (vf_spoofchk) {
		/* Check if the link state vf info type is supported by
		 * this kernel.
		 */
		tmp = (struct rtattr *)((char *)vf[IFLA_VF_SPOOFCHK] +
				vf[IFLA_VF_SPOOFCHK]->rta_len);

		if (tmp->rta_type != IFLA_VF_LINK_STATE)
			vf_linkstate = NULL;
		else
			vf_linkstate = RTA_DATA(vf[IFLA_VF_LINK_STATE]);
	} else
		vf_linkstate = NULL;

	fprintf(fp, "\n    vf %d MAC %s", vf_mac->vf,
		ll_addr_n2a((unsigned char *)&vf_mac->mac,
		ETH_ALEN, 0, b1, sizeof(b1)));
	if (vf_vlan->vlan)
		fprintf(fp, ", vlan %d", vf_vlan->vlan);
	if (vf_vlan->qos)
		fprintf(fp, ", qos %d", vf_vlan->qos);
	if (vf_tx_rate->rate)
		fprintf(fp, ", tx rate %d (Mbps)", vf_tx_rate->rate);

	if (vf[IFLA_VF_RATE]) {
		struct ifla_vf_rate *vf_rate = RTA_DATA(vf[IFLA_VF_RATE]);

		if (vf_rate->max_tx_rate)
			fprintf(fp, ", max_tx_rate %dMbps", vf_rate->max_tx_rate);
		if (vf_rate->min_tx_rate)
			fprintf(fp, ", min_tx_rate %dMbps", vf_rate->min_tx_rate);
	}

	if (vf_spoofchk && vf_spoofchk->setting != -1) {
		if (vf_spoofchk->setting)
			fprintf(fp, ", spoof checking on");
		else
			fprintf(fp, ", spoof checking off");
	}
	if (vf_linkstate) {
		if (vf_linkstate->link_state == IFLA_VF_LINK_STATE_AUTO)
			fprintf(fp, ", link-state auto");
		else if (vf_linkstate->link_state == IFLA_VF_LINK_STATE_ENABLE)
			fprintf(fp, ", link-state enable");
		else
			fprintf(fp, ", link-state disable");
	}
}

static void print_num(FILE *fp, unsigned width, uint64_t count)
{
	const char *prefix = "kMGTPE";
	const unsigned int base = use_iec ? 1024 : 1000;
	uint64_t powi = 1;
	uint16_t powj = 1;
	uint8_t precision = 2;
	char buf[64];

	if (!human_readable || count < base) {
		fprintf(fp, "%-*"PRIu64" ", width, count);
		return;
	}

	/* increase value by a factor of 1000/1024 and print
	 * if result is something a human can read */
	for(;;) {
		powi *= base;
		if (count / base < powi)
			break;

		if (!prefix[1])
			break;
		++prefix;
	}

	/* try to guess a good number of digits for precision */
	for (; precision > 0; precision--) {
		powj *= 10;
		if (count / powi < powj)
			break;
	}

	snprintf(buf, sizeof(buf), "%.*f%c%s", precision,
		(double) count / powi, *prefix, use_iec ? "i" : "");

	fprintf(fp, "%-*s ", width, buf);
}

static void print_link_stats64(FILE *fp, const struct rtnl_link_stats64 *s,
                               const struct rtattr *carrier_changes)
{
	/* RX stats */
	fprintf(fp, "    RX: bytes  packets  errors  dropped overrun mcast   %s%s",
		s->rx_compressed ? "compressed" : "", _SL_);

	fprintf(fp, "    ");
	print_num(fp, 10, s->rx_bytes);
	print_num(fp, 8, s->rx_packets);
	print_num(fp, 7, s->rx_errors);
	print_num(fp, 7, s->rx_dropped);
	print_num(fp, 7, s->rx_over_errors);
	print_num(fp, 7, s->multicast);
	if (s->rx_compressed)
		print_num(fp, 7, s->rx_compressed);

	/* RX error stats */
	if (show_stats > 1) {
		fprintf(fp, "%s", _SL_);
		fprintf(fp, "    RX errors: length   crc     frame   fifo    missed%s", _SL_);

		fprintf(fp, "               ");
		print_num(fp, 8, s->rx_length_errors);
		print_num(fp, 7, s->rx_crc_errors);
		print_num(fp, 7, s->rx_frame_errors);
		print_num(fp, 7, s->rx_fifo_errors);
		print_num(fp, 7, s->rx_missed_errors);
	}
	fprintf(fp, "%s", _SL_);

	/* TX stats */
	fprintf(fp, "    TX: bytes  packets  errors  dropped carrier collsns %s%s",
		s->tx_compressed ? "compressed" : "", _SL_);


	fprintf(fp, "    ");
	print_num(fp, 10, s->tx_bytes);
	print_num(fp, 8, s->tx_packets);
	print_num(fp, 7, s->tx_errors);
	print_num(fp, 7, s->tx_dropped);
	print_num(fp, 7, s->tx_carrier_errors);
	print_num(fp, 7, s->collisions);
	if (s->tx_compressed)
		print_num(fp, 7, s->tx_compressed);

	/* TX error stats */
	if (show_stats > 1) {
		fprintf(fp, "%s", _SL_);
		fprintf(fp, "    TX errors: aborted  fifo   window heartbeat");
                if (carrier_changes)
			fprintf(fp, " transns");
		fprintf(fp, "%s", _SL_);

		fprintf(fp, "               ");
		print_num(fp, 8, s->tx_aborted_errors);
		print_num(fp, 7, s->tx_fifo_errors);
		print_num(fp, 7, s->tx_window_errors);
		print_num(fp, 7, s->tx_heartbeat_errors);
		if (carrier_changes)
			print_num(fp, 7, *(uint32_t*)RTA_DATA(carrier_changes));
	}
}

static void print_link_stats32(FILE *fp, const struct rtnl_link_stats *s,
			       const struct rtattr *carrier_changes)
{
	/* RX stats */
	fprintf(fp, "    RX: bytes  packets  errors  dropped overrun mcast   %s%s",
		s->rx_compressed ? "compressed" : "", _SL_);


	fprintf(fp, "    ");
	print_num(fp, 10, s->rx_bytes);
	print_num(fp, 8, s->rx_packets);
	print_num(fp, 7, s->rx_errors);
	print_num(fp, 7, s->rx_dropped);
	print_num(fp, 7, s->rx_over_errors);
	print_num(fp, 7, s->multicast);
	if (s->rx_compressed)
		print_num(fp, 7, s->rx_compressed);

	/* RX error stats */
	if (show_stats > 1) {
		fprintf(fp, "%s", _SL_);
		fprintf(fp, "    RX errors: length   crc     frame   fifo    missed%s", _SL_);
		fprintf(fp, "               ");
		print_num(fp, 8, s->rx_length_errors);
		print_num(fp, 7, s->rx_crc_errors);
		print_num(fp, 7, s->rx_frame_errors);
		print_num(fp, 7, s->rx_fifo_errors);
		print_num(fp, 7, s->rx_missed_errors);
	}
	fprintf(fp, "%s", _SL_);

	/* TX stats */
	fprintf(fp, "    TX: bytes  packets  errors  dropped carrier collsns %s%s",
		s->tx_compressed ? "compressed" : "", _SL_);

	fprintf(fp, "    ");
	print_num(fp, 10, s->tx_bytes);
	print_num(fp, 8, s->tx_packets);
	print_num(fp, 7, s->tx_errors);
	print_num(fp, 7, s->tx_dropped);
	print_num(fp, 7, s->tx_carrier_errors);
	print_num(fp, 7, s->collisions);
	if (s->tx_compressed)
		print_num(fp, 7, s->tx_compressed);

	/* TX error stats */
	if (show_stats > 1) {
		fprintf(fp, "%s", _SL_);
		fprintf(fp, "    TX errors: aborted  fifo   window heartbeat");
                if (carrier_changes)
			fprintf(fp, " transns");
		fprintf(fp, "%s", _SL_);

		fprintf(fp, "               ");
		print_num(fp, 8, s->tx_aborted_errors);
		print_num(fp, 7, s->tx_fifo_errors);
		print_num(fp, 7, s->tx_window_errors);
		print_num(fp, 7, s->tx_heartbeat_errors);
		if (carrier_changes)
			print_num(fp, 7, *(uint32_t*)RTA_DATA(carrier_changes));
	}
}

static void __print_link_stats(FILE *fp, struct rtattr **tb)
{
	if (tb[IFLA_STATS64])
		print_link_stats64(fp, RTA_DATA(tb[IFLA_STATS64]),
					tb[IFLA_CARRIER_CHANGES]);
	else if (tb[IFLA_STATS])
		print_link_stats32(fp, RTA_DATA(tb[IFLA_STATS]),
					tb[IFLA_CARRIER_CHANGES]);
}

static void print_link_stats(FILE *fp, struct nlmsghdr *n)
{
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr * tb[IFLA_MAX+1];

	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi),
		     n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi)));
	__print_link_stats(fp, tb);
	fprintf(fp, "%s", _SL_);
}

int print_linkinfo(const struct sockaddr_nl *who,
		   struct nlmsghdr *n, void *arg)
{
	FILE *fp = (FILE*)arg;
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct rtattr * tb[IFLA_MAX+1];
	int len = n->nlmsg_len;
	unsigned m_flag = 0;

	if (n->nlmsg_type != RTM_NEWLINK && n->nlmsg_type != RTM_DELLINK)
		return 0;

	len -= NLMSG_LENGTH(sizeof(*ifi));
	if (len < 0)
		return -1;

	if (filter.ifindex && ifi->ifi_index != filter.ifindex)
		return 0;
	if (filter.up && !(ifi->ifi_flags&IFF_UP))
		return 0;

	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
	if (tb[IFLA_IFNAME] == NULL) {
		fprintf(stderr, "BUG: device with ifindex %d has nil ifname\n", ifi->ifi_index);
	}
	if (filter.label &&
	    (!filter.family || filter.family == AF_PACKET) &&
	    fnmatch(filter.label, RTA_DATA(tb[IFLA_IFNAME]), 0))
		return 0;

	if (tb[IFLA_GROUP]) {
		int group = *(int*)RTA_DATA(tb[IFLA_GROUP]);
		if (filter.group != -1 && group != filter.group)
			return -1;
	}

	if (tb[IFLA_MASTER]) {
		int master = *(int*)RTA_DATA(tb[IFLA_MASTER]);
		if (filter.master > 0 && master != filter.master)
			return -1;
	}
	else if (filter.master > 0)
		return -1;

	if (filter.kind) {
		if (tb[IFLA_LINKINFO]) {
			char *kind = parse_link_kind(tb[IFLA_LINKINFO]);

			if (strcmp(kind, filter.kind))
				return -1;
		} else {
			return -1;
		}
	}

	if (n->nlmsg_type == RTM_DELLINK)
		fprintf(fp, "Deleted ");

	fprintf(fp, "%d: %s", ifi->ifi_index,
		tb[IFLA_IFNAME] ? rta_getattr_str(tb[IFLA_IFNAME]) : "<nil>");

	if (tb[IFLA_LINK]) {
		SPRINT_BUF(b1);
		int iflink = *(int*)RTA_DATA(tb[IFLA_LINK]);
		if (iflink == 0)
			fprintf(fp, "@NONE: ");
		else {
			fprintf(fp, "@%s: ", ll_idx_n2a(iflink, b1));
			m_flag = ll_index_to_flags(iflink);
			m_flag = !(m_flag & IFF_UP);
		}
	} else {
		fprintf(fp, ": ");
	}
	print_link_flags(fp, ifi->ifi_flags, m_flag);

	if (tb[IFLA_MTU])
		fprintf(fp, "mtu %u ", *(int*)RTA_DATA(tb[IFLA_MTU]));
	if (tb[IFLA_QDISC])
		fprintf(fp, "qdisc %s ", rta_getattr_str(tb[IFLA_QDISC]));
	if (tb[IFLA_MASTER]) {
		SPRINT_BUF(b1);
		fprintf(fp, "master %s ", ll_idx_n2a(*(int*)RTA_DATA(tb[IFLA_MASTER]), b1));
	}

	if (tb[IFLA_PHYS_PORT_ID]) {
		SPRINT_BUF(b1);
		fprintf(fp, "portid %s ",
			hexstring_n2a(RTA_DATA(tb[IFLA_PHYS_PORT_ID]),
				      RTA_PAYLOAD(tb[IFLA_PHYS_PORT_ID]),
				      b1, sizeof(b1)));
	}

	if (tb[IFLA_OPERSTATE])
		print_operstate(fp, rta_getattr_u8(tb[IFLA_OPERSTATE]));

	if (do_link && tb[IFLA_LINKMODE])
		print_linkmode(fp, tb[IFLA_LINKMODE]);

	if (tb[IFLA_GROUP]) {
		SPRINT_BUF(b1);
		int group = *(int*)RTA_DATA(tb[IFLA_GROUP]);
		fprintf(fp, "group %s ", rtnl_group_n2a(group, b1, sizeof(b1)));
	}

	if (filter.showqueue)
		print_queuelen(fp, tb);

	if (!filter.family || filter.family == AF_PACKET || show_details) {
		SPRINT_BUF(b1);
		fprintf(fp, "%s", _SL_);
		fprintf(fp, "    link/%s ", ll_type_n2a(ifi->ifi_type, b1, sizeof(b1)));

		if (tb[IFLA_ADDRESS]) {
			fprintf(fp, "%s", ll_addr_n2a(RTA_DATA(tb[IFLA_ADDRESS]),
						      RTA_PAYLOAD(tb[IFLA_ADDRESS]),
						      ifi->ifi_type,
						      b1, sizeof(b1)));
		}
		if (tb[IFLA_BROADCAST]) {
			if (ifi->ifi_flags&IFF_POINTOPOINT)
				fprintf(fp, " peer ");
			else
				fprintf(fp, " brd ");
			fprintf(fp, "%s", ll_addr_n2a(RTA_DATA(tb[IFLA_BROADCAST]),
						      RTA_PAYLOAD(tb[IFLA_BROADCAST]),
						      ifi->ifi_type,
						      b1, sizeof(b1)));
		}
	}

	if (tb[IFLA_PROMISCUITY] && show_details)
		fprintf(fp, " promiscuity %u ",
			*(int*)RTA_DATA(tb[IFLA_PROMISCUITY]));

	if (tb[IFLA_LINKINFO] && show_details)
		print_linktype(fp, tb[IFLA_LINKINFO]);

	if (do_link && tb[IFLA_AF_SPEC] && show_details)
		print_af_spec(fp, tb[IFLA_AF_SPEC]);

	if ((do_link || show_details) && tb[IFLA_IFALIAS]) {
		fprintf(fp, "%s    alias %s", _SL_,
			rta_getattr_str(tb[IFLA_IFALIAS]));
	}

	if (do_link && show_stats) {
		fprintf(fp, "%s", _SL_);
		__print_link_stats(fp, tb);
	}

	if ((do_link || show_details) && tb[IFLA_VFINFO_LIST] && tb[IFLA_NUM_VF]) {
		struct rtattr *i, *vflist = tb[IFLA_VFINFO_LIST];
		int rem = RTA_PAYLOAD(vflist);
		for (i = RTA_DATA(vflist); RTA_OK(i, rem); i = RTA_NEXT(i, rem))
			print_vfinfo(fp, i);
	}

	fprintf(fp, "\n");
	fflush(fp);
	return 1;
}

static int flush_update(void)
{
	if (rtnl_send_check(&rth, filter.flushb, filter.flushp) < 0) {
		perror("Failed to send flush request");
		return -1;
	}
	filter.flushp = 0;
	return 0;
}

static int set_lifetime(unsigned int *lifetime, char *argv)
{
	if (strcmp(argv, "forever") == 0)
		*lifetime = INFINITY_LIFE_TIME;
	else if (get_u32(lifetime, argv, 0))
		return -1;

	return 0;
}

static unsigned int get_ifa_flags(struct ifaddrmsg *ifa,
				  struct rtattr *ifa_flags_attr)
{
	return ifa_flags_attr ? rta_getattr_u32(ifa_flags_attr) :
				ifa->ifa_flags;
}

int print_addrinfo(const struct sockaddr_nl *who, struct nlmsghdr *n,
		   void *arg)
{
	FILE *fp = arg;
	struct ifaddrmsg *ifa = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	int deprecated = 0;
	/* Use local copy of ifa_flags to not interfere with filtering code */
	unsigned int ifa_flags;
	struct rtattr * rta_tb[IFA_MAX+1];
	char abuf[256];
	SPRINT_BUF(b1);

	if (n->nlmsg_type != RTM_NEWADDR && n->nlmsg_type != RTM_DELADDR)
		return 0;
	len -= NLMSG_LENGTH(sizeof(*ifa));
	if (len < 0) {
		fprintf(stderr, "BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (filter.flushb && n->nlmsg_type != RTM_NEWADDR)
		return 0;

	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa),
		     n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa)));

	ifa_flags = get_ifa_flags(ifa, rta_tb[IFA_FLAGS]);

	if (!rta_tb[IFA_LOCAL])
		rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
	if (!rta_tb[IFA_ADDRESS])
		rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];

	if (filter.ifindex && filter.ifindex != ifa->ifa_index)
		return 0;
	if ((filter.scope^ifa->ifa_scope)&filter.scopemask)
		return 0;
	if ((filter.flags ^ ifa_flags) & filter.flagmask)
		return 0;
	if (filter.label) {
		SPRINT_BUF(b1);
		const char *label;
		if (rta_tb[IFA_LABEL])
			label = RTA_DATA(rta_tb[IFA_LABEL]);
		else
			label = ll_idx_n2a(ifa->ifa_index, b1);
		if (fnmatch(filter.label, label, 0) != 0)
			return 0;
	}
	if (filter.pfx.family) {
		if (rta_tb[IFA_LOCAL]) {
			inet_prefix dst;
			memset(&dst, 0, sizeof(dst));
			dst.family = ifa->ifa_family;
			memcpy(&dst.data, RTA_DATA(rta_tb[IFA_LOCAL]), RTA_PAYLOAD(rta_tb[IFA_LOCAL]));
			if (inet_addr_match(&dst, &filter.pfx, filter.pfx.bitlen))
				return 0;
		}
	}

	if (filter.family && filter.family != ifa->ifa_family)
		return 0;

	if (filter.flushb) {
		struct nlmsghdr *fn;
		if (NLMSG_ALIGN(filter.flushp) + n->nlmsg_len > filter.flushe) {
			if (flush_update())
				return -1;
		}
		fn = (struct nlmsghdr*)(filter.flushb + NLMSG_ALIGN(filter.flushp));
		memcpy(fn, n, n->nlmsg_len);
		fn->nlmsg_type = RTM_DELADDR;
		fn->nlmsg_flags = NLM_F_REQUEST;
		fn->nlmsg_seq = ++rth.seq;
		filter.flushp = (((char*)fn) + n->nlmsg_len) - filter.flushb;
		filter.flushed++;
		if (show_stats < 2)
			return 0;
	}

	if (n->nlmsg_type == RTM_DELADDR)
		fprintf(fp, "Deleted ");

	if (filter.oneline || filter.flushb)
		fprintf(fp, "%u: %s", ifa->ifa_index, ll_index_to_name(ifa->ifa_index));
	if (ifa->ifa_family == AF_INET)
		fprintf(fp, "    inet ");
	else if (ifa->ifa_family == AF_INET6)
		fprintf(fp, "    inet6 ");
	else if (ifa->ifa_family == AF_DECnet)
		fprintf(fp, "    dnet ");
	else if (ifa->ifa_family == AF_IPX)
		fprintf(fp, "     ipx ");
	else
		fprintf(fp, "    family %d ", ifa->ifa_family);

	if (rta_tb[IFA_LOCAL]) {
		fprintf(fp, "%s", format_host(ifa->ifa_family,
					      RTA_PAYLOAD(rta_tb[IFA_LOCAL]),
					      RTA_DATA(rta_tb[IFA_LOCAL]),
					      abuf, sizeof(abuf)));

		if (rta_tb[IFA_ADDRESS] == NULL ||
		    memcmp(RTA_DATA(rta_tb[IFA_ADDRESS]), RTA_DATA(rta_tb[IFA_LOCAL]),
			   ifa->ifa_family == AF_INET ? 4 : 16) == 0) {
			fprintf(fp, "/%d ", ifa->ifa_prefixlen);
		} else {
			fprintf(fp, " peer %s/%d ",
				format_host(ifa->ifa_family,
					    RTA_PAYLOAD(rta_tb[IFA_ADDRESS]),
					    RTA_DATA(rta_tb[IFA_ADDRESS]),
					    abuf, sizeof(abuf)),
				ifa->ifa_prefixlen);
		}
	}

	if (rta_tb[IFA_BROADCAST]) {
		fprintf(fp, "brd %s ",
			format_host(ifa->ifa_family,
				    RTA_PAYLOAD(rta_tb[IFA_BROADCAST]),
				    RTA_DATA(rta_tb[IFA_BROADCAST]),
				    abuf, sizeof(abuf)));
	}
	if (rta_tb[IFA_ANYCAST]) {
		fprintf(fp, "any %s ",
			format_host(ifa->ifa_family,
				    RTA_PAYLOAD(rta_tb[IFA_ANYCAST]),
				    RTA_DATA(rta_tb[IFA_ANYCAST]),
				    abuf, sizeof(abuf)));
	}
	fprintf(fp, "scope %s ", rtnl_rtscope_n2a(ifa->ifa_scope, b1, sizeof(b1)));
	if (ifa_flags & IFA_F_SECONDARY) {
		ifa_flags &= ~IFA_F_SECONDARY;
		if (ifa->ifa_family == AF_INET6)
			fprintf(fp, "temporary ");
		else
			fprintf(fp, "secondary ");
	}
	if (ifa_flags & IFA_F_TENTATIVE) {
		ifa_flags &= ~IFA_F_TENTATIVE;
		fprintf(fp, "tentative ");
	}
	if (ifa_flags & IFA_F_DEPRECATED) {
		ifa_flags &= ~IFA_F_DEPRECATED;
		deprecated = 1;
		fprintf(fp, "deprecated ");
	}
	if (ifa_flags & IFA_F_HOMEADDRESS) {
		ifa_flags &= ~IFA_F_HOMEADDRESS;
		fprintf(fp, "home ");
	}
	if (ifa_flags & IFA_F_NODAD) {
		ifa_flags &= ~IFA_F_NODAD;
		fprintf(fp, "nodad ");
	}
	if (ifa_flags & IFA_F_MANAGETEMPADDR) {
		ifa_flags &= ~IFA_F_MANAGETEMPADDR;
		fprintf(fp, "mngtmpaddr ");
	}
	if (ifa_flags & IFA_F_NOPREFIXROUTE) {
		ifa_flags &= ~IFA_F_NOPREFIXROUTE;
		fprintf(fp, "noprefixroute ");
	}
	if (!(ifa_flags & IFA_F_PERMANENT)) {
		fprintf(fp, "dynamic ");
	} else
		ifa_flags &= ~IFA_F_PERMANENT;
	if (ifa_flags & IFA_F_DADFAILED) {
		ifa_flags &= ~IFA_F_DADFAILED;
		fprintf(fp, "dadfailed ");
	}
	if (ifa_flags)
		fprintf(fp, "flags %02x ", ifa_flags);
	if (rta_tb[IFA_LABEL])
		fprintf(fp, "%s", rta_getattr_str(rta_tb[IFA_LABEL]));
	if (rta_tb[IFA_CACHEINFO]) {
		struct ifa_cacheinfo *ci = RTA_DATA(rta_tb[IFA_CACHEINFO]);
		fprintf(fp, "%s", _SL_);
		fprintf(fp, "       valid_lft ");
		if (ci->ifa_valid == INFINITY_LIFE_TIME)
			fprintf(fp, "forever");
		else
			fprintf(fp, "%usec", ci->ifa_valid);
		fprintf(fp, " preferred_lft ");
		if (ci->ifa_prefered == INFINITY_LIFE_TIME)
			fprintf(fp, "forever");
		else {
			if (deprecated)
				fprintf(fp, "%dsec", ci->ifa_prefered);
			else
				fprintf(fp, "%usec", ci->ifa_prefered);
		}
	}
	fprintf(fp, "\n");
	fflush(fp);
	return 0;
}

static int print_addrinfo_primary(const struct sockaddr_nl *who,
				  struct nlmsghdr *n, void *arg)
{
	struct ifaddrmsg *ifa = NLMSG_DATA(n);

	if (ifa->ifa_flags & IFA_F_SECONDARY)
		return 0;

	return print_addrinfo(who, n, arg);
}

static int print_addrinfo_secondary(const struct sockaddr_nl *who,
				    struct nlmsghdr *n, void *arg)
{
	struct ifaddrmsg *ifa = NLMSG_DATA(n);

	if (!(ifa->ifa_flags & IFA_F_SECONDARY))
		return 0;

	return print_addrinfo(who, n, arg);
}

struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr	  h;
};

struct nlmsg_chain
{
	struct nlmsg_list *head;
	struct nlmsg_list *tail;
};

static int print_selected_addrinfo(struct ifinfomsg *ifi,
				   struct nlmsg_list *ainfo, FILE *fp)
{
	for ( ;ainfo ;  ainfo = ainfo->next) {
		struct nlmsghdr *n = &ainfo->h;
		struct ifaddrmsg *ifa = NLMSG_DATA(n);

		if (n->nlmsg_type != RTM_NEWADDR)
			continue;

		if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifa)))
			return -1;

		if (ifa->ifa_index != ifi->ifi_index ||
		    (filter.family && filter.family != ifa->ifa_family))
			continue;

		if (filter.up && !(ifi->ifi_flags&IFF_UP))
			continue;

		print_addrinfo(NULL, n, fp);
	}
	return 0;
}


static int store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n,
		       void *arg)
{
	struct nlmsg_chain *lchain = (struct nlmsg_chain *)arg;
	struct nlmsg_list *h;

	h = malloc(n->nlmsg_len+sizeof(void*));
	if (h == NULL)
		return -1;

	memcpy(&h->h, n, n->nlmsg_len);
	h->next = NULL;

	if (lchain->tail)
		lchain->tail->next = h;
	else
		lchain->head = h;
	lchain->tail = h;

	ll_remember_index(who, n, NULL);
	return 0;
}

static __u32 ipadd_dump_magic = 0x47361222;

static int ipadd_save_prep(void)
{
	int ret;

	if (isatty(STDOUT_FILENO)) {
		fprintf(stderr, "Not sending a binary stream to stdout\n");
		return -1;
	}

	ret = write(STDOUT_FILENO, &ipadd_dump_magic, sizeof(ipadd_dump_magic));
	if (ret != sizeof(ipadd_dump_magic)) {
		fprintf(stderr, "Can't write magic to dump file\n");
		return -1;
	}

	return 0;
}

static int ipadd_dump_check_magic(void)
{
	int ret;
	__u32 magic = 0;

	if (isatty(STDIN_FILENO)) {
		fprintf(stderr, "Can't restore addr dump from a terminal\n");
		return -1;
	}

	ret = fread(&magic, sizeof(magic), 1, stdin);
	if (magic != ipadd_dump_magic) {
		fprintf(stderr, "Magic mismatch (%d elems, %x magic)\n", ret, magic);
		return -1;
	}

	return 0;
}

static int save_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n,
		       void *arg)
{
	int ret;

	ret = write(STDOUT_FILENO, n, n->nlmsg_len);
	if ((ret > 0) && (ret != n->nlmsg_len)) {
		fprintf(stderr, "Short write while saving nlmsg\n");
		ret = -EIO;
	}

	return ret == n->nlmsg_len ? 0 : ret;
}

static int show_handler(const struct sockaddr_nl *nl, struct nlmsghdr *n, void *arg)
{
	struct ifaddrmsg *ifa = NLMSG_DATA(n);

	printf("if%d:\n", ifa->ifa_index);
	print_addrinfo(NULL, n, stdout);
	return 0;
}

static int ipaddr_showdump(void)
{
	if (ipadd_dump_check_magic())
		exit(-1);

	exit(rtnl_from_file(stdin, &show_handler, NULL));
}

static int restore_handler(const struct sockaddr_nl *nl, struct nlmsghdr *n, void *arg)
{
	int ret;

	n->nlmsg_flags |= NLM_F_REQUEST | NLM_F_CREATE | NLM_F_ACK;

	ll_init_map(&rth);

	ret = rtnl_talk(&rth, n, 0, 0, n);
	if ((ret < 0) && (errno == EEXIST))
		ret = 0;

	return ret;
}

static int ipaddr_restore(void)
{
	if (ipadd_dump_check_magic())
		exit(-1);

	exit(rtnl_from_file(stdin, &restore_handler, NULL));
}

static void free_nlmsg_chain(struct nlmsg_chain *info)
{
	struct nlmsg_list *l, *n;

	for (l = info->head; l; l = n) {
		n = l->next;
		free(l);
	}
}

static void ipaddr_filter(struct nlmsg_chain *linfo, struct nlmsg_chain *ainfo)
{
	struct nlmsg_list *l, **lp;

	lp = &linfo->head;
	while ( (l = *lp) != NULL) {
		int ok = 0;
		int missing_net_address = 1;
		struct ifinfomsg *ifi = NLMSG_DATA(&l->h);
		struct nlmsg_list *a;

		for (a = ainfo->head; a; a = a->next) {
			struct nlmsghdr *n = &a->h;
			struct ifaddrmsg *ifa = NLMSG_DATA(n);
			struct rtattr *tb[IFA_MAX + 1];
			unsigned int ifa_flags;

			if (ifa->ifa_index != ifi->ifi_index)
				continue;
			missing_net_address = 0;
			if (filter.family && filter.family != ifa->ifa_family)
				continue;
			if ((filter.scope^ifa->ifa_scope)&filter.scopemask)
				continue;

			parse_rtattr(tb, IFA_MAX, IFA_RTA(ifa), IFA_PAYLOAD(n));
			ifa_flags = get_ifa_flags(ifa, tb[IFA_FLAGS]);

			if ((filter.flags ^ ifa_flags) & filter.flagmask)
				continue;
			if (filter.pfx.family || filter.label) {
				if (!tb[IFA_LOCAL])
					tb[IFA_LOCAL] = tb[IFA_ADDRESS];

				if (filter.pfx.family && tb[IFA_LOCAL]) {
					inet_prefix dst;
					memset(&dst, 0, sizeof(dst));
					dst.family = ifa->ifa_family;
					memcpy(&dst.data, RTA_DATA(tb[IFA_LOCAL]), RTA_PAYLOAD(tb[IFA_LOCAL]));
					if (inet_addr_match(&dst, &filter.pfx, filter.pfx.bitlen))
						continue;
				}
				if (filter.label) {
					SPRINT_BUF(b1);
					const char *label;
					if (tb[IFA_LABEL])
						label = RTA_DATA(tb[IFA_LABEL]);
					else
						label = ll_idx_n2a(ifa->ifa_index, b1);
					if (fnmatch(filter.label, label, 0) != 0)
						continue;
				}
			}

			ok = 1;
			break;
		}
		if (missing_net_address &&
		    (filter.family == AF_UNSPEC || filter.family == AF_PACKET))
			ok = 1;
		if (!ok) {
			*lp = l->next;
			free(l);
		} else
			lp = &l->next;
	}
}

static int ipaddr_flush(void)
{
	int round = 0;
	char flushb[4096-512];

	filter.flushb = flushb;
	filter.flushp = 0;
	filter.flushe = sizeof(flushb);

	while ((max_flush_loops == 0) || (round < max_flush_loops)) {
		const struct rtnl_dump_filter_arg a[3] = {
			{
				.filter = print_addrinfo_secondary,
				.arg1 = stdout,
			},
			{
				.filter = print_addrinfo_primary,
				.arg1 = stdout,
			},
			{
				.filter = NULL,
				.arg1 = NULL,
			},
		};
		if (rtnl_wilddump_request(&rth, filter.family, RTM_GETADDR) < 0) {
			perror("Cannot send dump request");
			exit(1);
		}
		filter.flushed = 0;
		if (rtnl_dump_filter_l(&rth, a) < 0) {
			fprintf(stderr, "Flush terminated\n");
			exit(1);
		}
		if (filter.flushed == 0) {
 flush_done:
			if (show_stats) {
				if (round == 0)
					printf("Nothing to flush.\n");
				else
					printf("*** Flush is complete after %d round%s ***\n", round, round>1?"s":"");
			}
			fflush(stdout);
			return 0;
		}
		round++;
		if (flush_update() < 0)
			return 1;

		if (show_stats) {
			printf("\n*** Round %d, deleting %d addresses ***\n", round, filter.flushed);
			fflush(stdout);
		}

		/* If we are flushing, and specifying primary, then we
		 * want to flush only a single round.  Otherwise, we'll
		 * start flushing secondaries that were promoted to
		 * primaries.
		 */
		if (!(filter.flags & IFA_F_SECONDARY) && (filter.flagmask & IFA_F_SECONDARY))
			goto flush_done;
	}
	fprintf(stderr, "*** Flush remains incomplete after %d rounds. ***\n", max_flush_loops);
	fflush(stderr);
	return 1;
}

static int ipaddr_list_flush_or_save(int argc, char **argv, int action)
{
	struct nlmsg_chain linfo = { NULL, NULL};
	struct nlmsg_chain ainfo = { NULL, NULL};
	struct nlmsg_list *l;
	char *filter_dev = NULL;
	int no_link = 0;

	ipaddr_reset_filter(oneline, 0);
	filter.showqueue = 1;

	if (filter.family == AF_UNSPEC)
		filter.family = preferred_family;

	filter.group = -1;

	if (action == IPADD_FLUSH) {
		if (argc <= 0) {
			fprintf(stderr, "Flush requires arguments.\n");

			return -1;
		}
		if (filter.family == AF_PACKET) {
			fprintf(stderr, "Cannot flush link addresses.\n");
			return -1;
		}
	}

	while (argc > 0) {
		if (strcmp(*argv, "to") == 0) {
			NEXT_ARG();
			get_prefix(&filter.pfx, *argv, filter.family);
			if (filter.family == AF_UNSPEC)
				filter.family = filter.pfx.family;
		} else if (strcmp(*argv, "scope") == 0) {
			unsigned scope = 0;
			NEXT_ARG();
			filter.scopemask = -1;
			if (rtnl_rtscope_a2n(&scope, *argv)) {
				if (strcmp(*argv, "all") != 0)
					invarg("invalid \"scope\"\n", *argv);
				scope = RT_SCOPE_NOWHERE;
				filter.scopemask = 0;
			}
			filter.scope = scope;
		} else if (strcmp(*argv, "up") == 0) {
			filter.up = 1;
		} else if (strcmp(*argv, "dynamic") == 0) {
			filter.flags &= ~IFA_F_PERMANENT;
			filter.flagmask |= IFA_F_PERMANENT;
		} else if (strcmp(*argv, "permanent") == 0) {
			filter.flags |= IFA_F_PERMANENT;
			filter.flagmask |= IFA_F_PERMANENT;
		} else if (strcmp(*argv, "secondary") == 0 ||
			   strcmp(*argv, "temporary") == 0) {
			filter.flags |= IFA_F_SECONDARY;
			filter.flagmask |= IFA_F_SECONDARY;
		} else if (strcmp(*argv, "primary") == 0) {
			filter.flags &= ~IFA_F_SECONDARY;
			filter.flagmask |= IFA_F_SECONDARY;
		} else if (strcmp(*argv, "tentative") == 0) {
			filter.flags |= IFA_F_TENTATIVE;
			filter.flagmask |= IFA_F_TENTATIVE;
		} else if (strcmp(*argv, "-tentative") == 0) {
			filter.flags &= ~IFA_F_TENTATIVE;
			filter.flagmask |= IFA_F_TENTATIVE;
		} else if (strcmp(*argv, "deprecated") == 0) {
			filter.flags |= IFA_F_DEPRECATED;
			filter.flagmask |= IFA_F_DEPRECATED;
		} else if (strcmp(*argv, "-deprecated") == 0) {
			filter.flags &= ~IFA_F_DEPRECATED;
			filter.flagmask |= IFA_F_DEPRECATED;
		} else if (strcmp(*argv, "home") == 0) {
			filter.flags |= IFA_F_HOMEADDRESS;
			filter.flagmask |= IFA_F_HOMEADDRESS;
		} else if (strcmp(*argv, "nodad") == 0) {
			filter.flags |= IFA_F_NODAD;
			filter.flagmask |= IFA_F_NODAD;
		} else if (strcmp(*argv, "mngtmpaddr") == 0) {
			filter.flags |= IFA_F_MANAGETEMPADDR;
			filter.flagmask |= IFA_F_MANAGETEMPADDR;
		} else if (strcmp(*argv, "noprefixroute") == 0) {
			filter.flags |= IFA_F_NOPREFIXROUTE;
			filter.flagmask |= IFA_F_NOPREFIXROUTE;
		} else if (strcmp(*argv, "dadfailed") == 0) {
			filter.flags |= IFA_F_DADFAILED;
			filter.flagmask |= IFA_F_DADFAILED;
		} else if (strcmp(*argv, "-dadfailed") == 0) {
			filter.flags &= ~IFA_F_DADFAILED;
			filter.flagmask |= IFA_F_DADFAILED;
		} else if (strcmp(*argv, "label") == 0) {
			NEXT_ARG();
			filter.label = *argv;
		} else if (strcmp(*argv, "group") == 0) {
			NEXT_ARG();
			if (rtnl_group_a2n(&filter.group, *argv))
				invarg("Invalid \"group\" value\n", *argv);
		} else if (strcmp(*argv, "master") == 0) {
			int ifindex;
			NEXT_ARG();
			ifindex = ll_name_to_index(*argv);
			if (!ifindex)
				invarg("Device does not exist\n", *argv);
			filter.master = ifindex;
		} else if (do_link && strcmp(*argv, "type") == 0) {
			NEXT_ARG();
			filter.kind = *argv;
		} else {
			if (strcmp(*argv, "dev") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			if (filter_dev)
				duparg2("dev", *argv);
			filter_dev = *argv;
		}
		argv++; argc--;
	}

	if (filter_dev) {
		filter.ifindex = ll_name_to_index(filter_dev);
		if (filter.ifindex <= 0) {
			fprintf(stderr, "Device \"%s\" does not exist.\n", filter_dev);
			return -1;
		}
	}

	if (action == IPADD_FLUSH)
		return ipaddr_flush();

	if (action == IPADD_SAVE) {
		if (ipadd_save_prep())
			exit(1);

		if (rtnl_wilddump_request(&rth, preferred_family, RTM_GETADDR) < 0) {
			perror("Cannot send dump request");
			exit(1);
		}

		if (rtnl_dump_filter(&rth, save_nlmsg, stdout) < 0) {
			fprintf(stderr, "Save terminated\n");
			exit(1);
		}

		exit(0);
	}

	/*
	 * If only filter_dev present and none of the other
	 * link filters are present, use RTM_GETLINK to get
	 * the link device
	 */
	if (filter_dev && filter.group == -1 && do_link == 1) {
		if (iplink_get(0, filter_dev, RTEXT_FILTER_VF) < 0) {
			perror("Cannot send link get request");
			exit(1);
		}
		exit(0);
	}

	if (rtnl_wilddump_request(&rth, preferred_family, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	if (rtnl_dump_filter(&rth, store_nlmsg, &linfo) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	if (filter.family != AF_PACKET) {
		if (filter.oneline)
			no_link = 1;

		if (rtnl_wilddump_request(&rth, filter.family, RTM_GETADDR) < 0) {
			perror("Cannot send dump request");
			exit(1);
		}

		if (rtnl_dump_filter(&rth, store_nlmsg, &ainfo) < 0) {
			fprintf(stderr, "Dump terminated\n");
			exit(1);
		}

		ipaddr_filter(&linfo, &ainfo);
	}

	for (l = linfo.head; l; l = l->next) {
		int res = 0;

		if (no_link || (res = print_linkinfo(NULL, &l->h, stdout)) >= 0) {
			struct ifinfomsg *ifi = NLMSG_DATA(&l->h);
			if (filter.family != AF_PACKET)
				print_selected_addrinfo(ifi,
							ainfo.head, stdout);
			if (res > 0 && !do_link && show_stats)
				print_link_stats(stdout, &l->h);
		}
	}
	fflush(stdout);

	free_nlmsg_chain(&ainfo);
	free_nlmsg_chain(&linfo);

	return 0;
}

static void
ipaddr_loop_each_vf(struct rtattr *tb[], int vfnum, int *min, int *max)
{
	struct rtattr *vflist = tb[IFLA_VFINFO_LIST];
	struct rtattr *i, *vf[IFLA_VF_MAX+1];
	struct ifla_vf_rate *vf_rate;
	int rem;

	rem = RTA_PAYLOAD(vflist);

	for (i = RTA_DATA(vflist); RTA_OK(i, rem); i = RTA_NEXT(i, rem)) {
		parse_rtattr_nested(vf, IFLA_VF_MAX, i);
		vf_rate = RTA_DATA(vf[IFLA_VF_RATE]);
		if (vf_rate->vf == vfnum) {
			*min = vf_rate->min_tx_rate;
			*max = vf_rate->max_tx_rate;
			return;
		}
	}
	fprintf(stderr, "Cannot find VF %d\n", vfnum);
	exit(1);
}

void ipaddr_get_vf_rate(int vfnum, int *min, int *max, int idx)
{
	struct nlmsg_chain linfo = { NULL, NULL};
	struct rtattr *tb[IFLA_MAX+1];
	struct ifinfomsg *ifi;
	struct nlmsg_list *l;
	struct nlmsghdr *n;
	int len;

	if (rtnl_wilddump_request(&rth, AF_UNSPEC, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}
	if (rtnl_dump_filter(&rth, store_nlmsg, &linfo) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}
	for (l = linfo.head; l; l = l->next) {
		n = &l->h;
		ifi = NLMSG_DATA(n);

		len = n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi));
		if (len < 0 || (idx && idx != ifi->ifi_index))
			continue;

		parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);

		if ((tb[IFLA_VFINFO_LIST] && tb[IFLA_NUM_VF])) {
			ipaddr_loop_each_vf(tb, vfnum, min, max);
			return;
		}
	}
}

int ipaddr_list_link(int argc, char **argv)
{
	preferred_family = AF_PACKET;
	do_link = 1;
	return ipaddr_list_flush_or_save(argc, argv, IPADD_LIST);
}

void ipaddr_reset_filter(int oneline, int ifindex)
{
	memset(&filter, 0, sizeof(filter));
	filter.oneline = oneline;
	filter.ifindex = ifindex;
}

static int default_scope(inet_prefix *lcl)
{
	if (lcl->family == AF_INET) {
		if (lcl->bytelen >= 1 && *(__u8*)&lcl->data == 127)
			return RT_SCOPE_HOST;
	}
	return 0;
}

static int ipaddr_modify(int cmd, int flags, int argc, char **argv)
{
	struct {
		struct nlmsghdr	n;
		struct ifaddrmsg	ifa;
		char			buf[256];
	} req;
	char  *d = NULL;
	char  *l = NULL;
	char  *lcl_arg = NULL;
	char  *valid_lftp = NULL;
	char  *preferred_lftp = NULL;
	inet_prefix lcl;
	inet_prefix peer;
	int local_len = 0;
	int peer_len = 0;
	int brd_len = 0;
	int any_len = 0;
	int scoped = 0;
	__u32 preferred_lft = INFINITY_LIFE_TIME;
	__u32 valid_lft = INFINITY_LIFE_TIME;
	struct ifa_cacheinfo cinfo;
	unsigned int ifa_flags = 0;

	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | flags;
	req.n.nlmsg_type = cmd;
	req.ifa.ifa_family = preferred_family;

	while (argc > 0) {
		if (strcmp(*argv, "peer") == 0 ||
		    strcmp(*argv, "remote") == 0) {
			NEXT_ARG();

			if (peer_len)
				duparg("peer", *argv);
			get_prefix(&peer, *argv, req.ifa.ifa_family);
			peer_len = peer.bytelen;
			if (req.ifa.ifa_family == AF_UNSPEC)
				req.ifa.ifa_family = peer.family;
			addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &peer.data, peer.bytelen);
			req.ifa.ifa_prefixlen = peer.bitlen;
		} else if (matches(*argv, "broadcast") == 0 ||
			   strcmp(*argv, "brd") == 0) {
			inet_prefix addr;
			NEXT_ARG();
			if (brd_len)
				duparg("broadcast", *argv);
			if (strcmp(*argv, "+") == 0)
				brd_len = -1;
			else if (strcmp(*argv, "-") == 0)
				brd_len = -2;
			else {
				get_addr(&addr, *argv, req.ifa.ifa_family);
				if (req.ifa.ifa_family == AF_UNSPEC)
					req.ifa.ifa_family = addr.family;
				addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &addr.data, addr.bytelen);
				brd_len = addr.bytelen;
			}
		} else if (strcmp(*argv, "anycast") == 0) {
			inet_prefix addr;
			NEXT_ARG();
			if (any_len)
				duparg("anycast", *argv);
			get_addr(&addr, *argv, req.ifa.ifa_family);
			if (req.ifa.ifa_family == AF_UNSPEC)
				req.ifa.ifa_family = addr.family;
			addattr_l(&req.n, sizeof(req), IFA_ANYCAST, &addr.data, addr.bytelen);
			any_len = addr.bytelen;
		} else if (strcmp(*argv, "scope") == 0) {
			unsigned scope = 0;
			NEXT_ARG();
			if (rtnl_rtscope_a2n(&scope, *argv))
				invarg("invalid scope value.", *argv);
			req.ifa.ifa_scope = scope;
			scoped = 1;
		} else if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			d = *argv;
		} else if (strcmp(*argv, "label") == 0) {
			NEXT_ARG();
			l = *argv;
			addattr_l(&req.n, sizeof(req), IFA_LABEL, l, strlen(l)+1);
		} else if (matches(*argv, "valid_lft") == 0) {
			if (valid_lftp)
				duparg("valid_lft", *argv);
			NEXT_ARG();
			valid_lftp = *argv;
			if (set_lifetime(&valid_lft, *argv))
				invarg("valid_lft value", *argv);
		} else if (matches(*argv, "preferred_lft") == 0) {
			if (preferred_lftp)
				duparg("preferred_lft", *argv);
			NEXT_ARG();
			preferred_lftp = *argv;
			if (set_lifetime(&preferred_lft, *argv))
				invarg("preferred_lft value", *argv);
		} else if (strcmp(*argv, "home") == 0) {
			ifa_flags |= IFA_F_HOMEADDRESS;
		} else if (strcmp(*argv, "nodad") == 0) {
			ifa_flags |= IFA_F_NODAD;
		} else if (strcmp(*argv, "mngtmpaddr") == 0) {
			ifa_flags |= IFA_F_MANAGETEMPADDR;
		} else if (strcmp(*argv, "noprefixroute") == 0) {
			ifa_flags |= IFA_F_NOPREFIXROUTE;
		} else {
			if (strcmp(*argv, "local") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			if (local_len)
				duparg2("local", *argv);
			lcl_arg = *argv;
			get_prefix(&lcl, *argv, req.ifa.ifa_family);
			if (req.ifa.ifa_family == AF_UNSPEC)
				req.ifa.ifa_family = lcl.family;
			addattr_l(&req.n, sizeof(req), IFA_LOCAL, &lcl.data, lcl.bytelen);
			local_len = lcl.bytelen;
		}
		argc--; argv++;
	}
	if (ifa_flags <= 0xff)
		req.ifa.ifa_flags = ifa_flags;
	else
		addattr32(&req.n, sizeof(req), IFA_FLAGS, ifa_flags);

	if (d == NULL) {
		fprintf(stderr, "Not enough information: \"dev\" argument is required.\n");
		return -1;
	}
	if (l && matches(d, l) != 0) {
		fprintf(stderr, "\"dev\" (%s) must match \"label\" (%s).\n", d, l);
		return -1;
	}

	if (peer_len == 0 && local_len) {
		if (cmd == RTM_DELADDR && lcl.family == AF_INET && !(lcl.flags & PREFIXLEN_SPECIFIED)) {
			fprintf(stderr,
			    "Warning: Executing wildcard deletion to stay compatible with old scripts.\n" \
			    "         Explicitly specify the prefix length (%s/%d) to avoid this warning.\n" \
			    "         This special behaviour is likely to disappear in further releases,\n" \
			    "         fix your scripts!\n", lcl_arg, local_len*8);
		} else {
			peer = lcl;
			addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &lcl.data, lcl.bytelen);
		}
	}
	if (req.ifa.ifa_prefixlen == 0)
		req.ifa.ifa_prefixlen = lcl.bitlen;

	if (brd_len < 0 && cmd != RTM_DELADDR) {
		inet_prefix brd;
		int i;
		if (req.ifa.ifa_family != AF_INET) {
			fprintf(stderr, "Broadcast can be set only for IPv4 addresses\n");
			return -1;
		}
		brd = peer;
		if (brd.bitlen <= 30) {
			for (i = 31; i >= brd.bitlen; i--) {
				if (brd_len == -1)
					brd.data[0] |= htonl(1<<(31-i));
				else
					brd.data[0] &= ~htonl(1<<(31-i));
			}
			addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &brd.data, brd.bytelen);
			brd_len = brd.bytelen;
		}
	}
	if (!scoped && cmd != RTM_DELADDR)
		req.ifa.ifa_scope = default_scope(&lcl);

	if ((req.ifa.ifa_index = ll_name_to_index(d)) == 0) {
		fprintf(stderr, "Cannot find device \"%s\"\n", d);
		return -1;
	}

	if (valid_lftp || preferred_lftp) {
		if (!valid_lft) {
			fprintf(stderr, "valid_lft is zero\n");
			return -1;
		}
		if (valid_lft < preferred_lft) {
			fprintf(stderr, "preferred_lft is greater than valid_lft\n");
			return -1;
		}

		memset(&cinfo, 0, sizeof(cinfo));
		cinfo.ifa_prefered = preferred_lft;
		cinfo.ifa_valid = valid_lft;
		addattr_l(&req.n, sizeof(req), IFA_CACHEINFO, &cinfo,
			  sizeof(cinfo));
	}

	if (rtnl_talk(&rth, &req.n, 0, 0, NULL) < 0)
		return -2;

	return 0;
}

int do_ipaddr(int argc, char **argv)
{
	if (argc < 1)
		return ipaddr_list_flush_or_save(0, NULL, IPADD_LIST);
	if (matches(*argv, "add") == 0)
		return ipaddr_modify(RTM_NEWADDR, NLM_F_CREATE|NLM_F_EXCL, argc-1, argv+1);
	if (matches(*argv, "change") == 0 ||
		strcmp(*argv, "chg") == 0)
		return ipaddr_modify(RTM_NEWADDR, NLM_F_REPLACE, argc-1, argv+1);
	if (matches(*argv, "replace") == 0)
		return ipaddr_modify(RTM_NEWADDR, NLM_F_CREATE|NLM_F_REPLACE, argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return ipaddr_modify(RTM_DELADDR, 0, argc-1, argv+1);
	if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
	    || matches(*argv, "lst") == 0)
		return ipaddr_list_flush_or_save(argc-1, argv+1, IPADD_LIST);
	if (matches(*argv, "flush") == 0)
		return ipaddr_list_flush_or_save(argc-1, argv+1, IPADD_FLUSH);
	if (matches(*argv, "save") == 0)
		return ipaddr_list_flush_or_save(argc-1, argv+1, IPADD_SAVE);
	if (matches(*argv, "showdump") == 0)
		return ipaddr_showdump();
	if (matches(*argv, "restore") == 0)
		return ipaddr_restore();
	if (matches(*argv, "help") == 0)
		usage();
	fprintf(stderr, "Command \"%s\" is unknown, try \"ip addr help\".\n", *argv);
	exit(-1);
}
