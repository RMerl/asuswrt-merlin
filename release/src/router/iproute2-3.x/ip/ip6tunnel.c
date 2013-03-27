/*
 * Copyright (C)2006 USAGI/WIDE Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/*
 * Author:
 *	Masahide NAKAMURA @USAGI
 */

#ifndef NO_IPV6

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/ip.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/if_tunnel.h>
#include <linux/ip6_tunnel.h>

#include "utils.h"
#include "tunnel.h"
#include "ip_common.h"

#define IP6_FLOWINFO_TCLASS	htonl(0x0FF00000)
#define IP6_FLOWINFO_FLOWLABEL	htonl(0x000FFFFF)

#define DEFAULT_TNL_HOP_LIMIT	(64)

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip -f inet6 tunnel { add | change | del | show } [ NAME ]\n");
	fprintf(stderr, "          [ mode { ip6ip6 | ipip6 | any } ]\n");
	fprintf(stderr, "          [ remote ADDR local ADDR ] [ dev PHYS_DEV ]\n");
	fprintf(stderr, "          [ encaplimit ELIM ]\n");
	fprintf(stderr ,"          [ hoplimit TTL ] [ tclass TCLASS ] [ flowlabel FLOWLABEL ]\n");
	fprintf(stderr, "          [ dscp inherit ]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Where: NAME      := STRING\n");
	fprintf(stderr, "       ADDR      := IPV6_ADDRESS\n");
	fprintf(stderr, "       ELIM      := { none | 0..255 }(default=%d)\n",
		IPV6_DEFAULT_TNL_ENCAP_LIMIT);
	fprintf(stderr, "       TTL       := 0..255 (default=%d)\n",
		DEFAULT_TNL_HOP_LIMIT);
	fprintf(stderr, "       TOS       := { 0x0..0xff | inherit }\n");
	fprintf(stderr, "       FLOWLABEL := { 0x0..0xfffff | inherit }\n");
	exit(-1);
}

static void print_tunnel(struct ip6_tnl_parm *p)
{
	char remote[64];
	char local[64];

	inet_ntop(AF_INET6, &p->raddr, remote, sizeof(remote));
	inet_ntop(AF_INET6, &p->laddr, local, sizeof(local));

	printf("%s: %s/ipv6 remote %s local %s",
	       p->name, tnl_strproto(p->proto), remote, local);
	if (p->link) {
		const char *n = ll_index_to_name(p->link);
		if (n)
			printf(" dev %s", n);
	}

	if (p->flags & IP6_TNL_F_IGN_ENCAP_LIMIT)
		printf(" encaplimit none");
	else
		printf(" encaplimit %u", p->encap_limit);

	printf(" hoplimit %u", p->hop_limit);

	if (p->flags & IP6_TNL_F_USE_ORIG_TCLASS)
		printf(" tclass inherit");
	else {
		__u32 val = ntohl(p->flowinfo & IP6_FLOWINFO_TCLASS);
		printf(" tclass 0x%02x", (__u8)(val >> 20));
	}

	if (p->flags & IP6_TNL_F_USE_ORIG_FLOWLABEL)
		printf(" flowlabel inherit");
	else
		printf(" flowlabel 0x%05x", ntohl(p->flowinfo & IP6_FLOWINFO_FLOWLABEL));

	printf(" (flowinfo 0x%08x)", ntohl(p->flowinfo));

	if (p->flags & IP6_TNL_F_RCV_DSCP_COPY)
		printf(" dscp inherit");
}

