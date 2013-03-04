/* Copyright 2000-2004 Joakim Axelsson (gozem@linux.nu)
 *                     Patrick Schaaf (bof@bof.de)
 *                     Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
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

#include <stdio.h>			/* *printf */
#include <string.h>			/* mem* */

#include "ipset.h"

#include <linux/netfilter_ipv4/ip_set_ipmap.h>

#define BUFLEN 30;

#define OPT_CREATE_FROM    0x01U
#define OPT_CREATE_TO      0x02U
#define OPT_CREATE_NETWORK 0x04U
#define OPT_CREATE_NETMASK 0x08U

#define OPT_ADDDEL_IP      0x01U

/* Initialize the create. */
static void
ipmap_create_init(void *data)
{
	struct ip_set_req_ipmap_create *mydata = data;

	DP("create INIT");
	mydata->netmask = 0xFFFFFFFF;
}

/* Function which parses command options; returns true if it ate an option */
static int
ipmap_create_parse(int c, char *argv[] UNUSED, void *data, unsigned *flags)
{
	struct ip_set_req_ipmap_create *mydata = data;
	unsigned int bits;

	DP("create_parse");

	switch (c) {
	case '1':
		parse_ip(optarg, &mydata->from);

		*flags |= OPT_CREATE_FROM;

		DP("--from %x (%s)", mydata->from,
		   ip_tostring_numeric(mydata->from));

		break;

	case '2':
		parse_ip(optarg, &mydata->to);

		*flags |= OPT_CREATE_TO;

		DP("--to %x (%s)", mydata->to,
		   ip_tostring_numeric(mydata->to));

		break;

	case '3':
		parse_ipandmask(optarg, &mydata->from, &mydata->to);

		/* Make to the last of from + mask */
		if (mydata->to)
			mydata->to = mydata->from | ~(mydata->to);
		else {
			mydata->from = 0x00000000;
			mydata->to = 0xFFFFFFFF;
		}
		*flags |= OPT_CREATE_NETWORK;

		DP("--network from %x (%s)", 
		   mydata->from, ip_tostring_numeric(mydata->from));
		DP("--network to %x (%s)", 
		   mydata->to, ip_tostring_numeric(mydata->to));

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
ipmap_create_final(void *data, unsigned int flags)
{
	struct ip_set_req_ipmap_create *mydata = data;
	ip_set_ip_t range;

	if (flags == 0)
		exit_error(PARAMETER_PROBLEM,
			   "Need to specify --from and --to, or --network\n");

	if (flags & OPT_CREATE_NETWORK) {
		/* --network */
		if ((flags & OPT_CREATE_FROM) || (flags & OPT_CREATE_TO))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --from or --to with --network\n");
	} else {
		/* --from --to */
		if ((flags & OPT_CREATE_FROM) == 0
		    || (flags & OPT_CREATE_TO) == 0)
			exit_error(PARAMETER_PROBLEM,
				   "Need to specify both --from and --to\n");
	}

	DP("from : %x to: %x diff: %x", 
	   mydata->from, mydata->to,
	   mydata->to - mydata->from);

	if (mydata->from > mydata->to)
		exit_error(PARAMETER_PROBLEM,
			   "From can't be lower than to.\n");

	if (flags & OPT_CREATE_NETMASK) {
		unsigned int mask_bits, netmask_bits;
		ip_set_ip_t mask;

		if ((mydata->from & mydata->netmask) != mydata->from)
			exit_error(PARAMETER_PROBLEM,
				   "%s is not a network address according to netmask %d\n",
				   ip_tostring_numeric(mydata->from),
				   mask_to_bits(mydata->netmask));
		
		mask = range_to_mask(mydata->from, mydata->to, &mask_bits);
		if (!mask
		    && (mydata->from || mydata->to != 0xFFFFFFFF)) {
			exit_error(PARAMETER_PROBLEM,
				   "You have to define a full network with --from"
				   " and --to if you specify the --network option\n");
		}
		netmask_bits = mask_to_bits(mydata->netmask);
		if (netmask_bits <= mask_bits) {
			exit_error(PARAMETER_PROBLEM,
				   "%d netmask specifies larger or equal netblock than the network itself\n");
		}
		range = (1<<(netmask_bits - mask_bits)) - 1;
	} else {
		range = mydata->to - mydata->from;
	}
	if (range > MAX_RANGE)
		exit_error(PARAMETER_PROBLEM,
			   "Range too large. Max is %d IPs in range\n",
			   MAX_RANGE+1);
}

/* Create commandline options */
static const struct option create_opts[] = {
	{.name = "from",	.has_arg = required_argument,	.val = '1'},
	{.name = "to",		.has_arg = required_argument,	.val = '2'},
	{.name = "network",	.has_arg = required_argument,	.val = '3'},
	{.name = "netmask",	.has_arg = required_argument,	.val = '4'},
	{NULL},
};

/* Add, del, test parser */
static ip_set_ip_t
ipmap_adt_parser(int cmd UNUSED, const char *arg, void *data)
{
	struct ip_set_req_ipmap *mydata = data;

	DP("ipmap: %p %p", arg, data);

	parse_ip(arg, &mydata->ip);
	DP("%s", ip_tostring_numeric(mydata->ip));

	return 1;	
}

/*
 * Print and save
 */

static void
ipmap_initheader(struct set *set, const void *data)
{
	const struct ip_set_req_ipmap_create *header = data;
	struct ip_set_ipmap *map = set->settype->header;
		
	memset(map, 0, sizeof(struct ip_set_ipmap));
	map->first_ip = header->from;
	map->last_ip = header->to;
	map->netmask = header->netmask;

	if (map->netmask == 0xFFFFFFFF) {
		map->hosts = 1;
		map->sizeid = map->last_ip - map->first_ip + 1;
	} else {
		unsigned int mask_bits, netmask_bits;
		ip_set_ip_t mask;
	
		mask = range_to_mask(header->from, header->to, &mask_bits);
		netmask_bits = mask_to_bits(header->netmask);

		DP("bits: %d %d", mask_bits, netmask_bits);
		map->hosts = 2 << (32 - netmask_bits - 1);
		map->sizeid = 2 << (netmask_bits - mask_bits - 1);
	}

	DP("%d %d", map->hosts, map->sizeid );
}

static void
ipmap_printheader(struct set *set, unsigned options)
{
	struct ip_set_ipmap *mysetdata = set->settype->header;

	printf(" from: %s", ip_tostring(mysetdata->first_ip, options));
	printf(" to: %s", ip_tostring(mysetdata->last_ip, options));
	if (mysetdata->netmask == 0xFFFFFFFF)
		printf("\n");
	else
		printf(" netmask: %d\n", mask_to_bits(mysetdata->netmask));
}

static inline void
__ipmap_printips_sorted(struct set *set, void *data,
			u_int32_t len UNUSED, unsigned options)
{
	struct ip_set_ipmap *mysetdata = set->settype->header;
	ip_set_ip_t id;

	for (id = 0; id < mysetdata->sizeid; id++)
		if (test_bit(id, data))
			printf("%s\n",
			       ip_tostring(mysetdata->first_ip
			       		   + id * mysetdata->hosts,
					   options));
}

static void
ipmap_printips_sorted(struct set *set, void *data,
		      u_int32_t len, unsigned options,
		      char dont_align)
{
	ip_set_ip_t *ip;
	size_t offset = 0;
	
	if (dont_align)
		return __ipmap_printips_sorted(set, data, len, options);
	
	while (offset < len) {
		DP("offset: %zu, len %u\n", offset, len);
		ip = data + offset;
		printf("%s\n", ip_tostring(*ip, options));
		offset += IPSET_ALIGN(sizeof(ip_set_ip_t));
	}
}

static void
ipmap_saveheader(struct set *set, unsigned options)
{
	struct ip_set_ipmap *mysetdata = set->settype->header;

	printf("-N %s %s --from %s",
	       set->name, set->settype->typename,
	       ip_tostring(mysetdata->first_ip, options));
	printf(" --to %s",
	       ip_tostring(mysetdata->last_ip, options));
	if (mysetdata->netmask == 0xFFFFFFFF)
		printf("\n");
	else
		printf(" --netmask %d\n",
		       mask_to_bits(mysetdata->netmask));
}

static inline void
__ipmap_saveips(struct set *set, void *data, u_int32_t len UNUSED,
		unsigned options)
{
	struct ip_set_ipmap *mysetdata = set->settype->header;
	ip_set_ip_t id;

	DP("%s", set->name);
	for (id = 0; id < mysetdata->sizeid; id++)
		if (test_bit(id, data))
			printf("-A %s %s\n",
			       set->name,
			       ip_tostring(mysetdata->first_ip 
			       		   + id * mysetdata->hosts,
					   options));
}

static void
ipmap_saveips(struct set *set, void *data, u_int32_t len,
	      unsigned options, char dont_align)
{
	ip_set_ip_t *ip;
	size_t offset = 0;
	
	if (dont_align)
		return __ipmap_saveips(set, data, len, options);
	
	while (offset < len) {
		ip = data + offset;
		printf("-A %s %s\n", set->name, ip_tostring(*ip, options));
		offset += IPSET_ALIGN(sizeof(ip_set_ip_t));
	}
}

static void
ipmap_usage(void)
{
	printf
	    ("-N set ipmap --from IP --to IP [--netmask CIDR-netmask]\n"
	     "-N set ipmap --network IP/mask [--netmask CIDR-netmask]\n"
	     "-A set IP\n"
	     "-D set IP\n"
	     "-T set IP\n");
}

static struct settype settype_ipmap = {
	.typename = SETTYPE_NAME,
	.protocol_version = IP_SET_PROTOCOL_VERSION,

	/* Create */
	.create_size = sizeof(struct ip_set_req_ipmap_create),
	.create_init = ipmap_create_init,
	.create_parse = ipmap_create_parse,
	.create_final = ipmap_create_final,
	.create_opts = create_opts,

	/* Add/del/test */
	.adt_size = sizeof(struct ip_set_req_ipmap),
	.adt_parser = ipmap_adt_parser,

	/* Printing */
	.header_size = sizeof(struct ip_set_ipmap),
	.initheader = ipmap_initheader,
	.printheader = ipmap_printheader,
	.printips = ipmap_printips_sorted,
	.printips_sorted = ipmap_printips_sorted,
	.saveheader = ipmap_saveheader,
	.saveips = ipmap_saveips,
	
	.usage = ipmap_usage,
};

CONSTRUCTOR(ipmap)
{
	settype_register(&settype_ipmap);

}
