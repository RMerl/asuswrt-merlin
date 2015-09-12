/** HND GMAC Forwarder Implementation: LAN(GMAC) <--FWD--> WLAN
 * Include WOFA dictionary with 3 stage lookup.
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
 * $Id$
 *
 * vim: set ts=4 noet sw=4 tw=80:
 * -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 */

#if defined(BCM_GMAC3)

#include <linux/if.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/interrupt.h>
#include <osl.h>
#include <hndfwd.h>
#include <bcmutils.h>
#include <proto/vlan.h>
#include <proto/ethernet.h>


/** HND WOFA WiFi Offload Forwarder Assist for 3GMAC Atlas */

/** WOFA Dictionary manipulation helper static function declarations. */
/** WOFA Dictionary symbol manipulation. */
static inline uint16 __wofa_sym48_hash16(const uint16 * key);
static inline uint16 __wofa_sym48_cmp16(const uint16 * key, const uint16 * sym);
static inline void   __wofa_sym48_set16(const uint16 * key, uint16 * sym);
/** WOFA Dictionary bloom filter manipulation. */
static inline uint32 __wofa_bloom_lkup16(const uint32 * bfilter, const uint16 hash16);
static inline void   __wofa_bloom_set16(uint32 * bfilter, const uint16 hash16);
static inline void   __wofa_bloom_clr16(uint32 * bfilter, const uint16 hash16);

/** WOFA based Forwarding static function declarations. */
/** WOFA Accessors. */
static inline uint32 __wofa_syms(struct wofa * wofa);
#if defined(WOFA_STATS)
static inline uint32 __wofa_hits(struct wofa * wofa);
static inline uint32 __wofa_miss(struct wofa * wofa);
#endif /* WOFA_STATS */

#if (WOFA_DICT_SYMBOL_SIZE != 6)
#error "Validated for 6Byte symbols"
#endif

#if (WOFA_DICT_BKT_MAX != 256)
#error "Dictionary hash table size uses a uint8 index"
#endif

/** WOFA Symbol: key, 32bit associated metadata, and runtime stats */
typedef struct wofa_sym {           /* 16Byte symbol + metadata */
	uint16         hash16;
	union {                         /* dictionary symbol ethernet MAC address */
		uint8       u8[WOFA_DICT_SYMBOL_SIZE];
		uint16     u16[WOFA_DICT_SYMBOL_SIZE / sizeof(uint16)];
	} key;
	uintptr_t      data;            /* 32bit meta data */
#if defined(WOFA_STATS)
	uint32         hits;            /* Per symbol hit statistics */
#endif
} wofa_sym_t;

/** WOFA Fast lookup of cached last hit hash16. */
typedef struct wofa_cached {
	uint16         hash16;          /* Cached recent hit ix */
	wofa_sym_t     * sym;           /* Cache recent symbol */
} wofa_cached_t;

/* WOFA Hash table bin. */
typedef struct wofa_bin {
	wofa_sym_t     bin[WOFA_DICT_BIN_MAX];
} wofa_bin_t;

/* WOFA Hash table bkt of bins. */
typedef struct wofa_bkt {
	wofa_bin_t     bkt[WOFA_DICT_BKT_MAX];
} wofa_bkt_t;

/** WOFA Dictionary of MAC Address symbols for lookup based forwarding.
 * WOFA Dictionary constituted of a cached, bloom filter and hash table lkup.
 */
typedef struct wofa_dict {
	wofa_cached_t  cached[WOFA_MAX_PORTS];
	uint32         bloomfilter[WOFA_BLOOMFILTER_WORDS];
	wofa_bkt_t     table;           /* Hash table: buckets and bins */
} wofa_dict_t;

/** Forwarder instance of a WOFA lkup system. */
typedef struct wofa {
	wofa_dict_t    dict;            /* Dictionary of 6B symbols and metadata */
	int            syms;            /* Number of symbols in dictionary */
#if defined(WOFA_STATS)
	uint32         hits;            /* Lookup successful statistics counter */
	uint32         miss;            /* Lookup failure statistics counter */
#endif /* WOFA_STATS */
} wofa_t;


/** Hash table and symbol manipulation helper routines. */

/** Compute 16bit hash, on a 16bit aligned 6Byte key. */
static inline uint16
__wofa_sym48_hash16(const uint16 * key)
{
	uint16 hash16;
	hash16 = (*(key + 0)) ^ (*(key + 1)) ^ (*(key + 2));
	return hash16;
}

/** Compare two 6Byte values that are 2Byte aligned. */
static inline uint16
__wofa_sym48_cmp16(const uint16 * key, const uint16 * sym)
{
	/* returns 0 if equal */
	return ((*(key + 0) ^ *(sym + 0)) |
	        (*(key + 1) ^ *(sym + 1)) |
	        (*(key + 2) ^ *(sym + 2)));
}

/** Set a 6Byte 16bit aligned symbol from a given 6B key. */
static inline void
__wofa_sym48_set16(const uint16 * key, uint16 * sym)
{
	*(sym + 0) = *(key + 0);
	*(sym + 1) = *(key + 1);
	*(sym + 2) = *(key + 2);
}

/** Single hash bloom filter manipulation helper routines. */

/* Lkup a bloom filter for a 16bit hash_key */
static inline uint32
__wofa_bloom_lkup16(const uint32 * bloom_filter, const uint16 hash16)
{
	return ((*(bloom_filter + (hash16 >> 5))) & (1U << (hash16 & 31)));
}

static inline void
__wofa_bloom_set16(uint32 * bloom_filter, const uint16 hash16)
{
	*(bloom_filter + (hash16 >> 5)) |= (1U << (hash16 & 31));
}

static inline void
__wofa_bloom_clr16(uint32 * bloom_filter, const uint16 hash16)
{
	*(bloom_filter + (hash16 >> 5)) &= ~(1U << (hash16 & 31));
}

#define DECLARE_WOFA_FN(FIELD)                                                 \
static inline uint32 __wofa_ ## FIELD(struct wofa * wofa)                      \
{ return wofa->FIELD; }
DECLARE_WOFA_FN(syms) /* __wofa_syms(): Return num active symbols */
#if defined(WOFA_STATS)
DECLARE_WOFA_FN(hits) /* __wofa_hits() Return num of successful lkups */
DECLARE_WOFA_FN(miss) /* __wofa_miss() Return num of unsuccessful lkups */
#endif /* WOFA_STATS */

/** Allocate and reset storage for dictionary and bloom filter. */
struct wofa *
wofa_init(void)
{
	int bin, bkt, port;
	gfp_t flags;
	wofa_dict_t * dict;
	struct wofa * wofa;
	const int wofa_sz = sizeof(struct wofa);

	flags = CAN_SLEEP() ? GFP_KERNEL : GFP_ATOMIC;
	wofa = (struct wofa *)kmalloc(wofa_sz, flags);

	if (wofa == WOFA_NULL) {
		WOFA_WARN(("%s Failed allocate wofa_sz<%u>\n", __FUNCTION__, wofa_sz));
		ASSERT(wofa != WOFA_NULL);
		return WOFA_NULL;
	}

	bzero(wofa, wofa_sz); /* Initializes bloomfilter state */
	dict = &wofa->dict;

	/* Initialize cache lkup state */
	for (port = 0; port < WOFA_MAX_PORTS; port++) {
		dict->cached[port].hash16 = 0;
		dict->cached[port].sym = &dict->table.bkt[0].bin[0];
	}

	/* Initialize hash lkup state */
	for (bkt = 0; bkt < WOFA_DICT_BKT_MAX; bkt++) {
		for (bin = 0; bin < WOFA_DICT_BIN_MAX; bin++) {
			dict->table.bkt[bkt].bin[bin].data = WOFA_DATA_INVALID;
		}
	}

	WOFA_STATS_CLR(wofa->hits);
	WOFA_STATS_CLR(wofa->miss);

	WOFA_TRACE(("WOFA<%p> size<%uKB:%uB>\n", wofa, wofa_sz/1024, wofa_sz));

	return wofa;
}

