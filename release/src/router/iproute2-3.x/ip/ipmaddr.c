/*
 * ipmaddr.c		"ip maddress".
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

#include "rt_names.h"
#include "utils.h"

static struct {
	char *dev;
	int  family;
} filter;

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr, "Usage: ip maddr [ add | del ] MULTIADDR dev STRING\n");
	fprintf(stderr, "       ip maddr show [ dev STRING ]\n");
	exit(-1);
}

static int parse_hex(char *str, unsigned char *addr, size_t size)
{
	int len=0;

	while (*str && (len < 2 * size)) {
		int tmp;
		if (str[1] == 0)
			return -1;
		if (sscanf(str, "%02x", &tmp) != 1)
			return -1;
		addr[len] = tmp;
		len++;
		str += 2;
	}
	return len;
}

struct ma_info
{
	struct ma_info *next;
	int		index;
	int		users;
	char		*features;
	char		name[IFNAMSIZ];
	inet_prefix	addr;
};

void maddr_ins(struct ma_info **lst, struct ma_info *m)
{
	struct ma_info *mp;

	for (; (mp=*lst) != NULL; lst = &mp->next) {
		if (mp->index > m->index)
			break;
	}
	m->next = *lst;
	*lst = m;
}

void read_dev_mcast(struct ma_info **result_p)
{
	char buf[256];
	FILE *fp = fopen("/proc/net/dev_mcast", "r");

	if (!fp)
		return;

	while (fgets(buf, sizeof(buf), fp)) {
		char hexa[256];
		struct ma_info m;
		int len;
		int st;

		memset(&m, 0, sizeof(m));
		sscanf(buf, "%d%s%d%d%s", &m.index, m.name, &m.users, &st,
		       hexa);
		if (filter.dev && strcmp(filter.dev, m.name))
			continue;

		m.addr.family = AF_PACKET;

		len = parse_hex(hexa, (unsigned char*)&m.addr.data, sizeof (m.addr.data));
		if (len >= 0) {
			struct ma_info *ma = malloc(sizeof(m));

			memcpy(ma, &m, sizeof(m));
			ma->addr.bytelen = len;
			ma->addr.bitlen = len<<3;
			if (st)
				ma->features = "static";
			maddr_ins(result_p, ma);
		}
	}
	fclose(fp);
}

void read_igmp(struct ma_info **result_p)
{
	struct ma_info m;
	char buf[256];
	FILE *fp = fopen("/proc/net/igmp", "r");

	if (!fp)
		return;
	memset(&m, 0, sizeof(m));
	if (!fgets(buf, sizeof(buf), fp)) {
		fclose(fp);
		return;
	}

	m.addr.family = AF_INET;
	m.addr.bitlen = 32;
	m.addr.bytelen = 4;

	while (fgets(buf, sizeof(buf), fp)) {
		struct ma_info *ma;

		if (buf[0] != '\t') {
			sscanf(buf, "%d%s", &m.index, m.name);
			continue;
		}

		if (filter.dev && strcmp(filter.dev, m.name))
			continue;

		sscanf(buf, "%08x%d", (__u32*)&m.addr.data, &m.users);

		ma = malloc(sizeof(m));
		memcpy(ma, &m, sizeof(m));
		maddr_ins(result_p, ma);
	}
	fclose(fp);
}


void read_igmp6(struct ma_info **result_p)
{
	char buf[256];
	FILE *fp = fopen("/proc/net/igmp6", "r");

	if (!fp)
		return;

	while (fgets(buf, sizeof(buf), fp)) {
		char hexa[256];
		struct ma_info m;
		int len;

		memset(&m, 0, sizeof(m));
		sscanf(buf, "%d%s%s%d", &m.index, m.name, hexa, &m.users);

		if (filter.dev && strcmp(filter.dev, m.name))
			continue;

		m.addr.family = AF_INET6;

		len = parse_hex(hexa, (unsigned char*)&m.addr.data, sizeof (m.addr.data));
		if (len >= 0) {
			struct ma_info *ma = malloc(sizeof(m));

			memcpy(ma, &m, sizeof(m));

			ma->addr.bytelen = len;
			ma->addr.bitlen = len<<3;
			maddr_ins(result_p, ma);
		}
	}
	fclose(fp);
}

static void print_maddr(FILE *fp, struct ma_info *list)
{
	fprintf(fp, "\t");

	if (list->addr.family == AF_PACKET) {
		SPRINT_BUF(b1);
		fprintf(fp, "link  %s", ll_addr_n2a((unsigned char*)list->addr.data,
						    list->addr.bytelen, 0,
						    b1, sizeof(b1)));
	} else {
		char abuf[256];
		switch(list->addr.family) {
		case AF_INET:
			fprintf(fp, "inet  ");
			break;
		case AF_INET6:
			fprintf(fp, "inet6 ");
			break;
		default:
			fprintf(fp, "family %d ", list->addr.family);
			break;
		}
		fprintf(fp, "%s",
			format_host(list->addr.family,
				    -1,
				    list->addr.data,
				    abuf, sizeof(abuf)));
	}
	if (list->users != 1)
		fprintf(fp, " users %d", list->users);
	if (list->features)
		fprintf(fp, " %s", list->features);
	fprintf(fp, "\n");
}

static void print_mlist(FILE *fp, struct ma_info *list)
{
	int cur_index = 0;

	for (; list; list = list->next) {
		if (oneline) {
			cur_index = list->index;
			fprintf(fp, "%d:\t%s%s", cur_index, list->name, _SL_);
		} else if (cur_index != list->index) {
			cur_index = list->index;
			fprintf(fp, "%d:\t%s\n", cur_index, list->name);
		}
		print_maddr(fp, list);
	}
}

static int multiaddr_list(int argc, char **argv)
{
	struct ma_info *list = NULL;

	if (!filter.family)
		filter.family = preferred_family;

	while (argc > 0) {
		if (1) {
			if (strcmp(*argv, "dev") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			if (filter.dev)
				duparg2("dev", *argv);
			filter.dev = *argv;
		}
		argv++; argc--;
	}

	if (!filter.family || filter.family == AF_PACKET)
		read_dev_mcast(&list);
	if (!filter.family || filter.family == AF_INET)
		read_igmp(&list);
	if (!filter.family || filter.family == AF_INET6)
		read_igmp6(&list);
	print_mlist(stdout, list);
	return 0;
}

int multiaddr_modify(int cmd, int argc, char **argv)
{
	struct ifreq ifr;
	int fd;

	memset(&ifr, 0, sizeof(ifr));

	if (cmd == RTM_NEWADDR)
		cmd = SIOCADDMULTI;
	else
		cmd = SIOCDELMULTI;

	while (argc > 0) {
		if (strcmp(*argv, "dev") == 0) {
			NEXT_ARG();
			if (ifr.ifr_name[0])
				duparg("dev", *argv);
			strncpy(ifr.ifr_name, *argv, IFNAMSIZ);
		} else {
			if (matches(*argv, "address") == 0) {
				NEXT_ARG();
			}
			if (matches(*argv, "help") == 0)
				usage();
			if (ifr.ifr_hwaddr.sa_data[0])
				duparg("address", *argv);
			if (ll_addr_a2n(ifr.ifr_hwaddr.sa_data,
					14, *argv) < 0) {
				fprintf(stderr, "Error: \"%s\" is not a legal ll address.\n", *argv);
				exit(1);
			}
		}
		argc--; argv++;
	}
	if (ifr.ifr_name[0] == 0) {
		fprintf(stderr, "Not enough information: \"dev\" is required.\n");
		exit(-1);
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("Cannot create socket");
		exit(1);
	}
	if (ioctl(fd, cmd, (char*)&ifr) != 0) {
		perror("ioctl");
		exit(1);
	}
	close(fd);

	exit(0);
}


int do_multiaddr(int argc, char **argv)
{
	if (argc < 1)
		return multiaddr_list(0, NULL);
	if (matches(*argv, "add") == 0)
		return multiaddr_modify(RTM_NEWADDR, argc-1, argv+1);
	if (matches(*argv, "delete") == 0)
		return multiaddr_modify(RTM_DELADDR, argc-1, argv+1);
	if (matches(*argv, "list") == 0 || matches(*argv, "show") == 0
	    || matches(*argv, "lst") == 0)
		return multiaddr_list(argc-1, argv+1);
	if (matches(*argv, "help") == 0)
		usage();
	fprintf(stderr, "Command \"%s\" is unknown, try \"ip maddr help\".\n", *argv);
	exit(-1);
}
