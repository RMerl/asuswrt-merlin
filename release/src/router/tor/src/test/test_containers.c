/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
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
cmp_without_first_(const void *a, const void **b)
{
  const char *s1 = a, *s2 = *b;
  return strcasecmp(s1+1, s2);
}

/** Run unit tests for basic dynamic-sized array functionality. */
static void
test_container_smartlist_basic(void *arg)
{
  smartlist_t *sl;
  char *v0 = tor_strdup("v0");
  char *v1 = tor_strdup("v1");
  char *v2 = tor_strdup("v2");
  char *v3 = tor_strdup("v3");
  char *v4 = tor_strdup("v4");
  char *v22 = tor_strdup("v22");
  char *v99 = tor_strdup("v99");
  char *v555 = tor_strdup("v555");

  /* XXXX test sort_digests, uniq_strings, uniq_digests */

  /* Test smartlist add, del_keeporder, insert, get. */
  (void)arg;
  sl = smartlist_new();
  smartlist_add(sl, v1);
  smartlist_add(sl, v2);
  smartlist_add(sl, v3);
  smartlist_add(sl, v4);
  smartlist_del_keeporder(sl, 1);
  smartlist_insert(sl, 1, v22);
  smartlist_insert(sl, 0, v0);
  smartlist_insert(sl, 5, v555);
  tt_ptr_op(v0,OP_EQ,   smartlist_get(sl,0));
  tt_ptr_op(v1,OP_EQ,   smartlist_get(sl,1));
  tt_ptr_op(v22,OP_EQ,  smartlist_get(sl,2));
  tt_ptr_op(v3,OP_EQ,   smartlist_get(sl,3));
  tt_ptr_op(v4,OP_EQ,   smartlist_get(sl,4));
  tt_ptr_op(v555,OP_EQ, smartlist_get(sl,5));
  /* Try deleting in the middle. */
  smartlist_del(sl, 1);
  tt_ptr_op(v555,OP_EQ, smartlist_get(sl, 1));
  /* Try deleting at the end. */
  smartlist_del(sl, 4);
  tt_int_op(4,OP_EQ, smartlist_len(sl));

  /* test isin. */
  tt_assert(smartlist_contains(sl, v3));
  tt_assert(!smartlist_contains(sl, v99));

 done:
  smartlist_free(sl);
  tor_free(v0);
  tor_free(v1);
  tor_free(v2);
  tor_free(v3);
  tor_free(v4);
  tor_free(v22);
  tor_free(v99);
  tor_free(v555);
}

