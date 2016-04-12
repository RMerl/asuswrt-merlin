/* Copyright (c) 2013-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define TOR_CHANNEL_INTERNAL_
#define CHANNEL_PRIVATE_
#include "or.h"
#include "channel.h"
/* For channel_note_destroy_not_pending */
#include "circuitlist.h"
#include "circuitmux.h"
/* For var_cell_free */
#include "connection_or.h"
/* For packed_cell stuff */
#define RELAY_PRIVATE
#include "relay.h"
/* For init/free stuff */
#include "scheduler.h"

/* Test suite stuff */
#include "test.h"
#include "fakechans.h"

/* This comes from channel.c */
extern uint64_t estimated_total_queue_size;

static int test_chan_accept_cells = 0;
static int test_chan_fixed_cells_recved = 0;
static int test_chan_var_cells_recved = 0;
static int test_cells_written = 0;
static int test_destroy_not_pending_calls = 0;
static int test_doesnt_want_writes_count = 0;
static int test_dumpstats_calls = 0;
static int test_has_waiting_cells_count = 0;
static double test_overhead_estimate = 1.0f;
static int test_releases_count = 0;
static circuitmux_t *test_target_cmux = NULL;
static unsigned int test_cmux_cells = 0;
static channel_t *dump_statistics_mock_target = NULL;
static int dump_statistics_mock_matches = 0;

static void chan_test_channel_dump_statistics_mock(
    channel_t *chan, int severity);
static int chan_test_channel_flush_from_first_active_circuit_mock(
    channel_t *chan, int max);
static unsigned int chan_test_circuitmux_num_cells_mock(circuitmux_t *cmux);
static void channel_note_destroy_not_pending_mock(channel_t *ch,
                                                  circid_t circid);
static void chan_test_cell_handler(channel_t *ch,
                                   cell_t *cell);
static const char * chan_test_describe_transport(channel_t *ch);
static void chan_test_dumpstats(channel_t *ch, int severity);
static void chan_test_var_cell_handler(channel_t *ch,
                                       var_cell_t *var_cell);
static void chan_test_close(channel_t *ch);
static void chan_test_error(channel_t *ch);
static void chan_test_finish_close(channel_t *ch);
static const char * chan_test_get_remote_descr(channel_t *ch, int flags);
static int chan_test_is_canonical(channel_t *ch, int req);
static size_t chan_test_num_bytes_queued(channel_t *ch);
static int chan_test_num_cells_writeable(channel_t *ch);
static int chan_test_write_cell(channel_t *ch, cell_t *cell);
static int chan_test_write_packed_cell(channel_t *ch,
                                       packed_cell_t *packed_cell);
static int chan_test_write_var_cell(channel_t *ch, var_cell_t *var_cell);
static void scheduler_channel_doesnt_want_writes_mock(channel_t *ch);

static void test_channel_dumpstats(void *arg);
static void test_channel_flush(void *arg);
static void test_channel_flushmux(void *arg);
static void test_channel_incoming(void *arg);
static void test_channel_lifecycle(void *arg);
static void test_channel_multi(void *arg);
static void test_channel_queue_size(void *arg);
static void test_channel_write(void *arg);

static void
channel_note_destroy_not_pending_mock(channel_t *ch,
                                      circid_t circid)
{
  (void)ch;
  (void)circid;

  ++test_destroy_not_pending_calls;
}

static const char *
chan_test_describe_transport(channel_t *ch)
{
  tt_assert(ch != NULL);

 done:
  return "Fake channel for unit tests";
}

/**
 * Mock for channel_dump_statistics(); if the channel matches the
 * target, bump a counter - otherwise ignore.
 */

static void
chan_test_channel_dump_statistics_mock(channel_t *chan, int severity)
{
  tt_assert(chan != NULL);

  (void)severity;

  if (chan != NULL && chan == dump_statistics_mock_target) {
    ++dump_statistics_mock_matches;
  }

 done:
  return;
}

/**
 * If the target cmux is the cmux for chan, make fake cells up to the
 * target number of cells and write them to chan.  Otherwise, invoke
 * the real channel_flush_from_first_active_circuit().
 */

static int
chan_test_channel_flush_from_first_active_circuit_mock(channel_t *chan,
                                                       int max)
{
  int result = 0, c = 0;
  packed_cell_t *cell = NULL;

  tt_assert(chan != NULL);
  if (test_target_cmux != NULL &&
      test_target_cmux == chan->cmux) {
    while (c <= max && test_cmux_cells > 0) {
      cell = packed_cell_new();
      channel_write_packed_cell(chan, cell);
      ++c;
      --test_cmux_cells;
    }
    result = c;
  } else {
    result = channel_flush_from_first_active_circuit__real(chan, max);
  }

 done:
  return result;
}

/**
 * If we have a target cmux set and this matches it, lie about how
 * many cells we have according to the number indicated; otherwise
 * pass to the real circuitmux_num_cells().
 */

static unsigned int
chan_test_circuitmux_num_cells_mock(circuitmux_t *cmux)
{
  unsigned int result = 0;

  tt_assert(cmux != NULL);
  if (cmux != NULL) {
    if (cmux == test_target_cmux) {
      result = test_cmux_cells;
    } else {
      result = circuitmux_num_cells__real(cmux);
    }
  }

 done:

  return result;
}

/*
 * Handle an incoming fixed-size cell for unit tests
 */

static void
chan_test_cell_handler(channel_t *ch,
                       cell_t *cell)
{
  tt_assert(ch);
  tt_assert(cell);

  tor_free(cell);
  ++test_chan_fixed_cells_recved;

 done:
  return;
}

/*
 * Fake transport-specific stats call
 */

static void
chan_test_dumpstats(channel_t *ch, int severity)
{
  tt_assert(ch != NULL);

  (void)severity;

  ++test_dumpstats_calls;

 done:
  return;
}

/*
 * Handle an incoming variable-size cell for unit tests
 */

static void
chan_test_var_cell_handler(channel_t *ch,
                           var_cell_t *var_cell)
{
  tt_assert(ch);
  tt_assert(var_cell);

  tor_free(var_cell);
  ++test_chan_var_cells_recved;

 done:
  return;
}

