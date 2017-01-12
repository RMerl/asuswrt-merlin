/* Copyright (c) 2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/* Unit tests for OOS handler */

#define CONNECTION_PRIVATE

#include "or.h"
#include "config.h"
#include "connection.h"
#include "connection_or.h"
#include "main.h"
#include "test.h"

static or_options_t mock_options;

static void
reset_options_mock(void)
{
  memset(&mock_options, 0, sizeof(or_options_t));
}

static const or_options_t *
mock_get_options(void)
{
  return &mock_options;
}

static int moribund_calls = 0;
static int moribund_conns = 0;

static int
mock_connection_count_moribund(void)
{
  ++moribund_calls;

  return moribund_conns;
}

/*
 * For unit test purposes it's sufficient to tell that
 * kill_conn_list_for_oos() was called with an approximately
 * sane argument; it's just the thing we returned from the
 * mock for pick_oos_victims().
 */

static int kill_conn_list_calls = 0;
static int kill_conn_list_killed = 0;

static void
kill_conn_list_mock(smartlist_t *conns)
{
  ++kill_conn_list_calls;

  tt_assert(conns != NULL);

  kill_conn_list_killed += smartlist_len(conns);

 done:
  return;
}

static int pick_oos_mock_calls = 0;
static int pick_oos_mock_fail = 0;
static int pick_oos_mock_last_n = 0;

static smartlist_t *
pick_oos_victims_mock(int n)
{
  smartlist_t *l = NULL;
  int i;

  ++pick_oos_mock_calls;

  tt_int_op(n, OP_GT, 0);

  if (!pick_oos_mock_fail) {
    /*
     * connection_check_oos() just passes the list onto
     * kill_conn_list_for_oos(); we don't need to simulate
     * its content for this mock, just its existence, but
     * we do need to check the parameter.
     */
    l = smartlist_new();
    for (i = 0; i < n; ++i) smartlist_add(l, NULL);
  } else {
    l = NULL;
  }

  pick_oos_mock_last_n = n;

 done:
  return l;
}

/** Unit test for the logic in connection_check_oos(), which is concerned
 * with comparing thresholds and connection counts to decide if an OOS has
 * occurred and if so, how many connections to try to kill, and then using
 * pick_oos_victims() and kill_conn_list_for_oos() to carry out its grim
 * duty.
 */