/** Run unit tests for smartlist-of-strings functionality. */
static void
test_container_smartlist_strings(void *arg)
{
  smartlist_t *sl = smartlist_new();
  char *cp=NULL, *cp_alloc=NULL;
  size_t sz;

  /* Test split and join */
  (void)arg;
  tt_int_op(0,OP_EQ, smartlist_len(sl));
  smartlist_split_string(sl, "abc", ":", 0, 0);
  tt_int_op(1,OP_EQ, smartlist_len(sl));
  tt_str_op("abc",OP_EQ, smartlist_get(sl, 0));
  smartlist_split_string(sl, "a::bc::", "::", 0, 0);
  tt_int_op(4,OP_EQ, smartlist_len(sl));
  tt_str_op("a",OP_EQ, smartlist_get(sl, 1));
  tt_str_op("bc",OP_EQ, smartlist_get(sl, 2));
  tt_str_op("",OP_EQ, smartlist_get(sl, 3));
  cp_alloc = smartlist_join_strings(sl, "", 0, NULL);
  tt_str_op(cp_alloc,OP_EQ, "abcabc");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "!", 0, NULL);
  tt_str_op(cp_alloc,OP_EQ, "abc!a!bc!");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "XY", 0, NULL);
  tt_str_op(cp_alloc,OP_EQ, "abcXYaXYbcXY");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "XY", 1, NULL);
  tt_str_op(cp_alloc,OP_EQ, "abcXYaXYbcXYXY");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "", 1, NULL);
  tt_str_op(cp_alloc,OP_EQ, "abcabc");
  tor_free(cp_alloc);

  smartlist_split_string(sl, "/def/  /ghijk", "/", 0, 0);
  tt_int_op(8,OP_EQ, smartlist_len(sl));
  tt_str_op("",OP_EQ, smartlist_get(sl, 4));
  tt_str_op("def",OP_EQ, smartlist_get(sl, 5));
  tt_str_op("  ",OP_EQ, smartlist_get(sl, 6));
  tt_str_op("ghijk",OP_EQ, smartlist_get(sl, 7));
  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_clear(sl);

  smartlist_split_string(sl, "a,bbd,cdef", ",", SPLIT_SKIP_SPACE, 0);
  tt_int_op(3,OP_EQ, smartlist_len(sl));
  tt_str_op("a",OP_EQ, smartlist_get(sl,0));
  tt_str_op("bbd",OP_EQ, smartlist_get(sl,1));
  tt_str_op("cdef",OP_EQ, smartlist_get(sl,2));
  smartlist_split_string(sl, " z <> zhasd <>  <> bnud<>   ", "<>",
                         SPLIT_SKIP_SPACE, 0);
  tt_int_op(8,OP_EQ, smartlist_len(sl));
  tt_str_op("z",OP_EQ, smartlist_get(sl,3));
  tt_str_op("zhasd",OP_EQ, smartlist_get(sl,4));
  tt_str_op("",OP_EQ, smartlist_get(sl,5));
  tt_str_op("bnud",OP_EQ, smartlist_get(sl,6));
  tt_str_op("",OP_EQ, smartlist_get(sl,7));

  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_clear(sl);

  smartlist_split_string(sl, " ab\tc \td ef  ", NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(4,OP_EQ, smartlist_len(sl));
  tt_str_op("ab",OP_EQ, smartlist_get(sl,0));
  tt_str_op("c",OP_EQ, smartlist_get(sl,1));
  tt_str_op("d",OP_EQ, smartlist_get(sl,2));
  tt_str_op("ef",OP_EQ, smartlist_get(sl,3));
  smartlist_split_string(sl, "ghi\tj", NULL,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(6,OP_EQ, smartlist_len(sl));
  tt_str_op("ghi",OP_EQ, smartlist_get(sl,4));
  tt_str_op("j",OP_EQ, smartlist_get(sl,5));

  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_clear(sl);

  cp_alloc = smartlist_join_strings(sl, "XY", 0, NULL);
  tt_str_op(cp_alloc,OP_EQ, "");
  tor_free(cp_alloc);
  cp_alloc = smartlist_join_strings(sl, "XY", 1, NULL);
  tt_str_op(cp_alloc,OP_EQ, "XY");
  tor_free(cp_alloc);

  smartlist_split_string(sl, " z <> zhasd <>  <> bnud<>   ", "<>",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(3,OP_EQ, smartlist_len(sl));
  tt_str_op("z",OP_EQ, smartlist_get(sl, 0));
  tt_str_op("zhasd",OP_EQ, smartlist_get(sl, 1));
  tt_str_op("bnud",OP_EQ, smartlist_get(sl, 2));
  smartlist_split_string(sl, " z <> zhasd <>  <> bnud<>   ", "<>",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 2);
  tt_int_op(5,OP_EQ, smartlist_len(sl));
  tt_str_op("z",OP_EQ, smartlist_get(sl, 3));
  tt_str_op("zhasd <>  <> bnud<>",OP_EQ, smartlist_get(sl, 4));
  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_clear(sl);

  smartlist_split_string(sl, "abcd\n", "\n",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(1,OP_EQ, smartlist_len(sl));
  tt_str_op("abcd",OP_EQ, smartlist_get(sl, 0));
  smartlist_split_string(sl, "efgh", "\n",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  tt_int_op(2,OP_EQ, smartlist_len(sl));
  tt_str_op("efgh",OP_EQ, smartlist_get(sl, 1));

  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_clear(sl);

  /* Test swapping, shuffling, and sorting. */
  smartlist_split_string(sl, "the,onion,router,by,arma,and,nickm", ",", 0, 0);
  tt_int_op(7,OP_EQ, smartlist_len(sl));
  smartlist_sort(sl, compare_strs_);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc,OP_EQ, "and,arma,by,nickm,onion,router,the");
  tor_free(cp_alloc);
  smartlist_swap(sl, 1, 5);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc,OP_EQ, "and,router,by,nickm,onion,arma,the");
  tor_free(cp_alloc);
  smartlist_shuffle(sl);
  tt_int_op(7,OP_EQ, smartlist_len(sl));
  tt_assert(smartlist_contains_string(sl, "and"));
  tt_assert(smartlist_contains_string(sl, "router"));
  tt_assert(smartlist_contains_string(sl, "by"));
  tt_assert(smartlist_contains_string(sl, "nickm"));
  tt_assert(smartlist_contains_string(sl, "onion"));
  tt_assert(smartlist_contains_string(sl, "arma"));
  tt_assert(smartlist_contains_string(sl, "the"));

  /* Test bsearch. */
  smartlist_sort(sl, compare_strs_);
  tt_str_op("nickm",OP_EQ, smartlist_bsearch(sl, "zNicKM",
                                        cmp_without_first_));
  tt_str_op("and",OP_EQ,
            smartlist_bsearch(sl, " AND", cmp_without_first_));
  tt_ptr_op(NULL,OP_EQ, smartlist_bsearch(sl, " ANz", cmp_without_first_));

  /* Test bsearch_idx */
  {
    int f;
    smartlist_t *tmp = NULL;

    tt_int_op(0,OP_EQ,smartlist_bsearch_idx(sl," aaa",cmp_without_first_,&f));
    tt_int_op(f,OP_EQ, 0);
    tt_int_op(0,OP_EQ, smartlist_bsearch_idx(sl," and",cmp_without_first_,&f));
    tt_int_op(f,OP_EQ, 1);
    tt_int_op(1,OP_EQ, smartlist_bsearch_idx(sl," arm",cmp_without_first_,&f));
    tt_int_op(f,OP_EQ, 0);
    tt_int_op(1,OP_EQ,
              smartlist_bsearch_idx(sl," arma",cmp_without_first_,&f));
    tt_int_op(f,OP_EQ, 1);
    tt_int_op(2,OP_EQ,
              smartlist_bsearch_idx(sl," armb",cmp_without_first_,&f));
    tt_int_op(f,OP_EQ, 0);
    tt_int_op(7,OP_EQ,
              smartlist_bsearch_idx(sl," zzzz",cmp_without_first_,&f));
    tt_int_op(f,OP_EQ, 0);

    /* Test trivial cases for list of length 0 or 1 */
    tmp = smartlist_new();
    tt_int_op(0,OP_EQ, smartlist_bsearch_idx(tmp, "foo",
                                     compare_strs_for_bsearch_, &f));
    tt_int_op(f,OP_EQ, 0);
    smartlist_insert(tmp, 0, (void *)("bar"));
    tt_int_op(1,OP_EQ, smartlist_bsearch_idx(tmp, "foo",
                                     compare_strs_for_bsearch_, &f));
    tt_int_op(f,OP_EQ, 0);
    tt_int_op(0,OP_EQ, smartlist_bsearch_idx(tmp, "aaa",
                                     compare_strs_for_bsearch_, &f));
    tt_int_op(f,OP_EQ, 0);
    tt_int_op(0,OP_EQ, smartlist_bsearch_idx(tmp, "bar",
                                     compare_strs_for_bsearch_, &f));
    tt_int_op(f,OP_EQ, 1);
    /* ... and one for length 2 */
    smartlist_insert(tmp, 1, (void *)("foo"));
    tt_int_op(1,OP_EQ, smartlist_bsearch_idx(tmp, "foo",
                                     compare_strs_for_bsearch_, &f));
    tt_int_op(f,OP_EQ, 1);
    tt_int_op(2,OP_EQ, smartlist_bsearch_idx(tmp, "goo",
                                     compare_strs_for_bsearch_, &f));
    tt_int_op(f,OP_EQ, 0);
    smartlist_free(tmp);
  }

  /* Test reverse() and pop_last() */
  smartlist_reverse(sl);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc,OP_EQ, "the,router,onion,nickm,by,arma,and");
  tor_free(cp_alloc);
  cp_alloc = smartlist_pop_last(sl);
  tt_str_op(cp_alloc,OP_EQ, "and");
  tor_free(cp_alloc);
  tt_int_op(smartlist_len(sl),OP_EQ, 6);
  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_clear(sl);
  cp_alloc = smartlist_pop_last(sl);
  tt_ptr_op(cp_alloc,OP_EQ, NULL);

  /* Test uniq() */
  smartlist_split_string(sl,
                     "50,noon,radar,a,man,a,plan,a,canal,panama,radar,noon,50",
                     ",", 0, 0);
  smartlist_sort(sl, compare_strs_);
  smartlist_uniq(sl, compare_strs_, tor_free_);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc,OP_EQ, "50,a,canal,man,noon,panama,plan,radar");
  tor_free(cp_alloc);

  /* Test contains_string, contains_string_case and contains_int_as_string */
  tt_assert(smartlist_contains_string(sl, "noon"));
  tt_assert(!smartlist_contains_string(sl, "noonoon"));
  tt_assert(smartlist_contains_string_case(sl, "nOOn"));
  tt_assert(!smartlist_contains_string_case(sl, "nooNooN"));
  tt_assert(smartlist_contains_int_as_string(sl, 50));
  tt_assert(!smartlist_contains_int_as_string(sl, 60));

  /* Test smartlist_choose */
  {
    int i;
    int allsame = 1;
    int allin = 1;
    void *first = smartlist_choose(sl);
    tt_assert(smartlist_contains(sl, first));
    for (i = 0; i < 100; ++i) {
      void *second = smartlist_choose(sl);
      if (second != first)
        allsame = 0;
      if (!smartlist_contains(sl, second))
        allin = 0;
    }
    tt_assert(!allsame);
    tt_assert(allin);
  }
  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_clear(sl);

  /* Test string_remove and remove and join_strings2 */
  smartlist_split_string(sl,
                    "Some say the Earth will end in ice and some in fire",
                    " ", 0, 0);
  cp = smartlist_get(sl, 4);
  tt_str_op(cp,OP_EQ, "will");
  smartlist_add(sl, cp);
  smartlist_remove(sl, cp);
  tor_free(cp);
  cp_alloc = smartlist_join_strings(sl, ",", 0, NULL);
  tt_str_op(cp_alloc,OP_EQ, "Some,say,the,Earth,fire,end,in,ice,and,some,in");
  tor_free(cp_alloc);
  smartlist_string_remove(sl, "in");
  cp_alloc = smartlist_join_strings2(sl, "+XX", 1, 0, &sz);
  tt_str_op(cp_alloc,OP_EQ, "Some+say+the+Earth+fire+end+some+ice+and");
  tt_int_op((int)sz,OP_EQ, 40);

 done:

  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_free(sl);
  tor_free(cp_alloc);
}

