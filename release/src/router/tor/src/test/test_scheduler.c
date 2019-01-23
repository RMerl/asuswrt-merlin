/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"

#include <math.h>
#include <event2/event.h>

#define TOR_CHANNEL_INTERNAL_
#define CHANNEL_PRIVATE_
#include "or.h"
#include "compat_libevent.h"
#include "channel.h"
#define SCHEDULER_PRIVATE_
#include "scheduler.h"

/* Test suite stuff */
#include "test.h"
#include "fakechans.h"

/* Event base for scheduelr tests */
static struct event_base *mock_event_base = NULL;

/* Statics controlling mocks */
static circuitmux_t *mock_ccm_tgt_1 = NULL;
static circuitmux_t *mock_ccm_tgt_2 = NULL;

static circuitmux_t *mock_cgp_tgt_1 = NULL;
static circuitmux_policy_t *mock_cgp_val_1 = NULL;
static circuitmux_t *mock_cgp_tgt_2 = NULL;
static circuitmux_policy_t *mock_cgp_val_2 = NULL;
static int scheduler_compare_channels_mock_ctr = 0;
static int scheduler_run_mock_ctr = 0;

static void channel_flush_some_cells_mock_free_all(void);
static void channel_flush_some_cells_mock_set(channel_t *chan,
                                              ssize_t num_cells);

/* Setup for mock event stuff */
static void mock_event_free_all(void);
static void mock_event_init(void);

/* Mocks used by scheduler tests */
static ssize_t channel_flush_some_cells_mock(channel_t *chan,
                                             ssize_t num_cells);
static int circuitmux_compare_muxes_mock(circuitmux_t *cmux_1,
                                         circuitmux_t *cmux_2);
static const circuitmux_policy_t * circuitmux_get_policy_mock(
    circuitmux_t *cmux);
static int scheduler_compare_channels_mock(const void *c1_v,
                                           const void *c2_v);
static void scheduler_run_noop_mock(void);
static struct event_base * tor_libevent_get_base_mock(void);

/* Scheduler test cases */
static void test_scheduler_channel_states(void *arg);
static void test_scheduler_compare_channels(void *arg);
static void test_scheduler_initfree(void *arg);
static void test_scheduler_loop(void *arg);
static void test_scheduler_queue_heuristic(void *arg);

/* Mock event init/free */

/* Shamelessly stolen from compat_libevent.c */
#define V(major, minor, patch) \
  (((major) << 24) | ((minor) << 16) | ((patch) << 8))

static void
mock_event_free_all(void)
{
  tt_assert(mock_event_base != NULL);

  if (mock_event_base) {
    event_base_free(mock_event_base);
    mock_event_base = NULL;
  }

  tt_ptr_op(mock_event_base, ==, NULL);

 done:
  return;
}

static void
mock_event_init(void)
{
  struct event_config *cfg = NULL;

  tt_ptr_op(mock_event_base, ==, NULL);

  /*
   * Really cut down from tor_libevent_initialize of
   * src/common/compat_libevent.c to kill config dependencies
   */

  if (!mock_event_base) {
    cfg = event_config_new();
#if LIBEVENT_VERSION_NUMBER >= V(2,0,9)
    /* We can enable changelist support with epoll, since we don't give
     * Libevent any dup'd fds.  This lets us avoid some syscalls. */
    event_config_set_flag(cfg, EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST);
#endif
    mock_event_base = event_base_new_with_config(cfg);
    event_config_free(cfg);
  }

  tt_assert(mock_event_base != NULL);

 done:
  return;
}

/* Mocks */

typedef struct {
  const channel_t *chan;
  ssize_t cells;
} flush_mock_channel_t;

static smartlist_t *chans_for_flush_mock = NULL;

static void
channel_flush_some_cells_mock_free_all(void)
{
  if (chans_for_flush_mock) {
    SMARTLIST_FOREACH_BEGIN(chans_for_flush_mock,
                            flush_mock_channel_t *,
                            flush_mock_ch) {
      SMARTLIST_DEL_CURRENT(chans_for_flush_mock, flush_mock_ch);
      tor_free(flush_mock_ch);
    } SMARTLIST_FOREACH_END(flush_mock_ch);

    smartlist_free(chans_for_flush_mock);
    chans_for_flush_mock = NULL;
  }
}

