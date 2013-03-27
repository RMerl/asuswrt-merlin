/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2010 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  
 */

/* Shared library add-on to iptables to add IP set mangling target. */
#include <stdbool.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>

#include <xtables.h>
#include <linux/netfilter/xt_set.h>
#include "libxt_set.h"

/* Revision 0 */

static void
set_target_help_v0(void)
{
	printf("SET target options:\n"
	       " --add-set name flags\n"
	       " --del-set name flags\n"
	       "		add/del src/dst IP/port from/to named sets,\n"
	       "		where flags are the comma separated list of\n"
	       "		'src' and 'dst' specifications.\n");
}

static const struct option set_target_opts_v0[] = {
	{.name = "add-set", .has_arg = true, .val = '1'},
	{.name = "del-set", .has_arg = true, .val = '2'},
	XT_GETOPT_TABLEEND,
};

static void
set_target_check_v0(unsigned int flags)
{
	if (!flags)
		xtables_error(PARAMETER_PROBLEM,
			   "You must specify either `--add-set' or `--del-set'");
}

static void
set_target_init_v0(struct xt_entry_target *target)
{
	struct xt_set_info_target_v0 *info =
		(struct xt_set_info_target_v0 *) target->data;

	info->add_set.index =
	info->del_set.index = IPSET_INVALID_ID;

}

static void
parse_target_v0(char **argv, int invert, unsigned int *flags,
		struct xt_set_info_v0 *info, const char *what)
{
	if (info->u.flags[0])
		xtables_error(PARAMETER_PROBLEM,
			      "--%s can be specified only once", what);

	if (!argv[optind]
	    || argv[optind][0] == '-' || argv[optind][0] == '!')
		xtables_error(PARAMETER_PROBLEM,
			      "--%s requires two args.", what);

	if (strlen(optarg) > IPSET_MAXNAMELEN - 1)
		xtables_error(PARAMETER_PROBLEM,
			      "setname `%s' too long, max %d characters.",
			      optarg, IPSET_MAXNAMELEN - 1);

	get_set_byname(optarg, (struct xt_set_info *)info);
	parse_dirs_v0(argv[optind], info);
	optind++;
	
	*flags = 1;
}

static int
set_target_parse_v0(int c, char **argv, int invert, unsigned int *flags,
		    const void *entry, struct xt_entry_target **target)
{
	struct xt_set_info_target_v0 *myinfo =
		(struct xt_set_info_target_v0 *) (*target)->data;

	switch (c) {
	case '1':		/* --add-set <set> <flags> */
		parse_target_v0(argv, invert, flags,
				&myinfo->add_set, "add-set");
		break;
	case '2':		/* --del-set <set>[:<flags>] <flags> */
		parse_target_v0(argv, invert, flags,
				&myinfo->del_set, "del-set");
		break;
	}
	return 1;
}

static void
print_target_v0(const char *prefix, const struct xt_set_info_v0 *info)
{
	int i;
	char setname[IPSET_MAXNAMELEN];

	if (info->index == IPSET_INVALID_ID)
		return;
	get_set_byid(setname, info->index);
	printf(" %s %s", prefix, setname);
	for (i = 0; i < IPSET_DIM_MAX; i++) {
		if (!info->u.flags[i])
			break;		
		printf("%s%s",
		       i == 0 ? " " : ",",
		       info->u.flags[i] & IPSET_SRC ? "src" : "dst");
	}
}

static void
set_target_print_v0(const void *ip, const struct xt_entry_target *target,
                    int numeric)
{
	const struct xt_set_info_target_v0 *info = (const void *)target->data;

	print_target_v0("add-set", &info->add_set);
	print_target_v0("del-set", &info->del_set);
}

static void
set_target_save_v0(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_set_info_target_v0 *info = (const void *)target->data;

	print_target_v0("--add-set", &info->add_set);
	print_target_v0("--del-set", &info->del_set);
}

/* Revision 1 */
static void
set_target_init_v1(struct xt_entry_target *target)
{
	struct xt_set_info_target_v1 *info =
		(struct xt_set_info_target_v1 *) target->data;

	info->add_set.index =
	info->del_set.index = IPSET_INVALID_ID;

}

