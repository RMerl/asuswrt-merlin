/* Shared library add-on to iptables to add MARK target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
/* For 64bit kernel / 32bit userspace */
#include "../include/linux/netfilter_ipv4/ipt_MARK.h"
#include "../include/linux/netfilter_ipv6/ip6t_MARK.h"

void param_act6(unsigned int status, const char *p1, ...);
int strtonum6(const char *s, char **end, unsigned int *value,
                  unsigned int min, unsigned int max);

#ifndef XT_MIN_ALIGN   
/* xt_entry has pointers and u_int64_t's in it, so if you align to
 *    it, you'll also align to any crazy matches and targets someone
 *       might write */      
#define XT_MIN_ALIGN (__alignof__(struct xt_entry))
#endif  
        
#ifndef XT_ALIGN
#define XT_ALIGN(s) (((s) + ((XT_MIN_ALIGN)-1)) & ~((XT_MIN_ALIGN)-1))
#endif

enum {
	F_MARK = 1 << 0,
};

/* Function which prints out usage message. */
static void MARK_help(void)
{
	printf(
"MARK target v%s options:\n"
"  --set-mark value                   Set nfmark value\n"
"  --and-mark value                   Binary AND the nfmark with value\n"
"  --or-mark  value                   Binary OR  the nfmark with value\n"
"\n",
IPTABLES_VERSION);
}

static const struct option MARK_opts[] = {
	{ "set-mark", 1, NULL, '1' },
	{ "and-mark", 1, NULL, '2' },
	{ "or-mark", 1, NULL, '3' },
	{ }
};

static const struct option mark_tg_opts[] = {
	{.name = "set-xmark", .has_arg = 1, .val = 'X'},
	{.name = "set-mark",  .has_arg = 1, .val = '='},
	{.name = "and-mark",  .has_arg = 1, .val = '&'},
	{.name = "or-mark",   .has_arg = 1, .val = '|'},
	{.name = "xor-mark",  .has_arg = 1, .val = '^'},
	{},
};

static void mark_tg_help(void)
{
	printf(
"MARK target options:\n"
"  --set-xmark value[/mask]  Clear bits in mask and XOR value into nfmark\n"
"  --set-mark value[/mask]   Clear bits in mask and OR value into nfmark\n"
"  --and-mark bits           Binary AND the nfmark with bits\n"
"  --or-mark bits            Binary OR the nfmark with bits\n"
"  --xor-mask bits           Binary XOR the nfmark with bits\n"
"\n");
}

/* Function which parses command options; returns true if it
   ate an option */
static int
MARK_parse_v0(int c, char **argv, int invert, unsigned int *flags,
              const void *entry, struct ip6t_entry_target **target)
{
	struct ip6t_mark_target_info *markinfo = (struct ip6t_mark_target_info *)(*target)->data;

	switch (c) {
	case '1':
		if (string_to_number_l(optarg, 0, 0, 
				     &markinfo->mark))
			exit_error(PARAMETER_PROBLEM, "Bad MARK value `%s'", optarg);
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "MARK target: Can't specify --set-mark twice");
		*flags = 1;
		break;
	case '2':
		exit_error(PARAMETER_PROBLEM,
			   "MARK target: kernel too old for --and-mark");
	case '3':
		exit_error(PARAMETER_PROBLEM,
			   "MARK target: kernel too old for --or-mark");
	default:
		return 0;
	}

	return 1;
}

static void MARK_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
		           "MARK target: Parameter --set/and/or-mark"
			   " is required");
}