static void
channel_flush_some_cells_mock_set(channel_t *chan, ssize_t num_cells)
{
  int found = 0;

  if (!chan) return;
  if (num_cells <= 0) return;

  if (!chans_for_flush_mock) {
    chans_for_flush_mock = smartlist_new();
  }

  SMARTLIST_FOREACH_BEGIN(chans_for_flush_mock,
                          flush_mock_channel_t *,
                          flush_mock_ch) {
    if (flush_mock_ch != NULL && flush_mock_ch->chan != NULL) {
      if (flush_mock_ch->chan == chan) {
        /* Found it */
        flush_mock_ch->cells = num_cells;
        found = 1;
        break;
      }
    } else {
      /* That shouldn't be there... */
      SMARTLIST_DEL_CURRENT(chans_for_flush_mock, flush_mock_ch);
      tor_free(flush_mock_ch);
    }
  } SMARTLIST_FOREACH_END(flush_mock_ch);

  if (! found) {
    /* The loop didn't find it */
    flush_mock_channel_t *flush_mock_ch;
    flush_mock_ch = tor_malloc_zero(sizeof(*flush_mock_ch));
    flush_mock_ch->chan = chan;
    flush_mock_ch->cells = num_cells;
    smartlist_add(chans_for_flush_mock, flush_mock_ch);
  }
}

static ssize_t
channel_flush_some_cells_mock(channel_t *chan, ssize_t num_cells)
{
  ssize_t flushed = 0, max;
  char unlimited = 0;
  flush_mock_channel_t *found = NULL;

  tt_assert(chan != NULL);
  if (chan) {
    if (num_cells < 0) {
      num_cells = 0;
      unlimited = 1;
    }

    /* Check if we have it */
    if (chans_for_flush_mock != NULL) {
      SMARTLIST_FOREACH_BEGIN(chans_for_flush_mock,
                              flush_mock_channel_t *,
                              flush_mock_ch) {
        if (flush_mock_ch != NULL && flush_mock_ch->chan != NULL) {
          if (flush_mock_ch->chan == chan) {
            /* Found it */
            found = flush_mock_ch;
            break;
          }
        } else {
          /* That shouldn't be there... */
          SMARTLIST_DEL_CURRENT(chans_for_flush_mock, flush_mock_ch);
          tor_free(flush_mock_ch);
        }
      } SMARTLIST_FOREACH_END(flush_mock_ch);

      if (found) {
        /* We found one */
        if (found->cells < 0) found->cells = 0;

        if (unlimited) max = found->cells;
        else max = MIN(found->cells, num_cells);

        flushed += max;
        found->cells -= max;

        if (found->cells <= 0) {
          smartlist_remove(chans_for_flush_mock, found);
          tor_free(found);
        }
      }
    }
  }

 done:
  return flushed;
}

static int
circuitmux_compare_muxes_mock(circuitmux_t *cmux_1,
                              circuitmux_t *cmux_2)
{
  int result = 0;

  tt_assert(cmux_1 != NULL);
  tt_assert(cmux_2 != NULL);

  if (cmux_1 != cmux_2) {
    if (cmux_1 == mock_ccm_tgt_1 && cmux_2 == mock_ccm_tgt_2) result = -1;
    else if (cmux_1 == mock_ccm_tgt_2 && cmux_2 == mock_ccm_tgt_1) {
      result = 1;
    } else {
      if (cmux_1 == mock_ccm_tgt_1 || cmux_1 == mock_ccm_tgt_2) result = -1;
      else if (cmux_2 == mock_ccm_tgt_1 || cmux_2 == mock_ccm_tgt_2) {
        result = 1;
      } else {
        result = circuitmux_compare_muxes__real(cmux_1, cmux_2);
      }
    }
  }
  /* else result = 0 always */

 done:
  return result;
}

static const circuitmux_policy_t *
circuitmux_get_policy_mock(circuitmux_t *cmux)
{
  const circuitmux_policy_t *result = NULL;

  tt_assert(cmux != NULL);
  if (cmux) {
    if (cmux == mock_cgp_tgt_1) result = mock_cgp_val_1;
    else if (cmux == mock_cgp_tgt_2) result = mock_cgp_val_2;
    else result = circuitmux_get_policy__real(cmux);
  }

 done:
  return result;
}

