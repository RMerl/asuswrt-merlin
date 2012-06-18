/* Shared library add-on to iptables to add ipv4 options matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_ipv4options.h>

#ifndef XTABLES_VERSION
#define XTABLES_VERSION IPTABLES_VERSION
#endif

#ifdef IPT_LIB_DIR
#define xtables_target iptables_target
#define xtables_register_target register_target
#endif

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"ipv4options v%s options:\n"
"      --ssrr    (match strict source routing flag)\n"
"      --lsrr    (match loose  source routing flag)\n"
"      --no-srr  (match packets with no source routing)\n\n"
"  [!] --rr      (match record route flag)\n\n"
"  [!] --ts      (match timestamp flag)\n\n"
"  [!] --ra      (match router-alert option)\n\n"
"  [!] --any-opt (match any option or no option at all if used with '!')\n",
XTABLES_VERSION);
}

static struct option opts[] = {
	{ "ssrr", 0, 0, '1' },
	{ "lsrr", 0, 0, '2' },
	{ "no-srr", 0, 0, '3'},
	{ "rr", 0, 0, '4'},
	{ "ts", 0, 0, '5'},
	{ "ra", 0, 0, '6'},
	{ "any-opt", 0, 0, '7'},
	{0}
};

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
#ifdef _XTABLES_H
      const void *entry, struct xt_entry_match **match)
#else
      const struct ipt_entry *entry, unsigned int *nfcache, struct ipt_entry_match **match)
#endif
{
	struct ipt_ipv4options_info *info = (struct ipt_ipv4options_info *)(*match)->data;

	switch (c)
	{
		/* strict-source-routing */
	case '1':
		if (invert) 
			exit_error(PARAMETER_PROBLEM,
				   "ipv4options: unexpected `!' with --ssrr");
		if (*flags & IPT_IPV4OPTION_MATCH_SSRR)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --ssrr twice");
		if (*flags & IPT_IPV4OPTION_MATCH_LSRR)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --ssrr with --lsrr");
		if (*flags & IPT_IPV4OPTION_DONT_MATCH_SRR)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --ssrr with --no-srr");

		info->options |= IPT_IPV4OPTION_MATCH_SSRR;
		*flags |= IPT_IPV4OPTION_MATCH_SSRR;
		break;

		/* loose-source-routing */
	case '2':
		if (invert) 
			exit_error(PARAMETER_PROBLEM,
				   "ipv4options: unexpected `!' with --lsrr");
		if (*flags & IPT_IPV4OPTION_MATCH_SSRR)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --lsrr twice");
		if (*flags & IPT_IPV4OPTION_MATCH_LSRR)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --lsrr with --ssrr");
		if (*flags & IPT_IPV4OPTION_DONT_MATCH_SRR)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --lsrr with --no-srr");
		info->options |= IPT_IPV4OPTION_MATCH_LSRR;
		*flags |= IPT_IPV4OPTION_MATCH_LSRR;
		break;

		/* no-source-routing */
	case '3':
		if (invert) 
			exit_error(PARAMETER_PROBLEM,
					   "ipv4options: unexpected `!' with --no-srr");
		if (*flags & IPT_IPV4OPTION_DONT_MATCH_SRR)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --no-srr twice");
		if (*flags & IPT_IPV4OPTION_MATCH_SSRR)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --no-srr with --ssrr");
		if (*flags & IPT_IPV4OPTION_MATCH_LSRR)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --no-srr with --lsrr");
		info->options |= IPT_IPV4OPTION_DONT_MATCH_SRR;
		*flags |= IPT_IPV4OPTION_DONT_MATCH_SRR;
		break;

		/* record-route */
	case '4':
		if ((!invert) && (*flags & IPT_IPV4OPTION_MATCH_RR))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --rr twice");	
		if (invert && (*flags & IPT_IPV4OPTION_DONT_MATCH_RR))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --rr twice");
		if ((!invert) && (*flags & IPT_IPV4OPTION_DONT_MATCH_RR))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --rr with ! --rr");
		if (invert && (*flags & IPT_IPV4OPTION_MATCH_RR))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --rr with --rr");
		if (invert) {
			info->options |= IPT_IPV4OPTION_DONT_MATCH_RR;
			*flags |= IPT_IPV4OPTION_DONT_MATCH_RR;
		}
		else {
			info->options |= IPT_IPV4OPTION_MATCH_RR;
			*flags |= IPT_IPV4OPTION_MATCH_RR;
		}
		break;

		/* timestamp */
	case '5':
		if ((!invert) && (*flags & IPT_IPV4OPTION_MATCH_TIMESTAMP))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --ts twice");	
		if (invert && (*flags & IPT_IPV4OPTION_DONT_MATCH_TIMESTAMP))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --ts twice");
		if ((!invert) && (*flags & IPT_IPV4OPTION_DONT_MATCH_TIMESTAMP))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --ts with ! --ts");
		if (invert && (*flags & IPT_IPV4OPTION_MATCH_TIMESTAMP))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --ts with --ts");
		if (invert) {
			info->options |= IPT_IPV4OPTION_DONT_MATCH_TIMESTAMP;
			*flags |= IPT_IPV4OPTION_DONT_MATCH_TIMESTAMP;
		}
		else {
			info->options |= IPT_IPV4OPTION_MATCH_TIMESTAMP;
			*flags |= IPT_IPV4OPTION_MATCH_TIMESTAMP;
		}
		break;

		/* router-alert  */
	case '6':
		if ((!invert) && (*flags & IPT_IPV4OPTION_MATCH_ROUTER_ALERT))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --ra twice");	
		if (invert && (*flags & IPT_IPV4OPTION_DONT_MATCH_ROUTER_ALERT))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --rr twice");
		if ((!invert) && (*flags & IPT_IPV4OPTION_DONT_MATCH_ROUTER_ALERT))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --ra with ! --ra");
		if (invert && (*flags & IPT_IPV4OPTION_MATCH_ROUTER_ALERT))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --ra with --ra");
		if (invert) {
			info->options |= IPT_IPV4OPTION_DONT_MATCH_ROUTER_ALERT;
			*flags |= IPT_IPV4OPTION_DONT_MATCH_ROUTER_ALERT;
		}
		else {
			info->options |= IPT_IPV4OPTION_MATCH_ROUTER_ALERT;
			*flags |= IPT_IPV4OPTION_MATCH_ROUTER_ALERT;
		}
		break;

		/* any option */
	case '7' :
		if ((!invert) && (*flags & IPT_IPV4OPTION_MATCH_ANY_OPT))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --any-opt twice");
		if (invert && (*flags & IPT_IPV4OPTION_MATCH_ANY_OPT))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --any-opt with --any-opt");
		if (invert && (*flags & IPT_IPV4OPTION_DONT_MATCH_ROUTER_ALERT))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --any-opt twice");
		if ((!invert) &&
		    ((*flags & IPT_IPV4OPTION_DONT_MATCH_SRR)       ||
		     (*flags & IPT_IPV4OPTION_DONT_MATCH_RR)        ||
		     (*flags & IPT_IPV4OPTION_DONT_MATCH_TIMESTAMP) ||
		     (*flags & IPT_IPV4OPTION_DONT_MATCH_ROUTER_ALERT)))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --any-opt with any other negative ipv4options match");
		if (invert &&
		    ((*flags & IPT_IPV4OPTION_MATCH_LSRR)      ||
		     (*flags & IPT_IPV4OPTION_MATCH_SSRR)      ||
		     (*flags & IPT_IPV4OPTION_MATCH_RR)        ||
		     (*flags & IPT_IPV4OPTION_MATCH_TIMESTAMP) ||
		     (*flags & IPT_IPV4OPTION_MATCH_ROUTER_ALERT)))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --any-opt with any other positive ipv4options match");
		if (invert) {
			info->options |= IPT_IPV4OPTION_DONT_MATCH_ANY_OPT;
			*flags |= IPT_IPV4OPTION_DONT_MATCH_ANY_OPT;	
		}
		else {
			info->options |= IPT_IPV4OPTION_MATCH_ANY_OPT;
			*flags |= IPT_IPV4OPTION_MATCH_ANY_OPT;
		}
		break;

	default:
		return 0;
	}
	return 1;
}

