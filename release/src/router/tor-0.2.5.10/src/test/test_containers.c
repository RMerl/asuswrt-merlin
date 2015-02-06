/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include "or.h"
#include "fp_pair.h"
#include "test.h"

/** Helper: return a tristate based on comparing the strings in *<b>a</b> and
 * *<b>b</b>. */
static int
compare_strs_(const void **a, const void **b)
{
  const char *s1 = *a, *s2 = *b;
  return strcmp(s1, s2);
}

/** Helper: return a tristate based on comparing the strings in <b>a</b> and
 * *<b>b</b>. */
static int
compare_strs_for_bsearch_(const void *a, const void **b)
{
  const char *s1 = a, *s2 = *b;
  return strcmp(s1, s2);
}

/** Helper: return a tristate based on comparing the strings in *<b>a</b> and
 * *<b>b</b>, excluding a's first character, and ignoring case. */
static int
compare_without_first_ch_(const void *a, const void **b)
{
  const char *s1 = a, *s2 = *b;
  return strcasecmp(s1+1, s2);
}

/** Run unit tests for basic dynamic-sized array functionality. */
static void
test_container_smartlist_basic(void)
{
  smartlist_t *sl;

  /* XXXX test sort_digests, uniq_strings, uniq_digests */

  /* Test smartlist add, del_keeporder, insert, get. */
  sl = smartlist_new();
  smartlist_add(sl, (void*)1);
  smartlist_add(sl, (void*)2);
  smartlist_add(sl, (void*)3);
  smartlist_add(sl, (void*)4);
  smartlist_del_keeporder(sl, 1);
  smartlist_insert(sl, 1, (void*)22);
  smartlist_insert(sl, 0, (void*)0);
  smartlist_insert(sl, 5, (void*)555);
  test_eq_ptr((void*)0,   smartlist_get(sl,0));
  test_eq_ptr((void*)1,   smartlist_get(sl,1));
  test_eq_ptr((void*)22,  smartlist_get(sl,2));
  test_eq_ptr((void*)3,   smartlist_get(sl,3));
  test_eq_ptr((void*)4,   smartlist_get(sl,4));
  test_eq_ptr((void*)555, smartlist_get(sl,5));
  /* Try deleting in the middle. */
  smartlist_del(sl, 1);
  test_eq_ptr((void*)555, smartlist_get(sl, 1));
  /* Try deleting at the end. */
  smartlist_del(sl, 4);
  test_eq(4, smartlist_len(sl));

  /* test isin. */
  test_assert(smartlist_contains(sl, (void*)3));
  test_assert(!smartlist_contains(sl, (void*)99));

 done:
  smartlist_free(sl);
}

