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
#include <net/if.h>

#include "libnetlink.h"
#include "ll_map.h"
#include "hlist.h"

struct ll_cache {
	struct hlist_node idx_hash;
	struct hlist_node name_hash;
	unsigned	flags;
	unsigned 	index;
	unsigned short	type;
	char		name[IFNAMSIZ];
};

#define IDXMAP_SIZE	1024
static struct hlist_head idx_head[IDXMAP_SIZE];
static struct hlist_head name_head[IDXMAP_SIZE];

static struct ll_cache *ll_get_by_index(unsigned index)
{
	struct hlist_node *n;
	unsigned h = index & (IDXMAP_SIZE - 1);

	hlist_for_each(n, &idx_head[h]) {
		struct ll_cache *im
			= container_of(n, struct ll_cache, idx_hash);
		if (im->index == index)
			return im;
	}

	return NULL;
}

static unsigned namehash(const char *str)
{
	unsigned hash = 5381;

	while (*str)
		hash = ((hash << 5) + hash) + *str++; /* hash * 33 + c */

	return hash;
}

static struct ll_cache *ll_get_by_name(const char *name)
{
	struct hlist_node *n;
	unsigned h = namehash(name) & (IDXMAP_SIZE - 1);

	hlist_for_each(n, &name_head[h]) {
		struct ll_cache *im
			= container_of(n, struct ll_cache, name_hash);

		if (strncmp(im->name, name, IFNAMSIZ) == 0)
			return im;
	}

	return NULL;
}

int ll_remember_index(const struct sockaddr_nl *who,
		      struct nlmsghdr *n, void *arg)
{
	unsigned int h;
	const char *ifname;
	struct ifinfomsg *ifi = NLMSG_DATA(n);
	struct ll_cache *im;
	struct rtattr *tb[IFLA_MAX+1];

	if (n->nlmsg_type != RTM_NEWLINK && n->nlmsg_type != RTM_DELLINK)
		return 0;

	if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifi)))
		return -1;

	im = ll_get_by_index(ifi->ifi_index);
	if (n->nlmsg_type == RTM_DELLINK) {
		if (im) {
			hlist_del(&im->name_hash);
			hlist_del(&im->idx_hash);
			free(im);
		}
		return 0;
	}

	memset(tb, 0, sizeof(tb));
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), IFLA_PAYLOAD(n));
	ifname = rta_getattr_str(tb[IFLA_IFNAME]);
	if (ifname == NULL)
		return 0;

	if (im) {
		/* change to existing entry */
		if (strcmp(im->name, ifname) != 0) {
			hlist_del(&im->name_hash);
			h = namehash(ifname) & (IDXMAP_SIZE - 1);
			hlist_add_head(&im->name_hash, &name_head[h]);
		}

		im->flags = ifi->ifi_flags;
		return 0;
	}

	im = malloc(sizeof(*im));
	if (im == NULL)
		return 0;
	im->index = ifi->ifi_index;
	strcpy(im->name, ifname);
	im->type = ifi->ifi_type;
	im->flags = ifi->ifi_flags;

	h = ifi->ifi_index & (IDXMAP_SIZE - 1);
	hlist_add_head(&im->idx_hash, &idx_head[h]);

	h = namehash(ifname) & (IDXMAP_SIZE - 1);
	hlist_add_head(&im->name_hash, &name_head[h]);

	return 0;
}

const char *ll_idx_n2a(unsigned idx, char *buf)
{
	const struct ll_cache *im;

	if (idx == 0)
		return "*";

	im = ll_get_by_index(idx);
	if (im)
		return im->name;

	if (if_indextoname(idx, buf) == NULL)
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

	im = ll_get_by_index(idx);
	return im ? im->type : -1;
}

int ll_index_to_flags(unsigned idx)
{
	const struct ll_cache *im;

	if (idx == 0)
		return 0;

	im = ll_get_by_index(idx);
	return im ? im->flags : -1;
}

unsigned ll_name_to_index(const char *name)
{
	const struct ll_cache *im;
	unsigned idx;

	if (name == NULL)
		return 0;

	im = ll_get_by_name(name);
	if (im)
		return im->index;

	idx = if_nametoindex(name);
	if (idx == 0)
		sscanf(name, "if%u", &idx);
	return idx;
}

void ll_init_map(struct rtnl_handle *rth)
{
	static int initialized;

	if (initialized)
		return;

	if (rtnl_wilddump_request(rth, AF_UNSPEC, RTM_GETLINK) < 0) {
		perror("Cannot send dump request");
		exit(1);
	}

	if (rtnl_dump_filter(rth, ll_remember_index, NULL) < 0) {
		fprintf(stderr, "Dump terminated\n");
		exit(1);
	}

	initialized = 1;
}