static void
test_oos_connection_check_oos(void *arg)
{
  (void)arg;

  /* Set up mocks */
  reset_options_mock();
  /* OOS handling is only sensitive to these fields */
  mock_options.ConnLimit = 32;
  mock_options.ConnLimit_ = 64;
  mock_options.ConnLimit_high_thresh = 60;
  mock_options.ConnLimit_low_thresh = 50;
  MOCK(get_options, mock_get_options);
  moribund_calls = 0;
  moribund_conns = 0;
  MOCK(connection_count_moribund, mock_connection_count_moribund);
  kill_conn_list_calls = 0;
  kill_conn_list_killed = 0;
  MOCK(kill_conn_list_for_oos, kill_conn_list_mock);
  pick_oos_mock_calls = 0;
  pick_oos_mock_fail = 0;
  MOCK(pick_oos_victims, pick_oos_victims_mock);

  /* No OOS case */
  connection_check_oos(50, 0);
  tt_int_op(moribund_calls, OP_EQ, 0);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 0);
  tt_int_op(kill_conn_list_calls, OP_EQ, 0);

  /* OOS from socket count, nothing moribund */
  connection_check_oos(62, 0);
  tt_int_op(moribund_calls, OP_EQ, 1);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 1);
  /* 12 == 62 - ConnLimit_low_thresh */
  tt_int_op(pick_oos_mock_last_n, OP_EQ, 12);
  tt_int_op(kill_conn_list_calls, OP_EQ, 1);
  tt_int_op(kill_conn_list_killed, OP_EQ, 12);

  /* OOS from socket count, some are moribund */
  kill_conn_list_killed = 0;
  moribund_conns = 5;
  connection_check_oos(62, 0);
  tt_int_op(moribund_calls, OP_EQ, 2);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 2);
  /* 7 == 62 - ConnLimit_low_thresh - moribund_conns */
  tt_int_op(pick_oos_mock_last_n, OP_EQ, 7);
  tt_int_op(kill_conn_list_calls, OP_EQ, 2);
  tt_int_op(kill_conn_list_killed, OP_EQ, 7);

  /* OOS from socket count, but pick fails */
  kill_conn_list_killed = 0;
  moribund_conns = 0;
  pick_oos_mock_fail = 1;
  connection_check_oos(62, 0);
  tt_int_op(moribund_calls, OP_EQ, 3);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 3);
  tt_int_op(kill_conn_list_calls, OP_EQ, 2);
  tt_int_op(kill_conn_list_killed, OP_EQ, 0);
  pick_oos_mock_fail = 0;

  /*
   * OOS from socket count with so many moribund conns
   * we have none to kill.
   */
  kill_conn_list_killed = 0;
  moribund_conns = 15;
  connection_check_oos(62, 0);
  tt_int_op(moribund_calls, OP_EQ, 4);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 3);
  tt_int_op(kill_conn_list_calls, OP_EQ, 2);

  /*
   * OOS from socket exhaustion; OOS handler will try to
   * kill 1/10 (5) of the connections.
   */
  kill_conn_list_killed = 0;
  moribund_conns = 0;
  connection_check_oos(50, 1);
  tt_int_op(moribund_calls, OP_EQ, 5);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 4);
  tt_int_op(kill_conn_list_calls, OP_EQ, 3);
  tt_int_op(kill_conn_list_killed, OP_EQ, 5);

  /* OOS from socket exhaustion with moribund conns */
  kill_conn_list_killed = 0;
  moribund_conns = 2;
  connection_check_oos(50, 1);
  tt_int_op(moribund_calls, OP_EQ, 6);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 5);
  tt_int_op(kill_conn_list_calls, OP_EQ, 4);
  tt_int_op(kill_conn_list_killed, OP_EQ, 3);

  /* OOS from socket exhaustion with many moribund conns */
  kill_conn_list_killed = 0;
  moribund_conns = 7;
  connection_check_oos(50, 1);
  tt_int_op(moribund_calls, OP_EQ, 7);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 5);
  tt_int_op(kill_conn_list_calls, OP_EQ, 4);

  /* OOS with both socket exhaustion and above-threshold */
  kill_conn_list_killed = 0;
  moribund_conns = 0;
  connection_check_oos(62, 1);
  tt_int_op(moribund_calls, OP_EQ, 8);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 6);
  tt_int_op(kill_conn_list_calls, OP_EQ, 5);
  tt_int_op(kill_conn_list_killed, OP_EQ, 12);

  /*
   * OOS with both socket exhaustion and above-threshold with some
   * moribund conns
   */
  kill_conn_list_killed = 0;
  moribund_conns = 5;
  connection_check_oos(62, 1);
  tt_int_op(moribund_calls, OP_EQ, 9);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 7);
  tt_int_op(kill_conn_list_calls, OP_EQ, 6);
  tt_int_op(kill_conn_list_killed, OP_EQ, 7);

  /*
   * OOS with both socket exhaustion and above-threshold with many
   * moribund conns
   */
  kill_conn_list_killed = 0;
  moribund_conns = 15;
  connection_check_oos(62, 1);
  tt_int_op(moribund_calls, OP_EQ, 10);
  tt_int_op(pick_oos_mock_calls, OP_EQ, 7);
  tt_int_op(kill_conn_list_calls, OP_EQ, 6);

 done:

  UNMOCK(pick_oos_victims);
  UNMOCK(kill_conn_list_for_oos);
  UNMOCK(connection_count_moribund);
  UNMOCK(get_options);

  return;
}

static int cfe_calls = 0;

static void
close_for_error_mock(or_connection_t *orconn, int flush)
{
  (void)flush;

  tt_assert(orconn != NULL);
  ++cfe_calls;

 done:
  return;
}

static int mark_calls = 0;

static void
mark_for_close_oos_mock(connection_t *conn,
                        int line, const char *file)
{
  (void)line;
  (void)file;

  tt_assert(conn != NULL);
  ++mark_calls;

 done:
  return;
}

static void
test_oos_kill_conn_list(void *arg)
{
  connection_t *c1, *c2;
  or_connection_t *or_c1 = NULL;
  dir_connection_t *dir_c2 = NULL;
  smartlist_t *l = NULL;
  (void)arg;

  /* Set up mocks */
  mark_calls = 0;
  MOCK(connection_mark_for_close_internal_, mark_for_close_oos_mock);
  cfe_calls = 0;
  MOCK(connection_or_close_for_error, close_for_error_mock);

  /* Make fake conns */
  or_c1 = tor_malloc_zero(sizeof(*or_c1));
  or_c1->base_.magic = OR_CONNECTION_MAGIC;
  or_c1->base_.type = CONN_TYPE_OR;
  c1 = TO_CONN(or_c1);
  dir_c2 = tor_malloc_zero(sizeof(*dir_c2));
  dir_c2->base_.magic = DIR_CONNECTION_MAGIC;
  dir_c2->base_.type = CONN_TYPE_DIR;
  dir_c2->base_.state = DIR_CONN_STATE_MIN_;
  dir_c2->base_.purpose = DIR_PURPOSE_MIN_;
  c2 = TO_CONN(dir_c2);

  tt_assert(c1 != NULL);
  tt_assert(c2 != NULL);

  /* Make list */
  l = smartlist_new();
  smartlist_add(l, c1);
  smartlist_add(l, c2);

  /* Run kill_conn_list_for_oos() */
  kill_conn_list_for_oos(l);

  /* Check call counters */
  tt_int_op(mark_calls, OP_EQ, 1);
  tt_int_op(cfe_calls, OP_EQ, 1);

 done:

  UNMOCK(connection_or_close_for_error);
  UNMOCK(connection_mark_for_close_internal_);

  if (l) smartlist_free(l);
  tor_free(or_c1);
  tor_free(dir_c2);

  return;
}

