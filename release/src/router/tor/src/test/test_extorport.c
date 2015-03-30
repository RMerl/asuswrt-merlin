/* Copyright (c) 2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define CONNECTION_PRIVATE
#define EXT_ORPORT_PRIVATE
#define MAIN_PRIVATE
#include "or.h"
#include "buffers.h"
#include "connection.h"
#include "connection_or.h"
#include "config.h"
#include "control.h"
#include "ext_orport.h"
#include "main.h"
#include "test.h"

/* Test connection_or_remove_from_ext_or_id_map and
 * connection_or_set_ext_or_identifier */
static void
test_ext_or_id_map(void *arg)
{
  or_connection_t *c1 = NULL, *c2 = NULL, *c3 = NULL;
  char *idp = NULL, *idp2 = NULL;
  (void)arg;

  /* pre-initialization */
  tt_ptr_op(NULL, ==, connection_or_get_by_ext_or_id("xxxxxxxxxxxxxxxxxxxx"));

  c1 = or_connection_new(CONN_TYPE_EXT_OR, AF_INET);
  c2 = or_connection_new(CONN_TYPE_EXT_OR, AF_INET);
  c3 = or_connection_new(CONN_TYPE_OR, AF_INET);

  tt_ptr_op(c1->ext_or_conn_id, !=, NULL);
  tt_ptr_op(c2->ext_or_conn_id, !=, NULL);
  tt_ptr_op(c3->ext_or_conn_id, ==, NULL);

  tt_ptr_op(c1, ==, connection_or_get_by_ext_or_id(c1->ext_or_conn_id));
  tt_ptr_op(c2, ==, connection_or_get_by_ext_or_id(c2->ext_or_conn_id));
  tt_ptr_op(NULL, ==, connection_or_get_by_ext_or_id("xxxxxxxxxxxxxxxxxxxx"));

  idp = tor_memdup(c2->ext_or_conn_id, EXT_OR_CONN_ID_LEN);

  /* Give c2 a new ID. */
  connection_or_set_ext_or_identifier(c2);
  test_mem_op(idp, !=, c2->ext_or_conn_id, EXT_OR_CONN_ID_LEN);
  idp2 = tor_memdup(c2->ext_or_conn_id, EXT_OR_CONN_ID_LEN);
  tt_assert(!tor_digest_is_zero(idp2));

  tt_ptr_op(NULL, ==, connection_or_get_by_ext_or_id(idp));
  tt_ptr_op(c2, ==, connection_or_get_by_ext_or_id(idp2));

  /* Now remove it. */
  connection_or_remove_from_ext_or_id_map(c2);
  tt_ptr_op(NULL, ==, connection_or_get_by_ext_or_id(idp));
  tt_ptr_op(NULL, ==, connection_or_get_by_ext_or_id(idp2));

 done:
  if (c1)
    connection_free_(TO_CONN(c1));
  if (c2)
    connection_free_(TO_CONN(c2));
  if (c3)
    connection_free_(TO_CONN(c3));
  tor_free(idp);
  tor_free(idp2);
  connection_or_clear_ext_or_id_map();
}

/* Simple connection_write_to_buf_impl_ replacement that unconditionally
 * writes to outbuf. */
static void
connection_write_to_buf_impl_replacement(const char *string, size_t len,
                                         connection_t *conn, int zlib)
{
  (void) zlib;

  tor_assert(string);
  tor_assert(conn);
  write_to_buf(string, len, conn->outbuf);
}

static char *
buf_get_contents(buf_t *buf, size_t *sz_out)
{
  char *out;
  *sz_out = buf_datalen(buf);
  if (*sz_out >= ULONG_MAX)
    return NULL; /* C'mon, really? */
  out = tor_malloc(*sz_out + 1);
  if (fetch_from_buf(out, (unsigned long)*sz_out, buf) != 0) {
    tor_free(out);
    return NULL;
  }
  out[*sz_out] = '\0'; /* Hopefully gratuitous. */
  return out;
}

