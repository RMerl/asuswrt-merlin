/* Copyright (c) 2012-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file ext_orport.c
 * \brief Code implementing the Extended ORPort.
*/

#define EXT_ORPORT_PRIVATE
#include "or.h"
#include "connection.h"
#include "connection_or.h"
#include "ext_orport.h"
#include "control.h"
#include "config.h"
#include "util.h"
#include "main.h"

/** Allocate and return a structure capable of holding an Extended
 *  ORPort message of body length <b>len</b>. */
ext_or_cmd_t *
ext_or_cmd_new(uint16_t len)
{
  size_t size = STRUCT_OFFSET(ext_or_cmd_t, body) + len;
  ext_or_cmd_t *cmd = tor_malloc(size);
  cmd->len = len;
  return cmd;
}

/** Deallocate the Extended ORPort message in <b>cmd</b>. */
void
ext_or_cmd_free(ext_or_cmd_t *cmd)
{
  tor_free(cmd);
}

/** Get an Extended ORPort message from <b>conn</b>, and place it in
 *  <b>out</b>. Return -1 on fail, 0 if we need more data, and 1 if we
 *  successfully extracted an Extended ORPort command from the
 *  buffer.  */
static int
connection_fetch_ext_or_cmd_from_buf(connection_t *conn, ext_or_cmd_t **out)
{
  IF_HAS_BUFFEREVENT(conn, {
    struct evbuffer *input = bufferevent_get_input(conn->bufev);
    return fetch_ext_or_command_from_evbuffer(input, out);
  }) ELSE_IF_NO_BUFFEREVENT {
    return fetch_ext_or_command_from_buf(conn->inbuf, out);
  }
}

/** Write an Extended ORPort message to <b>conn</b>. Use
 *  <b>command</b> as the command type, <b>bodylen</b> as the body
 *  length, and <b>body</b>, if it's present, as the body of the
 *  message. */
STATIC int
connection_write_ext_or_command(connection_t *conn,
                                uint16_t command,
                                const char *body,
                                size_t bodylen)
{
  char header[4];
  if (bodylen > UINT16_MAX)
    return -1;
  set_uint16(header, htons(command));
  set_uint16(header+2, htons(bodylen));
  connection_write_to_buf(header, 4, conn);
  if (bodylen) {
    tor_assert(body);
    connection_write_to_buf(body, bodylen, conn);
  }
  return 0;
}

/** Transition from an Extended ORPort which accepts Extended ORPort
 *  messages, to an Extended ORport which accepts OR traffic. */
static void
connection_ext_or_transition(or_connection_t *conn)
{
  tor_assert(conn->base_.type == CONN_TYPE_EXT_OR);

  conn->base_.type = CONN_TYPE_OR;
  TO_CONN(conn)->state = 0; // set the state to a neutral value
  control_event_or_conn_status(conn, OR_CONN_EVENT_NEW, 0);
  connection_tls_start_handshake(conn, 1);
}

/** Length of authentication cookie. */
#define EXT_OR_PORT_AUTH_COOKIE_LEN 32
/** Length of the header of the cookie file. */
#define EXT_OR_PORT_AUTH_COOKIE_HEADER_LEN 32
/** Static cookie file header. */
#define EXT_OR_PORT_AUTH_COOKIE_HEADER "! Extended ORPort Auth Cookie !\x0a"
/** Length of safe-cookie protocol hashes. */
#define EXT_OR_PORT_AUTH_HASH_LEN DIGEST256_LEN
/** Length of safe-cookie protocol nonces. */
#define EXT_OR_PORT_AUTH_NONCE_LEN 32
/** Safe-cookie protocol constants. */
#define EXT_OR_PORT_AUTH_SERVER_TO_CLIENT_CONST \
  "ExtORPort authentication server-to-client hash"
#define EXT_OR_PORT_AUTH_CLIENT_TO_SERVER_CONST \
  "ExtORPort authentication client-to-server hash"

/* Code to indicate cookie authentication */
#define EXT_OR_AUTHTYPE_SAFECOOKIE 0x01

/** If true, we've set ext_or_auth_cookie to a secret code and stored
 * it to disk. */
