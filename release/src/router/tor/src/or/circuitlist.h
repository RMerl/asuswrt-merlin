/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file circuitlist.h
 * \brief Header file for circuitlist.c.
 **/

#ifndef TOR_CIRCUITLIST_H
#define TOR_CIRCUITLIST_H

#include "testsupport.h"

MOCK_DECL(smartlist_t *, circuit_get_global_list, (void));
const char *circuit_state_to_string(int state);
const char *circuit_purpose_to_controller_string(uint8_t purpose);
const char *circuit_purpose_to_controller_hs_state_string(uint8_t purpose);
const char *circuit_purpose_to_string(uint8_t purpose);
void circuit_dump_by_conn(connection_t *conn, int severity);
void circuit_set_p_circid_chan(or_circuit_t *circ, circid_t id,
                               channel_t *chan);
void circuit_set_n_circid_chan(circuit_t *circ, circid_t id,
                               channel_t *chan);
void channel_mark_circid_unusable(channel_t *chan, circid_t id);
void channel_mark_circid_usable(channel_t *chan, circid_t id);
time_t circuit_id_when_marked_unusable_on_channel(circid_t circ_id,
                                                  channel_t *chan);
void circuit_set_state(circuit_t *circ, uint8_t state);
void circuit_close_all_marked(void);
int32_t circuit_initial_package_window(void);
origin_circuit_t *origin_circuit_new(void);
or_circuit_t *or_circuit_new(circid_t p_circ_id, channel_t *p_chan);
circuit_t *circuit_get_by_circid_channel(circid_t circ_id,
                                         channel_t *chan);
circuit_t *
circuit_get_by_circid_channel_even_if_marked(circid_t circ_id,
                                             channel_t *chan);
int circuit_id_in_use_on_channel(circid_t circ_id, channel_t *chan);
circuit_t *circuit_get_by_edge_conn(edge_connection_t *conn);
void circuit_unlink_all_from_channel(channel_t *chan, int reason);
origin_circuit_t *circuit_get_by_global_id(uint32_t id);
origin_circuit_t *circuit_get_ready_rend_circ_by_rend_data(
  const rend_data_t *rend_data);
origin_circuit_t *circuit_get_next_by_pk_and_purpose(origin_circuit_t *start,
                                         const char *digest, uint8_t purpose);
or_circuit_t *circuit_get_rendezvous(const uint8_t *cookie);
or_circuit_t *circuit_get_intro_point(const uint8_t *digest);
void circuit_set_rendezvous_cookie(or_circuit_t *circ, const uint8_t *cookie);
void circuit_set_intro_point_digest(or_circuit_t *circ, const uint8_t *digest);
origin_circuit_t *circuit_find_to_cannibalize(uint8_t purpose,
                                              extend_info_t *info, int flags);
void circuit_mark_all_unused_circs(void);
void circuit_mark_all_dirty_circs_as_unusable(void);
MOCK_DECL(void, circuit_mark_for_close_, (circuit_t *circ, int reason,
                                          int line, const char *file));
int circuit_get_cpath_len(origin_circuit_t *circ);
void circuit_clear_cpath(origin_circuit_t *circ);
crypt_path_t *circuit_get_cpath_hop(origin_circuit_t *circ, int hopnum);
void circuit_get_all_pending_on_channel(smartlist_t *out,
                                        channel_t *chan);
int circuit_count_pending_on_channel(channel_t *chan);

#define circuit_mark_for_close(c, reason)                               \
  circuit_mark_for_close_((c), (reason), __LINE__, SHORT_FILE__)

void assert_cpath_layer_ok(const crypt_path_t *cp);
void assert_circuit_ok(const circuit_t *c);
void circuit_free_all(void);
void circuits_handle_oom(size_t current_allocation);

void channel_note_destroy_pending(channel_t *chan, circid_t id);
MOCK_DECL(void, channel_note_destroy_not_pending,
          (channel_t *chan, circid_t id));

#ifdef CIRCUITLIST_PRIVATE
STATIC void circuit_free(circuit_t *circ);
STATIC size_t n_cells_in_circ_queues(const circuit_t *c);
STATIC uint32_t circuit_max_queued_data_age(const circuit_t *c, uint32_t now);
STATIC uint32_t circuit_max_queued_cell_age(const circuit_t *c, uint32_t now);
STATIC uint32_t circuit_max_queued_item_age(const circuit_t *c, uint32_t now);
#endif

#endif

