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
#include <string.h>
#include <netinet/ether.h>

#include "snooper.h"
#include "queue.h"

#ifdef DEBUG_SWITCH
#define log_switch(fmt, args...) log_debug("%s::" fmt, "switch", ##args)
#else
#define log_switch(...) do {} while (0)
#endif
#ifdef TEST
#define logger(level, fmt, args...) printf(fmt, ##args)
#endif

#define PORT_CPU PORT_MAX
static int cpu_portmap;
static int lan_portmap;
static int cpu_forward;

struct entry {
	TAILQ_ENTRY(entry) link;
	unsigned char ea[ETHER_ADDR_LEN];
	int ports;
};
static TAILQ_HEAD(, entry) entries = TAILQ_HEAD_INITIALIZER(entries);

static struct entry *get_entry(unsigned char *ea);
static struct entry *add_entry(unsigned char *ea, int ports);
static void del_entry(unsigned char *ea, int *ports);

static struct entry *get_entry(unsigned char *ea)
{
	static int port = 0;
	struct entry *entry;

	TAILQ_FOREACH(entry, &entries, link) {
		if (memcmp(ea, entry->ea, ETHER_ADDR_LEN) == 0)
			break;
	}
	if (!entry && (ea[0] & 1) == 0) {
		entry = calloc(1, sizeof(*entry));
		if (!entry)
			return NULL;
		memcpy(entry->ea, ea, ETHER_ADDR_LEN);
		entry->ports = port++ % 4;
		TAILQ_INSERT_TAIL(&entries, entry, link);
	}

	return entry;
}

static struct entry *add_entry(unsigned char *ea, int ports)
{
	struct entry *entry = get_entry(ea);

	if (!entry) {
		entry = calloc(1, sizeof(*entry));
		if (!entry)
			return NULL;
		memcpy(entry->ea, ea, ETHER_ADDR_LEN);
		TAILQ_INSERT_TAIL(&entries, entry, link);
	}
	entry->ports = ports;

	return entry;
}

static void del_entry(unsigned char *ea, int *ports)
{
	struct entry *entry = get_entry(ea);

	if (ports)
		*ports = entry ? entry->ports : -1;
	if (entry) {
		TAILQ_REMOVE(&entries, entry, link);
		free(entry);
	}
}

static void free_entries(void)
{
	struct entry *entry, *next;

	TAILQ_FOREACH_SAFE(entry, &entries, link, next) {
		TAILQ_REMOVE(&entries, entry, link);
		free(entry);
	}
}

int switch_init(char *ifname, int vid, int cputrap)
{
	lan_portmap = (1 << (PORT_MAX + 1)) - 1;
	log_switch("%-5s map@%s = " FMT_PORTS, "read",
	    ifname, ARG_PORTS(lan_portmap));

	cpu_portmap = 1 << PORT_CPU;
	log_switch("%-5s cpu@%s = " FMT_PORTS, "read",
	    ifname, ARG_PORTS(cpu_portmap));

	cpu_forward = cputrap;

	return add_entry(ifhwaddr, PORT_CPU) ? 0 : -1;
}

void switch_done(void)
{
	free_entries();
}

int switch_get_port(unsigned char *haddr)
{
	struct entry *entry = get_entry(haddr);
	int ports = entry ? entry->ports : -1;

	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "read",
	    ARG_EA(haddr), ARG_PORTS((ports < 0) ? -1 : 1 << ports));

	return ports;
}

int switch_add_portmap(unsigned char *maddr, int portmap)
{
	struct entry *entry = get_entry(maddr);
	int value, ports = entry ? entry->ports : 0;

	value = ports | portmap | cpu_portmap;
	if (value != ports) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "write",
		    ARG_EA(maddr), ARG_PORTS(value));
		add_entry(maddr, value);
	}

	return value;
}

int switch_del_portmap(unsigned char *maddr, int portmap)
{
	struct entry *entry = get_entry(maddr);
	int value, ports = entry ? entry->ports : 0;

	value = (ports & ~portmap) | cpu_portmap;
	if (value != ports) {
		log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "write",
		    ARG_EA(maddr), ARG_PORTS(value));
		add_entry(maddr, value);
	}

	return value;
}

int switch_clr_portmap(unsigned char *maddr)
{
	int ports;

	del_entry(maddr, &ports);
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "clear",
		ARG_EA(maddr), ARG_PORTS(ports));

	return ports;
}

int switch_set_floodmap(unsigned char *raddr, int portmap)
{
	if (!cpu_forward)
		return -1;

	portmap &= lan_portmap & ~cpu_portmap;
	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "flood",
	    ARG_EA(raddr), ARG_PORTS(portmap));

	return portmap;
}

int switch_clr_floodmap(unsigned char *raddr)
{
	if (!cpu_forward)
		return -1;

	log_switch("%-5s [" FMT_EA "] = " FMT_PORTS, "flood",
	    ARG_EA(raddr), ARG_PORTS(lan_portmap));

	return lan_portmap;
}

#ifdef TEST
int main(int argc, char *argv[])
{
	return 0;
}
#endif
