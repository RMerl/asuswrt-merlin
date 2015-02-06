/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file connection_or.h
 * \brief Header file for connection_or.c.
 **/

#ifndef TOR_CONNECTION_OR_H
#define TOR_CONNECTION_OR_H

void connection_or_remove_from_identity_map(or_connection_t *conn);
void connection_or_clear_identity_map(void);
void clear_broken_connection_map(int disable);
or_connection_t *connection_or_get_for_extend(const char *digest,
                                              const tor_addr_t *target_addr,
                                              const char **msg_out,
                                              int *launch_out);
void connection_or_set_bad_connections(const char *digest, int force);

void connection_or_block_renegotiation(or_connection_t *conn);
int connection_or_reached_eof(or_connection_t *conn);
int connection_or_process_inbuf(or_connection_t *conn);
int connection_or_flushed_some(or_connection_t *conn);
int connection_or_finished_flushing(or_connection_t *conn);
int connection_or_finished_connecting(or_connection_t *conn);
void connection_or_about_to_close(or_connection_t *conn);
int connection_or_digest_is_known_relay(const char *id_digest);
void connection_or_update_token_buckets(smartlist_t *conns,
                                        const or_options_t *options);

void connection_or_connect_failed(or_connection_t *conn,
                                  int reason, const char *msg);
void connection_or_notify_error(or_connection_t *conn,
                                int reason, const char *msg);
or_connection_t *connection_or_connect(const tor_addr_t *addr, uint16_t port,
                                       const char *id_digest,
                                       channel_tls_t *chan);

void connection_or_close_normally(or_connection_t *orconn, int flush);
void connection_or_close_for_error(or_connection_t *orconn, int flush);

void connection_or_report_broken_states(int severity, int domain);

MOCK_DECL(int,connection_tls_start_handshake,(or_connection_t *conn,
                                              int receiving));
int connection_tls_continue_handshake(or_connection_t *conn);
void connection_or_set_canonical(or_connection_t *or_conn,
                                 int is_canonical);

int connection_init_or_handshake_state(or_connection_t *conn,
                                       int started_here);
void connection_or_init_conn_from_address(or_connection_t *conn,
                                          const tor_addr_t *addr,
                                          uint16_t port,
                                          const char *id_digest,
                                          int started_here);
int connection_or_client_learned_peer_id(or_connection_t *conn,
                                         const uint8_t *peer_id);
time_t connection_or_client_used(or_connection_t *conn);
int connection_or_get_num_circuits(or_connection_t *conn);
void or_handshake_state_free(or_handshake_state_t *state);
void or_handshake_state_record_cell(or_connection_t *conn,
                                    or_handshake_state_t *state,
                                    const cell_t *cell,
                                    int incoming);
void or_handshake_state_record_var_cell(or_connection_t *conn,
                                        or_handshake_state_t *state,
                                        const var_cell_t *cell,
                                        int incoming);

int connection_or_set_state_open(or_connection_t *conn);
void connection_or_write_cell_to_buf(const cell_t *cell,
                                     or_connection_t *conn);
void connection_or_write_var_cell_to_buf(const var_cell_t *cell,
                                         or_connection_t *conn);
int connection_or_send_versions(or_connection_t *conn, int v3_plus);
int connection_or_send_netinfo(or_connection_t *conn);
int connection_or_send_certs_cell(or_connection_t *conn);
int connection_or_send_auth_challenge_cell(or_connection_t *conn);
int connection_or_compute_authenticate_cell_body(or_connection_t *conn,
                                                 uint8_t *out, size_t outlen,
                                                 crypto_pk_t *signing_key,
                                                 int server);
int connection_or_send_authenticate_cell(or_connection_t *conn, int type);

int is_or_protocol_version_known(uint16_t version);

void cell_pack(packed_cell_t *dest, const cell_t *src, int wide_circ_ids);
int var_cell_pack_header(const var_cell_t *cell, char *hdr_out,
                         int wide_circ_ids);
var_cell_t *var_cell_new(uint16_t payload_len);
void var_cell_free(var_cell_t *cell);

/** DOCDOC */
#define MIN_LINK_PROTO_FOR_WIDE_CIRC_IDS 4

#endif

