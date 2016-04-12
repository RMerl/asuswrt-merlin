/* Copyright (c) 2012-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define REPLAYCACHE_PRIVATE

#include "orconfig.h"
#include "or.h"
#include "replaycache.h"
#include "test.h"

static const char *test_buffer =
  "Lorem ipsum dolor sit amet, consectetur adipisici elit, sed do eiusmod"
  " tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim"
  " veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea"
  " commodo consequat. Duis aute irure dolor in reprehenderit in voluptate"
  " velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint"
  " occaecat cupidatat non proident, sunt in culpa qui officia deserunt"
  " mollit anim id est laborum.";

static void
test_replaycache_alloc(void *arg)
{
  replaycache_t *r = NULL;

  (void)arg;
  r = replaycache_new(600, 300);
  tt_assert(r != NULL);

 done:
  if (r) replaycache_free(r);

  return;
}

static void
test_replaycache_badalloc(void *arg)
{
  replaycache_t *r = NULL;

  /* Negative horizon should fail */
  (void)arg;
  r = replaycache_new(-600, 300);
  tt_assert(r == NULL);
  /* Negative interval should get adjusted to zero */
  r = replaycache_new(600, -300);
  tt_assert(r != NULL);
  tt_int_op(r->scrub_interval,OP_EQ, 0);
  replaycache_free(r);
  /* Negative horizon and negative interval should still fail */
  r = replaycache_new(-600, -300);
  tt_assert(r == NULL);

 done:
  if (r) replaycache_free(r);

  return;
}

static void
test_replaycache_free_null(void *arg)
{
  (void)arg;
  replaycache_free(NULL);
  /* Assert that we're here without horrible death */
  tt_assert(1);

 done:
  return;
}

static void
test_replaycache_miss(void *arg)
{
  replaycache_t *r = NULL;
  int result;

  (void)arg;
  r = replaycache_new(600, 300);
  tt_assert(r != NULL);

  result =
    replaycache_add_and_test_internal(1200, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 0);

  /* poke the bad-parameter error case too */
  result =
    replaycache_add_and_test_internal(1200, NULL, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 0);

 done:
  if (r) replaycache_free(r);

  return;
}

static void
test_replaycache_hit(void *arg)
{
  replaycache_t *r = NULL;
  int result;

  (void)arg;
  r = replaycache_new(600, 300);
  tt_assert(r != NULL);

  result =
    replaycache_add_and_test_internal(1200, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 0);

  result =
    replaycache_add_and_test_internal(1300, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 1);

 done:
  if (r) replaycache_free(r);

  return;
}

static void
test_replaycache_age(void *arg)
{
  replaycache_t *r = NULL;
  int result;

  (void)arg;
  r = replaycache_new(600, 300);
  tt_assert(r != NULL);

  result =
    replaycache_add_and_test_internal(1200, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 0);

  result =
    replaycache_add_and_test_internal(1300, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 1);

  result =
    replaycache_add_and_test_internal(3000, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 0);

 done:
  if (r) replaycache_free(r);

  return;
}

static void
test_replaycache_elapsed(void *arg)
{
  replaycache_t *r = NULL;
  int result;
  time_t elapsed;

  (void)arg;
  r = replaycache_new(600, 300);
  tt_assert(r != NULL);

  result =
    replaycache_add_and_test_internal(1200, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 0);

  result =
    replaycache_add_and_test_internal(1300, r, test_buffer,
        strlen(test_buffer), &elapsed);
  tt_int_op(result,OP_EQ, 1);
  tt_int_op(elapsed,OP_EQ, 100);

 done:
  if (r) replaycache_free(r);

  return;
}

static void
test_replaycache_noexpire(void *arg)
{
  replaycache_t *r = NULL;
  int result;

  (void)arg;
  r = replaycache_new(0, 0);
  tt_assert(r != NULL);

  result =
    replaycache_add_and_test_internal(1200, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 0);

  result =
    replaycache_add_and_test_internal(1300, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 1);

  result =
    replaycache_add_and_test_internal(3000, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 1);

 done:
  if (r) replaycache_free(r);

  return;
}

