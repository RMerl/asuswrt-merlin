/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file connection_or.c
 * \brief Functions to handle OR connections, TLS handshaking, and
 * cells on the network.
 **/
#include "or.h"
#include "buffers.h"
/*
 * Define this so we get channel internal functions, since we're implementing
 * part of a subclass (channel_tls_t).
 */
#define TOR_CHANNEL_INTERNAL_
#include "channel.h"
#include "channeltls.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuitstats.h"
#include "command.h"
#include "config.h"
#include "connection.h"
#include "connection_or.h"
#include "control.h"
#include "dirserv.h"
#include "entrynodes.h"
#include "geoip.h"
#include "main.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "reasons.h"
#include "relay.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"
#include "ext_orport.h"
#ifdef USE_BUFFEREVENTS
#include <event2/bufferevent_ssl.h>
#endif

static int connection_tls_finish_handshake(or_connection_t *conn);
static int connection_or_launch_v3_or_handshake(or_connection_t *conn);
static int connection_or_process_cells_from_inbuf(or_connection_t *conn);
static int connection_or_check_valid_tls_handshake(or_connection_t *conn,
                                                   int started_here,
                                                   char *digest_rcvd_out);

static void connection_or_tls_renegotiated_cb(tor_tls_t *tls, void *_conn);

static unsigned int
connection_or_is_bad_for_new_circs(or_connection_t *or_conn);
static void connection_or_mark_bad_for_new_circs(or_connection_t *or_conn);

/*
 * Call this when changing connection state, so notifications to the owning
 * channel can be handled.
 */

static void connection_or_change_state(or_connection_t *conn, uint8_t state);

#ifdef USE_BUFFEREVENTS
static void connection_or_handle_event_cb(struct bufferevent *bufev,
                                          short event, void *arg);
#include <event2/buffer.h>/*XXXX REMOVE */
#endif

/**************************************************************/

/** Map from identity digest of connected OR or desired OR to a connection_t
 * with that identity digest.  If there is more than one such connection_t,
 * they form a linked list, with next_with_same_id as the next pointer. */
static digestmap_t *orconn_identity_map = NULL;

/** Global map between Extended ORPort identifiers and OR
 *  connections. */
static digestmap_t *orconn_ext_or_id_map = NULL;

/** If conn is listed in orconn_identity_map, remove it, and clear
 * conn->identity_digest.  Otherwise do nothing. */
void
connection_or_remove_from_identity_map(or_connection_t *conn)
{
  or_connection_t *tmp;
  tor_assert(conn);
  if (!orconn_identity_map)
    return;
  tmp = digestmap_get(orconn_identity_map, conn->identity_digest);
  if (!tmp) {
    if (!tor_digest_is_zero(conn->identity_digest)) {
      log_warn(LD_BUG, "Didn't find connection '%s' on identity map when "
               "trying to remove it.",
               conn->nickname ? conn->nickname : "NULL");
    }
    return;
  }
  if (conn == tmp) {
    if (conn->next_with_same_id)
      digestmap_set(orconn_identity_map, conn->identity_digest,
                    conn->next_with_same_id);
    else
      digestmap_remove(orconn_identity_map, conn->identity_digest);
  } else {
    while (tmp->next_with_same_id) {
      if (tmp->next_with_same_id == conn) {
        tmp->next_with_same_id = conn->next_with_same_id;
        break;
      }
      tmp = tmp->next_with_same_id;
    }
  }
  memset(conn->identity_digest, 0, DIGEST_LEN);
  conn->next_with_same_id = NULL;
}

/** Remove all entries from the identity-to-orconn map, and clear
 * all identities in OR conns.*/
void
connection_or_clear_identity_map(void)
{
  smartlist_t *conns = get_connection_array();
  SMARTLIST_FOREACH(conns, connection_t *, conn,
  {
    if (conn->type == CONN_TYPE_OR) {
      or_connection_t *or_conn = TO_OR_CONN(conn);
      memset(or_conn->identity_digest, 0, DIGEST_LEN);
      or_conn->next_with_same_id = NULL;
    }
  });

  digestmap_free(orconn_identity_map, NULL);
  orconn_identity_map = NULL;
}

/** Change conn->identity_digest to digest, and add conn into
 * orconn_digest_map. */
static void
connection_or_set_identity_digest(or_connection_t *conn, const char *digest)
{
  or_connection_t *tmp;
  tor_assert(conn);
  tor_assert(digest);

  if (!orconn_identity_map)
    orconn_identity_map = digestmap_new();
  if (tor_memeq(conn->identity_digest, digest, DIGEST_LEN))
    return;

  /* If the identity was set previously, remove the old mapping. */
  if (! tor_digest_is_zero(conn->identity_digest)) {
    connection_or_remove_from_identity_map(conn);
    if (conn->chan)
      channel_clear_identity_digest(TLS_CHAN_TO_BASE(conn->chan));
  }

  memcpy(conn->identity_digest, digest, DIGEST_LEN);

  /* If we're setting the ID to zero, don't add a mapping. */
  if (tor_digest_is_zero(digest))
    return;

  tmp = digestmap_set(orconn_identity_map, digest, conn);
  conn->next_with_same_id = tmp;

  /* Deal with channels */
  if (conn->chan)
    channel_set_identity_digest(TLS_CHAN_TO_BASE(conn->chan), digest);

#if 1
  /* Testing code to check for bugs in representation. */
  for (; tmp; tmp = tmp->next_with_same_id) {
    tor_assert(tor_memeq(tmp->identity_digest, digest, DIGEST_LEN));
    tor_assert(tmp != conn);
  }
#endif
}

/** Remove the Extended ORPort identifier of <b>conn</b> from the
 *  global identifier list. Also, clear the identifier from the
 *  connection itself. */
void
connection_or_remove_from_ext_or_id_map(or_connection_t *conn)
{
  or_connection_t *tmp;
  if (!orconn_ext_or_id_map)
    return;
  if (!conn->ext_or_conn_id)
    return;

  tmp = digestmap_remove(orconn_ext_or_id_map, conn->ext_or_conn_id);
  if (!tor_digest_is_zero(conn->ext_or_conn_id))
    tor_assert(tmp == conn);

  memset(conn->ext_or_conn_id, 0, EXT_OR_CONN_ID_LEN);
}

/** Return the connection whose ext_or_id is <b>id</b>. Return NULL if no such
 * connection is found. */
or_connection_t *
connection_or_get_by_ext_or_id(const char *id)
{
  if (!orconn_ext_or_id_map)
    return NULL;
  return digestmap_get(orconn_ext_or_id_map, id);
}

/** Deallocate the global Extended ORPort identifier list */
void
connection_or_clear_ext_or_id_map(void)
{
  digestmap_free(orconn_ext_or_id_map, NULL);
  orconn_ext_or_id_map = NULL;
}

/** Creates an Extended ORPort identifier for <b>conn</b> and deposits
 *  it into the global list of identifiers. */
void
connection_or_set_ext_or_identifier(or_connection_t *conn)
{
  char random_id[EXT_OR_CONN_ID_LEN];
  or_connection_t *tmp;

  if (!orconn_ext_or_id_map)
    orconn_ext_or_id_map = digestmap_new();

  /* Remove any previous identifiers: */
  if (conn->ext_or_conn_id && !tor_digest_is_zero(conn->ext_or_conn_id))
      connection_or_remove_from_ext_or_id_map(conn);

  do {
    crypto_rand(random_id, sizeof(random_id));
  } while (digestmap_get(orconn_ext_or_id_map, random_id));

  if (!conn->ext_or_conn_id)
    conn->ext_or_conn_id = tor_malloc_zero(EXT_OR_CONN_ID_LEN);

  memcpy(conn->ext_or_conn_id, random_id, EXT_OR_CONN_ID_LEN);

  tmp = digestmap_set(orconn_ext_or_id_map, random_id, conn);
  tor_assert(!tmp);
}

/**************************************************************/

/** Map from a string describing what a non-open OR connection was doing when
 * failed, to an intptr_t describing the count of connections that failed that
 * way.  Note that the count is stored _as_ the pointer.
 */
static strmap_t *broken_connection_counts;

/** If true, do not record information in <b>broken_connection_counts</b>. */
static int disable_broken_connection_counts = 0;

/** Record that an OR connection failed in <b>state</b>. */
static void
note_broken_connection(const char *state)
{
  void *ptr;
  intptr_t val;
  if (disable_broken_connection_counts)
    return;

  if (!broken_connection_counts)
    broken_connection_counts = strmap_new();

  ptr = strmap_get(broken_connection_counts, state);
  val = (intptr_t)ptr;
  val++;
  ptr = (void*)val;
  strmap_set(broken_connection_counts, state, ptr);
}

/** Forget all recorded states for failed connections.  If
 * <b>stop_recording</b> is true, don't record any more. */
void
clear_broken_connection_map(int stop_recording)
{
  if (broken_connection_counts)
    strmap_free(broken_connection_counts, NULL);
  broken_connection_counts = NULL;
  if (stop_recording)
    disable_broken_connection_counts = 1;
}

/** Write a detailed description the state of <b>orconn</b> into the
 * <b>buflen</b>-byte buffer at <b>buf</b>.  This description includes not
 * only the OR-conn level state but also the TLS state.  It's useful for
 * diagnosing broken handshakes. */
static void
connection_or_get_state_description(or_connection_t *orconn,
                                    char *buf, size_t buflen)
{
  connection_t *conn = TO_CONN(orconn);
  const char *conn_state;
  char tls_state[256];

  tor_assert(conn->type == CONN_TYPE_OR || conn->type == CONN_TYPE_EXT_OR);

  conn_state = conn_state_to_string(conn->type, conn->state);
  tor_tls_get_state_description(orconn->tls, tls_state, sizeof(tls_state));

  tor_snprintf(buf, buflen, "%s with SSL state %s", conn_state, tls_state);
}

/** Record the current state of <b>orconn</b> as the state of a broken
 * connection. */
static void
connection_or_note_state_when_broken(or_connection_t *orconn)
{
  char buf[256];
  if (disable_broken_connection_counts)
    return;
  connection_or_get_state_description(orconn, buf, sizeof(buf));
  log_info(LD_HANDSHAKE,"Connection died in state '%s'", buf);
  note_broken_connection(buf);
}

/** Helper type used to sort connection states and find the most frequent. */
typedef struct broken_state_count_t {
  intptr_t count;
  const char *state;
} broken_state_count_t;

/** Helper function used to sort broken_state_count_t by frequency. */
static int
broken_state_count_compare(const void **a_ptr, const void **b_ptr)
{
  const broken_state_count_t *a = *a_ptr, *b = *b_ptr;
  if (b->count < a->count)
    return -1;
  else if (b->count == a->count)
    return 0;
  else
    return 1;
}

/** Upper limit on the number of different states to report for connection
 * failure. */
#define MAX_REASONS_TO_REPORT 10

/** Report a list of the top states for failed OR connections at log level
 * <b>severity</b>, in log domain <b>domain</b>. */
void
connection_or_report_broken_states(int severity, int domain)
{
  int total = 0;
  smartlist_t *items;

  if (!broken_connection_counts || disable_broken_connection_counts)
    return;

  items = smartlist_new();
  STRMAP_FOREACH(broken_connection_counts, state, void *, countptr) {
    broken_state_count_t *c = tor_malloc(sizeof(broken_state_count_t));
    c->count = (intptr_t)countptr;
    total += (int)c->count;
    c->state = state;
    smartlist_add(items, c);
  } STRMAP_FOREACH_END;

  smartlist_sort(items, broken_state_count_compare);

  tor_log(severity, domain, "%d connections have failed%s", total,
      smartlist_len(items) > MAX_REASONS_TO_REPORT ? ". Top reasons:" : ":");

  SMARTLIST_FOREACH_BEGIN(items, const broken_state_count_t *, c) {
    if (c_sl_idx > MAX_REASONS_TO_REPORT)
      break;
    tor_log(severity, domain,
        " %d connections died in state %s", (int)c->count, c->state);
  } SMARTLIST_FOREACH_END(c);

  SMARTLIST_FOREACH(items, broken_state_count_t *, c, tor_free(c));
  smartlist_free(items);
}

