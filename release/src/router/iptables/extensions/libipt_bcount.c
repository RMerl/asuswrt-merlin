/*

	bcount match (experimental)
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.
	
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_bcount.h>


#undef IPTABLES_SAVE


static void help(void)
{
	printf(
		"bcount match v0.00\n"
		"[!] --range min:max (upper range of 0x0FFFFFFF or more means unlimited)\n"
	);
}

static void init(struct ipt_entry_match *match, unsigned int *nfcache)
{
	struct ipt_bcount_match *info = (struct ipt_bcount_match *)match->data;
	info->min = 0xFFFFFFFF;
	info->max = 0x0FFFFFFF;
	*nfcache |= NFC_UNKNOWN;
}

static struct option opts[] = {
	{ .name = "range",	.has_arg = 1, .flag = 0, .val = '1' },
	{ .name = NULL }
};

static int parse(int c, char **argv, int invert, unsigned int *flags,
				 const struct ipt_entry *entry, unsigned int *nfcache,
				 struct ipt_entry_match **match)
{
	char *p;
	struct ipt_bcount_match *info;

	if (c != '1') return 0;

	info = (struct ipt_bcount_match *)(*match)->data;
	
	*flags = 1;
	check_inverse(optarg, &invert, &optind, 0);
	if (invert) info->invert = 1;

	info->min = strtoul(argv[optind - 1], &p, 0);
	if (*p == '+') {
		++p;
	}
	else if ((*p == '-') || (*p == ':')) {
		++p;
		if ((*p == 0) || (strcmp(p, "max") == 0)) {
			info->max = 0x0FFFFFFF;
		}
		else {
			info->max = strtoul(p, &p, 0);
			if (info->max > 0x0FFFFFFF) info->max = 0x0FFFFFFF;
		}
	}

	if ((*p != 0) || (info->min > info->max))
		exit_error(PARAMETER_PROBLEM, "Invalid range");
	return 1;
}

static void final_check(unsigned int flags)
{
	if (!flags) {
		exit_error(PARAMETER_PROBLEM, "Invalid range");
	}
}

static void print_match(const struct ipt_bcount_match *info)
{
	if (info->min != 0xFFFFFFFF) {
		if (info->invert) printf("! ");
		printf("--range %u", info->min);
		if (info->max == 0x0FFFFFFF) printf("+ ");
			else printf(":%u ", info->max);
	}
}

static void print(const struct ipt_ip *ip, const struct ipt_entry_match *match, int numeric)
{
	printf("bcount ");
	print_match((const struct ipt_bcount_match *)match->data);
}

static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
#ifdef IPTABLES_SAVE
	print_match((const struct ipt_bcount_match *)match->data);
#endif
}


static struct iptables_match bcount_match = {
	.name          = "bcount",
	.version       = IPTABLES_VERSION,
	.size          = IPT_ALIGN(sizeof(struct ipt_bcount_match)),
	.userspacesize = IPT_ALIGN(sizeof(struct ipt_bcount_match)),
	.help          = &help,
	.init          = &init,
	.parse         = &parse,
	.final_check   = &final_check,
	.print         = &print,
	.save          = &save,
	.extra_opts    = opts
};

void _init(void)
{
	register_match(&bcount_match);
}
