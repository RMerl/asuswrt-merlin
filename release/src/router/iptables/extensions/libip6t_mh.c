/* Shared library add-on to ip6tables to add mobility header support. */
/*
 * Copyright (C)2006 USAGI/WIDE Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author:
 *	Masahide NAKAMURA @USAGI <masahide.nakamura.cz@hitachi.com>
 *
 * Based on libip6t_{icmpv6,udp}.c
 */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_mh.h>

struct mh_name {
	const char *name;
	u_int8_t type;
};

static const struct mh_name mh_names[] = {
	{ "binding-refresh-request", 0, },
	/* Alias */ { "brr", 0, },
	{ "home-test-init", 1, },
	/* Alias */ { "hoti", 1, },
	{ "careof-test-init", 2, },
	/* Alias */ { "coti", 2, },
	{ "home-test", 3, },
	/* Alias */ { "hot", 3, },
	{ "careof-test", 4, },
	/* Alias */ { "cot", 4, },
	{ "binding-update", 5, },
	/* Alias */ { "bu", 5, },
	{ "binding-acknowledgement", 6, },
	/* Alias */ { "ba", 6, },
	{ "binding-error", 7, },
	/* Alias */ { "be", 7, },
};

static void print_types_all(void)
{
	unsigned int i;
	printf("Valid MH types:");

	for (i = 0; i < sizeof(mh_names)/sizeof(struct mh_name); i++) {
		if (i && mh_names[i].type == mh_names[i-1].type)
			printf(" (%s)", mh_names[i].name);
		else
			printf("\n%s", mh_names[i].name);
	}
	printf("\n");
}

static void help(void)
{
	printf(
"MH v%s options:\n"
" --mh-type [!] type[:type]	match mh type\n",
IPTABLES_VERSION);
	print_types_all();
}

static void init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
	struct ip6t_mh *mhinfo = (struct ip6t_mh *)m->data;

	mhinfo->types[1] = 0xFF;
}

static unsigned int name_to_type(const char *name)
{
	int namelen = strlen(name);
	unsigned int limit = sizeof(mh_names)/sizeof(struct mh_name);
	unsigned int match = limit;
	unsigned int i;

	for (i = 0; i < limit; i++) {
		if (strncasecmp(mh_names[i].name, name, namelen) == 0) {
			int len = strlen(mh_names[i].name);
			if (match == limit || len == namelen)
				match = i;
		}
	}

	if (match != limit) {
		return mh_names[match].type;
	} else {
		unsigned int number;

		if (string_to_number(name, 0, 255, &number) == -1)
			exit_error(PARAMETER_PROBLEM,
				   "Invalid MH type `%s'\n", name);
		return number;
	}
}

static void parse_mh_types(const char *mhtype, u_int8_t *types)
{
	char *buffer;
	char *cp;

	buffer = strdup(mhtype);
	if ((cp = strchr(buffer, ':')) == NULL)
		types[0] = types[1] = name_to_type(buffer);
	else {
		*cp = '\0';
		cp++;

		types[0] = buffer[0] ? name_to_type(buffer) : 0;
		types[1] = cp[0] ? name_to_type(cp) : 0xFF;

		if (types[0] > types[1])
			exit_error(PARAMETER_PROBLEM,
				   "Invalid MH type range (min > max)");
	}
	free(buffer);
}

#define MH_TYPES 0x01

static int parse(int c, char **argv, int invert, unsigned int *flags,
		 const struct ip6t_entry *entry,
		 unsigned int *nfcache,
		 struct ip6t_entry_match **match)
{
	struct ip6t_mh *mhinfo = (struct ip6t_mh *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & MH_TYPES)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--mh-type' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_mh_types(argv[optind-1], mhinfo->types);
		if (invert)
			mhinfo->invflags |= IP6T_MH_INV_TYPE;
		*flags |= MH_TYPES;
		break;

	default:
		return 0;
	}

	return 1;
}

/* Final check; we don't care. */
static void final_check(unsigned int flags)
{
}

static const char *type_to_name(u_int8_t type)
{
	unsigned int i;

	for (i = 0; i < sizeof(mh_names)/sizeof(struct mh_name); i++) {
		if (mh_names[i].type == type)
			return mh_names[i].name;
	}

	return NULL;
}

static void print_type(u_int8_t type, int numeric)
{
	const char *name;
	if (numeric || !(name = type_to_name(type)))
		printf("%u", type);
	else
		printf("%s", name);
}

static void print_types(u_int8_t min, u_int8_t max, int invert, int numeric)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFF || invert) {
		if (min == max) {
			printf("%s", inv);
			print_type(min, numeric);
		} else {
			printf("%s", inv);
			print_type(min, numeric);
			printf(":");
			print_type(max, numeric);
		}
		printf(" ");
	}
}

static void print(const struct ip6t_ip6 *ip,
		  const struct ip6t_entry_match *match,
		  int numeric)
{
	const struct ip6t_mh *mhinfo = (struct ip6t_mh *)match->data;

	printf("mh ");
	print_types(mhinfo->types[0], mhinfo->types[1],
		    mhinfo->invflags & IP6T_MH_INV_TYPE,
		    numeric);
	if (mhinfo->invflags & ~IP6T_MH_INV_MASK)
		printf("Unknown invflags: 0x%X ",
		       mhinfo->invflags & ~IP6T_MH_INV_MASK);
}

static void save(const struct ip6t_ip6 *ip,
		 const struct ip6t_entry_match *match)
{
	const struct ip6t_mh *mhinfo = (struct ip6t_mh *)match->data;

	if (mhinfo->types[0] == 0 && mhinfo->types[1] == 0xFF)
		return;

	if (mhinfo->invflags & IP6T_MH_INV_TYPE)
		printf("! ");

	if (mhinfo->types[0] != mhinfo->types[1])
		printf("--mh-type %u:%u ", mhinfo->types[0], mhinfo->types[1]);
	else
		printf("--mh-type %u ", mhinfo->types[0]);
}

static struct option opts[] = {
	{ "mh-type", 1, 0, '1' },
	{0}
};

static struct ip6tables_match mh = {
	.name		= "mh",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ip6t_mh)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ip6t_mh)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts,
};

void _init(void)
{
	register_match6(&mh);
}
