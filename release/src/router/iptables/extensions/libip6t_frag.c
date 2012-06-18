/* Shared library add-on to ip6tables to add Fragmentation header support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6t_frag.h>
                                        
/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"FRAG v%s options:\n"
" --fragid [!] id[:id]          match the id (range)\n"
" --fraglen [!] length          total length of this header\n"
" --fragres                     check the reserved filed, too\n"
" --fragfirst                   matches on the first fragment\n"
" [--fragmore|--fraglast]       there are more fragments or this\n"
"                               is the last one\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ .name = "fragid",    .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = "fraglen",   .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = "fragres",   .has_arg = 0, .flag = 0, .val = '3' },
	{ .name = "fragfirst", .has_arg = 0, .flag = 0, .val = '4' },
	{ .name = "fragmore",  .has_arg = 0, .flag = 0, .val = '5' },
	{ .name = "fraglast",  .has_arg = 0, .flag = 0, .val = '6' },
	{ .name = 0 }
};

static u_int32_t
parse_frag_id(const char *idstr, const char *typestr)
{
	unsigned long int id;
	char* ep;

	id = strtoul(idstr, &ep, 0);

	if ( idstr == ep ) {
		exit_error(PARAMETER_PROBLEM,
			   "FRAG no valid digits in %s `%s'", typestr, idstr);
	}
	if ( id == ULONG_MAX  && errno == ERANGE ) {
		exit_error(PARAMETER_PROBLEM,
			   "%s `%s' specified too big: would overflow",
			   typestr, idstr);
	}	
	if ( *idstr != '\0'  && *ep != '\0' ) {
		exit_error(PARAMETER_PROBLEM,
			   "FRAG error parsing %s `%s'", typestr, idstr);
	}
	return (u_int32_t) id;
}

static void
parse_frag_ids(const char *idstring, u_int32_t *ids)
{
	char *buffer;
	char *cp;

	buffer = strdup(idstring);
	if ((cp = strchr(buffer, ':')) == NULL)
		ids[0] = ids[1] = parse_frag_id(buffer,"id");
	else {
		*cp = '\0';
		cp++;

		ids[0] = buffer[0] ? parse_frag_id(buffer,"id") : 0;
		ids[1] = cp[0] ? parse_frag_id(cp,"id") : 0xFFFFFFFF;
	}
	free(buffer);
}

/* Initialize the match. */
static void
init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
	struct ip6t_frag *fraginfo = (struct ip6t_frag *)m->data;

	fraginfo->ids[0] = 0x0L;
	fraginfo->ids[1] = 0xFFFFFFFF;
	fraginfo->hdrlen = 0;
	fraginfo->flags = 0;
	fraginfo->invflags = 0;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_frag *fraginfo = (struct ip6t_frag *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & IP6T_FRAG_IDS)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--fragid' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_frag_ids(argv[optind-1], fraginfo->ids);
		if (invert)
			fraginfo->invflags |= IP6T_FRAG_INV_IDS;
		fraginfo->flags |= IP6T_FRAG_IDS;
		*flags |= IP6T_FRAG_IDS;
		break;
	case '2':
		if (*flags & IP6T_FRAG_LEN)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--fraglen' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		fraginfo->hdrlen = parse_frag_id(argv[optind-1], "length");
		if (invert)
			fraginfo->invflags |= IP6T_FRAG_INV_LEN;
		fraginfo->flags |= IP6T_FRAG_LEN;
		*flags |= IP6T_FRAG_LEN;
		break;
	case '3':
		if (*flags & IP6T_FRAG_RES)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--fragres' allowed");
		fraginfo->flags |= IP6T_FRAG_RES;
		*flags |= IP6T_FRAG_RES;
		break;
	case '4':
		if (*flags & IP6T_FRAG_FST)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--fragfirst' allowed");
		fraginfo->flags |= IP6T_FRAG_FST;
		*flags |= IP6T_FRAG_FST;
		break;
	case '5':
		if (*flags & (IP6T_FRAG_MF|IP6T_FRAG_NMF)) 
			exit_error(PARAMETER_PROBLEM,
			   "Only one `--fragmore' or `--fraglast' allowed");
		fraginfo->flags |= IP6T_FRAG_MF;
		*flags |= IP6T_FRAG_MF;
		break;
	case '6':
		if (*flags & (IP6T_FRAG_MF|IP6T_FRAG_NMF)) 
			exit_error(PARAMETER_PROBLEM,
			   "Only one `--fragmore' or `--fraglast' allowed");
		fraginfo->flags |= IP6T_FRAG_NMF;
		*flags |= IP6T_FRAG_NMF;
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
print_ids(const char *name, u_int32_t min, u_int32_t max,
	    int invert)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFFFFFF || invert) {
		printf("%s", name);
		if (min == max)
			printf(":%s%u ", inv, min);
		else
			printf("s:%s%u:%u ", inv, min, max);
	}
}

