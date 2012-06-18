/* Shared library add-on to iptables to add UDP support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"UDP v%s options:\n"
" --source-port [!] port[:port]\n"
" --sport ...\n"
"				match source port(s)\n"
" --destination-port [!] port[:port]\n"
" --dport ...\n"
"				match destination port(s)\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "source-port", 1, 0, '1' },
	{ "sport", 1, 0, '1' }, /* synonym */
	{ "destination-port", 1, 0, '2' },
	{ "dport", 1, 0, '2' }, /* synonym */
	{0}
};

static void
parse_udp_ports(const char *portstring, u_int16_t *ports)
{
	char *buffer;
	char *cp;

	buffer = strdup(portstring);
	if ((cp = strchr(buffer, ':')) == NULL)
		ports[0] = ports[1] = parse_port(buffer, "udp");
	else {
		*cp = '\0';
		cp++;

		ports[0] = buffer[0] ? parse_port(buffer, "udp") : 0;
		ports[1] = cp[0] ? parse_port(cp, "udp") : 0xFFFF;

		if (ports[0] > ports[1])
			exit_error(PARAMETER_PROBLEM,
				   "invalid portrange (min > max)");
	}
	free(buffer);
}

/* Initialize the match. */
static void
init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
	struct ip6t_udp *udpinfo = (struct ip6t_udp *)m->data;

	udpinfo->spts[1] = udpinfo->dpts[1] = 0xFFFF;
}

#define UDP_SRC_PORTS 0x01
#define UDP_DST_PORTS 0x02

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_udp *udpinfo = (struct ip6t_udp *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & UDP_SRC_PORTS)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--source-port' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_udp_ports(argv[optind-1], udpinfo->spts);
		if (invert)
			udpinfo->invflags |= IP6T_UDP_INV_SRCPT;
		*flags |= UDP_SRC_PORTS;
		break;

	case '2':
		if (*flags & UDP_DST_PORTS)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--destination-port' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_udp_ports(argv[optind-1], udpinfo->dpts);
		if (invert)
			udpinfo->invflags |= IP6T_UDP_INV_DSTPT;
		*flags |= UDP_DST_PORTS;
		break;

	default:
		return 0;
	}

	return 1;
}

/* Final check; we don't care. */
static void
final_check(unsigned int flags)
{
}

static char *
port_to_service(int port)
{
	struct servent *service;

	if ((service = getservbyport(htons(port), "udp")))
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

/* Prints out the union ipt_matchinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match, int numeric)
{
	const struct ip6t_udp *udp = (struct ip6t_udp *)match->data;

	printf("udp ");
	print_ports("spt", udp->spts[0], udp->spts[1],
		    udp->invflags & IP6T_UDP_INV_SRCPT,
		    numeric);
	print_ports("dpt", udp->dpts[0], udp->dpts[1],
		    udp->invflags & IP6T_UDP_INV_DSTPT,
		    numeric);
	if (udp->invflags & ~IP6T_UDP_INV_MASK)
		printf("Unknown invflags: 0x%X ",
		       udp->invflags & ~IP6T_UDP_INV_MASK);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void save(const struct ip6t_ip6 *ip, const struct ip6t_entry_match *match)
{
	const struct ip6t_udp *udpinfo = (struct ip6t_udp *)match->data;

	if (udpinfo->spts[0] != 0
	    || udpinfo->spts[1] != 0xFFFF) {
		if (udpinfo->invflags & IP6T_UDP_INV_SRCPT)
			printf("! ");
		if (udpinfo->spts[0]
		    != udpinfo->spts[1])
			printf("--sport %u:%u ",
			       udpinfo->spts[0],
			       udpinfo->spts[1]);
		else
			printf("--sport %u ",
			       udpinfo->spts[0]);
	}

	if (udpinfo->dpts[0] != 0
	    || udpinfo->dpts[1] != 0xFFFF) {
		if (udpinfo->invflags & IP6T_UDP_INV_DSTPT)
			printf("! ");
		if (udpinfo->dpts[0]
		    != udpinfo->dpts[1])
			printf("--dport %u:%u ",
			       udpinfo->dpts[0],
			       udpinfo->dpts[1]);
		else
			printf("--dport %u ",
			       udpinfo->dpts[0]);
	}
}

static struct ip6tables_match udp = {
	.name		= "udp",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ip6t_udp)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ip6t_udp)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts,
};

void
_init(void)
{
	register_match6(&udp);
}