static void
test_ext_or_write_command(void *arg)
{
  or_connection_t *c1;
  char *cp = NULL;
  char *buf = NULL;
  size_t sz;

  (void) arg;
  MOCK(connection_write_to_buf_impl_,
       connection_write_to_buf_impl_replacement);

  c1 = or_connection_new(CONN_TYPE_EXT_OR, AF_INET);
  tt_assert(c1);

  /* Length too long */
  tt_int_op(connection_write_ext_or_command(TO_CONN(c1), 100, "X", 100000),
            <, 0);

  /* Empty command */
  tt_int_op(connection_write_ext_or_command(TO_CONN(c1), 0x99, NULL, 0),
            ==, 0);
  cp = buf_get_contents(TO_CONN(c1)->outbuf, &sz);
  tt_int_op(sz, ==, 4);
  test_mem_op(cp, ==, "\x00\x99\x00\x00", 4);
  tor_free(cp);

  /* Medium command. */
  tt_int_op(connection_write_ext_or_command(TO_CONN(c1), 0x99,
                                            "Wai\0Hello", 9), ==, 0);
  cp = buf_get_contents(TO_CONN(c1)->outbuf, &sz);
  tt_int_op(sz, ==, 13);
  test_mem_op(cp, ==, "\x00\x99\x00\x09Wai\x00Hello", 13);
  tor_free(cp);

  /* Long command */
  buf = tor_malloc(65535);
  memset(buf, 'x', 65535);
  tt_int_op(connection_write_ext_or_command(TO_CONN(c1), 0xf00d,
                                            buf, 65535), ==, 0);
  cp = buf_get_contents(TO_CONN(c1)->outbuf, &sz);
  tt_int_op(sz, ==, 65539);
  test_mem_op(cp, ==, "\xf0\x0d\xff\xff", 4);
  test_mem_op(cp+4, ==, buf, 65535);
  tor_free(cp);

 done:
  if (c1)
    connection_free_(TO_CONN(c1));
  tor_free(cp);
  tor_free(buf);
  UNMOCK(connection_write_to_buf_impl_);
}

static int
write_bytes_to_file_fail(const char *fname, const char *str, size_t len,
                         int bin)
{
  (void) fname;
  (void) str;
  (void) len;
  (void) bin;

  return -1;
}

static void
test_ext_or_init_auth(void *arg)
{
  or_options_t *options = get_options_mutable();
  const char *fn;
  char *cp = NULL;
  struct stat st;
  char cookie0[32];
  (void)arg;

  /* Check default filename location */
  tor_free(options->DataDirectory);
  options->DataDirectory = tor_strdup("foo");
  cp = get_ext_or_auth_cookie_file_name();
  tt_str_op(cp, ==, "foo"PATH_SEPARATOR"extended_orport_auth_cookie");
  tor_free(cp);

  /* Shouldn't be initialized already, or our tests will be a bit
   * meaningless */
  ext_or_auth_cookie = tor_malloc_zero(32);
  test_assert(tor_mem_is_zero((char*)ext_or_auth_cookie, 32));

  /* Now make sure we use a temporary file */
  fn = get_fname("ext_cookie_file");
  options->ExtORPortCookieAuthFile = tor_strdup(fn);
  cp = get_ext_or_auth_cookie_file_name();
  tt_str_op(cp, ==, fn);
  tor_free(cp);

  /* Test the initialization function with a broken
     write_bytes_to_file(). See if the problem is handled properly. */
  MOCK(write_bytes_to_file, write_bytes_to_file_fail);
  tt_int_op(-1, ==, init_ext_or_cookie_authentication(1));
  tt_int_op(ext_or_auth_cookie_is_set, ==, 0);
  UNMOCK(write_bytes_to_file);

  /* Now do the actual initialization. */
  tt_int_op(0, ==, init_ext_or_cookie_authentication(1));
  tt_int_op(ext_or_auth_cookie_is_set, ==, 1);
  cp = read_file_to_str(fn, RFTS_BIN, &st);
  tt_ptr_op(cp, !=, NULL);
  tt_u64_op((uint64_t)st.st_size, ==, 64);
  test_memeq(cp, "! Extended ORPort Auth Cookie !\x0a", 32);
  test_memeq(cp+32, ext_or_auth_cookie, 32);
  memcpy(cookie0, ext_or_auth_cookie, 32);
  test_assert(!tor_mem_is_zero((char*)ext_or_auth_cookie, 32));

  /* Operation should be idempotent. */
  tt_int_op(0, ==, init_ext_or_cookie_authentication(1));
  test_memeq(cookie0, ext_or_auth_cookie, 32);

 done:
  tor_free(cp);
  ext_orport_free_all();
}

