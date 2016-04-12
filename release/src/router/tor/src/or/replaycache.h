/* Copyright (c) 2012-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file replaycache.h
 * \brief Header file for replaycache.c.
 **/

#ifndef TOR_REPLAYCACHE_H
#define TOR_REPLAYCACHE_H

typedef struct replaycache_s replaycache_t;

#ifdef REPLAYCACHE_PRIVATE

struct replaycache_s {
  /* Scrub interval */
  time_t scrub_interval;
  /* Last scrubbed */
  time_t scrubbed;
  /*
   * Horizon
   * (don't return true on digests in the cache but older than this)
   */
  time_t horizon;
  /*
   * Digest map: keys are digests, values are times the digest was last seen
   */
  digestmap_t *digests_seen;
};

#endif /* REPLAYCACHE_PRIVATE */

/* replaycache_t free/new */

void replaycache_free(replaycache_t *r);
replaycache_t * replaycache_new(time_t horizon, time_t interval);

#ifdef REPLAYCACHE_PRIVATE

/*
 * replaycache_t internal functions:
 *
 * These take the time to treat as the present as an argument for easy unit
 * testing.  For everything else, use the wrappers below instead.
 */

STATIC int replaycache_add_and_test_internal(
    time_t present, replaycache_t *r, const void *data, size_t len,
    time_t *elapsed);
STATIC void replaycache_scrub_if_needed_internal(
    time_t present, replaycache_t *r);

#endif /* REPLAYCACHE_PRIVATE */

/*
 * replaycache_t methods
 */

int replaycache_add_and_test(replaycache_t *r, const void *data, size_t len);
int replaycache_add_test_and_elapsed(
    replaycache_t *r, const void *data, size_t len, time_t *elapsed);
void replaycache_scrub_if_needed(replaycache_t *r);

#endif

