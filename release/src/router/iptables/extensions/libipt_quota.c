/*
 * Shared library add-on to iptables to add quota support
 *
 * Sam Johnston <samj@samj.net>
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>

#include <linux/netfilter_ipv4/ipt_quota.h>
#include <linux/netfilter_ipv4/ip_tables.h>

static struct option opts[] = {
        {"quota", 1, 0, '1'},
        {0}
};

/* print usage */
static void
help(void)
{
        printf("quota options:\n"
               " --quota quota			quota (bytes)\n" "\n");
}

/* print matchinfo */
static void
print(const struct ipt_ip *ip, const struct ipt_entry_match *match, int numeric)
{
        struct ipt_quota_info *q = (struct ipt_quota_info *) match->data;
        printf("quota: %llu bytes", (unsigned long long) q->quota);
}

/* save matchinfo */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
        struct ipt_quota_info *q = (struct ipt_quota_info *) match->data;
        printf("--quota %llu ", (unsigned long long) q->quota);
}

/* parse quota option */
static int
parse_quota(const char *s, u_int64_t * quota)
{
        *quota = strtoull(s, (char **) NULL, 10);

#ifdef DEBUG_IPT_QUOTA
        printf("Quota: %llu\n", *quota);
#endif

        if (*quota == -1)
                exit_error(PARAMETER_PROBLEM, "quota invalid: '%s'\n", s);
        else
                return 1;
}

/* parse all options, returning true if we found any for us */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache, struct ipt_entry_match **match)
{
        struct ipt_quota_info *info = (struct ipt_quota_info *) (*match)->data;

        switch (c) {
        case '1':
                if (check_inverse(optarg, &invert, NULL, 0))
                        exit_error(PARAMETER_PROBLEM, "quota: unexpected '!'");
                if (!parse_quota(optarg, &info->quota))
                        exit_error(PARAMETER_PROBLEM,
                                   "bad quota: '%s'", optarg);
                break;

        default:
                return 0;
        }
        return 1;
}

/* no final check */
static void
final_check(unsigned int flags)
{
}

struct iptables_match quota = { 
	.next		= NULL,
	.name		= "quota",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof (struct ipt_quota_info)),
	.userspacesize	= IPT_ALIGN(sizeof (struct ipt_quota_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void
_init(void)
{
        register_match(&quota);
}
