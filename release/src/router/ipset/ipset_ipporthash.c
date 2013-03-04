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
#include <string.h>			/* mem*, str* */

#include "ipset.h"

#include <linux/netfilter_ipv4/ip_set_ipporthash.h>

#define OPT_CREATE_HASHSIZE	0x01U
#define OPT_CREATE_PROBES	0x02U
#define OPT_CREATE_RESIZE	0x04U
#define OPT_CREATE_NETWORK	0x08U
#define OPT_CREATE_FROM		0x10U
#define OPT_CREATE_TO		0x20U

/* Initialize the create. */
static void
ipporthash_create_init(void *data)
{
	struct ip_set_req_ipporthash_create *mydata = data;

	DP("create INIT");

	/* Default create parameters */	
	mydata->hashsize = IP_NF_SET_HASHSIZE;
	mydata->probes = 8;
	mydata->resize = 50;
}

/* Function which parses command options; returns true if it ate an option */
static int
ipporthash_create_parse(int c, char *argv[] UNUSED, void *data,
			unsigned *flags)
{
	struct ip_set_req_ipporthash_create *mydata = data;
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
		parse_ip(optarg, &mydata->from);

		*flags |= OPT_CREATE_FROM;

		DP("--from %x (%s)", mydata->from,
		   ip_tostring_numeric(mydata->from));

		break;

	case '5':
		parse_ip(optarg, &mydata->to);

		*flags |= OPT_CREATE_TO;

		DP("--to %x (%s)", mydata->to,
		   ip_tostring_numeric(mydata->to));

		break;

	case '6':
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

	default:
		return 0;
	}

	return 1;
}

/* Final check; exit if not ok. */
static void
ipporthash_create_final(void *data, unsigned int flags)
{
	struct ip_set_req_ipporthash_create *mydata = data;

#ifdef IPSET_DEBUG
	DP("hashsize %u probes %u resize %u",
	   mydata->hashsize, mydata->probes, mydata->resize);
#endif

	if (flags & OPT_CREATE_NETWORK) {
		/* --network */
		if ((flags & OPT_CREATE_FROM) || (flags & OPT_CREATE_TO))
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --from or --to with --network\n");
	} else if (flags & (OPT_CREATE_FROM | OPT_CREATE_TO)) {
		/* --from --to */
		if (!(flags & OPT_CREATE_FROM) || !(flags & OPT_CREATE_TO))
			exit_error(PARAMETER_PROBLEM,
				   "Need to specify both --from and --to\n");
	} else {
		exit_error(PARAMETER_PROBLEM,
			   "Need to specify --from and --to, or --network\n");

	}

	DP("from : %x to: %x diff: %x", 
	   mydata->from, mydata->to,
	   mydata->to - mydata->from);

	if (mydata->from > mydata->to)
		exit_error(PARAMETER_PROBLEM,
			   "From can't be higher than to.\n");

	if (mydata->to - mydata->from > MAX_RANGE)
		exit_error(PARAMETER_PROBLEM,
			   "Range too large. Max is %d IPs in range\n",
			   MAX_RANGE+1);
}

/* Create commandline options */
static const struct option create_opts[] = {
	{.name = "hashsize",	.has_arg = required_argument,	.val = '1'},
	{.name = "probes",	.has_arg = required_argument,	.val = '2'},
	{.name = "resize",	.has_arg = required_argument,	.val = '3'},
	{.name = "from",	.has_arg = required_argument,	.val = '4'},
	{.name = "to",		.has_arg = required_argument,	.val = '5'},
	{.name = "network",	.has_arg = required_argument,	.val = '6'},
	{NULL},
};

/* Add, del, test parser */
static ip_set_ip_t
ipporthash_adt_parser(int cmd UNUSED, const char *arg, void *data)
{
	struct ip_set_req_ipporthash *mydata = data;
	char *saved = ipset_strdup(arg);
	char *ptr, *tmp = saved;

	DP("ipporthash: %p %p", arg, data);

	if (((ptr = strchr(tmp, ':')) || (ptr = strchr(tmp, '%'))) && ++warn_once == 1)
		fprintf(stderr, "Warning: please use ',' separator token between ip,port.\n"
			        "Next release won't support old separator tokens.\n");

	ptr = strsep(&tmp, ":%,");
	parse_ip(ptr, &mydata->ip);

	if (tmp)
		parse_port(tmp, &mydata->port);
	else
		exit_error(PARAMETER_PROBLEM,
			   "IP address and port must be specified: ip,port");

	if (!(mydata->ip || mydata->port))
		exit_error(PARAMETER_PROBLEM,
			  "Zero valued IP address and port `%s' specified", arg);
	ipset_free(saved);
	return 1;	
};

