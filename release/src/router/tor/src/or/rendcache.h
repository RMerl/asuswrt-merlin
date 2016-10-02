/* Copyright (c) 2015-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rendcache.h
 * \brief Header file for rendcache.c
 **/

#ifndef TOR_RENDCACHE_H
#define TOR_RENDCACHE_H

#include "or.h"
#include "rendcommon.h"

/** How old do we let hidden service descriptors get before discarding
 * them as too old? */
#define REND_CACHE_MAX_AGE (2*24*60*60)
/** How wrong do we assume our clock may be when checking whether hidden
 * services are too old or too new? */
#define REND_CACHE_MAX_SKEW (24*60*60)
/** How old do we keep an intro point failure entry in the failure cache? */
#define REND_CACHE_FAILURE_MAX_AGE (5*60)

/* Do not allow more than this many introduction points in a hidden service
 * descriptor */
#define MAX_INTRO_POINTS 10

/** A cached rendezvous descriptor. */
typedef struct rend_cache_entry_t {
  size_t len; /**< Length of <b>desc</b> */
  time_t last_served; /**< When did we last write this one to somebody?
                       * (HSDir only) */
  char *desc; /**< Service descriptor */
  rend_service_descriptor_t *parsed; /**< Parsed value of 'desc' */
} rend_cache_entry_t;

/* Introduction point failure type. */
typedef struct rend_cache_failure_intro_t {
  /* When this intro point failure occured thus we allocated this object and
   * cache it. */
  time_t created_ts;
  rend_intro_point_failure_t failure_type;
} rend_cache_failure_intro_t;

/** Cache failure object indexed by service ID. */
typedef struct rend_cache_failure_t {
  /* Contains rend_cache_failure_intro_t indexed by identity digest. */
  digestmap_t *intro_failures;
} rend_cache_failure_t;

typedef enum {
  REND_CACHE_TYPE_CLIENT  = 1,
  REND_CACHE_TYPE_SERVICE = 2,
} rend_cache_type_t;

void rend_cache_init(void);
void rend_cache_clean(time_t now, rend_cache_type_t cache_type);
void rend_cache_failure_clean(time_t now);
void rend_cache_clean_v2_descs_as_dir(time_t now, size_t min_to_remove);
void rend_cache_purge(void);
void rend_cache_free_all(void);
int rend_cache_lookup_entry(const char *query, int version,
                            rend_cache_entry_t **entry_out);
int rend_cache_lookup_v2_desc_as_service(const char *query,
                                         rend_cache_entry_t **entry_out);
int rend_cache_lookup_v2_desc_as_dir(const char *query, const char **desc);

int rend_cache_store_v2_desc_as_dir(const char *desc);
int rend_cache_store_v2_desc_as_service(const char *desc);
int rend_cache_store_v2_desc_as_client(const char *desc,
                                       const char *desc_id_base32,
                                       const rend_data_t *rend_query,
                                       rend_cache_entry_t **entry);
size_t rend_cache_get_total_allocation(void);

void rend_cache_intro_failure_note(rend_intro_point_failure_t failure,
                                   const uint8_t *identity,
                                   const char *service_id);
void rend_cache_failure_purge(void);

#ifdef RENDCACHE_PRIVATE

STATIC size_t rend_cache_entry_allocation(const rend_cache_entry_t *e);
STATIC void rend_cache_entry_free(rend_cache_entry_t *e);
STATIC void rend_cache_failure_intro_entry_free(rend_cache_failure_intro_t
                                                *entry);
STATIC void rend_cache_failure_entry_free(rend_cache_failure_t *entry);
STATIC int cache_failure_intro_lookup(const uint8_t *identity,
                                      const char *service_id,
                                      rend_cache_failure_intro_t
                                      **intro_entry);
STATIC void rend_cache_decrement_allocation(size_t n);
STATIC void rend_cache_increment_allocation(size_t n);
STATIC rend_cache_failure_intro_t *rend_cache_failure_intro_entry_new(
                                      rend_intro_point_failure_t failure);
STATIC rend_cache_failure_t *rend_cache_failure_entry_new(void);
STATIC void rend_cache_failure_remove(rend_service_descriptor_t *desc);
STATIC void cache_failure_intro_add(const uint8_t *identity,
                                    const char *service_id,
                                    rend_intro_point_failure_t failure);
STATIC void validate_intro_point_failure(const rend_service_descriptor_t *desc,
                                        const char *service_id);

STATIC void rend_cache_failure_entry_free_(void *entry);
#endif

#endif /* TOR_RENDCACHE_H */