static void
chan_test_close(channel_t *ch)
{
  tt_assert(ch);

 done:
  return;
}

/*
 * Close a channel through the error path
 */

static void
chan_test_error(channel_t *ch)
{
  tt_assert(ch);
  tt_assert(!(ch->state == CHANNEL_STATE_CLOSING ||
                ch->state == CHANNEL_STATE_ERROR ||
                ch->state == CHANNEL_STATE_CLOSED));

  channel_close_for_error(ch);

 done:
  return;
}

/*
 * Finish closing a channel from CHANNEL_STATE_CLOSING
 */

static void
chan_test_finish_close(channel_t *ch)
{
  tt_assert(ch);
  tt_assert(ch->state == CHANNEL_STATE_CLOSING);

  channel_closed(ch);

 done:
  return;
}

static const char *
chan_test_get_remote_descr(channel_t *ch, int flags)
{
  tt_assert(ch);
  tt_int_op(flags & ~(GRD_FLAG_ORIGINAL | GRD_FLAG_ADDR_ONLY), ==, 0);

 done:
  return "Fake channel for unit tests; no real endpoint";
}

static double
chan_test_get_overhead_estimate(channel_t *ch)
{
  tt_assert(ch);

 done:
  return test_overhead_estimate;
}

static int
chan_test_is_canonical(channel_t *ch, int req)
{
  tt_assert(ch != NULL);
  tt_assert(req == 0 || req == 1);

 done:
  /* Fake channels are always canonical */
  return 1;
}

static size_t
chan_test_num_bytes_queued(channel_t *ch)
{
  tt_assert(ch);

 done:
  return 0;
}

static int
chan_test_num_cells_writeable(channel_t *ch)
{
  tt_assert(ch);

 done:
  return 32;
}

static int
chan_test_write_cell(channel_t *ch, cell_t *cell)
{
  int rv = 0;

  tt_assert(ch);
  tt_assert(cell);

  if (test_chan_accept_cells) {
    /* Free the cell and bump the counter */
    tor_free(cell);
    ++test_cells_written;
    rv = 1;
  }
  /* else return 0, we didn't accept it */

 done:
  return rv;
}

static int
chan_test_write_packed_cell(channel_t *ch,
                            packed_cell_t *packed_cell)
{
  int rv = 0;

  tt_assert(ch);
  tt_assert(packed_cell);

  if (test_chan_accept_cells) {
    /* Free the cell and bump the counter */
    packed_cell_free(packed_cell);
    ++test_cells_written;
    rv = 1;
  }
  /* else return 0, we didn't accept it */

 done:
  return rv;
}

static int
chan_test_write_var_cell(channel_t *ch, var_cell_t *var_cell)
{
  int rv = 0;

  tt_assert(ch);
  tt_assert(var_cell);

  if (test_chan_accept_cells) {
    /* Free the cell and bump the counter */
    var_cell_free(var_cell);
    ++test_cells_written;
    rv = 1;
  }
  /* else return 0, we didn't accept it */

 done:
  return rv;
}

/**
 * Fill out c with a new fake cell for test suite use
 */

void
make_fake_cell(cell_t *c)
{
  tt_assert(c != NULL);

  c->circ_id = 1;
  c->command = CELL_RELAY;
  memset(c->payload, 0, CELL_PAYLOAD_SIZE);

 done:
  return;
}

/**
 * Fill out c with a new fake var_cell for test suite use
 */

void
make_fake_var_cell(var_cell_t *c)
{
  tt_assert(c != NULL);

  c->circ_id = 1;
  c->command = CELL_VERSIONS;
  c->payload_len = CELL_PAYLOAD_SIZE / 2;
  memset(c->payload, 0, c->payload_len);

 done:
  return;
}

/**
 * Set up a new fake channel for the test suite
 */

channel_t *
new_fake_channel(void)
{
  channel_t *chan = tor_malloc_zero(sizeof(channel_t));
  channel_init(chan);

  chan->close = chan_test_close;
  chan->get_overhead_estimate = chan_test_get_overhead_estimate;
  chan->get_remote_descr = chan_test_get_remote_descr;
  chan->num_bytes_queued = chan_test_num_bytes_queued;
  chan->num_cells_writeable = chan_test_num_cells_writeable;
  chan->write_cell = chan_test_write_cell;
  chan->write_packed_cell = chan_test_write_packed_cell;
  chan->write_var_cell = chan_test_write_var_cell;
  chan->state = CHANNEL_STATE_OPEN;

  return chan;
}

void
free_fake_channel(channel_t *chan)
{
  cell_queue_entry_t *cell, *cell_tmp;

  if (! chan)
    return;

  if (chan->cmux)
    circuitmux_free(chan->cmux);

  TOR_SIMPLEQ_FOREACH_SAFE(cell, &chan->incoming_queue, next, cell_tmp) {
      cell_queue_entry_free(cell, 0);
  }
  TOR_SIMPLEQ_FOREACH_SAFE(cell, &chan->outgoing_queue, next, cell_tmp) {
      cell_queue_entry_free(cell, 0);
  }

  tor_free(chan);
}

/**
 * Counter query for scheduler_channel_has_waiting_cells_mock()
 */

int
get_mock_scheduler_has_waiting_cells_count(void)
{
  return test_has_waiting_cells_count;
}

/**
 * Mock for scheduler_channel_has_waiting_cells()
 */

void
scheduler_channel_has_waiting_cells_mock(channel_t *ch)
{
  (void)ch;

  /* Increment counter */
  ++test_has_waiting_cells_count;

  return;
}

static void
scheduler_channel_doesnt_want_writes_mock(channel_t *ch)
{
  (void)ch;

  /* Increment counter */
  ++test_doesnt_want_writes_count;

  return;
}

/**
 * Counter query for scheduler_release_channel_mock()
 */

int
get_mock_scheduler_release_channel_count(void)
{
  return test_releases_count;
}

/**
 * Mock for scheduler_release_channel()
 */

