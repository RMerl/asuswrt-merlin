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
static const struct ipset_arg list_set_create_args0[] = {
	{ .name = { "size", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_SIZE,
	  .parse = ipset_parse_uint32,		.print = ipset_print_number,
	},
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ },
};

static const struct ipset_arg list_set_adt_args0[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "before", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NAMEREF,
	  .parse = ipset_parse_before,
	},
	{ .name = { "after", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NAMEREF,
	  .parse = ipset_parse_after,
	},
	{ },
};

static const char list_set_usage0[] =
"create SETNAME list:set\n"
"               [size VALUE] [timeout VALUE]\n"
"add    SETNAME NAME [before|after NAME] [timeout VALUE]\n"
"del    SETNAME NAME [before|after NAME]\n"
"test   SETNAME NAME [before|after NAME]\n\n"
"where NAME are existing set names.\n";

static struct ipset_type ipset_list_set0 = {
	.name = "list:set",
	.alias = { "setlist", NULL },
	.revision = 0,
	.family = NFPROTO_UNSPEC,
	.dimension = IPSET_DIM_ONE,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_setname,
			.print = ipset_print_name,
			.opt = IPSET_OPT_NAME
		},
	},
	.compat_parse_elem = ipset_parse_name_compat,
	.args = {
		[IPSET_CREATE] = list_set_create_args0,
		[IPSET_ADD] = list_set_adt_args0,
		[IPSET_DEL] = list_set_adt_args0,
		[IPSET_TEST] = list_set_adt_args0,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_NAME),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_NAME),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_NAME),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_SIZE)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF),
	},

	.usage = list_set_usage0,
	.description = "Initial revision",
};

/* Parse commandline arguments */
static const struct ipset_arg list_set_create_args1[] = {
	{ .name = { "size", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_SIZE,
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

static const struct ipset_arg list_set_adt_args1[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "before", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NAMEREF,
	  .parse = ipset_parse_before,
	},
	{ .name = { "after", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NAMEREF,
	  .parse = ipset_parse_after,
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

static const char list_set_usage1[] =
"create SETNAME list:set\n"
"               [size VALUE] [timeout VALUE] [counters\n"
"add    SETNAME NAME [before|after NAME] [timeout VALUE]\n"
"               [packets VALUE] [bytes VALUE]\n"
"del    SETNAME NAME [before|after NAME]\n"
"test   SETNAME NAME [before|after NAME]\n\n"
"where NAME are existing set names.\n";

static struct ipset_type ipset_list_set1 = {
	.name = "list:set",
	.alias = { "setlist", NULL },
	.revision = 1,
	.family = NFPROTO_UNSPEC,
	.dimension = IPSET_DIM_ONE,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_setname,
			.print = ipset_print_name,
			.opt = IPSET_OPT_NAME
		},
	},
	.compat_parse_elem = ipset_parse_name_compat,
	.args = {
		[IPSET_CREATE] = list_set_create_args1,
		[IPSET_ADD] = list_set_adt_args1,
		[IPSET_DEL] = list_set_adt_args1,
		[IPSET_TEST] = list_set_adt_args1,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_NAME),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_NAME),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_NAME),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_SIZE)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF),
	},

	.usage = list_set_usage1,
	.description = "counters support",
};

/* Parse commandline arguments */
static const struct ipset_arg list_set_create_args2[] = {
	{ .name = { "size", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_SIZE,
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

static const struct ipset_arg list_set_adt_args2[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "before", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NAMEREF,
	  .parse = ipset_parse_before,
	},
	{ .name = { "after", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NAMEREF,
	  .parse = ipset_parse_after,
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

static const char list_set_usage2[] =
"create SETNAME list:set\n"
"               [size VALUE] [timeout VALUE] [counters] [comment]\n"
"add    SETNAME NAME [before|after NAME] [timeout VALUE]\n"
"               [packets VALUE] [bytes VALUE] [comment STRING]\n"
"del    SETNAME NAME [before|after NAME]\n"
"test   SETNAME NAME [before|after NAME]\n\n"
"where NAME are existing set names.\n";

static struct ipset_type ipset_list_set2 = {
	.name = "list:set",
	.alias = { "setlist", NULL },
	.revision = 2,
	.family = NFPROTO_UNSPEC,
	.dimension = IPSET_DIM_ONE,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_setname,
			.print = ipset_print_name,
			.opt = IPSET_OPT_NAME
		},
	},
	.compat_parse_elem = ipset_parse_name_compat,
	.args = {
		[IPSET_CREATE] = list_set_create_args2,
		[IPSET_ADD] = list_set_adt_args2,
		[IPSET_DEL] = list_set_adt_args2,
		[IPSET_TEST] = list_set_adt_args2,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_NAME),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_NAME),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_NAME),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_SIZE)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS)
			| IPSET_FLAG(IPSET_OPT_CREATE_COMMENT),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES)
			| IPSET_FLAG(IPSET_OPT_ADT_COMMENT),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF),
	},

	.usage = list_set_usage2,
	.description = "comment support",
};

