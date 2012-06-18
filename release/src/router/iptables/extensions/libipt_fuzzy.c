/* 
   Shared library add-on to iptables to add match support for the fuzzy match.
   
   This file is distributed under the terms of the GNU General Public
   License (GPL). Copies of the GPL can be obtained from:
   ftp://prep.ai.mit.edu/pub/gnu/GPL

2002-08-07 Hime Aguiar e Oliveira Jr. <hime@engineer.com> : Initial version.
2003-06-09 Hime Aguiar e Oliveira Jr. <hime@engineer.com> : Bug corrections in
the save function , thanks to information given by Jean-Francois Patenaude .

*/

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_fuzzy.h>


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
	{ "lower-limit", 1 , 0 , '1' } ,
	{ "upper-limit", 1 , 0 , '2' } ,
	{ 0 }
};

/* Initialize data structures */
static void
init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	struct ipt_fuzzy_info *presentinfo = (struct ipt_fuzzy_info *)(m)->data;

	/*
	 * Default rates ( I'll improve this very soon with something based 
	 * on real statistics of the running machine ) .
	*/

	presentinfo->minimum_rate = 1000;
	presentinfo->maximum_rate = 2000;
}

#define IPT_FUZZY_OPT_MINIMUM	0x01
#define IPT_FUZZY_OPT_MAXIMUM	0x02

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{

struct ipt_fuzzy_info *fuzzyinfo = (struct ipt_fuzzy_info *)(*match)->data;

	u_int32_t num;

	switch (c) {

	case '1':
		
	if (invert)
	       	exit_error(PARAMETER_PROBLEM,"Can't specify ! --lower-limit");

	if (*flags & IPT_FUZZY_OPT_MINIMUM)
       	      exit_error(PARAMETER_PROBLEM,"Can't specify --lower-limit twice");
	
	if (string_to_number(optarg,1,MAXFUZZYRATE,&num) == -1 || num < 1)
			exit_error(PARAMETER_PROBLEM,"BAD --lower-limit");

		fuzzyinfo->minimum_rate = num ;

		*flags |= IPT_FUZZY_OPT_MINIMUM;
		
		break;

	case '2':

	if (invert)
		exit_error(PARAMETER_PROBLEM,"Can't specify ! --upper-limit");

	if (*flags & IPT_FUZZY_OPT_MAXIMUM)
	   exit_error(PARAMETER_PROBLEM,"Can't specify --upper-limit twice");

	if (string_to_number(optarg,1,MAXFUZZYRATE,&num) == -1 || num < 1)
		exit_error(PARAMETER_PROBLEM,"BAD --upper-limit");

		fuzzyinfo->maximum_rate = num ;

		*flags |= IPT_FUZZY_OPT_MAXIMUM;

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
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	const struct ipt_fuzzy_info *fuzzyinfo
		= (const struct ipt_fuzzy_info *)match->data;

	printf(" fuzzy: lower limit = %u pps - upper limit = %u pps ",fuzzyinfo->minimum_rate,fuzzyinfo->maximum_rate);

}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_fuzzy_info *fuzzyinfo
		= (const struct ipt_fuzzy_info *)match->data;

	printf("--lower-limit %u ",fuzzyinfo->minimum_rate);
	printf("--upper-limit %u ",fuzzyinfo->maximum_rate);

}

static struct iptables_match fuzzy_match = { 
	.next 		= NULL,
	.name		= "fuzzy",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_fuzzy_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_fuzzy_info)),
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
	register_match(&fuzzy_match);
}
