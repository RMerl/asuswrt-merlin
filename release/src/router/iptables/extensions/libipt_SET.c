/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2004 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.  
 */

/* Shared library add-on to iptables to add IP set matching. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <errno.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_set.h>
#include "libipt_set.h"

/* Function which prints out usage message. */
static void help(void)
{
	printf("set v%s options:\n"
	       " [!] --set     name flags\n"
	       "		'name' is the set name from to match,\n" 
	       "		'flags' are the comma separated list of\n"
	       "		'src' and 'dst'.\n"
	       "\n", IPTABLES_VERSION);
}

static struct option opts[] = {
	{"set", 1, 0, '1'},
	{0}
};

/* Initialize the match. */
static void init(struct ipt_entry_match *match, unsigned int *nfcache)
{
	struct ipt_set_info_match *info = 
		(struct ipt_set_info_match *) match->data;
	

	memset(info, 0, sizeof(struct ipt_set_info_match));

}

/* Function which parses command options; returns true if it ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache, struct ipt_entry_match **match)
{
	struct ipt_set_info_match *myinfo = 
		(struct ipt_set_info_match *) (*match)->data;
	struct ipt_set_info *info = &myinfo->match_set;

	switch (c) {
	case '1':		/* --set <set> <flag>[,<flag> */
		if (info->flags[0])
			exit_error(PARAMETER_PROBLEM,
				   "--set can be specified only once");

		check_inverse(optarg, &invert, &optind, 0);
		if (invert)
			info->flags[0] |= IPSET_MATCH_INV;

		if (!argv[optind]
		    || argv[optind][0] == '-'
		    || argv[optind][0] == '!')
			exit_error(PARAMETER_PROBLEM,
				   "--set requires two args.");

		if (strlen(argv[optind-1]) > IP_SET_MAXNAMELEN - 1)
			exit_error(PARAMETER_PROBLEM,
				   "setname `%s' too long, max %d characters.",
				   argv[optind-1], IP_SET_MAXNAMELEN - 1);

		get_set_byname(argv[optind - 1], info);
		parse_bindings(argv[optind], info);
		DEBUGP("parse: set index %u\n", info->index);
		optind++;
		
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

/* Final check; must have specified --set. */
static void final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "You must specify `--set' with proper arguments");
	DEBUGP("final check OK\n");
}

static void
print_match(const char *prefix, const struct ipt_set_info *info)
{
	int i;
	char setname[IP_SET_MAXNAMELEN];

	get_set_byid(setname, info->index);
	printf("%s%s %s", 
	       (info->flags[0] & IPSET_MATCH_INV) ? "! " : "",
	       prefix,
	       setname); 
	for (i = 0; i < IP_SET_MAX_BINDINGS; i++) {
		if (!info->flags[i])
			break;		
		printf("%s%s",
		       i == 0 ? " " : ",",
		       info->flags[i] & IPSET_SRC ? "src" : "dst");
	}
	printf(" ");
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match, int numeric)
{
	struct ipt_set_info_match *info = 
		(struct ipt_set_info_match *) match->data;

	print_match("set", &info->match_set);
}

/* Saves the matchinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip,
		 const struct ipt_entry_match *match)
{
	struct ipt_set_info_match *info = 
		(struct ipt_set_info_match *) match->data;

	print_match("--set", &info->match_set);
}

static
struct iptables_match set = {
	.name		= "set",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_set_info_match)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_set_info_match)),
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
	register_match(&set);
}