/** Free storage for dictionary and bloom filter. */
void
wofa_fini(struct wofa * wofa)
{
	if (wofa == WOFA_NULL)
		return;

	kfree(wofa);
}

/** Add a symbol to WOFA dictionary: hash table and bloom-filter. */
int
wofa_add(struct wofa * wofa, uint16 * symbol, uintptr_t data)
{
	uint16 hash16;
	wofa_dict_t * dict;

	WOFA_ASSERT(wofa != WOFA_NULL);
	WOFA_ASSERT(symbol != NULL);
	WOFA_ALIGN16(symbol);
	WOFA_ASSERT(data != WOFA_DATA_INVALID);

	dict = &wofa->dict;
	hash16 = __wofa_sym48_hash16(symbol);

	{   /* Search the exact match hash table */
		int added = 0;
		uint8 hash8;      /* CAUTION: 8bit hash index for 258 buckets */
		wofa_sym_t * sym; /* walk bins (collision list) in a bucket */
		wofa_sym_t * end; /* last bin in the bucket collision list */

		/* Fold the 16bit hash into a 8bit hash table index */
		hash8 = ((uint8)(hash16) ^ (uint8)(hash16 >> 8));

		/* Fetch the first bin in the hashed bucket */
		sym = &dict->table.bkt[hash8].bin[0];
		end = sym + WOFA_DICT_BIN_MAX; /* after last bin in collision list */

		while ((uintptr)sym < (uintptr)end) {

			/* Check for duplicates */
			if (sym->data != WOFA_DATA_INVALID) { /* valid entry */

				if (__wofa_sym48_cmp16(symbol, sym->key.u16) == 0) {
					/* Found a previous entry */
					if (added == 0) { /* overwrite duplicate */
						WOFA_ASSERT(sym->hash16 == hash16);
						sym->data = data;
						WOFA_STATS_CLR(sym->hits);
						added = 1;
					} else {  /* duplicate? */
						WOFA_WARN(("Found several duplicate symbol"));
						sym->data = WOFA_DATA_INVALID;
						sym->hash16 = 0; /* 0 may be a valid value */
						wofa->syms--;
					}
				}

			} else if (added == 0) { /* found an empty bin, insert here */

				__wofa_bloom_set16(dict->bloomfilter, hash16);

				__wofa_sym48_set16(symbol, sym->key.u16);

				sym->hash16 = hash16;
				sym->data = data;
				WOFA_STATS_CLR(sym->hits);
				WOFA_TRACE(("WOFA add " __EFMT "data<0x%p>\n",
				            __EVAL((uint8*)symbol), (void*)data));
				wofa->syms++;
				added = 1; /* used to clear duplicates */
			}

			sym++; /* next bin; for duplicates/overwrite */
		}

		if (added == 0) {
			WOFA_WARN(("WOFA add " __EFMT "data<0x%p> : bkt<%u> overflow\n",
			            __EVAL((uint8*)symbol), (void*)data, hash8));
		}
	}

	return BCME_OK;
}

/** Delete a symbol from the WOFA dictionary and clear the bloom filter if no
 * other elements have a matching hash.
 */
int
wofa_del(struct wofa * wofa, uint16 * symbol, uintptr_t data)
{
	uint16 hash16;
	wofa_dict_t * dict;

	WOFA_ASSERT(wofa != WOFA_NULL);
	WOFA_ASSERT(symbol != NULL);
	WOFA_ALIGN16(symbol);
	WOFA_ASSERT(data != WOFA_DATA_INVALID);

	dict = &wofa->dict;
	hash16 = __wofa_sym48_hash16(symbol);

	{   /* Clear cached entry */
		wofa_cached_t * cached = &dict->cached[0];
		wofa_cached_t * end    = cached + WOFA_MAX_PORTS;
		while ((uintptr)cached < (uintptr)end) {
			if (cached->hash16 == hash16) {
				cached->hash16 = 0; /* 0 is a valid value */
				cached->sym = &dict->table.bkt[0].bin[0];
			}
			cached++; /* next port's cached entry */
		}
	}

	{   /* Clear matching bins for the found bucket, in hash table */
		uint8 hash8;      /* CAUTION: 8bit hash index for 258 buckets */
		wofa_sym_t * sym; /* walk bins (collision list) in a bucket */
		wofa_sym_t * end; /* last bin in the bucket collision list */

		/* Fold the 16bit hash into a 8bit hash table index */
		hash8 = ((uint8)(hash16) ^ (uint8)(hash16 >> 8));

		sym = &dict->table.bkt[hash8].bin[0];
		end = sym + WOFA_DICT_BIN_MAX; /* after last bin in collision list */

		while ((uintptr)sym < (uintptr)end) {
			if ((sym->data != WOFA_DATA_INVALID) && (sym->hash16 == hash16)) {
				if (__wofa_sym48_cmp16(symbol, sym->key.u16) == 0) {
					if (sym->data != data) {
						WOFA_WARN((
						   "%s sym->data<0x%p> != wofa<0x%p>\n",
						   __FUNCTION__, (void*)sym->data, (void*)data));
					} else {
						sym->data = WOFA_DATA_INVALID;
						sym->hash16 = 0; /* 0 is a valid value */
						wofa->syms--;
					}
				}
			}
			sym++; /* next bin in bkt collision list */
		}
	}

	/*
	 * If hash8 was computed from lower 8bits of hash16, then do not need to
	 * walk entire table.
	 */
	{   /* Update bloom_filter */
		int bkt, bin;
		int found_hash16 = 0;

		for (bkt = 0; bkt < WOFA_DICT_BKT_MAX; bkt++) {
			for (bin = 0; bin < WOFA_DICT_BIN_MAX; bin++) {
				const wofa_sym_t * sym = &dict->table.bkt[bkt].bin[bin];
				if ((sym->data != WOFA_DATA_INVALID) &&
				        (sym->hash16 == hash16)) {
					found_hash16 = 1;
					break;
				}
			}
		}

		if (found_hash16 == 0) {
			__wofa_bloom_clr16(dict->bloomfilter, hash16);
		}
	}

	return BCME_OK;
}


/** Delete all symbols in the WOFA dictionary that match the wofa metadata and
 * clear the bloom filter.
 */
int
wofa_clr(struct wofa * wofa, uintptr_t data)
{
	wofa_dict_t * dict;

	WOFA_ASSERT(wofa != WOFA_NULL);
	WOFA_ASSERT(data != WOFA_DATA_INVALID);

	dict = &wofa->dict;

	{   /* Reset cache entries */
		int port;
		for (port = 0; port < WOFA_MAX_PORTS; port++) {
			dict->cached[port].hash16 = 0;
			dict->cached[port].sym = &dict->table.bkt[0].bin[0];
		}
	}

	/* Reset bloom filter */
	bzero(dict->bloomfilter, sizeof(uint32) * WOFA_BLOOMFILTER_WORDS);

	{	/* Rebuild bloom filter after flushing symbols. */
		uint16 hash16;
		int bkt, bin;
		for (bkt = 0; bkt < WOFA_DICT_BKT_MAX; bkt++) {
			for (bin = 0; bin < WOFA_DICT_BIN_MAX; bin++) {
				if (dict->table.bkt[bkt].bin[bin].data == data) {
					hash16 = dict->table.bkt[bkt].bin[bin].hash16;
					__wofa_bloom_set16(dict->bloomfilter, hash16);
				}
			}
		}
	}

	return BCME_OK;
}

