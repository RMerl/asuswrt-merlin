/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file control.h
 * \brief Header file for control.c.
 **/

#ifndef TOR_CONTROL_H
#define TOR_CONTROL_H

void control_update_global_event_mask(void);
void control_adjust_event_log_severity(void);

void control_ports_write_to_file(void);

/** Log information about the connection <b>conn</b>, protecting it as with
 * CONN_LOG_PROTECT. Example:
 *
 * LOG_FN_CONN(conn, (LOG_DEBUG, "Socket %d wants to write", conn->s));
 **/
#define LOG_FN_CONN(conn, args)                 \
  CONN_LOG_PROTECT(conn, log_fn args)

int connection_control_finished_flushing(control_connection_t *conn);
int connection_control_reached_eof(control_connection_t *conn);
void connection_control_closed(control_connection_t *conn);

int connection_control_process_inbuf(control_connection_t *conn);

#define EVENT_AUTHDIR_NEWDESCS 0x000D
#define EVENT_NS 0x000F
int control_event_is_interesting(int event);

int control_event_circuit_status(origin_circuit_t *circ,
                                 circuit_status_event_t e, int reason);
int control_event_circuit_purpose_changed(origin_circuit_t *circ,
                                          int old_purpose);
int control_event_circuit_cannibalized(origin_circuit_t *circ,
                                       int old_purpose,
                                       const struct timeval *old_tv_created);
int control_event_stream_status(entry_connection_t *conn,
                                stream_status_event_t e,
                                int reason);
int control_event_or_conn_status(or_connection_t *conn,
                                 or_conn_status_event_t e, int reason);
int control_event_bandwidth_used(uint32_t n_read, uint32_t n_written);
int control_event_stream_bandwidth(edge_connection_t *edge_conn);
int control_event_stream_bandwidth_used(void);
int control_event_circ_bandwidth_used(void);
int control_event_conn_bandwidth(connection_t *conn);
int control_event_conn_bandwidth_used(void);
int control_event_circuit_cell_stats(void);
int control_event_tb_empty(const char *bucket, uint32_t read_empty_time,
                           uint32_t write_empty_time,
                           int milliseconds_elapsed);
void control_event_logmsg(int severity, uint32_t domain, const char *msg);
int control_event_descriptors_changed(smartlist_t *routers);
int control_event_address_mapped(const char *from, const char *to,
                                 time_t expires, const char *error,
                                 const int cached);
int control_event_or_authdir_new_descriptor(const char *action,
                                            const char *desc,
                                            size_t desclen,
                                            const char *msg);
int control_event_my_descriptor_changed(void);
int control_event_networkstatus_changed(smartlist_t *statuses);

int control_event_newconsensus(const networkstatus_t *consensus);
int control_event_networkstatus_changed_single(const routerstatus_t *rs);
int control_event_general_status(int severity, const char *format, ...)
  CHECK_PRINTF(2,3);
int control_event_client_status(int severity, const char *format, ...)
  CHECK_PRINTF(2,3);
int control_event_server_status(int severity, const char *format, ...)
  CHECK_PRINTF(2,3);
int control_event_guard(const char *nickname, const char *digest,
                        const char *status);
int control_event_conf_changed(const smartlist_t *elements);
int control_event_buildtimeout_set(buildtimeout_set_event_t type,
                                   const char *args);
int control_event_signal(uintptr_t signal);

int init_control_cookie_authentication(int enabled);
char *get_controller_cookie_file_name(void);
smartlist_t *decode_hashed_passwords(config_line_t *passwords);
void disable_control_logging(void);
void enable_control_logging(void);

void monitor_owning_controller_process(const char *process_spec);

void control_event_bootstrap(bootstrap_status_t status, int progress);
MOCK_DECL(void, control_event_bootstrap_problem,(const char *warn,
                                                 int reason,
                                                 or_connection_t *or_conn));

void control_event_clients_seen(const char *controller_str);
void control_event_transport_launched(const char *mode,
                                      const char *transport_name,
                                      tor_addr_t *addr, uint16_t port);
const char *rend_auth_type_to_string(rend_auth_type_t auth_type);
MOCK_DECL(const char *, node_describe_longname_by_id,(const char *id_digest));
void control_event_hs_descriptor_requested(const rend_data_t *rend_query,
                                           const char *desc_id_base32,
                                           const char *hs_dir);
void control_event_hs_descriptor_receive_end(const char *action,
                                        const rend_data_t *rend_query,
                                        const char *hs_dir);
void control_event_hs_descriptor_received(const rend_data_t *rend_query,
                                          const char *hs_dir);