/*
 * Print and save
 */

static void
ipporthash_initheader(struct set *set, const void *data)
{
	const struct ip_set_req_ipporthash_create *header = data;
	struct ip_set_ipporthash *map = set->settype->header;

	memset(map, 0, sizeof(struct ip_set_ipporthash));
	map->hashsize = header->hashsize;
	map->probes = header->probes;
	map->resize = header->resize;
	map->first_ip = header->from;
	map->last_ip = header->to;
}

static void
ipporthash_printheader(struct set *set, unsigned options)
{
	struct ip_set_ipporthash *mysetdata = set->settype->header;

	printf(" from: %s", ip_tostring(mysetdata->first_ip, options));
	printf(" to: %s", ip_tostring(mysetdata->last_ip, options));
	printf(" hashsize: %u", mysetdata->hashsize);
	printf(" probes: %u", mysetdata->probes);
	printf(" resize: %u\n", mysetdata->resize);
}

static void
ipporthash_printips(struct set *set, void *data, u_int32_t len,
		    unsigned options, char dont_align)
{
	struct ip_set_ipporthash *mysetdata = set->settype->header;
	size_t offset = 0;
	ip_set_ip_t *ipptr, ip;
	uint16_t port;

	while (offset < len) {
		ipptr = data + offset;
		ip = (*ipptr>>16) + mysetdata->first_ip;
		port = (uint16_t) *ipptr;
		printf("%s,%s\n", 
		       ip_tostring(ip, options),
		       port_tostring(port, options));
		offset += IPSET_VALIGN(sizeof(ip_set_ip_t), dont_align);
	}
}

static void
ipporthash_saveheader(struct set *set, unsigned options)
{
	struct ip_set_ipporthash *mysetdata = set->settype->header;

	printf("-N %s %s --from %s",
	       set->name, set->settype->typename,
	       ip_tostring(mysetdata->first_ip, options));
	printf(" --to %s",
	       ip_tostring(mysetdata->last_ip, options));
	printf(" --hashsize %u --probes %u --resize %u\n",
	       mysetdata->hashsize, mysetdata->probes, mysetdata->resize);
}

/* Print save for an IP */
static void
ipporthash_saveips(struct set *set, void *data, u_int32_t len,
		   unsigned options, char dont_align)
{
	struct ip_set_ipporthash *mysetdata = set->settype->header;
	size_t offset = 0;
	ip_set_ip_t *ipptr, ip;
	uint16_t port;

	while (offset < len) {
		ipptr = data + offset;
		ip = (*ipptr>>16) + mysetdata->first_ip;
		port = (uint16_t) *ipptr;
		printf("-A %s %s,%s\n", set->name, 
		       ip_tostring(ip, options),
		       port_tostring(port, options));
		offset += IPSET_VALIGN(sizeof(ip_set_ip_t), dont_align);
	}
}

static void
ipporthash_usage(void)
{
	printf
	    ("-N set ipporthash --from IP --to IP\n"
	     "   [--hashsize hashsize] [--probes probes ] [--resize resize]\n"
	     "-N set ipporthash --network IP/mask\n"
	     "   [--hashsize hashsize] [--probes probes ] [--resize resize]\n"
	     "-A set IP,port\n"
	     "-D set IP,port\n"
	     "-T set IP,port\n");
}

static struct settype settype_ipporthash = {
	.typename = SETTYPE_NAME,
	.protocol_version = IP_SET_PROTOCOL_VERSION,

	/* Create */
	.create_size = sizeof(struct ip_set_req_ipporthash_create),
	.create_init = ipporthash_create_init,
	.create_parse = ipporthash_create_parse,
	.create_final = ipporthash_create_final,
	.create_opts = create_opts,

	/* Add/del/test */
	.adt_size = sizeof(struct ip_set_req_ipporthash),
	.adt_parser = ipporthash_adt_parser,

	/* Printing */
	.header_size = sizeof(struct ip_set_ipporthash),
	.initheader = ipporthash_initheader,
	.printheader = ipporthash_printheader,
	.printips = ipporthash_printips,
	.printips_sorted = ipporthash_printips,
	.saveheader = ipporthash_saveheader,
	.saveips = ipporthash_saveips,
	
	.usage = ipporthash_usage,
};

CONSTRUCTOR(ipporthash)
{
	settype_register(&settype_ipporthash);

}
