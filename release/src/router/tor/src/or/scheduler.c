/* * Copyright (c) 2013-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file scheduler.c
 * \brief Relay scheduling system
 **/

#include "or.h"

#define TOR_CHANNEL_INTERNAL_ /* For channel_flush_some_cells() */
#include "channel.h"

#include "compat_libevent.h"
#define SCHEDULER_PRIVATE_
#include "scheduler.h"

#ifdef HAVE_EVENT2_EVENT_H
#include <event2/event.h>
#else
#include <event.h>
#endif

/*
 * Scheduler high/low watermarks
 */

static uint32_t sched_q_low_water = 16384;
static uint32_t sched_q_high_water = 32768;

/*
 * Maximum cells to flush in a single call to channel_flush_some_cells();
 * setting this low means more calls, but too high and we could overshoot
 * sched_q_high_water.
 */

static uint32_t sched_max_flush_cells = 16;

/*
 * Write scheduling works by keeping track of which channels can
 * accept cells, and have cells to write.  From the scheduler's perspective,
 * a channel can be in four possible states:
 *
 * 1.) Not open for writes, no cells to send
 *     - Not much to do here, and the channel will have scheduler_state ==
 *       SCHED_CHAN_IDLE
 *     - Transitions from:
 *       - Open for writes/has cells by simultaneously draining all circuit
 *         queues and filling the output buffer.
 *     - Transitions to:
 *       - Not open for writes/has cells by arrival of cells on an attached
 *         circuit (this would be driven from append_cell_to_circuit_queue())
 *       - Open for writes/no cells by a channel type specific path;
 *         driven from connection_or_flushed_some() for channel_tls_t.
 *
 * 2.) Open for writes, no cells to send
 *     - Not much here either; this will be the state an idle but open channel
 *       can be expected to settle in.  It will have scheduler_state ==
 *       SCHED_CHAN_WAITING_FOR_CELLS
 *     - Transitions from:
 *       - Not open for writes/no cells by flushing some of the output
 *         buffer.
 *       - Open for writes/has cells by the scheduler moving cells from
 *         circuit queues to channel output queue, but not having enough
 *         to fill the output queue.
 *     - Transitions to:
 *       - Open for writes/has cells by arrival of new cells on an attached
 *         circuit, in append_cell_to_circuit_queue()
 *
 * 3.) Not open for writes, cells to send
 *     - This is the state of a busy circuit limited by output bandwidth;
 *       cells have piled up in the circuit queues waiting to be relayed.
 *       The channel will have scheduler_state == SCHED_CHAN_WAITING_TO_WRITE.
 *     - Transitions from:
 *       - Not open for writes/no cells by arrival of cells on an attached
 *         circuit
 *       - Open for writes/has cells by filling an output buffer without
 *         draining all cells from attached circuits
 *    - Transitions to:
 *       - Opens for writes/has cells by draining some of the output buffer
 *         via the connection_or_flushed_some() path (for channel_tls_t).
 *
 * 4.) Open for writes, cells to send
 *     - This connection is ready to relay some cells and waiting for
 *       the scheduler to choose it.  The channel will have scheduler_state ==
 *       SCHED_CHAN_PENDING.
 *     - Transitions from:
 *       - Not open for writes/has cells by the connection_or_flushed_some()
 *         path
 *       - Open for writes/no cells by the append_cell_to_circuit_queue()
 *         path
 *     - Transitions to:
 *       - Not open for writes/no cells by draining all circuit queues and
 *         simultaneously filling the output buffer.
 *       - Not open for writes/has cells by writing enough cells to fill the
 *         output buffer
 *       - Open for writes/no cells by draining all attached circuit queues
 *         without also filling the output buffer
 *
 * Other event-driven parts of the code move channels between these scheduling
 * states by calling scheduler functions; the scheduler only runs on open-for-
 * writes/has-cells channels and is the only path for those to transition to
 * other states.  The scheduler_run() function gives us the opportunity to do
 * scheduling work, and is called from other scheduler functions whenever a
 * state transition occurs, and periodically from the main event loop.
 */

