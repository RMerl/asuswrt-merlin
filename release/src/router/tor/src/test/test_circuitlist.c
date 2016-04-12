/* Copyright (c) 2013-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define TOR_CHANNEL_INTERNAL_
#define CIRCUITBUILD_PRIVATE
#define CIRCUITLIST_PRIVATE
#include "or.h"
#include "channel.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "test.h"

static channel_t *
new_fake_channel(void)
{
  channel_t *chan = tor_malloc_zero(sizeof(channel_t));
  channel_init(chan);
  return chan;
}

static struct {
  int ncalls;
  void *cmux;
  void *circ;
  cell_direction_t dir;
} cam;

static void
circuitmux_attach_mock(circuitmux_t *cmux, circuit_t *circ,
                         cell_direction_t dir)
{
  ++cam.ncalls;
  cam.cmux = cmux;
  cam.circ = circ;
  cam.dir = dir;
}

static struct {
  int ncalls;
  void *cmux;
  void *circ;
} cdm;

static void
circuitmux_detach_mock(circuitmux_t *cmux, circuit_t *circ)
{
  ++cdm.ncalls;
  cdm.cmux = cmux;
  cdm.circ = circ;
}

#define GOT_CMUX_ATTACH(mux_, circ_, dir_) do {  \
    tt_int_op(cam.ncalls, OP_EQ, 1);                \
    tt_ptr_op(cam.cmux, OP_EQ, (mux_));             \
    tt_ptr_op(cam.circ, OP_EQ, (circ_));            \
    tt_int_op(cam.dir, OP_EQ, (dir_));              \
    memset(&cam, 0, sizeof(cam));                \
  } while (0)

#define GOT_CMUX_DETACH(mux_, circ_) do {        \
    tt_int_op(cdm.ncalls, OP_EQ, 1);                \
    tt_ptr_op(cdm.cmux, OP_EQ, (mux_));             \
    tt_ptr_op(cdm.circ, OP_EQ, (circ_));            \
    memset(&cdm, 0, sizeof(cdm));                \
  } while (0)

static void
test_clist_maps(void *arg)
{
  channel_t *ch1 = new_fake_channel();
  channel_t *ch2 = new_fake_channel();
  channel_t *ch3 = new_fake_channel();
  or_circuit_t *or_c1=NULL, *or_c2=NULL;

  (void) arg;

  MOCK(circuitmux_attach_circuit, circuitmux_attach_mock);
  MOCK(circuitmux_detach_circuit, circuitmux_detach_mock);
  memset(&cam, 0, sizeof(cam));
  memset(&cdm, 0, sizeof(cdm));

  tt_assert(ch1);
  tt_assert(ch2);
  tt_assert(ch3);

  ch1->cmux = tor_malloc(1);
  ch2->cmux = tor_malloc(1);
  ch3->cmux = tor_malloc(1);

  or_c1 = or_circuit_new(100, ch2);
  tt_assert(or_c1);
  GOT_CMUX_ATTACH(ch2->cmux, or_c1, CELL_DIRECTION_IN);
  tt_int_op(or_c1->p_circ_id, OP_EQ, 100);
  tt_ptr_op(or_c1->p_chan, OP_EQ, ch2);

  or_c2 = or_circuit_new(100, ch1);
  tt_assert(or_c2);
  GOT_CMUX_ATTACH(ch1->cmux, or_c2, CELL_DIRECTION_IN);
  tt_int_op(or_c2->p_circ_id, OP_EQ, 100);
  tt_ptr_op(or_c2->p_chan, OP_EQ, ch1);

  circuit_set_n_circid_chan(TO_CIRCUIT(or_c1), 200, ch1);
  GOT_CMUX_ATTACH(ch1->cmux, or_c1, CELL_DIRECTION_OUT);

  circuit_set_n_circid_chan(TO_CIRCUIT(or_c2), 200, ch2);
  GOT_CMUX_ATTACH(ch2->cmux, or_c2, CELL_DIRECTION_OUT);

  tt_ptr_op(circuit_get_by_circid_channel(200, ch1), OP_EQ, TO_CIRCUIT(or_c1));
  tt_ptr_op(circuit_get_by_circid_channel(200, ch2), OP_EQ, TO_CIRCUIT(or_c2));
  tt_ptr_op(circuit_get_by_circid_channel(100, ch2), OP_EQ, TO_CIRCUIT(or_c1));
  /* Try the same thing again, to test the "fast" path. */
  tt_ptr_op(circuit_get_by_circid_channel(100, ch2), OP_EQ, TO_CIRCUIT(or_c1));
  tt_assert(circuit_id_in_use_on_channel(100, ch2));
  tt_assert(! circuit_id_in_use_on_channel(101, ch2));

  /* Try changing the circuitid and channel of that circuit. */
  circuit_set_p_circid_chan(or_c1, 500, ch3);
  GOT_CMUX_DETACH(ch2->cmux, TO_CIRCUIT(or_c1));
  GOT_CMUX_ATTACH(ch3->cmux, TO_CIRCUIT(or_c1), CELL_DIRECTION_IN);
  tt_ptr_op(circuit_get_by_circid_channel(100, ch2), OP_EQ, NULL);
  tt_assert(! circuit_id_in_use_on_channel(100, ch2));
  tt_ptr_op(circuit_get_by_circid_channel(500, ch3), OP_EQ, TO_CIRCUIT(or_c1));

  /* Now let's see about destroy handling. */
  tt_assert(! circuit_id_in_use_on_channel(205, ch2));
  tt_assert(circuit_id_in_use_on_channel(200, ch2));
  channel_note_destroy_pending(ch2, 200);
  channel_note_destroy_pending(ch2, 205);
  channel_note_destroy_pending(ch1, 100);
  tt_assert(circuit_id_in_use_on_channel(205, ch2))
  tt_assert(circuit_id_in_use_on_channel(200, ch2));
  tt_assert(circuit_id_in_use_on_channel(100, ch1));

  tt_assert(TO_CIRCUIT(or_c2)->n_delete_pending != 0);
  tt_ptr_op(circuit_get_by_circid_channel(200, ch2), OP_EQ, TO_CIRCUIT(or_c2));
  tt_ptr_op(circuit_get_by_circid_channel(100, ch1), OP_EQ, TO_CIRCUIT(or_c2));

  /* Okay, now free ch2 and make sure that the circuit ID is STILL not
   * usable, because we haven't declared the destroy to be nonpending */
  tt_int_op(cdm.ncalls, OP_EQ, 0);
  circuit_free(TO_CIRCUIT(or_c2));
  or_c2 = NULL; /* prevent free */
  tt_int_op(cdm.ncalls, OP_EQ, 2);
  memset(&cdm, 0, sizeof(cdm));
  tt_assert(circuit_id_in_use_on_channel(200, ch2));
  tt_assert(circuit_id_in_use_on_channel(100, ch1));
  tt_ptr_op(circuit_get_by_circid_channel(200, ch2), OP_EQ, NULL);
  tt_ptr_op(circuit_get_by_circid_channel(100, ch1), OP_EQ, NULL);

  /* Now say that the destroy is nonpending */
  channel_note_destroy_not_pending(ch2, 200);
  tt_ptr_op(circuit_get_by_circid_channel(200, ch2), OP_EQ, NULL);
  channel_note_destroy_not_pending(ch1, 100);
  tt_ptr_op(circuit_get_by_circid_channel(100, ch1), OP_EQ, NULL);
  tt_assert(! circuit_id_in_use_on_channel(200, ch2));
  tt_assert(! circuit_id_in_use_on_channel(100, ch1));

 done:
  if (or_c1)
    circuit_free(TO_CIRCUIT(or_c1));
  if (or_c2)
    circuit_free(TO_CIRCUIT(or_c2));
  if (ch1)
    tor_free(ch1->cmux);
  if (ch2)
    tor_free(ch2->cmux);
  if (ch3)
    tor_free(ch3->cmux);
  tor_free(ch1);
  tor_free(ch2);
  tor_free(ch3);
  UNMOCK(circuitmux_attach_circuit);
  UNMOCK(circuitmux_detach_circuit);
}