#define SET_TARGET_ADD		0x1
#define SET_TARGET_DEL		0x2
#define SET_TARGET_EXIST	0x4
#define SET_TARGET_TIMEOUT	0x8

static void
parse_target(char **argv, int invert, struct xt_set_info *info,
	     const char *what)
{
	if (info->dim)
		xtables_error(PARAMETER_PROBLEM,
			      "--%s can be specified only once", what);
	if (!argv[optind]
	    || argv[optind][0] == '-' || argv[optind][0] == '!')
		xtables_error(PARAMETER_PROBLEM,
			      "--%s requires two args.", what);

	if (strlen(optarg) > IPSET_MAXNAMELEN - 1)
		xtables_error(PARAMETER_PROBLEM,
			      "setname `%s' too long, max %d characters.",
			      optarg, IPSET_MAXNAMELEN - 1);

	get_set_byname(optarg, info);
	parse_dirs(argv[optind], info);
	optind++;
}

static int
set_target_parse_v1(int c, char **argv, int invert, unsigned int *flags,
		    const void *entry, struct xt_entry_target **target)
{
	struct xt_set_info_target_v1 *myinfo =
		(struct xt_set_info_target_v1 *) (*target)->data;

	switch (c) {
	case '1':		/* --add-set <set> <flags> */
		parse_target(argv, invert, &myinfo->add_set, "add-set");
		*flags |= SET_TARGET_ADD;
		break;
	case '2':		/* --del-set <set>[:<flags>] <flags> */
		parse_target(argv, invert, &myinfo->del_set, "del-set");
		*flags |= SET_TARGET_DEL;
		break;
	}
	return 1;
}

static void
print_target(const char *prefix, const struct xt_set_info *info)
{
	int i;
	char setname[IPSET_MAXNAMELEN];

	if (info->index == IPSET_INVALID_ID)
		return;
	get_set_byid(setname, info->index);
	printf(" %s %s", prefix, setname);
	for (i = 1; i <= info->dim; i++) {
		printf("%s%s",
		       i == 1 ? " " : ",",
		       info->flags & (1 << i) ? "src" : "dst");
	}
}

static void
set_target_print_v1(const void *ip, const struct xt_entry_target *target,
		    int numeric)
{
	const struct xt_set_info_target_v1 *info = (const void *)target->data;

	print_target("add-set", &info->add_set);
	print_target("del-set", &info->del_set);
}

static void
set_target_save_v1(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_set_info_target_v1 *info = (const void *)target->data;

	print_target("--add-set", &info->add_set);
	print_target("--del-set", &info->del_set);
}

/* Revision 2 */

static void
set_target_help_v2(void)
{
	printf("SET target options:\n"
	       " --add-set name flags [--exist] [--timeout n]\n"
	       " --del-set name flags\n"
	       "		add/del src/dst IP/port from/to named sets,\n"
	       "		where flags are the comma separated list of\n"
	       "		'src' and 'dst' specifications.\n");
}

static const struct option set_target_opts_v2[] = {
	{.name = "add-set", .has_arg = true,  .val = '1'},
	{.name = "del-set", .has_arg = true,  .val = '2'},
	{.name = "exist",   .has_arg = false, .val = '3'},
	{.name = "timeout", .has_arg = true,  .val = '4'},
	XT_GETOPT_TABLEEND,
};

static void
set_target_check_v2(unsigned int flags)
{
	if (!(flags & (SET_TARGET_ADD|SET_TARGET_DEL)))
		xtables_error(PARAMETER_PROBLEM,
			   "You must specify either `--add-set' or `--del-set'");
	if (!(flags & SET_TARGET_ADD)) {
		if (flags & SET_TARGET_EXIST)
			xtables_error(PARAMETER_PROBLEM,
				"Flag `--exist' can be used with `--add-set' only");
		if (flags & SET_TARGET_TIMEOUT)
			xtables_error(PARAMETER_PROBLEM,
				"Option `--timeout' can be used with `--add-set' only");
	}
}


static void
set_target_init_v2(struct xt_entry_target *target)
{
	struct xt_set_info_target_v2 *info =
		(struct xt_set_info_target_v2 *) target->data;

	info->add_set.index =
	info->del_set.index = IPSET_INVALID_ID;
	info->timeout = UINT32_MAX;
}

