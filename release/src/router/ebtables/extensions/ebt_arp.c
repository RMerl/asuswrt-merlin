/* ebt_arp
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 * Tim Gardner <timg@tpi.com>
 *
 * April, 2002
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include "../include/ethernetdb.h"
#include <linux/if_ether.h>
#include <linux/netfilter_bridge/ebt_arp.h>

#define ARP_OPCODE '1'
#define ARP_HTYPE  '2'
#define ARP_PTYPE  '3'
#define ARP_IP_S   '4'
#define ARP_IP_D   '5'
#define ARP_MAC_S  '6'
#define ARP_MAC_D  '7'
#define ARP_GRAT   '8'
static struct option opts[] =
{
	{ "arp-opcode"    , required_argument, 0, ARP_OPCODE },
	{ "arp-op"        , required_argument, 0, ARP_OPCODE },
	{ "arp-htype"     , required_argument, 0, ARP_HTYPE  },
	{ "arp-ptype"     , required_argument, 0, ARP_PTYPE  },
	{ "arp-ip-src"    , required_argument, 0, ARP_IP_S   },
	{ "arp-ip-dst"    , required_argument, 0, ARP_IP_D   },
	{ "arp-mac-src"   , required_argument, 0, ARP_MAC_S  },
	{ "arp-mac-dst"   , required_argument, 0, ARP_MAC_D  },
	{ "arp-gratuitous",       no_argument, 0, ARP_GRAT   },
	{ 0 }
};

#define NUMOPCODES 9
/* a few names */
static char *opcodes[] =
{
	"Request",
	"Reply",
	"Request_Reverse",
	"Reply_Reverse",
	"DRARP_Request",
	"DRARP_Reply",
	"DRARP_Error",
	"InARP_Request",
	"ARP_NAK",
};

static void print_help()
{
	int i;

	printf(
"arp options:\n"
"--arp-opcode  [!] opcode        : ARP opcode (integer or string)\n"
"--arp-htype   [!] type          : ARP hardware type (integer or string)\n"
"--arp-ptype   [!] type          : ARP protocol type (hexadecimal or string)\n"
"--arp-ip-src  [!] address[/mask]: ARP IP source specification\n"
"--arp-ip-dst  [!] address[/mask]: ARP IP target specification\n"
"--arp-mac-src [!] address[/mask]: ARP MAC source specification\n"
"--arp-mac-dst [!] address[/mask]: ARP MAC target specification\n"
"[!] --arp-gratuitous            : ARP gratuitous packet\n"
" opcode strings: \n");
	for (i = 0; i < NUMOPCODES; i++)
		printf(" %d = %s\n", i + 1, opcodes[i]);
	printf(
" hardware type string: 1 = Ethernet\n"
" protocol type string: see "_PATH_ETHERTYPES"\n");
}

static void init(struct ebt_entry_match *match)
{
	struct ebt_arp_info *arpinfo = (struct ebt_arp_info *)match->data;

	arpinfo->invflags = 0;
	arpinfo->bitmask = 0;
}


