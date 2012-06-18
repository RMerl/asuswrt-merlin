/* Shared library add-on to iptables for ECN matching
 *
 * (C) 2002 by Harald Welte <laforge@gnumonks.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libipt_ecn.c borrowed heavily from libipt_dscp.c
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_ecn.h>

static void help(void) 
{
	printf(
"ECN match v%s options\n"
"[!] --ecn-tcp-cwr 		Match CWR bit of TCP header\n"
"[!] --ecn-tcp-ece		Match ECE bit of TCP header\n"
"[!] --ecn-ip-ect [0..3]	Match ECN codepoint in IPv4 header\n",
	IPTABLES_VERSION);
}

static struct option opts[] = {
	{ .name = "ecn-tcp-cwr", .has_arg = 0, .flag = 0, .val = 'F' },
	{ .name = "ecn-tcp-ece", .has_arg = 0, .flag = 0, .val = 'G' },
	{ .name = "ecn-ip-ect",  .has_arg = 1, .flag = 0, .val = 'H' },
	{ .name = 0 }
};

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	unsigned int result;
	struct ipt_ecn_info *einfo
		= (struct ipt_ecn_info *)(*match)->data;

	switch (c) {
	case 'F':
		if (*flags & IPT_ECN_OP_MATCH_CWR)
			exit_error(PARAMETER_PROBLEM,
			           "ECN match: can only use parameter ONCE!");
		check_inverse(optarg, &invert, &optind, 0);
		einfo->operation |= IPT_ECN_OP_MATCH_CWR;
		if (invert)
			einfo->invert |= IPT_ECN_OP_MATCH_CWR;
		*flags |= IPT_ECN_OP_MATCH_CWR;
		break;

	case 'G':
		if (*flags & IPT_ECN_OP_MATCH_ECE)
			exit_error(PARAMETER_PROBLEM,
				   "ECN match: can only use parameter ONCE!");
		check_inverse(optarg, &invert, &optind, 0);
		einfo->operation |= IPT_ECN_OP_MATCH_ECE;
		if (invert)
			einfo->invert |= IPT_ECN_OP_MATCH_ECE;
		*flags |= IPT_ECN_OP_MATCH_ECE;
		break;

	case 'H':
		if (*flags & IPT_ECN_OP_MATCH_IP)
			exit_error(PARAMETER_PROBLEM,
				   "ECN match: can only use parameter ONCE!");
		check_inverse(optarg, &invert, &optind, 0);
		if (invert)
			einfo->invert |= IPT_ECN_OP_MATCH_IP;
		*flags |= IPT_ECN_OP_MATCH_IP;
		einfo->operation |= IPT_ECN_OP_MATCH_IP;
		if (string_to_number(optarg, 0, 3, &result))
			exit_error(PARAMETER_PROBLEM,
				   "ECN match: Value out of range");
		einfo->ip_ect = result;
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
		           "ECN match: some option required");
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	const struct ipt_ecn_info *einfo =
		(const struct ipt_ecn_info *)match->data;

	printf("ECN match ");

	if (einfo->operation & IPT_ECN_OP_MATCH_ECE) {
		if (einfo->invert & IPT_ECN_OP_MATCH_ECE)
			fputc('!', stdout);
		printf("ECE ");
	}

	if (einfo->operation & IPT_ECN_OP_MATCH_CWR) {
		if (einfo->invert & IPT_ECN_OP_MATCH_CWR)
			fputc('!', stdout);
		printf("CWR ");
	}

	if (einfo->operation & IPT_ECN_OP_MATCH_IP) {
		if (einfo->invert & IPT_ECN_OP_MATCH_IP)
			fputc('!', stdout);
		printf("ECT=%d ", einfo->ip_ect);
	}
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_ecn_info *einfo =
		(const struct ipt_ecn_info *)match->data;
	
	if (einfo->operation & IPT_ECN_OP_MATCH_ECE) {
		if (einfo->invert & IPT_ECN_OP_MATCH_ECE)
			printf("! ");
		printf("--ecn-tcp-ece ");
	}

	if (einfo->operation & IPT_ECN_OP_MATCH_CWR) {
		if (einfo->invert & IPT_ECN_OP_MATCH_CWR)
			printf("! ");
		printf("--ecn-tcp-cwr ");
	}

	if (einfo->operation & IPT_ECN_OP_MATCH_IP) {
		if (einfo->invert & IPT_ECN_OP_MATCH_IP)
			printf("! ");
		printf("--ecn-ip-ect %d", einfo->ip_ect);
	}
}

static
struct iptables_match ecn
= { .name          = "ecn",
    .version       = IPTABLES_VERSION,
    .size          = IPT_ALIGN(sizeof(struct ipt_ecn_info)),
    .userspacesize = IPT_ALIGN(sizeof(struct ipt_ecn_info)),
    .help          = &help,
    .parse         = &parse,
    .final_check   = &final_check,
    .print         = &print,
    .save          = &save,
    .extra_opts    = opts
};

void _init(void)
{
	register_match(&ecn);
}
