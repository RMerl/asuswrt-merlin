/* Shared library add-on to iptables for DSCP
 *
 * (C) 2002 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libipt_dscp.c borrowed heavily from libipt_tos.c
 *
 * --class support added by Iain Barnes
 * 
 * For a list of DSCP codepoints see 
 * http://www.iana.org/assignments/dscp-registry
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_dscp.h>

/* This is evil, but it's my code - HW*/
#include "libipt_dscp_helper.c"

static void help(void) 
{
	printf(
"DSCP match v%s options\n"
"[!] --dscp value		Match DSCP codepoint with numerical value\n"
"  		                This value can be in decimal (ex: 32)\n"
"               		or in hex (ex: 0x20)\n"
"[!] --dscp-class name		Match the DiffServ class. This value may\n"
"				be any of the BE,EF, AFxx or CSx classes\n"
"\n"
"				These two options are mutually exclusive !\n"
				, IPTABLES_VERSION
);
}

static struct option opts[] = {
	{ "dscp", 1, 0, 'F' },
	{ "dscp-class", 1, 0, 'G' },
	{ 0 }
};

static void
parse_dscp(const char *s, struct ipt_dscp_info *dinfo)
{
	unsigned int dscp;
       
	if (string_to_number(s, 0, 255, &dscp) == -1)
		exit_error(PARAMETER_PROBLEM,
			   "Invalid dscp `%s'\n", s);

	if (dscp > IPT_DSCP_MAX)
		exit_error(PARAMETER_PROBLEM,
			   "DSCP `%d` out of range\n", dscp);

    	dinfo->dscp = (u_int8_t )dscp;
    	return;
}


static void
parse_class(const char *s, struct ipt_dscp_info *dinfo)
{
	unsigned int dscp = class_to_dscp(s);

	/* Assign the value */
	dinfo->dscp = (u_int8_t)dscp;
}


static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_dscp_info *dinfo
		= (struct ipt_dscp_info *)(*match)->data;

	switch (c) {
	case 'F':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "DSCP match: Only use --dscp ONCE!");
		check_inverse(optarg, &invert, &optind, 0);
		parse_dscp(argv[optind-1], dinfo);
		if (invert)
			dinfo->invert = 1;
		*flags = 1;
		break;

	case 'G':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
					"DSCP match: Only use --dscp-class ONCE!");
		check_inverse(optarg, &invert, &optind, 0);
		parse_class(argv[optind - 1], dinfo);
		if (invert)
			dinfo->invert = 1;
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
		           "DSCP match: Parameter --dscp is required");
}

static void
print_dscp(u_int8_t dscp, int invert, int numeric)
{
	if (invert)
		fputc('!', stdout);

 	printf("0x%02x ", dscp);
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	const struct ipt_dscp_info *dinfo =
		(const struct ipt_dscp_info *)match->data;
	printf("DSCP match ");
	print_dscp(dinfo->dscp, dinfo->invert, numeric);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_dscp_info *dinfo =
		(const struct ipt_dscp_info *)match->data;

	printf("--dscp ");
	print_dscp(dinfo->dscp, dinfo->invert, 1);
}

static struct iptables_match dscp = { 
	.next 		= NULL,
	.name 		= "dscp",
	.version 	= IPTABLES_VERSION,
	.size 		= IPT_ALIGN(sizeof(struct ipt_dscp_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_dscp_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&dscp);
}