static void
final_check(unsigned int flags)
{
	if (flags == 0)
		exit_error(PARAMETER_PROBLEM,
			   "ipv4options match: you must specify some parameters. See iptables -m ipv4options --help for help.'");
}

/* Prints out the matchinfo. */
static void
#ifdef _XTABLES_H
print(const void *ip,
      const struct xt_entry_match *match,
#else
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
#endif
      int numeric)
{
	struct ipt_ipv4options_info *info = ((struct ipt_ipv4options_info *)match->data);

	printf(" IPV4OPTS");
	if (info->options & IPT_IPV4OPTION_MATCH_SSRR)
		printf(" SSRR");
	else if (info->options & IPT_IPV4OPTION_MATCH_LSRR)
		printf(" LSRR");
	else if (info->options & IPT_IPV4OPTION_DONT_MATCH_SRR)
		printf(" !SRR");
	if (info->options & IPT_IPV4OPTION_MATCH_RR)
		printf(" RR");
	else if (info->options & IPT_IPV4OPTION_DONT_MATCH_RR)
		printf(" !RR");
	if (info->options & IPT_IPV4OPTION_MATCH_TIMESTAMP)
		printf(" TS");
	else if (info->options & IPT_IPV4OPTION_DONT_MATCH_TIMESTAMP)
		printf(" !TS");
	if (info->options & IPT_IPV4OPTION_MATCH_ROUTER_ALERT)
		printf(" RA");
	else if (info->options & IPT_IPV4OPTION_DONT_MATCH_ROUTER_ALERT)
		printf(" !RA");
	if (info->options & IPT_IPV4OPTION_MATCH_ANY_OPT)
		printf(" ANYOPT ");
	else if (info->options & IPT_IPV4OPTION_DONT_MATCH_ANY_OPT)
		printf(" NOOPT");

	printf(" ");
}

