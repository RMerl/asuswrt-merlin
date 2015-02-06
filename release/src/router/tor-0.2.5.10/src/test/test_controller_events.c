/* Copyright (c) 2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define CONNECTION_PRIVATE
#define TOR_CHANNEL_INTERNAL_
#define CONTROL_PRIVATE
#include "or.h"
#include "channel.h"
#include "channeltls.h"
#include "connection.h"
#include "control.h"
#include "test.h"

static void
help_test_bucket_note_empty(uint32_t expected_msec_since_midnight,
                            int tokens_before, size_t tokens_removed,
                            uint32_t msec_since_epoch)
{
  uint32_t timestamp_var = 0;
  struct timeval tvnow;
  tvnow.tv_sec = msec_since_epoch / 1000;
  tvnow.tv_usec = (msec_since_epoch % 1000) * 1000;
  connection_buckets_note_empty_ts(&timestamp_var, tokens_before,
                                   tokens_removed, &tvnow);
  tt_int_op(expected_msec_since_midnight, ==, timestamp_var);

 done:
  ;
}

static void
test_cntev_bucket_note_empty(void *arg)
{
  (void)arg;

  /* Two cases with nothing to note, because bucket was empty before;
   * 86442200 == 1970-01-02 00:00:42.200000 */
  help_test_bucket_note_empty(0, 0, 0, 86442200);
  help_test_bucket_note_empty(0, -100, 100, 86442200);

  /* Nothing to note, because bucket has not been emptied. */
  help_test_bucket_note_empty(0, 101, 100, 86442200);

  /* Bucket was emptied, note 42200 msec since midnight. */
  help_test_bucket_note_empty(42200, 101, 101, 86442200);
  help_test_bucket_note_empty(42200, 101, 102, 86442200);
}

static void
test_cntev_bucket_millis_empty(void *arg)
{
  struct timeval tvnow;
  (void)arg;

  /* 1970-01-02 00:00:42.200000 */
  tvnow.tv_sec = 86400 + 42;
  tvnow.tv_usec = 200000;

  /* Bucket has not been refilled. */
  tt_int_op(0, ==, bucket_millis_empty(0, 42120, 0, 100, &tvnow));
  tt_int_op(0, ==, bucket_millis_empty(-10, 42120, -10, 100, &tvnow));

  /* Bucket was not empty. */
  tt_int_op(0, ==, bucket_millis_empty(10, 42120, 20, 100, &tvnow));

  /* Bucket has been emptied 80 msec ago and has just been refilled. */
  tt_int_op(80, ==, bucket_millis_empty(-20, 42120, -10, 100, &tvnow));
  tt_int_op(80, ==, bucket_millis_empty(-10, 42120, 0, 100, &tvnow));
  tt_int_op(80, ==, bucket_millis_empty(0, 42120, 10, 100, &tvnow));

  /* Bucket has been emptied 180 msec ago, last refill was 100 msec ago
   * which was insufficient to make it positive, so cap msec at 100. */
  tt_int_op(100, ==, bucket_millis_empty(0, 42020, 1, 100, &tvnow));

  /* 1970-01-02 00:00:00:050000 */
  tvnow.tv_sec = 86400;
  tvnow.tv_usec = 50000;

  /* Last emptied 30 msec before midnight, tvnow is 50 msec after
   * midnight, that's 80 msec in total. */
  tt_int_op(80, ==, bucket_millis_empty(0, 86400000 - 30, 1, 100, &tvnow));

 done:
  ;
}

static void
add_testing_cell_stats_entry(circuit_t *circ, uint8_t command,
                             unsigned int waiting_time,
                             unsigned int removed, unsigned int exitward)
{
  testing_cell_stats_entry_t *ent = tor_malloc_zero(
                                    sizeof(testing_cell_stats_entry_t));
  ent->command = command;
  ent->waiting_time = waiting_time;
  ent->removed = removed;
  ent->exitward = exitward;
  if (!circ->testing_cell_stats)
    circ->testing_cell_stats = smartlist_new();
  smartlist_add(circ->testing_cell_stats, ent);
}