STATIC int ext_or_auth_cookie_is_set = 0;
/** If ext_or_auth_cookie_is_set, a secret cookie that we've stored to disk
 * and which we're using to authenticate controllers.  (If the controller can
 * read it off disk, it has permission to connect.) */
STATIC uint8_t *ext_or_auth_cookie = NULL;

/** Helper: Return a newly allocated string containing a path to the
 * file where we store our authentication cookie. */
char *
get_ext_or_auth_cookie_file_name(void)
{
  const or_options_t *options = get_options();
  if (options->ExtORPortCookieAuthFile &&
      strlen(options->ExtORPortCookieAuthFile)) {
    return tor_strdup(options->ExtORPortCookieAuthFile);
  } else {
    return get_datadir_fname("extended_orport_auth_cookie");
  }
}

/* Initialize the cookie-based authentication system of the
 * Extended ORPort. If <b>is_enabled</b> is 0, then disable the cookie
 * authentication system. */
int
init_ext_or_cookie_authentication(int is_enabled)
{
  char *fname = NULL;
  int retval;

  if (!is_enabled) {
    ext_or_auth_cookie_is_set = 0;
    return 0;
  }

  fname = get_ext_or_auth_cookie_file_name();
  retval = init_cookie_authentication(fname, EXT_OR_PORT_AUTH_COOKIE_HEADER,
                                      EXT_OR_PORT_AUTH_COOKIE_HEADER_LEN,
                           get_options()->ExtORPortCookieAuthFileGroupReadable,
                                      &ext_or_auth_cookie,
                                      &ext_or_auth_cookie_is_set);
  tor_free(fname);
  return retval;
}

/** Read data from <b>conn</b> and see if the client sent us the
 *  authentication type that she prefers to use in this session.
 *
 *  Return -1 if we received corrupted data or if we don't support the
 *  authentication type. Return 0 if we need more data in
 *  <b>conn</b>. Return 1 if the authentication type negotiation was
 *  successful. */
static int
connection_ext_or_auth_neg_auth_type(connection_t *conn)
{
  char authtype[1] = {0};

  if (connection_get_inbuf_len(conn) < 1)
    return 0;

  if (connection_fetch_from_buf(authtype, 1, conn) < 0)
    return -1;

  log_debug(LD_GENERAL, "Client wants us to use %d auth type", authtype[0]);
  if (authtype[0] != EXT_OR_AUTHTYPE_SAFECOOKIE) {
    /* '1' is the only auth type supported atm */
    return -1;
  }

  conn->state = EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_NONCE;
  return 1;
}

