/* 
 * accounting match helper (libipt_account.c)
 * (C) 2003,2004 by Piotr Gasid³o (quaker@barbara.eu.org)
 *
 * Version: 0.1.6
 *
 * This software is distributed under the terms of GNU GPL
 */

#include <stdio.h>
#include <stdlib.h>
#include <iptables.h>
#include <string.h>
#include <getopt.h>

#include <linux/netfilter_ipv4/ipt_account.h>

#ifndef HIPQUAD
#define HIPQUAD(addr) \
	((unsigned char *)&addr)[3], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[0]
#endif
				
static void help(void) {
	printf(
			"account v%s options:\n"
			"--aaddr network/netmask\n"
			"	defines network/netmask for which make statistics.\n"
			"--aname name\n"
			"	defines name of list where statistics will be kept. If no is\n"
			"	specified DEFAULT will be used.\n"
			"--ashort\n"
			"       table will colect only short statistics (only total counters\n"
			"       without splitting it into protocols.\n"
	, 
	IPTABLES_VERSION);
};

static struct option opts[] = {
	{ .name = "aaddr",  .has_arg = 1, .flag = NULL, .val = 201 },
	{ .name = "aname",  .has_arg = 1, .flag = NULL, .val = 202 },
	{ .name = "ashort", .has_arg = 0, .flag = NULL, .val = 203 },
	{ .name = 0, .has_arg = 0, .flag = 0, .val = 0 }
};

/* Helper functions for parse_network */
int parseip(const char *parameter, u_int32_t *ip) {
	
	char buffer[16], *bufferptr, *dot;
	unsigned int i, shift, part;

	if (strlen(parameter) > 15)
		return 0;

	strncpy(buffer, parameter, 15);
	buffer[15] = 0;

	bufferptr = buffer;

	for (i = 0, shift = 24, *ip = 0; i < 3; i++, shift -= 8) {
		/* no dot */
		if ((dot = strchr(bufferptr, '.')) == NULL)
			return 0;
		/* not a number */
		if ((part = strtol(bufferptr, (char**)NULL, 10)) < 0) 
			return 0;	
		/* to big number */
		if (part > 255)
			return 0;
		*ip |= part << shift;		
		bufferptr = dot + 1;
	}
	/* not a number */
	if ((part = strtol(bufferptr, (char**)NULL, 10)) < 0) 
		return 0;
	/* to big number */
	if (part > 255)
		return 0;
	*ip |= part;
	return 1;
}

static void parsenetwork(const char *parameter, u_int32_t *network) {
	if (!parseip(parameter, network))
		exit_error(PARAMETER_PROBLEM, "account: wrong ip in network");
}

static void parsenetmaskasbits(const char *parameter, u_int32_t *netmask) {
	
	u_int32_t bits;
	
	if ((bits = strtol(parameter, (char **)NULL, 10)) < 0 || bits > 32)
		exit_error(PARAMETER_PROBLEM, "account: wrong netmask");

	*netmask = 0xffffffff << (32 - bits);
}

static void parsenetmaskasip(const char *parameter, u_int32_t *netmask) {
	if (!parseip(parameter, netmask))
		exit_error(PARAMETER_PROBLEM, "account: wrong ip in netmask");
}

static void parsenetmask(const char *parameter, u_int32_t *netmask) 
{
	if (strchr(parameter, '.') != NULL)
		parsenetmaskasip(parameter, netmask);
	else
		parsenetmaskasbits(parameter, netmask);
}

static void parsenetworkandnetmask(const char *parameter, u_int32_t *network, u_int32_t *netmask) 
{
	
	char buffer[32], *slash;

	if (strlen(parameter) > 31)
		/* text is to long, even for 255.255.255.255/255.255.255.255 */
		exit_error(PARAMETER_PROBLEM, "account: wrong network/netmask");

	strncpy(buffer, parameter, 31);
	buffer[31] = 0;

	/* check whether netmask is given */
	if ((slash = strchr(buffer, '/')) != NULL) {
		parsenetmask(slash + 1, netmask);
		*slash = 0;
	} else
		*netmask = 0xffffffff;
	parsenetwork(buffer, network);

	if ((*network & *netmask) != *network)
		exit_error(PARAMETER_PROBLEM, "account: wrong network/netmask");
}