/** Call this to change or_connection_t states, so the owning channel_tls_t can
 * be notified.
 */

static void
connection_or_change_state(or_connection_t *conn, uint8_t state)
{
  uint8_t old_state;

  tor_assert(conn);

  old_state = conn->base_.state;
  conn->base_.state = state;

  if (conn->chan)
    channel_tls_handle_state_change_on_orconn(conn->chan, conn,
                                              old_state, state);
}

/** Return the number of circuits using an or_connection_t; this used to
 * be an or_connection_t field, but it got moved to channel_t and we
 * shouldn't maintain two copies. */

int
connection_or_get_num_circuits(or_connection_t *conn)
{
  tor_assert(conn);

  if (conn->chan) {
    return channel_num_circuits(TLS_CHAN_TO_BASE(conn->chan));
  } else return 0;
}

/**************************************************************/

/** Pack the cell_t host-order structure <b>src</b> into network-order
 * in the buffer <b>dest</b>. See tor-spec.txt for details about the
 * wire format.
 *
 * Note that this function doesn't touch <b>dst</b>-\>next: the caller
 * should set it or clear it as appropriate.
 */
void
cell_pack(packed_cell_t *dst, const cell_t *src, int wide_circ_ids)
{
  char *dest = dst->body;
  if (wide_circ_ids) {
    set_uint32(dest, htonl(src->circ_id));
    dest += 4;
  } else {
    set_uint16(dest, htons(src->circ_id));
    dest += 2;
    memset(dest+CELL_MAX_NETWORK_SIZE-2, 0, 2); /*make sure it's clear */
  }
  set_uint8(dest, src->command);
  memcpy(dest+1, src->payload, CELL_PAYLOAD_SIZE);
}

/** Unpack the network-order buffer <b>src</b> into a host-order
 * cell_t structure <b>dest</b>.
 */
static void
cell_unpack(cell_t *dest, const char *src, int wide_circ_ids)
{
  if (wide_circ_ids) {
    dest->circ_id = ntohl(get_uint32(src));
    src += 4;
  } else {
    dest->circ_id = ntohs(get_uint16(src));
    src += 2;
  }
  dest->command = get_uint8(src);
  memcpy(dest->payload, src+1, CELL_PAYLOAD_SIZE);
}

/** Write the header of <b>cell</b> into the first VAR_CELL_MAX_HEADER_SIZE
 * bytes of <b>hdr_out</b>. Returns number of bytes used. */
int
var_cell_pack_header(const var_cell_t *cell, char *hdr_out, int wide_circ_ids)
{
  int r;
  if (wide_circ_ids) {
    set_uint32(hdr_out, htonl(cell->circ_id));
    hdr_out += 4;
    r = VAR_CELL_MAX_HEADER_SIZE;
  } else {
    set_uint16(hdr_out, htons(cell->circ_id));
    hdr_out += 2;
    r = VAR_CELL_MAX_HEADER_SIZE - 2;
  }
  set_uint8(hdr_out, cell->command);
  set_uint16(hdr_out+1, htons(cell->payload_len));
  return r;
}

/** Allocate and return a new var_cell_t with <b>payload_len</b> bytes of
 * payload space. */
var_cell_t *
var_cell_new(uint16_t payload_len)
{
  size_t size = STRUCT_OFFSET(var_cell_t, payload) + payload_len;
  var_cell_t *cell = tor_malloc_zero(size);
  cell->payload_len = payload_len;
  cell->command = 0;
  cell->circ_id = 0;
  return cell;
}

/** Release all space held by <b>cell</b>. */
void
var_cell_free(var_cell_t *cell)
{
  tor_free(cell);
}

/** We've received an EOF from <b>conn</b>. Mark it for close and return. */
int
connection_or_reached_eof(or_connection_t *conn)
{
  tor_assert(conn);

  log_info(LD_OR,"OR connection reached EOF. Closing.");
  connection_or_close_normally(conn, 1);

  return 0;
}

/** Handle any new bytes that have come in on connection <b>conn</b>.
 * If conn is in 'open' state, hand it to
 * connection_or_process_cells_from_inbuf()
 * (else do nothing).
 */
int
connection_or_process_inbuf(or_connection_t *conn)
{
  /** Don't let the inbuf of a nonopen OR connection grow beyond this many
   * bytes: it's either a broken client, a non-Tor client, or a DOS
   * attempt. */
#define MAX_OR_INBUF_WHEN_NONOPEN 0

  int ret = 0;
  tor_assert(conn);

  switch (conn->base_.state) {
    case OR_CONN_STATE_PROXY_HANDSHAKING:
      ret = connection_read_proxy_handshake(TO_CONN(conn));

      /* start TLS after handshake completion, or deal with error */
      if (ret == 1) {
        tor_assert(TO_CONN(conn)->proxy_state == PROXY_CONNECTED);
        if (connection_tls_start_handshake(conn, 0) < 0)
          ret = -1;
        /* Touch the channel's active timestamp if there is one */
        if (conn->chan)
          channel_timestamp_active(TLS_CHAN_TO_BASE(conn->chan));
      }
      if (ret < 0) {
        connection_or_close_for_error(conn, 0);
      }

      return ret;
    case OR_CONN_STATE_TLS_SERVER_RENEGOTIATING:
#ifdef USE_BUFFEREVENTS
      if (tor_tls_server_got_renegotiate(conn->tls))
        connection_or_tls_renegotiated_cb(conn->tls, conn);
      if (conn->base_.marked_for_close)
        return 0;
      /* fall through. */
#endif
    case OR_CONN_STATE_OPEN:
    case OR_CONN_STATE_OR_HANDSHAKING_V2:
    case OR_CONN_STATE_OR_HANDSHAKING_V3:
      return connection_or_process_cells_from_inbuf(conn);
    default:
      break; /* don't do anything */
  }

  /* This check was necessary with 0.2.2, when the TLS_SERVER_RENEGOTIATING
   * check would otherwise just let data accumulate.  It serves no purpose
   * in 0.2.3.
   *
   * XXX024 Remove this check once we verify that the above paragraph is
   * 100% true. */
  if (buf_datalen(conn->base_.inbuf) > MAX_OR_INBUF_WHEN_NONOPEN) {
    log_fn(LOG_PROTOCOL_WARN, LD_NET, "Accumulated too much data (%d bytes) "
           "on nonopen OR connection %s %s:%u in state %s; closing.",
           (int)buf_datalen(conn->base_.inbuf),
           connection_or_nonopen_was_started_here(conn) ? "to" : "from",
           conn->base_.address, conn->base_.port,
           conn_state_to_string(conn->base_.type, conn->base_.state));
    connection_or_close_for_error(conn, 0);
    ret = -1;
  }

  return ret;
}

/** When adding cells to an OR connection's outbuf, keep adding until the
 * outbuf is at least this long, or we run out of cells. */
#define OR_CONN_HIGHWATER (32*1024)

/** Add cells to an OR connection's outbuf whenever the outbuf's data length
 * drops below this size. */
#define OR_CONN_LOWWATER (16*1024)

/** Called whenever we have flushed some data on an or_conn: add more data
 * from active circuits. */
int
connection_or_flushed_some(or_connection_t *conn)
{
  size_t datalen, temp;
  ssize_t n, flushed;
  size_t cell_network_size = get_cell_network_size(conn->wide_circ_ids);

  /* If we're under the low water mark, add cells until we're just over the
   * high water mark. */
  datalen = connection_get_outbuf_len(TO_CONN(conn));
  if (datalen < OR_CONN_LOWWATER) {
    while ((conn->chan) && channel_tls_more_to_flush(conn->chan)) {
      /* Compute how many more cells we want at most */
      n = CEIL_DIV(OR_CONN_HIGHWATER - datalen, cell_network_size);
      /* Bail out if we don't want any more */
      if (n <= 0) break;
      /* We're still here; try to flush some more cells */
      flushed = channel_tls_flush_some_cells(conn->chan, n);
      /* Bail out if it says it didn't flush anything */
      if (flushed <= 0) break;
      /* How much in the outbuf now? */
      temp = connection_get_outbuf_len(TO_CONN(conn));
      /* Bail out if we didn't actually increase the outbuf size */
      if (temp <= datalen) break;
      /* Update datalen for the next iteration */
      datalen = temp;
    }
  }

  return 0;
}

/** Connection <b>conn</b> has finished writing and has no bytes left on
 * its outbuf.
 *
 * Otherwise it's in state "open": stop writing and return.
 *
 * If <b>conn</b> is broken, mark it for close and return -1, else
 * return 0.
 */
int
connection_or_finished_flushing(or_connection_t *conn)
{
  tor_assert(conn);
  assert_connection_ok(TO_CONN(conn),0);

  switch (conn->base_.state) {
    case OR_CONN_STATE_PROXY_HANDSHAKING:
    case OR_CONN_STATE_OPEN:
    case OR_CONN_STATE_OR_HANDSHAKING_V2:
    case OR_CONN_STATE_OR_HANDSHAKING_V3:
      break;
    default:
      log_err(LD_BUG,"Called in unexpected state %d.", conn->base_.state);
      tor_fragile_assert();
      return -1;
  }
  return 0;
}

/** Connected handler for OR connections: begin the TLS handshake.
 */
int
connection_or_finished_connecting(or_connection_t *or_conn)
{
  const int proxy_type = or_conn->proxy_type;
  connection_t *conn;

  tor_assert(or_conn);
  conn = TO_CONN(or_conn);
  tor_assert(conn->state == OR_CONN_STATE_CONNECTING);

  log_debug(LD_HANDSHAKE,"OR connect() to router at %s:%u finished.",
            conn->address,conn->port);
  control_event_bootstrap(BOOTSTRAP_STATUS_HANDSHAKE, 0);

  if (proxy_type != PROXY_NONE) {
    /* start proxy handshake */
    if (connection_proxy_connect(conn, proxy_type) < 0) {
      connection_or_close_for_error(or_conn, 0);
      return -1;
    }

    connection_start_reading(conn);
    connection_or_change_state(or_conn, OR_CONN_STATE_PROXY_HANDSHAKING);
    return 0;
  }

  if (connection_tls_start_handshake(or_conn, 0) < 0) {
    /* TLS handshaking error of some kind. */
    connection_or_close_for_error(or_conn, 0);
    return -1;
  }
  return 0;
}

/** Called when we're about to finally unlink and free an OR connection:
 * perform necessary accounting and cleanup */
