/* Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file control.c
 * \brief Implementation for Tor's control-socket interface.
 *   See doc/spec/control-spec.txt for full details on protocol.
 **/

#define CONTROL_PRIVATE

#include "or.h"
#include "addressmap.h"
#include "buffers.h"
#include "channel.h"
#include "channeltls.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuitstats.h"
#include "circuituse.h"
#include "command.h"
#include "config.h"
#include "confparse.h"
#include "connection.h"
#include "connection_edge.h"
#include "connection_or.h"
#include "control.h"
#include "directory.h"
#include "dirserv.h"
#include "dnsserv.h"
#include "entrynodes.h"
#include "geoip.h"
#include "hibernate.h"
#include "main.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "policies.h"
#include "reasons.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"
#include "routerparse.h"

#ifndef _WIN32
#include <pwd.h>
#include <sys/resource.h>
#endif

#include "procmon.h"

/** Yield true iff <b>s</b> is the state of a control_connection_t that has
 * finished authentication and is accepting commands. */
#define STATE_IS_OPEN(s) ((s) == CONTROL_CONN_STATE_OPEN)

/** Bitfield: The bit 1&lt;&lt;e is set if <b>any</b> open control
 * connection is interested in events of type <b>e</b>.  We use this
 * so that we can decide to skip generating event messages that nobody
 * has interest in without having to walk over the global connection
 * list to find out.
 **/
typedef uint64_t event_mask_t;

/** An event mask of all the events that any controller is interested in
 * receiving. */
static event_mask_t global_event_mask = 0;

/** True iff we have disabled log messages from being sent to the controller */
static int disable_log_messages = 0;

/** Macro: true if any control connection is interested in events of type
 * <b>e</b>. */
#define EVENT_IS_INTERESTING(e) \
  (!! (global_event_mask & (((uint64_t)1)<<(e))))

/** If we're using cookie-type authentication, how long should our cookies be?
 */
#define AUTHENTICATION_COOKIE_LEN 32

/** If true, we've set authentication_cookie to a secret code and
 * stored it to disk. */
static int authentication_cookie_is_set = 0;
/** If authentication_cookie_is_set, a secret cookie that we've stored to disk
 * and which we're using to authenticate controllers.  (If the controller can
 * read it off disk, it has permission to connect.) */
static uint8_t *authentication_cookie = NULL;

#define SAFECOOKIE_SERVER_TO_CONTROLLER_CONSTANT \
  "Tor safe cookie authentication server-to-controller hash"
#define SAFECOOKIE_CONTROLLER_TO_SERVER_CONSTANT \
  "Tor safe cookie authentication controller-to-server hash"
#define SAFECOOKIE_SERVER_NONCE_LEN DIGEST256_LEN

/** A sufficiently large size to record the last bootstrap phase string. */
#define BOOTSTRAP_MSG_LEN 1024

/** What was the last bootstrap phase message we sent? We keep track
 * of this so we can respond to getinfo status/bootstrap-phase queries. */
static char last_sent_bootstrap_message[BOOTSTRAP_MSG_LEN];

static void connection_printf_to_buf(control_connection_t *conn,
                                     const char *format, ...)
  CHECK_PRINTF(2,3);
static void send_control_event_impl(uint16_t event, event_format_t which,
                                    const char *format, va_list ap)
  CHECK_PRINTF(3,0);
static int control_event_status(int type, int severity, const char *format,
                                va_list args)
  CHECK_PRINTF(3,0);

static void send_control_done(control_connection_t *conn);
static void send_control_event(uint16_t event, event_format_t which,
                               const char *format, ...)
  CHECK_PRINTF(3,4);
static int handle_control_setconf(control_connection_t *conn, uint32_t len,
                                  char *body);
static int handle_control_resetconf(control_connection_t *conn, uint32_t len,
                                    char *body);
static int handle_control_getconf(control_connection_t *conn, uint32_t len,
                                  const char *body);
static int handle_control_loadconf(control_connection_t *conn, uint32_t len,
                                  const char *body);
static int handle_control_setevents(control_connection_t *conn, uint32_t len,
                                    const char *body);
static int handle_control_authenticate(control_connection_t *conn,
                                       uint32_t len,
                                       const char *body);
static int handle_control_signal(control_connection_t *conn, uint32_t len,
                                 const char *body);
static int handle_control_mapaddress(control_connection_t *conn, uint32_t len,
                                     const char *body);
static char *list_getinfo_options(void);
static int handle_control_getinfo(control_connection_t *conn, uint32_t len,
                                  const char *body);
static int handle_control_extendcircuit(control_connection_t *conn,
                                        uint32_t len,
                                        const char *body);
static int handle_control_setcircuitpurpose(control_connection_t *conn,
                                            uint32_t len, const char *body);
static int handle_control_attachstream(control_connection_t *conn,
                                       uint32_t len,
                                        const char *body);
static int handle_control_postdescriptor(control_connection_t *conn,
                                         uint32_t len,
                                         const char *body);
static int handle_control_redirectstream(control_connection_t *conn,
                                         uint32_t len,
                                         const char *body);
static int handle_control_closestream(control_connection_t *conn, uint32_t len,
                                      const char *body);
static int handle_control_closecircuit(control_connection_t *conn,
                                       uint32_t len,
                                       const char *body);
static int handle_control_resolve(control_connection_t *conn, uint32_t len,
                                  const char *body);
static int handle_control_usefeature(control_connection_t *conn,
                                     uint32_t len,
                                     const char *body);
static int write_stream_target_to_buf(entry_connection_t *conn, char *buf,
                                      size_t len);
static void orconn_target_get_name(char *buf, size_t len,
                                   or_connection_t *conn);

/** Given a control event code for a message event, return the corresponding
 * log severity. */
static INLINE int
event_to_log_severity(int event)
{
  switch (event) {
    case EVENT_DEBUG_MSG: return LOG_DEBUG;
    case EVENT_INFO_MSG: return LOG_INFO;
    case EVENT_NOTICE_MSG: return LOG_NOTICE;
    case EVENT_WARN_MSG: return LOG_WARN;
    case EVENT_ERR_MSG: return LOG_ERR;
    default: return -1;
  }
}

/** Given a log severity, return the corresponding control event code. */
static INLINE int
log_severity_to_event(int severity)
{
  switch (severity) {
    case LOG_DEBUG: return EVENT_DEBUG_MSG;
    case LOG_INFO: return EVENT_INFO_MSG;
    case LOG_NOTICE: return EVENT_NOTICE_MSG;
    case LOG_WARN: return EVENT_WARN_MSG;
    case LOG_ERR: return EVENT_ERR_MSG;
    default: return -1;
  }
}

/** Helper: clear bandwidth counters of all origin circuits. */
static void
clear_circ_bw_fields(void)
{
  circuit_t *circ;
  origin_circuit_t *ocirc;
  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    if (!CIRCUIT_IS_ORIGIN(circ))
      continue;
    ocirc = TO_ORIGIN_CIRCUIT(circ);
    ocirc->n_written_circ_bw = ocirc->n_read_circ_bw = 0;
  }
}

/** Set <b>global_event_mask*</b> to the bitwise OR of each live control
 * connection's event_mask field. */
void
control_update_global_event_mask(void)
{
  smartlist_t *conns = get_connection_array();
  event_mask_t old_mask, new_mask;
  old_mask = global_event_mask;

  global_event_mask = 0;
  SMARTLIST_FOREACH(conns, connection_t *, _conn,
  {
    if (_conn->type == CONN_TYPE_CONTROL &&
        STATE_IS_OPEN(_conn->state)) {
      control_connection_t *conn = TO_CONTROL_CONN(_conn);
      global_event_mask |= conn->event_mask;
    }
  });

  new_mask = global_event_mask;

  /* Handle the aftermath.  Set up the log callback to tell us only what
   * we want to hear...*/
  control_adjust_event_log_severity();

  /* ...then, if we've started logging stream or circ bw, clear the
   * appropriate fields. */
  if (! (old_mask & EVENT_STREAM_BANDWIDTH_USED) &&
      (new_mask & EVENT_STREAM_BANDWIDTH_USED)) {
    SMARTLIST_FOREACH(conns, connection_t *, conn,
    {
      if (conn->type == CONN_TYPE_AP) {
        edge_connection_t *edge_conn = TO_EDGE_CONN(conn);
        edge_conn->n_written = edge_conn->n_read = 0;
      }
    });
  }
  if (! (old_mask & EVENT_CIRC_BANDWIDTH_USED) &&
      (new_mask & EVENT_CIRC_BANDWIDTH_USED)) {
    clear_circ_bw_fields();
  }
}

/** Adjust the log severities that result in control_event_logmsg being called
 * to match the severity of log messages that any controllers are interested
 * in. */
void
control_adjust_event_log_severity(void)
{
  int i;
  int min_log_event=EVENT_ERR_MSG, max_log_event=EVENT_DEBUG_MSG;

  for (i = EVENT_DEBUG_MSG; i <= EVENT_ERR_MSG; ++i) {
    if (EVENT_IS_INTERESTING(i)) {
      min_log_event = i;
      break;
    }
  }
  for (i = EVENT_ERR_MSG; i >= EVENT_DEBUG_MSG; --i) {
    if (EVENT_IS_INTERESTING(i)) {
      max_log_event = i;
      break;
    }
  }
  if (EVENT_IS_INTERESTING(EVENT_STATUS_GENERAL)) {
    if (min_log_event > EVENT_NOTICE_MSG)
      min_log_event = EVENT_NOTICE_MSG;
    if (max_log_event < EVENT_ERR_MSG)
      max_log_event = EVENT_ERR_MSG;
  }
  if (min_log_event <= max_log_event)
    change_callback_log_severity(event_to_log_severity(min_log_event),
                                 event_to_log_severity(max_log_event),
                                 control_event_logmsg);
  else
    change_callback_log_severity(LOG_ERR, LOG_ERR,
                                 control_event_logmsg);
}

/** Return true iff the event with code <b>c</b> is being sent to any current
 * control connection.  This is useful if the amount of work needed to prepare
 * to call the appropriate control_event_...() function is high.
 */
int
control_event_is_interesting(int event)
{
  return EVENT_IS_INTERESTING(event);
}

/** Append a NUL-terminated string <b>s</b> to the end of
 * <b>conn</b>-\>outbuf.
 */
static INLINE void
connection_write_str_to_buf(const char *s, control_connection_t *conn)
{
  size_t len = strlen(s);
  connection_write_to_buf(s, len, TO_CONN(conn));
}

/** Given a <b>len</b>-character string in <b>data</b>, made of lines
 * terminated by CRLF, allocate a new string in *<b>out</b>, and copy the
 * contents of <b>data</b> into *<b>out</b>, adding a period before any period
 * that appears at the start of a line, and adding a period-CRLF line at
 * the end. Replace all LF characters sequences with CRLF.  Return the number
 * of bytes in *<b>out</b>.
 */
STATIC size_t
write_escaped_data(const char *data, size_t len, char **out)
{
  size_t sz_out = len+8;
  char *outp;
  const char *start = data, *end;
  int i;
  int start_of_line;
  for (i=0; i<(int)len; ++i) {
    if (data[i]== '\n')
      sz_out += 2; /* Maybe add a CR; maybe add a dot. */
  }
  *out = outp = tor_malloc(sz_out+1);
  end = data+len;
  start_of_line = 1;
  while (data < end) {
    if (*data == '\n') {
      if (data > start && data[-1] != '\r')
        *outp++ = '\r';
      start_of_line = 1;
    } else if (*data == '.') {
      if (start_of_line) {
        start_of_line = 0;
        *outp++ = '.';
      }
    } else {
      start_of_line = 0;
    }
    *outp++ = *data++;
  }
  if (outp < *out+2 || fast_memcmp(outp-2, "\r\n", 2)) {
    *outp++ = '\r';
    *outp++ = '\n';
  }
  *outp++ = '.';
  *outp++ = '\r';
  *outp++ = '\n';
  *outp = '\0'; /* NUL-terminate just in case. */
  tor_assert((outp - *out) <= (int)sz_out);
  return outp - *out;
}

/** Given a <b>len</b>-character string in <b>data</b>, made of lines
 * terminated by CRLF, allocate a new string in *<b>out</b>, and copy
 * the contents of <b>data</b> into *<b>out</b>, removing any period
 * that appears at the start of a line, and replacing all CRLF sequences
 * with LF.   Return the number of
 * bytes in *<b>out</b>. */
STATIC size_t
read_escaped_data(const char *data, size_t len, char **out)
{
  char *outp;
  const char *next;
  const char *end;

  *out = outp = tor_malloc(len+1);

  end = data+len;

  while (data < end) {
    /* we're at the start of a line. */
    if (*data == '.')
      ++data;
    next = memchr(data, '\n', end-data);
    if (next) {
      size_t n_to_copy = next-data;
      /* Don't copy a CR that precedes this LF. */
      if (n_to_copy && *(next-1) == '\r')
        --n_to_copy;
      memcpy(outp, data, n_to_copy);
      outp += n_to_copy;
      data = next+1; /* This will point at the start of the next line,
                      * or the end of the string, or a period. */
    } else {
      memcpy(outp, data, end-data);
      outp += (end-data);
      *outp = '\0';
      return outp - *out;
    }
    *outp++ = '\n';
  }

  *outp = '\0';
  return outp - *out;
}

/** If the first <b>in_len_max</b> characters in <b>start</b> contain a
 * double-quoted string with escaped characters, return the length of that
 * string (as encoded, including quotes).  Otherwise return -1. */
static INLINE int
get_escaped_string_length(const char *start, size_t in_len_max,
                          int *chars_out)
{
  const char *cp, *end;
  int chars = 0;

  if (*start != '\"')
    return -1;

  cp = start+1;
  end = start+in_len_max;

  /* Calculate length. */
  while (1) {
    if (cp >= end) {
      return -1; /* Too long. */
    } else if (*cp == '\\') {
      if (++cp == end)
        return -1; /* Can't escape EOS. */
      ++cp;
      ++chars;
    } else if (*cp == '\"') {
      break;
    } else {
      ++cp;
      ++chars;
    }
  }
  if (chars_out)
    *chars_out = chars;
  return (int)(cp - start+1);
}

/** As decode_escaped_string, but does not decode the string: copies the
 * entire thing, including quotation marks. */
static const char *
extract_escaped_string(const char *start, size_t in_len_max,
                       char **out, size_t *out_len)
{
  int length = get_escaped_string_length(start, in_len_max, NULL);
  if (length<0)
    return NULL;
  *out_len = length;
  *out = tor_strndup(start, *out_len);
  return start+length;
}

/** Given a pointer to a string starting at <b>start</b> containing
 * <b>in_len_max</b> characters, decode a string beginning with one double
 * quote, containing any number of non-quote characters or characters escaped
 * with a backslash, and ending with a final double quote.  Place the resulting
 * string (unquoted, unescaped) into a newly allocated string in *<b>out</b>;
 * store its length in <b>out_len</b>.  On success, return a pointer to the
 * character immediately following the escaped string.  On failure, return
 * NULL. */
static const char *
decode_escaped_string(const char *start, size_t in_len_max,
                   char **out, size_t *out_len)
{
  const char *cp, *end;
  char *outp;
  int len, n_chars = 0;

  len = get_escaped_string_length(start, in_len_max, &n_chars);
  if (len<0)
    return NULL;

  end = start+len-1; /* Index of last quote. */
  tor_assert(*end == '\"');
  outp = *out = tor_malloc(len+1);
  *out_len = n_chars;

  cp = start+1;
  while (cp < end) {
    if (*cp == '\\')
      ++cp;
    *outp++ = *cp++;
  }
  *outp = '\0';
  tor_assert((outp - *out) == (int)*out_len);

  return end+1;
}

/** Acts like sprintf, but writes its formatted string to the end of
 * <b>conn</b>-\>outbuf. */
static void
connection_printf_to_buf(control_connection_t *conn, const char *format, ...)
{
  va_list ap;
  char *buf = NULL;
  int len;

  va_start(ap,format);
  len = tor_vasprintf(&buf, format, ap);
  va_end(ap);

  if (len < 0) {
    log_err(LD_BUG, "Unable to format string for controller.");
    tor_assert(0);
  }

  connection_write_to_buf(buf, (size_t)len, TO_CONN(conn));

  tor_free(buf);
}

/** Write all of the open control ports to ControlPortWriteToFile */
void
control_ports_write_to_file(void)
{
  smartlist_t *lines;
  char *joined = NULL;
  const or_options_t *options = get_options();

  if (!options->ControlPortWriteToFile)
    return;

  lines = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(get_connection_array(), const connection_t *, conn) {
    if (conn->type != CONN_TYPE_CONTROL_LISTENER || conn->marked_for_close)
      continue;
#ifdef AF_UNIX
    if (conn->socket_family == AF_UNIX) {
      smartlist_add_asprintf(lines, "UNIX_PORT=%s\n", conn->address);
      continue;
    }
#endif
    smartlist_add_asprintf(lines, "PORT=%s:%d\n", conn->address, conn->port);
  } SMARTLIST_FOREACH_END(conn);

  joined = smartlist_join_strings(lines, "", 0, NULL);

  if (write_str_to_file(options->ControlPortWriteToFile, joined, 0) < 0) {
    log_warn(LD_CONTROL, "Writing %s failed: %s",
             options->ControlPortWriteToFile, strerror(errno));
  }
#ifndef _WIN32
  if (options->ControlPortFileGroupReadable) {
    if (chmod(options->ControlPortWriteToFile, 0640)) {
      log_warn(LD_FS,"Unable to make %s group-readable.",
               options->ControlPortWriteToFile);
    }
  }
#endif
  tor_free(joined);
  SMARTLIST_FOREACH(lines, char *, cp, tor_free(cp));
  smartlist_free(lines);
}

/** Send a "DONE" message down the control connection <b>conn</b>. */
static void
send_control_done(control_connection_t *conn)
{
  connection_write_str_to_buf("250 OK\r\n", conn);
}

/** Send an event to all v1 controllers that are listening for code
 * <b>event</b>.  The event's body is given by <b>msg</b>.
 *
 * If <b>which</b> & SHORT_NAMES, the event contains short-format names: send
 * it to controllers that haven't enabled the VERBOSE_NAMES feature.  If
 * <b>which</b> & LONG_NAMES, the event contains long-format names: send it
 * to controllers that <em>have</em> enabled VERBOSE_NAMES.
 *
 * The EXTENDED_FORMAT and NONEXTENDED_FORMAT flags behave similarly with
 * respect to the EXTENDED_EVENTS feature. */
MOCK_IMPL(STATIC void,
send_control_event_string,(uint16_t event, event_format_t which,
                           const char *msg))
{
  smartlist_t *conns = get_connection_array();
  (void)which;
  tor_assert(event >= EVENT_MIN_ && event <= EVENT_MAX_);

  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn) {
    if (conn->type == CONN_TYPE_CONTROL &&
        !conn->marked_for_close &&
        conn->state == CONTROL_CONN_STATE_OPEN) {
      control_connection_t *control_conn = TO_CONTROL_CONN(conn);

      if (control_conn->event_mask & (((event_mask_t)1)<<event)) {
        int is_err = 0;
        connection_write_to_buf(msg, strlen(msg), TO_CONN(control_conn));
        if (event == EVENT_ERR_MSG)
          is_err = 1;
        else if (event == EVENT_STATUS_GENERAL)
          is_err = !strcmpstart(msg, "STATUS_GENERAL ERR ");
        else if (event == EVENT_STATUS_CLIENT)
          is_err = !strcmpstart(msg, "STATUS_CLIENT ERR ");
        else if (event == EVENT_STATUS_SERVER)
          is_err = !strcmpstart(msg, "STATUS_SERVER ERR ");
        if (is_err)
          connection_flush(TO_CONN(control_conn));
      }
    }
  } SMARTLIST_FOREACH_END(conn);
}

/** Helper for send_control_event and control_event_status:
 * Send an event to all v1 controllers that are listening for code
 * <b>event</b>.  The event's body is created by the printf-style format in
 * <b>format</b>, and other arguments as provided. */
static void
send_control_event_impl(uint16_t event, event_format_t which,
                         const char *format, va_list ap)
{
  char *buf = NULL;
  int len;

  len = tor_vasprintf(&buf, format, ap);
  if (len < 0) {
    log_warn(LD_BUG, "Unable to format event for controller.");
    return;
  }

  send_control_event_string(event, which|ALL_FORMATS, buf);

  tor_free(buf);
}

/** Send an event to all v1 controllers that are listening for code
 * <b>event</b>.  The event's body is created by the printf-style format in
 * <b>format</b>, and other arguments as provided. */
static void
send_control_event(uint16_t event, event_format_t which,
                   const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  send_control_event_impl(event, which, format, ap);
  va_end(ap);
}

/** Given a text circuit <b>id</b>, return the corresponding circuit. */
static origin_circuit_t *
get_circ(const char *id)
{
  uint32_t n_id;
  int ok;
  n_id = (uint32_t) tor_parse_ulong(id, 10, 0, UINT32_MAX, &ok, NULL);
  if (!ok)
    return NULL;
  return circuit_get_by_global_id(n_id);
}

/** Given a text stream <b>id</b>, return the corresponding AP connection. */
static entry_connection_t *
get_stream(const char *id)
{
  uint64_t n_id;
  int ok;
  connection_t *conn;
  n_id = tor_parse_uint64(id, 10, 0, UINT64_MAX, &ok, NULL);
  if (!ok)
    return NULL;
  conn = connection_get_by_global_id(n_id);
  if (!conn || conn->type != CONN_TYPE_AP || conn->marked_for_close)
    return NULL;
  return TO_ENTRY_CONN(conn);
}

/** Helper for setconf and resetconf. Acts like setconf, except
 * it passes <b>use_defaults</b> on to options_trial_assign().  Modifies the
 * contents of body.
 */