void
scheduler_release_channel_mock(channel_t *ch)
{
  (void)ch;

  /* Increment counter */
  ++test_releases_count;

  return;
}

/**
 * Test for channel_dumpstats() and limited test for
 * channel_dump_statistics()
 */

static void
test_channel_dumpstats(void *arg)
{
  channel_t *ch = NULL;
  cell_t *cell = NULL;
  int old_count;

  (void)arg;

  /* Mock these for duration of the test */
  MOCK(scheduler_channel_doesnt_want_writes,
       scheduler_channel_doesnt_want_writes_mock);
  MOCK(scheduler_release_channel,
       scheduler_release_channel_mock);

  /* Set up a new fake channel */
  ch = new_fake_channel();
  tt_assert(ch);
  ch->cmux = circuitmux_alloc();

  /* Try to register it */
  channel_register(ch);
  tt_assert(ch->registered);

  /* Set up mock */
  dump_statistics_mock_target = ch;
  dump_statistics_mock_matches = 0;
  MOCK(channel_dump_statistics,
       chan_test_channel_dump_statistics_mock);

  /* Call channel_dumpstats() */
  channel_dumpstats(LOG_DEBUG);

  /* Assert that we hit the mock */
  tt_int_op(dump_statistics_mock_matches, ==, 1);

  /* Close the channel */
  channel_mark_for_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSING);
  chan_test_finish_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSED);

  /* Try again and hit the finished channel */
  channel_dumpstats(LOG_DEBUG);
  tt_int_op(dump_statistics_mock_matches, ==, 2);

  channel_run_cleanup();
  ch = NULL;

  /* Now we should hit nothing */
  channel_dumpstats(LOG_DEBUG);
  tt_int_op(dump_statistics_mock_matches, ==, 2);

  /* Unmock */
  UNMOCK(channel_dump_statistics);
  dump_statistics_mock_target = NULL;
  dump_statistics_mock_matches = 0;

  /* Now make another channel */
  ch = new_fake_channel();
  tt_assert(ch);
  ch->cmux = circuitmux_alloc();
  channel_register(ch);
  tt_assert(ch->registered);
  /* Lie about its age so dumpstats gets coverage for rate calculations */
  ch->timestamp_created = time(NULL) - 30;
  tt_assert(ch->timestamp_created > 0);
  tt_assert(time(NULL) > ch->timestamp_created);

  /* Put cells through it both ways to make the counters non-zero */
  cell = tor_malloc_zero(sizeof(*cell));
  make_fake_cell(cell);
  test_chan_accept_cells = 1;
  old_count = test_cells_written;
  channel_write_cell(ch, cell);
  cell = NULL;
  tt_int_op(test_cells_written, ==, old_count + 1);
  tt_assert(ch->n_bytes_xmitted > 0);
  tt_assert(ch->n_cells_xmitted > 0);

  /* Receive path */
  channel_set_cell_handlers(ch,
                            chan_test_cell_handler,
                            chan_test_var_cell_handler);
  tt_ptr_op(channel_get_cell_handler(ch), ==, chan_test_cell_handler);
  tt_ptr_op(channel_get_var_cell_handler(ch), ==, chan_test_var_cell_handler);
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  old_count = test_chan_fixed_cells_recved;
  channel_queue_cell(ch, cell);
  cell = NULL;
  tt_int_op(test_chan_fixed_cells_recved, ==, old_count + 1);
  tt_assert(ch->n_bytes_recved > 0);
  tt_assert(ch->n_cells_recved > 0);

  /* Test channel_dump_statistics */
  ch->describe_transport = chan_test_describe_transport;
  ch->dumpstats = chan_test_dumpstats;
  ch->is_canonical = chan_test_is_canonical;
  old_count = test_dumpstats_calls;
  channel_dump_statistics(ch, LOG_DEBUG);
  tt_int_op(test_dumpstats_calls, ==, old_count + 1);

  /* Close the channel */
  channel_mark_for_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSING);
  chan_test_finish_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSED);
  channel_run_cleanup();
  ch = NULL;

 done:
  tor_free(cell);
  free_fake_channel(ch);

  UNMOCK(scheduler_channel_doesnt_want_writes);
  UNMOCK(scheduler_release_channel);

  return;
}

static void
test_channel_flush(void *arg)
{
  channel_t *ch = NULL;
  cell_t *cell = NULL;
  packed_cell_t *p_cell = NULL;
  var_cell_t *v_cell = NULL;
  int init_count;

  (void)arg;

  ch = new_fake_channel();
  tt_assert(ch);

  /* Cache the original count */
  init_count = test_cells_written;

  /* Stop accepting so we can queue some */
  test_chan_accept_cells = 0;

  /* Queue a regular cell */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  channel_write_cell(ch, cell);
  /* It should be queued, so assert that we didn't write it */
  tt_int_op(test_cells_written, ==, init_count);

  /* Queue a var cell */
  v_cell = tor_malloc_zero(sizeof(var_cell_t) + CELL_PAYLOAD_SIZE);
  make_fake_var_cell(v_cell);
  channel_write_var_cell(ch, v_cell);
  /* It should be queued, so assert that we didn't write it */
  tt_int_op(test_cells_written, ==, init_count);

  /* Try a packed cell now */
  p_cell = packed_cell_new();
  tt_assert(p_cell);
  channel_write_packed_cell(ch, p_cell);
  /* It should be queued, so assert that we didn't write it */
  tt_int_op(test_cells_written, ==, init_count);

  /* Now allow writes through again */
  test_chan_accept_cells = 1;

  /* ...and flush */
  channel_flush_cells(ch);

  /* All three should have gone through */
  tt_int_op(test_cells_written, ==, init_count + 3);

 done:
  tor_free(ch);

  return;
}

/**
 * Channel flush tests that require cmux mocking
 */

