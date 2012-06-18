/* Shared library add-on to ip6tables to add Routing header support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <ip6tables.h>
/*#include <linux/in6.h>*/
#include <linux/netfilter_ipv6/ip6t_rt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
                                        
/*#define DEBUG	1*/

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"RT v%s options:\n"
" --rt-type [!] type            match the type\n"
" --rt-segsleft [!] num[:num]   match the Segments Left field (range)\n"
" --rt-len [!] length           total length of this header\n"
" --rt-0-res                    check the reserved filed, too (type 0)\n"
" --rt-0-addrs ADDR[,ADDR...]   Type=0 addresses (list, max: %d)\n"
" --rt-0-not-strict             List of Type=0 addresses not a strict list\n",
IPTABLES_VERSION, IP6T_RT_HOPS);
}

static struct option opts[] = {
	{ "rt-type", 1, 0, '1' },
	{ "rt-segsleft", 1, 0, '2' },
	{ "rt-len", 1, 0, '3' },
	{ "rt-0-res", 0, 0, '4' },
	{ "rt-0-addrs", 1, 0, '5' },
	{ "rt-0-not-strict", 0, 0, '6' },
	{0}
};

static u_int32_t
parse_rt_num(const char *idstr, const char *typestr)
{
	unsigned long int id;
	char* ep;

	id =  strtoul(idstr,&ep,0) ;

	if ( idstr == ep ) {
		exit_error(PARAMETER_PROBLEM,
			   "RT no valid digits in %s `%s'", typestr, idstr);
	}
	if ( id == ULONG_MAX  && errno == ERANGE ) {
		exit_error(PARAMETER_PROBLEM,
			   "%s `%s' specified too big: would overflow",
			   typestr, idstr);
	}	
	if ( *idstr != '\0'  && *ep != '\0' ) {
		exit_error(PARAMETER_PROBLEM,
			   "RT error parsing %s `%s'", typestr, idstr);
	}
	return (u_int32_t) id;
}

static void
parse_rt_segsleft(const char *idstring, u_int32_t *ids)
{
	char *buffer;
	char *cp;

	buffer = strdup(idstring);
	if ((cp = strchr(buffer, ':')) == NULL)
		ids[0] = ids[1] = parse_rt_num(buffer,"segsleft");
	else {
		*cp = '\0';
		cp++;

		ids[0] = buffer[0] ? parse_rt_num(buffer,"segsleft") : 0;
		ids[1] = cp[0] ? parse_rt_num(cp,"segsleft") : 0xFFFFFFFF;
	}
	free(buffer);
}

static char *
addr_to_numeric(const struct in6_addr *addrp)
{
	static char buf[50+1];
	return (char *)inet_ntop(AF_INET6, addrp, buf, sizeof(buf));
}

static struct in6_addr *
numeric_to_addr(const char *num)
{
	static struct in6_addr ap;
	int err;

	if ((err=inet_pton(AF_INET6, num, &ap)) == 1)
		return &ap;
#ifdef DEBUG
	fprintf(stderr, "\nnumeric2addr: %d\n", err);
#endif
        exit_error(PARAMETER_PROBLEM, "bad address: %s", num);

	return (struct in6_addr *)NULL;
}


static int
parse_addresses(const char *addrstr, struct in6_addr *addrp)
{
        char *buffer, *cp, *next;
        unsigned int i;
	
	buffer = strdup(addrstr);
        if (!buffer) exit_error(OTHER_PROBLEM, "strdup failed");
			
        for (cp=buffer, i=0; cp && i<IP6T_RT_HOPS; cp=next,i++)
        {
                next=strchr(cp, ',');
                if (next) *next++='\0';
		memcpy(&(addrp[i]), numeric_to_addr(cp), sizeof(struct in6_addr));
#if DEBUG
		printf("addr str: %s\n", cp);
		printf("addr ip6: %s\n", addr_to_numeric((numeric_to_addr(cp))));
		printf("addr [%d]: %s\n", i, addr_to_numeric(&(addrp[i])));
#endif
	}
        if (cp) exit_error(PARAMETER_PROBLEM, "too many addresses specified");

	free(buffer);

#if DEBUG
	printf("addr nr: %d\n", i);
#endif

	return i;
}