/** Run unit tests for smartlist set manipulation functions. */
static void
test_container_smartlist_overlap(void *arg)
{
  smartlist_t *sl = smartlist_new();
  smartlist_t *ints = smartlist_new();
  smartlist_t *odds = smartlist_new();
  smartlist_t *evens = smartlist_new();
  smartlist_t *primes = smartlist_new();
  int i;
  (void)arg;
  for (i=1; i < 10; i += 2)
    smartlist_add(odds, (void*)(uintptr_t)i);
  for (i=0; i < 10; i += 2)
    smartlist_add(evens, (void*)(uintptr_t)i);

  /* add_all */
  smartlist_add_all(ints, odds);
  smartlist_add_all(ints, evens);
  tt_int_op(smartlist_len(ints),OP_EQ, 10);

  smartlist_add(primes, (void*)2);
  smartlist_add(primes, (void*)3);
  smartlist_add(primes, (void*)5);
  smartlist_add(primes, (void*)7);

  /* overlap */
  tt_assert(smartlist_overlap(ints, odds));
  tt_assert(smartlist_overlap(odds, primes));
  tt_assert(smartlist_overlap(evens, primes));
  tt_assert(!smartlist_overlap(odds, evens));

  /* intersect */
  smartlist_add_all(sl, odds);
  smartlist_intersect(sl, primes);
  tt_int_op(smartlist_len(sl),OP_EQ, 3);
  tt_assert(smartlist_contains(sl, (void*)3));
  tt_assert(smartlist_contains(sl, (void*)5));
  tt_assert(smartlist_contains(sl, (void*)7));

  /* subtract */
  smartlist_add_all(sl, primes);
  smartlist_subtract(sl, odds);
  tt_int_op(smartlist_len(sl),OP_EQ, 1);
  tt_assert(smartlist_contains(sl, (void*)2));

 done:
  smartlist_free(odds);
  smartlist_free(evens);
  smartlist_free(ints);
  smartlist_free(primes);
  smartlist_free(sl);
}

