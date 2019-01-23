/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file main.c
 * \brief Toplevel module. Handles signals, multiplexes between
 * connections, implements main loop, and drives scheduled events.
 **/

#define MAIN_PRIVATE
#include "or.h"
#include "addressmap.h"
#include "backtrace.h"
#include "buffers.h"
#include "channel.h"
#include "channeltls.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "command.h"
#include "config.h"
#include "confparse.h"
#include "connection.h"
#include "connection_edge.h"
#include "connection_or.h"
#include "control.h"
#include "cpuworker.h"
#include "crypto_s2k.h"
#include "directory.h"
#include "dirserv.h"
#include "dirvote.h"
#include "dns.h"
#include "dnsserv.h"
#include "entrynodes.h"
#include "geoip.h"
#include "hibernate.h"
#include "keypin.h"
#include "main.h"
#include "microdesc.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "ntmain.h"
#include "onion.h"
#include "periodic.h"
#include "policies.h"
#include "protover.h"
#include "transports.h"
#include "relay.h"
#include "rendclient.h"
#include "rendcommon.h"
#include "rendservice.h"
#include "rephist.h"
#include "router.h"
#include "routerkeys.h"
#include "routerlist.h"
#include "routerparse.h"
#include "scheduler.h"
#include "shared_random.h"
#include "statefile.h"
#include "status.h"
#include "util_process.h"
#include "ext_orport.h"
#ifdef USE_DMALLOC
#include <dmalloc.h>
#include <openssl/crypto.h>
#endif
#include "memarea.h"
#include "sandbox.h"

#include <event2/event.h>

#ifdef HAVE_SYSTEMD
#   if defined(__COVERITY__) && !defined(__INCLUDE_LEVEL__)
/* Systemd's use of gcc's __INCLUDE_LEVEL__ extension macro appears to confuse
 * Coverity. Here's a kludge to unconfuse it.
 */
#   define __INCLUDE_LEVEL__ 2
#   endif
#include <systemd/sd-daemon.h>
#endif

void evdns_shutdown(int);

/********* PROTOTYPES **********/

static void dumpmemusage(int severity);
static void dumpstats(int severity); /* log stats */
static void conn_read_callback(evutil_socket_t fd, short event, void *_conn);
static void conn_write_callback(evutil_socket_t fd, short event, void *_conn);
static void second_elapsed_callback(periodic_timer_t *timer, void *args);
static int conn_close_if_marked(int i);
static void connection_start_reading_from_linked_conn(connection_t *conn);
static int connection_should_read_from_linked_conn(connection_t *conn);
static int run_main_loop_until_done(void);
static void process_signal(int sig);

/********* START VARIABLES **********/
int global_read_bucket; /**< Max number of bytes I can read this second. */
int global_write_bucket; /**< Max number of bytes I can write this second. */

/** Max number of relayed (bandwidth class 1) bytes I can read this second. */
int global_relayed_read_bucket;
/** Max number of relayed (bandwidth class 1) bytes I can write this second. */
int global_relayed_write_bucket;
/** What was the read bucket before the last second_elapsed_callback() call?
 * (used to determine how many bytes we've read). */
static int stats_prev_global_read_bucket;
/** What was the write bucket before the last second_elapsed_callback() call?
 * (used to determine how many bytes we've written). */
static int stats_prev_global_write_bucket;

/* DOCDOC stats_prev_n_read */
static uint64_t stats_prev_n_read = 0;
/* DOCDOC stats_prev_n_written */
static uint64_t stats_prev_n_written = 0;

/* XXX we might want to keep stats about global_relayed_*_bucket too. Or not.*/
/** How many bytes have we read since we started the process? */
static uint64_t stats_n_bytes_read = 0;
/** How many bytes have we written since we started the process? */
static uint64_t stats_n_bytes_written = 0;
/** What time did this process start up? */
time_t time_of_process_start = 0;
/** How many seconds have we been running? */
long stats_n_seconds_working = 0;

/** How often will we honor SIGNEWNYM requests? */
#define MAX_SIGNEWNYM_RATE 10
/** When did we last process a SIGNEWNYM request? */
static time_t time_of_last_signewnym = 0;
/** Is there a signewnym request we're currently waiting to handle? */
static int signewnym_is_pending = 0;
/** How many times have we called newnym? */
static unsigned newnym_epoch = 0;

/** Smartlist of all open connections. */
static smartlist_t *connection_array = NULL;
/** List of connections that have been marked for close and need to be freed
 * and removed from connection_array. */
static smartlist_t *closeable_connection_lst = NULL;
/** List of linked connections that are currently reading data into their
 * inbuf from their partner's outbuf. */
static smartlist_t *active_linked_connection_lst = NULL;
/** Flag: Set to true iff we entered the current libevent main loop via
 * <b>loop_once</b>. If so, there's no need to trigger a loopexit in order
 * to handle linked connections. */
static int called_loop_once = 0;

/** We set this to 1 when we've opened a circuit, so we can print a log
 * entry to inform the user that Tor is working.  We set it to 0 when
 * we think the fact that we once opened a circuit doesn't mean we can do so
 * any longer (a big time jump happened, when we notice our directory is
 * heinously out-of-date, etc.
 */
static int can_complete_circuits = 0;

/** How often do we check for router descriptors that we should download
 * when we have too little directory info? */
#define GREEDY_DESCRIPTOR_RETRY_INTERVAL (10)
/** How often do we check for router descriptors that we should download
 * when we have enough directory info? */
#define LAZY_DESCRIPTOR_RETRY_INTERVAL (60)

/** Decides our behavior when no logs are configured/before any
 * logs have been configured.  For 0, we log notice to stdout as normal.
 * For 1, we log warnings only.  For 2, we log nothing.
 */
int quiet_level = 0;

/********* END VARIABLES ************/

/****************************************************************************
 *
 * This section contains accessors and other methods on the connection_array
 * variables (which are global within this file and unavailable outside it).
 *
 ****************************************************************************/

/** Return 1 if we have successfully built a circuit, and nothing has changed
 * to make us think that maybe we can't.
 */
int
have_completed_a_circuit(void)
{
  return can_complete_circuits;
}

/** Note that we have successfully built a circuit, so that reachability
 * testing and introduction points and so on may be attempted. */
void
note_that_we_completed_a_circuit(void)
{
  can_complete_circuits = 1;
}

/** Note that something has happened (like a clock jump, or DisableNetwork) to
 * make us think that maybe we can't complete circuits. */
void
note_that_we_maybe_cant_complete_circuits(void)
{
  can_complete_circuits = 0;
}

/** Add <b>conn</b> to the array of connections that we can poll on.  The
 * connection's socket must be set; the connection starts out
 * non-reading and non-writing.
 */
int
connection_add_impl(connection_t *conn, int is_connecting)
{
  tor_assert(conn);
  tor_assert(SOCKET_OK(conn->s) ||
             conn->linked ||
             (conn->type == CONN_TYPE_AP &&
              TO_EDGE_CONN(conn)->is_dns_request));

  tor_assert(conn->conn_array_index == -1); /* can only connection_add once */
  conn->conn_array_index = smartlist_len(connection_array);
  smartlist_add(connection_array, conn);

  (void) is_connecting;

  if (SOCKET_OK(conn->s) || conn->linked) {
    conn->read_event = tor_event_new(tor_libevent_get_base(),
         conn->s, EV_READ|EV_PERSIST, conn_read_callback, conn);
    conn->write_event = tor_event_new(tor_libevent_get_base(),
         conn->s, EV_WRITE|EV_PERSIST, conn_write_callback, conn);
    /* XXXX CHECK FOR NULL RETURN! */
  }

  log_debug(LD_NET,"new conn type %s, socket %d, address %s, n_conns %d.",
            conn_type_to_string(conn->type), (int)conn->s, conn->address,
            smartlist_len(connection_array));

  return 0;
}

/** Tell libevent that we don't care about <b>conn</b> any more. */
void
connection_unregister_events(connection_t *conn)
{
  if (conn->read_event) {
    if (event_del(conn->read_event))
      log_warn(LD_BUG, "Error removing read event for %d", (int)conn->s);
    tor_free(conn->read_event);
  }
  if (conn->write_event) {
    if (event_del(conn->write_event))
      log_warn(LD_BUG, "Error removing write event for %d", (int)conn->s);
    tor_free(conn->write_event);
  }
  if (conn->type == CONN_TYPE_AP_DNS_LISTENER) {
    dnsserv_close_listener(conn);
  }
}

/** Remove the connection from the global list, and remove the
 * corresponding poll entry.  Calling this function will shift the last
 * connection (if any) into the position occupied by conn.
 */
int
connection_remove(connection_t *conn)
{
  int current_index;
  connection_t *tmp;

  tor_assert(conn);

  log_debug(LD_NET,"removing socket %d (type %s), n_conns now %d",
            (int)conn->s, conn_type_to_string(conn->type),
            smartlist_len(connection_array));

  if (conn->type == CONN_TYPE_AP && conn->socket_family == AF_UNIX) {
    log_info(LD_NET, "Closing SOCKS SocksSocket connection");
  }

  control_event_conn_bandwidth(conn);

  tor_assert(conn->conn_array_index >= 0);
  current_index = conn->conn_array_index;
  connection_unregister_events(conn); /* This is redundant, but cheap. */
  if (current_index == smartlist_len(connection_array)-1) { /* at the end */
    smartlist_del(connection_array, current_index);
    return 0;
  }

  /* replace this one with the one at the end */
  smartlist_del(connection_array, current_index);
  tmp = smartlist_get(connection_array, current_index);
  tmp->conn_array_index = current_index;

  return 0;
}

/** If <b>conn</b> is an edge conn, remove it from the list
 * of conn's on this circuit. If it's not on an edge,
 * flush and send destroys for all circuits on this conn.
 *
 * Remove it from connection_array (if applicable) and
 * from closeable_connection_list.
 *
 * Then free it.
 */
static void
connection_unlink(connection_t *conn)
{
  connection_about_to_close_connection(conn);
  if (conn->conn_array_index >= 0) {
    connection_remove(conn);
  }
  if (conn->linked_conn) {
    conn->linked_conn->linked_conn = NULL;
    if (! conn->linked_conn->marked_for_close &&
        conn->linked_conn->reading_from_linked_conn)
      connection_start_reading(conn->linked_conn);
    conn->linked_conn = NULL;
  }
  smartlist_remove(closeable_connection_lst, conn);
  smartlist_remove(active_linked_connection_lst, conn);
  if (conn->type == CONN_TYPE_EXIT) {
    assert_connection_edge_not_dns_pending(TO_EDGE_CONN(conn));
  }
  if (conn->type == CONN_TYPE_OR) {
    if (!tor_digest_is_zero(TO_OR_CONN(conn)->identity_digest))
      connection_or_remove_from_identity_map(TO_OR_CONN(conn));
    /* connection_unlink() can only get called if the connection
     * was already on the closeable list, and it got there by
     * connection_mark_for_close(), which was called from
     * connection_or_close_normally() or
     * connection_or_close_for_error(), so the channel should
     * already be in CHANNEL_STATE_CLOSING, and then the
     * connection_about_to_close_connection() goes to
     * connection_or_about_to_close(), which calls channel_closed()
     * to notify the channel_t layer, and closed the channel, so
     * nothing more to do here to deal with the channel associated
     * with an orconn.
     */
  }
  connection_free(conn);
}

/** Initialize the global connection list, closeable connection list,
 * and active connection list. */
STATIC void
init_connection_lists(void)
{
  if (!connection_array)
    connection_array = smartlist_new();
  if (!closeable_connection_lst)
    closeable_connection_lst = smartlist_new();
  if (!active_linked_connection_lst)
    active_linked_connection_lst = smartlist_new();
}

/** Schedule <b>conn</b> to be closed. **/
void
add_connection_to_closeable_list(connection_t *conn)
{
  tor_assert(!smartlist_contains(closeable_connection_lst, conn));
  tor_assert(conn->marked_for_close);
  assert_connection_ok(conn, time(NULL));
  smartlist_add(closeable_connection_lst, conn);
}

/** Return 1 if conn is on the closeable list, else return 0. */
int
connection_is_on_closeable_list(connection_t *conn)
{
  return smartlist_contains(closeable_connection_lst, conn);
}

/** Return true iff conn is in the current poll array. */
int
connection_in_array(connection_t *conn)
{
  return smartlist_contains(connection_array, conn);
}

/** Set <b>*array</b> to an array of all connections. <b>*array</b> must not
 * be modified.
 */
MOCK_IMPL(smartlist_t *,
get_connection_array, (void))
{
  if (!connection_array)
    connection_array = smartlist_new();
  return connection_array;
}

/** Provides the traffic read and written over the life of the process. */

MOCK_IMPL(uint64_t,
get_bytes_read,(void))
{
  return stats_n_bytes_read;
}

/* DOCDOC get_bytes_written */
MOCK_IMPL(uint64_t,
get_bytes_written,(void))
{
  return stats_n_bytes_written;
}

/** Set the event mask on <b>conn</b> to <b>events</b>.  (The event
 * mask is a bitmask whose bits are READ_EVENT and WRITE_EVENT)
 */
void
connection_watch_events(connection_t *conn, watchable_events_t events)
{
  if (events & READ_EVENT)
    connection_start_reading(conn);
  else
    connection_stop_reading(conn);

  if (events & WRITE_EVENT)
    connection_start_writing(conn);
  else
    connection_stop_writing(conn);
}

/** Return true iff <b>conn</b> is listening for read events. */
int
connection_is_reading(connection_t *conn)
{
  tor_assert(conn);

  return conn->reading_from_linked_conn ||
    (conn->read_event && event_pending(conn->read_event, EV_READ, NULL));
}

/** Check whether <b>conn</b> is correct in having (or not having) a
 * read/write event (passed in <b>ev</b>). On success, return 0. On failure,
 * log a warning and return -1. */
static int
connection_check_event(connection_t *conn, struct event *ev)
{
  int bad;

  if (conn->type == CONN_TYPE_AP && TO_EDGE_CONN(conn)->is_dns_request) {
    /* DNS requests which we launch through the dnsserv.c module do not have
     * any underlying socket or any underlying linked connection, so they
     * shouldn't have any attached events either.
     */
    bad = ev != NULL;
  } else {
    /* Everytyhing else should have an underlying socket, or a linked
     * connection (which is also tracked with a read_event/write_event pair).
     */
    bad = ev == NULL;
  }

  if (bad) {
    log_warn(LD_BUG, "Event missing on connection %p [%s;%s]. "
             "socket=%d. linked=%d. "
             "is_dns_request=%d. Marked_for_close=%s:%d",
             conn,
             conn_type_to_string(conn->type),
             conn_state_to_string(conn->type, conn->state),
             (int)conn->s, (int)conn->linked,
             (conn->type == CONN_TYPE_AP &&
                               TO_EDGE_CONN(conn)->is_dns_request),
             conn->marked_for_close_file ? conn->marked_for_close_file : "-",
             conn->marked_for_close
             );
    log_backtrace(LOG_WARN, LD_BUG, "Backtrace attached.");
    return -1;
  }
  return 0;
}

