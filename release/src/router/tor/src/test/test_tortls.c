/* Copyright (c) 2010-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define TORTLS_PRIVATE
#define LOG_PRIVATE
#include "orconfig.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

#ifdef __GNUC__
#define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#endif

#if __GNUC__ && GCC_VERSION >= 402
#if GCC_VERSION >= 406
#pragma GCC diagnostic push
#endif
/* Some versions of OpenSSL declare SSL_get_selected_srtp_profile twice in
 * srtp.h. Suppress the GCC warning so we can build with -Wredundant-decl. */
#pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

#include <openssl/opensslv.h>

#include <openssl/ssl.h>
#include <openssl/ssl3.h>
#include <openssl/err.h>
#include <openssl/asn1t.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bn.h>

#if __GNUC__ && GCC_VERSION >= 402
#if GCC_VERSION >= 406
#pragma GCC diagnostic pop
#else
#pragma GCC diagnostic warning "-Wredundant-decls"
#endif
#endif

#include "or.h"
#include "torlog.h"
#include "config.h"
#include "tortls.h"

#include "test.h"
#include "log_test_helpers.h"
#define NS_MODULE tortls

extern tor_tls_context_t *server_tls_context;
extern tor_tls_context_t *client_tls_context;

#if OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,1,0) \
    && !defined(LIBRESSL_VERSION_NUMBER)
#define OPENSSL_OPAQUE
#define SSL_STATE_STR "before SSL initialization"
#else
#define SSL_STATE_STR "before/accept initialization"
#endif

#ifndef OPENSSL_OPAQUE
static SSL_METHOD *
give_me_a_test_method(void)
{
  SSL_METHOD *method = tor_malloc_zero(sizeof(SSL_METHOD));
  memcpy(method, TLSv1_method(), sizeof(SSL_METHOD));
  return method;
}

static int
fake_num_ciphers(void)
{
  return 0;
}
#endif

static void
test_tortls_errno_to_tls_error(void *data)
{
  (void) data;
  tt_int_op(tor_errno_to_tls_error(SOCK_ERRNO(ECONNRESET)),OP_EQ,
            TOR_TLS_ERROR_CONNRESET);
  tt_int_op(tor_errno_to_tls_error(SOCK_ERRNO(ETIMEDOUT)),OP_EQ,
            TOR_TLS_ERROR_TIMEOUT);
  tt_int_op(tor_errno_to_tls_error(SOCK_ERRNO(EHOSTUNREACH)),OP_EQ,
            TOR_TLS_ERROR_NO_ROUTE);
  tt_int_op(tor_errno_to_tls_error(SOCK_ERRNO(ENETUNREACH)),OP_EQ,
            TOR_TLS_ERROR_NO_ROUTE);
  tt_int_op(tor_errno_to_tls_error(SOCK_ERRNO(ECONNREFUSED)),OP_EQ,
            TOR_TLS_ERROR_CONNREFUSED);
  tt_int_op(tor_errno_to_tls_error(0),OP_EQ,TOR_TLS_ERROR_MISC);
 done:
  (void)1;
}

static void
test_tortls_err_to_string(void *data)
{
  (void) data;
  tt_str_op(tor_tls_err_to_string(1),OP_EQ,"[Not an error.]");
  tt_str_op(tor_tls_err_to_string(TOR_TLS_ERROR_MISC),OP_EQ,"misc error");
  tt_str_op(tor_tls_err_to_string(TOR_TLS_ERROR_IO),OP_EQ,"unexpected close");
  tt_str_op(tor_tls_err_to_string(TOR_TLS_ERROR_CONNREFUSED),OP_EQ,
            "connection refused");
  tt_str_op(tor_tls_err_to_string(TOR_TLS_ERROR_CONNRESET),OP_EQ,
            "connection reset");
  tt_str_op(tor_tls_err_to_string(TOR_TLS_ERROR_NO_ROUTE),OP_EQ,
            "host unreachable");
  tt_str_op(tor_tls_err_to_string(TOR_TLS_ERROR_TIMEOUT),OP_EQ,
            "connection timed out");
  tt_str_op(tor_tls_err_to_string(TOR_TLS_CLOSE),OP_EQ,"closed");
  tt_str_op(tor_tls_err_to_string(TOR_TLS_WANTREAD),OP_EQ,"want to read");
  tt_str_op(tor_tls_err_to_string(TOR_TLS_WANTWRITE),OP_EQ,"want to write");
  tt_str_op(tor_tls_err_to_string(-100),OP_EQ,"(unknown error code)");
 done:
  (void)1;
}

static int
mock_tls_cert_matches_key(const tor_tls_t *tls, const tor_x509_cert_t *cert)
{
  (void) tls;
  (void) cert; // XXXX look at this.
  return 1;
}

static void
test_tortls_tor_tls_new(void *data)
{
  (void) data;
  MOCK(tor_tls_cert_matches_key, mock_tls_cert_matches_key);
  crypto_pk_t *key1 = NULL, *key2 = NULL;
  SSL_METHOD *method = NULL;

  key1 = pk_generate(2);
  key2 = pk_generate(3);

  tor_tls_t *tls = NULL;
  tt_int_op(tor_tls_context_init(TOR_TLS_CTX_IS_PUBLIC_SERVER,
                                 key1, key2, 86400), OP_EQ, 0);
  tls = tor_tls_new(-1, 0);
  tt_want(tls);
  tor_tls_free(tls); tls = NULL;

  SSL_CTX_free(client_tls_context->ctx);
  client_tls_context->ctx = NULL;
  tls = tor_tls_new(-1, 0);
  tt_assert(!tls);

#ifndef OPENSSL_OPAQUE
  method = give_me_a_test_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  method->num_ciphers = fake_num_ciphers;
  client_tls_context->ctx = ctx;
  tls = tor_tls_new(-1, 0);
  tt_assert(!tls);
#endif

 done:
  UNMOCK(tor_tls_cert_matches_key);
  crypto_pk_free(key1);
  crypto_pk_free(key2);
  tor_tls_free(tls);
  tor_free(method);
  tor_tls_free_all();
}

#define NS_MODULE tortls
NS_DECL(void, logv, (int severity, log_domain_mask_t domain,
                     const char *funcname, const char *suffix,
                     const char *format, va_list ap));

static void
NS(logv)(int severity, log_domain_mask_t domain,
         const char *funcname, const char *suffix, const char *format,
         va_list ap)
{
  (void) severity;
  (void) domain;
  (void) funcname;
  (void) suffix;
  (void) format;
  (void) ap; // XXXX look at this.
  CALLED(logv)++;
}

static void
test_tortls_tor_tls_get_error(void *data)
{
  (void) data;
  MOCK(tor_tls_cert_matches_key, mock_tls_cert_matches_key);
  crypto_pk_t *key1 = NULL, *key2 = NULL;
  key1 = pk_generate(2);
  key2 = pk_generate(3);

  tor_tls_t *tls = NULL;
  tt_int_op(tor_tls_context_init(TOR_TLS_CTX_IS_PUBLIC_SERVER,
                                 key1, key2, 86400), OP_EQ, 0);
  tls = tor_tls_new(-1, 0);
  NS_MOCK(logv);
  tt_int_op(CALLED(logv), OP_EQ, 0);
  tor_tls_get_error(tls, 0, 0,
                    (const char *)"test", 0, 0);
  tt_int_op(CALLED(logv), OP_EQ, 1);

 done:
  UNMOCK(tor_tls_cert_matches_key);
  NS_UNMOCK(logv);
  crypto_pk_free(key1);
  crypto_pk_free(key2);
  tor_tls_free(tls);
}

static void
test_tortls_get_state_description(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;
  char *buf;
  SSL_CTX *ctx;

  SSL_library_init();
  SSL_load_error_strings();

  ctx = SSL_CTX_new(SSLv23_method());

  buf = tor_malloc_zero(1000);
  tls = tor_malloc_zero(sizeof(tor_tls_t));

  tor_tls_get_state_description(NULL, buf, 20);
  tt_str_op(buf, OP_EQ, "(No SSL object)");

  SSL_free(tls->ssl);
  tls->ssl = NULL;
  tor_tls_get_state_description(tls, buf, 20);
  tt_str_op(buf, OP_EQ, "(No SSL object)");

  tls->ssl = SSL_new(ctx);
  tor_tls_get_state_description(tls, buf, 200);
  tt_str_op(buf, OP_EQ, SSL_STATE_STR " in HANDSHAKE");

  tls->state = TOR_TLS_ST_OPEN;
  tor_tls_get_state_description(tls, buf, 200);
  tt_str_op(buf, OP_EQ, SSL_STATE_STR " in OPEN");

  tls->state = TOR_TLS_ST_GOTCLOSE;
  tor_tls_get_state_description(tls, buf, 200);
  tt_str_op(buf, OP_EQ, SSL_STATE_STR " in GOTCLOSE");

  tls->state = TOR_TLS_ST_SENTCLOSE;
  tor_tls_get_state_description(tls, buf, 200);
  tt_str_op(buf, OP_EQ, SSL_STATE_STR " in SENTCLOSE");

  tls->state = TOR_TLS_ST_CLOSED;
  tor_tls_get_state_description(tls, buf, 200);
  tt_str_op(buf, OP_EQ, SSL_STATE_STR " in CLOSED");

  tls->state = TOR_TLS_ST_RENEGOTIATE;
  tor_tls_get_state_description(tls, buf, 200);
  tt_str_op(buf, OP_EQ, SSL_STATE_STR " in RENEGOTIATE");

  tls->state = TOR_TLS_ST_BUFFEREVENT;
  tor_tls_get_state_description(tls, buf, 200);
  tt_str_op(buf, OP_EQ, SSL_STATE_STR);

  tls->state = 7;
  tor_tls_get_state_description(tls, buf, 200);
  tt_str_op(buf, OP_EQ, SSL_STATE_STR " in unknown TLS state");

 done:
  SSL_CTX_free(ctx);
  SSL_free(tls->ssl);
  tor_free(buf);
  tor_free(tls);
}

extern int tor_tls_object_ex_data_index;

static void
test_tortls_get_by_ssl(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;
  tor_tls_t *res;
  SSL_CTX *ctx;
  SSL *ssl;

  SSL_library_init();
  SSL_load_error_strings();
  tor_tls_allocate_tor_tls_object_ex_data_index();

  ctx = SSL_CTX_new(SSLv23_method());
  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->magic = TOR_TLS_MAGIC;

  ssl = SSL_new(ctx);

  res = tor_tls_get_by_ssl(ssl);
  tt_assert(!res);

  SSL_set_ex_data(ssl, tor_tls_object_ex_data_index, tls);

  res = tor_tls_get_by_ssl(ssl);
  tt_assert(res == tls);

 done:
  SSL_free(ssl);
  SSL_CTX_free(ctx);
  tor_free(tls);
}

static void
test_tortls_allocate_tor_tls_object_ex_data_index(void *ignored)
{
  (void)ignored;
  int first;

  tor_tls_allocate_tor_tls_object_ex_data_index();

  first = tor_tls_object_ex_data_index;
  tor_tls_allocate_tor_tls_object_ex_data_index();
  tt_int_op(first, OP_EQ, tor_tls_object_ex_data_index);

 done:
  (void)0;
}

static void
test_tortls_log_one_error(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;
  SSL_CTX *ctx;
  SSL *ssl = NULL;

  SSL_library_init();
  SSL_load_error_strings();

  ctx = SSL_CTX_new(SSLv23_method());
  tls = tor_malloc_zero(sizeof(tor_tls_t));
  int previous_log = setup_capture_of_logs(LOG_INFO);

  tor_tls_log_one_error(NULL, 0, LOG_WARN, 0, "something");
  expect_log_msg("TLS error while something: "
            "(null) (in (null):(null):---)\n");

  mock_clean_saved_logs();
  tor_tls_log_one_error(tls, 0, LOG_WARN, 0, NULL);
  expect_log_msg("TLS error: (null) "
            "(in (null):(null):---)\n");

  mock_clean_saved_logs();
  tls->address = tor_strdup("127.hello");
  tor_tls_log_one_error(tls, 0, LOG_WARN, 0, NULL);
  expect_log_msg("TLS error with 127.hello: "
            "(null) (in (null):(null):---)\n");
  tor_free(tls->address);

  mock_clean_saved_logs();
  tls->address = tor_strdup("127.hello");
  tor_tls_log_one_error(tls, 0, LOG_WARN, 0, "blarg");
  expect_log_msg("TLS error while blarg with "
            "127.hello: (null) (in (null):(null):---)\n");

  mock_clean_saved_logs();
  tor_tls_log_one_error(tls, ERR_PACK(1, 2, 3), LOG_WARN, 0, NULL);
  expect_log_msg("TLS error with 127.hello: "
            "BN lib (in unknown library:(null):---)\n");

  mock_clean_saved_logs();
  tor_tls_log_one_error(tls, ERR_PACK(1, 2, SSL_R_HTTP_REQUEST),
                        LOG_WARN, 0, NULL);
  expect_log_severity(LOG_INFO);

  mock_clean_saved_logs();
  tor_tls_log_one_error(tls, ERR_PACK(1, 2, SSL_R_HTTPS_PROXY_REQUEST),
                        LOG_WARN, 0, NULL);
  expect_log_severity(LOG_INFO);

  mock_clean_saved_logs();
  tor_tls_log_one_error(tls, ERR_PACK(1, 2, SSL_R_RECORD_LENGTH_MISMATCH),
                        LOG_WARN, 0, NULL);
  expect_log_severity(LOG_INFO);

#ifndef OPENSSL_1_1_API
  mock_clean_saved_logs();
  tor_tls_log_one_error(tls, ERR_PACK(1, 2, SSL_R_RECORD_TOO_LARGE),
                        LOG_WARN, 0, NULL);
  expect_log_severity(LOG_INFO);
#endif

  mock_clean_saved_logs();
  tor_tls_log_one_error(tls, ERR_PACK(1, 2, SSL_R_UNKNOWN_PROTOCOL),
                        LOG_WARN, 0, NULL);
  expect_log_severity(LOG_INFO);

  mock_clean_saved_logs();
  tor_tls_log_one_error(tls, ERR_PACK(1, 2, SSL_R_UNSUPPORTED_PROTOCOL),
                        LOG_WARN, 0, NULL);
  expect_log_severity(LOG_INFO);

  tls->ssl = SSL_new(ctx);

  mock_clean_saved_logs();
  tor_tls_log_one_error(tls, 0, LOG_WARN, 0, NULL);
  expect_log_msg("TLS error with 127.hello: (null)"
            " (in (null):(null):" SSL_STATE_STR ")\n");

 done:
  teardown_capture_of_logs(previous_log);
  SSL_free(ssl);
  SSL_CTX_free(ctx);
  if (tls && tls->ssl)
    SSL_free(tls->ssl);
  if (tls)
    tor_free(tls->address);
  tor_free(tls);
}

