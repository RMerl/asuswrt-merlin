/* * Copyright (c) 2012-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file circuitmux_ewma.c
 * \brief EWMA circuit selection as a circuitmux_t policy
 **/

#define TOR_CIRCUITMUX_EWMA_C_

#include <math.h>

#include "or.h"
#include "circuitmux.h"
#include "circuitmux_ewma.h"
#include "networkstatus.h"

/*** EWMA parameter #defines ***/

/** How long does a tick last (seconds)? */
#define EWMA_TICK_LEN 10

/** The default per-tick scale factor, if it hasn't been overridden by a
 * consensus or a configuration setting.  zero means "disabled". */
#define EWMA_DEFAULT_HALFLIFE 0.0

/*** Some useful constant #defines ***/

/*DOCDOC*/
#define EPSILON 0.00001
/*DOCDOC*/
#define LOG_ONEHALF -0.69314718055994529

/*** EWMA structures ***/

typedef struct cell_ewma_s cell_ewma_t;
typedef struct ewma_policy_data_s ewma_policy_data_t;
typedef struct ewma_policy_circ_data_s ewma_policy_circ_data_t;

/**
 * The cell_ewma_t structure keeps track of how many cells a circuit has
 * transferred recently.  It keeps an EWMA (exponentially weighted moving
 * average) of the number of cells flushed from the circuit queue onto a
 * connection in channel_flush_from_first_active_circuit().
 */

struct cell_ewma_s {
  /** The last 'tick' at which we recalibrated cell_count.
   *
   * A cell sent at exactly the start of this tick has weight 1.0. Cells sent
   * since the start of this tick have weight greater than 1.0; ones sent
   * earlier have less weight. */
  unsigned int last_adjusted_tick;
  /** The EWMA of the cell count. */
  double cell_count;
  /** True iff this is the cell count for a circuit's previous
   * channel. */
  unsigned int is_for_p_chan : 1;
  /** The position of the circuit within the OR connection's priority
   * queue. */
  int heap_index;
};

struct ewma_policy_data_s {
  circuitmux_policy_data_t base_;

  /**
   * Priority queue of cell_ewma_t for circuits with queued cells waiting
   * for room to free up on the channel that owns this circuitmux.  Kept
   * in heap order according to EWMA.  This was formerly in channel_t, and
   * in or_connection_t before that.
   */
  smartlist_t *active_circuit_pqueue;

  /**
   * The tick on which the cell_ewma_ts in active_circuit_pqueue last had
   * their ewma values rescaled.  This was formerly in channel_t, and in
   * or_connection_t before that.
   */
  unsigned int active_circuit_pqueue_last_recalibrated;
};

struct ewma_policy_circ_data_s {
  circuitmux_policy_circ_data_t base_;

  /**
   * The EWMA count for the number of cells flushed from this circuit
   * onto this circuitmux.  Used to determine which circuit to flush
   * from next.  This was formerly in circuit_t and or_circuit_t.
   */
  cell_ewma_t cell_ewma;

  /**
   * Pointer back to the circuit_t this is for; since we're separating
   * out circuit selection policy like this, we can't attach cell_ewma_t
   * to the circuit_t any more, so we can't use SUBTYPE_P directly to a
   * circuit_t like before; instead get it here.
   */
  circuit_t *circ;
};

#define EWMA_POL_DATA_MAGIC 0x2fd8b16aU
#define EWMA_POL_CIRC_DATA_MAGIC 0x761e7747U

/*** Downcasts for the above types ***/

static ewma_policy_data_t *
TO_EWMA_POL_DATA(circuitmux_policy_data_t *);

static ewma_policy_circ_data_t *
TO_EWMA_POL_CIRC_DATA(circuitmux_policy_circ_data_t *);

/**
 * Downcast a circuitmux_policy_data_t to an ewma_policy_data_t and assert
 * if the cast is impossible.
 */

static INLINE ewma_policy_data_t *
TO_EWMA_POL_DATA(circuitmux_policy_data_t *pol)
{
  if (!pol) return NULL;
  else {
    tor_assert(pol->magic == EWMA_POL_DATA_MAGIC);
    return DOWNCAST(ewma_policy_data_t, pol);
  }
}

/**
 * Downcast a circuitmux_policy_circ_data_t to an ewma_policy_circ_data_t
 * and assert if the cast is impossible.
 */