/** Tell the main loop to stop notifying <b>conn</b> of any read events. */
MOCK_IMPL(void,
connection_stop_reading,(connection_t *conn))
{
  tor_assert(conn);

  if (connection_check_event(conn, conn->read_event) < 0) {
    return;
  }

  if (conn->linked) {
    conn->reading_from_linked_conn = 0;
    connection_stop_reading_from_linked_conn(conn);
  } else {
    if (event_del(conn->read_event))
      log_warn(LD_NET, "Error from libevent setting read event state for %d "
               "to unwatched: %s",
               (int)conn->s,
               tor_socket_strerror(tor_socket_errno(conn->s)));
  }
}

/** Tell the main loop to start notifying <b>conn</b> of any read events. */
MOCK_IMPL(void,
connection_start_reading,(connection_t *conn))
{
  tor_assert(conn);

  if (connection_check_event(conn, conn->read_event) < 0) {
    return;
  }

  if (conn->linked) {
    conn->reading_from_linked_conn = 1;
    if (connection_should_read_from_linked_conn(conn))
      connection_start_reading_from_linked_conn(conn);
  } else {
    if (event_add(conn->read_event, NULL))
      log_warn(LD_NET, "Error from libevent setting read event state for %d "
               "to watched: %s",
               (int)conn->s,
               tor_socket_strerror(tor_socket_errno(conn->s)));
  }
}

/** Return true iff <b>conn</b> is listening for write events. */
int
connection_is_writing(connection_t *conn)
{
  tor_assert(conn);

  return conn->writing_to_linked_conn ||
    (conn->write_event && event_pending(conn->write_event, EV_WRITE, NULL));
}

/** Tell the main loop to stop notifying <b>conn</b> of any write events. */
MOCK_IMPL(void,
connection_stop_writing,(connection_t *conn))
{
  tor_assert(conn);

  if (connection_check_event(conn, conn->write_event) < 0) {
    return;
  }

  if (conn->linked) {
    conn->writing_to_linked_conn = 0;
    if (conn->linked_conn)
      connection_stop_reading_from_linked_conn(conn->linked_conn);
  } else {
    if (event_del(conn->write_event))
      log_warn(LD_NET, "Error from libevent setting write event state for %d "
               "to unwatched: %s",
               (int)conn->s,
               tor_socket_strerror(tor_socket_errno(conn->s)));
  }
}

/** Tell the main loop to start notifying <b>conn</b> of any write events. */
MOCK_IMPL(void,
connection_start_writing,(connection_t *conn))
{
  tor_assert(conn);

  if (connection_check_event(conn, conn->write_event) < 0) {
    return;
  }

  if (conn->linked) {
    conn->writing_to_linked_conn = 1;
    if (conn->linked_conn &&
        connection_should_read_from_linked_conn(conn->linked_conn))
      connection_start_reading_from_linked_conn(conn->linked_conn);
  } else {
    if (event_add(conn->write_event, NULL))
      log_warn(LD_NET, "Error from libevent setting write event state for %d "
               "to watched: %s",
               (int)conn->s,
               tor_socket_strerror(tor_socket_errno(conn->s)));
  }
}

/** Return true iff <b>conn</b> is linked conn, and reading from the conn
 * linked to it would be good and feasible.  (Reading is "feasible" if the
 * other conn exists and has data in its outbuf, and is "good" if we have our
 * reading_from_linked_conn flag set and the other conn has its
 * writing_to_linked_conn flag set.)*/
static int
connection_should_read_from_linked_conn(connection_t *conn)
{
  if (conn->linked && conn->reading_from_linked_conn) {
    if (! conn->linked_conn ||
        (conn->linked_conn->writing_to_linked_conn &&
         buf_datalen(conn->linked_conn->outbuf)))
      return 1;
  }
  return 0;
}

/** If we called event_base_loop() and told it to never stop until it
 * runs out of events, now we've changed our mind: tell it we want it to
 * finish. */
void
tell_event_loop_to_finish(void)
{
  if (!called_loop_once) {
    struct timeval tv = { 0, 0 };
    tor_event_base_loopexit(tor_libevent_get_base(), &tv);
    called_loop_once = 1; /* hack to avoid adding more exit events */
  }
}

/** Helper: Tell the main loop to begin reading bytes into <b>conn</b> from
 * its linked connection, if it is not doing so already.  Called by
 * connection_start_reading and connection_start_writing as appropriate. */
static void
connection_start_reading_from_linked_conn(connection_t *conn)
{
  tor_assert(conn);
  tor_assert(conn->linked == 1);

  if (!conn->active_on_link) {
    conn->active_on_link = 1;
    smartlist_add(active_linked_connection_lst, conn);
    /* make sure that the event_base_loop() function exits at
     * the end of its run through the current connections, so we can
     * activate read events for linked connections. */
    tell_event_loop_to_finish();
  } else {
    tor_assert(smartlist_contains(active_linked_connection_lst, conn));
  }
}

/** Tell the main loop to stop reading bytes into <b>conn</b> from its linked
 * connection, if is currently doing so.  Called by connection_stop_reading,
 * connection_stop_writing, and connection_read. */
void
connection_stop_reading_from_linked_conn(connection_t *conn)
{
  tor_assert(conn);
  tor_assert(conn->linked == 1);

  if (conn->active_on_link) {
    conn->active_on_link = 0;
    /* FFFF We could keep an index here so we can smartlist_del
     * cleanly.  On the other hand, this doesn't show up on profiles,
     * so let's leave it alone for now. */
    smartlist_remove(active_linked_connection_lst, conn);
  } else {
    tor_assert(!smartlist_contains(active_linked_connection_lst, conn));
  }
}

/** Close all connections that have been scheduled to get closed. */
STATIC void
close_closeable_connections(void)
{
  int i;
  for (i = 0; i < smartlist_len(closeable_connection_lst); ) {
    connection_t *conn = smartlist_get(closeable_connection_lst, i);
    if (conn->conn_array_index < 0) {
      connection_unlink(conn); /* blow it away right now */
    } else {
      if (!conn_close_if_marked(conn->conn_array_index))
        ++i;
    }
  }
}

/** Count moribund connections for the OOS handler */
MOCK_IMPL(int,
connection_count_moribund, (void))
{
  int moribund = 0;

  /*
   * Count things we'll try to kill when close_closeable_connections()
   * runs next.
   */
  SMARTLIST_FOREACH_BEGIN(closeable_connection_lst, connection_t *, conn) {
    if (SOCKET_OK(conn->s) && connection_is_moribund(conn)) ++moribund;
  } SMARTLIST_FOREACH_END(conn);

  return moribund;
}

/** Libevent callback: this gets invoked when (connection_t*)<b>conn</b> has
 * some data to read. */
static void
conn_read_callback(evutil_socket_t fd, short event, void *_conn)
{
  connection_t *conn = _conn;
  (void)fd;
  (void)event;

  log_debug(LD_NET,"socket %d wants to read.",(int)conn->s);

  /* assert_connection_ok(conn, time(NULL)); */

  if (connection_handle_read(conn) < 0) {
    if (!conn->marked_for_close) {
#ifndef _WIN32
      log_warn(LD_BUG,"Unhandled error on read for %s connection "
               "(fd %d); removing",
               conn_type_to_string(conn->type), (int)conn->s);
      tor_fragile_assert();
#endif
      if (CONN_IS_EDGE(conn))
        connection_edge_end_errno(TO_EDGE_CONN(conn));
      connection_mark_for_close(conn);
    }
  }
  assert_connection_ok(conn, time(NULL));

  if (smartlist_len(closeable_connection_lst))
    close_closeable_connections();
}

/** Libevent callback: this gets invoked when (connection_t*)<b>conn</b> has
 * some data to write. */
static void
conn_write_callback(evutil_socket_t fd, short events, void *_conn)
{
  connection_t *conn = _conn;
  (void)fd;
  (void)events;

  LOG_FN_CONN(conn, (LOG_DEBUG, LD_NET, "socket %d wants to write.",
                     (int)conn->s));

  /* assert_connection_ok(conn, time(NULL)); */

  if (connection_handle_write(conn, 0) < 0) {
    if (!conn->marked_for_close) {
      /* this connection is broken. remove it. */
      log_fn(LOG_WARN,LD_BUG,
             "unhandled error on write for %s connection (fd %d); removing",
             conn_type_to_string(conn->type), (int)conn->s);
      tor_fragile_assert();
      if (CONN_IS_EDGE(conn)) {
        /* otherwise we cry wolf about duplicate close */
        edge_connection_t *edge_conn = TO_EDGE_CONN(conn);
        if (!edge_conn->end_reason)
          edge_conn->end_reason = END_STREAM_REASON_INTERNAL;
        edge_conn->edge_has_sent_end = 1;
      }
      connection_close_immediate(conn); /* So we don't try to flush. */
      connection_mark_for_close(conn);
    }
  }
  assert_connection_ok(conn, time(NULL));

  if (smartlist_len(closeable_connection_lst))
    close_closeable_connections();
}

/** If the connection at connection_array[i] is marked for close, then:
 *    - If it has data that it wants to flush, try to flush it.
 *    - If it _still_ has data to flush, and conn->hold_open_until_flushed is
 *      true, then leave the connection open and return.
 *    - Otherwise, remove the connection from connection_array and from
 *      all other lists, close it, and free it.
 * Returns 1 if the connection was closed, 0 otherwise.
 */
static int
conn_close_if_marked(int i)
{
  connection_t *conn;
  int retval;
  time_t now;

  conn = smartlist_get(connection_array, i);
  if (!conn->marked_for_close)
    return 0; /* nothing to see here, move along */
  now = time(NULL);
  assert_connection_ok(conn, now);
  /* assert_all_pending_dns_resolves_ok(); */

  log_debug(LD_NET,"Cleaning up connection (fd "TOR_SOCKET_T_FORMAT").",
            conn->s);

  /* If the connection we are about to close was trying to connect to
  a proxy server and failed, the client won't be able to use that
  proxy. We should warn the user about this. */
  if (conn->proxy_state == PROXY_INFANT)
    log_failed_proxy_connection(conn);

  if ((SOCKET_OK(conn->s) || conn->linked_conn) &&
      connection_wants_to_flush(conn)) {
    /* s == -1 means it's an incomplete edge connection, or that the socket
     * has already been closed as unflushable. */
    ssize_t sz = connection_bucket_write_limit(conn, now);
    if (!conn->hold_open_until_flushed)
      log_info(LD_NET,
               "Conn (addr %s, fd %d, type %s, state %d) marked, but wants "
               "to flush %d bytes. (Marked at %s:%d)",
               escaped_safe_str_client(conn->address),
               (int)conn->s, conn_type_to_string(conn->type), conn->state,
               (int)conn->outbuf_flushlen,
                conn->marked_for_close_file, conn->marked_for_close);
    if (conn->linked_conn) {
      retval = move_buf_to_buf(conn->linked_conn->inbuf, conn->outbuf,
                               &conn->outbuf_flushlen);
      if (retval >= 0) {
        /* The linked conn will notice that it has data when it notices that
         * we're gone. */
        connection_start_reading_from_linked_conn(conn->linked_conn);
      }
      log_debug(LD_GENERAL, "Flushed last %d bytes from a linked conn; "
               "%d left; flushlen %d; wants-to-flush==%d", retval,
                (int)connection_get_outbuf_len(conn),
                (int)conn->outbuf_flushlen,
                connection_wants_to_flush(conn));
    } else if (connection_speaks_cells(conn)) {
      if (conn->state == OR_CONN_STATE_OPEN) {
        retval = flush_buf_tls(TO_OR_CONN(conn)->tls, conn->outbuf, sz,
                               &conn->outbuf_flushlen);
      } else
        retval = -1; /* never flush non-open broken tls connections */
    } else {
      retval = flush_buf(conn->s, conn->outbuf, sz, &conn->outbuf_flushlen);
    }
    if (retval >= 0 && /* Technically, we could survive things like
                          TLS_WANT_WRITE here. But don't bother for now. */
        conn->hold_open_until_flushed && connection_wants_to_flush(conn)) {
      if (retval > 0) {
        LOG_FN_CONN(conn, (LOG_INFO,LD_NET,
                           "Holding conn (fd %d) open for more flushing.",
                           (int)conn->s));
        conn->timestamp_lastwritten = now; /* reset so we can flush more */
      } else if (sz == 0) {
        /* Also, retval==0.  If we get here, we didn't want to write anything
         * (because of rate-limiting) and we didn't. */

        /* Connection must flush before closing, but it's being rate-limited.
         * Let's remove from Libevent, and mark it as blocked on bandwidth
         * so it will be re-added on next token bucket refill. Prevents
         * busy Libevent loops where we keep ending up here and returning
         * 0 until we are no longer blocked on bandwidth.
         */
        if (connection_is_writing(conn)) {
          conn->write_blocked_on_bw = 1;
          connection_stop_writing(conn);
        }
        if (connection_is_reading(conn)) {
          /* XXXX+ We should make this code unreachable; if a connection is
           * marked for close and flushing, there is no point in reading to it
           * at all. Further, checking at this point is a bit of a hack: it
           * would make much more sense to react in
           * connection_handle_read_impl, or to just stop reading in
           * mark_and_flush */
          conn->read_blocked_on_bw = 1;
          connection_stop_reading(conn);
        }
      }
      return 0;
    }
    if (connection_wants_to_flush(conn)) {
      log_fn(LOG_INFO, LD_NET, "We stalled too much while trying to write %d "
             "bytes to address %s.  If this happens a lot, either "
             "something is wrong with your network connection, or "
             "something is wrong with theirs. "
             "(fd %d, type %s, state %d, marked at %s:%d).",
             (int)connection_get_outbuf_len(conn),
             escaped_safe_str_client(conn->address),
             (int)conn->s, conn_type_to_string(conn->type), conn->state,
             conn->marked_for_close_file,
             conn->marked_for_close);
    }
  }

  connection_unlink(conn); /* unlink, remove, free */
  return 1;
}

/** Implementation for directory_all_unreachable.  This is done in a callback,
 * since otherwise it would complicate Tor's control-flow graph beyond all
 * reason.
 */
static void
directory_all_unreachable_cb(evutil_socket_t fd, short event, void *arg)
{
  (void)fd;
  (void)event;
  (void)arg;

  connection_t *conn;

  while ((conn = connection_get_by_type_state(CONN_TYPE_AP,
                                              AP_CONN_STATE_CIRCUIT_WAIT))) {
    entry_connection_t *entry_conn = TO_ENTRY_CONN(conn);
    log_notice(LD_NET,
               "Is your network connection down? "
               "Failing connection to '%s:%d'.",
               safe_str_client(entry_conn->socks_request->address),
               entry_conn->socks_request->port);
    connection_mark_unattached_ap(entry_conn,
                                  END_STREAM_REASON_NET_UNREACHABLE);
  }
  control_event_general_error("DIR_ALL_UNREACHABLE");
}

static struct event *directory_all_unreachable_cb_event = NULL;

/** We've just tried every dirserver we know about, and none of
 * them were reachable. Assume the network is down. Change state
 * so next time an application connection arrives we'll delay it
 * and try another directory fetch. Kill off all the circuit_wait
 * streams that are waiting now, since they will all timeout anyway.
 */
