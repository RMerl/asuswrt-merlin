/* Shared library add-on to iptables for the XOR target
 * (C) 2000 by Tim Vandermeersch <Tim.Vandermeersch@pandora.be>
 * Based on libipt_TTL.c
 *
 * Version 1.0
 *
 * This program is distributed under the terms of GNU GPL
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_XOR.h>

#define	IPT_KEY_SET		1
#define IPT_BLOCKSIZE_SET	2

static void init(struct ipt_entry_target *t, unsigned int *nfcache) 
{
}

static void help(void) 
{
	printf(
		"XOR target v%s options\n"
		"  --key string	          Set key to \"string\"\n"
		"  --block-size		  Set block size\n",
		IPTABLES_VERSION);
}

static int parse(int c, char **argv, int invert, unsigned int *flags,
		const struct ipt_entry *entry, 
		struct ipt_entry_target **target)
{
	struct ipt_XOR_info *info = (struct ipt_XOR_info *) (*target)->data;
	
	if (!optarg)
		exit_error(PARAMETER_PROBLEM, "XOR: too few arguments");
	
	if (check_inverse(optarg, &invert, NULL, 0))
		exit_error(PARAMETER_PROBLEM, "XOR: unexpected '!'");

	switch (c) {	
		case '1':
			strncpy(info->key, optarg, 30);
			info->key[29] = '\0';
			*flags |= IPT_KEY_SET;
			break;
		case '2':
			info->block_size = atoi(optarg);
			*flags |= IPT_BLOCKSIZE_SET;
			break;
		default:
			return 0;
	}
	
	return 1;
}

static void final_check(unsigned int flags)
{
	if (!(flags & IPT_KEY_SET))
		exit_error(PARAMETER_PROBLEM, "XOR: You must specify a key");
	if (!(flags & IPT_BLOCKSIZE_SET))
		exit_error(PARAMETER_PROBLEM, "XOR: You must specify a block-size");
}

static void save (const struct ipt_ip *ip,
		const struct ipt_entry_target *target)
{
	const struct ipt_XOR_info *info = (struct ipt_XOR_info *) target->data;

	printf("--key %s ", info->key);
	printf("--block-size %u ", info->block_size);
}

static void print (const struct ipt_ip *ip,
	const struct ipt_entry_target *target, int numeric)
{
	const struct ipt_XOR_info *info = (struct ipt_XOR_info *) target->data;

	printf("key: %s ", info->key);
	printf("block-size: %u ", info->block_size);
}

static struct option opts[] = {
	{ "key", 1, 0, '1' },
	{ "block-size", 1, 0, '2' },
	{ 0 }
};

static struct iptables_target XOR = {
	.next		= NULL, 
	.name		= "XOR",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_XOR_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_XOR_info)),
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
	register_target(&XOR);
}
