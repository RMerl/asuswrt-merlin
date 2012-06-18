/*
 * libipt_osf.c
 *
 * Copyright (c) 2003 Evgeniy Polyakov <johnpol@2ka.mipt.ru>
 *
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

/*
 * iptables interface for OS fingerprint matching module.
 */

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_osf.h>

static void help(void)
{
	printf("OS fingerprint match options:\n"
		"--genre [!] string	Match a OS genre by passive fingerprinting.\n"
		"--smart		Use some smart extensions to determine OS (do not use TTL).\n"
		"--log level		Log all(or only first) determined genres even if "
					"they do not match desired one. "
					"Level may be 0(all) or 1(only first entry).\n"
		"--netlink		Log through netlink(NETLINK_NFLOG).\n",
		"--connector		Log through kernel connector [in 2.6.12-mm+].\n"
		);
}


static struct option opts[] = {
	{ .name = "genre",	.has_arg = 1, .flag = 0, .val = '1' },
	{ .name = "smart",	.has_arg = 0, .flag = 0, .val = '2' },
	{ .name = "log",	.has_arg = 1, .flag = 0, .val = '3' },
	{ .name = "netlink",	.has_arg = 0, .flag = 0, .val = '4' },
	{ .name = "connector",	.has_arg = 0, .flag = 0, .val = '5' },
	{ .name = 0 }
};

static void parse_string(const unsigned char *s, struct ipt_osf_info *info)
{
	if (strlen(s) < MAXGENRELEN) 
		strcpy(info->genre, s);
	else 
		exit_error(PARAMETER_PROBLEM, "Genre string too long `%s' [%d], max=%d", 
				s, strlen(s), MAXGENRELEN);
}

static int parse(int c, char **argv, int invert, unsigned int *flags,
      			const struct ipt_entry *entry,
      			unsigned int *nfcache,
      			struct ipt_entry_match **match)
{
	struct ipt_osf_info *info = (struct ipt_osf_info *)(*match)->data;
	
	switch(c) 
	{
		case '1': /* --genre */
			if (*flags & IPT_OSF_GENRE)
				exit_error(PARAMETER_PROBLEM, "Can't specify multiple genre parameter");
			check_inverse(optarg, &invert, &optind, 0);
			parse_string(argv[optind-1], info);
			if (invert)
				info->invert = 1;
			info->len=strlen((char *)info->genre);
			*flags |= IPT_OSF_GENRE;
			break;
		case '2': /* --smart */
			if (*flags & IPT_OSF_SMART)
				exit_error(PARAMETER_PROBLEM, "Can't specify multiple smart parameter");
			*flags |= IPT_OSF_SMART;
			info->flags |= IPT_OSF_SMART;
			break;
		case '3': /* --log */
			if (*flags & IPT_OSF_LOG)
				exit_error(PARAMETER_PROBLEM, "Can't specify multiple log parameter");
			*flags |= IPT_OSF_LOG;
			info->loglevel = atoi(argv[optind-1]);
			info->flags |= IPT_OSF_LOG;
			break;
		case '4': /* --netlink */
			if (*flags & IPT_OSF_NETLINK)
				exit_error(PARAMETER_PROBLEM, "Can't specify multiple netlink parameter");
			*flags |= IPT_OSF_NETLINK;
			info->flags |= IPT_OSF_NETLINK;
			break;
		case '5': /* --connector */
			if (*flags & IPT_OSF_CONNECTOR)
				exit_error(PARAMETER_PROBLEM, "Can't specify multiple connector parameter");
			*flags |= IPT_OSF_CONNECTOR;
			info->flags |= IPT_OSF_CONNECTOR;
			break;
		default:
			return 0;
	}

	return 1;
}

static void final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM, "OS fingerprint match: You must specify `--genre'");
}

static void print(const struct ipt_ip *ip, const struct ipt_entry_match *match, int numeric)
{
	const struct ipt_osf_info *info = (const struct ipt_osf_info*) match->data;

	printf("OS fingerprint match %s%s ", (info->invert) ? "!" : "", info->genre);
}

static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_osf_info *info = (const struct ipt_osf_info*) match->data;

	printf("--genre %s%s ", (info->invert) ? "! ": "", info->genre);
       if (info->flags & IPT_OSF_SMART)
               printf("--smart ");
       if (info->flags & IPT_OSF_LOG)
               printf("--log %d ", info->loglevel);
       if (info->flags & IPT_OSF_NETLINK)
               printf("--netlink ");
       if (info->flags & IPT_OSF_CONNECTOR)
               printf("--connector ");
}


static struct iptables_match osf_match = {
    .name          = "osf",
    .version       = IPTABLES_VERSION,
    .size          = IPT_ALIGN(sizeof(struct ipt_osf_info)),
    .userspacesize = IPT_ALIGN(sizeof(struct ipt_osf_info)),
    .help          = &help,
    .parse         = &parse,
    .final_check   = &final_check,
    .print         = &print,
    .save          = &save,
    .extra_opts    = opts
};


void _init(void)
{
	register_match(&osf_match);
}