void control_event_hs_descriptor_failed(const rend_data_t *rend_query,
                                        const char *hs_dir);

void control_free_all(void);

#ifdef CONTROL_PRIVATE
/* Recognized asynchronous event types.  It's okay to expand this list
 * because it is used both as a list of v0 event types, and as indices
 * into the bitfield to determine which controllers want which events.
 */
#define EVENT_MIN_                    0x0001
#define EVENT_CIRCUIT_STATUS          0x0001
#define EVENT_STREAM_STATUS           0x0002
#define EVENT_OR_CONN_STATUS          0x0003
#define EVENT_BANDWIDTH_USED          0x0004
#define EVENT_CIRCUIT_STATUS_MINOR    0x0005
#define EVENT_NEW_DESC                0x0006
#define EVENT_DEBUG_MSG               0x0007
#define EVENT_INFO_MSG                0x0008
#define EVENT_NOTICE_MSG              0x0009
#define EVENT_WARN_MSG                0x000A
#define EVENT_ERR_MSG                 0x000B
#define EVENT_ADDRMAP                 0x000C
/* Exposed above */
// #define EVENT_AUTHDIR_NEWDESCS     0x000D
#define EVENT_DESCCHANGED             0x000E
/* Exposed above */
// #define EVENT_NS                   0x000F
#define EVENT_STATUS_CLIENT           0x0010
#define EVENT_STATUS_SERVER           0x0011
#define EVENT_STATUS_GENERAL          0x0012
#define EVENT_GUARD                   0x0013
#define EVENT_STREAM_BANDWIDTH_USED   0x0014
#define EVENT_CLIENTS_SEEN            0x0015
#define EVENT_NEWCONSENSUS            0x0016
#define EVENT_BUILDTIMEOUT_SET        0x0017
#define EVENT_SIGNAL                  0x0018
#define EVENT_CONF_CHANGED            0x0019
#define EVENT_CONN_BW                 0x001A
#define EVENT_CELL_STATS              0x001B
#define EVENT_TB_EMPTY                0x001C
#define EVENT_CIRC_BANDWIDTH_USED     0x001D
#define EVENT_TRANSPORT_LAUNCHED      0x0020
#define EVENT_HS_DESC                 0x0021
#define EVENT_MAX_                    0x0021
/* If EVENT_MAX_ ever hits 0x003F, we need to make the mask into a
 * different structure, as it can only handle a maximum left shift of 1<<63. */

/* Used only by control.c and test.c */
STATIC size_t write_escaped_data(const char *data, size_t len, char **out);
STATIC size_t read_escaped_data(const char *data, size_t len, char **out);
/** Flag for event_format_t.  Indicates that we should use the one standard
    format.  (Other formats previous existed, and are now deprecated)
 */
#define ALL_FORMATS 1
/** Bit field of flags to select how to format a controller event.  Recognized
 * flag is ALL_FORMATS. */
typedef int event_format_t;

#ifdef TOR_UNIT_TESTS
MOCK_DECL(STATIC void,
send_control_event_string,(uint16_t event, event_format_t which,
                           const char *msg));

void control_testing_set_global_event_mask(uint64_t mask);
#endif

/** Helper structure: temporarily stores cell statistics for a circuit. */
typedef struct cell_stats_t {
  /** Number of cells added in app-ward direction by command. */
  uint64_t added_cells_appward[CELL_COMMAND_MAX_ + 1];
  /** Number of cells added in exit-ward direction by command. */
  uint64_t added_cells_exitward[CELL_COMMAND_MAX_ + 1];
  /** Number of cells removed in app-ward direction by command. */
  uint64_t removed_cells_appward[CELL_COMMAND_MAX_ + 1];
  /** Number of cells removed in exit-ward direction by command. */
  uint64_t removed_cells_exitward[CELL_COMMAND_MAX_ + 1];
  /** Total waiting time of cells in app-ward direction by command. */
  uint64_t total_time_appward[CELL_COMMAND_MAX_ + 1];
  /** Total waiting time of cells in exit-ward direction by command. */
  uint64_t total_time_exitward[CELL_COMMAND_MAX_ + 1];
} cell_stats_t;
void sum_up_cell_stats_by_command(circuit_t *circ,
                                  cell_stats_t *cell_stats);
void append_cell_stats_by_command(smartlist_t *event_parts,
                                  const char *key,
                                  const uint64_t *include_if_non_zero,
                                  const uint64_t *number_to_include);
void format_cell_stats(char **event_string, circuit_t *circ,
                       cell_stats_t *cell_stats);
#endif

#endif

