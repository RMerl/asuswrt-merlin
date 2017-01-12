/* Copyright (c) 2011-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file di_ops.c
 * \brief Functions for data-independent operations.
 **/

#include "orconfig.h"
#include "di_ops.h"
#include "torlog.h"
#include "util.h"

/**
 * Timing-safe version of memcmp.  As memcmp, compare the <b>sz</b> bytes at
 * <b>a</b> with the <b>sz</b> bytes at <b>b</b>, and return less than 0 if
 * the bytes at <b>a</b> lexically precede those at <b>b</b>, 0 if the byte
 * ranges are equal, and greater than zero if the bytes at <b>a</b> lexically
 * follow those at <b>b</b>.
 *
 * This implementation differs from memcmp in that its timing behavior is not
 * data-dependent: it should return in the same amount of time regardless of
 * the contents of <b>a</b> and <b>b</b>.
 */
int
tor_memcmp(const void *a, const void *b, size_t len)
{
#ifdef HAVE_TIMINGSAFE_MEMCMP
  return timingsafe_memcmp(a, b, len);
#else
  const uint8_t *x = a;
  const uint8_t *y = b;
  size_t i = len;
  int retval = 0;

  /* This loop goes from the end of the arrays to the start.  At the
   * start of every iteration, before we decrement i, we have set
   * "retval" equal to the result of memcmp(a+i,b+i,len-i).  During the
   * loop, we update retval by leaving it unchanged if x[i]==y[i] and
   * setting it to x[i]-y[i] if x[i]!= y[i].
   *
   * The following assumes we are on a system with two's-complement
   * arithmetic.  We check for this at configure-time with the check
   * that sets USING_TWOS_COMPLEMENT.  If we aren't two's complement, then
   * torint.h will stop compilation with an error.
   */
  while (i--) {
    int v1 = x[i];
    int v2 = y[i];
    int equal_p = v1 ^ v2;

    /* The following sets bits 8 and above of equal_p to 'equal_p ==
     * 0', and thus to v1 == v2.  (To see this, note that if v1 ==
     * v2, then v1^v2 == equal_p == 0, so equal_p-1 == -1, which is the
     * same as ~0 on a two's-complement machine.  Then note that if
     * v1 != v2, then 0 < v1 ^ v2 < 256, so 0 <= equal_p - 1 < 255.)
     */
    --equal_p;

    equal_p >>= 8;
    /* Thanks to (sign-preserving) arithmetic shift, equal_p is now
     * equal to -(v1 == v2), which is exactly what we need below.
     * (Since we're assuming two's-complement arithmetic, -1 is the
     * same as ~0 (all bits set).)
     *
     * (The result of an arithmetic shift on a negative value is
     * actually implementation-defined in standard C.  So how do we
     * get away with assuming it?  Easy.  We check.) */
#if ((-60 >> 8) != -1)
#error "According to cpp, right-shift doesn't perform sign-extension."
#endif
#ifndef RSHIFT_DOES_SIGN_EXTEND
#error "According to configure, right-shift doesn't perform sign-extension."
#endif

    /* If v1 == v2, equal_p is ~0, so this will leave retval
     * unchanged; otherwise, equal_p is 0, so this will zero it. */
    retval &= equal_p;

    /* If v1 == v2, then this adds 0, and leaves retval unchanged.
     * Otherwise, we just zeroed retval, so this sets it to v1 - v2. */
    retval += (v1 - v2);

    /* There.  Now retval is equal to its previous value if v1 == v2, and
     * equal to v1 - v2 if v1 != v2. */
  }

  return retval;
#endif /* timingsafe_memcmp */
}

/**
 * Timing-safe memory comparison.  Return true if the <b>sz</b> bytes at
 * <b>a</b> are the same as the <b>sz</b> bytes at <b>b</b>, and 0 otherwise.
 *
 * This implementation differs from !memcmp(a,b,sz) in that its timing
 * behavior is not data-dependent: it should return in the same amount of time
 * regardless of the contents of <b>a</b> and <b>b</b>.  It differs from
 * !tor_memcmp(a,b,sz) by being faster.
 */
int
tor_memeq(const void *a, const void *b, size_t sz)
{
  /* Treat a and b as byte ranges. */
  const uint8_t *ba = a, *bb = b;
  uint32_t any_difference = 0;
  while (sz--) {
    /* Set byte_diff to all of those bits that are different in *ba and *bb,
     * and advance both ba and bb. */
    const uint8_t byte_diff = *ba++ ^ *bb++;

    /* Set bits in any_difference if they are set in byte_diff. */
    any_difference |= byte_diff;
  }

  /* Now any_difference is 0 if there are no bits different between
   * a and b, and is nonzero if there are bits different between a
   * and b.  Now for paranoia's sake, let's convert it to 0 or 1.
   *
   * (If we say "!any_difference", the compiler might get smart enough
   * to optimize-out our data-independence stuff above.)
   *
   * To unpack:
   *
   * If any_difference == 0:
   *            any_difference - 1 == ~0
   *     (any_difference - 1) >> 8 == 0x00ffffff
   *     1 & ((any_difference - 1) >> 8) == 1
   *
   * If any_difference != 0:
   *            0 < any_difference < 256, so
   *            0 <= any_difference - 1 < 255
   *            (any_difference - 1) >> 8 == 0
   *            1 & ((any_difference - 1) >> 8) == 0
   */

  /*coverity[overflow]*/
  return 1 & ((any_difference - 1) >> 8);
}

