/* ebt_pkttype
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 *
 * April, 2003
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <netdb.h>
#include "../include/ebtables_u.h"
#include <linux/if_packet.h>
#include <linux/netfilter_bridge/ebt_pkttype.h>

char *classes[] =
{
	"host",
	"broadcast",
	"multicast",
	"otherhost",
	"outgoing",
	"loopback",
	"fastroute",
	"\0"
};

static struct option opts[] =
{
	{ "pkttype-type"        , required_argument, 0, '1' },
	{ 0 }
};

static void print_help()
{
	printf(
"pkttype options:\n"
"--pkttype-type    [!] type: class the packet belongs to\n"
"Possible values: broadcast, multicast, host, otherhost, or any other byte value (which would be pretty useless).\n");
}

static void init(struct ebt_entry_match *match)
{
	struct ebt_pkttype_info *pt = (struct ebt_pkttype_info *)match->data;

	pt->invert = 0;
}

static int parse(int c, char **argv, int argc, const struct ebt_u_entry *entry,
   unsigned int *flags, struct ebt_entry_match **match)
{
	struct ebt_pkttype_info *ptinfo = (struct ebt_pkttype_info *)(*match)->data;
	char *end;
	long int i;

	switch (c) {
	case '1':
		ebt_check_option2(flags, 1);
		if (ebt_check_inverse2(optarg))
			ptinfo->invert = 1;
		i = strtol(optarg, &end, 16);
		if (*end != '\0') {
			int j = 0;
			i = -1;
			while (classes[j][0])
				if (!strcasecmp(optarg, classes[j++])) {
					i = j - 1;
					break;
				}
		}
		if (i < 0 || i > 255)
			ebt_print_error2("Problem with specified pkttype class");
		ptinfo->pkt_type = (uint8_t)i;
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
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match)
{
	struct ebt_pkttype_info *pt = (struct ebt_pkttype_info *)match->data;
	int i = 0;

	printf("--pkttype-type %s", pt->invert ? "! " : "");
	while (classes[i++][0]);
	if (pt->pkt_type < i - 1)
		printf("%s ", classes[pt->pkt_type]);
	else
		printf("%d ", pt->pkt_type);
}

static int compare(const struct ebt_entry_match *m1,
   const struct ebt_entry_match *m2)
{
	struct ebt_pkttype_info *pt1 = (struct ebt_pkttype_info *)m1->data;
	struct ebt_pkttype_info *pt2 = (struct ebt_pkttype_info *)m2->data;

	if (pt1->invert != pt2->invert ||
	    pt1->pkt_type != pt2->pkt_type)
		return 0;
	return 1;
}

static struct ebt_u_match pkttype_match =
{
	.name		= "pkttype",
	.size		= sizeof(struct ebt_pkttype_info),
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
	ebt_register_match(&pkttype_match);
}