void
directory_all_unreachable(time_t now)
{
  (void)now;

  stats_n_seconds_working=0; /* reset it */

  if (!directory_all_unreachable_cb_event) {
    directory_all_unreachable_cb_event =
      tor_event_new(tor_libevent_get_base(),
                    -1, EV_READ, directory_all_unreachable_cb, NULL);
    tor_assert(directory_all_unreachable_cb_event);
  }

  event_active(directory_all_unreachable_cb_event, EV_READ, 1);
}

/** This function is called whenever we successfully pull down some new
 * network statuses or server descriptors. */
void
directory_info_has_arrived(time_t now, int from_cache, int suppress_logs)
{
  const or_options_t *options = get_options();

  if (!router_have_minimum_dir_info()) {
    int quiet = suppress_logs || from_cache ||
                directory_too_idle_to_fetch_descriptors(options, now);
    tor_log(quiet ? LOG_INFO : LOG_NOTICE, LD_DIR,
        "I learned some more directory information, but not enough to "
        "build a circuit: %s", get_dir_info_status_string());
    update_all_descriptor_downloads(now);
    return;
  } else {
    if (directory_fetches_from_authorities(options)) {
      update_all_descriptor_downloads(now);
    }

    /* if we have enough dir info, then update our guard status with
     * whatever we just learned. */
    entry_guards_compute_status(options, now);
    /* Don't even bother trying to get extrainfo until the rest of our
     * directory info is up-to-date */
    if (options->DownloadExtraInfo)
      update_extrainfo_downloads(now);
  }

  if (server_mode(options) && !net_is_disabled() && !from_cache &&
      (have_completed_a_circuit() || !any_predicted_circuits(now)))
    consider_testing_reachability(1, 1);
}

/** Perform regular maintenance tasks for a single connection.  This
 * function gets run once per second per connection by run_scheduled_events.
 */
static void
run_connection_housekeeping(int i, time_t now)
{
  cell_t cell;
  connection_t *conn = smartlist_get(connection_array, i);
  const or_options_t *options = get_options();
  or_connection_t *or_conn;
  channel_t *chan = NULL;
  int have_any_circuits;
  int past_keepalive =
    now >= conn->timestamp_lastwritten + options->KeepalivePeriod;

  if (conn->outbuf && !connection_get_outbuf_len(conn) &&
      conn->type == CONN_TYPE_OR)
    TO_OR_CONN(conn)->timestamp_lastempty = now;

  if (conn->marked_for_close) {
    /* nothing to do here */
    return;
  }

  /* Expire any directory connections that haven't been active (sent
   * if a server or received if a client) for 5 min */
  if (conn->type == CONN_TYPE_DIR &&
      ((DIR_CONN_IS_SERVER(conn) &&
        conn->timestamp_lastwritten
            + options->TestingDirConnectionMaxStall < now) ||
       (!DIR_CONN_IS_SERVER(conn) &&
        conn->timestamp_lastread
            + options->TestingDirConnectionMaxStall < now))) {
    log_info(LD_DIR,"Expiring wedged directory conn (fd %d, purpose %d)",
             (int)conn->s, conn->purpose);
    /* This check is temporary; it's to let us know whether we should consider
     * parsing partial serverdesc responses. */
    if (conn->purpose == DIR_PURPOSE_FETCH_SERVERDESC &&
        connection_get_inbuf_len(conn) >= 1024) {
      log_info(LD_DIR,"Trying to extract information from wedged server desc "
               "download.");
      connection_dir_reached_eof(TO_DIR_CONN(conn));
    } else {
      connection_mark_for_close(conn);
    }
    return;
  }

  if (!connection_speaks_cells(conn))
    return; /* we're all done here, the rest is just for OR conns */

  /* If we haven't written to an OR connection for a while, then either nuke
     the connection or send a keepalive, depending. */

  or_conn = TO_OR_CONN(conn);
  tor_assert(conn->outbuf);

  chan = TLS_CHAN_TO_BASE(or_conn->chan);
  tor_assert(chan);

  if (channel_num_circuits(chan) != 0) {
    have_any_circuits = 1;
    chan->timestamp_last_had_circuits = now;
  } else {
    have_any_circuits = 0;
  }

  if (channel_is_bad_for_new_circs(TLS_CHAN_TO_BASE(or_conn->chan)) &&
      ! have_any_circuits) {
    /* It's bad for new circuits, and has no unmarked circuits on it:
     * mark it now. */
    log_info(LD_OR,
             "Expiring non-used OR connection to fd %d (%s:%d) [Too old].",
             (int)conn->s, conn->address, conn->port);
    if (conn->state == OR_CONN_STATE_CONNECTING)
      connection_or_connect_failed(TO_OR_CONN(conn),
                                   END_OR_CONN_REASON_TIMEOUT,
                                   "Tor gave up on the connection");
    connection_or_close_normally(TO_OR_CONN(conn), 1);
  } else if (!connection_state_is_open(conn)) {
    if (past_keepalive) {
      /* We never managed to actually get this connection open and happy. */
      log_info(LD_OR,"Expiring non-open OR connection to fd %d (%s:%d).",
               (int)conn->s,conn->address, conn->port);
      connection_or_close_normally(TO_OR_CONN(conn), 0);
    }
  } else if (we_are_hibernating() &&
             ! have_any_circuits &&
             !connection_get_outbuf_len(conn)) {
    /* We're hibernating, there's no circuits, and nothing to flush.*/
    log_info(LD_OR,"Expiring non-used OR connection to fd %d (%s:%d) "
             "[Hibernating or exiting].",
             (int)conn->s,conn->address, conn->port);
    connection_or_close_normally(TO_OR_CONN(conn), 1);
  } else if (!have_any_circuits &&
             now - or_conn->idle_timeout >=
                                         chan->timestamp_last_had_circuits) {
    log_info(LD_OR,"Expiring non-used OR connection to fd %d (%s:%d) "
             "[no circuits for %d; timeout %d; %scanonical].",
             (int)conn->s, conn->address, conn->port,
             (int)(now - chan->timestamp_last_had_circuits),
             or_conn->idle_timeout,
             or_conn->is_canonical ? "" : "non");
    connection_or_close_normally(TO_OR_CONN(conn), 0);
  } else if (
      now >= or_conn->timestamp_lastempty + options->KeepalivePeriod*10 &&
      now >= conn->timestamp_lastwritten + options->KeepalivePeriod*10) {
    log_fn(LOG_PROTOCOL_WARN,LD_PROTOCOL,
           "Expiring stuck OR connection to fd %d (%s:%d). (%d bytes to "
           "flush; %d seconds since last write)",
           (int)conn->s, conn->address, conn->port,
           (int)connection_get_outbuf_len(conn),
           (int)(now-conn->timestamp_lastwritten));
    connection_or_close_normally(TO_OR_CONN(conn), 0);
  } else if (past_keepalive && !connection_get_outbuf_len(conn)) {
    /* send a padding cell */
    log_fn(LOG_DEBUG,LD_OR,"Sending keepalive to (%s:%d)",
           conn->address, conn->port);
    memset(&cell,0,sizeof(cell_t));
    cell.command = CELL_PADDING;
    connection_or_write_cell_to_buf(&cell, or_conn);
  }
}

/** Honor a NEWNYM request: make future requests unlinkable to past
 * requests. */
static void
signewnym_impl(time_t now)
{
  const or_options_t *options = get_options();
  if (!proxy_mode(options)) {
    log_info(LD_CONTROL, "Ignoring SIGNAL NEWNYM because client functionality "
             "is disabled.");
    return;
  }

  circuit_mark_all_dirty_circs_as_unusable();
  addressmap_clear_transient();
  rend_client_purge_state();
  time_of_last_signewnym = now;
  signewnym_is_pending = 0;

  ++newnym_epoch;

  control_event_signal(SIGNEWNYM);
}

/** Return the number of times that signewnym has been called. */
unsigned
get_signewnym_epoch(void)
{
  return newnym_epoch;
}

/** True iff we have initialized all the members of <b>periodic_events</b>.
 * Used to prevent double-initialization. */
static int periodic_events_initialized = 0;

/* Declare all the timer callback functions... */
#undef CALLBACK
#define CALLBACK(name) \
  static int name ## _callback(time_t, const or_options_t *)
CALLBACK(rotate_onion_key);
CALLBACK(check_ed_keys);
CALLBACK(launch_descriptor_fetches);
CALLBACK(rotate_x509_certificate);
CALLBACK(add_entropy);
CALLBACK(launch_reachability_tests);
CALLBACK(downrate_stability);
CALLBACK(save_stability);
CALLBACK(check_authority_cert);
CALLBACK(check_expired_networkstatus);
CALLBACK(write_stats_file);
CALLBACK(record_bridge_stats);
CALLBACK(clean_caches);
CALLBACK(rend_cache_failure_clean);
CALLBACK(retry_dns);
CALLBACK(check_descriptor);
CALLBACK(check_for_reachability_bw);
CALLBACK(fetch_networkstatus);
CALLBACK(retry_listeners);
CALLBACK(expire_old_ciruits_serverside);
CALLBACK(check_dns_honesty);
CALLBACK(write_bridge_ns);
CALLBACK(check_fw_helper_app);
CALLBACK(heartbeat);

#undef CALLBACK

/* Now we declare an array of periodic_event_item_t for each periodic event */
#define CALLBACK(name) PERIODIC_EVENT(name)

static periodic_event_item_t periodic_events[] = {
  CALLBACK(rotate_onion_key),
  CALLBACK(check_ed_keys),
  CALLBACK(launch_descriptor_fetches),
  CALLBACK(rotate_x509_certificate),
  CALLBACK(add_entropy),
  CALLBACK(launch_reachability_tests),
  CALLBACK(downrate_stability),
  CALLBACK(save_stability),
  CALLBACK(check_authority_cert),
  CALLBACK(check_expired_networkstatus),
  CALLBACK(write_stats_file),
  CALLBACK(record_bridge_stats),
  CALLBACK(clean_caches),
  CALLBACK(rend_cache_failure_clean),
  CALLBACK(retry_dns),
  CALLBACK(check_descriptor),
  CALLBACK(check_for_reachability_bw),
  CALLBACK(fetch_networkstatus),
  CALLBACK(retry_listeners),
  CALLBACK(expire_old_ciruits_serverside),
  CALLBACK(check_dns_honesty),
  CALLBACK(write_bridge_ns),
  CALLBACK(check_fw_helper_app),
  CALLBACK(heartbeat),
  END_OF_PERIODIC_EVENTS
};
#undef CALLBACK

/* These are pointers to members of periodic_events[] that are used to
 * implement particular callbacks.  We keep them separate here so that we
 * can access them by name.  We also keep them inside periodic_events[]
 * so that we can implement "reset all timers" in a reasonable way. */
static periodic_event_item_t *check_descriptor_event=NULL;
static periodic_event_item_t *fetch_networkstatus_event=NULL;
static periodic_event_item_t *launch_descriptor_fetches_event=NULL;
static periodic_event_item_t *check_dns_honesty_event=NULL;

/** Reset all the periodic events so we'll do all our actions again as if we
 * just started up.
 * Useful if our clock just moved back a long time from the future,
 * so we don't wait until that future arrives again before acting.
 */
void
reset_all_main_loop_timers(void)
{
  int i;
  for (i = 0; periodic_events[i].name; ++i) {
    periodic_event_reschedule(&periodic_events[i]);
  }
}

/** Return the member of periodic_events[] whose name is <b>name</b>.
 * Return NULL if no such event is found.
 */
static periodic_event_item_t *
find_periodic_event(const char *name)
{
  int i;
  for (i = 0; periodic_events[i].name; ++i) {
    if (strcmp(name, periodic_events[i].name) == 0)
      return &periodic_events[i];
  }
  return NULL;
}

/** Helper, run one second after setup:
 * Initializes all members of periodic_events and starts them running.
 *
 * (We do this one second after setup for backward-compatibility reasons;
 * it might not actually be necessary.) */
static void
initialize_periodic_events_cb(evutil_socket_t fd, short events, void *data)
{
  (void) fd;
  (void) events;
  (void) data;
  int i;
  for (i = 0; periodic_events[i].name; ++i) {
    periodic_event_launch(&periodic_events[i]);
  }
}

/** Set up all the members of periodic_events[], and configure them all to be
 * launched from a callback. */
STATIC void
initialize_periodic_events(void)
{
  tor_assert(periodic_events_initialized == 0);
  periodic_events_initialized = 1;

  int i;
  for (i = 0; periodic_events[i].name; ++i) {
    periodic_event_setup(&periodic_events[i]);
  }

#define NAMED_CALLBACK(name) \
  STMT_BEGIN name ## _event = find_periodic_event( #name ); STMT_END

  NAMED_CALLBACK(check_descriptor);
  NAMED_CALLBACK(fetch_networkstatus);
  NAMED_CALLBACK(launch_descriptor_fetches);
  NAMED_CALLBACK(check_dns_honesty);

  struct timeval one_second = { 1, 0 };
  event_base_once(tor_libevent_get_base(), -1, EV_TIMEOUT,
                  initialize_periodic_events_cb, NULL,
                  &one_second);
}

STATIC void
teardown_periodic_events(void)
{
  int i;
  for (i = 0; periodic_events[i].name; ++i) {
    periodic_event_destroy(&periodic_events[i]);
  }
}

/**
 * Update our schedule so that we'll check whether we need to update our
 * descriptor immediately, rather than after up to CHECK_DESCRIPTOR_INTERVAL
 * seconds.
 */
void
reschedule_descriptor_update_check(void)
{
  tor_assert(check_descriptor_event);
  periodic_event_reschedule(check_descriptor_event);
}

/**
 * Update our schedule so that we'll check whether we need to fetch directory
 * info immediately.
 */
void
reschedule_directory_downloads(void)
{
  tor_assert(fetch_networkstatus_event);
  tor_assert(launch_descriptor_fetches_event);

  periodic_event_reschedule(fetch_networkstatus_event);
  periodic_event_reschedule(launch_descriptor_fetches_event);
}

#define LONGEST_TIMER_PERIOD (30 * 86400)
/** Helper: Return the number of seconds between <b>now</b> and <b>next</b>,
 * clipped to the range [1 second, LONGEST_TIMER_PERIOD]. */
static inline int
safe_timer_diff(time_t now, time_t next)
{
  if (next > now) {
    /* There were no computers at signed TIME_MIN (1902 on 32-bit systems),
     * and nothing that could run Tor. It's a bug if 'next' is around then.
     * On 64-bit systems with signed TIME_MIN, TIME_MIN is before the Big
     * Bang. We cannot extrapolate past a singularity, but there was probably
     * nothing that could run Tor then, either.
     **/
    tor_assert(next > TIME_MIN + LONGEST_TIMER_PERIOD);

    if (next - LONGEST_TIMER_PERIOD > now)
      return LONGEST_TIMER_PERIOD;
    return (int)(next - now);
  } else {
    return 1;
  }
}

/** Perform regular maintenance tasks.  This function gets run once per
 * second by second_elapsed_callback().
 */