/** Run unit tests for smartlist-of-digests functions. */
static void
test_container_smartlist_digests(void *arg)
{
  smartlist_t *sl = smartlist_new();

  /* contains_digest */
  (void)arg;
  smartlist_add(sl, tor_memdup("AAAAAAAAAAAAAAAAAAAA", DIGEST_LEN));
  smartlist_add(sl, tor_memdup("\00090AAB2AAAAaasdAAAAA", DIGEST_LEN));
  smartlist_add(sl, tor_memdup("\00090AAB2AAAAaasdAAAAA", DIGEST_LEN));
  tt_int_op(0,OP_EQ, smartlist_contains_digest(NULL, "AAAAAAAAAAAAAAAAAAAA"));
  tt_assert(smartlist_contains_digest(sl, "AAAAAAAAAAAAAAAAAAAA"));
  tt_assert(smartlist_contains_digest(sl, "\00090AAB2AAAAaasdAAAAA"));
  tt_int_op(0,OP_EQ, smartlist_contains_digest(sl, "\00090AAB2AAABaasdAAAAA"));

  /* sort digests */
  smartlist_sort_digests(sl);
  tt_mem_op(smartlist_get(sl, 0),OP_EQ, "\00090AAB2AAAAaasdAAAAA", DIGEST_LEN);
  tt_mem_op(smartlist_get(sl, 1),OP_EQ, "\00090AAB2AAAAaasdAAAAA", DIGEST_LEN);
  tt_mem_op(smartlist_get(sl, 2),OP_EQ, "AAAAAAAAAAAAAAAAAAAA", DIGEST_LEN);
  tt_int_op(3,OP_EQ, smartlist_len(sl));

  /* uniq_digests */
  smartlist_uniq_digests(sl);
  tt_int_op(2,OP_EQ, smartlist_len(sl));
  tt_mem_op(smartlist_get(sl, 0),OP_EQ, "\00090AAB2AAAAaasdAAAAA", DIGEST_LEN);
  tt_mem_op(smartlist_get(sl, 1),OP_EQ, "AAAAAAAAAAAAAAAAAAAA", DIGEST_LEN);

 done:
  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_free(sl);
}

/** Run unit tests for concatenate-a-smartlist-of-strings functions. */
static void
test_container_smartlist_join(void *arg)
{
  smartlist_t *sl = smartlist_new();
  smartlist_t *sl2 = smartlist_new(), *sl3 = smartlist_new(),
    *sl4 = smartlist_new();
  char *joined=NULL;
  /* unique, sorted. */
  (void)arg;
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
    tt_str_op(cp1,OP_EQ, cp2);
    smartlist_add(sl4, cp1);
  } SMARTLIST_FOREACH_JOIN_END(cp1, cp2);

  SMARTLIST_FOREACH(sl3, const char *, cp,
                    tt_assert(smartlist_contains(sl2, cp) &&
                                !smartlist_contains_string(sl, cp)));
  SMARTLIST_FOREACH(sl4, const char *, cp,
                    tt_assert(smartlist_contains(sl, cp) &&
                                smartlist_contains_string(sl2, cp)));
  joined = smartlist_join_strings(sl3, ",", 0, NULL);
  tt_str_op(joined,OP_EQ, "Anemias,Anemias,Crossbowmen,Work");
  tor_free(joined);
  joined = smartlist_join_strings(sl4, ",", 0, NULL);
  tt_str_op(joined,OP_EQ,
            "Ambush,Anchorman,Anchorman,Bacon,Inhumane,Insurance,"
             "Knish,Know,Manners,Manners,Maraschinos,Wombats,Wombats");
  tor_free(joined);

 done:
  smartlist_free(sl4);
  smartlist_free(sl3);
  SMARTLIST_FOREACH(sl2, char *, cp, tor_free(cp));
  smartlist_free(sl2);
  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_free(sl);
  tor_free(joined);
}