static smartlist_t *conns_for_mock = NULL;

static smartlist_t *
get_conns_mock(void)
{
  return conns_for_mock;
}

/*
 * For this mock, we pretend all conns have either zero or one circuits,
 * depending on if this appears on the list of things to say have a circuit.
 */

static smartlist_t *conns_with_circs = NULL;

static int
get_num_circuits_mock(or_connection_t *conn)
{
  int circs = 0;

  tt_assert(conn != NULL);

  if (conns_with_circs &&
      smartlist_contains(conns_with_circs, TO_CONN(conn))) {
    circs = 1;
  }

 done:
  return circs;
}

static void
test_oos_pick_oos_victims(void *arg)
{
  (void)arg;
  or_connection_t *ortmp;
  dir_connection_t *dirtmp;
  smartlist_t *picked;

  /* Set up mocks */
  conns_for_mock = smartlist_new();
  MOCK(get_connection_array, get_conns_mock);
  conns_with_circs = smartlist_new();
  MOCK(connection_or_get_num_circuits, get_num_circuits_mock);

  /* Make some fake connections */
  ortmp = tor_malloc_zero(sizeof(*ortmp));
  ortmp->base_.magic = OR_CONNECTION_MAGIC;
  ortmp->base_.type = CONN_TYPE_OR;
  smartlist_add(conns_for_mock, TO_CONN(ortmp));
  /* We'll pretend this one has a circuit too */
  smartlist_add(conns_with_circs, TO_CONN(ortmp));
  /* Next one */
  ortmp = tor_malloc_zero(sizeof(*ortmp));
  ortmp->base_.magic = OR_CONNECTION_MAGIC;
  ortmp->base_.type = CONN_TYPE_OR;
  smartlist_add(conns_for_mock, TO_CONN(ortmp));
  /* Next one is moribund */
  ortmp = tor_malloc_zero(sizeof(*ortmp));
  ortmp->base_.magic = OR_CONNECTION_MAGIC;
  ortmp->base_.type = CONN_TYPE_OR;
  ortmp->base_.marked_for_close = 1;
  smartlist_add(conns_for_mock, TO_CONN(ortmp));
  /* Last one isn't an orconn */
  dirtmp = tor_malloc_zero(sizeof(*dirtmp));
  dirtmp->base_.magic = DIR_CONNECTION_MAGIC;
  dirtmp->base_.type = CONN_TYPE_DIR;
  smartlist_add(conns_for_mock, TO_CONN(dirtmp));

  /* Try picking one */
  picked = pick_oos_victims(1);
  /* It should be the one with circuits */
  tt_assert(picked != NULL);
  tt_int_op(smartlist_len(picked), OP_EQ, 1);
  tt_assert(smartlist_contains(picked, smartlist_get(conns_for_mock, 0)));
  smartlist_free(picked);

  /* Try picking none */
  picked = pick_oos_victims(0);
  /* We should get an empty list */
  tt_assert(picked != NULL);
  tt_int_op(smartlist_len(picked), OP_EQ, 0);
  smartlist_free(picked);

  /* Try picking two */
  picked = pick_oos_victims(2);
  /* We should get both active orconns */
  tt_assert(picked != NULL);
  tt_int_op(smartlist_len(picked), OP_EQ, 2);
  tt_assert(smartlist_contains(picked, smartlist_get(conns_for_mock, 0)));
  tt_assert(smartlist_contains(picked, smartlist_get(conns_for_mock, 1)));
  smartlist_free(picked);

  /* Try picking three - only two are eligible */
  picked = pick_oos_victims(3);
  tt_int_op(smartlist_len(picked), OP_EQ, 2);
  tt_assert(smartlist_contains(picked, smartlist_get(conns_for_mock, 0)));
  tt_assert(smartlist_contains(picked, smartlist_get(conns_for_mock, 1)));
  smartlist_free(picked);

 done:

  /* Free leftover stuff */
  if (conns_with_circs) {
    smartlist_free(conns_with_circs);
    conns_with_circs = NULL;
  }

  UNMOCK(connection_or_get_num_circuits);

  if (conns_for_mock) {
    SMARTLIST_FOREACH(conns_for_mock, connection_t *, c, tor_free(c));
    smartlist_free(conns_for_mock);
    conns_for_mock = NULL;
  }

  UNMOCK(get_connection_array);

  return;
}

struct testcase_t oos_tests[] = {
  { "connection_check_oos", test_oos_connection_check_oos,
    TT_FORK, NULL, NULL },
  { "kill_conn_list", test_oos_kill_conn_list, TT_FORK, NULL, NULL },
  { "pick_oos_victims", test_oos_pick_oos_victims, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

