/* Shared library add-on to ip6tables to add AH support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6t_ah.h>
                                        
/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"AH v%s options:\n"
" --ahspi [!] spi[:spi]         match spi (range)\n"
" --ahlen [!] length            total length of this header\n"
" --ahres                       check the reserved filed, too\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ .name = "ahspi", .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = "ahlen", .has_arg = 1, .flag = 0, .val = '2' },
	{ .name = "ahres", .has_arg = 0, .flag = 0, .val = '3' },
	{ .name = 0 }
};

static u_int32_t
parse_ah_spi(const char *spistr, const char *typestr)
{
	unsigned long int spi;
	char* ep;

	spi = strtoul(spistr, &ep, 0);

	if ( spistr == ep )
		exit_error(PARAMETER_PROBLEM,
			   "AH no valid digits in %s `%s'", typestr, spistr);

	if ( spi == ULONG_MAX  && errno == ERANGE )
		exit_error(PARAMETER_PROBLEM,
			   "%s `%s' specified too big: would overflow",
			   typestr, spistr);

	if ( *spistr != '\0'  && *ep != '\0' )
		exit_error(PARAMETER_PROBLEM,
			   "AH error parsing %s `%s'", typestr, spistr);

	return (u_int32_t) spi;
}

static void
parse_ah_spis(const char *spistring, u_int32_t *spis)
{
	char *buffer;
	char *cp;

	buffer = strdup(spistring);
	if ((cp = strchr(buffer, ':')) == NULL)
		spis[0] = spis[1] = parse_ah_spi(buffer, "spi");
	else {
		*cp = '\0';
		cp++;

		spis[0] = buffer[0] ? parse_ah_spi(buffer, "spi") : 0;
		spis[1] = cp[0] ? parse_ah_spi(cp, "spi") : 0xFFFFFFFF;
	}
	free(buffer);
}

/* Initialize the match. */
static void
init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
	struct ip6t_ah *ahinfo = (struct ip6t_ah *)m->data;

	ahinfo->spis[1] = 0xFFFFFFFF;
	ahinfo->hdrlen = 0;
	ahinfo->hdrres = 0;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_ah *ahinfo = (struct ip6t_ah *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & IP6T_AH_SPI)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--ahspi' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_ah_spis(argv[optind-1], ahinfo->spis);
		if (invert)
			ahinfo->invflags |= IP6T_AH_INV_SPI;
		*flags |= IP6T_AH_SPI;
		break;
	case '2':
		if (*flags & IP6T_AH_LEN)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--ahlen' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		ahinfo->hdrlen = parse_ah_spi(argv[optind-1], "length");
		if (invert)
			ahinfo->invflags |= IP6T_AH_INV_LEN;
		*flags |= IP6T_AH_LEN;
		break;
	case '3':
		if (*flags & IP6T_AH_RES)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--ahres' allowed");
		ahinfo->hdrres = 1;
		*flags |= IP6T_AH_RES;
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
print_spis(const char *name, u_int32_t min, u_int32_t max,
	    int invert)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFFFFFF || invert) {
		if (min == max)
			printf("%s:%s%u ", name, inv, min);
		else
			printf("%ss:%s%u:%u ", name, inv, min, max);
	}
}

static void
print_len(const char *name, u_int32_t len, int invert)
{
	const char *inv = invert ? "!" : "";

	if (len != 0 || invert)
		printf("%s:%s%u ", name, inv, len);
}

/* Prints out the union ip6t_matchinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match, int numeric)
{
	const struct ip6t_ah *ah = (struct ip6t_ah *)match->data;

	printf("ah ");
	print_spis("spi", ah->spis[0], ah->spis[1],
		    ah->invflags & IP6T_AH_INV_SPI);
	print_len("length", ah->hdrlen, 
		    ah->invflags & IP6T_AH_INV_LEN);

	if (ah->hdrres)
		printf("reserved ");

	if (ah->invflags & ~IP6T_AH_INV_MASK)
		printf("Unknown invflags: 0x%X ",
		       ah->invflags & ~IP6T_AH_INV_MASK);
}

/* Saves the union ip6t_matchinfo in parsable form to stdout. */
static void save(const struct ip6t_ip6 *ip, const struct ip6t_entry_match *match)
{
	const struct ip6t_ah *ahinfo = (struct ip6t_ah *)match->data;

	if (!(ahinfo->spis[0] == 0
	    && ahinfo->spis[1] == 0xFFFFFFFF)) {
		printf("--ahspi %s", 
			(ahinfo->invflags & IP6T_AH_INV_SPI) ? "! " : "");
		if (ahinfo->spis[0]
		    != ahinfo->spis[1])
			printf("%u:%u ",
			       ahinfo->spis[0],
			       ahinfo->spis[1]);
		else
			printf("%u ",
			       ahinfo->spis[0]);
	}

	if (ahinfo->hdrlen != 0 || (ahinfo->invflags & IP6T_AH_INV_LEN) ) {
		printf("--ahlen %s%u ", 
			(ahinfo->invflags & IP6T_AH_INV_LEN) ? "! " : "", 
			ahinfo->hdrlen);
	}

	if (ahinfo->hdrres != 0 )
		printf("--ahres ");
}

static
struct ip6tables_match ah = {
	.name          = "ah",
	.version       = IPTABLES_VERSION,
	.size          = IP6T_ALIGN(sizeof(struct ip6t_ah)),
	.userspacesize = IP6T_ALIGN(sizeof(struct ip6t_ah)),
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
	register_match6(&ah);
}