#ifndef OPENSSL_OPAQUE
static void
test_tortls_get_error(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;
  int ret;
  SSL_CTX *ctx;

  SSL_library_init();
  SSL_load_error_strings();

  ctx = SSL_CTX_new(SSLv23_method());
  int previous_log = setup_capture_of_logs(LOG_INFO);
  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = SSL_new(ctx);
  SSL_set_bio(tls->ssl, BIO_new(BIO_s_mem()), NULL);

  ret = tor_tls_get_error(tls, 0, 0, "something", LOG_WARN, 0);
  tt_int_op(ret, OP_EQ, TOR_TLS_ERROR_IO);
  expect_log_msg("TLS error: unexpected close while"
            " something (before/accept initialization)\n");

  mock_clean_saved_logs();
  ret = tor_tls_get_error(tls, 2, 0, "something", LOG_WARN, 0);
  tt_int_op(ret, OP_EQ, 0);
  expect_no_log_entry();

  mock_clean_saved_logs();
  ret = tor_tls_get_error(tls, 0, 1, "something", LOG_WARN, 0);
  tt_int_op(ret, OP_EQ, -11);
  expect_no_log_entry();

  mock_clean_saved_logs();
  ERR_clear_error();
  ERR_put_error(ERR_LIB_BN, 2, -1, "somewhere.c", 99);
  ret = tor_tls_get_error(tls, 0, 0, "something", LOG_WARN, 0);
  tt_int_op(ret, OP_EQ, TOR_TLS_ERROR_MISC);
  expect_log_msg("TLS error while something: (null)"
            " (in bignum routines:(null):before/accept initialization)\n");

  mock_clean_saved_logs();
  ERR_clear_error();
  tls->ssl->rwstate = SSL_READING;
  SSL_get_rbio(tls->ssl)->flags = BIO_FLAGS_READ;
  ret = tor_tls_get_error(tls, -1, 0, "something", LOG_WARN, 0);
  tt_int_op(ret, OP_EQ, TOR_TLS_WANTREAD);
  expect_no_log_entry();

  mock_clean_saved_logs();
  ERR_clear_error();
  tls->ssl->rwstate = SSL_READING;
  SSL_get_rbio(tls->ssl)->flags = BIO_FLAGS_WRITE;
  ret = tor_tls_get_error(tls, -1, 0, "something", LOG_WARN, 0);
  tt_int_op(ret, OP_EQ, TOR_TLS_WANTWRITE);
  expect_no_log_entry();

  mock_clean_saved_logs();
  ERR_clear_error();
  tls->ssl->rwstate = 0;
  tls->ssl->shutdown = SSL_RECEIVED_SHUTDOWN;
  tls->ssl->s3->warn_alert =SSL_AD_CLOSE_NOTIFY;
  ret = tor_tls_get_error(tls, 0, 0, "something", LOG_WARN, 0);
  tt_int_op(ret, OP_EQ, TOR_TLS_CLOSE);
  expect_log_entry();

  mock_clean_saved_logs();
  ret = tor_tls_get_error(tls, 0, 2, "something", LOG_WARN, 0);
  tt_int_op(ret, OP_EQ, -10);
  expect_no_log_entry();

  mock_clean_saved_logs();
  ERR_put_error(ERR_LIB_SYS, 2, -1, "somewhere.c", 99);
  ret = tor_tls_get_error(tls, -1, 0, "something", LOG_WARN, 0);
  tt_int_op(ret, OP_EQ, -9);
  expect_log_msg("TLS error while something: (null) (in system library:"
            "connect:before/accept initialization)\n");

 done:
  teardown_capture_of_logs(previous_log);
  SSL_free(tls->ssl);
  tor_free(tls);
  SSL_CTX_free(ctx);
}
#endif

static void
test_tortls_always_accept_verify_cb(void *ignored)
{
  (void)ignored;
  int ret;

  ret = always_accept_verify_cb(0, NULL);
  tt_int_op(ret, OP_EQ, 1);

 done:
  (void)0;
}

#ifndef OPENSSL_OPAQUE
static void
test_tortls_x509_cert_free(void *ignored)
{
  (void)ignored;
  tor_x509_cert_t *cert;

  cert = tor_malloc_zero(sizeof(tor_x509_cert_t));
  tor_x509_cert_free(cert);

  cert = tor_malloc_zero(sizeof(tor_x509_cert_t));
  cert->cert = tor_malloc_zero(sizeof(X509));
  cert->encoded = tor_malloc_zero(1);
  tor_x509_cert_free(cert);
}
#endif

static void
test_tortls_x509_cert_get_id_digests(void *ignored)
{
  (void)ignored;
  tor_x509_cert_t *cert;
  common_digests_t *d;
  const common_digests_t *res;
  cert = tor_malloc_zero(sizeof(tor_x509_cert_t));
  d = tor_malloc_zero(sizeof(common_digests_t));
  d->d[0][0] = 42;

  res = tor_x509_cert_get_id_digests(cert);
  tt_assert(!res);

  cert->pkey_digests_set = 1;
  cert->pkey_digests = *d;
  res = tor_x509_cert_get_id_digests(cert);
  tt_int_op(res->d[0][0], OP_EQ, 42);

 done:
  tor_free(cert);
  tor_free(d);
}

#ifndef OPENSSL_OPAQUE
static int
fixed_pub_cmp(const EVP_PKEY *a, const EVP_PKEY *b)
{
  (void) a; (void) b;
  return 1;
}

static void
fake_x509_free(X509 *cert)
{
  if (cert) {
    if (cert->cert_info) {
      if (cert->cert_info->key) {
        if (cert->cert_info->key->pkey) {
          tor_free(cert->cert_info->key->pkey);
        }
        tor_free(cert->cert_info->key);
      }
      tor_free(cert->cert_info);
    }
    tor_free(cert);
  }
}

static void
test_tortls_cert_matches_key(void *ignored)
{
  (void)ignored;
  int res;
  tor_tls_t *tls;
  tor_x509_cert_t *cert;
  X509 *one = NULL, *two = NULL;
  EVP_PKEY_ASN1_METHOD *meth = EVP_PKEY_asn1_new(999, 0, NULL, NULL);
  EVP_PKEY_asn1_set_public(meth, NULL, NULL, fixed_pub_cmp, NULL, NULL, NULL);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  cert = tor_malloc_zero(sizeof(tor_x509_cert_t));
  one = tor_malloc_zero(sizeof(X509));
  one->references = 1;
  two = tor_malloc_zero(sizeof(X509));
  two->references = 1;

  res = tor_tls_cert_matches_key(tls, cert);
  tt_int_op(res, OP_EQ, 0);

  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->session = tor_malloc_zero(sizeof(SSL_SESSION));
  tls->ssl->session->peer = one;
  res = tor_tls_cert_matches_key(tls, cert);
  tt_int_op(res, OP_EQ, 0);

  cert->cert = two;
  res = tor_tls_cert_matches_key(tls, cert);
  tt_int_op(res, OP_EQ, 0);

  one->cert_info = tor_malloc_zero(sizeof(X509_CINF));
  one->cert_info->key = tor_malloc_zero(sizeof(X509_PUBKEY));
  one->cert_info->key->pkey = tor_malloc_zero(sizeof(EVP_PKEY));
  one->cert_info->key->pkey->references = 1;
  one->cert_info->key->pkey->ameth = meth;
  one->cert_info->key->pkey->type = 1;

  two->cert_info = tor_malloc_zero(sizeof(X509_CINF));
  two->cert_info->key = tor_malloc_zero(sizeof(X509_PUBKEY));
  two->cert_info->key->pkey = tor_malloc_zero(sizeof(EVP_PKEY));
  two->cert_info->key->pkey->references = 1;
  two->cert_info->key->pkey->ameth = meth;
  two->cert_info->key->pkey->type = 2;

  res = tor_tls_cert_matches_key(tls, cert);
  tt_int_op(res, OP_EQ, 0);

  one->cert_info->key->pkey->type = 1;
  two->cert_info->key->pkey->type = 1;
  res = tor_tls_cert_matches_key(tls, cert);
  tt_int_op(res, OP_EQ, 1);

 done:
  EVP_PKEY_asn1_free(meth);
  tor_free(tls->ssl->session);
  tor_free(tls->ssl);
  tor_free(tls);
  tor_free(cert);
  fake_x509_free(one);
  fake_x509_free(two);
}

static void
test_tortls_cert_get_key(void *ignored)
{
  (void)ignored;
  tor_x509_cert_t *cert = NULL;
  crypto_pk_t *res = NULL;
  cert = tor_malloc_zero(sizeof(tor_x509_cert_t));
  X509 *key = NULL;
  key = tor_malloc_zero(sizeof(X509));
  key->references = 1;

  res = tor_tls_cert_get_key(cert);
  tt_assert(!res);

  cert->cert = key;
  key->cert_info = tor_malloc_zero(sizeof(X509_CINF));
  key->cert_info->key = tor_malloc_zero(sizeof(X509_PUBKEY));
  key->cert_info->key->pkey = tor_malloc_zero(sizeof(EVP_PKEY));
  key->cert_info->key->pkey->references = 1;
  key->cert_info->key->pkey->type = 2;
  res = tor_tls_cert_get_key(cert);
  tt_assert(!res);

 done:
  fake_x509_free(key);
  tor_free(cert);
  crypto_pk_free(res);
}
#endif

static void
test_tortls_get_my_client_auth_key(void *ignored)
{
  (void)ignored;
  crypto_pk_t *ret;
  crypto_pk_t *expected;
  tor_tls_context_t *ctx;
  RSA *k = RSA_new();

  ctx = tor_malloc_zero(sizeof(tor_tls_context_t));
  expected = crypto_new_pk_from_rsa_(k);
  ctx->auth_key = expected;

  client_tls_context = NULL;
  ret = tor_tls_get_my_client_auth_key();
  tt_assert(!ret);

  client_tls_context = ctx;
  ret = tor_tls_get_my_client_auth_key();
  tt_assert(ret == expected);

 done:
  RSA_free(k);
  tor_free(expected);
  tor_free(ctx);
}

static void
test_tortls_get_my_certs(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_context_t *ctx;
  const tor_x509_cert_t *link_cert_out = NULL;
  const tor_x509_cert_t *id_cert_out = NULL;

  ctx = tor_malloc_zero(sizeof(tor_tls_context_t));

  client_tls_context = NULL;
  ret = tor_tls_get_my_certs(0, NULL, NULL);
  tt_int_op(ret, OP_EQ, -1);

  server_tls_context = NULL;
  ret = tor_tls_get_my_certs(1, NULL, NULL);
  tt_int_op(ret, OP_EQ, -1);

  client_tls_context = ctx;
  ret = tor_tls_get_my_certs(0, NULL, NULL);
  tt_int_op(ret, OP_EQ, 0);

  client_tls_context = ctx;
  ret = tor_tls_get_my_certs(0, &link_cert_out, &id_cert_out);
  tt_int_op(ret, OP_EQ, 0);

  server_tls_context = ctx;
  ret = tor_tls_get_my_certs(1, &link_cert_out, &id_cert_out);
  tt_int_op(ret, OP_EQ, 0);

 done:
  (void)1;
}