static int
control_setconf_helper(control_connection_t *conn, uint32_t len, char *body,
                       int use_defaults)
{
  setopt_err_t opt_err;
  config_line_t *lines=NULL;
  char *start = body;
  char *errstring = NULL;
  const int clear_first = 1;

  char *config;
  smartlist_t *entries = smartlist_new();

  /* We have a string, "body", of the format '(key(=val|="val")?)' entries
   * separated by space.  break it into a list of configuration entries. */
  while (*body) {
    char *eq = body;
    char *key;
    char *entry;
    while (!TOR_ISSPACE(*eq) && *eq != '=')
      ++eq;
    key = tor_strndup(body, eq-body);
    body = eq+1;
    if (*eq == '=') {
      char *val=NULL;
      size_t val_len=0;
      if (*body != '\"') {
        char *val_start = body;
        while (!TOR_ISSPACE(*body))
          body++;
        val = tor_strndup(val_start, body-val_start);
        val_len = strlen(val);
      } else {
        body = (char*)extract_escaped_string(body, (len - (body-start)),
                                             &val, &val_len);
        if (!body) {
          connection_write_str_to_buf("551 Couldn't parse string\r\n", conn);
          SMARTLIST_FOREACH(entries, char *, cp, tor_free(cp));
          smartlist_free(entries);
          tor_free(key);
          return 0;
        }
      }
      tor_asprintf(&entry, "%s %s", key, val);
      tor_free(key);
      tor_free(val);
    } else {
      entry = key;
    }
    smartlist_add(entries, entry);
    while (TOR_ISSPACE(*body))
      ++body;
  }

  smartlist_add(entries, tor_strdup(""));
  config = smartlist_join_strings(entries, "\n", 0, NULL);
  SMARTLIST_FOREACH(entries, char *, cp, tor_free(cp));
  smartlist_free(entries);

  if (config_get_lines(config, &lines, 0) < 0) {
    log_warn(LD_CONTROL,"Controller gave us config lines we can't parse.");
    connection_write_str_to_buf("551 Couldn't parse configuration\r\n",
                                conn);
    tor_free(config);
    return 0;
  }
  tor_free(config);

  opt_err = options_trial_assign(lines, use_defaults, clear_first, &errstring);
  {
    const char *msg;
    switch (opt_err) {
      case SETOPT_ERR_MISC:
        msg = "552 Unrecognized option";
        break;
      case SETOPT_ERR_PARSE:
        msg = "513 Unacceptable option value";
        break;
      case SETOPT_ERR_TRANSITION:
        msg = "553 Transition not allowed";
        break;
      case SETOPT_ERR_SETTING:
      default:
        msg = "553 Unable to set option";
        break;
      case SETOPT_OK:
        config_free_lines(lines);
        send_control_done(conn);
        return 0;
    }
    log_warn(LD_CONTROL,
             "Controller gave us config lines that didn't validate: %s",
             errstring);
    connection_printf_to_buf(conn, "%s: %s\r\n", msg, errstring);
    config_free_lines(lines);
    tor_free(errstring);
    return 0;
  }
}

/** Called when we receive a SETCONF message: parse the body and try
 * to update our configuration.  Reply with a DONE or ERROR message.
 * Modifies the contents of body.*/
static int
handle_control_setconf(control_connection_t *conn, uint32_t len, char *body)
{
  return control_setconf_helper(conn, len, body, 0);
}

/** Called when we receive a RESETCONF message: parse the body and try
 * to update our configuration.  Reply with a DONE or ERROR message.
 * Modifies the contents of body. */
static int
handle_control_resetconf(control_connection_t *conn, uint32_t len, char *body)
{
  return control_setconf_helper(conn, len, body, 1);
}

/** Called when we receive a GETCONF message.  Parse the request, and
 * reply with a CONFVALUE or an ERROR message */
