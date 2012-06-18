/* Shared library add-on to iptables to add simple non load-balancing SNAT support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter/nf_nat.h>
/* For 64bit kernel / 32bit userspace */
#include "../include/linux/netfilter_ipv4/ipt_SAME.h"

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"SAME v%s options:\n"
" --to <ipaddr>-<ipaddr>\n"
"				Addresses to map source to.\n"
"				 May be specified more than\n"
"				  once for multiple ranges.\n"
" --nodst\n"
"				Don't use destination-ip in\n"
"				           source selection\n"
" --random\n"
"				Randomize source port\n"
,
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "to", 1, 0, '1' },
	{ "nodst", 0, 0, '2'},
	{ "random", 0, 0, '3' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
	struct ipt_same_info *mr = (struct ipt_same_info *)t->data;

	/* Set default to 0 */
	mr->rangesize = 0;
	mr->info = 0;
	mr->ipnum = 0;
	
}

/* Parses range of IPs */
static void
parse_to(char *arg, struct ip_nat_range *range)
{
	char *dash;
	struct in_addr *ip;

	range->flags |= IP_NAT_RANGE_MAP_IPS;
	dash = strchr(arg, '-');

	if (dash)
		*dash = '\0';

	ip = dotted_to_addr(arg);
	if (!ip)
		exit_error(PARAMETER_PROBLEM, "Bad IP address `%s'\n",
			   arg);
	range->min_ip = ip->s_addr;

	if (dash) {
		ip = dotted_to_addr(dash+1);
		if (!ip)
			exit_error(PARAMETER_PROBLEM, "Bad IP address `%s'\n",
				   dash+1);
	}
	range->max_ip = ip->s_addr;
	if (dash)
		if (range->min_ip > range->max_ip)
			exit_error(PARAMETER_PROBLEM, "Bad IP range `%s-%s'\n", 
				   arg, dash+1);
}

#define IPT_SAME_OPT_TO			0x01
#define IPT_SAME_OPT_NODST		0x02
#define IPT_SAME_OPT_RANDOM		0x04

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ipt_same_info *mr
		= (struct ipt_same_info *)(*target)->data;
	int count;

	switch (c) {
	case '1':
		if (mr->rangesize == IPT_SAME_MAX_RANGE)
			exit_error(PARAMETER_PROBLEM,
				   "Too many ranges specified, maximum "
				   "is %i ranges.\n",
				   IPT_SAME_MAX_RANGE);
		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --to");

		parse_to(optarg, &mr->range[mr->rangesize]);
		/* WTF do we need this for? */
		if (*flags & IPT_SAME_OPT_RANDOM)
			mr->range[mr->rangesize].flags 
				|= IP_NAT_RANGE_PROTO_RANDOM;
		mr->rangesize++;
		*flags |= IPT_SAME_OPT_TO;
		break;
		
	case '2':
		if (*flags & IPT_SAME_OPT_NODST)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --nodst twice");
		
		mr->info |= IPT_SAME_NODST;
		*flags |= IPT_SAME_OPT_NODST;
		break;

	case '3':	
		*flags |= IPT_SAME_OPT_RANDOM;
		for (count=0; count < mr->rangesize; count++)
			mr->range[count].flags |= IP_NAT_RANGE_PROTO_RANDOM;
		break;

	default:
		return 0;
	}
	
	return 1;
}

/* Final check; need --to. */
static void final_check(unsigned int flags)
{
	if (!(flags & IPT_SAME_OPT_TO))
		exit_error(PARAMETER_PROBLEM,
			   "SAME needs --to");
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	int count;
	struct ipt_same_info *mr
		= (struct ipt_same_info *)target->data;
	int random = 0;
	
	printf("same:");
	
	for (count = 0; count < mr->rangesize; count++) {
		struct ip_nat_range *r = &mr->range[count];
		struct in_addr a;

		a.s_addr = r->min_ip;

		printf("%s", addr_to_dotted(&a));
		a.s_addr = r->max_ip;
		
		if (r->min_ip == r->max_ip)
			printf(" ");
		else
			printf("-%s ", addr_to_dotted(&a));
		if (r->flags & IP_NAT_RANGE_PROTO_RANDOM) 
			random = 1;
	}
	
	if (mr->info & IPT_SAME_NODST)
		printf("nodst ");

	if (random)
		printf("random ");
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	int count;
	struct ipt_same_info *mr
		= (struct ipt_same_info *)target->data;
	int random = 0;

	for (count = 0; count < mr->rangesize; count++) {
		struct ip_nat_range *r = &mr->range[count];
		struct in_addr a;

		a.s_addr = r->min_ip;
		printf("--to %s", addr_to_dotted(&a));
		a.s_addr = r->max_ip;

		if (r->min_ip == r->max_ip)
			printf(" ");
		else
			printf("-%s ", addr_to_dotted(&a));
		if (r->flags & IP_NAT_RANGE_PROTO_RANDOM) 
			random = 1;
	}
	
	if (mr->info & IPT_SAME_NODST)
		printf("--nodst ");

	if (random)
		printf("--random ");
}

static struct iptables_target same = {
	.next		= NULL,
	.name		= "SAME",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_same_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_same_info)),
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
	register_target(&same);
}
