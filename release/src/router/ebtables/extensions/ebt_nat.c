/* ebt_nat
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 *
 * June, 2002
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include <netinet/ether.h>
#include <linux/netfilter_bridge/ebt_nat.h>

static int to_source_supplied, to_dest_supplied;

#define NAT_S '1'
#define NAT_D '1'
#define NAT_S_TARGET '2'
#define NAT_D_TARGET '2'
#define NAT_S_ARP '3'
static struct option opts_s[] =
{
	{ "to-source"     , required_argument, 0, NAT_S },
	{ "to-src"        , required_argument, 0, NAT_S },
	{ "snat-target"   , required_argument, 0, NAT_S_TARGET },
	{ "snat-arp"      ,       no_argument, 0, NAT_S_ARP },
	{ 0 }
};

static struct option opts_d[] =
{
	{ "to-destination", required_argument, 0, NAT_D },
	{ "to-dst"        , required_argument, 0, NAT_D },
	{ "dnat-target"   , required_argument, 0, NAT_D_TARGET },
	{ 0 }
};

static void print_help_s()
{
	printf(
	"snat options:\n"
	" --to-src address       : MAC address to map source to\n"
	" --snat-target target   : ACCEPT, DROP, RETURN or CONTINUE\n"
	" --snat-arp             : also change src address in arp msg\n");
}

static void print_help_d()
{
	printf(
	"dnat options:\n"
	" --to-dst address       : MAC address to map destination to\n"
	" --dnat-target target   : ACCEPT, DROP, RETURN or CONTINUE\n");
}

static void init_s(struct ebt_entry_target *target)
{
	struct ebt_nat_info *natinfo = (struct ebt_nat_info *)target->data;

	to_source_supplied = 0;
	natinfo->target = EBT_ACCEPT;
	return;
}

static void init_d(struct ebt_entry_target *target)
{
	struct ebt_nat_info *natinfo = (struct ebt_nat_info *)target->data;

	to_dest_supplied = 0;
	natinfo->target = EBT_ACCEPT;
	return;
}

#define OPT_SNAT         0x01
#define OPT_SNAT_TARGET  0x02
#define OPT_SNAT_ARP     0x04
static int parse_s(int c, char **argv, int argc,
   const struct ebt_u_entry *entry, unsigned int *flags,
   struct ebt_entry_target **target)
{
	struct ebt_nat_info *natinfo = (struct ebt_nat_info *)(*target)->data;
	struct ether_addr *addr;

	switch (c) {
	case NAT_S:
		ebt_check_option2(flags, OPT_SNAT);
		to_source_supplied = 1;
		if (!(addr = ether_aton(optarg)))
			ebt_print_error2("Problem with specified --to-source mac");
		memcpy(natinfo->mac, addr, ETH_ALEN);
		break;
	case NAT_S_TARGET:
		{ int tmp;
		ebt_check_option2(flags, OPT_SNAT_TARGET);
		if (FILL_TARGET(optarg, tmp))
			ebt_print_error2("Illegal --snat-target target");
		natinfo->target = (natinfo->target & ~EBT_VERDICT_BITS) | (tmp & EBT_VERDICT_BITS);
		}
		break;
	case NAT_S_ARP:
		ebt_check_option2(flags, OPT_SNAT_ARP);
		natinfo->target ^= NAT_ARP_BIT;
		break;
	default:
		return 0;
	}
	return 1;
}

#define OPT_DNAT        0x01
#define OPT_DNAT_TARGET 0x02
static int parse_d(int c, char **argv, int argc,
   const struct ebt_u_entry *entry, unsigned int *flags,
   struct ebt_entry_target **target)
{
	struct ebt_nat_info *natinfo = (struct ebt_nat_info *)(*target)->data;
	struct ether_addr *addr;

	switch (c) {
	case NAT_D:
		ebt_check_option2(flags, OPT_DNAT);
		to_dest_supplied = 1;
		if (!(addr = ether_aton(optarg)))
			ebt_print_error2("Problem with specified --to-destination mac");
		memcpy(natinfo->mac, addr, ETH_ALEN);
		break;
	case NAT_D_TARGET:
		ebt_check_option2(flags, OPT_DNAT_TARGET);
		if (FILL_TARGET(optarg, natinfo->target))
			ebt_print_error2("Illegal --dnat-target target");
		break;
	default:
		return 0;
	}
	return 1;
}

static void final_check_s(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target, const char *name,
   unsigned int hookmask, unsigned int time)
{
	struct ebt_nat_info *natinfo = (struct ebt_nat_info *)target->data;

	if (BASE_CHAIN && (natinfo->target | ~EBT_VERDICT_BITS) == EBT_RETURN) {
		ebt_print_error("--snat-target RETURN not allowed on base chain");
		return;
	}
	CLEAR_BASE_CHAIN_BIT;
	if ((hookmask & ~(1 << NF_BR_POST_ROUTING)) || strcmp(name, "nat")) {
		ebt_print_error("Wrong chain for snat");
	} else if (time == 0 && to_source_supplied == 0)
		ebt_print_error("No snat address supplied");
}

static void final_check_d(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target, const char *name,
   unsigned int hookmask, unsigned int time)
{
	struct ebt_nat_info *natinfo = (struct ebt_nat_info *)target->data;

	if (BASE_CHAIN && natinfo->target == EBT_RETURN) {
		ebt_print_error("--dnat-target RETURN not allowed on base chain");
		return;
	}
	CLEAR_BASE_CHAIN_BIT;
	if (((hookmask & ~((1 << NF_BR_PRE_ROUTING) | (1 << NF_BR_LOCAL_OUT)))
	   || strcmp(name, "nat")) &&
	   ((hookmask & ~(1 << NF_BR_BROUTING)) || strcmp(name, "broute"))) {
		ebt_print_error("Wrong chain for dnat");
	} else if (time == 0 && to_dest_supplied == 0)
		ebt_print_error("No dnat address supplied");
}

static void print_s(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target)
{
	struct ebt_nat_info *natinfo = (struct ebt_nat_info *)target->data;

	printf("--to-src ");
	ebt_print_mac(natinfo->mac);
	if (!(natinfo->target&NAT_ARP_BIT))
		printf(" --snat-arp");
	printf(" --snat-target %s", TARGET_NAME((natinfo->target|~EBT_VERDICT_BITS)));
}

static void print_d(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target)
{
	struct ebt_nat_info *natinfo = (struct ebt_nat_info *)target->data;

	printf("--to-dst ");
	ebt_print_mac(natinfo->mac);
	printf(" --dnat-target %s", TARGET_NAME(natinfo->target));
}

static int compare(const struct ebt_entry_target *t1,
   const struct ebt_entry_target *t2)
{
	struct ebt_nat_info *natinfo1 = (struct ebt_nat_info *)t1->data;
	struct ebt_nat_info *natinfo2 = (struct ebt_nat_info *)t2->data;

	return !memcmp(natinfo1->mac, natinfo2->mac, sizeof(natinfo1->mac)) &&
	   natinfo1->target == natinfo2->target;
}

static struct ebt_u_target snat_target =
{
	.name		= "snat",
	.size		= sizeof(struct ebt_nat_info),
	.help		= print_help_s,
	.init		= init_s,
	.parse		= parse_s,
	.final_check	= final_check_s,
	.print		= print_s,
	.compare	= compare,
	.extra_ops	= opts_s,
};

static struct ebt_u_target dnat_target =
{
	.name		= "dnat",
	.size		= sizeof(struct ebt_nat_info),
	.help		= print_help_d,
	.init		= init_d,
	.parse		= parse_d,
	.final_check	= final_check_d,
	.print		= print_d,
	.compare	= compare,
	.extra_ops	= opts_d,
};

void _init(void)
{
	ebt_register_target(&snat_target);
	ebt_register_target(&dnat_target);
}
