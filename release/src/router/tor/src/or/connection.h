/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file connection.h
 * \brief Header file for connection.c.
 **/

#ifndef TOR_CONNECTION_H
#define TOR_CONNECTION_H

/* XXXX For buf_datalen in inline function */
#include "buffers.h"

const char *conn_type_to_string(int type);
const char *conn_state_to_string(int type, int state);
int conn_listener_type_supports_af_unix(int type);

dir_connection_t *dir_connection_new(int socket_family);
or_connection_t *or_connection_new(int type, int socket_family);
edge_connection_t *edge_connection_new(int type, int socket_family);
entry_connection_t *entry_connection_new(int type, int socket_family);
control_connection_t *control_connection_new(int socket_family);
listener_connection_t *listener_connection_new(int type, int socket_family);
connection_t *connection_new(int type, int socket_family);

void connection_link_connections(connection_t *conn_a, connection_t *conn_b);
MOCK_DECL(void,connection_free,(connection_t *conn));
void connection_free_all(void);
void connection_about_to_close_connection(connection_t *conn);
void connection_close_immediate(connection_t *conn);
void connection_mark_for_close_(connection_t *conn,
                                int line, const char *file);
MOCK_DECL(void, connection_mark_for_close_internal_,
          (connection_t *conn, int line, const char *file));

#define connection_mark_for_close(c) \
  connection_mark_for_close_((c), __LINE__, SHORT_FILE__)
#define connection_mark_for_close_internal(c) \
  connection_mark_for_close_internal_((c), __LINE__, SHORT_FILE__)

/**
 * Mark 'c' for close, but try to hold it open until all the data is written.
 * Use the _internal versions of connection_mark_for_close; this should be
 * called when you either are sure that if this is an or_connection_t the
 * controlling channel has been notified (e.g. with
 * connection_or_notify_error()), or you actually are the
 * connection_or_close_for_error() or connection_or_close_normally function.
 * For all other cases, use connection_mark_and_flush() instead, which
 * checks for or_connection_t properly, instead.  See below.
 */
#define connection_mark_and_flush_internal_(c,line,file)                \
  do {                                                                  \
    connection_t *tmp_conn__ = (c);                                     \
    connection_mark_for_close_internal_(tmp_conn__, (line), (file));    \
    tmp_conn__->hold_open_until_flushed = 1;                            \
  } while (0)

#define connection_mark_and_flush_internal(c)            \
  connection_mark_and_flush_internal_((c), __LINE__, SHORT_FILE__)

/**
 * Mark 'c' for close, but try to hold it open until all the data is written.
 */
#define connection_mark_and_flush_(c,line,file)                           \
  do {                                                                    \
    connection_t *tmp_conn_ = (c);                                        \
    if (tmp_conn_->type == CONN_TYPE_OR) {                                \
      log_warn(LD_CHANNEL | LD_BUG,                                       \
               "Something tried to close (and flush) an or_connection_t"  \
               " without going through channels at %s:%d",                \
               file, line);                                               \
      connection_or_close_for_error(TO_OR_CONN(tmp_conn_), 1);            \
    } else {                                                              \
      connection_mark_and_flush_internal_(c, line, file);                 \
    }                                                                     \
  } while (0)

#define connection_mark_and_flush(c)            \
  connection_mark_and_flush_((c), __LINE__, SHORT_FILE__)

void connection_expire_held_open(void);

int connection_connect(connection_t *conn, const char *address,
                       const tor_addr_t *addr,
                       uint16_t port, int *socket_error);

#ifdef HAVE_SYS_UN_H

int connection_connect_unix(connection_t *conn, const char *socket_path,
                            int *socket_error);

#endif /* defined(HAVE_SYS_UN_H) */

/** Maximum size of information that we can fit into SOCKS5 username
    or password fields. */
#define MAX_SOCKS5_AUTH_FIELD_SIZE 255

/** Total maximum size of information that we can fit into SOCKS5
    username and password fields. */
#define MAX_SOCKS5_AUTH_SIZE_TOTAL 2*MAX_SOCKS5_AUTH_FIELD_SIZE

int connection_proxy_connect(connection_t *conn, int type);
int connection_read_proxy_handshake(connection_t *conn);
void log_failed_proxy_connection(connection_t *conn);
int get_proxy_addrport(tor_addr_t *addr, uint16_t *port, int *proxy_type,
                       const connection_t *conn);

int retry_all_listeners(smartlist_t *replaced_conns,
                        smartlist_t *new_conns,
                        int close_all_noncontrol);

void connection_mark_all_noncontrol_listeners(void);
void connection_mark_all_noncontrol_connections(void);

ssize_t connection_bucket_write_limit(connection_t *conn, time_t now);
int global_write_bucket_low(connection_t *conn, size_t attempt, int priority);
void connection_bucket_init(void);
void connection_bucket_refill(int seconds_elapsed, time_t now);

int connection_handle_read(connection_t *conn);

int connection_fetch_from_buf(char *string, size_t len, connection_t *conn);
int connection_fetch_from_buf_line(connection_t *conn, char *data,
                                   size_t *data_len);
int connection_fetch_from_buf_http(connection_t *conn,
                               char **headers_out, size_t max_headerlen,
                               char **body_out, size_t *body_used,
                               size_t max_bodylen, int force_complete);

int connection_wants_to_flush(connection_t *conn);
int connection_outbuf_too_full(connection_t *conn);
int connection_handle_write(connection_t *conn, int force);
int connection_flush(connection_t *conn);

