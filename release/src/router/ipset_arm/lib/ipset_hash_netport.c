/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <libipset/data.h>			/* IPSET_OPT_* */
#include <libipset/parse.h>			/* parser functions */
#include <libipset/print.h>			/* printing functions */
#include <libipset/ui.h>			/* ipset_port_usage */
#include <libipset/types.h>			/* prototypes */

/* Parse commandline arguments */
static const struct ipset_arg hash_netport_create_args1[] = {
	{ .name = { "family", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,		.print = ipset_print_family,
	},
	/* Alias: family inet */
	{ .name = { "-4", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	/* Alias: family inet6 */
	{ .name = { "-6", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	{ .name = { "hashsize", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_HASHSIZE,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
	},
	{ .name = { "maxelem", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_MAXELEM,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
	},
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ },
};

static const struct ipset_arg hash_netport_add_args1[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ },
};

static const char hash_netport_usage1[] =
"create SETNAME hash:net,port\n"
"		[family inet|inet6]\n"
"               [hashsize VALUE] [maxelem VALUE]\n"
"               [timeout VALUE]\n"
"add    SETNAME IP[/CIDR],PROTO:PORT [timeout VALUE]\n"
"del    SETNAME IP[/CIDR],PROTO:PORT\n"
"test   SETNAME IP[/CIDR],PROTO:PORT\n\n"
"where depending on the INET family\n"
"      IP is a valid IPv4 or IPv6 address (or hostname),\n"
"      CIDR is a valid IPv4 or IPv6 CIDR prefix.\n"
"      Adding/deleting multiple elements with TCP/SCTP/UDP/UDPLITE\n"
"      port range is supported both for IPv4 and IPv6.\n";

static struct ipset_type ipset_hash_netport1 = {
	.name = "hash:net,port",
	.alias = { "netporthash", NULL },
	.revision = 1,
	.family = NFPROTO_IPSET_IPV46,
	.dimension = IPSET_DIM_TWO,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ipnet,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
		[IPSET_DIM_TWO - 1] = {
			.parse = ipset_parse_proto_port,
			.print = ipset_print_proto_port,
			.opt = IPSET_OPT_PORT
		},
	},
	.args = {
		[IPSET_CREATE] = hash_netport_create_args1,
		[IPSET_ADD] = hash_netport_add_args1,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_HASHSIZE)
			| IPSET_FLAG(IPSET_OPT_MAXELEM)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_CIDR),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_CIDR),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_CIDR),
	},

	.usage = hash_netport_usage1,
	.usagefn = ipset_port_usage,
	.description = "SCTP and UDPLITE support",
};

static const char hash_netport_usage2[] =
"create SETNAME hash:net,port\n"
"		[family inet|inet6]\n"
"               [hashsize VALUE] [maxelem VALUE]\n"
"               [timeout VALUE]\n"
"add    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT [timeout VALUE]\n"
"del    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT\n"
"test   SETNAME IP[/CIDR],PROTO:PORT\n\n"
"where depending on the INET family\n"
"      IP is a valid IPv4 or IPv6 address (or hostname),\n"
"      CIDR is a valid IPv4 or IPv6 CIDR prefix.\n"
"      Adding/deleting multiple elements with IPv4 is supported.\n"
"      Adding/deleting multiple elements with TCP/SCTP/UDP/UDPLITE\n"
"      port range is supported both for IPv4 and IPv6.\n";

static struct ipset_type ipset_hash_netport2 = {
	.name = "hash:net,port",
	.alias = { "netporthash", NULL },
	.revision = 2,
	.family = NFPROTO_IPSET_IPV46,
	.dimension = IPSET_DIM_TWO,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip4_net6,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
		[IPSET_DIM_TWO - 1] = {
			.parse = ipset_parse_proto_port,
			.print = ipset_print_proto_port,
			.opt = IPSET_OPT_PORT
		},
	},
	.args = {
		[IPSET_CREATE] = hash_netport_create_args1,
		[IPSET_ADD] = hash_netport_add_args1,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_HASHSIZE)
			| IPSET_FLAG(IPSET_OPT_MAXELEM)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PROTO),
	},

	.usage = hash_netport_usage2,
	.usagefn = ipset_port_usage,
	.description = "Add/del range support",
};

static const struct ipset_arg hash_netport_add_args3[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "nomatch", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_NOMATCH,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ },
};