static void
test_channel_flushmux(void *arg)
{
  channel_t *ch = NULL;
  int old_count, q_len_before, q_len_after;
  ssize_t result;

  (void)arg;

  /* Install mocks we need for this test */
  MOCK(channel_flush_from_first_active_circuit,
       chan_test_channel_flush_from_first_active_circuit_mock);
  MOCK(circuitmux_num_cells,
       chan_test_circuitmux_num_cells_mock);

  ch = new_fake_channel();
  tt_assert(ch);
  ch->cmux = circuitmux_alloc();

  old_count = test_cells_written;

  test_target_cmux = ch->cmux;
  test_cmux_cells = 1;

  /* Enable cell acceptance */
  test_chan_accept_cells = 1;

  result = channel_flush_some_cells(ch, 1);

  tt_int_op(result, ==, 1);
  tt_int_op(test_cells_written, ==, old_count + 1);
  tt_int_op(test_cmux_cells, ==, 0);

  /* Now try it without accepting to force them into the queue */
  test_chan_accept_cells = 0;
  test_cmux_cells = 1;
  q_len_before = chan_cell_queue_len(&(ch->outgoing_queue));

  result = channel_flush_some_cells(ch, 1);

  /* We should not have actually flushed any */
  tt_int_op(result, ==, 0);
  tt_int_op(test_cells_written, ==, old_count + 1);
  /* But we should have gotten to the fake cellgen loop */
  tt_int_op(test_cmux_cells, ==, 0);
  /* ...and we should have a queued cell */
  q_len_after = chan_cell_queue_len(&(ch->outgoing_queue));
  tt_int_op(q_len_after, ==, q_len_before + 1);

  /* Now accept cells again and drain the queue */
  test_chan_accept_cells = 1;
  channel_flush_cells(ch);
  tt_int_op(test_cells_written, ==, old_count + 2);
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 0);

  test_target_cmux = NULL;
  test_cmux_cells = 0;

 done:
  if (ch)
    circuitmux_free(ch->cmux);
  tor_free(ch);

  UNMOCK(channel_flush_from_first_active_circuit);
  UNMOCK(circuitmux_num_cells);

  test_chan_accept_cells = 0;

  return;
}

static void
test_channel_incoming(void *arg)
{
  channel_t *ch = NULL;
  cell_t *cell = NULL;
  var_cell_t *var_cell = NULL;
  int old_count;

  (void)arg;

  /* Mock these for duration of the test */
  MOCK(scheduler_channel_doesnt_want_writes,
       scheduler_channel_doesnt_want_writes_mock);
  MOCK(scheduler_release_channel,
       scheduler_release_channel_mock);

  /* Accept cells to lower layer */
  test_chan_accept_cells = 1;
  /* Use default overhead factor */
  test_overhead_estimate = 1.0f;

  ch = new_fake_channel();
  tt_assert(ch);
  /* Start it off in OPENING */
  ch->state = CHANNEL_STATE_OPENING;
  /* We'll need a cmux */
  ch->cmux = circuitmux_alloc();

  /* Install incoming cell handlers */
  channel_set_cell_handlers(ch,
                            chan_test_cell_handler,
                            chan_test_var_cell_handler);
  /* Test cell handler getters */
  tt_ptr_op(channel_get_cell_handler(ch), ==, chan_test_cell_handler);
  tt_ptr_op(channel_get_var_cell_handler(ch), ==, chan_test_var_cell_handler);

  /* Try to register it */
  channel_register(ch);
  tt_assert(ch->registered);

  /* Open it */
  channel_change_state(ch, CHANNEL_STATE_OPEN);
  tt_int_op(ch->state, ==, CHANNEL_STATE_OPEN);

  /* Receive a fixed cell */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  old_count = test_chan_fixed_cells_recved;
  channel_queue_cell(ch, cell);
  cell = NULL;
  tt_int_op(test_chan_fixed_cells_recved, ==, old_count + 1);

  /* Receive a variable-size cell */
  var_cell = tor_malloc_zero(sizeof(var_cell_t) + CELL_PAYLOAD_SIZE);
  make_fake_var_cell(var_cell);
  old_count = test_chan_var_cells_recved;
  channel_queue_var_cell(ch, var_cell);
  var_cell = NULL;
  tt_int_op(test_chan_var_cells_recved, ==, old_count + 1);

  /* Close it */
  channel_mark_for_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSING);
  chan_test_finish_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSED);
  channel_run_cleanup();
  ch = NULL;

 done:
  free_fake_channel(ch);
  tor_free(cell);
  tor_free(var_cell);

  UNMOCK(scheduler_channel_doesnt_want_writes);
  UNMOCK(scheduler_release_channel);

  return;
}

/**
 * Normal channel lifecycle test:
 *
 * OPENING->OPEN->MAINT->OPEN->CLOSING->CLOSED
 */

