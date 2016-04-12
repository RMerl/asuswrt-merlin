/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file relay.h
 * \brief Header file for relay.c.
 **/

#ifndef TOR_RELAY_H
#define TOR_RELAY_H

extern uint64_t stats_n_relay_cells_relayed;
extern uint64_t stats_n_relay_cells_delivered;

int circuit_receive_relay_cell(cell_t *cell, circuit_t *circ,
                               cell_direction_t cell_direction);

void relay_header_pack(uint8_t *dest, const relay_header_t *src);
void relay_header_unpack(relay_header_t *dest, const uint8_t *src);
int relay_send_command_from_edge_(streamid_t stream_id, circuit_t *circ,
                               uint8_t relay_command, const char *payload,
                               size_t payload_len, crypt_path_t *cpath_layer,
                               const char *filename, int lineno);
#define relay_send_command_from_edge(stream_id, circ, relay_command, payload, \
                                     payload_len, cpath_layer)          \
  relay_send_command_from_edge_((stream_id), (circ), (relay_command),   \
                                (payload), (payload_len), (cpath_layer), \
                                __FILE__, __LINE__)
int connection_edge_send_command(edge_connection_t *fromconn,
                                 uint8_t relay_command, const char *payload,
                                 size_t payload_len);
int connection_edge_package_raw_inbuf(edge_connection_t *conn,
                                      int package_partial,
                                      int *max_cells);
void connection_edge_consider_sending_sendme(edge_connection_t *conn);

extern uint64_t stats_n_data_cells_packaged;
extern uint64_t stats_n_data_bytes_packaged;
extern uint64_t stats_n_data_cells_received;
extern uint64_t stats_n_data_bytes_received;

void dump_cell_pool_usage(int severity);
size_t packed_cell_mem_cost(void);

int have_been_under_memory_pressure(void);

/* For channeltls.c */
void packed_cell_free(packed_cell_t *cell);

void cell_queue_init(cell_queue_t *queue);
void cell_queue_clear(cell_queue_t *queue);
void cell_queue_append(cell_queue_t *queue, packed_cell_t *cell);
void cell_queue_append_packed_copy(circuit_t *circ, cell_queue_t *queue,
                                   int exitward, const cell_t *cell,
                                   int wide_circ_ids, int use_stats);

void append_cell_to_circuit_queue(circuit_t *circ, channel_t *chan,
                                  cell_t *cell, cell_direction_t direction,
                                  streamid_t fromstream);
void channel_unlink_all_circuits(channel_t *chan, smartlist_t *detached_out);
MOCK_DECL(int, channel_flush_from_first_active_circuit,
          (channel_t *chan, int max));
void assert_circuit_mux_okay(channel_t *chan);
void update_circuit_on_cmux_(circuit_t *circ, cell_direction_t direction,
                             const char *file, int lineno);
#define update_circuit_on_cmux(circ, direction) \
  update_circuit_on_cmux_((circ), (direction), SHORT_FILE__, __LINE__)

int append_address_to_payload(uint8_t *payload_out, const tor_addr_t *addr);
const uint8_t *decode_address_from_payload(tor_addr_t *addr_out,
                                        const uint8_t *payload,
                                        int payload_len);
void circuit_clear_cell_queue(circuit_t *circ, channel_t *chan);

void stream_choice_seed_weak_rng(void);

int relay_crypt(circuit_t *circ, cell_t *cell, cell_direction_t cell_direction,
                crypt_path_t **layer_hint, char *recognized);

circid_t packed_cell_get_circid(const packed_cell_t *cell, int wide_circ_ids);

#ifdef RELAY_PRIVATE
STATIC int connected_cell_parse(const relay_header_t *rh, const cell_t *cell,
                         tor_addr_t *addr_out, int *ttl_out);
/** An address-and-ttl tuple as yielded by resolved_cell_parse */
typedef struct address_ttl_s {
  tor_addr_t addr;
  char *hostname;
  int ttl;
} address_ttl_t;
STATIC void address_ttl_free(address_ttl_t *addr);
STATIC int resolved_cell_parse(const cell_t *cell, const relay_header_t *rh,
                               smartlist_t *addresses_out, int *errcode_out);
STATIC int connection_edge_process_resolved_cell(edge_connection_t *conn,
                                                 const cell_t *cell,
                                                 const relay_header_t *rh);
STATIC packed_cell_t *packed_cell_new(void);
STATIC packed_cell_t *cell_queue_pop(cell_queue_t *queue);
STATIC size_t cell_queues_get_total_allocation(void);
STATIC int cell_queues_check_size(void);
#endif

#endif