static int
scheduler_compare_channels_mock(const void *c1_v,
                                const void *c2_v)
{
  uintptr_t p1, p2;

  p1 = (uintptr_t)(c1_v);
  p2 = (uintptr_t)(c2_v);

  ++scheduler_compare_channels_mock_ctr;

  if (p1 == p2) return 0;
  else if (p1 < p2) return 1;
  else return -1;
}

static void
scheduler_run_noop_mock(void)
{
  ++scheduler_run_mock_ctr;
}

static struct event_base *
tor_libevent_get_base_mock(void)
{
  return mock_event_base;
}

/* Test cases */

static void
test_scheduler_channel_states(void *arg)
{
  channel_t *ch1 = NULL, *ch2 = NULL;
  int old_count;

  (void)arg;

  /* Set up libevent and scheduler */

  mock_event_init();
  MOCK(tor_libevent_get_base, tor_libevent_get_base_mock);
  scheduler_init();
  /*
   * Install the compare channels mock so we can test
   * scheduler_touch_channel().
   */
  MOCK(scheduler_compare_channels, scheduler_compare_channels_mock);
  /*
   * Disable scheduler_run so we can just check the state transitions
   * without having to make everything it might call work too.
   */
  MOCK(scheduler_run, scheduler_run_noop_mock);

  tt_int_op(smartlist_len(channels_pending), ==, 0);

  /* Set up a fake channel */
  ch1 = new_fake_channel();
  tt_assert(ch1);

  /* Start it off in OPENING */
  ch1->state = CHANNEL_STATE_OPENING;
  /* We'll need a cmux */
  ch1->cmux = circuitmux_alloc();
  /* Try to register it */
  channel_register(ch1);
  tt_assert(ch1->registered);

  /* It should start off in SCHED_CHAN_IDLE */
  tt_int_op(ch1->scheduler_state, ==, SCHED_CHAN_IDLE);

  /* Now get another one */
  ch2 = new_fake_channel();
  tt_assert(ch2);
  ch2->state = CHANNEL_STATE_OPENING;
  ch2->cmux = circuitmux_alloc();
  channel_register(ch2);
  tt_assert(ch2->registered);

  /* Send it to SCHED_CHAN_WAITING_TO_WRITE */
  scheduler_channel_has_waiting_cells(ch1);
  tt_int_op(ch1->scheduler_state, ==, SCHED_CHAN_WAITING_TO_WRITE);

  /* This should send it to SCHED_CHAN_PENDING */
  scheduler_channel_wants_writes(ch1);
  tt_int_op(ch1->scheduler_state, ==, SCHED_CHAN_PENDING);
  tt_int_op(smartlist_len(channels_pending), ==, 1);

  /* Now send ch2 to SCHED_CHAN_WAITING_FOR_CELLS */
  scheduler_channel_wants_writes(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_WAITING_FOR_CELLS);

  /* Drop ch2 back to idle */
  scheduler_channel_doesnt_want_writes(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_IDLE);

  /* ...and back to SCHED_CHAN_WAITING_FOR_CELLS */
  scheduler_channel_wants_writes(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_WAITING_FOR_CELLS);

  /* ...and this should kick ch2 into SCHED_CHAN_PENDING */
  scheduler_channel_has_waiting_cells(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_PENDING);
  tt_int_op(smartlist_len(channels_pending), ==, 2);

  /* This should send ch2 to SCHED_CHAN_WAITING_TO_WRITE */
  scheduler_channel_doesnt_want_writes(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_WAITING_TO_WRITE);
  tt_int_op(smartlist_len(channels_pending), ==, 1);

  /* ...and back to SCHED_CHAN_PENDING */
  scheduler_channel_wants_writes(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_PENDING);
  tt_int_op(smartlist_len(channels_pending), ==, 2);

  /* Now we exercise scheduler_touch_channel */
  old_count = scheduler_compare_channels_mock_ctr;
  scheduler_touch_channel(ch1);
  tt_assert(scheduler_compare_channels_mock_ctr > old_count);

  /* Close */
  channel_mark_for_close(ch1);
  tt_int_op(ch1->state, ==, CHANNEL_STATE_CLOSING);
  channel_mark_for_close(ch2);
  tt_int_op(ch2->state, ==, CHANNEL_STATE_CLOSING);
  channel_closed(ch1);
  tt_int_op(ch1->state, ==, CHANNEL_STATE_CLOSED);
  ch1 = NULL;
  channel_closed(ch2);
  tt_int_op(ch2->state, ==, CHANNEL_STATE_CLOSED);
  ch2 = NULL;

  /* Shut things down */

  channel_free_all();
  scheduler_free_all();
  mock_event_free_all();

 done:
  tor_free(ch1);
  tor_free(ch2);

  UNMOCK(scheduler_compare_channels);
  UNMOCK(scheduler_run);
  UNMOCK(tor_libevent_get_base);

  return;
}

