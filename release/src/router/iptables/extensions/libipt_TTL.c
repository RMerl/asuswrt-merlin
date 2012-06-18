/* Shared library add-on to iptables for the TTL target
 * (C) 2000 by Harald Welte <laforge@gnumonks.org>
 *
 * $Id: libipt_TTL.c 3507 2004-12-28 13:11:59Z /C=DE/ST=Berlin/L=Berlin/O=Netfilter Project/OU=Development/CN=rusty/emailAddress=rusty@netfilter.org $
 *
 * This program is distributed under the terms of GNU GPL
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_TTL.h>

#define IPT_TTL_USED	1

static void init(struct ipt_entry_target *t, unsigned int *nfcache) 
{
}

static void help(void) 
{
	printf(
"TTL target v%s options\n"
"  --ttl-set value		Set TTL to <value 0-255>\n"
"  --ttl-dec value		Decrement TTL by <value 1-255>\n"
"  --ttl-inc value		Increment TTL by <value 1-255>\n"
, IPTABLES_VERSION);
}

static int parse(int c, char **argv, int invert, unsigned int *flags,
		const struct ipt_entry *entry,
		struct ipt_entry_target **target)
{
	struct ipt_TTL_info *info = (struct ipt_TTL_info *) (*target)->data;
	unsigned int value;

	if (*flags & IPT_TTL_USED) {
		exit_error(PARAMETER_PROBLEM, 
				"Can't specify TTL option twice");
	}

	if (!optarg) 
		exit_error(PARAMETER_PROBLEM, 
				"TTL: You must specify a value");

	if (check_inverse(optarg, &invert, NULL, 0))
		exit_error(PARAMETER_PROBLEM,
				"TTL: unexpected `!'");
	
	if (string_to_number(optarg, 0, 255, &value) == -1)
		exit_error(PARAMETER_PROBLEM,
		           "TTL: Expected value between 0 and 255");

	switch (c) {

		case '1':
			info->mode = IPT_TTL_SET;
			break;

		case '2':
			if (value == 0) {
				exit_error(PARAMETER_PROBLEM,
					"TTL: decreasing by 0?");
			}

			info->mode = IPT_TTL_DEC;
			break;

		case '3':
			if (value == 0) {
				exit_error(PARAMETER_PROBLEM,
					"TTL: increasing by 0?");
			}

			info->mode = IPT_TTL_INC;
			break;

		default:
			return 0;

	}
	
	info->ttl = value;
	*flags |= IPT_TTL_USED;

	return 1;
}

static void final_check(unsigned int flags)
{
	if (!(flags & IPT_TTL_USED))
		exit_error(PARAMETER_PROBLEM,
				"TTL: You must specify an action");
}

static void save(const struct ipt_ip *ip,
		const struct ipt_entry_target *target)
{
	const struct ipt_TTL_info *info = 
		(struct ipt_TTL_info *) target->data;

	switch (info->mode) {
		case IPT_TTL_SET:
			printf("--ttl-set ");
			break;
		case IPT_TTL_DEC:
			printf("--ttl-dec ");
			break;

		case IPT_TTL_INC:
			printf("--ttl-inc ");
			break;
	}
	printf("%u ", info->ttl);
}

static void print(const struct ipt_ip *ip,
		const struct ipt_entry_target *target, int numeric)
{
	const struct ipt_TTL_info *info =
		(struct ipt_TTL_info *) target->data;

	printf("TTL ");
	switch (info->mode) {
		case IPT_TTL_SET:
			printf("set to ");
			break;
		case IPT_TTL_DEC:
			printf("decrement by ");
			break;
		case IPT_TTL_INC:
			printf("increment by ");
			break;
	}
	printf("%u ", info->ttl);
}

static struct option opts[] = {
	{ "ttl-set", 1, 0, '1' },
	{ "ttl-dec", 1, 0, '2' },
	{ "ttl-inc", 1, 0, '3' },
	{ 0 }
};

static struct iptables_target TTL = {
	.next		= NULL, 
	.name		= "TTL",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_TTL_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_TTL_info)),
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
	register_target(&TTL);
}