/** DOCDOC */
STATIC int
handle_client_auth_nonce(const char *client_nonce, size_t client_nonce_len,
                         char **client_hash_out,
                         char **reply_out, size_t *reply_len_out)
{
  char server_hash[EXT_OR_PORT_AUTH_HASH_LEN] = {0};
  char server_nonce[EXT_OR_PORT_AUTH_NONCE_LEN] = {0};
  char *reply;
  size_t reply_len;

  if (client_nonce_len != EXT_OR_PORT_AUTH_NONCE_LEN)
    return -1;

  /* Get our nonce */
  if (crypto_rand(server_nonce, EXT_OR_PORT_AUTH_NONCE_LEN) < 0)
    return -1;

  { /* set up macs */
    size_t hmac_s_msg_len = strlen(EXT_OR_PORT_AUTH_SERVER_TO_CLIENT_CONST) +
      2*EXT_OR_PORT_AUTH_NONCE_LEN;
    size_t hmac_c_msg_len = strlen(EXT_OR_PORT_AUTH_CLIENT_TO_SERVER_CONST) +
      2*EXT_OR_PORT_AUTH_NONCE_LEN;

    char *hmac_s_msg = tor_malloc_zero(hmac_s_msg_len);
    char *hmac_c_msg = tor_malloc_zero(hmac_c_msg_len);
    char *correct_client_hash = tor_malloc_zero(EXT_OR_PORT_AUTH_HASH_LEN);

    memcpy(hmac_s_msg,
           EXT_OR_PORT_AUTH_SERVER_TO_CLIENT_CONST,
           strlen(EXT_OR_PORT_AUTH_SERVER_TO_CLIENT_CONST));
    memcpy(hmac_s_msg + strlen(EXT_OR_PORT_AUTH_SERVER_TO_CLIENT_CONST),
           client_nonce, EXT_OR_PORT_AUTH_NONCE_LEN);
    memcpy(hmac_s_msg + strlen(EXT_OR_PORT_AUTH_SERVER_TO_CLIENT_CONST) +
           EXT_OR_PORT_AUTH_NONCE_LEN,
           server_nonce, EXT_OR_PORT_AUTH_NONCE_LEN);

    memcpy(hmac_c_msg,
           EXT_OR_PORT_AUTH_CLIENT_TO_SERVER_CONST,
           strlen(EXT_OR_PORT_AUTH_CLIENT_TO_SERVER_CONST));
    memcpy(hmac_c_msg + strlen(EXT_OR_PORT_AUTH_CLIENT_TO_SERVER_CONST),
           client_nonce, EXT_OR_PORT_AUTH_NONCE_LEN);
    memcpy(hmac_c_msg + strlen(EXT_OR_PORT_AUTH_CLIENT_TO_SERVER_CONST) +
           EXT_OR_PORT_AUTH_NONCE_LEN,
           server_nonce, EXT_OR_PORT_AUTH_NONCE_LEN);

    crypto_hmac_sha256(server_hash,
                       (char*)ext_or_auth_cookie,
                       EXT_OR_PORT_AUTH_COOKIE_LEN,
                       hmac_s_msg,
                       hmac_s_msg_len);

    crypto_hmac_sha256(correct_client_hash,
                       (char*)ext_or_auth_cookie,
                       EXT_OR_PORT_AUTH_COOKIE_LEN,
                       hmac_c_msg,
                       hmac_c_msg_len);

    /* Store the client hash we generated. We will need to compare it
       with the hash sent by the client. */
    *client_hash_out = correct_client_hash;

    memwipe(hmac_s_msg, 0, hmac_s_msg_len);
    memwipe(hmac_c_msg, 0, hmac_c_msg_len);

    tor_free(hmac_s_msg);
    tor_free(hmac_c_msg);
  }

  { /* debug logging */ /* XXX disable this codepath if not logging on debug?*/
    char server_hash_encoded[(2*EXT_OR_PORT_AUTH_HASH_LEN) + 1];
    char server_nonce_encoded[(2*EXT_OR_PORT_AUTH_NONCE_LEN) + 1];
    char client_nonce_encoded[(2*EXT_OR_PORT_AUTH_NONCE_LEN) + 1];

    base16_encode(server_hash_encoded, sizeof(server_hash_encoded),
                  server_hash, sizeof(server_hash));
    base16_encode(server_nonce_encoded, sizeof(server_nonce_encoded),
                  server_nonce, sizeof(server_nonce));
    base16_encode(client_nonce_encoded, sizeof(client_nonce_encoded),
                  client_nonce, EXT_OR_PORT_AUTH_NONCE_LEN);

    log_debug(LD_GENERAL,
              "server_hash: '%s'\nserver_nonce: '%s'\nclient_nonce: '%s'",
              server_hash_encoded, server_nonce_encoded, client_nonce_encoded);

    memwipe(server_hash_encoded, 0, sizeof(server_hash_encoded));
    memwipe(server_nonce_encoded, 0, sizeof(server_nonce_encoded));
    memwipe(client_nonce_encoded, 0, sizeof(client_nonce_encoded));
  }

  { /* write reply: (server_hash, server_nonce) */

    reply_len = EXT_OR_PORT_AUTH_COOKIE_LEN+EXT_OR_PORT_AUTH_NONCE_LEN;
    reply = tor_malloc_zero(reply_len);
    memcpy(reply, server_hash, EXT_OR_PORT_AUTH_HASH_LEN);
    memcpy(reply + EXT_OR_PORT_AUTH_HASH_LEN, server_nonce,
           EXT_OR_PORT_AUTH_NONCE_LEN);
  }

  *reply_out = reply;
  *reply_len_out = reply_len;

  return 0;
}

/** Read the client's nonce out of <b>conn</b>, setup the safe-cookie
 *  crypto, and then send our own hash and nonce to the client
 *
 *  Return -1 if there was an error; return 0 if we need more data in
 *  <b>conn</b>, and return 1 if we successfully retrieved the
 *  client's nonce and sent our own. */
