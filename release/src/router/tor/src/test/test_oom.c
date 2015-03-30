/* Copyright (c) 2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/* Unit tests for OOM handling logic */

#define RELAY_PRIVATE
#define BUFFERS_PRIVATE
#define CIRCUITLIST_PRIVATE
#define CONNECTION_PRIVATE
#include "or.h"
#include "buffers.h"
#include "circuitlist.h"
#include "compat_libevent.h"
#include "connection.h"
#include "config.h"
#ifdef ENABLE_MEMPOOLS
#include "mempool.h"
#endif
#include "relay.h"
#include "test.h"

/* small replacement mock for circuit_mark_for_close_ to avoid doing all
 * the other bookkeeping that comes with marking circuits. */
static void
circuit_mark_for_close_dummy_(circuit_t *circ, int reason, int line,
                              const char *file)
{
  (void) reason;
  if (circ->marked_for_close) {
    TT_FAIL(("Circuit already marked for close at %s:%d, but we are marking "
             "it again at %s:%d",
             circ->marked_for_close_file, (int)circ->marked_for_close,
             file, line));
  }

  circ->marked_for_close = line;
  circ->marked_for_close_file = file;
}

static circuit_t *
dummy_or_circuit_new(int n_p_cells, int n_n_cells)
{
  or_circuit_t *circ = or_circuit_new(0, NULL);
  int i;
  cell_t cell;

  for (i=0; i < n_p_cells; ++i) {
    crypto_rand((void*)&cell, sizeof(cell));
    cell_queue_append_packed_copy(TO_CIRCUIT(circ), &circ->p_chan_cells,
                                  0, &cell, 1, 0);
  }

  for (i=0; i < n_n_cells; ++i) {
    crypto_rand((void*)&cell, sizeof(cell));
    cell_queue_append_packed_copy(TO_CIRCUIT(circ),
                                  &TO_CIRCUIT(circ)->n_chan_cells,
                                  1, &cell, 1, 0);
  }

  TO_CIRCUIT(circ)->purpose = CIRCUIT_PURPOSE_OR;
  return TO_CIRCUIT(circ);
}

static circuit_t *
dummy_origin_circuit_new(int n_cells)
{
  origin_circuit_t *circ = origin_circuit_new();
  int i;
  cell_t cell;

  for (i=0; i < n_cells; ++i) {
    crypto_rand((void*)&cell, sizeof(cell));
    cell_queue_append_packed_copy(TO_CIRCUIT(circ),
                                  &TO_CIRCUIT(circ)->n_chan_cells,
                                  1, &cell, 1, 0);
  }

  TO_CIRCUIT(circ)->purpose = CIRCUIT_PURPOSE_C_GENERAL;
  return TO_CIRCUIT(circ);
}

static void
add_bytes_to_buf(generic_buffer_t *buf, size_t n_bytes)
{
  char b[3000];

  while (n_bytes) {
    size_t this_add = n_bytes > sizeof(b) ? sizeof(b) : n_bytes;
    crypto_rand(b, this_add);
    generic_buffer_add(buf, b, this_add);
    n_bytes -= this_add;
  }
}

static edge_connection_t *
dummy_edge_conn_new(circuit_t *circ,
                    int type, size_t in_bytes, size_t out_bytes)
{
  edge_connection_t *conn;
  generic_buffer_t *inbuf, *outbuf;

  if (type == CONN_TYPE_EXIT)
    conn = edge_connection_new(type, AF_INET);
  else
    conn = ENTRY_TO_EDGE_CONN(entry_connection_new(type, AF_INET));

#ifdef USE_BUFFEREVENTS
  inbuf = bufferevent_get_input(TO_CONN(conn)->bufev);
  outbuf = bufferevent_get_output(TO_CONN(conn)->bufev);
#else
  inbuf = TO_CONN(conn)->inbuf;
  outbuf = TO_CONN(conn)->outbuf;
#endif

  /* We add these bytes directly to the buffers, to avoid all the
   * edge connection read/write machinery. */
  add_bytes_to_buf(inbuf, in_bytes);
  add_bytes_to_buf(outbuf, out_bytes);

  conn->on_circuit = circ;
  if (type == CONN_TYPE_EXIT) {
    or_circuit_t *oc  = TO_OR_CIRCUIT(circ);
    conn->next_stream = oc->n_streams;
    oc->n_streams = conn;
  } else {
    origin_circuit_t *oc = TO_ORIGIN_CIRCUIT(circ);
    conn->next_stream = oc->p_streams;
    oc->p_streams = conn;
  }

  return conn;
}

