/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rephist.h
 * \brief Header file for rephist.c.
 **/

#ifndef TOR_REPHIST_H
#define TOR_REPHIST_H

void rep_hist_init(void);
void rep_hist_note_connect_failed(const char* nickname, time_t when);
void rep_hist_note_connect_succeeded(const char* nickname, time_t when);
void rep_hist_note_disconnect(const char* nickname, time_t when);
void rep_hist_note_connection_died(const char* nickname, time_t when);
void rep_hist_note_extend_succeeded(const char *from_name,
                                    const char *to_name);
void rep_hist_note_extend_failed(const char *from_name, const char *to_name);
void rep_hist_dump_stats(time_t now, int severity);
void rep_hist_note_bytes_read(size_t num_bytes, time_t when);
void rep_hist_note_bytes_written(size_t num_bytes, time_t when);

void rep_hist_make_router_pessimal(const char *id, time_t when);

void rep_hist_note_dir_bytes_read(size_t num_bytes, time_t when);
void rep_hist_note_dir_bytes_written(size_t num_bytes, time_t when);

int rep_hist_bandwidth_assess(void);
char *rep_hist_get_bandwidth_lines(void);
void rep_hist_update_state(or_state_t *state);
int rep_hist_load_state(or_state_t *state, char **err);
void rep_history_clean(time_t before);

void rep_hist_note_router_reachable(const char *id, const tor_addr_t *at_addr,
                                    const uint16_t at_port, time_t when);
void rep_hist_note_router_unreachable(const char *id, time_t when);
int rep_hist_record_mtbf_data(time_t now, int missing_means_down);
int rep_hist_load_mtbf_data(time_t now);

time_t rep_hist_downrate_old_runs(time_t now);
long rep_hist_get_uptime(const char *id, time_t when);
double rep_hist_get_stability(const char *id, time_t when);
double rep_hist_get_weighted_fractional_uptime(const char *id, time_t when);
long rep_hist_get_weighted_time_known(const char *id, time_t when);
int rep_hist_have_measured_enough_stability(void);

void rep_hist_note_used_port(time_t now, uint16_t port);
smartlist_t *rep_hist_get_predicted_ports(time_t now);
void rep_hist_remove_predicted_ports(const smartlist_t *rmv_ports);
void rep_hist_note_used_resolve(time_t now);
void rep_hist_note_used_internal(time_t now, int need_uptime,
                                 int need_capacity);
int rep_hist_get_predicted_internal(time_t now, int *need_uptime,
                                    int *need_capacity);

int any_predicted_circuits(time_t now);
int rep_hist_circbuilding_dormant(time_t now);

void note_crypto_pk_op(pk_op_t operation);
void dump_pk_ops(int severity);

void rep_hist_exit_stats_init(time_t now);
void rep_hist_reset_exit_stats(time_t now);
void rep_hist_exit_stats_term(void);
char *rep_hist_format_exit_stats(time_t now);
time_t rep_hist_exit_stats_write(time_t now);
void rep_hist_note_exit_bytes(uint16_t port, size_t num_written,
                              size_t num_read);
void rep_hist_note_exit_stream_opened(uint16_t port);

void rep_hist_buffer_stats_init(time_t now);
void rep_hist_buffer_stats_add_circ(circuit_t *circ,
                                    time_t end_of_interval);
time_t rep_hist_buffer_stats_write(time_t now);
void rep_hist_buffer_stats_term(void);
void rep_hist_add_buffer_stats(double mean_num_cells_in_queue,
     double mean_time_cells_in_queue, uint32_t processed_cells);
char *rep_hist_format_buffer_stats(time_t now);
void rep_hist_reset_buffer_stats(time_t now);

void rep_hist_desc_stats_init(time_t now);
void rep_hist_note_desc_served(const char * desc);
void rep_hist_desc_stats_term(void);
time_t rep_hist_desc_stats_write(time_t now);

void rep_hist_conn_stats_init(time_t now);
void rep_hist_note_or_conn_bytes(uint64_t conn_id, size_t num_read,
                                 size_t num_written, time_t when);
void rep_hist_reset_conn_stats(time_t now);
char *rep_hist_format_conn_stats(time_t now);
time_t rep_hist_conn_stats_write(time_t now);
void rep_hist_conn_stats_term(void);

void rep_hist_note_circuit_handshake_requested(uint16_t type);
void rep_hist_note_circuit_handshake_assigned(uint16_t type);
void rep_hist_log_circuit_handshake_stats(time_t now);

void rep_hist_free_all(void);

#endif