/* Implement di_digest256_map_t as a linked list of entries. */
struct di_digest256_map_t {
  struct di_digest256_map_t *next;
  uint8_t key[32];
  void *val;
};

/** Release all storage held in <b>map</b>, calling free_fn on each value
 * as we go. */
void
dimap_free(di_digest256_map_t *map, dimap_free_fn free_fn)
{
  while (map) {
    di_digest256_map_t *victim = map;
    map = map->next;
    if (free_fn)
      free_fn(victim->val);
    tor_free(victim);
  }
}

/** Adjust the map at *<b>map</b>, adding an entry for <b>key</b> ->
 * <b>val</b>, where <b>key</b> is a DIGEST256_LEN-byte key.
 *
 * The caller MUST NOT add a key that already appears in the map.
 */
void
dimap_add_entry(di_digest256_map_t **map,
                const uint8_t *key, void *val)
{
  di_digest256_map_t *new_ent;
  {
    void *old_val = dimap_search(*map, key, NULL);
    tor_assert(! old_val);
    tor_assert(val);
  }
  new_ent = tor_malloc_zero(sizeof(di_digest256_map_t));
  new_ent->next = *map;
  memcpy(new_ent->key, key, 32);
  new_ent->val = val;
  *map = new_ent;
}

/** Search the map at <b>map</b> for an entry whose key is <b>key</b> (a
 * DIGEST256_LEN-byte key) returning the corresponding value if we found one,
 * and returning <b>dflt_val</b> if the key wasn't found.
 *
 * This operation takes an amount of time dependent only on the length of
 * <b>map</b>, not on the position or presence of <b>key</b> within <b>map</b>.
 */
void *
dimap_search(const di_digest256_map_t *map, const uint8_t *key,
             void *dflt_val)
{
  uintptr_t result = (uintptr_t)dflt_val;

  while (map) {
    uintptr_t r = (uintptr_t) tor_memeq(map->key, key, 32);
    r -= 1; /* Now r is (uintptr_t)-1 if memeq returned false, and
             * 0 if memeq returned true. */

    result &= r;
    result |= ((uintptr_t)(map->val)) & ~r;

    map = map->next;
  }

  return (void *)result;
}

/**
 * Return true iff the <b>sz</b> bytes at <b>mem</b> are all zero. Runs in
 * time independent of the contents of <b>mem</b>.
 */
int
safe_mem_is_zero(const void *mem, size_t sz)
{
  uint32_t total = 0;
  const uint8_t *ptr = mem;

  while (sz--) {
    total |= *ptr++;
  }

  /*coverity[overflow]*/
  return 1 & ((total - 1) >> 8);
}

/** Time-invariant 64-bit greater-than; works on two integers in the range
 * (0,INT64_MAX). */
#if SIZEOF_VOID_P == 8
#define gt_i64_timei(a,b) ((a) > (b))
#else
static inline int
gt_i64_timei(uint64_t a, uint64_t b)
{
  int64_t diff = (int64_t) (b - a);
  int res = diff >> 63;
  return res & 1;
}
#endif

/**
 * Given an array of list of <b>n_entries</b> uint64_t values, whose sum is
 * <b>total</b>, find the first i such that the total of all elements 0...i is
 * greater than rand_val.
 *
 * Try to perform this operation in a constant-time way.
 */
int
select_array_member_cumulative_timei(const uint64_t *entries, int n_entries,
                                     uint64_t total, uint64_t rand_val)
{
  int i, i_chosen=-1, n_chosen=0;
  uint64_t total_so_far = 0;

  for (i = 0; i < n_entries; ++i) {
    total_so_far += entries[i];
    if (gt_i64_timei(total_so_far, rand_val)) {
      i_chosen = i;
      n_chosen++;
      /* Set rand_val to INT64_MAX rather than stopping the loop. This way,
       * the time we spend in the loop does not leak which element we chose. */
      rand_val = INT64_MAX;
    }
  }
  tor_assert(total_so_far == total);
  tor_assert(n_chosen == 1);
  tor_assert(i_chosen >= 0);
  tor_assert(i_chosen < n_entries);

  return i_chosen;
}

