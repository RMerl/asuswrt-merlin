/* Shared library add-on to iptables for conntrack matching support.
 * GPL (C) 2001  Marc Boucher (marc@mbsi.ca).
 */

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include <iptables.h>
#include <linux/netfilter/nf_conntrack_common.h>
/* For 64bit kernel / 32bit userspace */
#include "../include/linux/netfilter_ipv4/ipt_conntrack.h"

#ifndef IPT_CONNTRACK_STATE_UNTRACKED
#define IPT_CONNTRACK_STATE_UNTRACKED (1 << (IP_CT_NUMBER + 3))
#endif

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"conntrack match v%s options:\n"
" [!] --ctstate [INVALID|ESTABLISHED|NEW|RELATED|UNTRACKED|SNAT|DNAT][,...]\n"
"				State(s) to match\n"
" [!] --ctproto	proto		Protocol to match; by number or name, eg. `tcp'\n"
"     --ctorigsrc  [!] address[/mask]\n"
"				Original source specification\n"
"     --ctorigdst  [!] address[/mask]\n"
"				Original destination specification\n"
"     --ctreplsrc  [!] address[/mask]\n"
"				Reply source specification\n"
"     --ctrepldst  [!] address[/mask]\n"
"				Reply destination specification\n"
" [!] --ctstatus [NONE|EXPECTED|SEEN_REPLY|ASSURED|CONFIRMED][,...]\n"
"				Status(es) to match\n"
" [!] --ctexpire time[:time]	Match remaining lifetime in seconds against\n"
"				value or range of values (inclusive)\n"
"\n", IPTABLES_VERSION);
}



static struct option opts[] = {
	{ "ctstate", 1, 0, '1' },
	{ "ctproto", 1, 0, '2' },
	{ "ctorigsrc", 1, 0, '3' },
	{ "ctorigdst", 1, 0, '4' },
	{ "ctreplsrc", 1, 0, '5' },
	{ "ctrepldst", 1, 0, '6' },
	{ "ctstatus", 1, 0, '7' },
	{ "ctexpire", 1, 0, '8' },
	{0}
};

static int
parse_state(const char *state, size_t strlen, struct ipt_conntrack_info *sinfo)
{
	if (strncasecmp(state, "INVALID", strlen) == 0)
		sinfo->statemask |= IPT_CONNTRACK_STATE_INVALID;
	else if (strncasecmp(state, "NEW", strlen) == 0)
		sinfo->statemask |= IPT_CONNTRACK_STATE_BIT(IP_CT_NEW);
	else if (strncasecmp(state, "ESTABLISHED", strlen) == 0)
		sinfo->statemask |= IPT_CONNTRACK_STATE_BIT(IP_CT_ESTABLISHED);
	else if (strncasecmp(state, "RELATED", strlen) == 0)
		sinfo->statemask |= IPT_CONNTRACK_STATE_BIT(IP_CT_RELATED);
	else if (strncasecmp(state, "UNTRACKED", strlen) == 0)
		sinfo->statemask |= IPT_CONNTRACK_STATE_UNTRACKED;
	else if (strncasecmp(state, "SNAT", strlen) == 0)
		sinfo->statemask |= IPT_CONNTRACK_STATE_SNAT;
	else if (strncasecmp(state, "DNAT", strlen) == 0)
		sinfo->statemask |= IPT_CONNTRACK_STATE_DNAT;
	else
		return 0;
	return 1;
}

static void
parse_states(const char *arg, struct ipt_conntrack_info *sinfo)
{
	const char *comma;

	while ((comma = strchr(arg, ',')) != NULL) {
		if (comma == arg || !parse_state(arg, comma-arg, sinfo))
			exit_error(PARAMETER_PROBLEM, "Bad ctstate `%s'", arg);
		arg = comma+1;
	}

	if (strlen(arg) == 0 || !parse_state(arg, strlen(arg), sinfo))
		exit_error(PARAMETER_PROBLEM, "Bad ctstate `%s'", arg);
}

static int
parse_status(const char *status, size_t strlen, struct ipt_conntrack_info *sinfo)
{
	if (strncasecmp(status, "NONE", strlen) == 0)
		sinfo->statusmask |= 0;
	else if (strncasecmp(status, "EXPECTED", strlen) == 0)
		sinfo->statusmask |= IPS_EXPECTED;
	else if (strncasecmp(status, "SEEN_REPLY", strlen) == 0)
		sinfo->statusmask |= IPS_SEEN_REPLY;
	else if (strncasecmp(status, "ASSURED", strlen) == 0)
		sinfo->statusmask |= IPS_ASSURED;
#ifdef IPS_CONFIRMED
	else if (strncasecmp(status, "CONFIRMED", strlen) == 0)
		sinfo->stausmask |= IPS_CONFIRMED;
#endif
	else
		return 0;
	return 1;
}