MOCK_DECL(void, connection_write_to_buf_impl_,
          (const char *string, size_t len, connection_t *conn, int zlib));
/* DOCDOC connection_write_to_buf */
static void connection_write_to_buf(const char *string, size_t len,
                                    connection_t *conn);
/* DOCDOC connection_write_to_buf_zlib */
static void connection_write_to_buf_zlib(const char *string, size_t len,
                                         dir_connection_t *conn, int done);
static inline void
connection_write_to_buf(const char *string, size_t len, connection_t *conn)
{
  connection_write_to_buf_impl_(string, len, conn, 0);
}
static inline void
connection_write_to_buf_zlib(const char *string, size_t len,
                             dir_connection_t *conn, int done)
{
  connection_write_to_buf_impl_(string, len, TO_CONN(conn), done ? -1 : 1);
}

/* DOCDOC connection_get_inbuf_len */
static size_t connection_get_inbuf_len(connection_t *conn);
/* DOCDOC connection_get_outbuf_len */
static size_t connection_get_outbuf_len(connection_t *conn);

static inline size_t
connection_get_inbuf_len(connection_t *conn)
{
  return conn->inbuf ? buf_datalen(conn->inbuf) : 0;
}

static inline size_t
connection_get_outbuf_len(connection_t *conn)
{
    return conn->outbuf ? buf_datalen(conn->outbuf) : 0;
}

connection_t *connection_get_by_global_id(uint64_t id);

connection_t *connection_get_by_type(int type);
MOCK_DECL(connection_t *,connection_get_by_type_addr_port_purpose,(int type,
                                                  const tor_addr_t *addr,
                                                  uint16_t port, int purpose));
connection_t *connection_get_by_type_state(int type, int state);
connection_t *connection_get_by_type_state_rendquery(int type, int state,
                                                     const char *rendquery);
smartlist_t *connection_dir_list_by_purpose_and_resource(
                                                  int purpose,
                                                  const char *resource);
smartlist_t *connection_dir_list_by_purpose_resource_and_state(
                                                  int purpose,
                                                  const char *resource,
                                                  int state);

#define CONN_LEN_AND_FREE_TEMPLATE(sl) \
  STMT_BEGIN                           \
    int len = smartlist_len(sl);       \
    smartlist_free(sl);                \
    return len;                        \
  STMT_END

/** Return a count of directory connections that are fetching the item
 * described by <b>purpose</b>/<b>resource</b>. */
static inline int
connection_dir_count_by_purpose_and_resource(
                                             int purpose,
                                             const char *resource)
{
  smartlist_t *conns = connection_dir_list_by_purpose_and_resource(
                                                                   purpose,
                                                                   resource);
  CONN_LEN_AND_FREE_TEMPLATE(conns);
}

/** Return a count of directory connections that are fetching the item
 * described by <b>purpose</b>/<b>resource</b>/<b>state</b>. */
static inline int
connection_dir_count_by_purpose_resource_and_state(
                                                   int purpose,
                                                   const char *resource,
                                                   int state)
{
  smartlist_t *conns =
    connection_dir_list_by_purpose_resource_and_state(
                                                      purpose,
                                                      resource,
                                                      state);
  CONN_LEN_AND_FREE_TEMPLATE(conns);
}

#undef CONN_LEN_AND_FREE_TEMPLATE

int any_other_active_or_conns(const or_connection_t *this_conn);

/* || 0 is for -Wparentheses-equality (-Wall?) appeasement under clang */
#define connection_speaks_cells(conn) (((conn)->type == CONN_TYPE_OR) || 0)
int connection_is_listener(connection_t *conn);
int connection_state_is_open(connection_t *conn);
int connection_state_is_connecting(connection_t *conn);

char *alloc_http_authenticator(const char *authenticator);

void assert_connection_ok(connection_t *conn, time_t now);
int connection_or_nonopen_was_started_here(or_connection_t *conn);
void connection_dump_buffer_mem_stats(int severity);
void remove_file_if_very_old(const char *fname, time_t now);

void clock_skew_warning(const connection_t *conn, long apparent_skew,
                        int trusted, log_domain_mask_t domain,
                        const char *received, const char *source);

/** Check if a connection is on the way out so the OOS handler doesn't try
 * to kill more than it needs. */
static inline int
connection_is_moribund(connection_t *conn)
{
  if (conn != NULL &&
      (conn->conn_array_index < 0 ||
       conn->marked_for_close)) {
    return 1;
  } else {
    return 0;
  }
}

void connection_check_oos(int n_socks, int failed);

#ifdef CONNECTION_PRIVATE
STATIC void connection_free_(connection_t *conn);

/* Used only by connection.c and test*.c */
uint32_t bucket_millis_empty(int tokens_before, uint32_t last_empty_time,
                             int tokens_after, int milliseconds_elapsed,
                             const struct timeval *tvnow);
void connection_buckets_note_empty_ts(uint32_t *timestamp_var,
                                      int tokens_before,
                                      size_t tokens_removed,
                                      const struct timeval *tvnow);
MOCK_DECL(STATIC int,connection_connect_sockaddr,
                                            (connection_t *conn,
                                             const struct sockaddr *sa,
                                             socklen_t sa_len,
                                             const struct sockaddr *bindaddr,
                                             socklen_t bindaddr_len,
                                             int *socket_error));
MOCK_DECL(STATIC void, kill_conn_list_for_oos, (smartlist_t *conns));
MOCK_DECL(STATIC smartlist_t *, pick_oos_victims, (int n));

#endif

#endif

