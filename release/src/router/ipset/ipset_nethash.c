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

#include <linux/netfilter_ipv4/ip_set_nethash.h>

#define BUFLEN 30;

#define OPT_CREATE_HASHSIZE	0x01U
#define OPT_CREATE_PROBES	0x02U
#define OPT_CREATE_RESIZE	0x04U

/* Initialize the create. */
static void
nethash_create_init(void *data)
{
	struct ip_set_req_nethash_create *mydata = data;

	DP("create INIT");

	/* Default create parameters */	
	mydata->hashsize = IP_NF_SET_HASHSIZE;
	mydata->probes = 4;
	mydata->resize = 50;
}

/* Function which parses command options; returns true if it ate an option */
static int
nethash_create_parse(int c, char *argv[] UNUSED, void *data, unsigned *flags)
{
	struct ip_set_req_nethash_create *mydata = data;
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

	default:
		return 0;
	}

	return 1;
}

/* Final check; exit if not ok. */
static void
nethash_create_final(void *data UNUSED, unsigned int flags UNUSED)
{
}

/* Create commandline options */
static const struct option create_opts[] = {
	{.name = "hashsize",	.has_arg = required_argument,	.val = '1'},
	{.name = "probes",	.has_arg = required_argument,	.val = '2'},
	{.name = "resize",	.has_arg = required_argument,	.val = '3'},
	{NULL},
};

/* Add, del, test parser */
static ip_set_ip_t
nethash_adt_parser(int cmd, const char *arg, void *data)
{
	struct ip_set_req_nethash *mydata = data;
	char *saved = ipset_strdup(arg);
	char *ptr, *tmp = saved;
	ip_set_ip_t cidr;

	ptr = strsep(&tmp, "/");
	
	if (tmp == NULL) {
		if (cmd == CMD_TEST)
			cidr = 32;
		else
			exit_error(PARAMETER_PROBLEM,
				   "Missing cidr from `%s'", arg);
	} else
		if (string_to_number(tmp, 1, 31, &cidr))
			exit_error(PARAMETER_PROBLEM,
				   "Out of range cidr `%s' specified", arg);
	
	mydata->cidr = cidr;
	parse_ip(ptr, &mydata->ip);
#if 0
	if (!mydata->ip)
		exit_error(PARAMETER_PROBLEM,
			  "Zero valued IP address `%s' specified", ptr);
#endif
	ipset_free(saved);

	return 1;	
};

/*
 * Print and save
 */

static void
nethash_initheader(struct set *set, const void *data)
{
	const struct ip_set_req_nethash_create *header = data;
	struct ip_set_nethash *map = set->settype->header;

	memset(map, 0, sizeof(struct ip_set_nethash));
	map->hashsize = header->hashsize;
	map->probes = header->probes;
	map->resize = header->resize;
}

static void
nethash_printheader(struct set *set, unsigned options UNUSED)
{
	struct ip_set_nethash *mysetdata = set->settype->header;

	printf(" hashsize: %u", mysetdata->hashsize);
	printf(" probes: %u", mysetdata->probes);
	printf(" resize: %u\n", mysetdata->resize);
}

static char buf[20];

static char *
unpack_ip_tostring(ip_set_ip_t ip, unsigned options UNUSED)
{
	int i, j = 3;
	unsigned char a, b;

	ip = htonl(ip);	
	for (i = 3; i >= 0; i--)
		if (((unsigned char *)&ip)[i] != 0) {
			j = i;
			break;
		}
			
	a = ((unsigned char *)&ip)[j];
	if (a <= 128) {
		a = (a - 1) * 2;
		b = 7;
	} else if (a <= 192) {
		a = (a - 129) * 4;
		b = 6;
	} else if (a <= 224) {
		a = (a - 193) * 8;
		b = 5;
	} else if (a <= 240) {
		a = (a - 225) * 16;
		b = 4;
	} else if (a <= 248) {
		a = (a - 241) * 32;
		b = 3;
	} else if (a <= 252) {
		a = (a - 249) * 64;
		b = 2;
	} else if (a <= 254) {
		a = (a - 253) * 128;
		b = 1;
	} else {
		a = b = 0;
	}
	((unsigned char *)&ip)[j] = a;
	b += j * 8;
	
	sprintf(buf, "%u.%u.%u.%u/%u",
		((unsigned char *)&ip)[0],
		((unsigned char *)&ip)[1],
		((unsigned char *)&ip)[2],
		((unsigned char *)&ip)[3],
		b);

	DP("%s %s", ip_tostring(ntohl(ip), 0), buf);
	return buf;
}

static void
nethash_printips(struct set *set UNUSED, void *data, u_int32_t len,
		 unsigned options, char dont_align)
{
	size_t offset = 0;
	ip_set_ip_t *ip;

	while (offset < len) {
		ip = data + offset;
		printf("%s\n", unpack_ip_tostring(*ip, options));
		offset += IPSET_VALIGN(sizeof(ip_set_ip_t), dont_align);
	}
}

static void
nethash_saveheader(struct set *set, unsigned options UNUSED)
{
	struct ip_set_nethash *mysetdata = set->settype->header;

	printf("-N %s %s --hashsize %u --probes %u --resize %u\n",
	       set->name, set->settype->typename,
	       mysetdata->hashsize, mysetdata->probes, mysetdata->resize);
}

/* Print save for an IP */
static void
nethash_saveips(struct set *set UNUSED, void *data, u_int32_t len,
		unsigned options, char dont_align)
{
	size_t offset = 0;
	ip_set_ip_t *ip;

	while (offset < len) {
		ip = data + offset;
		printf("-A %s %s\n", set->name,
		       unpack_ip_tostring(*ip, options));
		offset += IPSET_VALIGN(sizeof(ip_set_ip_t), dont_align);
	}
}

static void
nethash_usage(void)
{
	printf
	    ("-N set nethash [--hashsize hashsize] [--probes probes ]\n"
	     "               [--resize resize]\n"
	     "-A set IP/cidr\n"
	     "-D set IP/cidr\n"
	     "-T set IP/cidr\n");
}

static struct settype settype_nethash = {
	.typename = SETTYPE_NAME,
	.protocol_version = IP_SET_PROTOCOL_VERSION,

	/* Create */
	.create_size = sizeof(struct ip_set_req_nethash_create),
	.create_init = nethash_create_init,
	.create_parse = nethash_create_parse,
	.create_final = nethash_create_final,
	.create_opts = create_opts,

	/* Add/del/test */
	.adt_size = sizeof(struct ip_set_req_nethash),
	.adt_parser = nethash_adt_parser,

	/* Printing */
	.header_size = sizeof(struct ip_set_nethash),
	.initheader = nethash_initheader,
	.printheader = nethash_printheader,
	.printips = nethash_printips,
	.printips_sorted = nethash_printips,
	.saveheader = nethash_saveheader,
	.saveips = nethash_saveips,
	
	.usage = nethash_usage,
};

CONSTRUCTOR(nethash)
{
	settype_register(&settype_nethash);

}
