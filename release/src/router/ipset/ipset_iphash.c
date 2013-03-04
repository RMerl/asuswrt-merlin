/* Copyright 2004 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify   
 * it under the terms of the GNU General Public License as published by   
 * the Free Software Foundation; either version 2 of the License, or      
 * (at your option) any later version.                                    
 *                                                                         
 * This program is distributed in the hope that it will be useful,        
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
 * GNU General Public License for more details.                           
 *                                                                         
 * You should have received a copy of the GNU General Public License      
 * along with this program; if not, write to the Free Software            
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <limits.h>			/* UINT_MAX */
#include <stdio.h>			/* *printf */
#include <string.h>			/* mem* */

#include "ipset.h"

#include <linux/netfilter_ipv4/ip_set_iphash.h>

#define BUFLEN 30;

#define OPT_CREATE_HASHSIZE	0x01U
#define OPT_CREATE_PROBES	0x02U
#define OPT_CREATE_RESIZE	0x04U
#define OPT_CREATE_NETMASK	0x08U

/* Initialize the create. */
static void
iphash_create_init(void *data)
{
	struct ip_set_req_iphash_create *mydata = data;

	DP("create INIT");

	/* Default create parameters */	
	mydata->hashsize = IP_NF_SET_HASHSIZE;
	mydata->probes = 8;
	mydata->resize = 50;
	
	mydata->netmask = 0xFFFFFFFF;
}

/* Function which parses command options; returns true if it ate an option */
static int
iphash_create_parse(int c, char *argv[] UNUSED, void *data, unsigned *flags)
{
	struct ip_set_req_iphash_create *mydata =
	    (struct ip_set_req_iphash_create *) data;
	unsigned int bits;
	ip_set_ip_t value;

	DP("create_parse");

	switch (c) {
	case '1':

		if (string_to_number(optarg, 1, UINT_MAX - 1, &mydata->hashsize))
			exit_error(PARAMETER_PROBLEM, "Invalid hashsize `%s' specified", optarg);

		*flags |= OPT_CREATE_HASHSIZE;

		DP("--hashsize %u", mydata->hashsize);
		
		break;

	case '2':

		if (string_to_number(optarg, 1, 65535, &value))
			exit_error(PARAMETER_PROBLEM, "Invalid probes `%s' specified", optarg);

		mydata->probes = value;
		*flags |= OPT_CREATE_PROBES;

		DP("--probes %u", mydata->probes);
		
		break;

	case '3':

		if (string_to_number(optarg, 0, 65535, &value))
			exit_error(PARAMETER_PROBLEM, "Invalid resize `%s' specified", optarg);

		mydata->resize = value;
		*flags |= OPT_CREATE_RESIZE;

		DP("--resize %u", mydata->resize);
		
		break;

	case '4':

		if (string_to_number(optarg, 0, 32, &bits))
			exit_error(PARAMETER_PROBLEM, 
				  "Invalid netmask `%s' specified", optarg);
		
		if (bits != 0)
			mydata->netmask = 0xFFFFFFFF << (32 - bits);

		*flags |= OPT_CREATE_NETMASK;

		DP("--netmask %x", mydata->netmask);
		
		break;

	default:
		return 0;
	}

	return 1;
}

/* Final check; exit if not ok. */
static void
iphash_create_final(void *data UNUSED, unsigned int flags UNUSED)
{
}

/* Create commandline options */
static const struct option create_opts[] = {
	{.name = "hashsize",	.has_arg = required_argument,	.val = '1'},
	{.name = "probes",	.has_arg = required_argument,	.val = '2'},
	{.name = "resize",	.has_arg = required_argument,	.val = '3'},
	{.name = "netmask",	.has_arg = required_argument,	.val = '4'},
	{NULL},
};

/* Add, del, test parser */
static ip_set_ip_t
iphash_adt_parser(int cmd UNUSED, const char *arg, void *data)
{
	struct ip_set_req_iphash *mydata = data;

	parse_ip(arg, &mydata->ip);
	if (!mydata->ip)
		exit_error(PARAMETER_PROBLEM,
			   "Zero valued IP address `%s' specified", arg);

	return mydata->ip;	
};