static void
test_channel_lifecycle(void *arg)
{
  channel_t *ch1 = NULL, *ch2 = NULL;
  cell_t *cell = NULL;
  int old_count, init_doesnt_want_writes_count;
  int init_releases_count;

  (void)arg;

  /* Mock these for the whole lifecycle test */
  MOCK(scheduler_channel_doesnt_want_writes,
       scheduler_channel_doesnt_want_writes_mock);
  MOCK(scheduler_release_channel,
       scheduler_release_channel_mock);

  /* Cache some initial counter values */
  init_doesnt_want_writes_count = test_doesnt_want_writes_count;
  init_releases_count = test_releases_count;

  /* Accept cells to lower layer */
  test_chan_accept_cells = 1;
  /* Use default overhead factor */
  test_overhead_estimate = 1.0f;

  ch1 = new_fake_channel();
  tt_assert(ch1);
  /* Start it off in OPENING */
  ch1->state = CHANNEL_STATE_OPENING;
  /* We'll need a cmux */
  ch1->cmux = circuitmux_alloc();

  /* Try to register it */
  channel_register(ch1);
  tt_assert(ch1->registered);

  /* Try to write a cell through (should queue) */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  old_count = test_cells_written;
  channel_write_cell(ch1, cell);
  tt_int_op(old_count, ==, test_cells_written);

  /* Move it to OPEN and flush */
  channel_change_state(ch1, CHANNEL_STATE_OPEN);

  /* Queue should drain */
  tt_int_op(old_count + 1, ==, test_cells_written);

  /* Get another one */
  ch2 = new_fake_channel();
  tt_assert(ch2);
  ch2->state = CHANNEL_STATE_OPENING;
  ch2->cmux = circuitmux_alloc();

  /* Register */
  channel_register(ch2);
  tt_assert(ch2->registered);

  /* Check counters */
  tt_int_op(test_doesnt_want_writes_count, ==, init_doesnt_want_writes_count);
  tt_int_op(test_releases_count, ==, init_releases_count);

  /* Move ch1 to MAINT */
  channel_change_state(ch1, CHANNEL_STATE_MAINT);
  tt_int_op(test_doesnt_want_writes_count, ==,
            init_doesnt_want_writes_count + 1);
  tt_int_op(test_releases_count, ==, init_releases_count);

  /* Move ch2 to OPEN */
  channel_change_state(ch2, CHANNEL_STATE_OPEN);
  tt_int_op(test_doesnt_want_writes_count, ==,
            init_doesnt_want_writes_count + 1);
  tt_int_op(test_releases_count, ==, init_releases_count);

  /* Move ch1 back to OPEN */
  channel_change_state(ch1, CHANNEL_STATE_OPEN);
  tt_int_op(test_doesnt_want_writes_count, ==,
            init_doesnt_want_writes_count + 1);
  tt_int_op(test_releases_count, ==, init_releases_count);

  /* Mark ch2 for close */
  channel_mark_for_close(ch2);
  tt_int_op(ch2->state, ==, CHANNEL_STATE_CLOSING);
  tt_int_op(test_doesnt_want_writes_count, ==,
            init_doesnt_want_writes_count + 1);
  tt_int_op(test_releases_count, ==, init_releases_count + 1);

  /* Shut down channels */
  channel_free_all();
  ch1 = ch2 = NULL;
  tt_int_op(test_doesnt_want_writes_count, ==,
            init_doesnt_want_writes_count + 1);
  /* channel_free() calls scheduler_release_channel() */
  tt_int_op(test_releases_count, ==, init_releases_count + 4);

 done:
  free_fake_channel(ch1);
  free_fake_channel(ch2);

  UNMOCK(scheduler_channel_doesnt_want_writes);
  UNMOCK(scheduler_release_channel);

  return;
}

/**
 * Weird channel lifecycle test:
 *
 * OPENING->CLOSING->CLOSED
 * OPENING->OPEN->CLOSING->ERROR
 * OPENING->OPEN->MAINT->CLOSING->CLOSED
 * OPENING->OPEN->MAINT->CLOSING->ERROR
 */

static void
test_channel_lifecycle_2(void *arg)
{
  channel_t *ch = NULL;

  (void)arg;

  /* Mock these for the whole lifecycle test */
  MOCK(scheduler_channel_doesnt_want_writes,
       scheduler_channel_doesnt_want_writes_mock);
  MOCK(scheduler_release_channel,
       scheduler_release_channel_mock);

  /* Accept cells to lower layer */
  test_chan_accept_cells = 1;
  /* Use default overhead factor */
  test_overhead_estimate = 1.0f;

  ch = new_fake_channel();
  tt_assert(ch);
  /* Start it off in OPENING */
  ch->state = CHANNEL_STATE_OPENING;
  /* The full lifecycle test needs a cmux */
  ch->cmux = circuitmux_alloc();

  /* Try to register it */
  channel_register(ch);
  tt_assert(ch->registered);

  /* Try to close it */
  channel_mark_for_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSING);

  /* Finish closing it */
  chan_test_finish_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSED);
  channel_run_cleanup();
  ch = NULL;

  /* Now try OPENING->OPEN->CLOSING->ERROR */
  ch = new_fake_channel();
  tt_assert(ch);
  ch->state = CHANNEL_STATE_OPENING;
  ch->cmux = circuitmux_alloc();
  channel_register(ch);
  tt_assert(ch->registered);

  /* Finish opening it */
  channel_change_state(ch, CHANNEL_STATE_OPEN);

  /* Error exit from lower layer */
  chan_test_error(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSING);
  chan_test_finish_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_ERROR);
  channel_run_cleanup();
  ch = NULL;

  /* OPENING->OPEN->MAINT->CLOSING->CLOSED close from maintenance state */
  ch = new_fake_channel();
  tt_assert(ch);
  ch->state = CHANNEL_STATE_OPENING;
  ch->cmux = circuitmux_alloc();
  channel_register(ch);
  tt_assert(ch->registered);

  /* Finish opening it */
  channel_change_state(ch, CHANNEL_STATE_OPEN);
  tt_int_op(ch->state, ==, CHANNEL_STATE_OPEN);

  /* Go to maintenance state */
  channel_change_state(ch, CHANNEL_STATE_MAINT);
  tt_int_op(ch->state, ==, CHANNEL_STATE_MAINT);

  /* Lower layer close */
  channel_mark_for_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSING);

  /* Finish */
  chan_test_finish_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSED);
  channel_run_cleanup();
  ch = NULL;

  /*
   * OPENING->OPEN->MAINT->CLOSING->CLOSED lower-layer close during
   * maintenance state
   */
  ch = new_fake_channel();
  tt_assert(ch);
  ch->state = CHANNEL_STATE_OPENING;
  ch->cmux = circuitmux_alloc();
  channel_register(ch);
  tt_assert(ch->registered);

  /* Finish opening it */
  channel_change_state(ch, CHANNEL_STATE_OPEN);
  tt_int_op(ch->state, ==, CHANNEL_STATE_OPEN);

  /* Go to maintenance state */
  channel_change_state(ch, CHANNEL_STATE_MAINT);
  tt_int_op(ch->state, ==, CHANNEL_STATE_MAINT);

  /* Lower layer close */
  channel_close_from_lower_layer(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSING);

  /* Finish */
  chan_test_finish_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSED);
  channel_run_cleanup();
  ch = NULL;

  /* OPENING->OPEN->MAINT->CLOSING->ERROR */
  ch = new_fake_channel();
  tt_assert(ch);
  ch->state = CHANNEL_STATE_OPENING;
  ch->cmux = circuitmux_alloc();
  channel_register(ch);
  tt_assert(ch->registered);

  /* Finish opening it */
  channel_change_state(ch, CHANNEL_STATE_OPEN);
  tt_int_op(ch->state, ==, CHANNEL_STATE_OPEN);

  /* Go to maintenance state */
  channel_change_state(ch, CHANNEL_STATE_MAINT);
  tt_int_op(ch->state, ==, CHANNEL_STATE_MAINT);

  /* Lower layer close */
  chan_test_error(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_CLOSING);

  /* Finish */
  chan_test_finish_close(ch);
  tt_int_op(ch->state, ==, CHANNEL_STATE_ERROR);
  channel_run_cleanup();
  ch = NULL;

  /* Shut down channels */
  channel_free_all();

 done:
  tor_free(ch);

  UNMOCK(scheduler_channel_doesnt_want_writes);
  UNMOCK(scheduler_release_channel);

  return;
}