/* Scheduler global data structures */

/*
 * We keep a list of channels that are pending - i.e, have cells to write
 * and can accept them to send.  The enum scheduler_state in channel_t
 * is reserved for our use.
 */

/* Pqueue of channels that can write and have cells (pending work) */
STATIC smartlist_t *channels_pending = NULL;

/*
 * This event runs the scheduler from its callback, and is manually
 * activated whenever a channel enters open for writes/cells to send.
 */

STATIC struct event *run_sched_ev = NULL;

/*
 * Queue heuristic; this is not the queue size, but an 'effective queuesize'
 * that ages out contributions from stalled channels.
 */

STATIC uint64_t queue_heuristic = 0;

/*
 * Timestamp for last queue heuristic update
 */

STATIC time_t queue_heuristic_timestamp = 0;

/* Scheduler static function declarations */

static void scheduler_evt_callback(evutil_socket_t fd,
                                   short events, void *arg);
static int scheduler_more_work(void);
static void scheduler_retrigger(void);
#if 0
static void scheduler_trigger(void);
#endif

/* Scheduler function implementations */

/** Free everything and shut down the scheduling system */

void
scheduler_free_all(void)
{
  log_debug(LD_SCHED, "Shutting down scheduler");

  if (run_sched_ev) {
    if (event_del(run_sched_ev) < 0) {
      log_warn(LD_BUG, "Problem deleting run_sched_ev");
    }
    tor_event_free(run_sched_ev);
    run_sched_ev = NULL;
  }

  if (channels_pending) {
    smartlist_free(channels_pending);
    channels_pending = NULL;
  }
}

/**
 * Comparison function to use when sorting pending channels
 */

MOCK_IMPL(STATIC int,
scheduler_compare_channels, (const void *c1_v, const void *c2_v))
{
  channel_t *c1 = NULL, *c2 = NULL;
  /* These are a workaround for -Wbad-function-cast throwing a fit */
  const circuitmux_policy_t *p1, *p2;
  uintptr_t p1_i, p2_i;

  tor_assert(c1_v);
  tor_assert(c2_v);

  c1 = (channel_t *)(c1_v);
  c2 = (channel_t *)(c2_v);

  tor_assert(c1);
  tor_assert(c2);

  if (c1 != c2) {
    if (circuitmux_get_policy(c1->cmux) ==
        circuitmux_get_policy(c2->cmux)) {
      /* Same cmux policy, so use the mux comparison */
      return circuitmux_compare_muxes(c1->cmux, c2->cmux);
    } else {
      /*
       * Different policies; not important to get this edge case perfect
       * because the current code never actually gives different channels
       * different cmux policies anyway.  Just use this arbitrary but
       * definite choice.
       */
      p1 = circuitmux_get_policy(c1->cmux);
      p2 = circuitmux_get_policy(c2->cmux);
      p1_i = (uintptr_t)p1;
      p2_i = (uintptr_t)p2;

      return (p1_i < p2_i) ? -1 : 1;
    }
  } else {
    /* c1 == c2, so always equal */
    return 0;
  }
}

/*
 * Scheduler event callback; this should get triggered once per event loop
 * if any scheduling work was created during the event loop.
 */

static void
scheduler_evt_callback(evutil_socket_t fd, short events, void *arg)
{
  (void)fd;
  (void)events;
  (void)arg;
  log_debug(LD_SCHED, "Scheduler event callback called");

  tor_assert(run_sched_ev);

  /* Run the scheduler */
  scheduler_run();

  /* Do we have more work to do? */
  if (scheduler_more_work()) scheduler_retrigger();
}

/** Mark a channel as no longer ready to accept writes */