static void
parse_statuses(const char *arg, struct ipt_conntrack_info *sinfo)
{
	const char *comma;

	while ((comma = strchr(arg, ',')) != NULL) {
		if (comma == arg || !parse_status(arg, comma-arg, sinfo))
			exit_error(PARAMETER_PROBLEM, "Bad ctstatus `%s'", arg);
		arg = comma+1;
	}

	if (strlen(arg) == 0 || !parse_status(arg, strlen(arg), sinfo))
		exit_error(PARAMETER_PROBLEM, "Bad ctstatus `%s'", arg);
}

#ifdef KERNEL_64_USERSPACE_32
static unsigned long long
parse_expire(const char *s)
{
	unsigned long long len;
	
	if (string_to_number_ll(s, 0, 0, &len) == -1)
		exit_error(PARAMETER_PROBLEM, "expire value invalid: `%s'\n", s);
	else
		return len;
}
#else
static unsigned long
parse_expire(const char *s)
{
	unsigned int len;
	
	if (string_to_number(s, 0, 0, &len) == -1)
		exit_error(PARAMETER_PROBLEM, "expire value invalid: `%s'\n", s);
	else
		return len;
}
#endif

/* If a single value is provided, min and max are both set to the value */
static void
parse_expires(const char *s, struct ipt_conntrack_info *sinfo)
{
	char *buffer;
	char *cp;

	buffer = strdup(s);
	if ((cp = strchr(buffer, ':')) == NULL)
		sinfo->expires_min = sinfo->expires_max = parse_expire(buffer);
	else {
		*cp = '\0';
		cp++;

		sinfo->expires_min = buffer[0] ? parse_expire(buffer) : 0;
		sinfo->expires_max = cp[0] ? parse_expire(cp) : -1;
	}
	free(buffer);
	
	if (sinfo->expires_min > sinfo->expires_max)
		exit_error(PARAMETER_PROBLEM,
#ifdef KERNEL_64_USERSPACE_32
		           "expire min. range value `%llu' greater than max. "
		           "range value `%llu'", sinfo->expires_min, sinfo->expires_max);
#else
		           "expire min. range value `%lu' greater than max. "
		           "range value `%lu'", sinfo->expires_min, sinfo->expires_max);
