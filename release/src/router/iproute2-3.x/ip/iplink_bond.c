/*
 * iplink_bond.c	Bonding device support
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Jiri Pirko <jiri@resnulli.us>
 *              Scott Feldman <sfeldma@cumulusnetworks.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_link.h>
#include <linux/if_ether.h>
#include <net/if.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

#define BOND_MAX_ARP_TARGETS    16

static const char *mode_tbl[] = {
	"balance-rr",
	"active-backup",
	"balance-xor",
	"broadcast",
	"802.3ad",
	"balance-tlb",
	"balance-alb",
	NULL,
};

static const char *arp_validate_tbl[] = {
	"none",
	"active",
	"backup",
	"all",
	NULL,
};

static const char *arp_all_targets_tbl[] = {
	"any",
	"all",
	NULL,
};

static const char *primary_reselect_tbl[] = {
	"always",
	"better",
	"failure",
	NULL,
};

static const char *fail_over_mac_tbl[] = {
	"none",
	"active",
	"follow",
	NULL,
};

static const char *xmit_hash_policy_tbl[] = {
	"layer2",
	"layer3+4",
	"layer2+3",
	"encap2+3",
	"encap3+4",
	NULL,
};

static const char *lacp_rate_tbl[] = {
	"slow",
	"fast",
	NULL,
};

static const char *ad_select_tbl[] = {
	"stable",
	"bandwidth",
	"count",
	NULL,
};

static const char *get_name(const char **tbl, int index)
{
	int i;

	for (i = 0; tbl[i]; i++)
		if (i == index)
			return tbl[i];

	return "UNKNOWN";
}

static int get_index(const char **tbl, char *name)
{
	int i, index;

	/* check for integer index passed in instead of name */
	if (get_integer(&index, name, 10) == 0)
		for (i = 0; tbl[i]; i++)
			if (i == index)
				return i;

	for (i = 0; tbl[i]; i++)
		if (strcmp(tbl[i], name) == 0)
			return i;

	return -1;
}

static void print_explain(FILE *f)
{
	fprintf(f,
		"Usage: ... bond [ mode BONDMODE ] [ active_slave SLAVE_DEV ]\n"
		"                [ clear_active_slave ] [ miimon MIIMON ]\n"
		"                [ updelay UPDELAY ] [ downdelay DOWNDELAY ]\n"
		"                [ use_carrier USE_CARRIER ]\n"
		"                [ arp_interval ARP_INTERVAL ]\n"
		"                [ arp_validate ARP_VALIDATE ]\n"
		"                [ arp_all_targets ARP_ALL_TARGETS ]\n"
		"                [ arp_ip_target [ ARP_IP_TARGET, ... ] ]\n"
		"                [ primary SLAVE_DEV ]\n"
		"                [ primary_reselect PRIMARY_RESELECT ]\n"
		"                [ fail_over_mac FAIL_OVER_MAC ]\n"
		"                [ xmit_hash_policy XMIT_HASH_POLICY ]\n"
		"                [ resend_igmp RESEND_IGMP ]\n"
		"                [ num_grat_arp|num_unsol_na NUM_GRAT_ARP|NUM_UNSOL_NA ]\n"
		"                [ all_slaves_active ALL_SLAVES_ACTIVE ]\n"
		"                [ min_links MIN_LINKS ]\n"
		"                [ lp_interval LP_INTERVAL ]\n"
		"                [ packets_per_slave PACKETS_PER_SLAVE ]\n"
		"                [ lacp_rate LACP_RATE ]\n"
		"                [ ad_select AD_SELECT ]\n"
		"\n"
		"BONDMODE := balance-rr|active-backup|balance-xor|broadcast|802.3ad|balance-tlb|balance-alb\n"
		"ARP_VALIDATE := none|active|backup|all\n"
		"ARP_ALL_TARGETS := any|all\n"
		"PRIMARY_RESELECT := always|better|failure\n"
		"FAIL_OVER_MAC := none|active|follow\n"
		"XMIT_HASH_POLICY := layer2|layer2+3|layer3+4\n"
		"LACP_RATE := slow|fast\n"
		"AD_SELECT := stable|bandwidth|count\n"
	);
}

static void explain(void)
{
	print_explain(stderr);
}