static void
test_container_smartlist_pos(void *arg)
{
  (void) arg;
  smartlist_t *sl = smartlist_new();

  smartlist_add(sl, tor_strdup("This"));
  smartlist_add(sl, tor_strdup("is"));
  smartlist_add(sl, tor_strdup("a"));
  smartlist_add(sl, tor_strdup("test"));
  smartlist_add(sl, tor_strdup("for"));
  smartlist_add(sl, tor_strdup("a"));
  smartlist_add(sl, tor_strdup("function"));

  /* Test string_pos */
  tt_int_op(smartlist_string_pos(NULL, "Fred"), ==, -1);
  tt_int_op(smartlist_string_pos(sl, "Fred"), ==, -1);
  tt_int_op(smartlist_string_pos(sl, "This"), ==, 0);
  tt_int_op(smartlist_string_pos(sl, "a"), ==, 2);
  tt_int_op(smartlist_string_pos(sl, "function"), ==, 6);

  /* Test pos */
  tt_int_op(smartlist_pos(NULL, "Fred"), ==, -1);
  tt_int_op(smartlist_pos(sl, "Fred"), ==, -1);
  tt_int_op(smartlist_pos(sl, "This"), ==, -1);
  tt_int_op(smartlist_pos(sl, "a"), ==, -1);
  tt_int_op(smartlist_pos(sl, "function"), ==, -1);
  tt_int_op(smartlist_pos(sl, smartlist_get(sl,0)), ==, 0);
  tt_int_op(smartlist_pos(sl, smartlist_get(sl,2)), ==, 2);
  tt_int_op(smartlist_pos(sl, smartlist_get(sl,5)), ==, 5);
  tt_int_op(smartlist_pos(sl, smartlist_get(sl,6)), ==, 6);

 done:
  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_free(sl);
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
test_container_bitarray(void *arg)
{
  bitarray_t *ba = NULL;
  int i, j, ok=1;

  (void)arg;
  ba = bitarray_init_zero(1);
  tt_assert(ba);
  tt_assert(! bitarray_is_set(ba, 0));
  bitarray_set(ba, 0);
  tt_assert(bitarray_is_set(ba, 0));
  bitarray_clear(ba, 0);
  tt_assert(! bitarray_is_set(ba, 0));
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
    tt_assert(ok);
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
test_container_digestset(void *arg)
{
  smartlist_t *included = smartlist_new();
  char d[DIGEST_LEN];
  int i;
  int ok = 1;
  int false_positives = 0;
  digestset_t *set = NULL;

  (void)arg;
  for (i = 0; i < 1000; ++i) {
    crypto_rand(d, DIGEST_LEN);
    smartlist_add(included, tor_memdup(d, DIGEST_LEN));
  }
  set = digestset_new(1000);
  SMARTLIST_FOREACH(included, const char *, cp,
                    if (digestset_contains(set, cp))
                      ok = 0);
  tt_assert(ok);
  SMARTLIST_FOREACH(included, const char *, cp,
                    digestset_add(set, cp));
  SMARTLIST_FOREACH(included, const char *, cp,
                    if (!digestset_contains(set, cp))
                      ok = 0);
  tt_assert(ok);
  for (i = 0; i < 1000; ++i) {
    crypto_rand(d, DIGEST_LEN);
    if (digestset_contains(set, d))
      ++false_positives;
  }
  tt_int_op(50, OP_GT, false_positives); /* Should be far lower. */

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
test_container_pqueue(void *arg)
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

  (void)arg;

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

  tt_int_op(smartlist_len(sl),OP_EQ, 11);
  tt_ptr_op(smartlist_get(sl, 0),OP_EQ, &apples);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &apples);
  tt_int_op(smartlist_len(sl),OP_EQ, 10);
  OK();
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &cows);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &daschunds);
  smartlist_pqueue_add(sl, cmp, offset, &chinchillas);
  OK();
  smartlist_pqueue_add(sl, cmp, offset, &fireflies);
  OK();
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &chinchillas);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &eggplants);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &fireflies);
  OK();
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &fish);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &frogs);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &lobsters);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &roquefort);
  OK();
  tt_int_op(smartlist_len(sl),OP_EQ, 3);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &squid);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &weissbier);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &zebras);
  tt_int_op(smartlist_len(sl),OP_EQ, 0);
  OK();

  /* Now test remove. */
  smartlist_pqueue_add(sl, cmp, offset, &cows);
  smartlist_pqueue_add(sl, cmp, offset, &fish);
  smartlist_pqueue_add(sl, cmp, offset, &frogs);
  smartlist_pqueue_add(sl, cmp, offset, &apples);
  smartlist_pqueue_add(sl, cmp, offset, &squid);
  smartlist_pqueue_add(sl, cmp, offset, &zebras);
  tt_int_op(smartlist_len(sl),OP_EQ, 6);
  OK();
  smartlist_pqueue_remove(sl, cmp, offset, &zebras);
  tt_int_op(smartlist_len(sl),OP_EQ, 5);
  OK();
  smartlist_pqueue_remove(sl, cmp, offset, &cows);
  tt_int_op(smartlist_len(sl),OP_EQ, 4);
  OK();
  smartlist_pqueue_remove(sl, cmp, offset, &apples);
  tt_int_op(smartlist_len(sl),OP_EQ, 3);
  OK();
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &fish);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &frogs);
  tt_ptr_op(smartlist_pqueue_pop(sl, cmp, offset),OP_EQ, &squid);
  tt_int_op(smartlist_len(sl),OP_EQ, 0);
  OK();

#undef OK

 done:

  smartlist_free(sl);
}

