/* Shared library add-on to iptables to add IPMARK target support.
 * (C) 2003 by Grzegorz Janoszka <Grzegorz.Janoszka@pro.onet.pl>
 *
 * based on original MARK target
 * 
 * This program is distributed under the terms of GNU GPL
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_IPMARK.h>

#define IPT_ADDR_USED        1
#define IPT_AND_MASK_USED    2
#define IPT_OR_MASK_USED     4

struct ipmarkinfo {
	struct ipt_entry_target t;
	struct ipt_ipmark_target_info ipmark;
};

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"IPMARK target v%s options:\n"
"  --addr src/dst         use source or destination ip address\n"
"  --and-mask value       logical AND ip address with this value becomes MARK\n"
"  --or-mask value        logical OR ip address with this value becomes MARK\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "addr", 1, 0, '1' },
	{ "and-mask", 1, 0, '2' },
	{ "or-mask", 1, 0, '3' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
	struct ipt_ipmark_target_info *ipmarkinfo =
		(struct ipt_ipmark_target_info *)t->data;

	ipmarkinfo->andmask=0xffffffff;
	ipmarkinfo->ormask=0;

}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ipt_ipmark_target_info *ipmarkinfo
		= (struct ipt_ipmark_target_info *)(*target)->data;

	switch (c) {
		char *end;
	case '1':
		if(!strcmp(optarg, "src")) ipmarkinfo->addr=IPT_IPMARK_SRC;
		  else if(!strcmp(optarg, "dst")) ipmarkinfo->addr=IPT_IPMARK_DST;
		    else exit_error(PARAMETER_PROBLEM, "Bad addr value `%s' - should be `src' or `dst'", optarg);
		if (*flags & IPT_ADDR_USED)
			exit_error(PARAMETER_PROBLEM,
			           "IPMARK target: Can't specify --addr twice");
		*flags |= IPT_ADDR_USED;
		break;
	
	case '2':
		ipmarkinfo->andmask = strtoul(optarg, &end, 0);
		if (*end != '\0' || end == optarg)
			exit_error(PARAMETER_PROBLEM, "Bad and-mask value `%s'", optarg);
		if (*flags & IPT_AND_MASK_USED)
			exit_error(PARAMETER_PROBLEM,
			           "IPMARK target: Can't specify --and-mask twice");
		*flags |= IPT_AND_MASK_USED;
		break;
	case '3':
		ipmarkinfo->ormask = strtoul(optarg, &end, 0);
		if (*end != '\0' || end == optarg)
			exit_error(PARAMETER_PROBLEM, "Bad or-mask value `%s'", optarg);
		if (*flags & IPT_OR_MASK_USED)
			exit_error(PARAMETER_PROBLEM,
			           "IPMARK target: Can't specify --or-mask twice");
		*flags |= IPT_OR_MASK_USED;
		break;

	default:
		return 0;
	}

	return 1;
}

static void
final_check(unsigned int flags)
{
	if (!(flags & IPT_ADDR_USED))
		exit_error(PARAMETER_PROBLEM,
		           "IPMARK target: Parameter --addr is required");
	if (!(flags & (IPT_AND_MASK_USED | IPT_OR_MASK_USED)))
		exit_error(PARAMETER_PROBLEM,
		           "IPMARK target: Parameter --and-mask or --or-mask is required");
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	const struct ipt_ipmark_target_info *ipmarkinfo =
		(const struct ipt_ipmark_target_info *)target->data;

	if(ipmarkinfo->addr == IPT_IPMARK_SRC)
	  printf("IPMARK src");
	else
	  printf("IPMARK dst");
	printf(" ip and 0x%lx or 0x%lx", ipmarkinfo->andmask, ipmarkinfo->ormask);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	const struct ipt_ipmark_target_info *ipmarkinfo =
		(const struct ipt_ipmark_target_info *)target->data;

	if(ipmarkinfo->addr == IPT_IPMARK_SRC)
	  printf("--addr=src ");
	else
	  printf("--addr=dst ");
	if(ipmarkinfo->andmask != 0xffffffff)
	  printf("--and-mask 0x%lx ", ipmarkinfo->andmask);
	if(ipmarkinfo->ormask != 0)
	  printf("--or-mask 0x%lx ", ipmarkinfo->ormask);
}

static struct iptables_target ipmark = { 
	.next		= NULL,
	.name		= "IPMARK",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_ipmark_target_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_ipmark_target_info)),
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
	register_target(&ipmark);
}
