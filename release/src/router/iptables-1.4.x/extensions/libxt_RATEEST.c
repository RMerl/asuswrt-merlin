#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_RATEEST.h>

struct rateest_tg_udata {
	unsigned int interval;
	unsigned int ewma_log;
};

static void
RATEEST_help(void)
{
	printf(
"RATEEST target options:\n"
"  --rateest-name name		Rate estimator name\n"
"  --rateest-interval sec	Rate measurement interval in seconds\n"
"  --rateest-ewmalog value	Rate measurement averaging time constant\n");
}

enum {
	O_NAME = 0,
	O_INTERVAL,
	O_EWMALOG,
};

#define s struct xt_rateest_target_info
static const struct xt_option_entry RATEEST_opts[] = {
	{.name = "rateest-name", .id = O_NAME, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, name)},
	{.name = "rateest-interval", .id = O_INTERVAL, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND},
	{.name = "rateest-ewmalog", .id = O_EWMALOG, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND},
	XTOPT_TABLEEND,
};
#undef s

/* Copied from iproute */
#define TIME_UNITS_PER_SEC	1000000

static int
RATEEST_get_time(unsigned int *rateest_time, const char *str)
{
	double t;
	char *p;

	t = strtod(str, &p);
	if (p == str)
		return -1;

	if (*p) {
		if (strcasecmp(p, "s") == 0 || strcasecmp(p, "sec")==0 ||
		    strcasecmp(p, "secs")==0)
			t *= TIME_UNITS_PER_SEC;
		else if (strcasecmp(p, "ms") == 0 || strcasecmp(p, "msec")==0 ||
			 strcasecmp(p, "msecs") == 0)
			t *= TIME_UNITS_PER_SEC/1000;
		else if (strcasecmp(p, "us") == 0 || strcasecmp(p, "usec")==0 ||
			 strcasecmp(p, "usecs") == 0)
			t *= TIME_UNITS_PER_SEC/1000000;
		else
			return -1;
	}

	*rateest_time = t;
	return 0;
}

static void
RATEEST_print_time(unsigned int rateest_time)
{
	double tmp = rateest_time;

	if (tmp >= TIME_UNITS_PER_SEC)
		printf(" %.1fs", tmp / TIME_UNITS_PER_SEC);
	else if (tmp >= TIME_UNITS_PER_SEC/1000)
		printf(" %.1fms", tmp / (TIME_UNITS_PER_SEC / 1000));
	else
		printf(" %uus", rateest_time);
}

static void RATEEST_parse(struct xt_option_call *cb)
{
	struct rateest_tg_udata *udata = cb->udata;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_INTERVAL:
		if (RATEEST_get_time(&udata->interval, cb->arg) < 0)
			xtables_error(PARAMETER_PROBLEM,
				   "RATEEST: bad interval value \"%s\"",
				   cb->arg);
		break;
	case O_EWMALOG:
		if (RATEEST_get_time(&udata->ewma_log, cb->arg) < 0)
			xtables_error(PARAMETER_PROBLEM,
				   "RATEEST: bad ewmalog value \"%s\"",
				   cb->arg);
		break;
	}
}

static void RATEEST_final_check(struct xt_fcheck_call *cb)
{
	struct xt_rateest_target_info *info = cb->data;
	struct rateest_tg_udata *udata = cb->udata;

	for (info->interval = 0; info->interval <= 5; info->interval++) {
		if (udata->interval <= (1 << info->interval) * (TIME_UNITS_PER_SEC / 4))
			break;
	}

	if (info->interval > 5)
		xtables_error(PARAMETER_PROBLEM,
			   "RATEEST: interval value is too large");
	info->interval -= 2;

	for (info->ewma_log = 1; info->ewma_log < 32; info->ewma_log++) {
		double w = 1.0 - 1.0 / (1 << info->ewma_log);
		if (udata->interval / (-log(w)) > udata->ewma_log)
			break;
	}
	info->ewma_log--;

	if (info->ewma_log == 0 || info->ewma_log >= 31)
		xtables_error(PARAMETER_PROBLEM,
			   "RATEEST: ewmalog value is out of range");
}

static void
__RATEEST_print(const struct xt_entry_target *target, const char *prefix)
{
	const struct xt_rateest_target_info *info = (const void *)target->data;
	unsigned int local_interval;
	unsigned int local_ewma_log;

	local_interval = (TIME_UNITS_PER_SEC << (info->interval + 2)) / 4;
	local_ewma_log = local_interval * (1 << (info->ewma_log));

	printf(" %sname %s", prefix, info->name);
	printf(" %sinterval", prefix);
	RATEEST_print_time(local_interval);
	printf(" %sewmalog", prefix);
	RATEEST_print_time(local_ewma_log);
}

static void
RATEEST_print(const void *ip, const struct xt_entry_target *target,
	      int numeric)
{
	__RATEEST_print(target, "");
}

static void
RATEEST_save(const void *ip, const struct xt_entry_target *target)
{
	__RATEEST_print(target, "--rateest-");
}

static struct xtables_target rateest_tg_reg = {
	.family		= NFPROTO_UNSPEC,
	.name		= "RATEEST",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_rateest_target_info)),
	.userspacesize	= offsetof(struct xt_rateest_target_info, est),
	.help		= RATEEST_help,
	.x6_parse	= RATEEST_parse,
	.x6_fcheck	= RATEEST_final_check,
	.print		= RATEEST_print,
	.save		= RATEEST_save,
	.x6_options	= RATEEST_opts,
	.udata_size	= sizeof(struct rateest_tg_udata),
};

void _init(void)
{
	xtables_register_target(&rateest_tg_reg);
}
