/* Shared library add-on to iptables for FTOS
 *
 * (C) 2000 by Matthew G. Marsh <mgm@paktronix.com>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 * libipt_FTOS.c borrowed heavily from libipt_TOS.c  11/09/2000
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_FTOS.h>

struct finfo {
 	struct ipt_entry_target t;
	u_int8_t ftos;
};

static void init(struct ipt_entry_target *t, unsigned int *nfcache) 
{
}

static void help(void) 
{
	printf(
"FTOS target options\n"
"  --set-ftos value		Set TOS field in packet header to value\n"
"  		                This value can be in decimal (ex: 32)\n"
"               		or in hex (ex: 0x20)\n"
);
}

static struct option opts[] = {
	{ "set-ftos", 1, 0, 'F' },
	{ 0 }
};

static void
parse_ftos(const unsigned char *s, struct ipt_FTOS_info *finfo)
{
	unsigned int ftos;
       
	if (string_to_number(s, 0, 255, &ftos) == -1)
		exit_error(PARAMETER_PROBLEM,
			   "Invalid ftos `%s'\n", s);
    	finfo->ftos = (u_int8_t )ftos;
    	return;
}

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      struct ipt_entry_target **target)
{
	struct ipt_FTOS_info *finfo
		= (struct ipt_FTOS_info *)(*target)->data;

	switch (c) {
	case 'F':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "FTOS target: Only use --set-ftos ONCE!");
		parse_ftos(optarg, finfo);
		*flags = 1;
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
		           "FTOS target: Parameter --set-ftos is required");
}

static void
print_ftos(u_int8_t ftos, int numeric)
{
 	printf("0x%02x ", ftos);
}

/* Prints out the targinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_target *target,
      int numeric)
{
	const struct ipt_FTOS_info *finfo =
		(const struct ipt_FTOS_info *)target->data;
	printf("TOS set ");
	print_ftos(finfo->ftos, numeric);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_target *target)
{
	const struct ipt_FTOS_info *finfo =
		(const struct ipt_FTOS_info *)target->data;

	printf("--set-ftos 0x%02x ", finfo->ftos);
}

static struct iptables_target ftos = {
	.next		= NULL,
	.name		= "FTOS",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_FTOS_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_FTOS_info)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_target(&ftos);
}
