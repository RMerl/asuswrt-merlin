/* Shared library add-on to iptables for DCCP matching
 *
 * (C) 2005 by Harald Welte <laforge@netfilter.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <ctype.h>

#include <iptables.h>
#include <linux/dccp.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_dccp.h>

#if 0
#define DEBUGP(format, first...) printf(format, ##first)
#define static
#else
#define DEBUGP(format, fist...) 
#endif

/* Initialize the match. */
static void
init(struct ipt_entry_match *m, 
     unsigned int *nfcache)
{
	struct ipt_dccp_info *einfo = (struct ipt_dccp_info *)m->data;

	memset(einfo, 0, sizeof(struct ipt_dccp_info));
}

static void help(void)
{
	printf(
"DCCP match v%s options\n"
" --source-port [!] port[:port]                          match source port(s)\n"
" --sport ...\n"
" --destination-port [!] port[:port]                     match destination port(s)\n"
" --dport ...\n"
,
	IPTABLES_VERSION);
}

static struct option opts[] = {
	{ .name = "source-port", .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = "sport", .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = "destination-port", .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = "dport", .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = "dccp-types", .has_arg = 1, .flag = 0, .val = '3' },
	{ .name = "dccp-option", .has_arg = 1, .flag = 0, .val = '4' },
	{ .name = 0 }
};

static void
parse_dccp_ports(const char *portstring, 
		 u_int16_t *ports)
{
	char *buffer;
	char *cp;

	buffer = strdup(portstring);
	DEBUGP("%s\n", portstring);
	if ((cp = strchr(buffer, ':')) == NULL) {
		ports[0] = ports[1] = parse_port(buffer, "dccp");
	}
	else {
		*cp = '\0';
		cp++;

		ports[0] = buffer[0] ? parse_port(buffer, "dccp") : 0;
		ports[1] = cp[0] ? parse_port(cp, "dccp") : 0xFFFF;

		if (ports[0] > ports[1])
			exit_error(PARAMETER_PROBLEM,
				   "invalid portrange (min > max)");
	}
	free(buffer);
}

static char *dccp_pkt_types[] = {
	[DCCP_PKT_REQUEST] 	= "REQUEST",
	[DCCP_PKT_RESPONSE]	= "RESPONSE",
	[DCCP_PKT_DATA]		= "DATA",
	[DCCP_PKT_ACK]		= "ACK",
	[DCCP_PKT_DATAACK]	= "DATAACK",
	[DCCP_PKT_CLOSEREQ]	= "CLOSEREQ",
	[DCCP_PKT_CLOSE]	= "CLOSE",
	[DCCP_PKT_RESET]	= "RESET",
	[DCCP_PKT_SYNC]		= "SYNC",
	[DCCP_PKT_SYNCACK]	= "SYNCACK",
	[DCCP_PKT_INVALID]	= "INVALID",
};

static u_int16_t
parse_dccp_types(const char *typestring)
{
	u_int16_t typemask = 0;
	char *ptr, *buffer;

	buffer = strdup(typestring);

	for (ptr = strtok(buffer, ","); ptr; ptr = strtok(NULL, ",")) {
		unsigned int i;
		for (i = 0; i < sizeof(dccp_pkt_types)/sizeof(char *); i++) {
			if (!strcasecmp(dccp_pkt_types[i], ptr)) {
				typemask |= (1 << i);
				break;
			}
		}
		if (i == sizeof(dccp_pkt_types)/sizeof(char *))
			exit_error(PARAMETER_PROBLEM,
				   "Unknown DCCP type `%s'", ptr);
	}

	free(buffer);
	return typemask;
}

static u_int8_t parse_dccp_option(char *optstring)
{
	unsigned int ret;

	if (string_to_number(optstring, 1, 255, &ret) == -1)
		exit_error(PARAMETER_PROBLEM, "Bad DCCP option `%s'",
			   optstring);

	return (u_int8_t)ret;
}

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_dccp_info *einfo
		= (struct ipt_dccp_info *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & IPT_DCCP_SRC_PORTS)
			exit_error(PARAMETER_PROBLEM,
			           "Only one `--source-port' allowed");
		einfo->flags |= IPT_DCCP_SRC_PORTS;
		check_inverse(optarg, &invert, &optind, 0);
		parse_dccp_ports(argv[optind-1], einfo->spts);
		if (invert)
			einfo->invflags |= IPT_DCCP_SRC_PORTS;
		*flags |= IPT_DCCP_SRC_PORTS;
		break;

	case '2':
		if (*flags & IPT_DCCP_DEST_PORTS)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--destination-port' allowed");
		einfo->flags |= IPT_DCCP_DEST_PORTS;
		check_inverse(optarg, &invert, &optind, 0);
		parse_dccp_ports(argv[optind-1], einfo->dpts);
		if (invert)
			einfo->invflags |= IPT_DCCP_DEST_PORTS;
		*flags |= IPT_DCCP_DEST_PORTS;
		break;

	case '3':
		if (*flags & IPT_DCCP_TYPE)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--dccp-types' allowed");
		einfo->flags |= IPT_DCCP_TYPE;
		check_inverse(optarg, &invert, &optind, 0);
		einfo->typemask = parse_dccp_types(argv[optind-1]);
		if (invert)
			einfo->invflags |= IPT_DCCP_TYPE;
		*flags |= IPT_DCCP_TYPE;
		break;

	case '4':
		if (*flags & IPT_DCCP_OPTION)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--dccp-option' allowed");
		einfo->flags |= IPT_DCCP_OPTION;
		check_inverse(optarg, &invert, &optind, 0);
		einfo->option = parse_dccp_option(argv[optind-1]);
		if (invert)
			einfo->invflags |= IPT_DCCP_OPTION;
		*flags |= IPT_DCCP_OPTION;
		break;
	default:
		return 0;
	}
	return 1;
}