static void
test_channel_multi(void *arg)
{
  channel_t *ch1 = NULL, *ch2 = NULL;
  uint64_t global_queue_estimate;
  cell_t *cell = NULL;

  (void)arg;

  /* Accept cells to lower layer */
  test_chan_accept_cells = 1;
  /* Use default overhead factor */
  test_overhead_estimate = 1.0f;

  ch1 = new_fake_channel();
  tt_assert(ch1);
  ch2 = new_fake_channel();
  tt_assert(ch2);

  /* Initial queue size update */
  channel_update_xmit_queue_size(ch1);
  tt_u64_op(ch1->bytes_queued_for_xmit, ==, 0);
  channel_update_xmit_queue_size(ch2);
  tt_u64_op(ch2->bytes_queued_for_xmit, ==, 0);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 0);

  /* Queue some cells, check queue estimates */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  channel_write_cell(ch1, cell);

  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  channel_write_cell(ch2, cell);

  channel_update_xmit_queue_size(ch1);
  channel_update_xmit_queue_size(ch2);
  tt_u64_op(ch1->bytes_queued_for_xmit, ==, 0);
  tt_u64_op(ch2->bytes_queued_for_xmit, ==, 0);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 0);

  /* Stop accepting cells at lower layer */
  test_chan_accept_cells = 0;

  /* Queue some cells and check queue estimates */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  channel_write_cell(ch1, cell);

  channel_update_xmit_queue_size(ch1);
  tt_u64_op(ch1->bytes_queued_for_xmit, ==, 512);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 512);

  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  channel_write_cell(ch2, cell);

  channel_update_xmit_queue_size(ch2);
  tt_u64_op(ch2->bytes_queued_for_xmit, ==, 512);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 1024);

  /* Allow cells through again */
  test_chan_accept_cells = 1;

  /* Flush chan 2 */
  channel_flush_cells(ch2);

  /* Update and check queue sizes */
  channel_update_xmit_queue_size(ch1);
  channel_update_xmit_queue_size(ch2);
  tt_u64_op(ch1->bytes_queued_for_xmit, ==, 512);
  tt_u64_op(ch2->bytes_queued_for_xmit, ==, 0);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 512);

  /* Flush chan 1 */
  channel_flush_cells(ch1);

  /* Update and check queue sizes */
  channel_update_xmit_queue_size(ch1);
  channel_update_xmit_queue_size(ch2);
  tt_u64_op(ch1->bytes_queued_for_xmit, ==, 0);
  tt_u64_op(ch2->bytes_queued_for_xmit, ==, 0);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 0);

  /* Now block again */
  test_chan_accept_cells = 0;

  /* Queue some cells */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  channel_write_cell(ch1, cell);

  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  channel_write_cell(ch2, cell);
  cell = NULL;

  /* Check the estimates */
  channel_update_xmit_queue_size(ch1);
  channel_update_xmit_queue_size(ch2);
  tt_u64_op(ch1->bytes_queued_for_xmit, ==, 512);
  tt_u64_op(ch2->bytes_queued_for_xmit, ==, 512);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 1024);

  /* Now close channel 2; it should be subtracted from the global queue */
  MOCK(scheduler_release_channel, scheduler_release_channel_mock);
  channel_mark_for_close(ch2);
  UNMOCK(scheduler_release_channel);

  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 512);

  /*
   * Since the fake channels aren't registered, channel_free_all() can't
   * see them properly.
   */
  MOCK(scheduler_release_channel, scheduler_release_channel_mock);
  channel_mark_for_close(ch1);
  UNMOCK(scheduler_release_channel);

  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 0);

  /* Now free everything */
  MOCK(scheduler_release_channel, scheduler_release_channel_mock);
  channel_free_all();
  UNMOCK(scheduler_release_channel);

 done:
  free_fake_channel(ch1);
  free_fake_channel(ch2);

  return;
}

/**
 * Check some hopefully-impossible edge cases in the channel queue we
 * can only trigger by doing evil things to the queue directly.
 */