static int
set_target_parse_v2(int c, char **argv, int invert, unsigned int *flags,
		    const void *entry, struct xt_entry_target **target)
{
	struct xt_set_info_target_v2 *myinfo =
		(struct xt_set_info_target_v2 *) (*target)->data;
	unsigned int timeout;

	switch (c) {
	case '1':		/* --add-set <set> <flags> */
		parse_target(argv, invert, &myinfo->add_set, "add-set");
		*flags |= SET_TARGET_ADD;
		break;
	case '2':		/* --del-set <set>[:<flags>] <flags> */
		parse_target(argv, invert, &myinfo->del_set, "del-set");
		*flags |= SET_TARGET_DEL;
		break;
	case '3':
		myinfo->flags |= IPSET_FLAG_EXIST;
		*flags |= SET_TARGET_EXIST;
		break;
	case '4':
		if (!xtables_strtoui(optarg, NULL, &timeout, 0, UINT32_MAX - 1))
			xtables_error(PARAMETER_PROBLEM,
				      "Invalid value for option --timeout "
				      "or out of range 0-%u", UINT32_MAX - 1);
		myinfo->timeout = timeout;
		*flags |= SET_TARGET_TIMEOUT;
		break;	
	}
	return 1;
}

static void
set_target_print_v2(const void *ip, const struct xt_entry_target *target,
		    int numeric)
{
	const struct xt_set_info_target_v2 *info = (const void *)target->data;

	print_target("add-set", &info->add_set);
	if (info->flags & IPSET_FLAG_EXIST)
		printf(" exist");
	if (info->timeout != UINT32_MAX)
		printf(" timeout %u", info->timeout);
	print_target("del-set", &info->del_set);
}

static void
set_target_save_v2(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_set_info_target_v2 *info = (const void *)target->data;

	print_target("--add-set", &info->add_set);
	if (info->flags & IPSET_FLAG_EXIST)
		printf(" --exist");
	if (info->timeout != UINT32_MAX)
		printf(" --timeout %u", info->timeout);
	print_target("--del-set", &info->del_set);
}

static struct xtables_target set_tg_reg[] = {
	{
		.name		= "SET",
		.revision	= 0,
		.version	= XTABLES_VERSION,
		.family		= NFPROTO_IPV4,
		.size		= XT_ALIGN(sizeof(struct xt_set_info_target_v0)),
		.userspacesize	= XT_ALIGN(sizeof(struct xt_set_info_target_v0)),
		.help		= set_target_help_v0,
		.init		= set_target_init_v0,
		.parse		= set_target_parse_v0,
		.final_check	= set_target_check_v0,
		.print		= set_target_print_v0,
		.save		= set_target_save_v0,
		.extra_opts	= set_target_opts_v0,
	},
	{
		.name		= "SET",
		.revision	= 1,
		.version	= XTABLES_VERSION,
		.family		= NFPROTO_UNSPEC,
		.size		= XT_ALIGN(sizeof(struct xt_set_info_target_v1)),
		.userspacesize	= XT_ALIGN(sizeof(struct xt_set_info_target_v1)),
		.help		= set_target_help_v0,
		.init		= set_target_init_v1,
		.parse		= set_target_parse_v1,
		.final_check	= set_target_check_v0,
		.print		= set_target_print_v1,
		.save		= set_target_save_v1,
		.extra_opts	= set_target_opts_v0,
	},
	{
		.name		= "SET",
		.revision	= 2,
		.version	= XTABLES_VERSION,
		.family		= NFPROTO_UNSPEC,
		.size		= XT_ALIGN(sizeof(struct xt_set_info_target_v2)),
		.userspacesize	= XT_ALIGN(sizeof(struct xt_set_info_target_v2)),
		.help		= set_target_help_v2,
		.init		= set_target_init_v2,
		.parse		= set_target_parse_v2,
		.final_check	= set_target_check_v2,
		.print		= set_target_print_v2,
		.save		= set_target_save_v2,
		.extra_opts	= set_target_opts_v2,
	},
};

void _init(void)
{
	xtables_register_targets(set_tg_reg, ARRAY_SIZE(set_tg_reg));
}