static void
test_ext_or_cookie_auth(void *arg)
{
  char *reply=NULL, *reply2=NULL, *client_hash=NULL, *client_hash2=NULL;
  size_t reply_len=0;
  char hmac1[32], hmac2[32];

  const char client_nonce[32] =
    "Who is the third who walks alway";
  char server_hash_input[] =
    "ExtORPort authentication server-to-client hash"
    "Who is the third who walks alway"
    "................................";
  char client_hash_input[] =
    "ExtORPort authentication client-to-server hash"
    "Who is the third who walks alway"
    "................................";

  (void)arg;

  tt_int_op(strlen(client_hash_input), ==, 46+32+32);
  tt_int_op(strlen(server_hash_input), ==, 46+32+32);

  ext_or_auth_cookie = tor_malloc_zero(32);
  memcpy(ext_or_auth_cookie, "s beside you? When I count, ther", 32);
  ext_or_auth_cookie_is_set = 1;

  /* For this authentication, the client sends 32 random bytes (ClientNonce)
   * The server replies with 32 byte ServerHash and 32 byte ServerNonce,
   * where ServerHash is:
   * HMAC-SHA256(CookieString,
   *   "ExtORPort authentication server-to-client hash" | ClientNonce |
   *    ServerNonce)"
   * The client must reply with 32-byte ClientHash, which we compute as:
   *   ClientHash is computed as:
   *        HMAC-SHA256(CookieString,
   *           "ExtORPort authentication client-to-server hash" | ClientNonce |
   *            ServerNonce)
   */

  /* Wrong length */
  tt_int_op(-1, ==,
            handle_client_auth_nonce(client_nonce, 33, &client_hash, &reply,
                                     &reply_len));
  tt_int_op(-1, ==,
            handle_client_auth_nonce(client_nonce, 31, &client_hash, &reply,
                                     &reply_len));

  /* Now let's try this for real! */
  tt_int_op(0, ==,
            handle_client_auth_nonce(client_nonce, 32, &client_hash, &reply,
                                     &reply_len));
  tt_int_op(reply_len, ==, 64);
  tt_ptr_op(reply, !=, NULL);
  tt_ptr_op(client_hash, !=, NULL);
  /* Fill in the server nonce into the hash inputs... */
  memcpy(server_hash_input+46+32, reply+32, 32);
  memcpy(client_hash_input+46+32, reply+32, 32);
  /* Check the HMACs are correct... */
  crypto_hmac_sha256(hmac1, (char*)ext_or_auth_cookie, 32, server_hash_input,
                     46+32+32);
  crypto_hmac_sha256(hmac2, (char*)ext_or_auth_cookie, 32, client_hash_input,
                     46+32+32);
  test_memeq(hmac1, reply, 32);
  test_memeq(hmac2, client_hash, 32);

  /* Now do it again and make sure that the results are *different* */
  tt_int_op(0, ==,
            handle_client_auth_nonce(client_nonce, 32, &client_hash2, &reply2,
                                     &reply_len));
  test_memneq(reply2, reply, reply_len);
  test_memneq(client_hash2, client_hash, 32);
  /* But that this one checks out too. */
  memcpy(server_hash_input+46+32, reply2+32, 32);
  memcpy(client_hash_input+46+32, reply2+32, 32);
  /* Check the HMACs are correct... */
  crypto_hmac_sha256(hmac1, (char*)ext_or_auth_cookie, 32, server_hash_input,
                     46+32+32);
  crypto_hmac_sha256(hmac2, (char*)ext_or_auth_cookie, 32, client_hash_input,
                     46+32+32);
  test_memeq(hmac1, reply2, 32);
  test_memeq(hmac2, client_hash2, 32);

 done:
  tor_free(reply);
  tor_free(client_hash);
  tor_free(reply2);
  tor_free(client_hash2);
}

static int
crypto_rand_return_tse_str(char *to, size_t n)
{
  if (n != 32) {
    TT_FAIL(("Asked for %d bytes, not 32", (int)n));
    return -1;
  }
  memcpy(to, "te road There is always another ", 32);
  return 0;
}

