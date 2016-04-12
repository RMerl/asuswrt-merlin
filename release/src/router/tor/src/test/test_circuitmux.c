/* Copyright (c) 2013-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define TOR_CHANNEL_INTERNAL_
#define CIRCUITMUX_PRIVATE
#define RELAY_PRIVATE
#include "or.h"
#include "channel.h"
#include "circuitmux.h"
#include "relay.h"
#include "scheduler.h"
#include "test.h"

/* XXXX duplicated function from test_circuitlist.c */
static channel_t *
new_fake_channel(void)
{
  channel_t *chan = tor_malloc_zero(sizeof(channel_t));
  channel_init(chan);
  return chan;
}

static int
has_queued_writes(channel_t *c)
{
  (void) c;
  return 1;
}

/** Test destroy cell queue with no interference from other queues. */
static void
test_cmux_destroy_cell_queue(void *arg)
{
  circuitmux_t *cmux = NULL;
  channel_t *ch = NULL;
  circuit_t *circ = NULL;
  cell_queue_t *cq = NULL;
  packed_cell_t *pc = NULL;
  tor_libevent_cfg cfg;

  memset(&cfg, 0, sizeof(cfg));

  tor_libevent_initialize(&cfg);
  scheduler_init();

  (void) arg;

  cmux = circuitmux_alloc();
  tt_assert(cmux);
  ch = new_fake_channel();
  ch->has_queued_writes = has_queued_writes;
  ch->wide_circ_ids = 1;

  circ = circuitmux_get_first_active_circuit(cmux, &cq);
  tt_assert(!circ);
  tt_assert(!cq);

  circuitmux_append_destroy_cell(ch, cmux, 100, 10);
  circuitmux_append_destroy_cell(ch, cmux, 190, 6);
  circuitmux_append_destroy_cell(ch, cmux, 30, 1);

  tt_int_op(circuitmux_num_cells(cmux), OP_EQ, 3);

  circ = circuitmux_get_first_active_circuit(cmux, &cq);
  tt_assert(!circ);
  tt_assert(cq);

  tt_int_op(cq->n, OP_EQ, 3);

  pc = cell_queue_pop(cq);
  tt_assert(pc);
  tt_mem_op(pc->body, OP_EQ, "\x00\x00\x00\x64\x04\x0a\x00\x00\x00", 9);
  packed_cell_free(pc);
  pc = NULL;

  tt_int_op(circuitmux_num_cells(cmux), OP_EQ, 2);

 done:
  circuitmux_free(cmux);
  channel_free(ch);
  packed_cell_free(pc);
}

struct testcase_t circuitmux_tests[] = {
  { "destroy_cell_queue", test_cmux_destroy_cell_queue, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

