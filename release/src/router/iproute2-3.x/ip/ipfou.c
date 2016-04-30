/*
 * ipfou.c	FOU (foo over UDP) support
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Authors:	Tom Herbert <therbert@google.com>
 */

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <linux/fou.h>
#include <linux/genetlink.h>
#include <linux/ip.h>
#include <arpa/inet.h>

#include "libgenl.h"
#include "utils.h"
#include "ip_common.h"

static void usage(void)
{
	fprintf(stderr, "Usage: ip fou add port PORT { ipproto PROTO  | gue }\n");
	fprintf(stderr, "       ip fou del port PORT\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Where: PROTO { ipproto-name | 1..255 }\n");
	fprintf(stderr, "       PORT { 1..65535 }\n");

	exit(-1);
}

/* netlink socket */
static struct rtnl_handle genl_rth = { .fd = -1 };
static int genl_family = -1;

#define FOU_REQUEST(_req, _bufsiz, _cmd, _flags)	\
	GENL_REQUEST(_req, _bufsiz, genl_family, 0,	\
		     FOU_GENL_VERSION, _cmd, _flags)

static int fou_parse_opt(int argc, char **argv, struct nlmsghdr *n,
			 bool adding)
{
	__u16 port;
	int port_set = 0;
	__u8 ipproto, type;
	bool gue_set = false;
	int ipproto_set = 0;

	while (argc > 0) {
		if (!matches(*argv, "port")) {
			NEXT_ARG();

			if (get_u16(&port, *argv, 0) || port == 0)
				invarg("invalid port", *argv);
			port = htons(port);
			port_set = 1;
		} else if (!matches(*argv, "ipproto")) {
			struct protoent *servptr;

			NEXT_ARG();

			servptr = getprotobyname(*argv);
			if (servptr)
				ipproto = servptr->p_proto;
			else if (get_u8(&ipproto, *argv, 0) || ipproto == 0)
				invarg("invalid ipproto", *argv);
			ipproto_set = 1;
		} else if (!matches(*argv, "gue")) {
			gue_set = true;
		} else {
			fprintf(stderr, "fou: unknown command \"%s\"?\n", *argv);
			usage();
			return -1;
		}
		argc--, argv++;
	}

	if (!port_set) {
		fprintf(stderr, "fou: missing port\n");
		return -1;
	}

	if (!ipproto_set && !gue_set && adding) {
		fprintf(stderr, "fou: must set ipproto or gue\n");
		return -1;
	}

	if (ipproto_set && gue_set) {
		fprintf(stderr, "fou: cannot set ipproto and gue\n");
		return -1;
	}

	type = gue_set ? FOU_ENCAP_GUE : FOU_ENCAP_DIRECT;

	addattr16(n, 1024, FOU_ATTR_PORT, port);
	addattr8(n, 1024, FOU_ATTR_TYPE, type);

	if (ipproto_set)
		addattr8(n, 1024, FOU_ATTR_IPPROTO, ipproto);

	return 0;
}

static int do_add(int argc, char **argv)
{
	FOU_REQUEST(req, 1024, FOU_CMD_ADD, NLM_F_REQUEST);

	fou_parse_opt(argc, argv, &req.n, true);

	if (rtnl_talk(&genl_rth, &req.n, 0, 0, NULL) < 0)
		return -2;

	return 0;
}

static int do_del(int argc, char **argv)
{
	FOU_REQUEST(req, 1024, FOU_CMD_DEL, NLM_F_REQUEST);

	fou_parse_opt(argc, argv, &req.n, false);

	if (rtnl_talk(&genl_rth, &req.n, 0, 0, NULL) < 0)
		return -2;

	return 0;
}

int do_ipfou(int argc, char **argv)
{
	if (genl_family < 0) {
		if (rtnl_open_byproto(&genl_rth, 0, NETLINK_GENERIC) < 0) {
			fprintf(stderr, "Cannot open generic netlink socket\n");
			exit(1);
		}

		genl_family = genl_resolve_family(&genl_rth, FOU_GENL_NAME);
		if (genl_family < 0)
			exit(1);
	}

	if (argc < 1)
		usage();

	if (matches(*argv, "add") == 0)
		return do_add(argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return do_del(argc-1, argv+1);
	if (matches(*argv, "help") == 0)
		usage();

	fprintf(stderr, "Command \"%s\" is unknown, try \"ip fou help\".\n", *argv);
	exit(-1);
}