/** Run unit tests for buffers.c */
static void
test_oom_circbuf(void *arg)
{
  or_options_t *options = get_options_mutable();
  circuit_t *c1 = NULL, *c2 = NULL, *c3 = NULL, *c4 = NULL;
  struct timeval tv = { 1389631048, 0 };

  (void) arg;

  MOCK(circuit_mark_for_close_, circuit_mark_for_close_dummy_);

#ifdef ENABLE_MEMPOOLS
  init_cell_pool();
#endif /* ENABLE_MEMPOOLS */

  /* Far too low for real life. */
  options->MaxMemInQueues = 256*packed_cell_mem_cost();
  options->CellStatistics = 0;

  tt_int_op(cell_queues_check_size(), ==, 0); /* We don't start out OOM. */
  tt_int_op(cell_queues_get_total_allocation(), ==, 0);
  tt_int_op(buf_get_total_allocation(), ==, 0);

  /* Now we're going to fake up some circuits and get them added to the global
     circuit list. */
  tv.tv_usec = 0;
  tor_gettimeofday_cache_set(&tv);
  c1 = dummy_origin_circuit_new(30);
  tv.tv_usec = 10*1000;
  tor_gettimeofday_cache_set(&tv);
  c2 = dummy_or_circuit_new(20, 20);

#ifdef ENABLE_MEMPOOLS
  tt_int_op(packed_cell_mem_cost(), ==,
            sizeof(packed_cell_t) + MP_POOL_ITEM_OVERHEAD);
#else
  tt_int_op(packed_cell_mem_cost(), ==,
            sizeof(packed_cell_t));
#endif /* ENABLE_MEMPOOLS */
  tt_int_op(cell_queues_get_total_allocation(), ==,
            packed_cell_mem_cost() * 70);
  tt_int_op(cell_queues_check_size(), ==, 0); /* We are still not OOM */

  tv.tv_usec = 20*1000;
  tor_gettimeofday_cache_set(&tv);
  c3 = dummy_or_circuit_new(100, 85);
  tt_int_op(cell_queues_check_size(), ==, 0); /* We are still not OOM */
  tt_int_op(cell_queues_get_total_allocation(), ==,
            packed_cell_mem_cost() * 255);

  tv.tv_usec = 30*1000;
  tor_gettimeofday_cache_set(&tv);
  /* Adding this cell will trigger our OOM handler. */
  c4 = dummy_or_circuit_new(2, 0);

  tt_int_op(cell_queues_get_total_allocation(), ==,
            packed_cell_mem_cost() * 257);

  tt_int_op(cell_queues_check_size(), ==, 1); /* We are now OOM */

  tt_assert(c1->marked_for_close);
  tt_assert(! c2->marked_for_close);
  tt_assert(! c3->marked_for_close);
  tt_assert(! c4->marked_for_close);

  tt_int_op(cell_queues_get_total_allocation(), ==,
            packed_cell_mem_cost() * (257 - 30));

  circuit_free(c1);
  tv.tv_usec = 0;
  tor_gettimeofday_cache_set(&tv); /* go back in time */
  c1 = dummy_or_circuit_new(90, 0);

  tv.tv_usec = 40*1000; /* go back to the future */
  tor_gettimeofday_cache_set(&tv);

  tt_int_op(cell_queues_check_size(), ==, 1); /* We are now OOM */

  tt_assert(c1->marked_for_close);
  tt_assert(! c2->marked_for_close);
  tt_assert(! c3->marked_for_close);
  tt_assert(! c4->marked_for_close);

  tt_int_op(cell_queues_get_total_allocation(), ==,
            packed_cell_mem_cost() * (257 - 30));

 done:
  circuit_free(c1);
  circuit_free(c2);
  circuit_free(c3);
  circuit_free(c4);

  UNMOCK(circuit_mark_for_close_);
}

