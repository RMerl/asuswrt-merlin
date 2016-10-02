/* Copyright (c) 2003, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_TORTLS_H
#define TOR_TORTLS_H

/**
 * \file tortls.h
 * \brief Headers for tortls.c
 **/

#include "crypto.h"
#include "compat_openssl.h"
#include "compat.h"
#include "testsupport.h"

/* Opaque structure to hold a TLS connection. */
typedef struct tor_tls_t tor_tls_t;

/* Opaque structure to hold an X509 certificate. */
typedef struct tor_x509_cert_t tor_x509_cert_t;

/* Possible return values for most tor_tls_* functions. */
#define MIN_TOR_TLS_ERROR_VAL_     -9
#define TOR_TLS_ERROR_MISC         -9
/* Rename to unexpected close or something. XXXX */
#define TOR_TLS_ERROR_IO           -8
#define TOR_TLS_ERROR_CONNREFUSED  -7
#define TOR_TLS_ERROR_CONNRESET    -6
#define TOR_TLS_ERROR_NO_ROUTE     -5
#define TOR_TLS_ERROR_TIMEOUT      -4
#define TOR_TLS_CLOSE              -3
#define TOR_TLS_WANTREAD           -2
#define TOR_TLS_WANTWRITE          -1
#define TOR_TLS_DONE                0

/** Collection of case statements for all TLS errors that are not due to
 * underlying IO failure. */
#define CASE_TOR_TLS_ERROR_ANY_NONIO            \
  case TOR_TLS_ERROR_MISC:                      \
  case TOR_TLS_ERROR_CONNREFUSED:               \
  case TOR_TLS_ERROR_CONNRESET:                 \
  case TOR_TLS_ERROR_NO_ROUTE:                  \
  case TOR_TLS_ERROR_TIMEOUT

/** Use this macro in a switch statement to catch _any_ TLS error.  That way,
 * if more errors are added, your switches will still work. */
#define CASE_TOR_TLS_ERROR_ANY                  \
  CASE_TOR_TLS_ERROR_ANY_NONIO:                 \
  case TOR_TLS_ERROR_IO

#define TOR_TLS_IS_ERROR(rv) ((rv) < TOR_TLS_CLOSE)

#ifdef TORTLS_PRIVATE
#define TOR_TLS_MAGIC 0x71571571

typedef enum {
    TOR_TLS_ST_HANDSHAKE, TOR_TLS_ST_OPEN, TOR_TLS_ST_GOTCLOSE,
    TOR_TLS_ST_SENTCLOSE, TOR_TLS_ST_CLOSED, TOR_TLS_ST_RENEGOTIATE,
    TOR_TLS_ST_BUFFEREVENT
} tor_tls_state_t;
#define tor_tls_state_bitfield_t ENUM_BF(tor_tls_state_t)

/** Holds a SSL_CTX object and related state used to configure TLS
 * connections.
 */
typedef struct tor_tls_context_t {
  int refcnt;
  SSL_CTX *ctx;
  tor_x509_cert_t *my_link_cert;
  tor_x509_cert_t *my_id_cert;
  tor_x509_cert_t *my_auth_cert;
  crypto_pk_t *link_key;
  crypto_pk_t *auth_key;
} tor_tls_context_t;

/** Structure that we use for a single certificate. */
struct tor_x509_cert_t {
  X509 *cert;
  uint8_t *encoded;
  size_t encoded_len;
  unsigned pkey_digests_set : 1;
  common_digests_t cert_digests;
  common_digests_t pkey_digests;
};

/** Holds a SSL object and its associated data.  Members are only
 * accessed from within tortls.c.
 */
