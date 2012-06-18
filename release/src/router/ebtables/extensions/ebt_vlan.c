/* ebt_vlan
 * 
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 * Nick Fedchik <nick@fedchik.org.ua> 
 *
 * June, 2002
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include "../include/ebtables_u.h"
#include "../include/ethernetdb.h"
#include <linux/netfilter_bridge/ebt_vlan.h>
#include <linux/if_ether.h>

#define NAME_VLAN_ID    "id"
#define NAME_VLAN_PRIO  "prio"
#define NAME_VLAN_ENCAP "encap"

#define VLAN_ID    '1'
#define VLAN_PRIO  '2'
#define VLAN_ENCAP '3'

static struct option opts[] = {
	{"vlan-id"   , required_argument, NULL, VLAN_ID},
	{"vlan-prio" , required_argument, NULL, VLAN_PRIO},
	{"vlan-encap", required_argument, NULL, VLAN_ENCAP},
	{ 0 }
};

/*
 * option inverse flags definition 
 */
#define OPT_VLAN_ID     0x01
#define OPT_VLAN_PRIO   0x02
#define OPT_VLAN_ENCAP  0x04
#define OPT_VLAN_FLAGS	(OPT_VLAN_ID | OPT_VLAN_PRIO | OPT_VLAN_ENCAP)

struct ethertypeent *ethent;

static void print_help()
{
	printf(
"vlan options:\n"
"--vlan-id [!] id       : vlan-tagged frame identifier, 0,1-4096 (integer)\n"
"--vlan-prio [!] prio   : Priority-tagged frame's user priority, 0-7 (integer)\n"
"--vlan-encap [!] encap : Encapsulated frame protocol (hexadecimal or name)\n");
}

static void init(struct ebt_entry_match *match)
{
	struct ebt_vlan_info *vlaninfo = (struct ebt_vlan_info *) match->data;
	vlaninfo->invflags = 0;
	vlaninfo->bitmask = 0;
}


static int parse(int c, char **argv, int argc, const struct ebt_u_entry *entry,
   unsigned int *flags, struct ebt_entry_match **match)
{
	struct ebt_vlan_info *vlaninfo = (struct ebt_vlan_info *) (*match)->data;
	char *end;
	struct ebt_vlan_info local;

	switch (c) {
	case VLAN_ID:
		ebt_check_option2(flags, OPT_VLAN_ID);
		if (ebt_check_inverse2(optarg))
			vlaninfo->invflags |= EBT_VLAN_ID;
		local.id = strtoul(optarg, &end, 10);
		if (local.id > 4094 || *end != '\0')
			ebt_print_error2("Invalid --vlan-id range ('%s')", optarg);
		vlaninfo->id = local.id;
		vlaninfo->bitmask |= EBT_VLAN_ID;
		break;
	case VLAN_PRIO:
		ebt_check_option2(flags, OPT_VLAN_PRIO);
		if (ebt_check_inverse2(optarg))
			vlaninfo->invflags |= EBT_VLAN_PRIO;
		local.prio = strtoul(optarg, &end, 10);
		if (local.prio >= 8 || *end != '\0')
			ebt_print_error2("Invalid --vlan-prio range ('%s')", optarg);
		vlaninfo->prio = local.prio;
		vlaninfo->bitmask |= EBT_VLAN_PRIO;
		break;
	case VLAN_ENCAP:
		ebt_check_option2(flags, OPT_VLAN_ENCAP);
		if (ebt_check_inverse2(optarg))
			vlaninfo->invflags |= EBT_VLAN_ENCAP;
		local.encap = strtoul(optarg, &end, 16);
		if (*end != '\0') {
			ethent = getethertypebyname(optarg);
			if (ethent == NULL)
				ebt_print_error("Unknown --vlan-encap value ('%s')", optarg);
			local.encap = ethent->e_ethertype;
		}
		if (local.encap < ETH_ZLEN)
			ebt_print_error2("Invalid --vlan-encap range ('%s')", optarg);
		vlaninfo->encap = htons(local.encap);
		vlaninfo->bitmask |= EBT_VLAN_ENCAP;
		break;
	default:
		return 0;

	}
	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match,
   const char *name, unsigned int hookmask, unsigned int time)
{
	if (entry->ethproto != ETH_P_8021Q || entry->invflags & EBT_IPROTO)
		ebt_print_error("For vlan filtering the protocol must be specified as 802_1Q");

	/* Check if specified vlan-id=0 (priority-tagged frame condition) 
	 * when vlan-prio was specified. */
	/* I see no reason why a user should be prohibited to match on a perhaps impossible situation <BDS>
	if (vlaninfo->bitmask & EBT_VLAN_PRIO &&
	    vlaninfo->id && vlaninfo->bitmask & EBT_VLAN_ID)
		ebt_print_error("When setting --vlan-prio the specified --vlan-id must be 0");*/
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match)
{
	struct ebt_vlan_info *vlaninfo = (struct ebt_vlan_info *) match->data;

	if (vlaninfo->bitmask & EBT_VLAN_ID) {
		printf("--vlan-id %s%d ", (vlaninfo->invflags & EBT_VLAN_ID) ? "! " : "", vlaninfo->id);
	}
	if (vlaninfo->bitmask & EBT_VLAN_PRIO) {
		printf("--vlan-prio %s%d ", (vlaninfo->invflags & EBT_VLAN_PRIO) ? "! " : "", vlaninfo->prio);
	}
	if (vlaninfo->bitmask & EBT_VLAN_ENCAP) {
		printf("--vlan-encap %s", (vlaninfo->invflags & EBT_VLAN_ENCAP) ? "! " : "");
		ethent = getethertypebynumber(ntohs(vlaninfo->encap));
		if (ethent != NULL) {
			printf("%s ", ethent->e_name);
		} else {
			printf("%4.4X ", ntohs(vlaninfo->encap));
		}
	}
}

static int compare(const struct ebt_entry_match *vlan1,
   const struct ebt_entry_match *vlan2)
{
	struct ebt_vlan_info *vlaninfo1 = (struct ebt_vlan_info *) vlan1->data;
	struct ebt_vlan_info *vlaninfo2 = (struct ebt_vlan_info *) vlan2->data;

	if (vlaninfo1->bitmask != vlaninfo2->bitmask)
		return 0;
	if (vlaninfo1->invflags != vlaninfo2->invflags)
		return 0;
	if (vlaninfo1->bitmask & EBT_VLAN_ID &&
	    vlaninfo1->id != vlaninfo2->id)
		return 0;
	if (vlaninfo1->bitmask & EBT_VLAN_PRIO &&
	    vlaninfo1->prio != vlaninfo2->prio)
		return 0;
	if (vlaninfo1->bitmask & EBT_VLAN_ENCAP &&
	    vlaninfo1->encap != vlaninfo2->encap)
		return 0;
	return 1;
}

static struct ebt_u_match vlan_match = {
	.name		= "vlan",
	.size		= sizeof(struct ebt_vlan_info),
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
	ebt_register_match(&vlan_match);
}
