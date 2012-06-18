/* 802_3
 *
 * Author:
 * Chris Vitale <csv@bluetail.com>
 *
 * May 2003
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include "../include/ethernetdb.h"
#include <linux/netfilter_bridge/ebt_802_3.h>

#define _802_3_SAP '1'
#define _802_3_TYPE '2'

static struct option opts[] =
{
	{ "802_3-sap"   , required_argument, 0, _802_3_SAP },
	{ "802_3-type"  , required_argument, 0, _802_3_TYPE },
	{ 0 }
};

static void print_help()
{
	printf(
"802_3 options:\n"
"--802_3-sap [!] protocol       : 802.3 DSAP/SSAP- 1 byte value (hex)\n"
"  DSAP and SSAP are always the same.  One SAP applies to both fields\n"
"--802_3-type [!] protocol      : 802.3 SNAP Type- 2 byte value (hex)\n"
"  Type implies SAP value 0xaa\n");
}

static void init(struct ebt_entry_match *match)
{
	struct ebt_802_3_info *info = (struct ebt_802_3_info *)match->data;

	info->invflags = 0;
	info->bitmask = 0;
}

static int parse(int c, char **argv, int argc, const struct ebt_u_entry *entry,
   unsigned int *flags, struct ebt_entry_match **match)
{
	struct ebt_802_3_info *info = (struct ebt_802_3_info *) (*match)->data;
	unsigned int i;
	char *end;

	switch (c) {
		case _802_3_SAP:
			ebt_check_option2(flags, _802_3_SAP);
			if (ebt_check_inverse2(optarg))
				info->invflags |= EBT_802_3_SAP;
			i = strtoul(optarg, &end, 16);
			if (i > 255 || *end != '\0') 
				ebt_print_error2("Problem with specified "
						"sap hex value, %x",i);
			info->sap = i; /* one byte, so no byte order worries */
			info->bitmask |= EBT_802_3_SAP;
			break;
		case _802_3_TYPE:
			ebt_check_option2(flags, _802_3_TYPE);
			if (ebt_check_inverse2(optarg))
				info->invflags |= EBT_802_3_TYPE;
			i = strtoul(optarg, &end, 16);
			if (i > 65535 || *end != '\0') {
				ebt_print_error2("Problem with the specified "
						"type hex value, %x",i);
			}
			info->type = htons(i);
			info->bitmask |= EBT_802_3_TYPE;
			break;
		default:
			return 0;
	}
	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match, const char *name,
   unsigned int hookmask, unsigned int time)
{
	if (!(entry->bitmask & EBT_802_3))
		ebt_print_error("For 802.3 DSAP/SSAP filtering the protocol "
				"must be LENGTH");
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match)
{
	struct ebt_802_3_info *info = (struct ebt_802_3_info *)match->data;

	if (info->bitmask & EBT_802_3_SAP) {
		printf("--802_3-sap ");
		if (info->invflags & EBT_802_3_SAP)
			printf("! ");
		printf("0x%.2x ", info->sap);
	}
	if (info->bitmask & EBT_802_3_TYPE) {
		printf("--802_3-type ");
		if (info->invflags & EBT_802_3_TYPE)
			printf("! ");
		printf("0x%.4x ", ntohs(info->type));
	}
}

static int compare(const struct ebt_entry_match *m1,
   const struct ebt_entry_match *m2)
{
	struct ebt_802_3_info *info1 = (struct ebt_802_3_info *)m1->data;
	struct ebt_802_3_info *info2 = (struct ebt_802_3_info *)m2->data;

	if (info1->bitmask != info2->bitmask)
		return 0;
	if (info1->invflags != info2->invflags)
		return 0;
	if (info1->bitmask & EBT_802_3_SAP) {
		if (info1->sap != info2->sap) 
			return 0;
	}
	if (info1->bitmask & EBT_802_3_TYPE) {
		if (info1->type != info2->type)
			return 0;
	}
	return 1;
}

static struct ebt_u_match _802_3_match = 
{
	.name		= "802_3",
	.size		= sizeof(struct ebt_802_3_info),
	.help		= print_help,
	.init		= init,
	.parse		= parse,
	.final_check	= final_check,
	.print		= print,
	.compare	= compare,
	.extra_ops	= opts,
};

void _init(void)
{
	ebt_register_match(&_802_3_match);
}