static int
handle_control_getconf(control_connection_t *conn, uint32_t body_len,
                       const char *body)
{
  smartlist_t *questions = smartlist_new();
  smartlist_t *answers = smartlist_new();
  smartlist_t *unrecognized = smartlist_new();
  char *msg = NULL;
  size_t msg_len;
  const or_options_t *options = get_options();
  int i, len;

  (void) body_len; /* body is NUL-terminated; so we can ignore len. */
  smartlist_split_string(questions, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH_BEGIN(questions, const char *, q) {
    if (!option_is_recognized(q)) {
      smartlist_add(unrecognized, (char*) q);
    } else {
      config_line_t *answer = option_get_assignment(options,q);
      if (!answer) {
        const char *name = option_get_canonical_name(q);
        smartlist_add_asprintf(answers, "250-%s\r\n", name);
      }

      while (answer) {
        config_line_t *next;
        smartlist_add_asprintf(answers, "250-%s=%s\r\n",
                     answer->key, answer->value);

        next = answer->next;
        tor_free(answer->key);
        tor_free(answer->value);
        tor_free(answer);
        answer = next;
      }
    }
  } SMARTLIST_FOREACH_END(q);

  if ((len = smartlist_len(unrecognized))) {
    for (i=0; i < len-1; ++i)
      connection_printf_to_buf(conn,
                               "552-Unrecognized configuration key \"%s\"\r\n",
                               (char*)smartlist_get(unrecognized, i));
    connection_printf_to_buf(conn,
                             "552 Unrecognized configuration key \"%s\"\r\n",
                             (char*)smartlist_get(unrecognized, len-1));
  } else if ((len = smartlist_len(answers))) {
    char *tmp = smartlist_get(answers, len-1);
    tor_assert(strlen(tmp)>4);
    tmp[3] = ' ';
    msg = smartlist_join_strings(answers, "", 0, &msg_len);
    connection_write_to_buf(msg, msg_len, TO_CONN(conn));
  } else {
    connection_write_str_to_buf("250 OK\r\n", conn);
  }

  SMARTLIST_FOREACH(answers, char *, cp, tor_free(cp));
  smartlist_free(answers);
  SMARTLIST_FOREACH(questions, char *, cp, tor_free(cp));
  smartlist_free(questions);
  smartlist_free(unrecognized);

  tor_free(msg);

  return 0;
}

/** Called when we get a +LOADCONF message. */
static int
handle_control_loadconf(control_connection_t *conn, uint32_t len,
                         const char *body)
{
  setopt_err_t retval;
  char *errstring = NULL;
  const char *msg = NULL;
  (void) len;

  retval = options_init_from_string(NULL, body, CMD_RUN_TOR, NULL, &errstring);

  if (retval != SETOPT_OK)
    log_warn(LD_CONTROL,
             "Controller gave us config file that didn't validate: %s",
             errstring);

  switch (retval) {
  case SETOPT_ERR_PARSE:
    msg = "552 Invalid config file";
    break;
  case SETOPT_ERR_TRANSITION:
    msg = "553 Transition not allowed";
    break;
  case SETOPT_ERR_SETTING:
    msg = "553 Unable to set option";
    break;
  case SETOPT_ERR_MISC:
  default:
    msg = "550 Unable to load config";
    break;
  case SETOPT_OK:
    break;
  }
  if (msg) {
    if (errstring)
      connection_printf_to_buf(conn, "%s: %s\r\n", msg, errstring);
    else
      connection_printf_to_buf(conn, "%s\r\n", msg);
  } else {
    send_control_done(conn);
  }
  tor_free(errstring);
  return 0;
}

/** Helper structure: maps event values to their names. */
struct control_event_t {
  uint16_t event_code;
  const char *event_name;
};
/** Table mapping event values to their names.  Used to implement SETEVENTS
 * and GETINFO events/names, and to keep they in sync. */
static const struct control_event_t control_event_table[] = {
  { EVENT_CIRCUIT_STATUS, "CIRC" },
  { EVENT_CIRCUIT_STATUS_MINOR, "CIRC_MINOR" },
  { EVENT_STREAM_STATUS, "STREAM" },
  { EVENT_OR_CONN_STATUS, "ORCONN" },
  { EVENT_BANDWIDTH_USED, "BW" },
  { EVENT_DEBUG_MSG, "DEBUG" },
  { EVENT_INFO_MSG, "INFO" },
  { EVENT_NOTICE_MSG, "NOTICE" },
  { EVENT_WARN_MSG, "WARN" },
  { EVENT_ERR_MSG, "ERR" },
  { EVENT_NEW_DESC, "NEWDESC" },
  { EVENT_ADDRMAP, "ADDRMAP" },
  { EVENT_AUTHDIR_NEWDESCS, "AUTHDIR_NEWDESCS" },
  { EVENT_DESCCHANGED, "DESCCHANGED" },
  { EVENT_NS, "NS" },
  { EVENT_STATUS_GENERAL, "STATUS_GENERAL" },
  { EVENT_STATUS_CLIENT, "STATUS_CLIENT" },
  { EVENT_STATUS_SERVER, "STATUS_SERVER" },
  { EVENT_GUARD, "GUARD" },
  { EVENT_STREAM_BANDWIDTH_USED, "STREAM_BW" },
  { EVENT_CLIENTS_SEEN, "CLIENTS_SEEN" },
  { EVENT_NEWCONSENSUS, "NEWCONSENSUS" },
  { EVENT_BUILDTIMEOUT_SET, "BUILDTIMEOUT_SET" },
  { EVENT_SIGNAL, "SIGNAL" },
  { EVENT_CONF_CHANGED, "CONF_CHANGED"},
  { EVENT_CONN_BW, "CONN_BW" },
  { EVENT_CELL_STATS, "CELL_STATS" },
  { EVENT_TB_EMPTY, "TB_EMPTY" },
  { EVENT_CIRC_BANDWIDTH_USED, "CIRC_BW" },
  { EVENT_TRANSPORT_LAUNCHED, "TRANSPORT_LAUNCHED" },
  { EVENT_HS_DESC, "HS_DESC" },
  { 0, NULL },
};

/** Called when we get a SETEVENTS message: update conn->event_mask,
 * and reply with DONE or ERROR. */
static int
handle_control_setevents(control_connection_t *conn, uint32_t len,
                         const char *body)
{
  int event_code = -1;
  event_mask_t event_mask = 0;
  smartlist_t *events = smartlist_new();

  (void) len;

  smartlist_split_string(events, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH_BEGIN(events, const char *, ev)
    {
      if (!strcasecmp(ev, "EXTENDED")) {
        continue;
      } else {
        int i;
        for (i = 0; control_event_table[i].event_name != NULL; ++i) {
          if (!strcasecmp(ev, control_event_table[i].event_name)) {
            event_code = control_event_table[i].event_code;
            break;
          }
        }

        if (event_code == -1) {
          connection_printf_to_buf(conn, "552 Unrecognized event \"%s\"\r\n",
                                   ev);
          SMARTLIST_FOREACH(events, char *, e, tor_free(e));
          smartlist_free(events);
          return 0;
        }
      }
      event_mask |= (((event_mask_t)1) << event_code);
    }
  SMARTLIST_FOREACH_END(ev);
  SMARTLIST_FOREACH(events, char *, e, tor_free(e));
  smartlist_free(events);

  conn->event_mask = event_mask;

  control_update_global_event_mask();
  send_control_done(conn);
  return 0;
}

/** Decode the hashed, base64'd passwords stored in <b>passwords</b>.
 * Return a smartlist of acceptable passwords (unterminated strings of
 * length S2K_SPECIFIER_LEN+DIGEST_LEN) on success, or NULL on failure.
 */
smartlist_t *
decode_hashed_passwords(config_line_t *passwords)
{
  char decoded[64];
  config_line_t *cl;
  smartlist_t *sl = smartlist_new();

  tor_assert(passwords);

  for (cl = passwords; cl; cl = cl->next) {
    const char *hashed = cl->value;

    if (!strcmpstart(hashed, "16:")) {
      if (base16_decode(decoded, sizeof(decoded), hashed+3, strlen(hashed+3))<0
          || strlen(hashed+3) != (S2K_SPECIFIER_LEN+DIGEST_LEN)*2) {
        goto err;
      }
    } else {
        if (base64_decode(decoded, sizeof(decoded), hashed, strlen(hashed))
            != S2K_SPECIFIER_LEN+DIGEST_LEN) {
          goto err;
        }
    }
    smartlist_add(sl, tor_memdup(decoded, S2K_SPECIFIER_LEN+DIGEST_LEN));
  }

  return sl;

 err:
  SMARTLIST_FOREACH(sl, char*, cp, tor_free(cp));
  smartlist_free(sl);
  return NULL;
}

/** Called when we get an AUTHENTICATE message.  Check whether the
 * authentication is valid, and if so, update the connection's state to
 * OPEN.  Reply with DONE or ERROR.
 */
static int
handle_control_authenticate(control_connection_t *conn, uint32_t len,
                            const char *body)
{
  int used_quoted_string = 0;
  const or_options_t *options = get_options();
  const char *errstr = NULL;
  char *password;
  size_t password_len;
  const char *cp;
  int i;
  int bad_cookie=0, bad_password=0;
  smartlist_t *sl = NULL;

  if (!len) {
    password = tor_strdup("");
    password_len = 0;
  } else if (TOR_ISXDIGIT(body[0])) {
    cp = body;
    while (TOR_ISXDIGIT(*cp))
      ++cp;
    i = (int)(cp - body);
    tor_assert(i>0);
    password_len = i/2;
    password = tor_malloc(password_len + 1);
    if (base16_decode(password, password_len+1, body, i)<0) {
      connection_write_str_to_buf(
            "551 Invalid hexadecimal encoding.  Maybe you tried a plain text "
            "password?  If so, the standard requires that you put it in "
            "double quotes.\r\n", conn);
      connection_mark_for_close(TO_CONN(conn));
      tor_free(password);
      return 0;
    }
  } else {
    if (!decode_escaped_string(body, len, &password, &password_len)) {
      connection_write_str_to_buf("551 Invalid quoted string.  You need "
            "to put the password in double quotes.\r\n", conn);
      connection_mark_for_close(TO_CONN(conn));
      return 0;
    }
    used_quoted_string = 1;
  }

  if (conn->safecookie_client_hash != NULL) {
    /* The controller has chosen safe cookie authentication; the only
     * acceptable authentication value is the controller-to-server
     * response. */

    tor_assert(authentication_cookie_is_set);

    if (password_len != DIGEST256_LEN) {
      log_warn(LD_CONTROL,
               "Got safe cookie authentication response with wrong length "
               "(%d)", (int)password_len);
      errstr = "Wrong length for safe cookie response.";
      goto err;
    }

    if (tor_memneq(conn->safecookie_client_hash, password, DIGEST256_LEN)) {
      log_warn(LD_CONTROL,
               "Got incorrect safe cookie authentication response");
      errstr = "Safe cookie response did not match expected value.";
      goto err;
    }

    tor_free(conn->safecookie_client_hash);
    goto ok;
  }

  if (!options->CookieAuthentication && !options->HashedControlPassword &&
      !options->HashedControlSessionPassword) {
    /* if Tor doesn't demand any stronger authentication, then
     * the controller can get in with anything. */
    goto ok;
  }

  if (options->CookieAuthentication) {
    int also_password = options->HashedControlPassword != NULL ||
      options->HashedControlSessionPassword != NULL;
    if (password_len != AUTHENTICATION_COOKIE_LEN) {
      if (!also_password) {
        log_warn(LD_CONTROL, "Got authentication cookie with wrong length "
                 "(%d)", (int)password_len);
        errstr = "Wrong length on authentication cookie.";
        goto err;
      }
      bad_cookie = 1;
    } else if (tor_memneq(authentication_cookie, password, password_len)) {
      if (!also_password) {
        log_warn(LD_CONTROL, "Got mismatched authentication cookie");
        errstr = "Authentication cookie did not match expected value.";
        goto err;
      }
      bad_cookie = 1;
    } else {
      goto ok;
    }
  }

  if (options->HashedControlPassword ||
      options->HashedControlSessionPassword) {
    int bad = 0;
    smartlist_t *sl_tmp;
    char received[DIGEST_LEN];
    int also_cookie = options->CookieAuthentication;
    sl = smartlist_new();
    if (options->HashedControlPassword) {
      sl_tmp = decode_hashed_passwords(options->HashedControlPassword);
      if (!sl_tmp)
        bad = 1;
      else {
        smartlist_add_all(sl, sl_tmp);
        smartlist_free(sl_tmp);
      }
    }
    if (options->HashedControlSessionPassword) {
      sl_tmp = decode_hashed_passwords(options->HashedControlSessionPassword);
      if (!sl_tmp)
        bad = 1;
      else {
        smartlist_add_all(sl, sl_tmp);
        smartlist_free(sl_tmp);
      }
    }
    if (bad) {
      if (!also_cookie) {
        log_warn(LD_CONTROL,
                 "Couldn't decode HashedControlPassword: invalid base16");
        errstr="Couldn't decode HashedControlPassword value in configuration.";
      }
      bad_password = 1;
      SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
      smartlist_free(sl);
    } else {
      SMARTLIST_FOREACH(sl, char *, expected,
      {
        secret_to_key(received,DIGEST_LEN,password,password_len,expected);
        if (tor_memeq(expected+S2K_SPECIFIER_LEN, received, DIGEST_LEN))
          goto ok;
      });
      SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
      smartlist_free(sl);

      if (used_quoted_string)
        errstr = "Password did not match HashedControlPassword value from "
          "configuration";
      else
        errstr = "Password did not match HashedControlPassword value from "
          "configuration. Maybe you tried a plain text password? "
          "If so, the standard requires that you put it in double quotes.";
      bad_password = 1;
      if (!also_cookie)
        goto err;
    }
  }

  /** We only get here if both kinds of authentication failed. */
  tor_assert(bad_password && bad_cookie);
  log_warn(LD_CONTROL, "Bad password or authentication cookie on controller.");
  errstr = "Password did not match HashedControlPassword *or* authentication "
    "cookie.";

 err:
  tor_free(password);
  connection_printf_to_buf(conn, "515 Authentication failed: %s\r\n",
                           errstr ? errstr : "Unknown reason.");
  connection_mark_for_close(TO_CONN(conn));
  return 0;
 ok:
  log_info(LD_CONTROL, "Authenticated control connection ("TOR_SOCKET_T_FORMAT
           ")", conn->base_.s);
  send_control_done(conn);
  conn->base_.state = CONTROL_CONN_STATE_OPEN;
  tor_free(password);
  if (sl) { /* clean up */
    SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
    smartlist_free(sl);
  }
  return 0;
}

/** Called when we get a SAVECONF command. Try to flush the current options to
 * disk, and report success or failure. */
static int
handle_control_saveconf(control_connection_t *conn, uint32_t len,
                        const char *body)
{
  (void) len;
  (void) body;
  if (options_save_current()<0) {
    connection_write_str_to_buf(
      "551 Unable to write configuration to disk.\r\n", conn);
  } else {
    send_control_done(conn);
  }
  return 0;
}

struct signal_t {
  int sig;
  const char *signal_name;
};

static const struct signal_t signal_table[] = {
  { SIGHUP, "RELOAD" },
  { SIGHUP, "HUP" },
  { SIGINT, "SHUTDOWN" },
  { SIGUSR1, "DUMP" },
  { SIGUSR1, "USR1" },
  { SIGUSR2, "DEBUG" },
  { SIGUSR2, "USR2" },
  { SIGTERM, "HALT" },
  { SIGTERM, "TERM" },
  { SIGTERM, "INT" },
  { SIGNEWNYM, "NEWNYM" },
  { SIGCLEARDNSCACHE, "CLEARDNSCACHE"},
  { 0, NULL },
};

/** Called when we get a SIGNAL command. React to the provided signal, and
 * report success or failure. (If the signal results in a shutdown, success
 * may not be reported.) */
static int
handle_control_signal(control_connection_t *conn, uint32_t len,
                      const char *body)
{
  int sig = -1;
  int i;
  int n = 0;
  char *s;

  (void) len;

  while (body[n] && ! TOR_ISSPACE(body[n]))
    ++n;
  s = tor_strndup(body, n);

  for (i = 0; signal_table[i].signal_name != NULL; ++i) {
    if (!strcasecmp(s, signal_table[i].signal_name)) {
      sig = signal_table[i].sig;
      break;
    }
  }

  if (sig < 0)
    connection_printf_to_buf(conn, "552 Unrecognized signal code \"%s\"\r\n",
                             s);
  tor_free(s);
  if (sig < 0)
    return 0;

  send_control_done(conn);
  /* Flush the "done" first if the signal might make us shut down. */
  if (sig == SIGTERM || sig == SIGINT)
    connection_flush(TO_CONN(conn));

  process_signal(sig);

  return 0;
}

/** Called when we get a TAKEOWNERSHIP command.  Mark this connection
 * as an owning connection, so that we will exit if the connection
 * closes. */
static int
handle_control_takeownership(control_connection_t *conn, uint32_t len,
                             const char *body)
{
  (void)len;
  (void)body;

  conn->is_owning_control_connection = 1;

  log_info(LD_CONTROL, "Control connection %d has taken ownership of this "
           "Tor instance.",
           (int)(conn->base_.s));

  send_control_done(conn);
  return 0;
}

/** Return true iff <b>addr</b> is unusable as a mapaddress target because of
 * containing funny characters. */
static int
address_is_invalid_mapaddress_target(const char *addr)
{
  if (!strcmpstart(addr, "*."))
    return address_is_invalid_destination(addr+2, 1);
  else
    return address_is_invalid_destination(addr, 1);
}

/** Called when we get a MAPADDRESS command; try to bind all listed addresses,
 * and report success or failure. */
static int
handle_control_mapaddress(control_connection_t *conn, uint32_t len,
                          const char *body)
{
  smartlist_t *elts;
  smartlist_t *lines;
  smartlist_t *reply;
  char *r;
  size_t sz;
  (void) len; /* body is NUL-terminated, so it's safe to ignore the length. */

  lines = smartlist_new();
  elts = smartlist_new();
  reply = smartlist_new();
  smartlist_split_string(lines, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH_BEGIN(lines, char *, line) {
    tor_strlower(line);
    smartlist_split_string(elts, line, "=", 0, 2);
    if (smartlist_len(elts) == 2) {
      const char *from = smartlist_get(elts,0);
      const char *to = smartlist_get(elts,1);
      if (address_is_invalid_mapaddress_target(to)) {
        smartlist_add_asprintf(reply,
                     "512-syntax error: invalid address '%s'", to);
        log_warn(LD_CONTROL,
                 "Skipping invalid argument '%s' in MapAddress msg", to);
      } else if (!strcmp(from, ".") || !strcmp(from, "0.0.0.0") ||
                 !strcmp(from, "::")) {
        const char type =
          !strcmp(from,".") ? RESOLVED_TYPE_HOSTNAME :
          (!strcmp(from, "0.0.0.0") ? RESOLVED_TYPE_IPV4 : RESOLVED_TYPE_IPV6);
        const char *address = addressmap_register_virtual_address(
                                                     type, tor_strdup(to));
        if (!address) {
          smartlist_add_asprintf(reply,
                       "451-resource exhausted: skipping '%s'", line);
          log_warn(LD_CONTROL,
                   "Unable to allocate address for '%s' in MapAddress msg",
                   safe_str_client(line));
        } else {
          smartlist_add_asprintf(reply, "250-%s=%s", address, to);
        }
      } else {
        const char *msg;
        if (addressmap_register_auto(from, to, 1,
                                     ADDRMAPSRC_CONTROLLER, &msg) < 0) {
          smartlist_add_asprintf(reply,
                                 "512-syntax error: invalid address mapping "
                                 " '%s': %s", line, msg);
          log_warn(LD_CONTROL,
                   "Skipping invalid argument '%s' in MapAddress msg: %s",
                   line, msg);
        } else {
          smartlist_add_asprintf(reply, "250-%s", line);
        }
      }
    } else {
      smartlist_add_asprintf(reply, "512-syntax error: mapping '%s' is "
                   "not of expected form 'foo=bar'.", line);
      log_info(LD_CONTROL, "Skipping MapAddress '%s': wrong "
                           "number of items.",
                           safe_str_client(line));
    }
    SMARTLIST_FOREACH(elts, char *, cp, tor_free(cp));
    smartlist_clear(elts);
  } SMARTLIST_FOREACH_END(line);
  SMARTLIST_FOREACH(lines, char *, cp, tor_free(cp));
  smartlist_free(lines);
  smartlist_free(elts);

  if (smartlist_len(reply)) {
    ((char*)smartlist_get(reply,smartlist_len(reply)-1))[3] = ' ';
    r = smartlist_join_strings(reply, "\r\n", 1, &sz);
    connection_write_to_buf(r, sz, TO_CONN(conn));
    tor_free(r);
  } else {
    const char *response =
      "512 syntax error: not enough arguments to mapaddress.\r\n";
    connection_write_to_buf(response, strlen(response), TO_CONN(conn));
  }

  SMARTLIST_FOREACH(reply, char *, cp, tor_free(cp));
  smartlist_free(reply);
  return 0;
}

/** Implementation helper for GETINFO: knows the answers for various
 * trivial-to-implement questions. */
static int
getinfo_helper_misc(control_connection_t *conn, const char *question,
                    char **answer, const char **errmsg)
{
  (void) conn;
  if (!strcmp(question, "version")) {
    *answer = tor_strdup(get_version());
  } else if (!strcmp(question, "config-file")) {
    *answer = tor_strdup(get_torrc_fname(0));
  } else if (!strcmp(question, "config-defaults-file")) {
    *answer = tor_strdup(get_torrc_fname(1));
  } else if (!strcmp(question, "config-text")) {
    *answer = options_dump(get_options(), OPTIONS_DUMP_MINIMAL);
  } else if (!strcmp(question, "info/names")) {
    *answer = list_getinfo_options();
  } else if (!strcmp(question, "dormant")) {
    int dormant = rep_hist_circbuilding_dormant(time(NULL));
    *answer = tor_strdup(dormant ? "1" : "0");
  } else if (!strcmp(question, "events/names")) {
    int i;
    smartlist_t *event_names = smartlist_new();

    for (i = 0; control_event_table[i].event_name != NULL; ++i) {
      smartlist_add(event_names, (char *)control_event_table[i].event_name);
    }

    *answer = smartlist_join_strings(event_names, " ", 0, NULL);

    smartlist_free(event_names);
  } else if (!strcmp(question, "signal/names")) {
    smartlist_t *signal_names = smartlist_new();
    int j;
    for (j = 0; signal_table[j].signal_name != NULL; ++j) {
      smartlist_add(signal_names, (char*)signal_table[j].signal_name);
    }

    *answer = smartlist_join_strings(signal_names, " ", 0, NULL);

    smartlist_free(signal_names);
  } else if (!strcmp(question, "features/names")) {
    *answer = tor_strdup("VERBOSE_NAMES EXTENDED_EVENTS");
  } else if (!strcmp(question, "address")) {
    uint32_t addr;
    if (router_pick_published_address(get_options(), &addr) < 0) {
      *errmsg = "Address unknown";
      return -1;
    }
    *answer = tor_dup_ip(addr);
  } else if (!strcmp(question, "traffic/read")) {
    tor_asprintf(answer, U64_FORMAT, U64_PRINTF_ARG(get_bytes_read()));
  } else if (!strcmp(question, "traffic/written")) {
    tor_asprintf(answer, U64_FORMAT, U64_PRINTF_ARG(get_bytes_written()));
  } else if (!strcmp(question, "process/pid")) {
    int myPid = -1;

    #ifdef _WIN32
      myPid = _getpid();
    #else
      myPid = getpid();
    #endif

    tor_asprintf(answer, "%d", myPid);
  } else if (!strcmp(question, "process/uid")) {
    #ifdef _WIN32
      *answer = tor_strdup("-1");
    #else
      int myUid = geteuid();
      tor_asprintf(answer, "%d", myUid);
    #endif
  } else if (!strcmp(question, "process/user")) {
    #ifdef _WIN32
      *answer = tor_strdup("");
    #else
      int myUid = geteuid();
      const struct passwd *myPwEntry = tor_getpwuid(myUid);

      if (myPwEntry) {
        *answer = tor_strdup(myPwEntry->pw_name);
      } else {
        *answer = tor_strdup("");
      }
    #endif
  } else if (!strcmp(question, "process/descriptor-limit")) {
    int max_fds=-1;
    set_max_file_descriptors(0, &max_fds);
    tor_asprintf(answer, "%d", max_fds);
  } else if (!strcmp(question, "limits/max-mem-in-queues")) {
    tor_asprintf(answer, U64_FORMAT,
                 U64_PRINTF_ARG(get_options()->MaxMemInQueues));
  } else if (!strcmp(question, "dir-usage")) {
    *answer = directory_dump_request_log();
  } else if (!strcmp(question, "fingerprint")) {
    crypto_pk_t *server_key;
    if (!server_mode(get_options())) {
      *errmsg = "Not running in server mode";
      return -1;
    }
    server_key = get_server_identity_key();
    *answer = tor_malloc(HEX_DIGEST_LEN+1);
    crypto_pk_get_fingerprint(server_key, *answer, 0);
  }
  return 0;
}

/** Awful hack: return a newly allocated string based on a routerinfo and
 * (possibly) an extrainfo, sticking the read-history and write-history from
 * <b>ei</b> into the resulting string.  The thing you get back won't
 * necessarily have a valid signature.
 *
 * New code should never use this; it's for backward compatibility.
 *
 * NOTE: <b>ri_body</b> is as returned by signed_descriptor_get_body: it might
 * not be NUL-terminated. */
static char *
munge_extrainfo_into_routerinfo(const char *ri_body,
                                const signed_descriptor_t *ri,
                                const signed_descriptor_t *ei)
{
  char *out = NULL, *outp;
  int i;
  const char *router_sig;
  const char *ei_body = signed_descriptor_get_body(ei);
  size_t ri_len = ri->signed_descriptor_len;
  size_t ei_len = ei->signed_descriptor_len;
  if (!ei_body)
    goto bail;

  outp = out = tor_malloc(ri_len+ei_len+1);
  if (!(router_sig = tor_memstr(ri_body, ri_len, "\nrouter-signature")))
    goto bail;
  ++router_sig;
  memcpy(out, ri_body, router_sig-ri_body);
  outp += router_sig-ri_body;

  for (i=0; i < 2; ++i) {
    const char *kwd = i ? "\nwrite-history " : "\nread-history ";
    const char *cp, *eol;
    if (!(cp = tor_memstr(ei_body, ei_len, kwd)))
      continue;
    ++cp;
    if (!(eol = memchr(cp, '\n', ei_len - (cp-ei_body))))
      continue;
    memcpy(outp, cp, eol-cp+1);
    outp += eol-cp+1;
  }
  memcpy(outp, router_sig, ri_len - (router_sig-ri_body));
  *outp++ = '\0';
  tor_assert(outp-out < (int)(ri_len+ei_len+1));

  return out;
 bail:
  tor_free(out);
  return tor_strndup(ri_body, ri->signed_descriptor_len);
}

/** Implementation helper for GETINFO: answers requests for information about
 * which ports are bound. */
static int
getinfo_helper_listeners(control_connection_t *control_conn,
                         const char *question,
                         char **answer, const char **errmsg)
{
  int type;
  smartlist_t *res;

  (void)control_conn;
  (void)errmsg;

  if (!strcmp(question, "net/listeners/or"))
    type = CONN_TYPE_OR_LISTENER;
  else if (!strcmp(question, "net/listeners/dir"))
    type = CONN_TYPE_DIR_LISTENER;
  else if (!strcmp(question, "net/listeners/socks"))
    type = CONN_TYPE_AP_LISTENER;
  else if (!strcmp(question, "net/listeners/trans"))
    type = CONN_TYPE_AP_TRANS_LISTENER;
  else if (!strcmp(question, "net/listeners/natd"))
    type = CONN_TYPE_AP_NATD_LISTENER;
  else if (!strcmp(question, "net/listeners/dns"))
    type = CONN_TYPE_AP_DNS_LISTENER;
  else if (!strcmp(question, "net/listeners/control"))
    type = CONN_TYPE_CONTROL_LISTENER;
  else
    return 0; /* unknown key */

  res = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(get_connection_array(), connection_t *, conn) {
    struct sockaddr_storage ss;
    socklen_t ss_len = sizeof(ss);

    if (conn->type != type || conn->marked_for_close || !SOCKET_OK(conn->s))
      continue;

    if (getsockname(conn->s, (struct sockaddr *)&ss, &ss_len) < 0) {
      smartlist_add_asprintf(res, "%s:%d", conn->address, (int)conn->port);
    } else {
      char *tmp = tor_sockaddr_to_str((struct sockaddr *)&ss);
      smartlist_add(res, esc_for_log(tmp));
      tor_free(tmp);
    }

  } SMARTLIST_FOREACH_END(conn);

  *answer = smartlist_join_strings(res, " ", 0, NULL);

  SMARTLIST_FOREACH(res, char *, cp, tor_free(cp));
  smartlist_free(res);
  return 0;
}

/** Implementation helper for GETINFO: knows the answers for questions about
 * directory information. */
static int
getinfo_helper_dir(control_connection_t *control_conn,
                   const char *question, char **answer,
                   const char **errmsg)
{
  const node_t *node;
  const routerinfo_t *ri = NULL;
  (void) control_conn;
  if (!strcmpstart(question, "desc/id/")) {
    node = node_get_by_hex_id(question+strlen("desc/id/"));
    if (node)
      ri = node->ri;
    if (ri) {
      const char *body = signed_descriptor_get_body(&ri->cache_info);
      if (body)
        *answer = tor_strndup(body, ri->cache_info.signed_descriptor_len);
    }
  } else if (!strcmpstart(question, "desc/name/")) {
    /* XXX023 Setting 'warn_if_unnamed' here is a bit silly -- the
     * warning goes to the user, not to the controller. */
    node = node_get_by_nickname(question+strlen("desc/name/"), 1);
    if (node)
      ri = node->ri;
    if (ri) {
      const char *body = signed_descriptor_get_body(&ri->cache_info);
      if (body)
        *answer = tor_strndup(body, ri->cache_info.signed_descriptor_len);
    }
  } else if (!strcmp(question, "desc/all-recent")) {
    routerlist_t *routerlist = router_get_routerlist();
    smartlist_t *sl = smartlist_new();
    if (routerlist && routerlist->routers) {
      SMARTLIST_FOREACH(routerlist->routers, const routerinfo_t *, ri,
      {
        const char *body = signed_descriptor_get_body(&ri->cache_info);
        if (body)
          smartlist_add(sl,
                  tor_strndup(body, ri->cache_info.signed_descriptor_len));
      });
    }
    *answer = smartlist_join_strings(sl, "", 0, NULL);
    SMARTLIST_FOREACH(sl, char *, c, tor_free(c));
    smartlist_free(sl);
  } else if (!strcmp(question, "desc/all-recent-extrainfo-hack")) {
    /* XXXX Remove this once Torstat asks for extrainfos. */
    routerlist_t *routerlist = router_get_routerlist();
    smartlist_t *sl = smartlist_new();
    if (routerlist && routerlist->routers) {
      SMARTLIST_FOREACH_BEGIN(routerlist->routers, const routerinfo_t *, ri) {
        const char *body = signed_descriptor_get_body(&ri->cache_info);
        signed_descriptor_t *ei = extrainfo_get_by_descriptor_digest(
                                     ri->cache_info.extra_info_digest);
        if (ei && body) {
          smartlist_add(sl, munge_extrainfo_into_routerinfo(body,
                                                        &ri->cache_info, ei));
        } else if (body) {
          smartlist_add(sl,
                  tor_strndup(body, ri->cache_info.signed_descriptor_len));
        }
      } SMARTLIST_FOREACH_END(ri);
    }
    *answer = smartlist_join_strings(sl, "", 0, NULL);
    SMARTLIST_FOREACH(sl, char *, c, tor_free(c));
    smartlist_free(sl);
  } else if (!strcmpstart(question, "md/id/")) {
    const node_t *node = node_get_by_hex_id(question+strlen("md/id/"));
    const microdesc_t *md = NULL;
    if (node) md = node->md;
    if (md && md->body) {
      *answer = tor_strndup(md->body, md->bodylen);
    }
  } else if (!strcmpstart(question, "md/name/")) {
    /* XXX023 Setting 'warn_if_unnamed' here is a bit silly -- the
     * warning goes to the user, not to the controller. */
    const node_t *node = node_get_by_nickname(question+strlen("md/name/"), 1);
    /* XXXX duplicated code */
    const microdesc_t *md = NULL;
    if (node) md = node->md;
    if (md && md->body) {
      *answer = tor_strndup(md->body, md->bodylen);
    }
  } else if (!strcmpstart(question, "desc-annotations/id/")) {
    node = node_get_by_hex_id(question+strlen("desc-annotations/id/"));
    if (node)
      ri = node->ri;
    if (ri) {
      const char *annotations =
        signed_descriptor_get_annotations(&ri->cache_info);
      if (annotations)
        *answer = tor_strndup(annotations,
                              ri->cache_info.annotations_len);
    }
  } else if (!strcmpstart(question, "dir/server/")) {
    size_t answer_len = 0;
    char *url = NULL;
    smartlist_t *descs = smartlist_new();
    const char *msg;
    int res;
    char *cp;
    tor_asprintf(&url, "/tor/%s", question+4);
    res = dirserv_get_routerdescs(descs, url, &msg);
    if (res) {
      log_warn(LD_CONTROL, "getinfo '%s': %s", question, msg);
      smartlist_free(descs);
      tor_free(url);
      *errmsg = msg;
      return -1;
    }
    SMARTLIST_FOREACH(descs, signed_descriptor_t *, sd,
                      answer_len += sd->signed_descriptor_len);
    cp = *answer = tor_malloc(answer_len+1);
    SMARTLIST_FOREACH(descs, signed_descriptor_t *, sd,
                      {
                        memcpy(cp, signed_descriptor_get_body(sd),
                               sd->signed_descriptor_len);
                        cp += sd->signed_descriptor_len;
                      });
    *cp = '\0';
    tor_free(url);
    smartlist_free(descs);
  } else if (!strcmpstart(question, "dir/status/")) {
    *answer = tor_strdup("");
  } else if (!strcmp(question, "dir/status-vote/current/consensus")) { /* v3 */
    if (directory_caches_dir_info(get_options())) {
      const cached_dir_t *consensus = dirserv_get_consensus("ns");
      if (consensus)
        *answer = tor_strdup(consensus->dir);
    }
    if (!*answer) { /* try loading it from disk */
      char *filename = get_datadir_fname("cached-consensus");
      *answer = read_file_to_str(filename, RFTS_IGNORE_MISSING, NULL);
      tor_free(filename);
    }
  } else if (!strcmp(question, "network-status")) { /* v1 */
    routerlist_t *routerlist = router_get_routerlist();
    if (!routerlist || !routerlist->routers ||
        list_server_status_v1(routerlist->routers, answer, 1) < 0) {
      return -1;
    }
  } else if (!strcmpstart(question, "extra-info/digest/")) {
    question += strlen("extra-info/digest/");
    if (strlen(question) == HEX_DIGEST_LEN) {
      char d[DIGEST_LEN];
      signed_descriptor_t *sd = NULL;
      if (base16_decode(d, sizeof(d), question, strlen(question))==0) {
        /* XXXX this test should move into extrainfo_get_by_descriptor_digest,
         * but I don't want to risk affecting other parts of the code,
         * especially since the rules for using our own extrainfo (including
         * when it might be freed) are different from those for using one
         * we have downloaded. */
        if (router_extrainfo_digest_is_me(d))
          sd = &(router_get_my_extrainfo()->cache_info);
        else
          sd = extrainfo_get_by_descriptor_digest(d);
      }
      if (sd) {
        const char *body = signed_descriptor_get_body(sd);
        if (body)
          *answer = tor_strndup(body, sd->signed_descriptor_len);
      }
    }
  }

  return 0;
}

/** Allocate and return a description of <b>circ</b>'s current status,
 * including its path (if any). */
static char *
circuit_describe_status_for_controller(origin_circuit_t *circ)
{
  char *rv;
  smartlist_t *descparts = smartlist_new();

  {
    char *vpath = circuit_list_path_for_controller(circ);
    if (*vpath) {
      smartlist_add(descparts, vpath);
    } else {
      tor_free(vpath); /* empty path; don't put an extra space in the result */
    }
  }

  {
    cpath_build_state_t *build_state = circ->build_state;
    smartlist_t *flaglist = smartlist_new();
    char *flaglist_joined;

    if (build_state->onehop_tunnel)
      smartlist_add(flaglist, (void *)"ONEHOP_TUNNEL");
    if (build_state->is_internal)
      smartlist_add(flaglist, (void *)"IS_INTERNAL");
    if (build_state->need_capacity)
      smartlist_add(flaglist, (void *)"NEED_CAPACITY");
    if (build_state->need_uptime)
      smartlist_add(flaglist, (void *)"NEED_UPTIME");

    /* Only emit a BUILD_FLAGS argument if it will have a non-empty value. */
    if (smartlist_len(flaglist)) {
      flaglist_joined = smartlist_join_strings(flaglist, ",", 0, NULL);

      smartlist_add_asprintf(descparts, "BUILD_FLAGS=%s", flaglist_joined);

      tor_free(flaglist_joined);
    }

    smartlist_free(flaglist);
  }

  smartlist_add_asprintf(descparts, "PURPOSE=%s",
                    circuit_purpose_to_controller_string(circ->base_.purpose));

  {
    const char *hs_state =
      circuit_purpose_to_controller_hs_state_string(circ->base_.purpose);

    if (hs_state != NULL) {
      smartlist_add_asprintf(descparts, "HS_STATE=%s", hs_state);
    }
  }

  if (circ->rend_data != NULL) {
    smartlist_add_asprintf(descparts, "REND_QUERY=%s",
                 circ->rend_data->onion_address);
  }

  {
    char tbuf[ISO_TIME_USEC_LEN+1];
    format_iso_time_nospace_usec(tbuf, &circ->base_.timestamp_created);

    smartlist_add_asprintf(descparts, "TIME_CREATED=%s", tbuf);
  }

  rv = smartlist_join_strings(descparts, " ", 0, NULL);

  SMARTLIST_FOREACH(descparts, char *, cp, tor_free(cp));
  smartlist_free(descparts);

  return rv;
}

/** Implementation helper for GETINFO: knows how to generate summaries of the
 * current states of things we send events about. */
static int
getinfo_helper_events(control_connection_t *control_conn,
                      const char *question, char **answer,
                      const char **errmsg)
{
  (void) control_conn;
  if (!strcmp(question, "circuit-status")) {
    circuit_t *circ_;
    smartlist_t *status = smartlist_new();
    TOR_LIST_FOREACH(circ_, circuit_get_global_list(), head) {
      origin_circuit_t *circ;
      char *circdesc;
      const char *state;
      if (! CIRCUIT_IS_ORIGIN(circ_) || circ_->marked_for_close)
        continue;
      circ = TO_ORIGIN_CIRCUIT(circ_);

      if (circ->base_.state == CIRCUIT_STATE_OPEN)
        state = "BUILT";
      else if (circ->cpath)
        state = "EXTENDED";
      else
        state = "LAUNCHED";

      circdesc = circuit_describe_status_for_controller(circ);

      smartlist_add_asprintf(status, "%lu %s%s%s",
                   (unsigned long)circ->global_identifier,
                   state, *circdesc ? " " : "", circdesc);
      tor_free(circdesc);
    }
    *answer = smartlist_join_strings(status, "\r\n", 0, NULL);
    SMARTLIST_FOREACH(status, char *, cp, tor_free(cp));
    smartlist_free(status);
  } else if (!strcmp(question, "stream-status")) {
    smartlist_t *conns = get_connection_array();
    smartlist_t *status = smartlist_new();
    char buf[256];
    SMARTLIST_FOREACH_BEGIN(conns, connection_t *, base_conn) {
      const char *state;
      entry_connection_t *conn;
      circuit_t *circ;
      origin_circuit_t *origin_circ = NULL;
      if (base_conn->type != CONN_TYPE_AP ||
          base_conn->marked_for_close ||
          base_conn->state == AP_CONN_STATE_SOCKS_WAIT ||
          base_conn->state == AP_CONN_STATE_NATD_WAIT)
        continue;
      conn = TO_ENTRY_CONN(base_conn);
      switch (base_conn->state)
        {
        case AP_CONN_STATE_CONTROLLER_WAIT:
        case AP_CONN_STATE_CIRCUIT_WAIT:
          if (conn->socks_request &&
              SOCKS_COMMAND_IS_RESOLVE(conn->socks_request->command))
            state = "NEWRESOLVE";
          else
            state = "NEW";
          break;
        case AP_CONN_STATE_RENDDESC_WAIT:
        case AP_CONN_STATE_CONNECT_WAIT:
          state = "SENTCONNECT"; break;
        case AP_CONN_STATE_RESOLVE_WAIT:
          state = "SENTRESOLVE"; break;
        case AP_CONN_STATE_OPEN:
          state = "SUCCEEDED"; break;
        default:
          log_warn(LD_BUG, "Asked for stream in unknown state %d",
                   base_conn->state);
          continue;
        }
      circ = circuit_get_by_edge_conn(ENTRY_TO_EDGE_CONN(conn));
      if (circ && CIRCUIT_IS_ORIGIN(circ))
        origin_circ = TO_ORIGIN_CIRCUIT(circ);
      write_stream_target_to_buf(conn, buf, sizeof(buf));
      smartlist_add_asprintf(status, "%lu %s %lu %s",
                   (unsigned long) base_conn->global_identifier,state,
                   origin_circ?
                         (unsigned long)origin_circ->global_identifier : 0ul,
                   buf);
    } SMARTLIST_FOREACH_END(base_conn);
    *answer = smartlist_join_strings(status, "\r\n", 0, NULL);
    SMARTLIST_FOREACH(status, char *, cp, tor_free(cp));
    smartlist_free(status);
  } else if (!strcmp(question, "orconn-status")) {
    smartlist_t *conns = get_connection_array();
    smartlist_t *status = smartlist_new();
    SMARTLIST_FOREACH_BEGIN(conns, connection_t *, base_conn) {
      const char *state;
      char name[128];
      or_connection_t *conn;
      if (base_conn->type != CONN_TYPE_OR || base_conn->marked_for_close)
        continue;
      conn = TO_OR_CONN(base_conn);
      if (conn->base_.state == OR_CONN_STATE_OPEN)
        state = "CONNECTED";
      else if (conn->nickname)
        state = "LAUNCHED";
      else
        state = "NEW";
      orconn_target_get_name(name, sizeof(name), conn);
      smartlist_add_asprintf(status, "%s %s", name, state);
    } SMARTLIST_FOREACH_END(base_conn);
    *answer = smartlist_join_strings(status, "\r\n", 0, NULL);
    SMARTLIST_FOREACH(status, char *, cp, tor_free(cp));
    smartlist_free(status);
  } else if (!strcmpstart(question, "address-mappings/")) {
    time_t min_e, max_e;
    smartlist_t *mappings;
    question += strlen("address-mappings/");
    if (!strcmp(question, "all")) {
      min_e = 0; max_e = TIME_MAX;
    } else if (!strcmp(question, "cache")) {
      min_e = 2; max_e = TIME_MAX;
    } else if (!strcmp(question, "config")) {
      min_e = 0; max_e = 0;
    } else if (!strcmp(question, "control")) {
      min_e = 1; max_e = 1;
    } else {
      return 0;
    }
    mappings = smartlist_new();
    addressmap_get_mappings(mappings, min_e, max_e, 1);
    *answer = smartlist_join_strings(mappings, "\r\n", 0, NULL);
    SMARTLIST_FOREACH(mappings, char *, cp, tor_free(cp));
    smartlist_free(mappings);
  } else if (!strcmpstart(question, "status/")) {
    /* Note that status/ is not a catch-all for events; there's only supposed
     * to be a status GETINFO if there's a corresponding STATUS event. */
    if (!strcmp(question, "status/circuit-established")) {
      *answer = tor_strdup(can_complete_circuit ? "1" : "0");
    } else if (!strcmp(question, "status/enough-dir-info")) {
      *answer = tor_strdup(router_have_minimum_dir_info() ? "1" : "0");
    } else if (!strcmp(question, "status/good-server-descriptor") ||
               !strcmp(question, "status/accepted-server-descriptor")) {
      /* They're equivalent for now, until we can figure out how to make
       * good-server-descriptor be what we want. See comment in
       * control-spec.txt. */
      *answer = tor_strdup(directories_have_accepted_server_descriptor()
                           ? "1" : "0");
    } else if (!strcmp(question, "status/reachability-succeeded/or")) {
      *answer = tor_strdup(check_whether_orport_reachable() ? "1" : "0");
    } else if (!strcmp(question, "status/reachability-succeeded/dir")) {
      *answer = tor_strdup(check_whether_dirport_reachable() ? "1" : "0");
    } else if (!strcmp(question, "status/reachability-succeeded")) {
      tor_asprintf(answer, "OR=%d DIR=%d",
                   check_whether_orport_reachable() ? 1 : 0,
                   check_whether_dirport_reachable() ? 1 : 0);
    } else if (!strcmp(question, "status/bootstrap-phase")) {
      *answer = tor_strdup(last_sent_bootstrap_message);
    } else if (!strcmpstart(question, "status/version/")) {
      int is_server = server_mode(get_options());
      networkstatus_t *c = networkstatus_get_latest_consensus();
      version_status_t status;
      const char *recommended;
      if (c) {
        recommended = is_server ? c->server_versions : c->client_versions;
        status = tor_version_is_obsolete(VERSION, recommended);
      } else {
        recommended = "?";
        status = VS_UNKNOWN;
      }

      if (!strcmp(question, "status/version/recommended")) {
        *answer = tor_strdup(recommended);
        return 0;
      }
      if (!strcmp(question, "status/version/current")) {
        switch (status)
          {
          case VS_RECOMMENDED: *answer = tor_strdup("recommended"); break;
          case VS_OLD: *answer = tor_strdup("obsolete"); break;
          case VS_NEW: *answer = tor_strdup("new"); break;
          case VS_NEW_IN_SERIES: *answer = tor_strdup("new in series"); break;
          case VS_UNRECOMMENDED: *answer = tor_strdup("unrecommended"); break;
          case VS_EMPTY: *answer = tor_strdup("none recommended"); break;
          case VS_UNKNOWN: *answer = tor_strdup("unknown"); break;
          default: tor_fragile_assert();
          }
      } else if (!strcmp(question, "status/version/num-versioning") ||
                 !strcmp(question, "status/version/num-concurring")) {
        tor_asprintf(answer, "%d", get_n_authorities(V3_DIRINFO));
        log_warn(LD_GENERAL, "%s is deprecated; it no longer gives useful "
                 "information", question);
      }
    } else if (!strcmp(question, "status/clients-seen")) {
      char *bridge_stats = geoip_get_bridge_stats_controller(time(NULL));
      if (!bridge_stats) {
        *errmsg = "No bridge-client stats available";
        return -1;
      }
      *answer = bridge_stats;
    } else {
      return 0;
    }
  }
  return 0;
}

/** Callback function for GETINFO: on a given control connection, try to
 * answer the question <b>q</b> and store the newly-allocated answer in
 * *<b>a</b>. If an internal error occurs, return -1 and optionally set
 * *<b>error_out</b> to point to an error message to be delivered to the
 * controller. On success, _or if the key is not recognized_, return 0. Do not
 * set <b>a</b> if the key is not recognized.
 */
typedef int (*getinfo_helper_t)(control_connection_t *,
                                const char *q, char **a,
                                const char **error_out);

/** A single item for the GETINFO question-to-answer-function table. */
typedef struct getinfo_item_t {
  const char *varname; /**< The value (or prefix) of the question. */
  getinfo_helper_t fn; /**< The function that knows the answer: NULL if
                        * this entry is documentation-only. */
  const char *desc; /**< Description of the variable. */
  int is_prefix; /** Must varname match exactly, or must it be a prefix? */
} getinfo_item_t;

#define ITEM(name, fn, desc) { name, getinfo_helper_##fn, desc, 0 }
#define PREFIX(name, fn, desc) { name, getinfo_helper_##fn, desc, 1 }
#define DOC(name, desc) { name, NULL, desc, 0 }

/** Table mapping questions accepted by GETINFO to the functions that know how
 * to answer them. */
static const getinfo_item_t getinfo_items[] = {
  ITEM("version", misc, "The current version of Tor."),
  ITEM("config-file", misc, "Current location of the \"torrc\" file."),
  ITEM("config-defaults-file", misc, "Current location of the defaults file."),
  ITEM("config-text", misc,
       "Return the string that would be written by a saveconf command."),
  ITEM("accounting/bytes", accounting,
       "Number of bytes read/written so far in the accounting interval."),
  ITEM("accounting/bytes-left", accounting,
      "Number of bytes left to write/read so far in the accounting interval."),
  ITEM("accounting/enabled", accounting, "Is accounting currently enabled?"),
  ITEM("accounting/hibernating", accounting, "Are we hibernating or awake?"),
  ITEM("accounting/interval-start", accounting,
       "Time when the accounting period starts."),
  ITEM("accounting/interval-end", accounting,
       "Time when the accounting period ends."),
  ITEM("accounting/interval-wake", accounting,
       "Time to wake up in this accounting period."),
  ITEM("helper-nodes", entry_guards, NULL), /* deprecated */
  ITEM("entry-guards", entry_guards,
       "Which nodes are we using as entry guards?"),
  ITEM("fingerprint", misc, NULL),
  PREFIX("config/", config, "Current configuration values."),
  DOC("config/names",
      "List of configuration options, types, and documentation."),
  DOC("config/defaults",
      "List of default values for configuration options. "
      "See also config/names"),
  ITEM("info/names", misc,
       "List of GETINFO options, types, and documentation."),
  ITEM("events/names", misc,
       "Events that the controller can ask for with SETEVENTS."),
  ITEM("signal/names", misc, "Signal names recognized by the SIGNAL command"),
  ITEM("features/names", misc, "What arguments can USEFEATURE take?"),
  PREFIX("desc/id/", dir, "Router descriptors by ID."),
  PREFIX("desc/name/", dir, "Router descriptors by nickname."),
  ITEM("desc/all-recent", dir,
       "All non-expired, non-superseded router descriptors."),
  ITEM("desc/all-recent-extrainfo-hack", dir, NULL), /* Hack. */
  PREFIX("md/id/", dir, "Microdescriptors by ID"),
  PREFIX("md/name/", dir, "Microdescriptors by name"),
  PREFIX("extra-info/digest/", dir, "Extra-info documents by digest."),
  PREFIX("net/listeners/", listeners, "Bound addresses by type"),
  ITEM("ns/all", networkstatus,
       "Brief summary of router status (v2 directory format)"),
  PREFIX("ns/id/", networkstatus,
         "Brief summary of router status by ID (v2 directory format)."),
  PREFIX("ns/name/", networkstatus,
         "Brief summary of router status by nickname (v2 directory format)."),
  PREFIX("ns/purpose/", networkstatus,
         "Brief summary of router status by purpose (v2 directory format)."),
  ITEM("network-status", dir,
       "Brief summary of router status (v1 directory format)"),
  ITEM("circuit-status", events, "List of current circuits originating here."),
  ITEM("stream-status", events,"List of current streams."),
  ITEM("orconn-status", events, "A list of current OR connections."),
  ITEM("dormant", misc,
       "Is Tor dormant (not building circuits because it's idle)?"),
  PREFIX("address-mappings/", events, NULL),
  DOC("address-mappings/all", "Current address mappings."),
  DOC("address-mappings/cache", "Current cached DNS replies."),
  DOC("address-mappings/config",
      "Current address mappings from configuration."),
  DOC("address-mappings/control", "Current address mappings from controller."),
  PREFIX("status/", events, NULL),
  DOC("status/circuit-established",
      "Whether we think client functionality is working."),
  DOC("status/enough-dir-info",
      "Whether we have enough up-to-date directory information to build "
      "circuits."),
  DOC("status/bootstrap-phase",
      "The last bootstrap phase status event that Tor sent."),
  DOC("status/clients-seen",
      "Breakdown of client countries seen by a bridge."),
  DOC("status/version/recommended", "List of currently recommended versions."),
  DOC("status/version/current", "Status of the current version."),
  DOC("status/version/num-versioning", "Number of versioning authorities."),
  DOC("status/version/num-concurring",
      "Number of versioning authorities agreeing on the status of the "
      "current version"),
  ITEM("address", misc, "IP address of this Tor host, if we can guess it."),
  ITEM("traffic/read", misc,"Bytes read since the process was started."),
  ITEM("traffic/written", misc,
       "Bytes written since the process was started."),
  ITEM("process/pid", misc, "Process id belonging to the main tor process."),
  ITEM("process/uid", misc, "User id running the tor process."),
  ITEM("process/user", misc,
       "Username under which the tor process is running."),
  ITEM("process/descriptor-limit", misc, "File descriptor limit."),
  ITEM("limits/max-mem-in-queues", misc, "Actual limit on memory in queues"),
  ITEM("dir-usage", misc, "Breakdown of bytes transferred over DirPort."),
  PREFIX("desc-annotations/id/", dir, "Router annotations by hexdigest."),
  PREFIX("dir/server/", dir,"Router descriptors as retrieved from a DirPort."),
  PREFIX("dir/status/", dir,
         "v2 networkstatus docs as retrieved from a DirPort."),
  ITEM("dir/status-vote/current/consensus", dir,
       "v3 Networkstatus consensus as retrieved from a DirPort."),
  ITEM("exit-policy/default", policies,
       "The default value appended to the configured exit policy."),
  ITEM("exit-policy/full", policies, "The entire exit policy of onion router"),
  ITEM("exit-policy/ipv4", policies, "IPv4 parts of exit policy"),
  ITEM("exit-policy/ipv6", policies, "IPv6 parts of exit policy"),
  PREFIX("ip-to-country/", geoip, "Perform a GEOIP lookup"),
  { NULL, NULL, NULL, 0 }
};

/** Allocate and return a list of recognized GETINFO options. */
static char *
list_getinfo_options(void)
{
  int i;
  smartlist_t *lines = smartlist_new();
  char *ans;
  for (i = 0; getinfo_items[i].varname; ++i) {
    if (!getinfo_items[i].desc)
      continue;

    smartlist_add_asprintf(lines, "%s%s -- %s\n",
                 getinfo_items[i].varname,
                 getinfo_items[i].is_prefix ? "*" : "",
                 getinfo_items[i].desc);
  }
  smartlist_sort_strings(lines);

  ans = smartlist_join_strings(lines, "", 0, NULL);
  SMARTLIST_FOREACH(lines, char *, cp, tor_free(cp));
  smartlist_free(lines);

  return ans;
}

/** Lookup the 'getinfo' entry <b>question</b>, and return
 * the answer in <b>*answer</b> (or NULL if key not recognized).
 * Return 0 if success or unrecognized, or -1 if recognized but
 * internal error. */
static int
handle_getinfo_helper(control_connection_t *control_conn,
                      const char *question, char **answer,
                      const char **err_out)
{
  int i;
  *answer = NULL; /* unrecognized key by default */

  for (i = 0; getinfo_items[i].varname; ++i) {
    int match;
    if (getinfo_items[i].is_prefix)
      match = !strcmpstart(question, getinfo_items[i].varname);
    else
      match = !strcmp(question, getinfo_items[i].varname);
    if (match) {
      tor_assert(getinfo_items[i].fn);
      return getinfo_items[i].fn(control_conn, question, answer, err_out);
    }
  }

  return 0; /* unrecognized */
}

/** Called when we receive a GETINFO command.  Try to fetch all requested
 * information, and reply with information or error message. */
static int
handle_control_getinfo(control_connection_t *conn, uint32_t len,
                       const char *body)
{
  smartlist_t *questions = smartlist_new();
  smartlist_t *answers = smartlist_new();
  smartlist_t *unrecognized = smartlist_new();
  char *msg = NULL, *ans = NULL;
  int i;
  (void) len; /* body is NUL-terminated, so it's safe to ignore the length. */

  smartlist_split_string(questions, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH_BEGIN(questions, const char *, q) {
    const char *errmsg = NULL;
    if (handle_getinfo_helper(conn, q, &ans, &errmsg) < 0) {
      if (!errmsg)
        errmsg = "Internal error";
      connection_printf_to_buf(conn, "551 %s\r\n", errmsg);
      goto done;
    }
    if (!ans) {
      smartlist_add(unrecognized, (char*)q);
    } else {
      smartlist_add(answers, tor_strdup(q));
      smartlist_add(answers, ans);
    }
  } SMARTLIST_FOREACH_END(q);
  if (smartlist_len(unrecognized)) {
    for (i=0; i < smartlist_len(unrecognized)-1; ++i)
      connection_printf_to_buf(conn,
                               "552-Unrecognized key \"%s\"\r\n",
                               (char*)smartlist_get(unrecognized, i));
    connection_printf_to_buf(conn,
                             "552 Unrecognized key \"%s\"\r\n",
                             (char*)smartlist_get(unrecognized, i));
    goto done;
  }

  for (i = 0; i < smartlist_len(answers); i += 2) {
    char *k = smartlist_get(answers, i);
    char *v = smartlist_get(answers, i+1);
    if (!strchr(v, '\n') && !strchr(v, '\r')) {
      connection_printf_to_buf(conn, "250-%s=", k);
      connection_write_str_to_buf(v, conn);
      connection_write_str_to_buf("\r\n", conn);
    } else {
      char *esc = NULL;
      size_t esc_len;
      esc_len = write_escaped_data(v, strlen(v), &esc);
      connection_printf_to_buf(conn, "250+%s=\r\n", k);
      connection_write_to_buf(esc, esc_len, TO_CONN(conn));
      tor_free(esc);
    }
  }
  connection_write_str_to_buf("250 OK\r\n", conn);

 done:
  SMARTLIST_FOREACH(answers, char *, cp, tor_free(cp));
  smartlist_free(answers);
  SMARTLIST_FOREACH(questions, char *, cp, tor_free(cp));
  smartlist_free(questions);
  smartlist_free(unrecognized);
  tor_free(msg);

  return 0;
}

/** Given a string, convert it to a circuit purpose. */
static uint8_t
circuit_purpose_from_string(const char *string)
{
  if (!strcasecmpstart(string, "purpose="))
    string += strlen("purpose=");

  if (!strcasecmp(string, "general"))
    return CIRCUIT_PURPOSE_C_GENERAL;
  else if (!strcasecmp(string, "controller"))
    return CIRCUIT_PURPOSE_CONTROLLER;
  else
    return CIRCUIT_PURPOSE_UNKNOWN;
}

/** Return a newly allocated smartlist containing the arguments to the command
 * waiting in <b>body</b>. If there are fewer than <b>min_args</b> arguments,
 * or if <b>max_args</b> is nonnegative and there are more than
 * <b>max_args</b> arguments, send a 512 error to the controller, using
 * <b>command</b> as the command name in the error message. */
static smartlist_t *
getargs_helper(const char *command, control_connection_t *conn,
               const char *body, int min_args, int max_args)
{
  smartlist_t *args = smartlist_new();
  smartlist_split_string(args, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  if (smartlist_len(args) < min_args) {
    connection_printf_to_buf(conn, "512 Missing argument to %s\r\n",command);
    goto err;
  } else if (max_args >= 0 && smartlist_len(args) > max_args) {
    connection_printf_to_buf(conn, "512 Too many arguments to %s\r\n",command);
    goto err;
  }
  return args;
 err:
  SMARTLIST_FOREACH(args, char *, s, tor_free(s));
  smartlist_free(args);
  return NULL;
}

/** Helper.  Return the first element of <b>sl</b> at index <b>start_at</b> or
 * higher that starts with <b>prefix</b>, case-insensitive.  Return NULL if no
 * such element exists. */
static const char *
find_element_starting_with(smartlist_t *sl, int start_at, const char *prefix)
{
  int i;
  for (i = start_at; i < smartlist_len(sl); ++i) {
    const char *elt = smartlist_get(sl, i);
    if (!strcasecmpstart(elt, prefix))
      return elt;
  }
  return NULL;
}

/** Helper.  Return true iff s is an argument that we should treat as a
 * key-value pair. */
static int
is_keyval_pair(const char *s)
{
  /* An argument is a key-value pair if it has an =, and it isn't of the form
   * $fingeprint=name */
  return strchr(s, '=') && s[0] != '$';
}

/** Called when we get an EXTENDCIRCUIT message.  Try to extend the listed
 * circuit, and report success or failure. */
static int
handle_control_extendcircuit(control_connection_t *conn, uint32_t len,
                             const char *body)
{
  smartlist_t *router_nicknames=NULL, *nodes=NULL;
  origin_circuit_t *circ = NULL;
  int zero_circ;
  uint8_t intended_purpose = CIRCUIT_PURPOSE_C_GENERAL;
  smartlist_t *args;
  (void) len;

  router_nicknames = smartlist_new();

  args = getargs_helper("EXTENDCIRCUIT", conn, body, 1, -1);
  if (!args)
    goto done;

  zero_circ = !strcmp("0", (char*)smartlist_get(args,0));

  if (zero_circ) {
    const char *purp = find_element_starting_with(args, 1, "PURPOSE=");

    if (purp) {
      intended_purpose = circuit_purpose_from_string(purp);
      if (intended_purpose == CIRCUIT_PURPOSE_UNKNOWN) {
        connection_printf_to_buf(conn, "552 Unknown purpose \"%s\"\r\n", purp);
        SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
        smartlist_free(args);
        goto done;
      }
    }

    if ((smartlist_len(args) == 1) ||
        (smartlist_len(args) >= 2 && is_keyval_pair(smartlist_get(args, 1)))) {
      // "EXTENDCIRCUIT 0" || EXTENDCIRCUIT 0 foo=bar"
      circ = circuit_launch(intended_purpose, CIRCLAUNCH_NEED_CAPACITY);
      if (!circ) {
        connection_write_str_to_buf("551 Couldn't start circuit\r\n", conn);
      } else {
        connection_printf_to_buf(conn, "250 EXTENDED %lu\r\n",
                  (unsigned long)circ->global_identifier);
      }
      SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
      smartlist_free(args);
      goto done;
    }
    // "EXTENDCIRCUIT 0 router1,router2" ||
    // "EXTENDCIRCUIT 0 router1,router2 PURPOSE=foo"
  }

  if (!zero_circ && !(circ = get_circ(smartlist_get(args,0)))) {
    connection_printf_to_buf(conn, "552 Unknown circuit \"%s\"\r\n",
                             (char*)smartlist_get(args, 0));
    SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
    smartlist_free(args);
    goto done;
  }

  smartlist_split_string(router_nicknames, smartlist_get(args,1), ",", 0, 0);

  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);

  nodes = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(router_nicknames, const char *, n) {
    const node_t *node = node_get_by_nickname(n, 1);
    if (!node) {
      connection_printf_to_buf(conn, "552 No such router \"%s\"\r\n", n);
      goto done;
    }
    if (!node_has_descriptor(node)) {
      connection_printf_to_buf(conn, "552 No descriptor for \"%s\"\r\n", n);
      goto done;
    }
    smartlist_add(nodes, (void*)node);
  } SMARTLIST_FOREACH_END(n);
  if (!smartlist_len(nodes)) {
    connection_write_str_to_buf("512 No router names provided\r\n", conn);
    goto done;
  }

  if (zero_circ) {
    /* start a new circuit */
    circ = origin_circuit_init(intended_purpose, 0);
  }

  /* now circ refers to something that is ready to be extended */
  SMARTLIST_FOREACH(nodes, const node_t *, node,
  {
    extend_info_t *info = extend_info_from_node(node, 0);
    tor_assert(info); /* True, since node_has_descriptor(node) == true */
    circuit_append_new_exit(circ, info);
    extend_info_free(info);
  });

  /* now that we've populated the cpath, start extending */
  if (zero_circ) {
    int err_reason = 0;
    if ((err_reason = circuit_handle_first_hop(circ)) < 0) {
      circuit_mark_for_close(TO_CIRCUIT(circ), -err_reason);
      connection_write_str_to_buf("551 Couldn't start circuit\r\n", conn);
      goto done;
    }
  } else {
    if (circ->base_.state == CIRCUIT_STATE_OPEN) {
      int err_reason = 0;
      circuit_set_state(TO_CIRCUIT(circ), CIRCUIT_STATE_BUILDING);
      if ((err_reason = circuit_send_next_onion_skin(circ)) < 0) {
        log_info(LD_CONTROL,
                 "send_next_onion_skin failed; circuit marked for closing.");
        circuit_mark_for_close(TO_CIRCUIT(circ), -err_reason);
        connection_write_str_to_buf("551 Couldn't send onion skin\r\n", conn);
        goto done;
      }
    }
  }

  connection_printf_to_buf(conn, "250 EXTENDED %lu\r\n",
                             (unsigned long)circ->global_identifier);
  if (zero_circ) /* send a 'launched' event, for completeness */
    control_event_circuit_status(circ, CIRC_EVENT_LAUNCHED, 0);
 done:
  SMARTLIST_FOREACH(router_nicknames, char *, n, tor_free(n));
  smartlist_free(router_nicknames);
  smartlist_free(nodes);
  return 0;
}

/** Called when we get a SETCIRCUITPURPOSE message. If we can find the
 * circuit and it's a valid purpose, change it. */
static int
handle_control_setcircuitpurpose(control_connection_t *conn,
                                 uint32_t len, const char *body)
{
  origin_circuit_t *circ = NULL;
  uint8_t new_purpose;
  smartlist_t *args;
  (void) len; /* body is NUL-terminated, so it's safe to ignore the length. */

  args = getargs_helper("SETCIRCUITPURPOSE", conn, body, 2, -1);
  if (!args)
    goto done;

  if (!(circ = get_circ(smartlist_get(args,0)))) {
    connection_printf_to_buf(conn, "552 Unknown circuit \"%s\"\r\n",
                             (char*)smartlist_get(args, 0));
    goto done;
  }

  {
    const char *purp = find_element_starting_with(args,1,"PURPOSE=");
    if (!purp) {
      connection_write_str_to_buf("552 No purpose given\r\n", conn);
      goto done;
    }
    new_purpose = circuit_purpose_from_string(purp);
    if (new_purpose == CIRCUIT_PURPOSE_UNKNOWN) {
      connection_printf_to_buf(conn, "552 Unknown purpose \"%s\"\r\n", purp);
      goto done;
    }
  }

  circuit_change_purpose(TO_CIRCUIT(circ), new_purpose);
  connection_write_str_to_buf("250 OK\r\n", conn);

 done:
  if (args) {
    SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
    smartlist_free(args);
  }
  return 0;
}

/** Called when we get an ATTACHSTREAM message.  Try to attach the requested
 * stream, and report success or failure. */
static int
handle_control_attachstream(control_connection_t *conn, uint32_t len,
                            const char *body)
{
  entry_connection_t *ap_conn = NULL;
  origin_circuit_t *circ = NULL;
  int zero_circ;
  smartlist_t *args;
  crypt_path_t *cpath=NULL;
  int hop=0, hop_line_ok=1;
  (void) len;

  args = getargs_helper("ATTACHSTREAM", conn, body, 2, -1);
  if (!args)
    return 0;

  zero_circ = !strcmp("0", (char*)smartlist_get(args,1));

  if (!(ap_conn = get_stream(smartlist_get(args, 0)))) {
    connection_printf_to_buf(conn, "552 Unknown stream \"%s\"\r\n",
                             (char*)smartlist_get(args, 0));
  } else if (!zero_circ && !(circ = get_circ(smartlist_get(args, 1)))) {
    connection_printf_to_buf(conn, "552 Unknown circuit \"%s\"\r\n",
                             (char*)smartlist_get(args, 1));
  } else if (circ) {
    const char *hopstring = find_element_starting_with(args,2,"HOP=");
    if (hopstring) {
      hopstring += strlen("HOP=");
      hop = (int) tor_parse_ulong(hopstring, 10, 0, INT_MAX,
                                  &hop_line_ok, NULL);
      if (!hop_line_ok) { /* broken hop line */
        connection_printf_to_buf(conn, "552 Bad value hop=%s\r\n", hopstring);
      }
    }
  }
  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  if (!ap_conn || (!zero_circ && !circ) || !hop_line_ok)
    return 0;

  if (ENTRY_TO_CONN(ap_conn)->state != AP_CONN_STATE_CONTROLLER_WAIT &&
      ENTRY_TO_CONN(ap_conn)->state != AP_CONN_STATE_CONNECT_WAIT &&
      ENTRY_TO_CONN(ap_conn)->state != AP_CONN_STATE_RESOLVE_WAIT) {
    connection_write_str_to_buf(
                    "555 Connection is not managed by controller.\r\n",
                    conn);
    return 0;
  }

  /* Do we need to detach it first? */
  if (ENTRY_TO_CONN(ap_conn)->state != AP_CONN_STATE_CONTROLLER_WAIT) {
    edge_connection_t *edge_conn = ENTRY_TO_EDGE_CONN(ap_conn);
    circuit_t *tmpcirc = circuit_get_by_edge_conn(edge_conn);
    connection_edge_end(edge_conn, END_STREAM_REASON_TIMEOUT);
    /* Un-mark it as ending, since we're going to reuse it. */
    edge_conn->edge_has_sent_end = 0;
    edge_conn->end_reason = 0;
    if (tmpcirc)
      circuit_detach_stream(tmpcirc, edge_conn);
    TO_CONN(edge_conn)->state = AP_CONN_STATE_CONTROLLER_WAIT;
  }

  if (circ && (circ->base_.state != CIRCUIT_STATE_OPEN)) {
    connection_write_str_to_buf(
                    "551 Can't attach stream to non-open origin circuit\r\n",
                    conn);
    return 0;
  }
  /* Is this a single hop circuit? */
  if (circ && (circuit_get_cpath_len(circ)<2 || hop==1)) {
    const node_t *node = NULL;
    char *exit_digest = NULL;
    if (circ->build_state &&
        circ->build_state->chosen_exit &&
        !tor_digest_is_zero(circ->build_state->chosen_exit->identity_digest)) {
      exit_digest = circ->build_state->chosen_exit->identity_digest;
      node = node_get_by_id(exit_digest);
    }
    /* Do both the client and relay allow one-hop exit circuits? */
    if (!node ||
        !node_allows_single_hop_exits(node) ||
        !get_options()->AllowSingleHopCircuits) {
      connection_write_str_to_buf(
      "551 Can't attach stream to this one-hop circuit.\r\n", conn);
      return 0;
    }
    tor_assert(exit_digest);
    ap_conn->chosen_exit_name = tor_strdup(hex_str(exit_digest, DIGEST_LEN));
  }

  if (circ && hop>0) {
    /* find this hop in the circuit, and set cpath */
    cpath = circuit_get_cpath_hop(circ, hop);
    if (!cpath) {
      connection_printf_to_buf(conn,
                               "551 Circuit doesn't have %d hops.\r\n", hop);
      return 0;
    }
  }
  if (connection_ap_handshake_rewrite_and_attach(ap_conn, circ, cpath) < 0) {
    connection_write_str_to_buf("551 Unable to attach stream\r\n", conn);
    return 0;
  }
  send_control_done(conn);
  return 0;
}

/** Called when we get a POSTDESCRIPTOR message.  Try to learn the provided
 * descriptor, and report success or failure. */
static int
handle_control_postdescriptor(control_connection_t *conn, uint32_t len,
                              const char *body)
{
  char *desc;
  const char *msg=NULL;
  uint8_t purpose = ROUTER_PURPOSE_GENERAL;
  int cache = 0; /* eventually, we may switch this to 1 */

  char *cp = memchr(body, '\n', len);
  smartlist_t *args = smartlist_new();
  tor_assert(cp);
  *cp++ = '\0';

  smartlist_split_string(args, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH_BEGIN(args, char *, option) {
    if (!strcasecmpstart(option, "purpose=")) {
      option += strlen("purpose=");
      purpose = router_purpose_from_string(option);
      if (purpose == ROUTER_PURPOSE_UNKNOWN) {
        connection_printf_to_buf(conn, "552 Unknown purpose \"%s\"\r\n",
                                 option);
        goto done;
      }
    } else if (!strcasecmpstart(option, "cache=")) {
      option += strlen("cache=");
      if (!strcasecmp(option, "no"))
        cache = 0;
      else if (!strcasecmp(option, "yes"))
        cache = 1;
      else {
        connection_printf_to_buf(conn, "552 Unknown cache request \"%s\"\r\n",
                                 option);
        goto done;
      }
    } else { /* unrecognized argument? */
      connection_printf_to_buf(conn,
        "512 Unexpected argument \"%s\" to postdescriptor\r\n", option);
      goto done;
    }
  } SMARTLIST_FOREACH_END(option);

  read_escaped_data(cp, len-(cp-body), &desc);

  switch (router_load_single_router(desc, purpose, cache, &msg)) {
  case -1:
    if (!msg) msg = "Could not parse descriptor";
    connection_printf_to_buf(conn, "554 %s\r\n", msg);
    break;
  case 0:
    if (!msg) msg = "Descriptor not added";
    connection_printf_to_buf(conn, "251 %s\r\n",msg);
    break;
  case 1:
    send_control_done(conn);
    break;
  }

  tor_free(desc);
 done:
  SMARTLIST_FOREACH(args, char *, arg, tor_free(arg));
  smartlist_free(args);
  return 0;
}

/** Called when we receive a REDIRECTSTERAM command.  Try to change the target
 * address of the named AP stream, and report success or failure. */
static int
handle_control_redirectstream(control_connection_t *conn, uint32_t len,
                              const char *body)
{
  entry_connection_t *ap_conn = NULL;
  char *new_addr = NULL;
  uint16_t new_port = 0;
  smartlist_t *args;
  (void) len;

  args = getargs_helper("REDIRECTSTREAM", conn, body, 2, -1);
  if (!args)
    return 0;

  if (!(ap_conn = get_stream(smartlist_get(args, 0)))
           || !ap_conn->socks_request) {
    connection_printf_to_buf(conn, "552 Unknown stream \"%s\"\r\n",
                             (char*)smartlist_get(args, 0));
  } else {
    int ok = 1;
    if (smartlist_len(args) > 2) { /* they included a port too */
      new_port = (uint16_t) tor_parse_ulong(smartlist_get(args, 2),
                                            10, 1, 65535, &ok, NULL);
    }
    if (!ok) {
      connection_printf_to_buf(conn, "512 Cannot parse port \"%s\"\r\n",
                               (char*)smartlist_get(args, 2));
    } else {
      new_addr = tor_strdup(smartlist_get(args, 1));
    }
  }

  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  if (!new_addr)
    return 0;

  strlcpy(ap_conn->socks_request->address, new_addr,
          sizeof(ap_conn->socks_request->address));
  if (new_port)
    ap_conn->socks_request->port = new_port;
  tor_free(new_addr);
  send_control_done(conn);
  return 0;
}

/** Called when we get a CLOSESTREAM command; try to close the named stream
 * and report success or failure. */
static int
handle_control_closestream(control_connection_t *conn, uint32_t len,
                           const char *body)
{
  entry_connection_t *ap_conn=NULL;
  uint8_t reason=0;
  smartlist_t *args;
  int ok;
  (void) len;

  args = getargs_helper("CLOSESTREAM", conn, body, 2, -1);
  if (!args)
    return 0;

  else if (!(ap_conn = get_stream(smartlist_get(args, 0))))
    connection_printf_to_buf(conn, "552 Unknown stream \"%s\"\r\n",
                             (char*)smartlist_get(args, 0));
  else {
    reason = (uint8_t) tor_parse_ulong(smartlist_get(args,1), 10, 0, 255,
                                       &ok, NULL);
    if (!ok) {
      connection_printf_to_buf(conn, "552 Unrecognized reason \"%s\"\r\n",
                               (char*)smartlist_get(args, 1));
      ap_conn = NULL;
    }
  }
  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  if (!ap_conn)
    return 0;

  connection_mark_unattached_ap(ap_conn, reason);
  send_control_done(conn);
  return 0;
}

/** Called when we get a CLOSECIRCUIT command; try to close the named circuit
 * and report success or failure. */
static int
handle_control_closecircuit(control_connection_t *conn, uint32_t len,
                            const char *body)
{
  origin_circuit_t *circ = NULL;
  int safe = 0;
  smartlist_t *args;
  (void) len;

  args = getargs_helper("CLOSECIRCUIT", conn, body, 1, -1);
  if (!args)
    return 0;

  if (!(circ=get_circ(smartlist_get(args, 0))))
    connection_printf_to_buf(conn, "552 Unknown circuit \"%s\"\r\n",
                             (char*)smartlist_get(args, 0));
  else {
    int i;
    for (i=1; i < smartlist_len(args); ++i) {
      if (!strcasecmp(smartlist_get(args, i), "IfUnused"))
        safe = 1;
      else
        log_info(LD_CONTROL, "Skipping unknown option %s",
                 (char*)smartlist_get(args,i));
    }
  }
  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  if (!circ)
    return 0;

  if (!safe || !circ->p_streams) {
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_REQUESTED);
  }

  send_control_done(conn);
  return 0;
}

/** Called when we get a RESOLVE command: start trying to resolve
 * the listed addresses. */
static int
handle_control_resolve(control_connection_t *conn, uint32_t len,
                       const char *body)
{
  smartlist_t *args, *failed;
  int is_reverse = 0;
  (void) len; /* body is nul-terminated; it's safe to ignore the length */

  if (!(conn->event_mask & (((event_mask_t)1)<<EVENT_ADDRMAP))) {
    log_warn(LD_CONTROL, "Controller asked us to resolve an address, but "
             "isn't listening for ADDRMAP events.  It probably won't see "
             "the answer.");
  }
  args = smartlist_new();
  smartlist_split_string(args, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  {
    const char *modearg = find_element_starting_with(args, 0, "mode=");
    if (modearg && !strcasecmp(modearg, "mode=reverse"))
      is_reverse = 1;
  }
  failed = smartlist_new();
  SMARTLIST_FOREACH(args, const char *, arg, {
      if (!is_keyval_pair(arg)) {
          if (dnsserv_launch_request(arg, is_reverse, conn)<0)
            smartlist_add(failed, (char*)arg);
      }
  });

  send_control_done(conn);
  SMARTLIST_FOREACH(failed, const char *, arg, {
      control_event_address_mapped(arg, arg, time(NULL),
                                   "internal", 0);
  });

  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  smartlist_free(failed);
  return 0;
}

/** Called when we get a PROTOCOLINFO command: send back a reply. */
static int
handle_control_protocolinfo(control_connection_t *conn, uint32_t len,
                            const char *body)
{
  const char *bad_arg = NULL;
  smartlist_t *args;
  (void)len;

  conn->have_sent_protocolinfo = 1;
  args = smartlist_new();
  smartlist_split_string(args, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH(args, const char *, arg, {
      int ok;
      tor_parse_long(arg, 10, 0, LONG_MAX, &ok, NULL);
      if (!ok) {
        bad_arg = arg;
        break;
      }
    });
  if (bad_arg) {
    connection_printf_to_buf(conn, "513 No such version %s\r\n",
                             escaped(bad_arg));
    /* Don't tolerate bad arguments when not authenticated. */
    if (!STATE_IS_OPEN(TO_CONN(conn)->state))
      connection_mark_for_close(TO_CONN(conn));
    goto done;
  } else {
    const or_options_t *options = get_options();
    int cookies = options->CookieAuthentication;
    char *cfile = get_controller_cookie_file_name();
    char *abs_cfile;
    char *esc_cfile;
    char *methods;
    abs_cfile = make_path_absolute(cfile);
    esc_cfile = esc_for_log(abs_cfile);
    {
      int passwd = (options->HashedControlPassword != NULL ||
                    options->HashedControlSessionPassword != NULL);
      smartlist_t *mlist = smartlist_new();
      if (cookies) {
        smartlist_add(mlist, (char*)"COOKIE");
        smartlist_add(mlist, (char*)"SAFECOOKIE");
      }
      if (passwd)
        smartlist_add(mlist, (char*)"HASHEDPASSWORD");
      if (!cookies && !passwd)
        smartlist_add(mlist, (char*)"NULL");
      methods = smartlist_join_strings(mlist, ",", 0, NULL);
      smartlist_free(mlist);
    }

    connection_printf_to_buf(conn,
                             "250-PROTOCOLINFO 1\r\n"
                             "250-AUTH METHODS=%s%s%s\r\n"
                             "250-VERSION Tor=%s\r\n"
                             "250 OK\r\n",
                             methods,
                             cookies?" COOKIEFILE=":"",
                             cookies?esc_cfile:"",
                             escaped(VERSION));
    tor_free(methods);
    tor_free(cfile);
    tor_free(abs_cfile);
    tor_free(esc_cfile);
  }
 done:
  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  return 0;
}

/** Called when we get an AUTHCHALLENGE command. */
static int
handle_control_authchallenge(control_connection_t *conn, uint32_t len,
                             const char *body)
{
  const char *cp = body;
  char *client_nonce;
  size_t client_nonce_len;
  char server_hash[DIGEST256_LEN];
  char server_hash_encoded[HEX_DIGEST256_LEN+1];
  char server_nonce[SAFECOOKIE_SERVER_NONCE_LEN];
  char server_nonce_encoded[(2*SAFECOOKIE_SERVER_NONCE_LEN) + 1];

  cp += strspn(cp, " \t\n\r");
  if (!strcasecmpstart(cp, "SAFECOOKIE")) {
    cp += strlen("SAFECOOKIE");
  } else {
    connection_write_str_to_buf("513 AUTHCHALLENGE only supports SAFECOOKIE "
                                "authentication\r\n", conn);
    connection_mark_for_close(TO_CONN(conn));
    return -1;
  }

  if (!authentication_cookie_is_set) {
    connection_write_str_to_buf("515 Cookie authentication is disabled\r\n",
                                conn);
    connection_mark_for_close(TO_CONN(conn));
    return -1;
  }

  cp += strspn(cp, " \t\n\r");
  if (*cp == '"') {
    const char *newcp =
      decode_escaped_string(cp, len - (cp - body),
                            &client_nonce, &client_nonce_len);
    if (newcp == NULL) {
      connection_write_str_to_buf("513 Invalid quoted client nonce\r\n",
                                  conn);
      connection_mark_for_close(TO_CONN(conn));
      return -1;
    }
    cp = newcp;
  } else {
    size_t client_nonce_encoded_len = strspn(cp, "0123456789ABCDEFabcdef");

    client_nonce_len = client_nonce_encoded_len / 2;
    client_nonce = tor_malloc_zero(client_nonce_len);

    if (base16_decode(client_nonce, client_nonce_len,
                      cp, client_nonce_encoded_len) < 0) {
      connection_write_str_to_buf("513 Invalid base16 client nonce\r\n",
                                  conn);
      connection_mark_for_close(TO_CONN(conn));
      tor_free(client_nonce);
      return -1;
    }

    cp += client_nonce_encoded_len;
  }

  cp += strspn(cp, " \t\n\r");
  if (*cp != '\0' ||
      cp != body + len) {
    connection_write_str_to_buf("513 Junk at end of AUTHCHALLENGE command\r\n",
                                conn);
    connection_mark_for_close(TO_CONN(conn));
    tor_free(client_nonce);
    return -1;
  }

  tor_assert(!crypto_rand(server_nonce, SAFECOOKIE_SERVER_NONCE_LEN));

  /* Now compute and send the server-to-controller response, and the
   * server's nonce. */
  tor_assert(authentication_cookie != NULL);

  {
    size_t tmp_len = (AUTHENTICATION_COOKIE_LEN +
                      client_nonce_len +
                      SAFECOOKIE_SERVER_NONCE_LEN);
    char *tmp = tor_malloc_zero(tmp_len);
    char *client_hash = tor_malloc_zero(DIGEST256_LEN);
    memcpy(tmp, authentication_cookie, AUTHENTICATION_COOKIE_LEN);
    memcpy(tmp + AUTHENTICATION_COOKIE_LEN, client_nonce, client_nonce_len);
    memcpy(tmp + AUTHENTICATION_COOKIE_LEN + client_nonce_len,
           server_nonce, SAFECOOKIE_SERVER_NONCE_LEN);

    crypto_hmac_sha256(server_hash,
                       SAFECOOKIE_SERVER_TO_CONTROLLER_CONSTANT,
                       strlen(SAFECOOKIE_SERVER_TO_CONTROLLER_CONSTANT),
                       tmp,
                       tmp_len);

    crypto_hmac_sha256(client_hash,
                       SAFECOOKIE_CONTROLLER_TO_SERVER_CONSTANT,
                       strlen(SAFECOOKIE_CONTROLLER_TO_SERVER_CONSTANT),
                       tmp,
                       tmp_len);

    conn->safecookie_client_hash = client_hash;

    tor_free(tmp);
  }

  base16_encode(server_hash_encoded, sizeof(server_hash_encoded),
                server_hash, sizeof(server_hash));
  base16_encode(server_nonce_encoded, sizeof(server_nonce_encoded),
                server_nonce, sizeof(server_nonce));

  connection_printf_to_buf(conn,
                           "250 AUTHCHALLENGE SERVERHASH=%s "
                           "SERVERNONCE=%s\r\n",
                           server_hash_encoded,
                           server_nonce_encoded);

  tor_free(client_nonce);
  return 0;
}

/** Called when we get a USEFEATURE command: parse the feature list, and
 * set up the control_connection's options properly. */
static int
handle_control_usefeature(control_connection_t *conn,
                          uint32_t len,
                          const char *body)
{
  smartlist_t *args;
  int bad = 0;
  (void) len; /* body is nul-terminated; it's safe to ignore the length */
  args = smartlist_new();
  smartlist_split_string(args, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH_BEGIN(args, const char *, arg) {
      if (!strcasecmp(arg, "VERBOSE_NAMES"))
        ;
      else if (!strcasecmp(arg, "EXTENDED_EVENTS"))
        ;
      else {
        connection_printf_to_buf(conn, "552 Unrecognized feature \"%s\"\r\n",
                                 arg);
        bad = 1;
        break;
      }
  } SMARTLIST_FOREACH_END(arg);

  if (!bad) {
    send_control_done(conn);
  }

  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  return 0;
}

/** Implementation for the DROPGUARDS command. */
static int
handle_control_dropguards(control_connection_t *conn,
                          uint32_t len,
                          const char *body)
{
  smartlist_t *args;
  (void) len; /* body is nul-terminated; it's safe to ignore the length */
  args = smartlist_new();
  smartlist_split_string(args, body, " ",
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);

  if (smartlist_len(args)) {
    connection_printf_to_buf(conn, "512 Too many arguments to DROPGUARDS\r\n");
  } else {
    remove_all_entry_guards();
    send_control_done(conn);
  }

  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  return 0;
}

/** Called when <b>conn</b> has no more bytes left on its outbuf. */
int
connection_control_finished_flushing(control_connection_t *conn)
{
  tor_assert(conn);
  return 0;
}

/** Called when <b>conn</b> has gotten its socket closed. */
int
connection_control_reached_eof(control_connection_t *conn)
{
  tor_assert(conn);

  log_info(LD_CONTROL,"Control connection reached EOF. Closing.");
  connection_mark_for_close(TO_CONN(conn));
  return 0;
}

static void lost_owning_controller(const char *owner_type,
                                   const char *loss_manner)
  ATTR_NORETURN;

/** Shut down this Tor instance in the same way that SIGINT would, but
 * with a log message appropriate for the loss of an owning controller. */
static void
lost_owning_controller(const char *owner_type, const char *loss_manner)
{
  log_notice(LD_CONTROL, "Owning controller %s has %s -- exiting now.",
             owner_type, loss_manner);

  /* XXXX Perhaps this chunk of code should be a separate function,
   * called here and by process_signal(SIGINT). */
  tor_cleanup();
  exit(0);
}

/** Called when <b>conn</b> is being freed. */
void
connection_control_closed(control_connection_t *conn)
{
  tor_assert(conn);

  conn->event_mask = 0;
  control_update_global_event_mask();

  if (conn->is_owning_control_connection) {
    lost_owning_controller("connection", "closed");
  }
}

/** Return true iff <b>cmd</b> is allowable (or at least forgivable) at this
 * stage of the protocol. */
static int
is_valid_initial_command(control_connection_t *conn, const char *cmd)
{
  if (conn->base_.state == CONTROL_CONN_STATE_OPEN)
    return 1;
  if (!strcasecmp(cmd, "PROTOCOLINFO"))
    return (!conn->have_sent_protocolinfo &&
            conn->safecookie_client_hash == NULL);
  if (!strcasecmp(cmd, "AUTHCHALLENGE"))
    return (conn->safecookie_client_hash == NULL);
  if (!strcasecmp(cmd, "AUTHENTICATE") ||
      !strcasecmp(cmd, "QUIT"))
    return 1;
  return 0;
}

/** Do not accept any control command of more than 1MB in length.  Anything
 * that needs to be anywhere near this long probably means that one of our
 * interfaces is broken. */
#define MAX_COMMAND_LINE_LENGTH (1024*1024)

/** Wrapper around peek_(evbuffer|buf)_has_control0 command: presents the same
 * interface as those underlying functions, but takes a connection_t intead of
 * an evbuffer or a buf_t.
 */
static int
peek_connection_has_control0_command(connection_t *conn)
{
  IF_HAS_BUFFEREVENT(conn, {
    struct evbuffer *input = bufferevent_get_input(conn->bufev);
    return peek_evbuffer_has_control0_command(input);
  }) ELSE_IF_NO_BUFFEREVENT {
    return peek_buf_has_control0_command(conn->inbuf);
  }
}

/** Called when data has arrived on a v1 control connection: Try to fetch
 * commands from conn->inbuf, and execute them.
 */
int
connection_control_process_inbuf(control_connection_t *conn)
{
  size_t data_len;
  uint32_t cmd_data_len;
  int cmd_len;
  char *args;

  tor_assert(conn);
  tor_assert(conn->base_.state == CONTROL_CONN_STATE_OPEN ||
             conn->base_.state == CONTROL_CONN_STATE_NEEDAUTH);

  if (!conn->incoming_cmd) {
    conn->incoming_cmd = tor_malloc(1024);
    conn->incoming_cmd_len = 1024;
    conn->incoming_cmd_cur_len = 0;
  }

  if (conn->base_.state == CONTROL_CONN_STATE_NEEDAUTH &&
      peek_connection_has_control0_command(TO_CONN(conn))) {
    /* Detect v0 commands and send a "no more v0" message. */
    size_t body_len;
    char buf[128];
    set_uint16(buf+2, htons(0x0000)); /* type == error */
    set_uint16(buf+4, htons(0x0001)); /* code == internal error */
    strlcpy(buf+6, "The v0 control protocol is not supported by Tor 0.1.2.17 "
            "and later; upgrade your controller.",
            sizeof(buf)-6);
    body_len = 2+strlen(buf+6)+2; /* code, msg, nul. */
    set_uint16(buf+0, htons(body_len));
    connection_write_to_buf(buf, 4+body_len, TO_CONN(conn));

    connection_mark_and_flush(TO_CONN(conn));
    return 0;
  }

 again:
  while (1) {
    size_t last_idx;
    int r;
    /* First, fetch a line. */
    do {
      data_len = conn->incoming_cmd_len - conn->incoming_cmd_cur_len;
      r = connection_fetch_from_buf_line(TO_CONN(conn),
                              conn->incoming_cmd+conn->incoming_cmd_cur_len,
                              &data_len);
      if (r == 0)
        /* Line not all here yet. Wait. */
        return 0;
      else if (r == -1) {
        if (data_len + conn->incoming_cmd_cur_len > MAX_COMMAND_LINE_LENGTH) {
          connection_write_str_to_buf("500 Line too long.\r\n", conn);
          connection_stop_reading(TO_CONN(conn));
          connection_mark_and_flush(TO_CONN(conn));
        }
        while (conn->incoming_cmd_len < data_len+conn->incoming_cmd_cur_len)
          conn->incoming_cmd_len *= 2;
        conn->incoming_cmd = tor_realloc(conn->incoming_cmd,
                                         conn->incoming_cmd_len);
      }
    } while (r != 1);

    tor_assert(data_len);

    last_idx = conn->incoming_cmd_cur_len;
    conn->incoming_cmd_cur_len += (int)data_len;

    /* We have appended a line to incoming_cmd.  Is the command done? */
    if (last_idx == 0 && *conn->incoming_cmd != '+')
      /* One line command, didn't start with '+'. */
      break;
    /* XXXX this code duplication is kind of dumb. */
    if (last_idx+3 == conn->incoming_cmd_cur_len &&
        tor_memeq(conn->incoming_cmd + last_idx, ".\r\n", 3)) {
      /* Just appended ".\r\n"; we're done. Remove it. */
      conn->incoming_cmd[last_idx] = '\0';
      conn->incoming_cmd_cur_len -= 3;
      break;
    } else if (last_idx+2 == conn->incoming_cmd_cur_len &&
               tor_memeq(conn->incoming_cmd + last_idx, ".\n", 2)) {
      /* Just appended ".\n"; we're done. Remove it. */
      conn->incoming_cmd[last_idx] = '\0';
      conn->incoming_cmd_cur_len -= 2;
      break;
    }
    /* Otherwise, read another line. */
  }
  data_len = conn->incoming_cmd_cur_len;
  /* Okay, we now have a command sitting on conn->incoming_cmd. See if we
   * recognize it.
   */
  cmd_len = 0;
  while ((size_t)cmd_len < data_len
         && !TOR_ISSPACE(conn->incoming_cmd[cmd_len]))
    ++cmd_len;

  conn->incoming_cmd[cmd_len]='\0';
  args = conn->incoming_cmd+cmd_len+1;
  tor_assert(data_len>(size_t)cmd_len);
  data_len -= (cmd_len+1); /* skip the command and NUL we added after it */
  while (TOR_ISSPACE(*args)) {
    ++args;
    --data_len;
  }

  /* If the connection is already closing, ignore further commands */
  if (TO_CONN(conn)->marked_for_close) {
    return 0;
  }

  /* Otherwise, Quit is always valid. */
  if (!strcasecmp(conn->incoming_cmd, "QUIT")) {
    connection_write_str_to_buf("250 closing connection\r\n", conn);
    connection_mark_and_flush(TO_CONN(conn));
    return 0;
  }

  if (conn->base_.state == CONTROL_CONN_STATE_NEEDAUTH &&
      !is_valid_initial_command(conn, conn->incoming_cmd)) {
    connection_write_str_to_buf("514 Authentication required.\r\n", conn);
    connection_mark_for_close(TO_CONN(conn));
    return 0;
  }

  if (data_len >= UINT32_MAX) {
    connection_write_str_to_buf("500 A 4GB command? Nice try.\r\n", conn);
    connection_mark_for_close(TO_CONN(conn));
    return 0;
  }

  /* XXXX Why is this not implemented as a table like the GETINFO
   * items are?  Even handling the plus signs at the beginnings of
   * commands wouldn't be very hard with proper macros. */
  cmd_data_len = (uint32_t)data_len;
  if (!strcasecmp(conn->incoming_cmd, "SETCONF")) {
    if (handle_control_setconf(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "RESETCONF")) {
    if (handle_control_resetconf(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "GETCONF")) {
    if (handle_control_getconf(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "+LOADCONF")) {
    if (handle_control_loadconf(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "SETEVENTS")) {
    if (handle_control_setevents(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "AUTHENTICATE")) {
    if (handle_control_authenticate(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "SAVECONF")) {
    if (handle_control_saveconf(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "SIGNAL")) {
    if (handle_control_signal(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "TAKEOWNERSHIP")) {
    if (handle_control_takeownership(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "MAPADDRESS")) {
    if (handle_control_mapaddress(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "GETINFO")) {
    if (handle_control_getinfo(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "EXTENDCIRCUIT")) {
    if (handle_control_extendcircuit(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "SETCIRCUITPURPOSE")) {
    if (handle_control_setcircuitpurpose(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "SETROUTERPURPOSE")) {
    connection_write_str_to_buf("511 SETROUTERPURPOSE is obsolete.\r\n", conn);
  } else if (!strcasecmp(conn->incoming_cmd, "ATTACHSTREAM")) {
    if (handle_control_attachstream(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "+POSTDESCRIPTOR")) {
    if (handle_control_postdescriptor(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "REDIRECTSTREAM")) {
    if (handle_control_redirectstream(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "CLOSESTREAM")) {
    if (handle_control_closestream(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "CLOSECIRCUIT")) {
    if (handle_control_closecircuit(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "USEFEATURE")) {
    if (handle_control_usefeature(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "RESOLVE")) {
    if (handle_control_resolve(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "PROTOCOLINFO")) {
    if (handle_control_protocolinfo(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "AUTHCHALLENGE")) {
    if (handle_control_authchallenge(conn, cmd_data_len, args))
      return -1;
  } else if (!strcasecmp(conn->incoming_cmd, "DROPGUARDS")) {
    if (handle_control_dropguards(conn, cmd_data_len, args))
      return -1;
  } else {
    connection_printf_to_buf(conn, "510 Unrecognized command \"%s\"\r\n",
                             conn->incoming_cmd);
  }

  conn->incoming_cmd_cur_len = 0;
  goto again;
}

/** Something major has happened to circuit <b>circ</b>: tell any
 * interested control connections. */
int
control_event_circuit_status(origin_circuit_t *circ, circuit_status_event_t tp,
                             int reason_code)
{
  const char *status;
  char reasons[64] = "";
  if (!EVENT_IS_INTERESTING(EVENT_CIRCUIT_STATUS))
    return 0;
  tor_assert(circ);

  switch (tp)
    {
    case CIRC_EVENT_LAUNCHED: status = "LAUNCHED"; break;
    case CIRC_EVENT_BUILT: status = "BUILT"; break;
    case CIRC_EVENT_EXTENDED: status = "EXTENDED"; break;
    case CIRC_EVENT_FAILED: status = "FAILED"; break;
    case CIRC_EVENT_CLOSED: status = "CLOSED"; break;
    default:
      log_warn(LD_BUG, "Unrecognized status code %d", (int)tp);
      tor_fragile_assert();
      return 0;
    }

  if (tp == CIRC_EVENT_FAILED || tp == CIRC_EVENT_CLOSED) {
    const char *reason_str = circuit_end_reason_to_control_string(reason_code);
    char unk_reason_buf[16];
    if (!reason_str) {
      tor_snprintf(unk_reason_buf, 16, "UNKNOWN_%d", reason_code);
      reason_str = unk_reason_buf;
    }
    if (reason_code > 0 && reason_code & END_CIRC_REASON_FLAG_REMOTE) {
      tor_snprintf(reasons, sizeof(reasons),
                   " REASON=DESTROYED REMOTE_REASON=%s", reason_str);
    } else {
      tor_snprintf(reasons, sizeof(reasons),
                   " REASON=%s", reason_str);
    }
  }

  {
    char *circdesc = circuit_describe_status_for_controller(circ);
    const char *sp = strlen(circdesc) ? " " : "";
    send_control_event(EVENT_CIRCUIT_STATUS, ALL_FORMATS,
                                "650 CIRC %lu %s%s%s%s\r\n",
                                (unsigned long)circ->global_identifier,
                                status, sp,
                                circdesc,
                                reasons);
    tor_free(circdesc);
  }

  return 0;
}

/** Something minor has happened to circuit <b>circ</b>: tell any
 * interested control connections. */
static int
control_event_circuit_status_minor(origin_circuit_t *circ,
                                   circuit_status_minor_event_t e,
                                   int purpose, const struct timeval *tv)
{
  const char *event_desc;
  char event_tail[160] = "";
  if (!EVENT_IS_INTERESTING(EVENT_CIRCUIT_STATUS_MINOR))
    return 0;
  tor_assert(circ);

  switch (e)
    {
    case CIRC_MINOR_EVENT_PURPOSE_CHANGED:
      event_desc = "PURPOSE_CHANGED";

      {
        /* event_tail can currently be up to 68 chars long */
        const char *hs_state_str =
          circuit_purpose_to_controller_hs_state_string(purpose);
        tor_snprintf(event_tail, sizeof(event_tail),
                     " OLD_PURPOSE=%s%s%s",
                     circuit_purpose_to_controller_string(purpose),
                     (hs_state_str != NULL) ? " OLD_HS_STATE=" : "",
                     (hs_state_str != NULL) ? hs_state_str : "");
      }

      break;
    case CIRC_MINOR_EVENT_CANNIBALIZED:
      event_desc = "CANNIBALIZED";

      {
        /* event_tail can currently be up to 130 chars long */
        const char *hs_state_str =
          circuit_purpose_to_controller_hs_state_string(purpose);
        const struct timeval *old_timestamp_began = tv;
        char tbuf[ISO_TIME_USEC_LEN+1];
        format_iso_time_nospace_usec(tbuf, old_timestamp_began);

        tor_snprintf(event_tail, sizeof(event_tail),
                     " OLD_PURPOSE=%s%s%s OLD_TIME_CREATED=%s",
                     circuit_purpose_to_controller_string(purpose),
                     (hs_state_str != NULL) ? " OLD_HS_STATE=" : "",
                     (hs_state_str != NULL) ? hs_state_str : "",
                     tbuf);
      }

      break;
    default:
      log_warn(LD_BUG, "Unrecognized status code %d", (int)e);
      tor_fragile_assert();
      return 0;
    }

  {
    char *circdesc = circuit_describe_status_for_controller(circ);
    const char *sp = strlen(circdesc) ? " " : "";
    send_control_event(EVENT_CIRCUIT_STATUS_MINOR, ALL_FORMATS,
                       "650 CIRC_MINOR %lu %s%s%s%s\r\n",
                       (unsigned long)circ->global_identifier,
                       event_desc, sp,
                       circdesc,
                       event_tail);
    tor_free(circdesc);
  }

  return 0;
}

/**
 * <b>circ</b> has changed its purpose from <b>old_purpose</b>: tell any
 * interested controllers.
 */
int
control_event_circuit_purpose_changed(origin_circuit_t *circ,
                                      int old_purpose)
{
  return control_event_circuit_status_minor(circ,
                                            CIRC_MINOR_EVENT_PURPOSE_CHANGED,
                                            old_purpose,
                                            NULL);
}

/**
 * <b>circ</b> has changed its purpose from <b>old_purpose</b>, and its
 * created-time from <b>old_tv_created</b>: tell any interested controllers.
 */
int
control_event_circuit_cannibalized(origin_circuit_t *circ,
                                   int old_purpose,
                                   const struct timeval *old_tv_created)
{
  return control_event_circuit_status_minor(circ,
                                            CIRC_MINOR_EVENT_CANNIBALIZED,
                                            old_purpose,
                                            old_tv_created);
}

/** Given an AP connection <b>conn</b> and a <b>len</b>-character buffer
 * <b>buf</b>, determine the address:port combination requested on
 * <b>conn</b>, and write it to <b>buf</b>.  Return 0 on success, -1 on
 * failure. */
static int
write_stream_target_to_buf(entry_connection_t *conn, char *buf, size_t len)
{
  char buf2[256];
  if (conn->chosen_exit_name)
    if (tor_snprintf(buf2, sizeof(buf2), ".%s.exit", conn->chosen_exit_name)<0)
      return -1;
  if (!conn->socks_request)
    return -1;
  if (tor_snprintf(buf, len, "%s%s%s:%d",
               conn->socks_request->address,
               conn->chosen_exit_name ? buf2 : "",
               !conn->chosen_exit_name && connection_edge_is_rendezvous_stream(
                                     ENTRY_TO_EDGE_CONN(conn)) ? ".onion" : "",
               conn->socks_request->port)<0)
    return -1;
  return 0;
}

/** Something has happened to the stream associated with AP connection
 * <b>conn</b>: tell any interested control connections. */
int
control_event_stream_status(entry_connection_t *conn, stream_status_event_t tp,
                            int reason_code)
{
  char reason_buf[64];
  char addrport_buf[64];
  const char *status;
  circuit_t *circ;
  origin_circuit_t *origin_circ = NULL;
  char buf[256];
  const char *purpose = "";
  tor_assert(conn->socks_request);

  if (!EVENT_IS_INTERESTING(EVENT_STREAM_STATUS))
    return 0;

  if (tp == STREAM_EVENT_CLOSED &&
      (reason_code & END_STREAM_REASON_FLAG_ALREADY_SENT_CLOSED))
    return 0;

  write_stream_target_to_buf(conn, buf, sizeof(buf));

  reason_buf[0] = '\0';
  switch (tp)
    {
    case STREAM_EVENT_SENT_CONNECT: status = "SENTCONNECT"; break;
    case STREAM_EVENT_SENT_RESOLVE: status = "SENTRESOLVE"; break;
    case STREAM_EVENT_SUCCEEDED: status = "SUCCEEDED"; break;
    case STREAM_EVENT_FAILED: status = "FAILED"; break;
    case STREAM_EVENT_CLOSED: status = "CLOSED"; break;
    case STREAM_EVENT_NEW: status = "NEW"; break;
    case STREAM_EVENT_NEW_RESOLVE: status = "NEWRESOLVE"; break;
    case STREAM_EVENT_FAILED_RETRIABLE: status = "DETACHED"; break;
    case STREAM_EVENT_REMAP: status = "REMAP"; break;
    default:
      log_warn(LD_BUG, "Unrecognized status code %d", (int)tp);
      return 0;
    }
  if (reason_code && (tp == STREAM_EVENT_FAILED ||
                      tp == STREAM_EVENT_CLOSED ||
                      tp == STREAM_EVENT_FAILED_RETRIABLE)) {
    const char *reason_str = stream_end_reason_to_control_string(reason_code);
    char *r = NULL;
    if (!reason_str) {
      tor_asprintf(&r, " UNKNOWN_%d", reason_code);
      reason_str = r;
    }
    if (reason_code & END_STREAM_REASON_FLAG_REMOTE)
      tor_snprintf(reason_buf, sizeof(reason_buf),
                   " REASON=END REMOTE_REASON=%s", reason_str);
    else
      tor_snprintf(reason_buf, sizeof(reason_buf),
                   " REASON=%s", reason_str);
    tor_free(r);
  } else if (reason_code && tp == STREAM_EVENT_REMAP) {
    switch (reason_code) {
    case REMAP_STREAM_SOURCE_CACHE:
      strlcpy(reason_buf, " SOURCE=CACHE", sizeof(reason_buf));
      break;
    case REMAP_STREAM_SOURCE_EXIT:
      strlcpy(reason_buf, " SOURCE=EXIT", sizeof(reason_buf));
      break;
    default:
      tor_snprintf(reason_buf, sizeof(reason_buf), " REASON=UNKNOWN_%d",
                   reason_code);
      /* XXX do we want SOURCE=UNKNOWN_%d above instead? -RD */
      break;
    }
  }

  if (tp == STREAM_EVENT_NEW || tp == STREAM_EVENT_NEW_RESOLVE) {
    /*
     * When the control conn is an AF_UNIX socket and we have no address,
     * it gets set to "(Tor_internal)"; see dnsserv_launch_request() in
     * dnsserv.c.
     */
    if (strcmp(ENTRY_TO_CONN(conn)->address, "(Tor_internal)") != 0) {
      tor_snprintf(addrport_buf,sizeof(addrport_buf), " SOURCE_ADDR=%s:%d",
                   ENTRY_TO_CONN(conn)->address, ENTRY_TO_CONN(conn)->port);
    } else {
      /*
       * else leave it blank so control on AF_UNIX doesn't need to make
       * something up.
       */
      addrport_buf[0] = '\0';
    }
  } else {
    addrport_buf[0] = '\0';
  }

  if (tp == STREAM_EVENT_NEW_RESOLVE) {
    purpose = " PURPOSE=DNS_REQUEST";
  } else if (tp == STREAM_EVENT_NEW) {
    if (conn->use_begindir) {
      connection_t *linked = ENTRY_TO_CONN(conn)->linked_conn;
      int linked_dir_purpose = -1;
      if (linked && linked->type == CONN_TYPE_DIR)
        linked_dir_purpose = linked->purpose;
      if (DIR_PURPOSE_IS_UPLOAD(linked_dir_purpose))
        purpose = " PURPOSE=DIR_UPLOAD";
      else
        purpose = " PURPOSE=DIR_FETCH";
    } else
      purpose = " PURPOSE=USER";
  }

  circ = circuit_get_by_edge_conn(ENTRY_TO_EDGE_CONN(conn));
  if (circ && CIRCUIT_IS_ORIGIN(circ))
    origin_circ = TO_ORIGIN_CIRCUIT(circ);
  send_control_event(EVENT_STREAM_STATUS, ALL_FORMATS,
                        "650 STREAM "U64_FORMAT" %s %lu %s%s%s%s\r\n",
                     U64_PRINTF_ARG(ENTRY_TO_CONN(conn)->global_identifier),
                     status,
                        origin_circ?
                           (unsigned long)origin_circ->global_identifier : 0ul,
                        buf, reason_buf, addrport_buf, purpose);

  /* XXX need to specify its intended exit, etc? */

  return 0;
}

/** Figure out the best name for the target router of an OR connection
 * <b>conn</b>, and write it into the <b>len</b>-character buffer
 * <b>name</b>. */
static void
orconn_target_get_name(char *name, size_t len, or_connection_t *conn)
{
  const node_t *node = node_get_by_id(conn->identity_digest);
  if (node) {
    tor_assert(len > MAX_VERBOSE_NICKNAME_LEN);
    node_get_verbose_nickname(node, name);
  } else if (! tor_digest_is_zero(conn->identity_digest)) {
    name[0] = '$';
    base16_encode(name+1, len-1, conn->identity_digest,
                  DIGEST_LEN);
  } else {
    tor_snprintf(name, len, "%s:%d",
                 conn->base_.address, conn->base_.port);
  }
}

/** Called when the status of an OR connection <b>conn</b> changes: tell any
 * interested control connections. <b>tp</b> is the new status for the
 * connection.  If <b>conn</b> has just closed or failed, then <b>reason</b>
 * may be the reason why.
 */
int
control_event_or_conn_status(or_connection_t *conn, or_conn_status_event_t tp,
                             int reason)
{
  int ncircs = 0;
  const char *status;
  char name[128];
  char ncircs_buf[32] = {0}; /* > 8 + log10(2^32)=10 + 2 */

  if (!EVENT_IS_INTERESTING(EVENT_OR_CONN_STATUS))
    return 0;

  switch (tp)
    {
    case OR_CONN_EVENT_LAUNCHED: status = "LAUNCHED"; break;
    case OR_CONN_EVENT_CONNECTED: status = "CONNECTED"; break;
    case OR_CONN_EVENT_FAILED: status = "FAILED"; break;
    case OR_CONN_EVENT_CLOSED: status = "CLOSED"; break;
    case OR_CONN_EVENT_NEW: status = "NEW"; break;
    default:
      log_warn(LD_BUG, "Unrecognized status code %d", (int)tp);
      return 0;
    }
  if (conn->chan) {
    ncircs = circuit_count_pending_on_channel(TLS_CHAN_TO_BASE(conn->chan));
  } else {
    ncircs = 0;
  }
  ncircs += connection_or_get_num_circuits(conn);
  if (ncircs && (tp == OR_CONN_EVENT_FAILED || tp == OR_CONN_EVENT_CLOSED)) {
    tor_snprintf(ncircs_buf, sizeof(ncircs_buf), " NCIRCS=%d", ncircs);
  }

  orconn_target_get_name(name, sizeof(name), conn);
  send_control_event(EVENT_OR_CONN_STATUS, ALL_FORMATS,
                              "650 ORCONN %s %s%s%s%s ID="U64_FORMAT"\r\n",
                              name, status,
                              reason ? " REASON=" : "",
                              orconn_end_reason_to_control_string(reason),
                              ncircs_buf,
                              U64_PRINTF_ARG(conn->base_.global_identifier));

  return 0;
}

/**
 * Print out STREAM_BW event for a single conn
 */
int
control_event_stream_bandwidth(edge_connection_t *edge_conn)
{
  circuit_t *circ;
  origin_circuit_t *ocirc;
  if (EVENT_IS_INTERESTING(EVENT_STREAM_BANDWIDTH_USED)) {
    if (!edge_conn->n_read && !edge_conn->n_written)
      return 0;

    send_control_event(EVENT_STREAM_BANDWIDTH_USED, ALL_FORMATS,
                       "650 STREAM_BW "U64_FORMAT" %lu %lu\r\n",
                       U64_PRINTF_ARG(edge_conn->base_.global_identifier),
                       (unsigned long)edge_conn->n_read,
                       (unsigned long)edge_conn->n_written);

    circ = circuit_get_by_edge_conn(edge_conn);
    if (circ && CIRCUIT_IS_ORIGIN(circ)) {
      ocirc = TO_ORIGIN_CIRCUIT(circ);
      ocirc->n_read_circ_bw += edge_conn->n_read;
      ocirc->n_written_circ_bw += edge_conn->n_written;
    }
    edge_conn->n_written = edge_conn->n_read = 0;
  }

  return 0;
}

/** A second or more has elapsed: tell any interested control
 * connections how much bandwidth streams have used. */
int
control_event_stream_bandwidth_used(void)
{
  if (EVENT_IS_INTERESTING(EVENT_STREAM_BANDWIDTH_USED)) {
    smartlist_t *conns = get_connection_array();
    edge_connection_t *edge_conn;

    SMARTLIST_FOREACH_BEGIN(conns, connection_t *, conn)
    {
        if (conn->type != CONN_TYPE_AP)
          continue;
        edge_conn = TO_EDGE_CONN(conn);
        if (!edge_conn->n_read && !edge_conn->n_written)
          continue;

        send_control_event(EVENT_STREAM_BANDWIDTH_USED, ALL_FORMATS,
                           "650 STREAM_BW "U64_FORMAT" %lu %lu\r\n",
                           U64_PRINTF_ARG(edge_conn->base_.global_identifier),
                           (unsigned long)edge_conn->n_read,
                           (unsigned long)edge_conn->n_written);

        edge_conn->n_written = edge_conn->n_read = 0;
    }
    SMARTLIST_FOREACH_END(conn);
  }

  return 0;
}

/** A second or more has elapsed: tell any interested control connections
 * how much bandwidth origin circuits have used. */
int
control_event_circ_bandwidth_used(void)
{
  circuit_t *circ;
  origin_circuit_t *ocirc;
  if (!EVENT_IS_INTERESTING(EVENT_CIRC_BANDWIDTH_USED))
    return 0;

  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    if (!CIRCUIT_IS_ORIGIN(circ))
      continue;
    ocirc = TO_ORIGIN_CIRCUIT(circ);
    if (!ocirc->n_read_circ_bw && !ocirc->n_written_circ_bw)
      continue;
    send_control_event(EVENT_CIRC_BANDWIDTH_USED, ALL_FORMATS,
                       "650 CIRC_BW ID=%d READ=%lu WRITTEN=%lu\r\n",
                       ocirc->global_identifier,
                       (unsigned long)ocirc->n_read_circ_bw,
                       (unsigned long)ocirc->n_written_circ_bw);
    ocirc->n_written_circ_bw = ocirc->n_read_circ_bw = 0;
  }

  return 0;
}

/** Print out CONN_BW event for a single OR/DIR/EXIT <b>conn</b> and reset
  * bandwidth counters. */
int
control_event_conn_bandwidth(connection_t *conn)
{
  const char *conn_type_str;
  if (!get_options()->TestingEnableConnBwEvent ||
      !EVENT_IS_INTERESTING(EVENT_CONN_BW))
    return 0;
  if (!conn->n_read_conn_bw && !conn->n_written_conn_bw)
    return 0;
  switch (conn->type) {
    case CONN_TYPE_OR:
      conn_type_str = "OR";
      break;
    case CONN_TYPE_DIR:
      conn_type_str = "DIR";
      break;
    case CONN_TYPE_EXIT:
      conn_type_str = "EXIT";
      break;
    default:
      return 0;
  }
  send_control_event(EVENT_CONN_BW, ALL_FORMATS,
                     "650 CONN_BW ID="U64_FORMAT" TYPE=%s "
                     "READ=%lu WRITTEN=%lu\r\n",
                     U64_PRINTF_ARG(conn->global_identifier),
                     conn_type_str,
                     (unsigned long)conn->n_read_conn_bw,
                     (unsigned long)conn->n_written_conn_bw);
  conn->n_written_conn_bw = conn->n_read_conn_bw = 0;
  return 0;
}

/** A second or more has elapsed: tell any interested control
 * connections how much bandwidth connections have used. */
int
control_event_conn_bandwidth_used(void)
{
  if (get_options()->TestingEnableConnBwEvent &&
      EVENT_IS_INTERESTING(EVENT_CONN_BW)) {
    SMARTLIST_FOREACH(get_connection_array(), connection_t *, conn,
                      control_event_conn_bandwidth(conn));
  }
  return 0;
}

/** Helper: iterate over cell statistics of <b>circ</b> and sum up added
 * cells, removed cells, and waiting times by cell command and direction.
 * Store results in <b>cell_stats</b>.  Free cell statistics of the
 * circuit afterwards. */
void
sum_up_cell_stats_by_command(circuit_t *circ, cell_stats_t *cell_stats)
{
  memset(cell_stats, 0, sizeof(cell_stats_t));
  SMARTLIST_FOREACH_BEGIN(circ->testing_cell_stats,
                          testing_cell_stats_entry_t *, ent) {
    tor_assert(ent->command <= CELL_COMMAND_MAX_);
    if (!ent->removed && !ent->exitward) {
      cell_stats->added_cells_appward[ent->command] += 1;
    } else if (!ent->removed && ent->exitward) {
      cell_stats->added_cells_exitward[ent->command] += 1;
    } else if (!ent->exitward) {
      cell_stats->removed_cells_appward[ent->command] += 1;
      cell_stats->total_time_appward[ent->command] += ent->waiting_time * 10;
    } else {
      cell_stats->removed_cells_exitward[ent->command] += 1;
      cell_stats->total_time_exitward[ent->command] += ent->waiting_time * 10;
    }
    tor_free(ent);
  } SMARTLIST_FOREACH_END(ent);
  smartlist_free(circ->testing_cell_stats);
  circ->testing_cell_stats = NULL;
}

/** Helper: append a cell statistics string to <code>event_parts</code>,
 * prefixed with <code>key</code>=.  Statistics consist of comma-separated
 * key:value pairs with lower-case command strings as keys and cell
 * numbers or total waiting times as values.  A key:value pair is included
 * if the entry in <code>include_if_non_zero</code> is not zero, but with
 * the (possibly zero) entry from <code>number_to_include</code>.  Both
 * arrays are expected to have a length of CELL_COMMAND_MAX_ + 1.  If no
 * entry in <code>include_if_non_zero</code> is positive, no string will
 * be added to <code>event_parts</code>. */
void
append_cell_stats_by_command(smartlist_t *event_parts, const char *key,
                             const uint64_t *include_if_non_zero,
                             const uint64_t *number_to_include)
{
  smartlist_t *key_value_strings = smartlist_new();
  int i;
  for (i = 0; i <= CELL_COMMAND_MAX_; i++) {
    if (include_if_non_zero[i] > 0) {
      smartlist_add_asprintf(key_value_strings, "%s:"U64_FORMAT,
                             cell_command_to_string(i),
                             U64_PRINTF_ARG(number_to_include[i]));
    }
  }
  if (smartlist_len(key_value_strings) > 0) {
    char *joined = smartlist_join_strings(key_value_strings, ",", 0, NULL);
    smartlist_add_asprintf(event_parts, "%s=%s", key, joined);
    SMARTLIST_FOREACH(key_value_strings, char *, cp, tor_free(cp));
    tor_free(joined);
  }
  smartlist_free(key_value_strings);
}

/** Helper: format <b>cell_stats</b> for <b>circ</b> for inclusion in a
 * CELL_STATS event and write result string to <b>event_string</b>. */
void
format_cell_stats(char **event_string, circuit_t *circ,
                  cell_stats_t *cell_stats)
{
  smartlist_t *event_parts = smartlist_new();
  if (CIRCUIT_IS_ORIGIN(circ)) {
    origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(circ);
    smartlist_add_asprintf(event_parts, "ID=%lu",
                 (unsigned long)ocirc->global_identifier);
  } else if (TO_OR_CIRCUIT(circ)->p_chan) {
    or_circuit_t *or_circ = TO_OR_CIRCUIT(circ);
    smartlist_add_asprintf(event_parts, "InboundQueue=%lu",
                 (unsigned long)or_circ->p_circ_id);
    smartlist_add_asprintf(event_parts, "InboundConn="U64_FORMAT,
                 U64_PRINTF_ARG(or_circ->p_chan->global_identifier));
    append_cell_stats_by_command(event_parts, "InboundAdded",
                                 cell_stats->added_cells_appward,
                                 cell_stats->added_cells_appward);
    append_cell_stats_by_command(event_parts, "InboundRemoved",
                                 cell_stats->removed_cells_appward,
                                 cell_stats->removed_cells_appward);
    append_cell_stats_by_command(event_parts, "InboundTime",
                                 cell_stats->removed_cells_appward,
                                 cell_stats->total_time_appward);
  }
  if (circ->n_chan) {
    smartlist_add_asprintf(event_parts, "OutboundQueue=%lu",
                     (unsigned long)circ->n_circ_id);
    smartlist_add_asprintf(event_parts, "OutboundConn="U64_FORMAT,
                 U64_PRINTF_ARG(circ->n_chan->global_identifier));
    append_cell_stats_by_command(event_parts, "OutboundAdded",
                                 cell_stats->added_cells_exitward,
                                 cell_stats->added_cells_exitward);
    append_cell_stats_by_command(event_parts, "OutboundRemoved",
                                 cell_stats->removed_cells_exitward,
                                 cell_stats->removed_cells_exitward);
    append_cell_stats_by_command(event_parts, "OutboundTime",
                                 cell_stats->removed_cells_exitward,
                                 cell_stats->total_time_exitward);
  }
  *event_string = smartlist_join_strings(event_parts, " ", 0, NULL);
  SMARTLIST_FOREACH(event_parts, char *, cp, tor_free(cp));
  smartlist_free(event_parts);
}

/** A second or more has elapsed: tell any interested control connection
 * how many cells have been processed for a given circuit. */
int
control_event_circuit_cell_stats(void)
{
  circuit_t *circ;
  cell_stats_t *cell_stats;
  char *event_string;
  if (!get_options()->TestingEnableCellStatsEvent ||
      !EVENT_IS_INTERESTING(EVENT_CELL_STATS))
    return 0;
  cell_stats = tor_malloc(sizeof(cell_stats_t));;
  TOR_LIST_FOREACH(circ, circuit_get_global_list(), head) {
    if (!circ->testing_cell_stats)
      continue;
    sum_up_cell_stats_by_command(circ, cell_stats);
    format_cell_stats(&event_string, circ, cell_stats);
    send_control_event(EVENT_CELL_STATS, ALL_FORMATS,
                       "650 CELL_STATS %s\r\n", event_string);
    tor_free(event_string);
  }
  tor_free(cell_stats);
  return 0;
}

/** Tokens in <b>bucket</b> have been refilled: the read bucket was empty
 * for <b>read_empty_time</b> millis, the write bucket was empty for
 * <b>write_empty_time</b> millis, and buckets were last refilled
 * <b>milliseconds_elapsed</b> millis ago.  Only emit TB_EMPTY event if
 * either read or write bucket have been empty before. */
int
control_event_tb_empty(const char *bucket, uint32_t read_empty_time,
                       uint32_t write_empty_time,
                       int milliseconds_elapsed)
{
  if (get_options()->TestingEnableTbEmptyEvent &&
      EVENT_IS_INTERESTING(EVENT_TB_EMPTY) &&
      (read_empty_time > 0 || write_empty_time > 0)) {
    send_control_event(EVENT_TB_EMPTY, ALL_FORMATS,
                       "650 TB_EMPTY %s READ=%d WRITTEN=%d "
                       "LAST=%d\r\n",
                       bucket, read_empty_time, write_empty_time,
                       milliseconds_elapsed);
  }
  return 0;
}

/** A second or more has elapsed: tell any interested control
 * connections how much bandwidth we used. */
int
control_event_bandwidth_used(uint32_t n_read, uint32_t n_written)
{
  if (EVENT_IS_INTERESTING(EVENT_BANDWIDTH_USED)) {
    send_control_event(EVENT_BANDWIDTH_USED, ALL_FORMATS,
                       "650 BW %lu %lu\r\n",
                       (unsigned long)n_read,
                       (unsigned long)n_written);
  }

  return 0;
}

/** Called when we are sending a log message to the controllers: suspend
 * sending further log messages to the controllers until we're done.  Used by
 * CONN_LOG_PROTECT. */
void
disable_control_logging(void)
{
  ++disable_log_messages;
}

/** We're done sending a log message to the controllers: re-enable controller
 * logging.  Used by CONN_LOG_PROTECT. */
void
enable_control_logging(void)
{
  if (--disable_log_messages < 0)
    tor_assert(0);
}

/** We got a log message: tell any interested control connections. */
void
control_event_logmsg(int severity, uint32_t domain, const char *msg)
{
  int event;

  /* Don't even think of trying to add stuff to a buffer from a cpuworker
   * thread. */
  if (! in_main_thread())
    return;

  if (disable_log_messages)
    return;

  if (domain == LD_BUG && EVENT_IS_INTERESTING(EVENT_STATUS_GENERAL) &&
      severity <= LOG_NOTICE) {
    char *esc = esc_for_log(msg);
    ++disable_log_messages;
    control_event_general_status(severity, "BUG REASON=%s", esc);
    --disable_log_messages;
    tor_free(esc);
  }

  event = log_severity_to_event(severity);
  if (event >= 0 && EVENT_IS_INTERESTING(event)) {
    char *b = NULL;
    const char *s;
    if (strchr(msg, '\n')) {
      char *cp;
      b = tor_strdup(msg);
      for (cp = b; *cp; ++cp)
        if (*cp == '\r' || *cp == '\n')
          *cp = ' ';
    }
    switch (severity) {
      case LOG_DEBUG: s = "DEBUG"; break;
      case LOG_INFO: s = "INFO"; break;
      case LOG_NOTICE: s = "NOTICE"; break;
      case LOG_WARN: s = "WARN"; break;
      case LOG_ERR: s = "ERR"; break;
      default: s = "UnknownLogSeverity"; break;
    }
    ++disable_log_messages;
    send_control_event(event, ALL_FORMATS, "650 %s %s\r\n", s, b?b:msg);
    --disable_log_messages;
    tor_free(b);
  }
}

/** Called whenever we receive new router descriptors: tell any
 * interested control connections.  <b>routers</b> is a list of
 * routerinfo_t's.
 */
int
control_event_descriptors_changed(smartlist_t *routers)
{
  char *msg;

  if (!EVENT_IS_INTERESTING(EVENT_NEW_DESC))
    return 0;

  {
    smartlist_t *names = smartlist_new();
    char *ids;
    SMARTLIST_FOREACH(routers, routerinfo_t *, ri, {
        char *b = tor_malloc(MAX_VERBOSE_NICKNAME_LEN+1);
        router_get_verbose_nickname(b, ri);
        smartlist_add(names, b);
      });
    ids = smartlist_join_strings(names, " ", 0, NULL);
    tor_asprintf(&msg, "650 NEWDESC %s\r\n", ids);
    send_control_event_string(EVENT_NEW_DESC, ALL_FORMATS, msg);
    tor_free(ids);
    tor_free(msg);
    SMARTLIST_FOREACH(names, char *, cp, tor_free(cp));
    smartlist_free(names);
  }
  return 0;
}

/** Called when an address mapping on <b>from</b> from changes to <b>to</b>.
 * <b>expires</b> values less than 3 are special; see connection_edge.c.  If
 * <b>error</b> is non-NULL, it is an error code describing the failure
 * mode of the mapping.
 */
int
control_event_address_mapped(const char *from, const char *to, time_t expires,
                             const char *error, const int cached)
{
  if (!EVENT_IS_INTERESTING(EVENT_ADDRMAP))
    return 0;

  if (expires < 3 || expires == TIME_MAX)
    send_control_event(EVENT_ADDRMAP, ALL_FORMATS,
                                "650 ADDRMAP %s %s NEVER %s%s"
                                "CACHED=\"%s\"\r\n",
                                  from, to, error?error:"", error?" ":"",
                                cached?"YES":"NO");
  else {
    char buf[ISO_TIME_LEN+1];
    char buf2[ISO_TIME_LEN+1];
    format_local_iso_time(buf,expires);
    format_iso_time(buf2,expires);
    send_control_event(EVENT_ADDRMAP, ALL_FORMATS,
                                "650 ADDRMAP %s %s \"%s\""
                                " %s%sEXPIRES=\"%s\" CACHED=\"%s\"\r\n",
                                from, to, buf,
                                error?error:"", error?" ":"",
                                buf2, cached?"YES":"NO");
  }

  return 0;
}

/** The authoritative dirserver has received a new descriptor that
 * has passed basic syntax checks and is properly self-signed.
 *
 * Notify any interested party of the new descriptor and what has
 * been done with it, and also optionally give an explanation/reason. */
int
control_event_or_authdir_new_descriptor(const char *action,
                                        const char *desc, size_t desclen,
                                        const char *msg)
{
  char firstline[1024];
  char *buf;
  size_t totallen;
  char *esc = NULL;
  size_t esclen;

  if (!EVENT_IS_INTERESTING(EVENT_AUTHDIR_NEWDESCS))
    return 0;

  tor_snprintf(firstline, sizeof(firstline),
               "650+AUTHDIR_NEWDESC=\r\n%s\r\n%s\r\n",
               action,
               msg ? msg : "");

  /* Escape the server descriptor properly */
  esclen = write_escaped_data(desc, desclen, &esc);

  totallen = strlen(firstline) + esclen + 1;
  buf = tor_malloc(totallen);
  strlcpy(buf, firstline, totallen);
  strlcpy(buf+strlen(firstline), esc, totallen);
  send_control_event_string(EVENT_AUTHDIR_NEWDESCS, ALL_FORMATS,
                            buf);
  send_control_event_string(EVENT_AUTHDIR_NEWDESCS, ALL_FORMATS,
                            "650 OK\r\n");
  tor_free(esc);
  tor_free(buf);

  return 0;
}

/** Helper function for NS-style events. Constructs and sends an event
 * of type <b>event</b> with string <b>event_string</b> out of the set of
 * networkstatuses <b>statuses</b>. Currently it is used for NS events
 * and NEWCONSENSUS events. */
static int
control_event_networkstatus_changed_helper(smartlist_t *statuses,
                                           uint16_t event,
                                           const char *event_string)
{
  smartlist_t *strs;
  char *s, *esc = NULL;
  if (!EVENT_IS_INTERESTING(event) || !smartlist_len(statuses))
    return 0;

  strs = smartlist_new();
  smartlist_add(strs, tor_strdup("650+"));
  smartlist_add(strs, tor_strdup(event_string));
  smartlist_add(strs, tor_strdup("\r\n"));
  SMARTLIST_FOREACH(statuses, const routerstatus_t *, rs,
    {
      s = networkstatus_getinfo_helper_single(rs);
      if (!s) continue;
      smartlist_add(strs, s);
    });

  s = smartlist_join_strings(strs, "", 0, NULL);
  write_escaped_data(s, strlen(s), &esc);
  SMARTLIST_FOREACH(strs, char *, cp, tor_free(cp));
  smartlist_free(strs);
  tor_free(s);
  send_control_event_string(event, ALL_FORMATS, esc);
  send_control_event_string(event, ALL_FORMATS,
                            "650 OK\r\n");

  tor_free(esc);
  return 0;
}

/** Called when the routerstatus_ts <b>statuses</b> have changed: sends
 * an NS event to any controller that cares. */
int
control_event_networkstatus_changed(smartlist_t *statuses)
{
  return control_event_networkstatus_changed_helper(statuses, EVENT_NS, "NS");
}

/** Called when we get a new consensus networkstatus. Sends a NEWCONSENSUS
 * event consisting of an NS-style line for each relay in the consensus. */
int
control_event_newconsensus(const networkstatus_t *consensus)
{
  if (!control_event_is_interesting(EVENT_NEWCONSENSUS))
    return 0;
  return control_event_networkstatus_changed_helper(
           consensus->routerstatus_list, EVENT_NEWCONSENSUS, "NEWCONSENSUS");
}

/** Called when we compute a new circuitbuildtimeout */
int
control_event_buildtimeout_set(buildtimeout_set_event_t type,
                               const char *args)
{
  const char *type_string = NULL;

  if (!control_event_is_interesting(EVENT_BUILDTIMEOUT_SET))
    return 0;

  switch (type) {
    case BUILDTIMEOUT_SET_EVENT_COMPUTED:
      type_string = "COMPUTED";
      break;
    case BUILDTIMEOUT_SET_EVENT_RESET:
      type_string = "RESET";
      break;
    case BUILDTIMEOUT_SET_EVENT_SUSPENDED:
      type_string = "SUSPENDED";
      break;
    case BUILDTIMEOUT_SET_EVENT_DISCARD:
      type_string = "DISCARD";
      break;
    case BUILDTIMEOUT_SET_EVENT_RESUME:
      type_string = "RESUME";
      break;
    default:
      type_string = "UNKNOWN";
      break;
  }

  send_control_event(EVENT_BUILDTIMEOUT_SET, ALL_FORMATS,
                     "650 BUILDTIMEOUT_SET %s %s\r\n",
                     type_string, args);

  return 0;
}

/** Called when a signal has been processed from signal_callback */
int
control_event_signal(uintptr_t signal)
{
  const char *signal_string = NULL;

  if (!control_event_is_interesting(EVENT_SIGNAL))
    return 0;

  switch (signal) {
    case SIGHUP:
      signal_string = "RELOAD";
      break;
    case SIGUSR1:
      signal_string = "DUMP";
      break;
    case SIGUSR2:
      signal_string = "DEBUG";
      break;
    case SIGNEWNYM:
      signal_string = "NEWNYM";
      break;
    case SIGCLEARDNSCACHE:
      signal_string = "CLEARDNSCACHE";
      break;
    default:
      log_warn(LD_BUG, "Unrecognized signal %lu in control_event_signal",
               (unsigned long)signal);
      return -1;
  }

  send_control_event(EVENT_SIGNAL, ALL_FORMATS, "650 SIGNAL %s\r\n",
                     signal_string);
  return 0;
}

/** Called when a single local_routerstatus_t has changed: Sends an NS event
 * to any controller that cares. */
int
control_event_networkstatus_changed_single(const routerstatus_t *rs)
{
  smartlist_t *statuses;
  int r;

  if (!EVENT_IS_INTERESTING(EVENT_NS))
    return 0;

  statuses = smartlist_new();
  smartlist_add(statuses, (void*)rs);
  r = control_event_networkstatus_changed(statuses);
  smartlist_free(statuses);
  return r;
}

/** Our own router descriptor has changed; tell any controllers that care.
 */
int
control_event_my_descriptor_changed(void)
{
  send_control_event(EVENT_DESCCHANGED, ALL_FORMATS, "650 DESCCHANGED\r\n");
  return 0;
}

/** Helper: sends a status event where <b>type</b> is one of
 * EVENT_STATUS_{GENERAL,CLIENT,SERVER}, where <b>severity</b> is one of
 * LOG_{NOTICE,WARN,ERR}, and where <b>format</b> is a printf-style format
 * string corresponding to <b>args</b>. */
static int
control_event_status(int type, int severity, const char *format, va_list args)
{
  char *user_buf = NULL;
  char format_buf[160];
  const char *status, *sev;

  switch (type) {
    case EVENT_STATUS_GENERAL:
      status = "STATUS_GENERAL";
      break;
    case EVENT_STATUS_CLIENT:
      status = "STATUS_CLIENT";
      break;
    case EVENT_STATUS_SERVER:
      status = "STATUS_SERVER";
      break;
    default:
      log_warn(LD_BUG, "Unrecognized status type %d", type);
      return -1;
  }
  switch (severity) {
    case LOG_NOTICE:
      sev = "NOTICE";
      break;
    case LOG_WARN:
      sev = "WARN";
      break;
    case LOG_ERR:
      sev = "ERR";
      break;
    default:
      log_warn(LD_BUG, "Unrecognized status severity %d", severity);
      return -1;
  }
  if (tor_snprintf(format_buf, sizeof(format_buf), "650 %s %s",
                   status, sev)<0) {
    log_warn(LD_BUG, "Format string too long.");
    return -1;
  }
  tor_vasprintf(&user_buf, format, args);

  send_control_event(type, ALL_FORMATS, "%s %s\r\n", format_buf, user_buf);
  tor_free(user_buf);
  return 0;
}

/** Format and send an EVENT_STATUS_GENERAL event whose main text is obtained
 * by formatting the arguments using the printf-style <b>format</b>. */
int
control_event_general_status(int severity, const char *format, ...)
{
  va_list ap;
  int r;
  if (!EVENT_IS_INTERESTING(EVENT_STATUS_GENERAL))
    return 0;

  va_start(ap, format);
  r = control_event_status(EVENT_STATUS_GENERAL, severity, format, ap);
  va_end(ap);
  return r;
}

/** Format and send an EVENT_STATUS_CLIENT event whose main text is obtained
 * by formatting the arguments using the printf-style <b>format</b>. */
int
control_event_client_status(int severity, const char *format, ...)
{
  va_list ap;
  int r;
  if (!EVENT_IS_INTERESTING(EVENT_STATUS_CLIENT))
    return 0;

  va_start(ap, format);
  r = control_event_status(EVENT_STATUS_CLIENT, severity, format, ap);
  va_end(ap);
  return r;
}

/** Format and send an EVENT_STATUS_SERVER event whose main text is obtained
 * by formatting the arguments using the printf-style <b>format</b>. */
int
control_event_server_status(int severity, const char *format, ...)
{
  va_list ap;
  int r;
  if (!EVENT_IS_INTERESTING(EVENT_STATUS_SERVER))
    return 0;

  va_start(ap, format);
  r = control_event_status(EVENT_STATUS_SERVER, severity, format, ap);
  va_end(ap);
  return r;
}

/** Called when the status of an entry guard with the given <b>nickname</b>
 * and identity <b>digest</b> has changed to <b>status</b>: tells any
 * controllers that care. */
int
control_event_guard(const char *nickname, const char *digest,
                    const char *status)
{
  char hbuf[HEX_DIGEST_LEN+1];
  base16_encode(hbuf, sizeof(hbuf), digest, DIGEST_LEN);
  if (!EVENT_IS_INTERESTING(EVENT_GUARD))
    return 0;

  {
    char buf[MAX_VERBOSE_NICKNAME_LEN+1];
    const node_t *node = node_get_by_id(digest);
    if (node) {
      node_get_verbose_nickname(node, buf);
    } else {
      tor_snprintf(buf, sizeof(buf), "$%s~%s", hbuf, nickname);
    }
    send_control_event(EVENT_GUARD, ALL_FORMATS,
                       "650 GUARD ENTRY %s %s\r\n", buf, status);
  }
  return 0;
}

/** Called when a configuration option changes. This is generally triggered
 * by SETCONF requests and RELOAD/SIGHUP signals. The <b>elements</b> is
 * a smartlist_t containing (key, value, ...) pairs in sequence.
 * <b>value</b> can be NULL. */
int
control_event_conf_changed(const smartlist_t *elements)
{
  int i;
  char *result;
  smartlist_t *lines;
  if (!EVENT_IS_INTERESTING(EVENT_CONF_CHANGED) ||
      smartlist_len(elements) == 0) {
    return 0;
  }
  lines = smartlist_new();
  for (i = 0; i < smartlist_len(elements); i += 2) {
    char *k = smartlist_get(elements, i);
    char *v = smartlist_get(elements, i+1);
    if (v == NULL) {
      smartlist_add_asprintf(lines, "650-%s", k);
    } else {
      smartlist_add_asprintf(lines, "650-%s=%s", k, v);
    }
  }
  result = smartlist_join_strings(lines, "\r\n", 0, NULL);
  send_control_event(EVENT_CONF_CHANGED, 0,
    "650-CONF_CHANGED\r\n%s\r\n650 OK\r\n", result);
  tor_free(result);
  SMARTLIST_FOREACH(lines, char *, cp, tor_free(cp));
  smartlist_free(lines);
  return 0;
}

/** Helper: Return a newly allocated string containing a path to the
 * file where we store our authentication cookie. */
char *
get_controller_cookie_file_name(void)
{
  const or_options_t *options = get_options();
  if (options->CookieAuthFile && strlen(options->CookieAuthFile)) {
    return tor_strdup(options->CookieAuthFile);
  } else {
    return get_datadir_fname("control_auth_cookie");
  }
}

/* Initialize the cookie-based authentication system of the
 * ControlPort. If <b>enabled</b> is 0, then disable the cookie
 * authentication system.  */
int
init_control_cookie_authentication(int enabled)
{
  char *fname = NULL;
  int retval;

  if (!enabled) {
    authentication_cookie_is_set = 0;
    return 0;
  }

  fname = get_controller_cookie_file_name();
  retval = init_cookie_authentication(fname, "", /* no header */
                                      AUTHENTICATION_COOKIE_LEN,
                                   get_options()->CookieAuthFileGroupReadable,
                                      &authentication_cookie,
                                      &authentication_cookie_is_set);
  tor_free(fname);
  return retval;
}

/** A copy of the process specifier of Tor's owning controller, or
 * NULL if this Tor instance is not currently owned by a process. */
static char *owning_controller_process_spec = NULL;

/** A process-termination monitor for Tor's owning controller, or NULL
 * if this Tor instance is not currently owned by a process. */
static tor_process_monitor_t *owning_controller_process_monitor = NULL;

static void owning_controller_procmon_cb(void *unused) ATTR_NORETURN;

/** Process-termination monitor callback for Tor's owning controller
 * process. */
static void
owning_controller_procmon_cb(void *unused)
{
  (void)unused;

  lost_owning_controller("process", "vanished");
}

/** Set <b>process_spec</b> as Tor's owning controller process.
 * Exit on failure. */
void
monitor_owning_controller_process(const char *process_spec)
{
  const char *msg;

  tor_assert((owning_controller_process_spec == NULL) ==
             (owning_controller_process_monitor == NULL));

  if (owning_controller_process_spec != NULL) {
    if ((process_spec != NULL) && !strcmp(process_spec,
                                          owning_controller_process_spec)) {
      /* Same process -- return now, instead of disposing of and
       * recreating the process-termination monitor. */
      return;
    }

    /* We are currently owned by a process, and we should no longer be
     * owned by it.  Free the process-termination monitor. */
    tor_process_monitor_free(owning_controller_process_monitor);
    owning_controller_process_monitor = NULL;

    tor_free(owning_controller_process_spec);
    owning_controller_process_spec = NULL;
  }

  tor_assert((owning_controller_process_spec == NULL) &&
             (owning_controller_process_monitor == NULL));

  if (process_spec == NULL)
    return;

  owning_controller_process_spec = tor_strdup(process_spec);
  owning_controller_process_monitor =
    tor_process_monitor_new(tor_libevent_get_base(),
                            owning_controller_process_spec,
                            LD_CONTROL,
                            owning_controller_procmon_cb, NULL,
                            &msg);

  if (owning_controller_process_monitor == NULL) {
    log_err(LD_BUG, "Couldn't create process-termination monitor for "
            "owning controller: %s.  Exiting.",
            msg);
    owning_controller_process_spec = NULL;
    tor_cleanup();
    exit(0);
  }
}

/** Convert the name of a bootstrapping phase <b>s</b> into strings
 * <b>tag</b> and <b>summary</b> suitable for display by the controller. */
static int
bootstrap_status_to_string(bootstrap_status_t s, const char **tag,
                           const char **summary)
{
  switch (s) {
    case BOOTSTRAP_STATUS_UNDEF:
      *tag = "undef";
      *summary = "Undefined";
      break;
    case BOOTSTRAP_STATUS_STARTING:
      *tag = "starting";
      *summary = "Starting";
      break;
    case BOOTSTRAP_STATUS_CONN_DIR:
      *tag = "conn_dir";
      *summary = "Connecting to directory server";
      break;
    case BOOTSTRAP_STATUS_HANDSHAKE:
      *tag = "status_handshake";
      *summary = "Finishing handshake";
      break;
    case BOOTSTRAP_STATUS_HANDSHAKE_DIR:
      *tag = "handshake_dir";
      *summary = "Finishing handshake with directory server";
      break;
    case BOOTSTRAP_STATUS_ONEHOP_CREATE:
      *tag = "onehop_create";
      *summary = "Establishing an encrypted directory connection";
      break;
    case BOOTSTRAP_STATUS_REQUESTING_STATUS:
      *tag = "requesting_status";
      *summary = "Asking for networkstatus consensus";
      break;
    case BOOTSTRAP_STATUS_LOADING_STATUS:
      *tag = "loading_status";
      *summary = "Loading networkstatus consensus";
      break;
    case BOOTSTRAP_STATUS_LOADING_KEYS:
      *tag = "loading_keys";
      *summary = "Loading authority key certs";
      break;
    case BOOTSTRAP_STATUS_REQUESTING_DESCRIPTORS:
      *tag = "requesting_descriptors";
      *summary = "Asking for relay descriptors";
      break;
    case BOOTSTRAP_STATUS_LOADING_DESCRIPTORS:
      *tag = "loading_descriptors";
      *summary = "Loading relay descriptors";
      break;
    case BOOTSTRAP_STATUS_CONN_OR:
      *tag = "conn_or";
      *summary = "Connecting to the Tor network";
      break;
    case BOOTSTRAP_STATUS_HANDSHAKE_OR:
      *tag = "handshake_or";
      *summary = "Finishing handshake with first hop";
      break;
    case BOOTSTRAP_STATUS_CIRCUIT_CREATE:
      *tag = "circuit_create";
      *summary = "Establishing a Tor circuit";
      break;
    case BOOTSTRAP_STATUS_DONE:
      *tag = "done";
      *summary = "Done";
      break;
    default:
//      log_warn(LD_BUG, "Unrecognized bootstrap status code %d", s);
      *tag = *summary = "unknown";
      return -1;
  }
  return 0;
}

/** What percentage through the bootstrap process are we? We remember
 * this so we can avoid sending redundant bootstrap status events, and
 * so we can guess context for the bootstrap messages which are
 * ambiguous. It starts at 'undef', but gets set to 'starting' while
 * Tor initializes. */
static int bootstrap_percent = BOOTSTRAP_STATUS_UNDEF;

/** As bootstrap_percent, but holds the bootstrapping level at which we last
 * logged a NOTICE-level message. We use this, plus BOOTSTRAP_PCT_INCREMENT,
 * to avoid flooding the log with a new message every time we get a few more
 * microdescriptors */
static int notice_bootstrap_percent = 0;

/** How many problems have we had getting to the next bootstrapping phase?
 * These include failure to establish a connection to a Tor relay,
 * failures to finish the TLS handshake, failures to validate the
 * consensus document, etc. */
static int bootstrap_problems = 0;

/** We only tell the controller once we've hit a threshold of problems
 * for the current phase. */
#define BOOTSTRAP_PROBLEM_THRESHOLD 10

/** When our bootstrapping progress level changes, but our bootstrapping
 * status has not advanced, we only log at NOTICE when we have made at least
 * this much progress.
 */
#define BOOTSTRAP_PCT_INCREMENT 5

/** Called when Tor has made progress at bootstrapping its directory
 * information and initial circuits.
 *
 * <b>status</b> is the new status, that is, what task we will be doing
 * next. <b>progress</b> is zero if we just started this task, else it
 * represents progress on the task. */
void
control_event_bootstrap(bootstrap_status_t status, int progress)
{
  const char *tag, *summary;
  char buf[BOOTSTRAP_MSG_LEN];

  if (bootstrap_percent == BOOTSTRAP_STATUS_DONE)
    return; /* already bootstrapped; nothing to be done here. */

  /* special case for handshaking status, since our TLS handshaking code
   * can't distinguish what the connection is going to be for. */
  if (status == BOOTSTRAP_STATUS_HANDSHAKE) {
    if (bootstrap_percent < BOOTSTRAP_STATUS_CONN_OR) {
      status = BOOTSTRAP_STATUS_HANDSHAKE_DIR;
    } else {
      status = BOOTSTRAP_STATUS_HANDSHAKE_OR;
    }
  }

  if (status > bootstrap_percent ||
      (progress && progress > bootstrap_percent)) {
    int loglevel = LOG_NOTICE;
    bootstrap_status_to_string(status, &tag, &summary);

    if (status <= bootstrap_percent &&
        (progress < notice_bootstrap_percent + BOOTSTRAP_PCT_INCREMENT)) {
      /* We log the message at info if the status hasn't advanced, and if less
       * than BOOTSTRAP_PCT_INCREMENT progress has been made.
       */
      loglevel = LOG_INFO;
    }

    tor_log(loglevel, LD_CONTROL,
            "Bootstrapped %d%%: %s", progress ? progress : status, summary);
    tor_snprintf(buf, sizeof(buf),
        "BOOTSTRAP PROGRESS=%d TAG=%s SUMMARY=\"%s\"",
        progress ? progress : status, tag, summary);
    tor_snprintf(last_sent_bootstrap_message,
                 sizeof(last_sent_bootstrap_message),
                 "NOTICE %s", buf);
    control_event_client_status(LOG_NOTICE, "%s", buf);
    if (status > bootstrap_percent) {
      bootstrap_percent = status; /* new milestone reached */
    }
    if (progress > bootstrap_percent) {
      /* incremental progress within a milestone */
      bootstrap_percent = progress;
      bootstrap_problems = 0; /* Progress! Reset our problem counter. */
    }
    if (loglevel == LOG_NOTICE &&
        bootstrap_percent > notice_bootstrap_percent) {
      /* Remember that we gave a notice at this level. */
      notice_bootstrap_percent = bootstrap_percent;
    }
  }
}

/** Called when Tor has failed to make bootstrapping progress in a way
 * that indicates a problem. <b>warn</b> gives a hint as to why, and
 * <b>reason</b> provides an "or_conn_end_reason" tag.  <b>or_conn</b>
 * is the connection that caused this problem.
 */
MOCK_IMPL(void,
          control_event_bootstrap_problem, (const char *warn, int reason,
                                            or_connection_t *or_conn))
{
  int status = bootstrap_percent;
  const char *tag = "", *summary = "";
  char buf[BOOTSTRAP_MSG_LEN];
  const char *recommendation = "ignore";
  int severity;

  /* bootstrap_percent must not be in "undefined" state here. */
  tor_assert(status >= 0);

  if (or_conn->have_noted_bootstrap_problem)
    return;

  or_conn->have_noted_bootstrap_problem = 1;

  if (bootstrap_percent == 100)
    return; /* already bootstrapped; nothing to be done here. */

  bootstrap_problems++;

  if (bootstrap_problems >= BOOTSTRAP_PROBLEM_THRESHOLD)
    recommendation = "warn";

  if (reason == END_OR_CONN_REASON_NO_ROUTE)
    recommendation = "warn";

  /* If we are using bridges and all our OR connections are now
     closed, it means that we totally failed to connect to our
     bridges. Throw a warning. */
  if (get_options()->UseBridges && !any_other_active_or_conns(or_conn))
    recommendation = "warn";

  if (we_are_hibernating())
    recommendation = "ignore";

  while (status>=0 && bootstrap_status_to_string(status, &tag, &summary) < 0)
    status--; /* find a recognized status string based on current progress */
  status = bootstrap_percent; /* set status back to the actual number */

  severity = !strcmp(recommendation, "warn") ? LOG_WARN : LOG_INFO;

  log_fn(severity,
         LD_CONTROL, "Problem bootstrapping. Stuck at %d%%: %s. (%s; %s; "
         "count %d; recommendation %s)",
         status, summary, warn,
         orconn_end_reason_to_control_string(reason),
         bootstrap_problems, recommendation);

  connection_or_report_broken_states(severity, LD_HANDSHAKE);

  tor_snprintf(buf, sizeof(buf),
      "BOOTSTRAP PROGRESS=%d TAG=%s SUMMARY=\"%s\" WARNING=\"%s\" REASON=%s "
      "COUNT=%d RECOMMENDATION=%s",
      bootstrap_percent, tag, summary, warn,
      orconn_end_reason_to_control_string(reason), bootstrap_problems,
      recommendation);
  tor_snprintf(last_sent_bootstrap_message,
               sizeof(last_sent_bootstrap_message),
               "WARN %s", buf);
  control_event_client_status(LOG_WARN, "%s", buf);
}

/** We just generated a new summary of which countries we've seen clients
 * from recently. Send a copy to the controller in case it wants to
 * display it for the user. */
void
control_event_clients_seen(const char *controller_str)
{
  send_control_event(EVENT_CLIENTS_SEEN, 0,
    "650 CLIENTS_SEEN %s\r\n", controller_str);
}

/** A new pluggable transport called <b>transport_name</b> was
 *  launched on <b>addr</b>:<b>port</b>. <b>mode</b> is either
 *  "server" or "client" depending on the mode of the pluggable
 *  transport.
 *  "650" SP "TRANSPORT_LAUNCHED" SP Mode SP Name SP Address SP Port
 */
void
control_event_transport_launched(const char *mode, const char *transport_name,
                                 tor_addr_t *addr, uint16_t port)
{
  send_control_event(EVENT_TRANSPORT_LAUNCHED, ALL_FORMATS,
                     "650 TRANSPORT_LAUNCHED %s %s %s %u\r\n",
                     mode, transport_name, fmt_addr(addr), port);
}

/** Convert rendezvous auth type to string for HS_DESC control events
 */
const char *
rend_auth_type_to_string(rend_auth_type_t auth_type)
{
  const char *str;

  switch (auth_type) {
    case REND_NO_AUTH:
      str = "NO_AUTH";
      break;
    case REND_BASIC_AUTH:
      str = "BASIC_AUTH";
      break;
    case REND_STEALTH_AUTH:
      str = "STEALTH_AUTH";
      break;
    default:
      str = "UNKNOWN";
  }

  return str;
}

/** Return a longname the node whose identity is <b>id_digest</b>. If
 * node_get_by_id() returns NULL, base 16 encoding of <b>id_digest</b> is
 * returned instead.
 *
 * This function is not thread-safe.  Each call to this function invalidates
 * previous values returned by this function.
 */
MOCK_IMPL(const char *,
node_describe_longname_by_id,(const char *id_digest))
{
  static char longname[MAX_VERBOSE_NICKNAME_LEN+1];
  node_get_verbose_nickname_by_id(id_digest, longname);
  return longname;
}

/** send HS_DESC requested event.
 *
 * <b>rend_query</b> is used to fetch requested onion address and auth type.
 * <b>hs_dir</b> is the description of contacting hs directory.
 * <b>desc_id_base32</b> is the ID of requested hs descriptor.
 */
void
control_event_hs_descriptor_requested(const rend_data_t *rend_query,
                                      const char *id_digest,
                                      const char *desc_id_base32)
{
  if (!id_digest || !rend_query || !desc_id_base32) {
    log_warn(LD_BUG, "Called with rend_query==%p, "
             "id_digest==%p, desc_id_base32==%p",
             rend_query, id_digest, desc_id_base32);
    return;
  }

  send_control_event(EVENT_HS_DESC, ALL_FORMATS,
                     "650 HS_DESC REQUESTED %s %s %s %s\r\n",
                     rend_query->onion_address,
                     rend_auth_type_to_string(rend_query->auth_type),
                     node_describe_longname_by_id(id_digest),
                     desc_id_base32);
}

/** send HS_DESC event after got response from hs directory.
 *
 * NOTE: this is an internal function used by following functions:
 * control_event_hs_descriptor_received
 * control_event_hs_descriptor_failed
 *
 * So do not call this function directly.
 */
void
control_event_hs_descriptor_receive_end(const char *action,
                                        const rend_data_t *rend_query,
                                        const char *id_digest)
{
  if (!action || !rend_query || !id_digest) {
    log_warn(LD_BUG, "Called with action==%p, rend_query==%p, "
             "id_digest==%p", action, rend_query, id_digest);
    return;
  }

  send_control_event(EVENT_HS_DESC, ALL_FORMATS,
                     "650 HS_DESC %s %s %s %s\r\n",
                     action,
                     rend_query->onion_address,
                     rend_auth_type_to_string(rend_query->auth_type),
                     node_describe_longname_by_id(id_digest));
}

/** send HS_DESC RECEIVED event
 *
 * called when a we successfully received a hidden service descriptor.
 */
void
control_event_hs_descriptor_received(const rend_data_t *rend_query,
                                     const char *id_digest)
{
  if (!rend_query || !id_digest) {
    log_warn(LD_BUG, "Called with rend_query==%p, id_digest==%p",
             rend_query, id_digest);
    return;
  }
  control_event_hs_descriptor_receive_end("RECEIVED", rend_query, id_digest);
}

/** send HS_DESC FAILED event
 *
 * called when request for hidden service descriptor returned failure.
 */
void
control_event_hs_descriptor_failed(const rend_data_t *rend_query,
                                   const char *id_digest)
{
  if (!rend_query || !id_digest) {
    log_warn(LD_BUG, "Called with rend_query==%p, id_digest==%p",
             rend_query, id_digest);
    return;
  }
  control_event_hs_descriptor_receive_end("FAILED", rend_query, id_digest);
}

/** Free any leftover allocated memory of the control.c subsystem. */
void
control_free_all(void)
{
  if (authentication_cookie) /* Free the auth cookie */
    tor_free(authentication_cookie);
}

#ifdef TOR_UNIT_TESTS
/* For testing: change the value of global_event_mask */
void
control_testing_set_global_event_mask(uint64_t mask)
{
  global_event_mask = mask;
}
#endif