static void
final_check(unsigned int flags)
{
}

static char *
port_to_service(int port)
{
	struct servent *service;

	if ((service = getservbyport(htons(port), "dccp")))
		return service->s_name;

	return NULL;
}

static void
print_port(u_int16_t port, int numeric)
{
	char *service;

	if (numeric || (service = port_to_service(port)) == NULL)
		printf("%u", port);
	else
		printf("%s", service);
}

static void
print_ports(const char *name, u_int16_t min, u_int16_t max,
	    int invert, int numeric)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFF || invert) {
		printf("%s", name);
		if (min == max) {
			printf(":%s", inv);
			print_port(min, numeric);
		} else {
			printf("s:%s", inv);
			print_port(min, numeric);
			printf(":");
			print_port(max, numeric);
		}
		printf(" ");
	}
}

static void
print_types(u_int16_t types, int inverted, int numeric)
{
	int have_type = 0;

	if (inverted)
		printf("! ");

	while (types) {
		unsigned int i;

		for (i = 0; !(types & (1 << i)); i++);

		if (have_type)
			printf(",");
		else
			have_type = 1;

		if (numeric)
			printf("%u", i);
		else
			printf("%s", dccp_pkt_types[i]);

		types &= ~(1 << i);
	}
}

static void
print_option(u_int8_t option, int invert, int numeric)
{
	if (option || invert)
		printf("option=%s%u ", invert ? "!" : "", option);
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	const struct ipt_dccp_info *einfo =
		(const struct ipt_dccp_info *)match->data;

	printf("dccp ");

	if (einfo->flags & IPT_DCCP_SRC_PORTS) {
		print_ports("spt", einfo->spts[0], einfo->spts[1],
			einfo->invflags & IPT_DCCP_SRC_PORTS,
			numeric);
	}

	if (einfo->flags & IPT_DCCP_DEST_PORTS) {
		print_ports("dpt", einfo->dpts[0], einfo->dpts[1],
			einfo->invflags & IPT_DCCP_DEST_PORTS,
			numeric);
	}

	if (einfo->flags & IPT_DCCP_TYPE) {
		print_types(einfo->typemask,
			   einfo->invflags & IPT_DCCP_TYPE,
			   numeric);
	}

	if (einfo->flags & IPT_DCCP_OPTION) {
		print_option(einfo->option,
			     einfo->invflags & IPT_DCCP_OPTION, numeric);
	}
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const struct ipt_ip *ip, 
     const struct ipt_entry_match *match)
{
	const struct ipt_dccp_info *einfo =
		(const struct ipt_dccp_info *)match->data;

	if (einfo->flags & IPT_DCCP_SRC_PORTS) {
		if (einfo->invflags & IPT_DCCP_SRC_PORTS)
			printf("! ");
		if (einfo->spts[0] != einfo->spts[1])
			printf("--sport %u:%u ", 
			       einfo->spts[0], einfo->spts[1]);
		else
			printf("--sport %u ", einfo->spts[0]);
	}

	if (einfo->flags & IPT_DCCP_DEST_PORTS) {
		if (einfo->invflags & IPT_DCCP_DEST_PORTS)
			printf("! ");
		if (einfo->dpts[0] != einfo->dpts[1])
			printf("--dport %u:%u ",
			       einfo->dpts[0], einfo->dpts[1]);
		else
			printf("--dport %u ", einfo->dpts[0]);
	}

	if (einfo->flags & IPT_DCCP_TYPE) {
		printf("--dccp-type ");
		print_types(einfo->typemask, einfo->invflags & IPT_DCCP_TYPE,0);
	}

	if (einfo->flags & IPT_DCCP_OPTION) {
		printf("--dccp-option %s%u ", 
			einfo->typemask & IPT_DCCP_OPTION ? "! " : "",
			einfo->option);
	}
}

static
struct iptables_match dccp
= { .name          = "dccp",
    .version       = IPTABLES_VERSION,
    .size          = IPT_ALIGN(sizeof(struct ipt_dccp_info)),
    .userspacesize = IPT_ALIGN(sizeof(struct ipt_dccp_info)),
    .help          = &help,
    .init          = &init,
    .parse         = &parse,
    .final_check   = &final_check,
    .print         = &print,
    .save          = &save,
    .extra_opts    = opts
};

void _init(void)
{
	register_match(&dccp);
}

