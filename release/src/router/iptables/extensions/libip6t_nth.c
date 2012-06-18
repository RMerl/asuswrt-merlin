/* 
   Shared library add-on to iptables to add match support for every Nth packet
   
   This file is distributed under the terms of the GNU General Public
   License (GPL). Copies of the GPL can be obtained from:
   ftp://prep.ai.mit.edu/pub/gnu/GPL

   2001-07-17 Fabrice MARIE <fabrice@netfilter.org> : initial development.
   2001-09-20 Richard Wagner (rwagner@cloudnet.com)
        * added support for multiple counters
        * added support for matching on individual packets
          in the counter cycle
*/

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <getopt.h>
#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_nth.h>


/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"nth v%s options:\n"
"   --every     Nth              Match every Nth packet\n"
"  [--counter]  num              Use counter 0-%u (default:0)\n"
"  [--start]    num              Initialize the counter at the number 'num'\n"
"                                instead of 0. Must be between 0 and Nth-1\n"
"  [--packet]   num              Match on 'num' packet. Must be between 0\n"
"                                and Nth-1.\n\n"
"                                If --packet is used for a counter than\n"
"                                there must be Nth number of --packet\n"
"                                rules, covering all values between 0 and\n"
"                                Nth-1 inclusively.\n",
IPTABLES_VERSION, IP6T_NTH_NUM_COUNTERS-1);
}

static struct option opts[] = {
	{ "every", 1, 0, '1' },
	{ "start", 1, 0, '2' },
        { "counter", 1, 0, '3' },
        { "packet", 1, 0, '4' },
	{ 0 }
};

#define IP6T_NTH_OPT_EVERY	0x01
#define IP6T_NTH_OPT_NOT_EVERY	0x02
#define IP6T_NTH_OPT_START	0x04
#define IP6T_NTH_OPT_COUNTER     0x08
#define IP6T_NTH_OPT_PACKET      0x10

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_nth_info *nthinfo = (struct ip6t_nth_info *)(*match)->data;
	unsigned int num;

	switch (c) {
	case '1':
		/* check for common mistakes... */
		if ((!invert) && (*flags & IP6T_NTH_OPT_EVERY))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --every twice");
		if (invert && (*flags & IP6T_NTH_OPT_NOT_EVERY))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --every twice");
		if ((!invert) && (*flags & IP6T_NTH_OPT_NOT_EVERY))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --every with ! --every");
		if (invert && (*flags & IP6T_NTH_OPT_EVERY))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify ! --every with --every");

		/* Remember, this function will interpret a leading 0 to be 
		   Octal, a leading 0x to be hexdecimal... */
                if (string_to_number(optarg, 2, 100, &num) == -1 || num < 2)
                        exit_error(PARAMETER_PROBLEM,
                                   "bad --every `%s', must be between 2 and 100", optarg);

		/* assign the values */
		nthinfo->every = num-1;
		nthinfo->startat = 0;
                nthinfo->packet = 0xFF;
                if(!(*flags & IP6T_NTH_OPT_EVERY))
                {
                        nthinfo->counter = 0;
                }
		if (invert)
		{
			*flags |= IP6T_NTH_OPT_NOT_EVERY;
			nthinfo->not = 1;
		}
		else
		{
			*flags |= IP6T_NTH_OPT_EVERY;
			nthinfo->not = 0;
		}
		break;
	case '2':
		/* check for common mistakes... */
		if (!((*flags & IP6T_NTH_OPT_EVERY) ||
		      (*flags & IP6T_NTH_OPT_NOT_EVERY)))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --start before --every");
		if (invert)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify with ! --start");
		if (*flags & IP6T_NTH_OPT_START)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --start twice");
		if (string_to_number(optarg, 0, nthinfo->every, &num) == -1)
                        exit_error(PARAMETER_PROBLEM,
                                   "bad --start `%s', must between 0 and %u", optarg, nthinfo->every);
		*flags |= IP6T_NTH_OPT_START;
		nthinfo->startat = num;
		break;
        case '3':
                /* check for common mistakes... */
                if (invert)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify with ! --counter");
                if (*flags & IP6T_NTH_OPT_COUNTER)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --counter twice");
                if (string_to_number(optarg, 0, IP6T_NTH_NUM_COUNTERS-1, &num) == -1)
                        exit_error(PARAMETER_PROBLEM,
                                   "bad --counter `%s', must between 0 and %u", optarg, IP6T_NTH_NUM_COUNTERS-1);
                /* assign the values */
                *flags |= IP6T_NTH_OPT_COUNTER;
                nthinfo->counter = num;
                break;
        case '4':
                /* check for common mistakes... */
                if (!((*flags & IP6T_NTH_OPT_EVERY) ||
                      (*flags & IP6T_NTH_OPT_NOT_EVERY)))
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --packet before --every");
                if ((*flags & IP6T_NTH_OPT_NOT_EVERY))
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --packet with ! --every");
                if (invert)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify with ! --packet");
                if (*flags & IP6T_NTH_OPT_PACKET)
                        exit_error(PARAMETER_PROBLEM,
                                   "Can't specify --packet twice");
                if (string_to_number(optarg, 0, nthinfo->every, &num) == -1)
                        exit_error(PARAMETER_PROBLEM,
                                   "bad --packet `%s', must between 0 and %u", optarg, nthinfo->every);
                *flags |= IP6T_NTH_OPT_PACKET;
                nthinfo->packet = num;
                break;
	default:
		return 0;
	}
	return 1;
}

/* Final check; nothing. */
static void final_check(unsigned int flags)
{
}

/* Prints out the targinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match,
      int numeric)
{
	const struct ip6t_nth_info *nthinfo
		= (const struct ip6t_nth_info *)match->data;

	if (nthinfo->not == 1)
		printf(" !");
	printf("every %uth ", (nthinfo->every +1));
	if (nthinfo->counter != 0) 
		printf("counter #%u ", (nthinfo->counter));
        if (nthinfo->packet != 0xFF)
                printf("packet #%u ", nthinfo->packet);
	if (nthinfo->startat != 0)
		printf("start at %u ", nthinfo->startat);
}

/* Saves the union ip6t_targinfo in parsable form to stdout. */
static void
save(const struct ip6t_ip6 *ip, const struct ip6t_entry_match *match)
{
	const struct ip6t_nth_info *nthinfo
		= (const struct ip6t_nth_info *)match->data;

	if (nthinfo->not == 1)
		printf("! ");
	printf("--every %u ", (nthinfo->every +1));
	printf("--counter %u ", (nthinfo->counter));
	if (nthinfo->startat != 0)
		printf("--start %u ", nthinfo->startat );
        if (nthinfo->packet != 0xFF)
                printf("--packet %u ", nthinfo->packet );
}

struct ip6tables_match nth = {
	.name 		= "nth",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ip6t_nth_info)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ip6t_nth_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts,
};

void _init(void)
{
	register_match6(&nth);
}