/* Saves the data in parsable form to stdout. */
static void
#ifdef _XTABLES_H
save(const void *ip,
     const struct xt_entry_match *match)
#else
save(const struct ipt_ip *ip,
     const struct ipt_entry_match *match)
#endif
{
	struct ipt_ipv4options_info *info = ((struct ipt_ipv4options_info *)match->data);

	if (info->options & IPT_IPV4OPTION_MATCH_SSRR)
		printf(" --ssrr");
	else if (info->options & IPT_IPV4OPTION_MATCH_LSRR)
		printf(" --lsrr");
	else if (info->options & IPT_IPV4OPTION_DONT_MATCH_SRR)
		printf(" --no-srr");
	if (info->options & IPT_IPV4OPTION_MATCH_RR)
		printf(" --rr");
	else if (info->options & IPT_IPV4OPTION_DONT_MATCH_RR)
		printf(" ! --rr");
	if (info->options & IPT_IPV4OPTION_MATCH_TIMESTAMP)
		printf(" --ts");
	else if (info->options & IPT_IPV4OPTION_DONT_MATCH_TIMESTAMP)
		printf(" ! --ts");
	if (info->options & IPT_IPV4OPTION_MATCH_ROUTER_ALERT)
		printf(" --ra");
	else if (info->options & IPT_IPV4OPTION_DONT_MATCH_ROUTER_ALERT)
		printf(" ! --ra");
	if (info->options & IPT_IPV4OPTION_MATCH_ANY_OPT)
		printf(" --any-opt");
	if (info->options & IPT_IPV4OPTION_DONT_MATCH_ANY_OPT)
		printf(" ! --any-opt");

	printf(" ");
}

static struct xtables_match ipv4options_struct = { 
	.next		= NULL,
	.name		= "ipv4options",
	.version	= XTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_ipv4options_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_ipv4options_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	xtables_register_match(&ipv4options_struct);
}