static void
test_scheduler_compare_channels(void *arg)
{
  /* We don't actually need whole fake channels... */
  channel_t c1, c2;
  /* ...and some dummy circuitmuxes too */
  circuitmux_t *cm1 = NULL, *cm2 = NULL;
  int result;

  (void)arg;

  /* We can't actually see sizeof(circuitmux_t) from here */
  cm1 = tor_malloc_zero(sizeof(void *));
  cm2 = tor_malloc_zero(sizeof(void *));

  c1.cmux = cm1;
  c2.cmux = cm2;

  /* Configure circuitmux_get_policy() mock */
  mock_cgp_tgt_1 = cm1;
  mock_cgp_tgt_2 = cm2;

  /*
   * This is to test the different-policies case, which uses the policy
   * cast to an uintptr_t as an arbitrary but definite thing to compare.
   */
  mock_cgp_val_1 = tor_malloc_zero(16);
  mock_cgp_val_2 = tor_malloc_zero(16);
  if ( ((uintptr_t) mock_cgp_val_1) > ((uintptr_t) mock_cgp_val_2) ) {
    void *tmp = mock_cgp_val_1;
    mock_cgp_val_1 = mock_cgp_val_2;
    mock_cgp_val_2 = tmp;
  }

  MOCK(circuitmux_get_policy, circuitmux_get_policy_mock);

  /* Now set up circuitmux_compare_muxes() mock using cm1/cm2 */
  mock_ccm_tgt_1 = cm1;
  mock_ccm_tgt_2 = cm2;
  MOCK(circuitmux_compare_muxes, circuitmux_compare_muxes_mock);

  /* Equal-channel case */
  result = scheduler_compare_channels(&c1, &c1);
  tt_int_op(result, ==, 0);

  /* Distinct channels, distinct policies */
  result = scheduler_compare_channels(&c1, &c2);
  tt_int_op(result, ==, -1);
  result = scheduler_compare_channels(&c2, &c1);
  tt_int_op(result, ==, 1);

  /* Distinct channels, same policy */
  tor_free(mock_cgp_val_2);
  mock_cgp_val_2 = mock_cgp_val_1;
  result = scheduler_compare_channels(&c1, &c2);
  tt_int_op(result, ==, -1);
  result = scheduler_compare_channels(&c2, &c1);
  tt_int_op(result, ==, 1);

 done:

  UNMOCK(circuitmux_compare_muxes);
  mock_ccm_tgt_1 = NULL;
  mock_ccm_tgt_2 = NULL;

  UNMOCK(circuitmux_get_policy);
  mock_cgp_tgt_1 = NULL;
  mock_cgp_tgt_2 = NULL;

  tor_free(cm1);
  tor_free(cm2);

  if (mock_cgp_val_1 != mock_cgp_val_2)
    tor_free(mock_cgp_val_1);
  tor_free(mock_cgp_val_2);
  mock_cgp_val_1 = NULL;
  mock_cgp_val_2 = NULL;

  return;
}

static void
test_scheduler_initfree(void *arg)
{
  (void)arg;

  tt_ptr_op(channels_pending, ==, NULL);
  tt_ptr_op(run_sched_ev, ==, NULL);

  mock_event_init();
  MOCK(tor_libevent_get_base, tor_libevent_get_base_mock);

  scheduler_init();

  tt_assert(channels_pending != NULL);
  tt_assert(run_sched_ev != NULL);

  scheduler_free_all();

  UNMOCK(tor_libevent_get_base);
  mock_event_free_all();

  tt_ptr_op(channels_pending, ==, NULL);
  tt_ptr_op(run_sched_ev, ==, NULL);

 done:
  return;
}