/** Lookup a symbol in the WOFA dictionary returning the wofa metadata.
 * 3 Step lookup is performed. First the last hit cached entry is tested.
 */
uintptr_t
wofa_lkup(struct wofa * wofa, uint16 * symbol, const int port)
{
	uint16 hash16;
	wofa_sym_t * sym;
	wofa_dict_t * dict;

	WOFA_ASSERT(wofa != WOFA_NULL);
	WOFA_ASSERT(symbol != NULL);
	WOFA_ALIGN16(symbol);

	dict = &wofa->dict;
	hash16 = __wofa_sym48_hash16(symbol);

	/* Lkup the cached hit entry first */
	if (hash16 == dict->cached[port].hash16) {
		sym = dict->cached[port].sym;
		if (__wofa_sym48_cmp16(symbol, sym->key.u16) == 0) {
			goto found_symbol;
		}
	}

	/* Now Lkup bloom filter and quickly exit on failure */
	if (__wofa_bloom_lkup16(dict->bloomfilter, hash16) == 0U) {
		goto bloomfilter_lkup_failure;
	}

	/* Single hash bloom filter susceptible to false positives, exact match */

	{   /* Now search the exact match hash table: testing all bins in bkt */
		uint8 hash8;
		wofa_sym_t * end; /* last bin in the bucket collision list */

		/* CAUTION: 8bit hash index for 258 buckets */
		/* Fold the 16bit hash into a 8bit hash table index */
		hash8 = ((uint8)(hash16) ^ (uint8)(hash16 >> 8));

		sym = &dict->table.bkt[hash8].bin[0];
		end = sym + WOFA_DICT_BIN_MAX; /* after last bin in collision list */

		/* Walk the bins for the hashed bucket */
		while ((uintptr)sym < (uintptr)end) {

			if (sym->hash16 == hash16) {
				/* Test exact match */
				if (__wofa_sym48_cmp16(symbol, sym->key.u16) == 0) {

					/* Cache the new hit index */
					dict->cached[port].hash16 = hash16;
					dict->cached[port].sym = sym;

					goto found_symbol;
				}
			}

			sym++; /* next bin in collision list */
		}
	}

bloomfilter_lkup_failure:
	WOFA_STATS_INCR(wofa->miss);

	return WOFA_DATA_INVALID;

found_symbol:
	WOFA_STATS_INCR(wofa->hits);
	WOFA_STATS_INCR(sym->hits);

	if (sym->data == WOFA_DATA_INVALID) {
		WOFA_WARN(("%s sym->data == WOFA_DATA_INVALID\n", __FUNCTION__));
	}

	return sym->data; /* return associated wofa metadata */
}

/** Debug dump a forwaring unit's WOFA table. */
void
wofa_dump(struct bcmstrbuf *b, struct wofa * wofa)
{
	int dump, word, port, bkt, bin;
	wofa_sym_t * sym;
	wofa_dict_t * dict;

	if (wofa == WOFA_NULL)
		return;

	dict = &wofa->dict;

	bcm_bprintf(b, "WOFA Symbols<%u>\n", __wofa_syms(wofa));

#if defined(WOFA_STATS)
	bcm_bprintf(b, "WOFA Statistics: hits<%u> miss<%u>\n",
	            __wofa_hits(wofa), __wofa_miss(wofa));
#endif /* WOFA_STATS */

	bcm_bprintf(b, "WOFA Cached Table Dump:\n");
	for (port = 0; port < WOFA_MAX_PORTS; port++) {
		bcm_bprintf(b, "\tPort<%2d> : hash<0x%04x> sym<%p>\n",
		            port, dict->cached[port].hash16, dict->cached[port].sym);
	}

	bcm_bprintf(b, "WOFA Bloom Filter Dump\n");
	for (word = 0; word < WOFA_BLOOMFILTER_WORDS; word++) {
		if (dict->bloomfilter[word] != 0) {
			bcm_bprintf(b, "\tBF[%04u] = 0x%08x\n",
			            word, dict->bloomfilter[word]);
		}
	}

	dump = 0;

	bcm_bprintf(b, "WOFA Hash Table Dump\n");
	for (bkt = 0; bkt < WOFA_DICT_BKT_MAX; bkt++) {
		for (bin = 0; bin < WOFA_DICT_BIN_MAX; bin++) {
			sym = &dict->table.bkt[bkt].bin[bin];
			if (sym->data != WOFA_DATA_INVALID) {
				bcm_bprintf(b, "\t" __EFMT "data<0x%p> "
#if defined(WOFA_STATS)
				            "hits<%u> "
#endif
				            "BktBin<%03d:%d] sym<%p> hash<0x%04X:%05u>\n",
				            __EVAL(sym->key.u8), (void*)sym->data,
#if defined(WOFA_STATS)
				            sym->hits,
#endif
				            bkt, bin, sym, sym->hash16, (int)sym->hash16);

				if (++dump > 32) {
					bcm_bprintf(b, "... too many to dump ...\n");
					break;
				}
			}
		}
	}

	bcm_bprintf(b, "\n\n");
}

/** HND Forwarder for 3GMAC Atlas */

/* forward declaration */
struct fwder_if;
struct fwder_cpumap;

#define FWDER_WOFA_NULL         (WOFA_NULL)
#define FWDER_NET_DEVICE_NULL   ((struct net_device *)NULL)
#define FWDER_BYPASS_FN_NULL    ((fwder_bypass_fn_t)NULL)

#if (WOFA_MAX_PORTS != (FWDER_MAX_IF + 1))
#error "mismatch WOFA_MAX_PORTS and FWDER_MAX_IF"
#endif

/* Safe fetch of a string member given a structure pointer. */
#define __SSTR(_struct_ptr, _member) \
	(((_struct_ptr) != NULL) ? (_struct_ptr)->_member : "null")

/** Forwarder static function delcarations. */
/** Forwarder accessor string formating. */
static inline const char * __fwder_dir(const int dir);
static inline const char * __fwder_mode(const int mode);
static inline const char * __fwder_chan(const int chan);
/** Forwarder default bypass handler. */
static int _fwder_bypass_fn(fwder_t * fwder, struct sk_buff * skbs, int skb_cnt,
                            struct net_device * rx_dev);
/** Forwarder cpumap configuration. */
static int _fwder_cpumap_config(int radio, struct fwder_cpumap *map);
static int _fwder_cpumap_parse(const char *fwder_cpumap_nvar_str);
/** Forwarder instance accessor. */
static inline fwder_t * ___fwder_self(const fwder_dir_t dir, int funit);
static inline fwder_t * __fwder_self(const fwder_dir_t dir, int funit);
static inline void      __fwder_sync_devs_cnt(fwder_t * fwder_dn);
static inline struct fwder_if * __fwder_if(int unit, int subunit);