static int
connection_ext_or_auth_handle_client_nonce(connection_t *conn)
{
  char client_nonce[EXT_OR_PORT_AUTH_NONCE_LEN];
  char *reply=NULL;
  size_t reply_len=0;

  if (!ext_or_auth_cookie_is_set) { /* this should not happen */
    log_warn(LD_BUG, "Extended ORPort authentication cookie was not set. "
             "That's weird since we should have done that on startup. "
             "This might be a Tor bug, please file a bug report. ");
    return -1;
  }

  if (connection_get_inbuf_len(conn) < EXT_OR_PORT_AUTH_NONCE_LEN)
    return 0;

  if (connection_fetch_from_buf(client_nonce,
                                EXT_OR_PORT_AUTH_NONCE_LEN, conn) < 0)
    return -1;

  /* We extract the ClientNonce from the received data, and use it to
     calculate ServerHash and ServerNonce according to proposal 217.

     We also calculate our own ClientHash value and save it in the
     connection state. We validate it later against the ClientHash
     sent by the client.  */
  if (handle_client_auth_nonce(client_nonce, sizeof(client_nonce),
                            &TO_OR_CONN(conn)->ext_or_auth_correct_client_hash,
                            &reply, &reply_len) < 0)
    return -1;

  connection_write_to_buf(reply, reply_len, conn);

  memwipe(reply, 0, reply_len);
  tor_free(reply);

  log_debug(LD_GENERAL, "Got client nonce, and sent our own nonce and hash.");

  conn->state = EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_HASH;
  return 1;
}

#define connection_ext_or_auth_send_result_success(c)  \
  connection_ext_or_auth_send_result(c, 1)
#define connection_ext_or_auth_send_result_fail(c)  \
  connection_ext_or_auth_send_result(c, 0)

/** Send authentication results to <b>conn</b>. Successful results if
 *  <b>success</b> is set; failure results otherwise. */
static void
connection_ext_or_auth_send_result(connection_t *conn, int success)
{
  if (success)
    connection_write_to_buf("\x01", 1, conn);
  else
    connection_write_to_buf("\x00", 1, conn);
}

/** Receive the client's hash from <b>conn</b>, validate that it's
 *  correct, and then send the authentication results to the client.
 *
 *  Return -1 if there was an error during validation; return 0 if we
 *  need more data in <b>conn</b>, and return 1 if we successfully
 *  validated the client's hash and sent a happy authentication
 *  result. */
static int
connection_ext_or_auth_handle_client_hash(connection_t *conn)
{
  char provided_client_hash[EXT_OR_PORT_AUTH_HASH_LEN] = {0};

  if (connection_get_inbuf_len(conn) < EXT_OR_PORT_AUTH_HASH_LEN)
    return 0;

  if (connection_fetch_from_buf(provided_client_hash,
                                EXT_OR_PORT_AUTH_HASH_LEN, conn) < 0)
    return -1;

  if (tor_memneq(TO_OR_CONN(conn)->ext_or_auth_correct_client_hash,
                 provided_client_hash, EXT_OR_PORT_AUTH_HASH_LEN)) {
    log_warn(LD_GENERAL, "Incorrect client hash. Authentication failed.");
    connection_ext_or_auth_send_result_fail(conn);
    return -1;
  }

  log_debug(LD_GENERAL, "Got client's hash and it was legit.");

  /* send positive auth result */
  connection_ext_or_auth_send_result_success(conn);
  conn->state = EXT_OR_CONN_STATE_OPEN;
  return 1;
}

/** Handle data from <b>or_conn</b> received on Extended ORPort.
 *  Return -1 on error. 0 on unsufficient data. 1 on correct. */
