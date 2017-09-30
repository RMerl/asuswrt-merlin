/*
 * IPv6 Hop Limit matching module
 * Maciej Soltysiak <solt@dns.toxicfilms.tv>
 * Based on HW's ttl match
 * This program is released under the terms of GNU GPL
 * Cleanups by Stephane Ouellette <ouellettes@videotron.ca>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ip6tables.h>

#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_hl.h>

static void help(void) 
{
	printf(
"HL match v%s options:\n"
"  --hl-eq [!] value	Match hop limit value\n"
"  --hl-lt value	Match HL < value\n"
"  --hl-gt value	Match HL > value\n"
, IPTABLES_VERSION);
}

static int parse(int c, char **argv, int invert, unsigned int *flags,
		const struct ip6t_entry *entry, unsigned int *nfcache,
		struct ip6t_entry_match **match)
{
	struct ip6t_hl_info *info = (struct ip6t_hl_info *) (*match)->data;
	u_int8_t value;

	check_inverse(optarg, &invert, &optind, 0);
	value = atoi(argv[optind-1]);

	if (*flags) 
		exit_error(PARAMETER_PROBLEM, 
				"Can't specify HL option twice");

	if (!optarg)
		exit_error(PARAMETER_PROBLEM,
				"hl: You must specify a value");
	switch (c) {
		case '2':
			if (invert)
				info->mode = IP6T_HL_NE;
			else
				info->mode = IP6T_HL_EQ;

			/* is 0 allowed? */
			info->hop_limit = value;
			*flags = 1;

			break;
		case '3':
			if (invert) 
				exit_error(PARAMETER_PROBLEM,
						"hl: unexpected `!'");

			info->mode = IP6T_HL_LT;
			info->hop_limit = value;
			*flags = 1;

			break;
		case '4':
			if (invert)
				exit_error(PARAMETER_PROBLEM,
						"hl: unexpected `!'");

			info->mode = IP6T_HL_GT;
			info->hop_limit = value;
			*flags = 1;

			break;
		default:
			return 0;
	}

	return 1;
}

static void final_check(unsigned int flags)
{
	if (!flags) 
		exit_error(PARAMETER_PROBLEM,
			"HL match: You must specify one of "
			"`--hl-eq', `--hl-lt', `--hl-gt'");
}

static void print(const struct ip6t_ip6 *ip, 
		const struct ip6t_entry_match *match,
		int numeric)
{
	static const char *op[] = {
		[IP6T_HL_EQ] = "==",
		[IP6T_HL_NE] = "!=",
		[IP6T_HL_LT] = "<",
		[IP6T_HL_GT] = ">" };

	const struct ip6t_hl_info *info = 
		(struct ip6t_hl_info *) match->data;

	printf("HL match HL %s %u ", op[info->mode], info->hop_limit);
}

static void save(const struct ip6t_ip6 *ip, 
		const struct ip6t_entry_match *match)
{
	static const char *op[] = {
		[IP6T_HL_EQ] = "eq",
		[IP6T_HL_NE] = "eq !",
		[IP6T_HL_LT] = "lt",
		[IP6T_HL_GT] = "gt" };

	const struct ip6t_hl_info *info =
		(struct ip6t_hl_info *) match->data;

	printf("--hl-%s %u ", op[info->mode], info->hop_limit);
}

static struct option opts[] = {
	{ .name = "hl",    .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = "hl-eq", .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = "hl-lt", .has_arg = 1, .flag = 0, .val = '3' },
	{ .name = "hl-gt", .has_arg = 1, .flag = 0, .val = '4' },
	{ 0 }
};

static
struct ip6tables_match hl = {
	.name          = "hl",
	.version       = IPTABLES_VERSION,
	.size          = IP6T_ALIGN(sizeof(struct ip6t_hl_info)),
	.userspacesize = IP6T_ALIGN(sizeof(struct ip6t_hl_info)),
	.help          = &help,
	.parse         = &parse,
	.final_check   = &final_check,
	.print         = &print,
	.save          = &save,
	.extra_opts    = opts
};


void _init(void) 
{
	register_match6(&hl);
}
