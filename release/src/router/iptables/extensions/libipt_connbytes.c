/* Shared library add-on to iptables to add byte tracking support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter_ipv4/ipt_connbytes.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"connbytes v%s options:\n"
" [!] --connbytes from:[to]\n"
"     --connbytes-dir [original, reply, both]\n"
"     --connbytes-mode [packets, bytes, avgpkt]\n"
"\n", IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "connbytes", 1, 0, '1' },
	{ "connbytes-dir", 1, 0, '2' },
	{ "connbytes-mode", 1, 0, '3' },
	{0}
};

static void
parse_range(const char *arg, struct ipt_connbytes_info *si)
{
	char *colon,*p;

	si->count.from = strtoul(arg,&colon,10);
	if (*colon != ':') 
		exit_error(PARAMETER_PROBLEM, "Bad range `%s'", arg);
	si->count.to = strtoul(colon+1,&p,10);
	if (p == colon+1) {
		/* second number omited */
		si->count.to = 0xffffffff;
	}
	if (si->count.from > si->count.to)
		exit_error(PARAMETER_PROBLEM, "%llu should be less than %llu",
			   si->count.from, si->count.to);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_connbytes_info *sinfo = (struct ipt_connbytes_info *)(*match)->data;
	unsigned long i;

	switch (c) {
	case '1':
		if (check_inverse(optarg, &invert, &optind, 0))
			optind++;

		parse_range(argv[optind-1], sinfo);
		if (invert) {
			i = sinfo->count.from;
			sinfo->count.from = sinfo->count.to;
			sinfo->count.to = i;
		}
		*flags |= 1;
		break;
	case '2':
		if (!strcmp(optarg, "original"))
			sinfo->direction = IPT_CONNBYTES_DIR_ORIGINAL;
		else if (!strcmp(optarg, "reply"))
			sinfo->direction = IPT_CONNBYTES_DIR_REPLY;
		else if (!strcmp(optarg, "both"))
			sinfo->direction = IPT_CONNBYTES_DIR_BOTH;
		else
			exit_error(PARAMETER_PROBLEM,
				   "Unknown --connbytes-dir `%s'", optarg);

		*flags |= 2;
		break;
	case '3':
		if (!strcmp(optarg, "packets"))
			sinfo->what = IPT_CONNBYTES_PKTS;
		else if (!strcmp(optarg, "bytes"))
			sinfo->what = IPT_CONNBYTES_BYTES;
		else if (!strcmp(optarg, "avgpkt"))
			sinfo->what = IPT_CONNBYTES_AVGPKT;
		else
			exit_error(PARAMETER_PROBLEM,
				   "Unknown --connbytes-mode `%s'", optarg);
		*flags |= 4;
		break;
	default:
		return 0;
	}

	return 1;
}

static void final_check(unsigned int flags)
{
	if (flags != 7)
		exit_error(PARAMETER_PROBLEM, "You must specify `--connbytes'"
			   "`--connbytes-dir' and `--connbytes-mode'");
}

static void print_mode(struct ipt_connbytes_info *sinfo)
{
	switch (sinfo->what) {
		case IPT_CONNBYTES_PKTS:
			fputs("packets ", stdout);
			break;
		case IPT_CONNBYTES_BYTES:
			fputs("bytes ", stdout);
			break;
		case IPT_CONNBYTES_AVGPKT:
			fputs("avgpkt ", stdout);
			break;
		default:
			fputs("unknown ", stdout);
			break;
	}
}

static void print_direction(struct ipt_connbytes_info *sinfo)
{
	switch (sinfo->direction) {
		case IPT_CONNBYTES_DIR_ORIGINAL:
			fputs("original ", stdout);
			break;
		case IPT_CONNBYTES_DIR_REPLY:
			fputs("reply ", stdout);
			break;
		case IPT_CONNBYTES_DIR_BOTH:
			fputs("both ", stdout);
			break;
		default:
			fputs("unknown ", stdout);
			break;
	}
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_connbytes_info *sinfo = (struct ipt_connbytes_info *)match->data;

	if (sinfo->count.from > sinfo->count.to) 
		printf("connbytes ! %llu:%llu ", sinfo->count.to,
			sinfo->count.from);
	else
		printf("connbytes %llu:%llu ",sinfo->count.from,
			sinfo->count.to);
	print_mode(sinfo);

	fputs("direction ", stdout);
	print_direction(sinfo);
}

/* Saves the matchinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	struct ipt_connbytes_info *sinfo = (struct ipt_connbytes_info *)match->data;

	if (sinfo->count.from > sinfo->count.to) 
		printf("! --connbytes %llu:%llu ", sinfo->count.to,
			sinfo->count.from);
	else
		printf("--connbytes %llu:%llu ", sinfo->count.from,
			sinfo->count.to);

	fputs("--connbytes-mode ", stdout);
	print_mode(sinfo);

	fputs("--connbytes-dir ", stdout);
	print_direction(sinfo);
}

static struct iptables_match state = {
	.next 		= NULL,
	.name 		= "connbytes",
	.version 	= IPTABLES_VERSION,
	.size 		= IPT_ALIGN(sizeof(struct ipt_connbytes_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_connbytes_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save 		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&state);
}
