/* Copyright (c) 2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "or.h"
#include "fp_pair.h"

/* Define fp_pair_map_t structures */

struct fp_pair_map_entry_s {
  HT_ENTRY(fp_pair_map_entry_s) node;
  void *val;
  fp_pair_t key;
};

struct fp_pair_map_s {
  HT_HEAD(fp_pair_map_impl, fp_pair_map_entry_s) head;
};

/*
 * Hash function and equality checker for fp_pair_map_t
 */

/** Compare fp_pair_entry_t objects by key value. */
static INLINE int
fp_pair_map_entries_eq(const fp_pair_map_entry_t *a,
                       const fp_pair_map_entry_t *b)
{
  return tor_memeq(&(a->key), &(b->key), sizeof(fp_pair_t));
}

/** Return a hash value for an fp_pair_entry_t. */
static INLINE unsigned int
fp_pair_map_entry_hash(const fp_pair_map_entry_t *a)
{
  tor_assert(sizeof(a->key) == DIGEST_LEN*2);
  return (unsigned) siphash24g(&a->key, DIGEST_LEN*2);
}

/*
 * Hash table functions for fp_pair_map_t
 */

HT_PROTOTYPE(fp_pair_map_impl, fp_pair_map_entry_s, node,
             fp_pair_map_entry_hash, fp_pair_map_entries_eq)
HT_GENERATE(fp_pair_map_impl, fp_pair_map_entry_s, node,
            fp_pair_map_entry_hash, fp_pair_map_entries_eq,
            0.6, tor_malloc, tor_realloc, tor_free)

/** Constructor to create a new empty map from fp_pair_t to void *
 */

fp_pair_map_t *
fp_pair_map_new(void)
{
  fp_pair_map_t *result;

  result = tor_malloc(sizeof(fp_pair_map_t));
  HT_INIT(fp_pair_map_impl, &result->head);
  return result;
}

/** Set the current value for key to val; returns the previous
 * value for key if one was set, or NULL if one was not.
 */

void *
fp_pair_map_set(fp_pair_map_t *map, const fp_pair_t *key, void *val)
{
  fp_pair_map_entry_t *resolve;
  fp_pair_map_entry_t search;
  void *oldval;

  tor_assert(map);
  tor_assert(key);
  tor_assert(val);

  memcpy(&(search.key), key, sizeof(*key));
  resolve = HT_FIND(fp_pair_map_impl, &(map->head), &search);
  if (resolve) {
    oldval = resolve->val;
    resolve->val = val;
  } else {
    resolve = tor_malloc_zero(sizeof(fp_pair_map_entry_t));
    memcpy(&(resolve->key), key, sizeof(*key));
    resolve->val = val;
    HT_INSERT(fp_pair_map_impl, &(map->head), resolve);
    oldval = NULL;
  }

  return oldval;
}

/** Set the current value for the key (first, second) to val; returns
 * the previous value for key if one was set, or NULL if one was not.
 */

void *
fp_pair_map_set_by_digests(fp_pair_map_t *map,
                           const char *first, const char *second,
                           void *val)
{
  fp_pair_t k;

  tor_assert(first);
  tor_assert(second);

  memcpy(k.first, first, DIGEST_LEN);
  memcpy(k.second, second, DIGEST_LEN);

  return fp_pair_map_set(map, &k, val);
}

/** Return the current value associated with key, or NULL if no value is set.
 */

void *
fp_pair_map_get(const fp_pair_map_t *map, const fp_pair_t *key)
{
  fp_pair_map_entry_t *resolve;
  fp_pair_map_entry_t search;
  void *val = NULL;

  tor_assert(map);
  tor_assert(key);

  memcpy(&(search.key), key, sizeof(*key));
  resolve = HT_FIND(fp_pair_map_impl, &(map->head), &search);
  if (resolve) val = resolve->val;

  return val;
}

/** Return the current value associated the key (first, second), or
 * NULL if no value is set.
 */