static INLINE ewma_policy_circ_data_t *
TO_EWMA_POL_CIRC_DATA(circuitmux_policy_circ_data_t *pol)
{
  if (!pol) return NULL;
  else {
    tor_assert(pol->magic == EWMA_POL_CIRC_DATA_MAGIC);
    return DOWNCAST(ewma_policy_circ_data_t, pol);
  }
}

/*** Static declarations for circuitmux_ewma.c ***/

static void add_cell_ewma(ewma_policy_data_t *pol, cell_ewma_t *ewma);
static int compare_cell_ewma_counts(const void *p1, const void *p2);
static unsigned cell_ewma_tick_from_timeval(const struct timeval *now,
                                            double *remainder_out);
static circuit_t * cell_ewma_to_circuit(cell_ewma_t *ewma);
static INLINE double get_scale_factor(unsigned from_tick, unsigned to_tick);
static cell_ewma_t * pop_first_cell_ewma(ewma_policy_data_t *pol);
static void remove_cell_ewma(ewma_policy_data_t *pol, cell_ewma_t *ewma);
static void scale_single_cell_ewma(cell_ewma_t *ewma, unsigned cur_tick);
static void scale_active_circuits(ewma_policy_data_t *pol,
                                  unsigned cur_tick);

/*** Circuitmux policy methods ***/

static circuitmux_policy_data_t * ewma_alloc_cmux_data(circuitmux_t *cmux);
static void ewma_free_cmux_data(circuitmux_t *cmux,
                                circuitmux_policy_data_t *pol_data);
static circuitmux_policy_circ_data_t *
ewma_alloc_circ_data(circuitmux_t *cmux, circuitmux_policy_data_t *pol_data,
                     circuit_t *circ, cell_direction_t direction,
                     unsigned int cell_count);
static void
ewma_free_circ_data(circuitmux_t *cmux,
                    circuitmux_policy_data_t *pol_data,
                    circuit_t *circ,
                    circuitmux_policy_circ_data_t *pol_circ_data);
static void
ewma_notify_circ_active(circuitmux_t *cmux,
                        circuitmux_policy_data_t *pol_data,
                        circuit_t *circ,
                        circuitmux_policy_circ_data_t *pol_circ_data);
static void
ewma_notify_circ_inactive(circuitmux_t *cmux,
                          circuitmux_policy_data_t *pol_data,
                          circuit_t *circ,
                          circuitmux_policy_circ_data_t *pol_circ_data);
static void
ewma_notify_xmit_cells(circuitmux_t *cmux,
                       circuitmux_policy_data_t *pol_data,
                       circuit_t *circ,
                       circuitmux_policy_circ_data_t *pol_circ_data,
                       unsigned int n_cells);
static circuit_t *
ewma_pick_active_circuit(circuitmux_t *cmux,
                         circuitmux_policy_data_t *pol_data);
static int
ewma_cmp_cmux(circuitmux_t *cmux_1, circuitmux_policy_data_t *pol_data_1,
              circuitmux_t *cmux_2, circuitmux_policy_data_t *pol_data_2);

/*** EWMA global variables ***/

/** The per-tick scale factor to be used when computing cell-count EWMA
 * values.  (A cell sent N ticks before the start of the current tick
 * has value ewma_scale_factor ** N.)
 */
static double ewma_scale_factor = 0.1;
/* DOCDOC ewma_enabled */
static int ewma_enabled = 0;

/*** EWMA circuitmux_policy_t method table ***/

circuitmux_policy_t ewma_policy = {
  /*.alloc_cmux_data =*/ ewma_alloc_cmux_data,
  /*.free_cmux_data =*/ ewma_free_cmux_data,
  /*.alloc_circ_data =*/ ewma_alloc_circ_data,
  /*.free_circ_data =*/ ewma_free_circ_data,
  /*.notify_circ_active =*/ ewma_notify_circ_active,
  /*.notify_circ_inactive =*/ ewma_notify_circ_inactive,
  /*.notify_set_n_cells =*/ NULL, /* EWMA doesn't need this */
  /*.notify_xmit_cells =*/ ewma_notify_xmit_cells,
  /*.pick_active_circuit =*/ ewma_pick_active_circuit,
  /*.cmp_cmux =*/ ewma_cmp_cmux
};

/*** EWMA method implementations using the below EWMA helper functions ***/

/**
 * Allocate an ewma_policy_data_t and upcast it to a circuitmux_policy_data_t;
 * this is called when setting the policy on a circuitmux_t to ewma_policy.
 */

