/* Copyright 2000, 2001, 2002 Joakim Axelsson (gozem@linux.nu)
 *                            Patrick Schaaf (bof@bof.de)
 *                            Martin Josefsson (gandalf@wlug.westbo.se)
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
#include <stdlib.h>			/* mem* */
#include <string.h>			/* str* */
#include <net/ethernet.h>		/* ETH_ALEN */

#include "ipset.h"

#include <linux/netfilter_ipv4/ip_set_macipmap.h>

#define BUFLEN 30;

#define OPT_CREATE_FROM    0x01U
#define OPT_CREATE_TO      0x02U
#define OPT_CREATE_NETWORK 0x04U
#define OPT_CREATE_MATCHUNSET	0x08U

#define OPT_ADDDEL_IP      0x01U
#define OPT_ADDDEL_MAC     0x02U

/* Initialize the create. */
static void
macipmap_create_init(void *data UNUSED)
{
	DP("create INIT");
	/* Nothing */
}

/* Function which parses command options; returns true if it ate an option */
static int
macipmap_create_parse(int c, char *argv[] UNUSED, void *data, unsigned *flags)
{
	struct ip_set_req_macipmap_create *mydata = data;

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
		mydata->to = mydata->from | (~mydata->to);

		*flags |= OPT_CREATE_NETWORK;

		DP("--network from %x (%s)", 
		   mydata->from, ip_tostring_numeric(mydata->from));
		DP("--network to %x (%s)", 
		   mydata->to, ip_tostring_numeric(mydata->to));

		break;

	case '4':
		mydata->flags |= IPSET_MACIP_MATCHUNSET;

		*flags |= OPT_CREATE_MATCHUNSET;

		DP("--matchunset");

		break;

	default:
		return 0;
	}

	return 1;
}

/* Final check; exit if not ok. */
static void
macipmap_create_final(void *data, unsigned int flags)
{
	struct ip_set_req_macipmap_create *mydata = data;

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


	DP("from : %x to: %x  diff: %d  match unset: %d", mydata->from,
	   mydata->to, mydata->to - mydata->from,
	   flags & OPT_CREATE_MATCHUNSET);

	if (mydata->from > mydata->to)
		exit_error(PARAMETER_PROBLEM,
			   "From can't be lower than to.\n");

	if (mydata->to - mydata->from > MAX_RANGE)
		exit_error(PARAMETER_PROBLEM,
			   "Range too large. Max is %d IPs in range\n",
			   MAX_RANGE+1);
}

/* Create commandline options */
static const struct option create_opts[] = {
	{.name = "from",	.has_arg = required_argument,	.val = '1'},
	{.name = "to",		.has_arg = required_argument,	.val = '2'},
	{.name = "network",	.has_arg = required_argument,	.val = '3'},
	{.name = "matchunset",	.has_arg = no_argument,		.val = '4'},
	{NULL},
};

static void
parse_mac(const char *mac, unsigned char *ethernet)
{
	unsigned int i = 0;

	if (strlen(mac) != ETH_ALEN * 3 - 1)
		exit_error(PARAMETER_PROBLEM, "Bad mac address `%s'", mac);

	for (i = 0; i < ETH_ALEN; i++) {
		long number;
		char *end;

		number = strtol(mac + i * 3, &end, 16);

		if (end == mac + i * 3 + 2 && number >= 0 && number <= 255)
			ethernet[i] = number;
		else
			exit_error(PARAMETER_PROBLEM,
				   "Bad mac address `%s'", mac);
	}
}

/* Add, del, test parser */
static ip_set_ip_t
macipmap_adt_parser(int cmd UNUSED, const char *arg, void *data)
{
	struct ip_set_req_macipmap *mydata = data;
	char *saved = ipset_strdup(arg);
	char *ptr, *tmp = saved;

	DP("macipmap: %p %p", arg, data);

	ptr = strsep(&tmp, ",");
	if (!tmp) {
		tmp = saved;
		ptr = strsep(&tmp, ":%");	
		if (tmp && ++warn_once == 1)
			fprintf(stderr, "Warning: please use ',' separator token between ip,mac.\n"
				        "Next release won't support old separator tokens.\n");
	}
	parse_ip(ptr, &mydata->ip);

	if (tmp)
		parse_mac(tmp, mydata->ethernet);
	else
		memset(mydata->ethernet, 0, ETH_ALEN);	

	free(saved);

	return 1;	
}

/*
 * Print and save
 */

static void
macipmap_initheader(struct set *set, const void *data)
{
	const struct ip_set_req_macipmap_create *header = data;
	struct ip_set_macipmap *map = set->settype->header;

	memset(map, 0, sizeof(struct ip_set_macipmap));
	map->first_ip = header->from;
	map->last_ip = header->to;
	map->flags = header->flags;
}