static void
test_scheduler_loop(void *arg)
{
  channel_t *ch1 = NULL, *ch2 = NULL;

  (void)arg;

  /* Set up libevent and scheduler */

  mock_event_init();
  MOCK(tor_libevent_get_base, tor_libevent_get_base_mock);
  scheduler_init();
  /*
   * Install the compare channels mock so we can test
   * scheduler_touch_channel().
   */
  MOCK(scheduler_compare_channels, scheduler_compare_channels_mock);
  /*
   * Disable scheduler_run so we can just check the state transitions
   * without having to make everything it might call work too.
   */
  MOCK(scheduler_run, scheduler_run_noop_mock);

  tt_int_op(smartlist_len(channels_pending), ==, 0);

  /* Set up a fake channel */
  ch1 = new_fake_channel();
  tt_assert(ch1);

  /* Start it off in OPENING */
  ch1->state = CHANNEL_STATE_OPENING;
  /* We'll need a cmux */
  ch1->cmux = circuitmux_alloc();
  /* Try to register it */
  channel_register(ch1);
  tt_assert(ch1->registered);
  /* Finish opening it */
  channel_change_state(ch1, CHANNEL_STATE_OPEN);

  /* It should start off in SCHED_CHAN_IDLE */
  tt_int_op(ch1->scheduler_state, ==, SCHED_CHAN_IDLE);

  /* Now get another one */
  ch2 = new_fake_channel();
  tt_assert(ch2);
  ch2->state = CHANNEL_STATE_OPENING;
  ch2->cmux = circuitmux_alloc();
  channel_register(ch2);
  tt_assert(ch2->registered);
  /*
   * Don't open ch2; then channel_num_cells_writeable() will return
   * zero and we'll get coverage of that exception case in scheduler_run()
   */

  tt_int_op(ch1->state, ==, CHANNEL_STATE_OPEN);
  tt_int_op(ch2->state, ==, CHANNEL_STATE_OPENING);

  /* Send it to SCHED_CHAN_WAITING_TO_WRITE */
  scheduler_channel_has_waiting_cells(ch1);
  tt_int_op(ch1->scheduler_state, ==, SCHED_CHAN_WAITING_TO_WRITE);

  /* This should send it to SCHED_CHAN_PENDING */
  scheduler_channel_wants_writes(ch1);
  tt_int_op(ch1->scheduler_state, ==, SCHED_CHAN_PENDING);
  tt_int_op(smartlist_len(channels_pending), ==, 1);

  /* Now send ch2 to SCHED_CHAN_WAITING_FOR_CELLS */
  scheduler_channel_wants_writes(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_WAITING_FOR_CELLS);

  /* Drop ch2 back to idle */
  scheduler_channel_doesnt_want_writes(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_IDLE);

  /* ...and back to SCHED_CHAN_WAITING_FOR_CELLS */
  scheduler_channel_wants_writes(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_WAITING_FOR_CELLS);

  /* ...and this should kick ch2 into SCHED_CHAN_PENDING */
  scheduler_channel_has_waiting_cells(ch2);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_PENDING);
  tt_int_op(smartlist_len(channels_pending), ==, 2);

  /*
   * Now we've got two pending channels and need to fire off
   * scheduler_run(); first, unmock it.
   */

  UNMOCK(scheduler_run);

  scheduler_run();

  /* Now re-mock it */
  MOCK(scheduler_run, scheduler_run_noop_mock);

  /*
   * Assert that they're still in the states we left and aren't still
   * pending
   */
  tt_int_op(ch1->state, ==, CHANNEL_STATE_OPEN);
  tt_int_op(ch2->state, ==, CHANNEL_STATE_OPENING);
  tt_assert(ch1->scheduler_state != SCHED_CHAN_PENDING);
  tt_assert(ch2->scheduler_state != SCHED_CHAN_PENDING);
  tt_int_op(smartlist_len(channels_pending), ==, 0);

  /* Now, finish opening ch2, and get both back to pending */
  channel_change_state(ch2, CHANNEL_STATE_OPEN);
  scheduler_channel_wants_writes(ch1);
  scheduler_channel_wants_writes(ch2);
  scheduler_channel_has_waiting_cells(ch1);
  scheduler_channel_has_waiting_cells(ch2);
  tt_int_op(ch1->state, ==, CHANNEL_STATE_OPEN);
  tt_int_op(ch2->state, ==, CHANNEL_STATE_OPEN);
  tt_int_op(ch1->scheduler_state, ==, SCHED_CHAN_PENDING);
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_PENDING);
  tt_int_op(smartlist_len(channels_pending), ==, 2);

  /* Now, set up the channel_flush_some_cells() mock */
  MOCK(channel_flush_some_cells, channel_flush_some_cells_mock);
  /*
   * 16 cells on ch1 means it'll completely drain into the 32 cells
   * fakechan's num_cells_writeable() returns.
   */
  channel_flush_some_cells_mock_set(ch1, 16);
  /*
   * This one should get sent back to pending, since num_cells_writeable()
   * will still return non-zero.
   */
  channel_flush_some_cells_mock_set(ch2, 48);

  /*
   * And re-run the scheduler_run() loop with non-zero returns from
   * channel_flush_some_cells() this time.
   */
  UNMOCK(scheduler_run);

  scheduler_run();

  /* Now re-mock it */
  MOCK(scheduler_run, scheduler_run_noop_mock);

  /*
   * ch1 should have gone to SCHED_CHAN_WAITING_FOR_CELLS, with 16 flushed
   * and 32 writeable.
   */
  tt_int_op(ch1->scheduler_state, ==, SCHED_CHAN_WAITING_FOR_CELLS);
  /*
   * ...ch2 should also have gone to SCHED_CHAN_WAITING_FOR_CELLS, with
   * channel_more_to_flush() returning false and channel_num_cells_writeable()
   * > 0/
   */
  tt_int_op(ch2->scheduler_state, ==, SCHED_CHAN_WAITING_FOR_CELLS);

  /* Close */
  channel_mark_for_close(ch1);
  tt_int_op(ch1->state, ==, CHANNEL_STATE_CLOSING);
  channel_mark_for_close(ch2);
  tt_int_op(ch2->state, ==, CHANNEL_STATE_CLOSING);
  channel_closed(ch1);
  tt_int_op(ch1->state, ==, CHANNEL_STATE_CLOSED);
  ch1 = NULL;
  channel_closed(ch2);
  tt_int_op(ch2->state, ==, CHANNEL_STATE_CLOSED);
  ch2 = NULL;

  /* Shut things down */
  channel_flush_some_cells_mock_free_all();
  channel_free_all();
  scheduler_free_all();
  mock_event_free_all();

 done:
  tor_free(ch1);
  tor_free(ch2);

  UNMOCK(channel_flush_some_cells);
  UNMOCK(scheduler_compare_channels);
  UNMOCK(scheduler_run);
  UNMOCK(tor_libevent_get_base);
}

