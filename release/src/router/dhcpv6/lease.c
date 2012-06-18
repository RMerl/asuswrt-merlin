#include <sys/types.h>
/*
 * Copyright (C) 1998 and 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/queue.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <netinet/in.h>
#include "dhcp6.h"
#include "config.h"
#include "common.h"

#ifndef FALSE
#define FALSE 	0
#define TRUE	!FALSE
#endif

struct hash_entry {
	LIST_ENTRY(hash_entry) list;
	char *val;
	char flag;	/* 0x01: DHCP6_LEASE_DECLINED */
};

/* marked as declined (e.g. someone has been using the same address) */
#define	DHCP6_LEASE_DECLINED	0x01	

LIST_HEAD(hash_head, hash_entry);

typedef unsigned int (*pfn_hash_t)(void *val) ;
typedef int (*pfh_hash_match_t)(void *val1, void *val2); 

struct hash_table {
	struct hash_head *table;
	unsigned int size;
	pfn_hash_t hash;
	pfh_hash_match_t match;
};

#ifndef DHCP6_LEASE_TABLE_SIZE
#define DHCP6_LEASE_TABLE_SIZE	256
#endif

static struct hash_table dhcp6_lease_table;

static unsigned int in6_addr_hash __P((void *));
static int in6_addr_match __P((void *, void *));

static int hash_table_init __P((struct hash_table *, unsigned int,
				pfn_hash_t, pfh_hash_match_t));
static void hash_table_cleanup __P((struct hash_table *));
static int hash_table_add __P((struct hash_table *, void *, unsigned int));
static int hash_table_remove __P((struct hash_table *, void *));
static struct hash_entry * hash_table_find __P((struct hash_table *, void *));

int
lease_init(void)
{
	dprintf(LOG_DEBUG, FNAME, "called");

	if (hash_table_init(&dhcp6_lease_table, DHCP6_LEASE_TABLE_SIZE,
		in6_addr_hash, in6_addr_match) != 0) {
		return (-1);
	}

	return (0);
}

void
lease_cleanup(void)
{
	hash_table_cleanup(&dhcp6_lease_table);
}

int
lease_address(addr)
	struct in6_addr *addr;
{
	if (!addr)
		return (FALSE);

	dprintf(LOG_DEBUG, FNAME, "addr=%s", in6addr2str(addr, 0));

	if (hash_table_find(&dhcp6_lease_table, addr)) {
		dprintf(LOG_WARNING, FNAME, "already leased: %s",
			in6addr2str(addr, 0));
		return (FALSE);
	}

	if (hash_table_add(&dhcp6_lease_table, addr, sizeof(*addr)) != 0) {
		return (FALSE);
	}

	return (TRUE);
}

void
release_address(addr)
	struct in6_addr *addr;
{
	if (!addr)
		return;

	dprintf(LOG_DEBUG, FNAME, "addr=%s", in6addr2str(addr, 0));

	if (hash_table_remove(&dhcp6_lease_table, addr) != 0) {
		dprintf(LOG_WARNING, FNAME, "not found: %s", in6addr2str(addr, 0));
	}
}

void
decline_address(addr)
	struct in6_addr *addr;
{
	struct hash_entry *entry;

	if (!addr)
		return;

	dprintf(LOG_DEBUG, FNAME, "addr=%s", in6addr2str(addr, 0));

	entry = hash_table_find(&dhcp6_lease_table, addr);
	if (entry == NULL) {
		dprintf(LOG_WARNING, FNAME, "not found: %s",
			in6addr2str(addr, 0));
		return;
	}

	entry->flag |= DHCP6_LEASE_DECLINED;
}

int
is_leased(addr)
	struct in6_addr *addr;
{
	return (hash_table_find(&dhcp6_lease_table, addr) != NULL);
}

static unsigned int
in6_addr_hash(val)
	void *val;
{
	u_int8_t *addr = ((struct in6_addr *)val)->s6_addr;
	unsigned int hash = 0;
	int i;

	for (i = 0; i < 16; i++) {
		hash += addr[i];
	}

	return (hash);
}

static int
in6_addr_match(val1, val2)
	void *val1, *val2;
{
	struct in6_addr * addr1 = val1;
	struct in6_addr * addr2 = val2;

	return (memcmp(addr1->s6_addr, addr2->s6_addr, 16) == 0);
}

/*
 * hash table
 */
static int
hash_table_init(table, size, hash, match)
	struct hash_table *table; 
	unsigned int size;
	pfn_hash_t hash;
	pfh_hash_match_t match;
{
	int i;

	if (!table || !hash || !match) {
		return (-1);
	}

	if ((table->table = malloc(sizeof(*table->table) * size)) == NULL) {
		return (-1);
	}

	for (i = 0; i < size; i++)
		LIST_INIT(&table->table[i]);

	table->size = size;
	table->hash = hash;
	table->match = match;

	return (0);
}

static void
hash_table_cleanup(table)
	struct hash_table *table; 
{
	int i;

	if (!table) {
		return;
	}

	for (i = 0; i < table->size; i++) {
		while (!LIST_EMPTY(&table->table[i])) {
			struct hash_entry *entry = LIST_FIRST(&table->table[i]);
			LIST_REMOVE(entry, list);
			if (entry->val)
				free(entry->val);
			free(entry);
		}
	}
	free(table->table);
	memset(table, 0, sizeof(*table));
}

static int
hash_table_add(table, val, size)
	struct hash_table *table; 
	void *val;
	unsigned int size;
{
	struct hash_entry *entry = NULL;
	int i = 0;

	if (!table || !val) {
		return (-1);
	}

	if ((entry = malloc(sizeof(*entry))) == NULL) {
		return (-1);
	}
	memset(entry, 0, sizeof(*entry));

	if ((entry->val = malloc(size)) == NULL) {
		return (-1);
	}
	memcpy(entry->val, val, size);

	i = table->hash(val) % table->size;
	LIST_INSERT_HEAD(&table->table[i], entry, list);

	return (0);
}

static int
hash_table_remove(table, val)
	struct hash_table *table; 
	void *val;
{
	struct hash_entry *entry;

	if (!table || !val) {
		return (-1);
	}

	if ((entry = hash_table_find(table, val)) == NULL) {
		return (-1);
	}

	LIST_REMOVE(entry, list);
	if (entry->val)
		free(entry->val);
	free(entry);

	return (0);
}

static struct hash_entry *
hash_table_find(table, val)
	struct hash_table *table; 
	void *val;
{
	struct hash_entry *entry;
	int i;

	if (!table || !val) {
		return (NULL);
	}

	i = table->hash(val) % table->size;
	LIST_FOREACH(entry, &table->table[i], list)
	{
		if (table->match(val, entry->val)) {
			return (entry);
		}
	}

	return (NULL);
}

