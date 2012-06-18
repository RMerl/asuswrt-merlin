/* Shared library add-on to iptables to add ROUTE target support.
 * Author : Cedric de Launois, <delaunois@info.ucl.ac.be>
 * v 1.11 2004/11/23
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_ROUTE.h>

#ifndef XTABLES_VERSION
#define XTABLES_VERSION IPTABLES_VERSION
#endif

#ifdef IPT_LIB_DIR
#define xtables_target iptables_target
#define xtables_register_target register_target
#endif

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

static struct option opts[] = {
	{ "oif", 1, 0, '1' },
	{ "iif", 1, 0, '2' },
	{ "gw", 1, 0, '3' },
	{ "continue", 0, 0, '4' },
	{ "tee", 0, 0, '5' },
	{ 0 }
};

/* Initialize the target. */
static void
#ifdef _XTABLES_H
init(struct xt_entry_target *t)
#else
init(struct ipt_entry_target *t, unsigned int *nfcache)
#endif
{
	struct ipt_route_target_info *route_info = 
		(struct ipt_route_target_info*)t->data;

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
static int
parse(int c, char **argv, int invert, unsigned int *flags,
#ifdef _XTABLES_H
      const void *entry, struct xt_entry_target **target)
#else
      const struct ipt_entry *entry, struct ipt_entry_target **target)
#endif
{
	struct ipt_route_target_info *route_info = 
		(struct ipt_route_target_info*)(*target)->data;

	switch (c) {
	case '1':
		if (*flags & IPT_ROUTE_OPT_OIF)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --oif twice");

		if (*flags & IPT_ROUTE_OPT_IIF)
			exit_error(PARAMETER_PROBLEM,
				   "Can't use --oif and --iif together");

		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --oif");

		if (strlen(optarg) > sizeof(route_info->oif) - 1)
			exit_error(PARAMETER_PROBLEM,
				   "Maximum interface name length %u",
				   sizeof(route_info->oif) - 1);

		strcpy(route_info->oif, optarg);
		*flags |= IPT_ROUTE_OPT_OIF;
		break;

	case '2':
		if (*flags & IPT_ROUTE_OPT_IIF)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --iif twice");

		if (*flags & IPT_ROUTE_OPT_OIF)
			exit_error(PARAMETER_PROBLEM,
				   "Can't use --iif and --oif together");

		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --iif");

		if (strlen(optarg) > sizeof(route_info->iif) - 1)
			exit_error(PARAMETER_PROBLEM,
				   "Maximum interface name length %u",
				   sizeof(route_info->iif) - 1);

		strcpy(route_info->iif, optarg);
		*flags |= IPT_ROUTE_OPT_IIF;
		break;

	case '3':
		if (*flags & IPT_ROUTE_OPT_GW)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --gw twice");

		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --gw");

		if (!inet_aton(optarg, (struct in_addr*)&route_info->gw)) {
			exit_error(PARAMETER_PROBLEM,
				   "Invalid IP address %s",
				   optarg);
		}

		*flags |= IPT_ROUTE_OPT_GW;
		break;

	case '4':
		if (*flags & IPT_ROUTE_OPT_CONTINUE)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --continue twice");
		if (*flags & IPT_ROUTE_OPT_TEE)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --continue AND --tee");

		route_info->flags |= IPT_ROUTE_CONTINUE;
		*flags |= IPT_ROUTE_OPT_CONTINUE;

		break;

	case '5':
		if (*flags & IPT_ROUTE_OPT_TEE)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --tee twice");
		if (*flags & IPT_ROUTE_OPT_CONTINUE)
			exit_error(PARAMETER_PROBLEM,
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
		exit_error(PARAMETER_PROBLEM,
		           "ROUTE target: oif, iif or gw option required");

	if ((flags & (IPT_ROUTE_OPT_CONTINUE|IPT_ROUTE_OPT_TEE)) && (flags & IPT_ROUTE_OPT_IIF))
		exit_error(PARAMETER_PROBLEM,
			   "ROUTE target: can't continue traversing the rules with iif option");
}


/* Prints out the targinfo. */
static void
#ifdef _XTABLES_H
print(const void *ip,
      const struct xt_entry_target *target,
#else
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
#endif
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
		struct in_addr gw = { route_info->gw };
		printf("gw:%s ", inet_ntoa(gw));
	}

	if (route_info->flags & IPT_ROUTE_CONTINUE)
		printf("continue");

	if (route_info->flags & IPT_ROUTE_TEE)
		printf("tee");

}

static void
#ifdef _XTABLES_H
save(const void *ip,
     const struct xt_entry_target *target)
#else
save(const struct ipt_ip *ip,
     const struct ipt_entry_target *target)
#endif
{
	const struct ipt_route_target_info *route_info
		= (const struct ipt_route_target_info *)target->data;

	if (route_info->oif[0])
		printf("--oif %s ", route_info->oif);

	if (route_info->iif[0])
		printf("--iif %s ", route_info->iif);

	if (route_info->gw) {
		struct in_addr gw = { route_info->gw };
		printf("--gw %s ", inet_ntoa(gw));
	}

	if (route_info->flags & IPT_ROUTE_CONTINUE)
		printf("--continue ");

	if (route_info->flags & IPT_ROUTE_TEE)
		printf("--tee ");
}


static struct xtables_target route = { 
	.next		= NULL,
	.name		= "ROUTE",
	.version	= XTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_route_target_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_route_target_info)),
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
	xtables_register_target(&route);
}
