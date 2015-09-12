/*
 * NVRAM variable manipulation (common)
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: nvram.c 241182 2011-02-17 21:50:03Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <bcmnvram.h>
#include <sbsdram.h>

extern struct nvram_tuple *_nvram_realloc(struct nvram_tuple *t, const char *name,
                                          const char *value);
extern void _nvram_free(struct nvram_tuple *t);
extern int _nvram_read(void *buf, int idx);

char *_nvram_get(const char *name);
int _nvram_set(const char *name, const char *value);
int _nvram_unset(const char *name);
int _nvram_getall(char *buf, int count);
int _nvram_commit(struct nvram_header *header);
int _nvram_init(void *sih, int idx);
void _nvram_exit(void);
uint8 nvram_calc_crc(struct nvram_header *nvh);

#define NVRAM_HASH_TABLE_SIZE	257

static struct nvram_tuple *BCMINITDATA(nvram_hash)[NVRAM_HASH_TABLE_SIZE];
static struct nvram_tuple *nvram_dead;

/* curr_nvram_hash is used to reference for either nvram_hash or */
/* nvram_hash array                                              */
/* default to nvram_hash                                         */
struct nvram_tuple **BCMINITDATA(curr_nvram_hash) = nvram_hash;


#if defined(_CFE_) && defined(BCM_DEVINFO)
void _nvram_hash_select(int idx);
int _nvram_hash_sync(void);

static struct nvram_tuple *BCMINITDATA(devinfo_hash)[NVRAM_HASH_TABLE_SIZE];

void _nvram_hash_select(int idx)
{
	if (idx == 1) {
		curr_nvram_hash = devinfo_hash;
		return;
	}

	/* alll others, default to nvram_hash */
	curr_nvram_hash = nvram_hash;
}
#endif /* _CFE_ && BCM_DEVINFO */

/* Free all tuples. Should be locked. */
static void
BCMINITFN(nvram_free)(void)
{
	uint i;
	struct nvram_tuple *t, *next;

	/* Free hash table */
	for (i = 0; i < NVRAM_HASH_TABLE_SIZE; i++) {
		for (t = curr_nvram_hash[i]; t; t = next) {
			next = t->next;
			_nvram_free(t);
		}
		curr_nvram_hash[i] = NULL;
	}

	/* Free dead table */
	for (t = nvram_dead; t; t = next) {
		next = t->next;
		_nvram_free(t);
	}
	nvram_dead = NULL;

	/* Indicate to per-port code that all tuples have been freed */
	_nvram_free(NULL);
}

/* String hash */
static INLINE uint
hash(const char *s)
{
	uint hashval = 0;

	while (*s)
		hashval = 31 *hashval + *s++;

	return hashval;
}

/* (Re)initialize the hash table. Should be locked. */
static int
BCMINITFN(nvram_rehash)(struct nvram_header *header)
{
	char buf[] = "0xXXXXXXXX", *name, *value, *end, *eq;
	char *nvram_space_str = _nvram_get("nvram_space");
	unsigned long nvram_space = MAX_NVRAM_SPACE;

	if (nvram_space_str)
		nvram_space =  bcm_strtoul(nvram_space_str, NULL, 0);

	if (nvram_space < DEF_NVRAM_SPACE)
		nvram_space = DEF_NVRAM_SPACE;

	/* (Re)initialize hash table */
	nvram_free();

	/* Parse and set "name=value\0 ... \0\0" */
	name = (char *) &header[1];
	end = (char *) header + nvram_space - 2;
	end[0] = end[1] = '\0';
	for (; *name; name = value + strlen(value) + 1) {
		if (!(eq = strchr(name, '=')))
			break;
		*eq = '\0';
		value = eq + 1;
		_nvram_set(name, value);
		*eq = '=';
	}

#if defined(_CFE_) && defined(BCM_DEVINFO)
	if (curr_nvram_hash != nvram_hash) {
		/* init devinfo nvram variables */
		return 0;
	}
#endif /* _CFE_ && BCM_DEVINFO */

	/* Set special SDRAM parameters */
	if (!_nvram_get("sdram_init")) {
		sprintf(buf, "0x%04X", (uint16)(header->crc_ver_init >> 16));
		_nvram_set("sdram_init", buf);
	}
	if (!_nvram_get("sdram_config")) {
		sprintf(buf, "0x%04X", (uint16)(header->config_refresh & 0xffff));
		_nvram_set("sdram_config", buf);
	}
	if (!_nvram_get("sdram_refresh")) {
		sprintf(buf, "0x%04X", (uint16)((header->config_refresh >> 16) & 0xffff));
		_nvram_set("sdram_refresh", buf);
	}
	if (!_nvram_get("sdram_ncdl")) {
		sprintf(buf, "0x%08X", header->config_ncdl);
		_nvram_set("sdram_ncdl", buf);
	}

	return 0;
}