MOCK_IMPL(void,
scheduler_channel_doesnt_want_writes,(channel_t *chan))
{
  tor_assert(chan);

  tor_assert(channels_pending);

  /* If it's already in pending, we can put it in waiting_to_write */
  if (chan->scheduler_state == SCHED_CHAN_PENDING) {
    /*
     * It's in channels_pending, so it shouldn't be in any of
     * the other lists.  It can't write any more, so it goes to
     * channels_waiting_to_write.
     */
    smartlist_pqueue_remove(channels_pending,
                            scheduler_compare_channels,
                            STRUCT_OFFSET(channel_t, sched_heap_idx),
                            chan);
    chan->scheduler_state = SCHED_CHAN_WAITING_TO_WRITE;
    log_debug(LD_SCHED,
              "Channel " U64_FORMAT " at %p went from pending "
              "to waiting_to_write",
              U64_PRINTF_ARG(chan->global_identifier), chan);
  } else {
    /*
     * It's not in pending, so it can't become waiting_to_write; it's
     * either not in any of the lists (nothing to do) or it's already in
     * waiting_for_cells (remove it, can't write any more).
     */
    if (chan->scheduler_state == SCHED_CHAN_WAITING_FOR_CELLS) {
      chan->scheduler_state = SCHED_CHAN_IDLE;
      log_debug(LD_SCHED,
                "Channel " U64_FORMAT " at %p left waiting_for_cells",
                U64_PRINTF_ARG(chan->global_identifier), chan);
    }
  }
}

/** Mark a channel as having waiting cells */

MOCK_IMPL(void,
scheduler_channel_has_waiting_cells,(channel_t *chan))
{
  int became_pending = 0;

  tor_assert(chan);
  tor_assert(channels_pending);

  /* First, check if this one also writeable */
  if (chan->scheduler_state == SCHED_CHAN_WAITING_FOR_CELLS) {
    /*
     * It's in channels_waiting_for_cells, so it shouldn't be in any of
     * the other lists.  It has waiting cells now, so it goes to
     * channels_pending.
     */
    chan->scheduler_state = SCHED_CHAN_PENDING;
    smartlist_pqueue_add(channels_pending,
                         scheduler_compare_channels,
                         STRUCT_OFFSET(channel_t, sched_heap_idx),
                         chan);
    log_debug(LD_SCHED,
              "Channel " U64_FORMAT " at %p went from waiting_for_cells "
              "to pending",
              U64_PRINTF_ARG(chan->global_identifier), chan);
    became_pending = 1;
  } else {
    /*
     * It's not in waiting_for_cells, so it can't become pending; it's
     * either not in any of the lists (we add it to waiting_to_write)
     * or it's already in waiting_to_write or pending (we do nothing)
     */
    if (!(chan->scheduler_state == SCHED_CHAN_WAITING_TO_WRITE ||
          chan->scheduler_state == SCHED_CHAN_PENDING)) {
      chan->scheduler_state = SCHED_CHAN_WAITING_TO_WRITE;
      log_debug(LD_SCHED,
                "Channel " U64_FORMAT " at %p entered waiting_to_write",
                U64_PRINTF_ARG(chan->global_identifier), chan);
    }
  }

  /*
   * If we made a channel pending, we potentially have scheduling work
   * to do.
   */
  if (became_pending) scheduler_retrigger();
}

/** Set up the scheduling system */

void
scheduler_init(void)
{
  log_debug(LD_SCHED, "Initting scheduler");

  tor_assert(!run_sched_ev);
  run_sched_ev = tor_event_new(tor_libevent_get_base(), -1,
                               0, scheduler_evt_callback, NULL);

  channels_pending = smartlist_new();
  queue_heuristic = 0;
  queue_heuristic_timestamp = approx_time();
}

/** Check if there's more scheduling work */

static int
scheduler_more_work(void)
{
  tor_assert(channels_pending);

  return ((scheduler_get_queue_heuristic() < sched_q_low_water) &&
          ((smartlist_len(channels_pending) > 0))) ? 1 : 0;
}

/** Retrigger the scheduler in a way safe to use from the callback */

static void
scheduler_retrigger(void)
{
  tor_assert(run_sched_ev);
  event_active(run_sched_ev, EV_TIMEOUT, 1);
}

