/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file connection_edge.h
 * \brief Header file for connection_edge.c.
 **/

#ifndef TOR_CONNECTION_EDGE_H
#define TOR_CONNECTION_EDGE_H

#include "testsupport.h"

#define connection_mark_unattached_ap(conn, endreason) \
  connection_mark_unattached_ap_((conn), (endreason), __LINE__, SHORT_FILE__)

MOCK_DECL(void,connection_mark_unattached_ap_,
          (entry_connection_t *conn, int endreason,
           int line, const char *file));
int connection_edge_reached_eof(edge_connection_t *conn);
int connection_edge_process_inbuf(edge_connection_t *conn,
                                  int package_partial);
int connection_edge_destroy(circid_t circ_id, edge_connection_t *conn);
int connection_edge_end(edge_connection_t *conn, uint8_t reason);
int connection_edge_end_errno(edge_connection_t *conn);
int connection_edge_flushed_some(edge_connection_t *conn);
int connection_edge_finished_flushing(edge_connection_t *conn);
int connection_edge_finished_connecting(edge_connection_t *conn);

void connection_ap_about_to_close(entry_connection_t *edge_conn);
void connection_exit_about_to_close(edge_connection_t *edge_conn);

int connection_ap_handshake_send_begin(entry_connection_t *ap_conn);
int connection_ap_handshake_send_resolve(entry_connection_t *ap_conn);

entry_connection_t  *connection_ap_make_link(connection_t *partner,
                                            char *address, uint16_t port,
                                            const char *digest,
                                            int session_group,
                                            int isolation_flags,
                                            int use_begindir, int want_onehop);
void connection_ap_handshake_socks_reply(entry_connection_t *conn, char *reply,
                                         size_t replylen,
                                         int endreason);
MOCK_DECL(void,connection_ap_handshake_socks_resolved,
          (entry_connection_t *conn,
           int answer_type,
           size_t answer_len,
           const uint8_t *answer,
           int ttl,
           time_t expires));
void connection_ap_handshake_socks_resolved_addr(entry_connection_t *conn,
                                                 const tor_addr_t *answer,
                                                 int ttl,
                                                 time_t expires);

int connection_exit_begin_conn(cell_t *cell, circuit_t *circ);
int connection_exit_begin_resolve(cell_t *cell, or_circuit_t *circ);
void connection_exit_connect(edge_connection_t *conn);
int connection_edge_is_rendezvous_stream(edge_connection_t *conn);
int connection_ap_can_use_exit(const entry_connection_t *conn,
                               const node_t *exit);
void connection_ap_expire_beginning(void);
void connection_ap_attach_pending(void);
void connection_ap_fail_onehop(const char *failed_digest,
                               cpath_build_state_t *build_state);
void circuit_discard_optional_exit_enclaves(extend_info_t *info);
int connection_ap_detach_retriable(entry_connection_t *conn,
                                   origin_circuit_t *circ,
                                   int reason);
int connection_ap_process_transparent(entry_connection_t *conn);

int address_is_invalid_destination(const char *address, int client);

int connection_ap_rewrite_and_attach_if_allowed(entry_connection_t *conn,
                                                origin_circuit_t *circ,
                                                crypt_path_t *cpath);
int connection_ap_handshake_rewrite_and_attach(entry_connection_t *conn,
                                               origin_circuit_t *circ,
                                               crypt_path_t *cpath);

/** Possible return values for parse_extended_hostname. */
typedef enum hostname_type_t {
  NORMAL_HOSTNAME, ONION_HOSTNAME, EXIT_HOSTNAME, BAD_HOSTNAME
} hostname_type_t;
hostname_type_t parse_extended_hostname(char *address);

#if defined(HAVE_NET_IF_H) && defined(HAVE_NET_PFVAR_H)
int get_pf_socket(void);
#endif

int connection_edge_compatible_with_circuit(const entry_connection_t *conn,
                                            const origin_circuit_t *circ);
int connection_edge_update_circuit_isolation(const entry_connection_t *conn,
                                             origin_circuit_t *circ,
                                             int dry_run);
void circuit_clear_isolation(origin_circuit_t *circ);
streamid_t get_unique_stream_id_by_circ(origin_circuit_t *circ);

/** @name Begin-cell flags
 *
 * These flags are used in RELAY_BEGIN cells to change the default behavior
 * of the cell.
 *
 * @{
 **/
/** When this flag is set, the client is willing to get connected to IPv6
 * addresses */
#define BEGIN_FLAG_IPV6_OK        (1u<<0)
/** When this flag is set, the client DOES NOT support connecting to IPv4
 * addresses.  (The sense of this flag is inverted from IPV6_OK, so that the
 * old default behavior of Tor is equivalent to having all flags set to 0.)
 **/
#define BEGIN_FLAG_IPV4_NOT_OK    (1u<<1)
/** When this flag is set, if we find both an IPv4 and an IPv6 address,
 * we use the IPv6 address.  Otherwise we use the IPv4 address. */
#define BEGIN_FLAG_IPV6_PREFERRED (1u<<2)
/**@}*/

#ifdef CONNECTION_EDGE_PRIVATE

/** A parsed BEGIN or BEGIN_DIR cell */
typedef struct begin_cell_t {
  /** The address the client has asked us to connect to, or NULL if this is
   * a BEGIN_DIR cell*/
  char *address;
  /** The flags specified in the BEGIN cell's body.  One or more of
   * BEGIN_FLAG_*. */
  uint32_t flags;
  /** The client's requested port. */
  uint16_t port;
  /** The client's requested Stream ID */
  uint16_t stream_id;
  /** True iff this is a BEGIN_DIR cell. */
  unsigned is_begindir : 1;
} begin_cell_t;

STATIC int begin_cell_parse(const cell_t *cell, begin_cell_t *bcell,
                     uint8_t *end_reason_out);
STATIC int connected_cell_format_payload(uint8_t *payload_out,
                                  const tor_addr_t *addr,
                                  uint32_t ttl);
#endif

#endif