/** Run unit tests for string-to-void* map functions */
static void
test_container_strmap(void *arg)
{
  strmap_t *map;
  strmap_iter_t *iter;
  const char *k;
  void *v;
  char *visited = NULL;
  smartlist_t *found_keys = NULL;
  char *v1 = tor_strdup("v1");
  char *v99 = tor_strdup("v99");
  char *v100 = tor_strdup("v100");
  char *v101 = tor_strdup("v101");
  char *v102 = tor_strdup("v102");
  char *v103 = tor_strdup("v103");
  char *v104 = tor_strdup("v104");
  char *v105 = tor_strdup("v105");

  (void)arg;
  map = strmap_new();
  tt_assert(map);
  tt_int_op(strmap_size(map),OP_EQ, 0);
  tt_assert(strmap_isempty(map));
  v = strmap_set(map, "K1", v99);
  tt_ptr_op(v,OP_EQ, NULL);
  tt_assert(!strmap_isempty(map));
  v = strmap_set(map, "K2", v101);
  tt_ptr_op(v,OP_EQ, NULL);
  v = strmap_set(map, "K1", v100);
  tt_ptr_op(v,OP_EQ, v99);
  tt_ptr_op(strmap_get(map,"K1"),OP_EQ, v100);
  tt_ptr_op(strmap_get(map,"K2"),OP_EQ, v101);
  tt_ptr_op(strmap_get(map,"K-not-there"),OP_EQ, NULL);
  strmap_assert_ok(map);

  v = strmap_remove(map,"K2");
  strmap_assert_ok(map);
  tt_ptr_op(v,OP_EQ, v101);
  tt_ptr_op(strmap_get(map,"K2"),OP_EQ, NULL);
  tt_ptr_op(strmap_remove(map,"K2"),OP_EQ, NULL);

  strmap_set(map, "K2", v101);
  strmap_set(map, "K3", v102);
  strmap_set(map, "K4", v103);
  tt_int_op(strmap_size(map),OP_EQ, 4);
  strmap_assert_ok(map);
  strmap_set(map, "K5", v104);
  strmap_set(map, "K6", v105);
  strmap_assert_ok(map);

  /* Test iterator. */
  iter = strmap_iter_init(map);
  found_keys = smartlist_new();
  while (!strmap_iter_done(iter)) {
    strmap_iter_get(iter,&k,&v);
    smartlist_add(found_keys, tor_strdup(k));
    tt_ptr_op(v,OP_EQ, strmap_get(map, k));

    if (!strcmp(k, "K2")) {
      iter = strmap_iter_next_rmv(map,iter);
    } else {
      iter = strmap_iter_next(map,iter);
    }
  }

  /* Make sure we removed K2, but not the others. */
  tt_ptr_op(strmap_get(map, "K2"),OP_EQ, NULL);
  tt_ptr_op(strmap_get(map, "K5"),OP_EQ, v104);
  /* Make sure we visited everyone once */
  smartlist_sort_strings(found_keys);
  visited = smartlist_join_strings(found_keys, ":", 0, NULL);
  tt_str_op(visited,OP_EQ, "K1:K2:K3:K4:K5:K6");

  strmap_assert_ok(map);
  /* Clean up after ourselves. */
  strmap_free(map, NULL);
  map = NULL;

  /* Now try some lc functions. */
  map = strmap_new();
  strmap_set_lc(map,"Ab.C", v1);
  tt_ptr_op(strmap_get(map,"ab.c"),OP_EQ, v1);
  strmap_assert_ok(map);
  tt_ptr_op(strmap_get_lc(map,"AB.C"),OP_EQ, v1);
  tt_ptr_op(strmap_get(map,"AB.C"),OP_EQ, NULL);
  tt_ptr_op(strmap_remove_lc(map,"aB.C"),OP_EQ, v1);
  strmap_assert_ok(map);
  tt_ptr_op(strmap_get_lc(map,"AB.C"),OP_EQ, NULL);

 done:
  if (map)
    strmap_free(map,NULL);
  if (found_keys) {
    SMARTLIST_FOREACH(found_keys, char *, cp, tor_free(cp));
    smartlist_free(found_keys);
  }
  tor_free(visited);
  tor_free(v1);
  tor_free(v99);
  tor_free(v100);
  tor_free(v101);
  tor_free(v102);
  tor_free(v103);
  tor_free(v104);
  tor_free(v105);
}

/** Run unit tests for getting the median of a list. */
static void
test_container_order_functions(void *arg)
{
  int lst[25], n = 0;
  uint32_t lst_2[25];
  //  int a=12,b=24,c=25,d=60,e=77;

#define median() median_int(lst, n)

  (void)arg;
  lst[n++] = 12;
  tt_int_op(12,OP_EQ, median()); /* 12 */
  lst[n++] = 77;
  //smartlist_shuffle(sl);
  tt_int_op(12,OP_EQ, median()); /* 12, 77 */
  lst[n++] = 77;
  //smartlist_shuffle(sl);
  tt_int_op(77,OP_EQ, median()); /* 12, 77, 77 */
  lst[n++] = 24;
  tt_int_op(24,OP_EQ, median()); /* 12,24,77,77 */
  lst[n++] = 60;
  lst[n++] = 12;
  lst[n++] = 25;
  //smartlist_shuffle(sl);
  tt_int_op(25,OP_EQ, median()); /* 12,12,24,25,60,77,77 */
#undef median

#define third_quartile() third_quartile_uint32(lst_2, n)

  n = 0;
  lst_2[n++] = 1;
  tt_int_op(1,OP_EQ, third_quartile()); /* ~1~ */
  lst_2[n++] = 2;
  tt_int_op(2,OP_EQ, third_quartile()); /* 1, ~2~ */
  lst_2[n++] = 3;
  lst_2[n++] = 4;
  lst_2[n++] = 5;
  tt_int_op(4,OP_EQ, third_quartile()); /* 1, 2, 3, ~4~, 5 */
  lst_2[n++] = 6;
  lst_2[n++] = 7;
  lst_2[n++] = 8;
  lst_2[n++] = 9;
  tt_int_op(7,OP_EQ, third_quartile()); /* 1, 2, 3, 4, 5, 6, ~7~, 8, 9 */
  lst_2[n++] = 10;
  lst_2[n++] = 11;
  /* 1, 2, 3, 4, 5, 6, 7, 8, ~9~, 10, 11 */
  tt_int_op(9,OP_EQ, third_quartile());

#undef third_quartile

  double dbls[] = { 1.0, 10.0, 100.0, 1e4, 1e5, 1e6 };
  tt_double_eq(1.0, median_double(dbls, 1));
  tt_double_eq(1.0, median_double(dbls, 2));
  tt_double_eq(10.0, median_double(dbls, 3));
  tt_double_eq(10.0, median_double(dbls, 4));
  tt_double_eq(100.0, median_double(dbls, 5));
  tt_double_eq(100.0, median_double(dbls, 6));

  time_t times[] = { 5, 10, 20, 25, 15 };

  tt_assert(5 == median_time(times, 1));
  tt_assert(5 == median_time(times, 2));
  tt_assert(10 == median_time(times, 3));
  tt_assert(10 == median_time(times, 4));
  tt_assert(15 == median_time(times, 5));

  int32_t int32s[] = { -5, -10, -50, 100 };
  tt_int_op(-5, ==, median_int32(int32s, 1));
  tt_int_op(-10, ==, median_int32(int32s, 2));
  tt_int_op(-10, ==, median_int32(int32s, 3));
  tt_int_op(-10, ==, median_int32(int32s, 4));

  long longs[] = { -30, 30, 100, -100, 7 };
  tt_int_op(7, ==, find_nth_long(longs, 5, 2));

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
  tt_ptr_op(NULL, OP_EQ, dimap_search(map, key1, NULL));
  tt_ptr_op(NULL, OP_EQ, dimap_search(map, key2, NULL));
  tt_ptr_op(v3, OP_EQ, dimap_search(map, key2, v3));
  dimap_free(map, NULL);
  map = NULL;

  /* Add a single entry. */
  dimap_add_entry(&map, key1, v1);
  tt_ptr_op(NULL, OP_EQ, dimap_search(map, key2, NULL));
  tt_ptr_op(v3, OP_EQ, dimap_search(map, key2, v3));
  tt_ptr_op(v1, OP_EQ, dimap_search(map, key1, NULL));

  /* Now try it with three entries in the map. */
  dimap_add_entry(&map, key2, v2);
  dimap_add_entry(&map, key3, v3);
  tt_ptr_op(v1, OP_EQ, dimap_search(map, key1, NULL));
  tt_ptr_op(v2, OP_EQ, dimap_search(map, key2, NULL));
  tt_ptr_op(v3, OP_EQ, dimap_search(map, key3, NULL));
  tt_ptr_op(NULL, OP_EQ, dimap_search(map, key4, NULL));
  tt_ptr_op(v1, OP_EQ, dimap_search(map, key4, v1));

 done:
  tor_free(v1);
  tor_free(v2);
  tor_free(v3);
  dimap_free(map, NULL);
}

