/* Shared library add-on to iptables to add bridge port matching support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6t_physdev.h>
#if defined(__GLIBC__) && __GLIBC__ == 2
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif

static void
help(void)
{
	printf(
"physdev v%s options:\n"
" --physdev-in [!] input name[+]		bridge port name ([+] for wildcard)\n"
" --physdev-out [!] output name[+]	bridge port name ([+] for wildcard)\n"
" [!] --physdev-is-in			arrived on a bridge device\n"
" [!] --physdev-is-out			will leave on a bridge device\n"
" [!] --physdev-is-bridged		it's a bridged packet\n"
"\n", IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "physdev-in", 1, 0, '1' },
	{ "physdev-out", 1, 0, '2' },
	{ "physdev-is-in", 0, 0, '3' },
	{ "physdev-is-out", 0, 0, '4' },
	{ "physdev-is-bridged", 0, 0, '5' },
	{0}
};

static void
init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
}

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_physdev_info *info =
		(struct ip6t_physdev_info*)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & IP6T_PHYSDEV_OP_IN)
			goto multiple_use;
		check_inverse(optarg, &invert, &optind, 0);
		parse_interface(argv[optind-1], info->physindev,
				(unsigned char *)info->in_mask);
		if (invert)
			info->invert |= IP6T_PHYSDEV_OP_IN;
		info->bitmask |= IP6T_PHYSDEV_OP_IN;
		*flags |= IP6T_PHYSDEV_OP_IN;
		break;

	case '2':
		if (*flags & IP6T_PHYSDEV_OP_OUT)
			goto multiple_use;
		check_inverse(optarg, &invert, &optind, 0);
		parse_interface(argv[optind-1], info->physoutdev,
				(unsigned char *)info->out_mask);
		if (invert)
			info->invert |= IP6T_PHYSDEV_OP_OUT;
		info->bitmask |= IP6T_PHYSDEV_OP_OUT;
		*flags |= IP6T_PHYSDEV_OP_OUT;
		break;

	case '3':
		if (*flags & IP6T_PHYSDEV_OP_ISIN)
			goto multiple_use;
		check_inverse(optarg, &invert, &optind, 0);
		info->bitmask |= IP6T_PHYSDEV_OP_ISIN;
		if (invert)
			info->invert |= IP6T_PHYSDEV_OP_ISIN;
		*flags |= IP6T_PHYSDEV_OP_ISIN;
		break;

	case '4':
		if (*flags & IP6T_PHYSDEV_OP_ISOUT)
			goto multiple_use;
		check_inverse(optarg, &invert, &optind, 0);
		info->bitmask |= IP6T_PHYSDEV_OP_ISOUT;
		if (invert)
			info->invert |= IP6T_PHYSDEV_OP_ISOUT;
		*flags |= IP6T_PHYSDEV_OP_ISOUT;
		break;

	case '5':
		if (*flags & IP6T_PHYSDEV_OP_BRIDGED)
			goto multiple_use;
		check_inverse(optarg, &invert, &optind, 0);
		if (invert)
			info->invert |= IP6T_PHYSDEV_OP_BRIDGED;
		*flags |= IP6T_PHYSDEV_OP_BRIDGED;
		info->bitmask |= IP6T_PHYSDEV_OP_BRIDGED;
		break;

	default:
		return 0;
	}

	return 1;
multiple_use:
	exit_error(PARAMETER_PROBLEM,
	   "multiple use of the same physdev option is not allowed");

}

static void final_check(unsigned int flags)
{
	if (flags == 0)
		exit_error(PARAMETER_PROBLEM, "PHYSDEV: no physdev option specified");
}

static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match,
      int numeric)
{
	struct ip6t_physdev_info *info =
		(struct ip6t_physdev_info*)match->data;

	printf("PHYSDEV match");
	if (info->bitmask & IP6T_PHYSDEV_OP_ISIN)
		printf("%s --physdev-is-in",
		       info->invert & IP6T_PHYSDEV_OP_ISIN ? " !":"");
	if (info->bitmask & IP6T_PHYSDEV_OP_IN)
		printf("%s --physdev-in %s",
		(info->invert & IP6T_PHYSDEV_OP_IN) ? " !":"", info->physindev);

	if (info->bitmask & IP6T_PHYSDEV_OP_ISOUT)
		printf("%s --physdev-is-out",
		       info->invert & IP6T_PHYSDEV_OP_ISOUT ? " !":"");
	if (info->bitmask & IP6T_PHYSDEV_OP_OUT)
		printf("%s --physdev-out %s",
		(info->invert & IP6T_PHYSDEV_OP_OUT) ? " !":"", info->physoutdev);
	if (info->bitmask & IP6T_PHYSDEV_OP_BRIDGED)
		printf("%s --physdev-is-bridged",
		       info->invert & IP6T_PHYSDEV_OP_BRIDGED ? " !":"");
	printf(" ");
}

static void save(const struct ip6t_ip6 *ip, const struct ip6t_entry_match *match)
{
	struct ip6t_physdev_info *info =
		(struct ip6t_physdev_info*)match->data;

	if (info->bitmask & IP6T_PHYSDEV_OP_ISIN)
		printf("%s --physdev-is-in",
		       info->invert & IP6T_PHYSDEV_OP_ISIN ? " !":"");
	if (info->bitmask & IP6T_PHYSDEV_OP_IN)
		printf("%s --physdev-in %s",
		(info->invert & IP6T_PHYSDEV_OP_IN) ? " !":"", info->physindev);

	if (info->bitmask & IP6T_PHYSDEV_OP_ISOUT)
		printf("%s --physdev-is-out",
		       info->invert & IP6T_PHYSDEV_OP_ISOUT ? " !":"");
	if (info->bitmask & IP6T_PHYSDEV_OP_OUT)
		printf("%s --physdev-out %s",
		(info->invert & IP6T_PHYSDEV_OP_OUT) ? " !":"", info->physoutdev);
	if (info->bitmask & IP6T_PHYSDEV_OP_BRIDGED)
		printf("%s --physdev-is-bridged",
		       info->invert & IP6T_PHYSDEV_OP_BRIDGED ? " !":"");
	printf(" ");
}

static struct ip6tables_match physdev = {
	.name		= "physdev",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ip6t_physdev_info)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ip6t_physdev_info)),
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
	register_match6(&physdev);
}