static void
test_ext_or_cookie_auth_testvec(void *arg)
{
  char *reply=NULL, *client_hash=NULL;
  size_t reply_len;
  char *mem_op_hex_tmp=NULL;

  const char client_nonce[] = "But when I look ahead up the whi";
  (void)arg;

  ext_or_auth_cookie = tor_malloc_zero(32);
  memcpy(ext_or_auth_cookie, "Gliding wrapt in a brown mantle," , 32);
  ext_or_auth_cookie_is_set = 1;

  MOCK(crypto_rand, crypto_rand_return_tse_str);

  tt_int_op(0, ==,
            handle_client_auth_nonce(client_nonce, 32, &client_hash, &reply,
                                     &reply_len));
  tt_ptr_op(reply, !=, NULL );
  tt_uint_op(reply_len, ==, 64);
  test_memeq(reply+32, "te road There is always another ", 32);
  /* HMACSHA256("Gliding wrapt in a brown mantle,"
   *     "ExtORPort authentication server-to-client hash"
   *     "But when I look ahead up the write road There is always another ");
   */
  test_memeq_hex(reply,
                 "ec80ed6e546d3b36fdfc22fe1315416b"
                 "029f1ade7610d910878b62eeb7403821");
  /* HMACSHA256("Gliding wrapt in a brown mantle,"
   *     "ExtORPort authentication client-to-server hash"
   *     "But when I look ahead up the write road There is always another ");
   * (Both values computed using Python CLI.)
   */
  test_memeq_hex(client_hash,
                 "ab391732dd2ed968cd40c087d1b1f25b"
                 "33b3cd77ff79bd80c2074bbf438119a2");

 done:
  UNMOCK(crypto_rand);
  tor_free(reply);
  tor_free(client_hash);
  tor_free(mem_op_hex_tmp);
}

static void
ignore_bootstrap_problem(const char *warn, int reason,
                         or_connection_t *conn)
{
  (void)warn;
  (void)reason;
  (void)conn;
}

static int is_reading = 1;
static int handshake_start_called = 0;

static void
note_read_stopped(connection_t *conn)
{
  (void)conn;
  is_reading=0;
}
static void
note_read_started(connection_t *conn)
{
  (void)conn;
  is_reading=1;
}
static int
handshake_start(or_connection_t *conn, int receiving)
{
  if (!conn || !receiving)
    TT_FAIL(("Bad arguments to handshake_start"));
  handshake_start_called = 1;
  return 0;
}

#define WRITE(s,n)                                                      \
  do {                                                                  \
    write_to_buf((s), (n), TO_CONN(conn)->inbuf);                       \
  } while (0)
#define CONTAINS(s,n)                                           \
  do {                                                          \
    tt_int_op((n), <=, sizeof(b));                              \
    tt_int_op(buf_datalen(TO_CONN(conn)->outbuf), ==, (n));     \
    if ((n)) {                                                  \
      fetch_from_buf(b, (n), TO_CONN(conn)->outbuf);            \
      test_memeq(b, (s), (n));                                  \
    }                                                           \
  } while (0)

/* Helper: Do a successful Extended ORPort authentication handshake. */
static void
do_ext_or_handshake(or_connection_t *conn)
{
  char b[256];

  tt_int_op(0, ==, connection_ext_or_start_auth(conn));
  CONTAINS("\x01\x00", 2);
  WRITE("\x01", 1);
  WRITE("But when I look ahead up the whi", 32);
  MOCK(crypto_rand, crypto_rand_return_tse_str);
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  UNMOCK(crypto_rand);
  tt_int_op(TO_CONN(conn)->state, ==, EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_HASH);
  CONTAINS("\xec\x80\xed\x6e\x54\x6d\x3b\x36\xfd\xfc\x22\xfe\x13\x15\x41\x6b"
           "\x02\x9f\x1a\xde\x76\x10\xd9\x10\x87\x8b\x62\xee\xb7\x40\x38\x21"
           "te road There is always another ", 64);
  /* Send the right response this time. */
  WRITE("\xab\x39\x17\x32\xdd\x2e\xd9\x68\xcd\x40\xc0\x87\xd1\xb1\xf2\x5b"
        "\x33\xb3\xcd\x77\xff\x79\xbd\x80\xc2\x07\x4b\xbf\x43\x81\x19\xa2",
        32);
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  CONTAINS("\x01", 1);
  tt_assert(! TO_CONN(conn)->marked_for_close);
  tt_int_op(TO_CONN(conn)->state, ==, EXT_OR_CONN_STATE_OPEN);

 done: ;
}