static void
test_channel_queue_impossible(void *arg)
{
  channel_t *ch = NULL;
  cell_t *cell = NULL;
  packed_cell_t *packed_cell = NULL;
  var_cell_t *var_cell = NULL;
  int old_count;
  cell_queue_entry_t *q = NULL;
  uint64_t global_queue_estimate;
  uintptr_t cellintptr;

  /* Cache the global queue size (see below) */
  global_queue_estimate = channel_get_global_queue_estimate();

  (void)arg;

  ch = new_fake_channel();
  tt_assert(ch);

  /* We test queueing here; tell it not to accept cells */
  test_chan_accept_cells = 0;
  /* ...and keep it from trying to flush the queue */
  ch->state = CHANNEL_STATE_MAINT;

  /* Cache the cell written count */
  old_count = test_cells_written;

  /* Assert that the queue is initially empty */
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 0);

  /* Get a fresh cell and write it to the channel*/
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  cellintptr = (uintptr_t)(void*)cell;
  channel_write_cell(ch, cell);

  /* Now it should be queued */
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 1);
  q = TOR_SIMPLEQ_FIRST(&(ch->outgoing_queue));
  tt_assert(q);
  if (q) {
    tt_int_op(q->type, ==, CELL_QUEUE_FIXED);
    tt_assert((uintptr_t)q->u.fixed.cell == cellintptr);
  }
  /* Do perverse things to it */
  tor_free(q->u.fixed.cell);
  q->u.fixed.cell = NULL;

  /*
   * Now change back to open with channel_change_state() and assert that it
   * gets thrown away properly.
   */
  test_chan_accept_cells = 1;
  channel_change_state(ch, CHANNEL_STATE_OPEN);
  tt_assert(test_cells_written == old_count);
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 0);

  /* Same thing but for a var_cell */

  test_chan_accept_cells = 0;
  ch->state = CHANNEL_STATE_MAINT;
  var_cell = tor_malloc_zero(sizeof(var_cell_t) + CELL_PAYLOAD_SIZE);
  make_fake_var_cell(var_cell);
  cellintptr = (uintptr_t)(void*)var_cell;
  channel_write_var_cell(ch, var_cell);

  /* Check that it's queued */
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 1);
  q = TOR_SIMPLEQ_FIRST(&(ch->outgoing_queue));
  tt_assert(q);
  if (q) {
    tt_int_op(q->type, ==, CELL_QUEUE_VAR);
    tt_assert((uintptr_t)q->u.var.var_cell == cellintptr);
  }

  /* Remove the cell from the queue entry */
  tor_free(q->u.var.var_cell);
  q->u.var.var_cell = NULL;

  /* Let it drain and check that the bad entry is discarded */
  test_chan_accept_cells = 1;
  channel_change_state(ch, CHANNEL_STATE_OPEN);
  tt_assert(test_cells_written == old_count);
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 0);

  /* Same thing with a packed_cell */

  test_chan_accept_cells = 0;
  ch->state = CHANNEL_STATE_MAINT;
  packed_cell = packed_cell_new();
  tt_assert(packed_cell);
  cellintptr = (uintptr_t)(void*)packed_cell;
  channel_write_packed_cell(ch, packed_cell);

  /* Check that it's queued */
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 1);
  q = TOR_SIMPLEQ_FIRST(&(ch->outgoing_queue));
  tt_assert(q);
  if (q) {
    tt_int_op(q->type, ==, CELL_QUEUE_PACKED);
    tt_assert((uintptr_t)q->u.packed.packed_cell == cellintptr);
  }

  /* Remove the cell from the queue entry */
  packed_cell_free(q->u.packed.packed_cell);
  q->u.packed.packed_cell = NULL;

  /* Let it drain and check that the bad entry is discarded */
  test_chan_accept_cells = 1;
  channel_change_state(ch, CHANNEL_STATE_OPEN);
  tt_assert(test_cells_written == old_count);
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 0);

  /* Unknown cell type case */
  test_chan_accept_cells = 0;
  ch->state = CHANNEL_STATE_MAINT;
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  cellintptr = (uintptr_t)(void*)cell;
  channel_write_cell(ch, cell);

  /* Check that it's queued */
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 1);
  q = TOR_SIMPLEQ_FIRST(&(ch->outgoing_queue));
  tt_assert(q);
  if (q) {
    tt_int_op(q->type, ==, CELL_QUEUE_FIXED);
    tt_assert((uintptr_t)q->u.fixed.cell == cellintptr);
  }
  /* Clobber it, including the queue entry type */
  tor_free(q->u.fixed.cell);
  q->u.fixed.cell = NULL;
  q->type = CELL_QUEUE_PACKED + 1;

  /* Let it drain and check that the bad entry is discarded */
  test_chan_accept_cells = 1;
  channel_change_state(ch, CHANNEL_STATE_OPEN);
  tt_assert(test_cells_written == old_count);
  tt_int_op(chan_cell_queue_len(&(ch->outgoing_queue)), ==, 0);

 done:
  free_fake_channel(ch);

  /*
   * Doing that meant that we couldn't correctly adjust the queue size
   * for the var cell, so manually reset the global queue size estimate
   * so the next test doesn't break if we run with --no-fork.
   */
  estimated_total_queue_size = global_queue_estimate;

  return;
}

static void
test_channel_queue_size(void *arg)
{
  channel_t *ch = NULL;
  cell_t *cell = NULL;
  int n, old_count;
  uint64_t global_queue_estimate;

  (void)arg;

  ch = new_fake_channel();
  tt_assert(ch);

  /* Initial queue size update */
  channel_update_xmit_queue_size(ch);
  tt_u64_op(ch->bytes_queued_for_xmit, ==, 0);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 0);

  /* Test the call-through to our fake lower layer */
  n = channel_num_cells_writeable(ch);
  /* chan_test_num_cells_writeable() always returns 32 */
  tt_int_op(n, ==, 32);

  /*
   * Now we queue some cells and check that channel_num_cells_writeable()
   * adjusts properly
   */

  /* tell it not to accept cells */
  test_chan_accept_cells = 0;
  /* ...and keep it from trying to flush the queue */
  ch->state = CHANNEL_STATE_MAINT;

  /* Get a fresh cell */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);

  old_count = test_cells_written;
  channel_write_cell(ch, cell);
  /* Assert that it got queued, not written through, correctly */
  tt_int_op(test_cells_written, ==, old_count);

  /* Now check chan_test_num_cells_writeable() again */
  n = channel_num_cells_writeable(ch);
  tt_int_op(n, ==, 0); /* Should return 0 since we're in CHANNEL_STATE_MAINT */

  /* Update queue size estimates */
  channel_update_xmit_queue_size(ch);
  /* One cell, times an overhead factor of 1.0 */
  tt_u64_op(ch->bytes_queued_for_xmit, ==, 512);
  /* Try a different overhead factor */
  test_overhead_estimate = 0.5f;
  /* This one should be ignored since it's below 1.0 */
  channel_update_xmit_queue_size(ch);
  tt_u64_op(ch->bytes_queued_for_xmit, ==, 512);
  /* Now try a larger one */
  test_overhead_estimate = 2.0f;
  channel_update_xmit_queue_size(ch);
  tt_u64_op(ch->bytes_queued_for_xmit, ==, 1024);
  /* Go back to 1.0 */
  test_overhead_estimate = 1.0f;
  channel_update_xmit_queue_size(ch);
  tt_u64_op(ch->bytes_queued_for_xmit, ==, 512);
  /* Check the global estimate too */
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 512);

  /* Go to open */
  old_count = test_cells_written;
  channel_change_state(ch, CHANNEL_STATE_OPEN);

  /*
   * It should try to write, but we aren't accepting cells right now, so
   * it'll requeue
   */
  tt_int_op(test_cells_written, ==, old_count);

  /* Check the queue size again */
  channel_update_xmit_queue_size(ch);
  tt_u64_op(ch->bytes_queued_for_xmit, ==, 512);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 512);

  /*
   * Now the cell is in the queue, and we're open, so we should get 31
   * writeable cells.
   */
  n = channel_num_cells_writeable(ch);
  tt_int_op(n, ==, 31);

  /* Accept cells again */
  test_chan_accept_cells = 1;
  /* ...and re-process the queue */
  old_count = test_cells_written;
  channel_flush_cells(ch);
  tt_int_op(test_cells_written, ==, old_count + 1);

  /* Should have 32 writeable now */
  n = channel_num_cells_writeable(ch);
  tt_int_op(n, ==, 32);

  /* Should have queue size estimate of zero */
  channel_update_xmit_queue_size(ch);
  tt_u64_op(ch->bytes_queued_for_xmit, ==, 0);
  global_queue_estimate = channel_get_global_queue_estimate();
  tt_u64_op(global_queue_estimate, ==, 0);

  /* Okay, now we're done with this one */
  MOCK(scheduler_release_channel, scheduler_release_channel_mock);
  channel_mark_for_close(ch);
  UNMOCK(scheduler_release_channel);

 done:
  free_fake_channel(ch);

  return;
}