struct tor_tls_t {
  uint32_t magic;
  tor_tls_context_t *context; /** A link to the context object for this tls. */
  SSL *ssl; /**< An OpenSSL SSL object. */
  int socket; /**< The underlying file descriptor for this TLS connection. */
  char *address; /**< An address to log when describing this connection. */
  tor_tls_state_bitfield_t state : 3; /**< The current SSL state,
                                       * depending on which operations
                                       * have completed successfully. */
  unsigned int isServer:1; /**< True iff this is a server-side connection */
  unsigned int wasV2Handshake:1; /**< True iff the original handshake for
                                  * this connection used the updated version
                                  * of the connection protocol (client sends
                                  * different cipher list, server sends only
                                  * one certificate). */
  /** True iff we should call negotiated_callback when we're done reading. */
  unsigned int got_renegotiate:1;
  /** Return value from tor_tls_classify_client_ciphers, or 0 if we haven't
   * called that function yet. */
  int8_t client_cipher_list_type;
  /** Incremented every time we start the server side of a handshake. */
  uint8_t server_handshake_count;
  size_t wantwrite_n; /**< 0 normally, >0 if we returned wantwrite last
                       * time. */
  /** Last values retrieved from BIO_number_read()/write(); see
   * tor_tls_get_n_raw_bytes() for usage.
   */
  unsigned long last_write_count;
  unsigned long last_read_count;
  /** If set, a callback to invoke whenever the client tries to renegotiate
   * the handshake. */
  void (*negotiated_callback)(tor_tls_t *tls, void *arg);
  /** Argument to pass to negotiated_callback. */
  void *callback_arg;
};

STATIC int tor_errno_to_tls_error(int e);
STATIC int tor_tls_get_error(tor_tls_t *tls, int r, int extra,
                  const char *doing, int severity, int domain);
STATIC tor_tls_t *tor_tls_get_by_ssl(const SSL *ssl);
STATIC void tor_tls_allocate_tor_tls_object_ex_data_index(void);
STATIC int always_accept_verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx);
STATIC int tor_tls_classify_client_ciphers(const SSL *ssl,
                                           STACK_OF(SSL_CIPHER) *peer_ciphers);
STATIC int tor_tls_client_is_using_v2_ciphers(const SSL *ssl);
MOCK_DECL(STATIC void, try_to_extract_certs_from_tls,
          (int severity, tor_tls_t *tls, X509 **cert_out, X509 **id_cert_out));
#ifndef HAVE_SSL_SESSION_GET_MASTER_KEY
STATIC size_t SSL_SESSION_get_master_key(SSL_SESSION *s, uint8_t *out,
                                         size_t len);
#endif
STATIC void tor_tls_debug_state_callback(const SSL *ssl, int type, int val);
STATIC void tor_tls_server_info_callback(const SSL *ssl, int type, int val);
STATIC int tor_tls_session_secret_cb(SSL *ssl, void *secret,
                            int *secret_len,
                            STACK_OF(SSL_CIPHER) *peer_ciphers,
                            CONST_IF_OPENSSL_1_1_API SSL_CIPHER **cipher,
                            void *arg);
STATIC int find_cipher_by_id(const SSL *ssl, const SSL_METHOD *m,
                             uint16_t cipher);
MOCK_DECL(STATIC X509*, tor_tls_create_certificate,(crypto_pk_t *rsa,
                                                    crypto_pk_t *rsa_sign,
                                                    const char *cname,
                                                    const char *cname_sign,
                                                  unsigned int cert_lifetime));
STATIC tor_tls_context_t *tor_tls_context_new(crypto_pk_t *identity,
                   unsigned int key_lifetime, unsigned flags, int is_client);
MOCK_DECL(STATIC tor_x509_cert_t *, tor_x509_cert_new,(X509 *x509_cert));
STATIC int tor_tls_context_init_one(tor_tls_context_t **ppcontext,
                                    crypto_pk_t *identity,
                                    unsigned int key_lifetime,
                                    unsigned int flags,
                                    int is_client);
STATIC void tls_log_errors(tor_tls_t *tls, int severity, int domain,
                           const char *doing);
#endif

const char *tor_tls_err_to_string(int err);
void tor_tls_get_state_description(tor_tls_t *tls, char *buf, size_t sz);

void tor_tls_free_all(void);

#define TOR_TLS_CTX_IS_PUBLIC_SERVER (1u<<0)
#define TOR_TLS_CTX_USE_ECDHE_P256   (1u<<1)
#define TOR_TLS_CTX_USE_ECDHE_P224   (1u<<2)

int tor_tls_context_init(unsigned flags,
                         crypto_pk_t *client_identity,
                         crypto_pk_t *server_identity,
                         unsigned int key_lifetime);