void *
fp_pair_map_get_by_digests(const fp_pair_map_t *map,
                           const char *first, const char *second)
{
  fp_pair_t k;

  tor_assert(first);
  tor_assert(second);

  memcpy(k.first, first, DIGEST_LEN);
  memcpy(k.second, second, DIGEST_LEN);

  return fp_pair_map_get(map, &k);
}

/** Remove the value currently associated with key from the map.
 * Return the value if one was set, or NULL if there was no entry for
 * key.  The caller must free any storage associated with the
 * returned value.
 */

void *
fp_pair_map_remove(fp_pair_map_t *map, const fp_pair_t *key)
{
  fp_pair_map_entry_t *resolve;
  fp_pair_map_entry_t search;
  void *val = NULL;

  tor_assert(map);
  tor_assert(key);

  memcpy(&(search.key), key, sizeof(*key));
  resolve = HT_REMOVE(fp_pair_map_impl, &(map->head), &search);
  if (resolve) {
    val = resolve->val;
    tor_free(resolve);
  }

  return val;
}

/** Remove all entries from map, and deallocate storage for those entries.
 * If free_val is provided, it is invoked on every value in map.
 */

void
fp_pair_map_free(fp_pair_map_t *map, void (*free_val)(void*))
{
  fp_pair_map_entry_t **ent, **next, *this;

  if (map) {
    for (ent = HT_START(fp_pair_map_impl, &(map->head));
         ent != NULL; ent = next) {
      this = *ent;
      next = HT_NEXT_RMV(fp_pair_map_impl, &(map->head), ent);
      if (free_val) free_val(this->val);
      tor_free(this);
    }
    tor_assert(HT_EMPTY(&(map->head)));
    HT_CLEAR(fp_pair_map_impl, &(map->head));
    tor_free(map);
  }
}

/** Return true iff map has no entries.
 */

int
fp_pair_map_isempty(const fp_pair_map_t *map)
{
  tor_assert(map);

  return HT_EMPTY(&(map->head));
}

/** Return the number of items in map.
 */

int
fp_pair_map_size(const fp_pair_map_t *map)
{
  tor_assert(map);

  return HT_SIZE(&(map->head));
}

/** return an iterator pointing to the start of map.
 */

fp_pair_map_iter_t *
fp_pair_map_iter_init(fp_pair_map_t *map)
{
  tor_assert(map);

  return HT_START(fp_pair_map_impl, &(map->head));
}

/** Advance iter a single step to the next entry of map, and return
 * its new value.
 */

fp_pair_map_iter_t *
fp_pair_map_iter_next(fp_pair_map_t *map, fp_pair_map_iter_t *iter)
{
  tor_assert(map);
  tor_assert(iter);

  return HT_NEXT(fp_pair_map_impl, &(map->head), iter);
}

/** Advance iter a single step to the next entry of map, removing the current
 * entry, and return its new value.
 */

fp_pair_map_iter_t *
fp_pair_map_iter_next_rmv(fp_pair_map_t *map, fp_pair_map_iter_t *iter)
{
  fp_pair_map_entry_t *rmv;

  tor_assert(map);
  tor_assert(iter);
  tor_assert(*iter);

  rmv = *iter;
  iter = HT_NEXT_RMV(fp_pair_map_impl, &(map->head), iter);
  tor_free(rmv);

  return iter;
}

/** Set *key_out and *val_out to the current entry pointed to by iter.
 */

void
fp_pair_map_iter_get(fp_pair_map_iter_t *iter,
                     fp_pair_t *key_out, void **val_out)
{
  tor_assert(iter);
  tor_assert(*iter);

  if (key_out) memcpy(key_out, &((*iter)->key), sizeof(fp_pair_t));
  if (val_out) *val_out = (*iter)->val;
}

/** Return true iff iter has advanced past the last entry of its map.
 */

int
fp_pair_map_iter_done(fp_pair_map_iter_t *iter)
{
  return (iter == NULL);
}

/** Assert if anything has gone wrong with the internal
 * representation of map.
 */

void
fp_pair_map_assert_ok(const fp_pair_map_t *map)
{
  tor_assert(!fp_pair_map_impl_HT_REP_IS_BAD_(&(map->head)));
}