void
connection_or_about_to_close(or_connection_t *or_conn)
{
  time_t now = time(NULL);
  connection_t *conn = TO_CONN(or_conn);

  /* Tell the controlling channel we're closed */
  if (or_conn->chan) {
    channel_closed(TLS_CHAN_TO_BASE(or_conn->chan));
    /*
     * NULL this out because the channel might hang around a little
     * longer before channel_run_cleanup() gets it.
     */
    or_conn->chan->conn = NULL;
    or_conn->chan = NULL;
  }

  /* Remember why we're closing this connection. */
  if (conn->state != OR_CONN_STATE_OPEN) {
    /* now mark things down as needed */
    if (connection_or_nonopen_was_started_here(or_conn)) {
      const or_options_t *options = get_options();
      connection_or_note_state_when_broken(or_conn);
      rep_hist_note_connect_failed(or_conn->identity_digest, now);
      entry_guard_register_connect_status(or_conn->identity_digest,0,
                                          !options->HTTPSProxy, now);
      if (conn->state >= OR_CONN_STATE_TLS_HANDSHAKING) {
        int reason = tls_error_to_orconn_end_reason(or_conn->tls_error);
        control_event_or_conn_status(or_conn, OR_CONN_EVENT_FAILED,
                                     reason);
        if (!authdir_mode_tests_reachability(options))
          control_event_bootstrap_problem(
                orconn_end_reason_to_control_string(reason),
                reason, or_conn);
      }
    }
  } else if (conn->hold_open_until_flushed) {
    /* We only set hold_open_until_flushed when we're intentionally
     * closing a connection. */
    rep_hist_note_disconnect(or_conn->identity_digest, now);
    control_event_or_conn_status(or_conn, OR_CONN_EVENT_CLOSED,
                tls_error_to_orconn_end_reason(or_conn->tls_error));
  } else if (!tor_digest_is_zero(or_conn->identity_digest)) {
    rep_hist_note_connection_died(or_conn->identity_digest, now);
    control_event_or_conn_status(or_conn, OR_CONN_EVENT_CLOSED,
                tls_error_to_orconn_end_reason(or_conn->tls_error));
  }
}

/** Return 1 if identity digest <b>id_digest</b> is known to be a
 * currently or recently running relay. Otherwise return 0. */
int
connection_or_digest_is_known_relay(const char *id_digest)
{
  if (router_get_consensus_status_by_id(id_digest))
    return 1; /* It's in the consensus: "yes" */
  if (router_get_by_id_digest(id_digest))
    return 1; /* Not in the consensus, but we have a descriptor for
               * it. Probably it was in a recent consensus. "Yes". */
  return 0;
}

/** Set the per-conn read and write limits for <b>conn</b>. If it's a known
 * relay, we will rely on the global read and write buckets, so give it
 * per-conn limits that are big enough they'll never matter. But if it's
 * not a known relay, first check if we set PerConnBwRate/Burst, then
 * check if the consensus sets them, else default to 'big enough'.
 *
 * If <b>reset</b> is true, set the bucket to be full.  Otherwise, just
 * clip the bucket if it happens to be <em>too</em> full.
 */
static void
connection_or_update_token_buckets_helper(or_connection_t *conn, int reset,
                                          const or_options_t *options)
{
  int rate, burst; /* per-connection rate limiting params */
  if (connection_or_digest_is_known_relay(conn->identity_digest)) {
    /* It's in the consensus, or we have a descriptor for it meaning it
     * was probably in a recent consensus. It's a recognized relay:
     * give it full bandwidth. */
    rate = (int)options->BandwidthRate;
    burst = (int)options->BandwidthBurst;
  } else {
    /* Not a recognized relay. Squeeze it down based on the suggested
     * bandwidth parameters in the consensus, but allow local config
     * options to override. */
    rate = options->PerConnBWRate ? (int)options->PerConnBWRate :
        networkstatus_get_param(NULL, "perconnbwrate",
                                (int)options->BandwidthRate, 1, INT32_MAX);
    burst = options->PerConnBWBurst ? (int)options->PerConnBWBurst :
        networkstatus_get_param(NULL, "perconnbwburst",
                                (int)options->BandwidthBurst, 1, INT32_MAX);
  }

  conn->bandwidthrate = rate;
  conn->bandwidthburst = burst;
#ifdef USE_BUFFEREVENTS
  {
    const struct timeval *tick = tor_libevent_get_one_tick_timeout();
    struct ev_token_bucket_cfg *cfg, *old_cfg;
    int64_t rate64 = (((int64_t)rate) * options->TokenBucketRefillInterval)
      / 1000;
    /* This can't overflow, since TokenBucketRefillInterval <= 1000,
     * and rate started out less than INT_MAX. */
    int rate_per_tick = (int) rate64;

    cfg = ev_token_bucket_cfg_new(rate_per_tick, burst, rate_per_tick,
                                  burst, tick);
    old_cfg = conn->bucket_cfg;
    if (conn->base_.bufev)
      tor_set_bufferevent_rate_limit(conn->base_.bufev, cfg);
    if (old_cfg)
      ev_token_bucket_cfg_free(old_cfg);
    conn->bucket_cfg = cfg;
    (void) reset; /* No way to do this with libevent yet. */
  }
#else
  if (reset) { /* set up the token buckets to be full */
    conn->read_bucket = conn->write_bucket = burst;
    return;
  }
  /* If the new token bucket is smaller, take out the extra tokens.
   * (If it's larger, don't -- the buckets can grow to reach the cap.) */
  if (conn->read_bucket > burst)
    conn->read_bucket = burst;
  if (conn->write_bucket > burst)
    conn->write_bucket = burst;
#endif
}

/** Either our set of relays or our per-conn rate limits have changed.
 * Go through all the OR connections and update their token buckets to make
 * sure they don't exceed their maximum values. */
void
connection_or_update_token_buckets(smartlist_t *conns,
                                   const or_options_t *options)
{
  SMARTLIST_FOREACH(conns, connection_t *, conn,
  {
    if (connection_speaks_cells(conn))
      connection_or_update_token_buckets_helper(TO_OR_CONN(conn), 0, options);
  });
}

/** How long do we wait before killing non-canonical OR connections with no
 * circuits?  In Tor versions up to 0.2.1.25 and 0.2.2.12-alpha, we waited 15
 * minutes before cancelling these connections, which caused fast relays to
 * accrue many many idle connections. Hopefully 3-4.5 minutes is low enough
 * that it kills most idle connections, without being so low that we cause
 * clients to bounce on and off.
 *
 * For canonical connections, the limit is higher, at 15-22.5 minutes.
 *
 * For each OR connection, we randomly add up to 50% extra to its idle_timeout
 * field, to avoid exposing when exactly the last circuit closed.  Since we're
 * storing idle_timeout in a uint16_t, don't let these values get higher than
 * 12 hours or so without revising connection_or_set_canonical and/or expanding
 * idle_timeout.
 */
#define IDLE_OR_CONN_TIMEOUT_NONCANONICAL 180
#define IDLE_OR_CONN_TIMEOUT_CANONICAL 900

/* Mark <b>or_conn</b> as canonical if <b>is_canonical</b> is set, and
 * non-canonical otherwise. Adjust idle_timeout accordingly.
 */
void
connection_or_set_canonical(or_connection_t *or_conn,
                            int is_canonical)
{
  const unsigned int timeout_base = is_canonical ?
    IDLE_OR_CONN_TIMEOUT_CANONICAL : IDLE_OR_CONN_TIMEOUT_NONCANONICAL;

  if (bool_eq(is_canonical, or_conn->is_canonical) &&
      or_conn->idle_timeout != 0) {
    /* Don't recalculate an existing idle_timeout unless the canonical
     * status changed. */
    return;
  }

  or_conn->is_canonical = !! is_canonical; /* force to a 1-bit boolean */
  or_conn->idle_timeout = timeout_base + crypto_rand_int(timeout_base / 2);
}

/** If we don't necessarily know the router we're connecting to, but we
 * have an addr/port/id_digest, then fill in as much as we can. Start
 * by checking to see if this describes a router we know.
 * <b>started_here</b> is 1 if we are the initiator of <b>conn</b> and
 * 0 if it's an incoming connection.  */
void
connection_or_init_conn_from_address(or_connection_t *conn,
                                     const tor_addr_t *addr, uint16_t port,
                                     const char *id_digest,
                                     int started_here)
{
  const node_t *r = node_get_by_id(id_digest);
  connection_or_set_identity_digest(conn, id_digest);
  connection_or_update_token_buckets_helper(conn, 1, get_options());

  conn->base_.port = port;
  tor_addr_copy(&conn->base_.addr, addr);
  tor_addr_copy(&conn->real_addr, addr);
  if (r) {
    tor_addr_port_t node_ap;
    node_get_pref_orport(r, &node_ap);
    /* XXXX proposal 186 is making this more complex.  For now, a conn
       is canonical when it uses the _preferred_ address. */
    if (tor_addr_eq(&conn->base_.addr, &node_ap.addr))
      connection_or_set_canonical(conn, 1);
    if (!started_here) {
      /* Override the addr/port, so our log messages will make sense.
       * This is dangerous, since if we ever try looking up a conn by
       * its actual addr/port, we won't remember. Careful! */
      /* XXXX arma: this is stupid, and it's the reason we need real_addr
       * to track is_canonical properly.  What requires it? */
      /* XXXX <arma> i believe the reason we did this, originally, is because
       * we wanted to log what OR a connection was to, and if we logged the
       * right IP address and port 56244, that wouldn't be as helpful. now we
       * log the "right" port too, so we know if it's moria1 or moria2.
       */
      tor_addr_copy(&conn->base_.addr, &node_ap.addr);
      conn->base_.port = node_ap.port;
    }
    conn->nickname = tor_strdup(node_get_nickname(r));
    tor_free(conn->base_.address);
    conn->base_.address = tor_dup_addr(&node_ap.addr);
  } else {
    const char *n;
    /* If we're an authoritative directory server, we may know a
     * nickname for this router. */
    n = dirserv_get_nickname_by_digest(id_digest);
    if (n) {
      conn->nickname = tor_strdup(n);
    } else {
      conn->nickname = tor_malloc(HEX_DIGEST_LEN+2);
      conn->nickname[0] = '$';
      base16_encode(conn->nickname+1, HEX_DIGEST_LEN+1,
                    conn->identity_digest, DIGEST_LEN);
    }
    tor_free(conn->base_.address);
    conn->base_.address = tor_dup_addr(addr);
  }

  /*
   * We have to tell channeltls.c to update the channel marks (local, in
   * particular), since we may have changed the address.
   */

  if (conn->chan) {
    channel_tls_update_marks(conn);
  }
}

/** These just pass all the is_bad_for_new_circs manipulation on to
 * channel_t */

static unsigned int
connection_or_is_bad_for_new_circs(or_connection_t *or_conn)
{
  tor_assert(or_conn);

  if (or_conn->chan)
    return channel_is_bad_for_new_circs(TLS_CHAN_TO_BASE(or_conn->chan));
  else return 0;
}

static void
connection_or_mark_bad_for_new_circs(or_connection_t *or_conn)
{
  tor_assert(or_conn);

  if (or_conn->chan)
    channel_mark_bad_for_new_circs(TLS_CHAN_TO_BASE(or_conn->chan));
}

/** How old do we let a connection to an OR get before deciding it's
 * too old for new circuits? */
#define TIME_BEFORE_OR_CONN_IS_TOO_OLD (60*60*24*7)

/** Given the head of the linked list for all the or_connections with a given
 * identity, set elements of that list as is_bad_for_new_circs as
 * appropriate. Helper for connection_or_set_bad_connections().
 *
 * Specifically, we set the is_bad_for_new_circs flag on:
 *    - all connections if <b>force</b> is true.
 *    - all connections that are too old.
 *    - all open non-canonical connections for which a canonical connection
 *      exists to the same router.
 *    - all open canonical connections for which a 'better' canonical
 *      connection exists to the same router.
 *    - all open non-canonical connections for which a 'better' non-canonical
 *      connection exists to the same router at the same address.
 *
 * See channel_is_better() in channel.c for our idea of what makes one OR
 * connection better than another.
 */