/** Forwarder Debug and audit. */
static void _fwder_dump(struct bcmstrbuf *b, const fwder_t * fwder);
static void _fwder_devs_dump(struct bcmstrbuf *b, dll_t * fwder_if_dll);

static struct wofa * _fwder_wofa[FWDER_MAX_UNIT] =
{ FWDER_WOFA_NULL, FWDER_WOFA_NULL };

/* fwder enum to string conversions for debug dump: fwder_dir(), fwder_mode() */
static const char * _fwder_dir_g[FWDER_MAX_DIR] = { "UP", "DN" };
static const char * _fwder_mode_g[FWDER_MAX_MODE] = { "NIC", "DGL" };
static const char * _fwder_chan_g[FWDER_MAX_CHAN] = { "UPPER", "LOWER", "UNDEF" };

#define DECLARE_FWDER_ENUM_STR(name, max)                                      \
	static inline const char * __fwder_##name(const int name)                  \
	{ return (name < max) ? _fwder_##name##_g[name] : "invalid"; }
DECLARE_FWDER_ENUM_STR(dir, FWDER_MAX_DIR)   /* __fwder_dir(int) */
DECLARE_FWDER_ENUM_STR(mode, FWDER_MAX_MODE) /* __fwder_mode(int) */
DECLARE_FWDER_ENUM_STR(chan, FWDER_MAX_CHAN) /* __fwder_chan(int) */


/** Default dummy xmit handler bound to the fwder. */
static int
_fwder_bypass_fn(fwder_t * fwder, struct sk_buff * skbs, int skb_cnt,
                 struct net_device * rx_dev)
{
	ASSERT(fwder != FWDER_NULL);

	FWDER_PTRACE(("%s fwder<%p:%s> error skbs<%p> skb_cnt<%d> rx_dev<%p:%s>\n",
	              __FUNCTION__, fwder, __SSTR(fwder, name), skbs, skb_cnt,
	              rx_dev, __SSTR(rx_dev, name)));

	FWDER_STATS_ADD(fwder->dropped, skb_cnt);

	return FWDER_FAILURE;
}

/** HND Forwarder Radio to CPU/Fwder unit mapping. */
typedef struct fwder_cpumap {
	fwder_mode_t mode;
	fwder_chan_t chan;
	int band, irq, cpu;
	int unit; /* fwder unit assigned from FWDER_CPUMAP_NVAR nvram settings */
} fwder_cpumap_t;

#define _FWDER_CPUMAP_INI_(ix)                                                 \
	{ .mode = FWDER_NIC_MODE, .chan = FWDER_UNDEF_CHAN,                        \
	  .band = 0, .irq = 0, .cpu = -1, .unit = ix }

static fwder_cpumap_t fwder_cpumap_g[FWDER_MAX_RADIO] = {
	_FWDER_CPUMAP_INI_(0),
	_FWDER_CPUMAP_INI_(1),
	_FWDER_CPUMAP_INI_(2),
	_FWDER_CPUMAP_INI_(3)
};

/** Assign a fwder_unit number: [0 .. FWDER_MAX_RADIO) */
int
fwder_assign(fwder_mode_t mode, int radio_unit)
{
	FWDER_TRACE(("%s mode<%s> radio_unit<%d>\n",
	             __FUNCTION__, __fwder_mode(mode), radio_unit));
	ASSERT(fwder_cpumap_g[radio_unit].mode == mode);
	ASSERT(fwder_cpumap_g[radio_unit].cpu != -1);

	return fwder_cpumap_g[radio_unit].unit;
}

/** Assign the IRQ affinity for a radio identified by its assigned fwder_unit */
int
fwder_affinity(fwder_dir_t dir, int fwder_unit, int irq)
{
	int cpu_core;

	FWDER_TRACE(("%s dir<%s> fwder_unit<%d> irq<%d>\n",
	             __FUNCTION__, __fwder_dir(dir), fwder_unit, irq));
	FWDER_ASSERT((dir < FWDER_MAX_DIR));
	FWDER_ASSERT((fwder_unit < FWDER_MAX_RADIO));

	if (dir == FWDER_UPSTREAM) { /* Radio Interface */
		int radio_unit;
		fwder_cpumap_t * cpumap = &fwder_cpumap_g[0];

		for (radio_unit = 0; radio_unit < FWDER_MAX_RADIO; radio_unit++) {
			cpumap = &fwder_cpumap_g[radio_unit];
			if (cpumap->unit == fwder_unit)
				break;
		}

		FWDER_ASSERT((cpumap->unit == fwder_unit) && (cpumap->irq == irq));

		cpu_core = cpumap->cpu;
	} else { /* GMAC forwarder */
		cpu_core = fwder_unit;
	}

	FWDER_ASSERT((cpu_core < NR_CPUS));

	FWDER_TRACE(("%s irq_set_affinity irq<%d> cpu_core<%d>\n",
	             __FUNCTION__, irq, cpu_core));

	/* Setup IRQ affinity */
	irq_set_affinity(irq, cpumask_of(cpu_core));

	return FWDER_SUCCESS;
}

/** Assign a fwder_unit number given a CPU map configurtion.
 * When multiple radios share the same IRQ, they will be assigned to the same
 * CPU by allocating a fwder_unit number to allow modulo-2 CPU mapping.
 * The radio unit (probe sequence) and its configuration is used to allocate a
 * fwder_unit : [0 .. FWDER_MAX_RADIO)
 */
static int
_fwder_cpumap_config(int radio_unit, struct fwder_cpumap *map)
{
	int radio, fwder_unit;
	fwder_cpumap_t * cpumap;

	FWDER_TRACE(("%s %d. mode<%s> chan<%s> band<%d> irq<%d> cpu<%d>",
	             __FUNCTION__, radio_unit, __fwder_mode(map->mode),
	             __fwder_chan(map->chan), map->band, map->irq, map->cpu));
	FWDER_ASSERT((radio_unit < FWDER_MAX_RADIO));

	cpumap = &fwder_cpumap_g[radio_unit]; /* radio to be configured */
	FWDER_ASSERT((cpumap->cpu == -1));

	/* First radio sharing the IRQ will use the cpu core as fwder_unit. */
	fwder_unit = map->cpu;

	/* Traverse previously assigned cpumap and fetch the fwder_unit */
	for (radio = 0; radio < radio_unit; radio++) {
		if (fwder_cpumap_g[radio].irq == map->irq) {
			if (fwder_cpumap_g[radio].cpu != map->cpu) {
				FWDER_ERROR(("ERROR %s: unit:irq:cpu<%d:%d:%d> "
				           "mismatch <%d:%d:%d>\n", __FUNCTION__, radio,
				           fwder_cpumap_g[radio].irq, fwder_cpumap_g[radio].cpu,
				           radio_unit, map->irq, map->cpu));
				return FWDER_FAILURE;
			}

			/* Subsequent radios sharing IRQ will use first previous radio's
			 * fwder_unit + max forwarder(s) allowing a module-FWDER_MAX_UNIT
			 */
			fwder_unit = fwder_cpumap_g[radio].unit + FWDER_MAX_UNIT;
		}
	}

	cpumap->unit = fwder_unit; /* allocate fwder_unit for this radio. */
	cpumap->mode = map->mode;
	cpumap->chan = map->chan;
	cpumap->band = map->band;
	cpumap->irq  = map->irq;
	cpumap->cpu  = map->cpu;

	FWDER_TRACE((" fwder_unit<%d>\n", cpumap->unit));

	return cpumap->unit;
}

/** Parse the FWDER_CPUMAP_NVAR nvram variable and apply configuration. */
static int
_fwder_cpumap_parse(const char *fwder_cpumap_nvar_str)
{
	int radio_unit = 0;
	char parse_nvar_str[128]; /* local copy for strtok parsing */
	char *cpumap_all_str = parse_nvar_str; /* cpumap str for all radios */
	char *cpumap_per_str; /* cpumap str per radio */

	if (fwder_cpumap_nvar_str == NULL)
		return FWDER_FAILURE;

	FWDER_TRACE(("%s = [%s]\n", FWDER_CPUMAP_NVAR, fwder_cpumap_nvar_str));

	/* Make a local copy */
	strncpy(parse_nvar_str, fwder_cpumap_nvar_str, sizeof(parse_nvar_str));
	parse_nvar_str[sizeof(parse_nvar_str)-1] = '\0';

	/* Fetch each radio's cpumap substr */
	while ((cpumap_per_str = bcmstrtok((char **)&cpumap_all_str, " ", NULL))
		    != NULL) { /* per radio cpumap substr parsing */
		fwder_cpumap_t cpumap;
		char mode, chan;

		if (sscanf(cpumap_per_str, "%c:%c:%d:%d:%d", &mode, &chan,
		                       &cpumap.band, &cpumap.irq, &cpumap.cpu) != 5) {
			FWDER_ERROR(("ERROR %s: parsing radio cpumap %s\n",
			             __FUNCTION__, cpumap_per_str));
			return FWDER_FAILURE;
		}
		FWDER_ASSERT((cpumap.cpu < NR_CPUS));

		cpumap.mode = (mode == 'd') ? FWDER_DNG_MODE : FWDER_NIC_MODE;
		if (chan == 'u')
			cpumap.chan = FWDER_UPPER_CHAN;
		else if (chan == 'l')
			cpumap.chan = FWDER_LOWER_CHAN;
		else
			cpumap.chan = FWDER_UNDEF_CHAN;

		if (_fwder_cpumap_config(radio_unit, &cpumap) == FWDER_FAILURE) {
			FWDER_ERROR(("ERROR %s: configuring radio cpumap\n", __FUNCTION__));
			return FWDER_FAILURE;
		}

		radio_unit++; /* next radio unit */
	}

	return FWDER_SUCCESS;
}


/** HND Forwarder runtime state:
 * Two (one per radio) sets of <upstream,dnstream> forwarder's are maintained.
 */
#if defined(CONFIG_SMP)

#define _FWDER_INI_(NAME)                                                      \
	{                                                                          \
		.lock      = __SPIN_LOCK_UNLOCKED(.lock),                              \
		.lflags    = 0UL,                                                      \
		.mate      = FWDER_NULL,                                               \
		.bypass_fn = _fwder_bypass_fn,                                         \
		.dev_def   = FWDER_NET_DEVICE_NULL,                                    \
		.devs_cnt  = 0,                                                        \
		.devs_dll  = { .next_p = NULL, .prev_p = NULL },                       \
		.wofa      = FWDER_WOFA_NULL,                                          \
		.mode      = FWDER_NIC_MODE,                                           \
		.dataoff   = 0,                                                        \
		.osh       = NULL,                                                     \
		.name      = #NAME                                                     \
	}

/** Static declaration of fwder objects per cpu, accessed via per_cpu, */
DEFINE_PER_CPU(struct fwder, fwder_upstream_g) = _FWDER_INI_(upstream);
DEFINE_PER_CPU(struct fwder, fwder_dnstream_g) = _FWDER_INI_(dnstream);

/** Fetch an upstream or dnstream forwarder instance, by CPU# as unit. */
#define FWDER_GET(fwder, funit)	        &per_cpu((fwder), (funit))

#else  /* ! CONFIG_SMP */

#define _FWDER_INI_(NAME, FUNIT)                                               \
	{                                                                          \
		.lflags    = 0UL,                                                      \
		.mate      = FWDER_NULL,                                               \
		.bypass_fn = _fwder_bypass_fn,                                         \
		.dev_def   = FWDER_NET_DEVICE_NULL,                                    \
		.devs_cnt  = 0,                                                        \
		.devs_dll  = { .next_p = NULL, .prev_p = NULL },                       \
		.wofa      = FWDER_WOFA_NULL,                                          \
		.mode      = FWDER_NIC_MODE,                                           \
		.dataoff   = 0,                                                        \
		.osh       = (void*)NULL,                                              \
		.unit      = FUNIT,                                                    \
		.name      = #NAME                                                     \
	}


/** Static declaration of a set of upstream fwder indexed by GMAC/radio unit. */
fwder_t fwder_upstream_g[FWDER_MAX_UNIT] = {
	_FWDER_INI_(upstream, 0),
	_FWDER_INI_(upstream, 1)
};

/** Static declaration of a set of dnstream fwder indexed by GMAC/radio unit. */
fwder_t fwder_dnstream_g[FWDER_MAX_UNIT] = {
	_FWDER_INI_(dnstream, 0),
	_FWDER_INI_(dnstream, 1)
};

/** Fetch an upstream or dnstream forwarder instance, indexed by unit. */
#define FWDER_GET(fwder, funit)         &fwder[funit]

#endif /* ! CONFIG_SMP */


/** Mapping a interface unit to a fwedr unit. */
#define FWDER_UNIT(unit)                ((unit) % FWDER_MAX_UNIT)

/** Used to instantiate a pool of virtual fwder if per fwder.
 * When multiple WLAN interfaces bind to a fwder, a list of all actively bound
 * WLAN interfaces is maintained in the upstream fwder. This list will be used
 * to broadcast packets to all active WLAN interfaces (excluding the WLAN
 * interface on which the broadcast packet was received).
 */
typedef struct fwder_if {
	dll_t             node;         /* Double Linked List node */
	struct net_device * dev;        /* Network device associated with wlif */
} fwder_if_t;

/** Free pool of fwder_if objects. */
typedef struct fwder_if_pool {
	fwder_if_t     pool[FWDER_MAX_RADIO][FWDER_MAX_IF];
} fwder_if_pool_t;

/** Static declaration of a global if pool: an array of fwder_if objects.
 * Accessible from either CPU core (lock free).
 */
struct fwder_if_pool _fwder_if_pool_g;

/** Fetch the forwarder object for a given a direction and unit. */
static inline fwder_t *
___fwder_self(const fwder_dir_t dir, int funit)
{
	fwder_t * fwder;

	if (dir == (int)FWDER_UPSTREAM)
		fwder = FWDER_GET(fwder_upstream_g, funit);
	else
		fwder = FWDER_GET(fwder_dnstream_g, funit);

	/* fwder's unit may not be set yet. */
	return fwder;
}

static inline fwder_t *
__fwder_self(const fwder_dir_t dir, int funit)
{
	fwder_t * fwder = ___fwder_self(dir, funit);
	FWDER_ASSERT(fwder->unit == funit);
	return fwder;
}

/** Update number of devices in upstream forwarder. */
static inline void
__fwder_sync_devs_cnt(fwder_t * fwder_dn)
{
	fwder_t * fwder_up = fwder_dn->mate;
	fwder_up->devs_cnt = fwder_dn->devs_cnt + 1; /* upstream has one fwdXX */
}

/** Fetch the fwder if object, give a unit and subunit. */
static inline fwder_if_t *
__fwder_if(int unit, int subunit)
{
	FWDER_ASSERT(unit < FWDER_MAX_RADIO);
	FWDER_ASSERT(subunit < FWDER_MAX_IF);
	return &_fwder_if_pool_g.pool[unit][subunit];
}

/** Instantiate and initialize forwarder, WOFA and free if pool. */
int
fwder_init(void)
{
	int dir, funit, unit, subunit;
	fwder_if_t * fwder_if;
	fwder_t * fwder;

	if (_fwder_wofa[0] != FWDER_WOFA_NULL)
		return FWDER_SUCCESS;

	FWDER_TRACE(("%s: WOFA dictionary, fwder_if pool, fwder, cpumap\n",
	             __FUNCTION__));

	/* Instantiate WOFA dictionary. */
	for (funit = 0; funit < FWDER_MAX_UNIT; funit++) {
		_fwder_wofa[funit] = wofa_init();
		if (_fwder_wofa[funit] == FWDER_WOFA_NULL)
			return FWDER_FAILURE;
	}

	/* Initialize the free pool of fwder_if */
	for (unit = 0; unit < FWDER_MAX_RADIO; unit++) {
		for (subunit = 0; subunit < FWDER_MAX_IF; subunit++) {
			fwder_if = __fwder_if(unit, subunit);
			fwder_if->dev = FWDER_NET_DEVICE_NULL;
		}
	}

#if defined(CONFIG_SMP)
	FWDER_TRACE(("%s SMP Per CPU objects.\n", __FUNCTION__));
#else
	FWDER_TRACE(("%s global pair.\n", __FUNCTION__));
#endif

#if defined(CONFIG_SMP)
	for_each_online_cpu(funit)
#else
	for (funit = 0; funit < FWDER_MAX_UNIT; funit++)
#endif /* ! CONFIG_SMP */
	{
		/* Initialize the fwder instances */
		for (dir = (int)FWDER_UPSTREAM; dir < (int)FWDER_MAX_DIR; dir++) {

			fwder = ___fwder_self(dir, funit);
			fwder->mate = ___fwder_self((dir + 1) % FWDER_MAX_DIR, funit);

			fwder->bypass_fn = _fwder_bypass_fn;

			fwder->devs_cnt = 0;
			fwder->dev_def = FWDER_NET_DEVICE_NULL;
			dll_init(&fwder->devs_dll);

			fwder->wofa = _fwder_wofa[funit];

			FWDER_STATS_CLR(fwder->transmit);
			FWDER_STATS_CLR(fwder->dropped);
			FWDER_STATS_CLR(fwder->flooded);

			fwder->mode = FWDER_NIC_MODE;
			fwder->dataoff = 0;
			fwder->unit = funit;
		}

	} /* for_each_online_cpu | for funit = 0 .. FWDER_MAX_UNIT */

	{   /* Initialize the radio to fwder_unit mapping from FWDER_CPUMAP_NVAR */
		const char * fwder_cpumap_nvar;
		fwder_cpumap_nvar = getvar(NULL, FWDER_CPUMAP_NVAR);

		if (fwder_cpumap_nvar == NULL) {
			fwder_cpumap_nvar = FWDER_CPUMAP_DEFAULT; /* default cpumap */
			FWDER_ERROR(("ERROR %s: %s nvram not present, using default\n",
			             __FUNCTION__, FWDER_CPUMAP_NVAR));
		}

		if (_fwder_cpumap_parse(fwder_cpumap_nvar) == FWDER_FAILURE) {
			FWDER_ERROR(("ERROR %s: failure parsing %s\n",
			             __FUNCTION__, fwder_cpumap_nvar));
			return FWDER_FAILURE;
		}
	}
	return FWDER_SUCCESS;
}

/** Destruct forwarder. */
int
fwder_exit(void)
{
	int funit;
	fwder_t * fwder;

#if defined(CONFIG_SMP)
	for_each_online_cpu(funit)
#else
	for (funit = 0; funit < FWDER_MAX_UNIT; funit++)
#endif /* ! CONFIG_SMP */
	{
		fwder = __fwder_self(FWDER_UPSTREAM, funit);
		wofa_fini(fwder->wofa);
		fwder->mate->wofa = fwder->wofa = FWDER_WOFA_NULL;
		_fwder_wofa[funit] = FWDER_WOFA_NULL;
	}

	return FWDER_SUCCESS;
}


/** Register a bypass handler and return the reverse dir forwarder object.
 * fwder_attach is called by the GMAC forwarder and the primary WLAN interface.
 *
 * The primary WLAN interface will attach a NULL dev.
 * WLAN interfaces, including the primary interface, needs to explicitly invoke
 * fwder_bind().
 */
fwder_t *
fwder_attach(fwder_dir_t dir, int unit, fwder_mode_t mode,
             fwder_bypass_fn_t bypass_fn, struct net_device * dev, void * osh)
{
	int funit;
	fwder_t * self;
	fwder_t * mate; /* Reverse direction forwarder, returned */

	FWDER_TRACE(("%s: dir<%s> unit<%d> mode<%s> <%p,%pS> dev<%p:%s>\n",
	             __FUNCTION__, __fwder_dir(dir), unit, __fwder_mode(mode),
	             bypass_fn, bypass_fn, dev, __SSTR(dev, name)));

	ASSERT((int)dir < (int)FWDER_MAX_DIR);
	ASSERT((int)mode < (int)FWDER_MAX_MODE);
	ASSERT(bypass_fn != FWDER_BYPASS_FN_NULL);
	if (dir == FWDER_DNSTREAM) {
		ASSERT(dev == FWDER_NET_DEVICE_NULL);
	} else {
		ASSERT(dev != FWDER_NET_DEVICE_NULL);
	}

	/* Use the unit # and direction to fetch the fwder and mate's fwder */
	funit = FWDER_UNIT(unit);
	self = __fwder_self(dir, funit);

	_FWDER_LOCK(self);                                     /* ++LOCK */

	/* Configure self */
	self->mode = mode; /* in dnstream dir, mode will be used by fwd#0, fwd#1 */
	self->bypass_fn = bypass_fn;
	if (dir == FWDER_UPSTREAM) {
		self->devs_cnt = 1;
		self->dev_def = dev;
		FWDER_ASSERT(dev != FWDER_NET_DEVICE_NULL);
	}
	self->osh = osh;

	/* Return the mate's fwder */
	mate = self->mate;
	FWDER_ASSERT(mate->unit == self->unit);

	_FWDER_UNLOCK(self);                                   /* --LOCK */

	return mate;
}

/** Dettach an interface from the forwarder by dettaching the bypass handler.
 * fwder_dettach is called by the GMAC forwarder and the primary WLAN interface.
 */
fwder_t *
fwder_dettach(fwder_t * mate, fwder_dir_t dir, int unit)
{
	int funit;
	fwder_t * self;

	FWDER_TRACE(("%s: mate<%p> dir<%s> unit<%d>\n",
	            __FUNCTION__, mate, __fwder_dir(dir), unit));

	ASSERT(dir < FWDER_MAX_DIR);
	ASSERT(unit < FWDER_MAX_RADIO);

	if (mate == FWDER_NULL)
		return FWDER_NULL;

	funit = FWDER_UNIT(unit);
	self = __fwder_self(dir, funit);

	ASSERT(self == mate->mate);
	FWDER_ASSERT(self->unit == mate->unit);

	_FWDER_LOCK(self);                                     /* ++LOCK */

	/* A WLAN unit dettached */
	if (dir == (int)FWDER_DNSTREAM) {
		int subunit;
		fwder_if_t * fwder_if;

		for (subunit = 0; subunit < FWDER_MAX_IF; subunit++) {
			fwder_if = __fwder_if(unit, subunit);

			/* Ensure no interfaces are still bound to the forwarder */
			if (fwder_if->dev != FWDER_NET_DEVICE_NULL) {
				/* Move to pool's free list */
				FWDER_ASSERT(!dll_empty(&self->devs_dll));
				fwder_flush(self, (uintptr_t)fwder_if->dev);
				self->devs_cnt--;
				dll_delete(&fwder_if->node);
				fwder_if->dev = FWDER_NET_DEVICE_NULL;
			}
		}

		__fwder_sync_devs_cnt(self); /* sync upstream fwder devs_cnt */
	}

	if (self->devs_cnt == 0) {
		self->bypass_fn = _fwder_bypass_fn;
		self->dev_def = FWDER_NET_DEVICE_NULL;
		self->mode = FWDER_NIC_MODE;
	}

	if (dir == FWDER_UPSTREAM) {
		self->dev_def = FWDER_NET_DEVICE_NULL;
	}

	_FWDER_UNLOCK(self);                                   /* --LOCK */

	return FWDER_NULL;
}

/** Given an upstream fwder handle, register a default interface
 * to mate downstream fwder. Deregister by using a NULL net_device.
 */
int
fwder_register(fwder_t * fwder, struct net_device * dev)
{
	FWDER_TRACE(("%s fwder<%p> dev<%p:%s>\n", __FUNCTION__,
	             fwder, dev, __SSTR(dev, name)));

	if (fwder == FWDER_NULL)
		return FWDER_FAILURE;

	ASSERT(fwder == __fwder_self(FWDER_UPSTREAM, fwder->unit));

	/* register with downstream fwder */
	fwder->mate->dev_def = dev;

	return FWDER_SUCCESS;
}

/** Given a downstream fwder handle, fetch the default registered device. */
struct net_device *
fwder_default(fwder_t * fwder)
{
	if (fwder == FWDER_NULL)
		return FWDER_NET_DEVICE_NULL;

	ASSERT(fwder == __fwder_self(FWDER_DNSTREAM, fwder->unit));

	return fwder->dev_def;
}

/** Bind/Unbind HW switching to a primary/virtual interface.
 * WLAN interfaces use the fwder_bind to attach a primary or virtual interface
 * once the net_device is registered. The WLAN interface name will be fetched
 * from the nvram fwd_wlandevs to determine eligibility for HW switching.
 *
 * Note: The interface is added to the downstream forwarder.
 *       mate points to upstream forwarder.
 */
fwder_t *
fwder_bind(fwder_t * mate, int unit, int subunit, struct net_device * dev,
	bool attach)
{
	int funit;
	fwder_t * self;
	fwder_if_t * fwder_if;

	FWDER_TRACE(("%s unit<%d> subunit<%d> dev<%p:%s>\n", __FUNCTION__,
	             unit, subunit, dev, __SSTR(dev, name)));

	ASSERT(unit < FWDER_MAX_RADIO);
	ASSERT(subunit < FWDER_MAX_IF);
	ASSERT(dev != FWDER_NET_DEVICE_NULL);

	/* Primary did not attach a fwder */
	if (mate == FWDER_NULL)
		return FWDER_NULL;

	{	/* Check if this interface is eligible for HW switching */
		const char * fwder_wlifs_nvar;
		fwder_wlifs_nvar = getvar(NULL, FWDER_WLIFS_NVAR);
		if (bcmstrstr(fwder_wlifs_nvar, dev->name) == NULL) {
			return FWDER_NULL;
		}
	}

	funit = FWDER_UNIT(unit);
	fwder_if = __fwder_if(unit, subunit);

	/* Fetch the fwder and the fwder_if */
	self = mate->mate; /* downstream direction forwarder */
	FWDER_ASSERT(mate == __fwder_self(FWDER_UPSTREAM, funit));
	FWDER_ASSERT(self == __fwder_self(FWDER_DNSTREAM, funit));
	FWDER_ASSERT(mate->unit == self->unit);

	/* Add the interface to the downstream forwarder (used by et). */
	_FWDER_LOCK(self);                                     /* ++LOCK */

	if (attach == TRUE) {

		/* Check if wlif<subunit> is re-binding. */
		if (fwder_if->dev != FWDER_NET_DEVICE_NULL) {
			FWDER_WARN(("%s re bind at<%d,%d> new dev<%p:%s> old dev<%p:%s>\n",
			            __FUNCTION__, unit, subunit, dev, __SSTR(dev, name),
			            fwder_if->dev, __SSTR(fwder_if->dev, name)));
			ASSERT(dev == fwder_if->dev);
			fwder_if->dev = dev;
			goto unlock_ret;
		}

		/* Move to active if list. */
		fwder_if->dev = dev;
		dll_append(&self->devs_dll, &fwder_if->node);

		self->devs_cnt++;

	} else { /* attach == FALSE */

		if (fwder_if->dev != FWDER_NET_DEVICE_NULL) {
			/* Move to pool's free list */
			dll_delete(&fwder_if->node);
			fwder_if->dev = FWDER_NET_DEVICE_NULL;

			/* Is attached, so dettach */
			self->devs_cnt--;
			FWDER_ASSERT(self->devs_cnt >= 0);

		} else { /* already dettached, do nothing. */
			FWDER_WARN(("%s unbind NULL dev at<%d,%d>\n",
			            __FUNCTION__, unit, subunit));
		}
	}

	__fwder_sync_devs_cnt(self); /* sync upstream fwder devs_cnt */

unlock_ret:

	_FWDER_UNLOCK(self);                                   /* --LOCK */

	return mate;
}

/** Add a station to a forwarder's WOFA on association or reassociation. */
int
fwder_reassoc(fwder_t * fwder, uint16 * symbol, uintptr_t data)
{
	int err;
	if (fwder == FWDER_NULL)
		return FWDER_FAILURE;

	FWDER_TRACE(("%s fwder<%p,%s> " __EFMT "data<0x%p>\n", __FUNCTION__, fwder,
	            __SSTR(fwder, name), __EVAL((uint8*)symbol), (void*)data));
	FWDER_ASSERT(fwder->wofa == fwder->mate->wofa);

	err = wofa_add(fwder->wofa, symbol, data);

	return err;
}

/** Delete a station from a forwarder's WOFA on deassociation. */
int
fwder_deassoc(fwder_t * fwder, uint16 * symbol, uintptr_t data)
{
	int err;
	if (fwder == FWDER_NULL)
		return FWDER_FAILURE;

	FWDER_TRACE(("%s fwder<%p,%s> " __EFMT "data<0x%p>\n", __FUNCTION__, fwder,
	            __SSTR(fwder, name), __EVAL((uint8*)symbol), (void*)data));
	FWDER_ASSERT(fwder->wofa == fwder->mate->wofa);

	err = wofa_del(fwder->wofa, symbol, data);

	return err;
}

/** Flush all entries in the forwarder's WOFA containing the metadata */
int
fwder_flush(fwder_t * fwder, uintptr_t data)
{
	int err;
	if ((fwder == FWDER_NULL) || (data == WOFA_DATA_INVALID))
		return FWDER_FAILURE;

	FWDER_TRACE(("%s fwder<%p,%s> data<0x%p>\n", __FUNCTION__,
	            fwder, __SSTR(fwder, name), (void*)data));
	FWDER_ASSERT(fwder->wofa == fwder->mate->wofa);

	err = wofa_clr(fwder->wofa, data);

	return err;
}

/** Lookup WOFA for a station (by Mac Address) assocatied with a forwarder. */
uintptr_t
fwder_lookup(fwder_t * fwder, uint16 * symbol, const int port)
{
	uintptr_t data;
	FWDER_ASSERT(fwder != FWDER_NULL);
	FWDER_PTRACE(("%s fwder<%p,%s> " __EFMT "port<%d>\n", __FUNCTION__,
	              fwder, __SSTR(fwder, name), __EVAL((uint8*)symbol), port));
	data = wofa_lkup(fwder->wofa, symbol, port);
	return data;
}

/** Flood a packet to all interfaces. Free original if clone is FALSE. */
int
fwder_flood(fwder_t * fwder, struct sk_buff * skb, void * osh, bool clone,
            fwder_flood_fn_t dev_start_xmit)
{
	int ret = FWDER_SUCCESS;
	struct sk_buff * nskb;
	struct net_device * dev;
	dll_t * item, * next, * list;

	FWDER_PTRACE(("%s fwder<%p,%s> skb<0x%p>, osh<%p>\n", __FUNCTION__,
	              fwder, __SSTR(fwder, name), skb, osh));

	/* Traverse the list of bound interfaces in the downstream forwarder. */
	list = &fwder->devs_dll;
	for (item = dll_head_p(list); !dll_end(list, item); item = next) {
		next = dll_next_p(item);

		dev = ((fwder_if_t *)item)->dev; /* fetch fwder interface's netdevice */

		/* do not flood back to self, so skb->dev must be properly set */
		if ((dev != skb->dev) && (dev->flags & IFF_UP)) {

			/* Use PKTDUP, which will manipulate the appropriate CTF fields */
			if ((nskb = (struct sk_buff *)PKTDUP(osh, skb)) == NULL) {
				ret = FWDER_FAILURE;
				break;
			}

			FWDER_PTRACE(("%s skb<0x%p> %pS\n", __FUNCTION__,
			              nskb, dev_start_xmit));

			/* dispatch to either NIC or DHD start xmit */
			nskb->dev = dev;
			dev_start_xmit(nskb, dev, TRUE);

			FWDER_STATS_ADD(fwder->flooded, 1);
		}
	}

	if (clone == FALSE) { /* free original skb */
		PKTFREE(osh, skb, FALSE);
	}

	return ret;
}

/** Fixup a packet received from a GMAC forwarder. */
void
fwder_fixup(fwder_t * fwder, struct sk_buff * skb)
{
	FWDER_PTRACE(("%s fwder<%p,%s> skb<%p>\n", __FUNCTION__,
	              fwder, __SSTR(fwder, name), skb));
	/* strip off rxhdr and popping of BRCM TAG */
	__skb_pull(skb, fwder->dataoff);
	ASSERT(((ulong)skb->data & 3) == 2); /* aligned 2-mod-4 */

	/* strip off 4Byte CRC32 at tail end */
	skb_trim(skb, skb->len - ETHER_CRC_LEN);

	PKTCLRFWDERBUF(fwder->osh, skb); /* redundant, but safe */
}

/** Downstream forwarder discarding a packet in the context of GMAC forwarder */
void
fwder_discard(fwder_t * fwder, struct sk_buff * skb)
{
	FWDER_PTRACE(("%s fwder<%p,%s> skb<%p>\n", __FUNCTION__,
	              fwder, __SSTR(fwder, name), skb));
	PKTFRMFWDER(fwder->osh, skb, 1);
	PKTFREE(fwder->osh, skb, FALSE);
}

/** Debug dump the Radio to forwarder cpu mapping. */
static void
_fwder_cpumap_dump(struct bcmstrbuf *b)
{
	int ix;
	fwder_cpumap_t * cpumap;
	for (ix = 0; ix < FWDER_MAX_RADIO; ix++) {
		cpumap = &fwder_cpumap_g[ix];
		if (cpumap->cpu == -1) continue;
		bcm_bprintf(b,
		    "%d. mode<%s> chan<%s> band<%d> irq<%d> cpu<%d> unit<%d>\n",
		    ix, __fwder_mode(cpumap->mode), __fwder_chan(cpumap->chan),
		    cpumap->band, cpumap->irq, cpumap->cpu, cpumap->unit);
	}
}

/** Debug dump a forwarding object. */
static void
_fwder_dump(struct bcmstrbuf *b, const fwder_t * fwder)
{
	if (fwder == FWDER_NULL)
		return;

	bcm_bprintf(b, "Fwder[%p,%s]: unit<%u> mate<%p:%s> mode<%s>"
#if defined(FWDER_STATS)
	            " transmit<%u> dropped<%u> flooded<%u>\n"
#endif
	            "\tdevs_cnt<%u> dev<%p:%s> bypass<%p:%pS>\n", fwder,
	            __SSTR(fwder, name), fwder->unit, fwder->mate,
	            __SSTR(fwder->mate, name), __fwder_mode(fwder->mode),
#if defined(FWDER_STATS)
	            fwder->transmit, fwder->dropped, fwder->flooded,
#endif
	            fwder->devs_cnt, fwder->dev_def, __SSTR(fwder->dev_def, name),
	            fwder->bypass_fn, fwder->bypass_fn);
}

/** Debug dump the list of bound interfaces (net_device). */
static void
_fwder_devs_dump(struct bcmstrbuf *b, dll_t * fwder_if_dll)
{
	struct net_device * dev;
	dll_t * item, * next;
	bcm_bprintf(b, "\tBound devices:\n");
	/* Traverse the list of active interfaces */
	/* Do additional checking for item because fwder_if_dll doesn't be initialized
	 * by dll_init when gmac_fwd is FALSE.
	 */
	for (item = dll_head_p(fwder_if_dll);
	     item && !dll_end(fwder_if_dll, item);
	     item = next)
	{
		next = dll_next_p(item);
		dev = ((fwder_if_t *)item)->dev;
		bcm_bprintf(b, "\t\tdev<%p,%s>\n", dev, __SSTR(dev, name));
	}
}

/** Debug dump all forwarding objects units<0,1>: <Upstream,Dnstream> pairs. */
void
fwder_dump(struct bcmstrbuf *b)
{
	int unit;
	fwder_t * fwder;

	ASSERT(b != NULL);

	bcm_bprintf(b, "Fwder Dump default bypass<%p,%pS>\n",
	            _fwder_bypass_fn, _fwder_bypass_fn);

	/* Dump the Radio to Fwder unit cpu map */
	_fwder_cpumap_dump(b);

	/* Traverse the two bidirectional forwarders. */
#if defined(CONFIG_SMP)
	for_each_online_cpu(unit)
#else
	for (unit = 0; unit < FWDER_MAX_UNIT; unit++)
#endif  /* !CONFIG_SMP */
	{
		fwder = FWDER_GET(fwder_upstream_g, unit);
		_fwder_dump(b, fwder); /* dump upstream forwarder */
		_fwder_dump(b, fwder->mate); /* dump mate downstream forwarder */
		_fwder_devs_dump(b, &fwder->devs_dll); /* dump bound interfaces */
		wofa_dump(b, fwder->wofa); /* dump WOFA ARL */
	}	/* for_each_online_cpu | for unit = 0 .. FWDER_MAX_UNIT */
}
#endif  /*  BCM_GMAC3 */
