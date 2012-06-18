/* ipv6header match - matches IPv6 packets based
on whether they contain certain headers */

/* Original idea: Brad Chapman 
 * Rewritten by: Andras Kis-Szabo <kisza@sch.bme.hu> */

#include <getopt.h>
#include <ip6tables.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>

#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/netfilter_ipv6/ip6t_ipv6header.h>

/* This maybe required 
#include <linux/in.h>
#include <linux/in6.h>
*/


/* A few hardcoded protocols for 'all' and in case the user has no
 *    /etc/protocols */
struct pprot {
	char *name;
	u_int8_t num;
};

struct numflag {
	u_int8_t proto;
	u_int8_t flag;
};

static const struct pprot chain_protos[] = {
	{ "hop-by-hop", IPPROTO_HOPOPTS },
	{ "protocol", IPPROTO_RAW },
	{ "hop", IPPROTO_HOPOPTS },
	{ "dst", IPPROTO_DSTOPTS },
	{ "route", IPPROTO_ROUTING },
	{ "frag", IPPROTO_FRAGMENT },
	{ "auth", IPPROTO_AH },
	{ "esp", IPPROTO_ESP },
	{ "none", IPPROTO_NONE },
	{ "prot", IPPROTO_RAW },
	{ "0", IPPROTO_HOPOPTS },
	{ "60", IPPROTO_DSTOPTS },
	{ "43", IPPROTO_ROUTING },
	{ "44", IPPROTO_FRAGMENT },
	{ "51", IPPROTO_AH },
	{ "50", IPPROTO_ESP },
	{ "59", IPPROTO_NONE },
	{ "255", IPPROTO_RAW },
	/* { "all", 0 }, */
};

static const struct numflag chain_flags[] = {
	{ IPPROTO_HOPOPTS, MASK_HOPOPTS },
	{ IPPROTO_DSTOPTS, MASK_DSTOPTS },
	{ IPPROTO_ROUTING, MASK_ROUTING },
	{ IPPROTO_FRAGMENT, MASK_FRAGMENT },
	{ IPPROTO_AH, MASK_AH },
	{ IPPROTO_ESP, MASK_ESP },
	{ IPPROTO_NONE, MASK_NONE },
	{ IPPROTO_RAW, MASK_PROTO },
};

static char *
proto_to_name(u_int8_t proto, int nolookup)
{
        unsigned int i;

        if (proto && !nolookup) {
                struct protoent *pent = getprotobynumber(proto);
                if (pent)
                        return pent->p_name;
        }

        for (i = 0; i < sizeof(chain_protos)/sizeof(struct pprot); i++)
                if (chain_protos[i].num == proto)
                        return chain_protos[i].name;

        return NULL;
}

static u_int16_t
name_to_proto(const char *s)
{
        unsigned int proto=0;
        struct protoent *pent;

        if ((pent = getprotobyname(s)))
        	proto = pent->p_proto;
        else {
        	unsigned int i;
        	for (i = 0;
        		i < sizeof(chain_protos)/sizeof(struct pprot);
        		i++) {
        		if (strcmp(s, chain_protos[i].name) == 0) {
        			proto = chain_protos[i].num;
        			break;
        		}
        	}

        	if (i == sizeof(chain_protos)/sizeof(struct pprot))
        		exit_error(PARAMETER_PROBLEM,
        			"unknown header `%s' specified",
        			s);
        }

        return (u_int16_t)proto;
}

static unsigned int 
add_proto_to_mask(int proto){
	unsigned int i=0, flag=0;

	for (i = 0;
		i < sizeof(chain_flags)/sizeof(struct numflag);
		i++) {
			if (proto == chain_flags[i].proto){
				flag = chain_flags[i].flag;
				break;
			}
	}

	if (i == sizeof(chain_flags)/sizeof(struct numflag))
		exit_error(PARAMETER_PROBLEM,
		"unknown header `%d' specified",
		proto);
	
	return flag;
}	

