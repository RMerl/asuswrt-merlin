/* Shared library add-on to iptables to add TCPMSS target support.
 *
 * Copyright (c) 2000 Marc Boucher
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_TCPMSS.h>

struct mssinfo {
	struct ip6t_entry_target t;
	struct ip6t_tcpmss_info mss;
};

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"TCPMSS target v%s mutually-exclusive options:\n"
"  --set-mss value               explicitly set MSS option to specified value\n"
"  --clamp-mss-to-pmtu           automatically clamp MSS value to (path_MTU - 60)\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "set-mss", 1, 0, '1' },
	{ "clamp-mss-to-pmtu", 0, 0, '2' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ip6t_entry_target *t, unsigned int *nfcache)
{
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      struct ip6t_entry_target **target)
{
	struct ip6t_tcpmss_info *mssinfo
		= (struct ip6t_tcpmss_info *)(*target)->data;

	switch (c) {
		unsigned int mssval;

	case '1':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "TCPMSS target: Only one option may be specified");
		if (string_to_number(optarg, 0, 65535 - 60, &mssval) == -1)
			exit_error(PARAMETER_PROBLEM, "Bad TCPMSS value `%s'", optarg);

		mssinfo->mss = mssval;
		*flags = 1;
		break;

	case '2':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "TCPMSS target: Only one option may be specified");
		mssinfo->mss = IP6T_TCPMSS_CLAMP_PMTU;
		*flags = 1;
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
		           "TCPMSS target: At least one parameter is required");
}

/* Prints out the targinfo. */
static void
print(const struct ip6t_ip6 *ip6,
      const struct ip6t_entry_target *target,
      int numeric)
{
	const struct ip6t_tcpmss_info *mssinfo =
		(const struct ip6t_tcpmss_info *)target->data;
	if(mssinfo->mss == IP6T_TCPMSS_CLAMP_PMTU)
		printf("TCPMSS clamp to PMTU ");
	else
		printf("TCPMSS set %u ", mssinfo->mss);
}

/* Saves the union ip6t_targinfo in parsable form to stdout. */
static void
save(const struct ip6t_ip6 *ip, const struct ip6t_entry_target *target)
{
	const struct ip6t_tcpmss_info *mssinfo =
		(const struct ip6t_tcpmss_info *)target->data;

	if(mssinfo->mss == IP6T_TCPMSS_CLAMP_PMTU)
		printf("--clamp-mss-to-pmtu ");
	else
		printf("--set-mss %u ", mssinfo->mss);
}

static struct ip6tables_target mss = {
	.next		= NULL,
	.name		= "TCPMSS",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ip6t_tcpmss_info)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ip6t_tcpmss_info)),
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
	register_target6(&mss);
}
