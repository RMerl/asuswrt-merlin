/* Shared library add-on to iptables to add simple load-balance support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>

#define BREAKUP_IP(x) (x)>>24, ((x)>>16) & 0xFF, ((x)>>8) & 0xFF, (x) & 0xFF

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"BALANCE v%s options:\n"
" --to-destination <ipaddr>-<ipaddr>\n"
"				Addresses to map destination to.\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "to-destination", 1, 0, '1' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
	struct ip_nat_multi_range *mr = (struct ip_nat_multi_range *)t->data;

	/* Actually, it's 0, but it's ignored at the moment. */
	mr->rangesize = 1;

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
	else
		exit_error(PARAMETER_PROBLEM, "Bad IP range `%s'\n", arg);

	ip = dotted_to_addr(arg);
	if (!ip)
		exit_error(PARAMETER_PROBLEM, "Bad IP address `%s'\n",
			   arg);
	range->min_ip = ip->s_addr;
	ip = dotted_to_addr(dash+1);
	if (!ip)
		exit_error(PARAMETER_PROBLEM, "Bad IP address `%s'\n",
			   dash+1);
	range->max_ip = ip->s_addr;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ip_nat_multi_range *mr
		= (struct ip_nat_multi_range *)(*target)->data;

	switch (c) {
	case '1':
		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --to-destination");

		parse_to(optarg, &mr->range[0]);
		*flags = 1;
		return 1;

	default:
		return 0;
	}
}

/* Final check; need --to-dest. */
static void final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "BALANCE needs --to-destination");
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	struct ip_nat_multi_range *mr
		= (struct ip_nat_multi_range *)target->data;
	struct ip_nat_range *r = &mr->range[0];
	struct in_addr a;

	a.s_addr = r->min_ip;

	printf("balance %s", addr_to_dotted(&a));
	a.s_addr = r->max_ip;
	printf("-%s ", addr_to_dotted(&a));
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	struct ip_nat_multi_range *mr
		= (struct ip_nat_multi_range *)target->data;
	struct ip_nat_range *r = &mr->range[0];
	struct in_addr a;

	a.s_addr = r->min_ip;
	printf("--to-destination %s", addr_to_dotted(&a));
	a.s_addr = r->max_ip;
	printf("-%s ", addr_to_dotted(&a));
}

static struct iptables_target balance = { 
	.next		= NULL,
	.name		= "BALANCE",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ip_nat_multi_range)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ip_nat_multi_range)),
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
	register_target(&balance);
}