static int bond_parse_opt(struct link_util *lu, int argc, char **argv,
			  struct nlmsghdr *n)
{
	__u8 mode, use_carrier, primary_reselect, fail_over_mac;
	__u8 xmit_hash_policy, num_peer_notif, all_slaves_active;
	__u8 lacp_rate, ad_select;
	__u32 miimon, updelay, downdelay, arp_interval, arp_validate;
	__u32 arp_all_targets, resend_igmp, min_links, lp_interval;
	__u32 packets_per_slave;
	unsigned ifindex;

	while (argc > 0) {
		if (matches(*argv, "mode") == 0) {
			NEXT_ARG();
			if (get_index(mode_tbl, *argv) < 0) {
				invarg("invalid mode", *argv);
				return -1;
			}
			mode = get_index(mode_tbl, *argv);
			addattr8(n, 1024, IFLA_BOND_MODE, mode);
		} else if (matches(*argv, "active_slave") == 0) {
			NEXT_ARG();
			ifindex = if_nametoindex(*argv);
			if (!ifindex)
				return -1;
			addattr32(n, 1024, IFLA_BOND_ACTIVE_SLAVE, ifindex);
		} else if (matches(*argv, "clear_active_slave") == 0) {
			addattr32(n, 1024, IFLA_BOND_ACTIVE_SLAVE, 0);
		} else if (matches(*argv, "miimon") == 0) {
			NEXT_ARG();
			if (get_u32(&miimon, *argv, 0)) {
				invarg("invalid miimon", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BOND_MIIMON, miimon);
		} else if (matches(*argv, "updelay") == 0) {
			NEXT_ARG();
			if (get_u32(&updelay, *argv, 0)) {
				invarg("invalid updelay", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BOND_UPDELAY, updelay);
		} else if (matches(*argv, "downdelay") == 0) {
			NEXT_ARG();
			if (get_u32(&downdelay, *argv, 0)) {
				invarg("invalid downdelay", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BOND_DOWNDELAY, downdelay);
		} else if (matches(*argv, "use_carrier") == 0) {
			NEXT_ARG();
			if (get_u8(&use_carrier, *argv, 0)) {
				invarg("invalid use_carrier", *argv);
				return -1;
			}
			addattr8(n, 1024, IFLA_BOND_USE_CARRIER, use_carrier);
		} else if (matches(*argv, "arp_interval") == 0) {
			NEXT_ARG();
			if (get_u32(&arp_interval, *argv, 0)) {
				invarg("invalid arp_interval", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BOND_ARP_INTERVAL, arp_interval);
		} else if (matches(*argv, "arp_ip_target") == 0) {
			struct rtattr * nest = addattr_nest(n, 1024,
				IFLA_BOND_ARP_IP_TARGET);
			if (NEXT_ARG_OK()) {
				NEXT_ARG();
				char *targets = strdupa(*argv);
				char *target = strtok(targets, ",");
				int i;

				for(i = 0; target && i < BOND_MAX_ARP_TARGETS; i++) {
					__u32 addr = get_addr32(target);
					addattr32(n, 1024, i, addr);
					target = strtok(NULL, ",");
				}
				addattr_nest_end(n, nest);
			}
			addattr_nest_end(n, nest);
		} else if (matches(*argv, "arp_validate") == 0) {
			NEXT_ARG();
			if (get_index(arp_validate_tbl, *argv) < 0) {
				invarg("invalid arp_validate", *argv);
				return -1;
			}
			arp_validate = get_index(arp_validate_tbl, *argv);
			addattr32(n, 1024, IFLA_BOND_ARP_VALIDATE, arp_validate);
		} else if (matches(*argv, "arp_all_targets") == 0) {
			NEXT_ARG();
			if (get_index(arp_all_targets_tbl, *argv) < 0) {
				invarg("invalid arp_all_targets", *argv);
				return -1;
			}
			arp_all_targets = get_index(arp_all_targets_tbl, *argv);
			addattr32(n, 1024, IFLA_BOND_ARP_ALL_TARGETS, arp_all_targets);
		} else if (matches(*argv, "primary") == 0) {
			NEXT_ARG();
			ifindex = if_nametoindex(*argv);
			if (!ifindex)
				return -1;
			addattr32(n, 1024, IFLA_BOND_PRIMARY, ifindex);
		} else if (matches(*argv, "primary_reselect") == 0) {
			NEXT_ARG();
			if (get_index(primary_reselect_tbl, *argv) < 0) {
				invarg("invalid primary_reselect", *argv);
				return -1;
			}
			primary_reselect = get_index(primary_reselect_tbl, *argv);
			addattr8(n, 1024, IFLA_BOND_PRIMARY_RESELECT,
				 primary_reselect);
		} else if (matches(*argv, "fail_over_mac") == 0) {
			NEXT_ARG();
			if (get_index(fail_over_mac_tbl, *argv) < 0) {
				invarg("invalid fail_over_mac", *argv);
				return -1;
			}
			fail_over_mac = get_index(fail_over_mac_tbl, *argv);
			addattr8(n, 1024, IFLA_BOND_FAIL_OVER_MAC,
				 fail_over_mac);
		} else if (matches(*argv, "xmit_hash_policy") == 0) {
			NEXT_ARG();
			if (get_index(xmit_hash_policy_tbl, *argv) < 0) {
				invarg("invalid xmit_hash_policy", *argv);
				return -1;
			}
			xmit_hash_policy = get_index(xmit_hash_policy_tbl, *argv);
			addattr8(n, 1024, IFLA_BOND_XMIT_HASH_POLICY,
				 xmit_hash_policy);
		} else if (matches(*argv, "resend_igmp") == 0) {
			NEXT_ARG();
			if (get_u32(&resend_igmp, *argv, 0)) {
				invarg("invalid resend_igmp", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BOND_RESEND_IGMP, resend_igmp);
		} else if (matches(*argv, "num_grat_arp") == 0 ||
			   matches(*argv, "num_unsol_na") == 0) {
			NEXT_ARG();
			if (get_u8(&num_peer_notif, *argv, 0)) {
				invarg("invalid num_grat_arp|num_unsol_na",
				       *argv);
				return -1;
			}
			addattr8(n, 1024, IFLA_BOND_NUM_PEER_NOTIF,
				 num_peer_notif);
		} else if (matches(*argv, "all_slaves_active") == 0) {
			NEXT_ARG();
			if (get_u8(&all_slaves_active, *argv, 0)) {
				invarg("invalid all_slaves_active", *argv);
				return -1;
			}
			addattr8(n, 1024, IFLA_BOND_ALL_SLAVES_ACTIVE,
				 all_slaves_active);
		} else if (matches(*argv, "min_links") == 0) {
			NEXT_ARG();
			if (get_u32(&min_links, *argv, 0)) {
				invarg("invalid min_links", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BOND_MIN_LINKS, min_links);
		} else if (matches(*argv, "lp_interval") == 0) {
			NEXT_ARG();
			if (get_u32(&lp_interval, *argv, 0)) {
				invarg("invalid lp_interval", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BOND_LP_INTERVAL, lp_interval);
		} else if (matches(*argv, "packets_per_slave") == 0) {
			NEXT_ARG();
			if (get_u32(&packets_per_slave, *argv, 0)) {
				invarg("invalid packets_per_slave", *argv);
				return -1;
			}
			addattr32(n, 1024, IFLA_BOND_PACKETS_PER_SLAVE,
				  packets_per_slave);
		} else if (matches(*argv, "lacp_rate") == 0) {
			NEXT_ARG();
			if (get_index(lacp_rate_tbl, *argv) < 0) {
				invarg("invalid lacp_rate", *argv);
				return -1;
			}
			lacp_rate = get_index(lacp_rate_tbl, *argv);
			addattr8(n, 1024, IFLA_BOND_AD_LACP_RATE, lacp_rate);
		} else if (matches(*argv, "ad_select") == 0) {
			NEXT_ARG();
			if (get_index(ad_select_tbl, *argv) < 0) {
				invarg("invalid ad_select", *argv);
				return -1;
			}
			ad_select = get_index(ad_select_tbl, *argv);
			addattr8(n, 1024, IFLA_BOND_AD_SELECT, ad_select);
		} else if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "bond: unknown command \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--, argv++;
	}

	return 0;
}

static void bond_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	unsigned ifindex;

	if (!tb)
		return;

	if (tb[IFLA_BOND_MODE]) {
		const char *mode = get_name(mode_tbl,
			rta_getattr_u8(tb[IFLA_BOND_MODE]));
		fprintf(f, "mode %s ", mode);
	}

	if (tb[IFLA_BOND_ACTIVE_SLAVE] &&
	    (ifindex = rta_getattr_u32(tb[IFLA_BOND_ACTIVE_SLAVE]))) {
		char buf[IFNAMSIZ];
		const char *n = if_indextoname(ifindex, buf);

		if (n)
			fprintf(f, "active_slave %s ", n);
		else
			fprintf(f, "active_slave %u ", ifindex);
	}

	if (tb[IFLA_BOND_MIIMON])
		fprintf(f, "miimon %u ", rta_getattr_u32(tb[IFLA_BOND_MIIMON]));

	if (tb[IFLA_BOND_UPDELAY])
		fprintf(f, "updelay %u ", rta_getattr_u32(tb[IFLA_BOND_UPDELAY]));

	if (tb[IFLA_BOND_DOWNDELAY])
		fprintf(f, "downdelay %u ",
			rta_getattr_u32(tb[IFLA_BOND_DOWNDELAY]));

	if (tb[IFLA_BOND_USE_CARRIER])
		fprintf(f, "use_carrier %u ",
			rta_getattr_u8(tb[IFLA_BOND_USE_CARRIER]));

	if (tb[IFLA_BOND_ARP_INTERVAL])
		fprintf(f, "arp_interval %u ",
			rta_getattr_u32(tb[IFLA_BOND_ARP_INTERVAL]));

	if (tb[IFLA_BOND_ARP_IP_TARGET]) {
		struct rtattr *iptb[BOND_MAX_ARP_TARGETS + 1];
		char buf[INET_ADDRSTRLEN];
		int i;

		parse_rtattr_nested(iptb, BOND_MAX_ARP_TARGETS,
			tb[IFLA_BOND_ARP_IP_TARGET]);

		if (iptb[0])
			fprintf(f, "arp_ip_target ");

		for (i = 0; i < BOND_MAX_ARP_TARGETS; i++) {
			if (iptb[i])
				fprintf(f, "%s",
					rt_addr_n2a(AF_INET,
						    RTA_DATA(iptb[i]),
						    buf,
						    INET_ADDRSTRLEN));
			if (i < BOND_MAX_ARP_TARGETS-1 && iptb[i+1])
				fprintf(f, ",");
		}

		if (iptb[0])
			fprintf(f, " ");
	}

	if (tb[IFLA_BOND_ARP_VALIDATE]) {
		const char *arp_validate = get_name(arp_validate_tbl,
			rta_getattr_u32(tb[IFLA_BOND_ARP_VALIDATE]));
		fprintf(f, "arp_validate %s ", arp_validate);
	}

	if (tb[IFLA_BOND_ARP_ALL_TARGETS]) {
		const char *arp_all_targets = get_name(arp_all_targets_tbl,
			rta_getattr_u32(tb[IFLA_BOND_ARP_ALL_TARGETS]));
		fprintf(f, "arp_all_targets %s ", arp_all_targets);
	}

	if (tb[IFLA_BOND_PRIMARY] &&
	    (ifindex = rta_getattr_u32(tb[IFLA_BOND_PRIMARY]))) {
		char buf[IFNAMSIZ];
		const char *n = if_indextoname(ifindex, buf);

		if (n)
			fprintf(f, "primary %s ", n);
		else
			fprintf(f, "primary %u ", ifindex);
	}

	if (tb[IFLA_BOND_PRIMARY_RESELECT]) {
		const char *primary_reselect = get_name(primary_reselect_tbl,
			rta_getattr_u8(tb[IFLA_BOND_PRIMARY_RESELECT]));
		fprintf(f, "primary_reselect %s ", primary_reselect);
	}

	if (tb[IFLA_BOND_FAIL_OVER_MAC]) {
		const char *fail_over_mac = get_name(fail_over_mac_tbl,
			rta_getattr_u8(tb[IFLA_BOND_FAIL_OVER_MAC]));
		fprintf(f, "fail_over_mac %s ", fail_over_mac);
	}

	if (tb[IFLA_BOND_XMIT_HASH_POLICY]) {
		const char *xmit_hash_policy = get_name(xmit_hash_policy_tbl,
			rta_getattr_u8(tb[IFLA_BOND_XMIT_HASH_POLICY]));
		fprintf(f, "xmit_hash_policy %s ", xmit_hash_policy);
	}

	if (tb[IFLA_BOND_RESEND_IGMP])
		fprintf(f, "resend_igmp %u ",
			rta_getattr_u32(tb[IFLA_BOND_RESEND_IGMP]));

	if (tb[IFLA_BOND_NUM_PEER_NOTIF])
		fprintf(f, "num_grat_arp %u ",
			rta_getattr_u8(tb[IFLA_BOND_NUM_PEER_NOTIF]));

	if (tb[IFLA_BOND_ALL_SLAVES_ACTIVE])
		fprintf(f, "all_slaves_active %u ",
			rta_getattr_u8(tb[IFLA_BOND_ALL_SLAVES_ACTIVE]));

	if (tb[IFLA_BOND_MIN_LINKS])
		fprintf(f, "min_links %u ",
			rta_getattr_u32(tb[IFLA_BOND_MIN_LINKS]));

	if (tb[IFLA_BOND_LP_INTERVAL])
		fprintf(f, "lp_interval %u ",
			rta_getattr_u32(tb[IFLA_BOND_LP_INTERVAL]));

	if (tb[IFLA_BOND_PACKETS_PER_SLAVE])
		fprintf(f, "packets_per_slave %u ",
			rta_getattr_u32(tb[IFLA_BOND_PACKETS_PER_SLAVE]));

	if (tb[IFLA_BOND_AD_LACP_RATE]) {
		const char *lacp_rate = get_name(lacp_rate_tbl,
			rta_getattr_u8(tb[IFLA_BOND_AD_LACP_RATE]));
		fprintf(f, "lacp_rate %s ", lacp_rate);
	}

	if (tb[IFLA_BOND_AD_SELECT]) {
		const char *ad_select = get_name(ad_select_tbl,
			rta_getattr_u8(tb[IFLA_BOND_AD_SELECT]));
		fprintf(f, "ad_select %s ", ad_select);
	}

	if (tb[IFLA_BOND_AD_INFO]) {
		struct rtattr *adtb[IFLA_BOND_AD_INFO_MAX + 1];

		parse_rtattr_nested(adtb, IFLA_BOND_AD_INFO_MAX,
			tb[IFLA_BOND_AD_INFO]);

		if (adtb[IFLA_BOND_AD_INFO_AGGREGATOR])
			fprintf(f, "ad_aggregator %d ",
			  rta_getattr_u16(adtb[IFLA_BOND_AD_INFO_AGGREGATOR]));

		if (adtb[IFLA_BOND_AD_INFO_NUM_PORTS])
			fprintf(f, "ad_num_ports %d ",
			  rta_getattr_u16(adtb[IFLA_BOND_AD_INFO_NUM_PORTS]));

		if (adtb[IFLA_BOND_AD_INFO_ACTOR_KEY])
			fprintf(f, "ad_actor_key %d ",
			  rta_getattr_u16(adtb[IFLA_BOND_AD_INFO_ACTOR_KEY]));

		if (adtb[IFLA_BOND_AD_INFO_PARTNER_KEY])
			fprintf(f, "ad_partner_key %d ",
			  rta_getattr_u16(adtb[IFLA_BOND_AD_INFO_PARTNER_KEY]));

		if (adtb[IFLA_BOND_AD_INFO_PARTNER_MAC]) {
			unsigned char *p =
				RTA_DATA(adtb[IFLA_BOND_AD_INFO_PARTNER_MAC]);
			SPRINT_BUF(b);
			fprintf(f, "ad_partner_mac %s ",
				ll_addr_n2a(p, ETH_ALEN, 0, b, sizeof(b)));
		}
	}
}

static void bond_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_explain(f);
}

struct link_util bond_link_util = {
	.id		= "bond",
	.maxattr	= IFLA_BOND_MAX,
	.parse_opt	= bond_parse_opt,
	.print_opt	= bond_print_opt,
	.print_help	= bond_print_help,
};