/** Run unit tests for fp_pair-to-void* map functions */
static void
test_container_fp_pair_map(void *arg)
{
  fp_pair_map_t *map;
  fp_pair_t fp1, fp2, fp3, fp4, fp5, fp6;
  void *v;
  fp_pair_map_iter_t *iter;
  fp_pair_t k;
  char *v99 = tor_strdup("99");
  char *v100 = tor_strdup("v100");
  char *v101 = tor_strdup("v101");
  char *v102 = tor_strdup("v102");
  char *v103 = tor_strdup("v103");
  char *v104 = tor_strdup("v104");
  char *v105 = tor_strdup("v105");

  (void)arg;
  map = fp_pair_map_new();
  tt_assert(map);
  tt_int_op(fp_pair_map_size(map),OP_EQ, 0);
  tt_assert(fp_pair_map_isempty(map));

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

  v = fp_pair_map_set(map, &fp1, v99);
  tt_ptr_op(v, OP_EQ, NULL);
  tt_assert(!fp_pair_map_isempty(map));
  v = fp_pair_map_set(map, &fp2, v101);
  tt_ptr_op(v, OP_EQ, NULL);
  v = fp_pair_map_set(map, &fp1, v100);
  tt_ptr_op(v, OP_EQ, v99);
  tt_ptr_op(fp_pair_map_get(map, &fp1),OP_EQ, v100);
  tt_ptr_op(fp_pair_map_get(map, &fp2),OP_EQ, v101);
  tt_ptr_op(fp_pair_map_get(map, &fp3),OP_EQ, NULL);
  fp_pair_map_assert_ok(map);

  v = fp_pair_map_remove(map, &fp2);
  fp_pair_map_assert_ok(map);
  tt_ptr_op(v,OP_EQ, v101);
  tt_ptr_op(fp_pair_map_get(map, &fp2),OP_EQ, NULL);
  tt_ptr_op(fp_pair_map_remove(map, &fp2),OP_EQ, NULL);

  fp_pair_map_set(map, &fp2, v101);
  fp_pair_map_set(map, &fp3, v102);
  fp_pair_map_set(map, &fp4, v103);
  tt_int_op(fp_pair_map_size(map),OP_EQ, 4);
  fp_pair_map_assert_ok(map);
  fp_pair_map_set(map, &fp5, v104);
  fp_pair_map_set(map, &fp6, v105);
  fp_pair_map_assert_ok(map);

  /* Test iterator. */
  iter = fp_pair_map_iter_init(map);
  while (!fp_pair_map_iter_done(iter)) {
    fp_pair_map_iter_get(iter, &k, &v);
    tt_ptr_op(v,OP_EQ, fp_pair_map_get(map, &k));

    if (tor_memeq(&fp2, &k, sizeof(fp2))) {
      iter = fp_pair_map_iter_next_rmv(map, iter);
    } else {
      iter = fp_pair_map_iter_next(map, iter);
    }
  }

  /* Make sure we removed fp2, but not the others. */
  tt_ptr_op(fp_pair_map_get(map, &fp2),OP_EQ, NULL);
  tt_ptr_op(fp_pair_map_get(map, &fp5),OP_EQ, v104);

  fp_pair_map_assert_ok(map);
  /* Clean up after ourselves. */
  fp_pair_map_free(map, NULL);
  map = NULL;

 done:
  if (map)
    fp_pair_map_free(map, NULL);
  tor_free(v99);
  tor_free(v100);
  tor_free(v101);
  tor_free(v102);
  tor_free(v103);
  tor_free(v104);
  tor_free(v105);
}

