/* Shared library add-on to iptables to add connection rate tracking
 * support.
 *
 * Copyright (c) 2004 Nuutti Kotivuori <naked@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 **/
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter_ipv4/ipt_connrate.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"connrate v%s options:\n"
" --connrate [!] [from]:[to]\n"
"				Match connection transfer rate in bytes\n"
"				per second. `inf' can be used for maximum\n"
"				expressible value.\n"
"\n", IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "connrate", 1, 0, '1' },
	{0}
};

static u_int32_t
parse_value(const char *arg, u_int32_t def)
{
	char *end;
	size_t len;
	u_int32_t value;

	len = strlen(arg);
	if(len == 0)
		return def;
	if(strcmp(arg, "inf") == 0)
		return 0xFFFFFFFF;
	value = strtoul(arg, &end, 0);
	if(*end != '\0')
		exit_error(PARAMETER_PROBLEM,
			   "Bad value in range `%s'", arg);
	return value;
}

static void
parse_range(const char *arg, struct ipt_connrate_info *si)
{
	char *buffer;
	char *colon;

	buffer = strdup(arg);
	if ((colon = strchr(buffer, ':')) == NULL)
		exit_error(PARAMETER_PROBLEM, "Bad range `%s'", arg);
	*colon = '\0';
	si->from = parse_value(buffer, 0);
	si->to = parse_value(colon+1, 0xFFFFFFFF);
	if (si->from > si->to)
		exit_error(PARAMETER_PROBLEM, "%u should be less than %u", si->from,si->to);
	free(buffer);
}

#define CONNRATE_OPT 0x01

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_connrate_info *sinfo = (struct ipt_connrate_info *)(*match)->data;
	u_int32_t tmp;

	switch (c) {
	case '1':
		if (*flags & CONNRATE_OPT)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--connrate' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_range(argv[optind-1], sinfo);
		if (invert) {
			tmp = sinfo->from;
			sinfo->from = sinfo->to;
			sinfo->to = tmp;
		}
		*flags |= CONNRATE_OPT;
		break;

	default:
		return 0;
	}

	return 1;
}

static void final_check(unsigned int flags)
{
	if (!(flags & CONNRATE_OPT))
		exit_error(PARAMETER_PROBLEM,
			   "connrate match: You must specify `--connrate'");
}

static void
print_value(u_int32_t value)
{
	if(value == 0xFFFFFFFF)
		printf("inf");
	else
		printf("%u", value);
}

static void
print_range(struct ipt_connrate_info *sinfo)
{
	if (sinfo->from > sinfo->to) {
		printf("! ");
		print_value(sinfo->to);
		printf(":");
		print_value(sinfo->from);
	} else {
		print_value(sinfo->from);
		printf(":");
		print_value(sinfo->to);
	}
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_connrate_info *sinfo = (struct ipt_connrate_info *)match->data;

	printf("connrate ");
	print_range(sinfo);
	printf(" ");
}

/* Saves the matchinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	struct ipt_connrate_info *sinfo = (struct ipt_connrate_info *)match->data;

	printf("--connrate ");
	print_range(sinfo);
	printf(" ");
}

static struct iptables_match state = { 
	.next 		= NULL,
	.name		= "connrate",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_connrate_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_connrate_info)),
	.help 		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&state);
}
