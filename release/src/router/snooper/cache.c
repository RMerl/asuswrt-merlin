/*
 * Ethernet Switch IGMP Snooper
 * Copyright (C) 2014 ASUSTeK Inc.
 * All Rights Reserved.

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <netinet/ether.h>

#include "snooper.h"
#include "queue.h"

#ifdef DEBUG_CACHE
#define log_cache(fmt, args...) log_debug("%s::" fmt, "cache", ##args)
#else
#define log_cache(...) do {} while (0)
#endif

#define GROUP_POOL_SIZE 512
#define MEMBER_POOL_SIZE 1024
#define HOST_POOL_SIZE 32
#define HOST_TTL 3

#define HASH_SIZE 64
#define HASH_INDEX(ea) (ether_hash(ea) % HASH_SIZE)

#undef HOSTPOOL_STATIC
#undef GROUP_POOL_STATIC

struct host_entry {
	STAILQ_ENTRY(host_entry) link;
	LIST_ENTRY(host_entry) hash;
	unsigned long time;
	int port;
	unsigned char ea[ETHER_ADDR_LEN];
};
static struct {
	STAILQ_HEAD(, host_entry) pool;
	LIST_HEAD(, host_entry) hash[HASH_SIZE];
	int count;
#ifdef HOST_POOL_STATIC
	struct host_entry entries[HOST_POOL_SIZE];
#endif
} hosts;

struct member_entry {
	LIST_ENTRY(member_entry) link;
	unsigned long time;
	in_addr_t addr;
};
static struct {
	LIST_HEAD(, member_entry) free;
	int count;
} members;

static struct {
	int expire_support:1;
	int expire_active:1;
} ports;

struct group_entry {
	STAILQ_ENTRY(group_entry) link;
	LIST_ENTRY(group_entry) hash;
	LIST_HEAD(, member_entry) members[PORT_MAX + 1];
	unsigned long ptime[PORT_MAX + 1];
	unsigned long time;
	int portmap;
	struct timer_entry timer;
	unsigned char ea[ETHER_ADDR_LEN];
};
static struct {
	STAILQ_HEAD(, group_entry) pool;
	LIST_HEAD(, group_entry) hash[HASH_SIZE];
	int count;
#ifdef GROUP_POOL_STATIC
	struct group_entry entries[GROUP_POOL_SIZE];
#endif
} groups;
static struct group_entry routers;

static void group_timer(struct timer_entry *timer, void *data);
static void router_timer(struct timer_entry *timer, void *data);

static struct host_entry *get_host(unsigned char *ea, unsigned long time)
{
	struct host_entry *host, *prev;
	int index = HASH_INDEX(ea);

	LIST_FOREACH(host, &hosts.hash[index], hash) {
		if (memcmp(host->ea, ea, ETHER_ADDR_LEN) == 0)
			return host;
	}

	if (hosts.count < HOST_POOL_SIZE) {
#ifdef HOST_POOL_STATIC
		host = &hosts.entries[hosts.count++];
#else
		host = calloc(1, sizeof(*host));
		if (!host)
			hosts.count++;
#endif
	}
	if (!host) {
		prev = NULL;
		STAILQ_FOREACH(host, &hosts.pool, link) {
			if (time_before(host->time, time))
				break;
			prev = host;
		}
		if (!host)
			return NULL;
		LIST_REMOVE(host, hash);
		if (prev)
			STAILQ_REMOVE_AFTER(&hosts.pool, prev, link);
		else	STAILQ_REMOVE_HEAD(&hosts.pool, link);
		memset(&host, 0, sizeof(*host));
	}

	memcpy(host->ea, ea, ETHER_ADDR_LEN);
	LIST_INSERT_HEAD(&hosts.hash[index], host, hash);
	STAILQ_INSERT_TAIL(&hosts.pool, host, link);

	return host;
}

int get_port(unsigned char *haddr)
{
	struct host_entry *host;
	unsigned long time = now();
	int port;

	host = get_host(haddr, time);
	if (host && time_after_eq(host->time, time)) {
		log_cache("%-6s [" FMT_EA "] = " FMT_PORTS, "port",
		    ARG_EA(haddr), ARG_PORTS((host->port < 0) ? -1 : 1 << host->port));
		return host->port;
	}

	port = switch_get_port(haddr);
	log_cache("%-6s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(haddr), ARG_PORTS((port < 0) ? -1 : 1 << port));

	if (host && 0 <= port && port <= PORT_MAX) {
		host->port = port;
		host->time = time + HOST_TTL * TIMER_HZ;
	}

	return port;
}

static struct member_entry *get_member(struct group_entry *group, in_addr_t addr, int port, int allocate)
{
	struct member_entry *member;
	int i;

	LIST_FOREACH(member, &group->members[port], link) {
		if (member->addr == addr)
			return member;
	}
	if (!allocate)
		return NULL;

	if (allocate == 2) {
		for (i = 0; i <= PORT_MAX; i++) {
			if (i == port)
				continue;
			LIST_FOREACH(member, &group->members[i], link) {
				if (member->addr == addr) {
					LIST_REMOVE(member, link);
					goto relink;
				}
			}
		}
	}

	member = LIST_FIRST(&members.free);
	if (member) {
		LIST_REMOVE(member, link);
		memset(member, 0, sizeof(*member));
	} else
	if (members.count < MEMBER_POOL_SIZE) {
		member = calloc(1, sizeof(*member));
		if (member)
			members.count++;
	}
	if (!member)
		return NULL;

relink:
//	member->time = now();
	member->addr = addr;
	LIST_INSERT_HEAD(&group->members[port], member, link);

	return member;
}

static void consume_member(struct member_entry *member)
{
	LIST_REMOVE(member, link);
	LIST_INSERT_HEAD(&members.free, member, link);
}

static void init_group(struct group_entry *group)
{
	int port;

//	group->time = now();
	group->portmap = 0;
	for (port = 0; port <= PORT_MAX; port++)
		LIST_INIT(&group->members[port]);
}

static struct group_entry *get_group(unsigned char *ea, int allocate)
{
	struct group_entry *group, *prev;
	int index = HASH_INDEX(ea);

	LIST_FOREACH(group, &groups.hash[index], hash) {
		if (memcmp(group->ea, ea, ETHER_ADDR_LEN) == 0)
			return group;
	}
	if (!allocate)
		return NULL;

	if (groups.count < GROUP_POOL_SIZE) {
#ifdef GROUP_POOL_STATIC
		group = &groups.entries[groups.count++];
#else
		group = calloc(1, sizeof(*group));
		if (group)
			groups.count++;
#endif
	}
	if (!group) {
		prev = NULL;
		STAILQ_FOREACH(group, &groups.pool, link) {
			if (group->portmap == 0)
				break;
			prev = group;
		}
		if (!group)
			return NULL;
		LIST_REMOVE(group, hash);
		if (prev)
			STAILQ_REMOVE_AFTER(&groups.pool, prev, link);
		else 	STAILQ_REMOVE_HEAD(&groups.pool, link);
		switch_clr_portmap(group->ea);
		memset(&group, 0, sizeof(group));
	}

	init_group(group);
	set_timer(&group->timer, group_timer, group);
	memcpy(group->ea, ea, ETHER_ADDR_LEN);
	LIST_INSERT_HEAD(&groups.hash[index], group, hash);
	STAILQ_INSERT_TAIL(&groups.pool, group, link);

	return group;
}

static int isempty_port(struct group_entry *group, int port)
{
	return LIST_EMPTY(&group->members[port]);
}

static void consume_port(struct group_entry *group, int port)
{
	struct member_entry *member, *next;

	group->portmap &= ~(1 << port);
	LIST_FOREACH_SAFE(member, &group->members[port], link, next)
		consume_member(member);
}

static void consume_group(struct group_entry *group)
{
	int port;

	del_timer(&group->timer);
	group->portmap = 0;
	for (port = 0; port <= PORT_MAX; port++)
		consume_port(group, port);
}

static int get_portmap(struct group_entry *group)
{
	int port, portmap = 0;

	for (port = 0; port <= PORT_MAX; port++) {
		if (isempty_port(group, port))
			continue;
		portmap |= 1 << port;
	}

	return portmap;
}

int init_cache(void)
{
	int index;

	memset(&hosts, 0, sizeof(hosts));
	memset(&members, 0, sizeof(members));
	memset(&ports, 0, sizeof(ports));
	memset(&groups, 0, sizeof(groups));
	memset(&routers, 0, sizeof(routers));

	STAILQ_INIT(&hosts.pool);
	LIST_INIT(&members.free);
	STAILQ_INIT(&groups.pool);
	for (index = 0; index < HASH_SIZE; index++) {
		LIST_INIT(&groups.hash[index]);
		LIST_INIT(&hosts.hash[index]);
	}

	init_group(&routers);
	set_timer(&routers.timer, router_timer, &routers);

	log_cache("%-6s pool(%u x hash) = %u, entries(%u x %u) = %u", "groups",
	    HASH_SIZE, sizeof(groups), GROUP_POOL_SIZE, sizeof(struct group_entry),
#ifdef HOST_POOL_STATIC
	    0 *
#endif
	    GROUP_POOL_SIZE * sizeof(struct group_entry));
	log_cache("%-6s pool = %u", "ports",
	    sizeof(ports));
	log_cache("%-6s pool = %u, entries(%u x %u) = %u", "member",
	    sizeof(members), MEMBER_POOL_SIZE, sizeof(struct member_entry),
	    MEMBER_POOL_SIZE * sizeof(struct member_entry));
	log_cache("%-6s pool(%u x hash) = %u, entries(%u x %u) = %u", "hosts",
	    HASH_SIZE, sizeof(hosts), HOST_POOL_SIZE, sizeof(struct host_entry),
#ifdef GROUP_POOL_STATIC
	    0 *
#endif
	    HOST_POOL_SIZE * sizeof(struct host_entry));

	if (switch_set_floodmap(routers.ea, 0) == 0)
		ports.expire_support = ports.expire_active = 1;

	return 0;
}

static void group_timer(struct timer_entry *timer, void *data)
{
	struct group_entry *group = data;
	unsigned long expires, time = now();
	int port, portmap;

	portmap = group->portmap;
	if (portmap == 0)
		return;

	if (time_after(group->time, time)) {
		expires = time + ~0UL/2;
		group->portmap = get_portmap(group);
		if (ports.expire_support && ports.expire_active) {
			for (port = 0; port <= PORT_MAX; port++) {
				if (isempty_port(group, port))
					continue;
				if (time_after(group->ptime[port], time)) {
					if (time_after(group->ptime[port], group->time))
						group->ptime[port] = group->time;
					else if (time_before(group->ptime[port], expires))
						expires = group->ptime[port];
					continue;
				} else
					consume_port(group, port);
			}
			portmap = (portmap ^ group->portmap) & portmap;
		} else
			portmap = (portmap ^ group->portmap) & portmap;
		if (group->portmap) {
			if (time_before(group->time, expires))
				expires = group->time;
			mod_timer(timer, expires);
		} else
			consume_group(group);
	} else
		consume_group(group);

	log_cache("%-6s [" FMT_EA "] - " FMT_PORTS, "expire",
	    ARG_EA(group->ea), ARG_PORTS(portmap));

	portmap &= ~routers.portmap;

	if (portmap)
		switch_del_portmap(group->ea, portmap);
}

int add_member(unsigned char *maddr, in_addr_t addr, int port, int timeout)
{
	struct group_entry *group;
	struct member_entry *member;
	struct timer_entry *timer;
	int i, portmap;

	if (port < 0 || port > PORT_MAX)
		return -1;

	group = get_group(maddr, 1);
	if (group) {
		portmap = group->portmap;

		group->time = now() + timeout;
		member = get_member(group, addr, port, 2);
		if (member) {
			group->ptime[port] = member->time = group->time;
			if (ports.expire_support && !ports.expire_active) {
				for (i = 0; i <= PORT_MAX; i++) {
					if (i == port || isempty_port(group, i))
						continue;
					group->ptime[i] = group->time;
				}
			}
		}
		group->portmap |= get_portmap(group);
		portmap = (portmap ^ group->portmap) & group->portmap;

		timer = &group->timer;
		if (!timer_pending(timer) || time_before(group->time, timer->expires))
			mod_timer(timer, group->time);

		log_cache("%-6s [" FMT_EA "] + " FMT_PORTS " add " FMT_IP " timeout %d", "member",
		    ARG_EA(group->ea), ARG_PORTS(portmap), ARG_IP(&addr), timeout / TIMER_HZ);
	} else
		portmap = 0;

	if (portmap)
		switch_add_portmap(maddr, portmap | routers.portmap);

	return portmap;
}

int del_member(unsigned char *maddr, in_addr_t addr, int port, int timeout)
{
	struct group_entry *group;
	struct member_entry *member;
	struct timer_entry *timer;
	unsigned long time;
	int portmap;

	if (port < 0 || port > PORT_MAX)
		return -1;

	group = get_group(maddr, 0);
	if (group) {
		portmap = group->portmap;

		member = get_member(group, addr, port, 0);
		if (member)
			consume_member(member);
		if (timeout) {
			portmap = (portmap ^ get_portmap(group)) & portmap;
			if (portmap) {
				timer = &group->timer;
				time = now() + timeout;
				if (!timer_pending(timer) || time_after(timer->expires, time))
					mod_timer(timer, time);
			}
		} else {
			group->portmap = get_portmap(group);
			portmap = (portmap ^ group->portmap) & portmap;
			if (portmap && group->portmap == 0)
				consume_group(group);
		}

		log_cache("%-6s [" FMT_EA "] - " FMT_PORTS " del " FMT_IP, "member",
		    ARG_EA(group->ea), ARG_PORTS(portmap), ARG_IP(&addr));
	} else
		portmap = 0;

	portmap &= ~routers.portmap;
	if (portmap && timeout == 0)
		switch_del_portmap(maddr, portmap);

	return portmap;
}

static void router_timer(struct timer_entry *timer, void *data)
{
	struct group_entry *group = data;
	struct member_entry *member, *next;
	unsigned long time = now();
	int port, portmap, groupmap;

	portmap = group->portmap;
	if (portmap < 0)
		return;

	if (time_after(group->time, time)) {
		group->time = time + ~0UL/2;
		for (port = 0; port <= PORT_MAX; port++) {
			LIST_FOREACH_SAFE(member, &group->members[port], link, next) {
				if (time_after(member->time, time)) {
					if (time_before(member->time, group->time))
						group->time = member->time;
					continue;
				} else
					consume_member(member);
			}
		}
		group->portmap = get_portmap(group);
		portmap = (portmap ^ group->portmap) & portmap;
		if (group->portmap)
			mod_timer(timer, group->time);
		else
			consume_group(group);
	} else
		consume_group(group);

	log_cache("%-6s [" FMT_EA "] - " FMT_PORTS, "expire",
	    ARG_EA(group->ea), ARG_PORTS(portmap));

	if (portmap) {
		if (ports.expire_support) {
			groupmap = switch_set_floodmap(group->ea, group->portmap);
			if (groupmap >= 0)
				ports.expire_active = (groupmap == 0);
		}
		STAILQ_FOREACH(group, &groups.pool, link) {
			groupmap = portmap & ~group->portmap;
			if (groupmap)
				switch_del_portmap(group->ea, groupmap);
		}
	}
}

int add_router(in_addr_t addr, int port, int timeout)
{
	struct group_entry *group;
	struct member_entry *member;
	struct timer_entry *timer;
	int portmap, groupmap;

	if (port < 0 || port > PORT_MAX)
		return -1;

	group = &routers;
	if (group) {
		portmap = group->portmap;

		group->time = now() + timeout;
		member = get_member(group, addr, port, 1);
		if (member)
			group->ptime[port] = member->time = group->time;
		group->portmap = get_portmap(group);
		portmap = (portmap ^ group->portmap) & group->portmap;

		timer = &routers.timer;
		if (!timer_pending(timer) || time_after(timer->expires, group->time))
			mod_timer(timer, group->time);

		log_cache("%-6s [" FMT_EA "] + " FMT_PORTS " add " FMT_IP " timeout %d", "router",
		    ARG_EA(group->ea), ARG_PORTS(portmap), ARG_IP(&addr), timeout / TIMER_HZ);
	} else
		portmap = 0;

	if (portmap) {
		if (ports.expire_support) {
			groupmap = switch_set_floodmap(group->ea, group->portmap);
			if (groupmap >= 0)
				ports.expire_active = (groupmap == 0);
		}
		STAILQ_FOREACH(group, &groups.pool, link) {
			groupmap = portmap & ~group->portmap;
			if (groupmap)
				switch_add_portmap(group->ea, groupmap);
		}
	}

	return portmap;
}

int expire_members(unsigned char *maddr, int timeout)
{
	struct group_entry *group;
	unsigned long time = now() + timeout;

	if (maddr) {
		group = get_group(maddr, 0);
		if (!group)
			return -1;
		group->time = time;
		if (!timer_pending(&group->timer) || time_after(group->timer.expires, time))
			mod_timer(&group->timer, time);

		log_cache("%-6s [" FMT_EA "] = " FMT_PORTS " set timeout %d", "expire",
		    ARG_EA(group->ea), ARG_PORTS(group->portmap), timeout / TIMER_HZ);
	} else
	STAILQ_FOREACH(group, &groups.pool, link) {
		group->time = time;
		if (!timer_pending(&group->timer) || time_after(group->timer.expires, time))
			mod_timer(&group->timer, time);

		log_cache("%-6s [" FMT_EA "] = " FMT_PORTS " set timeout %d", "expire",
		    ARG_EA(group->ea), ARG_PORTS(group->portmap), timeout / TIMER_HZ);
	}

	return 0;
}

int purge_cache(void)
{
	struct group_entry *group;
	struct member_entry *member, *next_member;
	struct host_entry *host;

	while ((group = STAILQ_FIRST(&groups.pool))) {
		consume_group(group);
		LIST_REMOVE(group, hash);
		STAILQ_REMOVE_HEAD(&groups.pool, link);
		switch_clr_portmap(group->ea);
#ifndef GROUP_POOL_STATIC
		free(group);
#endif
	}
	consume_group(&routers);

	if (ports.expire_support)
		switch_clr_floodmap(routers.ea);

	LIST_FOREACH_SAFE(member, &members.free, link, next_member) {
		LIST_REMOVE(member, link);
		free(member);
	}

	while ((host = STAILQ_FIRST(&hosts.pool))) {
		STAILQ_REMOVE_HEAD(&hosts.pool, link);
		LIST_REMOVE(host, hash);
#ifndef HOST_POOL_STATIC
		free(host);
#endif
	}

	return 0;
}