static int parse_args(int argc, char **argv, int cmd, struct ip6_tnl_parm *p)
{
	int count = 0;
	char medium[IFNAMSIZ];

	memset(medium, 0, sizeof(medium));

	while (argc > 0) {
		if (strcmp(*argv, "mode") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "ipv6/ipv6") == 0 ||
			    strcmp(*argv, "ip6ip6") == 0)
				p->proto = IPPROTO_IPV6;
			else if (strcmp(*argv, "ip/ipv6") == 0 ||
				 strcmp(*argv, "ipv4/ipv6") == 0 ||
				 strcmp(*argv, "ipip6") == 0 ||
				 strcmp(*argv, "ip4ip6") == 0)
				p->proto = IPPROTO_IPIP;
			else if (strcmp(*argv, "any/ipv6") == 0 ||
				 strcmp(*argv, "any") == 0)
				p->proto = 0;
			else {
                                fprintf(stderr,"Cannot guess tunnel mode.\n");
                                exit(-1);
                        }
                } else if (strcmp(*argv, "remote") == 0) {
			inet_prefix raddr;
			NEXT_ARG();
			get_prefix(&raddr, *argv, preferred_family);
			if (raddr.family == AF_UNSPEC)
				invarg("\"remote\" address family is AF_UNSPEC", *argv);
			memcpy(&p->raddr, &raddr.data, sizeof(p->raddr));
		} else if (strcmp(*argv, "local") == 0) {
			inet_prefix laddr;
			NEXT_ARG();
			get_prefix(&laddr, *argv, preferred_family);
			if (laddr.family == AF_UNSPEC)
				invarg("\"local\" address family is AF_UNSPEC", *argv);
			memcpy(&p->laddr, &laddr.data, sizeof(p->laddr));
		} else if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			strncpy(medium, *argv, IFNAMSIZ - 1);
		} else if (strcmp(*argv, "encaplimit") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "none") == 0) {
				p->flags |= IP6_TNL_F_IGN_ENCAP_LIMIT;
			} else {
				__u8 uval;
				if (get_u8(&uval, *argv, 0) < -1)
					invarg("invalid ELIM", *argv);
				p->encap_limit = uval;
			}
		} else if (strcmp(*argv, "hoplimit") == 0 ||
			   strcmp(*argv, "ttl") == 0 ||
			   strcmp(*argv, "hlim") == 0) {
			__u8 uval;
			NEXT_ARG();
			if (get_u8(&uval, *argv, 0))
				invarg("invalid TTL", *argv);
			p->hop_limit = uval;
		} else if (strcmp(*argv, "tclass") == 0 ||
			   strcmp(*argv, "tc") == 0 ||
			   strcmp(*argv, "tos") == 0 ||
			   matches(*argv, "dsfield") == 0) {
			__u8 uval;
			NEXT_ARG();
			if (strcmp(*argv, "inherit") == 0)
				p->flags |= IP6_TNL_F_USE_ORIG_TCLASS;
			else {
				if (get_u8(&uval, *argv, 16))
					invarg("invalid TClass", *argv);
				p->flowinfo |= htonl((__u32)uval << 20) & IP6_FLOWINFO_TCLASS;
				p->flags &= ~IP6_TNL_F_USE_ORIG_TCLASS;
			}
		} else if (strcmp(*argv, "flowlabel") == 0 ||
			   strcmp(*argv, "fl") == 0) {
			__u32 uval;
			NEXT_ARG();
			if (strcmp(*argv, "inherit") == 0)
				p->flags |= IP6_TNL_F_USE_ORIG_FLOWLABEL;
			else {
				if (get_u32(&uval, *argv, 16))
					invarg("invalid Flowlabel", *argv);
				if (uval > 0xFFFFF)
					invarg("invalid Flowlabel", *argv);
				p->flowinfo |= htonl(uval) & IP6_FLOWINFO_FLOWLABEL;
				p->flags &= ~IP6_TNL_F_USE_ORIG_FLOWLABEL;
			}
		} else if (strcmp(*argv, "dscp") == 0) {
			NEXT_ARG();
			if (strcmp(*argv, "inherit") != 0)
				invarg("not inherit", *argv);
			p->flags |= IP6_TNL_F_RCV_DSCP_COPY;
		} else {
			if (strcmp(*argv, "name") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			if (p->name[0])
				duparg2("name", *argv);
			strncpy(p->name, *argv, IFNAMSIZ - 1);
			if (cmd == SIOCCHGTUNNEL && count == 0) {
				struct ip6_tnl_parm old_p;
				memset(&old_p, 0, sizeof(old_p));
				if (tnl_get_ioctl(*argv, &old_p))
					return -1;
				*p = old_p;
			}
		}
		count++;
		argc--; argv++;
	}
	if (medium[0]) {
		p->link = ll_name_to_index(medium);
		if (p->link == 0)
			return -1;
	}
	return 0;
}