static void
test_replaycache_scrub(void *arg)
{
  replaycache_t *r = NULL;
  int result;

  (void)arg;
  r = replaycache_new(600, 300);
  tt_assert(r != NULL);

  /* Set up like in test_replaycache_hit() */
  result =
    replaycache_add_and_test_internal(100, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 0);

  result =
    replaycache_add_and_test_internal(200, r, test_buffer,
        strlen(test_buffer), NULL);
  tt_int_op(result,OP_EQ, 1);

  /*
   * Poke a few replaycache_scrub_if_needed_internal() error cases that
   * can't happen through replaycache_add_and_test_internal()
   */

  /* Null cache */
  replaycache_scrub_if_needed_internal(300, NULL);
  /* Assert we're still here */
  tt_assert(1);

  /* Make sure we hit the aging-out case too */
  replaycache_scrub_if_needed_internal(1500, r);
  /* Assert that we aged it */
  tt_int_op(digestmap_size(r->digests_seen),OP_EQ, 0);

 done:
  if (r) replaycache_free(r);

  return;
}

static void
test_replaycache_future(void *arg)
{
  replaycache_t *r = NULL;
  int result;
  time_t elapsed = 0;

  (void)arg;
  r = replaycache_new(600, 300);
  tt_assert(r != NULL);

  /* Set up like in test_replaycache_hit() */
  result =
    replaycache_add_and_test_internal(100, r, test_buffer,
        strlen(test_buffer), &elapsed);
  tt_int_op(result,OP_EQ, 0);
  /* elapsed should still be 0, since it wasn't written */
  tt_int_op(elapsed,OP_EQ, 0);

  result =
    replaycache_add_and_test_internal(200, r, test_buffer,
        strlen(test_buffer), &elapsed);
  tt_int_op(result,OP_EQ, 1);
  /* elapsed should be the time since the last hit */
  tt_int_op(elapsed,OP_EQ, 100);

  /*
   * Now let's turn the clock back to get coverage on the cache entry from the
   * future not-supposed-to-happen case.
   */
  result =
    replaycache_add_and_test_internal(150, r, test_buffer,
        strlen(test_buffer), &elapsed);
  /* We should still get a hit */
  tt_int_op(result,OP_EQ, 1);
  /* ...but it shouldn't let us see a negative elapsed time */
  tt_int_op(elapsed,OP_EQ, 0);

 done:
  if (r) replaycache_free(r);

  return;
}

static void
test_replaycache_realtime(void *arg)
{
  replaycache_t *r = NULL;
  /*
   * Negative so we fail if replaycache_add_test_and_elapsed() doesn't
   * write to elapsed.
   */
  time_t elapsed = -1;
  int result;

  /* Test the realtime as well as *_internal() entry points */
  (void)arg;
  r = replaycache_new(600, 300);
  tt_assert(r != NULL);

  /* This should miss */
  result =
    replaycache_add_and_test(r, test_buffer, strlen(test_buffer));
  tt_int_op(result,OP_EQ, 0);

  /* This should hit */
  result =
    replaycache_add_and_test(r, test_buffer, strlen(test_buffer));
  tt_int_op(result,OP_EQ, 1);

  /* This should hit and return a small elapsed time */
  result =
    replaycache_add_test_and_elapsed(r, test_buffer,
                                     strlen(test_buffer), &elapsed);
  tt_int_op(result,OP_EQ, 1);
  tt_assert(elapsed >= 0);
  tt_assert(elapsed <= 5);

  /* Scrub it to exercise that entry point too */
  replaycache_scrub_if_needed(r);

 done:
  if (r) replaycache_free(r);
  return;
}

#define REPLAYCACHE_LEGACY(name) \
  { #name, test_replaycache_ ## name , 0, NULL, NULL }

struct testcase_t replaycache_tests[] = {
  REPLAYCACHE_LEGACY(alloc),
  REPLAYCACHE_LEGACY(badalloc),
  REPLAYCACHE_LEGACY(free_null),
  REPLAYCACHE_LEGACY(miss),
  REPLAYCACHE_LEGACY(hit),
  REPLAYCACHE_LEGACY(age),
  REPLAYCACHE_LEGACY(elapsed),
  REPLAYCACHE_LEGACY(noexpire),
  REPLAYCACHE_LEGACY(scrub),
  REPLAYCACHE_LEGACY(future),
  REPLAYCACHE_LEGACY(realtime),
  END_OF_TESTCASES
};