/* Initialize the match. */
static void
init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
	struct ip6t_rt *rtinfo = (struct ip6t_rt *)m->data;

	rtinfo->rt_type = 0x0L;
	rtinfo->segsleft[0] = 0x0L;
	rtinfo->segsleft[1] = 0xFFFFFFFF;
	rtinfo->hdrlen = 0;
	rtinfo->flags = 0;
	rtinfo->invflags = 0;
	rtinfo->addrnr = 0;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_rt *rtinfo = (struct ip6t_rt *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & IP6T_RT_TYP)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--rt-type' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		rtinfo->rt_type = parse_rt_num(argv[optind-1], "type");
		if (invert)
			rtinfo->invflags |= IP6T_RT_INV_TYP;
		rtinfo->flags |= IP6T_RT_TYP;
		*flags |= IP6T_RT_TYP;
		break;
	case '2':
		if (*flags & IP6T_RT_SGS)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--rt-segsleft' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_rt_segsleft(argv[optind-1], rtinfo->segsleft);
		if (invert)
			rtinfo->invflags |= IP6T_RT_INV_SGS;
		rtinfo->flags |= IP6T_RT_SGS;
		*flags |= IP6T_RT_SGS;
		break;
	case '3':
		if (*flags & IP6T_RT_LEN)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--rt-len' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		rtinfo->hdrlen = parse_rt_num(argv[optind-1], "length");
		if (invert)
			rtinfo->invflags |= IP6T_RT_INV_LEN;
		rtinfo->flags |= IP6T_RT_LEN;
		*flags |= IP6T_RT_LEN;
		break;
	case '4':
		if (*flags & IP6T_RT_RES)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--rt-0-res' allowed");
		if ( !(*flags & IP6T_RT_TYP) || (rtinfo->rt_type != 0) || (rtinfo->invflags & IP6T_RT_INV_TYP) )
			exit_error(PARAMETER_PROBLEM,
				   "`--rt-type 0' required before `--rt-0-res'");
		rtinfo->flags |= IP6T_RT_RES;
		*flags |= IP6T_RT_RES;
		break;
	case '5':
		if (*flags & IP6T_RT_FST)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--rt-0-addrs' allowed");
		if ( !(*flags & IP6T_RT_TYP) || (rtinfo->rt_type != 0) || (rtinfo->invflags & IP6T_RT_INV_TYP) )
			exit_error(PARAMETER_PROBLEM,
				   "`--rt-type 0' required before `--rt-0-addrs'");
		check_inverse(optarg, &invert, &optind, 0);
		if (invert)
			exit_error(PARAMETER_PROBLEM,
				   " '!' not allowed with `--rt-0-addrs'");
		rtinfo->addrnr = parse_addresses(argv[optind-1], rtinfo->addrs);
		rtinfo->flags |= IP6T_RT_FST;
		*flags |= IP6T_RT_FST;
		break;
	case '6':
		if (*flags & IP6T_RT_FST_NSTRICT)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--rt-0-not-strict' allowed");
		if ( !(*flags & IP6T_RT_FST) )
			exit_error(PARAMETER_PROBLEM,
				   "`--rt-0-addr ...' required before `--rt-0-not-strict'");
		rtinfo->flags |= IP6T_RT_FST_NSTRICT;
		*flags |= IP6T_RT_FST_NSTRICT;
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

static void
print_nums(const char *name, u_int32_t min, u_int32_t max,
	    int invert)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFFFFFF || invert) {
		printf("%s", name);
		if (min == max) {
			printf(":%s", inv);
			printf("%u", min);
		} else {
			printf("s:%s", inv);
			printf("%u",min);
			printf(":");
			printf("%u",max);
		}
		printf(" ");
	}
}

