/* Shared library add-on to iptables to add CONNMARK target support.
 *
 * (C) 2002,2004 MARA Systems AB <http://www.marasystems.com>
 * by Henrik Nordstrom <hno@marasystems.com>
 *
 * Version 1.1
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include "../include/linux/netfilter_ipv4/ipt_CONNMARK.h"

#if 0
struct markinfo {
	struct ipt_entry_target t;
	struct ipt_connmark_target_info mark;
};
#endif

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"CONNMARK target v%s options:\n"
"  --set-mark value[/mask]       Set conntrack mark value\n"
"  --save-mark [--mask mask]     Save the packet nfmark in the connection\n"
"  --restore-mark [--mask mask]  Restore saved nfmark value\n"
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "set-mark", 1, 0, '1' },
	{ "save-mark", 0, 0, '2' },
	{ "restore-mark", 0, 0, '3' },
	{ "mask", 1, 0, '4' },
	{ 0 }
};

/* Initialize the target. */
static void
init(struct ipt_entry_target *t, unsigned int *nfcache)
{
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ipt_connmark_target_info *markinfo
		= (struct ipt_connmark_target_info *)(*target)->data;

	markinfo->mask = 0xffffffffUL;

	switch (c) {
		char *end;
	case '1':
		markinfo->mode = IPT_CONNMARK_SET;

		markinfo->mark = strtoul(optarg, &end, 0);
		if (*end == '/' && end[1] != '\0')
		    markinfo->mask = strtoul(end+1, &end, 0);

		if (*end != '\0' || end == optarg)
			exit_error(PARAMETER_PROBLEM, "Bad MARK value `%s'", optarg);
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "CONNMARK target: Can't specify --set-mark twice");
		*flags = 1;
		break;
	case '2':
		markinfo->mode = IPT_CONNMARK_SAVE;
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "CONNMARK target: Can't specify --save-mark twice");
		*flags = 1;
		break;
	case '3':
		markinfo->mode = IPT_CONNMARK_RESTORE;
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "CONNMARK target: Can't specify --restore-mark twice");
		*flags = 1;
		break;
	case '4':
		if (!*flags)
			exit_error(PARAMETER_PROBLEM,
			           "CONNMARK target: Can't specify --mask without a operation");
		markinfo->mask = strtoul(optarg, &end, 0);

		if (*end != '\0' || end == optarg)
			exit_error(PARAMETER_PROBLEM, "Bad MASK value `%s'", optarg);
		break;
	default:
		return 0;
	}

	return 1;
}

static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
		           "CONNMARK target: No operation specified");
}

static void
print_mark(unsigned long mark)
{
	printf("0x%lx", mark);
}

static void
print_mask(const char *text, unsigned long mask)
{
	if (mask != 0xffffffffUL)
		printf("%s0x%lx", text, mask);
}


/* Prints out the target info. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	const struct ipt_connmark_target_info *markinfo =
		(const struct ipt_connmark_target_info *)target->data;
	switch (markinfo->mode) {
	case IPT_CONNMARK_SET:
	    printf("CONNMARK set ");
	    print_mark(markinfo->mark);
	    print_mask("/", markinfo->mask);
	    printf(" ");
	    break;
	case IPT_CONNMARK_SAVE:
	    printf("CONNMARK save ");
	    print_mask("mask ", markinfo->mask);
	    printf(" ");
	    break;
	case IPT_CONNMARK_RESTORE:
	    printf("CONNMARK restore ");
	    print_mask("mask ", markinfo->mask);
	    break;
	default:
	    printf("ERROR: UNKNOWN CONNMARK MODE ");
	    break;
	}
}

/* Saves the target into in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	const struct ipt_connmark_target_info *markinfo =
		(const struct ipt_connmark_target_info *)target->data;

	switch (markinfo->mode) {
	case IPT_CONNMARK_SET:
	    printf("--set-mark ");
	    print_mark(markinfo->mark);
	    print_mask("/", markinfo->mask);
	    printf(" ");
	    break;
	case IPT_CONNMARK_SAVE:
	    printf("--save-mark ");
	    print_mask("--mask ", markinfo->mask);
	    break;
	case IPT_CONNMARK_RESTORE:
	    printf("--restore-mark ");
	    print_mask("--mask ", markinfo->mask);
	    break;
	default:
	    printf("ERROR: UNKNOWN CONNMARK MODE ");
	    break;
	}
}

static struct iptables_target connmark_target = {
    .name          = "CONNMARK",
    .version       = IPTABLES_VERSION,
    .size          = IPT_ALIGN(sizeof(struct ipt_connmark_target_info)),
    .userspacesize = IPT_ALIGN(sizeof(struct ipt_connmark_target_info)),
    .help          = &help,
    .init          = &init,
    .parse         = &parse,
    .final_check   = &final_check,
    .print         = &print,
    .save          = &save,
    .extra_opts    = opts
};

void _init(void)
{
	register_target(&connmark_target);
}