/* Parse commandline arguments */
static const struct ipset_arg list_set_create_args3[] = {
	{ .name = { "size", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_SIZE,
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
	{ .name = { "skbinfo", NULL },
	  .has_arg = IPSET_NO_ARG,		.opt = IPSET_OPT_SKBINFO,
	  .parse = ipset_parse_flag,		.print = ipset_print_flag,
	},
	{ },
};

static const struct ipset_arg list_set_adt_args3[] = {
	{ .name = { "timeout", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_TIMEOUT,
	  .parse = ipset_parse_timeout,		.print = ipset_print_number,
	},
	{ .name = { "before", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NAMEREF,
	  .parse = ipset_parse_before,
	},
	{ .name = { "after", NULL },
	  .has_arg = IPSET_MANDATORY_ARG,	.opt = IPSET_OPT_NAMEREF,
	  .parse = ipset_parse_after,
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

static const char list_set_usage3[] =
"create SETNAME list:set\n"
"               [size VALUE] [timeout VALUE] [counters] [comment]\n"
"		[skbinfo]\n"
"add    SETNAME NAME [before|after NAME] [timeout VALUE]\n"
"               [packets VALUE] [bytes VALUE] [comment STRING]\n"
"		[skbmark VALUE] [skbprio VALUE] [skbqueue VALUE]\n"
"del    SETNAME NAME [before|after NAME]\n"
"test   SETNAME NAME [before|after NAME]\n\n"
"where NAME are existing set names.\n";

static struct ipset_type ipset_list_set3 = {
	.name = "list:set",
	.alias = { "setlist", NULL },
	.revision = 3,
	.family = NFPROTO_UNSPEC,
	.dimension = IPSET_DIM_ONE,
	.elem = {
		[IPSET_DIM_ONE - 1] = {
			.parse = ipset_parse_setname,
			.print = ipset_print_name,
			.opt = IPSET_OPT_NAME
		},
	},
	.compat_parse_elem = ipset_parse_name_compat,
	.args = {
		[IPSET_CREATE] = list_set_create_args3,
		[IPSET_ADD] = list_set_adt_args3,
		[IPSET_DEL] = list_set_adt_args2,
		[IPSET_TEST] = list_set_adt_args2,
	},
	.mandatory = {
		[IPSET_CREATE] = 0,
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_NAME),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_NAME),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_NAME),
	},
	.full = {
		[IPSET_CREATE] = IPSET_FLAG(IPSET_OPT_SIZE)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_COUNTERS)
			| IPSET_FLAG(IPSET_OPT_CREATE_COMMENT)
			| IPSET_FLAG(IPSET_OPT_SKBINFO),
		[IPSET_ADD] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF)
			| IPSET_FLAG(IPSET_OPT_TIMEOUT)
			| IPSET_FLAG(IPSET_OPT_PACKETS)
			| IPSET_FLAG(IPSET_OPT_BYTES)
			| IPSET_FLAG(IPSET_OPT_ADT_COMMENT)
			| IPSET_FLAG(IPSET_OPT_SKBMARK)
			| IPSET_FLAG(IPSET_OPT_SKBPRIO)
			| IPSET_FLAG(IPSET_OPT_SKBQUEUE),
		[IPSET_DEL] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF),
		[IPSET_TEST] = IPSET_FLAG(IPSET_OPT_NAME)
			| IPSET_FLAG(IPSET_OPT_BEFORE)
			| IPSET_FLAG(IPSET_OPT_NAMEREF),
	},

	.usage = list_set_usage3,
	.description = "skbinfo support",
};
void _init(void);
void _init(void)
{
	ipset_type_add(&ipset_list_set0);
	ipset_type_add(&ipset_list_set1);
	ipset_type_add(&ipset_list_set2);
	ipset_type_add(&ipset_list_set3);
}