#endif
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_conntrack_info *sinfo = (struct ipt_conntrack_info *)(*match)->data;
	char *protocol = NULL;
	unsigned int naddrs = 0;
	struct in_addr *addrs = NULL;


	switch (c) {
	case '1':
		check_inverse(optarg, &invert, &optind, 0);

		parse_states(argv[optind-1], sinfo);
		if (invert) {
			sinfo->invflags |= IPT_CONNTRACK_STATE;
		}
		sinfo->flags |= IPT_CONNTRACK_STATE;
		break;

	case '2':
		check_inverse(optarg, &invert, &optind, 0);

		if(invert)
			sinfo->invflags |= IPT_CONNTRACK_PROTO;

		/* Canonicalize into lower case */
		for (protocol = argv[optind-1]; *protocol; protocol++)
			*protocol = tolower(*protocol);

		protocol = argv[optind-1];
		sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.protonum = parse_protocol(protocol);

		if (sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.protonum == 0
		    && (sinfo->invflags & IPT_INV_PROTO))
			exit_error(PARAMETER_PROBLEM,
				   "rule would never match protocol");

		sinfo->flags |= IPT_CONNTRACK_PROTO;
		break;

	case '3':
		check_inverse(optarg, &invert, &optind, 9);

		if (invert)
			sinfo->invflags |= IPT_CONNTRACK_ORIGSRC;

		parse_hostnetworkmask(argv[optind-1], &addrs,
					&sinfo->sipmsk[IP_CT_DIR_ORIGINAL],
					&naddrs);
		if(naddrs > 1)
			exit_error(PARAMETER_PROBLEM,
				"multiple IP addresses not allowed");

		if(naddrs == 1) {
			sinfo->tuple[IP_CT_DIR_ORIGINAL].src.ip = addrs[0].s_addr;
		}

		sinfo->flags |= IPT_CONNTRACK_ORIGSRC;
		break;

	case '4':
		check_inverse(optarg, &invert, &optind, 0);

		if (invert)
			sinfo->invflags |= IPT_CONNTRACK_ORIGDST;

		parse_hostnetworkmask(argv[optind-1], &addrs,
					&sinfo->dipmsk[IP_CT_DIR_ORIGINAL],
					&naddrs);
		if(naddrs > 1)
			exit_error(PARAMETER_PROBLEM,
				"multiple IP addresses not allowed");

		if(naddrs == 1) {
			sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.ip = addrs[0].s_addr;
		}

		sinfo->flags |= IPT_CONNTRACK_ORIGDST;
		break;

	case '5':
		check_inverse(optarg, &invert, &optind, 0);

		if (invert)
			sinfo->invflags |= IPT_CONNTRACK_REPLSRC;

		parse_hostnetworkmask(argv[optind-1], &addrs,
					&sinfo->sipmsk[IP_CT_DIR_REPLY],
					&naddrs);
		if(naddrs > 1)
			exit_error(PARAMETER_PROBLEM,
				"multiple IP addresses not allowed");

		if(naddrs == 1) {
			sinfo->tuple[IP_CT_DIR_REPLY].src.ip = addrs[0].s_addr;
		}

		sinfo->flags |= IPT_CONNTRACK_REPLSRC;
		break;

	case '6':
		check_inverse(optarg, &invert, &optind, 0);

		if (invert)
			sinfo->invflags |= IPT_CONNTRACK_REPLDST;

		parse_hostnetworkmask(argv[optind-1], &addrs,
					&sinfo->dipmsk[IP_CT_DIR_REPLY],
					&naddrs);
		if(naddrs > 1)
			exit_error(PARAMETER_PROBLEM,
				"multiple IP addresses not allowed");

		if(naddrs == 1) {
			sinfo->tuple[IP_CT_DIR_REPLY].dst.ip = addrs[0].s_addr;
		}

		sinfo->flags |= IPT_CONNTRACK_REPLDST;
		break;

	case '7':
		check_inverse(optarg, &invert, &optind, 0);

		parse_statuses(argv[optind-1], sinfo);
		if (invert) {
			sinfo->invflags |= IPT_CONNTRACK_STATUS;
		}
		sinfo->flags |= IPT_CONNTRACK_STATUS;
		break;

	case '8':
		check_inverse(optarg, &invert, &optind, 0);

		parse_expires(argv[optind-1], sinfo);
		if (invert) {
			sinfo->invflags |= IPT_CONNTRACK_EXPIRES;
		}
		sinfo->flags |= IPT_CONNTRACK_EXPIRES;
		break;

	default:
		return 0;
	}

	*flags = sinfo->flags;
	return 1;
}

static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM, "You must specify one or more options");
}

static void
print_state(unsigned int statemask)
{
	const char *sep = "";

	if (statemask & IPT_CONNTRACK_STATE_INVALID) {
		printf("%sINVALID", sep);
		sep = ",";
	}
	if (statemask & IPT_CONNTRACK_STATE_BIT(IP_CT_NEW)) {
		printf("%sNEW", sep);
		sep = ",";
	}
	if (statemask & IPT_CONNTRACK_STATE_BIT(IP_CT_RELATED)) {
		printf("%sRELATED", sep);
		sep = ",";
	}
	if (statemask & IPT_CONNTRACK_STATE_BIT(IP_CT_ESTABLISHED)) {
		printf("%sESTABLISHED", sep);
		sep = ",";
	}
	if (statemask & IPT_CONNTRACK_STATE_UNTRACKED) {
		printf("%sUNTRACKED", sep);
		sep = ",";
	}
	if (statemask & IPT_CONNTRACK_STATE_SNAT) {
		printf("%sSNAT", sep);
		sep = ",";
	}
	if (statemask & IPT_CONNTRACK_STATE_DNAT) {
		printf("%sDNAT", sep);
		sep = ",";
	}
	printf(" ");
}

static void
print_status(unsigned int statusmask)
{
	const char *sep = "";

	if (statusmask & IPS_EXPECTED) {
		printf("%sEXPECTED", sep);
		sep = ",";
	}
	if (statusmask & IPS_SEEN_REPLY) {
		printf("%sSEEN_REPLY", sep);
		sep = ",";
	}
	if (statusmask & IPS_ASSURED) {
		printf("%sASSURED", sep);
		sep = ",";
	}
#ifdef IPS_CONFIRMED
	if (statusmask & IPS_CONFIRMED) {
		printf("%sCONFIRMED", sep);
		sep =",";
	}
#endif
	if (statusmask == 0) {
		printf("%sNONE", sep);
		sep = ",";
	}
	printf(" ");
}

