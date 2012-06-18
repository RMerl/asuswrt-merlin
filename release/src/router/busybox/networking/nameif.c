/* vi: set sw=4 ts=4: */
/*
 * nameif.c - Naming Interfaces based on MAC address for busybox.
 *
 * Written 2000 by Andi Kleen.
 * Busybox port 2002 by Nick Fedchik <nick@fedchik.org.ua>
 *			Glenn McGrath
 * Extended matching support 2008 by Nico Erfurth <masta@perlgolf.de>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"
#include <syslog.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <linux/sockios.h>

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

/* Taken from linux/sockios.h */
#define SIOCSIFNAME	0x8923	/* set interface name */

/* Octets in one Ethernet addr, from <linux/if_ether.h> */
#define ETH_ALEN	6

#ifndef ifr_newname
#define ifr_newname ifr_ifru.ifru_slave
#endif

typedef struct ethtable_s {
	struct ethtable_s *next;
	struct ethtable_s *prev;
	char *ifname;
	struct ether_addr *mac;
#if ENABLE_FEATURE_NAMEIF_EXTENDED
	char *bus_info;
	char *driver;
#endif
} ethtable_t;

#if ENABLE_FEATURE_NAMEIF_EXTENDED
/* Cut'n'paste from ethtool.h */
#define ETHTOOL_BUSINFO_LEN 32
/* these strings are set to whatever the driver author decides... */
struct ethtool_drvinfo {
	uint32_t cmd;
	char  driver[32]; /* driver short name, "tulip", "eepro100" */
	char  version[32];  /* driver version string */
	char  fw_version[32]; /* firmware version string, if applicable */
	char  bus_info[ETHTOOL_BUSINFO_LEN];  /* Bus info for this IF. */
	/* For PCI devices, use pci_dev->slot_name. */
	char  reserved1[32];
	char  reserved2[16];
	uint32_t n_stats;  /* number of u64's from ETHTOOL_GSTATS */
	uint32_t testinfo_len;
	uint32_t eedump_len; /* Size of data from ETHTOOL_GEEPROM (bytes) */
	uint32_t regdump_len;  /* Size of data from ETHTOOL_GREGS (bytes) */
};
#define ETHTOOL_GDRVINFO  0x00000003 /* Get driver info. */
#endif


static void nameif_parse_selector(ethtable_t *ch, char *selector)
{
	struct ether_addr *lmac;
#if ENABLE_FEATURE_NAMEIF_EXTENDED
	int found_selector = 0;

	while (*selector) {
		char *next;
#endif
		selector = skip_whitespace(selector);
#if ENABLE_FEATURE_NAMEIF_EXTENDED
		if (*selector == '\0')
			break;
		/* Search for the end .... */
		next = skip_non_whitespace(selector);
		if (*next)
			*next++ = '\0';
		/* Check for selectors, mac= is assumed */
		if (strncmp(selector, "bus=", 4) == 0) {
			ch->bus_info = xstrdup(selector + 4);
			found_selector++;
		} else if (strncmp(selector, "driver=", 7) == 0) {
			ch->driver = xstrdup(selector + 7);
			found_selector++;
		} else {
#endif
			lmac = xmalloc(ETH_ALEN);
			ch->mac = ether_aton_r(selector + (strncmp(selector, "mac=", 4) != 0 ? 0 : 4), lmac);
			if (ch->mac == NULL)
				bb_error_msg_and_die("can't parse %s", selector);
#if  ENABLE_FEATURE_NAMEIF_EXTENDED
			found_selector++;
		};
		selector = next;
	}
	if (found_selector == 0)
		bb_error_msg_and_die("no selectors found for %s", ch->ifname);
#endif
}

static void prepend_new_eth_table(ethtable_t **clist, char *ifname, char *selector)
{
	ethtable_t *ch;
	if (strlen(ifname) >= IFNAMSIZ)
		bb_error_msg_and_die("interface name '%s' too long", ifname);
	ch = xzalloc(sizeof(*ch));
	ch->ifname = xstrdup(ifname);
	nameif_parse_selector(ch, selector);
	ch->next = *clist;
	if (*clist)
		(*clist)->prev = ch;
	*clist = ch;
}

