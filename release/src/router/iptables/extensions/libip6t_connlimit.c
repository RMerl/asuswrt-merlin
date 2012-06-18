/* Shared library add-on to iptables to add connection limit support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <getopt.h>
#include <ip6tables.h>
#include <linux/netfilter/xt_connlimit.h>

/* Function which prints out usage message. */
static void connlimit_help(void)
{
	printf(
"connlimit v%s options:\n"
"[!] --connlimit-above n        match if the number of existing "
"                               connections is (not) above n\n"
"    --connlimit-mask n         group hosts using mask\n"
"\n", IPTABLES_VERSION);
}

static const struct option connlimit_opts[] = {
	{"connlimit-above", 1, 0, '1'},
	{"connlimit-mask",  1, 0, '2'},
	{0},
};

static void connlimit_init(struct ip6t_entry_match *match, unsigned int *nfc)
{
	struct xt_connlimit_info *info = (void *)match->data;
	info->v6_mask[0] =
	info->v6_mask[1] =
	info->v6_mask[2] =
	info->v6_mask[3] = 0xFFFFFFFF;
}

static void prefix_to_netmask(u_int32_t *mask, unsigned int prefix_len)
{
	if (prefix_len == 0) {
		mask[0] = mask[1] = mask[2] = mask[3] = 0;
	} else if (prefix_len <= 32) {
		mask[0] <<= 32 - prefix_len;
		mask[1] = mask[2] = mask[3] = 0;
	} else if (prefix_len <= 64) {
		mask[1] <<= 32 - (prefix_len - 32);
		mask[2] = mask[3] = 0;
	} else if (prefix_len <= 96) {
		mask[2] <<= 32 - (prefix_len - 64);
		mask[3] = 0;
	} else if (prefix_len <= 128) {
		mask[3] <<= 32 - (prefix_len - 96);
	}
	mask[0] = htonl(mask[0]);
	mask[1] = htonl(mask[1]);
	mask[2] = htonl(mask[2]);
	mask[3] = htonl(mask[3]);
}

static int connlimit_parse(int c, char **argv, int invert, unsigned int *flags,
                           const struct ip6t_entry *entry,
                           unsigned int *nfcache,
                           struct ip6t_entry_match **match)
{
	struct xt_connlimit_info *info = (void *)(*match)->data;
	char *err;
	int i;

	if (*flags & c)
		exit_error(PARAMETER_PROBLEM,
		           "--connlimit-above and/or --connlimit-mask may "
			   "only be given once");

	switch (c) {
	case 1:
		check_inverse(optarg, &invert, &optind, 0);
		info->limit   = strtoul(argv[optind-1], NULL, 0);
		info->inverse = invert;
		break;
	case 2:
		i = strtoul(argv[optind-1], &err, 0);
		if (i > 128 || *err != '\0')
			exit_error(PARAMETER_PROBLEM,
				"--connlimit-mask must be between 0 and 128");
		prefix_to_netmask(info->v6_mask, i);
		break;
	default:
		return 0;
	}

	*flags |= c;
	return 1;
}

/* Final check */
static void connlimit_check(unsigned int flags)
{
	if (!(flags & 1))
		exit_error(PARAMETER_PROBLEM,
		           "You must specify \"--connlimit-above\"");
}

static unsigned int count_bits(const u_int32_t *mask)
{
	unsigned int bits = 0, i;
	u_int32_t tmp[4];

	for (i = 0; i < 4; ++i)
		for (tmp[i] = ~ntohl(mask[i]); tmp[i] != 0; tmp[i] >>= 1)
			++bits;
	return 128 - bits;
}

/* Prints out the matchinfo. */
static void connlimit_print(const struct ip6t_ip6 *ip,
                            const struct ip6t_entry_match *match, int numeric)
{
	const struct xt_connlimit_info *info = (const void *)match->data;

	printf("#conn/%u %s %u ", count_bits(info->v6_mask),
	       info->inverse ? "<=" : ">", info->limit);
}

/* Saves the matchinfo in parsable form to stdout. */
static void connlimit_save(const struct ip6t_ip6 *ip,
                           const struct ip6t_entry_match *match)
{
	const struct xt_connlimit_info *info = (const void *)match->data;

	printf("%s--connlimit-above %u --connlimit-mask %u ",
	       info->inverse ? "! " : "", info->limit,
	       count_bits(info->v6_mask));
}

static struct ip6tables_match connlimit_reg = {
	.name          = "connlimit",
	.version       = IPTABLES_VERSION,
	.size          = IP6T_ALIGN(sizeof(struct xt_connlimit_info)),
	.userspacesize = offsetof(struct xt_connlimit_info, data),
	.help          = connlimit_help,
	.init          = connlimit_init,
	.parse         = connlimit_parse,
	.final_check   = connlimit_check,
	.print         = connlimit_print,
	.save          = connlimit_save,
	.extra_opts    = connlimit_opts,
};

void _init(void)
{
	register_match6(&connlimit_reg);
}