static void
test_rend_token_maps(void *arg)
{
  or_circuit_t *c1, *c2, *c3, *c4;
  const uint8_t tok1[REND_TOKEN_LEN] = "The cat can't tell y";
  const uint8_t tok2[REND_TOKEN_LEN] = "ou its name, and it ";
  const uint8_t tok3[REND_TOKEN_LEN] = "doesn't really care.";
  /* -- Adapted from a quote by Fredrik Lundh. */

  (void)arg;
  (void)tok1; //xxxx
  c1 = or_circuit_new(0, NULL);
  c2 = or_circuit_new(0, NULL);
  c3 = or_circuit_new(0, NULL);
  c4 = or_circuit_new(0, NULL);

  /* Make sure we really filled up the tok* variables */
  tt_int_op(tok1[REND_TOKEN_LEN-1], OP_EQ, 'y');
  tt_int_op(tok2[REND_TOKEN_LEN-1], OP_EQ, ' ');
  tt_int_op(tok3[REND_TOKEN_LEN-1], OP_EQ, '.');

  /* No maps; nothing there. */
  tt_ptr_op(NULL, OP_EQ, circuit_get_rendezvous(tok1));
  tt_ptr_op(NULL, OP_EQ, circuit_get_intro_point(tok1));

  circuit_set_rendezvous_cookie(c1, tok1);
  circuit_set_intro_point_digest(c2, tok2);

  tt_ptr_op(NULL, OP_EQ, circuit_get_rendezvous(tok3));
  tt_ptr_op(NULL, OP_EQ, circuit_get_intro_point(tok3));
  tt_ptr_op(NULL, OP_EQ, circuit_get_rendezvous(tok2));
  tt_ptr_op(NULL, OP_EQ, circuit_get_intro_point(tok1));

  /* Without purpose set, we don't get the circuits */
  tt_ptr_op(NULL, OP_EQ, circuit_get_rendezvous(tok1));
  tt_ptr_op(NULL, OP_EQ, circuit_get_intro_point(tok2));

  c1->base_.purpose = CIRCUIT_PURPOSE_REND_POINT_WAITING;
  c2->base_.purpose = CIRCUIT_PURPOSE_INTRO_POINT;

  /* Okay, make sure they show up now. */
  tt_ptr_op(c1, OP_EQ, circuit_get_rendezvous(tok1));
  tt_ptr_op(c2, OP_EQ, circuit_get_intro_point(tok2));

  /* Two items at the same place with the same token. */
  c3->base_.purpose = CIRCUIT_PURPOSE_REND_POINT_WAITING;
  circuit_set_rendezvous_cookie(c3, tok2);
  tt_ptr_op(c2, OP_EQ, circuit_get_intro_point(tok2));
  tt_ptr_op(c3, OP_EQ, circuit_get_rendezvous(tok2));

  /* Marking a circuit makes it not get returned any more */
  circuit_mark_for_close(TO_CIRCUIT(c1), END_CIRC_REASON_FINISHED);
  tt_ptr_op(NULL, OP_EQ, circuit_get_rendezvous(tok1));
  circuit_free(TO_CIRCUIT(c1));
  c1 = NULL;

  /* Freeing a circuit makes it not get returned any more. */
  circuit_free(TO_CIRCUIT(c2));
  c2 = NULL;
  tt_ptr_op(NULL, OP_EQ, circuit_get_intro_point(tok2));

  /* c3 -- are you still there? */
  tt_ptr_op(c3, OP_EQ, circuit_get_rendezvous(tok2));
  /* Change its cookie.  This never happens in Tor per se, but hey. */
  c3->base_.purpose = CIRCUIT_PURPOSE_INTRO_POINT;
  circuit_set_intro_point_digest(c3, tok3);

  tt_ptr_op(NULL, OP_EQ, circuit_get_rendezvous(tok2));
  tt_ptr_op(c3, OP_EQ, circuit_get_intro_point(tok3));

  /* Now replace c3 with c4. */
  c4->base_.purpose = CIRCUIT_PURPOSE_INTRO_POINT;
  circuit_set_intro_point_digest(c4, tok3);

  tt_ptr_op(c4, OP_EQ, circuit_get_intro_point(tok3));

  tt_ptr_op(c3->rendinfo, OP_EQ, NULL);
  tt_ptr_op(c4->rendinfo, OP_NE, NULL);
  tt_mem_op(c4->rendinfo, OP_EQ, tok3, REND_TOKEN_LEN);

  /* Now clear c4's cookie. */
  circuit_set_intro_point_digest(c4, NULL);
  tt_ptr_op(c4->rendinfo, OP_EQ, NULL);
  tt_ptr_op(NULL, OP_EQ, circuit_get_intro_point(tok3));

 done:
  if (c1)
    circuit_free(TO_CIRCUIT(c1));
  if (c2)
    circuit_free(TO_CIRCUIT(c2));
  if (c3)
    circuit_free(TO_CIRCUIT(c3));
  if (c4)
    circuit_free(TO_CIRCUIT(c4));
}

