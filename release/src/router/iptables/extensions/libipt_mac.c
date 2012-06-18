/* Shared library add-on to iptables to add MAC address support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#if defined(__GLIBC__) && __GLIBC__ == 2
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif
#include <iptables.h>
#include <linux/netfilter_ipv4/ipt_mac.h>

/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"MAC v%s options:\n"
" --mac-source [!] XX:XX:XX:XX:XX:XX\n"
"				Match source MAC address\n"
"\n", IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "mac-source", 1, 0, '1' },
	{0}
};

static void
parse_mac(const char *mac, struct ipt_mac_info *info)
{
	unsigned int i = 0;

	if (strlen(mac) != ETH_ALEN*3-1)
		exit_error(PARAMETER_PROBLEM, "Bad mac address `%s'", mac);

	for (i = 0; i < ETH_ALEN; i++) {
		long number;
		char *end;

		number = strtol(mac + i*3, &end, 16);

		if (end == mac + i*3 + 2
		    && number >= 0
		    && number <= 255)
			info->srcaddr[i] = number;
		else
			exit_error(PARAMETER_PROBLEM,
				   "Bad mac address `%s'", mac);
	}
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct ipt_entry *entry,
      unsigned int *nfcache,
      struct ipt_entry_match **match)
{
	struct ipt_mac_info *macinfo = (struct ipt_mac_info *)(*match)->data;

	switch (c) {
	case '1':
		check_inverse(optarg, &invert, &optind, 0);
		parse_mac(argv[optind-1], macinfo);
		if (invert)
			macinfo->invert = 1;
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

static void print_mac(unsigned char macaddress[ETH_ALEN])
{
	unsigned int i;

	printf("%02X", macaddress[0]);
	for (i = 1; i < ETH_ALEN; i++)
		printf(":%02X", macaddress[i]);
	printf(" ");
}

/* Final check; must have specified --mac. */
static void final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "You must specify `--mac-source'");
}

/* Prints out the matchinfo. */
static void
print(const struct ipt_ip *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	printf("MAC ");

	if (((struct ipt_mac_info *)match->data)->invert)
		printf("! ");
	
	print_mac(((struct ipt_mac_info *)match->data)->srcaddr);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void save(const struct ipt_ip *ip, const struct ipt_entry_match *match)
{
	if (((struct ipt_mac_info *)match->data)->invert)
		printf("! ");

	printf("--mac-source ");
	print_mac(((struct ipt_mac_info *)match->data)->srcaddr);
}

static struct iptables_match mac = { 
	.next		= NULL,
 	.name		= "mac",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_mac_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_mac_info)),
	.help		= &help,
	.parse		= &parse,
	.final_check	= &final_check,
	.print		= &print,
	.save		= &save,
	.extra_opts	= opts
};

void _init(void)
{
	register_match(&mac);
}
