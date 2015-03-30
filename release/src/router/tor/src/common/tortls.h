/* Copyright (c) 2003, Roger Dingledine
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_TORTLS_H
#define TOR_TORTLS_H

/**
 * \file tortls.h
 * \brief Headers for tortls.c
 **/

#include "crypto.h"
#include "compat.h"
#include "testsupport.h"

/* Opaque structure to hold a TLS connection. */
typedef struct tor_tls_t tor_tls_t;

/* Opaque structure to hold an X509 certificate. */
typedef struct tor_cert_t tor_cert_t;

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
tor_cert_t *tor_tls_get_peer_cert(tor_tls_t *tls);
int tor_tls_verify(int severity, tor_tls_t *tls, crypto_pk_t **identity);
int tor_tls_check_lifetime(int severity,
                           tor_tls_t *tls, int past_tolerance,
                           int future_tolerance);
int tor_tls_read(tor_tls_t *tls, char *cp, size_t len);
int tor_tls_write(tor_tls_t *tls, const char *cp, size_t n);
int tor_tls_handshake(tor_tls_t *tls);
int tor_tls_finish_handshake(tor_tls_t *tls);
int tor_tls_renegotiate(tor_tls_t *tls);
void tor_tls_unblock_renegotiation(tor_tls_t *tls);
void tor_tls_block_renegotiation(tor_tls_t *tls);
void tor_tls_assert_renegotiation_unblocked(tor_tls_t *tls);
int tor_tls_shutdown(tor_tls_t *tls);
int tor_tls_get_pending_bytes(tor_tls_t *tls);
size_t tor_tls_get_forced_write_size(tor_tls_t *tls);

void tor_tls_get_n_raw_bytes(tor_tls_t *tls,
                             size_t *n_read, size_t *n_written);

void tor_tls_get_buffer_sizes(tor_tls_t *tls,
                              size_t *rbuf_capacity, size_t *rbuf_bytes,
                              size_t *wbuf_capacity, size_t *wbuf_bytes);

MOCK_DECL(double, tls_get_write_overhead_ratio, (void));

int tor_tls_used_v1_handshake(tor_tls_t *tls);
int tor_tls_received_v3_certificate(tor_tls_t *tls);
int tor_tls_get_num_server_handshakes(tor_tls_t *tls);
int tor_tls_server_got_renegotiate(tor_tls_t *tls);
int tor_tls_get_tlssecrets(tor_tls_t *tls, uint8_t *secrets_out);

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

void tor_cert_free(tor_cert_t *cert);
tor_cert_t *tor_cert_decode(const uint8_t *certificate,
                            size_t certificate_len);
void tor_cert_get_der(const tor_cert_t *cert,
                      const uint8_t **encoded_out, size_t *size_out);
const digests_t *tor_cert_get_id_digests(const tor_cert_t *cert);
const digests_t *tor_cert_get_cert_digests(const tor_cert_t *cert);
int tor_tls_get_my_certs(int server,
                         const tor_cert_t **link_cert_out,
                         const tor_cert_t **id_cert_out);
crypto_pk_t *tor_tls_get_my_client_auth_key(void);
crypto_pk_t *tor_tls_cert_get_key(tor_cert_t *cert);
int tor_tls_cert_matches_key(const tor_tls_t *tls, const tor_cert_t *cert);
int tor_tls_cert_is_valid(int severity,
                          const tor_cert_t *cert,
                          const tor_cert_t *signing_cert,
                          int check_rsa_1024);
const char *tor_tls_get_ciphersuite_name(tor_tls_t *tls);

#endif