static void
connection_or_group_set_badness(or_connection_t *head, int force)
{
  or_connection_t *or_conn = NULL, *best = NULL;
  int n_old = 0, n_inprogress = 0, n_canonical = 0, n_other = 0;
  time_t now = time(NULL);

  /* Pass 1: expire everything that's old, and see what the status of
   * everything else is. */
  for (or_conn = head; or_conn; or_conn = or_conn->next_with_same_id) {
    if (or_conn->base_.marked_for_close ||
        connection_or_is_bad_for_new_circs(or_conn))
      continue;
    if (force ||
        or_conn->base_.timestamp_created + TIME_BEFORE_OR_CONN_IS_TOO_OLD
          < now) {
      log_info(LD_OR,
               "Marking OR conn to %s:%d as too old for new circuits "
               "(fd "TOR_SOCKET_T_FORMAT", %d secs old).",
               or_conn->base_.address, or_conn->base_.port, or_conn->base_.s,
               (int)(now - or_conn->base_.timestamp_created));
      connection_or_mark_bad_for_new_circs(or_conn);
    }

    if (connection_or_is_bad_for_new_circs(or_conn)) {
      ++n_old;
    } else if (or_conn->base_.state != OR_CONN_STATE_OPEN) {
      ++n_inprogress;
    } else if (or_conn->is_canonical) {
      ++n_canonical;
    } else {
      ++n_other;
    }
  }

  /* Pass 2: We know how about how good the best connection is.
   * expire everything that's worse, and find the very best if we can. */
  for (or_conn = head; or_conn; or_conn = or_conn->next_with_same_id) {
    if (or_conn->base_.marked_for_close ||
        connection_or_is_bad_for_new_circs(or_conn))
      continue; /* This one doesn't need to be marked bad. */
    if (or_conn->base_.state != OR_CONN_STATE_OPEN)
      continue; /* Don't mark anything bad until we have seen what happens
                 * when the connection finishes. */
    if (n_canonical && !or_conn->is_canonical) {
      /* We have at least one open canonical connection to this router,
       * and this one is open but not canonical.  Mark it bad. */
      log_info(LD_OR,
               "Marking OR conn to %s:%d as unsuitable for new circuits: "
               "(fd "TOR_SOCKET_T_FORMAT", %d secs old).  It is not "
               "canonical, and we have another connection to that OR that is.",
               or_conn->base_.address, or_conn->base_.port, or_conn->base_.s,
               (int)(now - or_conn->base_.timestamp_created));
      connection_or_mark_bad_for_new_circs(or_conn);
      continue;
    }

    if (!best ||
        channel_is_better(now,
                          TLS_CHAN_TO_BASE(or_conn->chan),
                          TLS_CHAN_TO_BASE(best->chan),
                          0)) {
      best = or_conn;
    }
  }

  if (!best)
    return;

  /* Pass 3: One connection to OR is best.  If it's canonical, mark as bad
   * every other open connection.  If it's non-canonical, mark as bad
   * every other open connection to the same address.
   *
   * XXXX This isn't optimal; if we have connections to an OR at multiple
   *   addresses, we'd like to pick the best _for each address_, and mark as
   *   bad every open connection that isn't best for its address.  But this
   *   can only occur in cases where the other OR is old (so we have no
   *   canonical connection to it), or where all the connections to the OR are
   *   at noncanonical addresses and we have no good direct connection (which
   *   means we aren't at risk of attaching circuits to it anyway).  As
   *   0.1.2.x dies out, the first case will go away, and the second one is
   *   "mostly harmless", so a fix can wait until somebody is bored.
   */
  for (or_conn = head; or_conn; or_conn = or_conn->next_with_same_id) {
    if (or_conn->base_.marked_for_close ||
        connection_or_is_bad_for_new_circs(or_conn) ||
        or_conn->base_.state != OR_CONN_STATE_OPEN)
      continue;
    if (or_conn != best &&
        channel_is_better(now,
                          TLS_CHAN_TO_BASE(best->chan),
                          TLS_CHAN_TO_BASE(or_conn->chan), 1)) {
      /* This isn't the best conn, _and_ the best conn is better than it,
         even when we're being forgiving. */
      if (best->is_canonical) {
        log_info(LD_OR,
                 "Marking OR conn to %s:%d as unsuitable for new circuits: "
                 "(fd "TOR_SOCKET_T_FORMAT", %d secs old). "
                 "We have a better canonical one "
                 "(fd "TOR_SOCKET_T_FORMAT"; %d secs old).",
                 or_conn->base_.address, or_conn->base_.port, or_conn->base_.s,
                 (int)(now - or_conn->base_.timestamp_created),
                 best->base_.s, (int)(now - best->base_.timestamp_created));
        connection_or_mark_bad_for_new_circs(or_conn);
      } else if (!tor_addr_compare(&or_conn->real_addr,
                                   &best->real_addr, CMP_EXACT)) {
        log_info(LD_OR,
                 "Marking OR conn to %s:%d as unsuitable for new circuits: "
                 "(fd "TOR_SOCKET_T_FORMAT", %d secs old).  We have a better "
                 "one with the "
                 "same address (fd "TOR_SOCKET_T_FORMAT"; %d secs old).",
                 or_conn->base_.address, or_conn->base_.port, or_conn->base_.s,
                 (int)(now - or_conn->base_.timestamp_created),
                 best->base_.s, (int)(now - best->base_.timestamp_created));
        connection_or_mark_bad_for_new_circs(or_conn);
      }
    }
  }
}

/** Go through all the OR connections (or if <b>digest</b> is non-NULL, just
 * the OR connections with that digest), and set the is_bad_for_new_circs
 * flag based on the rules in connection_or_group_set_badness() (or just
 * always set it if <b>force</b> is true).
 */
void
connection_or_set_bad_connections(const char *digest, int force)
{
  if (!orconn_identity_map)
    return;

  DIGESTMAP_FOREACH(orconn_identity_map, identity, or_connection_t *, conn) {
    if (!digest || tor_memeq(digest, conn->identity_digest, DIGEST_LEN))
      connection_or_group_set_badness(conn, force);
  } DIGESTMAP_FOREACH_END;
}

/** <b>conn</b> is in the 'connecting' state, and it failed to complete
 * a TCP connection. Send notifications appropriately.
 *
 * <b>reason</b> specifies the or_conn_end_reason for the failure;
 * <b>msg</b> specifies the strerror-style error message.
 */
void
connection_or_connect_failed(or_connection_t *conn,
                             int reason, const char *msg)
{
  control_event_or_conn_status(conn, OR_CONN_EVENT_FAILED, reason);
  if (!authdir_mode_tests_reachability(get_options()))
    control_event_bootstrap_problem(msg, reason, conn);
}

/** <b>conn</b> got an error in connection_handle_read_impl() or
 * connection_handle_write_impl() and is going to die soon.
 *
 * <b>reason</b> specifies the or_conn_end_reason for the failure;
 * <b>msg</b> specifies the strerror-style error message.
 */
void
connection_or_notify_error(or_connection_t *conn,
                           int reason, const char *msg)
{
  channel_t *chan;

  tor_assert(conn);

  /* If we're connecting, call connect_failed() too */
  if (TO_CONN(conn)->state == OR_CONN_STATE_CONNECTING)
    connection_or_connect_failed(conn, reason, msg);

  /* Tell the controlling channel if we have one */
  if (conn->chan) {
    chan = TLS_CHAN_TO_BASE(conn->chan);
    /* Don't transition if we're already in closing, closed or error */
    if (!(chan->state == CHANNEL_STATE_CLOSING ||
          chan->state == CHANNEL_STATE_CLOSED ||
          chan->state == CHANNEL_STATE_ERROR)) {
      channel_close_for_error(chan);
    }
  }

  /* No need to mark for error because connection.c is about to do that */
}

/** Launch a new OR connection to <b>addr</b>:<b>port</b> and expect to
 * handshake with an OR with identity digest <b>id_digest</b>.  Optionally,
 * pass in a pointer to a channel using this connection.
 *
 * If <b>id_digest</b> is me, do nothing. If we're already connected to it,
 * return that connection. If the connect() is in progress, set the
 * new conn's state to 'connecting' and return it. If connect() succeeds,
 * call connection_tls_start_handshake() on it.
 *
 * This function is called from router_retry_connections(), for
 * ORs connecting to ORs, and circuit_establish_circuit(), for
 * OPs connecting to ORs.
 *
 * Return the launched conn, or NULL if it failed.
 */
or_connection_t *
connection_or_connect(const tor_addr_t *_addr, uint16_t port,
                      const char *id_digest,
                      channel_tls_t *chan)
{
  or_connection_t *conn;
  const or_options_t *options = get_options();
  int socket_error = 0;
  tor_addr_t addr;

  int r;
  tor_addr_t proxy_addr;
  uint16_t proxy_port;
  int proxy_type;

  tor_assert(_addr);
  tor_assert(id_digest);
  tor_addr_copy(&addr, _addr);

  if (server_mode(options) && router_digest_is_me(id_digest)) {
    log_info(LD_PROTOCOL,"Client asked me to connect to myself. Refusing.");
    return NULL;
  }

  conn = or_connection_new(CONN_TYPE_OR, tor_addr_family(&addr));

  /*
   * Set up conn so it's got all the data we need to remember for channels
   *
   * This stuff needs to happen before connection_or_init_conn_from_address()
   * so connection_or_set_identity_digest() and such know where to look to
   * keep the channel up to date.
   */
  conn->chan = chan;
  chan->conn = conn;
  connection_or_init_conn_from_address(conn, &addr, port, id_digest, 1);
  connection_or_change_state(conn, OR_CONN_STATE_CONNECTING);
  control_event_or_conn_status(conn, OR_CONN_EVENT_LAUNCHED, 0);

  conn->is_outgoing = 1;

  /* If we are using a proxy server, find it and use it. */
  r = get_proxy_addrport(&proxy_addr, &proxy_port, &proxy_type, TO_CONN(conn));
  if (r == 0) {
    conn->proxy_type = proxy_type;
    if (proxy_type != PROXY_NONE) {
      tor_addr_copy(&addr, &proxy_addr);
      port = proxy_port;
      conn->base_.proxy_state = PROXY_INFANT;
    }
  } else {
    /* get_proxy_addrport() might fail if we have a Bridge line that
       references a transport, but no ClientTransportPlugin lines
       defining its transport proxy. If this is the case, let's try to
       output a useful log message to the user. */
    const char *transport_name =
      find_transport_name_by_bridge_addrport(&TO_CONN(conn)->addr,
                                             TO_CONN(conn)->port);

    if (transport_name) {
      log_warn(LD_GENERAL, "We were supposed to connect to bridge '%s' "
               "using pluggable transport '%s', but we can't find a pluggable "
               "transport proxy supporting '%s'. This can happen if you "
               "haven't provided a ClientTransportPlugin line, or if "
               "your pluggable transport proxy stopped running.",
               fmt_addrport(&TO_CONN(conn)->addr, TO_CONN(conn)->port),
               transport_name, transport_name);

      control_event_bootstrap_problem(
                                "Can't connect to bridge",
                                END_OR_CONN_REASON_PT_MISSING,
                                conn);

    } else {
      log_warn(LD_GENERAL, "Tried to connect to '%s' through a proxy, but "
               "the proxy address could not be found.",
               fmt_addrport(&TO_CONN(conn)->addr, TO_CONN(conn)->port));
    }

    connection_free(TO_CONN(conn));
    return NULL;
  }

  switch (connection_connect(TO_CONN(conn), conn->base_.address,
                             &addr, port, &socket_error)) {
    case -1:
      /* If the connection failed immediately, and we're using
       * a proxy, our proxy is down. Don't blame the Tor server. */
      if (conn->base_.proxy_state == PROXY_INFANT)
        entry_guard_register_connect_status(conn->identity_digest,
                                            0, 1, time(NULL));
      connection_or_connect_failed(conn,
                                   errno_to_orconn_end_reason(socket_error),
                                   tor_socket_strerror(socket_error));
      connection_free(TO_CONN(conn));
      return NULL;
    case 0:
      connection_watch_events(TO_CONN(conn), READ_EVENT | WRITE_EVENT);
      /* writable indicates finish, readable indicates broken link,
         error indicates broken link on windows */
      return conn;
    /* case 1: fall through */
  }

  if (connection_or_finished_connecting(conn) < 0) {
    /* already marked for close */
    return NULL;
  }
  return conn;
}