static int
connection_ext_or_auth_process_inbuf(or_connection_t *or_conn)
{
  connection_t *conn = TO_CONN(or_conn);

  /* State transitions of the Extended ORPort authentication protocol:

     EXT_OR_CONN_STATE_AUTH_WAIT_AUTH_TYPE (start state) ->
     EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_NONCE ->
     EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_HASH ->
     EXT_OR_CONN_STATE_OPEN

     During EXT_OR_CONN_STATE_OPEN, data is handled by
     connection_ext_or_process_inbuf().
  */

  switch (conn->state) { /* Functionify */
  case EXT_OR_CONN_STATE_AUTH_WAIT_AUTH_TYPE:
    return connection_ext_or_auth_neg_auth_type(conn);

  case EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_NONCE:
    return connection_ext_or_auth_handle_client_nonce(conn);

  case EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_HASH:
    return connection_ext_or_auth_handle_client_hash(conn);

  default:
    log_warn(LD_BUG, "Encountered unexpected connection state %d while trying "
             "to process Extended ORPort authentication data.", conn->state);
    return -1;
  }
}

/** Extended ORPort commands (Transport-to-Bridge) */
#define EXT_OR_CMD_TB_DONE 0x0000
#define EXT_OR_CMD_TB_USERADDR 0x0001
#define EXT_OR_CMD_TB_TRANSPORT 0x0002

/** Extended ORPort commands (Bridge-to-Transport) */
#define EXT_OR_CMD_BT_OKAY 0x1000
#define EXT_OR_CMD_BT_DENY 0x1001
#define EXT_OR_CMD_BT_CONTROL 0x1002

/** Process a USERADDR command from the Extended
 *  ORPort. <b>payload</b> is a payload of size <b>len</b>.
 *
 *  If the USERADDR command was well formed, change the address of
 *  <b>conn</b> to the address on the USERADDR command.
 *
 *  Return 0 on success and -1 on error. */
static int
connection_ext_or_handle_cmd_useraddr(connection_t *conn,
                                      const char *payload, uint16_t len)
{
  /* Copy address string. */
  tor_addr_t addr;
  uint16_t port;
  char *addr_str;
  char *address_part=NULL;
  int res;
  if (memchr(payload, '\0', len)) {
    log_fn(LOG_PROTOCOL_WARN, LD_NET, "Unexpected NUL in ExtORPort UserAddr");
    return -1;
  }

  addr_str = tor_memdup_nulterm(payload, len);

  res = tor_addr_port_split(LOG_INFO, addr_str, &address_part, &port);
  tor_free(addr_str);
  if (res<0)
    return -1;

  res = tor_addr_parse(&addr, address_part);
  tor_free(address_part);
  if (res<0)
    return -1;

  { /* do some logging */
    char *old_address = tor_dup_addr(&conn->addr);
    char *new_address = tor_dup_addr(&addr);

    log_debug(LD_NET, "Received USERADDR."
             "We rewrite our address from '%s:%u' to '%s:%u'.",
             safe_str(old_address), conn->port, safe_str(new_address), port);

    tor_free(old_address);
    tor_free(new_address);
  }

  /* record the address */
  tor_addr_copy(&conn->addr, &addr);
  conn->port = port;
  if (conn->address) {
    tor_free(conn->address);
  }
  conn->address = tor_dup_addr(&addr);

  return 0;
}

/** Process a TRANSPORT command from the Extended
 *  ORPort. <b>payload</b> is a payload of size <b>len</b>.
 *
 *  If the TRANSPORT command was well formed, register the name of the
 *  transport on <b>conn</b>.
 *
 *  Return 0 on success and -1 on error. */
static int
connection_ext_or_handle_cmd_transport(or_connection_t *conn,
                                       const char *payload, uint16_t len)
{
  char *transport_str;
  if (memchr(payload, '\0', len)) {
    log_fn(LOG_PROTOCOL_WARN, LD_NET, "Unexpected NUL in ExtORPort Transport");
    return -1;
  }

  transport_str = tor_memdup_nulterm(payload, len);

  /* Transport names MUST be C-identifiers. */
  if (!string_is_C_identifier(transport_str)) {
    tor_free(transport_str);
    return -1;
  }

  /* If ext_or_transport is already occupied (because the PT sent two
   *  TRANSPORT commands), deallocate the old name and keep the new
   *  one */
  if (conn->ext_or_transport)
    tor_free(conn->ext_or_transport);

  conn->ext_or_transport = transport_str;
  return 0;
}

#define EXT_OR_CONN_STATE_IS_AUTHENTICATING(st) \
  ((st) <= EXT_OR_CONN_STATE_AUTH_MAX)

