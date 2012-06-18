#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <iptables.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter/xt_NFLOG.h>

enum {
	NFLOG_GROUP	= 0x1,
	NFLOG_PREFIX	= 0x2,
	NFLOG_RANGE	= 0x4,
	NFLOG_THRESHOLD	= 0x8,
};

static struct option opts[] = {
	{ "nflog-group",     1, 0, NFLOG_GROUP },
	{ "nflog-prefix",    1, 0, NFLOG_PREFIX },
	{ "nflog-range",     1, 0, NFLOG_RANGE },
	{ "nflog-threshold", 1, 0, NFLOG_THRESHOLD },
};

static void help(void)
{
	printf("NFLOG v%s options:\n"
	       " --nflog-group NUM		NETLINK group used for logging\n"
	       " --nflog-range NUM		Number of byte to copy\n"
	       " --nflog-threshold NUM		Message threshold of in-kernel queue\n"
	       " --nflog-prefix STRING		Prefix string for log messages\n\n",
	       IPTABLES_VERSION);
}

static void init(struct xt_entry_target *t, unsigned int *nfcache)
{
	struct xt_nflog_info *info = (struct xt_nflog_info *)t->data;

	info->group	= 0;
	info->threshold	= XT_NFLOG_DEFAULT_THRESHOLD;
}

static int parse(int c, char **argv, int invert, unsigned int *flags,
		 const struct ipt_entry *entry,
		 struct xt_entry_target **target)
{
	struct xt_nflog_info *info = (struct xt_nflog_info *)(*target)->data;
	int n;

	switch (c) {
	case NFLOG_GROUP:
		if (*flags & NFLOG_GROUP)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --nflog-group twice");
		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --nflog-group");

		n = atoi(optarg);
		if (n < 0)
			exit_error(PARAMETER_PROBLEM,
				   "--nflog-group can not be negative");
		info->group = n;
		break;
	case NFLOG_PREFIX:
		if (*flags & NFLOG_PREFIX)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --nflog-prefix twice");
		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --nflog-prefix");

		n = strlen(optarg);
		if (n == 0)
			exit_error(PARAMETER_PROBLEM,
				   "No prefix specified for --nflog-prefix");
		if (n >= sizeof(info->prefix))
			exit_error(PARAMETER_PROBLEM,
				   "--nflog-prefix too long, max %Zu characters",
				   sizeof(info->prefix) - 1);
		if (n != strlen(strtok(optarg, "\n")))
			exit_error(PARAMETER_PROBLEM,
				   "Newlines are not allowed in --nflog-prefix");
		strcpy(info->prefix, optarg);
		break;
	case NFLOG_RANGE:
		if (*flags & NFLOG_RANGE)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --nflog-range twice");
		n = atoi(optarg);
		if (n < 0)
			exit_error(PARAMETER_PROBLEM,
				   "Invalid --nflog-range, must be >= 0");
		info->len = n;
		break;
	case NFLOG_THRESHOLD:
		if (*flags & NFLOG_THRESHOLD)
			exit_error(PARAMETER_PROBLEM,
				   "Can't specify --nflog-threshold twice");
		n = atoi(optarg);
		if (n < 1)
			exit_error(PARAMETER_PROBLEM,
				   "Invalid --nflog-threshold, must be >= 1");
		info->threshold = n;
		break;
	default:
		return 0;
	}
	*flags |= c;
	return 1;
}

static void final_check(unsigned int flags)
{
	return;
}

static void nflog_print(const struct xt_nflog_info *info, char *prefix)
{
	if (info->prefix[0] != '\0')
		printf("%snflog-prefix \"%s\" ", prefix, info->prefix);
	if (info->group)
		printf("%snflog-group %u ", prefix, info->group);
	if (info->len)
		printf("%snflog-range %u ", prefix, info->len);
	if (info->threshold != XT_NFLOG_DEFAULT_THRESHOLD)
		printf("%snflog-threshold %u ", prefix, info->threshold);
}

static void print(const struct ipt_ip *ip, const struct xt_entry_target *target,
		  int numeric)
{
	const struct xt_nflog_info *info = (struct xt_nflog_info *)target->data;

	nflog_print(info, "");
}

static void save(const struct ipt_ip *ip, const struct xt_entry_target *target)
{
	const struct xt_nflog_info *info = (struct xt_nflog_info *)target->data;

	nflog_print(info, "--");
}

static struct iptables_target nflog = {
	.name		= "NFLOG",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_nflog_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_nflog_info)),
	.help		= help,
	.init		= init,
	.parse		= parse,
	.final_check	= final_check,
	.print		= print,
	.save		= save,
	.extra_opts	= opts,
};

void _init(void)
{
	register_target(&nflog);
}