#ifndef OPENSSL_OPAQUE
static void
test_tortls_get_ciphersuite_name(void *ignored)
{
  (void)ignored;
  const char *ret;
  tor_tls_t *ctx;
  ctx = tor_malloc_zero(sizeof(tor_tls_t));
  ctx->ssl = tor_malloc_zero(sizeof(SSL));

  ret = tor_tls_get_ciphersuite_name(ctx);
  tt_str_op(ret, OP_EQ, "(NONE)");

 done:
  tor_free(ctx->ssl);
  tor_free(ctx);
}

static SSL_CIPHER *
get_cipher_by_name(const char *name)
{
  int i;
  const SSL_METHOD *method = SSLv23_method();
  int num = method->num_ciphers();
  for (i = 0; i < num; ++i) {
    const SSL_CIPHER *cipher = method->get_cipher(i);
    const char *ciphername = SSL_CIPHER_get_name(cipher);
    if (!strcmp(ciphername, name)) {
      return (SSL_CIPHER *)cipher;
    }
  }

  return NULL;
}

static SSL_CIPHER *
get_cipher_by_id(uint16_t id)
{
  int i;
  const SSL_METHOD *method = SSLv23_method();
  int num = method->num_ciphers();
  for (i = 0; i < num; ++i) {
    const SSL_CIPHER *cipher = method->get_cipher(i);
    if (id == (SSL_CIPHER_get_id(cipher) & 0xffff)) {
      return (SSL_CIPHER *)cipher;
    }
  }

  return NULL;
}

extern uint16_t v2_cipher_list[];

static void
test_tortls_classify_client_ciphers(void *ignored)
{
  (void)ignored;
  int i;
  int ret;
  SSL_CTX *ctx;
  SSL *ssl;
  tor_tls_t *tls;
  STACK_OF(SSL_CIPHER) *ciphers;
  SSL_CIPHER *tmp_cipher;

  SSL_library_init();
  SSL_load_error_strings();
  tor_tls_allocate_tor_tls_object_ex_data_index();

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->magic = TOR_TLS_MAGIC;

  ctx = SSL_CTX_new(TLSv1_method());
  ssl = SSL_new(ctx);
  tls->ssl = ssl;

  ciphers = sk_SSL_CIPHER_new_null();

  ret = tor_tls_classify_client_ciphers(ssl, NULL);
  tt_int_op(ret, OP_EQ, -1);

  SSL_set_ex_data(ssl, tor_tls_object_ex_data_index, tls);
  tls->client_cipher_list_type = 42;

  ret = tor_tls_classify_client_ciphers(ssl, NULL);
  tt_int_op(ret, OP_EQ, 42);

  tls->client_cipher_list_type = 0;
  ret = tor_tls_classify_client_ciphers(ssl, ciphers);
  tt_int_op(ret, OP_EQ, 1);
  tt_int_op(tls->client_cipher_list_type, OP_EQ, 1);

  tls->client_cipher_list_type = 0;
  ret = tor_tls_classify_client_ciphers(ssl, SSL_get_ciphers(ssl));
  tt_int_op(ret, OP_EQ, 3);
  tt_int_op(tls->client_cipher_list_type, OP_EQ, 3);

  SSL_CIPHER *one = get_cipher_by_name(TLS1_TXT_DHE_RSA_WITH_AES_128_SHA),
    *two = get_cipher_by_name(TLS1_TXT_DHE_RSA_WITH_AES_256_SHA),
    *three = get_cipher_by_name(SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA),
    *four = NULL;
  sk_SSL_CIPHER_push(ciphers, one);
  sk_SSL_CIPHER_push(ciphers, two);
  sk_SSL_CIPHER_push(ciphers, three);
  sk_SSL_CIPHER_push(ciphers, four);

  tls->client_cipher_list_type = 0;
  ret = tor_tls_classify_client_ciphers(ssl, ciphers);
  tt_int_op(ret, OP_EQ, 1);
  tt_int_op(tls->client_cipher_list_type, OP_EQ, 1);

  sk_SSL_CIPHER_zero(ciphers);

  one = get_cipher_by_name("ECDH-RSA-AES256-GCM-SHA384");
  one->id = 0x00ff;
  two = get_cipher_by_name("ECDH-RSA-AES128-GCM-SHA256");
  two->id = 0x0000;
  sk_SSL_CIPHER_push(ciphers, one);
  tls->client_cipher_list_type = 0;
  ret = tor_tls_classify_client_ciphers(ssl, ciphers);
  tt_int_op(ret, OP_EQ, 3);
  tt_int_op(tls->client_cipher_list_type, OP_EQ, 3);

  sk_SSL_CIPHER_push(ciphers, two);
  tls->client_cipher_list_type = 0;
  ret = tor_tls_classify_client_ciphers(ssl, ciphers);
  tt_int_op(ret, OP_EQ, 3);
  tt_int_op(tls->client_cipher_list_type, OP_EQ, 3);

  one->id = 0xC00A;
  tls->client_cipher_list_type = 0;
  ret = tor_tls_classify_client_ciphers(ssl, ciphers);
  tt_int_op(ret, OP_EQ, 3);
  tt_int_op(tls->client_cipher_list_type, OP_EQ, 3);

  sk_SSL_CIPHER_zero(ciphers);
  for (i=0; v2_cipher_list[i]; i++) {
    tmp_cipher = get_cipher_by_id(v2_cipher_list[i]);
    tt_assert(tmp_cipher);
    sk_SSL_CIPHER_push(ciphers, tmp_cipher);
  }
  tls->client_cipher_list_type = 0;
  ret = tor_tls_classify_client_ciphers(ssl, ciphers);
  tt_int_op(ret, OP_EQ, 2);
  tt_int_op(tls->client_cipher_list_type, OP_EQ, 2);

 done:
  sk_SSL_CIPHER_free(ciphers);
  SSL_free(tls->ssl);
  tor_free(tls);
  SSL_CTX_free(ctx);
}
#endif

static void
test_tortls_client_is_using_v2_ciphers(void *ignored)
{
  (void)ignored;

#ifdef HAVE_SSL_GET_CLIENT_CIPHERS
  tt_skip();
 done:
  (void)1;
#else
  int ret;
  SSL_CTX *ctx;
  SSL *ssl;
  SSL_SESSION *sess;
  STACK_OF(SSL_CIPHER) *ciphers;

  SSL_library_init();
  SSL_load_error_strings();

  ctx = SSL_CTX_new(TLSv1_method());
  ssl = SSL_new(ctx);
  sess = SSL_SESSION_new();

  ret = tor_tls_client_is_using_v2_ciphers(ssl);
  tt_int_op(ret, OP_EQ, -1);

  ssl->session = sess;
  ret = tor_tls_client_is_using_v2_ciphers(ssl);
  tt_int_op(ret, OP_EQ, 0);

  ciphers = sk_SSL_CIPHER_new_null();
  SSL_CIPHER *one = get_cipher_by_name("ECDH-RSA-AES256-GCM-SHA384");
  one->id = 0x00ff;
  sk_SSL_CIPHER_push(ciphers, one);
  sess->ciphers = ciphers;
  ret = tor_tls_client_is_using_v2_ciphers(ssl);
  tt_int_op(ret, OP_EQ, 1);
 done:
  SSL_free(ssl);
  SSL_CTX_free(ctx);
#endif
}

#ifndef OPENSSL_OPAQUE
static X509 *fixed_try_to_extract_certs_from_tls_cert_out_result = NULL;
static X509 *fixed_try_to_extract_certs_from_tls_id_cert_out_result = NULL;

static void
fixed_try_to_extract_certs_from_tls(int severity, tor_tls_t *tls,
                                    X509 **cert_out, X509 **id_cert_out)
{
  (void) severity;
  (void) tls;
  *cert_out = fixed_try_to_extract_certs_from_tls_cert_out_result;
  *id_cert_out = fixed_try_to_extract_certs_from_tls_id_cert_out_result;
}
#endif

#ifndef OPENSSL_OPAQUE
static const char* notCompletelyValidCertString =
  "-----BEGIN CERTIFICATE-----\n"
  "MIICVjCCAb8CAg37MA0GCSqGSIb3DQEBBQUAMIGbMQswCQYDVQQGEwJKUDEOMAwG\n"
  "A1UECBMFVG9reW8xEDAOBgNVBAcTB0NodW8ta3UxETAPBgNVBAoTCEZyYW5rNERE\n"
  "MRgwFgYDVQQLEw9XZWJDZXJ0IFN1cHBvcnQxGDAWBgNVBAMTD0ZyYW5rNEREIFdl\n"
  "YiBDQTEjMCEGCSqGSIb3DQEJARYUc3VwcG9ydEBmcmFuazRkZC5jb20wHhcNMTIw\n"
  "ODIyMDUyNzIzWhcNMTcwODIxMDUyNzIzWjBKMQswCQYDVQQGEwJKUDEOMAwGA1UE\n"
  "CAwFVG9reW8xETAPBgNVBAoMCEZyYW5rNEREMRgwFgYDVQQDDA93d3cuZXhhbXBs\n"
  "ZS5jb20wgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAMYBBrx5PlP0WNI/ZdzD\n"
  "+6Pktmurn+F2kQYbtc7XQh8/LTBvCo+P6iZoLEmUA9e7EXLRxgU1CVqeAi7QcAn9\n"
  "MwBlc8ksFJHB0rtf9pmf8Oza9E0Bynlq/4/Kb1x+d+AyhL7oK9tQwB24uHOueHi1\n"
  "C/iVv8CSWKiYe6hzN1txYe8rAgMBAAEwDQYJKoZIhvcNAQEFBQADgYEAASPdjigJ\n"
  "kXCqKWpnZ/Oc75EUcMi6HztaW8abUMlYXPIgkV2F7YanHOB7K4f7OOLjiz8DTPFf\n"
  "jC9UeuErhaA/zzWi8ewMTFZW/WshOrm3fNvcMrMLKtH534JKvcdMg6qIdjTFINIr\n"
  "evnAhf0cwULaebn+lMs8Pdl7y37+sfluVok=\n"
  "-----END CERTIFICATE-----\n";
#endif

static const char* validCertString = "-----BEGIN CERTIFICATE-----\n"
  "MIIDpTCCAY0CAg3+MA0GCSqGSIb3DQEBBQUAMF4xCzAJBgNVBAYTAlVTMREwDwYD\n"
  "VQQIDAhJbGxpbm9pczEQMA4GA1UEBwwHQ2hpY2FnbzEUMBIGA1UECgwLVG9yIFRl\n"
  "c3RpbmcxFDASBgNVBAMMC1RvciBUZXN0aW5nMB4XDTE1MDkwNjEzMzk1OVoXDTQz\n"
  "MDEyMjEzMzk1OVowVjELMAkGA1UEBhMCVVMxEDAOBgNVBAcMB0NoaWNhZ28xFDAS\n"
  "BgNVBAoMC1RvciBUZXN0aW5nMR8wHQYDVQQDDBZ0ZXN0aW5nLnRvcnByb2plY3Qu\n"
  "b3JnMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDoT6uyVVhWyOF3wkHjjYbd\n"
  "nKaykyRv4JVtKQdZ4OpEErmX1zw4MmyzpQNV6iR4bQnWiyLfzyVJMZDIC/WILBfX\n"
  "w2Pza/yuLgUvDc3twMuhOACzOQVO8PrEF/aVv2+hbCCy2udXvKhnYn+CCXl3ozc8\n"
  "XcKYvujTXDyvGWY3xwAjlQIDAQABMA0GCSqGSIb3DQEBBQUAA4ICAQCUvnhzQWuQ\n"
  "MrN+pERkE+zcTI/9dGS90rUMMLgu8VDNqTa0TUQh8uO0EQ6uDvI8Js6e8tgwS0BR\n"
  "UBahqb7ZHv+rejGCBr5OudqD+x4STiiuPNJVs86JTLN8SpM9CHjIBH5WCCN2KOy3\n"
  "mevNoRcRRyYJzSFULCunIK6FGulszigMYGscrO4oiTkZiHPh9KvWT40IMiHfL+Lw\n"
  "EtEWiLex6064LcA2YQ1AMuSZyCexks63lcfaFmQbkYOKqXa1oLkIRuDsOaSVjTfe\n"
  "vec+X6jvf12cFTKS5WIeqkKF2Irt+dJoiHEGTe5RscUMN/f+gqHPzfFz5dR23sxo\n"
  "g+HC6MZHlFkLAOx3wW6epPS8A/m1mw3zMPoTnb2U2YYt8T0dJMMlUn/7Y1sEAa+a\n"
  "dSTMaeUf6VnJ//11m454EZl1to9Z7oJOgqmFffSrdD4BGIWe8f7hhW6L1Enmqe/J\n"
  "BKL3wbzZh80O1W0bndAwhnEEhlzneFY84cbBo9pmVxpODHkUcStpr5Z7pBDrcL21\n"
  "Ss/aB/1YrsVXhdvJdOGxl3Mnl9dUY57CympLGlT8f0pPS6GAKOelECOhFMHmJd8L\n"
  "dj3XQSmKtYHevZ6IvuMXSlB/fJvSjSlkCuLo5+kJoaqPuRu+i/S1qxeRy3CBwmnE\n"
  "LdSNdcX4N79GQJ996PA8+mUCQG7YRtK+WA==\n"
  "-----END CERTIFICATE-----\n";