/** Mark orconn for close and transition the associated channel, if any, to
 * the closing state.
 *
 * It's safe to call this and connection_or_close_for_error() any time, and
 * channel layer will treat it as a connection closing for reasons outside
 * its control, like the remote end closing it.  It can also be a local
 * reason that's specific to connection_t/or_connection_t rather than
 * the channel mechanism, such as expiration of old connections in
 * run_connection_housekeeping().  If you want to close a channel_t
 * from somewhere that logically works in terms of generic channels
 * rather than connections, use channel_mark_for_close(); see also
 * the comment on that function in channel.c.
 */

void
connection_or_close_normally(or_connection_t *orconn, int flush)
{
  channel_t *chan = NULL;

  tor_assert(orconn);
  if (flush) connection_mark_and_flush_internal(TO_CONN(orconn));
  else connection_mark_for_close_internal(TO_CONN(orconn));
  if (orconn->chan) {
    chan = TLS_CHAN_TO_BASE(orconn->chan);
    /* Don't transition if we're already in closing, closed or error */
    if (!(chan->state == CHANNEL_STATE_CLOSING ||
          chan->state == CHANNEL_STATE_CLOSED ||
          chan->state == CHANNEL_STATE_ERROR)) {
      channel_close_from_lower_layer(chan);
    }
  }
}

/** Mark orconn for close and transition the associated channel, if any, to
 * the error state.
 */

void
connection_or_close_for_error(or_connection_t *orconn, int flush)
{
  channel_t *chan = NULL;

  tor_assert(orconn);
  if (flush) connection_mark_and_flush_internal(TO_CONN(orconn));
  else connection_mark_for_close_internal(TO_CONN(orconn));
  if (orconn->chan) {
    chan = TLS_CHAN_TO_BASE(orconn->chan);
    /* Don't transition if we're already in closing, closed or error */
    if (!(chan->state == CHANNEL_STATE_CLOSING ||
          chan->state == CHANNEL_STATE_CLOSED ||
          chan->state == CHANNEL_STATE_ERROR)) {
      channel_close_for_error(chan);
    }
  }
}

/** Begin the tls handshake with <b>conn</b>. <b>receiving</b> is 0 if
 * we initiated the connection, else it's 1.
 *
 * Assign a new tls object to conn->tls, begin reading on <b>conn</b>, and
 * pass <b>conn</b> to connection_tls_continue_handshake().
 *
 * Return -1 if <b>conn</b> is broken, else return 0.
 */
MOCK_IMPL(int,
connection_tls_start_handshake,(or_connection_t *conn, int receiving))
{
  channel_listener_t *chan_listener;
  channel_t *chan;

  /* Incoming connections will need a new channel passed to the
   * channel_tls_listener */
  if (receiving) {
    /* It shouldn't already be set */
    tor_assert(!(conn->chan));
    chan_listener = channel_tls_get_listener();
    if (!chan_listener) {
      chan_listener = channel_tls_start_listener();
      command_setup_listener(chan_listener);
    }
    chan = channel_tls_handle_incoming(conn);
    channel_listener_queue_incoming(chan_listener, chan);
  }

  connection_or_change_state(conn, OR_CONN_STATE_TLS_HANDSHAKING);
  tor_assert(!conn->tls);
  conn->tls = tor_tls_new(conn->base_.s, receiving);
  if (!conn->tls) {
    log_warn(LD_BUG,"tor_tls_new failed. Closing.");
    return -1;
  }
  tor_tls_set_logged_address(conn->tls, // XXX client and relay?
      escaped_safe_str(conn->base_.address));

#ifdef USE_BUFFEREVENTS
  if (connection_type_uses_bufferevent(TO_CONN(conn))) {
    const int filtering = get_options()->UseFilteringSSLBufferevents;
    struct bufferevent *b =
      tor_tls_init_bufferevent(conn->tls, conn->base_.bufev, conn->base_.s,
                               receiving, filtering);
    if (!b) {
      log_warn(LD_BUG,"tor_tls_init_bufferevent failed. Closing.");
      return -1;
    }
    conn->base_.bufev = b;
    if (conn->bucket_cfg)
      tor_set_bufferevent_rate_limit(conn->base_.bufev, conn->bucket_cfg);
    connection_enable_rate_limiting(TO_CONN(conn));

    connection_configure_bufferevent_callbacks(TO_CONN(conn));
    bufferevent_setcb(b,
                      connection_handle_read_cb,
                      connection_handle_write_cb,
                      connection_or_handle_event_cb,/* overriding this one*/
                      TO_CONN(conn));
  }
#endif
  connection_start_reading(TO_CONN(conn));
  log_debug(LD_HANDSHAKE,"starting TLS handshake on fd "TOR_SOCKET_T_FORMAT,
            conn->base_.s);
  note_crypto_pk_op(receiving ? TLS_HANDSHAKE_S : TLS_HANDSHAKE_C);

  IF_HAS_BUFFEREVENT(TO_CONN(conn), {
    /* ???? */;
  }) ELSE_IF_NO_BUFFEREVENT {
    if (connection_tls_continue_handshake(conn) < 0)
      return -1;
  }
  return 0;
}

/** Block all future attempts to renegotiate on 'conn' */
void
connection_or_block_renegotiation(or_connection_t *conn)
{
  tor_tls_t *tls = conn->tls;
  if (!tls)
    return;
  tor_tls_set_renegotiate_callback(tls, NULL, NULL);
  tor_tls_block_renegotiation(tls);
}

/** Invoked on the server side from inside tor_tls_read() when the server
 * gets a successful TLS renegotiation from the client. */
static void
connection_or_tls_renegotiated_cb(tor_tls_t *tls, void *_conn)
{
  or_connection_t *conn = _conn;
  (void)tls;

  /* Don't invoke this again. */
  connection_or_block_renegotiation(conn);

  if (connection_tls_finish_handshake(conn) < 0) {
    /* XXXX_TLS double-check that it's ok to do this from inside read. */
    /* XXXX_TLS double-check that this verifies certificates. */
    connection_or_close_for_error(conn, 0);
  }
}

/** Move forward with the tls handshake. If it finishes, hand
 * <b>conn</b> to connection_tls_finish_handshake().
 *
 * Return -1 if <b>conn</b> is broken, else return 0.
 */
int
connection_tls_continue_handshake(or_connection_t *conn)
{
  int result;
  check_no_tls_errors();
 again:
  if (conn->base_.state == OR_CONN_STATE_TLS_CLIENT_RENEGOTIATING) {
    // log_notice(LD_OR, "Renegotiate with %p", conn->tls);
    result = tor_tls_renegotiate(conn->tls);
    // log_notice(LD_OR, "Result: %d", result);
  } else {
    tor_assert(conn->base_.state == OR_CONN_STATE_TLS_HANDSHAKING);
    // log_notice(LD_OR, "Continue handshake with %p", conn->tls);
    result = tor_tls_handshake(conn->tls);
    // log_notice(LD_OR, "Result: %d", result);
  }
  switch (result) {
    CASE_TOR_TLS_ERROR_ANY:
    log_info(LD_OR,"tls error [%s]. breaking connection.",
             tor_tls_err_to_string(result));
      return -1;
    case TOR_TLS_DONE:
      if (! tor_tls_used_v1_handshake(conn->tls)) {
        if (!tor_tls_is_server(conn->tls)) {
          if (conn->base_.state == OR_CONN_STATE_TLS_HANDSHAKING) {
            if (tor_tls_received_v3_certificate(conn->tls)) {
              log_info(LD_OR, "Client got a v3 cert!  Moving on to v3 "
                       "handshake with ciphersuite %s",
                       tor_tls_get_ciphersuite_name(conn->tls));
              return connection_or_launch_v3_or_handshake(conn);
            } else {
              log_debug(LD_OR, "Done with initial SSL handshake (client-side)."
                        " Requesting renegotiation.");
              connection_or_change_state(conn,
                  OR_CONN_STATE_TLS_CLIENT_RENEGOTIATING);
              goto again;
            }
          }
          // log_notice(LD_OR,"Done. state was %d.", conn->base_.state);
        } else {
          /* v2/v3 handshake, but not a client. */
          log_debug(LD_OR, "Done with initial SSL handshake (server-side). "
                           "Expecting renegotiation or VERSIONS cell");
          tor_tls_set_renegotiate_callback(conn->tls,
                                           connection_or_tls_renegotiated_cb,
                                           conn);
          connection_or_change_state(conn,
              OR_CONN_STATE_TLS_SERVER_RENEGOTIATING);
          connection_stop_writing(TO_CONN(conn));
          connection_start_reading(TO_CONN(conn));
          return 0;
        }
      }
      return connection_tls_finish_handshake(conn);
    case TOR_TLS_WANTWRITE:
      connection_start_writing(TO_CONN(conn));
      log_debug(LD_OR,"wanted write");
      return 0;
    case TOR_TLS_WANTREAD: /* handshaking conns are *always* reading */
      log_debug(LD_OR,"wanted read");
      return 0;
    case TOR_TLS_CLOSE:
      log_info(LD_OR,"tls closed. breaking connection.");
      return -1;
  }
  return 0;
}

#ifdef USE_BUFFEREVENTS
static void
connection_or_handle_event_cb(struct bufferevent *bufev, short event,
                              void *arg)
{
  struct or_connection_t *conn = TO_OR_CONN(arg);

  /* XXXX cut-and-paste code; should become a function. */
  if (event & BEV_EVENT_CONNECTED) {
    if (conn->base_.state == OR_CONN_STATE_TLS_HANDSHAKING) {
      if (tor_tls_finish_handshake(conn->tls) < 0) {
        log_warn(LD_OR, "Problem finishing handshake");
        connection_or_close_for_error(conn, 0);
        return;
      }
    }

    if (! tor_tls_used_v1_handshake(conn->tls)) {
      if (!tor_tls_is_server(conn->tls)) {
        if (conn->base_.state == OR_CONN_STATE_TLS_HANDSHAKING) {
          if (tor_tls_received_v3_certificate(conn->tls)) {
            log_info(LD_OR, "Client got a v3 cert!");
            if (connection_or_launch_v3_or_handshake(conn) < 0)
              connection_or_close_for_error(conn, 0);
            return;
          } else {
            connection_or_change_state(conn,
                OR_CONN_STATE_TLS_CLIENT_RENEGOTIATING);
            tor_tls_unblock_renegotiation(conn->tls);
            if (bufferevent_ssl_renegotiate(conn->base_.bufev)<0) {
              log_warn(LD_OR, "Start_renegotiating went badly.");
              connection_or_close_for_error(conn, 0);
            }
            tor_tls_unblock_renegotiation(conn->tls);
            return; /* ???? */
          }
        }
      } else {
        const int handshakes = tor_tls_get_num_server_handshakes(conn->tls);

        if (handshakes == 1) {
          /* v2 or v3 handshake, as a server. Only got one handshake, so
           * wait for the next one. */
          tor_tls_set_renegotiate_callback(conn->tls,
                                           connection_or_tls_renegotiated_cb,
                                           conn);
          connection_or_change_state(conn,
              OR_CONN_STATE_TLS_SERVER_RENEGOTIATING);
        } else if (handshakes == 2) {
          /* v2 handshake, as a server.  Two handshakes happened already,
           * so we treat renegotiation as done.
           */
          connection_or_tls_renegotiated_cb(conn->tls, conn);
        } else if (handshakes > 2) {
          log_warn(LD_OR, "More than two handshakes done on connection. "
                   "Closing.");
          connection_or_close_for_error(conn, 0);
        } else {
          log_warn(LD_BUG, "We were unexpectedly told that a connection "
                   "got %d handshakes. Closing.", handshakes);
          connection_or_close_for_error(conn, 0);
        }
        return;
      }
    }
    connection_watch_events(TO_CONN(conn), READ_EVENT|WRITE_EVENT);
    if (connection_tls_finish_handshake(conn) < 0)
      connection_or_close_for_error(conn, 0); /* ???? */
    return;
  }

  if (event & BEV_EVENT_ERROR) {
    unsigned long err;
    while ((err = bufferevent_get_openssl_error(bufev))) {
      tor_tls_log_one_error(conn->tls, err, LOG_WARN, LD_OR,
                            "handshaking (with bufferevent)");
    }
  }

  connection_handle_event_cb(bufev, event, arg);
}
#endif