static circuitmux_policy_data_t *
ewma_alloc_cmux_data(circuitmux_t *cmux)
{
  ewma_policy_data_t *pol = NULL;

  tor_assert(cmux);

  pol = tor_malloc_zero(sizeof(*pol));
  pol->base_.magic = EWMA_POL_DATA_MAGIC;
  pol->active_circuit_pqueue = smartlist_new();
  pol->active_circuit_pqueue_last_recalibrated = cell_ewma_get_tick();

  return TO_CMUX_POL_DATA(pol);
}

/**
 * Free an ewma_policy_data_t allocated with ewma_alloc_cmux_data()
 */

static void
ewma_free_cmux_data(circuitmux_t *cmux,
                    circuitmux_policy_data_t *pol_data)
{
  ewma_policy_data_t *pol = NULL;

  tor_assert(cmux);
  if (!pol_data) return;

  pol = TO_EWMA_POL_DATA(pol_data);

  smartlist_free(pol->active_circuit_pqueue);
  tor_free(pol);
}

/**
 * Allocate an ewma_policy_circ_data_t and upcast it to a
 * circuitmux_policy_data_t; this is called when attaching a circuit to a
 * circuitmux_t with ewma_policy.
 */

static circuitmux_policy_circ_data_t *
ewma_alloc_circ_data(circuitmux_t *cmux,
                     circuitmux_policy_data_t *pol_data,
                     circuit_t *circ,
                     cell_direction_t direction,
                     unsigned int cell_count)
{
  ewma_policy_circ_data_t *cdata = NULL;

  tor_assert(cmux);
  tor_assert(pol_data);
  tor_assert(circ);
  tor_assert(direction == CELL_DIRECTION_OUT ||
             direction == CELL_DIRECTION_IN);
  /* Shut the compiler up without triggering -Wtautological-compare */
  (void)cell_count;

  cdata = tor_malloc_zero(sizeof(*cdata));
  cdata->base_.magic = EWMA_POL_CIRC_DATA_MAGIC;
  cdata->circ = circ;

  /*
   * Initialize the cell_ewma_t structure (formerly in
   * init_circuit_base())
   */
  cdata->cell_ewma.last_adjusted_tick = cell_ewma_get_tick();
  cdata->cell_ewma.cell_count = 0.0;
  cdata->cell_ewma.heap_index = -1;
  if (direction == CELL_DIRECTION_IN) {
    cdata->cell_ewma.is_for_p_chan = 1;
  } else {
    cdata->cell_ewma.is_for_p_chan = 0;
  }

  return TO_CMUX_POL_CIRC_DATA(cdata);
}

/**
 * Free an ewma_policy_circ_data_t allocated with ewma_alloc_circ_data()
 */

static void
ewma_free_circ_data(circuitmux_t *cmux,
                    circuitmux_policy_data_t *pol_data,
                    circuit_t *circ,
                    circuitmux_policy_circ_data_t *pol_circ_data)

{
  ewma_policy_circ_data_t *cdata = NULL;

  tor_assert(cmux);
  tor_assert(circ);
  tor_assert(pol_data);

  if (!pol_circ_data) return;

  cdata = TO_EWMA_POL_CIRC_DATA(pol_circ_data);

  tor_free(cdata);
}

/**
 * Handle circuit activation; this inserts the circuit's cell_ewma into
 * the active_circuits_pqueue.
 */

static void
ewma_notify_circ_active(circuitmux_t *cmux,
                        circuitmux_policy_data_t *pol_data,
                        circuit_t *circ,
                        circuitmux_policy_circ_data_t *pol_circ_data)
{
  ewma_policy_data_t *pol = NULL;
  ewma_policy_circ_data_t *cdata = NULL;

  tor_assert(cmux);
  tor_assert(pol_data);
  tor_assert(circ);
  tor_assert(pol_circ_data);

  pol = TO_EWMA_POL_DATA(pol_data);
  cdata = TO_EWMA_POL_CIRC_DATA(pol_circ_data);

  add_cell_ewma(pol, &(cdata->cell_ewma));
}

/**
 * Handle circuit deactivation; this removes the circuit's cell_ewma from
 * the active_circuits_pqueue.
 */

static void
ewma_notify_circ_inactive(circuitmux_t *cmux,
                          circuitmux_policy_data_t *pol_data,
                          circuit_t *circ,
                          circuitmux_policy_circ_data_t *pol_circ_data)
{
  ewma_policy_data_t *pol = NULL;
  ewma_policy_circ_data_t *cdata = NULL;

  tor_assert(cmux);
  tor_assert(pol_data);
  tor_assert(circ);
  tor_assert(pol_circ_data);

  pol = TO_EWMA_POL_DATA(pol_data);
  cdata = TO_EWMA_POL_CIRC_DATA(pol_circ_data);

  remove_cell_ewma(pol, &(cdata->cell_ewma));
}

