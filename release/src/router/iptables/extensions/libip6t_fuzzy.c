/*
   Shared library add-on to iptables to add match support for the fuzzy match.

   This file is distributed under the terms of the GNU General Public
   License (GPL). Copies of the GPL can be obtained from:
   ftp://prep.ai.mit.edu/pub/gnu/GPL

2002-08-07 Hime Aguiar e Oliveira Jr. <hime@engineer.com> : Initial version.
2003-04-08 Maciej Soltysiak <solt@dns.toxicfilms.tv> : IPv6 Port
2003-06-09 Hime Aguiar e Oliveira Jr. <hime@engineer.com> : Bug corrections in
the save function , thanks to information given by Jean-Francois Patenaude.

*/

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <getopt.h>
#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_fuzzy.h>


static void
help(void)
{
	printf(
"fuzzy v%s options:\n"
"                      --lower-limit number (in packets per second)\n"
"                      --upper-limit number\n"
,IPTABLES_VERSION);
};

static struct option opts[] = {
	{ .name = "lower-limit", .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = "upper-limit", .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = 0 }
};

/* Initialize data structures */
static void
init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
	struct ip6t_fuzzy_info *presentinfo = (struct ip6t_fuzzy_info *)(m)->data;
	/*
	 * Default rates ( I'll improve this very soon with something based
	 * on real statistics of the running machine ) .
	*/

	presentinfo->minimum_rate = 1000;
	presentinfo->maximum_rate = 2000;
}

#define IP6T_FUZZY_OPT_MINIMUM	0x01
#define IP6T_FUZZY_OPT_MAXIMUM	0x02

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_fuzzy_info *fuzzyinfo =
		(struct ip6t_fuzzy_info *)(*match)->data;

	u_int32_t num;

	switch (c) {

	case '1':

	if (invert)
	       	exit_error(PARAMETER_PROBLEM,"Can't specify ! --lower-limit");

	if (*flags & IP6T_FUZZY_OPT_MINIMUM)
       	      exit_error(PARAMETER_PROBLEM,"Can't specify --lower-limit twice");

	if (string_to_number(optarg,1,MAXFUZZYRATE,&num) == -1 || num < 1)
			exit_error(PARAMETER_PROBLEM,"BAD --lower-limit");

		fuzzyinfo->minimum_rate = num ;

		*flags |= IP6T_FUZZY_OPT_MINIMUM;

		break;

	case '2':

	if (invert)
		exit_error(PARAMETER_PROBLEM,"Can't specify ! --upper-limit");

	if (*flags & IP6T_FUZZY_OPT_MAXIMUM)
	   exit_error(PARAMETER_PROBLEM,"Can't specify --upper-limit twice");

	if (string_to_number(optarg,1,MAXFUZZYRATE,&num) == -1 || num < 1)
		exit_error(PARAMETER_PROBLEM,"BAD --upper-limit");

		fuzzyinfo->maximum_rate = num;

		*flags |= IP6T_FUZZY_OPT_MAXIMUM;

		break ;

	default:
		return 0;
	}
	return 1;
}

static void final_check(unsigned int flags)
{
}

static void
print(const struct ip6t_ip6 *ipv6,
      const struct ip6t_entry_match *match,
      int numeric)
{
	const struct ip6t_fuzzy_info *fuzzyinfo
		= (const struct ip6t_fuzzy_info *)match->data;

	printf(" fuzzy: lower limit = %u pps - upper limit = %u pps ",
		fuzzyinfo->minimum_rate, fuzzyinfo->maximum_rate);
}

/* Saves the union ip6t_targinfo in parsable form to stdout. */
static void
save(const struct ip6t_ip6 *ipv6, const struct ip6t_entry_match *match)
{
	const struct ip6t_fuzzy_info *fuzzyinfo
		= (const struct ip6t_fuzzy_info *)match->data;

	printf("--lower-limit %u --upper-limit %u ",
		fuzzyinfo->minimum_rate, fuzzyinfo->maximum_rate);
}

struct ip6tables_match fuzzy_match = {
	.name          = "fuzzy",
	.version       = IPTABLES_VERSION,
	.size          = IP6T_ALIGN(sizeof(struct ip6t_fuzzy_info)),
	.userspacesize = IP6T_ALIGN(sizeof(struct ip6t_fuzzy_info)),
	.help          = &help,
	.init          = &init,
	.parse         = &parse,
	.final_check   = &final_check,
	.print         = &print,
	.save          = &save,
	.extra_opts    = opts
};

void _init(void)
{
	register_match6(&fuzzy_match);
}
