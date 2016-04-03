/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <libipset/data.h>			/* IPSET_OPT_* */
#include <libipset/parse.h>			/* parser functions */
#include <libipset/print.h>			/* printing functions */
#include <libipset/types.h>			/* prototypes */

/* Parse commandline arguments */
static const struct ipset_arg bitmap_ip_create_args0[] = {
	{ .name = { "range", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_netrange,	.print = ipset_print_ip,
	},
	{ .name = { "netmask", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NETMASK,
	  .parse = ipset_parse_netmask,		.print = ipset_print_number,
	},
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	/* Backward compatibility */
	{ .name = { "from", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_single_ip,
	},
	{ .name = { "to", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP_TO,
	  .parse = ipset_parse_single_ip,
	},
	{ .name = { "network", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_net,
	},
	{ },
};

static const struct ipset_arg bitmap_ip_add_args0[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ },
};

static const char bitmap_ip_usage0[] =
"create SETNAME bitmap:ip range IP/CIDR|FROM-TO\n"
"               [netmask CIDR] [timeout VALUE]\n"
"add    SETNAME IP|IP/CIDR|FROM-TO [timeout VALUE]\n"
"del    SETNAME IP|IP/CIDR|FROM-TO\n"
"test   SETNAME IP\n\n"
"where IP, FROM and TO are IPv4 addresses (or hostnames),\n"
"      CIDR is a valid IPv4 CIDR prefix.\n";

static struct ipset_type ipset_bitmap_ip0 = {
	.name = "bitmap:ip",
	.alias = { "ipmap", NULL },
	.revision = 0,
	.family = NFPROTO_IPV4,
	.dimension = IPSET_DIM_ONE,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
	},
	.args = {
		[IPSET_CREATE] = bitmap_ip_create_args0,
		[IPSET_ADD] = bitmap_ip_add_args0,
	},
	.mandatory = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_NETMASK)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP),
	},

	.usage = bitmap_ip_usage0,
	.description = "Initial revision",
};