static void
run_scheduled_events(time_t now)
{
  const or_options_t *options = get_options();

  /* 0. See if we've been asked to shut down and our timeout has
   * expired; or if our bandwidth limits are exhausted and we
   * should hibernate; or if it's time to wake up from hibernation.
   */
  consider_hibernation(now);

  /* 0b. If we've deferred a signewnym, make sure it gets handled
   * eventually. */
  if (signewnym_is_pending &&
      time_of_last_signewnym + MAX_SIGNEWNYM_RATE <= now) {
    log_info(LD_CONTROL, "Honoring delayed NEWNYM request");
    signewnym_impl(now);
  }

  /* 0c. If we've deferred log messages for the controller, handle them now */
  flush_pending_log_callbacks();

  if (options->UseBridges && !options->DisableNetwork) {
    fetch_bridge_descriptors(options, now);
  }

  if (accounting_is_enabled(options)) {
    accounting_run_housekeeping(now);
  }

  if (authdir_mode_v3(options)) {
    dirvote_act(options, now);
  }

  /* 3a. Every second, we examine pending circuits and prune the
   *    ones which have been pending for more than a few seconds.
   *    We do this before step 4, so it can try building more if
   *    it's not comfortable with the number of available circuits.
   */
  /* (If our circuit build timeout can ever become lower than a second (which
   * it can't, currently), we should do this more often.) */
  circuit_expire_building();

  /* 3b. Also look at pending streams and prune the ones that 'began'
   *     a long time ago but haven't gotten a 'connected' yet.
   *     Do this before step 4, so we can put them back into pending
   *     state to be picked up by the new circuit.
   */
  connection_ap_expire_beginning();

  /* 3c. And expire connections that we've held open for too long.
   */
  connection_expire_held_open();

  /* 4. Every second, we try a new circuit if there are no valid
   *    circuits. Every NewCircuitPeriod seconds, we expire circuits
   *    that became dirty more than MaxCircuitDirtiness seconds ago,
   *    and we make a new circ if there are no clean circuits.
   */
  const int have_dir_info = router_have_minimum_dir_info();
  if (have_dir_info && !net_is_disabled()) {
    circuit_build_needed_circs(now);
  } else {
    circuit_expire_old_circs_as_needed(now);
  }

  if (!net_is_disabled()) {
    /* This is usually redundant with circuit_build_needed_circs() above,
     * but it is very fast when there is no work to do. */
    connection_ap_attach_pending(0);
  }

  /* 5. We do housekeeping for each connection... */
  connection_or_set_bad_connections(NULL, 0);
  int i;
  for (i=0;i<smartlist_len(connection_array);i++) {
    run_connection_housekeeping(i, now);
  }

  /* 6. And remove any marked circuits... */
  circuit_close_all_marked();

  /* 7. And upload service descriptors if necessary. */
  if (have_completed_a_circuit() && !net_is_disabled()) {
    rend_consider_services_upload(now);
    rend_consider_descriptor_republication();
  }

  /* 8. and blow away any connections that need to die. have to do this now,
   * because if we marked a conn for close and left its socket -1, then
   * we'll pass it to poll/select and bad things will happen.
   */
  close_closeable_connections();

  /* 8b. And if anything in our state is ready to get flushed to disk, we
   * flush it. */
  or_state_save(now);

  /* 8c. Do channel cleanup just like for connections */
  channel_run_cleanup();
  channel_listener_run_cleanup();

  /* 11b. check pending unconfigured managed proxies */
  if (!net_is_disabled() && pt_proxies_configuration_pending())
    pt_configure_remaining_proxies();
}