/** Run unit tests for smartlist-of-strings functionality. */
static void
test_container_smartlist_strings(void)
{
  smartlist_t *sl = smartlist_new();
  char *cp=NULL, *cp_alloc=NULL;
  size_t sz;

  /* Test split and join */
  test_eq(0, smartlist_len(sl));
  smartlist_split_string(sl, "abc", ":", 0, 0);
  test_eq(1, smartlist_len(sl));
  test_streq("abc", smartlist_get(sl, 0));
  smartlist_split_string(sl, "a::bc::", "::", 0, 0);
  test_eq(4, smartlist_len(sl));
  test_streq("a", smartlist_get(sl, 1));
  test_streq("bc", smartlist_get(sl, 2));
  test_streq("", smartlist_get(sl, 3));
  cp_alloc = smartlist_join_strings(sl, "", 0, NULL);
  test_streq(cp_alloc, "abcabc");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "!", 0, NULL);
  test_streq(cp_alloc, "abc!a!bc!");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "XY", 0, NULL);
  test_streq(cp_alloc, "abcXYaXYbcXY");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "XY", 1, NULL);
  test_streq(cp_alloc, "abcXYaXYbcXYXY");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "", 1, NULL);
  test_streq(cp_alloc, "abcabc");
  tor_free(cp_alloc);

  smartlist_split_string(sl, "/def/  /ghijk", "/", 0, 0);
  test_eq(8, smartlist_len(sl));
  test_streq("", smartlist_get(sl, 4));
  test_streq("def", smartlist_get(sl, 5));
  test_streq("  ", smartlist_get(sl, 6));
  test_streq("ghijk", smartlist_get(sl, 7));
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  smartlist_split_string(sl, "a,bbd,cdef", ",", SPLIT_SKIP_SPACE, 0);
  test_eq(3, smartlist_len(sl));
  test_streq("a", smartlist_get(sl,0));
  test_streq("bbd", smartlist_get(sl,1));
  test_streq("cdef", smartlist_get(sl,2));
  smartlist_split_string(sl, " z <> zhasd <>  <> bnud<>   ", "<>",
                         SPLIT_SKIP_SPACE, 0);
  test_eq(8, smartlist_len(sl));
  test_streq("z", smartlist_get(sl,3));
  test_streq("zhasd", smartlist_get(sl,4));
  test_streq("", smartlist_get(sl,5));
  test_streq("bnud", smartlist_get(sl,6));
  test_streq("", smartlist_get(sl,7));

  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  smartlist_split_string(sl, " ab\tc \td ef  ", NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  test_eq(4, smartlist_len(sl));
  test_streq("ab", smartlist_get(sl,0));
  test_streq("c", smartlist_get(sl,1));
  test_streq("d", smartlist_get(sl,2));
  test_streq("ef", smartlist_get(sl,3));
  smartlist_split_string(sl, "ghi\tj", NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  test_eq(6, smartlist_len(sl));
  test_streq("ghi", smartlist_get(sl,4));
  test_streq("j", smartlist_get(sl,5));

  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  cp_alloc = smartlist_join_strings(sl, "XY", 0, NULL);
  test_streq(cp_alloc, "");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "XY", 1, NULL);
  test_streq(cp_alloc, "XY");
  tor_free(cp_alloc);

  smartlist_split_string(sl, " z <> zhasd <>  <> bnud<>   ", "<>",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  test_eq(3, smartlist_len(sl));
  test_streq("z", smartlist_get(sl, 0));
  test_streq("zhasd", smartlist_get(sl, 1));
  test_streq("bnud", smartlist_get(sl, 2));
  smartlist_split_string(sl, " z <> zhasd <>  <> bnud<>   ", "<>",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 2);
  test_eq(5, smartlist_len(sl));
  test_streq("z", smartlist_get(sl, 3));
  test_streq("zhasd <>  <> bnud<>", smartlist_get(sl, 4));
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  smartlist_split_string(sl, "abcd\n", "\n",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  test_eq(1, smartlist_len(sl));
  test_streq("abcd", smartlist_get(sl, 0));
  smartlist_split_string(sl, "efgh", "\n",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  test_eq(2, smartlist_len(sl));
  test_streq("efgh", smartlist_get(sl, 1));

  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Test swapping, shuffling, and sorting. */
  smartlist_split_string(sl, "the,onion,router,by,arma,and,nickm", ",", 0, 0);
  test_eq(7, smartlist_len(sl));
  smartlist_sort(sl, compare_strs_);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  test_streq(cp_alloc,"and,arma,by,nickm,onion,router,the");
  tor_free(cp_alloc);
  smartlist_swap(sl, 1, 5);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  test_streq(cp_alloc,"and,router,by,nickm,onion,arma,the");
  tor_free(cp_alloc);
  smartlist_shuffle(sl);
  test_eq(7, smartlist_len(sl));
  test_assert(smartlist_contains_string(sl, "and"));
  test_assert(smartlist_contains_string(sl, "router"));
  test_assert(smartlist_contains_string(sl, "by"));
  test_assert(smartlist_contains_string(sl, "nickm"));
  test_assert(smartlist_contains_string(sl, "onion"));
  test_assert(smartlist_contains_string(sl, "arma"));
  test_assert(smartlist_contains_string(sl, "the"));

  /* Test bsearch. */
  smartlist_sort(sl, compare_strs_);
  test_streq("nickm", smartlist_bsearch(sl, "zNicKM",
                                        compare_without_first_ch_));
  test_streq("and", smartlist_bsearch(sl, " AND", compare_without_first_ch_));
  test_eq_ptr(NULL, smartlist_bsearch(sl, " ANz", compare_without_first_ch_));

  /* Test bsearch_idx */
  {
    int f;
    smartlist_t *tmp = NULL;

    test_eq(0, smartlist_bsearch_idx(sl," aaa",compare_without_first_ch_,&f));
    test_eq(f, 0);
    test_eq(0, smartlist_bsearch_idx(sl," and",compare_without_first_ch_,&f));
    test_eq(f, 1);
    test_eq(1, smartlist_bsearch_idx(sl," arm",compare_without_first_ch_,&f));
    test_eq(f, 0);
    test_eq(1, smartlist_bsearch_idx(sl," arma",compare_without_first_ch_,&f));
    test_eq(f, 1);
    test_eq(2, smartlist_bsearch_idx(sl," armb",compare_without_first_ch_,&f));
    test_eq(f, 0);
    test_eq(7, smartlist_bsearch_idx(sl," zzzz",compare_without_first_ch_,&f));
    test_eq(f, 0);

    /* Test trivial cases for list of length 0 or 1 */
    tmp = smartlist_new();
    test_eq(0, smartlist_bsearch_idx(tmp, "foo",
                                     compare_strs_for_bsearch_, &f));
    test_eq(f, 0);
    smartlist_insert(tmp, 0, (void *)("bar"));
    test_eq(1, smartlist_bsearch_idx(tmp, "foo",
                                     compare_strs_for_bsearch_, &f));
    test_eq(f, 0);
    test_eq(0, smartlist_bsearch_idx(tmp, "aaa",
                                     compare_strs_for_bsearch_, &f));
    test_eq(f, 0);
    test_eq(0, smartlist_bsearch_idx(tmp, "bar",
                                     compare_strs_for_bsearch_, &f));
    test_eq(f, 1);
    /* ... and one for length 2 */
    smartlist_insert(tmp, 1, (void *)("foo"));
    test_eq(1, smartlist_bsearch_idx(tmp, "foo",
                                     compare_strs_for_bsearch_, &f));
    test_eq(f, 1);
    test_eq(2, smartlist_bsearch_idx(tmp, "goo",
                                     compare_strs_for_bsearch_, &f));
    test_eq(f, 0);
    smartlist_free(tmp);
  }

  /* Test reverse() and pop_last() */
  smartlist_reverse(sl);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  test_streq(cp_alloc,"the,router,onion,nickm,by,arma,and");
  tor_free(cp_alloc);
  cp_alloc = smartlist_pop_last(sl);
  test_streq(cp_alloc, "and");
  tor_free(cp_alloc);
  test_eq(smartlist_len(sl), 6);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);
  cp_alloc = smartlist_pop_last(sl);
  test_eq_ptr(cp_alloc, NULL);

  /* Test uniq() */
  smartlist_split_string(sl,
                     "50,noon,radar,a,man,a,plan,a,canal,panama,radar,noon,50",
                     ",", 0, 0);
  smartlist_sort(sl, compare_strs_);
  smartlist_uniq(sl, compare_strs_, tor_free_);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  test_streq(cp_alloc, "50,a,canal,man,noon,panama,plan,radar");
  tor_free(cp_alloc);

  /* Test contains_string, contains_string_case and contains_int_as_string */
  test_assert(smartlist_contains_string(sl, "noon"));
  test_assert(!smartlist_contains_string(sl, "noonoon"));
  test_assert(smartlist_contains_string_case(sl, "nOOn"));
  test_assert(!smartlist_contains_string_case(sl, "nooNooN"));
  test_assert(smartlist_contains_int_as_string(sl, 50));
  test_assert(!smartlist_contains_int_as_string(sl, 60));

  /* Test smartlist_choose */
  {
    int i;
    int allsame = 1;
    int allin = 1;
    void *first = smartlist_choose(sl);
    test_assert(smartlist_contains(sl, first));
    for (i = 0; i < 100; ++i) {
      void *second = smartlist_choose(sl);
      if (second != first)
        allsame = 0;
      if (!smartlist_contains(sl, second))
        allin = 0;
    }
    test_assert(!allsame);
    test_assert(allin);
  }
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Test string_remove and remove and join_strings2 */
  smartlist_split_string(sl,
                    "Some say the Earth will end in ice and some in fire",
                    " ", 0, 0);
  cp = smartlist_get(sl, 4);
  test_streq(cp, "will");
  smartlist_add(sl, cp);
  smartlist_remove(sl, cp);
  tor_free(cp);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  test_streq(cp_alloc, "Some,say,the,Earth,fire,end,in,ice,and,some,in");
  tor_free(cp_alloc);
  smartlist_string_remove(sl, "in");
  cp_alloc = smartlist_join_strings2(sl, "+XX", 1, 0, &sz);
  test_streq(cp_alloc, "Some+say+the+Earth+fire+end+some+ice+and");
  test_eq((int)sz, 40);

 done:

  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_free(sl);
  tor_free(cp_alloc);
}

/** Run unit tests for smartlist set manipulation functions. */
static void
test_container_smartlist_overlap(void)
{
  smartlist_t *sl = smartlist_new();
  smartlist_t *ints = smartlist_new();
  smartlist_t *odds = smartlist_new();
  smartlist_t *evens = smartlist_new();
  smartlist_t *primes = smartlist_new();
  int i;
  for (i=1; i < 10; i += 2)
    smartlist_add(odds, (void*)(uintptr_t)i);
  for (i=0; i < 10; i += 2)
    smartlist_add(evens, (void*)(uintptr_t)i);

  /* add_all */
  smartlist_add_all(ints, odds);
  smartlist_add_all(ints, evens);
  test_eq(smartlist_len(ints), 10);

  smartlist_add(primes, (void*)2);
  smartlist_add(primes, (void*)3);
  smartlist_add(primes, (void*)5);
  smartlist_add(primes, (void*)7);

  /* overlap */
  test_assert(smartlist_overlap(ints, odds));
  test_assert(smartlist_overlap(odds, primes));
  test_assert(smartlist_overlap(evens, primes));
  test_assert(!smartlist_overlap(odds, evens));

  /* intersect */
  smartlist_add_all(sl, odds);
  smartlist_intersect(sl, primes);
  test_eq(smartlist_len(sl), 3);
  test_assert(smartlist_contains(sl, (void*)3));
  test_assert(smartlist_contains(sl, (void*)5));
  test_assert(smartlist_contains(sl, (void*)7));

  /* subtract */
  smartlist_add_all(sl, primes);
  smartlist_subtract(sl, odds);
  test_eq(smartlist_len(sl), 1);
  test_assert(smartlist_contains(sl, (void*)2));

 done:
  smartlist_free(odds);
  smartlist_free(evens);
  smartlist_free(ints);
  smartlist_free(primes);
  smartlist_free(sl);
}

/** Run unit tests for smartlist-of-digests functions. */
static void
test_container_smartlist_digests(void)
{
  smartlist_t *sl = smartlist_new();

  /* contains_digest */
  smartlist_add(sl, tor_memdup("AAAAAAAAAAAAAAAAAAAA", DIGEST_LEN));
  smartlist_add(sl, tor_memdup("\00090AAB2AAAAaasdAAAAA", DIGEST_LEN));
  smartlist_add(sl, tor_memdup("\00090AAB2AAAAaasdAAAAA", DIGEST_LEN));
  test_eq(0, smartlist_contains_digest(NULL, "AAAAAAAAAAAAAAAAAAAA"));
  test_assert(smartlist_contains_digest(sl, "AAAAAAAAAAAAAAAAAAAA"));
  test_assert(smartlist_contains_digest(sl, "\00090AAB2AAAAaasdAAAAA"));
  test_eq(0, smartlist_contains_digest(sl, "\00090AAB2AAABaasdAAAAA"));

  /* sort digests */
  smartlist_sort_digests(sl);
  test_memeq(smartlist_get(sl, 0), "\00090AAB2AAAAaasdAAAAA", DIGEST_LEN);
  test_memeq(smartlist_get(sl, 1), "\00090AAB2AAAAaasdAAAAA", DIGEST_LEN);
  test_memeq(smartlist_get(sl, 2), "AAAAAAAAAAAAAAAAAAAA", DIGEST_LEN);
  test_eq(3, smartlist_len(sl));

  /* uniq_digests */
  smartlist_uniq_digests(sl);
  test_eq(2, smartlist_len(sl));
  test_memeq(smartlist_get(sl, 0), "\00090AAB2AAAAaasdAAAAA", DIGEST_LEN);
  test_memeq(smartlist_get(sl, 1), "AAAAAAAAAAAAAAAAAAAA", DIGEST_LEN);

 done:
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_free(sl);
}

/** Run unit tests for concatenate-a-smartlist-of-strings functions. */
static void
test_container_smartlist_join(void)
{
  smartlist_t *sl = smartlist_new();
  smartlist_t *sl2 = smartlist_new(), *sl3 = smartlist_new(),
    *sl4 = smartlist_new();
  char *joined=NULL;
  /* unique, sorted. */
  smartlist_split_string(sl,
                         "Abashments Ambush Anchorman Bacon Banks Borscht "
                         "Bunks Inhumane Insurance Knish Know Manners "
                         "Maraschinos Stamina Sunbonnets Unicorns Wombats",
                         " ", 0, 0);
  /* non-unique, sorted. */
  smartlist_split_string(sl2,
                         "Ambush Anchorman Anchorman Anemias Anemias Bacon "
                         "Crossbowmen Inhumane Insurance Knish Know Manners "
                         "Manners Maraschinos Wombats Wombats Work",
                         " ", 0, 0);
  SMARTLIST_FOREACH_JOIN(sl, char *, cp1,
                         sl2, char *, cp2,
                         strcmp(cp1,cp2),
                         smartlist_add(sl3, cp2)) {
    test_streq(cp1, cp2);
    smartlist_add(sl4, cp1);
  } SMARTLIST_FOREACH_JOIN_END(cp1, cp2);

  SMARTLIST_FOREACH(sl3, const char *, cp,
                    test_assert(smartlist_contains(sl2, cp) &&
                                !smartlist_contains_string(sl, cp)));
  SMARTLIST_FOREACH(sl4, const char *, cp,
                    test_assert(smartlist_contains(sl, cp) &&
                                smartlist_contains_string(sl2, cp)));
  joined = smartlist_join_strings(sl3, ",", 0, NULL);
  test_streq(joined, "Anemias,Anemias,Crossbowmen,Work");
  tor_free(joined);
  joined = smartlist_join_strings(sl4, ",", 0, NULL);
  test_streq(joined, "Ambush,Anchorman,Anchorman,Bacon,Inhumane,Insurance,"
             "Knish,Know,Manners,Manners,Maraschinos,Wombats,Wombats");
  tor_free(joined);

 done:
  smartlist_free(sl4);
  smartlist_free(sl3);
  SMARTLIST_FOREACH(sl2, char *, cp, tor_free(cp));
  smartlist_free(sl2);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_free(sl);
  tor_free(joined);
}

static void
test_container_smartlist_ints_eq(void *arg)
{
  smartlist_t *sl1 = NULL, *sl2 = NULL;
  int x;
  (void)arg;

  tt_assert(smartlist_ints_eq(NULL, NULL));

  sl1 = smartlist_new();
  tt_assert(!smartlist_ints_eq(sl1, NULL));
  tt_assert(!smartlist_ints_eq(NULL, sl1));

  sl2 = smartlist_new();
  tt_assert(smartlist_ints_eq(sl1, sl2));

  x = 5;
  smartlist_add(sl1, tor_memdup(&x, sizeof(int)));
  smartlist_add(sl2, tor_memdup(&x, sizeof(int)));
  x = 90;
  smartlist_add(sl1, tor_memdup(&x, sizeof(int)));
  smartlist_add(sl2, tor_memdup(&x, sizeof(int)));
  tt_assert(smartlist_ints_eq(sl1, sl2));

  x = -50;
  smartlist_add(sl1, tor_memdup(&x, sizeof(int)));
  tt_assert(! smartlist_ints_eq(sl1, sl2));
  tt_assert(! smartlist_ints_eq(sl2, sl1));
  smartlist_add(sl2, tor_memdup(&x, sizeof(int)));
  tt_assert(smartlist_ints_eq(sl1, sl2));

  *(int*)smartlist_get(sl1, 1) = 101010;
  tt_assert(! smartlist_ints_eq(sl2, sl1));
  *(int*)smartlist_get(sl2, 1) = 101010;
  tt_assert(smartlist_ints_eq(sl1, sl2));

 done:
  if (sl1)
    SMARTLIST_FOREACH(sl1, int *, ip, tor_free(ip));
  if (sl2)
    SMARTLIST_FOREACH(sl2, int *, ip, tor_free(ip));
  smartlist_free(sl1);
  smartlist_free(sl2);
}

/** Run unit tests for bitarray code */
static void
test_container_bitarray(void)
{
  bitarray_t *ba = NULL;
  int i, j, ok=1;

  ba = bitarray_init_zero(1);
  test_assert(ba);
  test_assert(! bitarray_is_set(ba, 0));
  bitarray_set(ba, 0);
  test_assert(bitarray_is_set(ba, 0));
  bitarray_clear(ba, 0);
  test_assert(! bitarray_is_set(ba, 0));
  bitarray_free(ba);

  ba = bitarray_init_zero(1023);
  for (i = 1; i < 64; ) {
    for (j = 0; j < 1023; ++j) {
      if (j % i)
        bitarray_set(ba, j);
      else
        bitarray_clear(ba, j);
    }
    for (j = 0; j < 1023; ++j) {
      if (!bool_eq(bitarray_is_set(ba, j), j%i))
        ok = 0;
    }
    test_assert(ok);
    if (i < 7)
      ++i;
    else if (i == 28)
      i = 32;
    else
      i += 7;
  }

 done:
  if (ba)
    bitarray_free(ba);
}

/** Run unit tests for digest set code (implemented as a hashtable or as a
 * bloom filter) */
static void
test_container_digestset(void)
{
  smartlist_t *included = smartlist_new();
  char d[DIGEST_LEN];
  int i;
  int ok = 1;
  int false_positives = 0;
  digestset_t *set = NULL;

  for (i = 0; i < 1000; ++i) {
    crypto_rand(d, DIGEST_LEN);
    smartlist_add(included, tor_memdup(d, DIGEST_LEN));
  }
  set = digestset_new(1000);
  SMARTLIST_FOREACH(included, const char *, cp,
                    if (digestset_contains(set, cp))
                      ok = 0);
  test_assert(ok);
  SMARTLIST_FOREACH(included, const char *, cp,
                    digestset_add(set, cp));
  SMARTLIST_FOREACH(included, const char *, cp,
                    if (!digestset_contains(set, cp))
                      ok = 0);
  test_assert(ok);
  for (i = 0; i < 1000; ++i) {
    crypto_rand(d, DIGEST_LEN);
    if (digestset_contains(set, d))
      ++false_positives;
  }
  test_assert(false_positives < 50); /* Should be far lower. */

 done:
  if (set)
    digestset_free(set);
  SMARTLIST_FOREACH(included, char *, cp, tor_free(cp));
  smartlist_free(included);
}

typedef struct pq_entry_t {
  const char *val;
  int idx;
} pq_entry_t;

/** Helper: return a tristate based on comparing two pq_entry_t values. */
static int
compare_strings_for_pqueue_(const void *p1, const void *p2)
{
  const pq_entry_t *e1=p1, *e2=p2;
  return strcmp(e1->val, e2->val);
}

/** Run unit tests for heap-based priority queue functions. */
static void
test_container_pqueue(void)
{
  smartlist_t *sl = smartlist_new();
  int (*cmp)(const void *, const void*);
  const int offset = STRUCT_OFFSET(pq_entry_t, idx);
#define ENTRY(s) pq_entry_t s = { #s, -1 }
  ENTRY(cows);
  ENTRY(zebras);
  ENTRY(fish);
  ENTRY(frogs);
  ENTRY(apples);
  ENTRY(squid);
  ENTRY(daschunds);
  ENTRY(eggplants);
  ENTRY(weissbier);
  ENTRY(lobsters);
  ENTRY(roquefort);
  ENTRY(chinchillas);
  ENTRY(fireflies);

#define OK() smartlist_pqueue_assert_ok(sl, cmp, offset)

  cmp = compare_strings_for_pqueue_;
  smartlist_pqueue_add(sl, cmp, offset, &cows);
  smartlist_pqueue_add(sl, cmp, offset, &zebras);
  smartlist_pqueue_add(sl, cmp, offset, &fish);
  smartlist_pqueue_add(sl, cmp, offset, &frogs);
  smartlist_pqueue_add(sl, cmp, offset, &apples);
  smartlist_pqueue_add(sl, cmp, offset, &squid);
  smartlist_pqueue_add(sl, cmp, offset, &daschunds);
  smartlist_pqueue_add(sl, cmp, offset, &eggplants);
  smartlist_pqueue_add(sl, cmp, offset, &weissbier);
  smartlist_pqueue_add(sl, cmp, offset, &lobsters);
  smartlist_pqueue_add(sl, cmp, offset, &roquefort);

  OK();

  test_eq(smartlist_len(sl), 11);
  test_eq_ptr(smartlist_get(sl, 0), &apples);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &apples);
  test_eq(smartlist_len(sl), 10);
  OK();
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &cows);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &daschunds);
  smartlist_pqueue_add(sl, cmp, offset, &chinchillas);
  OK();
  smartlist_pqueue_add(sl, cmp, offset, &fireflies);
  OK();
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &chinchillas);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &eggplants);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &fireflies);
  OK();
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &fish);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &frogs);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &lobsters);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &roquefort);
  OK();
  test_eq(smartlist_len(sl), 3);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &squid);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &weissbier);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &zebras);
  test_eq(smartlist_len(sl), 0);
  OK();

  /* Now test remove. */
  smartlist_pqueue_add(sl, cmp, offset, &cows);
  smartlist_pqueue_add(sl, cmp, offset, &fish);
  smartlist_pqueue_add(sl, cmp, offset, &frogs);
  smartlist_pqueue_add(sl, cmp, offset, &apples);
  smartlist_pqueue_add(sl, cmp, offset, &squid);
  smartlist_pqueue_add(sl, cmp, offset, &zebras);
  test_eq(smartlist_len(sl), 6);
  OK();
  smartlist_pqueue_remove(sl, cmp, offset, &zebras);
  test_eq(smartlist_len(sl), 5);
  OK();
  smartlist_pqueue_remove(sl, cmp, offset, &cows);
  test_eq(smartlist_len(sl), 4);
  OK();
  smartlist_pqueue_remove(sl, cmp, offset, &apples);
  test_eq(smartlist_len(sl), 3);
  OK();
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &fish);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &frogs);
  test_eq_ptr(smartlist_pqueue_pop(sl, cmp, offset), &squid);
  test_eq(smartlist_len(sl), 0);
  OK();

