/*
 * rt_names.c		rtnetlink names DB.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <asm/types.h>
#include <linux/rtnetlink.h>

#include "rt_names.h"

struct rtnl_hash_entry {
	struct rtnl_hash_entry *next;
	char *			name;
	unsigned int		id;
};

static void
rtnl_hash_initialize(char *file, struct rtnl_hash_entry **hash, int size)
{
	struct rtnl_hash_entry *entry;
	char buf[512];
	FILE *fp;

	fp = fopen(file, "r");
	if (!fp)
		return;
	while (fgets(buf, sizeof(buf), fp)) {
		char *p = buf;
		int id;
		char namebuf[512];

		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '#' || *p == '\n' || *p == 0)
			continue;
		if (sscanf(p, "0x%x %s\n", &id, namebuf) != 2 &&
		    sscanf(p, "0x%x %s #", &id, namebuf) != 2 &&
		    sscanf(p, "%d %s\n", &id, namebuf) != 2 &&
		    sscanf(p, "%d %s #", &id, namebuf) != 2) {
			fprintf(stderr, "Database %s is corrupted at %s\n",
				file, p);
			fclose(fp);
			return;
		}

		if (id<0)
			continue;
		entry = malloc(sizeof(*entry));
		entry->id   = id;
		entry->name = strdup(namebuf);
		entry->next = hash[id & (size - 1)];
		hash[id & (size - 1)] = entry;
	}
	fclose(fp);
}

static void rtnl_tab_initialize(char *file, char **tab, int size)
{
	char buf[512];
	FILE *fp;

	fp = fopen(file, "r");
	if (!fp)
		return;
	while (fgets(buf, sizeof(buf), fp)) {
		char *p = buf;
		int id;
		char namebuf[512];

		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '#' || *p == '\n' || *p == 0)
			continue;
		if (sscanf(p, "0x%x %s\n", &id, namebuf) != 2 &&
		    sscanf(p, "0x%x %s #", &id, namebuf) != 2 &&
		    sscanf(p, "%d %s\n", &id, namebuf) != 2 &&
		    sscanf(p, "%d %s #", &id, namebuf) != 2) {
			fprintf(stderr, "Database %s is corrupted at %s\n",
				file, p);
			fclose(fp);
			return;
		}

		if (id<0 || id>size)
			continue;

		tab[id] = strdup(namebuf);
	}
	fclose(fp);
}

static char * rtnl_rtprot_tab[256] = {
	[RTPROT_UNSPEC] = "none",
	[RTPROT_REDIRECT] ="redirect",
	[RTPROT_KERNEL] = "kernel",
	[RTPROT_BOOT] = "boot",
	[RTPROT_STATIC] = "static",

	[RTPROT_GATED] = "gated",
	[RTPROT_RA] = "ra",
	[RTPROT_MRT] =	"mrt",
	[RTPROT_ZEBRA] ="zebra",
	[RTPROT_BIRD] = "bird",
	[RTPROT_DNROUTED] = "dnrouted",
	[RTPROT_XORP] = "xorp",
	[RTPROT_NTK] = "ntk",
	[RTPROT_DHCP] = "dhcp",
};



static int rtnl_rtprot_init;

static void rtnl_rtprot_initialize(void)
{
	rtnl_rtprot_init = 1;
	rtnl_tab_initialize("/etc/iproute2/rt_protos",
			    rtnl_rtprot_tab, 256);
}

char * rtnl_rtprot_n2a(int id, char *buf, int len)
{
	if (id<0 || id>=256) {
		snprintf(buf, len, "%d", id);
		return buf;
	}
	if (!rtnl_rtprot_tab[id]) {
		if (!rtnl_rtprot_init)
			rtnl_rtprot_initialize();
	}
	if (rtnl_rtprot_tab[id])
		return rtnl_rtprot_tab[id];
	snprintf(buf, len, "%d", id);
	return buf;
}

int rtnl_rtprot_a2n(__u32 *id, char *arg)
{
	static char *cache = NULL;
	static unsigned long res;
	char *end;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	if (!rtnl_rtprot_init)
		rtnl_rtprot_initialize();

	for (i=0; i<256; i++) {
		if (rtnl_rtprot_tab[i] &&
		    strcmp(rtnl_rtprot_tab[i], arg) == 0) {
			cache = rtnl_rtprot_tab[i];
			res = i;
			*id = res;
			return 0;
		}
	}

	res = strtoul(arg, &end, 0);
	if (!end || end == arg || *end || res > 255)
		return -1;
	*id = res;
	return 0;
}



static char * rtnl_rtscope_tab[256] = {
	"global",
};

static int rtnl_rtscope_init;

static void rtnl_rtscope_initialize(void)
{
	rtnl_rtscope_init = 1;
	rtnl_rtscope_tab[255] = "nowhere";
	rtnl_rtscope_tab[254] = "host";
	rtnl_rtscope_tab[253] = "link";
	rtnl_rtscope_tab[200] = "site";
	rtnl_tab_initialize("/etc/iproute2/rt_scopes",
			    rtnl_rtscope_tab, 256);
}

char * rtnl_rtscope_n2a(int id, char *buf, int len)
{
	if (id<0 || id>=256) {
		snprintf(buf, len, "%d", id);
		return buf;
	}
	if (!rtnl_rtscope_tab[id]) {
		if (!rtnl_rtscope_init)
			rtnl_rtscope_initialize();
	}
	if (rtnl_rtscope_tab[id])
		return rtnl_rtscope_tab[id];
	snprintf(buf, len, "%d", id);
	return buf;
}

int rtnl_rtscope_a2n(__u32 *id, char *arg)
{
	static char *cache = NULL;
	static unsigned long res;
	char *end;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	if (!rtnl_rtscope_init)
		rtnl_rtscope_initialize();

	for (i=0; i<256; i++) {
		if (rtnl_rtscope_tab[i] &&
		    strcmp(rtnl_rtscope_tab[i], arg) == 0) {
			cache = rtnl_rtscope_tab[i];
			res = i;
			*id = res;
			return 0;
		}
	}

	res = strtoul(arg, &end, 0);
	if (!end || end == arg || *end || res > 255)
		return -1;
	*id = res;
	return 0;
}



static char * rtnl_rtrealm_tab[256] = {
	"unknown",
};

static int rtnl_rtrealm_init;

static void rtnl_rtrealm_initialize(void)
{
	rtnl_rtrealm_init = 1;
	rtnl_tab_initialize("/etc/iproute2/rt_realms",
			    rtnl_rtrealm_tab, 256);
}

char * rtnl_rtrealm_n2a(int id, char *buf, int len)
{
	if (id<0 || id>=256) {
		snprintf(buf, len, "%d", id);
		return buf;
	}
	if (!rtnl_rtrealm_tab[id]) {
		if (!rtnl_rtrealm_init)
			rtnl_rtrealm_initialize();
	}
	if (rtnl_rtrealm_tab[id])
		return rtnl_rtrealm_tab[id];
	snprintf(buf, len, "%d", id);
	return buf;
}


int rtnl_rtrealm_a2n(__u32 *id, char *arg)
{
	static char *cache = NULL;
	static unsigned long res;
	char *end;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	if (!rtnl_rtrealm_init)
		rtnl_rtrealm_initialize();

	for (i=0; i<256; i++) {
		if (rtnl_rtrealm_tab[i] &&
		    strcmp(rtnl_rtrealm_tab[i], arg) == 0) {
			cache = rtnl_rtrealm_tab[i];
			res = i;
			*id = res;
			return 0;
		}
	}

	res = strtoul(arg, &end, 0);
	if (!end || end == arg || *end || res > 255)
		return -1;
	*id = res;
	return 0;
}


static struct rtnl_hash_entry dflt_table_entry  = { .id = 253, .name = "default" };
static struct rtnl_hash_entry main_table_entry  = { .id = 254, .name = "main" };
static struct rtnl_hash_entry local_table_entry = { .id = 255, .name = "local" };

static struct rtnl_hash_entry * rtnl_rttable_hash[256] = {
	[253] = &dflt_table_entry,
	[254] = &main_table_entry,
	[255] = &local_table_entry,
};

static int rtnl_rttable_init;

static void rtnl_rttable_initialize(void)
{
	rtnl_rttable_init = 1;
	rtnl_hash_initialize("/etc/iproute2/rt_tables",
			     rtnl_rttable_hash, 256);
}

char * rtnl_rttable_n2a(__u32 id, char *buf, int len)
{
	struct rtnl_hash_entry *entry;

	if (id > RT_TABLE_MAX) {
		snprintf(buf, len, "%u", id);
		return buf;
	}
	if (!rtnl_rttable_init)
		rtnl_rttable_initialize();
	entry = rtnl_rttable_hash[id & 255];
	while (entry && entry->id != id)
		entry = entry->next;
	if (entry)
		return entry->name;
	snprintf(buf, len, "%u", id);
	return buf;
}

int rtnl_rttable_a2n(__u32 *id, char *arg)
{
	static char *cache = NULL;
	static unsigned long res;
	struct rtnl_hash_entry *entry;
	char *end;
	__u32 i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	if (!rtnl_rttable_init)
		rtnl_rttable_initialize();

	for (i=0; i<256; i++) {
		entry = rtnl_rttable_hash[i];
		while (entry && strcmp(entry->name, arg))
			entry = entry->next;
		if (entry) {
			cache = entry->name;
			res = entry->id;
			*id = res;
			return 0;
		}
	}

	i = strtoul(arg, &end, 0);
	if (!end || end == arg || *end || i > RT_TABLE_MAX)
		return -1;
	*id = i;
	return 0;
}


static char * rtnl_rtdsfield_tab[256] = {
	"0",
};

static int rtnl_rtdsfield_init;

static void rtnl_rtdsfield_initialize(void)
{
	rtnl_rtdsfield_init = 1;
	rtnl_tab_initialize("/etc/iproute2/rt_dsfield",
			    rtnl_rtdsfield_tab, 256);
}

char * rtnl_dsfield_n2a(int id, char *buf, int len)
{
	if (id<0 || id>=256) {
		snprintf(buf, len, "%d", id);
		return buf;
	}
	if (!rtnl_rtdsfield_tab[id]) {
		if (!rtnl_rtdsfield_init)
			rtnl_rtdsfield_initialize();
	}
	if (rtnl_rtdsfield_tab[id])
		return rtnl_rtdsfield_tab[id];
	snprintf(buf, len, "0x%02x", id);
	return buf;
}


int rtnl_dsfield_a2n(__u32 *id, char *arg)
{
	static char *cache = NULL;
	static unsigned long res;
	char *end;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	if (!rtnl_rtdsfield_init)
		rtnl_rtdsfield_initialize();

	for (i=0; i<256; i++) {
		if (rtnl_rtdsfield_tab[i] &&
		    strcmp(rtnl_rtdsfield_tab[i], arg) == 0) {
			cache = rtnl_rtdsfield_tab[i];
			res = i;
			*id = res;
			return 0;
		}
	}

	res = strtoul(arg, &end, 16);
	if (!end || end == arg || *end || res > 255)
		return -1;
	*id = res;
	return 0;
}


static struct rtnl_hash_entry dflt_group_entry  = { .id = 0, .name = "default" };

static struct rtnl_hash_entry * rtnl_group_hash[256] = {
	[0] = &dflt_group_entry,
};

static int rtnl_group_init;

static void rtnl_group_initialize(void)
{
	rtnl_group_init = 1;
	rtnl_hash_initialize("/etc/iproute2/group",
			     rtnl_group_hash, 256);
}

int rtnl_group_a2n(int *id, char *arg)
{
	static char *cache = NULL;
	static unsigned long res;
	struct rtnl_hash_entry *entry;
	char *end;
	int i;

	if (cache && strcmp(cache, arg) == 0) {
		*id = res;
		return 0;
	}

	if (!rtnl_group_init)
		rtnl_group_initialize();

	for (i=0; i<256; i++) {
		entry = rtnl_group_hash[i];
		while (entry && strcmp(entry->name, arg))
			entry = entry->next;
		if (entry) {
			cache = entry->name;
			res = entry->id;
			*id = res;
			return 0;
		}
	}

	i = strtol(arg, &end, 0);
	if (!end || end == arg || *end || i < 0)
		return -1;
	*id = i;
	return 0;
}