static void
print_addr(struct in_addr *addr, struct in_addr *mask, int inv, int numeric)
{
	char buf[BUFSIZ];

        if (inv) 
               	printf("! ");

	if (mask->s_addr == 0L && !numeric)
		printf("%s ", "anywhere");
	else {
		if (numeric)
			sprintf(buf, "%s", addr_to_dotted(addr));
		else
			sprintf(buf, "%s", addr_to_anyname(addr));
		strcat(buf, mask_to_dotted(mask));
		printf("%s ", buf);
	}
}

/* Saves the matchinfo in parsable form to stdout. */
static void
matchinfo_print(const struct ipt_ip *ip, const struct ipt_entry_match *match, int numeric, const char *optpfx)
{
	struct ipt_conntrack_info *sinfo = (struct ipt_conntrack_info *)match->data;

	if(sinfo->flags & IPT_CONNTRACK_STATE) {
		printf("%sctstate ", optpfx);
        	if (sinfo->invflags & IPT_CONNTRACK_STATE)
                	printf("! ");
		print_state(sinfo->statemask);
	}

	if(sinfo->flags & IPT_CONNTRACK_PROTO) {
		printf("%sctproto ", optpfx);
        	if (sinfo->invflags & IPT_CONNTRACK_PROTO)
                	printf("! ");
		printf("%u ", sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.protonum);
	}

	if(sinfo->flags & IPT_CONNTRACK_ORIGSRC) {
		printf("%sctorigsrc ", optpfx);

		print_addr(
		    (struct in_addr *)&sinfo->tuple[IP_CT_DIR_ORIGINAL].src.ip,
		    &sinfo->sipmsk[IP_CT_DIR_ORIGINAL],
		    sinfo->invflags & IPT_CONNTRACK_ORIGSRC,
		    numeric);
	}

	if(sinfo->flags & IPT_CONNTRACK_ORIGDST) {
		printf("%sctorigdst ", optpfx);

		print_addr(
		    (struct in_addr *)&sinfo->tuple[IP_CT_DIR_ORIGINAL].dst.ip,
		    &sinfo->dipmsk[IP_CT_DIR_ORIGINAL],
		    sinfo->invflags & IPT_CONNTRACK_ORIGDST,
		    numeric);
	}

	if(sinfo->flags & IPT_CONNTRACK_REPLSRC) {
		printf("%sctreplsrc ", optpfx);

		print_addr(
		    (struct in_addr *)&sinfo->tuple[IP_CT_DIR_REPLY].src.ip,
		    &sinfo->sipmsk[IP_CT_DIR_REPLY],
		    sinfo->invflags & IPT_CONNTRACK_REPLSRC,
		    numeric);
	}

	if(sinfo->flags & IPT_CONNTRACK_REPLDST) {
		printf("%sctrepldst ", optpfx);

		print_addr(
		    (struct in_addr *)&sinfo->tuple[IP_CT_DIR_REPLY].dst.ip,
		    &sinfo->dipmsk[IP_CT_DIR_REPLY],
		    sinfo->invflags & IPT_CONNTRACK_REPLDST,
		    numeric);
	}

	if(sinfo->flags & IPT_CONNTRACK_STATUS) {
		printf("%sctstatus ", optpfx);
        	if (sinfo->invflags & IPT_CONNTRACK_STATUS)
                	printf("! ");
		print_status(sinfo->statusmask);
	}

	if(sinfo->flags & IPT_CONNTRACK_EXPIRES) {
		printf("%sctexpire ", optpfx);
        	if (sinfo->invflags & IPT_CONNTRACK_EXPIRES)
                	printf("! ");

#ifdef KERNEL_64_USERSPACE_32
        	if (sinfo->expires_max == sinfo->expires_min)
                	printf("%llu ", sinfo->expires_min);
        	else
                	printf("%llu:%llu ", sinfo->expires_min, sinfo->expires_max);
#else
        	if (sinfo->expires_max == sinfo->expires_min)
                	printf("%lu ", sinfo->expires_min);
        	else
                	printf("%lu:%lu ", sinfo->expires_min, sinfo->expires_max);
#endif
	}
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	matchinfo_print(ip, match, numeric, "");
}

/* Saves the matchinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	matchinfo_print(ip, match, 1, "--");
}

static struct iptables_match conntrack = { 
	.next 		= NULL,
	.name		= "conntrack",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_conntrack_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_conntrack_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&conntrack);
}