static void
print_addresses(int addrnr, struct in6_addr *addrp)
{
	unsigned int i;

	for(i=0; i<addrnr; i++){
		printf("%s%c", addr_to_numeric(&(addrp[i])), (i!=addrnr-1)?',':' ');
	}
}

/* Prints out the union ip6t_matchinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match, int numeric)
{
	const struct ip6t_rt *rtinfo = (struct ip6t_rt *)match->data;

	printf("rt ");
	if (rtinfo->flags & IP6T_RT_TYP)
	    printf("type:%s%d ", rtinfo->invflags & IP6T_RT_INV_TYP ? "!" : "",
		    rtinfo->rt_type);
	print_nums("segsleft", rtinfo->segsleft[0], rtinfo->segsleft[1],
		    rtinfo->invflags & IP6T_RT_INV_SGS);
	if (rtinfo->flags & IP6T_RT_LEN) {
		printf("length");
		printf(":%s", rtinfo->invflags & IP6T_RT_INV_LEN ? "!" : "");
		printf("%u", rtinfo->hdrlen);
		printf(" ");
	}
	if (rtinfo->flags & IP6T_RT_RES) printf("reserved ");
	if (rtinfo->flags & IP6T_RT_FST) printf("0-addrs ");
	print_addresses(rtinfo->addrnr, (struct in6_addr *)rtinfo->addrs);
	if (rtinfo->flags & IP6T_RT_FST_NSTRICT) printf("0-not-strict ");
	if (rtinfo->invflags & ~IP6T_RT_INV_MASK)
		printf("Unknown invflags: 0x%X ",
		       rtinfo->invflags & ~IP6T_RT_INV_MASK);
}

/* Saves the union ip6t_matchinfo in parsable form to stdout. */
static void save(const struct ip6t_ip6 *ip, const struct ip6t_entry_match *match)
{
	const struct ip6t_rt *rtinfo = (struct ip6t_rt *)match->data;

	if (rtinfo->flags & IP6T_RT_TYP) {
		printf("--rt-type %s%u ", 
			(rtinfo->invflags & IP6T_RT_INV_TYP) ? "! " : "", 
			rtinfo->rt_type);
	}

	if (!(rtinfo->segsleft[0] == 0
	    && rtinfo->segsleft[1] == 0xFFFFFFFF)) {
		printf("--rt-segsleft %s", 
			(rtinfo->invflags & IP6T_RT_INV_SGS) ? "! " : "");
		if (rtinfo->segsleft[0]
		    != rtinfo->segsleft[1])
			printf("%u:%u ",
			       rtinfo->segsleft[0],
			       rtinfo->segsleft[1]);
		else
			printf("%u ",
			       rtinfo->segsleft[0]);
	}

	if (rtinfo->flags & IP6T_RT_LEN) {
		printf("--rt-len %s%u ", 
			(rtinfo->invflags & IP6T_RT_INV_LEN) ? "! " : "", 
			rtinfo->hdrlen);
	}

	if (rtinfo->flags & IP6T_RT_RES) printf("--rt-0-res ");
	if (rtinfo->flags & IP6T_RT_FST) printf("--rt-0-addrs ");
	print_addresses(rtinfo->addrnr, (struct in6_addr *)rtinfo->addrs);
	if (rtinfo->flags & IP6T_RT_FST_NSTRICT) printf("--rt-0-not-strict ");

}

static struct ip6tables_match rt = {
	.name		= "rt",
	.version	= IPTABLES_VERSION,
	.size		= IP6T_ALIGN(sizeof(struct ip6t_rt)),
	.userspacesize	= IP6T_ALIGN(sizeof(struct ip6t_rt)),
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
	register_match6(&rt);
}