static void
test_cntev_sum_up_cell_stats(void *arg)
{
  or_circuit_t *or_circ;
  circuit_t *circ;
  cell_stats_t *cell_stats = NULL;
  (void)arg;

  /* This circuit is fake. */
  or_circ = tor_malloc_zero(sizeof(or_circuit_t));
  or_circ->base_.magic = OR_CIRCUIT_MAGIC;
  or_circ->base_.purpose = CIRCUIT_PURPOSE_OR;
  circ = TO_CIRCUIT(or_circ);

  /* A single RELAY cell was added to the appward queue. */
  cell_stats = tor_malloc_zero(sizeof(cell_stats_t));
  add_testing_cell_stats_entry(circ, CELL_RELAY, 0, 0, 0);
  sum_up_cell_stats_by_command(circ, cell_stats);
  tt_u64_op(1, ==, cell_stats->added_cells_appward[CELL_RELAY]);

  /* A single RELAY cell was added to the exitward queue. */
  add_testing_cell_stats_entry(circ, CELL_RELAY, 0, 0, 1);
  sum_up_cell_stats_by_command(circ, cell_stats);
  tt_u64_op(1, ==, cell_stats->added_cells_exitward[CELL_RELAY]);

  /* A single RELAY cell was removed from the appward queue where it spent
   * 20 msec. */
  add_testing_cell_stats_entry(circ, CELL_RELAY, 2, 1, 0);
  sum_up_cell_stats_by_command(circ, cell_stats);
  tt_u64_op(20, ==, cell_stats->total_time_appward[CELL_RELAY]);
  tt_u64_op(1, ==, cell_stats->removed_cells_appward[CELL_RELAY]);

  /* A single RELAY cell was removed from the exitward queue where it
   * spent 30 msec. */
  add_testing_cell_stats_entry(circ, CELL_RELAY, 3, 1, 1);
  sum_up_cell_stats_by_command(circ, cell_stats);
  tt_u64_op(30, ==, cell_stats->total_time_exitward[CELL_RELAY]);
  tt_u64_op(1, ==, cell_stats->removed_cells_exitward[CELL_RELAY]);

 done:
  tor_free(cell_stats);
  tor_free(or_circ);
}

static void
test_cntev_append_cell_stats(void *arg)
{
  smartlist_t *event_parts;
  char *cp = NULL;
  const char *key = "Z";
  uint64_t include_if_non_zero[CELL_COMMAND_MAX_ + 1],
           number_to_include[CELL_COMMAND_MAX_ + 1];
  (void)arg;

  event_parts = smartlist_new();
  memset(include_if_non_zero, 0,
         (CELL_COMMAND_MAX_ + 1) * sizeof(uint64_t));
  memset(number_to_include, 0,
         (CELL_COMMAND_MAX_ + 1) * sizeof(uint64_t));

  /* All array entries empty. */
  append_cell_stats_by_command(event_parts, key,
                               include_if_non_zero,
                               number_to_include);
  tt_int_op(0, ==, smartlist_len(event_parts));

  /* There's a RELAY cell to include, but the corresponding field in
   * include_if_non_zero is still zero. */
  number_to_include[CELL_RELAY] = 1;
  append_cell_stats_by_command(event_parts, key,
                               include_if_non_zero,
                               number_to_include);
  tt_int_op(0, ==, smartlist_len(event_parts));

  /* Now include single RELAY cell. */
  include_if_non_zero[CELL_RELAY] = 2;
  append_cell_stats_by_command(event_parts, key,
                               include_if_non_zero,
                               number_to_include);
  cp = smartlist_pop_last(event_parts);
  tt_str_op("Z=relay:1", ==, cp);
  tor_free(cp);

  /* Add four CREATE cells. */
  include_if_non_zero[CELL_CREATE] = 3;
  number_to_include[CELL_CREATE] = 4;
  append_cell_stats_by_command(event_parts, key,
                               include_if_non_zero,
                               number_to_include);
  cp = smartlist_pop_last(event_parts);
  tt_str_op("Z=create:4,relay:1", ==, cp);

 done:
  tor_free(cp);
  smartlist_free(event_parts);
}