static void
test_pick_circid(void *arg)
{
  bitarray_t *ba = NULL;
  channel_t *chan1, *chan2;
  circid_t circid;
  int i;
  (void) arg;

  chan1 = tor_malloc_zero(sizeof(channel_t));
  chan2 = tor_malloc_zero(sizeof(channel_t));
  chan2->wide_circ_ids = 1;

  chan1->circ_id_type = CIRC_ID_TYPE_NEITHER;
  tt_int_op(0, OP_EQ, get_unique_circ_id_by_chan(chan1));

  /* Basic tests, with no collisions */
  chan1->circ_id_type = CIRC_ID_TYPE_LOWER;
  for (i = 0; i < 50; ++i) {
    circid = get_unique_circ_id_by_chan(chan1);
    tt_uint_op(0, OP_LT, circid);
    tt_uint_op(circid, OP_LT, (1<<15));
  }
  chan1->circ_id_type = CIRC_ID_TYPE_HIGHER;
  for (i = 0; i < 50; ++i) {
    circid = get_unique_circ_id_by_chan(chan1);
    tt_uint_op((1<<15), OP_LT, circid);
    tt_uint_op(circid, OP_LT, (1<<16));
  }

  chan2->circ_id_type = CIRC_ID_TYPE_LOWER;
  for (i = 0; i < 50; ++i) {
    circid = get_unique_circ_id_by_chan(chan2);
    tt_uint_op(0, OP_LT, circid);
    tt_uint_op(circid, OP_LT, (1u<<31));
  }
  chan2->circ_id_type = CIRC_ID_TYPE_HIGHER;
  for (i = 0; i < 50; ++i) {
    circid = get_unique_circ_id_by_chan(chan2);
    tt_uint_op((1u<<31), OP_LT, circid);
  }

  /* Now make sure that we can behave well when we are full up on circuits */
  chan1->circ_id_type = CIRC_ID_TYPE_LOWER;
  chan2->circ_id_type = CIRC_ID_TYPE_LOWER;
  chan1->wide_circ_ids = chan2->wide_circ_ids = 0;
  ba = bitarray_init_zero((1<<15));
  for (i = 0; i < (1<<15); ++i) {
    circid = get_unique_circ_id_by_chan(chan1);
    if (circid == 0) {
      tt_int_op(i, OP_GT, (1<<14));
      break;
    }
    tt_uint_op(circid, OP_LT, (1<<15));
    tt_assert(! bitarray_is_set(ba, circid));
    bitarray_set(ba, circid);
    channel_mark_circid_unusable(chan1, circid);
  }
  tt_int_op(i, OP_LT, (1<<15));
  /* Make sure that being full on chan1 does not interfere with chan2 */
  for (i = 0; i < 100; ++i) {
    circid = get_unique_circ_id_by_chan(chan2);
    tt_uint_op(circid, OP_GT, 0);
    tt_uint_op(circid, OP_LT, (1<<15));
    channel_mark_circid_unusable(chan2, circid);
  }

 done:
  tor_free(chan1);
  tor_free(chan2);
  bitarray_free(ba);
  circuit_free_all();
}

struct testcase_t circuitlist_tests[] = {
  { "maps", test_clist_maps, TT_FORK, NULL, NULL },
  { "rend_token_maps", test_rend_token_maps, TT_FORK, NULL, NULL },
  { "pick_circid", test_pick_circid, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