static int mark_tg_parse(int c, char **argv, int invert, unsigned int *flags,
                         const void *entry, struct ip6t_entry_target **target)
{
	struct xt_mark_tginfo2 *info = (void *)(*target)->data;
	unsigned int value, mask = ~0U;
	char *end;

	switch (c) {
	case 'X': /* --set-xmark */
	case '=': /* --set-mark */
		param_act6(P_ONE_ACTION, "MARK", *flags & F_MARK);
		param_act6(P_NO_INVERT, "MARK", "--set-xmark/--set-mark", invert);
		if (!strtonum6(optarg, &end, &value, 0, ~0U))
			param_act6(P_BAD_VALUE, "MARK", "--set-xmark/--set-mark", optarg);
		if (*end == '/')
			if (!strtonum6(end + 1, &end, &mask, 0, ~0U))
				param_act6(P_BAD_VALUE, "MARK", "--set-xmark/--set-mark", optarg);
		if (*end != '\0')
			param_act6(P_BAD_VALUE, "MARK", "--set-xmark/--set-mark", optarg);
		info->mark = value;
		info->mask = mask;

		if (c == '=')
			info->mask = value | mask;
		break;

	case '&': /* --and-mark */
		param_act6(P_ONE_ACTION, "MARK", *flags & F_MARK);
		param_act6(P_NO_INVERT, "MARK", "--and-mark", invert);
		if (!strtonum6(optarg, NULL, &mask, 0, ~0U))
			param_act6(P_BAD_VALUE, "MARK", "--and-mark", optarg);
		info->mark = 0;
		info->mask = ~mask;
		break;

	case '|': /* --or-mark */
		param_act6(P_ONE_ACTION, "MARK", *flags & F_MARK);
		param_act6(P_NO_INVERT, "MARK", "--or-mark", invert);
		if (!strtonum6(optarg, NULL, &value, 0, ~0U))
			param_act6(P_BAD_VALUE, "MARK", "--or-mark", optarg);
		info->mark = value;
		info->mask = value;
		break;

	case '^': /* --xor-mark */
		param_act6(P_ONE_ACTION, "MARK", *flags & F_MARK);
		param_act6(P_NO_INVERT, "MARK", "--xor-mark", invert);
		if (!strtonum6(optarg, NULL, &value, 0, ~0U))
			param_act6(P_BAD_VALUE, "MARK", "--xor-mark", optarg);
		info->mark = value;
		info->mask = 0;
		break;

	default:
		return 0;
	}

	*flags |= F_MARK;
	return 1;
}

static void mark_tg_check(unsigned int flags)
{
	if (flags == 0)
		exit_error(PARAMETER_PROBLEM, "MARK: One of the --set-xmark, "
		           "--{and,or,xor,set}-mark options is required");
}

static void
print_mark(unsigned long mark)
{
	printf("0x%lx ", mark);
}

/* Prints out the targinfo. */
static void MARK_print_v0(const void *ip,
                          const struct ip6t_entry_target *target, int numeric)
{
	const struct ip6t_mark_target_info *markinfo =
		(const struct ip6t_mark_target_info *)target->data;
	printf("MARK set ");
	print_mark(markinfo->mark);
}

/* Saves the union ip6t_targinfo in parsable form to stdout. */
static void MARK_save_v0(const void *ip, const struct ip6t_entry_target *target)
{
	const struct ip6t_mark_target_info *markinfo =
		(const struct ip6t_mark_target_info *)target->data;

	printf("--set-mark ");
	print_mark(markinfo->mark);
}

static void mark_tg_print(const void *ip, const struct ip6t_entry_target *target,
                          int numeric)
{
	const struct xt_mark_tginfo2 *info = (const void *)target->data;

	if (info->mark == 0)
		printf("MARK and 0x%x ", (unsigned int)(u_int32_t)~info->mask);
	else if (info->mark == info->mask)
		printf("MARK or 0x%x ", info->mark);
	else if (info->mask == 0)
		printf("MARK xor 0x%x ", info->mark);
	else
		printf("MARK xset 0x%x/0x%x ", info->mark, info->mask);
}

static void mark_tg_save(const void *ip, const struct ip6t_entry_target *target)
{
	const struct xt_mark_tginfo2 *info = (const void *)target->data;

	printf("--set-xmark 0x%x/0x%x ", info->mark, info->mask);
}

static struct ip6tables_target mark_target6_v0 = {
	.next	       = NULL,
        .name           = "MARK",
        .version        = IPTABLES_VERSION,
        .revision       = 0,
        .size           = IP6T_ALIGN(sizeof(struct ip6t_mark_target_info)),
        .userspacesize  = IP6T_ALIGN(sizeof(struct ip6t_mark_target_info)),
        .help           = &MARK_help,
        .parse          = &MARK_parse_v0,
        .final_check    = &MARK_check,
        .print          = &MARK_print_v0,
        .save           = &MARK_save_v0,
        .extra_opts     = MARK_opts,
};

static struct ip6tables_target mark_tg6_reg_v2 = {
	.next	       = NULL,
	.version       = IPTABLES_VERSION,
	.name          = "MARK",
	.revision      = 2,
	.size          = XT_ALIGN(sizeof(struct xt_mark_tginfo2)),
	.userspacesize = XT_ALIGN(sizeof(struct xt_mark_tginfo2)),
	.help          = &mark_tg_help,
	.parse         = &mark_tg_parse,
	.final_check   = &mark_tg_check,
	.print         = &mark_tg_print,
	.save          = &mark_tg_save,
	.extra_opts    = mark_tg_opts,
};

void _init(void)
{
        //register_target6(&mark_target6_v0);
        register_target6(&mark_tg6_reg_v2);
}