static const char* caCertString = "-----BEGIN CERTIFICATE-----\n"
  "MIIFjzCCA3egAwIBAgIJAKd5WgyfPMYRMA0GCSqGSIb3DQEBCwUAMF4xCzAJBgNV\n"
  "BAYTAlVTMREwDwYDVQQIDAhJbGxpbm9pczEQMA4GA1UEBwwHQ2hpY2FnbzEUMBIG\n"
  "A1UECgwLVG9yIFRlc3RpbmcxFDASBgNVBAMMC1RvciBUZXN0aW5nMB4XDTE1MDkw\n"
  "NjEzMzc0MVoXDTQzMDEyMjEzMzc0MVowXjELMAkGA1UEBhMCVVMxETAPBgNVBAgM\n"
  "CElsbGlub2lzMRAwDgYDVQQHDAdDaGljYWdvMRQwEgYDVQQKDAtUb3IgVGVzdGlu\n"
  "ZzEUMBIGA1UEAwwLVG9yIFRlc3RpbmcwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAw\n"
  "ggIKAoICAQCpLMUEiLW5leUgBZoEJms2V7lZRhIAjnJBhVMHD0e3UubNknmaQoxf\n"
  "ARz3rvqOaRd0JlV+qM9qE0DjiYcCVP1cAfqAo9d83uS1vwY3YMVJzADlaIiHfyVW\n"
  "uEgBy0vvkeUBqaua24dYlcwsemOiXYLu41yM1wkcGHW1AhBNHppY6cznb8TyLgNM\n"
  "2x3SGUdzc5XMyAFx51faKGBA3wjs+Hg1PLY7d30nmCgEOBavpm5I1disM/0k+Mcy\n"
  "YmAKEo/iHJX/rQzO4b9znP69juLlR8PDBUJEVIG/CYb6+uw8MjjUyiWXYoqfVmN2\n"
  "hm/lH8b6rXw1a2Aa3VTeD0DxaWeacMYHY/i01fd5n7hCoDTRNdSw5KJ0L3Z0SKTu\n"
  "0lzffKzDaIfyZGlpW5qdouACkWYzsaitQOePVE01PIdO30vUfzNTFDfy42ccx3Di\n"
  "59UCu+IXB+eMtrBfsok0Qc63vtF1linJgjHW1z/8ujk8F7/qkOfODhk4l7wngc2A\n"
  "EmwWFIFoGaiTEZHB9qteXr4unbXZ0AHpM02uGGwZEGohjFyebEb73M+J57WKKAFb\n"
  "PqbLcGUksL1SHNBNAJcVLttX55sO4nbidOS/kA3m+F1R04MBTyQF9qA6YDDHqdI3\n"
  "h/3pw0Z4fxVouTYT4/NfRnX4JTP4u+7Mpcoof28VME0qWqD1LnRhFQIDAQABo1Aw\n"
  "TjAdBgNVHQ4EFgQUMoAgIXH7pZ3QMRwTjT+DM9Yo/v0wHwYDVR0jBBgwFoAUMoAg\n"
  "IXH7pZ3QMRwTjT+DM9Yo/v0wDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOC\n"
  "AgEAUJxacjXR9sT+Xs6ISFiUsyd0T6WVKMnV46xrYJHirGfx+krWHrjxMY+ZtxYD\n"
  "DBDGlo11Qc4v6QrclNf5QUBfIiGQsP9Cm6hHcQ+Tpg9HHCgSqG1YNPwCPReCR4br\n"
  "BLvLfrfkcBL2IWM0PdQdCze+59DBfipsULD2mEn9fjYRXQEwb2QWtQ9qRc20Yb/x\n"
  "Q4b/+CvUodLkaq7B8MHz0BV8HHcBoph6DYaRmO/N+hPauIuSp6XyaGYcEefGKVKj\n"
  "G2+fcsdyXsoijNdL8vNKwm4j2gVwCBnw16J00yfFoV46YcbfqEdJB2je0XSvwXqt\n"
  "14AOTngxso2h9k9HLtrfpO1ZG/B5AcCMs1lzbZ2fp5DPHtjvvmvA2RJqgo3yjw4W\n"
  "4DHAuTglYFlC3mDHNfNtcGP20JvepcQNzNP2UzwcpOc94hfKikOFw+gf9Vf1qd0y\n"
  "h/Sk6OZHn2+JVUPiWHIQV98Vtoh4RmUZDJD+b55ia3fQGTGzt4z1XFzQYSva5sfs\n"
  "wocS/papthqWldQU7x+3wofNd5CNU1x6WKXG/yw30IT/4F8ADJD6GeygNT8QJYvt\n"
  "u/8lAkbOy6B9xGmSvr0Kk1oq9P2NshA6kalxp1Oz/DTNDdL4AeBXV3JmM6WWCjGn\n"
  "Yy1RT69d0rwYc5u/vnqODz1IjvT90smsrkBumGt791FAFeg=\n"
  "-----END CERTIFICATE-----\n";

static X509 *
read_cert_from(const char *str)
{
  BIO *bio = BIO_new(BIO_s_mem());
  BIO_write(bio, str, (int) strlen(str));
  X509 *res = PEM_read_bio_X509(bio, NULL, NULL, NULL);
  BIO_free(bio);
  return res;
}