static const char hash_netport_usage3[] =
"create SETNAME hash:net,port\n"
"		[family inet|inet6]\n"
"               [hashsize VALUE] [maxelem VALUE]\n"
"               [timeout VALUE]\n"
"add    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT [timeout VALUE] [nomatch]\n"
"del    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT\n"
"test   SETNAME IP[/CIDR],PROTO:PORT\n\n"
"where depending on the INET family\n"
"      IP is a valid IPv4 or IPv6 address (or hostname),\n"
"      CIDR is a valid IPv4 or IPv6 CIDR prefix.\n"
"      Adding/deleting multiple elements with IPv4 is supported.\n"
"      Adding/deleting multiple elements with TCP/SCTP/UDP/UDPLITE\n"
"      port range is supported both for IPv4 and IPv6.\n";

static struct ipset_type ipset_hash_netport3 = {
	.name = "hash:net,port",
	.alias = { "netporthash", NULL },
	.revision = 3,
	.family = NFPROTO_IPSET_IPV46,
	.dimension = IPSET_DIM_TWO,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip4_net6,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
		[IPSET_DIM_TWO - 1] = {
			.parse = ipset_parse_proto_port,
			.print = ipset_print_proto_port,
			.opt = IPSET_OPT_PORT
		},
	},
	.args = {
		[IPSET_CREATE] = hash_netport_create_args1,
		[IPSET_ADD] = hash_netport_add_args3,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_HASHSIZE)
			| IPSET_FLAG(IPSET_OPT_MAXELEM)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_NOMATCH),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PROTO),
	},

	.usage = hash_netport_usage3,
	.usagefn = ipset_port_usage,
	.description = "nomatch flag support",
};

