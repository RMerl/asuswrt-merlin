/* Shared library add-on to ip666666tables for NFQ
 *
 * (C) 2005 by Harald Welte <laforge@netfilter.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv4/ipt_NFQUEUE.h>

static void init(struct ip6t_entry_target *t, unsigned int *nfcache) 
{
}

static void help(void) 
{
	printf(
"NFQUEUE target options\n"
"  --queue-num value		Send packet to QUEUE number <value>.\n"
"  		                Valid queue numbers are 0-65535\n"
);
}

static struct option opts[] = {
	{ "queue-num", 1, 0, 'F' },
	{ 0 }
};

static void
parse_num(const char *s, struct ipt_NFQ_info *tinfo)
{
	unsigned int num;
       
	if (string_to_number(s, 0, 65535, &num) == -1)
		exit_error(PARAMETER_PROBLEM,
			   "Invalid queue number `%s'\n", s);

    	tinfo->queuenum = num & 0xffff;
    	return;
}

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      struct ip6t_entry_target **target)
{
	struct ipt_NFQ_info *tinfo
		= (struct ipt_NFQ_info *)(*target)->data;

	switch (c) {
	case 'F':
		if (*flags)
			exit_error(PARAMETER_PROBLEM, "NFQUEUE target: "
				   "Only use --queue-num ONCE!");
		parse_num(optarg, tinfo);
		break;
	default:
		return 0;
	}

	return 1;
}

static void
final_check(unsigned int flags)
{
}

/* Prints out the targinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_target *target,
      int numeric)
{
	const struct ipt_NFQ_info *tinfo =
		(const struct ipt_NFQ_info *)target->data;
	printf("NFQUEUE num %u", tinfo->queuenum);
}

/* Saves the union ip6t_targinfo in parsable form to stdout. */
static void
save(const struct ip6t_ip6 *ip, const struct ip6t_entry_target *target)
{
	const struct ipt_NFQ_info *tinfo =
		(const struct ipt_NFQ_info *)target->data;

	printf("--queue-num %u ", tinfo->queuenum);
}

static struct ip6tables_target nfqueue = { 
	.next		= NULL,
	.name		= "NFQUEUE",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ipt_NFQ_info)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ipt_NFQ_info)),
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
	register_target6(&nfqueue);
}
