/* Shared library add-on to iptables to add AH support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_ah.h>
                                        
/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"AH v%s options:\n"
" --ahspi [!] spi[:spi]\n"
"				match spi (range)\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "ahspi", 1, 0, '1' },
	{0}
};

static u_int32_t
parse_ah_spi(const char *spistr)
{
	unsigned long int spi;
	char* ep;

	spi =  strtoul(spistr,&ep,0) ;

	if ( spistr == ep ) {
		exit_error(PARAMETER_PROBLEM,
			   "AH no valid digits in spi `%s'", spistr);
	}
	if ( spi == ULONG_MAX  && errno == ERANGE ) {
		exit_error(PARAMETER_PROBLEM,
			   "spi `%s' specified too big: would overflow", spistr);
	}	
	if ( *spistr != '\0'  && *ep != '\0' ) {
		exit_error(PARAMETER_PROBLEM,
			   "AH error parsing spi `%s'", spistr);
	}
	return (u_int32_t) spi;
}

static void
parse_ah_spis(const char *spistring, u_int32_t *spis)
{
	char *buffer;
	char *cp;

	buffer = strdup(spistring);
	if ((cp = strchr(buffer, ':')) == NULL)
		spis[0] = spis[1] = parse_ah_spi(buffer);
	else {
		*cp = '\0';
		cp++;

		spis[0] = buffer[0] ? parse_ah_spi(buffer) : 0;
		spis[1] = cp[0] ? parse_ah_spi(cp) : 0xFFFFFFFF;
	}
	free(buffer);
}

/* Initialize the match. */
static void
init(struct ipt_entry_match *m, unsigned int *nfcache)
{
	struct ipt_ah *ahinfo = (struct ipt_ah *)m->data;

	ahinfo->spis[1] = 0xFFFFFFFF;
}

#define AH_SPI 0x01

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_ah *ahinfo = (struct ipt_ah *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & AH_SPI)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--ahspi' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_ah_spis(argv[optind-1], ahinfo->spis);
		if (invert)
			ahinfo->invflags |= IPT_AH_INV_SPI;
		*flags |= AH_SPI;
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

/* Prints out the union ipt_matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match, int numeric)
{
	const struct ipt_ah *ah = (struct ipt_ah *)match->data;

	printf("ah ");
	print_spis("spi", ah->spis[0], ah->spis[1],
		    ah->invflags & IPT_AH_INV_SPI);
	if (ah->invflags & ~IPT_AH_INV_MASK)
		printf("Unknown invflags: 0x%X ",
		       ah->invflags & ~IPT_AH_INV_MASK);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	const struct ipt_ah *ahinfo = (struct ipt_ah *)match->data;

	if (!(ahinfo->spis[0] == 0
	    && ahinfo->spis[1] == 0xFFFFFFFF)) {
		printf("--ahspi %s", 
			(ahinfo->invflags & IPT_AH_INV_SPI) ? "! " : "");
		if (ahinfo->spis[0]
		    != ahinfo->spis[1])
			printf("%u:%u ",
			       ahinfo->spis[0],
			       ahinfo->spis[1]);
		else
			printf("%u ",
			       ahinfo->spis[0]);
	}

}

static struct iptables_match ah = { 
	.next 		= NULL,
	.name 		= "ah",
	.version 	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_ah)),
	.userspacesize 	= IPT_ALIGN(sizeof(struct ipt_ah)),
	.help 		= &help,
	.init 		= &init,
	.parse 		= &parse,
	.final_check 	= &final_check,
	.print 		= &print,
	.save 		= &save,
	.extra_opts 	= opts
};

void
_init(void)
{
	register_match(&ah);
}