/**
 * Update cell_ewma for this circuit after we've sent some cells, and
 * remove/reinsert it in the queue.  This used to be done (brokenly,
 * see bug 6816) in channel_flush_from_first_active_circuit().
 */

static void
ewma_notify_xmit_cells(circuitmux_t *cmux,
                       circuitmux_policy_data_t *pol_data,
                       circuit_t *circ,
                       circuitmux_policy_circ_data_t *pol_circ_data,
                       unsigned int n_cells)
{
  ewma_policy_data_t *pol = NULL;
  ewma_policy_circ_data_t *cdata = NULL;
  unsigned int tick;
  double fractional_tick, ewma_increment;
  /* The current (hi-res) time */
  struct timeval now_hires;
  cell_ewma_t *cell_ewma, *tmp;

  tor_assert(cmux);
  tor_assert(pol_data);
  tor_assert(circ);
  tor_assert(pol_circ_data);
  tor_assert(n_cells > 0);

  pol = TO_EWMA_POL_DATA(pol_data);
  cdata = TO_EWMA_POL_CIRC_DATA(pol_circ_data);

  /* Rescale the EWMAs if needed */
  tor_gettimeofday_cached(&now_hires);
  tick = cell_ewma_tick_from_timeval(&now_hires, &fractional_tick);

  if (tick != pol->active_circuit_pqueue_last_recalibrated) {
    scale_active_circuits(pol, tick);
  }

  /* How much do we adjust the cell count in cell_ewma by? */
  ewma_increment =
    ((double)(n_cells)) * pow(ewma_scale_factor, -fractional_tick);

  /* Do the adjustment */
  cell_ewma = &(cdata->cell_ewma);
  cell_ewma->cell_count += ewma_increment;

  /*
   * Since we just sent on this circuit, it should be at the head of
   * the queue.  Pop the head, assert that it matches, then re-add.
   */
  tmp = pop_first_cell_ewma(pol);
  tor_assert(tmp == cell_ewma);
  add_cell_ewma(pol, cell_ewma);
}

/**
 * Pick the preferred circuit to send from; this will be the one with
 * the lowest EWMA value in the priority queue.  This used to be done
 * in channel_flush_from_first_active_circuit().
 */

static circuit_t *
ewma_pick_active_circuit(circuitmux_t *cmux,
                         circuitmux_policy_data_t *pol_data)
{
  ewma_policy_data_t *pol = NULL;
  circuit_t *circ = NULL;
  cell_ewma_t *cell_ewma = NULL;

  tor_assert(cmux);
  tor_assert(pol_data);

  pol = TO_EWMA_POL_DATA(pol_data);

  if (smartlist_len(pol->active_circuit_pqueue) > 0) {
    /* Get the head of the queue */
    cell_ewma = smartlist_get(pol->active_circuit_pqueue, 0);
    circ = cell_ewma_to_circuit(cell_ewma);
  }

  return circ;
}

/**
 * Compare two EWMA cmuxes, and return -1, 0 or 1 to indicate which should
 * be more preferred - see circuitmux_compare_muxes() of circuitmux.c.
 */

static int
ewma_cmp_cmux(circuitmux_t *cmux_1, circuitmux_policy_data_t *pol_data_1,
              circuitmux_t *cmux_2, circuitmux_policy_data_t *pol_data_2)
{
  ewma_policy_data_t *p1 = NULL, *p2 = NULL;
  cell_ewma_t *ce1 = NULL, *ce2 = NULL;

  tor_assert(cmux_1);
  tor_assert(pol_data_1);
  tor_assert(cmux_2);
  tor_assert(pol_data_2);

  p1 = TO_EWMA_POL_DATA(pol_data_1);
  p2 = TO_EWMA_POL_DATA(pol_data_1);

  if (p1 != p2) {
    /* Get the head cell_ewma_t from each queue */
    if (smartlist_len(p1->active_circuit_pqueue) > 0) {
      ce1 = smartlist_get(p1->active_circuit_pqueue, 0);
    }

    if (smartlist_len(p2->active_circuit_pqueue) > 0) {
      ce2 = smartlist_get(p2->active_circuit_pqueue, 0);
    }

    /* Got both of them? */
    if (ce1 != NULL && ce2 != NULL) {
      /* Pick whichever one has the better best circuit */
      return compare_cell_ewma_counts(ce1, ce2);
    } else {
      if (ce1 != NULL ) {
        /* We only have a circuit on cmux_1, so prefer it */
        return -1;
      } else if (ce2 != NULL) {
        /* We only have a circuit on cmux_2, so prefer it */
        return 1;
      } else {
        /* No circuits at all; no preference */
        return 0;
      }
    }
  } else {
    /* We got identical params */
    return 0;
  }
}