/* Prints out the union ip6t_matchinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match, int numeric)
{
	const struct ip6t_frag *frag = (struct ip6t_frag *)match->data;

	printf("frag ");
	print_ids("id", frag->ids[0], frag->ids[1],
		    frag->invflags & IP6T_FRAG_INV_IDS);

	if (frag->flags & IP6T_FRAG_LEN) {
		printf("length:%s%u ",
			frag->invflags & IP6T_FRAG_INV_LEN ? "!" : "",
			frag->hdrlen);
	}

	if (frag->flags & IP6T_FRAG_RES)
		printf("reserved ");

	if (frag->flags & IP6T_FRAG_FST)
		printf("first ");

	if (frag->flags & IP6T_FRAG_MF)
		printf("more ");

	if (frag->flags & IP6T_FRAG_NMF)
		printf("last ");

	if (frag->invflags & ~IP6T_FRAG_INV_MASK)
		printf("Unknown invflags: 0x%X ",
		       frag->invflags & ~IP6T_FRAG_INV_MASK);
}

/* Saves the union ip6t_matchinfo in parsable form to stdout. */
static void save(const struct ip6t_ip6 *ip, const struct ip6t_entry_match *match)
{
	const struct ip6t_frag *fraginfo = (struct ip6t_frag *)match->data;

	if (!(fraginfo->ids[0] == 0
	    && fraginfo->ids[1] == 0xFFFFFFFF)) {
		printf("--fragid %s", 
			(fraginfo->invflags & IP6T_FRAG_INV_IDS) ? "! " : "");
		if (fraginfo->ids[0]
		    != fraginfo->ids[1])
			printf("%u:%u ",
			       fraginfo->ids[0],
			       fraginfo->ids[1]);
		else
			printf("%u ",
			       fraginfo->ids[0]);
	}

	if (fraginfo->flags & IP6T_FRAG_LEN) {
		printf("--fraglen %s%u ", 
			(fraginfo->invflags & IP6T_FRAG_INV_LEN) ? "! " : "", 
			fraginfo->hdrlen);
	}

	if (fraginfo->flags & IP6T_FRAG_RES)
		printf("--fragres ");

	if (fraginfo->flags & IP6T_FRAG_FST)
		printf("--fragfirst ");

	if (fraginfo->flags & IP6T_FRAG_MF)
		printf("--fragmore ");

	if (fraginfo->flags & IP6T_FRAG_NMF)
		printf("--fraglast ");
}

static
struct ip6tables_match frag = {
	.name          = "frag",
	.version       = IPTABLES_VERSION,
	.size          = IP6T_ALIGN(sizeof(struct ip6t_frag)),
	.userspacesize = IP6T_ALIGN(sizeof(struct ip6t_frag)),
	.help          = &help,
	.init          = &init,
	.parse         = &parse,
	.final_check   = &final_check,
	.print         = &print,
	.save          = &save,
	.extra_opts    = opts
};

void
_init(void)
{
	register_match6(&frag);
}
