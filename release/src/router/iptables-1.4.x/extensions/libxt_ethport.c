/*
 * IP tables module for matching the value of the incoming ether port
 * for Ralink SoC platform.
 *
 * (C) 2009 by yy_huang@ralinktech.com.tw
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "iptables.h"
#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_ethport.h>

static void ethport_help(void)
{
	printf(
"Ethport match v%s options\n"
"[!] --portnum value		Match Ether port number with numerical value\n"
"  		                This value can be in decimal (ex: 3)\n"
"               		or in hex (ex: 0x3)\n"
				, XTABLES_VERSION
);
}

static const struct option ethport_opts[] = {
	{.name = "portnum", .has_arg = true, .val = 'F'},
	XT_GETOPT_TABLEEND,
};

static void
parse_ethport(const char *s, struct xt_ethport_info *dinfo)
{
	unsigned int portnum;
       
	if (!xtables_strtoui(s, NULL, &portnum, 0, UINT8_MAX))
		xtables_error(PARAMETER_PROBLEM,
			   "Invalid portnum `%s'\n", s);

	if (portnum > XT_ETHPORT_MAX)
		xtables_error(PARAMETER_PROBLEM,
			   "Ethport `%d` out of range\n", portnum);

    	dinfo->portnum = (u_int8_t )portnum;
    	return;
}


static int
ethport_parse(int c, char **argv, int invert, unsigned int *flags,
           const void *entry, struct xt_entry_match **match)
{
	struct xt_ethport_info *dinfo
		= (struct xt_ethport_info *)(*match)->data;

	switch (c) {
	case 'F':
		if (*flags)
			xtables_error(PARAMETER_PROBLEM,
			           "Ethport match: Only use --portnum ONCE!");
		parse_ethport(argv[optind-1], dinfo);
		if (invert)
			dinfo->invert = 1;
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

static void ethport_check(unsigned int flags)
{
	if (!flags)
		xtables_error(PARAMETER_PROBLEM,
		           "Ethport match: Parameter --portnum is required");
}

static void
print_ethport(u_int8_t portnum, int invert, int numeric)
{
	if (invert)
		fputc('!', stdout);

 	printf("0x%02x ", portnum);
}

/* Prints out the matchinfo. */
static void
ethport_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_ethport_info *dinfo = (const struct xt_ethport_info *)match->data;
	printf("Ethport match ");
	print_ethport(dinfo->portnum, dinfo->invert, numeric);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void ethport_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_ethport_info *dinfo = (const struct xt_ethport_info *)match->data;

	printf("--portnum ");
	print_ethport(dinfo->portnum, dinfo->invert, 1);
}

static struct xtables_match ethport_match = {
	.family		= NFPROTO_IPV4,
	.name 		= "ethport",
	.version 	= XTABLES_VERSION,
	.size 		= XT_ALIGN(sizeof(struct xt_ethport_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_ethport_info)),
	.help		= ethport_help,
	.parse		= ethport_parse,
	.final_check	= ethport_check,
	.print		= ethport_print,
	.save		= ethport_save,
	.extra_opts	= ethport_opts,
};

static struct xtables_match ethport_match6 = {
	.family		= NFPROTO_UNSPEC,
	.name 		= "ethport",
	.version 	= XTABLES_VERSION,
	.size 		= XT_ALIGN(sizeof(struct xt_ethport_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_ethport_info)),
	.help		= ethport_help,
	.parse		= ethport_parse,
	.final_check	= ethport_check,
	.print		= ethport_print,
	.save		= ethport_save,
	.extra_opts	= ethport_opts,
};

void _init(void)
{
	xtables_register_match(&ethport_match);
	xtables_register_match(&ethport_match6);
}