tor_tls_t *tor_tls_new(int sock, int is_server);
void tor_tls_set_logged_address(tor_tls_t *tls, const char *address);
void tor_tls_set_renegotiate_callback(tor_tls_t *tls,
                                      void (*cb)(tor_tls_t *, void *arg),
                                      void *arg);
int tor_tls_is_server(tor_tls_t *tls);
void tor_tls_free(tor_tls_t *tls);
int tor_tls_peer_has_cert(tor_tls_t *tls);
MOCK_DECL(tor_x509_cert_t *,tor_tls_get_peer_cert,(tor_tls_t *tls));
int tor_tls_verify(int severity, tor_tls_t *tls, crypto_pk_t **identity);
int tor_tls_check_lifetime(int severity,
                           tor_tls_t *tls, int past_tolerance,
                           int future_tolerance);
MOCK_DECL(int, tor_tls_read, (tor_tls_t *tls, char *cp, size_t len));
int tor_tls_write(tor_tls_t *tls, const char *cp, size_t n);
int tor_tls_handshake(tor_tls_t *tls);
int tor_tls_finish_handshake(tor_tls_t *tls);
void tor_tls_unblock_renegotiation(tor_tls_t *tls);
void tor_tls_block_renegotiation(tor_tls_t *tls);
void tor_tls_assert_renegotiation_unblocked(tor_tls_t *tls);
int tor_tls_shutdown(tor_tls_t *tls);
int tor_tls_get_pending_bytes(tor_tls_t *tls);
size_t tor_tls_get_forced_write_size(tor_tls_t *tls);

void tor_tls_get_n_raw_bytes(tor_tls_t *tls,
                             size_t *n_read, size_t *n_written);

int tor_tls_get_buffer_sizes(tor_tls_t *tls,
                              size_t *rbuf_capacity, size_t *rbuf_bytes,
                              size_t *wbuf_capacity, size_t *wbuf_bytes);

MOCK_DECL(double, tls_get_write_overhead_ratio, (void));

int tor_tls_used_v1_handshake(tor_tls_t *tls);
int tor_tls_get_num_server_handshakes(tor_tls_t *tls);
int tor_tls_server_got_renegotiate(tor_tls_t *tls);
MOCK_DECL(int,tor_tls_get_tlssecrets,(tor_tls_t *tls, uint8_t *secrets_out));

/* Log and abort if there are unhandled TLS errors in OpenSSL's error stack.
 */
#define check_no_tls_errors() check_no_tls_errors_(__FILE__,__LINE__)

void check_no_tls_errors_(const char *fname, int line);
void tor_tls_log_one_error(tor_tls_t *tls, unsigned long err,
                           int severity, int domain, const char *doing);

#ifdef USE_BUFFEREVENTS
int tor_tls_start_renegotiating(tor_tls_t *tls);
struct bufferevent *tor_tls_init_bufferevent(tor_tls_t *tls,
                                     struct bufferevent *bufev_in,
                                      evutil_socket_t socket, int receiving,
                                     int filter);
#endif

void tor_x509_cert_free(tor_x509_cert_t *cert);
tor_x509_cert_t *tor_x509_cert_decode(const uint8_t *certificate,
                            size_t certificate_len);
void tor_x509_cert_get_der(const tor_x509_cert_t *cert,
                      const uint8_t **encoded_out, size_t *size_out);
const common_digests_t *tor_x509_cert_get_id_digests(
                      const tor_x509_cert_t *cert);
const common_digests_t *tor_x509_cert_get_cert_digests(
                      const tor_x509_cert_t *cert);
int tor_tls_get_my_certs(int server,
                         const tor_x509_cert_t **link_cert_out,
                         const tor_x509_cert_t **id_cert_out);
crypto_pk_t *tor_tls_get_my_client_auth_key(void);
crypto_pk_t *tor_tls_cert_get_key(tor_x509_cert_t *cert);
MOCK_DECL(int,tor_tls_cert_matches_key,(const tor_tls_t *tls,
                                        const tor_x509_cert_t *cert));
int tor_tls_cert_is_valid(int severity,
                          const tor_x509_cert_t *cert,
                          const tor_x509_cert_t *signing_cert,
                          int check_rsa_1024);
const char *tor_tls_get_ciphersuite_name(tor_tls_t *tls);

int evaluate_ecgroup_for_tls(const char *ecgroup);

#endif