/*
 * Print and save
 */

static void
iphash_initheader(struct set *set, const void *data)
{
	const struct ip_set_req_iphash_create *header = data;
	struct ip_set_iphash *map = set->settype->header;

	memset(map, 0, sizeof(struct ip_set_iphash));
	map->hashsize = header->hashsize;
	map->probes = header->probes;
	map->resize = header->resize;
	map->netmask = header->netmask;
}

static unsigned int
mask_to_bits(ip_set_ip_t mask)
{
	unsigned int bits = 32;
	ip_set_ip_t maskaddr;
	
	if (mask == 0xFFFFFFFF)
		return bits;
	
	maskaddr = 0xFFFFFFFE;
	while (--bits > 0 && maskaddr != mask)
		maskaddr <<= 1;
	
	return bits;
}

static void
iphash_printheader(struct set *set, unsigned options UNUSED)
{
	struct ip_set_iphash *mysetdata = set->settype->header;

	printf(" hashsize: %u", mysetdata->hashsize);
	printf(" probes: %u", mysetdata->probes);
	printf(" resize: %u", mysetdata->resize);
	if (mysetdata->netmask == 0xFFFFFFFF)
		printf("\n");
	else
		printf(" netmask: %d\n", mask_to_bits(mysetdata->netmask));
}

static void
iphash_printips(struct set *set UNUSED, void *data, u_int32_t len,
		unsigned options, char dont_align)
{
	size_t offset = 0;
	ip_set_ip_t *ip;

	while (offset < len) {
		ip = data + offset;
		printf("%s\n", ip_tostring(*ip, options));
		offset += IPSET_VALIGN(sizeof(ip_set_ip_t), dont_align);
	}
}

static void
iphash_saveheader(struct set *set, unsigned options UNUSED)
{
	struct ip_set_iphash *mysetdata = set->settype->header;

	printf("-N %s %s --hashsize %u --probes %u --resize %u",
	       set->name, set->settype->typename,
	       mysetdata->hashsize, mysetdata->probes, mysetdata->resize);
	if (mysetdata->netmask == 0xFFFFFFFF)
		printf("\n");
	else
		printf(" --netmask %d\n", mask_to_bits(mysetdata->netmask));
}

/* Print save for an IP */
static void
iphash_saveips(struct set *set UNUSED, void *data, u_int32_t len,
	       unsigned options, char dont_align)
{
	size_t offset = 0;
	ip_set_ip_t *ip;

	while (offset < len) {
		ip = data + offset;
		printf("-A %s %s\n", set->name, ip_tostring(*ip, options));
		offset += IPSET_VALIGN(sizeof(ip_set_ip_t), dont_align);
	}
}

static void
iphash_usage(void)
{
	printf
	    ("-N set iphash [--hashsize hashsize] [--probes probes ]\n"
	     "              [--resize resize] [--netmask CIDR-netmask]\n"
	     "-A set IP\n"
	     "-D set IP\n"
	     "-T set IP\n");
}

static struct settype settype_iphash = {
	.typename = SETTYPE_NAME,
	.protocol_version = IP_SET_PROTOCOL_VERSION,

	/* Create */
	.create_size = sizeof(struct ip_set_req_iphash_create),
	.create_init = iphash_create_init,
	.create_parse = iphash_create_parse,
	.create_final = iphash_create_final,
	.create_opts = create_opts,

	/* Add/del/test */
	.adt_size = sizeof(struct ip_set_req_iphash),
	.adt_parser = iphash_adt_parser,

	/* Printing */
	.header_size = sizeof(struct ip_set_iphash),
	.initheader = iphash_initheader,
	.printheader = iphash_printheader,
	.printips = iphash_printips,
	.printips_sorted = iphash_printips,
	.saveheader = iphash_saveheader,
	.saveips = iphash_saveips,
	
	.usage = iphash_usage,
};

CONSTRUCTOR(iphash)
{
	settype_register(&settype_iphash);

}