static int
rotate_onion_key_callback(time_t now, const or_options_t *options)
{
  /* 1a. Every MIN_ONION_KEY_LIFETIME seconds, rotate the onion keys,
   *  shut down and restart all cpuworkers, and update the directory if
   *  necessary.
   */
  if (server_mode(options)) {
    time_t rotation_time = get_onion_key_set_at()+MIN_ONION_KEY_LIFETIME;
    if (rotation_time > now) {
      return safe_timer_diff(now, rotation_time);
    }

    log_info(LD_GENERAL,"Rotating onion key.");
    rotate_onion_key();
    cpuworkers_rotate_keyinfo();
    if (router_rebuild_descriptor(1)<0) {
      log_info(LD_CONFIG, "Couldn't rebuild router descriptor");
    }
    if (advertised_server_mode() && !options->DisableNetwork)
      router_upload_dir_desc_to_dirservers(0);
    return MIN_ONION_KEY_LIFETIME;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

static int
check_ed_keys_callback(time_t now, const or_options_t *options)
{
  if (server_mode(options)) {
    if (should_make_new_ed_keys(options, now)) {
      if (load_ed_keys(options, now) < 0 ||
          generate_ed_link_cert(options, now)) {
        log_err(LD_OR, "Unable to update Ed25519 keys!  Exiting.");
        tor_cleanup();
        exit(0);
      }
    }
    return 30;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

static int
launch_descriptor_fetches_callback(time_t now, const or_options_t *options)
{
  if (should_delay_dir_fetches(options, NULL))
      return PERIODIC_EVENT_NO_UPDATE;

  update_all_descriptor_downloads(now);
  update_extrainfo_downloads(now);
  if (router_have_minimum_dir_info())
    return LAZY_DESCRIPTOR_RETRY_INTERVAL;
  else
    return GREEDY_DESCRIPTOR_RETRY_INTERVAL;
}

static int
rotate_x509_certificate_callback(time_t now, const or_options_t *options)
{
  static int first = 1;
  (void)now;
  (void)options;
  if (first) {
    first = 0;
    return MAX_SSL_KEY_LIFETIME_INTERNAL;
  }

  /* 1b. Every MAX_SSL_KEY_LIFETIME_INTERNAL seconds, we change our
   * TLS context. */
  log_info(LD_GENERAL,"Rotating tls context.");
  if (router_initialize_tls_context() < 0) {
    log_err(LD_BUG, "Error reinitializing TLS context");
    tor_assert_unreached();
  }

  /* We also make sure to rotate the TLS connections themselves if they've
   * been up for too long -- but that's done via is_bad_for_new_circs in
   * run_connection_housekeeping() above. */
  return MAX_SSL_KEY_LIFETIME_INTERNAL;
}

static int
add_entropy_callback(time_t now, const or_options_t *options)
{
  (void)now;
  (void)options;
  /* We already seeded once, so don't die on failure. */
  if (crypto_seed_rng() < 0) {
    log_warn(LD_GENERAL, "Tried to re-seed RNG, but failed. We already "
             "seeded once, though, so we won't exit here.");
  }

  /** How often do we add more entropy to OpenSSL's RNG pool? */
#define ENTROPY_INTERVAL (60*60)
  return ENTROPY_INTERVAL;
}

static int
launch_reachability_tests_callback(time_t now, const or_options_t *options)
{
  if (authdir_mode_tests_reachability(options) &&
      !net_is_disabled()) {
    /* try to determine reachability of the other Tor relays */
    dirserv_test_reachability(now);
  }
  return REACHABILITY_TEST_INTERVAL;
}

static int
downrate_stability_callback(time_t now, const or_options_t *options)
{
  (void)options;
  /* 1d. Periodically, we discount older stability information so that new
   * stability info counts more, and save the stability information to disk as
   * appropriate. */
  time_t next = rep_hist_downrate_old_runs(now);
  return safe_timer_diff(now, next);
}

static int
save_stability_callback(time_t now, const or_options_t *options)
{
  if (authdir_mode_tests_reachability(options)) {
    if (rep_hist_record_mtbf_data(now, 1)<0) {
      log_warn(LD_GENERAL, "Couldn't store mtbf data.");
    }
  }
#define SAVE_STABILITY_INTERVAL (30*60)
  return SAVE_STABILITY_INTERVAL;
}

static int
check_authority_cert_callback(time_t now, const or_options_t *options)
{
  (void)now;
  (void)options;
  /* 1e. Periodically, if we're a v3 authority, we check whether our cert is
   * close to expiring and warn the admin if it is. */
  v3_authority_check_key_expiry();
#define CHECK_V3_CERTIFICATE_INTERVAL (5*60)
  return CHECK_V3_CERTIFICATE_INTERVAL;
}

static int
check_expired_networkstatus_callback(time_t now, const or_options_t *options)
{
  (void)options;
  /* 1f. Check whether our networkstatus has expired.
   */
  networkstatus_t *ns = networkstatus_get_latest_consensus();
  /*XXXX RD: This value needs to be the same as REASONABLY_LIVE_TIME in
   * networkstatus_get_reasonably_live_consensus(), but that value is way
   * way too high.  Arma: is the bridge issue there resolved yet? -NM */
#define NS_EXPIRY_SLOP (24*60*60)
  if (ns && ns->valid_until < now+NS_EXPIRY_SLOP &&
      router_have_minimum_dir_info()) {
    router_dir_info_changed();
  }
#define CHECK_EXPIRED_NS_INTERVAL (2*60)
  return CHECK_EXPIRED_NS_INTERVAL;
}

static int
write_stats_file_callback(time_t now, const or_options_t *options)
{
  /* 1g. Check whether we should write statistics to disk.
   */
#define CHECK_WRITE_STATS_INTERVAL (60*60)
  time_t next_time_to_write_stats_files = now + CHECK_WRITE_STATS_INTERVAL;
  if (options->CellStatistics) {
    time_t next_write =
      rep_hist_buffer_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->DirReqStatistics) {
    time_t next_write = geoip_dirreq_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->EntryStatistics) {
    time_t next_write = geoip_entry_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->HiddenServiceStatistics) {
    time_t next_write = rep_hist_hs_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->ExitPortStatistics) {
    time_t next_write = rep_hist_exit_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->ConnDirectionStatistics) {
    time_t next_write = rep_hist_conn_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }
  if (options->BridgeAuthoritativeDir) {
    time_t next_write = rep_hist_desc_stats_write(now);
    if (next_write && next_write < next_time_to_write_stats_files)
      next_time_to_write_stats_files = next_write;
  }

  return safe_timer_diff(now, next_time_to_write_stats_files);
}

static int
record_bridge_stats_callback(time_t now, const or_options_t *options)
{
  static int should_init_bridge_stats = 1;

  /* 1h. Check whether we should write bridge statistics to disk.
   */
  if (should_record_bridge_info(options)) {
    if (should_init_bridge_stats) {
      /* (Re-)initialize bridge statistics. */
        geoip_bridge_stats_init(now);
        should_init_bridge_stats = 0;
        return WRITE_STATS_INTERVAL;
    } else {
      /* Possibly write bridge statistics to disk and ask when to write
       * them next time. */
      time_t next = geoip_bridge_stats_write(now);
      return safe_timer_diff(now, next);
    }
  } else if (!should_init_bridge_stats) {
    /* Bridge mode was turned off. Ensure that stats are re-initialized
     * next time bridge mode is turned on. */
    should_init_bridge_stats = 1;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

static int
clean_caches_callback(time_t now, const or_options_t *options)
{
  /* Remove old information from rephist and the rend cache. */
  rep_history_clean(now - options->RephistTrackTime);
  rend_cache_clean(now, REND_CACHE_TYPE_CLIENT);
  rend_cache_clean(now, REND_CACHE_TYPE_SERVICE);
  rend_cache_clean_v2_descs_as_dir(now, 0);
  microdesc_cache_rebuild(NULL, 0);
#define CLEAN_CACHES_INTERVAL (30*60)
  return CLEAN_CACHES_INTERVAL;
}

static int
rend_cache_failure_clean_callback(time_t now, const or_options_t *options)
{
  (void)options;
  /* We don't keep entries that are more than five minutes old so we try to
   * clean it as soon as we can since we want to make sure the client waits
   * as little as possible for reachability reasons. */
  rend_cache_failure_clean(now);
  return 30;
}

static int
retry_dns_callback(time_t now, const or_options_t *options)
{
  (void)now;
#define RETRY_DNS_INTERVAL (10*60)
  /* If we're a server and initializing dns failed, retry periodically. */
  if (server_mode(options) && has_dns_init_failed())
    dns_init();
  return RETRY_DNS_INTERVAL;
}

  /* 2. Periodically, we consider force-uploading our descriptor
   * (if we've passed our internal checks). */

static int
check_descriptor_callback(time_t now, const or_options_t *options)
{
/** How often do we check whether part of our router info has changed in a
 * way that would require an upload? That includes checking whether our IP
 * address has changed. */
#define CHECK_DESCRIPTOR_INTERVAL (60)

  /* 2b. Once per minute, regenerate and upload the descriptor if the old
   * one is inaccurate. */
  if (!options->DisableNetwork) {
    check_descriptor_bandwidth_changed(now);
    check_descriptor_ipaddress_changed(now);
    mark_my_descriptor_dirty_if_too_old(now);
    consider_publishable_server(0);
    /* If any networkstatus documents are no longer recent, we need to
     * update all the descriptors' running status. */
    /* Remove dead routers. */
    /* XXXX This doesn't belong here, but it was here in the pre-
     * XXXX refactoring code. */
    routerlist_remove_old_routers();
  }

  return CHECK_DESCRIPTOR_INTERVAL;
}

static int
check_for_reachability_bw_callback(time_t now, const or_options_t *options)
{
  /* XXXX This whole thing was stuck in the middle of what is now
   * XXXX check_descriptor_callback.  I'm not sure it's right. */

  static int dirport_reachability_count = 0;
  /* also, check religiously for reachability, if it's within the first
   * 20 minutes of our uptime. */
  if (server_mode(options) &&
      (have_completed_a_circuit() || !any_predicted_circuits(now)) &&
      !we_are_hibernating()) {
    if (stats_n_seconds_working < TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT) {
      consider_testing_reachability(1, dirport_reachability_count==0);
      if (++dirport_reachability_count > 5)
        dirport_reachability_count = 0;
      return 1;
    } else {
      /* If we haven't checked for 12 hours and our bandwidth estimate is
       * low, do another bandwidth test. This is especially important for
       * bridges, since they might go long periods without much use. */
      const routerinfo_t *me = router_get_my_routerinfo();
      static int first_time = 1;
      if (!first_time && me &&
          me->bandwidthcapacity < me->bandwidthrate &&
          me->bandwidthcapacity < 51200) {
        reset_bandwidth_test();
      }
      first_time = 0;
#define BANDWIDTH_RECHECK_INTERVAL (12*60*60)
      return BANDWIDTH_RECHECK_INTERVAL;
    }
  }
  return CHECK_DESCRIPTOR_INTERVAL;
}

static int
fetch_networkstatus_callback(time_t now, const or_options_t *options)
{
  /* 2c. Every minute (or every second if TestingTorNetwork, or during
   * client bootstrap), check whether we want to download any networkstatus
   * documents. */

  /* How often do we check whether we should download network status
   * documents? */
  const int we_are_bootstrapping = networkstatus_consensus_is_bootstrapping(
                                                                        now);
  const int prefer_mirrors = !directory_fetches_from_authorities(
                                                              get_options());
  int networkstatus_dl_check_interval = 60;
  /* check more often when testing, or when bootstrapping from mirrors
   * (connection limits prevent too many connections being made) */
  if (options->TestingTorNetwork
      || (we_are_bootstrapping && prefer_mirrors)) {
    networkstatus_dl_check_interval = 1;
  }

  if (should_delay_dir_fetches(options, NULL))
    return PERIODIC_EVENT_NO_UPDATE;

  update_networkstatus_downloads(now);
  return networkstatus_dl_check_interval;
}

static int
retry_listeners_callback(time_t now, const or_options_t *options)
{
  (void)now;
  (void)options;
  /* 3d. And every 60 seconds, we relaunch listeners if any died. */
  if (!net_is_disabled()) {
    retry_all_listeners(NULL, NULL, 0);
    return 60;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

static int
expire_old_ciruits_serverside_callback(time_t now, const or_options_t *options)
{
  (void)options;
  /* every 11 seconds, so not usually the same second as other such events */
  circuit_expire_old_circuits_serverside(now);
  return 11;
}

static int
check_dns_honesty_callback(time_t now, const or_options_t *options)
{
  (void)now;
  /* 9. and if we're an exit node, check whether our DNS is telling stories
   * to us. */
  if (net_is_disabled() ||
      ! public_server_mode(options) ||
      router_my_exit_policy_is_reject_star())
    return PERIODIC_EVENT_NO_UPDATE;

  static int first_time = 1;
  if (first_time) {
    /* Don't launch right when we start */
    first_time = 0;
    return crypto_rand_int_range(60, 180);
  }

  dns_launch_correctness_checks();
  return 12*3600 + crypto_rand_int(12*3600);
}

static int
write_bridge_ns_callback(time_t now, const or_options_t *options)
{
  /* 10. write bridge networkstatus file to disk */
  if (options->BridgeAuthoritativeDir) {
    networkstatus_dump_bridge_status_to_file(now);
#define BRIDGE_STATUSFILE_INTERVAL (30*60)
     return BRIDGE_STATUSFILE_INTERVAL;
  }
  return PERIODIC_EVENT_NO_UPDATE;
}

static int
check_fw_helper_app_callback(time_t now, const or_options_t *options)
{
  if (net_is_disabled() ||
      ! server_mode(options) ||
      ! options->PortForwarding) {
    return PERIODIC_EVENT_NO_UPDATE;
  }
  /* 11. check the port forwarding app */

#define PORT_FORWARDING_CHECK_INTERVAL 5
  smartlist_t *ports_to_forward = get_list_of_ports_to_forward();
  if (ports_to_forward) {
    tor_check_port_forwarding(options->PortForwardingHelper,
                              ports_to_forward,
                              now);

    SMARTLIST_FOREACH(ports_to_forward, char *, cp, tor_free(cp));
    smartlist_free(ports_to_forward);
  }
  return PORT_FORWARDING_CHECK_INTERVAL;
}

/** Callback to write heartbeat message in the logs. */
static int
heartbeat_callback(time_t now, const or_options_t *options)
{
  static int first = 1;

  /* Check if heartbeat is disabled */
  if (!options->HeartbeatPeriod) {
    return PERIODIC_EVENT_NO_UPDATE;
  }

  /* Write the heartbeat message */
  if (first) {
    first = 0; /* Skip the first one. */
  } else {
    log_heartbeat(now);
  }

  return options->HeartbeatPeriod;
}

/** Timer: used to invoke second_elapsed_callback() once per second. */
static periodic_timer_t *second_timer = NULL;
/** Number of libevent errors in the last second: we die if we get too many. */
static int n_libevent_errors = 0;

/** Libevent callback: invoked once every second. */
static void
second_elapsed_callback(periodic_timer_t *timer, void *arg)
{
  /* XXXX This could be sensibly refactored into multiple callbacks, and we
   * could use Libevent's timers for this rather than checking the current
   * time against a bunch of timeouts every second. */
  static time_t current_second = 0;
  time_t now;
  size_t bytes_written;
  size_t bytes_read;
  int seconds_elapsed;
  const or_options_t *options = get_options();
  (void)timer;
  (void)arg;

  n_libevent_errors = 0;

  /* log_notice(LD_GENERAL, "Tick."); */
  now = time(NULL);
  update_approx_time(now);

  /* the second has rolled over. check more stuff. */
  seconds_elapsed = current_second ? (int)(now - current_second) : 0;
  bytes_read = (size_t)(stats_n_bytes_read - stats_prev_n_read);
  bytes_written = (size_t)(stats_n_bytes_written - stats_prev_n_written);
  stats_prev_n_read = stats_n_bytes_read;
  stats_prev_n_written = stats_n_bytes_written;

  control_event_bandwidth_used((uint32_t)bytes_read,(uint32_t)bytes_written);
  control_event_stream_bandwidth_used();
  control_event_conn_bandwidth_used();
  control_event_circ_bandwidth_used();
  control_event_circuit_cell_stats();

  if (server_mode(options) &&
      !net_is_disabled() &&
      seconds_elapsed > 0 &&
      have_completed_a_circuit() &&
      stats_n_seconds_working / TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT !=
      (stats_n_seconds_working+seconds_elapsed) /
        TIMEOUT_UNTIL_UNREACHABILITY_COMPLAINT) {
    /* every 20 minutes, check and complain if necessary */
    const routerinfo_t *me = router_get_my_routerinfo();
    if (me && !check_whether_orport_reachable(options)) {
      char *address = tor_dup_ip(me->addr);
      log_warn(LD_CONFIG,"Your server (%s:%d) has not managed to confirm that "
               "its ORPort is reachable. Relays do not publish descriptors "
               "until their ORPort and DirPort are reachable. Please check "
               "your firewalls, ports, address, /etc/hosts file, etc.",
               address, me->or_port);
      control_event_server_status(LOG_WARN,
                                  "REACHABILITY_FAILED ORADDRESS=%s:%d",
                                  address, me->or_port);
      tor_free(address);
    }

    if (me && !check_whether_dirport_reachable(options)) {
      char *address = tor_dup_ip(me->addr);
      log_warn(LD_CONFIG,
               "Your server (%s:%d) has not managed to confirm that its "
               "DirPort is reachable. Relays do not publish descriptors "
               "until their ORPort and DirPort are reachable. Please check "
               "your firewalls, ports, address, /etc/hosts file, etc.",
               address, me->dir_port);
      control_event_server_status(LOG_WARN,
                                  "REACHABILITY_FAILED DIRADDRESS=%s:%d",
                                  address, me->dir_port);
      tor_free(address);
    }
  }

/** If more than this many seconds have elapsed, probably the clock
 * jumped: doesn't count. */
#define NUM_JUMPED_SECONDS_BEFORE_WARN 100
  if (seconds_elapsed < -NUM_JUMPED_SECONDS_BEFORE_WARN ||
      seconds_elapsed >= NUM_JUMPED_SECONDS_BEFORE_WARN) {
    circuit_note_clock_jumped(seconds_elapsed);
  } else if (seconds_elapsed > 0)
    stats_n_seconds_working += seconds_elapsed;

  run_scheduled_events(now);

  current_second = now; /* remember which second it is, for next time */
}

#ifdef HAVE_SYSTEMD_209
static periodic_timer_t *systemd_watchdog_timer = NULL;

/** Libevent callback: invoked to reset systemd watchdog. */
static void
systemd_watchdog_callback(periodic_timer_t *timer, void *arg)
{
  (void)timer;
  (void)arg;
  sd_notify(0, "WATCHDOG=1");
}
#endif

/** Timer: used to invoke refill_callback(). */
static periodic_timer_t *refill_timer = NULL;

/** Libevent callback: invoked periodically to refill token buckets
 * and count r/w bytes. */
static void
refill_callback(periodic_timer_t *timer, void *arg)
{
  static struct timeval current_millisecond;
  struct timeval now;

  size_t bytes_written;
  size_t bytes_read;
  int milliseconds_elapsed = 0;
  int seconds_rolled_over = 0;

  const or_options_t *options = get_options();

  (void)timer;
  (void)arg;

  tor_gettimeofday(&now);

  /* If this is our first time, no time has passed. */
  if (current_millisecond.tv_sec) {
    long mdiff = tv_mdiff(&current_millisecond, &now);
    if (mdiff > INT_MAX)
      mdiff = INT_MAX;
    milliseconds_elapsed = (int)mdiff;
    seconds_rolled_over = (int)(now.tv_sec - current_millisecond.tv_sec);
  }

  bytes_written = stats_prev_global_write_bucket - global_write_bucket;
  bytes_read = stats_prev_global_read_bucket - global_read_bucket;

  stats_n_bytes_read += bytes_read;
  stats_n_bytes_written += bytes_written;
  if (accounting_is_enabled(options) && milliseconds_elapsed >= 0)
    accounting_add_bytes(bytes_read, bytes_written, seconds_rolled_over);

  if (milliseconds_elapsed > 0)
    connection_bucket_refill(milliseconds_elapsed, (time_t)now.tv_sec);

  stats_prev_global_read_bucket = global_read_bucket;
  stats_prev_global_write_bucket = global_write_bucket;

  current_millisecond = now; /* remember what time it is, for next time */
}

#ifndef _WIN32
/** Called when a possibly ignorable libevent error occurs; ensures that we
 * don't get into an infinite loop by ignoring too many errors from
 * libevent. */
static int
got_libevent_error(void)
{
  if (++n_libevent_errors > 8) {
    log_err(LD_NET, "Too many libevent errors in one second; dying");
    return -1;
  }
  return 0;
}
#endif

#define UPTIME_CUTOFF_FOR_NEW_BANDWIDTH_TEST (6*60*60)

/** Called when our IP address seems to have changed. <b>at_interface</b>
 * should be true if we detected a change in our interface, and false if we
 * detected a change in our published address. */
void
ip_address_changed(int at_interface)
{
  const or_options_t *options = get_options();
  int server = server_mode(options);
  int exit_reject_interfaces = (server && options->ExitRelay
                                && options->ExitPolicyRejectLocalInterfaces);

  if (at_interface) {
    if (! server) {
      /* Okay, change our keys. */
      if (init_keys_client() < 0)
        log_warn(LD_GENERAL, "Unable to rotate keys after IP change!");
    }
  } else {
    if (server) {
      if (stats_n_seconds_working > UPTIME_CUTOFF_FOR_NEW_BANDWIDTH_TEST)
        reset_bandwidth_test();
      stats_n_seconds_working = 0;
      router_reset_reachability();
    }
  }

  /* Exit relays incorporate interface addresses in their exit policies when
   * ExitPolicyRejectLocalInterfaces is set */
  if (exit_reject_interfaces || (server && !at_interface)) {
    mark_my_descriptor_dirty("IP address changed");
  }

  dns_servers_relaunch_checks();
}

/** Forget what we've learned about the correctness of our DNS servers, and
 * start learning again. */
void
dns_servers_relaunch_checks(void)
{
  if (server_mode(get_options())) {
    dns_reset_correctness_checks();
    if (periodic_events_initialized) {
      tor_assert(check_dns_honesty_event);
      periodic_event_reschedule(check_dns_honesty_event);
    }
  }
}

/** Called when we get a SIGHUP: reload configuration files and keys,
 * retry all connections, and so on. */
static int
do_hup(void)
{
  const or_options_t *options = get_options();

#ifdef USE_DMALLOC
  dmalloc_log_stats();
  dmalloc_log_changed(0, 1, 0, 0);
#endif

  log_notice(LD_GENERAL,"Received reload signal (hup). Reloading config and "
             "resetting internal state.");
  if (accounting_is_enabled(options))
    accounting_record_bandwidth_usage(time(NULL), get_or_state());

  router_reset_warnings();
  routerlist_reset_warnings();
  /* first, reload config variables, in case they've changed */
  if (options->ReloadTorrcOnSIGHUP) {
    /* no need to provide argc/v, they've been cached in init_from_config */
    if (options_init_from_torrc(0, NULL) < 0) {
      log_err(LD_CONFIG,"Reading config failed--see warnings above. "
              "For usage, try -h.");
      return -1;
    }
    options = get_options(); /* they have changed now */
    /* Logs are only truncated the first time they are opened, but were
       probably intended to be cleaned up on signal. */
    if (options->TruncateLogFile)
      truncate_logs();
  } else {
    char *msg = NULL;
    log_notice(LD_GENERAL, "Not reloading config file: the controller told "
               "us not to.");
    /* Make stuff get rescanned, reloaded, etc. */
    if (set_options((or_options_t*)options, &msg) < 0) {
      if (!msg)
        msg = tor_strdup("Unknown error");
      log_warn(LD_GENERAL, "Unable to re-set previous options: %s", msg);
      tor_free(msg);
    }
  }
  if (authdir_mode_handles_descs(options, -1)) {
    /* reload the approved-routers file */
    if (dirserv_load_fingerprint_file() < 0) {
      /* warnings are logged from dirserv_load_fingerprint_file() directly */
      log_info(LD_GENERAL, "Error reloading fingerprints. "
               "Continuing with old list.");
    }
  }

  /* Rotate away from the old dirty circuits. This has to be done
   * after we've read the new options, but before we start using
   * circuits for directory fetches. */
  circuit_mark_all_dirty_circs_as_unusable();

  /* retry appropriate downloads */
  router_reset_status_download_failures();
  router_reset_descriptor_download_failures();
  if (!options->DisableNetwork)
    update_networkstatus_downloads(time(NULL));

  /* We'll retry routerstatus downloads in about 10 seconds; no need to
   * force a retry there. */

  if (server_mode(options)) {
    /* Maybe we've been given a new ed25519 key or certificate?
     */
    time_t now = approx_time();
    if (load_ed_keys(options, now) < 0 ||
         generate_ed_link_cert(options, now)) {
      log_warn(LD_OR, "Problem reloading Ed25519 keys; still using old keys.");
    }

    /* Update cpuworker and dnsworker processes, so they get up-to-date
     * configuration options. */
    cpuworkers_rotate_keyinfo();
    dns_reset();
  }
  return 0;
}

/** Tor main loop. */
int
do_main_loop(void)
{
  time_t now;

  /* initialize the periodic events first, so that code that depends on the
   * events being present does not assert.
   */
  if (! periodic_events_initialized) {
    initialize_periodic_events();
  }

  /* initialize dns resolve map, spawn workers if needed */
  if (dns_init() < 0) {
    if (get_options()->ServerDNSAllowBrokenConfig)
      log_warn(LD_GENERAL, "Couldn't set up any working nameservers. "
               "Network not up yet?  Will try again soon.");
    else {
      log_err(LD_GENERAL,"Error initializing dns subsystem; exiting.  To "
              "retry instead, set the ServerDNSAllowBrokenResolvConf option.");
    }
  }

  handle_signals(1);

  /* load the private keys, if we're supposed to have them, and set up the
   * TLS context. */
  if (! client_identity_key_is_set()) {
    if (init_keys() < 0) {
      log_err(LD_OR, "Error initializing keys; exiting");
      return -1;
    }
  }

  /* Set up our buckets */
  connection_bucket_init();
  stats_prev_global_read_bucket = global_read_bucket;
  stats_prev_global_write_bucket = global_write_bucket;

  /* initialize the bootstrap status events to know we're starting up */
  control_event_bootstrap(BOOTSTRAP_STATUS_STARTING, 0);

  /* Initialize the keypinning log. */
  if (authdir_mode_v3(get_options())) {
    char *fname = get_datadir_fname("key-pinning-journal");
    int r = 0;
    if (keypin_load_journal(fname)<0) {
      log_err(LD_DIR, "Error loading key-pinning journal: %s",strerror(errno));
      r = -1;
    }
    if (keypin_open_journal(fname)<0) {
      log_err(LD_DIR, "Error opening key-pinning journal: %s",strerror(errno));
      r = -1;
    }
    tor_free(fname);
    if (r)
      return r;
  }
  {
    /* This is the old name for key-pinning-journal.  These got corrupted
     * in a couple of cases by #16530, so we started over. See #16580 for
     * the rationale and for other options we didn't take.  We can remove
     * this code once all the authorities that ran 0.2.7.1-alpha-dev are
     * upgraded.
     */
    char *fname = get_datadir_fname("key-pinning-entries");
    unlink(fname);
    tor_free(fname);
  }

  if (trusted_dirs_reload_certs()) {
    log_warn(LD_DIR,
             "Couldn't load all cached v3 certificates. Starting anyway.");
  }
  if (router_reload_consensus_networkstatus()) {
    return -1;
  }
  /* load the routers file, or assign the defaults. */
  if (router_reload_router_list()) {
    return -1;
  }
  /* load the networkstatuses. (This launches a download for new routers as
   * appropriate.)
   */
  now = time(NULL);
  directory_info_has_arrived(now, 1, 0);

  if (server_mode(get_options())) {
    /* launch cpuworkers. Need to do this *after* we've read the onion key. */
    cpu_init();
  }

  /* Setup shared random protocol subsystem. */
  if (authdir_mode_publishes_statuses(get_options())) {
    if (sr_init(1) < 0) {
      return -1;
    }
  }

  /* set up once-a-second callback. */
  if (! second_timer) {
    struct timeval one_second;
    one_second.tv_sec = 1;
    one_second.tv_usec = 0;

    second_timer = periodic_timer_new(tor_libevent_get_base(),
                                      &one_second,
                                      second_elapsed_callback,
                                      NULL);
    tor_assert(second_timer);
  }

#ifdef HAVE_SYSTEMD_209
  uint64_t watchdog_delay;
  /* set up systemd watchdog notification. */
  if (sd_watchdog_enabled(1, &watchdog_delay) > 0) {
    if (! systemd_watchdog_timer) {
      struct timeval watchdog;
      /* The manager will "act on" us if we don't send them a notification
       * every 'watchdog_delay' microseconds.  So, send notifications twice
       * that often.  */
      watchdog_delay /= 2;
      watchdog.tv_sec = watchdog_delay  / 1000000;
      watchdog.tv_usec = watchdog_delay % 1000000;

      systemd_watchdog_timer = periodic_timer_new(tor_libevent_get_base(),
                                                  &watchdog,
                                                  systemd_watchdog_callback,
                                                  NULL);
      tor_assert(systemd_watchdog_timer);
    }
  }
#endif

  if (!refill_timer) {
    struct timeval refill_interval;
    int msecs = get_options()->TokenBucketRefillInterval;

    refill_interval.tv_sec =  msecs/1000;
    refill_interval.tv_usec = (msecs%1000)*1000;

    refill_timer = periodic_timer_new(tor_libevent_get_base(),
                                      &refill_interval,
                                      refill_callback,
                                      NULL);
    tor_assert(refill_timer);
  }

#ifdef HAVE_SYSTEMD
  {
    const int r = sd_notify(0, "READY=1");
    if (r < 0) {
      log_warn(LD_GENERAL, "Unable to send readiness to systemd: %s",
               strerror(r));
    } else if (r > 0) {
      log_notice(LD_GENERAL, "Signaled readiness to systemd");
    } else {
      log_info(LD_GENERAL, "Systemd NOTIFY_SOCKET not present.");
    }
  }
#endif

  return run_main_loop_until_done();
}

/**
 * Run the main loop a single time. Return 0 for "exit"; -1 for "exit with
 * error", and 1 for "run this again."
 */
static int
run_main_loop_once(void)
{
  int loop_result;

  if (nt_service_is_stopping())
    return 0;

#ifndef _WIN32
  /* Make it easier to tell whether libevent failure is our fault or not. */
  errno = 0;
#endif
  /* All active linked conns should get their read events activated. */
  SMARTLIST_FOREACH(active_linked_connection_lst, connection_t *, conn,
                    event_active(conn->read_event, EV_READ, 1));
  called_loop_once = smartlist_len(active_linked_connection_lst) ? 1 : 0;

  update_approx_time(time(NULL));

  /* poll until we have an event, or the second ends, or until we have
   * some active linked connections to trigger events for. */
  loop_result = event_base_loop(tor_libevent_get_base(),
                                called_loop_once ? EVLOOP_ONCE : 0);

  /* let catch() handle things like ^c, and otherwise don't worry about it */
  if (loop_result < 0) {
    int e = tor_socket_errno(-1);
    /* let the program survive things like ^z */
    if (e != EINTR && !ERRNO_IS_EINPROGRESS(e)) {
      log_err(LD_NET,"libevent call with %s failed: %s [%d]",
              tor_libevent_get_method(), tor_socket_strerror(e), e);
      return -1;
#ifndef _WIN32
    } else if (e == EINVAL) {
      log_warn(LD_NET, "EINVAL from libevent: should you upgrade libevent?");
      if (got_libevent_error())
        return -1;
#endif
    } else {
      tor_assert_nonfatal_once(! ERRNO_IS_EINPROGRESS(e));
      log_debug(LD_NET,"libevent call interrupted.");
      /* You can't trust the results of this poll(). Go back to the
       * top of the big for loop. */
      return 1;
    }
  }

  /* This will be pretty fast if nothing new is pending. Note that this gets
   * called once per libevent loop, which will make it happen once per group
   * of events that fire, or once per second. */
  connection_ap_attach_pending(0);

  return 1;
}

/** Run the run_main_loop_once() function until it declares itself done,
 * and return its final return value.
 *
 * Shadow won't invoke this function, so don't fill it up with things.
 */
static int
run_main_loop_until_done(void)
{
  int loop_result = 1;
  do {
    loop_result = run_main_loop_once();
  } while (loop_result == 1);
  return loop_result;
}

/** Libevent callback: invoked when we get a signal.
 */
static void
signal_callback(evutil_socket_t fd, short events, void *arg)
{
  const int *sigptr = arg;
  const int sig = *sigptr;
  (void)fd;
  (void)events;

  process_signal(sig);
}

/** Do the work of acting on a signal received in <b>sig</b> */
static void
process_signal(int sig)
{
  switch (sig)
    {
    case SIGTERM:
      log_notice(LD_GENERAL,"Catching signal TERM, exiting cleanly.");
      tor_cleanup();
      exit(0);
      break;
    case SIGINT:
      if (!server_mode(get_options())) { /* do it now */
        log_notice(LD_GENERAL,"Interrupt: exiting cleanly.");
        tor_cleanup();
        exit(0);
      }
#ifdef HAVE_SYSTEMD
      sd_notify(0, "STOPPING=1");
#endif
      hibernate_begin_shutdown();
      break;
#ifdef SIGPIPE
    case SIGPIPE:
      log_debug(LD_GENERAL,"Caught SIGPIPE. Ignoring.");
      break;
#endif
    case SIGUSR1:
      /* prefer to log it at INFO, but make sure we always see it */
      dumpstats(get_min_log_level()<LOG_INFO ? get_min_log_level() : LOG_INFO);
      control_event_signal(sig);
      break;
    case SIGUSR2:
      switch_logs_debug();
      log_debug(LD_GENERAL,"Caught USR2, going to loglevel debug. "
                "Send HUP to change back.");
      control_event_signal(sig);
      break;
    case SIGHUP:
#ifdef HAVE_SYSTEMD
      sd_notify(0, "RELOADING=1");
#endif
      if (do_hup() < 0) {
        log_warn(LD_CONFIG,"Restart failed (config error?). Exiting.");
        tor_cleanup();
        exit(1);
      }
#ifdef HAVE_SYSTEMD
      sd_notify(0, "READY=1");
#endif
      control_event_signal(sig);
      break;
#ifdef SIGCHLD
    case SIGCHLD:
      notify_pending_waitpid_callbacks();
      break;
#endif
    case SIGNEWNYM: {
      time_t now = time(NULL);
      if (time_of_last_signewnym + MAX_SIGNEWNYM_RATE > now) {
        signewnym_is_pending = 1;
        log_notice(LD_CONTROL,
            "Rate limiting NEWNYM request: delaying by %d second(s)",
            (int)(MAX_SIGNEWNYM_RATE+time_of_last_signewnym-now));
      } else {
        signewnym_impl(now);
      }
      break;
    }
    case SIGCLEARDNSCACHE:
      addressmap_clear_transient();
      control_event_signal(sig);
      break;
    case SIGHEARTBEAT:
      log_heartbeat(time(NULL));
      control_event_signal(sig);
      break;
  }
}

/** Returns Tor's uptime. */
MOCK_IMPL(long,
get_uptime,(void))
{
  return stats_n_seconds_working;
}

/**
 * Write current memory usage information to the log.
 */
static void
dumpmemusage(int severity)
{
  connection_dump_buffer_mem_stats(severity);
  tor_log(severity, LD_GENERAL, "In rephist: "U64_FORMAT" used by %d Tors.",
      U64_PRINTF_ARG(rephist_total_alloc), rephist_total_num);
  dump_routerlist_mem_usage(severity);
  dump_cell_pool_usage(severity);
  dump_dns_mem_usage(severity);
  tor_log_mallinfo(severity);
}

/** Write all statistics to the log, with log level <b>severity</b>. Called
 * in response to a SIGUSR1. */
static void
dumpstats(int severity)
{
  time_t now = time(NULL);
  time_t elapsed;
  size_t rbuf_cap, wbuf_cap, rbuf_len, wbuf_len;

  tor_log(severity, LD_GENERAL, "Dumping stats:");

  SMARTLIST_FOREACH_BEGIN(connection_array, connection_t *, conn) {
    int i = conn_sl_idx;
    tor_log(severity, LD_GENERAL,
        "Conn %d (socket %d) type %d (%s), state %d (%s), created %d secs ago",
        i, (int)conn->s, conn->type, conn_type_to_string(conn->type),
        conn->state, conn_state_to_string(conn->type, conn->state),
        (int)(now - conn->timestamp_created));
    if (!connection_is_listener(conn)) {
      tor_log(severity,LD_GENERAL,
          "Conn %d is to %s:%d.", i,
          safe_str_client(conn->address),
          conn->port);
      tor_log(severity,LD_GENERAL,
          "Conn %d: %d bytes waiting on inbuf (len %d, last read %d secs ago)",
          i,
          (int)connection_get_inbuf_len(conn),
          (int)buf_allocation(conn->inbuf),
          (int)(now - conn->timestamp_lastread));
      tor_log(severity,LD_GENERAL,
          "Conn %d: %d bytes waiting on outbuf "
          "(len %d, last written %d secs ago)",i,
          (int)connection_get_outbuf_len(conn),
          (int)buf_allocation(conn->outbuf),
          (int)(now - conn->timestamp_lastwritten));
      if (conn->type == CONN_TYPE_OR) {
        or_connection_t *or_conn = TO_OR_CONN(conn);
        if (or_conn->tls) {
          if (tor_tls_get_buffer_sizes(or_conn->tls, &rbuf_cap, &rbuf_len,
                                       &wbuf_cap, &wbuf_len) == 0) {
            tor_log(severity, LD_GENERAL,
                "Conn %d: %d/%d bytes used on OpenSSL read buffer; "
                "%d/%d bytes used on write buffer.",
                i, (int)rbuf_len, (int)rbuf_cap, (int)wbuf_len, (int)wbuf_cap);
          }
        }
      }
    }
    circuit_dump_by_conn(conn, severity); /* dump info about all the circuits
                                           * using this conn */
  } SMARTLIST_FOREACH_END(conn);

  channel_dumpstats(severity);
  channel_listener_dumpstats(severity);

  tor_log(severity, LD_NET,
      "Cells processed: "U64_FORMAT" padding\n"
      "                 "U64_FORMAT" create\n"
      "                 "U64_FORMAT" created\n"
      "                 "U64_FORMAT" relay\n"
      "                        ("U64_FORMAT" relayed)\n"
      "                        ("U64_FORMAT" delivered)\n"
      "                 "U64_FORMAT" destroy",
      U64_PRINTF_ARG(stats_n_padding_cells_processed),
      U64_PRINTF_ARG(stats_n_create_cells_processed),
      U64_PRINTF_ARG(stats_n_created_cells_processed),
      U64_PRINTF_ARG(stats_n_relay_cells_processed),
      U64_PRINTF_ARG(stats_n_relay_cells_relayed),
      U64_PRINTF_ARG(stats_n_relay_cells_delivered),
      U64_PRINTF_ARG(stats_n_destroy_cells_processed));
  if (stats_n_data_cells_packaged)
    tor_log(severity,LD_NET,"Average packaged cell fullness: %2.3f%%",
        100*(U64_TO_DBL(stats_n_data_bytes_packaged) /
             U64_TO_DBL(stats_n_data_cells_packaged*RELAY_PAYLOAD_SIZE)) );
  if (stats_n_data_cells_received)
    tor_log(severity,LD_NET,"Average delivered cell fullness: %2.3f%%",
        100*(U64_TO_DBL(stats_n_data_bytes_received) /
             U64_TO_DBL(stats_n_data_cells_received*RELAY_PAYLOAD_SIZE)) );

  cpuworker_log_onionskin_overhead(severity, ONION_HANDSHAKE_TYPE_TAP, "TAP");
  cpuworker_log_onionskin_overhead(severity, ONION_HANDSHAKE_TYPE_NTOR,"ntor");

  if (now - time_of_process_start >= 0)
    elapsed = now - time_of_process_start;
  else
    elapsed = 0;

  if (elapsed) {
    tor_log(severity, LD_NET,
        "Average bandwidth: "U64_FORMAT"/%d = %d bytes/sec reading",
        U64_PRINTF_ARG(stats_n_bytes_read),
        (int)elapsed,
        (int) (stats_n_bytes_read/elapsed));
    tor_log(severity, LD_NET,
        "Average bandwidth: "U64_FORMAT"/%d = %d bytes/sec writing",
        U64_PRINTF_ARG(stats_n_bytes_written),
        (int)elapsed,
        (int) (stats_n_bytes_written/elapsed));
  }

  tor_log(severity, LD_NET, "--------------- Dumping memory information:");
  dumpmemusage(severity);

  rep_hist_dump_stats(now,severity);
  rend_service_dump_stats(severity);
  dump_pk_ops(severity);
  dump_distinct_digest_count(severity);
}

/** Called by exit() as we shut down the process.
 */
static void
exit_function(void)
{
  /* NOTE: If we ever daemonize, this gets called immediately.  That's
   * okay for now, because we only use this on Windows.  */
#ifdef _WIN32
  WSACleanup();
#endif
}

#ifdef _WIN32
#define UNIX_ONLY 0
#else
#define UNIX_ONLY 1
#endif
static struct {
  int signal_value;
  int try_to_register;
  struct event *signal_event;
} signal_handlers[] = {
#ifdef SIGINT
  { SIGINT, UNIX_ONLY, NULL }, /* do a controlled slow shutdown */
#endif
#ifdef SIGTERM
  { SIGTERM, UNIX_ONLY, NULL }, /* to terminate now */
#endif
#ifdef SIGPIPE
  { SIGPIPE, UNIX_ONLY, NULL }, /* otherwise SIGPIPE kills us */
#endif
#ifdef SIGUSR1
  { SIGUSR1, UNIX_ONLY, NULL }, /* dump stats */
#endif
#ifdef SIGUSR2
  { SIGUSR2, UNIX_ONLY, NULL }, /* go to loglevel debug */
#endif
#ifdef SIGHUP
  { SIGHUP, UNIX_ONLY, NULL }, /* to reload config, retry conns, etc */
#endif
#ifdef SIGXFSZ
  { SIGXFSZ, UNIX_ONLY, NULL }, /* handle file-too-big resource exhaustion */
#endif
#ifdef SIGCHLD
  { SIGCHLD, UNIX_ONLY, NULL }, /* handle dns/cpu workers that exit */
#endif
  /* These are controller-only */
  { SIGNEWNYM, 0, NULL },
  { SIGCLEARDNSCACHE, 0, NULL },
  { SIGHEARTBEAT, 0, NULL },
  { -1, -1, NULL }
};

/** Set up the signal handlers for either parent or child process */
void
handle_signals(int is_parent)
{
  int i;
  if (is_parent) {
    for (i = 0; signal_handlers[i].signal_value >= 0; ++i) {
      if (signal_handlers[i].try_to_register) {
        signal_handlers[i].signal_event =
          tor_evsignal_new(tor_libevent_get_base(),
                           signal_handlers[i].signal_value,
                           signal_callback,
                           &signal_handlers[i].signal_value);
        if (event_add(signal_handlers[i].signal_event, NULL))
          log_warn(LD_BUG, "Error from libevent when adding "
                   "event for signal %d",
                   signal_handlers[i].signal_value);
      } else {
        signal_handlers[i].signal_event =
          tor_event_new(tor_libevent_get_base(), -1,
                        EV_SIGNAL, signal_callback,
                        &signal_handlers[i].signal_value);
      }
    }
  } else {
#ifndef _WIN32
    struct sigaction action;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_IGN;
    sigaction(SIGINT,  &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGPIPE, &action, NULL);
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);
    sigaction(SIGHUP,  &action, NULL);
#ifdef SIGXFSZ
    sigaction(SIGXFSZ, &action, NULL);
#endif
#endif
  }
}

/* Make sure the signal handler for signal_num will be called. */
void
activate_signal(int signal_num)
{
  int i;
  for (i = 0; signal_handlers[i].signal_value >= 0; ++i) {
    if (signal_handlers[i].signal_value == signal_num) {
      event_active(signal_handlers[i].signal_event, EV_SIGNAL, 1);
      return;
    }
  }
}

/** Main entry point for the Tor command-line client.
 */
int
tor_init(int argc, char *argv[])
{
  char progname[256];
  int quiet = 0;

  time_of_process_start = time(NULL);
  init_connection_lists();
  /* Have the log set up with our application name. */
  tor_snprintf(progname, sizeof(progname), "Tor %s", get_version());
  log_set_application_name(progname);

  /* Set up the crypto nice and early */
  if (crypto_early_init() < 0) {
    log_err(LD_GENERAL, "Unable to initialize the crypto subsystem!");
    return -1;
  }

  /* Initialize the history structures. */
  rep_hist_init();
  /* Initialize the service cache. */
  rend_cache_init();
  addressmap_init(); /* Init the client dns cache. Do it always, since it's
                      * cheap. */

  {
  /* We search for the "quiet" option first, since it decides whether we
   * will log anything at all to the command line. */
    config_line_t *opts = NULL, *cmdline_opts = NULL;
    const config_line_t *cl;
    (void) config_parse_commandline(argc, argv, 1, &opts, &cmdline_opts);
    for (cl = cmdline_opts; cl; cl = cl->next) {
      if (!strcmp(cl->key, "--hush"))
        quiet = 1;
      if (!strcmp(cl->key, "--quiet") ||
          !strcmp(cl->key, "--dump-config"))
        quiet = 2;
      /* The following options imply --hush */
      if (!strcmp(cl->key, "--version") || !strcmp(cl->key, "--digests") ||
          !strcmp(cl->key, "--list-torrc-options") ||
          !strcmp(cl->key, "--library-versions") ||
          !strcmp(cl->key, "--hash-password") ||
          !strcmp(cl->key, "-h") || !strcmp(cl->key, "--help")) {
        if (quiet < 1)
          quiet = 1;
      }
    }
    config_free_lines(opts);
    config_free_lines(cmdline_opts);
  }

 /* give it somewhere to log to initially */
  switch (quiet) {
    case 2:
      /* no initial logging */
      break;
    case 1:
      add_temp_log(LOG_WARN);
      break;
    default:
      add_temp_log(LOG_NOTICE);
  }
  quiet_level = quiet;

  {
    const char *version = get_version();

    log_notice(LD_GENERAL, "Tor %s running on %s with Libevent %s, "
               "OpenSSL %s and Zlib %s.", version,
               get_uname(),
               tor_libevent_get_version_str(),
               crypto_openssl_get_version_str(),
               tor_zlib_get_version_str());

    log_notice(LD_GENERAL, "Tor can't help you if you use it wrong! "
               "Learn how to be safe at "
               "https://www.torproject.org/download/download#warning");

    if (strstr(version, "alpha") || strstr(version, "beta"))
      log_notice(LD_GENERAL, "This version is not a stable Tor release. "
                 "Expect more bugs than usual.");
  }

  if (network_init()<0) {
    log_err(LD_BUG,"Error initializing network; exiting.");
    return -1;
  }
  atexit(exit_function);

  if (options_init_from_torrc(argc,argv) < 0) {
    log_err(LD_CONFIG,"Reading config failed--see warnings above.");
    return -1;
  }

  /* The options are now initialised */
  const or_options_t *options = get_options();

#ifndef _WIN32
  if (geteuid()==0)
    log_warn(LD_GENERAL,"You are running Tor as root. You don't need to, "
             "and you probably shouldn't.");
#endif

  if (crypto_global_init(options->HardwareAccel,
                         options->AccelName,
                         options->AccelDir)) {
    log_err(LD_BUG, "Unable to initialize OpenSSL. Exiting.");
    return -1;
  }
  stream_choice_seed_weak_rng();
  if (tor_init_libevent_rng() < 0) {
    log_warn(LD_NET, "Problem initializing libevent RNG.");
  }

  /* Scan/clean unparseable descroptors; after reading config */
  routerparse_init();

  return 0;
}

/** A lockfile structure, used to prevent two Tors from messing with the
 * data directory at once.  If this variable is non-NULL, we're holding
 * the lockfile. */
static tor_lockfile_t *lockfile = NULL;

/** Try to grab the lock file described in <b>options</b>, if we do not
 * already have it.  If <b>err_if_locked</b> is true, warn if somebody else is
 * holding the lock, and exit if we can't get it after waiting.  Otherwise,
 * return -1 if we can't get the lockfile.  Return 0 on success.
 */
int
try_locking(const or_options_t *options, int err_if_locked)
{
  if (lockfile)
    return 0;
  else {
    char *fname = options_get_datadir_fname2_suffix(options, "lock",NULL,NULL);
    int already_locked = 0;
    tor_lockfile_t *lf = tor_lockfile_lock(fname, 0, &already_locked);
    tor_free(fname);
    if (!lf) {
      if (err_if_locked && already_locked) {
        int r;
        log_warn(LD_GENERAL, "It looks like another Tor process is running "
                 "with the same data directory.  Waiting 5 seconds to see "
                 "if it goes away.");
#ifndef _WIN32
        sleep(5);
#else
        Sleep(5000);
#endif
        r = try_locking(options, 0);
        if (r<0) {
          log_err(LD_GENERAL, "No, it's still there.  Exiting.");
          exit(0);
        }
        return r;
      }
      return -1;
    }
    lockfile = lf;
    return 0;
  }
}

/** Return true iff we've successfully acquired the lock file. */
int
have_lockfile(void)
{
  return lockfile != NULL;
}

/** If we have successfully acquired the lock file, release it. */
void
release_lockfile(void)
{
  if (lockfile) {
    tor_lockfile_unlock(lockfile);
    lockfile = NULL;
  }
}

/** Free all memory that we might have allocated somewhere.
 * If <b>postfork</b>, we are a worker process and we want to free
 * only the parts of memory that we won't touch. If !<b>postfork</b>,
 * Tor is shutting down and we should free everything.
 *
 * Helps us find the real leaks with dmalloc and the like. Also valgrind
 * should then report 0 reachable in its leak report (in an ideal world --
 * in practice libevent, SSL, libc etc never quite free everything). */
void
tor_free_all(int postfork)
{
  if (!postfork) {
    evdns_shutdown(1);
  }
  geoip_free_all();
  dirvote_free_all();
  routerlist_free_all();
  networkstatus_free_all();
  addressmap_free_all();
  dirserv_free_all();
  rend_service_free_all();
  rend_cache_free_all();
  rend_service_authorization_free_all();
  rep_hist_free_all();
  dns_free_all();
  clear_pending_onions();
  circuit_free_all();
  entry_guards_free_all();
  pt_free_all();
  channel_tls_free_all();
  channel_free_all();
  connection_free_all();
  connection_edge_free_all();
  scheduler_free_all();
  nodelist_free_all();
  microdesc_free_all();
  routerparse_free_all();
  ext_orport_free_all();
  control_free_all();
  sandbox_free_getaddrinfo_cache();
  protover_free_all();
  if (!postfork) {
    config_free_all();
    or_state_free_all();
    router_free_all();
    routerkeys_free_all();
    policies_free_all();
  }
  if (!postfork) {
    tor_tls_free_all();
#ifndef _WIN32
    tor_getpwnam(NULL);
#endif
  }
  /* stuff in main.c */

  smartlist_free(connection_array);
  smartlist_free(closeable_connection_lst);
  smartlist_free(active_linked_connection_lst);
  periodic_timer_free(second_timer);
  teardown_periodic_events();
  periodic_timer_free(refill_timer);

  if (!postfork) {
    release_lockfile();
  }
  /* Stuff in util.c and address.c*/
  if (!postfork) {
    escaped(NULL);
    esc_router_info(NULL);
    logs_free_all(); /* free log strings. do this last so logs keep working. */
  }
}

/** Do whatever cleanup is necessary before shutting Tor down. */
void
tor_cleanup(void)
{
  const or_options_t *options = get_options();
  if (options->command == CMD_RUN_TOR) {
    time_t now = time(NULL);
    /* Remove our pid file. We don't care if there was an error when we
     * unlink, nothing we could do about it anyways. */
    if (options->PidFile) {
      if (unlink(options->PidFile) != 0) {
        log_warn(LD_FS, "Couldn't unlink pid file %s: %s",
                 options->PidFile, strerror(errno));
      }
    }
    if (options->ControlPortWriteToFile) {
      if (unlink(options->ControlPortWriteToFile) != 0) {
        log_warn(LD_FS, "Couldn't unlink control port file %s: %s",
                 options->ControlPortWriteToFile,
                 strerror(errno));
      }
    }
    if (accounting_is_enabled(options))
      accounting_record_bandwidth_usage(now, get_or_state());
    or_state_mark_dirty(get_or_state(), 0); /* force an immediate save. */
    or_state_save(now);
    if (authdir_mode(options)) {
      sr_save_and_cleanup();
    }
    if (authdir_mode_tests_reachability(options))
      rep_hist_record_mtbf_data(now, 0);
    keypin_close_journal();
  }
#ifdef USE_DMALLOC
  dmalloc_log_stats();
#endif
  tor_free_all(0); /* We could move tor_free_all back into the ifdef below
                      later, if it makes shutdown unacceptably slow.  But for
                      now, leave it here: it's helped us catch bugs in the
                      past. */
  crypto_global_cleanup();
#ifdef USE_DMALLOC
  dmalloc_log_unfreed();
  dmalloc_shutdown();
#endif
}

/** Read/create keys as needed, and echo our fingerprint to stdout. */
static int
do_list_fingerprint(void)
{
  char buf[FINGERPRINT_LEN+1];
  crypto_pk_t *k;
  const char *nickname = get_options()->Nickname;
  sandbox_disable_getaddrinfo_cache();
  if (!server_mode(get_options())) {
    log_err(LD_GENERAL,
            "Clients don't have long-term identity keys. Exiting.");
    return -1;
  }
  tor_assert(nickname);
  if (init_keys() < 0) {
    log_err(LD_GENERAL,"Error initializing keys; exiting.");
    return -1;
  }
  if (!(k = get_server_identity_key())) {
    log_err(LD_GENERAL,"Error: missing identity key.");
    return -1;
  }
  if (crypto_pk_get_fingerprint(k, buf, 1)<0) {
    log_err(LD_BUG, "Error computing fingerprint");
    return -1;
  }
  printf("%s %s\n", nickname, buf);
  return 0;
}

/** Entry point for password hashing: take the desired password from
 * the command line, and print its salted hash to stdout. **/
static void
do_hash_password(void)
{

  char output[256];
  char key[S2K_RFC2440_SPECIFIER_LEN+DIGEST_LEN];

  crypto_rand(key, S2K_RFC2440_SPECIFIER_LEN-1);
  key[S2K_RFC2440_SPECIFIER_LEN-1] = (uint8_t)96; /* Hash 64 K of data. */
  secret_to_key_rfc2440(key+S2K_RFC2440_SPECIFIER_LEN, DIGEST_LEN,
                get_options()->command_arg, strlen(get_options()->command_arg),
                key);
  base16_encode(output, sizeof(output), key, sizeof(key));
  printf("16:%s\n",output);
}

/** Entry point for configuration dumping: write the configuration to
 * stdout. */
static int
do_dump_config(void)
{
  const or_options_t *options = get_options();
  const char *arg = options->command_arg;
  int how;
  char *opts;

  if (!strcmp(arg, "short")) {
    how = OPTIONS_DUMP_MINIMAL;
  } else if (!strcmp(arg, "non-builtin")) {
    how = OPTIONS_DUMP_DEFAULTS;
  } else if (!strcmp(arg, "full")) {
    how = OPTIONS_DUMP_ALL;
  } else {
    fprintf(stderr, "No valid argument to --dump-config found!\n");
    fprintf(stderr, "Please select 'short', 'non-builtin', or 'full'.\n");

    return -1;
  }

  opts = options_dump(options, how);
  printf("%s", opts);
  tor_free(opts);

  return 0;
}

static void
init_addrinfo(void)
{
  if (! server_mode(get_options()) ||
      (get_options()->Address && strlen(get_options()->Address) > 0)) {
    /* We don't need to seed our own hostname, because we won't be calling
     * resolve_my_address on it.
     */
    return;
  }
  char hname[256];

  // host name to sandbox
  gethostname(hname, sizeof(hname));
  sandbox_add_addrinfo(hname);
}

static sandbox_cfg_t*
sandbox_init_filter(void)
{
  const or_options_t *options = get_options();
  sandbox_cfg_t *cfg = sandbox_cfg_new();
  int i;

  sandbox_cfg_allow_openat_filename(&cfg,
      get_datadir_fname("cached-status"));

#define OPEN(name)                              \
  sandbox_cfg_allow_open_filename(&cfg, tor_strdup(name))

#define OPEN_DATADIR(name)                      \
  sandbox_cfg_allow_open_filename(&cfg, get_datadir_fname(name))

#define OPEN_DATADIR2(name, name2)                       \
  sandbox_cfg_allow_open_filename(&cfg, get_datadir_fname2((name), (name2)))

#define OPEN_DATADIR_SUFFIX(name, suffix) do {  \
    OPEN_DATADIR(name);                         \
    OPEN_DATADIR(name suffix);                  \
  } while (0)

#define OPEN_DATADIR2_SUFFIX(name, name2, suffix) do {  \
    OPEN_DATADIR2(name, name2);                         \
    OPEN_DATADIR2(name, name2 suffix);                  \
  } while (0)

  OPEN(options->DataDirectory);
  OPEN_DATADIR("keys");
  OPEN_DATADIR_SUFFIX("cached-certs", ".tmp");
  OPEN_DATADIR_SUFFIX("cached-consensus", ".tmp");
  OPEN_DATADIR_SUFFIX("unverified-consensus", ".tmp");
  OPEN_DATADIR_SUFFIX("unverified-microdesc-consensus", ".tmp");
  OPEN_DATADIR_SUFFIX("cached-microdesc-consensus", ".tmp");
  OPEN_DATADIR_SUFFIX("cached-microdescs", ".tmp");
  OPEN_DATADIR_SUFFIX("cached-microdescs.new", ".tmp");
  OPEN_DATADIR_SUFFIX("cached-descriptors", ".tmp");
  OPEN_DATADIR_SUFFIX("cached-descriptors.new", ".tmp");
  OPEN_DATADIR("cached-descriptors.tmp.tmp");
  OPEN_DATADIR_SUFFIX("cached-extrainfo", ".tmp");
  OPEN_DATADIR_SUFFIX("cached-extrainfo.new", ".tmp");
  OPEN_DATADIR("cached-extrainfo.tmp.tmp");
  OPEN_DATADIR_SUFFIX("state", ".tmp");
  OPEN_DATADIR_SUFFIX("sr-state", ".tmp");
  OPEN_DATADIR_SUFFIX("unparseable-desc", ".tmp");
  OPEN_DATADIR_SUFFIX("v3-status-votes", ".tmp");
  OPEN_DATADIR("key-pinning-journal");
  OPEN("/dev/srandom");
  OPEN("/dev/urandom");
  OPEN("/dev/random");
  OPEN("/etc/hosts");
  OPEN("/proc/meminfo");

  if (options->BridgeAuthoritativeDir)
    OPEN_DATADIR_SUFFIX("networkstatus-bridges", ".tmp");

  if (authdir_mode_handles_descs(options, -1))
    OPEN_DATADIR("approved-routers");

  if (options->ServerDNSResolvConfFile)
    sandbox_cfg_allow_open_filename(&cfg,
                                tor_strdup(options->ServerDNSResolvConfFile));
  else
    sandbox_cfg_allow_open_filename(&cfg, tor_strdup("/etc/resolv.conf"));

  for (i = 0; i < 2; ++i) {
    if (get_torrc_fname(i)) {
      sandbox_cfg_allow_open_filename(&cfg, tor_strdup(get_torrc_fname(i)));
    }
  }

#define RENAME_SUFFIX(name, suffix)        \
  sandbox_cfg_allow_rename(&cfg,           \
      get_datadir_fname(name suffix),      \
      get_datadir_fname(name))

#define RENAME_SUFFIX2(prefix, name, suffix) \
  sandbox_cfg_allow_rename(&cfg,                                        \
                           get_datadir_fname2(prefix, name suffix),     \
                           get_datadir_fname2(prefix, name))

  RENAME_SUFFIX("cached-certs", ".tmp");
  RENAME_SUFFIX("cached-consensus", ".tmp");
  RENAME_SUFFIX("unverified-consensus", ".tmp");
  RENAME_SUFFIX("unverified-microdesc-consensus", ".tmp");
  RENAME_SUFFIX("cached-microdesc-consensus", ".tmp");
  RENAME_SUFFIX("cached-microdescs", ".tmp");
  RENAME_SUFFIX("cached-microdescs", ".new");
  RENAME_SUFFIX("cached-microdescs.new", ".tmp");
  RENAME_SUFFIX("cached-descriptors", ".tmp");
  RENAME_SUFFIX("cached-descriptors", ".new");
  RENAME_SUFFIX("cached-descriptors.new", ".tmp");
  RENAME_SUFFIX("cached-extrainfo", ".tmp");
  RENAME_SUFFIX("cached-extrainfo", ".new");
  RENAME_SUFFIX("cached-extrainfo.new", ".tmp");
  RENAME_SUFFIX("state", ".tmp");
  RENAME_SUFFIX("sr-state", ".tmp");
  RENAME_SUFFIX("unparseable-desc", ".tmp");
  RENAME_SUFFIX("v3-status-votes", ".tmp");

  if (options->BridgeAuthoritativeDir)
    RENAME_SUFFIX("networkstatus-bridges", ".tmp");

#define STAT_DATADIR(name)                      \
  sandbox_cfg_allow_stat_filename(&cfg, get_datadir_fname(name))

#define STAT_DATADIR2(name, name2)                                      \
  sandbox_cfg_allow_stat_filename(&cfg, get_datadir_fname2((name), (name2)))

  STAT_DATADIR(NULL);
  STAT_DATADIR("lock");
  STAT_DATADIR("state");
  STAT_DATADIR("router-stability");
  STAT_DATADIR("cached-extrainfo.new");

  {
    smartlist_t *files = smartlist_new();
    tor_log_get_logfile_names(files);
    SMARTLIST_FOREACH(files, char *, file_name, {
      /* steals reference */
      sandbox_cfg_allow_open_filename(&cfg, file_name);
    });
    smartlist_free(files);
  }

  {
    smartlist_t *files = smartlist_new();
    smartlist_t *dirs = smartlist_new();
    rend_services_add_filenames_to_lists(files, dirs);
    SMARTLIST_FOREACH(files, char *, file_name, {
      char *tmp_name = NULL;
      tor_asprintf(&tmp_name, "%s.tmp", file_name);
      sandbox_cfg_allow_rename(&cfg,
                               tor_strdup(tmp_name), tor_strdup(file_name));
      /* steals references */
      sandbox_cfg_allow_open_filename(&cfg, file_name);
      sandbox_cfg_allow_open_filename(&cfg, tmp_name);
    });
    SMARTLIST_FOREACH(dirs, char *, dir, {
      /* steals reference */
      sandbox_cfg_allow_stat_filename(&cfg, dir);
    });
    smartlist_free(files);
    smartlist_free(dirs);
  }

  {
    char *fname;
    if ((fname = get_controller_cookie_file_name())) {
      sandbox_cfg_allow_open_filename(&cfg, fname);
    }
    if ((fname = get_ext_or_auth_cookie_file_name())) {
      sandbox_cfg_allow_open_filename(&cfg, fname);
    }
  }

  SMARTLIST_FOREACH_BEGIN(get_configured_ports(), port_cfg_t *, port) {
    if (!port->is_unix_addr)
      continue;
    /* When we open an AF_UNIX address, we want permission to open the
     * directory that holds it. */
    char *dirname = tor_strdup(port->unix_addr);
    if (get_parent_directory(dirname) == 0) {
      OPEN(dirname);
    }
    tor_free(dirname);
    sandbox_cfg_allow_chmod_filename(&cfg, tor_strdup(port->unix_addr));
    sandbox_cfg_allow_chown_filename(&cfg, tor_strdup(port->unix_addr));
  } SMARTLIST_FOREACH_END(port);

  if (options->DirPortFrontPage) {
    sandbox_cfg_allow_open_filename(&cfg,
                                    tor_strdup(options->DirPortFrontPage));
  }

  // orport
  if (server_mode(get_options())) {

    OPEN_DATADIR2_SUFFIX("keys", "secret_id_key", ".tmp");
    OPEN_DATADIR2_SUFFIX("keys", "secret_onion_key", ".tmp");
    OPEN_DATADIR2_SUFFIX("keys", "secret_onion_key_ntor", ".tmp");
    OPEN_DATADIR2("keys", "secret_id_key.old");
    OPEN_DATADIR2("keys", "secret_onion_key.old");
    OPEN_DATADIR2("keys", "secret_onion_key_ntor.old");

    OPEN_DATADIR2_SUFFIX("keys", "ed25519_master_id_secret_key", ".tmp");
    OPEN_DATADIR2_SUFFIX("keys", "ed25519_master_id_secret_key_encrypted",
                         ".tmp");
    OPEN_DATADIR2_SUFFIX("keys", "ed25519_master_id_public_key", ".tmp");
    OPEN_DATADIR2_SUFFIX("keys", "ed25519_signing_secret_key", ".tmp");
    OPEN_DATADIR2_SUFFIX("keys", "ed25519_signing_secret_key_encrypted",
                         ".tmp");
    OPEN_DATADIR2_SUFFIX("keys", "ed25519_signing_public_key", ".tmp");
    OPEN_DATADIR2_SUFFIX("keys", "ed25519_signing_cert", ".tmp");

    OPEN_DATADIR2_SUFFIX("stats", "bridge-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "dirreq-stats", ".tmp");

    OPEN_DATADIR2_SUFFIX("stats", "entry-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "exit-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "buffer-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "conn-stats", ".tmp");
    OPEN_DATADIR2_SUFFIX("stats", "hidserv-stats", ".tmp");

    OPEN_DATADIR("approved-routers");
    OPEN_DATADIR_SUFFIX("fingerprint", ".tmp");
    OPEN_DATADIR_SUFFIX("hashed-fingerprint", ".tmp");
    OPEN_DATADIR_SUFFIX("router-stability", ".tmp");

    OPEN("/etc/resolv.conf");

    RENAME_SUFFIX("fingerprint", ".tmp");
    RENAME_SUFFIX2("keys", "secret_onion_key_ntor", ".tmp");
    RENAME_SUFFIX2("keys", "secret_id_key", ".tmp");
    RENAME_SUFFIX2("keys", "secret_id_key.old", ".tmp");
    RENAME_SUFFIX2("keys", "secret_onion_key", ".tmp");
    RENAME_SUFFIX2("keys", "secret_onion_key.old", ".tmp");
    RENAME_SUFFIX2("stats", "bridge-stats", ".tmp");
    RENAME_SUFFIX2("stats", "dirreq-stats", ".tmp");
    RENAME_SUFFIX2("stats", "entry-stats", ".tmp");
    RENAME_SUFFIX2("stats", "exit-stats", ".tmp");
    RENAME_SUFFIX2("stats", "buffer-stats", ".tmp");
    RENAME_SUFFIX2("stats", "conn-stats", ".tmp");
    RENAME_SUFFIX2("stats", "hidserv-stats", ".tmp");
    RENAME_SUFFIX("hashed-fingerprint", ".tmp");
    RENAME_SUFFIX("router-stability", ".tmp");

    RENAME_SUFFIX2("keys", "ed25519_master_id_secret_key", ".tmp");
    RENAME_SUFFIX2("keys", "ed25519_master_id_secret_key_encrypted", ".tmp");
    RENAME_SUFFIX2("keys", "ed25519_master_id_public_key", ".tmp");
    RENAME_SUFFIX2("keys", "ed25519_signing_secret_key", ".tmp");
    RENAME_SUFFIX2("keys", "ed25519_signing_cert", ".tmp");

    sandbox_cfg_allow_rename(&cfg,
             get_datadir_fname2("keys", "secret_onion_key"),
             get_datadir_fname2("keys", "secret_onion_key.old"));
    sandbox_cfg_allow_rename(&cfg,
             get_datadir_fname2("keys", "secret_onion_key_ntor"),
             get_datadir_fname2("keys", "secret_onion_key_ntor.old"));

    STAT_DATADIR("keys");
    OPEN_DATADIR("stats");
    STAT_DATADIR("stats");
    STAT_DATADIR2("stats", "dirreq-stats");
  }

  init_addrinfo();

  return cfg;
}

/** Main entry point for the Tor process.  Called from main(). */
/* This function is distinct from main() only so we can link main.c into
 * the unittest binary without conflicting with the unittests' main. */
int
tor_main(int argc, char *argv[])
{
  int result = 0;

#ifdef _WIN32
#ifndef HeapEnableTerminationOnCorruption
#define HeapEnableTerminationOnCorruption 1
#endif
  /* On heap corruption, just give up; don't try to play along. */
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
  /* Call SetProcessDEPPolicy to permanently enable DEP.
     The function will not resolve on earlier versions of Windows,
     and failure is not dangerous. */
  HMODULE hMod = GetModuleHandleA("Kernel32.dll");
  if (hMod) {
    typedef BOOL (WINAPI *PSETDEP)(DWORD);
    PSETDEP setdeppolicy = (PSETDEP)GetProcAddress(hMod,
                           "SetProcessDEPPolicy");
    if (setdeppolicy) {
      /* PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION */
      setdeppolicy(3);
    }
  }
#endif

  configure_backtrace_handler(get_version());

  update_approx_time(time(NULL));
  tor_threads_init();
  init_logging(0);
#ifdef USE_DMALLOC
  {
    /* Instruct OpenSSL to use our internal wrappers for malloc,
       realloc and free. */
    int r = CRYPTO_set_mem_ex_functions(tor_malloc_, tor_realloc_, tor_free_);
    tor_assert(r);
  }
#endif
#ifdef NT_SERVICE
  {
     int done = 0;
     result = nt_service_parse_options(argc, argv, &done);
     if (done) return result;
  }
#endif
  if (tor_init(argc, argv)<0)
    return -1;

  if (get_options()->Sandbox && get_options()->command == CMD_RUN_TOR) {
    sandbox_cfg_t* cfg = sandbox_init_filter();

    if (sandbox_init(cfg)) {
      log_err(LD_BUG,"Failed to create syscall sandbox filter");
      return -1;
    }

    // registering libevent rng
#ifdef HAVE_EVUTIL_SECURE_RNG_SET_URANDOM_DEVICE_FILE
    evutil_secure_rng_set_urandom_device_file(
        (char*) sandbox_intern_string("/dev/urandom"));
#endif
  }

  monotime_init();

  switch (get_options()->command) {
  case CMD_RUN_TOR:
#ifdef NT_SERVICE
    nt_service_set_state(SERVICE_RUNNING);
#endif
    result = do_main_loop();
    break;
  case CMD_KEYGEN:
    result = load_ed_keys(get_options(), time(NULL));
    break;
  case CMD_LIST_FINGERPRINT:
    result = do_list_fingerprint();
    break;
  case CMD_HASH_PASSWORD:
    do_hash_password();
    result = 0;
    break;
  case CMD_VERIFY_CONFIG:
    if (quiet_level == 0)
      printf("Configuration was valid\n");
    result = 0;
    break;
  case CMD_DUMP_CONFIG:
    result = do_dump_config();
    break;
  case CMD_RUN_UNITTESTS: /* only set by test.c */
  default:
    log_warn(LD_BUG,"Illegal command number %d: internal error.",
             get_options()->command);
    result = -1;
  }
  tor_cleanup();
  return result;
}