/** Notify the scheduler of a channel being closed */

MOCK_IMPL(void,
scheduler_release_channel,(channel_t *chan))
{
  tor_assert(chan);
  tor_assert(channels_pending);

  if (chan->scheduler_state == SCHED_CHAN_PENDING) {
    smartlist_pqueue_remove(channels_pending,
                            scheduler_compare_channels,
                            STRUCT_OFFSET(channel_t, sched_heap_idx),
                            chan);
  }

  chan->scheduler_state = SCHED_CHAN_IDLE;
}

/** Run the scheduling algorithm if necessary */

MOCK_IMPL(void,
scheduler_run, (void))
{
  int n_cells, n_chans_before, n_chans_after;
  uint64_t q_len_before, q_heur_before, q_len_after, q_heur_after;
  ssize_t flushed, flushed_this_time;
  smartlist_t *to_readd = NULL;
  channel_t *chan = NULL;

  log_debug(LD_SCHED, "We have a chance to run the scheduler");

  if (scheduler_get_queue_heuristic() < sched_q_low_water) {
    n_chans_before = smartlist_len(channels_pending);
    q_len_before = channel_get_global_queue_estimate();
    q_heur_before = scheduler_get_queue_heuristic();

    while (scheduler_get_queue_heuristic() <= sched_q_high_water &&
           smartlist_len(channels_pending) > 0) {
      /* Pop off a channel */
      chan = smartlist_pqueue_pop(channels_pending,
                                  scheduler_compare_channels,
                                  STRUCT_OFFSET(channel_t, sched_heap_idx));
      tor_assert(chan);

      /* Figure out how many cells we can write */
      n_cells = channel_num_cells_writeable(chan);
      if (n_cells > 0) {
        log_debug(LD_SCHED,
                  "Scheduler saw pending channel " U64_FORMAT " at %p with "
                  "%d cells writeable",
                  U64_PRINTF_ARG(chan->global_identifier), chan, n_cells);

        flushed = 0;
        while (flushed < n_cells &&
               scheduler_get_queue_heuristic() <= sched_q_high_water) {
          flushed_this_time =
            channel_flush_some_cells(chan,
                                     MIN(sched_max_flush_cells,
                                         (size_t) n_cells - flushed));
          if (flushed_this_time <= 0) break;
          flushed += flushed_this_time;
        }

        if (flushed < n_cells) {
          /* We ran out of cells to flush */
          chan->scheduler_state = SCHED_CHAN_WAITING_FOR_CELLS;
          log_debug(LD_SCHED,
                    "Channel " U64_FORMAT " at %p "
                    "entered waiting_for_cells from pending",
                    U64_PRINTF_ARG(chan->global_identifier),
                    chan);
        } else {
          /* The channel may still have some cells */
          if (channel_more_to_flush(chan)) {
          /* The channel goes to either pending or waiting_to_write */
            if (channel_num_cells_writeable(chan) > 0) {
              /* Add it back to pending later */
              if (!to_readd) to_readd = smartlist_new();
              smartlist_add(to_readd, chan);
              log_debug(LD_SCHED,
                        "Channel " U64_FORMAT " at %p "
                        "is still pending",
                        U64_PRINTF_ARG(chan->global_identifier),
                        chan);
            } else {
              /* It's waiting to be able to write more */
              chan->scheduler_state = SCHED_CHAN_WAITING_TO_WRITE;
              log_debug(LD_SCHED,
                        "Channel " U64_FORMAT " at %p "
                        "entered waiting_to_write from pending",
                        U64_PRINTF_ARG(chan->global_identifier),
                        chan);
            }
          } else {
            /* No cells left; it can go to idle or waiting_for_cells */
            if (channel_num_cells_writeable(chan) > 0) {
              /*
               * It can still accept writes, so it goes to
               * waiting_for_cells
               */
              chan->scheduler_state = SCHED_CHAN_WAITING_FOR_CELLS;
              log_debug(LD_SCHED,
                        "Channel " U64_FORMAT " at %p "
                        "entered waiting_for_cells from pending",
                        U64_PRINTF_ARG(chan->global_identifier),
                        chan);
            } else {
              /*
               * We exactly filled up the output queue with all available
               * cells; go to idle.
               */
              chan->scheduler_state = SCHED_CHAN_IDLE;
              log_debug(LD_SCHED,
                        "Channel " U64_FORMAT " at %p "
                        "become idle from pending",
                        U64_PRINTF_ARG(chan->global_identifier),
                        chan);
            }
          }
        }

        log_debug(LD_SCHED,
                  "Scheduler flushed %d cells onto pending channel "
                  U64_FORMAT " at %p",
                  (int)flushed, U64_PRINTF_ARG(chan->global_identifier),
                  chan);
      } else {
        log_info(LD_SCHED,
                 "Scheduler saw pending channel " U64_FORMAT " at %p with "
                 "no cells writeable",
                 U64_PRINTF_ARG(chan->global_identifier), chan);
        /* Put it back to WAITING_TO_WRITE */
        chan->scheduler_state = SCHED_CHAN_WAITING_TO_WRITE;
      }
    }

    /* Readd any channels we need to */
    if (to_readd) {
      SMARTLIST_FOREACH_BEGIN(to_readd, channel_t *, chan) {
        chan->scheduler_state = SCHED_CHAN_PENDING;
        smartlist_pqueue_add(channels_pending,
                             scheduler_compare_channels,
                             STRUCT_OFFSET(channel_t, sched_heap_idx),
                             chan);
      } SMARTLIST_FOREACH_END(chan);
      smartlist_free(to_readd);
    }

    n_chans_after = smartlist_len(channels_pending);
    q_len_after = channel_get_global_queue_estimate();
    q_heur_after = scheduler_get_queue_heuristic();
    log_debug(LD_SCHED,
              "Scheduler handled %d of %d pending channels, queue size from "
              U64_FORMAT " to " U64_FORMAT ", queue heuristic from "
              U64_FORMAT " to " U64_FORMAT,
              n_chans_before - n_chans_after, n_chans_before,
              U64_PRINTF_ARG(q_len_before), U64_PRINTF_ARG(q_len_after),
              U64_PRINTF_ARG(q_heur_before), U64_PRINTF_ARG(q_heur_after));
  }
}

