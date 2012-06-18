/* Shared library add-on to iptables to add tcp MSS matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_tcpmss.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"tcpmss match v%s options:\n"
"[!] --mss value[:value]	Match TCP MSS range.\n"
"				(only valid for TCP SYN or SYN/ACK packets)\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "mss", 1, 0, '1' },
	{0}
};

static u_int16_t
parse_tcp_mssvalue(const char *mssvalue)
{
	unsigned int mssvaluenum;

	if (string_to_number(mssvalue, 0, 65535, &mssvaluenum) != -1)
		return (u_int16_t)mssvaluenum;

	exit_error(PARAMETER_PROBLEM,
		   "Invalid mss `%s' specified", mssvalue);
}

static void
parse_tcp_mssvalues(const char *mssvaluestring,
		    u_int16_t *mss_min, u_int16_t *mss_max)
{
	char *buffer;
	char *cp;

	buffer = strdup(mssvaluestring);
	if ((cp = strchr(buffer, ':')) == NULL)
		*mss_min = *mss_max = parse_tcp_mssvalue(buffer);
	else {
		*cp = '\0';
		cp++;

		*mss_min = buffer[0] ? parse_tcp_mssvalue(buffer) : 0;
		*mss_max = cp[0] ? parse_tcp_mssvalue(cp) : 0xFFFF;
	}
	free(buffer);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_tcpmss_match_info *mssinfo =
		(struct ipt_tcpmss_match_info *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--mss' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_tcp_mssvalues(argv[optind-1],
				    &mssinfo->mss_min, &mssinfo->mss_max);
		if (invert)
			mssinfo->invert = 1;
		*flags = 1;
		break;
	default:
		return 0;
	}
	return 1;
}

static void
print_tcpmss(u_int16_t mss_min, u_int16_t mss_max, int invert, int numeric)
{
	if (invert)
		printf("! ");

	if (mss_min == mss_max)
		printf("%u ", mss_min);
	else
		printf("%u:%u ", mss_min, mss_max);
}

/* Final check; must have specified --mss. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "tcpmss match: You must specify `--mss'");
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	const struct ipt_tcpmss_match_info *mssinfo =
		(const struct ipt_tcpmss_match_info *)match->data;

	printf("tcpmss match ");
	print_tcpmss(mssinfo->mss_min, mssinfo->mss_max,
		     mssinfo->invert, numeric);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_tcpmss_match_info *mssinfo =
		(const struct ipt_tcpmss_match_info *)match->data;

	printf("--mss ");
	print_tcpmss(mssinfo->mss_min, mssinfo->mss_max,
		     mssinfo->invert, 0);
}

static struct iptables_match tcpmss = {
	.next		= NULL,
	.name		= "tcpmss",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_tcpmss_match_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_tcpmss_match_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&tcpmss);
}
