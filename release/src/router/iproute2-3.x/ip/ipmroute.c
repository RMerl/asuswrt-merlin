/*
 * ipmroute.c		"ip mroute".
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>

#include "utils.h"

char filter_dev[16];
int  filter_family;

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip mroute show [ PREFIX ] [ from PREFIX ] [ iif DEVICE ]\n");
#if 0
	fprintf(stderr, "Usage: ip mroute [ add | del ] DESTINATION from SOURCE [ iif DEVICE ] [ oif DEVICE ]\n");
#endif
	exit(-1);
}

static char *viftable[32];

struct rtfilter
{
	inet_prefix mdst;
	inet_prefix msrc;
} filter;

static void read_viftable(void)
{
	char buf[256];
	FILE *fp = fopen("/proc/net/ip_mr_vif", "r");

	if (!fp)
		return;

	if (!fgets(buf, sizeof(buf), fp)) {
		fclose(fp);
		return;
	}
	while (fgets(buf, sizeof(buf), fp)) {
		int vifi;
		char dev[256];

		if (sscanf(buf, "%d%s", &vifi, dev) < 2)
			continue;

		if (vifi<0 || vifi>31)
			continue;

		viftable[vifi] = strdup(dev);
	}
	fclose(fp);
}

static void read_mroute_list(FILE *ofp)
{
	char buf[256];
	FILE *fp = fopen("/proc/net/ip_mr_cache", "r");

	if (!fp)
		return;

	if (!fgets(buf, sizeof(buf), fp)) {
		fclose(fp);
		return;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		inet_prefix maddr, msrc;
		unsigned pkts, b, w;
		int vifi;
		char oiflist[256];
		char sbuf[256];
		char mbuf[256];
		char obuf[256];

		oiflist[0] = 0;
		if (sscanf(buf, "%x%x%d%u%u%u %[^\n]",
			   maddr.data, msrc.data, &vifi,
			   &pkts, &b, &w, oiflist) < 6)
			continue;

		if (vifi!=-1 && (vifi < 0 || vifi>31))
			continue;

		if (filter_dev[0] && (vifi<0 || strcmp(filter_dev, viftable[vifi])))
			continue;
		if (filter.mdst.family && inet_addr_match(&maddr, &filter.mdst, filter.mdst.bitlen))
			continue;
		if (filter.msrc.family && inet_addr_match(&msrc, &filter.msrc, filter.msrc.bitlen))
			continue;

		snprintf(obuf, sizeof(obuf), "(%s, %s)",
			 format_host(AF_INET, 4, &msrc.data[0], sbuf, sizeof(sbuf)),
			 format_host(AF_INET, 4, &maddr.data[0], mbuf, sizeof(mbuf)));

		fprintf(ofp, "%-32s Iif: ", obuf);

		if (vifi == -1)
			fprintf(ofp, "unresolved ");
		else
			fprintf(ofp, "%-10s ", viftable[vifi]);

		if (oiflist[0]) {
			char *next = NULL;
			char *p = oiflist;
			int ovifi, ottl;

			fprintf(ofp, "Oifs: ");

			while (p) {
				next = strchr(p, ' ');
				if (next) {
					*next = 0;
					next++;
				}
				if (sscanf(p, "%d:%d", &ovifi, &ottl)<2) {
					p = next;
					continue;
				}
				p = next;

				fprintf(ofp, "%s", viftable[ovifi]);
				if (ottl>1)
					fprintf(ofp, "(ttl %d) ", ovifi);
				else
					fprintf(ofp, " ");
			}
		}

		if (show_stats && b) {
			fprintf(ofp, "%s  %u packets, %u bytes", _SL_, pkts, b);
			if (w)
				fprintf(ofp, ", %u arrived on wrong iif.", w);
		}
		fprintf(ofp, "\n");
	}
	fclose(fp);
}


static int mroute_list(int argc, char **argv)
{
	while (argc > 0) {
		if (strcmp(*argv, "iif") == 0) {
			NEXT_ARG();
			strncpy(filter_dev, *argv, sizeof(filter_dev)-1);
		} else if (matches(*argv, "from") == 0) {
			NEXT_ARG();
			get_prefix(&filter.msrc, *argv, AF_INET);
		} else {
			if (strcmp(*argv, "to") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			get_prefix(&filter.mdst, *argv, AF_INET);
		}
		argv++; argc--;
	}

	read_viftable();
	read_mroute_list(stdout);
	return 0;
}

int do_multiroute(int argc, char **argv)
{
	if (argc < 1)
		return mroute_list(0, NULL);
#if 0
	if (matches(*argv, "add") == 0)
		return mroute_modify(RTM_NEWADDR, argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return mroute_modify(RTM_DELADDR, argc-1, argv+1);
	if (matches(*argv, "get") == 0)
		return mroute_get(argc-1, argv+1);
#endif
	if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
	    || matches(*argv, "lst") == 0)
		return mroute_list(argc-1, argv+1);
	if (matches(*argv, "help") == 0)
		usage();
	fprintf(stderr, "Command \"%s\" is unknown, try \"ip mroute help\".\n", *argv);
	exit(-1);
}