#ifndef OPENSSL_OPAQUE
static void
test_tortls_verify(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  crypto_pk_t *k = NULL;
  X509 *cert1 = NULL, *cert2 = NULL, *invalidCert = NULL,
    *validCert = NULL, *caCert = NULL;

  cert1 = tor_malloc_zero(sizeof(X509));
  cert1->references = 10;

  cert2 = tor_malloc_zero(sizeof(X509));
  cert2->references = 10;

  validCert = read_cert_from(validCertString);
  caCert = read_cert_from(caCertString);
  invalidCert = read_cert_from(notCompletelyValidCertString);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  ret = tor_tls_verify(LOG_WARN, tls, &k);
  tt_int_op(ret, OP_EQ, -1);

  MOCK(try_to_extract_certs_from_tls, fixed_try_to_extract_certs_from_tls);

  fixed_try_to_extract_certs_from_tls_cert_out_result = cert1;
  ret = tor_tls_verify(LOG_WARN, tls, &k);
  tt_int_op(ret, OP_EQ, -1);

  fixed_try_to_extract_certs_from_tls_id_cert_out_result = cert2;
  ret = tor_tls_verify(LOG_WARN, tls, &k);
  tt_int_op(ret, OP_EQ, -1);

  fixed_try_to_extract_certs_from_tls_cert_out_result = invalidCert;
  fixed_try_to_extract_certs_from_tls_id_cert_out_result = invalidCert;

  ret = tor_tls_verify(LOG_WARN, tls, &k);
  tt_int_op(ret, OP_EQ, -1);

  fixed_try_to_extract_certs_from_tls_cert_out_result = validCert;
  fixed_try_to_extract_certs_from_tls_id_cert_out_result = caCert;

  ret = tor_tls_verify(LOG_WARN, tls, &k);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(k);

 done:
  UNMOCK(try_to_extract_certs_from_tls);
  tor_free(cert1);
  tor_free(cert2);
  tor_free(tls);
  tor_free(k);
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_check_lifetime(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  X509 *validCert = read_cert_from(validCertString);
  time_t now = time(NULL);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  ret = tor_tls_check_lifetime(LOG_WARN, tls, 0, 0);
  tt_int_op(ret, OP_EQ, -1);

  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->session = tor_malloc_zero(sizeof(SSL_SESSION));
  tls->ssl->session->peer = validCert;
  ret = tor_tls_check_lifetime(LOG_WARN, tls, 0, 0);
  tt_int_op(ret, OP_EQ, 0);

  ASN1_STRING_free(validCert->cert_info->validity->notBefore);
  validCert->cert_info->validity->notBefore = ASN1_TIME_set(NULL, now-10);
  ASN1_STRING_free(validCert->cert_info->validity->notAfter);
  validCert->cert_info->validity->notAfter = ASN1_TIME_set(NULL, now+60);

  ret = tor_tls_check_lifetime(LOG_WARN, tls, 0, -1000);
  tt_int_op(ret, OP_EQ, -1);

  ret = tor_tls_check_lifetime(LOG_WARN, tls, -1000, 0);
  tt_int_op(ret, OP_EQ, -1);

 done:
  tor_free(tls->ssl->session);
  tor_free(tls->ssl);
  tor_free(tls);
  X509_free(validCert);
}
#endif

#ifndef OPENSSL_OPAQUE
static int fixed_ssl_pending_result = 0;

static int
fixed_ssl_pending(const SSL *ignored)
{
  (void)ignored;
  return fixed_ssl_pending_result;
}

static void
test_tortls_get_pending_bytes(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  SSL_METHOD *method;

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  method = tor_malloc_zero(sizeof(SSL_METHOD));
  method->ssl_pending = fixed_ssl_pending;
  tls->ssl->method = method;

  fixed_ssl_pending_result = 42;
  ret = tor_tls_get_pending_bytes(tls);
  tt_int_op(ret, OP_EQ, 42);

 done:
  tor_free(method);
  tor_free(tls->ssl);
  tor_free(tls);
}
#endif

static void
test_tortls_get_forced_write_size(void *ignored)
{
  (void)ignored;
  long ret;
  tor_tls_t *tls;

  tls = tor_malloc_zero(sizeof(tor_tls_t));

  tls->wantwrite_n = 43;
  ret = tor_tls_get_forced_write_size(tls);
  tt_int_op(ret, OP_EQ, 43);

 done:
  tor_free(tls);
}

extern uint64_t total_bytes_written_over_tls;
extern uint64_t total_bytes_written_by_tls;

static void
test_tortls_get_write_overhead_ratio(void *ignored)
{
  (void)ignored;
  double ret;

  total_bytes_written_over_tls = 0;
  ret = tls_get_write_overhead_ratio();
  tt_int_op(ret, OP_EQ, 1.0);

  total_bytes_written_by_tls = 10;
  total_bytes_written_over_tls = 1;
  ret = tls_get_write_overhead_ratio();
  tt_int_op(ret, OP_EQ, 10.0);

  total_bytes_written_by_tls = 10;
  total_bytes_written_over_tls = 2;
  ret = tls_get_write_overhead_ratio();
  tt_int_op(ret, OP_EQ, 5.0);

 done:
  (void)0;
}

static void
test_tortls_used_v1_handshake(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  tls = tor_malloc_zero(sizeof(tor_tls_t));

  // These tests assume both V2 handshake server and client are enabled
  tls->wasV2Handshake = 0;
  ret = tor_tls_used_v1_handshake(tls);
  tt_int_op(ret, OP_EQ, 1);

  tls->wasV2Handshake = 1;
  ret = tor_tls_used_v1_handshake(tls);
  tt_int_op(ret, OP_EQ, 0);

 done:
  tor_free(tls);
}

static void
test_tortls_get_num_server_handshakes(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;

  tls = tor_malloc_zero(sizeof(tor_tls_t));

  tls->server_handshake_count = 3;
  ret = tor_tls_get_num_server_handshakes(tls);
  tt_int_op(ret, OP_EQ, 3);

 done:
  tor_free(tls);
}

static void
test_tortls_server_got_renegotiate(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;

  tls = tor_malloc_zero(sizeof(tor_tls_t));

  tls->got_renegotiate = 1;
  ret = tor_tls_server_got_renegotiate(tls);
  tt_int_op(ret, OP_EQ, 1);

 done:
  tor_free(tls);
}

#ifndef OPENSSL_OPAQUE
static void
test_tortls_SSL_SESSION_get_master_key(void *ignored)
{
  (void)ignored;
  size_t ret;
  tor_tls_t *tls;
  uint8_t *out;
  out = tor_malloc_zero(1);
  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->session = tor_malloc_zero(sizeof(SSL_SESSION));
  tls->ssl->session->master_key_length = 1;

#ifndef HAVE_SSL_SESSION_GET_MASTER_KEY
  tls->ssl->session->master_key[0] = 43;
  ret = SSL_SESSION_get_master_key(tls->ssl->session, out, 0);
  tt_int_op(ret, OP_EQ, 1);
  tt_int_op(out[0], OP_EQ, 0);

  ret = SSL_SESSION_get_master_key(tls->ssl->session, out, 1);
  tt_int_op(ret, OP_EQ, 1);
  tt_int_op(out[0], OP_EQ, 43);

 done:
#endif
  tor_free(tls->ssl->session);
  tor_free(tls->ssl);
  tor_free(tls);
  tor_free(out);
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_get_tlssecrets(void *ignored)
{
  (void)ignored;
  int ret;
  uint8_t *secret_out = tor_malloc_zero(DIGEST256_LEN);;
  tor_tls_t *tls;
  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->session = tor_malloc_zero(sizeof(SSL_SESSION));
  tls->ssl->session->master_key_length = 1;
  tls->ssl->s3 = tor_malloc_zero(sizeof(SSL3_STATE));

  ret = tor_tls_get_tlssecrets(tls, secret_out);
  tt_int_op(ret, OP_EQ, 0);

 done:
  tor_free(secret_out);
  tor_free(tls->ssl->s3);
  tor_free(tls->ssl->session);
  tor_free(tls->ssl);
  tor_free(tls);
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_get_buffer_sizes(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  size_t rbuf_c=-1, rbuf_b=-1, wbuf_c=-1, wbuf_b=-1;

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->s3 = tor_malloc_zero(sizeof(SSL3_STATE));

  tls->ssl->s3->rbuf.buf = NULL;
  tls->ssl->s3->rbuf.len = 1;
  tls->ssl->s3->rbuf.offset = 0;
  tls->ssl->s3->rbuf.left = 42;

  tls->ssl->s3->wbuf.buf = NULL;
  tls->ssl->s3->wbuf.len = 2;
  tls->ssl->s3->wbuf.offset = 0;
  tls->ssl->s3->wbuf.left = 43;

  ret = tor_tls_get_buffer_sizes(tls, &rbuf_c, &rbuf_b, &wbuf_c, &wbuf_b);
#if OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,1,0)
  tt_int_op(ret, OP_EQ, -1);
#else
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(rbuf_c, OP_EQ, 0);
  tt_int_op(wbuf_c, OP_EQ, 0);
  tt_int_op(rbuf_b, OP_EQ, 42);
  tt_int_op(wbuf_b, OP_EQ, 43);

  tls->ssl->s3->rbuf.buf = tor_malloc_zero(1);
  tls->ssl->s3->wbuf.buf = tor_malloc_zero(1);
  ret = tor_tls_get_buffer_sizes(tls, &rbuf_c, &rbuf_b, &wbuf_c, &wbuf_b);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(rbuf_c, OP_EQ, 1);
  tt_int_op(wbuf_c, OP_EQ, 2);

#endif

 done:
  tor_free(tls->ssl->s3->rbuf.buf);
  tor_free(tls->ssl->s3->wbuf.buf);
  tor_free(tls->ssl->s3);
  tor_free(tls->ssl);
  tor_free(tls);
}
#endif

static void
test_tortls_evaluate_ecgroup_for_tls(void *ignored)
{
  (void)ignored;
  int ret;

  ret = evaluate_ecgroup_for_tls(NULL);
  tt_int_op(ret, OP_EQ, 1);

  ret = evaluate_ecgroup_for_tls("foobar");
  tt_int_op(ret, OP_EQ, 0);

  ret = evaluate_ecgroup_for_tls("P256");
  tt_int_op(ret, OP_EQ, 1);

  ret = evaluate_ecgroup_for_tls("P224");
  //  tt_int_op(ret, OP_EQ, 1); This varies between machines

 done:
  (void)0;
}

#ifndef OPENSSL_OPAQUE
typedef struct cert_pkey_st_local
{
  X509 *x509;
  EVP_PKEY *privatekey;
  const EVP_MD *digest;
} CERT_PKEY_local;

typedef struct sess_cert_st_local
{
  STACK_OF(X509) *cert_chain;
  int peer_cert_type;
  CERT_PKEY_local *peer_key;
  CERT_PKEY_local peer_pkeys[8];
  int references;
} SESS_CERT_local;

static void
test_tortls_try_to_extract_certs_from_tls(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;
  X509 *cert = NULL, *id_cert = NULL, *c1 = NULL, *c2 = NULL;
  SESS_CERT_local *sess = NULL;

  c1 = read_cert_from(validCertString);
  c2 = read_cert_from(caCertString);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->session = tor_malloc_zero(sizeof(SSL_SESSION));
  sess = tor_malloc_zero(sizeof(SESS_CERT_local));
  tls->ssl->session->sess_cert = (void *)sess;

  try_to_extract_certs_from_tls(LOG_WARN, tls, &cert, &id_cert);
  tt_assert(!cert);
  tt_assert(!id_cert);

  tls->ssl->session->peer = c1;
  try_to_extract_certs_from_tls(LOG_WARN, tls, &cert, &id_cert);
  tt_assert(cert == c1);
  tt_assert(!id_cert);
  X509_free(cert); /* decrease refcnt */

  sess->cert_chain = sk_X509_new_null();
  try_to_extract_certs_from_tls(LOG_WARN, tls, &cert, &id_cert);
  tt_assert(cert == c1);
  tt_assert(!id_cert);
  X509_free(cert); /* decrease refcnt */

  sk_X509_push(sess->cert_chain, c1);
  sk_X509_push(sess->cert_chain, c2);

  try_to_extract_certs_from_tls(LOG_WARN, tls, &cert, &id_cert);
  tt_assert(cert == c1);
  tt_assert(id_cert);
  X509_free(cert); /* decrease refcnt */

 done:
  sk_X509_free(sess->cert_chain);
  tor_free(sess);
  tor_free(tls->ssl->session);
  tor_free(tls->ssl);
  tor_free(tls);
  X509_free(c1);
  X509_free(c2);
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_get_peer_cert(void *ignored)
{
  (void)ignored;
  tor_x509_cert_t *ret;
  tor_tls_t *tls;
  X509 *cert = NULL;

  cert = read_cert_from(validCertString);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->session = tor_malloc_zero(sizeof(SSL_SESSION));

  ret = tor_tls_get_peer_cert(tls);
  tt_assert(!ret);

  tls->ssl->session->peer = cert;
  ret = tor_tls_get_peer_cert(tls);
  tt_assert(ret);
  tt_assert(ret->cert == cert);

 done:
  tor_x509_cert_free(ret);
  tor_free(tls->ssl->session);
  tor_free(tls->ssl);
  tor_free(tls);
  X509_free(cert);
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_peer_has_cert(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  X509 *cert = NULL;

  cert = read_cert_from(validCertString);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->session = tor_malloc_zero(sizeof(SSL_SESSION));

  ret = tor_tls_peer_has_cert(tls);
  tt_assert(!ret);

  tls->ssl->session->peer = cert;
  ret = tor_tls_peer_has_cert(tls);
  tt_assert(ret);

 done:
  tor_free(tls->ssl->session);
  tor_free(tls->ssl);
  tor_free(tls);
  X509_free(cert);
}
#endif

static void
test_tortls_is_server(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;
  int ret;

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->isServer = 1;
  ret = tor_tls_is_server(tls);
  tt_int_op(ret, OP_EQ, 1);

 done:
  tor_free(tls);
}

#ifndef OPENSSL_OPAQUE
static void
test_tortls_session_secret_cb(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;
  SSL_CTX *ctx;
  STACK_OF(SSL_CIPHER) *ciphers = NULL;
  SSL_CIPHER *one;

  SSL_library_init();
  SSL_load_error_strings();
  tor_tls_allocate_tor_tls_object_ex_data_index();

  tls = tor_malloc_zero(sizeof(tor_tls_t));

  tls->magic = TOR_TLS_MAGIC;

  ctx = SSL_CTX_new(TLSv1_method());
  tls->ssl = SSL_new(ctx);
  SSL_set_ex_data(tls->ssl, tor_tls_object_ex_data_index, tls);

  SSL_set_session_secret_cb(tls->ssl, tor_tls_session_secret_cb, NULL);

  tor_tls_session_secret_cb(tls->ssl, NULL, NULL, NULL, NULL, NULL);
  tt_assert(!tls->ssl->tls_session_secret_cb);

  one = get_cipher_by_name("ECDH-RSA-AES256-GCM-SHA384");
  one->id = 0x00ff;
  ciphers = sk_SSL_CIPHER_new_null();
  sk_SSL_CIPHER_push(ciphers, one);

  tls->client_cipher_list_type = 0;
  tor_tls_session_secret_cb(tls->ssl, NULL, NULL, ciphers, NULL, NULL);
  tt_assert(!tls->ssl->tls_session_secret_cb);

 done:
  sk_SSL_CIPHER_free(ciphers);
  SSL_free(tls->ssl);
  SSL_CTX_free(ctx);
  tor_free(tls);
}
#endif

#ifndef OPENSSL_OPAQUE
/* TODO: It seems block_renegotiation and unblock_renegotiation and
 * using different blags. This might not be correct */
static void
test_tortls_block_renegotiation(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->s3 = tor_malloc_zero(sizeof(SSL3_STATE));
#ifndef SUPPORT_UNSAFE_RENEGOTIATION_FLAG
#define SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION 0
#endif

  tls->ssl->s3->flags = SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION;

  tor_tls_block_renegotiation(tls);

#ifndef OPENSSL_1_1_API
  tt_assert(!(tls->ssl->s3->flags &
              SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION));
#endif

 done:
  tor_free(tls->ssl->s3);
  tor_free(tls->ssl);
  tor_free(tls);
}

static void
test_tortls_unblock_renegotiation(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tor_tls_unblock_renegotiation(tls);

  tt_uint_op(SSL_get_options(tls->ssl) &
             SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION, OP_EQ,
             SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);

 done:
  tor_free(tls->ssl);
  tor_free(tls);
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_assert_renegotiation_unblocked(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tor_tls_unblock_renegotiation(tls);
  tor_tls_assert_renegotiation_unblocked(tls);
  /* No assertion here - this test will fail if tor_assert is turned on
   * and things are bad. */

  tor_free(tls->ssl);
  tor_free(tls);
}
#endif

static void
test_tortls_set_logged_address(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;

  tls = tor_malloc_zero(sizeof(tor_tls_t));

  tor_tls_set_logged_address(tls, "foo bar");

  tt_str_op(tls->address, OP_EQ, "foo bar");

  tor_tls_set_logged_address(tls, "foo bar 2");
  tt_str_op(tls->address, OP_EQ, "foo bar 2");

 done:
  tor_free(tls->address);
  tor_free(tls);
}

#ifndef OPENSSL_OPAQUE
static void
example_cb(tor_tls_t *t, void *arg)
{
  (void)t;
  (void)arg;
}

static void
test_tortls_set_renegotiate_callback(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;
  const char *arg = "hello";

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));

  tor_tls_set_renegotiate_callback(tls, example_cb, (void*)arg);
  tt_assert(tls->negotiated_callback == example_cb);
  tt_assert(tls->callback_arg == arg);
  tt_assert(!tls->got_renegotiate);

  /* Assumes V2_HANDSHAKE_SERVER */
  tt_assert(tls->ssl->info_callback == tor_tls_server_info_callback);

  tor_tls_set_renegotiate_callback(tls, NULL, (void*)arg);
  tt_assert(tls->ssl->info_callback == tor_tls_debug_state_callback);

 done:
  tor_free(tls->ssl);
  tor_free(tls);
}
#endif

#ifndef OPENSSL_OPAQUE
static SSL_CIPHER *fixed_cipher1 = NULL;
static SSL_CIPHER *fixed_cipher2 = NULL;
static const SSL_CIPHER *
fake_get_cipher(unsigned ncipher)
{

  switch (ncipher) {
  case 1:
    return fixed_cipher1;
  case 2:
    return fixed_cipher2;
  default:
    return NULL;
  }
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_find_cipher_by_id(void *ignored)
{
  (void)ignored;
  int ret;
  SSL *ssl;
  SSL_CTX *ctx;
  const SSL_METHOD *m = TLSv1_method();
  SSL_METHOD *empty_method = tor_malloc_zero(sizeof(SSL_METHOD));

  fixed_cipher1 = tor_malloc_zero(sizeof(SSL_CIPHER));
  fixed_cipher2 = tor_malloc_zero(sizeof(SSL_CIPHER));
  fixed_cipher2->id = 0xC00A;

  SSL_library_init();
  SSL_load_error_strings();

  ctx = SSL_CTX_new(m);
  ssl = SSL_new(ctx);

  ret = find_cipher_by_id(ssl, NULL, 0xC00A);
  tt_int_op(ret, OP_EQ, 1);

  ret = find_cipher_by_id(ssl, m, 0xC00A);
  tt_int_op(ret, OP_EQ, 1);

  ret = find_cipher_by_id(ssl, m, 0xFFFF);
  tt_int_op(ret, OP_EQ, 0);

  ret = find_cipher_by_id(ssl, empty_method, 0xC00A);
  tt_int_op(ret, OP_EQ, 1);

  ret = find_cipher_by_id(ssl, empty_method, 0xFFFF);
#ifdef HAVE_SSL_CIPHER_FIND
  tt_int_op(ret, OP_EQ, 0);
#else
  tt_int_op(ret, OP_EQ, 1);
#endif

  empty_method->get_cipher = fake_get_cipher;
  ret = find_cipher_by_id(ssl, empty_method, 0xC00A);
  tt_int_op(ret, OP_EQ, 1);

  empty_method->get_cipher = m->get_cipher;
  empty_method->num_ciphers = m->num_ciphers;
  ret = find_cipher_by_id(ssl, empty_method, 0xC00A);
  tt_int_op(ret, OP_EQ, 1);

  empty_method->get_cipher = fake_get_cipher;
  empty_method->num_ciphers = m->num_ciphers;
  ret = find_cipher_by_id(ssl, empty_method, 0xC00A);
  tt_int_op(ret, OP_EQ, 1);

  empty_method->num_ciphers = fake_num_ciphers;
  ret = find_cipher_by_id(ssl, empty_method, 0xC00A);
#ifdef HAVE_SSL_CIPHER_FIND
  tt_int_op(ret, OP_EQ, 1);
#else
  tt_int_op(ret, OP_EQ, 0);
#endif

 done:
  tor_free(empty_method);
  SSL_free(ssl);
  SSL_CTX_free(ctx);
  tor_free(fixed_cipher1);
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_debug_state_callback(void *ignored)
{
  (void)ignored;
  SSL *ssl;
  char *buf = tor_malloc_zero(1000);
  int n;

  int previous_log = setup_capture_of_logs(LOG_DEBUG);

  ssl = tor_malloc_zero(sizeof(SSL));

  tor_tls_debug_state_callback(ssl, 32, 45);

  n = tor_snprintf(buf, 1000, "SSL %p is now in state unknown"
               " state [type=32,val=45].\n", ssl);
  /* tor's snprintf returns -1 on error */
  tt_int_op(n, OP_NE, -1);
  expect_log_msg(buf);

 done:
  teardown_capture_of_logs(previous_log);
  tor_free(buf);
  tor_free(ssl);
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_server_info_callback(void *ignored)
{
  (void)ignored;
  tor_tls_t *tls;
  SSL_CTX *ctx;
  SSL *ssl;
  int previous_log = setup_capture_of_logs(LOG_WARN);

  SSL_library_init();
  SSL_load_error_strings();

  ctx = SSL_CTX_new(TLSv1_method());
  ssl = SSL_new(ctx);

  tor_tls_allocate_tor_tls_object_ex_data_index();

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->magic = TOR_TLS_MAGIC;
  tls->ssl = ssl;

  tor_tls_server_info_callback(NULL, 0, 0);

  SSL_set_state(ssl, SSL3_ST_SW_SRVR_HELLO_A);
  mock_clean_saved_logs();
  tor_tls_server_info_callback(ssl, SSL_CB_ACCEPT_LOOP, 0);
  expect_log_msg("Couldn't look up the tls for an SSL*. How odd!\n");

  SSL_set_state(ssl, SSL3_ST_SW_SRVR_HELLO_B);
  mock_clean_saved_logs();
  tor_tls_server_info_callback(ssl, SSL_CB_ACCEPT_LOOP, 0);
  expect_log_msg("Couldn't look up the tls for an SSL*. How odd!\n");

  SSL_set_state(ssl, 99);
  mock_clean_saved_logs();
  tor_tls_server_info_callback(ssl, SSL_CB_ACCEPT_LOOP, 0);
  expect_no_log_entry();

  SSL_set_ex_data(tls->ssl, tor_tls_object_ex_data_index, tls);
  SSL_set_state(ssl, SSL3_ST_SW_SRVR_HELLO_B);
  tls->negotiated_callback = 0;
  tls->server_handshake_count = 120;
  tor_tls_server_info_callback(ssl, SSL_CB_ACCEPT_LOOP, 0);
  tt_int_op(tls->server_handshake_count, OP_EQ, 121);

  tls->server_handshake_count = 127;
  tls->negotiated_callback = (void *)1;
  tor_tls_server_info_callback(ssl, SSL_CB_ACCEPT_LOOP, 0);
  tt_int_op(tls->server_handshake_count, OP_EQ, 127);
  tt_int_op(tls->got_renegotiate, OP_EQ, 1);

  tls->ssl->session = SSL_SESSION_new();
  tls->wasV2Handshake = 0;
  tor_tls_server_info_callback(ssl, SSL_CB_ACCEPT_LOOP, 0);
  tt_int_op(tls->wasV2Handshake, OP_EQ, 0);

 done:
  teardown_capture_of_logs(previous_log);
  SSL_free(ssl);
  SSL_CTX_free(ctx);
  tor_free(tls);
}
#endif

#ifndef OPENSSL_OPAQUE
static int fixed_ssl_read_result_index;
static int fixed_ssl_read_result[5];
static int fixed_ssl_shutdown_result;

static int
fixed_ssl_read(SSL *s, void *buf, int len)
{
  (void)s;
  (void)buf;
  (void)len;
  return fixed_ssl_read_result[fixed_ssl_read_result_index++];
}

static int
fixed_ssl_shutdown(SSL *s)
{
  (void)s;
  return fixed_ssl_shutdown_result;
}

#ifndef LIBRESSL_VERSION_NUMBER
static int fixed_ssl_state_to_set;
static tor_tls_t *fixed_tls;

static int
setting_version_ssl_shutdown(SSL *s)
{
  s->version = SSL2_VERSION;
  return fixed_ssl_shutdown_result;
}

static int
setting_version_and_state_ssl_shutdown(SSL *s)
{
  fixed_tls->state = fixed_ssl_state_to_set;
  s->version = SSL2_VERSION;
  return fixed_ssl_shutdown_result;
}
#endif

static int
dummy_handshake_func(SSL *s)
{
  (void)s;
  return 1;
}

static void
test_tortls_shutdown(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  SSL_METHOD *method = give_me_a_test_method();
  int previous_log = setup_capture_of_logs(LOG_WARN);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->ssl->method = method;
  method->ssl_read = fixed_ssl_read;
  method->ssl_shutdown = fixed_ssl_shutdown;

  ret = tor_tls_shutdown(tls);
  tt_int_op(ret, OP_EQ, -9);

  tls->state = TOR_TLS_ST_SENTCLOSE;
  fixed_ssl_read_result_index = 0;
  fixed_ssl_read_result[0] = 10;
  fixed_ssl_read_result[1] = -1;
  ret = tor_tls_shutdown(tls);
  tt_int_op(ret, OP_EQ, -9);

#ifndef LIBRESSL_VERSION_NUMBER
  tls->ssl->handshake_func = dummy_handshake_func;

  fixed_ssl_read_result_index = 0;
  fixed_ssl_read_result[0] = 10;
  fixed_ssl_read_result[1] = 42;
  fixed_ssl_read_result[2] = 0;
  fixed_ssl_shutdown_result = 1;
  ERR_clear_error();
  tls->ssl->version = SSL2_VERSION;
  ret = tor_tls_shutdown(tls);
  tt_int_op(ret, OP_EQ, TOR_TLS_DONE);
  tt_int_op(tls->state, OP_EQ, TOR_TLS_ST_CLOSED);

  fixed_ssl_read_result_index = 0;
  fixed_ssl_read_result[0] = 10;
  fixed_ssl_read_result[1] = 42;
  fixed_ssl_read_result[2] = 0;
  fixed_ssl_shutdown_result = 0;
  ERR_clear_error();
  tls->ssl->version = 0;
  ret = tor_tls_shutdown(tls);
  tt_int_op(ret, OP_EQ, TOR_TLS_DONE);
  tt_int_op(tls->state, OP_EQ, TOR_TLS_ST_CLOSED);

  fixed_ssl_read_result_index = 0;
  fixed_ssl_read_result[0] = 10;
  fixed_ssl_read_result[1] = 42;
  fixed_ssl_read_result[2] = 0;
  fixed_ssl_shutdown_result = 0;
  ERR_clear_error();
  tls->ssl->version = 0;
  method->ssl_shutdown = setting_version_ssl_shutdown;
  ret = tor_tls_shutdown(tls);
  tt_int_op(ret, OP_EQ, TOR_TLS_ERROR_MISC);

  fixed_ssl_read_result_index = 0;
  fixed_ssl_read_result[0] = 10;
  fixed_ssl_read_result[1] = 42;
  fixed_ssl_read_result[2] = 0;
  fixed_ssl_shutdown_result = 0;
  fixed_tls = tls;
  fixed_ssl_state_to_set = TOR_TLS_ST_GOTCLOSE;
  ERR_clear_error();
  tls->ssl->version = 0;
  method->ssl_shutdown = setting_version_and_state_ssl_shutdown;
  ret = tor_tls_shutdown(tls);
  tt_int_op(ret, OP_EQ, TOR_TLS_ERROR_MISC);

  fixed_ssl_read_result_index = 0;
  fixed_ssl_read_result[0] = 10;
  fixed_ssl_read_result[1] = 42;
  fixed_ssl_read_result[2] = 0;
  fixed_ssl_read_result[3] = -1;
  fixed_ssl_shutdown_result = 0;
  fixed_tls = tls;
  fixed_ssl_state_to_set = 0;
  ERR_clear_error();
  tls->ssl->version = 0;
  method->ssl_shutdown = setting_version_and_state_ssl_shutdown;
  ret = tor_tls_shutdown(tls);
  tt_int_op(ret, OP_EQ, TOR_TLS_ERROR_MISC);
#endif

 done:
  teardown_capture_of_logs(previous_log);
  tor_free(method);
  tor_free(tls->ssl);
  tor_free(tls);
}

static int negotiated_callback_called;

static void
negotiated_callback_setter(tor_tls_t *t, void *arg)
{
  (void)t;
  (void)arg;
  negotiated_callback_called++;
}

static void
test_tortls_read(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  char buf[100];
  SSL_METHOD *method = give_me_a_test_method();
  int previous_log = setup_capture_of_logs(LOG_WARN);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->state = TOR_TLS_ST_OPEN;

  ret = tor_tls_read(tls, buf, 10);
  tt_int_op(ret, OP_EQ, -9);

  /* These tests assume that V2_HANDSHAKE_SERVER is set */
  tls->ssl->handshake_func = dummy_handshake_func;
  tls->ssl->method = method;
  method->ssl_read = fixed_ssl_read;
  fixed_ssl_read_result_index = 0;
  fixed_ssl_read_result[0] = 42;
  tls->state = TOR_TLS_ST_OPEN;
  ERR_clear_error();
  ret = tor_tls_read(tls, buf, 10);
  tt_int_op(ret, OP_EQ, 42);

  tls->state = TOR_TLS_ST_OPEN;
  tls->got_renegotiate = 1;
  fixed_ssl_read_result_index = 0;
  ERR_clear_error();
  ret = tor_tls_read(tls, buf, 10);
  tt_int_op(tls->got_renegotiate, OP_EQ, 0);

  tls->state = TOR_TLS_ST_OPEN;
  tls->got_renegotiate = 1;
  negotiated_callback_called = 0;
  tls->negotiated_callback = negotiated_callback_setter;
  fixed_ssl_read_result_index = 0;
  ERR_clear_error();
  ret = tor_tls_read(tls, buf, 10);
  tt_int_op(negotiated_callback_called, OP_EQ, 1);

#ifndef LIBRESSL_VERSION_NUMBER
  fixed_ssl_read_result_index = 0;
  fixed_ssl_read_result[0] = 0;
  tls->ssl->version = SSL2_VERSION;
  ERR_clear_error();
  ret = tor_tls_read(tls, buf, 10);
  tt_int_op(ret, OP_EQ, TOR_TLS_CLOSE);
  tt_int_op(tls->state, OP_EQ, TOR_TLS_ST_CLOSED);
#endif
  // TODO: fill up

 done:
  teardown_capture_of_logs(previous_log);
  tor_free(tls->ssl);
  tor_free(tls);
  tor_free(method);
}

static int fixed_ssl_write_result;

static int
fixed_ssl_write(SSL *s, const void *buf, int len)
{
  (void)s;
  (void)buf;
  (void)len;
  return fixed_ssl_write_result;
}

static void
test_tortls_write(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  SSL_METHOD *method = give_me_a_test_method();
  char buf[100];
  int previous_log = setup_capture_of_logs(LOG_WARN);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = tor_malloc_zero(sizeof(SSL));
  tls->state = TOR_TLS_ST_OPEN;

  ret = tor_tls_write(tls, buf, 0);
  tt_int_op(ret, OP_EQ, 0);

  ret = tor_tls_write(tls, buf, 10);
  tt_int_op(ret, OP_EQ, -9);

  tls->ssl->method = method;
  tls->wantwrite_n = 1;
  ret = tor_tls_write(tls, buf, 10);
  tt_int_op(tls->wantwrite_n, OP_EQ, 0);

  method->ssl_write = fixed_ssl_write;
  tls->ssl->handshake_func = dummy_handshake_func;
  fixed_ssl_write_result = 1;
  ERR_clear_error();
  ret = tor_tls_write(tls, buf, 10);
  tt_int_op(ret, OP_EQ, 1);

  fixed_ssl_write_result = -1;
  ERR_clear_error();
  tls->ssl->rwstate = SSL_READING;
  SSL_set_bio(tls->ssl, BIO_new(BIO_s_mem()), NULL);
  SSL_get_rbio(tls->ssl)->flags = BIO_FLAGS_READ;
  ret = tor_tls_write(tls, buf, 10);
  tt_int_op(ret, OP_EQ, TOR_TLS_WANTREAD);

  ERR_clear_error();
  tls->ssl->rwstate = SSL_READING;
  SSL_set_bio(tls->ssl, BIO_new(BIO_s_mem()), NULL);
  SSL_get_rbio(tls->ssl)->flags = BIO_FLAGS_WRITE;
  ret = tor_tls_write(tls, buf, 10);
  tt_int_op(ret, OP_EQ, TOR_TLS_WANTWRITE);

 done:
  teardown_capture_of_logs(previous_log);
  BIO_free(tls->ssl->rbio);
  tor_free(tls->ssl);
  tor_free(tls);
  tor_free(method);
}
#endif

#ifndef OPENSSL_OPAQUE
static int fixed_ssl_accept_result;
static int fixed_ssl_connect_result;

static int
setting_error_ssl_accept(SSL *ssl)
{
  (void)ssl;
  ERR_put_error(ERR_LIB_BN, 2, -1, "somewhere.c", 99);
  ERR_put_error(ERR_LIB_SYS, 2, -1, "somewhere.c", 99);
  return fixed_ssl_accept_result;
}

static int
setting_error_ssl_connect(SSL *ssl)
{
  (void)ssl;
  ERR_put_error(ERR_LIB_BN, 2, -1, "somewhere.c", 99);
  ERR_put_error(ERR_LIB_SYS, 2, -1, "somewhere.c", 99);
  return fixed_ssl_connect_result;
}

static int
fixed_ssl_accept(SSL *ssl)
{
  (void) ssl;
  return fixed_ssl_accept_result;
}

static void
test_tortls_handshake(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  SSL_CTX *ctx;
  SSL_METHOD *method = give_me_a_test_method();
  int previous_log = setup_capture_of_logs(LOG_INFO);

  SSL_library_init();
  SSL_load_error_strings();

  ctx = SSL_CTX_new(TLSv1_method());

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = SSL_new(ctx);
  tls->state = TOR_TLS_ST_HANDSHAKE;

  ret = tor_tls_handshake(tls);
  tt_int_op(ret, OP_EQ, -9);

  tls->isServer = 1;
  tls->state = TOR_TLS_ST_HANDSHAKE;
  ret = tor_tls_handshake(tls);
  tt_int_op(ret, OP_EQ, -9);

  tls->ssl->method = method;
  method->ssl_accept = fixed_ssl_accept;
  fixed_ssl_accept_result = 2;
  ERR_clear_error();
  tls->state = TOR_TLS_ST_HANDSHAKE;
  ret = tor_tls_handshake(tls);
  tt_int_op(tls->state, OP_EQ, TOR_TLS_ST_OPEN);

  method->ssl_accept = setting_error_ssl_accept;
  fixed_ssl_accept_result = 1;
  ERR_clear_error();
  mock_clean_saved_logs();
  tls->state = TOR_TLS_ST_HANDSHAKE;
  ret = tor_tls_handshake(tls);
  tt_int_op(ret, OP_EQ, TOR_TLS_ERROR_MISC);
  expect_log_entry();
  /* This fails on jessie.  Investigate why! */
#if 0
  expect_log_msg("TLS error while handshaking: (null) (in bignum routines:"
            "(null):SSLv3 write client hello B)\n");
  expect_log_msg("TLS error while handshaking: (null) (in system library:"
            "connect:SSLv3 write client hello B)\n");
#endif
  expect_log_severity(LOG_INFO);

  tls->isServer = 0;
  method->ssl_connect = setting_error_ssl_connect;
  fixed_ssl_connect_result = 1;
  ERR_clear_error();
  mock_clean_saved_logs();
  tls->state = TOR_TLS_ST_HANDSHAKE;
  ret = tor_tls_handshake(tls);
  tt_int_op(ret, OP_EQ, TOR_TLS_ERROR_MISC);
  expect_log_entry();
#if 0
  /* See above */
  expect_log_msg("TLS error while handshaking: "
            "(null) (in bignum routines:(null):SSLv3 write client hello B)\n");
  expect_log_msg("TLS error while handshaking: "
            "(null) (in system library:connect:SSLv3 write client hello B)\n");
#endif
  expect_log_severity(LOG_WARN);

 done:
  teardown_capture_of_logs(previous_log);
  SSL_free(tls->ssl);
  SSL_CTX_free(ctx);
  tor_free(tls);
  tor_free(method);
}
#endif

#ifndef OPENSSL_OPAQUE
static void
test_tortls_finish_handshake(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_t *tls;
  SSL_CTX *ctx;
  SSL_METHOD *method = give_me_a_test_method();
  SSL_library_init();
  SSL_load_error_strings();

  X509 *c1 = read_cert_from(validCertString);
  SESS_CERT_local *sess = NULL;

  ctx = SSL_CTX_new(method);

  tls = tor_malloc_zero(sizeof(tor_tls_t));
  tls->ssl = SSL_new(ctx);
  tls->state = TOR_TLS_ST_OPEN;

  ret = tor_tls_finish_handshake(tls);
  tt_int_op(ret, OP_EQ, 0);

  tls->isServer = 1;
  tls->wasV2Handshake = 0;
  ret = tor_tls_finish_handshake(tls);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tls->wasV2Handshake, OP_EQ, 1);

  tls->wasV2Handshake = 1;
  ret = tor_tls_finish_handshake(tls);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tls->wasV2Handshake, OP_EQ, 1);

  tls->wasV2Handshake = 1;
  tls->ssl->session = SSL_SESSION_new();
  ret = tor_tls_finish_handshake(tls);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tls->wasV2Handshake, OP_EQ, 0);

  tls->isServer = 0;

  sess = tor_malloc_zero(sizeof(SESS_CERT_local));
  tls->ssl->session->sess_cert = (void *)sess;
  sess->cert_chain = sk_X509_new_null();
  sk_X509_push(sess->cert_chain, c1);
  tls->ssl->session->peer = c1;
  tls->wasV2Handshake = 0;
  ret = tor_tls_finish_handshake(tls);
  tt_int_op(ret, OP_EQ, 0);
  tt_int_op(tls->wasV2Handshake, OP_EQ, 1);

  method->num_ciphers = fake_num_ciphers;
  ret = tor_tls_finish_handshake(tls);
  tt_int_op(ret, OP_EQ, -9);

 done:
  if (sess)
    sk_X509_free(sess->cert_chain);
  if (tls->ssl && tls->ssl->session) {
    tor_free(tls->ssl->session->sess_cert);
  }
  SSL_free(tls->ssl);
  tor_free(tls);
  SSL_CTX_free(ctx);
  tor_free(method);
}
#endif

static int fixed_crypto_pk_new_result_index;
static crypto_pk_t *fixed_crypto_pk_new_result[5];

static crypto_pk_t *
fixed_crypto_pk_new(void)
{
  return fixed_crypto_pk_new_result[fixed_crypto_pk_new_result_index++];
}

#ifndef OPENSSL_OPAQUE
static int fixed_crypto_pk_generate_key_with_bits_result_index;
static int fixed_crypto_pk_generate_key_with_bits_result[5];
static int fixed_tor_tls_create_certificate_result_index;
static X509 *fixed_tor_tls_create_certificate_result[5];
static int fixed_tor_x509_cert_new_result_index;
static tor_x509_cert_t *fixed_tor_x509_cert_new_result[5];

static int
fixed_crypto_pk_generate_key_with_bits(crypto_pk_t *env, int bits)
{
  (void)env;
  (void)bits;
  return fixed_crypto_pk_generate_key_with_bits_result[
                    fixed_crypto_pk_generate_key_with_bits_result_index++];
}

static X509 *
fixed_tor_tls_create_certificate(crypto_pk_t *rsa,
                                 crypto_pk_t *rsa_sign,
                                 const char *cname,
                                 const char *cname_sign,
                                 unsigned int cert_lifetime)
{
  (void)rsa;
  (void)rsa_sign;
  (void)cname;
  (void)cname_sign;
  (void)cert_lifetime;
  return fixed_tor_tls_create_certificate_result[
                             fixed_tor_tls_create_certificate_result_index++];
}

static tor_x509_cert_t *
fixed_tor_x509_cert_new(X509 *x509_cert)
{
  (void) x509_cert;
  return fixed_tor_x509_cert_new_result[
                                      fixed_tor_x509_cert_new_result_index++];
}

static void
test_tortls_context_new(void *ignored)
{
  (void)ignored;
  tor_tls_context_t *ret;
  crypto_pk_t *pk1, *pk2, *pk3, *pk4, *pk5, *pk6, *pk7, *pk8, *pk9, *pk10,
    *pk11, *pk12, *pk13, *pk14, *pk15, *pk16, *pk17, *pk18;

  pk1 = crypto_pk_new();
  pk2 = crypto_pk_new();
  pk3 = crypto_pk_new();
  pk4 = crypto_pk_new();
  pk5 = crypto_pk_new();
  pk6 = crypto_pk_new();
  pk7 = crypto_pk_new();
  pk8 = crypto_pk_new();
  pk9 = crypto_pk_new();
  pk10 = crypto_pk_new();
  pk11 = crypto_pk_new();
  pk12 = crypto_pk_new();
  pk13 = crypto_pk_new();
  pk14 = crypto_pk_new();
  pk15 = crypto_pk_new();
  pk16 = crypto_pk_new();
  pk17 = crypto_pk_new();
  pk18 = crypto_pk_new();

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = NULL;
  MOCK(crypto_pk_new, fixed_crypto_pk_new);
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  MOCK(crypto_pk_generate_key_with_bits,
       fixed_crypto_pk_generate_key_with_bits);
  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk1;
  fixed_crypto_pk_new_result[1] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result[0] = -1;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk2;
  fixed_crypto_pk_new_result[1] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result[0] = 0;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk3;
  fixed_crypto_pk_new_result[1] = pk4;
  fixed_crypto_pk_new_result[2] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result[0] = 0;
  fixed_crypto_pk_generate_key_with_bits_result[1] = -1;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  MOCK(tor_tls_create_certificate, fixed_tor_tls_create_certificate);

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk5;
  fixed_crypto_pk_new_result[1] = pk6;
  fixed_crypto_pk_new_result[2] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  fixed_crypto_pk_generate_key_with_bits_result[1] = 0;
  fixed_tor_tls_create_certificate_result_index = 0;
  fixed_tor_tls_create_certificate_result[0] = NULL;
  fixed_tor_tls_create_certificate_result[1] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[2] = tor_malloc_zero(sizeof(X509));
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk7;
  fixed_crypto_pk_new_result[1] = pk8;
  fixed_crypto_pk_new_result[2] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  fixed_tor_tls_create_certificate_result_index = 0;
  fixed_tor_tls_create_certificate_result[0] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[1] = NULL;
  fixed_tor_tls_create_certificate_result[2] = tor_malloc_zero(sizeof(X509));
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk9;
  fixed_crypto_pk_new_result[1] = pk10;
  fixed_crypto_pk_new_result[2] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  fixed_tor_tls_create_certificate_result_index = 0;
  fixed_tor_tls_create_certificate_result[0] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[1] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[2] = NULL;
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  MOCK(tor_x509_cert_new, fixed_tor_x509_cert_new);
  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk11;
  fixed_crypto_pk_new_result[1] = pk12;
  fixed_crypto_pk_new_result[2] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  fixed_tor_tls_create_certificate_result_index = 0;
  fixed_tor_tls_create_certificate_result[0] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[1] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[2] = tor_malloc_zero(sizeof(X509));
  fixed_tor_x509_cert_new_result_index = 0;
  fixed_tor_x509_cert_new_result[0] = NULL;
  fixed_tor_x509_cert_new_result[1] = NULL;
  fixed_tor_x509_cert_new_result[2] = NULL;
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk13;
  fixed_crypto_pk_new_result[1] = pk14;
  fixed_crypto_pk_new_result[2] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  fixed_tor_tls_create_certificate_result_index = 0;
  fixed_tor_tls_create_certificate_result[0] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[1] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[2] = tor_malloc_zero(sizeof(X509));
  fixed_tor_x509_cert_new_result_index = 0;
  fixed_tor_x509_cert_new_result[0] = tor_malloc_zero(sizeof(tor_x509_cert_t));
  fixed_tor_x509_cert_new_result[1] = NULL;
  fixed_tor_x509_cert_new_result[2] = NULL;
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk15;
  fixed_crypto_pk_new_result[1] = pk16;
  fixed_crypto_pk_new_result[2] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  fixed_tor_tls_create_certificate_result_index = 0;
  fixed_tor_tls_create_certificate_result[0] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[1] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[2] = tor_malloc_zero(sizeof(X509));
  fixed_tor_x509_cert_new_result_index = 0;
  fixed_tor_x509_cert_new_result[0] = tor_malloc_zero(sizeof(tor_x509_cert_t));
  fixed_tor_x509_cert_new_result[1] = tor_malloc_zero(sizeof(tor_x509_cert_t));
  fixed_tor_x509_cert_new_result[2] = NULL;
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = pk17;
  fixed_crypto_pk_new_result[1] = pk18;
  fixed_crypto_pk_new_result[2] = NULL;
  fixed_crypto_pk_generate_key_with_bits_result_index = 0;
  fixed_tor_tls_create_certificate_result_index = 0;
  fixed_tor_tls_create_certificate_result[0] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[1] = tor_malloc_zero(sizeof(X509));
  fixed_tor_tls_create_certificate_result[2] = tor_malloc_zero(sizeof(X509));
  fixed_tor_x509_cert_new_result_index = 0;
  fixed_tor_x509_cert_new_result[0] = tor_malloc_zero(sizeof(tor_x509_cert_t));
  fixed_tor_x509_cert_new_result[1] = tor_malloc_zero(sizeof(tor_x509_cert_t));
  fixed_tor_x509_cert_new_result[2] = tor_malloc_zero(sizeof(tor_x509_cert_t));
  ret = tor_tls_context_new(NULL, 0, 0, 0);
  tt_assert(!ret);

 done:
  UNMOCK(tor_x509_cert_new);
  UNMOCK(tor_tls_create_certificate);
  UNMOCK(crypto_pk_generate_key_with_bits);
  UNMOCK(crypto_pk_new);
}
#endif

static int fixed_crypto_pk_get_evp_pkey_result_index = 0;
static EVP_PKEY *fixed_crypto_pk_get_evp_pkey_result[5];

static EVP_PKEY *
fixed_crypto_pk_get_evp_pkey_(crypto_pk_t *env, int private)
{
  (void) env;
  (void) private;
  return fixed_crypto_pk_get_evp_pkey_result[
                               fixed_crypto_pk_get_evp_pkey_result_index++];
}

static void
test_tortls_create_certificate(void *ignored)
{
  (void)ignored;
  X509 *ret;
  crypto_pk_t *pk1, *pk2;

  pk1 = crypto_pk_new();
  pk2 = crypto_pk_new();

  MOCK(crypto_pk_get_evp_pkey_, fixed_crypto_pk_get_evp_pkey_);
  fixed_crypto_pk_get_evp_pkey_result_index = 0;
  fixed_crypto_pk_get_evp_pkey_result[0] = NULL;
  ret = tor_tls_create_certificate(pk1, pk2, "hello", "hello2", 1);
  tt_assert(!ret);

  fixed_crypto_pk_get_evp_pkey_result_index = 0;
  fixed_crypto_pk_get_evp_pkey_result[0] = EVP_PKEY_new();
  fixed_crypto_pk_get_evp_pkey_result[1] = NULL;
  ret = tor_tls_create_certificate(pk1, pk2, "hello", "hello2", 1);
  tt_assert(!ret);

  fixed_crypto_pk_get_evp_pkey_result_index = 0;
  fixed_crypto_pk_get_evp_pkey_result[0] = EVP_PKEY_new();
  fixed_crypto_pk_get_evp_pkey_result[1] = EVP_PKEY_new();
  ret = tor_tls_create_certificate(pk1, pk2, "hello", "hello2", 1);
  tt_assert(!ret);

 done:
  UNMOCK(crypto_pk_get_evp_pkey_);
  crypto_pk_free(pk1);
  crypto_pk_free(pk2);
}

static void
test_tortls_cert_new(void *ignored)
{
  (void)ignored;
  tor_x509_cert_t *ret;
  X509 *cert = read_cert_from(validCertString);

  ret = tor_x509_cert_new(NULL);
  tt_assert(!ret);

  ret = tor_x509_cert_new(cert);
  tt_assert(ret);
  tor_x509_cert_free(ret);
  ret = NULL;

#if 0
  cert = read_cert_from(validCertString);
  /* XXX this doesn't do what you think: it alters a copy of the pubkey. */
  X509_get_pubkey(cert)->type = EVP_PKEY_DSA;
  ret = tor_x509_cert_new(cert);
  tt_assert(ret);
#endif

#ifndef OPENSSL_OPAQUE
  cert = read_cert_from(validCertString);
  X509_CINF_free(cert->cert_info);
  cert->cert_info = NULL;
  ret = tor_x509_cert_new(cert);
  tt_assert(ret);
#endif

 done:
  tor_x509_cert_free(ret);
}

static void
test_tortls_cert_is_valid(void *ignored)
{
  (void)ignored;
  int ret;
  tor_x509_cert_t *cert = NULL, *scert = NULL;

  scert = tor_malloc_zero(sizeof(tor_x509_cert_t));
  ret = tor_tls_cert_is_valid(LOG_WARN, cert, scert, 0);
  tt_int_op(ret, OP_EQ, 0);

  cert = tor_malloc_zero(sizeof(tor_x509_cert_t));
  ret = tor_tls_cert_is_valid(LOG_WARN, cert, scert, 0);
  tt_int_op(ret, OP_EQ, 0);
  tor_free(scert);
  tor_free(cert);

  cert = tor_x509_cert_new(read_cert_from(validCertString));
  scert = tor_x509_cert_new(read_cert_from(caCertString));
  ret = tor_tls_cert_is_valid(LOG_WARN, cert, scert, 0);
  tt_int_op(ret, OP_EQ, 1);

#ifndef OPENSSL_OPAQUE
  tor_x509_cert_free(cert);
  tor_x509_cert_free(scert);
  cert = tor_x509_cert_new(read_cert_from(validCertString));
  scert = tor_x509_cert_new(read_cert_from(caCertString));
  ASN1_TIME_free(cert->cert->cert_info->validity->notAfter);
  cert->cert->cert_info->validity->notAfter =
    ASN1_TIME_set(NULL, time(NULL)-1000000);
  ret = tor_tls_cert_is_valid(LOG_WARN, cert, scert, 0);
  tt_int_op(ret, OP_EQ, 0);

  tor_x509_cert_free(cert);
  tor_x509_cert_free(scert);
  cert = tor_x509_cert_new(read_cert_from(validCertString));
  scert = tor_x509_cert_new(read_cert_from(caCertString));
  X509_PUBKEY_free(cert->cert->cert_info->key);
  cert->cert->cert_info->key = NULL;
  ret = tor_tls_cert_is_valid(LOG_WARN, cert, scert, 1);
  tt_int_op(ret, OP_EQ, 0);
#endif

#if 0
  tor_x509_cert_free(cert);
  tor_x509_cert_free(scert);
  cert = tor_x509_cert_new(read_cert_from(validCertString));
  scert = tor_x509_cert_new(read_cert_from(caCertString));
  /* This doesn't actually change the key in the cert. XXXXXX */
  BN_one(EVP_PKEY_get1_RSA(X509_get_pubkey(cert->cert))->n);
  ret = tor_tls_cert_is_valid(LOG_WARN, cert, scert, 1);
  tt_int_op(ret, OP_EQ, 0);

  tor_x509_cert_free(cert);
  tor_x509_cert_free(scert);
  cert = tor_x509_cert_new(read_cert_from(validCertString));
  scert = tor_x509_cert_new(read_cert_from(caCertString));
  /* This doesn't actually change the key in the cert. XXXXXX */
  X509_get_pubkey(cert->cert)->type = EVP_PKEY_EC;
  ret = tor_tls_cert_is_valid(LOG_WARN, cert, scert, 1);
  tt_int_op(ret, OP_EQ, 0);

  tor_x509_cert_free(cert);
  tor_x509_cert_free(scert);
  cert = tor_x509_cert_new(read_cert_from(validCertString));
  scert = tor_x509_cert_new(read_cert_from(caCertString));
  /* This doesn't actually change the key in the cert. XXXXXX */
  X509_get_pubkey(cert->cert)->type = EVP_PKEY_EC;
  ret = tor_tls_cert_is_valid(LOG_WARN, cert, scert, 0);
  tt_int_op(ret, OP_EQ, 1);

  tor_x509_cert_free(cert);
  tor_x509_cert_free(scert);
  cert = tor_x509_cert_new(read_cert_from(validCertString));
  scert = tor_x509_cert_new(read_cert_from(caCertString));
  /* This doesn't actually change the key in the cert. XXXXXX */
  X509_get_pubkey(cert->cert)->type = EVP_PKEY_EC;
  X509_get_pubkey(cert->cert)->ameth = NULL;
  ret = tor_tls_cert_is_valid(LOG_WARN, cert, scert, 0);
  tt_int_op(ret, OP_EQ, 0);
#endif

 done:
  tor_x509_cert_free(cert);
  tor_x509_cert_free(scert);
}

static void
test_tortls_context_init_one(void *ignored)
{
  (void)ignored;
  int ret;
  tor_tls_context_t *old = NULL;

  MOCK(crypto_pk_new, fixed_crypto_pk_new);

  fixed_crypto_pk_new_result_index = 0;
  fixed_crypto_pk_new_result[0] = NULL;
  ret = tor_tls_context_init_one(&old, NULL, 0, 0, 0);
  tt_int_op(ret, OP_EQ, -1);

 done:
  UNMOCK(crypto_pk_new);
}

#define LOCAL_TEST_CASE(name, flags)                    \
  { #name, test_tortls_##name, (flags|TT_FORK), NULL, NULL }

#ifdef OPENSSL_OPAQUE
#define INTRUSIVE_TEST_CASE(name, flags)        \
  { #name, NULL, TT_SKIP, NULL, NULL }
#else
#define INTRUSIVE_TEST_CASE(name, flags) LOCAL_TEST_CASE(name, flags)
#endif

struct testcase_t tortls_tests[] = {
  LOCAL_TEST_CASE(errno_to_tls_error, 0),
  LOCAL_TEST_CASE(err_to_string, 0),
  LOCAL_TEST_CASE(tor_tls_new, TT_FORK),
  LOCAL_TEST_CASE(tor_tls_get_error, 0),
  LOCAL_TEST_CASE(get_state_description, TT_FORK),
  LOCAL_TEST_CASE(get_by_ssl, TT_FORK),
  LOCAL_TEST_CASE(allocate_tor_tls_object_ex_data_index, TT_FORK),
  LOCAL_TEST_CASE(log_one_error, TT_FORK),
  INTRUSIVE_TEST_CASE(get_error, TT_FORK),
  LOCAL_TEST_CASE(always_accept_verify_cb, 0),
  INTRUSIVE_TEST_CASE(x509_cert_free, 0),
  LOCAL_TEST_CASE(x509_cert_get_id_digests, 0),
  INTRUSIVE_TEST_CASE(cert_matches_key, 0),
  INTRUSIVE_TEST_CASE(cert_get_key, 0),
  LOCAL_TEST_CASE(get_my_client_auth_key, TT_FORK),
  LOCAL_TEST_CASE(get_my_certs, TT_FORK),
  INTRUSIVE_TEST_CASE(get_ciphersuite_name, 0),
  INTRUSIVE_TEST_CASE(classify_client_ciphers, 0),
  LOCAL_TEST_CASE(client_is_using_v2_ciphers, 0),
  INTRUSIVE_TEST_CASE(verify, 0),
  INTRUSIVE_TEST_CASE(check_lifetime, 0),
  INTRUSIVE_TEST_CASE(get_pending_bytes, 0),
  LOCAL_TEST_CASE(get_forced_write_size, 0),
  LOCAL_TEST_CASE(get_write_overhead_ratio, TT_FORK),
  LOCAL_TEST_CASE(used_v1_handshake, TT_FORK),
  LOCAL_TEST_CASE(get_num_server_handshakes, 0),
  LOCAL_TEST_CASE(server_got_renegotiate, 0),
  INTRUSIVE_TEST_CASE(SSL_SESSION_get_master_key, 0),
  INTRUSIVE_TEST_CASE(get_tlssecrets, 0),
  INTRUSIVE_TEST_CASE(get_buffer_sizes, 0),
  LOCAL_TEST_CASE(evaluate_ecgroup_for_tls, 0),
  INTRUSIVE_TEST_CASE(try_to_extract_certs_from_tls, 0),
  INTRUSIVE_TEST_CASE(get_peer_cert, 0),
  INTRUSIVE_TEST_CASE(peer_has_cert, 0),
  INTRUSIVE_TEST_CASE(shutdown, 0),
  INTRUSIVE_TEST_CASE(finish_handshake, 0),
  INTRUSIVE_TEST_CASE(handshake, 0),
  INTRUSIVE_TEST_CASE(write, 0),
  INTRUSIVE_TEST_CASE(read, 0),
  INTRUSIVE_TEST_CASE(server_info_callback, 0),
  LOCAL_TEST_CASE(is_server, 0),
  INTRUSIVE_TEST_CASE(assert_renegotiation_unblocked, 0),
  INTRUSIVE_TEST_CASE(block_renegotiation, 0),
  INTRUSIVE_TEST_CASE(unblock_renegotiation, 0),
  INTRUSIVE_TEST_CASE(set_renegotiate_callback, 0),
  LOCAL_TEST_CASE(set_logged_address, 0),
  INTRUSIVE_TEST_CASE(find_cipher_by_id, 0),
  INTRUSIVE_TEST_CASE(session_secret_cb, 0),
  INTRUSIVE_TEST_CASE(debug_state_callback, 0),
  INTRUSIVE_TEST_CASE(context_new, 0),
  LOCAL_TEST_CASE(create_certificate, 0),
  LOCAL_TEST_CASE(cert_new, 0),
  LOCAL_TEST_CASE(cert_is_valid, 0),
  LOCAL_TEST_CASE(context_init_one, 0),
  END_OF_TESTCASES
};

