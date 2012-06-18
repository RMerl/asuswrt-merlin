/* 
   Shared library add-on to iptables to add layer 7 matching support. 

   http://l7-filter.sf.net
  
   By Matthew Strait <quadong@users.sf.net>, Dec 2003.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
   http://www.gnu.org/licenses/gpl.txt
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <dirent.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_childlevel.h>

/* Function which prints out usage message. */
static void help(void)
{
	printf(
	"CHILDLEVEL match v%s options:\n"
	"--level <n>  : Match childlevel n (0 == master)\n",
	IPTABLES_VERSION);
	fputc('\n', stdout);
}

static struct option opts[] = {
	{ .name = "level", .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = 0 }
};

/* Function which parses command options; returns true if it ate an option */
static int parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry, unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_childlevel_info *childlevelinfo = 
		(struct ipt_childlevel_info *)(*match)->data;

	switch (c) {
	case '1':
		check_inverse(optarg, &invert, &optind, 0);
		childlevelinfo->childlevel = atoi(argv[optind-1]);
		if (invert)
			childlevelinfo->invert = 1;
		*flags = 1;
		break;
	default:
		return 0;
	}

	return 1;
}

/* Final check; must have specified --level. */
static void final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "CHILDLEVEL match: You must specify `--level'");
}

static void print_protocol(int n, int invert, int numeric)
{
	fputs("childlevel ", stdout);
	if (invert) fputc('!', stdout);
	printf("%d ", n);
}

/* Prints out the matchinfo. */
static void print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	printf("CHILDLEVEL ");

	print_protocol(((struct ipt_childlevel_info *)match->data)->childlevel,
		  ((struct ipt_childlevel_info *)match->data)->invert, numeric);
}
/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
        const struct ipt_childlevel_info *info =
            (const struct ipt_childlevel_info*) match->data;

        printf("--childlevel %s%d ", (info->invert) ? "! ": "", info->childlevel);
}

static struct iptables_match childlevel = { 
	.name		= "childlevel",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_childlevel_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_childlevel_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&childlevel);
}
