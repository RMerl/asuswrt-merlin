/* Shared library add-on to iptables to add multiple TCP port support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_mport.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"mport v%s options:\n"
" --source-ports port[,port:port,port...]\n"
" --sports ...\n"
"				match source port(s)\n"
" --destination-ports port[,port:port,port...]\n"
" --dports ...\n"
"				match destination port(s)\n"
" --ports port[,port:port,port]\n"
"				match both source and destination port(s)\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "source-ports", 1, 0, '1' },
	{ "sports", 1, 0, '1' }, /* synonym */
	{ "destination-ports", 1, 0, '2' },
	{ "dports", 1, 0, '2' }, /* synonym */
	{ "ports", 1, 0, '3' },
	{0}
};

static void
parse_multi_ports(const char *portstring, struct ipt_mport *minfo,
                  const char *proto)
{
	char *buffer, *cp, *next, *range;
	unsigned int i;
        u_int16_t m;

	buffer = strdup(portstring);
	if (!buffer) exit_error(OTHER_PROBLEM, "strdup failed");

        minfo->pflags = 0;

	for (cp=buffer, i=0, m=1; cp && i<IPT_MULTI_PORTS; cp=next,i++,m<<=1)
	{
		next=strchr(cp, ',');
		if (next) *next++='\0';
                range = strchr(cp, ':');
                if (range) {
                        if (i == IPT_MULTI_PORTS-1)
                                exit_error(PARAMETER_PROBLEM,
                                           "too many ports specified");
                        *range++ = '\0';
                }
		minfo->ports[i] = parse_port(cp, proto);
                if (range) {
                        minfo->pflags |= m;
                        minfo->ports[++i] = parse_port(range, proto);
                        if (minfo->ports[i-1] >= minfo->ports[i])
                                exit_error(PARAMETER_PROBLEM,
                                           "invalid portrange specified");
                        m <<= 1;
                }
	}
	if (cp) exit_error(PARAMETER_PROBLEM, "too many ports specified");
        if (i == IPT_MULTI_PORTS-1)
                minfo->ports[i] = minfo->ports[i-1];
        else if (i < IPT_MULTI_PORTS-1) {
                minfo->ports[i] = ~0;
                minfo->pflags |= 1<<i;
        }
	free(buffer);
}

/* Initialize the match. */
static void
init(struct ipt_entry_match *m, unsigned int *nfcache)
{
}

static const char *
check_proto(const struct ipt_entry *entry)
{
	if (entry->ip.proto == IPPROTO_TCP)
		return "tcp";
	else if (entry->ip.proto == IPPROTO_UDP)
		return "udp";
	else if (!entry->ip.proto)
		exit_error(PARAMETER_PROBLEM,
			   "multiport needs `-p tcp' or `-p udp'");
	else
		exit_error(PARAMETER_PROBLEM,
			   "multiport only works with TCP or UDP");
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	const char *proto;
	struct ipt_mport *minfo
		= (struct ipt_mport *)(*match)->data;

	switch (c) {
	case '1':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(entry);
		parse_multi_ports(argv[optind-1], minfo, proto);
		minfo->flags = IPT_MPORT_SOURCE;
		break;

	case '2':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(entry);
		parse_multi_ports(argv[optind-1], minfo, proto);
		minfo->flags = IPT_MPORT_DESTINATION;
		break;

	case '3':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(entry);
		parse_multi_ports(argv[optind-1], minfo, proto);
		minfo->flags = IPT_MPORT_EITHER;
		break;

	default:
		return 0;
	}

	if (invert)
		exit_error(PARAMETER_PROBLEM,
			   "multiport does not support invert");

	if (*flags)
		exit_error(PARAMETER_PROBLEM,
			   "multiport can only have one option");
	*flags = 1;
	return 1;
}

/* Final check; must specify something. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM, "mport expects an option");
}

static char *
port_to_service(int port, u_int8_t proto)
{
	struct servent *service;

	if ((service = getservbyport(htons(port),
				     proto == IPPROTO_TCP ? "tcp" : "udp")))
		return service->s_name;

	return NULL;
}

static void
print_port(u_int16_t port, u_int8_t protocol, int numeric)
{
	char *service;

	if (numeric || (service = port_to_service(port, protocol)) == NULL)
		printf("%u", port);
	else
		printf("%s", service);
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	const struct ipt_mport *minfo
		= (const struct ipt_mport *)match->data;
	unsigned int i;
        u_int16_t pflags = minfo->pflags;

	printf("mport ");

	switch (minfo->flags) {
	case IPT_MPORT_SOURCE:
		printf("sports ");
		break;

	case IPT_MPORT_DESTINATION:
		printf("dports ");
		break;

	case IPT_MPORT_EITHER:
		printf("ports ");
		break;

	default:
		printf("ERROR ");
		break;
	}

	for (i=0; i < IPT_MULTI_PORTS; i++) {
                if (pflags & (1<<i)
                    && minfo->ports[i] == 65535)
                        break;
                if (i == IPT_MULTI_PORTS-1
                    && minfo->ports[i-1] == minfo->ports[i])
                        break;
		printf("%s", i ? "," : "");
		print_port(minfo->ports[i], ip->proto, numeric);
                if (pflags & (1<<i)) {
                        printf(":");
                        print_port(minfo->ports[++i], ip->proto, numeric);
                }
	}
	printf(" ");
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_mport *minfo
		= (const struct ipt_mport *)match->data;
	unsigned int i;
        u_int16_t pflags = minfo->pflags;

	switch (minfo->flags) {
	case IPT_MPORT_SOURCE:
		printf("--sports ");
		break;

	case IPT_MPORT_DESTINATION:
		printf("--dports ");
		break;

	case IPT_MPORT_EITHER:
		printf("--ports ");
		break;
	}

	for (i=0; i < IPT_MULTI_PORTS; i++) {
                if (pflags & (1<<i)
                    && minfo->ports[i] == 65535)
                        break;
                if (i == IPT_MULTI_PORTS-1
                    && minfo->ports[i-1] == minfo->ports[i])
                        break;
		printf("%s", i ? "," : "");
		print_port(minfo->ports[i], ip->proto, 1);
                if (pflags & (1<<i)) {
                        printf(":");
                        print_port(minfo->ports[++i], ip->proto, 1);
                }
	}
	printf(" ");
}

static struct iptables_match mport = { 
	.next		= NULL,
	.name		= "mport",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_mport)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_mport)),
	.help		= &help,
	.init		= &init,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void
_init(void)
{
	register_match(&mport);
}
