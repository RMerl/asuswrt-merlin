/* Shared library add-on to iptables to add ROUTE v6 target support.
 * Author : Cedric de Launois, <delaunois@info.ucl.ac.be>
 * v 1.1 2004/11/23
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <ip6tables.h>
#include <xtables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_ROUTE.h>

#ifndef XTABLES_VERSION
#define XTABLES_VERSION IPTABLES_VERSION
#endif

/* compile IP6T_ROUTE_TEE support even if kernel headers are unpatched */
#ifndef IP6T_ROUTE_TEE
#define IP6T_ROUTE_TEE		0x02
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
	{ .name = "oif", .has_arg = true, .val = '1' },
	{ .name = "iif", .has_arg = true, .val = '2' },
	{ .name = "gw", .has_arg = true, .val = '3' },
	{ .name = "continue", .has_arg = false, .val = '4' },
	{ .name = "tee", .has_arg = false, .val = '5' },
	XT_GETOPT_TABLEEND
};

/* Initialize the target. */
static void
init(struct xt_entry_target *t)
{
	struct ip6t_route_target_info *route_info = 
		(struct ip6t_route_target_info*)t->data;

	route_info->oif[0] = '\0';
	route_info->iif[0] = '\0';
	route_info->gw[0] = 0;
	route_info->gw[1] = 0;
	route_info->gw[2] = 0;
	route_info->gw[3] = 0;
	route_info->flags = 0;
}


#define IP6T_ROUTE_OPT_OIF      0x01
#define IP6T_ROUTE_OPT_IIF      0x02
#define IP6T_ROUTE_OPT_GW       0x04
#define IP6T_ROUTE_OPT_CONTINUE 0x08
#define IP6T_ROUTE_OPT_TEE      0x10

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry, struct xt_entry_target **target)
{
	struct ip6t_route_target_info *route_info = 
		(struct ip6t_route_target_info*)(*target)->data;

	switch (c) {
	case '1':
		if (*flags & IP6T_ROUTE_OPT_OIF)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --oif twice");

		if (strlen(optarg) > sizeof(route_info->oif) - 1)
			xtables_error(PARAMETER_PROBLEM,
				   "Maximum interface name length %u",
				   sizeof(route_info->oif) - 1);

		strcpy(route_info->oif, optarg);
		*flags |= IP6T_ROUTE_OPT_OIF;
		break;

	case '2':
		xtables_error(PARAMETER_PROBLEM,
			   "--iif option not implemented");
		break;

	case '3':
		if (*flags & IP6T_ROUTE_OPT_GW)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --gw twice");

		if (!inet_pton(AF_INET6, optarg, (struct in6_addr*)&route_info->gw)) {
			xtables_error(PARAMETER_PROBLEM,
				   "Invalid IPv6 address %s",
				   optarg);
		}

		*flags |= IP6T_ROUTE_OPT_GW;
		break;

	case '4':
		if (*flags & IP6T_ROUTE_OPT_CONTINUE)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --continue twice");
		if (*flags & IP6T_ROUTE_OPT_TEE)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --continue AND --tee");

		route_info->flags |= IP6T_ROUTE_CONTINUE;
		*flags |= IP6T_ROUTE_OPT_CONTINUE;

		break;

	case '5':
		if (*flags & IP6T_ROUTE_OPT_TEE)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --tee twice");
		if (*flags & IP6T_ROUTE_OPT_CONTINUE)
			xtables_error(PARAMETER_PROBLEM,
				   "Can't specify --tee AND --continue");

		route_info->flags |= IP6T_ROUTE_TEE;
		*flags |= IP6T_ROUTE_OPT_TEE;

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
		           "ROUTE target: oif or gw option required");
}


/* Prints out the targinfo. */
static void
print(const void *ip,
      const struct xt_entry_target *target,
      int numeric)
{
	const struct ip6t_route_target_info *route_info
		= (const struct ip6t_route_target_info *)target->data;

	printf("ROUTE ");

	if (route_info->oif[0])
		printf("oif:%s ", route_info->oif);

	if (route_info->gw[0] 
	    || route_info->gw[1] 
	    || route_info->gw[2] 
	    || route_info->gw[3]) {
		char address[INET6_ADDRSTRLEN];
		printf("gw:%s ", inet_ntop(AF_INET6, route_info->gw, address, INET6_ADDRSTRLEN));
	}

	if (route_info->flags & IP6T_ROUTE_CONTINUE)
		printf("continue");

	if (route_info->flags & IP6T_ROUTE_TEE)
		printf("tee");

}


static void
save(const void *ip,
     const struct xt_entry_target *target)
{
	const struct ip6t_route_target_info *route_info
		= (const struct ip6t_route_target_info *)target->data;

	if (route_info->oif[0])
		printf("--oif %s ", route_info->oif);

	if (route_info->gw[0] 
	    || route_info->gw[1] 
	    || route_info->gw[2] 
	    || route_info->gw[3]) {
		char address[INET6_ADDRSTRLEN];
		printf("--gw %s ", inet_ntop(AF_INET6, route_info->gw, address, INET6_ADDRSTRLEN));
	}

	if (route_info->flags & IP6T_ROUTE_CONTINUE)
		printf("--continue ");

	if (route_info->flags & IP6T_ROUTE_TEE)
		printf("--tee ");
}


static struct xtables_target route = {
	.name 		= "ROUTE",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct ip6t_route_target_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ip6t_route_target_info)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts,
};

void _init(void)
{
	xtables_register_target(&route);
}
