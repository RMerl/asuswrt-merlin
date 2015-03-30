/* Copyright (c) 2003-2004, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file di_ops.h
 * \brief Headers for di_ops.c
 **/

#ifndef TOR_DI_OPS_H
#define TOR_DI_OPS_H

#include "orconfig.h"
#include "torint.h"

int tor_memcmp(const void *a, const void *b, size_t sz);
int tor_memeq(const void *a, const void *b, size_t sz);
#define tor_memneq(a,b,sz) (!tor_memeq((a),(b),(sz)))

/** Alias for the platform's memcmp() function.  This function is
 * <em>not</em> data-independent: we define this alias so that we can
 * mark cases where we are deliberately using a data-dependent memcmp()
 * implementation.
 */
#define fast_memcmp(a,b,c) (memcmp((a),(b),(c)))
#define fast_memeq(a,b,c)  (0==memcmp((a),(b),(c)))
#define fast_memneq(a,b,c) (0!=memcmp((a),(b),(c)))

int safe_mem_is_zero(const void *mem, size_t sz);

/** A type for a map from DIGEST256_LEN-byte blobs to void*, such that
 * data lookups take an amount of time proportional only to the size
 * of the map, and not to the position or presence of the item in the map.
 *
 * Not efficient for large maps! */
typedef struct di_digest256_map_t di_digest256_map_t;
typedef void (*dimap_free_fn)(void *);

void dimap_free(di_digest256_map_t *map, dimap_free_fn free_fn);
void dimap_add_entry(di_digest256_map_t **map,
                     const uint8_t *key, void *val);
void *dimap_search(const di_digest256_map_t *map, const uint8_t *key,
                   void *dflt_val);

#endif