/** Helper for sorting cell_ewma_t values in their priority queue. */
static int
compare_cell_ewma_counts(const void *p1, const void *p2)
{
  const cell_ewma_t *e1 = p1, *e2 = p2;

  if (e1->cell_count < e2->cell_count)
    return -1;
  else if (e1->cell_count > e2->cell_count)
    return 1;
  else
    return 0;
}

/** Given a cell_ewma_t, return a pointer to the circuit containing it. */
static circuit_t *
cell_ewma_to_circuit(cell_ewma_t *ewma)
{
  ewma_policy_circ_data_t *cdata = NULL;

  tor_assert(ewma);
  cdata = SUBTYPE_P(ewma, ewma_policy_circ_data_t, cell_ewma);
  tor_assert(cdata);

  return cdata->circ;
}

/* ==== Functions for scaling cell_ewma_t ====

   When choosing which cells to relay first, we favor circuits that have been
   quiet recently.  This gives better latency on connections that aren't
   pushing lots of data, and makes the network feel more interactive.

   Conceptually, we take an exponentially weighted mean average of the number
   of cells a circuit has sent, and allow active circuits (those with cells to
   relay) to send cells in reverse order of their exponentially-weighted mean
   average (EWMA) cell count.  [That is, a cell sent N seconds ago 'counts'
   F^N times as much as a cell sent now, for 0<F<1.0, and we favor the
   circuit that has sent the fewest cells]

   If 'double' had infinite precision, we could do this simply by counting a
   cell sent at startup as having weight 1.0, and a cell sent N seconds later
   as having weight F^-N.  This way, we would never need to re-scale
   any already-sent cells.

   To prevent double from overflowing, we could count a cell sent now as
   having weight 1.0 and a cell sent N seconds ago as having weight F^N.
   This, however, would mean we'd need to re-scale *ALL* old circuits every
   time we wanted to send a cell.

   So as a compromise, we divide time into 'ticks' (currently, 10-second
   increments) and say that a cell sent at the start of a current tick is
   worth 1.0, a cell sent N seconds before the start of the current tick is
   worth F^N, and a cell sent N seconds after the start of the current tick is
   worth F^-N.  This way we don't overflow, and we don't need to constantly
   rescale.
 */

/** Given a timeval <b>now</b>, compute the cell_ewma tick in which it occurs
 * and the fraction of the tick that has elapsed between the start of the tick
 * and <b>now</b>.  Return the former and store the latter in
 * *<b>remainder_out</b>.
 *
 * These tick values are not meant to be shared between Tor instances, or used
 * for other purposes. */

static unsigned
cell_ewma_tick_from_timeval(const struct timeval *now,
                            double *remainder_out)
{
  unsigned res = (unsigned) (now->tv_sec / EWMA_TICK_LEN);
  /* rem */
  double rem = (now->tv_sec % EWMA_TICK_LEN) +
    ((double)(now->tv_usec)) / 1.0e6;
  *remainder_out = rem / EWMA_TICK_LEN;
  return res;
}

/** Tell the caller whether ewma_enabled is set */
int
cell_ewma_enabled(void)
{
  return ewma_enabled;
}

/** Compute and return the current cell_ewma tick. */
unsigned int
cell_ewma_get_tick(void)
{
  return ((unsigned)approx_time() / EWMA_TICK_LEN);
}

