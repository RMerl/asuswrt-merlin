/*

	macsave match (experimental)
	Copyright (C) 2006 Jonathan Zarate

	Licensed under GNU GPL v2 or later.

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_macsave.h>


static void help(void)
{
	printf(
		"macsave match (experimental) v0.01\n"
		"Copyright (C) 2006 Jonathan Zarate\n"
		"Options:\n"
		"[!] --mac <mac address>\n");
}

static void init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	*nfcache |= NFC_UNKNOWN;
}

static struct option opts[] = {
	{ .name = "mac",  .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = 0 }
};

static int parse(int c, char **argv, int invert, unsigned int *flags,
				 const struct ipt_entry *entry, unsigned int *nfcache,
				 struct ipt_entry_match **match)
{
	struct ipt_macsave_match_info *info;
	char *mac;
	long n;
	int i;

	if (c != '1') return 0;

	if (*flags) exit_error(PARAMETER_PROBLEM, "Multiple MACs are not supported");
	*flags = 1;

	info = (struct ipt_macsave_match_info *)(*match)->data;

	check_inverse(optarg, &invert, &optind, 0);
	if (invert) info->invert = 1;

	mac = argv[optind - 1];
	i = 0;
	while (*mac) {
		n = strtol(mac, &mac, 16);
		if ((n < 0) || (n > 255)) break;
		info->mac[i++] = n;
		if (i == 6) break;
		if ((*mac != ':') && (*mac != '-')) exit_error(PARAMETER_PROBLEM, "Invalid MAC address");
		++mac;
	}
	if ((i != 6) || (*mac != 0)) exit_error(PARAMETER_PROBLEM, "Invalid MAC address");
	return 1;
}

static void final_check(unsigned int flags)
{
	if (flags != 1) exit_error(PARAMETER_PROBLEM, "--mac expected");
}

static void print_match(const struct ipt_macsave_match_info *info)
{
	printf("--mac %02X:%02X:%02X:%02X:%02X:%02X ", 
		info->mac[0], info->mac[1], info->mac[2],
		info->mac[3], info->mac[4], info->mac[5]);
}

static void print(const struct ipt_ip *ip, const struct ipt_entry_match *match, int numeric)
{
	printf("macsave ");
	print_match((const struct ipt_macsave_match_info *)match->data);
}

static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	print_match((const struct ipt_macsave_match_info *)match->data);
}


static struct iptables_match macsave_match = {
	.name          = "macsave",
	.version       = IPTABLES_VERSION,
	.size          = IPT_ALIGN(sizeof(struct ipt_macsave_match_info)),
	.userspacesize = IPT_ALIGN(sizeof(struct ipt_macsave_match_info)),
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
	register_match(&macsave_match);
}