/** Return 1 if we initiated this connection, or 0 if it started
 * out as an incoming connection.
 */
int
connection_or_nonopen_was_started_here(or_connection_t *conn)
{
  tor_assert(conn->base_.type == CONN_TYPE_OR ||
             conn->base_.type == CONN_TYPE_EXT_OR);
  if (!conn->tls)
    return 1; /* it's still in proxy states or something */
  if (conn->handshake_state)
    return conn->handshake_state->started_here;
  return !tor_tls_is_server(conn->tls);
}

/** <b>Conn</b> just completed its handshake. Return 0 if all is well, and
 * return -1 if he is lying, broken, or otherwise something is wrong.
 *
 * If we initiated this connection (<b>started_here</b> is true), make sure
 * the other side sent a correctly formed certificate. If I initiated the
 * connection, make sure it's the right guy.
 *
 * Otherwise (if we _didn't_ initiate this connection), it's okay for
 * the certificate to be weird or absent.
 *
 * If we return 0, and the certificate is as expected, write a hash of the
 * identity key into <b>digest_rcvd_out</b>, which must have DIGEST_LEN
 * space in it.
 * If the certificate is invalid or missing on an incoming connection,
 * we return 0 and set <b>digest_rcvd_out</b> to DIGEST_LEN NUL bytes.
 * (If we return -1, the contents of this buffer are undefined.)
 *
 * As side effects,
 * 1) Set conn->circ_id_type according to tor-spec.txt.
 * 2) If we're an authdirserver and we initiated the connection: drop all
 *    descriptors that claim to be on that IP/port but that aren't
 *    this guy; and note that this guy is reachable.
 * 3) If this is a bridge and we didn't configure its identity
 *    fingerprint, remember the keyid we just learned.
 */
static int
connection_or_check_valid_tls_handshake(or_connection_t *conn,
                                        int started_here,
                                        char *digest_rcvd_out)
{
  crypto_pk_t *identity_rcvd=NULL;
  const or_options_t *options = get_options();
  int severity = server_mode(options) ? LOG_PROTOCOL_WARN : LOG_WARN;
  const char *safe_address =
    started_here ? conn->base_.address :
                   safe_str_client(conn->base_.address);
  const char *conn_type = started_here ? "outgoing" : "incoming";
  int has_cert = 0;

  check_no_tls_errors();
  has_cert = tor_tls_peer_has_cert(conn->tls);
  if (started_here && !has_cert) {
    log_info(LD_HANDSHAKE,"Tried connecting to router at %s:%d, but it didn't "
             "send a cert! Closing.",
             safe_address, conn->base_.port);
    return -1;
  } else if (!has_cert) {
    log_debug(LD_HANDSHAKE,"Got incoming connection with no certificate. "
              "That's ok.");
  }
  check_no_tls_errors();

  if (has_cert) {
    int v = tor_tls_verify(started_here?severity:LOG_INFO,
                           conn->tls, &identity_rcvd);
    if (started_here && v<0) {
      log_fn(severity,LD_HANDSHAKE,"Tried connecting to router at %s:%d: It"
             " has a cert but it's invalid. Closing.",
             safe_address, conn->base_.port);
        return -1;
    } else if (v<0) {
      log_info(LD_HANDSHAKE,"Incoming connection gave us an invalid cert "
               "chain; ignoring.");
    } else {
      log_debug(LD_HANDSHAKE,
                "The certificate seems to be valid on %s connection "
                "with %s:%d", conn_type, safe_address, conn->base_.port);
    }
    check_no_tls_errors();
  }

  if (identity_rcvd) {
    crypto_pk_get_digest(identity_rcvd, digest_rcvd_out);
  } else {
    memset(digest_rcvd_out, 0, DIGEST_LEN);
  }

  tor_assert(conn->chan);
  channel_set_circid_type(TLS_CHAN_TO_BASE(conn->chan), identity_rcvd, 1);

  crypto_pk_free(identity_rcvd);

  if (started_here)
    return connection_or_client_learned_peer_id(conn,
                                     (const uint8_t*)digest_rcvd_out);

  return 0;
}

/** Called when we (as a connection initiator) have definitively,
 * authenticatedly, learned that ID of the Tor instance on the other
 * side of <b>conn</b> is <b>peer_id</b>.  For v1 and v2 handshakes,
 * this is right after we get a certificate chain in a TLS handshake
 * or renegotiation.  For v3 handshakes, this is right after we get a
 * certificate chain in a CERTS cell.
 *
 * If we want any particular ID before, record the one we got.
 *
 * If we wanted an ID, but we didn't get it, log a warning and return -1.
 *
 * If we're testing reachability, remember what we learned.
 *
 * Return 0 on success, -1 on failure.
 */
int
connection_or_client_learned_peer_id(or_connection_t *conn,
                                     const uint8_t *peer_id)
{
  const or_options_t *options = get_options();
  int severity = server_mode(options) ? LOG_PROTOCOL_WARN : LOG_WARN;

  if (tor_digest_is_zero(conn->identity_digest)) {
    connection_or_set_identity_digest(conn, (const char*)peer_id);
    tor_free(conn->nickname);
    conn->nickname = tor_malloc(HEX_DIGEST_LEN+2);
    conn->nickname[0] = '$';
    base16_encode(conn->nickname+1, HEX_DIGEST_LEN+1,
                  conn->identity_digest, DIGEST_LEN);
    log_info(LD_HANDSHAKE, "Connected to router %s at %s:%d without knowing "
                    "its key. Hoping for the best.",
                    conn->nickname, conn->base_.address, conn->base_.port);
    /* if it's a bridge and we didn't know its identity fingerprint, now
     * we do -- remember it for future attempts. */
    learned_router_identity(&conn->base_.addr, conn->base_.port,
                            (const char*)peer_id);
  }

  if (tor_memneq(peer_id, conn->identity_digest, DIGEST_LEN)) {
    /* I was aiming for a particular digest. I didn't get it! */
    char seen[HEX_DIGEST_LEN+1];
    char expected[HEX_DIGEST_LEN+1];
    base16_encode(seen, sizeof(seen), (const char*)peer_id, DIGEST_LEN);
    base16_encode(expected, sizeof(expected), conn->identity_digest,
                  DIGEST_LEN);
    log_fn(severity, LD_HANDSHAKE,
           "Tried connecting to router at %s:%d, but identity key was not "
           "as expected: wanted %s but got %s.",
           conn->base_.address, conn->base_.port, expected, seen);
    entry_guard_register_connect_status(conn->identity_digest, 0, 1,
                                        time(NULL));
    control_event_or_conn_status(conn, OR_CONN_EVENT_FAILED,
                                 END_OR_CONN_REASON_OR_IDENTITY);
    if (!authdir_mode_tests_reachability(options))
      control_event_bootstrap_problem(
                                "Unexpected identity in router certificate",
                                END_OR_CONN_REASON_OR_IDENTITY,
                                conn);
    return -1;
  }
  if (authdir_mode_tests_reachability(options)) {
    dirserv_orconn_tls_done(&conn->base_.addr, conn->base_.port,
                            (const char*)peer_id);
  }

  return 0;
}

/** Return when a client used this, for connection.c, since client_used
 * is now one of the timestamps of channel_t */

time_t
connection_or_client_used(or_connection_t *conn)
{
  tor_assert(conn);

  if (conn->chan) {
    return channel_when_last_client(TLS_CHAN_TO_BASE(conn->chan));
  } else return 0;
}

/** The v1/v2 TLS handshake is finished.
 *
 * Make sure we are happy with the person we just handshaked with.
 *
 * If he initiated the connection, make sure he's not already connected,
 * then initialize conn from the information in router.
 *
 * If all is successful, call circuit_n_conn_done() to handle events
 * that have been pending on the <tls handshake completion. Also set the
 * directory to be dirty (only matters if I'm an authdirserver).
 *
 * If this is a v2 TLS handshake, send a versions cell.
 */
static int
connection_tls_finish_handshake(or_connection_t *conn)
{
  char digest_rcvd[DIGEST_LEN];
  int started_here = connection_or_nonopen_was_started_here(conn);

  log_debug(LD_HANDSHAKE,"%s tls handshake on %p with %s done, using "
            "ciphersuite %s. verifying.",
            started_here?"outgoing":"incoming",
            conn,
            safe_str_client(conn->base_.address),
            tor_tls_get_ciphersuite_name(conn->tls));

  if (connection_or_check_valid_tls_handshake(conn, started_here,
                                              digest_rcvd) < 0)
    return -1;

  circuit_build_times_network_is_live(get_circuit_build_times_mutable());

  if (tor_tls_used_v1_handshake(conn->tls)) {
    conn->link_proto = 1;
    if (!started_here) {
      connection_or_init_conn_from_address(conn, &conn->base_.addr,
                                           conn->base_.port, digest_rcvd, 0);
    }
    tor_tls_block_renegotiation(conn->tls);
    return connection_or_set_state_open(conn);
  } else {
    connection_or_change_state(conn, OR_CONN_STATE_OR_HANDSHAKING_V2);
    if (connection_init_or_handshake_state(conn, started_here) < 0)
      return -1;
    if (!started_here) {
      connection_or_init_conn_from_address(conn, &conn->base_.addr,
                                           conn->base_.port, digest_rcvd, 0);
    }
    return connection_or_send_versions(conn, 0);
  }
}

/**
 * Called as client when initial TLS handshake is done, and we notice
 * that we got a v3-handshake signalling certificate from the server.
 * Set up structures, do bookkeeping, and send the versions cell.
 * Return 0 on success and -1 on failure.
 */
static int
connection_or_launch_v3_or_handshake(or_connection_t *conn)
{
  tor_assert(connection_or_nonopen_was_started_here(conn));
  tor_assert(tor_tls_received_v3_certificate(conn->tls));

  circuit_build_times_network_is_live(get_circuit_build_times_mutable());

  connection_or_change_state(conn, OR_CONN_STATE_OR_HANDSHAKING_V3);
  if (connection_init_or_handshake_state(conn, 1) < 0)
    return -1;

  return connection_or_send_versions(conn, 1);
}

/** Allocate a new connection handshake state for the connection
 * <b>conn</b>.  Return 0 on success, -1 on failure. */