static void
help(void)
{
	printf(
"ipv6header v%s match options:\n"
"--header [!] headers     Type of header to match, by name\n"
"                         names: hop,dst,route,frag,auth,esp,none,proto\n"
"                    long names: hop-by-hop,ipv6-opts,ipv6-route,\n"
"                                ipv6-frag,ah,esp,ipv6-nonxt,protocol\n"
"                       numbers: 0,60,43,44,51,50,59\n"
"--soft                    The header CONTAINS the specified extensions\n",
	IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "header", 1, 0, '1' },
	{ "soft", 0, 0, '2' },
	{ 0 }
};

static void
init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
	struct ip6t_ipv6header_info *info = (struct ip6t_ipv6header_info *)m->data;

	info->matchflags = 0x00;
	info->invflags = 0x00;
	info->modeflag = 0x00;
}

static unsigned int
parse_header(const char *flags) {
        unsigned int ret = 0;
        char *ptr;
        char *buffer;

        buffer = strdup(flags);

        for (ptr = strtok(buffer, ","); ptr; ptr = strtok(NULL, ",")) 
		ret |= add_proto_to_mask(name_to_proto(ptr));
                
        free(buffer);
        return ret;
}

#define IPV6_HDR_HEADER	0x01
#define IPV6_HDR_SOFT	0x02

/* Parses command options; returns 0 if it ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_ipv6header_info *info = (struct ip6t_ipv6header_info *)(*match)->data;

	switch (c) {
		case '1' : 
			/* Parse the provided header names */
			if (*flags & IPV6_HDR_HEADER)
				exit_error(PARAMETER_PROBLEM,
					"Only one `--header' allowed");

			check_inverse(optarg, &invert, &optind, 0);

			if (! (info->matchflags = parse_header(argv[optind-1])) )
				exit_error(PARAMETER_PROBLEM, "ip6t_ipv6header: cannot parse header names");

			if (invert) 
				info->invflags |= 0xFF;
			*flags |= IPV6_HDR_HEADER;
			break;
		case '2' : 
			/* Soft-mode requested? */
			if (*flags & IPV6_HDR_SOFT)
				exit_error(PARAMETER_PROBLEM,
					"Only one `--soft' allowed");

			info->modeflag |= 0xFF;
			*flags |= IPV6_HDR_SOFT;
			break;
		default:
			return 0;
	}

	return 1;
}

/* Checks the flags variable */
static void
final_check(unsigned int flags)
{
	if (!flags) exit_error(PARAMETER_PROBLEM, "ip6t_ipv6header: no options specified");
}

static void
print_header(u_int8_t flags){
        int have_flag = 0;

        while (flags) {
                unsigned int i;

                for (i = 0; (flags & chain_flags[i].flag) == 0; i++);

                if (have_flag)
                        printf(",");

                printf("%s", proto_to_name(chain_flags[i].proto,0));
                have_flag = 1;

                flags &= ~chain_flags[i].flag;
        }

        if (!have_flag)
                printf("NONE");
}

/* Prints out the match */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match,
      int numeric)
{
	const struct ip6t_ipv6header_info *info = (const struct ip6t_ipv6header_info *)match->data;
	printf("ipv6header ");

        if (info->matchflags || info->invflags) {
                printf("flags:%s", info->invflags ? "!" : "");
                if (numeric)
                        printf("0x%02X ", info->matchflags);
                else {
                        print_header(info->matchflags);
                        printf(" ");
                }
        }

	if (info->modeflag)
		printf("soft ");

	return;
}

/* Saves the match */
static void
save(const struct ip6t_ip6 *ip,
     const struct ip6t_entry_match *match)
{

	const struct ip6t_ipv6header_info *info = (const struct ip6t_ipv6header_info *)match->data;

	printf("--header ");
	printf("%s", info->invflags ? "!" : "");
	print_header(info->matchflags);
	printf(" ");
	if (info->modeflag)
		printf("--soft ");

	return;
}

static
struct ip6tables_match ipv6header = {
	.name		= "ipv6header",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ip6t_ipv6header_info)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ip6t_ipv6header_info)),
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
	register_match6(&ipv6header);
}
