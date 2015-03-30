/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file circuituse.h
 * \brief Header file for circuituse.c.
 **/

#ifndef TOR_CIRCUITUSE_H
#define TOR_CIRCUITUSE_H

void circuit_expire_building(void);
void circuit_remove_handled_ports(smartlist_t *needed_ports);
int circuit_stream_is_being_handled(entry_connection_t *conn, uint16_t port,
                                    int min);
void circuit_log_ancient_one_hop_circuits(int age);
#if 0
int circuit_conforms_to_options(const origin_circuit_t *circ,
                                const or_options_t *options);
#endif
void circuit_build_needed_circs(time_t now);
void circuit_expire_old_circs_as_needed(time_t now);
void circuit_detach_stream(circuit_t *circ, edge_connection_t *conn);

void circuit_expire_old_circuits_serverside(time_t now);

void reset_bandwidth_test(void);
int circuit_enough_testing_circs(void);

void circuit_has_opened(origin_circuit_t *circ);
void circuit_try_attaching_streams(origin_circuit_t *circ);
void circuit_build_failed(origin_circuit_t *circ);

/** Flag to set when a circuit should have only a single hop. */
#define CIRCLAUNCH_ONEHOP_TUNNEL  (1<<0)
/** Flag to set when a circuit needs to be built of high-uptime nodes */
#define CIRCLAUNCH_NEED_UPTIME    (1<<1)
/** Flag to set when a circuit needs to be built of high-capacity nodes */
#define CIRCLAUNCH_NEED_CAPACITY  (1<<2)
/** Flag to set when the last hop of a circuit doesn't need to be an
 * exit node. */
#define CIRCLAUNCH_IS_INTERNAL    (1<<3)
origin_circuit_t *circuit_launch_by_extend_info(uint8_t purpose,
                                                extend_info_t *info,
                                                int flags);
origin_circuit_t *circuit_launch(uint8_t purpose, int flags);
void circuit_reset_failure_count(int timeout);
int connection_ap_handshake_attach_chosen_circuit(entry_connection_t *conn,
                                                  origin_circuit_t *circ,
                                                  crypt_path_t *cpath);
int connection_ap_handshake_attach_circuit(entry_connection_t *conn);

void circuit_change_purpose(circuit_t *circ, uint8_t new_purpose);

int hostname_in_track_host_exits(const or_options_t *options,
                                 const char *address);
void mark_circuit_unusable_for_new_conns(origin_circuit_t *circ);

#endif

