/*
 * ll_map.c
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <linux/if.h>

#include "libnetlink.h"
#include "ll_map.h"

extern unsigned int if_nametoindex (const char *);

struct ll_cache
{
	struct ll_cache   *idx_next;
	unsigned	flags;
	int		index;
	unsigned short	type;
	unsigned short	alen;
	char		name[IFNAMSIZ];
	unsigned char	addr[20];
};

#define IDXMAP_SIZE	1024
static struct ll_cache *idx_head[IDXMAP_SIZE];

static inline struct ll_cache *idxhead(int idx)
{
	return idx_head[idx & (IDXMAP_SIZE - 1)];
}

int ll_remember_index(const struct sockaddr_nl *who,
		      struct nlmsghdr *n, void *arg)
{
	int h;
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct ll_cache *im, **imp;
	struct rtattr *tb[IFLA_MAX+1];

	if (n->nlmsg_type != RTM_NEWLINK)
		return 0;

	if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifi)))
		return -1;

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), IFLA_PAYLOAD(n));
	if (tb[IFLA_IFNAME] == NULL)
		return 0;

	h = ifi->ifi_index & (IDXMAP_SIZE - 1);
	for (imp = &idx_head[h]; (im=*imp)!=NULL; imp = &im->idx_next)
		if (im->index == ifi->ifi_index)
			break;

	if (im == NULL) {
		im = malloc(sizeof(*im));
		if (im == NULL)
			return 0;
		im->idx_next = *imp;
		im->index = ifi->ifi_index;
		*imp = im;
	}

	im->type = ifi->ifi_type;
	im->flags = ifi->ifi_flags;
	if (tb[IFLA_ADDRESS]) {
		int alen;
		im->alen = alen = RTA_PAYLOAD(tb[IFLA_ADDRESS]);
		if (alen > sizeof(im->addr))
			alen = sizeof(im->addr);
		memcpy(im->addr, RTA_DATA(tb[IFLA_ADDRESS]), alen);
	} else {
		im->alen = 0;
		memset(im->addr, 0, sizeof(im->addr));
	}
	strcpy(im->name, RTA_DATA(tb[IFLA_IFNAME]));
	return 0;
}

const char *ll_idx_n2a(unsigned idx, char *buf)
{
	const struct ll_cache *im;

	if (idx == 0)
		return "*";

	for (im = idxhead(idx); im; im = im->idx_next)
		if (im->index == idx)
			return im->name;

	snprintf(buf, IFNAMSIZ, "if%d", idx);
	return buf;
}


const char *ll_index_to_name(unsigned idx)
{
	static char nbuf[IFNAMSIZ];

	return ll_idx_n2a(idx, nbuf);
}

int ll_index_to_type(unsigned idx)
{
	const struct ll_cache *im;

	if (idx == 0)
		return -1;
	for (im = idxhead(idx); im; im = im->idx_next)
		if (im->index == idx)
			return im->type;
	return -1;
}

unsigned ll_index_to_flags(unsigned idx)
{
	const struct ll_cache *im;

	if (idx == 0)
		return 0;

	for (im = idxhead(idx); im; im = im->idx_next)
		if (im->index == idx)
			return im->flags;
	return 0;
}

unsigned ll_index_to_addr(unsigned idx, unsigned char *addr,
			  unsigned alen)
{
	const struct ll_cache *im;

	if (idx == 0)
		return 0;

	for (im = idxhead(idx); im; im = im->idx_next) {
		if (im->index == idx) {
			if (alen > sizeof(im->addr))
				alen = sizeof(im->addr);
			if (alen > im->alen)
				alen = im->alen;
			memcpy(addr, im->addr, alen);
			return alen;
		}
	}
	return 0;
}

unsigned ll_name_to_index(const char *name)
{
	static char ncache[IFNAMSIZ];
	static int icache;
	struct ll_cache *im;
	int i;
	unsigned idx;

	if (name == NULL)
		return 0;

	if (icache && strcmp(name, ncache) == 0)
		return icache;

	for (i=0; i<IDXMAP_SIZE; i++) {
		for (im = idx_head[i]; im; im = im->idx_next) {
			if (strcmp(im->name, name) == 0) {
				icache = im->index;
				strcpy(ncache, name);
				return im->index;
			}
		}
	}

	idx = if_nametoindex(name);
	if (idx == 0)
		sscanf(name, "if%u", &idx);
	return idx;
}

int ll_init_map(struct rtnl_handle *rth)
{
	static int initialized;

	if (initialized)
		return 0;

	if (rtnl_wilddump_request(rth, AF_UNSPEC, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	if (rtnl_dump_filter(rth, ll_remember_index, NULL, NULL, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	initialized = 1;

	return 0;
}