/* Parse commandline arguments */
static const struct ipset_arg hash_netport_create_args4[] = {
	{ .name = { "family", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,		.print = ipset_print_family,
	},
	/* Alias: family inet */
	{ .name = { "-4", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	/* Alias: family inet6 */
	{ .name = { "-6", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	{ .name = { "hashsize", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_HASHSIZE,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
	},
	{ .name = { "maxelem", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_MAXELEM,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
	},
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "counters", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_COUNTERS,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ },
};

static const struct ipset_arg hash_netport_add_args4[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "nomatch", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_NOMATCH,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
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

static const struct ipset_arg hash_netport_test_args4[] = {
	{ .name = { "nomatch", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_NOMATCH,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ },
};

static const char hash_netport_usage4[] =
"create SETNAME hash:net,port\n"
"		[family inet|inet6]\n"
"               [hashsize VALUE] [maxelem VALUE]\n"
"               [timeout VALUE] [counters]\n"
"add    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT [timeout VALUE] [nomatch]\n"
"               [packets VALUE] [bytes VALUE]\n"
"del    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT\n"
"test   SETNAME IP[/CIDR],PROTO:PORT\n\n"
"where depending on the INET family\n"
"      IP is a valid IPv4 or IPv6 address (or hostname),\n"
"      CIDR is a valid IPv4 or IPv6 CIDR prefix.\n"
"      Adding/deleting multiple elements with IPv4 is supported.\n"
"      Adding/deleting multiple elements with TCP/SCTP/UDP/UDPLITE\n"
"      port range is supported both for IPv4 and IPv6.\n";

static struct ipset_type ipset_hash_netport4 = {
	.name = "hash:net,port",
	.alias = { "netporthash", NULL },
	.revision = 4,
	.family = NFPROTO_IPSET_IPV46,
	.dimension = IPSET_DIM_TWO,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip4_net6,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
		[IPSET_DIM_TWO - 1] = {
			.parse = ipset_parse_proto_port,
			.print = ipset_print_proto_port,
			.opt = IPSET_OPT_PORT
		},
	},
	.args = {
		[IPSET_CREATE] = hash_netport_create_args4,
		[IPSET_ADD] = hash_netport_add_args4,
		[IPSET_TEST] = hash_netport_test_args4,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_HASHSIZE)
			| IPSET_FLAG(IPSET_OPT_MAXELEM)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_NOMATCH)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_NOMATCH),
	},

	.usage = hash_netport_usage4,
	.usagefn = ipset_port_usage,
	.description = "counters support",
};

/* Parse commandline arguments */
static const struct ipset_arg hash_netport_create_args5[] = {
	{ .name = { "family", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,		.print = ipset_print_family,
	},
	/* Alias: family inet */
	{ .name = { "-4", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	/* Alias: family inet6 */
	{ .name = { "-6", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	{ .name = { "hashsize", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_HASHSIZE,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
	},
	{ .name = { "maxelem", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_MAXELEM,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
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
	{ },
};

static const struct ipset_arg hash_netport_add_args5[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "nomatch", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_NOMATCH,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
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

static const struct ipset_arg hash_netport_test_args5[] = {
	{ .name = { "nomatch", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_NOMATCH,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ },
};

static const char hash_netport_usage5[] =
"create SETNAME hash:net,port\n"
"		[family inet|inet6]\n"
"               [hashsize VALUE] [maxelem VALUE]\n"
"               [timeout VALUE] [counters] [comment]\n"
"add    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT [timeout VALUE] [nomatch]\n"
"               [packets VALUE] [bytes VALUE] [comment \"string\"]\n"
"del    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT\n"
"test   SETNAME IP[/CIDR],PROTO:PORT\n\n"
"where depending on the INET family\n"
"      IP is a valid IPv4 or IPv6 address (or hostname),\n"
"      CIDR is a valid IPv4 or IPv6 CIDR prefix.\n"
"      Adding/deleting multiple elements with IPv4 is supported.\n"
"      Adding/deleting multiple elements with TCP/SCTP/UDP/UDPLITE\n"
"      port range is supported both for IPv4 and IPv6.\n";

static struct ipset_type ipset_hash_netport5 = {
	.name = "hash:net,port",
	.alias = { "netporthash", NULL },
	.revision = 5,
	.family = NFPROTO_IPSET_IPV46,
	.dimension = IPSET_DIM_TWO,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip4_net6,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
		[IPSET_DIM_TWO - 1] = {
			.parse = ipset_parse_proto_port,
			.print = ipset_print_proto_port,
			.opt = IPSET_OPT_PORT
		},
	},
	.args = {
		[IPSET_CREATE] = hash_netport_create_args5,
		[IPSET_ADD] = hash_netport_add_args5,
		[IPSET_TEST] = hash_netport_test_args5,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_HASHSIZE)
			| IPSET_FLAG(IPSET_OPT_MAXELEM)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS)
			| IPSET_FLAG(IPSET_OPT_CREATE_COMMENT),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_NOMATCH)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES)
			| IPSET_FLAG(IPSET_OPT_ADT_COMMENT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_NOMATCH),
	},

	.usage = hash_netport_usage5,
	.usagefn = ipset_port_usage,
	.description = "comment support",
};

/* Parse commandline arguments */
static const struct ipset_arg hash_netport_create_args6[] = {
	{ .name = { "family", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,		.print = ipset_print_family,
	},
	/* Alias: family inet */
	{ .name = { "-4", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	/* Alias: family inet6 */
	{ .name = { "-6", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	{ .name = { "hashsize", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_HASHSIZE,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
	},
	{ .name = { "maxelem", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_MAXELEM,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
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
	{ .name = { "forceadd", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FORCEADD,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ },
};

static const char hash_netport_usage6[] =
"create SETNAME hash:net,port\n"
"		[family inet|inet6]\n"
"               [hashsize VALUE] [maxelem VALUE]\n"
"               [timeout VALUE] [counters] [comment]\n"
"		[forceadd]\n"
"add    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT [timeout VALUE] [nomatch]\n"
"               [packets VALUE] [bytes VALUE] [comment \"string\"]\n"
"del    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT\n"
"test   SETNAME IP[/CIDR],PROTO:PORT\n\n"
"where depending on the INET family\n"
"      IP is a valid IPv4 or IPv6 address (or hostname),\n"
"      CIDR is a valid IPv4 or IPv6 CIDR prefix.\n"
"      Adding/deleting multiple elements with IPv4 is supported.\n"
"      Adding/deleting multiple elements with TCP/SCTP/UDP/UDPLITE\n"
"      port range is supported both for IPv4 and IPv6.\n";

static struct ipset_type ipset_hash_netport6 = {
	.name = "hash:net,port",
	.alias = { "netporthash", NULL },
	.revision = 6,
	.family = NFPROTO_IPSET_IPV46,
	.dimension = IPSET_DIM_TWO,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip4_net6,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
		[IPSET_DIM_TWO - 1] = {
			.parse = ipset_parse_proto_port,
			.print = ipset_print_proto_port,
			.opt = IPSET_OPT_PORT
		},
	},
	.args = {
		[IPSET_CREATE] = hash_netport_create_args6,
		[IPSET_ADD] = hash_netport_add_args5,
		[IPSET_TEST] = hash_netport_test_args5,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_HASHSIZE)
			| IPSET_FLAG(IPSET_OPT_MAXELEM)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS)
			| IPSET_FLAG(IPSET_OPT_CREATE_COMMENT)
			| IPSET_FLAG(IPSET_OPT_FORCEADD),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_NOMATCH)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES)
			| IPSET_FLAG(IPSET_OPT_ADT_COMMENT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_NOMATCH),
	},

	.usage = hash_netport_usage6,
	.usagefn = ipset_port_usage,
	.description = "forceadd support",
};

/* Parse commandline arguments */
static const struct ipset_arg hash_netport_create_args7[] = {
	{ .name = { "family", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,		.print = ipset_print_family,
	},
	/* Alias: family inet */
	{ .name = { "-4", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	/* Alias: family inet6 */
	{ .name = { "-6", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FAMILY,
	  .parse = ipset_parse_family,
	},
	{ .name = { "hashsize", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_HASHSIZE,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
	},
	{ .name = { "maxelem", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_MAXELEM,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
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
	{ .name = { "forceadd", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_FORCEADD,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ .name = { "skbinfo", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_SKBINFO,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ },
};

static const struct ipset_arg hash_netport_add_args7[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "nomatch", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_NOMATCH,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
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
	  .has_arg = IPSET_MANDATORY_ARG,       .opt = IPSET_OPT_SKBMARK,
	  .parse = ipset_parse_skbmark,         .print = ipset_print_skbmark,
	},
	{ .name = { "skbprio", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,       .opt = IPSET_OPT_SKBPRIO,
	  .parse = ipset_parse_skbprio,         .print = ipset_print_skbprio,
	},
	{ .name = { "skbqueue", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,       .opt = IPSET_OPT_SKBQUEUE,
	  .parse = ipset_parse_uint16,          .print = ipset_print_number,
	},
	{ },
};

static const char hash_netport_usage7[] =
"create SETNAME hash:net,port\n"
"		[family inet|inet6]\n"
"               [hashsize VALUE] [maxelem VALUE]\n"
"               [timeout VALUE] [counters] [comment]\n"
"		[forceadd] [skbinfo]\n"
"add    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT [timeout VALUE] [nomatch]\n"
"               [packets VALUE] [bytes VALUE] [comment \"string\"]\n"
"		[skbmark VALUE] [skbprio VALUE] [skbqueue VALUE]\n"
"del    SETNAME IP[/CIDR]|FROM-TO,PROTO:PORT\n"
"test   SETNAME IP[/CIDR],PROTO:PORT\n\n"
"where depending on the INET family\n"
"      IP is a valid IPv4 or IPv6 address (or hostname),\n"
"      CIDR is a valid IPv4 or IPv6 CIDR prefix.\n"
"      Adding/deleting multiple elements with IPv4 is supported.\n"
"      Adding/deleting multiple elements with TCP/SCTP/UDP/UDPLITE\n"
"      port range is supported both for IPv4 and IPv6.\n";

static struct ipset_type ipset_hash_netport7 = {
	.name = "hash:net,port",
	.alias = { "netporthash", NULL },
	.revision = 7,
	.family = NFPROTO_IPSET_IPV46,
	.dimension = IPSET_DIM_TWO,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_ip4_net6,
			.print = ipset_print_ip,
			.opt = IPSET_OPT_IP
		},
		[IPSET_DIM_TWO - 1] = {
			.parse = ipset_parse_proto_port,
			.print = ipset_print_proto_port,
			.opt = IPSET_OPT_PORT
		},
	},
	.args = {
		[IPSET_CREATE] = hash_netport_create_args7,
		[IPSET_ADD] = hash_netport_add_args7,
		[IPSET_TEST] = hash_netport_test_args5,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_PORT),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_HASHSIZE)
			| IPSET_FLAG(IPSET_OPT_MAXELEM)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS)
			| IPSET_FLAG(IPSET_OPT_CREATE_COMMENT)
			| IPSET_FLAG(IPSET_OPT_FORCEADD)
			| IPSET_FLAG(IPSET_OPT_SKBINFO),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_NOMATCH)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES)
			| IPSET_FLAG(IPSET_OPT_ADT_COMMENT)
			| IPSET_FLAG(IPSET_OPT_SKBMARK)
			| IPSET_FLAG(IPSET_OPT_SKBPRIO)
			| IPSET_FLAG(IPSET_OPT_SKBQUEUE),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_IP_TO)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PORT_TO)
			| IPSET_FLAG(IPSET_OPT_PROTO),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_IP)
			| IPSET_FLAG(IPSET_OPT_CIDR)
			| IPSET_FLAG(IPSET_OPT_PORT)
			| IPSET_FLAG(IPSET_OPT_PROTO)
			| IPSET_FLAG(IPSET_OPT_NOMATCH),
	},

	.usage = hash_netport_usage7,
	.usagefn = ipset_port_usage,
	.description = "skbinfo support",
};

void _init(void);
void _init(void)
{
	ipset_type_add(&ipset_hash_netport1);
	ipset_type_add(&ipset_hash_netport2);
	ipset_type_add(&ipset_hash_netport3);
	ipset_type_add(&ipset_hash_netport4);
	ipset_type_add(&ipset_hash_netport5);
	ipset_type_add(&ipset_hash_netport6);
	ipset_type_add(&ipset_hash_netport7);
}
