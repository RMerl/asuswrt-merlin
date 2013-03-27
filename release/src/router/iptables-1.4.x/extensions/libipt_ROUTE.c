/* Shared library add-on to iptables to add ROUTE target support.
 * Author : Cedric de Launois, <delaunois@info.ucl.ac.be>
 * v 1.11 2004/11/23
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_ROUTE.h>

/* compile IPT_ROUTE_TEE support even if kernel headers are unpatched */
#ifndef IPT_ROUTE_TEE
#define IPT_ROUTE_TEE		0x02
#endif

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"ROUTE target v%s options:\n"
"    --oif   \tifname \t\tRoute packet through `ifname' network interface\n"
"    --iif   \tifname \t\tChange packet's incoming interface to `ifname'\n"
"    --gw    \tip     \t\tRoute packet via this gateway `ip'\n"
"    --continue\t     \t\tRoute packet and continue traversing the\n"
"            \t       \t\trules. Not valid with --iif or --tee.\n"
"    --tee\t  \t\tDuplicate packet, route the duplicate,\n"
"            \t       \t\tcontinue traversing with original packet.\n"
"            \t       \t\tNot valid with --iif or --continue.\n"
"\n",
"1.11");
}

static struct option route_opts[] = {
	{.name = "oif", .has_arg = true, .val = '1' },
	{.name = "iif", .has_arg = true, .val = '2' },
	{.name = "gw", .has_arg = true, .val = '3' },
	{.name = "continue", .has_arg = false, .val = '4' },
	{.name = "tee", .has_arg = false, .val = '5' },
	XT_GETOPT_TABLEEND,
};

/* Initialize the target. */
static void ROUTE_init(struct xt_entry_target *t)
{
	struct ipt_route_target_info *route_info = (struct ipt_route_target_info*)t->data;

	route_info->oif[0] = '\0';
	route_info->iif[0] = '\0';
	route_info->gw = 0;
	route_info->flags = 0;
}


#define IPT_ROUTE_OPT_OIF      0x01
#define IPT_ROUTE_OPT_IIF      0x02
#define IPT_ROUTE_OPT_GW       0x04
#define IPT_ROUTE_OPT_CONTINUE 0x08
#define IPT_ROUTE_OPT_TEE      0x10

/* Function which parses command options; returns true if it
   ate an option */
static int ROUTE_parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry, struct xt_entry_target **target)
{
	struct ipt_route_target_info *route_info = 
		(struct ipt_route_target_info*)(*target)->data;

	switch (c) {
	case '1':
		if (*flags & IPT_ROUTE_OPT_OIF)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --oif twice");

		if (*flags & IPT_ROUTE_OPT_IIF)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't use --oif and --iif together");

		if (strlen(optarg) > sizeof(route_info->oif) - 1)
			xtables_error(PARAMETER_PROBLEM,
				   "Maximum interface name length %u",
				   sizeof(route_info->oif) - 1);

		strcpy(route_info->oif, optarg);
		*flags |= IPT_ROUTE_OPT_OIF;
		break;

	case '2':
		if (*flags & IPT_ROUTE_OPT_IIF)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --iif twice");

		if (*flags & IPT_ROUTE_OPT_OIF)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't use --iif and --oif together");

		if (strlen(optarg) > sizeof(route_info->iif) - 1)
			xtables_error(PARAMETER_PROBLEM,
				   "Maximum interface name length %u",
				   sizeof(route_info->iif) - 1);

		strcpy(route_info->iif, optarg);
		*flags |= IPT_ROUTE_OPT_IIF;
		break;

	case '3':
		if (*flags & IPT_ROUTE_OPT_GW)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --gw twice");

		if (!inet_aton(optarg, (struct in_addr*)&route_info->gw)) {
			xtables_error(PARAMETER_PROBLEM,
				   "Invalid IP address %s",
				   optarg);
		}

		*flags |= IPT_ROUTE_OPT_GW;
		break;

	case '4':
		if (*flags & IPT_ROUTE_OPT_CONTINUE)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --continue twice");
		if (*flags & IPT_ROUTE_OPT_TEE)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --continue AND --tee");

		route_info->flags |= IPT_ROUTE_CONTINUE;
		*flags |= IPT_ROUTE_OPT_CONTINUE;

		break;

	case '5':
		if (*flags & IPT_ROUTE_OPT_TEE)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --tee twice");
		if (*flags & IPT_ROUTE_OPT_CONTINUE)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --tee AND --continue");

		route_info->flags |= IPT_ROUTE_TEE;
		*flags |= IPT_ROUTE_OPT_TEE;

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
		xtables_error(PARAMETER_PROBLEM,
		           "ROUTE target: oif, iif or gw option required");

	if ((flags & (IPT_ROUTE_OPT_CONTINUE|IPT_ROUTE_OPT_TEE)) && (flags & IPT_ROUTE_OPT_IIF))
		xtables_error(PARAMETER_PROBLEM,
			   "ROUTE target: can't continue traversing the rules with iif option");
}


/* Prints out the targinfo. */
static void
ROUTE_print(const void *ip, const struct xt_entry_target *target,
      int numeric)
{
	const struct ipt_route_target_info *route_info
		= (const struct ipt_route_target_info *)target->data;

	printf("ROUTE ");

	if (route_info->oif[0])
		printf("oif:%s ", route_info->oif);

	if (route_info->iif[0])
		printf("iif:%s ", route_info->iif);

	if (route_info->gw) {
		struct in_addr ip_1 = { route_info->gw };
		printf("gw:%s ", inet_ntoa(ip_1));
	}

	if (route_info->flags & IPT_ROUTE_CONTINUE)
		printf("continue");

	if (route_info->flags & IPT_ROUTE_TEE)
		printf("tee");

}


static void ROUTE_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_route_target_info *route_info
		= (const struct ipt_route_target_info *)target->data;

	if (route_info->oif[0])
		printf("--oif %s ", route_info->oif);

	if (route_info->iif[0])
		printf("--iif %s ", route_info->iif);

	if (route_info->gw) {
		struct in_addr ip_1 = { route_info->gw };
		printf("--gw %s ", inet_ntoa(ip_1));
	}

	if (route_info->flags & IPT_ROUTE_CONTINUE)
		printf("--continue ");

	if (route_info->flags & IPT_ROUTE_TEE)
		printf("--tee ");
}


static struct xtables_target route_target = {
	.family		= NFPROTO_IPV4,
	.name		= "ROUTE",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct ipt_route_target_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_route_target_info)),
	.help		= help,
	.init		= ROUTE_init,
	.parse		= ROUTE_parse,
	.final_check	= final_check,
	.print		= ROUTE_print,
	.save		= ROUTE_save,
	.extra_opts	= route_opts
};

void _init(void)
{
	xtables_register_target(&route_target);
}