#undef OK

 done:

  smartlist_free(sl);
}

/** Run unit tests for string-to-void* map functions */
static void
test_container_strmap(void)
{
  strmap_t *map;
  strmap_iter_t *iter;
  const char *k;
  void *v;
  char *visited = NULL;
  smartlist_t *found_keys = NULL;

  map = strmap_new();
  test_assert(map);
  test_eq(strmap_size(map), 0);
  test_assert(strmap_isempty(map));
  v = strmap_set(map, "K1", (void*)99);
  test_eq_ptr(v, NULL);
  test_assert(!strmap_isempty(map));
  v = strmap_set(map, "K2", (void*)101);
  test_eq_ptr(v, NULL);
  v = strmap_set(map, "K1", (void*)100);
  test_eq_ptr(v, (void*)99);
  test_eq_ptr(strmap_get(map,"K1"), (void*)100);
  test_eq_ptr(strmap_get(map,"K2"), (void*)101);
  test_eq_ptr(strmap_get(map,"K-not-there"), NULL);
  strmap_assert_ok(map);

  v = strmap_remove(map,"K2");
  strmap_assert_ok(map);
  test_eq_ptr(v, (void*)101);
  test_eq_ptr(strmap_get(map,"K2"), NULL);
  test_eq_ptr(strmap_remove(map,"K2"), NULL);

  strmap_set(map, "K2", (void*)101);
  strmap_set(map, "K3", (void*)102);
  strmap_set(map, "K4", (void*)103);
  test_eq(strmap_size(map), 4);
  strmap_assert_ok(map);
  strmap_set(map, "K5", (void*)104);
  strmap_set(map, "K6", (void*)105);
  strmap_assert_ok(map);

  /* Test iterator. */
  iter = strmap_iter_init(map);
  found_keys = smartlist_new();
  while (!strmap_iter_done(iter)) {
    strmap_iter_get(iter,&k,&v);
    smartlist_add(found_keys, tor_strdup(k));
    test_eq_ptr(v, strmap_get(map, k));

    if (!strcmp(k, "K2")) {
      iter = strmap_iter_next_rmv(map,iter);
    } else {
      iter = strmap_iter_next(map,iter);
    }
  }

  /* Make sure we removed K2, but not the others. */
  test_eq_ptr(strmap_get(map, "K2"), NULL);
  test_eq_ptr(strmap_get(map, "K5"), (void*)104);
  /* Make sure we visited everyone once */
  smartlist_sort_strings(found_keys);
  visited = smartlist_join_strings(found_keys, ":", 0, NULL);
  test_streq(visited, "K1:K2:K3:K4:K5:K6");

  strmap_assert_ok(map);
  /* Clean up after ourselves. */
  strmap_free(map, NULL);
  map = NULL;

  /* Now try some lc functions. */
  map = strmap_new();
  strmap_set_lc(map,"Ab.C", (void*)1);
  test_eq_ptr(strmap_get(map,"ab.c"), (void*)1);
  strmap_assert_ok(map);
  test_eq_ptr(strmap_get_lc(map,"AB.C"), (void*)1);
  test_eq_ptr(strmap_get(map,"AB.C"), NULL);
  test_eq_ptr(strmap_remove_lc(map,"aB.C"), (void*)1);
  strmap_assert_ok(map);
  test_eq_ptr(strmap_get_lc(map,"AB.C"), NULL);

 done:
  if (map)
    strmap_free(map,NULL);
  if (found_keys) {
    SMARTLIST_FOREACH(found_keys, char *, cp, tor_free(cp));
    smartlist_free(found_keys);
  }
  tor_free(visited);
}

