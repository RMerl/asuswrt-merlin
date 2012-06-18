/* Shared library add-on to ip6tables to add ESP support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <ip6tables.h>
#include <linux/netfilter_ipv6/ip6t_esp.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"ESP v%s options:\n"
" --espspi [!] spi[:spi]        match spi (range)\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ .name = "espspi", .has_arg = 1, .flag = 0, .val = '1' },
	{ .name = 0 }
};

static u_int32_t
parse_esp_spi(const char *spistr)
{
	unsigned long int spi;
	char* ep;

	spi = strtoul(spistr, &ep, 0);

	if ( spistr == ep ) {
		exit_error(PARAMETER_PROBLEM,
			   "ESP no valid digits in spi `%s'", spistr);
	}
	if ( spi == ULONG_MAX  && errno == ERANGE ) {
		exit_error(PARAMETER_PROBLEM,
			   "spi `%s' specified too big: would overflow", spistr);
	}	
	if ( *spistr != '\0'  && *ep != '\0' ) {
		exit_error(PARAMETER_PROBLEM,
			   "ESP error parsing spi `%s'", spistr);
	}
	return (u_int32_t) spi;
}

static void
parse_esp_spis(const char *spistring, u_int32_t *spis)
{
	char *buffer;
	char *cp;

	buffer = strdup(spistring);
	if ((cp = strchr(buffer, ':')) == NULL)
		spis[0] = spis[1] = parse_esp_spi(buffer);
	else {
		*cp = '\0';
		cp++;

		spis[0] = buffer[0] ? parse_esp_spi(buffer) : 0;
		spis[1] = cp[0] ? parse_esp_spi(cp) : 0xFFFFFFFF;
		if (spis[0] > spis[1])
			exit_error(PARAMETER_PROBLEM,
				   "Invalid ESP spi range: %s", spistring);
	}
	free(buffer);
}

/* Initialize the match. */
static void
init(struct ip6t_entry_match *m, unsigned int *nfcache)
{
	struct ip6t_esp *espinfo = (struct ip6t_esp *)m->data;

	espinfo->spis[1] = 0xFFFFFFFF;
}

#define ESP_SPI 0x01

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ip6t_entry *entry,
      unsigned int *nfcache,
      struct ip6t_entry_match **match)
{
	struct ip6t_esp *espinfo = (struct ip6t_esp *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & ESP_SPI)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--espspi' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_esp_spis(argv[optind-1], espinfo->spis);
		if (invert)
			espinfo->invflags |= IP6T_ESP_INV_SPI;
		*flags |= ESP_SPI;
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

/* Prints out the union ip6t_matchinfo. */
static void
print(const struct ip6t_ip6 *ip,
      const struct ip6t_entry_match *match, int numeric)
{
	const struct ip6t_esp *esp = (struct ip6t_esp *)match->data;

	printf("esp ");
	print_spis("spi", esp->spis[0], esp->spis[1],
		    esp->invflags & IP6T_ESP_INV_SPI);
	if (esp->invflags & ~IP6T_ESP_INV_MASK)
		printf("Unknown invflags: 0x%X ",
		       esp->invflags & ~IP6T_ESP_INV_MASK);
}

/* Saves the union ip6t_matchinfo in parsable form to stdout. */
static void save(const struct ip6t_ip6 *ip, const struct ip6t_entry_match *match)
{
	const struct ip6t_esp *espinfo = (struct ip6t_esp *)match->data;

	if (!(espinfo->spis[0] == 0
	    && espinfo->spis[1] == 0xFFFFFFFF)) {
		printf("--espspi %s", 
			(espinfo->invflags & IP6T_ESP_INV_SPI) ? "! " : "");
		if (espinfo->spis[0]
		    != espinfo->spis[1])
			printf("%u:%u ",
			       espinfo->spis[0],
			       espinfo->spis[1]);
		else
			printf("%u ",
			       espinfo->spis[0]);
	}

}

static
struct ip6tables_match esp = {
	.name          = "esp",
	.version       = IPTABLES_VERSION,
	.size          = IP6T_ALIGN(sizeof(struct ip6t_esp)),
	.userspacesize = IP6T_ALIGN(sizeof(struct ip6t_esp)),
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
	register_match6(&esp);
}