#define OPT_OPCODE 0x01
#define OPT_HTYPE  0x02
#define OPT_PTYPE  0x04
#define OPT_IP_S   0x08
#define OPT_IP_D   0x10
#define OPT_MAC_S  0x20
#define OPT_MAC_D  0x40
#define OPT_GRAT   0x80
static int parse(int c, char **argv, int argc, const struct ebt_u_entry *entry,
   unsigned int *flags, struct ebt_entry_match **match)
{
	struct ebt_arp_info *arpinfo = (struct ebt_arp_info *)(*match)->data;
	long int i;
	char *end;
	uint32_t *addr;
	uint32_t *mask;
	unsigned char *maddr;
	unsigned char *mmask;

	switch (c) {
	case ARP_OPCODE:
		ebt_check_option2(flags, OPT_OPCODE);
		if (ebt_check_inverse2(optarg))
			arpinfo->invflags |= EBT_ARP_OPCODE;
		i = strtol(optarg, &end, 10);
		if (i < 0 || i >= (0x1 << 16) || *end !='\0') {
			for (i = 0; i < NUMOPCODES; i++)
				if (!strcasecmp(opcodes[i], optarg))
					break;
			if (i == NUMOPCODES)
				ebt_print_error2("Problem with specified ARP opcode");
			i++;
		}
		arpinfo->opcode = htons(i);
		arpinfo->bitmask |= EBT_ARP_OPCODE;
		break;

	case ARP_HTYPE:
		ebt_check_option2(flags, OPT_HTYPE);
		if (ebt_check_inverse2(optarg))
			arpinfo->invflags |= EBT_ARP_HTYPE;
		i = strtol(optarg, &end, 10);
		if (i < 0 || i >= (0x1 << 16) || *end !='\0') {
			if (!strcasecmp("Ethernet", argv[optind - 1]))
				i = 1;
			else
				ebt_print_error2("Problem with specified ARP hardware type");
		}
		arpinfo->htype = htons(i);
		arpinfo->bitmask |= EBT_ARP_HTYPE;
		break;

	case ARP_PTYPE:
	{
		uint16_t proto;

		ebt_check_option2(flags, OPT_PTYPE);
		if (ebt_check_inverse2(optarg))
			arpinfo->invflags |= EBT_ARP_PTYPE;

		i = strtol(optarg, &end, 16);
		if (i < 0 || i >= (0x1 << 16) || *end !='\0') {
			struct ethertypeent *ent;

			ent = getethertypebyname(argv[optind - 1]);
			if (!ent)
				ebt_print_error2("Problem with specified ARP "
						"protocol type");
			proto = ent->e_ethertype;

		} else
			proto = i;
		arpinfo->ptype = htons(proto);
		arpinfo->bitmask |= EBT_ARP_PTYPE;
		break;
	}

	case ARP_IP_S:
	case ARP_IP_D:
		if (c == ARP_IP_S) {
			ebt_check_option2(flags, OPT_IP_S);
			addr = &arpinfo->saddr;
			mask = &arpinfo->smsk;
			arpinfo->bitmask |= EBT_ARP_SRC_IP;
		} else {
			ebt_check_option2(flags, OPT_IP_D);
			addr = &arpinfo->daddr;
			mask = &arpinfo->dmsk;
			arpinfo->bitmask |= EBT_ARP_DST_IP;
		}
		if (ebt_check_inverse2(optarg)) {
			if (c == ARP_IP_S)
				arpinfo->invflags |= EBT_ARP_SRC_IP;
			else
				arpinfo->invflags |= EBT_ARP_DST_IP;
		}
		ebt_parse_ip_address(optarg, addr, mask);
		break;

	case ARP_MAC_S:
	case ARP_MAC_D:
		if (c == ARP_MAC_S) {
			ebt_check_option2(flags, OPT_MAC_S);
			maddr = arpinfo->smaddr;
			mmask = arpinfo->smmsk;
			arpinfo->bitmask |= EBT_ARP_SRC_MAC;
		} else {
			ebt_check_option2(flags, OPT_MAC_D);
			maddr = arpinfo->dmaddr;
			mmask = arpinfo->dmmsk;
			arpinfo->bitmask |= EBT_ARP_DST_MAC;
		}
		if (ebt_check_inverse2(optarg)) {
			if (c == ARP_MAC_S)
				arpinfo->invflags |= EBT_ARP_SRC_MAC;
			else
				arpinfo->invflags |= EBT_ARP_DST_MAC;
		}
		if (ebt_get_mac_and_mask(optarg, maddr, mmask))
			ebt_print_error2("Problem with ARP MAC address argument");
		break;
	case ARP_GRAT:
		ebt_check_option2(flags, OPT_GRAT);
		arpinfo->bitmask |= EBT_ARP_GRAT;
		if (ebt_invert)
			arpinfo->invflags |= EBT_ARP_GRAT;
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
	if ((entry->ethproto != ETH_P_ARP && entry->ethproto != ETH_P_RARP) ||
	    entry->invflags & EBT_IPROTO)
		ebt_print_error("For (R)ARP filtering the protocol must be specified as ARP or RARP");
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match)
{
	struct ebt_arp_info *arpinfo = (struct ebt_arp_info *)match->data;
	int i;

	if (arpinfo->bitmask & EBT_ARP_OPCODE) {
		int opcode = ntohs(arpinfo->opcode);
		printf("--arp-op ");
		if (arpinfo->invflags & EBT_ARP_OPCODE)
			printf("! ");
		if (opcode > 0 && opcode <= NUMOPCODES)
			printf("%s ", opcodes[opcode - 1]);
		else
			printf("%d ", opcode);
	}
	if (arpinfo->bitmask & EBT_ARP_HTYPE) {
		printf("--arp-htype ");
		if (arpinfo->invflags & EBT_ARP_HTYPE)
			printf("! ");
		printf("%d ", ntohs(arpinfo->htype));
	}
	if (arpinfo->bitmask & EBT_ARP_PTYPE) {
		struct ethertypeent *ent;

		printf("--arp-ptype ");
		if (arpinfo->invflags & EBT_ARP_PTYPE)
			printf("! ");
		ent = getethertypebynumber(ntohs(arpinfo->ptype));
		if (!ent)
			printf("0x%x ", ntohs(arpinfo->ptype));
		else
			printf("%s ", ent->e_name);
	}
	if (arpinfo->bitmask & EBT_ARP_SRC_IP) {
		printf("--arp-ip-src ");
		if (arpinfo->invflags & EBT_ARP_SRC_IP)
			printf("! ");
		for (i = 0; i < 4; i++)
			printf("%d%s", ((unsigned char *)&arpinfo->saddr)[i],
			   (i == 3) ? "" : ".");
		printf("%s ", ebt_mask_to_dotted(arpinfo->smsk));
	}
	if (arpinfo->bitmask & EBT_ARP_DST_IP) {
		printf("--arp-ip-dst ");
		if (arpinfo->invflags & EBT_ARP_DST_IP)
			printf("! ");
		for (i = 0; i < 4; i++)
			printf("%d%s", ((unsigned char *)&arpinfo->daddr)[i],
			   (i == 3) ? "" : ".");
		printf("%s ", ebt_mask_to_dotted(arpinfo->dmsk));
	}
	if (arpinfo->bitmask & EBT_ARP_SRC_MAC) {
		printf("--arp-mac-src ");
		if (arpinfo->invflags & EBT_ARP_SRC_MAC)
			printf("! ");
		ebt_print_mac_and_mask(arpinfo->smaddr, arpinfo->smmsk);
		printf(" ");
	}
	if (arpinfo->bitmask & EBT_ARP_DST_MAC) {
		printf("--arp-mac-dst ");
		if (arpinfo->invflags & EBT_ARP_DST_MAC)
			printf("! ");
		ebt_print_mac_and_mask(arpinfo->dmaddr, arpinfo->dmmsk);
		printf(" ");
	}
	if (arpinfo->bitmask & EBT_ARP_GRAT) {
		if (arpinfo->invflags & EBT_ARP_GRAT)
			printf("! ");
		printf("--arp-gratuitous ");
	}
}

static int compare(const struct ebt_entry_match *m1,
   const struct ebt_entry_match *m2)
{
	struct ebt_arp_info *arpinfo1 = (struct ebt_arp_info *)m1->data;
	struct ebt_arp_info *arpinfo2 = (struct ebt_arp_info *)m2->data;

	if (arpinfo1->bitmask != arpinfo2->bitmask)
		return 0;
	if (arpinfo1->invflags != arpinfo2->invflags)
		return 0;
	if (arpinfo1->bitmask & EBT_ARP_OPCODE) {
		if (arpinfo1->opcode != arpinfo2->opcode)
			return 0;
	}
	if (arpinfo1->bitmask & EBT_ARP_HTYPE) {
		if (arpinfo1->htype != arpinfo2->htype)
			return 0;
	}
	if (arpinfo1->bitmask & EBT_ARP_PTYPE) {
		if (arpinfo1->ptype != arpinfo2->ptype)
			return 0;
	}
	if (arpinfo1->bitmask & EBT_ARP_SRC_IP) {
		if (arpinfo1->saddr != arpinfo2->saddr)
			return 0;
		if (arpinfo1->smsk != arpinfo2->smsk)
			return 0;
	}
	if (arpinfo1->bitmask & EBT_ARP_DST_IP) {
		if (arpinfo1->daddr != arpinfo2->daddr)
			return 0;
		if (arpinfo1->dmsk != arpinfo2->dmsk)
			return 0;
	}
	if (arpinfo1->bitmask & EBT_ARP_SRC_MAC) {
		if (memcmp(arpinfo1->smaddr, arpinfo2->smaddr, ETH_ALEN))
			return 0;
		if (memcmp(arpinfo1->smmsk, arpinfo2->smmsk, ETH_ALEN))
			return 0;
	}
	if (arpinfo1->bitmask & EBT_ARP_DST_MAC) {
		if (memcmp(arpinfo1->dmaddr, arpinfo2->dmaddr, ETH_ALEN))
			return 0;
		if (memcmp(arpinfo1->dmmsk, arpinfo2->dmmsk, ETH_ALEN))
			return 0;
	}
	return 1;
}

static struct ebt_u_match arp_match =
{
	.name		= "arp",
	.size		= sizeof(struct ebt_arp_info),
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
	ebt_register_match(&arp_match);
}