static void
test_channel_write(void *arg)
{
  channel_t *ch = NULL;
  cell_t *cell = tor_malloc_zero(sizeof(cell_t));
  packed_cell_t *packed_cell = NULL;
  var_cell_t *var_cell =
    tor_malloc_zero(sizeof(var_cell_t) + CELL_PAYLOAD_SIZE);
  int old_count;

  (void)arg;

  packed_cell = packed_cell_new();
  tt_assert(packed_cell);

  ch = new_fake_channel();
  tt_assert(ch);
  make_fake_cell(cell);
  make_fake_var_cell(var_cell);

  /* Tell it to accept cells */
  test_chan_accept_cells = 1;

  old_count = test_cells_written;
  channel_write_cell(ch, cell);
  cell = NULL;
  tt_assert(test_cells_written == old_count + 1);

  channel_write_var_cell(ch, var_cell);
  var_cell = NULL;
  tt_assert(test_cells_written == old_count + 2);

  channel_write_packed_cell(ch, packed_cell);
  packed_cell = NULL;
  tt_assert(test_cells_written == old_count + 3);

  /* Now we test queueing; tell it not to accept cells */
  test_chan_accept_cells = 0;
  /* ...and keep it from trying to flush the queue */
  ch->state = CHANNEL_STATE_MAINT;

  /* Get a fresh cell */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);

  old_count = test_cells_written;
  channel_write_cell(ch, cell);
  tt_assert(test_cells_written == old_count);

  /*
   * Now change back to open with channel_change_state() and assert that it
   * gets drained from the queue.
   */
  test_chan_accept_cells = 1;
  channel_change_state(ch, CHANNEL_STATE_OPEN);
  tt_assert(test_cells_written == old_count + 1);

  /*
   * Check the note destroy case
   */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  cell->command = CELL_DESTROY;

  /* Set up the mock */
  MOCK(channel_note_destroy_not_pending,
       channel_note_destroy_not_pending_mock);

  old_count = test_destroy_not_pending_calls;
  channel_write_cell(ch, cell);
  tt_assert(test_destroy_not_pending_calls == old_count + 1);

  /* Now send a non-destroy and check we don't call it */
  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  channel_write_cell(ch, cell);
  tt_assert(test_destroy_not_pending_calls == old_count + 1);

  UNMOCK(channel_note_destroy_not_pending);

  /*
   * Now switch it to CLOSING so we can test the discard-cells case
   * in the channel_write_*() functions.
   */
  MOCK(scheduler_release_channel, scheduler_release_channel_mock);
  channel_mark_for_close(ch);
  UNMOCK(scheduler_release_channel);

  /* Send cells that will drop in the closing state */
  old_count = test_cells_written;

  cell = tor_malloc_zero(sizeof(cell_t));
  make_fake_cell(cell);
  channel_write_cell(ch, cell);
  cell = NULL;
  tt_assert(test_cells_written == old_count);

  var_cell = tor_malloc_zero(sizeof(var_cell_t) + CELL_PAYLOAD_SIZE);
  make_fake_var_cell(var_cell);
  channel_write_var_cell(ch, var_cell);
  var_cell = NULL;
  tt_assert(test_cells_written == old_count);

  packed_cell = packed_cell_new();
  channel_write_packed_cell(ch, packed_cell);
  packed_cell = NULL;
  tt_assert(test_cells_written == old_count);

 done:
  free_fake_channel(ch);
  tor_free(var_cell);
  tor_free(cell);
  packed_cell_free(packed_cell);
  return;
}

struct testcase_t channel_tests[] = {
  { "dumpstats", test_channel_dumpstats, TT_FORK, NULL, NULL },
  { "flush", test_channel_flush, TT_FORK, NULL, NULL },
  { "flushmux", test_channel_flushmux, TT_FORK, NULL, NULL },
  { "incoming", test_channel_incoming, TT_FORK, NULL, NULL },
  { "lifecycle", test_channel_lifecycle, TT_FORK, NULL, NULL },
  { "lifecycle_2", test_channel_lifecycle_2, TT_FORK, NULL, NULL },
  { "multi", test_channel_multi, TT_FORK, NULL, NULL },
  { "queue_impossible", test_channel_queue_impossible, TT_FORK, NULL, NULL },
  { "queue_size", test_channel_queue_size, TT_FORK, NULL, NULL },
  { "write", test_channel_write, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