/* Parse commandline arguments */
static const struct ipset_arg bitmap_ip_create_args1[] = {
	{ .name = { "range", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_netrange,	.print = ipset_print_ip,
	},
	{ .name = { "netmask", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NETMASK,
	  .parse = ipset_parse_netmask,		.print = ipset_print_number,
	},
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "counters", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_COUNTERS,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	/* Backward compatibility */
	{ .name = { "from", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_single_ip,
	},
	{ .name = { "to", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP_TO,
	  .parse = ipset_parse_single_ip,
	},
	{ .name = { "network", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_net,
	},
	{ },
};

static const struct ipset_arg bitmap_ip_add_args1[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "packets", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_PACKETS,
	  .parse = ipset_parse_uint64,		.print = ipset_print_number,
	},
	{ .name = { "bytes", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_BYTES,
	  .parse = ipset_parse_uint64,		.print = ipset_print_number,
	},
	{ },
};

static const char bitmap_ip_usage1[] =
"create SETNAME bitmap:ip range IP/CIDR|FROM-TO\n"
"               [netmask CIDR] [timeout VALUE] [counters]\n"
"add    SETNAME IP|IP/CIDR|FROM-TO [timeout VALUE]\n"
"               [packets VALUE] [bytes VALUE]\n"
"del    SETNAME IP|IP/CIDR|FROM-TO\n"
"test   SETNAME IP\n\n"
"where IP, FROM and TO are IPv4 addresses (or hostnames),\n"
"      CIDR is a valid IPv4 CIDR prefix.\n";

static struct ipset_type ipset_bitmap_ip1 = {
	.name = "bitmap:ip",
	.alias = { "ipmap", NULL },
	.revision = 1,
	.family = NFPROTO_IPV4,
	.dimension = IPSET_DIM_ONE,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
	},
	.args = {
		[IPSET_CREATE] = bitmap_ip_create_args1,
		[IPSET_ADD] = bitmap_ip_add_args1,
	},
	.mandatory = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_NETMASK)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP),
	},

	.usage = bitmap_ip_usage1,
	.description = "counters support",
};

/* Parse commandline arguments */
static const struct ipset_arg bitmap_ip_create_args2[] = {
	{ .name = { "range", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_netrange,	.print = ipset_print_ip,
	},
	{ .name = { "netmask", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NETMASK,
	  .parse = ipset_parse_netmask,		.print = ipset_print_number,
	},
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "counters", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_COUNTERS,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ .name = { "comment", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_CREATE_COMMENT,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	/* Backward compatibility */
	{ .name = { "from", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_single_ip,
	},
	{ .name = { "to", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP_TO,
	  .parse = ipset_parse_single_ip,
	},
	{ .name = { "network", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_net,
	},
	{ },
};

static const struct ipset_arg bitmap_ip_add_args2[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "packets", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_PACKETS,
	  .parse = ipset_parse_uint64,		.print = ipset_print_number,
	},
	{ .name = { "bytes", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_BYTES,
	  .parse = ipset_parse_uint64,		.print = ipset_print_number,
	},
	{ .name = { "comment", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_ADT_COMMENT,
	  .parse = ipset_parse_comment,		.print = ipset_print_comment,
	},
	{ },
};

static const char bitmap_ip_usage2[] =
"create SETNAME bitmap:ip range IP/CIDR|FROM-TO\n"
"               [netmask CIDR] [timeout VALUE] [counters] [comment]\n"
"add    SETNAME IP|IP/CIDR|FROM-TO [timeout VALUE]\n"
"               [packets VALUE] [bytes VALUE] [comment \"string\"]\n"
"del    SETNAME IP|IP/CIDR|FROM-TO\n"
"test   SETNAME IP\n\n"
"where IP, FROM and TO are IPv4 addresses (or hostnames),\n"
"      CIDR is a valid IPv4 CIDR prefix.\n";

static struct ipset_type ipset_bitmap_ip2 = {
	.name = "bitmap:ip",
	.alias = { "ipmap", NULL },
	.revision = 2,
	.family = NFPROTO_IPV4,
	.dimension = IPSET_DIM_ONE,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
	},
	.args = {
		[IPSET_CREATE] = bitmap_ip_create_args2,
		[IPSET_ADD] = bitmap_ip_add_args2,
	},
	.mandatory = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_NETMASK)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS)
			| IPSET_FLAG(IPSET_OPT_CREATE_COMMENT),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES)
			| IPSET_FLAG(IPSET_OPT_ADT_COMMENT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP),
	},

	.usage = bitmap_ip_usage2,
	.description = "comment support",
};

/* Parse commandline arguments */
static const struct ipset_arg bitmap_ip_create_args3[] = {
	{ .name = { "range", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_netrange,	.print = ipset_print_ip,
	},
	{ .name = { "netmask", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NETMASK,
	  .parse = ipset_parse_netmask,		.print = ipset_print_number,
	},
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "counters", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_COUNTERS,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ .name = { "comment", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_CREATE_COMMENT,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ .name = { "skbinfo", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_SKBINFO,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	/* Backward compatibility */
	{ .name = { "from", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_single_ip,
	},
	{ .name = { "to", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP_TO,
	  .parse = ipset_parse_single_ip,
	},
	{ .name = { "network", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_IP,
	  .parse = ipset_parse_net,
	},
	{ },
};

static const struct ipset_arg bitmap_ip_add_args3[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "packets", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_PACKETS,
	  .parse = ipset_parse_uint64,		.print = ipset_print_number,
	},
	{ .name = { "bytes", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_BYTES,
	  .parse = ipset_parse_uint64,		.print = ipset_print_number,
	},
	{ .name = { "comment", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_ADT_COMMENT,
	  .parse = ipset_parse_comment,		.print = ipset_print_comment,
	},
	{ .name = { "skbmark", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_SKBMARK,
	  .parse = ipset_parse_skbmark,		.print = ipset_print_skbmark,
	},
	{ .name = { "skbprio", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_SKBPRIO,
	  .parse = ipset_parse_skbprio,		.print = ipset_print_skbprio,
	},
	{ .name = { "skbqueue", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_SKBQUEUE,
	  .parse = ipset_parse_uint16,		.print = ipset_print_number,
	},
	{ },
};

static const char bitmap_ip_usage3[] =
"create SETNAME bitmap:ip range IP/CIDR|FROM-TO\n"
"               [netmask CIDR] [timeout VALUE] [counters] [comment]\n"
"		[skbinfo]\n"
"add    SETNAME IP|IP/CIDR|FROM-TO [timeout VALUE]\n"
"               [packets VALUE] [bytes VALUE] [comment \"string\"]\n"
"		[skbmark VALUE] [skbprio VALUE] [skbqueue VALUE]\n"
"del    SETNAME IP|IP/CIDR|FROM-TO\n"
"test   SETNAME IP\n\n"
"where IP, FROM and TO are IPv4 addresses (or hostnames),\n"
"      CIDR is a valid IPv4 CIDR prefix.\n";

static struct ipset_type ipset_bitmap_ip3 = {
	.name = "bitmap:ip",
	.alias = { "ipmap", NULL },
	.revision = 3,
	.family = NFPROTO_IPV4,
	.dimension = IPSET_DIM_ONE,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
	},
	.args = {
		[IPSET_CREATE] = bitmap_ip_create_args3,
		[IPSET_ADD] = bitmap_ip_add_args3,
	},
	.mandatory = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_NETMASK)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS)
			| IPSET_FLAG(IPSET_OPT_CREATE_COMMENT)
			| IPSET_FLAG(IPSET_OPT_SKBINFO),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES)
			| IPSET_FLAG(IPSET_OPT_ADT_COMMENT)
			| IPSET_FLAG(IPSET_OPT_SKBMARK)
			| IPSET_FLAG(IPSET_OPT_SKBPRIO)
			| IPSET_FLAG(IPSET_OPT_SKBQUEUE),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_IP_TO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP),
	},

	.usage = bitmap_ip_usage3,
	.description = "skbinfo support",
};
void _init(void);
void _init(void)
{
	ipset_type_add(&ipset_bitmap_ip0);
	ipset_type_add(&ipset_bitmap_ip1);
	ipset_type_add(&ipset_bitmap_ip2);
	ipset_type_add(&ipset_bitmap_ip3);
}