/* Get the value of an NVRAM variable. Should be locked. */
char *
_nvram_get(const char *name)
{
	uint i;
	struct nvram_tuple *t;
	char *value;

	if (!name)
		return NULL;

	/* Hash the name */
	i = hash(name) % NVRAM_HASH_TABLE_SIZE;

	/* Find the associated tuple in the hash table */
	for (t = curr_nvram_hash[i]; t && strcmp(t->name, name); t = t->next);

	value = t ? t->value : NULL;

	return value;
}

/* Set the value of an NVRAM variable. Should be locked. */
int
BCMINITFN(_nvram_set)(const char *name, const char *value)
{
	uint i;
	struct nvram_tuple *t, *u, **prev;

	/* Hash the name */
	i = hash(name) % NVRAM_HASH_TABLE_SIZE;

	/* Find the associated tuple in the hash table */
	for (prev = &curr_nvram_hash[i], t = *prev; t && strcmp(t->name, name);
	     prev = &t->next, t = *prev);

	/* (Re)allocate tuple */
	if (!(u = _nvram_realloc(t, name, value)))
		return -12; /* -ENOMEM */

	/* Value reallocated */
	if (t && t == u)
		return 0;

	/* Move old tuple to the dead table */
	if (t) {
		*prev = t->next;
		t->next = nvram_dead;
		nvram_dead = t;
	}

	/* Add new tuple to the hash table */
	u->next = curr_nvram_hash[i];
	curr_nvram_hash[i] = u;

	return 0;
}

/* Unset the value of an NVRAM variable. Should be locked. */
int
BCMINITFN(_nvram_unset)(const char *name)
{
	uint i;
	struct nvram_tuple *t, **prev;

	if (!name)
		return 0;

	/* Hash the name */
	i = hash(name) % NVRAM_HASH_TABLE_SIZE;

	/* Find the associated tuple in the hash table */
	for (prev = &curr_nvram_hash[i], t = *prev; t && strcmp(t->name, name);
	     prev = &t->next, t = *prev);

	/* Move it to the dead table */
	if (t) {
		*prev = t->next;
		t->next = nvram_dead;
		nvram_dead = t;
	}

	return 0;
}

/* Get all NVRAM variables. Should be locked. */
int
_nvram_getall(char *buf, int count)
{
	uint i;
	struct nvram_tuple *t;
	int len = 0;

	bzero(buf, count);

	/* Write name=value\0 ... \0\0 */
	for (i = 0; i < NVRAM_HASH_TABLE_SIZE; i++) {
		for (t = curr_nvram_hash[i]; t; t = t->next) {
			if ((count - len) > (strlen(t->name) + 1 + strlen(t->value) + 1))
				len += sprintf(buf + len, "%s=%s", t->name, t->value) + 1;
			else
				break;
		}
	}

	return 0;
}