/** Run unit tests for buffers.c */
static void
test_oom_streambuf(void *arg)
{
  or_options_t *options = get_options_mutable();
  circuit_t *c1 = NULL, *c2 = NULL, *c3 = NULL, *c4 = NULL, *c5 = NULL;
  struct timeval tv = { 1389641159, 0 };
  uint32_t tvms;
  int i;
  smartlist_t *edgeconns = smartlist_new();

  (void) arg;

  MOCK(circuit_mark_for_close_, circuit_mark_for_close_dummy_);

#ifdef ENABLE_MEMPOOLS
  init_cell_pool();
#endif /* ENABLE_MEMPOOLS */

  /* Far too low for real life. */
  options->MaxMemInQueues = 81*packed_cell_mem_cost() + 4096 * 34;
  options->CellStatistics = 0;

  tt_int_op(cell_queues_check_size(), ==, 0); /* We don't start out OOM. */
  tt_int_op(cell_queues_get_total_allocation(), ==, 0);
  tt_int_op(buf_get_total_allocation(), ==, 0);

  /* Start all circuits with a bit of data queued in cells */
  tv.tv_usec = 500*1000; /* go halfway into the second. */
  tor_gettimeofday_cache_set(&tv);
  c1 = dummy_or_circuit_new(10,10);
  tv.tv_usec = 510*1000;
  tor_gettimeofday_cache_set(&tv);
  c2 = dummy_origin_circuit_new(20);
  tv.tv_usec = 520*1000;
  tor_gettimeofday_cache_set(&tv);
  c3 = dummy_or_circuit_new(20,20);
  tv.tv_usec = 530*1000;
  tor_gettimeofday_cache_set(&tv);
  c4 = dummy_or_circuit_new(0,0);
  tt_int_op(cell_queues_get_total_allocation(), ==,
            packed_cell_mem_cost() * 80);

  tv.tv_usec = 600*1000;
  tor_gettimeofday_cache_set(&tv);

  /* Add some connections to c1...c4. */
  for (i = 0; i < 4; ++i) {
    edge_connection_t *ec;
    /* link it to a circuit */
    tv.tv_usec += 10*1000;
    tor_gettimeofday_cache_set(&tv);
    ec = dummy_edge_conn_new(c1, CONN_TYPE_EXIT, 1000, 1000);
    tt_assert(ec);
    smartlist_add(edgeconns, ec);
    tv.tv_usec += 10*1000;
    tor_gettimeofday_cache_set(&tv);
    ec = dummy_edge_conn_new(c2, CONN_TYPE_AP, 1000, 1000);
    tt_assert(ec);
    smartlist_add(edgeconns, ec);
    tv.tv_usec += 10*1000;
    tor_gettimeofday_cache_set(&tv);
    ec = dummy_edge_conn_new(c4, CONN_TYPE_EXIT, 1000, 1000); /* Yes, 4 twice*/
    tt_assert(ec);
    smartlist_add(edgeconns, ec);
    tv.tv_usec += 10*1000;
    tor_gettimeofday_cache_set(&tv);
    ec = dummy_edge_conn_new(c4, CONN_TYPE_EXIT, 1000, 1000);
    smartlist_add(edgeconns, ec);
    tt_assert(ec);
  }

  tv.tv_sec += 1;
  tv.tv_usec = 0;
  tvms = (uint32_t) tv_to_msec(&tv);

  tt_int_op(circuit_max_queued_cell_age(c1, tvms), ==, 500);
  tt_int_op(circuit_max_queued_cell_age(c2, tvms), ==, 490);
  tt_int_op(circuit_max_queued_cell_age(c3, tvms), ==, 480);
  tt_int_op(circuit_max_queued_cell_age(c4, tvms), ==, 0);

  tt_int_op(circuit_max_queued_data_age(c1, tvms), ==, 390);
  tt_int_op(circuit_max_queued_data_age(c2, tvms), ==, 380);
  tt_int_op(circuit_max_queued_data_age(c3, tvms), ==, 0);
  tt_int_op(circuit_max_queued_data_age(c4, tvms), ==, 370);

  tt_int_op(circuit_max_queued_item_age(c1, tvms), ==, 500);
  tt_int_op(circuit_max_queued_item_age(c2, tvms), ==, 490);
  tt_int_op(circuit_max_queued_item_age(c3, tvms), ==, 480);
  tt_int_op(circuit_max_queued_item_age(c4, tvms), ==, 370);

  tt_int_op(cell_queues_get_total_allocation(), ==,
            packed_cell_mem_cost() * 80);
  tt_int_op(buf_get_total_allocation(), ==, 4096*16*2);

  /* Now give c4 a very old buffer of modest size */
  {
    edge_connection_t *ec;
    tv.tv_sec -= 1;
    tv.tv_usec = 0;
    tor_gettimeofday_cache_set(&tv);
    ec = dummy_edge_conn_new(c4, CONN_TYPE_EXIT, 1000, 1000);
    tt_assert(ec);
    smartlist_add(edgeconns, ec);
  }
  tt_int_op(buf_get_total_allocation(), ==, 4096*17*2);
  tt_int_op(circuit_max_queued_item_age(c4, tvms), ==, 1000);

  tt_int_op(cell_queues_check_size(), ==, 0);

  /* And run over the limit. */
  tv.tv_usec = 800*1000;
  tor_gettimeofday_cache_set(&tv);
  c5 = dummy_or_circuit_new(0,5);

  tt_int_op(cell_queues_get_total_allocation(), ==,
            packed_cell_mem_cost() * 85);
  tt_int_op(buf_get_total_allocation(), ==, 4096*17*2);

  tt_int_op(cell_queues_check_size(), ==, 1); /* We are now OOM */

  /* C4 should have died. */
  tt_assert(! c1->marked_for_close);
  tt_assert(! c2->marked_for_close);
  tt_assert(! c3->marked_for_close);
  tt_assert(c4->marked_for_close);
  tt_assert(! c5->marked_for_close);

  tt_int_op(cell_queues_get_total_allocation(), ==,
            packed_cell_mem_cost() * 85);
  tt_int_op(buf_get_total_allocation(), ==, 4096*8*2);

 done:
  circuit_free(c1);
  circuit_free(c2);
  circuit_free(c3);
  circuit_free(c4);
  circuit_free(c5);

  SMARTLIST_FOREACH(edgeconns, edge_connection_t *, ec,
                    connection_free_(TO_CONN(ec)));
  smartlist_free(edgeconns);

  UNMOCK(circuit_mark_for_close_);
}

struct testcase_t oom_tests[] = {
  { "circbuf", test_oom_circbuf, TT_FORK, NULL, NULL },
  { "streambuf", test_oom_streambuf, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

