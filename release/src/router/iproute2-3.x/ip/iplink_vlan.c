/*
 * iplink_vlan.c	VLAN device support
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:     Patrick McHardy <kaber@trash.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_vlan.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static void print_explain(FILE *f)
{
	fprintf(f,
		"Usage: ... vlan [ protocol VLANPROTO ] id VLANID"
		"                [ FLAG-LIST ]\n"
		"                [ ingress-qos-map QOS-MAP ] [ egress-qos-map QOS-MAP ]\n"
		"\n"
		"VLANPROTO: [ 802.1Q / 802.1ad ]\n"
		"VLANID := 0-4095\n"
		"FLAG-LIST := [ FLAG-LIST ] FLAG\n"
		"FLAG := [ reorder_hdr { on | off } ] [ gvrp { on | off } ] [ mvrp { on | off } ]\n"
		"        [ loose_binding { on | off } ]\n"
		"QOS-MAP := [ QOS-MAP ] QOS-MAPPING\n"
		"QOS-MAPPING := FROM:TO\n"
	);
}

static void explain(void)
{
	print_explain(stderr);
}

static int on_off(const char *msg, const char *arg)
{
	fprintf(stderr, "Error: argument of \"%s\" must be \"on\" or \"off\", not \"%s\"\n", msg, arg);
	return -1;
}

static int vlan_parse_qos_map(int *argcp, char ***argvp, struct nlmsghdr *n,
			      int attrtype)
{
	int argc = *argcp;
	char **argv = *argvp;
	struct ifla_vlan_qos_mapping m;
	struct rtattr *tail;

	tail = NLMSG_TAIL(n);
	addattr_l(n, 1024, attrtype, NULL, 0);

	while (argc > 0) {
		char *colon = strchr(*argv, ':');

		if (!colon)
			break;
		*colon = '\0';

		if (get_u32(&m.from, *argv, 0))
			return 1;
		if (get_u32(&m.to, colon + 1, 0))
			return 1;
		argc--, argv++;

		addattr_l(n, 1024, IFLA_VLAN_QOS_MAPPING, &m, sizeof(m));
	}

	tail->rta_len = (void *) NLMSG_TAIL(n) - (void *)tail;

	*argcp = argc;
	*argvp = argv;
	return 0;
}

static int vlan_parse_opt(struct link_util *lu, int argc, char **argv,
			  struct nlmsghdr *n)
{
	struct ifla_vlan_flags flags = { 0 };
	__u16 id, proto;

	while (argc > 0) {
		if (matches(*argv, "protocol") == 0) {
			NEXT_ARG();
			if (ll_proto_a2n(&proto, *argv))
				invarg("protocol is invalid", *argv);
			addattr_l(n, 1024, IFLA_VLAN_PROTOCOL, &proto, 2);
		} else if (matches(*argv, "id") == 0) {
			NEXT_ARG();
			if (get_u16(&id, *argv, 0))
				invarg("id is invalid", *argv);
			addattr_l(n, 1024, IFLA_VLAN_ID, &id, 2);
		} else if (matches(*argv, "reorder_hdr") == 0) {
			NEXT_ARG();
			flags.mask |= VLAN_FLAG_REORDER_HDR;
			if (strcmp(*argv, "on") == 0)
				flags.flags |= VLAN_FLAG_REORDER_HDR;
			else if (strcmp(*argv, "off") == 0)
				flags.flags &= ~VLAN_FLAG_REORDER_HDR;
			else
				return on_off("reorder_hdr", *argv);
		} else if (matches(*argv, "gvrp") == 0) {
			NEXT_ARG();
			flags.mask |= VLAN_FLAG_GVRP;
			if (strcmp(*argv, "on") == 0)
				flags.flags |= VLAN_FLAG_GVRP;
			else if (strcmp(*argv, "off") == 0)
				flags.flags &= ~VLAN_FLAG_GVRP;
			else
				return on_off("gvrp", *argv);
		} else if (matches(*argv, "mvrp") == 0) {
			NEXT_ARG();
			flags.mask |= VLAN_FLAG_MVRP;
			if (strcmp(*argv, "on") == 0)
				flags.flags |= VLAN_FLAG_MVRP;
			else if (strcmp(*argv, "off") == 0)
				flags.flags &= ~VLAN_FLAG_MVRP;
			else
				return on_off("mvrp", *argv);
		} else if (matches(*argv, "loose_binding") == 0) {
			NEXT_ARG();
			flags.mask |= VLAN_FLAG_LOOSE_BINDING;
			if (strcmp(*argv, "on") == 0)
				flags.flags |= VLAN_FLAG_LOOSE_BINDING;
			else if (strcmp(*argv, "off") == 0)
				flags.flags &= ~VLAN_FLAG_LOOSE_BINDING;
			else
				return on_off("loose_binding", *argv);
		} else if (matches(*argv, "ingress-qos-map") == 0) {
			NEXT_ARG();
			if (vlan_parse_qos_map(&argc, &argv, n,
					       IFLA_VLAN_INGRESS_QOS))
				invarg("invalid ingress-qos-map", *argv);
			continue;
		} else if (matches(*argv, "egress-qos-map") == 0) {
			NEXT_ARG();
			if (vlan_parse_qos_map(&argc, &argv, n,
					       IFLA_VLAN_EGRESS_QOS))
				invarg("invalid egress-qos-map", *argv);
			continue;
		} else if (matches(*argv, "help") == 0) {
			explain();
			return -1;
		} else {
			fprintf(stderr, "vlan: unknown command \"%s\"?\n", *argv);
			explain();
			return -1;
		}
		argc--, argv++;
	}

	if (flags.mask)
		addattr_l(n, 1024, IFLA_VLAN_FLAGS, &flags, sizeof(flags));

	return 0;
}

static void vlan_print_map(FILE *f, char *name, struct rtattr *attr)
{
	struct ifla_vlan_qos_mapping *m;
	struct rtattr *i;
	int rem;

	fprintf(f, "\n      %s { ", name);

	rem = RTA_PAYLOAD(attr);
	for (i = RTA_DATA(attr); RTA_OK(i, rem); i = RTA_NEXT(i, rem)) {
		m = RTA_DATA(i);
		fprintf(f, "%u:%u ", m->from, m->to);
	}
	fprintf(f, "} ");
}

static void vlan_print_flags(FILE *fp, __u32 flags)
{
	fprintf(fp, "<");
#define _PF(f)	if (flags & VLAN_FLAG_##f) { \
			flags &= ~ VLAN_FLAG_##f; \
			fprintf(fp, #f "%s", flags ? "," : ""); \
		}
	_PF(REORDER_HDR);
	_PF(GVRP);
	_PF(MVRP);
	_PF(LOOSE_BINDING);
#undef _PF
	if (flags)
		fprintf(fp, "%x", flags);
	fprintf(fp, "> ");
}

static void vlan_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	struct ifla_vlan_flags *flags;
	SPRINT_BUF(b1);

	if (!tb)
		return;

	if (tb[IFLA_VLAN_PROTOCOL] &&
	    RTA_PAYLOAD(tb[IFLA_VLAN_PROTOCOL]) < sizeof(__u16))
		return;
	if (!tb[IFLA_VLAN_ID] ||
	    RTA_PAYLOAD(tb[IFLA_VLAN_ID]) < sizeof(__u16))
		return;

	if (tb[IFLA_VLAN_PROTOCOL])
		fprintf(f, "protocol %s ",
			ll_proto_n2a(rta_getattr_u16(tb[IFLA_VLAN_PROTOCOL]),
				     b1, sizeof(b1)));
	else
		fprintf(f, "protocol 802.1q ");

	fprintf(f, "id %u ", rta_getattr_u16(tb[IFLA_VLAN_ID]));

	if (tb[IFLA_VLAN_FLAGS]) {
		if (RTA_PAYLOAD(tb[IFLA_VLAN_FLAGS]) < sizeof(*flags))
			return;
		flags = RTA_DATA(tb[IFLA_VLAN_FLAGS]);
		vlan_print_flags(f, flags->flags);
	}
	if (tb[IFLA_VLAN_INGRESS_QOS])
		vlan_print_map(f, "ingress-qos-map", tb[IFLA_VLAN_INGRESS_QOS]);
	if (tb[IFLA_VLAN_EGRESS_QOS])
		vlan_print_map(f, "egress-qos-map", tb[IFLA_VLAN_EGRESS_QOS]);
}

static void vlan_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_explain(f);
}

struct link_util vlan_link_util = {
	.id		= "vlan",
	.maxattr	= IFLA_VLAN_MAX,
	.parse_opt	= vlan_parse_opt,
	.print_opt	= vlan_print_opt,
	.print_help	= vlan_print_help,
};