/* Regenerate NVRAM. Should be locked. */
int
BCMINITFN(_nvram_commit)(struct nvram_header *header)
{
	char *init, *config, *refresh, *ncdl;
	char *ptr, *end;
	int i;
	struct nvram_tuple *t;
	char *nvram_space_str = _nvram_get("nvram_space");
	unsigned long nvram_space = DEF_NVRAM_SPACE;

	if (nvram_space_str)
		nvram_space =  bcm_strtoul(nvram_space_str, NULL, 0);

	if (nvram_space < DEF_NVRAM_SPACE)
		nvram_space = DEF_NVRAM_SPACE;

	/* Regenerate header */
	header->magic = NVRAM_MAGIC;
	header->crc_ver_init = (NVRAM_VERSION << 8);
	if (!(init = _nvram_get("sdram_init")) ||
	    !(config = _nvram_get("sdram_config")) ||
	    !(refresh = _nvram_get("sdram_refresh")) ||
	    !(ncdl = _nvram_get("sdram_ncdl"))) {
		header->crc_ver_init |= SDRAM_INIT << 16;
		header->config_refresh = SDRAM_CONFIG;
		header->config_refresh |= SDRAM_REFRESH << 16;
		header->config_ncdl = 0;
	} else {
		header->crc_ver_init |= (bcm_strtoul(init, NULL, 0) & 0xffff) << 16;
		header->config_refresh = bcm_strtoul(config, NULL, 0) & 0xffff;
		header->config_refresh |= (bcm_strtoul(refresh, NULL, 0) & 0xffff) << 16;
		header->config_ncdl = bcm_strtoul(ncdl, NULL, 0);
	}

	/* Clear data area */
	ptr = (char *) header + sizeof(struct nvram_header);
	bzero(ptr, nvram_space - sizeof(struct nvram_header));

	/* Leave space for a double NUL at the end */
	end = (char *) header + nvram_space - 2;

	/* Write out all tuples */
	for (i = 0; i < NVRAM_HASH_TABLE_SIZE; i++) {
		for (t = curr_nvram_hash[i]; t; t = t->next) {
			if ((ptr + strlen(t->name) + 1 + strlen(t->value) + 1) > end)
				break;
			ptr += sprintf(ptr, "%s=%s", t->name, t->value) + 1;
		}
	}

	/* End with a double NUL */
	ptr += 2;

	/* Set new length */
	header->len = ROUNDUP(ptr - (char *) header, 4);

	/* Set new CRC8 */
	header->crc_ver_init |= nvram_calc_crc(header);

	/* Reinitialize hash table */
	return nvram_rehash(header);
}

/* Initialize hash table. Should be locked. */
int
BCMINITFN(_nvram_init)(void *sih, int idx)
{
	struct nvram_header *header;
	int ret;


	if (!(header = (struct nvram_header *) MALLOC(si_osh(sih), MAX_NVRAM_SPACE))) {
		printf("nvram_init: out of memory\n");
		return -12; /* -ENOMEM */
	}

	if ((ret = _nvram_read(header, idx)) == 0 &&
	    header->magic == NVRAM_MAGIC)
		nvram_rehash(header);

	MFREE(si_osh(sih), header, MAX_NVRAM_SPACE);
	return ret;
}

/* Free hash table. Should be locked. */
void
BCMINITFN(_nvram_exit)(void)
{
	nvram_free();

#if defined(_CFE_) && defined(BCM_DEVINFO)
	/* free up devinfo hash table, etc */
	_nvram_hash_select(1);
	nvram_free();
	_nvram_hash_select(0);
#endif /* _CFE_ && BCM_DEVINFO */
}

/* returns the CRC8 of the nvram */
uint8
BCMINITFN(nvram_calc_crc)(struct nvram_header *nvh)
{
	struct nvram_header tmp;
	uint8 crc;

	/* Little-endian CRC8 over the last 11 bytes of the header */
	tmp.crc_ver_init = htol32((nvh->crc_ver_init & NVRAM_CRC_VER_MASK));
	tmp.config_refresh = htol32(nvh->config_refresh);
	tmp.config_ncdl = htol32(nvh->config_ncdl);

	crc = hndcrc8((uint8 *) &tmp + NVRAM_CRC_START_POSITION,
		sizeof(struct nvram_header) - NVRAM_CRC_START_POSITION,
		CRC8_INIT_VALUE);

	/* Continue CRC8 over data bytes */
	crc = hndcrc8((uint8 *) &nvh[1], nvh->len - sizeof(struct nvram_header), crc);

	return crc;
}

#if defined(_CFE_) && defined(BCM_DEVINFO)
/* sync nvram hash table with devinfo nvram hash table */
int
BCMINITFN(_nvram_hash_sync)(void)
{
	uint i;
	struct nvram_tuple *t, *next;

	/* traverse and _nvram_set() devinfo's entry to default hash table */
	_nvram_hash_select(0);
	for (i = 0; i < NVRAM_HASH_TABLE_SIZE; i++) {
		for (t = devinfo_hash[i]; t; t = next) {
			_nvram_set(t->name, t->value);
			next = t->next;
		}
	}

	return (0);
}
#endif /* _CFE_ && BCM_DEVINFO */