/** Run unit tests for getting the median of a list. */
static void
test_container_order_functions(void)
{
  int lst[25], n = 0;
  //  int a=12,b=24,c=25,d=60,e=77;

#define median() median_int(lst, n)

  lst[n++] = 12;
  test_eq(12, median()); /* 12 */
  lst[n++] = 77;
  //smartlist_shuffle(sl);
  test_eq(12, median()); /* 12, 77 */
  lst[n++] = 77;
  //smartlist_shuffle(sl);
  test_eq(77, median()); /* 12, 77, 77 */
  lst[n++] = 24;
  test_eq(24, median()); /* 12,24,77,77 */
  lst[n++] = 60;
  lst[n++] = 12;
  lst[n++] = 25;
  //smartlist_shuffle(sl);
  test_eq(25, median()); /* 12,12,24,25,60,77,77 */
#undef median

 done:
  ;
}

static void
test_container_di_map(void *arg)
{
  di_digest256_map_t *map = NULL;
  const uint8_t key1[] = "In view of the fact that it was ";
  const uint8_t key2[] = "superficially convincing, being ";
  const uint8_t key3[] = "properly enciphered in a one-tim";
  const uint8_t key4[] = "e cipher scheduled for use today";
  char *v1 = tor_strdup(", it came close to causing a disaster...");
  char *v2 = tor_strdup("I regret to have to advise you that the mission");
  char *v3 = tor_strdup("was actually initiated...");
  /* -- John Brunner, _The Shockwave Rider_ */

  (void)arg;

  /* Try searching on an empty map. */
  tt_ptr_op(NULL, ==, dimap_search(map, key1, NULL));
  tt_ptr_op(NULL, ==, dimap_search(map, key2, NULL));
  tt_ptr_op(v3, ==, dimap_search(map, key2, v3));
  dimap_free(map, NULL);
  map = NULL;

  /* Add a single entry. */
  dimap_add_entry(&map, key1, v1);
  tt_ptr_op(NULL, ==, dimap_search(map, key2, NULL));
  tt_ptr_op(v3, ==, dimap_search(map, key2, v3));
  tt_ptr_op(v1, ==, dimap_search(map, key1, NULL));

  /* Now try it with three entries in the map. */
  dimap_add_entry(&map, key2, v2);
  dimap_add_entry(&map, key3, v3);
  tt_ptr_op(v1, ==, dimap_search(map, key1, NULL));
  tt_ptr_op(v2, ==, dimap_search(map, key2, NULL));
  tt_ptr_op(v3, ==, dimap_search(map, key3, NULL));
  tt_ptr_op(NULL, ==, dimap_search(map, key4, NULL));
  tt_ptr_op(v1, ==, dimap_search(map, key4, v1));

 done:
  tor_free(v1);
  tor_free(v2);
  tor_free(v3);
  dimap_free(map, NULL);
}