/** Trigger the scheduling event so we run the scheduler later */

#if 0
static void
scheduler_trigger(void)
{
  log_debug(LD_SCHED, "Triggering scheduler event");

  tor_assert(run_sched_ev);

  event_add(run_sched_ev, EV_TIMEOUT, 1);
}
#endif

/** Mark a channel as ready to accept writes */

void
scheduler_channel_wants_writes(channel_t *chan)
{
  int became_pending = 0;

  tor_assert(chan);
  tor_assert(channels_pending);

  /* If it's already in waiting_to_write, we can put it in pending */
  if (chan->scheduler_state == SCHED_CHAN_WAITING_TO_WRITE) {
    /*
     * It can write now, so it goes to channels_pending.
     */
    smartlist_pqueue_add(channels_pending,
                         scheduler_compare_channels,
                         STRUCT_OFFSET(channel_t, sched_heap_idx),
                         chan);
    chan->scheduler_state = SCHED_CHAN_PENDING;
    log_debug(LD_SCHED,
              "Channel " U64_FORMAT " at %p went from waiting_to_write "
              "to pending",
              U64_PRINTF_ARG(chan->global_identifier), chan);
    became_pending = 1;
  } else {
    /*
     * It's not in SCHED_CHAN_WAITING_TO_WRITE, so it can't become pending;
     * it's either idle and goes to WAITING_FOR_CELLS, or it's a no-op.
     */
    if (!(chan->scheduler_state == SCHED_CHAN_WAITING_FOR_CELLS ||
          chan->scheduler_state == SCHED_CHAN_PENDING)) {
      chan->scheduler_state = SCHED_CHAN_WAITING_FOR_CELLS;
      log_debug(LD_SCHED,
                "Channel " U64_FORMAT " at %p entered waiting_for_cells",
                U64_PRINTF_ARG(chan->global_identifier), chan);
    }
  }

  /*
   * If we made a channel pending, we potentially have scheduling work
   * to do.
   */
  if (became_pending) scheduler_retrigger();
}