static void ip6_tnl_parm_init(struct ip6_tnl_parm *p, int apply_default)
{
	memset(p, 0, sizeof(*p));
	p->proto = IPPROTO_IPV6;
	if (apply_default) {
		p->hop_limit = DEFAULT_TNL_HOP_LIMIT;
		p->encap_limit = IPV6_DEFAULT_TNL_ENCAP_LIMIT;
	}
}

/*
 * @p1: user specified parameter
 * @p2: database entry
 */
static int ip6_tnl_parm_match(const struct ip6_tnl_parm *p1,
			      const struct ip6_tnl_parm *p2)
{
	return ((!p1->link || p1->link == p2->link) &&
		(!p1->name[0] || strcmp(p1->name, p2->name) == 0) &&
		(memcmp(&p1->laddr, &in6addr_any, sizeof(p1->laddr)) == 0 ||
		 memcmp(&p1->laddr, &p2->laddr, sizeof(p1->laddr)) == 0) &&
		(memcmp(&p1->raddr, &in6addr_any, sizeof(p1->raddr)) == 0 ||
		 memcmp(&p1->raddr, &p2->raddr, sizeof(p1->raddr)) == 0) &&
		(!p1->proto || !p2->proto || p1->proto == p2->proto) &&
		(!p1->encap_limit || p1->encap_limit == p2->encap_limit) &&
		(!p1->hop_limit || p1->hop_limit == p2->hop_limit) &&
		(!(p1->flowinfo & IP6_FLOWINFO_TCLASS) ||
		 !((p1->flowinfo ^ p2->flowinfo) & IP6_FLOWINFO_TCLASS)) &&
		(!(p1->flowinfo & IP6_FLOWINFO_FLOWLABEL) ||
		 !((p1->flowinfo ^ p2->flowinfo) & IP6_FLOWINFO_FLOWLABEL)) &&
		(!p1->flags || (p1->flags & p2->flags)));
}

static int do_tunnels_list(struct ip6_tnl_parm *p)
{
	char buf[512];
	int err = -1;
	FILE *fp = fopen("/proc/net/dev", "r");
	if (fp == NULL) {
		perror("fopen");
		goto end;
	}

	/* skip two lines at the begenning of the file */
	if (!fgets(buf, sizeof(buf), fp) ||
	    !fgets(buf, sizeof(buf), fp)) {
		fprintf(stderr, "/proc/net/dev read error\n");
		return -1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char name[IFNAMSIZ];
		int index, type;
		unsigned long rx_bytes, rx_packets, rx_errs, rx_drops,
			rx_fifo, rx_frame,
			tx_bytes, tx_packets, tx_errs, tx_drops,
			tx_fifo, tx_colls, tx_carrier, rx_multi;
		struct ip6_tnl_parm p1;
		char *ptr;

		buf[sizeof(buf) - 1] = '\0';
		if ((ptr = strchr(buf, ':')) == NULL ||
		    (*ptr++ = 0, sscanf(buf, "%s", name) != 1)) {
			fprintf(stderr, "Wrong format of /proc/net/dev. Sorry.\n");
			goto end;
		}
		if (sscanf(ptr, "%ld%ld%ld%ld%ld%ld%ld%*d%ld%ld%ld%ld%ld%ld%ld",
			   &rx_bytes, &rx_packets, &rx_errs, &rx_drops,
			   &rx_fifo, &rx_frame, &rx_multi,
			   &tx_bytes, &tx_packets, &tx_errs, &tx_drops,
			   &tx_fifo, &tx_colls, &tx_carrier) != 14)
			continue;
		if (p->name[0] && strcmp(p->name, name))
			continue;
		index = ll_name_to_index(name);
		if (index == 0)
			continue;
		type = ll_index_to_type(index);
		if (type == -1) {
			fprintf(stderr, "Failed to get type of [%s]\n", name);
			continue;
		}
		if (type != ARPHRD_TUNNEL6)
			continue;
		memset(&p1, 0, sizeof(p1));
		ip6_tnl_parm_init(&p1, 0);
		strcpy(p1.name, name);
		p1.link = ll_name_to_index(p1.name);
		if (p1.link == 0)
			continue;
		if (tnl_get_ioctl(p1.name, &p1))
			continue;
		if (!ip6_tnl_parm_match(p, &p1))
			continue;
		print_tunnel(&p1);
		if (show_stats) {
			printf("%s", _SL_);
			printf("RX: Packets    Bytes        Errors CsumErrs OutOfSeq Mcasts%s", _SL_);
			printf("    %-10ld %-12ld %-6ld %-8ld %-8ld %-8ld%s",
			       rx_packets, rx_bytes, rx_errs, rx_frame, rx_fifo, rx_multi, _SL_);
			printf("TX: Packets    Bytes        Errors DeadLoop NoRoute  NoBufs%s", _SL_);
			printf("    %-10ld %-12ld %-6ld %-8ld %-8ld %-6ld",
			       tx_packets, tx_bytes, tx_errs, tx_colls, tx_carrier, tx_drops);
		}
		printf("\n");
	}
	err = 0;

 end:
	if (fp)
		fclose(fp);
	return err;
}