/* Function gets network & netmask from argument after --aaddr */
static void parse_network(const char *parameter, struct t_ipt_account_info *info) {

	parsenetworkandnetmask(parameter, &info->network, &info->netmask);
	
}

/* validate netmask */
inline int valid_netmask(u_int32_t netmask) {
	while (netmask & 0x80000000)
		netmask <<= 1;
	if (netmask != 0)
		return 0;
        return 1;
}

/* validate network/netmask pair */
inline int valid_network_and_netmask(struct t_ipt_account_info *info) {
	if (!valid_netmask(info->netmask))
		return 0;
	if ((info->network & info->netmask) != info->network)
		return 0;
	return 1;
}



/* Function initializes match */
static void init(struct ipt_entry_match *match, 
		 unsigned int *nfcache) {
	
	struct t_ipt_account_info *info = (struct t_ipt_account_info *)(match)->data;


	/* set default table name to DEFAULT */
	strncpy(info->name, "DEFAULT", IPT_ACCOUNT_NAME_LEN);
	info->shortlisting = 0;
	
}

/* Function parses match's arguments */
static int parse(int c, char **argv, 
		  int invert, 
		  unsigned int *flags,
                  const struct ipt_entry *entry,
                  unsigned int *nfcache,
                  struct ipt_entry_match **match) {
	
	struct t_ipt_account_info *info = (struct t_ipt_account_info *)(*match)->data;

	switch (c) {
		
		/* --aaddr */
		case 201:
			parse_network(optarg, info);
			if (!valid_network_and_netmask(info))
				exit_error(PARAMETER_PROBLEM, "account: wrong network/netmask");
			*flags = 1;
			break;
			
		/* --aname */
		case 202:
			if (strlen(optarg) < IPT_ACCOUNT_NAME_LEN)
				strncpy(info->name, optarg, IPT_ACCOUNT_NAME_LEN);
			else
				exit_error(PARAMETER_PROBLEM, "account: Too long table name");			
			break;	
		/* --ashort */
		case 203:
			info->shortlisting = 1;
			break;
		default:
			return 0;			
	}
	return 1;	
}

/* Final check whether network/netmask was specified */
static void final_check(unsigned int flags) {
	if (!flags)
		exit_error(PARAMETER_PROBLEM, "account: You need specify '--aaddr' parameter");
}

/* Function used for printing rule with account match for iptables -L */
static void print(const struct ipt_ip *ip,
                  const struct ipt_entry_match *match, 
		  int numeric) {
	
	struct t_ipt_account_info *info = (struct t_ipt_account_info *)match->data;
	
	printf("account: ");
	printf("network/netmask: ");
	printf("%u.%u.%u.%u/%u.%u.%u.%u ",
			HIPQUAD(info->network),
			HIPQUAD(info->netmask)
	      );
	
	printf("name: %s ", info->name);
	if (info->shortlisting)
		printf("short-listing ");
}

/* Function used for saving rule containing account match */
static void save(const struct ipt_ip *ip, 
		 const struct ipt_entry_match *match) {

	struct t_ipt_account_info *info = (struct t_ipt_account_info *)match->data;
	
	printf("--aaddr ");
	printf("%u.%u.%u.%u/%u.%u.%u.%u ",
			 HIPQUAD(info->network),
			 HIPQUAD(info->netmask)
	       );
	
	printf("--aname %s ", info->name);
	if (info->shortlisting)
		printf("--ashort ");
}
	
static struct iptables_match account = {
	.next = NULL,
	.name = "account",
	.version = IPTABLES_VERSION,
	.size = IPT_ALIGN(sizeof(struct t_ipt_account_info)),
	.userspacesize = IPT_ALIGN(sizeof(struct t_ipt_account_info)),
	.help = &help,
	.init = &init,
	.parse = &parse,
	.final_check = &final_check,
	.print = &print,
	.save = &save,
	.extra_opts = opts
};

/* Function which registers match */
void _init(void)
{
	register_match(&account);
}
	
