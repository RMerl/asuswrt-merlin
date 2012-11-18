/* Copyright 2005 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
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

#include <linux/netfilter_ipv4/ip_set_iptree.h>

#define BUFLEN 30;

#define OPT_CREATE_TIMEOUT    0x01U

/* Initialize the create. */
static void
iptree_create_init(void *data)
{
	struct ip_set_req_iptree_create *mydata = data;

	DP("create INIT");
	mydata->timeout = 0;
}

/* Function which parses command options; returns true if it ate an option */
static int
iptree_create_parse(int c, char *argv[] UNUSED, void *data, unsigned *flags)
{
	struct ip_set_req_iptree_create *mydata = data;

	DP("create_parse");

	switch (c) {
	case '1':
		string_to_number(optarg, 0, UINT_MAX, &mydata->timeout);

		*flags |= OPT_CREATE_TIMEOUT;

		DP("--timeout %u", mydata->timeout);

		break;
	default:
		return 0;
	}

	return 1;
}

/* Final check; exit if not ok. */
static void
iptree_create_final(void *data UNUSED, unsigned int flags UNUSED)
{
}

/* Create commandline options */
static const struct option create_opts[] = {
	{.name = "timeout",	.has_arg = required_argument,	.val = '1'},
	{NULL},
};

/* Add, del, test parser */
static ip_set_ip_t
iptree_adt_parser(int cmd UNUSED, const char *arg, void *data)
{
	struct ip_set_req_iptree *mydata = data;
	char *saved = ipset_strdup(arg);
	char *ptr, *tmp = saved;

	DP("iptree: %p %p", arg, data);

	if (((ptr = strchr(tmp, ':')) || (ptr = strchr(tmp, '%'))) && ++warn_once == 1)
		fprintf(stderr, "Warning: please use ',' separator token between ip,timeout.\n"
			        "Next release won't support old separator tokens.\n");

	ptr = strsep(&tmp, ":%,");
	parse_ip(ptr, &mydata->ip);

	if (tmp)
		string_to_number(tmp, 0, UINT_MAX, &mydata->timeout);
	else
		mydata->timeout = 0;	

	ipset_free(saved);
	return 1;	
}

/*
 * Print and save
 */

static void
iptree_initheader(struct set *set, const void *data)
{
	const struct ip_set_req_iptree_create *header = data;
	struct ip_set_iptree *map = set->settype->header;
		
	map->timeout = header->timeout;
}

static void
iptree_printheader(struct set *set, unsigned options UNUSED)
{
	struct ip_set_iptree *mysetdata = set->settype->header;

	if (mysetdata->timeout)
		printf(" timeout: %u", mysetdata->timeout);
	printf("\n");
}

static void
iptree_printips_sorted(struct set *set, void *data, u_int32_t len,
		       unsigned options, char dont_align)
{
	struct ip_set_iptree *mysetdata = set->settype->header;
	struct ip_set_req_iptree *req;
	size_t offset = 0;

	while (len >= offset + sizeof(struct ip_set_req_iptree)) {
		req = (struct ip_set_req_iptree *)(data + offset);
		if (mysetdata->timeout)
			printf("%s,%u\n", ip_tostring(req->ip, options),
					  req->timeout);
		else
			printf("%s\n", ip_tostring(req->ip, options));
		offset += IPSET_VALIGN(sizeof(struct ip_set_req_iptree), dont_align);
	}
}

static void
iptree_saveheader(struct set *set, unsigned options UNUSED)
{
	struct ip_set_iptree *mysetdata = set->settype->header;

	if (mysetdata->timeout)
		printf("-N %s %s --timeout %u\n",
		       set->name, set->settype->typename,
		       mysetdata->timeout);
	else
		printf("-N %s %s\n",
		       set->name, set->settype->typename);
}

static void
iptree_saveips(struct set *set, void *data, u_int32_t len,
	       unsigned options, char dont_align)
{
	struct ip_set_iptree *mysetdata = set->settype->header;
	struct ip_set_req_iptree *req;
	size_t offset = 0;

	DP("%s", set->name);

	while (len >= offset + sizeof(struct ip_set_req_iptree)) {
		req = (struct ip_set_req_iptree *)(data + offset);
		if (mysetdata->timeout)
			printf("-A %s %s,%u\n",
				set->name, 
				ip_tostring(req->ip, options),
				req->timeout);
		else
			printf("-A %s %s\n", 
				set->name,
				ip_tostring(req->ip, options));
		offset += IPSET_VALIGN(sizeof(struct ip_set_req_iptree), dont_align);
	}
}

static void
iptree_usage(void)
{
	printf
	    ("-N set iptree [--timeout value]\n"
	     "-A set IP[,timeout]\n"
	     "-D set IP\n"
	     "-T set IP\n");
}

static struct settype settype_iptree = {
	.typename = SETTYPE_NAME,
	.protocol_version = IP_SET_PROTOCOL_VERSION,

	/* Create */
	.create_size = sizeof(struct ip_set_req_iptree_create),
	.create_init = iptree_create_init,
	.create_parse = iptree_create_parse,
	.create_final = iptree_create_final,
	.create_opts = create_opts,

	/* Add/del/test */
	.adt_size = sizeof(struct ip_set_req_iptree),
	.adt_parser = iptree_adt_parser,

	/* Printing */
	.header_size = sizeof(struct ip_set_iptree),
	.initheader = iptree_initheader,
	.printheader = iptree_printheader,
	.printips = iptree_printips_sorted,	/* We only have sorted version */
	.printips_sorted = iptree_printips_sorted,
	.saveheader = iptree_saveheader,
	.saveips = iptree_saveips,
	
	.usage = iptree_usage,
};

CONSTRUCTOR(iptree)
{
	settype_register(&settype_iptree);

}