#if ENABLE_FEATURE_CLEAN_UP
static void delete_eth_table(ethtable_t *ch)
{
	free(ch->ifname);
#if ENABLE_FEATURE_NAMEIF_EXTENDED
	free(ch->bus_info);
	free(ch->driver);
#endif
	free(ch->mac);
	free(ch);
};
#else
void delete_eth_table(ethtable_t *ch);
#endif

int nameif_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int nameif_main(int argc, char **argv)
{
	ethtable_t *clist = NULL;
	const char *fname = "/etc/mactab";
	int ctl_sk;
	ethtable_t *ch;
	parser_t *parser;
	char *token[2];

	if (1 & getopt32(argv, "sc:", &fname)) {
		openlog(applet_name, 0, LOG_LOCAL0);
		/* Why not just "="? I assume logging to stderr
		 * can't hurt. 2>/dev/null if you don't like it: */
		logmode |= LOGMODE_SYSLOG;
	}
	argc -= optind;
	argv += optind;

	if (argc & 1)
		bb_show_usage();

	if (argc) {
		while (*argv) {
			char *ifname = xstrdup(*argv++);
			prepend_new_eth_table(&clist, ifname, *argv++);
		}
	} else {
		parser = config_open(fname);
		while (config_read(parser, token, 2, 2, "# \t", PARSE_NORMAL))
			prepend_new_eth_table(&clist, token[0], token[1]);
		config_close(parser);
	}

	ctl_sk = xsocket(PF_INET, SOCK_DGRAM, 0);
	parser = config_open2("/proc/net/dev", xfopen_for_read);

	while (clist && config_read(parser, token, 2, 2, "\0: \t", PARSE_NORMAL)) {
		struct ifreq ifr;
#if  ENABLE_FEATURE_NAMEIF_EXTENDED
		struct ethtool_drvinfo drvinfo;
#endif
		if (parser->lineno < 2)
			continue; /* Skip the first two lines */

		/* Find the current interface name and copy it to ifr.ifr_name */
		memset(&ifr, 0, sizeof(struct ifreq));
		strncpy_IFNAMSIZ(ifr.ifr_name, token[0]);

#if ENABLE_FEATURE_NAMEIF_EXTENDED
		/* Check for driver etc. */
		memset(&drvinfo, 0, sizeof(struct ethtool_drvinfo));
		drvinfo.cmd = ETHTOOL_GDRVINFO;
		ifr.ifr_data = (caddr_t) &drvinfo;
		/* Get driver and businfo first, so we have it in drvinfo */
		ioctl(ctl_sk, SIOCETHTOOL, &ifr);
#endif
		ioctl(ctl_sk, SIOCGIFHWADDR, &ifr);

		/* Search the list for a matching device */
		for (ch = clist; ch; ch = ch->next) {
#if ENABLE_FEATURE_NAMEIF_EXTENDED
			if (ch->bus_info && strcmp(ch->bus_info, drvinfo.bus_info) != 0)
				continue;
			if (ch->driver && strcmp(ch->driver, drvinfo.driver) != 0)
				continue;
#endif
			if (ch->mac && memcmp(ch->mac, ifr.ifr_hwaddr.sa_data, ETH_ALEN) != 0)
				continue;
			/* if we came here, all selectors have matched */
			break;
		}
		/* Nothing found for current interface */
		if (!ch)
			continue;

		if (strcmp(ifr.ifr_name, ch->ifname) != 0) {
			strcpy(ifr.ifr_newname, ch->ifname);
			ioctl_or_perror_and_die(ctl_sk, SIOCSIFNAME, &ifr,
					"can't change ifname %s to %s",
					ifr.ifr_name, ch->ifname);
		}
		/* Remove list entry of renamed interface */
		if (ch->prev != NULL)
			ch->prev->next = ch->next;
		else
			clist = ch->next;
		if (ch->next != NULL)
			ch->next->prev = ch->prev;
		if (ENABLE_FEATURE_CLEAN_UP)
			delete_eth_table(ch);
	}
	if (ENABLE_FEATURE_CLEAN_UP) {
		for (ch = clist; ch; ch = ch->next)
			delete_eth_table(ch);
		config_close(parser);
	};

	return 0;
}
