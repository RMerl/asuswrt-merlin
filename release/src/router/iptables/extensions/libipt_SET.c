/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2004 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  
 */

/* Shared library add-on to iptables to add IP set mangling target. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ipt_set.h>
#include "libipt_set.h"

/* Function which prints out usage message. */
static void help(void)
{
	printf("SET v%s options:\n"
	       " --add-set name flags\n"
	       " --del-set name flags\n"
	       "		add/del src/dst IP/port from/to named sets,\n"
	       "		where flags are the comma separated list of\n"
	       "		'src' and 'dst'.\n"
	       "\n", IPTABLES_VERSION);
}

static struct option opts[] = {
	{"add-set",   1, 0, '1'},
	{"del-set",   1, 0, '2'},
	{0}
};

/* Initialize the target. */
static void init(struct ipt_entry_target *target, unsigned int *nfcache)
{
	struct ipt_set_info_target *info =
	    (struct ipt_set_info_target *) target->data;

	memset(info, 0, sizeof(struct ipt_set_info_target));
	info->add_set.index =
	info->del_set.index = IP_SET_INVALID_ID;

}

static void
parse_target(char **argv, int invert, unsigned int *flags,
             struct ipt_set_info *info, const char *what)
{
	if (info->flags[0])
		exit_error(PARAMETER_PROBLEM,
			   "--%s can be specified only once", what);

	if (check_inverse(optarg, &invert, NULL, 0))
		exit_error(PARAMETER_PROBLEM,
			   "Unexpected `!' after --%s", what);

	if (!argv[optind]
	    || argv[optind][0] == '-' || argv[optind][0] == '!')
		exit_error(PARAMETER_PROBLEM,
			   "--%s requires two args.", what);

	if (strlen(argv[optind-1]) > IP_SET_MAXNAMELEN - 1)
		exit_error(PARAMETER_PROBLEM,
			   "setname `%s' too long, max %d characters.",
			   argv[optind-1], IP_SET_MAXNAMELEN - 1);

	get_set_byname(argv[optind - 1], info);
	parse_bindings(argv[optind], info);
	optind++;
	
	*flags = 1;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry, struct ipt_entry_target **target)
{
	struct ipt_set_info_target *myinfo =
	    (struct ipt_set_info_target *) (*target)->data;

	switch (c) {
	case '1':		/* --add-set <set> <flags> */
		parse_target(argv, invert, flags,
			     &myinfo->add_set, "add-set");
		break;
	case '2':		/* --del-set <set>[:<flags>] <flags> */
		parse_target(argv, invert, flags,
			     &myinfo->del_set, "del-set");
		break;

	default:
		return 0;
	}
	return 1;
}

/* Final check; must specify at least one. */
static void final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "You must specify either `--add-set' or `--del-set'");
}

static void
print_target(const char *prefix, const struct ipt_set_info *info)
{
	int i;
	char setname[IP_SET_MAXNAMELEN];

	if (info->index == IP_SET_INVALID_ID)
		return;
	get_set_byid(setname, info->index);
	printf("%s %s", prefix, setname);
	for (i = 0; i < IP_SET_MAX_BINDINGS; i++) {
		if (!info->flags[i])
			break;		
		printf("%s%s",
		       i == 0 ? " " : ",",
		       info->flags[i] & IPSET_SRC ? "src" : "dst");
	}
	printf(" ");
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target, int numeric)
{
	struct ipt_set_info_target *info =
	    (struct ipt_set_info_target *) target->data;

	print_target("add-set", &info->add_set);
	print_target("del-set", &info->del_set);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	struct ipt_set_info_target *info =
	    (struct ipt_set_info_target *) target->data;

	print_target("--add-set", &info->add_set);
	print_target("--del-set", &info->del_set);
}

static
struct iptables_target ipt_set_target 
= {
	.name		= "SET",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_set_info_target)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_set_info_target)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_target(&ipt_set_target);
}
