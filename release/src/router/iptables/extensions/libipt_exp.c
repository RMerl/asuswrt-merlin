/*

	Experimental Netfilter Crap
	Copyright (C) 2006 Jonathan Zarate

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_exp.h>


static void help(void)
{
	printf(
		"exp match v0.00 options:\n"
		"  how should I know?!\n");
}

static void init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	*nfcache |= NFC_UNKNOWN;
}

static struct option opts[] = {
	{ .name = "foo",  .has_arg = 0, .flag = 0, .val = '1' },
	{ .name = 0 }
};

static int parse(int c, char **argv, int invert, unsigned int *flags,
				 const struct ipt_entry *entry, unsigned int *nfcache,
				 struct ipt_entry_match **match)
{
	struct ipt_exp_info *info;

	if ((c < '1') || (c > '1')) return 0;

	// reserved
	
	info = (struct ipt_exp_info *)(*match)->data;
	switch (c) {
	case '1':
		info->dummy = 1;	// blah
		break;
	}
	return 1;
}

static void final_check(unsigned int flags)
{
}

static void print(const struct ipt_ip *ip, const struct ipt_entry_match *match, int numeric)
{
	printf("exp ");
}

static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
}


static struct iptables_match exp_match = {
	.name          = "exp",
	.version       = IPTABLES_VERSION,
	.size          = IPT_ALIGN(sizeof(struct ipt_exp_info)),
	.userspacesize = IPT_ALIGN(sizeof(struct ipt_exp_info)),
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
	register_match(&exp_match);
}