int
connection_init_or_handshake_state(or_connection_t *conn, int started_here)
{
  or_handshake_state_t *s;
  if (conn->handshake_state) {
    log_warn(LD_BUG, "Duplicate call to connection_init_or_handshake_state!");
    return 0;
  }
  s = conn->handshake_state = tor_malloc_zero(sizeof(or_handshake_state_t));
  s->started_here = started_here ? 1 : 0;
  s->digest_sent_data = 1;
  s->digest_received_data = 1;
  return 0;
}

/** Free all storage held by <b>state</b>. */
void
or_handshake_state_free(or_handshake_state_t *state)
{
  if (!state)
    return;
  crypto_digest_free(state->digest_sent);
  crypto_digest_free(state->digest_received);
  tor_cert_free(state->auth_cert);
  tor_cert_free(state->id_cert);
  memwipe(state, 0xBE, sizeof(or_handshake_state_t));
  tor_free(state);
}

/**
 * Remember that <b>cell</b> has been transmitted (if <b>incoming</b> is
 * false) or received (if <b>incoming</b> is true) during a V3 handshake using
 * <b>state</b>.
 *
 * (We don't record the cell, but we keep a digest of everything sent or
 * received during the v3 handshake, and the client signs it in an
 * authenticate cell.)
 */
void
or_handshake_state_record_cell(or_connection_t *conn,
                               or_handshake_state_t *state,
                               const cell_t *cell,
                               int incoming)
{
  size_t cell_network_size = get_cell_network_size(conn->wide_circ_ids);
  crypto_digest_t *d, **dptr;
  packed_cell_t packed;
  if (incoming) {
    if (!state->digest_received_data)
      return;
  } else {
    if (!state->digest_sent_data)
      return;
  }
  if (!incoming) {
    log_warn(LD_BUG, "We shouldn't be sending any non-variable-length cells "
             "while making a handshake digest.  But we think we are sending "
             "one with type %d.", (int)cell->command);
  }
  dptr = incoming ? &state->digest_received : &state->digest_sent;
  if (! *dptr)
    *dptr = crypto_digest256_new(DIGEST_SHA256);

  d = *dptr;
  /* Re-packing like this is a little inefficient, but we don't have to do
     this very often at all. */
  cell_pack(&packed, cell, conn->wide_circ_ids);
  crypto_digest_add_bytes(d, packed.body, cell_network_size);
  memwipe(&packed, 0, sizeof(packed));
}

/** Remember that a variable-length <b>cell</b> has been transmitted (if
 * <b>incoming</b> is false) or received (if <b>incoming</b> is true) during a
 * V3 handshake using <b>state</b>.
 *
 * (We don't record the cell, but we keep a digest of everything sent or
 * received during the v3 handshake, and the client signs it in an
 * authenticate cell.)
 */
void
or_handshake_state_record_var_cell(or_connection_t *conn,
                                   or_handshake_state_t *state,
                                   const var_cell_t *cell,
                                   int incoming)
{
  crypto_digest_t *d, **dptr;
  int n;
  char buf[VAR_CELL_MAX_HEADER_SIZE];
  if (incoming) {
    if (!state->digest_received_data)
      return;
  } else {
    if (!state->digest_sent_data)
      return;
  }
  dptr = incoming ? &state->digest_received : &state->digest_sent;
  if (! *dptr)
    *dptr = crypto_digest256_new(DIGEST_SHA256);

  d = *dptr;

  n = var_cell_pack_header(cell, buf, conn->wide_circ_ids);
  crypto_digest_add_bytes(d, buf, n);
  crypto_digest_add_bytes(d, (const char *)cell->payload, cell->payload_len);

  memwipe(buf, 0, sizeof(buf));
}

/** Set <b>conn</b>'s state to OR_CONN_STATE_OPEN, and tell other subsystems
 * as appropriate.  Called when we are done with all TLS and OR handshaking.
 */
int
connection_or_set_state_open(or_connection_t *conn)
{
  connection_or_change_state(conn, OR_CONN_STATE_OPEN);
  control_event_or_conn_status(conn, OR_CONN_EVENT_CONNECTED, 0);

  or_handshake_state_free(conn->handshake_state);
  conn->handshake_state = NULL;
  IF_HAS_BUFFEREVENT(TO_CONN(conn), {
    connection_watch_events(TO_CONN(conn), READ_EVENT|WRITE_EVENT);
  }) ELSE_IF_NO_BUFFEREVENT {
    connection_start_reading(TO_CONN(conn));
  }

  return 0;
}

/** Pack <b>cell</b> into wire-format, and write it onto <b>conn</b>'s outbuf.
 * For cells that use or affect a circuit, this should only be called by
 * connection_or_flush_from_first_active_circuit().
 */
void
connection_or_write_cell_to_buf(const cell_t *cell, or_connection_t *conn)
{
  packed_cell_t networkcell;
  size_t cell_network_size = get_cell_network_size(conn->wide_circ_ids);

  tor_assert(cell);
  tor_assert(conn);

  cell_pack(&networkcell, cell, conn->wide_circ_ids);

  connection_write_to_buf(networkcell.body, cell_network_size, TO_CONN(conn));

  /* Touch the channel's active timestamp if there is one */
  if (conn->chan)
    channel_timestamp_active(TLS_CHAN_TO_BASE(conn->chan));

  if (conn->base_.state == OR_CONN_STATE_OR_HANDSHAKING_V3)
    or_handshake_state_record_cell(conn, conn->handshake_state, cell, 0);
}

/** Pack a variable-length <b>cell</b> into wire-format, and write it onto
 * <b>conn</b>'s outbuf.  Right now, this <em>DOES NOT</em> support cells that
 * affect a circuit.
 */
void
connection_or_write_var_cell_to_buf(const var_cell_t *cell,
                                    or_connection_t *conn)
{
  int n;
  char hdr[VAR_CELL_MAX_HEADER_SIZE];
  tor_assert(cell);
  tor_assert(conn);
  n = var_cell_pack_header(cell, hdr, conn->wide_circ_ids);
  connection_write_to_buf(hdr, n, TO_CONN(conn));
  connection_write_to_buf((char*)cell->payload,
                          cell->payload_len, TO_CONN(conn));
  if (conn->base_.state == OR_CONN_STATE_OR_HANDSHAKING_V3)
    or_handshake_state_record_var_cell(conn, conn->handshake_state, cell, 0);

  /* Touch the channel's active timestamp if there is one */
  if (conn->chan)
    channel_timestamp_active(TLS_CHAN_TO_BASE(conn->chan));
}

/** See whether there's a variable-length cell waiting on <b>or_conn</b>'s
 * inbuf.  Return values as for fetch_var_cell_from_buf(). */
static int
connection_fetch_var_cell_from_buf(or_connection_t *or_conn, var_cell_t **out)
{
  connection_t *conn = TO_CONN(or_conn);
  IF_HAS_BUFFEREVENT(conn, {
    struct evbuffer *input = bufferevent_get_input(conn->bufev);
    return fetch_var_cell_from_evbuffer(input, out, or_conn->link_proto);
  }) ELSE_IF_NO_BUFFEREVENT {
    return fetch_var_cell_from_buf(conn->inbuf, out, or_conn->link_proto);
  }
}

/** Process cells from <b>conn</b>'s inbuf.
 *
 * Loop: while inbuf contains a cell, pull it off the inbuf, unpack it,
 * and hand it to command_process_cell().
 *
 * Always return 0.
 */
static int
connection_or_process_cells_from_inbuf(or_connection_t *conn)
{
  var_cell_t *var_cell;

  while (1) {
    log_debug(LD_OR,
              TOR_SOCKET_T_FORMAT": starting, inbuf_datalen %d "
              "(%d pending in tls object).",
              conn->base_.s,(int)connection_get_inbuf_len(TO_CONN(conn)),
              tor_tls_get_pending_bytes(conn->tls));
    if (connection_fetch_var_cell_from_buf(conn, &var_cell)) {
      if (!var_cell)
        return 0; /* not yet. */

      /* Touch the channel's active timestamp if there is one */
      if (conn->chan)
        channel_timestamp_active(TLS_CHAN_TO_BASE(conn->chan));

      circuit_build_times_network_is_live(get_circuit_build_times_mutable());
      channel_tls_handle_var_cell(var_cell, conn);
      var_cell_free(var_cell);
    } else {
      const int wide_circ_ids = conn->wide_circ_ids;
      size_t cell_network_size = get_cell_network_size(conn->wide_circ_ids);
      char buf[CELL_MAX_NETWORK_SIZE];
      cell_t cell;
      if (connection_get_inbuf_len(TO_CONN(conn))
          < cell_network_size) /* whole response available? */
        return 0; /* not yet */

      /* Touch the channel's active timestamp if there is one */
      if (conn->chan)
        channel_timestamp_active(TLS_CHAN_TO_BASE(conn->chan));

      circuit_build_times_network_is_live(get_circuit_build_times_mutable());
      connection_fetch_from_buf(buf, cell_network_size, TO_CONN(conn));

      /* retrieve cell info from buf (create the host-order struct from the
       * network-order string) */
      cell_unpack(&cell, buf, wide_circ_ids);

      channel_tls_handle_cell(&cell, conn);
    }
  }
}

/** Array of recognized link protocol versions. */
static const uint16_t or_protocol_versions[] = { 1, 2, 3, 4 };
/** Number of versions in <b>or_protocol_versions</b>. */
static const int n_or_protocol_versions =
  (int)( sizeof(or_protocol_versions)/sizeof(uint16_t) );

/** Return true iff <b>v</b> is a link protocol version that this Tor
 * implementation believes it can support. */
int
is_or_protocol_version_known(uint16_t v)
{
  int i;
  for (i = 0; i < n_or_protocol_versions; ++i) {
    if (or_protocol_versions[i] == v)
      return 1;
  }
  return 0;
}

/** Send a VERSIONS cell on <b>conn</b>, telling the other host about the
 * link protocol versions that this Tor can support.
 *
 * If <b>v3_plus</b>, this is part of a V3 protocol handshake, so only
 * allow protocol version v3 or later.  If not <b>v3_plus</b>, this is
 * not part of a v3 protocol handshake, so don't allow protocol v3 or
 * later.
 **/
int
connection_or_send_versions(or_connection_t *conn, int v3_plus)
{
  var_cell_t *cell;
  int i;
  int n_versions = 0;
  const int min_version = v3_plus ? 3 : 0;
  const int max_version = v3_plus ? UINT16_MAX : 2;
  tor_assert(conn->handshake_state &&
             !conn->handshake_state->sent_versions_at);
  cell = var_cell_new(n_or_protocol_versions * 2);
  cell->command = CELL_VERSIONS;
  for (i = 0; i < n_or_protocol_versions; ++i) {
    uint16_t v = or_protocol_versions[i];
    if (v < min_version || v > max_version)
      continue;
    set_uint16(cell->payload+(2*n_versions), htons(v));
    ++n_versions;
  }
  cell->payload_len = n_versions * 2;

  connection_or_write_var_cell_to_buf(cell, conn);
  conn->handshake_state->sent_versions_at = time(NULL);

  var_cell_free(cell);
  return 0;
}

/** Send a NETINFO cell on <b>conn</b>, telling the other server what we know
 * about their address, our address, and the current time. */