static void
test_scheduler_queue_heuristic(void *arg)
{
  time_t now = approx_time();
  uint64_t qh;

  (void)arg;

  queue_heuristic = 0;
  queue_heuristic_timestamp = 0;

  /* Not yet inited case */
  scheduler_update_queue_heuristic(now - 180);
  tt_u64_op(queue_heuristic, ==, 0);
  tt_int_op(queue_heuristic_timestamp, ==, now - 180);

  queue_heuristic = 1000000000L;
  queue_heuristic_timestamp = now - 120;

  scheduler_update_queue_heuristic(now - 119);
  tt_u64_op(queue_heuristic, ==, 500000000L);
  tt_int_op(queue_heuristic_timestamp, ==, now - 119);

  scheduler_update_queue_heuristic(now - 116);
  tt_u64_op(queue_heuristic, ==, 62500000L);
  tt_int_op(queue_heuristic_timestamp, ==, now - 116);

  qh = scheduler_get_queue_heuristic();
  tt_u64_op(qh, ==, 0);

 done:
  return;
}

struct testcase_t scheduler_tests[] = {
  { "channel_states", test_scheduler_channel_states, TT_FORK, NULL, NULL },
  { "compare_channels", test_scheduler_compare_channels,
    TT_FORK, NULL, NULL },
  { "initfree", test_scheduler_initfree, TT_FORK, NULL, NULL },
  { "loop", test_scheduler_loop, TT_FORK, NULL, NULL },
  { "queue_heuristic", test_scheduler_queue_heuristic,
    TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

