/* ebt_log
 *
 * Authors:
 * Bart De Schuymer <bdschuym@pandora.be>
 *
 * April, 2002
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include <linux/netfilter_bridge/ebt_log.h>

/*
 * copied from syslog.h
 * used for the LOG target
 */
#define	LOG_EMERG	0 /* system is unusable               */
#define	LOG_ALERT	1 /* action must be taken immediately */
#define	LOG_CRIT	2 /* critical conditions              */
#define	LOG_ERR		3 /* error conditions                 */
#define	LOG_WARNING	4 /* warning conditions               */
#define	LOG_NOTICE	5 /* normal but significant condition */
#define	LOG_INFO	6 /* informational                    */
#define	LOG_DEBUG	7 /* debug-level messages             */

#define LOG_DEFAULT_LEVEL LOG_INFO

typedef struct _code {
	char *c_name;
	int c_val;
} CODE;

static CODE eight_priority[] = {
	{ "emerg", LOG_EMERG },
	{ "alert", LOG_ALERT },
	{ "crit", LOG_CRIT },
	{ "error", LOG_ERR },
	{ "warning", LOG_WARNING },
	{ "notice", LOG_NOTICE },
	{ "info", LOG_INFO },
	{ "debug", LOG_DEBUG }
};

static int name_to_loglevel(char* arg)
{
	int i;
	
	for (i = 0; i < 8; i++)
		if (!strcmp(arg, eight_priority[i].c_name))
			return eight_priority[i].c_val;
	/* return bad loglevel */
	return 9;
}

#define LOG_PREFIX '1'
#define LOG_LEVEL  '2'
#define LOG_ARP    '3'
#define LOG_IP     '4'
#define LOG_LOG    '5'
#define LOG_IP6    '6'
static struct option opts[] =
{
	{ "log-prefix", required_argument, 0, LOG_PREFIX },
	{ "log-level" , required_argument, 0, LOG_LEVEL  },
	{ "log-arp"   , no_argument      , 0, LOG_ARP    },
	{ "log-ip"    , no_argument      , 0, LOG_IP     },
	{ "log"       , no_argument      , 0, LOG_LOG    },
	{ "log-ip6"   , no_argument      , 0, LOG_IP6    },
	{ 0 }
};

static void print_help()
{
	int i;

	printf(
"log options:\n"
"--log               : use this if you're not specifying anything\n"
"--log-level level   : level = [1-8] or a string\n"
"--log-prefix prefix : max. %d chars.\n"
"--log-ip            : put ip info. in the log for ip packets\n"
"--log-arp           : put (r)arp info. in the log for (r)arp packets\n"
"--log-ip6           : put ip6 info. in the log for ip6 packets\n"
	, EBT_LOG_PREFIX_SIZE - 1);
	printf("levels:\n");
	for (i = 0; i < 8; i++)
		printf("%d = %s\n", eight_priority[i].c_val,
		   eight_priority[i].c_name);
}

static void init(struct ebt_entry_watcher *watcher)
{
	struct ebt_log_info *loginfo = (struct ebt_log_info *)watcher->data;

	loginfo->bitmask = 0;
	loginfo->prefix[0] = '\0';
	loginfo->loglevel = LOG_NOTICE;
}

#define OPT_PREFIX 0x01
#define OPT_LEVEL  0x02
#define OPT_ARP    0x04
#define OPT_IP     0x08
#define OPT_LOG    0x10
#define OPT_IP6    0x20
static int parse(int c, char **argv, int argc, const struct ebt_u_entry *entry,
   unsigned int *flags, struct ebt_entry_watcher **watcher)
{
	struct ebt_log_info *loginfo = (struct ebt_log_info *)(*watcher)->data;
	long int i;
	char *end;

	switch (c) {
	case LOG_PREFIX:
		ebt_check_option2(flags, OPT_PREFIX);
		if (ebt_check_inverse(optarg))
			ebt_print_error2("Unexpected `!' after --log-prefix");
		if (strlen(optarg) > sizeof(loginfo->prefix) - 1)
			ebt_print_error2("Prefix too long");
		if (strchr(optarg, '\"'))
			ebt_print_error2("Use of \\\" is not allowed in the prefix");
		strcpy((char *)loginfo->prefix, (char *)optarg);
		break;

	case LOG_LEVEL:
		ebt_check_option2(flags, OPT_LEVEL);
		i = strtol(optarg, &end, 16);
		if (*end != '\0' || i < 0 || i > 7)
			loginfo->loglevel = name_to_loglevel(optarg);
		else
			loginfo->loglevel = i;
		if (loginfo->loglevel == 9)
			ebt_print_error2("Problem with the log-level");
		break;

	case LOG_IP:
		ebt_check_option2(flags, OPT_IP);
		if (ebt_check_inverse(optarg))
			ebt_print_error2("Unexpected `!' after --log-ip");
		loginfo->bitmask |= EBT_LOG_IP;
		break;

	case LOG_ARP:
		ebt_check_option2(flags, OPT_ARP);
		if (ebt_check_inverse(optarg))
			ebt_print_error2("Unexpected `!' after --log-arp");
		loginfo->bitmask |= EBT_LOG_ARP;
		break;

	case LOG_LOG:
		ebt_check_option2(flags, OPT_LOG);
		if (ebt_check_inverse(optarg))
			ebt_print_error2("Unexpected `!' after --log");
		break;

	case LOG_IP6:
		ebt_check_option2(flags, OPT_IP6);
		if (ebt_check_inverse(optarg))
			ebt_print_error2("Unexpected `!' after --log-ip6");
		loginfo->bitmask |= EBT_LOG_IP6;
		break;
	default:
		return 0;
	}
	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_watcher *watcher, const char *name,
   unsigned int hookmask, unsigned int time)
{
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_watcher *watcher)
{
	struct ebt_log_info *loginfo = (struct ebt_log_info *)watcher->data;

	printf("--log-level %s --log-prefix \"%s\"",
		eight_priority[loginfo->loglevel].c_name,
		loginfo->prefix);
	if (loginfo->bitmask & EBT_LOG_IP)
		printf(" --log-ip");
	if (loginfo->bitmask & EBT_LOG_ARP)
		printf(" --log-arp");
	if (loginfo->bitmask & EBT_LOG_IP6)
		printf(" --log-ip6");
	printf(" ");
}

static int compare(const struct ebt_entry_watcher *w1,
   const struct ebt_entry_watcher *w2)
{
	struct ebt_log_info *loginfo1 = (struct ebt_log_info *)w1->data;
	struct ebt_log_info *loginfo2 = (struct ebt_log_info *)w2->data;

	if (loginfo1->loglevel != loginfo2->loglevel)
		return 0;
	if (loginfo1->bitmask != loginfo2->bitmask)
		return 0;
	return !strcmp((char *)loginfo1->prefix, (char *)loginfo2->prefix);
}

static struct ebt_u_watcher log_watcher =
{
	.name		= "log",
	.size		= sizeof(struct ebt_log_info),
	.help		= print_help,
	.init		= init,
	.parse		= parse,
	.final_check	= final_check,
	.print		= print,
	.compare	= compare,
	.extra_ops	= opts,
};

void _init(void)
{
	ebt_register_watcher(&log_watcher);
}