static void
macipmap_printheader(struct set *set, unsigned options)
{
	struct ip_set_macipmap *mysetdata = set->settype->header;

	printf(" from: %s", ip_tostring(mysetdata->first_ip, options));
	printf(" to: %s", ip_tostring(mysetdata->last_ip, options));

	if (mysetdata->flags & IPSET_MACIP_MATCHUNSET)
		printf(" matchunset");
	printf("\n");
}

static void
print_mac(unsigned char macaddress[ETH_ALEN])
{
	unsigned int i;

	printf("%02X", macaddress[0]);
	for (i = 1; i < ETH_ALEN; i++)
		printf(":%02X", macaddress[i]);
}

static inline void
__macipmap_printips_sorted(struct set *set, void *data,
			   u_int32_t len UNUSED, unsigned options)
{
	struct ip_set_macipmap *mysetdata = set->settype->header;
	struct ip_set_macip *table = data;
	u_int32_t addr = mysetdata->first_ip;

	while (addr <= mysetdata->last_ip) {
		if (table[addr - mysetdata->first_ip].match) {
			printf("%s,", ip_tostring(addr, options));
			print_mac(table[addr - mysetdata->first_ip].
				  ethernet);
			printf("\n");
		}
		addr++;
	}
}

static void
macipmap_printips_sorted(struct set *set, void *data,
			 u_int32_t len, unsigned options,
			 char dont_align)
{
	struct ip_set_req_macipmap *d;
	size_t offset = 0;
	
	if (dont_align)
		return __macipmap_printips_sorted(set, data, len, options);
	
	while (offset < len) {
		d = data + offset;
		printf("%s,", ip_tostring(d->ip, options));
		print_mac(d->ethernet);
		printf("\n");
		offset += IPSET_ALIGN(sizeof(struct ip_set_req_macipmap));
	}
}

static void
macipmap_saveheader(struct set *set, unsigned options)
{
	struct ip_set_macipmap *mysetdata = set->settype->header;

	printf("-N %s %s --from %s",
	       set->name, set->settype->typename,
	       ip_tostring(mysetdata->first_ip, options));
	printf(" --to %s", ip_tostring(mysetdata->last_ip, options));

	if (mysetdata->flags & IPSET_MACIP_MATCHUNSET)
		printf(" --matchunset");
	printf("\n");
}

static inline void
__macipmap_saveips(struct set *set, void *data,
		   u_int32_t len UNUSED, unsigned options)
{
	struct ip_set_macipmap *mysetdata = set->settype->header;
	struct ip_set_macip *table = data;
	u_int32_t addr = mysetdata->first_ip;

	while (addr <= mysetdata->last_ip) {
		if (table[addr - mysetdata->first_ip].match) {
			printf("-A %s %s,",
			       set->name, ip_tostring(addr, options));
			print_mac(table[addr - mysetdata->first_ip].
				  ethernet);
			printf("\n");
		}
		addr++;
	}
}

static void
macipmap_saveips(struct set *set, void *data,
		 u_int32_t len, unsigned options,
		 char dont_align)
{
	struct ip_set_req_macipmap *d;
	size_t offset = 0;
	
	if (dont_align)
		return __macipmap_saveips(set, data, len, options);
	
	while (offset < len) {
		d = data + offset;
		printf("-A %s %s,", set->name, ip_tostring(d->ip, options));
		print_mac(d->ethernet);
		printf("\n");
		offset += IPSET_ALIGN(sizeof(struct ip_set_req_macipmap));
	}
}

static void
macipmap_usage(void)
{
	printf
	    ("-N set macipmap --from IP --to IP [--matchunset]\n"
	     "-N set macipmap --network IP/mask [--matchunset]\n"
	     "-A set IP[,MAC]\n"
	     "-D set IP[,MAC]\n"
	     "-T set IP[,MAC]\n");
}

static struct settype settype_macipmap = {
	.typename = SETTYPE_NAME,
	.protocol_version = IP_SET_PROTOCOL_VERSION,

	/* Create */
	.create_size = sizeof(struct ip_set_req_macipmap_create),
	.create_init = macipmap_create_init,
	.create_parse = macipmap_create_parse,
	.create_final = macipmap_create_final,
	.create_opts = create_opts,

	/* Add/del/test */
	.adt_size = sizeof(struct ip_set_req_macipmap),
	.adt_parser = macipmap_adt_parser,

	/* Printing */
	.header_size = sizeof(struct ip_set_macipmap),
	.initheader = macipmap_initheader,
	.printheader = macipmap_printheader,
	.printips = macipmap_printips_sorted,
	.printips_sorted = macipmap_printips_sorted,
	.saveheader = macipmap_saveheader,
	.saveips = macipmap_saveips,

	.usage = macipmap_usage,
};

CONSTRUCTOR(macipmap)
{
	settype_register(&settype_macipmap);

}
