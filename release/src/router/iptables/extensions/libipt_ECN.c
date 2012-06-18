/* Shared library add-on to iptables for ECN, $Version$
 *
 * (C) 2002 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libipt_ECN.c borrowed heavily from libipt_DSCP.c
 *
 * $Id: libipt_ECN.c 3507 2004-12-28 13:11:59Z /C=DE/ST=Berlin/L=Berlin/O=Netfilter Project/OU=Development/CN=rusty/emailAddress=rusty@netfilter.org $
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_ECN.h>

static void init(struct ipt_entry_target *t, unsigned int *nfcache) 
{
}

static void help(void) 
{
	printf(
"ECN target v%s options\n"
"  --ecn-tcp-remove		Remove all ECN bits from TCP header\n",
		IPTABLES_VERSION);
}

#if 0
"ECN target v%s EXPERIMENTAL options (use with extreme care!)\n"
"  --ecn-ip-ect			Set the IPv4 ECT codepoint (0 to 3)\n"
"  --ecn-tcp-cwr		Set the IPv4 CWR bit (0 or 1)\n"
"  --ecn-tcp-ece		Set the IPv4 ECE bit (0 or 1)\n",
#endif


static struct option opts[] = {
	{ "ecn-tcp-remove", 0, 0, 'F' },
	{ "ecn-tcp-cwr", 1, 0, 'G' },
	{ "ecn-tcp-ece", 1, 0, 'H' },
	{ "ecn-ip-ect", 1, 0, '9' },
	{ 0 }
};

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	unsigned int result;
	struct ipt_ECN_info *einfo
		= (struct ipt_ECN_info *)(*target)->data;

	switch (c) {
	case 'F':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			        "ECN target: Only use --ecn-tcp-remove ONCE!");
		einfo->operation = IPT_ECN_OP_SET_ECE | IPT_ECN_OP_SET_CWR;
		einfo->proto.tcp.ece = 0;
		einfo->proto.tcp.cwr = 0;
		*flags = 1;
		break;
	case 'G':
		if (*flags & IPT_ECN_OP_SET_CWR)
			exit_error(PARAMETER_PROBLEM,
				"ECN target: Only use --ecn-tcp-cwr ONCE!");
		if (string_to_number(optarg, 0, 1, &result))
			exit_error(PARAMETER_PROBLEM,
				   "ECN target: Value out of range");
		einfo->operation |= IPT_ECN_OP_SET_CWR;
		einfo->proto.tcp.cwr = result;
		*flags |= IPT_ECN_OP_SET_CWR;
		break;
	case 'H':
		if (*flags & IPT_ECN_OP_SET_ECE)
			exit_error(PARAMETER_PROBLEM,
				"ECN target: Only use --ecn-tcp-ece ONCE!");
		if (string_to_number(optarg, 0, 1, &result))
			exit_error(PARAMETER_PROBLEM,
				   "ECN target: Value out of range");
		einfo->operation |= IPT_ECN_OP_SET_ECE;
		einfo->proto.tcp.ece = result;
		*flags |= IPT_ECN_OP_SET_ECE;
		break;
	case '9':
		if (*flags & IPT_ECN_OP_SET_IP)
			exit_error(PARAMETER_PROBLEM,
				"ECN target: Only use --ecn-ip-ect ONCE!");
		if (string_to_number(optarg, 0, 3, &result))
			exit_error(PARAMETER_PROBLEM,
				   "ECN target: Value out of range");
		einfo->operation |= IPT_ECN_OP_SET_IP;
		einfo->ip_ect = result;
		*flags |= IPT_ECN_OP_SET_IP;
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
		           "ECN target: Parameter --ecn-tcp-remove is required");
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	const struct ipt_ECN_info *einfo =
		(const struct ipt_ECN_info *)target->data;

	printf("ECN ");

	if (einfo->operation == (IPT_ECN_OP_SET_ECE|IPT_ECN_OP_SET_CWR)
	    && einfo->proto.tcp.ece == 0
	    && einfo->proto.tcp.cwr == 0)
		printf("TCP remove ");
	else {
		if (einfo->operation & IPT_ECN_OP_SET_ECE)
			printf("ECE=%u ", einfo->proto.tcp.ece);

		if (einfo->operation & IPT_ECN_OP_SET_CWR)
			printf("CWR=%u ", einfo->proto.tcp.cwr);

		if (einfo->operation & IPT_ECN_OP_SET_IP)
			printf("ECT codepoint=%u ", einfo->ip_ect);
	}
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	const struct ipt_ECN_info *einfo =
		(const struct ipt_ECN_info *)target->data;

	if (einfo->operation == (IPT_ECN_OP_SET_ECE|IPT_ECN_OP_SET_CWR)
	    && einfo->proto.tcp.ece == 0
	    && einfo->proto.tcp.cwr == 0)
		printf("--ecn-tcp-remove ");
	else {

		if (einfo->operation & IPT_ECN_OP_SET_ECE)
			printf("--ecn-tcp-ece %d ", einfo->proto.tcp.ece);

		if (einfo->operation & IPT_ECN_OP_SET_CWR)
			printf("--ecn-tcp-cwr %d ", einfo->proto.tcp.cwr);

		if (einfo->operation & IPT_ECN_OP_SET_IP)
			printf("--ecn-ip-ect %d ", einfo->ip_ect);
	}
}

static
struct iptables_target ecn = { 
	.next		= NULL,
	.name		= "ECN",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_ECN_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_ECN_info)),
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
	register_target(&ecn);
}
