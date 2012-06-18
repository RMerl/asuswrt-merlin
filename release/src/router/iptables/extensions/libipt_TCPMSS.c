/* Shared library add-on to iptables to add TCPMSS target support.
 *
 * Copyright (c) 2000 Marc Boucher
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_TCPMSS.h>

struct mssinfo {
	struct ipt_entry_target t;
	struct ipt_tcpmss_info mss;
};

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"TCPMSS target v%s mutually-exclusive options:\n"
"  --set-mss value               explicitly set MSS option to specified value\n"
"  --clamp-mss-to-pmtu           automatically clamp MSS value to (path_MTU - 40)\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "set-mss", 1, 0, '1' },
	{ "clamp-mss-to-pmtu", 0, 0, '2' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ipt_tcpmss_info *mssinfo
		= (struct ipt_tcpmss_info *)(*target)->data;

	switch (c) {
		unsigned int mssval;

	case '1':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "TCPMSS target: Only one option may be specified");
		if (string_to_number(optarg, 0, 65535 - 40, &mssval) == -1)
			exit_error(PARAMETER_PROBLEM, "Bad TCPMSS value `%s'", optarg);
		
		mssinfo->mss = mssval;
		*flags = 1;
		break;

	case '2':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "TCPMSS target: Only one option may be specified");
		mssinfo->mss = IPT_TCPMSS_CLAMP_PMTU;
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
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	const struct ipt_tcpmss_info *mssinfo =
		(const struct ipt_tcpmss_info *)target->data;
	if(mssinfo->mss == IPT_TCPMSS_CLAMP_PMTU)
		printf("TCPMSS clamp to PMTU ");
	else
		printf("TCPMSS set %u ", mssinfo->mss);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	const struct ipt_tcpmss_info *mssinfo =
		(const struct ipt_tcpmss_info *)target->data;

	if(mssinfo->mss == IPT_TCPMSS_CLAMP_PMTU)
		printf("--clamp-mss-to-pmtu ");
	else
		printf("--set-mss %u ", mssinfo->mss);
}

static struct iptables_target mss = {
	.next		= NULL,
	.name		= "TCPMSS",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_tcpmss_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_tcpmss_info)),
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
	register_target(&mss);
}