static void
test_container_smartlist_most_frequent(void *arg)
{
  (void) arg;
  smartlist_t *sl = smartlist_new();

  int count = -1;
  const char *cp;

  cp = smartlist_get_most_frequent_string_(sl, &count);
  tt_int_op(count, ==, 0);
  tt_ptr_op(cp, ==, NULL);

  /* String must be sorted before we call get_most_frequent */
  smartlist_split_string(sl, "abc:def:ghi", ":", 0, 0);

  cp = smartlist_get_most_frequent_string_(sl, &count);
  tt_int_op(count, ==, 1);
  tt_str_op(cp, ==, "ghi"); /* Ties broken in favor of later element */

  smartlist_split_string(sl, "def:ghi", ":", 0, 0);
  smartlist_sort_strings(sl);

  cp = smartlist_get_most_frequent_string_(sl, &count);
  tt_int_op(count, ==, 2);
  tt_ptr_op(cp, !=, NULL);
  tt_str_op(cp, ==, "ghi"); /* Ties broken in favor of later element */

  smartlist_split_string(sl, "def:abc:qwop", ":", 0, 0);
  smartlist_sort_strings(sl);

  cp = smartlist_get_most_frequent_string_(sl, &count);
  tt_int_op(count, ==, 3);
  tt_ptr_op(cp, !=, NULL);
  tt_str_op(cp, ==, "def"); /* No tie */

 done:
  SMARTLIST_FOREACH(sl, char *, str, tor_free(str));
  smartlist_free(sl);
}

static void
test_container_smartlist_sort_ptrs(void *arg)
{
  (void)arg;
  int array[10];
  int *arrayptrs[11];
  smartlist_t *sl = smartlist_new();
  unsigned i=0, j;

  for (j = 0; j < ARRAY_LENGTH(array); ++j) {
    smartlist_add(sl, &array[j]);
    arrayptrs[i++] = &array[j];
    if (j == 5) {
      smartlist_add(sl, &array[j]);
      arrayptrs[i++] = &array[j];
    }
  }

  for (i = 0; i < 10; ++i) {
    smartlist_shuffle(sl);
    smartlist_sort_pointers(sl);
    for (j = 0; j < ARRAY_LENGTH(arrayptrs); ++j) {
      tt_ptr_op(smartlist_get(sl, j), ==, arrayptrs[j]);
    }
  }

 done:
  smartlist_free(sl);
}

static void
test_container_smartlist_strings_eq(void *arg)
{
  (void)arg;
  smartlist_t *sl1 = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
#define EQ_SHOULD_SAY(s1,s2,val)                                \
  do {                                                          \
    SMARTLIST_FOREACH(sl1, char *, cp, tor_free(cp));           \
    SMARTLIST_FOREACH(sl2, char *, cp, tor_free(cp));           \
    smartlist_clear(sl1);                                       \
    smartlist_clear(sl2);                                       \
    smartlist_split_string(sl1, (s1), ":", 0, 0);               \
    smartlist_split_string(sl2, (s2), ":", 0, 0);               \
    tt_int_op((val), OP_EQ, smartlist_strings_eq(sl1, sl2));    \
  } while (0)

  /* Both NULL, so equal */
  tt_int_op(1, ==, smartlist_strings_eq(NULL, NULL));

  /* One NULL, not equal. */
  tt_int_op(0, ==, smartlist_strings_eq(NULL, sl1));
  tt_int_op(0, ==, smartlist_strings_eq(sl1, NULL));

  /* Both empty, both equal. */
  EQ_SHOULD_SAY("", "", 1);

  /* One empty, not equal */
  EQ_SHOULD_SAY("", "ab", 0);
  EQ_SHOULD_SAY("", "xy:z", 0);
  EQ_SHOULD_SAY("abc", "", 0);
  EQ_SHOULD_SAY("abc:cd", "", 0);

  /* Different lengths, not equal. */
  EQ_SHOULD_SAY("hello:world", "hello", 0);
  EQ_SHOULD_SAY("hello", "hello:friends", 0);

  /* Same lengths, not equal */
  EQ_SHOULD_SAY("Hello:world", "goodbye:world", 0);
  EQ_SHOULD_SAY("Hello:world", "Hello:stars", 0);

  /* Actually equal */
  EQ_SHOULD_SAY("ABC", "ABC", 1);
  EQ_SHOULD_SAY(" ab : cd : e", " ab : cd : e", 1);

 done:
  SMARTLIST_FOREACH(sl1, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(sl2, char *, cp, tor_free(cp));
  smartlist_free(sl1);
  smartlist_free(sl2);
}

#define CONTAINER_LEGACY(name)                                          \
  { #name, test_container_ ## name , 0, NULL, NULL }

#define CONTAINER(name, flags)                                          \
  { #name, test_container_ ## name, (flags), NULL, NULL }

struct testcase_t container_tests[] = {
  CONTAINER_LEGACY(smartlist_basic),
  CONTAINER_LEGACY(smartlist_strings),
  CONTAINER_LEGACY(smartlist_overlap),
  CONTAINER_LEGACY(smartlist_digests),
  CONTAINER_LEGACY(smartlist_join),
  CONTAINER_LEGACY(smartlist_pos),
  CONTAINER(smartlist_ints_eq, 0),
  CONTAINER_LEGACY(bitarray),
  CONTAINER_LEGACY(digestset),
  CONTAINER_LEGACY(strmap),
  CONTAINER_LEGACY(pqueue),
  CONTAINER_LEGACY(order_functions),
  CONTAINER(di_map, 0),
  CONTAINER_LEGACY(fp_pair_map),
  CONTAINER(smartlist_most_frequent, 0),
  CONTAINER(smartlist_sort_ptrs, 0),
  CONTAINER(smartlist_strings_eq, 0),
  END_OF_TESTCASES
};

