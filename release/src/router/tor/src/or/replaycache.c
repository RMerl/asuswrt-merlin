 /* Copyright (c) 2012-2015, The Tor Project, Inc. */
 /* See LICENSE for licensing information */

/*
 * \file replaycache.c
 *
 * \brief Self-scrubbing replay cache for rendservice.c
 */

#define REPLAYCACHE_PRIVATE

#include "or.h"
#include "replaycache.h"

/** Free the replaycache r and all of its entries.
 */

void
replaycache_free(replaycache_t *r)
{
  if (!r) {
    log_info(LD_BUG, "replaycache_free() called on NULL");
    return;
  }

  if (r->digests_seen) digestmap_free(r->digests_seen, tor_free_);

  tor_free(r);
}

/** Allocate a new, empty replay detection cache, where horizon is the time
 * for entries to age out and interval is the time after which the cache
 * should be scrubbed for old entries.
 */

replaycache_t *
replaycache_new(time_t horizon, time_t interval)
{
  replaycache_t *r = NULL;

  if (horizon < 0) {
    log_info(LD_BUG, "replaycache_new() called with negative"
        " horizon parameter");
    goto err;
  }

  if (interval < 0) {
    log_info(LD_BUG, "replaycache_new() called with negative interval"
        " parameter");
    interval = 0;
  }

  r = tor_malloc(sizeof(*r));
  r->scrub_interval = interval;
  r->scrubbed = 0;
  r->horizon = horizon;
  r->digests_seen = digestmap_new();

 err:
  return r;
}

/** See documentation for replaycache_add_and_test()
 */

STATIC int
replaycache_add_and_test_internal(
    time_t present, replaycache_t *r, const void *data, size_t len,
    time_t *elapsed)
{
  int rv = 0;
  char digest[DIGEST_LEN];
  time_t *access_time;

  /* sanity check */
  if (present <= 0 || !r || !data || len == 0) {
    log_info(LD_BUG, "replaycache_add_and_test_internal() called with stupid"
        " parameters; please fix this.");
    goto done;
  }

  /* compute digest */
  crypto_digest(digest, (const char *)data, len);

  /* check map */
  access_time = digestmap_get(r->digests_seen, digest);

  /* seen before? */
  if (access_time != NULL) {
    /*
     * If it's far enough in the past, no hit.  If the horizon is zero, we
     * never expire.
     */
    if (*access_time >= present - r->horizon || r->horizon == 0) {
      /* replay cache hit, return 1 */
      rv = 1;
      /* If we want to output an elapsed time, do so */
      if (elapsed) {
        if (present >= *access_time) {
          *elapsed = present - *access_time;
        } else {
          /* We shouldn't really be seeing hits from the future, but... */
          *elapsed = 0;
        }
      }
    }
    /*
     * If it's ahead of the cached time, update
     */
    if (*access_time < present) {
      *access_time = present;
    }
  } else {
    /* No, so no hit and update the digest map with the current time */
    access_time = tor_malloc(sizeof(*access_time));
    *access_time = present;
    digestmap_set(r->digests_seen, digest, access_time);
  }

  /* now scrub the cache if it's time */
  replaycache_scrub_if_needed_internal(present, r);

 done:
  return rv;
}

/** See documentation for replaycache_scrub_if_needed()
 */

STATIC void
replaycache_scrub_if_needed_internal(time_t present, replaycache_t *r)
{
  digestmap_iter_t *itr = NULL;
  const char *digest;
  void *valp;
  time_t *access_time;

  /* sanity check */
  if (!r || !(r->digests_seen)) {
    log_info(LD_BUG, "replaycache_scrub_if_needed_internal() called with"
        " stupid parameters; please fix this.");
    return;
  }

  /* scrub time yet? (scrubbed == 0 indicates never scrubbed before) */
  if (present - r->scrubbed < r->scrub_interval && r->scrubbed > 0) return;

  /* if we're never expiring, don't bother scrubbing */
  if (r->horizon == 0) return;

  /* okay, scrub time */
  itr = digestmap_iter_init(r->digests_seen);
  while (!digestmap_iter_done(itr)) {
    digestmap_iter_get(itr, &digest, &valp);
    access_time = (time_t *)valp;
    /* aged out yet? */
    if (*access_time < present - r->horizon) {
      /* Advance the iterator and remove this one */
      itr = digestmap_iter_next_rmv(r->digests_seen, itr);
      /* Free the value removed */
      tor_free(access_time);
    } else {
      /* Just advance the iterator */
      itr = digestmap_iter_next(r->digests_seen, itr);
    }
  }

  /* update scrubbed timestamp */
  if (present > r->scrubbed) r->scrubbed = present;
}

/** Test the buffer of length len point to by data against the replay cache r;
 * the digest of the buffer will be added to the cache at the current time,
 * and the function will return 1 if it was already seen within the cache's
 * horizon, or 0 otherwise.
 */

int
replaycache_add_and_test(replaycache_t *r, const void *data, size_t len)
{
  return replaycache_add_and_test_internal(time(NULL), r, data, len, NULL);
}

/** Like replaycache_add_and_test(), but if it's a hit also return the time
 * elapsed since this digest was last seen.
 */

int
replaycache_add_test_and_elapsed(
    replaycache_t *r, const void *data, size_t len, time_t *elapsed)
{
  return replaycache_add_and_test_internal(time(NULL), r, data, len, elapsed);
}

/** Scrub aged entries out of r if sufficiently long has elapsed since r was
 * last scrubbed.
 */

void
replaycache_scrub_if_needed(replaycache_t *r)
{
  replaycache_scrub_if_needed_internal(time(NULL), r);
}