static void
test_cntev_format_cell_stats(void *arg)
{
  char *event_string = NULL;
  origin_circuit_t *ocirc = NULL;
  or_circuit_t *or_circ = NULL;
  cell_stats_t *cell_stats = NULL;
  channel_tls_t *n_chan=NULL, *p_chan=NULL;
  (void)arg;

  n_chan = tor_malloc_zero(sizeof(channel_tls_t));
  n_chan->base_.global_identifier = 1;

  ocirc = tor_malloc_zero(sizeof(origin_circuit_t));
  ocirc->base_.magic = ORIGIN_CIRCUIT_MAGIC;
  ocirc->base_.purpose = CIRCUIT_PURPOSE_C_GENERAL;
  ocirc->global_identifier = 2;
  ocirc->base_.n_circ_id = 3;
  ocirc->base_.n_chan = &(n_chan->base_);

  /* Origin circuit was completely idle. */
  cell_stats = tor_malloc_zero(sizeof(cell_stats_t));
  format_cell_stats(&event_string, TO_CIRCUIT(ocirc), cell_stats);
  tt_str_op("ID=2 OutboundQueue=3 OutboundConn=1", ==, event_string);
  tor_free(event_string);

  /* Origin circuit had 4 RELAY cells added to its exitward queue. */
  cell_stats->added_cells_exitward[CELL_RELAY] = 4;
  format_cell_stats(&event_string, TO_CIRCUIT(ocirc), cell_stats);
  tt_str_op("ID=2 OutboundQueue=3 OutboundConn=1 OutboundAdded=relay:4",
            ==, event_string);
  tor_free(event_string);

  /* Origin circuit also had 5 CREATE2 cells added to its exitward
   * queue. */
  cell_stats->added_cells_exitward[CELL_CREATE2] = 5;
  format_cell_stats(&event_string, TO_CIRCUIT(ocirc), cell_stats);
  tt_str_op("ID=2 OutboundQueue=3 OutboundConn=1 OutboundAdded=relay:4,"
            "create2:5", ==, event_string);
  tor_free(event_string);

  /* Origin circuit also had 7 RELAY cells removed from its exitward queue
   * which together spent 6 msec in the queue. */
  cell_stats->total_time_exitward[CELL_RELAY] = 6;
  cell_stats->removed_cells_exitward[CELL_RELAY] = 7;
  format_cell_stats(&event_string, TO_CIRCUIT(ocirc), cell_stats);
  tt_str_op("ID=2 OutboundQueue=3 OutboundConn=1 OutboundAdded=relay:4,"
            "create2:5 OutboundRemoved=relay:7 OutboundTime=relay:6",
            ==, event_string);
  tor_free(event_string);

  p_chan = tor_malloc_zero(sizeof(channel_tls_t));
  p_chan->base_.global_identifier = 2;

  or_circ = tor_malloc_zero(sizeof(or_circuit_t));
  or_circ->base_.magic = OR_CIRCUIT_MAGIC;
  or_circ->base_.purpose = CIRCUIT_PURPOSE_OR;
  or_circ->p_circ_id = 8;
  or_circ->p_chan = &(p_chan->base_);
  or_circ->base_.n_circ_id = 9;
  or_circ->base_.n_chan = &(n_chan->base_);

  tor_free(cell_stats);

  /* OR circuit was idle. */
  cell_stats = tor_malloc_zero(sizeof(cell_stats_t));
  format_cell_stats(&event_string, TO_CIRCUIT(or_circ), cell_stats);
  tt_str_op("InboundQueue=8 InboundConn=2 OutboundQueue=9 OutboundConn=1",
            ==, event_string);
  tor_free(event_string);

  /* OR circuit had 3 RELAY cells added to its appward queue. */
  cell_stats->added_cells_appward[CELL_RELAY] = 3;
  format_cell_stats(&event_string, TO_CIRCUIT(or_circ), cell_stats);
  tt_str_op("InboundQueue=8 InboundConn=2 InboundAdded=relay:3 "
            "OutboundQueue=9 OutboundConn=1", ==, event_string);
  tor_free(event_string);

  /* OR circuit had 7 RELAY cells removed from its appward queue which
   * together spent 6 msec in the queue. */
  cell_stats->total_time_appward[CELL_RELAY] = 6;
  cell_stats->removed_cells_appward[CELL_RELAY] = 7;
  format_cell_stats(&event_string, TO_CIRCUIT(or_circ), cell_stats);
  tt_str_op("InboundQueue=8 InboundConn=2 InboundAdded=relay:3 "
            "InboundRemoved=relay:7 InboundTime=relay:6 "
            "OutboundQueue=9 OutboundConn=1", ==, event_string);

 done:
  tor_free(cell_stats);
  tor_free(event_string);
  tor_free(or_circ);
  tor_free(ocirc);
  tor_free(p_chan);
  tor_free(n_chan);
}

#define TEST(name, flags)                                               \
  { #name, test_cntev_ ## name, flags, 0, NULL }

struct testcase_t controller_event_tests[] = {
  TEST(bucket_note_empty, 0),
  TEST(bucket_millis_empty, 0),
  TEST(sum_up_cell_stats, 0),
  TEST(append_cell_stats, 0),
  TEST(format_cell_stats, 0),
  END_OF_TESTCASES
};