/** Process Extended ORPort messages from <b>or_conn</b>. */
int
connection_ext_or_process_inbuf(or_connection_t *or_conn)
{
  connection_t *conn = TO_CONN(or_conn);
  ext_or_cmd_t *command;
  int r;

  /* DOCDOC Document the state machine and transitions in this function */

  /* If we are still in the authentication stage, process traffic as
     authentication data: */
  while (EXT_OR_CONN_STATE_IS_AUTHENTICATING(conn->state)) {
    log_debug(LD_GENERAL, "Got Extended ORPort authentication data (%u).",
              (unsigned int) connection_get_inbuf_len(conn));
    r = connection_ext_or_auth_process_inbuf(or_conn);
    if (r < 0) {
      connection_mark_for_close(conn);
      return -1;
    } else if (r == 0) {
      return 0;
    }
    /* if r > 0, loop and process more data (if any). */
  }

  while (1) {
    log_debug(LD_GENERAL, "Got Extended ORPort data.");
    command = NULL;
    r = connection_fetch_ext_or_cmd_from_buf(conn, &command);
    if (r < 0)
      goto err;
    else if (r == 0)
      return 0; /* need to wait for more data */

    /* Got a command! */
    tor_assert(command);

    if (command->cmd == EXT_OR_CMD_TB_DONE) {
      if (connection_get_inbuf_len(conn)) {
        /* The inbuf isn't empty; the client is misbehaving. */
        goto err;
      }

      log_debug(LD_NET, "Received DONE.");

      /* If the transport proxy did not use the TRANSPORT command to
       * specify the transport name, mark this as unknown transport. */
      if (!or_conn->ext_or_transport) {
        /* We write this string this way to avoid ??>, which is a C
         * trigraph. */
        or_conn->ext_or_transport = tor_strdup("<?" "?>");
      }

      connection_write_ext_or_command(conn, EXT_OR_CMD_BT_OKAY, NULL, 0);

      /* can't transition immediately; need to flush first. */
      conn->state = EXT_OR_CONN_STATE_FLUSHING;
      connection_stop_reading(conn);
    } else if (command->cmd == EXT_OR_CMD_TB_USERADDR) {
      if (connection_ext_or_handle_cmd_useraddr(conn,
                                            command->body, command->len) < 0)
        goto err;
    } else if (command->cmd == EXT_OR_CMD_TB_TRANSPORT) {
      if (connection_ext_or_handle_cmd_transport(or_conn,
                                             command->body, command->len) < 0)
        goto err;
    } else {
      log_notice(LD_NET,"Got Extended ORPort command we don't regognize (%u).",
                 command->cmd);
    }

    ext_or_cmd_free(command);
  }

  return 0;

 err:
  ext_or_cmd_free(command);
  connection_mark_for_close(conn);
  return -1;
}

/** <b>conn</b> finished flushing Extended ORPort messages to the
 *  network, and is now ready to accept OR traffic. This function
 *  does the transition. */
int
connection_ext_or_finished_flushing(or_connection_t *conn)
{
  if (conn->base_.state == EXT_OR_CONN_STATE_FLUSHING) {
    connection_start_reading(TO_CONN(conn));
    connection_ext_or_transition(conn);
  }
  return 0;
}

/** Initiate Extended ORPort authentication, by sending the list of
 *  supported authentication types to the client. */
int
connection_ext_or_start_auth(or_connection_t *or_conn)
{
  connection_t *conn = TO_CONN(or_conn);
  const uint8_t authtypes[] = {
    /* We only support authtype '1' for now. */
    EXT_OR_AUTHTYPE_SAFECOOKIE,
    /* Marks the end of the list. */
    0
  };

  log_debug(LD_GENERAL,
           "ExtORPort authentication: Sending supported authentication types");

  connection_write_to_buf((const char *)authtypes, sizeof(authtypes), conn);
  conn->state = EXT_OR_CONN_STATE_AUTH_WAIT_AUTH_TYPE;

  return 0;
}

/** Free any leftover allocated memory of the ext_orport.c subsystem. */
void
ext_orport_free_all(void)
{
  if (ext_or_auth_cookie) /* Free the auth cookie */
    tor_free(ext_or_auth_cookie);
}