static void
test_ext_or_handshake(void *arg)
{
  or_connection_t *conn=NULL;
  char b[256];

  (void) arg;
  MOCK(connection_write_to_buf_impl_,
       connection_write_to_buf_impl_replacement);
  /* Use same authenticators as for test_ext_or_cookie_auth_testvec */
  ext_or_auth_cookie = tor_malloc_zero(32);
  memcpy(ext_or_auth_cookie, "Gliding wrapt in a brown mantle," , 32);
  ext_or_auth_cookie_is_set = 1;

  init_connection_lists();

  conn = or_connection_new(CONN_TYPE_EXT_OR, AF_INET);
  tt_int_op(0, ==, connection_ext_or_start_auth(conn));
  /* The server starts by telling us about the one supported authtype. */
  CONTAINS("\x01\x00", 2);
  /* Say the client hasn't responded yet. */
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  /* Let's say the client replies badly. */
  WRITE("\x99", 1);
  tt_int_op(-1, ==, connection_ext_or_process_inbuf(conn));
  CONTAINS("", 0);
  tt_assert(TO_CONN(conn)->marked_for_close);
  close_closeable_connections();
  conn = NULL;

  /* Okay, try again. */
  conn = or_connection_new(CONN_TYPE_EXT_OR, AF_INET);
  tt_int_op(0, ==, connection_ext_or_start_auth(conn));
  CONTAINS("\x01\x00", 2);
  /* Let's say the client replies sensibly this time. "Yes, AUTHTYPE_COOKIE
   * sounds delicious. Let's have some of that!" */
  WRITE("\x01", 1);
  /* Let's say that the client also sends part of a nonce. */
  WRITE("But when I look ", 16);
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  CONTAINS("", 0);
  tt_int_op(TO_CONN(conn)->state, ==,
            EXT_OR_CONN_STATE_AUTH_WAIT_CLIENT_NONCE);
  /* Pump it again. Nothing should happen. */
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  /* send the rest of the nonce. */
  WRITE("ahead up the whi", 16);
  MOCK(crypto_rand, crypto_rand_return_tse_str);
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  UNMOCK(crypto_rand);
  /* We should get the right reply from the server. */
  CONTAINS("\xec\x80\xed\x6e\x54\x6d\x3b\x36\xfd\xfc\x22\xfe\x13\x15\x41\x6b"
           "\x02\x9f\x1a\xde\x76\x10\xd9\x10\x87\x8b\x62\xee\xb7\x40\x38\x21"
           "te road There is always another ", 64);
  /* Send the wrong response. */
  WRITE("not with a bang but a whimper...", 32);
  MOCK(control_event_bootstrap_problem, ignore_bootstrap_problem);
  tt_int_op(-1, ==, connection_ext_or_process_inbuf(conn));
  CONTAINS("\x00", 1);
  tt_assert(TO_CONN(conn)->marked_for_close);
  /* XXXX Hold-open-until-flushed. */
  close_closeable_connections();
  conn = NULL;
  UNMOCK(control_event_bootstrap_problem);

  MOCK(connection_start_reading, note_read_started);
  MOCK(connection_stop_reading, note_read_stopped);
  MOCK(connection_tls_start_handshake, handshake_start);

  /* Okay, this time let's succeed. */
  conn = or_connection_new(CONN_TYPE_EXT_OR, AF_INET);
  do_ext_or_handshake(conn);

  /* Now let's run through some messages. */
  /* First let's send some junk and make sure it's ignored. */
  WRITE("\xff\xf0\x00\x03""ABC", 7);
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  CONTAINS("", 0);
  /* Now let's send a USERADDR command. */
  WRITE("\x00\x01\x00\x0c""1.2.3.4:5678", 16);
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  tt_int_op(TO_CONN(conn)->port, ==, 5678);
  tt_int_op(tor_addr_to_ipv4h(&TO_CONN(conn)->addr), ==, 0x01020304);
  /* Now let's send a TRANSPORT command. */
  WRITE("\x00\x02\x00\x07""rfc1149", 11);
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  tt_ptr_op(NULL, !=, conn->ext_or_transport);
  tt_str_op("rfc1149", ==, conn->ext_or_transport);
  tt_int_op(is_reading,==,1);
  tt_int_op(TO_CONN(conn)->state, ==, EXT_OR_CONN_STATE_OPEN);
  /* DONE */
  WRITE("\x00\x00\x00\x00", 4);
  tt_int_op(0, ==, connection_ext_or_process_inbuf(conn));
  tt_int_op(TO_CONN(conn)->state, ==, EXT_OR_CONN_STATE_FLUSHING);
  tt_int_op(is_reading,==,0);
  CONTAINS("\x10\x00\x00\x00", 4);
  tt_int_op(handshake_start_called,==,0);
  tt_int_op(0, ==, connection_ext_or_finished_flushing(conn));
  tt_int_op(is_reading,==,1);
  tt_int_op(handshake_start_called,==,1);
  tt_int_op(TO_CONN(conn)->type, ==, CONN_TYPE_OR);
  tt_int_op(TO_CONN(conn)->state, ==, 0);
  close_closeable_connections();
  conn = NULL;

  /* Okay, this time let's succeed the handshake but fail the USERADDR
     command. */
  conn = or_connection_new(CONN_TYPE_EXT_OR, AF_INET);
  do_ext_or_handshake(conn);
  /* USERADDR command with an extra NUL byte */
  WRITE("\x00\x01\x00\x0d""1.2.3.4:5678\x00", 17);
  MOCK(control_event_bootstrap_problem, ignore_bootstrap_problem);
  tt_int_op(-1, ==, connection_ext_or_process_inbuf(conn));
  CONTAINS("", 0);
  tt_assert(TO_CONN(conn)->marked_for_close);
  close_closeable_connections();
  conn = NULL;
  UNMOCK(control_event_bootstrap_problem);

  /* Now fail the TRANSPORT command. */
  conn = or_connection_new(CONN_TYPE_EXT_OR, AF_INET);
  do_ext_or_handshake(conn);
  /* TRANSPORT command with an extra NUL byte */
  WRITE("\x00\x02\x00\x08""rfc1149\x00", 12);
  MOCK(control_event_bootstrap_problem, ignore_bootstrap_problem);
  tt_int_op(-1, ==, connection_ext_or_process_inbuf(conn));
  CONTAINS("", 0);
  tt_assert(TO_CONN(conn)->marked_for_close);
  close_closeable_connections();
  conn = NULL;
  UNMOCK(control_event_bootstrap_problem);

  /* Now fail the TRANSPORT command. */
  conn = or_connection_new(CONN_TYPE_EXT_OR, AF_INET);
  do_ext_or_handshake(conn);
  /* TRANSPORT command with transport name with symbols (not a
     C-identifier) */
  WRITE("\x00\x02\x00\x07""rf*1149", 11);
  MOCK(control_event_bootstrap_problem, ignore_bootstrap_problem);
  tt_int_op(-1, ==, connection_ext_or_process_inbuf(conn));
  CONTAINS("", 0);
  tt_assert(TO_CONN(conn)->marked_for_close);
  close_closeable_connections();
  conn = NULL;
  UNMOCK(control_event_bootstrap_problem);

 done:
  UNMOCK(connection_write_to_buf_impl_);
  UNMOCK(crypto_rand);
  if (conn)
    connection_free_(TO_CONN(conn));
#undef CONTAINS
#undef WRITE
}

struct testcase_t extorport_tests[] = {
  { "id_map", test_ext_or_id_map, TT_FORK, NULL, NULL },
  { "write_command", test_ext_or_write_command, TT_FORK, NULL, NULL },
  { "init_auth", test_ext_or_init_auth, TT_FORK, NULL, NULL },
  { "cookie_auth", test_ext_or_cookie_auth, TT_FORK, NULL, NULL },
  { "cookie_auth_testvec", test_ext_or_cookie_auth_testvec, TT_FORK,
    NULL, NULL },
  { "handshake", test_ext_or_handshake, TT_FORK, NULL, NULL },
  END_OF_TESTCASES
};