/** Run unit tests for fp_pair-to-void* map functions */
static void
test_container_fp_pair_map(void)
{
  fp_pair_map_t *map;
  fp_pair_t fp1, fp2, fp3, fp4, fp5, fp6;
  void *v;
  fp_pair_map_iter_t *iter;
  fp_pair_t k;

  map = fp_pair_map_new();
  test_assert(map);
  test_eq(fp_pair_map_size(map), 0);
  test_assert(fp_pair_map_isempty(map));

  memset(fp1.first, 0x11, DIGEST_LEN);
  memset(fp1.second, 0x12, DIGEST_LEN);
  memset(fp2.first, 0x21, DIGEST_LEN);
  memset(fp2.second, 0x22, DIGEST_LEN);
  memset(fp3.first, 0x31, DIGEST_LEN);
  memset(fp3.second, 0x32, DIGEST_LEN);
  memset(fp4.first, 0x41, DIGEST_LEN);
  memset(fp4.second, 0x42, DIGEST_LEN);
  memset(fp5.first, 0x51, DIGEST_LEN);
  memset(fp5.second, 0x52, DIGEST_LEN);
  memset(fp6.first, 0x61, DIGEST_LEN);
  memset(fp6.second, 0x62, DIGEST_LEN);

  v = fp_pair_map_set(map, &fp1, (void*)99);
  tt_ptr_op(v, ==, NULL);
  test_assert(!fp_pair_map_isempty(map));
  v = fp_pair_map_set(map, &fp2, (void*)101);
  tt_ptr_op(v, ==, NULL);
  v = fp_pair_map_set(map, &fp1, (void*)100);
  tt_ptr_op(v, ==, (void*)99);
  test_eq_ptr(fp_pair_map_get(map, &fp1), (void*)100);
  test_eq_ptr(fp_pair_map_get(map, &fp2), (void*)101);
  test_eq_ptr(fp_pair_map_get(map, &fp3), NULL);
  fp_pair_map_assert_ok(map);

  v = fp_pair_map_remove(map, &fp2);
  fp_pair_map_assert_ok(map);
  test_eq_ptr(v, (void*)101);
  test_eq_ptr(fp_pair_map_get(map, &fp2), NULL);
  test_eq_ptr(fp_pair_map_remove(map, &fp2), NULL);

  fp_pair_map_set(map, &fp2, (void*)101);
  fp_pair_map_set(map, &fp3, (void*)102);
  fp_pair_map_set(map, &fp4, (void*)103);
  test_eq(fp_pair_map_size(map), 4);
  fp_pair_map_assert_ok(map);
  fp_pair_map_set(map, &fp5, (void*)104);
  fp_pair_map_set(map, &fp6, (void*)105);
  fp_pair_map_assert_ok(map);

  /* Test iterator. */
  iter = fp_pair_map_iter_init(map);
  while (!fp_pair_map_iter_done(iter)) {
    fp_pair_map_iter_get(iter, &k, &v);
    test_eq_ptr(v, fp_pair_map_get(map, &k));

    if (tor_memeq(&fp2, &k, sizeof(fp2))) {
      iter = fp_pair_map_iter_next_rmv(map, iter);
    } else {
      iter = fp_pair_map_iter_next(map, iter);
    }
  }

  /* Make sure we removed fp2, but not the others. */
  test_eq_ptr(fp_pair_map_get(map, &fp2), NULL);
  test_eq_ptr(fp_pair_map_get(map, &fp5), (void*)104);

  fp_pair_map_assert_ok(map);
  /* Clean up after ourselves. */
  fp_pair_map_free(map, NULL);
  map = NULL;

 done:
  if (map)
    fp_pair_map_free(map, NULL);
}

#define CONTAINER_LEGACY(name)                                          \
  { #name, legacy_test_helper, 0, &legacy_setup, test_container_ ## name }

#define CONTAINER(name, flags)                                          \
  { #name, test_container_ ## name, (flags), NULL, NULL }

struct testcase_t container_tests[] = {
  CONTAINER_LEGACY(smartlist_basic),
  CONTAINER_LEGACY(smartlist_strings),
  CONTAINER_LEGACY(smartlist_overlap),
  CONTAINER_LEGACY(smartlist_digests),
  CONTAINER_LEGACY(smartlist_join),
  CONTAINER(smartlist_ints_eq, 0),
  CONTAINER_LEGACY(bitarray),
  CONTAINER_LEGACY(digestset),
  CONTAINER_LEGACY(strmap),
  CONTAINER_LEGACY(pqueue),
  CONTAINER_LEGACY(order_functions),
  CONTAINER(di_map, 0),
  CONTAINER_LEGACY(fp_pair_map),
  END_OF_TESTCASES
};