int
connection_or_send_netinfo(or_connection_t *conn)
{
  cell_t cell;
  time_t now = time(NULL);
  const routerinfo_t *me;
  int len;
  uint8_t *out;

  tor_assert(conn->handshake_state);

  if (conn->handshake_state->sent_netinfo) {
    log_warn(LD_BUG, "Attempted to send an extra netinfo cell on a connection "
             "where we already sent one.");
    return 0;
  }

  memset(&cell, 0, sizeof(cell_t));
  cell.command = CELL_NETINFO;

  /* Timestamp, if we're a relay. */
  if (public_server_mode(get_options()) || ! conn->is_outgoing)
    set_uint32(cell.payload, htonl((uint32_t)now));

  /* Their address. */
  out = cell.payload + 4;
  /* We use &conn->real_addr below, unless it hasn't yet been set. If it
   * hasn't yet been set, we know that base_.addr hasn't been tampered with
   * yet either. */
  len = append_address_to_payload(out, !tor_addr_is_null(&conn->real_addr)
                                       ? &conn->real_addr : &conn->base_.addr);
  if (len<0)
    return -1;
  out += len;

  /* My address -- only include it if I'm a public relay, or if I'm a
   * bridge and this is an incoming connection. If I'm a bridge and this
   * is an outgoing connection, act like a normal client and omit it. */
  if ((public_server_mode(get_options()) || !conn->is_outgoing) &&
      (me = router_get_my_routerinfo())) {
    tor_addr_t my_addr;
    *out++ = 1 + !tor_addr_is_null(&me->ipv6_addr);

    tor_addr_from_ipv4h(&my_addr, me->addr);
    len = append_address_to_payload(out, &my_addr);
    if (len < 0)
      return -1;
    out += len;

    if (!tor_addr_is_null(&me->ipv6_addr)) {
      len = append_address_to_payload(out, &me->ipv6_addr);
      if (len < 0)
        return -1;
    }
  } else {
    *out = 0;
  }

  conn->handshake_state->digest_sent_data = 0;
  conn->handshake_state->sent_netinfo = 1;
  connection_or_write_cell_to_buf(&cell, conn);

  return 0;
}

/** Send a CERTS cell on the connection <b>conn</b>.  Return 0 on success, -1
 * on failure. */
int
connection_or_send_certs_cell(or_connection_t *conn)
{
  const tor_cert_t *link_cert = NULL, *id_cert = NULL;
  const uint8_t *link_encoded = NULL, *id_encoded = NULL;
  size_t link_len, id_len;
  var_cell_t *cell;
  size_t cell_len;
  ssize_t pos;
  int server_mode;

  tor_assert(conn->base_.state == OR_CONN_STATE_OR_HANDSHAKING_V3);

  if (! conn->handshake_state)
    return -1;
  server_mode = ! conn->handshake_state->started_here;
  if (tor_tls_get_my_certs(server_mode, &link_cert, &id_cert) < 0)
    return -1;
  tor_cert_get_der(link_cert, &link_encoded, &link_len);
  tor_cert_get_der(id_cert, &id_encoded, &id_len);

  cell_len = 1 /* 1 byte: num certs in cell */ +
             2 * ( 1 + 2 ) /* For each cert: 1 byte for type, 2 for length */ +
             link_len + id_len;
  cell = var_cell_new(cell_len);
  cell->command = CELL_CERTS;
  cell->payload[0] = 2;
  pos = 1;

  if (server_mode)
    cell->payload[pos] = OR_CERT_TYPE_TLS_LINK; /* Link cert  */
  else
    cell->payload[pos] = OR_CERT_TYPE_AUTH_1024; /* client authentication */
  set_uint16(&cell->payload[pos+1], htons(link_len));
  memcpy(&cell->payload[pos+3], link_encoded, link_len);
  pos += 3 + link_len;

  cell->payload[pos] = OR_CERT_TYPE_ID_1024; /* ID cert */
  set_uint16(&cell->payload[pos+1], htons(id_len));
  memcpy(&cell->payload[pos+3], id_encoded, id_len);
  pos += 3 + id_len;

  tor_assert(pos == (int)cell_len); /* Otherwise we just smashed the heap */

  connection_or_write_var_cell_to_buf(cell, conn);
  var_cell_free(cell);

  return 0;
}

/** Send an AUTH_CHALLENGE cell on the connection <b>conn</b>. Return 0
 * on success, -1 on failure. */
int
connection_or_send_auth_challenge_cell(or_connection_t *conn)
{
  var_cell_t *cell;
  uint8_t *cp;
  uint8_t challenge[OR_AUTH_CHALLENGE_LEN];
  tor_assert(conn->base_.state == OR_CONN_STATE_OR_HANDSHAKING_V3);

  if (! conn->handshake_state)
    return -1;

  if (crypto_rand((char*)challenge, OR_AUTH_CHALLENGE_LEN) < 0)
    return -1;
  cell = var_cell_new(OR_AUTH_CHALLENGE_LEN + 4);
  cell->command = CELL_AUTH_CHALLENGE;
  memcpy(cell->payload, challenge, OR_AUTH_CHALLENGE_LEN);
  cp = cell->payload + OR_AUTH_CHALLENGE_LEN;
  set_uint16(cp, htons(1)); /* We recognize one authentication type. */
  set_uint16(cp+2, htons(AUTHTYPE_RSA_SHA256_TLSSECRET));

  connection_or_write_var_cell_to_buf(cell, conn);
  var_cell_free(cell);
  memwipe(challenge, 0, sizeof(challenge));

  return 0;
}

/** Compute the main body of an AUTHENTICATE cell that a client can use
 * to authenticate itself on a v3 handshake for <b>conn</b>.  Write it to the
 * <b>outlen</b>-byte buffer at <b>out</b>.
 *
 * If <b>server</b> is true, only calculate the first
 * V3_AUTH_FIXED_PART_LEN bytes -- the part of the authenticator that's
 * determined by the rest of the handshake, and which match the provided value
 * exactly.
 *
 * If <b>server</b> is false and <b>signing_key</b> is NULL, calculate the
 * first V3_AUTH_BODY_LEN bytes of the authenticator (that is, everything
 * that should be signed), but don't actually sign it.
 *
 * If <b>server</b> is false and <b>signing_key</b> is provided, calculate the
 * entire authenticator, signed with <b>signing_key</b>.
 *
 * Return the length of the cell body on success, and -1 on failure.
 */
int
connection_or_compute_authenticate_cell_body(or_connection_t *conn,
                                             uint8_t *out, size_t outlen,
                                             crypto_pk_t *signing_key,
                                             int server)
{
  uint8_t *ptr;

  /* assert state is reasonable XXXX */

  if (outlen < V3_AUTH_FIXED_PART_LEN ||
      (!server && outlen < V3_AUTH_BODY_LEN))
    return -1;

  ptr = out;

  /* Type: 8 bytes. */
  memcpy(ptr, "AUTH0001", 8);
  ptr += 8;

  {
    const tor_cert_t *id_cert=NULL, *link_cert=NULL;
    const digests_t *my_digests, *their_digests;
    const uint8_t *my_id, *their_id, *client_id, *server_id;
    if (tor_tls_get_my_certs(server, &link_cert, &id_cert))
      return -1;
    my_digests = tor_cert_get_id_digests(id_cert);
    their_digests = tor_cert_get_id_digests(conn->handshake_state->id_cert);
    tor_assert(my_digests);
    tor_assert(their_digests);
    my_id = (uint8_t*)my_digests->d[DIGEST_SHA256];
    their_id = (uint8_t*)their_digests->d[DIGEST_SHA256];

    client_id = server ? their_id : my_id;
    server_id = server ? my_id : their_id;

    /* Client ID digest: 32 octets. */
    memcpy(ptr, client_id, 32);
    ptr += 32;

    /* Server ID digest: 32 octets. */
    memcpy(ptr, server_id, 32);
    ptr += 32;
  }

  {
    crypto_digest_t *server_d, *client_d;
    if (server) {
      server_d = conn->handshake_state->digest_sent;
      client_d = conn->handshake_state->digest_received;
    } else {
      client_d = conn->handshake_state->digest_sent;
      server_d = conn->handshake_state->digest_received;
    }

    /* Server log digest : 32 octets */
    crypto_digest_get_digest(server_d, (char*)ptr, 32);
    ptr += 32;

    /* Client log digest : 32 octets */
    crypto_digest_get_digest(client_d, (char*)ptr, 32);
    ptr += 32;
  }

  {
    /* Digest of cert used on TLS link : 32 octets. */
    const tor_cert_t *cert = NULL;
    tor_cert_t *freecert = NULL;
    if (server) {
      tor_tls_get_my_certs(1, &cert, NULL);
    } else {
      freecert = tor_tls_get_peer_cert(conn->tls);
      cert = freecert;
    }
    if (!cert)
      return -1;
    memcpy(ptr, tor_cert_get_cert_digests(cert)->d[DIGEST_SHA256], 32);

    if (freecert)
      tor_cert_free(freecert);
    ptr += 32;
  }

  /* HMAC of clientrandom and serverrandom using master key : 32 octets */
  tor_tls_get_tlssecrets(conn->tls, ptr);
  ptr += 32;

  tor_assert(ptr - out == V3_AUTH_FIXED_PART_LEN);

  if (server)
    return V3_AUTH_FIXED_PART_LEN; // ptr-out

  /* 8 octets were reserved for the current time, but we're trying to get out
   * of the habit of sending time around willynilly.  Fortunately, nothing
   * checks it.  That's followed by 16 bytes of nonce. */
  crypto_rand((char*)ptr, 24);
  ptr += 24;

  tor_assert(ptr - out == V3_AUTH_BODY_LEN);

  if (!signing_key)
    return V3_AUTH_BODY_LEN; // ptr - out

  {
    int siglen;
    char d[32];
    crypto_digest256(d, (char*)out, ptr-out, DIGEST_SHA256);
    siglen = crypto_pk_private_sign(signing_key,
                                    (char*)ptr, outlen - (ptr-out),
                                    d, 32);
    if (siglen < 0)
      return -1;

    ptr += siglen;
    tor_assert(ptr <= out+outlen);
    return (int)(ptr - out);
  }
}

/** Send an AUTHENTICATE cell on the connection <b>conn</b>.  Return 0 on
 * success, -1 on failure */
int
connection_or_send_authenticate_cell(or_connection_t *conn, int authtype)
{
  var_cell_t *cell;
  crypto_pk_t *pk = tor_tls_get_my_client_auth_key();
  int authlen;
  size_t cell_maxlen;
  /* XXXX make sure we're actually supposed to send this! */

  if (!pk) {
    log_warn(LD_BUG, "Can't compute authenticate cell: no client auth key");
    return -1;
  }
  if (authtype != AUTHTYPE_RSA_SHA256_TLSSECRET) {
    log_warn(LD_BUG, "Tried to send authenticate cell with unknown "
             "authentication type %d", authtype);
    return -1;
  }

  cell_maxlen = 4 + /* overhead */
    V3_AUTH_BODY_LEN + /* Authentication body */
    crypto_pk_keysize(pk) + /* Max signature length */
    16 /* add a few extra bytes just in case. */;

  cell = var_cell_new(cell_maxlen);
  cell->command = CELL_AUTHENTICATE;
  set_uint16(cell->payload, htons(AUTHTYPE_RSA_SHA256_TLSSECRET));
  /* skip over length ; we don't know that yet. */

  authlen = connection_or_compute_authenticate_cell_body(conn,
                                                         cell->payload+4,
                                                         cell_maxlen-4,
                                                         pk,
                                                         0 /* not server */);
  if (authlen < 0) {
    log_warn(LD_BUG, "Unable to compute authenticate cell!");
    var_cell_free(cell);
    return -1;
  }
  tor_assert(authlen + 4 <= cell->payload_len);
  set_uint16(cell->payload+2, htons(authlen));
  cell->payload_len = authlen + 4;

  connection_or_write_var_cell_to_buf(cell, conn);
  var_cell_free(cell);

  return 0;
}