/** Adjust the global cell scale factor based on <b>options</b> */
void
cell_ewma_set_scale_factor(const or_options_t *options,
                           const networkstatus_t *consensus)
{
  int32_t halflife_ms;
  double halflife;
  const char *source;
  if (options && options->CircuitPriorityHalflife >= -EPSILON) {
    halflife = options->CircuitPriorityHalflife;
    source = "CircuitPriorityHalflife in configuration";
  } else if (consensus && (halflife_ms = networkstatus_get_param(
                 consensus, "CircuitPriorityHalflifeMsec",
                 -1, -1, INT32_MAX)) >= 0) {
    halflife = ((double)halflife_ms)/1000.0;
    source = "CircuitPriorityHalflifeMsec in consensus";
  } else {
    halflife = EWMA_DEFAULT_HALFLIFE;
    source = "Default value";
  }

  if (halflife <= EPSILON) {
    /* The cell EWMA algorithm is disabled. */
    ewma_scale_factor = 0.1;
    ewma_enabled = 0;
    log_info(LD_OR,
             "Disabled cell_ewma algorithm because of value in %s",
             source);
  } else {
    /* convert halflife into halflife-per-tick. */
    halflife /= EWMA_TICK_LEN;
    /* compute per-tick scale factor. */
    ewma_scale_factor = exp( LOG_ONEHALF / halflife );
    ewma_enabled = 1;
    log_info(LD_OR,
             "Enabled cell_ewma algorithm because of value in %s; "
             "scale factor is %f per %d seconds",
             source, ewma_scale_factor, EWMA_TICK_LEN);
  }
}

/** Return the multiplier necessary to convert the value of a cell sent in
 * 'from_tick' to one sent in 'to_tick'. */
static INLINE double
get_scale_factor(unsigned from_tick, unsigned to_tick)
{
  /* This math can wrap around, but that's okay: unsigned overflow is
     well-defined */
  int diff = (int)(to_tick - from_tick);
  return pow(ewma_scale_factor, diff);
}

/** Adjust the cell count of <b>ewma</b> so that it is scaled with respect to
 * <b>cur_tick</b> */
static void
scale_single_cell_ewma(cell_ewma_t *ewma, unsigned cur_tick)
{
  double factor = get_scale_factor(ewma->last_adjusted_tick, cur_tick);
  ewma->cell_count *= factor;
  ewma->last_adjusted_tick = cur_tick;
}

/** Adjust the cell count of every active circuit on <b>chan</b> so
 * that they are scaled with respect to <b>cur_tick</b> */
static void
scale_active_circuits(ewma_policy_data_t *pol, unsigned cur_tick)
{
  double factor;

  tor_assert(pol);
  tor_assert(pol->active_circuit_pqueue);

  factor =
    get_scale_factor(
      pol->active_circuit_pqueue_last_recalibrated,
      cur_tick);
  /** Ordinarily it isn't okay to change the value of an element in a heap,
   * but it's okay here, since we are preserving the order. */
  SMARTLIST_FOREACH_BEGIN(
      pol->active_circuit_pqueue,
      cell_ewma_t *, e) {
    tor_assert(e->last_adjusted_tick ==
               pol->active_circuit_pqueue_last_recalibrated);
    e->cell_count *= factor;
    e->last_adjusted_tick = cur_tick;
  } SMARTLIST_FOREACH_END(e);
  pol->active_circuit_pqueue_last_recalibrated = cur_tick;
}

/** Rescale <b>ewma</b> to the same scale as <b>pol</b>, and add it to
 * <b>pol</b>'s priority queue of active circuits */
static void
add_cell_ewma(ewma_policy_data_t *pol, cell_ewma_t *ewma)
{
  tor_assert(pol);
  tor_assert(pol->active_circuit_pqueue);
  tor_assert(ewma);
  tor_assert(ewma->heap_index == -1);

  scale_single_cell_ewma(
      ewma,
      pol->active_circuit_pqueue_last_recalibrated);

  smartlist_pqueue_add(pol->active_circuit_pqueue,
                       compare_cell_ewma_counts,
                       STRUCT_OFFSET(cell_ewma_t, heap_index),
                       ewma);
}

/** Remove <b>ewma</b> from <b>pol</b>'s priority queue of active circuits */
static void
remove_cell_ewma(ewma_policy_data_t *pol, cell_ewma_t *ewma)
{
  tor_assert(pol);
  tor_assert(pol->active_circuit_pqueue);
  tor_assert(ewma);
  tor_assert(ewma->heap_index != -1);

  smartlist_pqueue_remove(pol->active_circuit_pqueue,
                          compare_cell_ewma_counts,
                          STRUCT_OFFSET(cell_ewma_t, heap_index),
                          ewma);
}

/** Remove and return the first cell_ewma_t from pol's priority queue of
 * active circuits.  Requires that the priority queue is nonempty. */
static cell_ewma_t *
pop_first_cell_ewma(ewma_policy_data_t *pol)
{
  tor_assert(pol);
  tor_assert(pol->active_circuit_pqueue);

  return smartlist_pqueue_pop(pol->active_circuit_pqueue,
                              compare_cell_ewma_counts,
                              STRUCT_OFFSET(cell_ewma_t, heap_index));
}