static int do_show(int argc, char **argv)
{
        struct ip6_tnl_parm p;

	ll_init_map(&rth);
	ip6_tnl_parm_init(&p, 0);
	p.proto = 0;  /* default to any */

        if (parse_args(argc, argv, SIOCGETTUNNEL, &p) < 0)
                return -1;

	if (!p.name[0] || show_stats)
		do_tunnels_list(&p);
	else {
		if (tnl_get_ioctl(p.name, &p))
			return -1;
		print_tunnel(&p);
		printf("\n");
	}

        return 0;
}

static int do_add(int cmd, int argc, char **argv)
{
	struct ip6_tnl_parm p;

	ip6_tnl_parm_init(&p, 1);

	if (parse_args(argc, argv, cmd, &p) < 0)
		return -1;

	return tnl_add_ioctl(cmd,
			     cmd == SIOCCHGTUNNEL && p.name[0] ?
			     p.name : "ip6tnl0", p.name, &p);
}

static int do_del(int argc, char **argv)
{
	struct ip6_tnl_parm p;

	ip6_tnl_parm_init(&p, 1);

	if (parse_args(argc, argv, SIOCDELTUNNEL, &p) < 0)
		return -1;

	return tnl_del_ioctl(p.name[0] ? p.name : "ip6tnl0", p.name, &p);
}

int do_ip6tunnel(int argc, char **argv)
{
	switch (preferred_family) {
	case AF_UNSPEC:
		preferred_family = AF_INET6;
		break;
	case AF_INET6:
		break;
	default:
		fprintf(stderr, "Unsupported family:%d\n", preferred_family);
		exit(-1);
	}

	if (argc > 0) {
		if (matches(*argv, "add") == 0)
			return do_add(SIOCADDTUNNEL, argc - 1, argv + 1);
		if (matches(*argv, "change") == 0)
			return do_add(SIOCCHGTUNNEL, argc - 1, argv + 1);
		if (matches(*argv, "del") == 0)
			return do_del(argc - 1, argv + 1);
		if (matches(*argv, "show") == 0 ||
		    matches(*argv, "lst") == 0 ||
		    matches(*argv, "list") == 0)
			return do_show(argc - 1, argv + 1);
		if (matches(*argv, "help") == 0)
			usage();
	} else
		return do_show(0, NULL);

	fprintf(stderr, "Command \"%s\" is unknown, try \"ip -f inet6 tunnel help\".\n", *argv);
	exit(-1);
}

#endif // NO_IPV6