/**
 * Notify the scheduler that a channel's position in the pqueue may have
 * changed
 */

void
scheduler_touch_channel(channel_t *chan)
{
  tor_assert(chan);

  if (chan->scheduler_state == SCHED_CHAN_PENDING) {
    /* Remove and re-add it */
    smartlist_pqueue_remove(channels_pending,
                            scheduler_compare_channels,
                            STRUCT_OFFSET(channel_t, sched_heap_idx),
                            chan);
    smartlist_pqueue_add(channels_pending,
                         scheduler_compare_channels,
                         STRUCT_OFFSET(channel_t, sched_heap_idx),
                         chan);
  }
  /* else no-op, since it isn't in the queue */
}

/**
 * Notify the scheduler of a queue size adjustment, to recalculate the
 * queue heuristic.
 */

void
scheduler_adjust_queue_size(channel_t *chan, int dir, uint64_t adj)
{
  time_t now = approx_time();

  log_debug(LD_SCHED,
            "Queue size adjustment by %s" U64_FORMAT " for channel "
            U64_FORMAT,
            (dir >= 0) ? "+" : "-",
            U64_PRINTF_ARG(adj),
            U64_PRINTF_ARG(chan->global_identifier));

  /* Get the queue heuristic up to date */
  scheduler_update_queue_heuristic(now);

  /* Adjust as appropriate */
  if (dir >= 0) {
    /* Increasing it */
    queue_heuristic += adj;
  } else {
    /* Decreasing it */
    if (queue_heuristic > adj) queue_heuristic -= adj;
    else queue_heuristic = 0;
  }

  log_debug(LD_SCHED,
            "Queue heuristic is now " U64_FORMAT,
            U64_PRINTF_ARG(queue_heuristic));
}

/**
 * Query the current value of the queue heuristic
 */

STATIC uint64_t
scheduler_get_queue_heuristic(void)
{
  time_t now = approx_time();

  scheduler_update_queue_heuristic(now);

  return queue_heuristic;
}

/**
 * Adjust the queue heuristic value to the present time
 */

STATIC void
scheduler_update_queue_heuristic(time_t now)
{
  time_t diff;

  if (queue_heuristic_timestamp == 0) {
    /*
     * Nothing we can sensibly do; must not have been initted properly.
     * Oh well.
     */
    queue_heuristic_timestamp = now;
  } else if (queue_heuristic_timestamp < now) {
    diff = now - queue_heuristic_timestamp;
    /*
     * This is a simple exponential age-out; the other proposed alternative
     * was a linear age-out using the bandwidth history in rephist.c; I'm
     * going with this out of concern that if an adversary can jam the
     * scheduler long enough, it would cause the bandwidth to drop to
     * zero and render the aging mechanism ineffective thereafter.
     */
    if (0 <= diff && diff < 64) queue_heuristic >>= diff;
    else queue_heuristic = 0;

    queue_heuristic_timestamp = now;

    log_debug(LD_SCHED,
              "Queue heuristic is now " U64_FORMAT,
              U64_PRINTF_ARG(queue_heuristic));
  }
  /* else no update needed, or time went backward */
}

/**
 * Set scheduler watermarks and flush size
 */

void
scheduler_set_watermarks(uint32_t lo, uint32_t hi, uint32_t max_flush)
{
  /* Sanity assertions - caller should ensure these are true */
  tor_assert(lo > 0);
  tor_assert(hi > lo);
  tor_assert(max_flush > 0);

  sched_q_low_water = lo;
  sched_q_high_water = hi;
  sched_max_flush_cells = max_flush;
}

