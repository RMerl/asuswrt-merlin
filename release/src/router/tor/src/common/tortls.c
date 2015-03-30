/* Copyright (c) 2003, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file tortls.c
 * \brief Wrapper functions to present a consistent interface to
 * TLS, SSL, and X.509 functions from OpenSSL.
 **/

/* (Unlike other tor functions, these
 * are prefixed with tor_ in order to avoid conflicting with OpenSSL
 * functions and variables.)
 */

#include "orconfig.h"

#if defined (WINCE)
#include <WinSock2.h>
#endif

#include <assert.h>
#ifdef _WIN32 /*wrkard for dtls1.h >= 0.9.8m of "#include <winsock.h>"*/
 #ifndef _WIN32_WINNT
 #define _WIN32_WINNT 0x0501
 #endif
 #define WIN32_LEAN_AND_MEAN
 #if defined(_MSC_VER) && (_MSC_VER < 1300)
    #include <winsock.h>
 #else
    #include <winsock2.h>
    #include <ws2tcpip.h>
 #endif
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

#include <openssl/ssl.h>
#include <openssl/ssl3.h>
#include <openssl/err.h>
#include <openssl/tls1.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/opensslv.h>

#if __GNUC__ && GCC_VERSION >= 402
#if GCC_VERSION >= 406
#pragma GCC diagnostic pop
#else
#pragma GCC diagnostic warning "-Wredundant-decls"
#endif
#endif

#ifdef USE_BUFFEREVENTS
#include <event2/bufferevent_ssl.h>
#include <event2/buffer.h>
#include <event2/event.h>
#include "compat_libevent.h"
#endif

#include "crypto.h"
#include "tortls.h"
#include "util.h"
#include "torlog.h"
#include "container.h"
#include <string.h>

#if OPENSSL_VERSION_NUMBER < OPENSSL_V_SERIES(0,9,8)
#error "We require OpenSSL >= 0.9.8"
#endif

/* Enable the "v2" TLS handshake.
 */
#define V2_HANDSHAKE_SERVER
#define V2_HANDSHAKE_CLIENT

/* Copied from or.h */
#define LEGAL_NICKNAME_CHARACTERS \
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

/** How long do identity certificates live? (sec) */
#define IDENTITY_CERT_LIFETIME  (365*24*60*60)

#define ADDR(tls) (((tls) && (tls)->address) ? tls->address : "peer")

#if (OPENSSL_VERSION_NUMBER  <  OPENSSL_V(0,9,8,'s') ||         \
     (OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(0,9,9) &&      \
      OPENSSL_VERSION_NUMBER <  OPENSSL_V(1,0,0,'f')))
/* This is a version of OpenSSL before 0.9.8s/1.0.0f. It does not have
 * the CVE-2011-4576 fix, and as such it can't use RELEASE_BUFFERS and
 * SSL3 safely at the same time.
 */
#define DISABLE_SSL3_HANDSHAKE
#endif

/* We redefine these so that we can run correctly even if the vendor gives us
 * a version of OpenSSL that does not match its header files.  (Apple: I am
 * looking at you.)
 */
#ifndef SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
#define SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION 0x00040000L
#endif
#ifndef SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
#define SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION 0x0010
#endif

/** Does the run-time openssl version look like we need
 * SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION? */
static int use_unsafe_renegotiation_op = 0;
/** Does the run-time openssl version look like we need
 * SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION? */
static int use_unsafe_renegotiation_flag = 0;

/** Structure that we use for a single certificate. */
struct tor_cert_t {
  X509 *cert;
  uint8_t *encoded;
  size_t encoded_len;
  unsigned pkey_digests_set : 1;
  digests_t cert_digests;
  digests_t pkey_digests;
};

/** Holds a SSL_CTX object and related state used to configure TLS
 * connections.
 */
typedef struct tor_tls_context_t {
  int refcnt;
  SSL_CTX *ctx;
  tor_cert_t *my_link_cert;
  tor_cert_t *my_id_cert;
  tor_cert_t *my_auth_cert;
  crypto_pk_t *link_key;
  crypto_pk_t *auth_key;
} tor_tls_context_t;

/** Return values for tor_tls_classify_client_ciphers.
 *
 * @{
 */
/** An error occurred when examining the client ciphers */
#define CIPHERS_ERR -1
/** The client cipher list indicates that a v1 handshake was in use. */
#define CIPHERS_V1 1
/** The client cipher list indicates that the client is using the v2 or the
 * v3 handshake, but that it is (probably!) lying about what ciphers it
 * supports */
#define CIPHERS_V2 2
/** The client cipher list indicates that the client is using the v2 or the
 * v3 handshake, and that it is telling the truth about what ciphers it
 * supports */
#define CIPHERS_UNRESTRICTED 3
/** @} */

#define TOR_TLS_MAGIC 0x71571571

typedef enum {
    TOR_TLS_ST_HANDSHAKE, TOR_TLS_ST_OPEN, TOR_TLS_ST_GOTCLOSE,
    TOR_TLS_ST_SENTCLOSE, TOR_TLS_ST_CLOSED, TOR_TLS_ST_RENEGOTIATE,
    TOR_TLS_ST_BUFFEREVENT
} tor_tls_state_t;
#define tor_tls_state_bitfield_t ENUM_BF(tor_tls_state_t)

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

#ifdef V2_HANDSHAKE_CLIENT
/** An array of fake SSL_CIPHER objects that we use in order to trick OpenSSL
 * in client mode into advertising the ciphers we want.  See
 * rectify_client_ciphers() for details. */
static SSL_CIPHER *CLIENT_CIPHER_DUMMIES = NULL;
/** A stack of SSL_CIPHER objects, some real, some fake.
 * See rectify_client_ciphers() for details. */
static STACK_OF(SSL_CIPHER) *CLIENT_CIPHER_STACK = NULL;
#endif

/** The ex_data index in which we store a pointer to an SSL object's
 * corresponding tor_tls_t object. */
static int tor_tls_object_ex_data_index = -1;

/** Helper: Allocate tor_tls_object_ex_data_index. */
static void
tor_tls_allocate_tor_tls_object_ex_data_index(void)
{
  if (tor_tls_object_ex_data_index == -1) {
    tor_tls_object_ex_data_index =
      SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);
    tor_assert(tor_tls_object_ex_data_index != -1);
  }
}

/** Helper: given a SSL* pointer, return the tor_tls_t object using that
 * pointer. */
static INLINE tor_tls_t *
tor_tls_get_by_ssl(const SSL *ssl)
{
  tor_tls_t *result = SSL_get_ex_data(ssl, tor_tls_object_ex_data_index);
  if (result)
    tor_assert(result->magic == TOR_TLS_MAGIC);
  return result;
}

static void tor_tls_context_decref(tor_tls_context_t *ctx);
static void tor_tls_context_incref(tor_tls_context_t *ctx);
static X509* tor_tls_create_certificate(crypto_pk_t *rsa,
                                        crypto_pk_t *rsa_sign,
                                        const char *cname,
                                        const char *cname_sign,
                                        unsigned int cert_lifetime);

static int tor_tls_context_init_one(tor_tls_context_t **ppcontext,
                                    crypto_pk_t *identity,
                                    unsigned int key_lifetime,
                                    unsigned int flags,
                                    int is_client);
static tor_tls_context_t *tor_tls_context_new(crypto_pk_t *identity,
                                              unsigned int key_lifetime,
                                              unsigned int flags,
                                              int is_client);
static int check_cert_lifetime_internal(int severity, const X509 *cert,
                                   int past_tolerance, int future_tolerance);

/** Global TLS contexts. We keep them here because nobody else needs
 * to touch them.
 *
 * @{ */
static tor_tls_context_t *server_tls_context = NULL;
static tor_tls_context_t *client_tls_context = NULL;
/**@}*/

/** True iff tor_tls_init() has been called. */
static int tls_library_is_initialized = 0;

/* Module-internal error codes. */
#define TOR_TLS_SYSCALL_    (MIN_TOR_TLS_ERROR_VAL_ - 2)
#define TOR_TLS_ZERORETURN_ (MIN_TOR_TLS_ERROR_VAL_ - 1)

/** Write a description of the current state of <b>tls</b> into the
 * <b>sz</b>-byte buffer at <b>buf</b>. */
void
tor_tls_get_state_description(tor_tls_t *tls, char *buf, size_t sz)
{
  const char *ssl_state;
  const char *tortls_state;

  if (PREDICT_UNLIKELY(!tls || !tls->ssl)) {
    strlcpy(buf, "(No SSL object)", sz);
    return;
  }

  ssl_state = SSL_state_string_long(tls->ssl);
  switch (tls->state) {
#define CASE(st) case TOR_TLS_ST_##st: tortls_state = " in "#st ; break
    CASE(HANDSHAKE);
    CASE(OPEN);
    CASE(GOTCLOSE);
    CASE(SENTCLOSE);
    CASE(CLOSED);
    CASE(RENEGOTIATE);
#undef CASE
  case TOR_TLS_ST_BUFFEREVENT:
    tortls_state = "";
    break;
  default:
    tortls_state = " in unknown TLS state";
    break;
  }

  tor_snprintf(buf, sz, "%s%s", ssl_state, tortls_state);
}

/** Log a single error <b>err</b> as returned by ERR_get_error(), which was
 * received while performing an operation <b>doing</b> on <b>tls</b>.  Log
 * the message at <b>severity</b>, in log domain <b>domain</b>. */
void
tor_tls_log_one_error(tor_tls_t *tls, unsigned long err,
                  int severity, int domain, const char *doing)
{
  const char *state = NULL, *addr;
  const char *msg, *lib, *func;

  state = (tls && tls->ssl)?SSL_state_string_long(tls->ssl):"---";

  addr = tls ? tls->address : NULL;

  /* Some errors are known-benign, meaning they are the fault of the other
   * side of the connection. The caller doesn't know this, so override the
   * priority for those cases. */
  switch (ERR_GET_REASON(err)) {
    case SSL_R_HTTP_REQUEST:
    case SSL_R_HTTPS_PROXY_REQUEST:
    case SSL_R_RECORD_LENGTH_MISMATCH:
    case SSL_R_RECORD_TOO_LARGE:
    case SSL_R_UNKNOWN_PROTOCOL:
    case SSL_R_UNSUPPORTED_PROTOCOL:
      severity = LOG_INFO;
      break;
    default:
      break;
  }

  msg = (const char*)ERR_reason_error_string(err);
  lib = (const char*)ERR_lib_error_string(err);
  func = (const char*)ERR_func_error_string(err);
  if (!msg) msg = "(null)";
  if (!lib) lib = "(null)";
  if (!func) func = "(null)";
  if (doing) {
    tor_log(severity, domain, "TLS error while %s%s%s: %s (in %s:%s:%s)",
        doing, addr?" with ":"", addr?addr:"",
        msg, lib, func, state);
  } else {
    tor_log(severity, domain, "TLS error%s%s: %s (in %s:%s:%s)",
        addr?" with ":"", addr?addr:"",
        msg, lib, func, state);
  }
}

/** Log all pending tls errors at level <b>severity</b> in log domain
 * <b>domain</b>.  Use <b>doing</b> to describe our current activities.
 */
static void
tls_log_errors(tor_tls_t *tls, int severity, int domain, const char *doing)
{
  unsigned long err;

  while ((err = ERR_get_error()) != 0) {
    tor_tls_log_one_error(tls, err, severity, domain, doing);
  }
}

/** Convert an errno (or a WSAerrno on windows) into a TOR_TLS_* error
 * code. */
static int
tor_errno_to_tls_error(int e)
{
  switch (e) {
    case SOCK_ERRNO(ECONNRESET): // most common
      return TOR_TLS_ERROR_CONNRESET;
    case SOCK_ERRNO(ETIMEDOUT):
      return TOR_TLS_ERROR_TIMEOUT;
    case SOCK_ERRNO(EHOSTUNREACH):
    case SOCK_ERRNO(ENETUNREACH):
      return TOR_TLS_ERROR_NO_ROUTE;
    case SOCK_ERRNO(ECONNREFUSED):
      return TOR_TLS_ERROR_CONNREFUSED; // least common
    default:
      return TOR_TLS_ERROR_MISC;
  }
}

/** Given a TOR_TLS_* error code, return a string equivalent. */
const char *
tor_tls_err_to_string(int err)
{
  if (err >= 0)
    return "[Not an error.]";
  switch (err) {
    case TOR_TLS_ERROR_MISC: return "misc error";
    case TOR_TLS_ERROR_IO: return "unexpected close";
    case TOR_TLS_ERROR_CONNREFUSED: return "connection refused";
    case TOR_TLS_ERROR_CONNRESET: return "connection reset";
    case TOR_TLS_ERROR_NO_ROUTE: return "host unreachable";
    case TOR_TLS_ERROR_TIMEOUT: return "connection timed out";
    case TOR_TLS_CLOSE: return "closed";
    case TOR_TLS_WANTREAD: return "want to read";
    case TOR_TLS_WANTWRITE: return "want to write";
    default: return "(unknown error code)";
  }
}

#define CATCH_SYSCALL 1
#define CATCH_ZERO    2

/** Given a TLS object and the result of an SSL_* call, use
 * SSL_get_error to determine whether an error has occurred, and if so
 * which one.  Return one of TOR_TLS_{DONE|WANTREAD|WANTWRITE|ERROR}.
 * If extra&CATCH_SYSCALL is true, return TOR_TLS_SYSCALL_ instead of
 * reporting syscall errors.  If extra&CATCH_ZERO is true, return
 * TOR_TLS_ZERORETURN_ instead of reporting zero-return errors.
 *
 * If an error has occurred, log it at level <b>severity</b> and describe the
 * current action as <b>doing</b>.
 */
static int
tor_tls_get_error(tor_tls_t *tls, int r, int extra,
                  const char *doing, int severity, int domain)
{
  int err = SSL_get_error(tls->ssl, r);
  int tor_error = TOR_TLS_ERROR_MISC;
  switch (err) {
    case SSL_ERROR_NONE:
      return TOR_TLS_DONE;
    case SSL_ERROR_WANT_READ:
      return TOR_TLS_WANTREAD;
    case SSL_ERROR_WANT_WRITE:
      return TOR_TLS_WANTWRITE;
    case SSL_ERROR_SYSCALL:
      if (extra&CATCH_SYSCALL)
        return TOR_TLS_SYSCALL_;
      if (r == 0) {
        tor_log(severity, LD_NET, "TLS error: unexpected close while %s (%s)",
            doing, SSL_state_string_long(tls->ssl));
        tor_error = TOR_TLS_ERROR_IO;
      } else {
        int e = tor_socket_errno(tls->socket);
        tor_log(severity, LD_NET,
            "TLS error: <syscall error while %s> (errno=%d: %s; state=%s)",
            doing, e, tor_socket_strerror(e),
            SSL_state_string_long(tls->ssl));
        tor_error = tor_errno_to_tls_error(e);
      }
      tls_log_errors(tls, severity, domain, doing);
      return tor_error;
    case SSL_ERROR_ZERO_RETURN:
      if (extra&CATCH_ZERO)
        return TOR_TLS_ZERORETURN_;
      tor_log(severity, LD_NET, "TLS connection closed while %s in state %s",
          doing, SSL_state_string_long(tls->ssl));
      tls_log_errors(tls, severity, domain, doing);
      return TOR_TLS_CLOSE;
    default:
      tls_log_errors(tls, severity, domain, doing);
      return TOR_TLS_ERROR_MISC;
  }
}

/** Initialize OpenSSL, unless it has already been initialized.
 */
static void
tor_tls_init(void)
{
  if (!tls_library_is_initialized) {
    long version;
    SSL_library_init();
    SSL_load_error_strings();

    version = SSLeay();

    /* OpenSSL 0.9.8l introduced SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION
     * here, but without thinking too hard about it: it turns out that the
     * flag in question needed to be set at the last minute, and that it
     * conflicted with an existing flag number that had already been added
     * in the OpenSSL 1.0.0 betas.  OpenSSL 0.9.8m thoughtfully replaced
     * the flag with an option and (it seems) broke anything that used
     * SSL3_FLAGS_* for the purpose.  So we need to know how to do both,
     * and we mustn't use the SSL3_FLAGS option with anything besides
     * OpenSSL 0.9.8l.
     *
     * No, we can't just set flag 0x0010 everywhere.  It breaks Tor with
     * OpenSSL 1.0.0beta3 and later.  On the other hand, we might be able to
     * set option 0x00040000L everywhere.
     *
     * No, we can't simply detect whether the flag or the option is present
     * in the headers at build-time: some vendors (notably Apple) like to
     * leave their headers out of sync with their libraries.
     *
     * Yes, it _is_ almost as if the OpenSSL developers decided that no
     * program should be allowed to use renegotiation unless it first passed
     * a test of intelligence and determination.
     */
    if (version > OPENSSL_V(0,9,8,'k') && version <= OPENSSL_V(0,9,8,'l')) {
      log_info(LD_GENERAL, "OpenSSL %s looks like version 0.9.8l, but "
                 "some vendors have backported renegotiation code from "
                 "0.9.8m without updating the version number. "
                 "I will try SSL3_FLAGS and SSL_OP to enable renegotation.",
                 SSLeay_version(SSLEAY_VERSION));
      use_unsafe_renegotiation_flag = 1;
      use_unsafe_renegotiation_op = 1;
    } else if (version > OPENSSL_V(0,9,8,'l')) {
      log_info(LD_GENERAL, "OpenSSL %s looks like version 0.9.8m or later; "
                 "I will try SSL_OP to enable renegotiation",
                 SSLeay_version(SSLEAY_VERSION));
      use_unsafe_renegotiation_op = 1;
    } else if (version <= OPENSSL_V(0,9,8,'k')) {
      log_info(LD_GENERAL, "OpenSSL %s [%lx] looks like it's older than "
                 "0.9.8l, but some vendors have backported 0.9.8l's "
                 "renegotiation code to earlier versions, and some have "
                 "backported the code from 0.9.8m or 0.9.8n.  I'll set both "
                 "SSL3_FLAGS and SSL_OP just to be safe.",
                 SSLeay_version(SSLEAY_VERSION), version);
      use_unsafe_renegotiation_flag = 1;
      use_unsafe_renegotiation_op = 1;
    } else {
      /* this is dead code, yes? */
      log_info(LD_GENERAL, "OpenSSL %s has version %lx",
               SSLeay_version(SSLEAY_VERSION), version);
    }

#if (SIZEOF_VOID_P >= 8 &&                              \
     !defined(OPENSSL_NO_EC) &&                         \
     OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,0,1))
    if (version >= OPENSSL_V_SERIES(1,0,1)) {
      /* Warn if we could *almost* be running with much faster ECDH.
         If we're built for a 64-bit target, using OpenSSL 1.0.1, but we
         don't have one of the built-in __uint128-based speedups, we are
         just one build operation away from an accelerated handshake.

         (We could be looking at OPENSSL_NO_EC_NISTP_64_GCC_128 instead of
          doing this test, but that gives compile-time options, not runtime
          behavior.)
      */
      EC_KEY *key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
      const EC_GROUP *g = key ? EC_KEY_get0_group(key) : NULL;
      const EC_METHOD *m = g ? EC_GROUP_method_of(g) : NULL;
      const int warn = (m == EC_GFp_simple_method() ||
                        m == EC_GFp_mont_method() ||
                        m == EC_GFp_nist_method());
      EC_KEY_free(key);

      if (warn)
        log_notice(LD_GENERAL, "We were built to run on a 64-bit CPU, with "
                   "OpenSSL 1.0.1 or later, but with a version of OpenSSL "
                   "that apparently lacks accelerated support for the NIST "
                   "P-224 and P-256 groups. Building openssl with such "
                   "support (using the enable-ec_nistp_64_gcc_128 option "
                   "when configuring it) would make ECDH much faster.");
    }
#endif

    tor_tls_allocate_tor_tls_object_ex_data_index();

    tls_library_is_initialized = 1;
  }
}

/** Free all global TLS structures. */
void
tor_tls_free_all(void)
{
  if (server_tls_context) {
    tor_tls_context_t *ctx = server_tls_context;
    server_tls_context = NULL;
    tor_tls_context_decref(ctx);
  }
  if (client_tls_context) {
    tor_tls_context_t *ctx = client_tls_context;
    client_tls_context = NULL;
    tor_tls_context_decref(ctx);
  }
#ifdef V2_HANDSHAKE_CLIENT
  if (CLIENT_CIPHER_DUMMIES)
    tor_free(CLIENT_CIPHER_DUMMIES);
  if (CLIENT_CIPHER_STACK)
    sk_SSL_CIPHER_free(CLIENT_CIPHER_STACK);
#endif
}

/** We need to give OpenSSL a callback to verify certificates. This is
 * it: We always accept peer certs and complete the handshake.  We
 * don't validate them until later.
 */
static int
always_accept_verify_cb(int preverify_ok,
                        X509_STORE_CTX *x509_ctx)
{
  (void) preverify_ok;
  (void) x509_ctx;
  return 1;
}

/** Return a newly allocated X509 name with commonName <b>cname</b>. */
static X509_NAME *
tor_x509_name_new(const char *cname)
{
  int nid;
  X509_NAME *name;
  if (!(name = X509_NAME_new()))
    return NULL;
  if ((nid = OBJ_txt2nid("commonName")) == NID_undef) goto error;
  if (!(X509_NAME_add_entry_by_NID(name, nid, MBSTRING_ASC,
                                   (unsigned char*)cname, -1, -1, 0)))
    goto error;
  return name;
 error:
  X509_NAME_free(name);
  return NULL;
}

/** Generate and sign an X509 certificate with the public key <b>rsa</b>,
 * signed by the private key <b>rsa_sign</b>.  The commonName of the
 * certificate will be <b>cname</b>; the commonName of the issuer will be
 * <b>cname_sign</b>. The cert will be valid for <b>cert_lifetime</b>
 * seconds, starting from some time in the past.
 *
 * Return a certificate on success, NULL on failure.
 */
static X509 *
tor_tls_create_certificate(crypto_pk_t *rsa,
                           crypto_pk_t *rsa_sign,
                           const char *cname,
                           const char *cname_sign,
                           unsigned int cert_lifetime)
{
  /* OpenSSL generates self-signed certificates with random 64-bit serial
   * numbers, so let's do that too. */
#define SERIAL_NUMBER_SIZE 8

  time_t start_time, end_time;
  BIGNUM *serial_number = NULL;
  unsigned char serial_tmp[SERIAL_NUMBER_SIZE];
  EVP_PKEY *sign_pkey = NULL, *pkey=NULL;
  X509 *x509 = NULL;
  X509_NAME *name = NULL, *name_issuer=NULL;

  tor_tls_init();

  /* Make sure we're part-way through the certificate lifetime, rather
   * than having it start right now. Don't choose quite uniformly, since
   * then we might pick a time where we're about to expire. Lastly, be
   * sure to start on a day boundary. */
  start_time = time(NULL) - crypto_rand_int(cert_lifetime) + 2*24*3600;
  start_time -= start_time % (24*3600);

  tor_assert(rsa);
  tor_assert(cname);
  tor_assert(rsa_sign);
  tor_assert(cname_sign);
  if (!(sign_pkey = crypto_pk_get_evp_pkey_(rsa_sign,1)))
    goto error;
  if (!(pkey = crypto_pk_get_evp_pkey_(rsa,0)))
    goto error;
  if (!(x509 = X509_new()))
    goto error;
  if (!(X509_set_version(x509, 2)))
    goto error;

  { /* our serial number is 8 random bytes. */
    if (crypto_rand((char *)serial_tmp, sizeof(serial_tmp)) < 0)
      goto error;
    if (!(serial_number = BN_bin2bn(serial_tmp, sizeof(serial_tmp), NULL)))
      goto error;
    if (!(BN_to_ASN1_INTEGER(serial_number, X509_get_serialNumber(x509))))
      goto error;
  }

  if (!(name = tor_x509_name_new(cname)))
    goto error;
  if (!(X509_set_subject_name(x509, name)))
    goto error;
  if (!(name_issuer = tor_x509_name_new(cname_sign)))
    goto error;
  if (!(X509_set_issuer_name(x509, name_issuer)))
    goto error;

  if (!X509_time_adj(X509_get_notBefore(x509),0,&start_time))
    goto error;
  end_time = start_time + cert_lifetime;
  if (!X509_time_adj(X509_get_notAfter(x509),0,&end_time))
    goto error;
  if (!X509_set_pubkey(x509, pkey))
    goto error;
  if (!X509_sign(x509, sign_pkey, EVP_sha1()))
    goto error;

  goto done;
 error:
  if (x509) {
    X509_free(x509);
    x509 = NULL;
  }
 done:
  tls_log_errors(NULL, LOG_WARN, LD_NET, "generating certificate");
  if (sign_pkey)
    EVP_PKEY_free(sign_pkey);
  if (pkey)
    EVP_PKEY_free(pkey);
  if (serial_number)
    BN_clear_free(serial_number);
  if (name)
    X509_NAME_free(name);
  if (name_issuer)
    X509_NAME_free(name_issuer);
  return x509;

#undef SERIAL_NUMBER_SIZE
}

/** List of ciphers that servers should select from when the client might be
 * claiming extra unsupported ciphers in order to avoid fingerprinting.  */
#define SERVER_CIPHER_LIST                         \
  (TLS1_TXT_DHE_RSA_WITH_AES_256_SHA ":"           \
   TLS1_TXT_DHE_RSA_WITH_AES_128_SHA ":"           \
   SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA)

/** List of ciphers that servers should select from when we actually have
 * our choice of what cipher to use. */
const char UNRESTRICTED_SERVER_CIPHER_LIST[] =
  /* This list is autogenerated with the gen_server_ciphers.py script;
   * don't hand-edit it. */
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_256_GCM_SHA384
       TLS1_TXT_ECDHE_RSA_WITH_AES_256_GCM_SHA384 ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256
       TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256 ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_256_SHA384
       TLS1_TXT_ECDHE_RSA_WITH_AES_256_SHA384 ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_128_SHA256
       TLS1_TXT_ECDHE_RSA_WITH_AES_128_SHA256 ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA
       TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA ":"
#endif
#ifdef TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA
       TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_256_GCM_SHA384
       TLS1_TXT_DHE_RSA_WITH_AES_256_GCM_SHA384 ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_128_GCM_SHA256
       TLS1_TXT_DHE_RSA_WITH_AES_128_GCM_SHA256 ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_256_SHA256
       TLS1_TXT_DHE_RSA_WITH_AES_256_SHA256 ":"
#endif
#ifdef TLS1_TXT_DHE_RSA_WITH_AES_128_SHA256
       TLS1_TXT_DHE_RSA_WITH_AES_128_SHA256 ":"
#endif
       /* Required */
       TLS1_TXT_DHE_RSA_WITH_AES_256_SHA ":"
       /* Required */
       TLS1_TXT_DHE_RSA_WITH_AES_128_SHA ":"
#ifdef TLS1_TXT_ECDHE_RSA_WITH_DES_192_CBC3_SHA
       TLS1_TXT_ECDHE_RSA_WITH_DES_192_CBC3_SHA ":"
#endif
       /* Required */
       SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA;

/* Note: to set up your own private testing network with link crypto
 * disabled, set your Tors' cipher list to
 * (SSL3_TXT_RSA_NULL_SHA).  If you do this, you won't be able to communicate
 * with any of the "real" Tors, though. */

#ifdef V2_HANDSHAKE_CLIENT
#define CIPHER(id, name) name ":"
#define XCIPHER(id, name)
/** List of ciphers that clients should advertise, omitting items that
 * our OpenSSL doesn't know about. */
static const char CLIENT_CIPHER_LIST[] =
#include "./ciphers.inc"
  /* Tell it not to use SSLv2 ciphers, so that it can select an SSLv3 version
   * of any cipher we say. */
  "!SSLv2"
  ;
#undef CIPHER
#undef XCIPHER

/** Holds a cipher that we want to advertise, and its 2-byte ID. */
typedef struct cipher_info_t { unsigned id; const char *name; } cipher_info_t;
/** A list of all the ciphers that clients should advertise, including items
 * that OpenSSL might not know about. */
static const cipher_info_t CLIENT_CIPHER_INFO_LIST[] = {
#define CIPHER(id, name) { id, name },
#define XCIPHER(id, name) { id, #name },
#include "./ciphers.inc"
#undef CIPHER
#undef XCIPHER
};

/** The length of CLIENT_CIPHER_INFO_LIST and CLIENT_CIPHER_DUMMIES. */
static const int N_CLIENT_CIPHERS =
  sizeof(CLIENT_CIPHER_INFO_LIST)/sizeof(CLIENT_CIPHER_INFO_LIST[0]);
#endif

#ifndef V2_HANDSHAKE_CLIENT
#undef CLIENT_CIPHER_LIST
#define CLIENT_CIPHER_LIST  (TLS1_TXT_DHE_RSA_WITH_AES_128_SHA ":"      \
                             SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA)
#endif

/** Free all storage held in <b>cert</b> */
void
tor_cert_free(tor_cert_t *cert)
{
  if (! cert)
    return;
  if (cert->cert)
    X509_free(cert->cert);
  tor_free(cert->encoded);
  memwipe(cert, 0x03, sizeof(*cert));
  tor_free(cert);
}

/**
 * Allocate a new tor_cert_t to hold the certificate "x509_cert".
 *
 * Steals a reference to x509_cert.
 */
static tor_cert_t *
tor_cert_new(X509 *x509_cert)
{
  tor_cert_t *cert;
  EVP_PKEY *pkey;
  RSA *rsa;
  int length;
  unsigned char *buf = NULL;

  if (!x509_cert)
    return NULL;

  length = i2d_X509(x509_cert, &buf);
  cert = tor_malloc_zero(sizeof(tor_cert_t));
  if (length <= 0 || buf == NULL) {
    tor_free(cert);
    log_err(LD_CRYPTO, "Couldn't get length of encoded x509 certificate");
    X509_free(x509_cert);
    return NULL;
  }
  cert->encoded_len = (size_t) length;
  cert->encoded = tor_malloc(length);
  memcpy(cert->encoded, buf, length);
  OPENSSL_free(buf);

  cert->cert = x509_cert;

  crypto_digest_all(&cert->cert_digests,
                    (char*)cert->encoded, cert->encoded_len);

  if ((pkey = X509_get_pubkey(x509_cert)) &&
      (rsa = EVP_PKEY_get1_RSA(pkey))) {
    crypto_pk_t *pk = crypto_new_pk_from_rsa_(rsa);
    crypto_pk_get_all_digests(pk, &cert->pkey_digests);
    cert->pkey_digests_set = 1;
    crypto_pk_free(pk);
    EVP_PKEY_free(pkey);
  }

  return cert;
}

/** Read a DER-encoded X509 cert, of length exactly <b>certificate_len</b>,
 * from a <b>certificate</b>.  Return a newly allocated tor_cert_t on success
 * and NULL on failure. */
tor_cert_t *
tor_cert_decode(const uint8_t *certificate, size_t certificate_len)
{
  X509 *x509;
  const unsigned char *cp = (const unsigned char *)certificate;
  tor_cert_t *newcert;
  tor_assert(certificate);

  if (certificate_len > INT_MAX)
    return NULL;

  x509 = d2i_X509(NULL, &cp, (int)certificate_len);

  if (!x509)
    return NULL; /* Couldn't decode */
  if (cp - certificate != (int)certificate_len) {
    X509_free(x509);
    return NULL; /* Didn't use all the bytes */
  }
  newcert = tor_cert_new(x509);
  if (!newcert) {
    return NULL;
  }
  if (newcert->encoded_len != certificate_len ||
      fast_memneq(newcert->encoded, certificate, certificate_len)) {
    /* Cert wasn't in DER */
    tor_cert_free(newcert);
    return NULL;
  }
  return newcert;
}

/** Set *<b>encoded_out</b> and *<b>size_out</b> to <b>cert</b>'s encoded DER
 * representation and length, respectively. */
void
tor_cert_get_der(const tor_cert_t *cert,
                 const uint8_t **encoded_out, size_t *size_out)
{
  tor_assert(cert);
  tor_assert(encoded_out);
  tor_assert(size_out);
  *encoded_out = cert->encoded;
  *size_out = cert->encoded_len;
}

/** Return a set of digests for the public key in <b>cert</b>, or NULL if this
 * cert's public key is not one we know how to take the digest of. */
const digests_t *
tor_cert_get_id_digests(const tor_cert_t *cert)
{
  if (cert->pkey_digests_set)
    return &cert->pkey_digests;
  else
    return NULL;
}

/** Return a set of digests for the public key in <b>cert</b>. */
const digests_t *
tor_cert_get_cert_digests(const tor_cert_t *cert)
{
  return &cert->cert_digests;
}

/** Remove a reference to <b>ctx</b>, and free it if it has no more
 * references. */
static void
tor_tls_context_decref(tor_tls_context_t *ctx)
{
  tor_assert(ctx);
  if (--ctx->refcnt == 0) {
    SSL_CTX_free(ctx->ctx);
    tor_cert_free(ctx->my_link_cert);
    tor_cert_free(ctx->my_id_cert);
    tor_cert_free(ctx->my_auth_cert);
    crypto_pk_free(ctx->link_key);
    crypto_pk_free(ctx->auth_key);
    tor_free(ctx);
  }
}

/** Set *<b>link_cert_out</b> and *<b>id_cert_out</b> to the link certificate
 * and ID certificate that we're currently using for our V3 in-protocol
 * handshake's certificate chain.  If <b>server</b> is true, provide the certs
 * that we use in server mode; otherwise, provide the certs that we use in
 * client mode. */
int
tor_tls_get_my_certs(int server,
                     const tor_cert_t **link_cert_out,
                     const tor_cert_t **id_cert_out)
{
  tor_tls_context_t *ctx = server ? server_tls_context : client_tls_context;
  if (! ctx)
    return -1;
  if (link_cert_out)
    *link_cert_out = server ? ctx->my_link_cert : ctx->my_auth_cert;
  if (id_cert_out)
    *id_cert_out = ctx->my_id_cert;
  return 0;
}

/**
 * Return the authentication key that we use to authenticate ourselves as a
 * client in the V3 in-protocol handshake.
 */
crypto_pk_t *
tor_tls_get_my_client_auth_key(void)
{
  if (! client_tls_context)
    return NULL;
  return client_tls_context->auth_key;
}

/**
 * Return a newly allocated copy of the public key that a certificate
 * certifies.  Return NULL if the cert's key is not RSA.
 */
crypto_pk_t *
tor_tls_cert_get_key(tor_cert_t *cert)
{
  crypto_pk_t *result = NULL;
  EVP_PKEY *pkey = X509_get_pubkey(cert->cert);
  RSA *rsa;
  if (!pkey)
    return NULL;
  rsa = EVP_PKEY_get1_RSA(pkey);
  if (!rsa) {
    EVP_PKEY_free(pkey);
    return NULL;
  }
  result = crypto_new_pk_from_rsa_(rsa);
  EVP_PKEY_free(pkey);
  return result;
}

/** Return true iff the other side of <b>tls</b> has authenticated to us, and
 * the key certified in <b>cert</b> is the same as the key they used to do it.
 */
int
tor_tls_cert_matches_key(const tor_tls_t *tls, const tor_cert_t *cert)
{
  X509 *peercert = SSL_get_peer_certificate(tls->ssl);
  EVP_PKEY *link_key = NULL, *cert_key = NULL;
  int result;

  if (!peercert)
    return 0;
  link_key = X509_get_pubkey(peercert);
  cert_key = X509_get_pubkey(cert->cert);

  result = link_key && cert_key && EVP_PKEY_cmp(cert_key, link_key) == 1;

  X509_free(peercert);
  if (link_key)
    EVP_PKEY_free(link_key);
  if (cert_key)
    EVP_PKEY_free(cert_key);

  return result;
}

/** Check whether <b>cert</b> is well-formed, currently live, and correctly
 * signed by the public key in <b>signing_cert</b>.  If <b>check_rsa_1024</b>,
 * make sure that it has an RSA key with 1024 bits; otherwise, just check that
 * the key is long enough. Return 1 if the cert is good, and 0 if it's bad or
 * we couldn't check it. */
int
tor_tls_cert_is_valid(int severity,
                      const tor_cert_t *cert,
                      const tor_cert_t *signing_cert,
                      int check_rsa_1024)
{
  EVP_PKEY *cert_key;
  EVP_PKEY *signing_key = X509_get_pubkey(signing_cert->cert);
  int r, key_ok = 0;
  if (!signing_key)
    return 0;
  r = X509_verify(cert->cert, signing_key);
  EVP_PKEY_free(signing_key);
  if (r <= 0)
    return 0;

  /* okay, the signature checked out right.  Now let's check the check the
   * lifetime. */
  if (check_cert_lifetime_internal(severity, cert->cert,
                                   48*60*60, 30*24*60*60) < 0)
    return 0;

  cert_key = X509_get_pubkey(cert->cert);
  if (check_rsa_1024 && cert_key) {
    RSA *rsa = EVP_PKEY_get1_RSA(cert_key);
    if (rsa && BN_num_bits(rsa->n) == 1024)
      key_ok = 1;
    if (rsa)
      RSA_free(rsa);
  } else if (cert_key) {
    int min_bits = 1024;
#ifdef EVP_PKEY_EC
    if (EVP_PKEY_type(cert_key->type) == EVP_PKEY_EC)
      min_bits = 128;
#endif
    if (EVP_PKEY_bits(cert_key) >= min_bits)
      key_ok = 1;
  }
  EVP_PKEY_free(cert_key);
  if (!key_ok)
    return 0;

  /* XXXX compare DNs or anything? */

  return 1;
}

/** Increase the reference count of <b>ctx</b>. */
static void
tor_tls_context_incref(tor_tls_context_t *ctx)
{
  ++ctx->refcnt;
}

/** Create new global client and server TLS contexts.
 *
 * If <b>server_identity</b> is NULL, this will not generate a server
 * TLS context. If TOR_TLS_CTX_IS_PUBLIC_SERVER is set in <b>flags</b>, use
 * the same TLS context for incoming and outgoing connections, and
 * ignore <b>client_identity</b>. If one of TOR_TLS_CTX_USE_ECDHE_P{224,256}
 * is set in <b>flags</b>, use that ECDHE group if possible; otherwise use
 * the default ECDHE group. */
int
tor_tls_context_init(unsigned flags,
                     crypto_pk_t *client_identity,
                     crypto_pk_t *server_identity,
                     unsigned int key_lifetime)
{
  int rv1 = 0;
  int rv2 = 0;
  const int is_public_server = flags & TOR_TLS_CTX_IS_PUBLIC_SERVER;

  if (is_public_server) {
    tor_tls_context_t *new_ctx;
    tor_tls_context_t *old_ctx;

    tor_assert(server_identity != NULL);

    rv1 = tor_tls_context_init_one(&server_tls_context,
                                   server_identity,
                                   key_lifetime, flags, 0);

    if (rv1 >= 0) {
      new_ctx = server_tls_context;
      tor_tls_context_incref(new_ctx);
      old_ctx = client_tls_context;
      client_tls_context = new_ctx;

      if (old_ctx != NULL) {
        tor_tls_context_decref(old_ctx);
      }
    }
  } else {
    if (server_identity != NULL) {
      rv1 = tor_tls_context_init_one(&server_tls_context,
                                     server_identity,
                                     key_lifetime,
                                     flags,
                                     0);
    } else {
      tor_tls_context_t *old_ctx = server_tls_context;
      server_tls_context = NULL;

      if (old_ctx != NULL) {
        tor_tls_context_decref(old_ctx);
      }
    }

    rv2 = tor_tls_context_init_one(&client_tls_context,
                                   client_identity,
                                   key_lifetime,
                                   flags,
                                   1);
  }

  return MIN(rv1, rv2);
}

/** Create a new global TLS context.
 *
 * You can call this function multiple times.  Each time you call it,
 * it generates new certificates; all new connections will use
 * the new SSL context.
 */
static int
tor_tls_context_init_one(tor_tls_context_t **ppcontext,
                         crypto_pk_t *identity,
                         unsigned int key_lifetime,
                         unsigned int flags,
                         int is_client)
{
  tor_tls_context_t *new_ctx = tor_tls_context_new(identity,
                                                   key_lifetime,
                                                   flags,
                                                   is_client);
  tor_tls_context_t *old_ctx = *ppcontext;

  if (new_ctx != NULL) {
    *ppcontext = new_ctx;

    /* Free the old context if one existed. */
    if (old_ctx != NULL) {
      /* This is safe even if there are open connections: we reference-
       * count tor_tls_context_t objects. */
      tor_tls_context_decref(old_ctx);
    }
  }

  return ((new_ctx != NULL) ? 0 : -1);
}

/** Create a new TLS context for use with Tor TLS handshakes.
 * <b>identity</b> should be set to the identity key used to sign the
 * certificate.
 */
static tor_tls_context_t *
tor_tls_context_new(crypto_pk_t *identity, unsigned int key_lifetime,
                    unsigned flags, int is_client)
{
  crypto_pk_t *rsa = NULL, *rsa_auth = NULL;
  EVP_PKEY *pkey = NULL;
  tor_tls_context_t *result = NULL;
  X509 *cert = NULL, *idcert = NULL, *authcert = NULL;
  char *nickname = NULL, *nn2 = NULL;

  tor_tls_init();
  nickname = crypto_random_hostname(8, 20, "www.", ".net");
#ifdef DISABLE_V3_LINKPROTO_SERVERSIDE
  nn2 = crypto_random_hostname(8, 20, "www.", ".net");
#else
  nn2 = crypto_random_hostname(8, 20, "www.", ".com");
#endif

  /* Generate short-term RSA key for use with TLS. */
  if (!(rsa = crypto_pk_new()))
    goto error;
  if (crypto_pk_generate_key(rsa)<0)
    goto error;
  if (!is_client) {
    /* Generate short-term RSA key for use in the in-protocol ("v3")
     * authentication handshake. */
    if (!(rsa_auth = crypto_pk_new()))
      goto error;
    if (crypto_pk_generate_key(rsa_auth)<0)
      goto error;
    /* Create a link certificate signed by identity key. */
    cert = tor_tls_create_certificate(rsa, identity, nickname, nn2,
                                      key_lifetime);
    /* Create self-signed certificate for identity key. */
    idcert = tor_tls_create_certificate(identity, identity, nn2, nn2,
                                        IDENTITY_CERT_LIFETIME);
    /* Create an authentication certificate signed by identity key. */
    authcert = tor_tls_create_certificate(rsa_auth, identity, nickname, nn2,
                                          key_lifetime);
    if (!cert || !idcert || !authcert) {
      log_warn(LD_CRYPTO, "Error creating certificate");
      goto error;
    }
  }

  result = tor_malloc_zero(sizeof(tor_tls_context_t));
  result->refcnt = 1;
  if (!is_client) {
    result->my_link_cert = tor_cert_new(X509_dup(cert));
    result->my_id_cert = tor_cert_new(X509_dup(idcert));
    result->my_auth_cert = tor_cert_new(X509_dup(authcert));
    if (!result->my_link_cert || !result->my_id_cert || !result->my_auth_cert)
      goto error;
    result->link_key = crypto_pk_dup_key(rsa);
    result->auth_key = crypto_pk_dup_key(rsa_auth);
  }

#if 0
  /* Tell OpenSSL to only use TLS1.  This may have subtly different results
   * from SSLv23_method() with SSLv2 and SSLv3 disabled, so we need to do some
   * investigation before we consider adjusting it. It should be compatible
   * with existing Tors. */
  if (!(result->ctx = SSL_CTX_new(TLSv1_method())))
    goto error;
#endif

  /* Tell OpenSSL to use TLS 1.0 or later but not SSL2 or SSL3. */
  if (!(result->ctx = SSL_CTX_new(SSLv23_method())))
    goto error;
  SSL_CTX_set_options(result->ctx, SSL_OP_NO_SSLv2);
  SSL_CTX_set_options(result->ctx, SSL_OP_NO_SSLv3);

  /* Prefer the server's ordering of ciphers: the client's ordering has
  * historically been chosen for fingerprinting resistance. */
  SSL_CTX_set_options(result->ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);

  /* Disable TLS1.1 and TLS1.2 if they exist.  We need to do this to
   * workaround a bug present in all OpenSSL 1.0.1 versions (as of 1
   * June 2012), wherein renegotiating while using one of these TLS
   * protocols will cause the client to send a TLS 1.0 ServerHello
   * rather than a ServerHello written with the appropriate protocol
   * version.  Once some version of OpenSSL does TLS1.1 and TLS1.2
   * renegotiation properly, we can turn them back on when built with
   * that version. */
#if OPENSSL_VERSION_NUMBER < OPENSSL_V(1,0,1,'e')
#ifdef SSL_OP_NO_TLSv1_2
  SSL_CTX_set_options(result->ctx, SSL_OP_NO_TLSv1_2);
#endif
#ifdef SSL_OP_NO_TLSv1_1
  SSL_CTX_set_options(result->ctx, SSL_OP_NO_TLSv1_1);
#endif
#endif

  /* Disable TLS tickets if they're supported.  We never want to use them;
   * using them can make our perfect forward secrecy a little worse, *and*
   * create an opportunity to fingerprint us (since it's unusual to use them
   * with TLS sessions turned off).
   *
   * In 0.2.4, clients advertise support for them though, to avoid a TLS
   * distinguishability vector.  This can give us worse PFS, though, if we
   * get a server that doesn't set SSL_OP_NO_TICKET.  With luck, there will
   * be few such servers by the time 0.2.4 is more stable.
   */
#ifdef SSL_OP_NO_TICKET
  if (! is_client) {
    SSL_CTX_set_options(result->ctx, SSL_OP_NO_TICKET);
  }
#endif

  /* XXX This block is now obsolete. */
  if (
#ifdef DISABLE_SSL3_HANDSHAKE
      1 ||
#endif
      SSLeay()  <  OPENSSL_V(0,9,8,'s') ||
      (SSLeay() >= OPENSSL_V_SERIES(0,9,9) &&
       SSLeay() <  OPENSSL_V(1,0,0,'f'))) {
    /* And not SSL3 if it's subject to CVE-2011-4576. */
    log_info(LD_NET, "Disabling SSLv3 because this OpenSSL version "
             "might otherwise be vulnerable to CVE-2011-4576 "
             "(compile-time version %08lx (%s); "
             "runtime version %08lx (%s))",
             (unsigned long)OPENSSL_VERSION_NUMBER, OPENSSL_VERSION_TEXT,
             (unsigned long)SSLeay(), SSLeay_version(SSLEAY_VERSION));
    SSL_CTX_set_options(result->ctx, SSL_OP_NO_SSLv3);
  }

  SSL_CTX_set_options(result->ctx, SSL_OP_SINGLE_DH_USE);
  SSL_CTX_set_options(result->ctx, SSL_OP_SINGLE_ECDH_USE);

#ifdef SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION
  SSL_CTX_set_options(result->ctx,
                      SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#endif
  /* Yes, we know what we are doing here.  No, we do not treat a renegotiation
   * as authenticating any earlier-received data.
   */
  if (use_unsafe_renegotiation_op) {
    SSL_CTX_set_options(result->ctx,
                        SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);
  }
#ifndef OPENSSL_NO_COMP
  /* Don't actually allow compression; it uses ram and time, but the data
   * we transmit is all encrypted anyway. */
  if (result->ctx->comp_methods)
    result->ctx->comp_methods = NULL;
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
  SSL_CTX_set_mode(result->ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
  if (! is_client) {
    if (cert && !SSL_CTX_use_certificate(result->ctx,cert))
      goto error;
    X509_free(cert); /* We just added a reference to cert. */
    cert=NULL;
    if (idcert) {
      X509_STORE *s = SSL_CTX_get_cert_store(result->ctx);
      tor_assert(s);
      X509_STORE_add_cert(s, idcert);
      X509_free(idcert); /* The context now owns the reference to idcert */
      idcert = NULL;
    }
  }
  SSL_CTX_set_session_cache_mode(result->ctx, SSL_SESS_CACHE_OFF);
  if (!is_client) {
    tor_assert(rsa);
    if (!(pkey = crypto_pk_get_evp_pkey_(rsa,1)))
      goto error;
    if (!SSL_CTX_use_PrivateKey(result->ctx, pkey))
      goto error;
    EVP_PKEY_free(pkey);
    pkey = NULL;
    if (!SSL_CTX_check_private_key(result->ctx))
      goto error;
  }
  {
    crypto_dh_t *dh = crypto_dh_new(DH_TYPE_TLS);
    tor_assert(dh);
    SSL_CTX_set_tmp_dh(result->ctx, crypto_dh_get_dh_(dh));
    crypto_dh_free(dh);
  }
#if (!defined(OPENSSL_NO_EC) &&                         \
     OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,0,0))
  if (! is_client) {
    int nid;
    EC_KEY *ec_key;
    if (flags & TOR_TLS_CTX_USE_ECDHE_P224)
      nid = NID_secp224r1;
    else if (flags & TOR_TLS_CTX_USE_ECDHE_P256)
      nid = NID_X9_62_prime256v1;
    else
      nid = NID_X9_62_prime256v1;
    /* Use P-256 for ECDHE. */
    ec_key = EC_KEY_new_by_curve_name(nid);
    if (ec_key != NULL) /*XXXX Handle errors? */
      SSL_CTX_set_tmp_ecdh(result->ctx, ec_key);
    EC_KEY_free(ec_key);
  }
#else
  (void)flags;
#endif
  SSL_CTX_set_verify(result->ctx, SSL_VERIFY_PEER,
                     always_accept_verify_cb);
  /* let us realloc bufs that we're writing from */
  SSL_CTX_set_mode(result->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  if (rsa)
    crypto_pk_free(rsa);
  if (rsa_auth)
    crypto_pk_free(rsa_auth);
  X509_free(authcert);
  tor_free(nickname);
  tor_free(nn2);
  return result;

 error:
  tls_log_errors(NULL, LOG_WARN, LD_NET, "creating TLS context");
  tor_free(nickname);
  tor_free(nn2);
  if (pkey)
    EVP_PKEY_free(pkey);
  if (rsa)
    crypto_pk_free(rsa);
  if (rsa_auth)
    crypto_pk_free(rsa_auth);
  if (result)
    tor_tls_context_decref(result);
  if (cert)
    X509_free(cert);
  if (idcert)
    X509_free(idcert);
  if (authcert)
    X509_free(authcert);
  return NULL;
}

/** Invoked when a TLS state changes: log the change at severity 'debug' */
static void
tor_tls_debug_state_callback(const SSL *ssl, int type, int val)
{
  log_debug(LD_HANDSHAKE, "SSL %p is now in state %s [type=%d,val=%d].",
            ssl, SSL_state_string_long(ssl), type, val);
}

/* Return the name of the negotiated ciphersuite in use on <b>tls</b> */
const char *
tor_tls_get_ciphersuite_name(tor_tls_t *tls)
{
  return SSL_get_cipher(tls->ssl);
}

#ifdef V2_HANDSHAKE_SERVER

/* Here's the old V2 cipher list we sent from 0.2.1.1-alpha up to
 * 0.2.3.17-beta. If a client is using this list, we can't believe the ciphers
 * that it claims to support.  We'll prune this list to remove the ciphers
 * *we* don't recognize. */
static uint16_t v2_cipher_list[] = {
  0xc00a, /* TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CBC_SHA */
  0xc014, /* TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA */
  0x0039, /* TLS1_TXT_DHE_RSA_WITH_AES_256_SHA */
  0x0038, /* TLS1_TXT_DHE_DSS_WITH_AES_256_SHA */
  0xc00f, /* TLS1_TXT_ECDH_RSA_WITH_AES_256_CBC_SHA */
  0xc005, /* TLS1_TXT_ECDH_ECDSA_WITH_AES_256_CBC_SHA */
  0x0035, /* TLS1_TXT_RSA_WITH_AES_256_SHA */
  0xc007, /* TLS1_TXT_ECDHE_ECDSA_WITH_RC4_128_SHA */
  0xc009, /* TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CBC_SHA */
  0xc011, /* TLS1_TXT_ECDHE_RSA_WITH_RC4_128_SHA */
  0xc013, /* TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA */
  0x0033, /* TLS1_TXT_DHE_RSA_WITH_AES_128_SHA */
  0x0032, /* TLS1_TXT_DHE_DSS_WITH_AES_128_SHA */
  0xc00c, /* TLS1_TXT_ECDH_RSA_WITH_RC4_128_SHA */
  0xc00e, /* TLS1_TXT_ECDH_RSA_WITH_AES_128_CBC_SHA */
  0xc002, /* TLS1_TXT_ECDH_ECDSA_WITH_RC4_128_SHA */
  0xc004, /* TLS1_TXT_ECDH_ECDSA_WITH_AES_128_CBC_SHA */
  0x0004, /* SSL3_TXT_RSA_RC4_128_MD5 */
  0x0005, /* SSL3_TXT_RSA_RC4_128_SHA */
  0x002f, /* TLS1_TXT_RSA_WITH_AES_128_SHA */
  0xc008, /* TLS1_TXT_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA */
  0xc012, /* TLS1_TXT_ECDHE_RSA_WITH_DES_192_CBC3_SHA */
  0x0016, /* SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA */
  0x0013, /* SSL3_TXT_EDH_DSS_DES_192_CBC3_SHA */
  0xc00d, /* TLS1_TXT_ECDH_RSA_WITH_DES_192_CBC3_SHA */
  0xc003, /* TLS1_TXT_ECDH_ECDSA_WITH_DES_192_CBC3_SHA */
  0xfeff, /* SSL3_TXT_RSA_FIPS_WITH_3DES_EDE_CBC_SHA */
  0x000a, /* SSL3_TXT_RSA_DES_192_CBC3_SHA */
  0
};
/** Have we removed the unrecognized ciphers from v2_cipher_list yet? */
static int v2_cipher_list_pruned = 0;

/** Return 0 if <b>m</b> does not support the cipher with ID <b>cipher</b>;
 * return 1 if it does support it, or if we have no way to tell. */
static int
find_cipher_by_id(const SSL_METHOD *m, uint16_t cipher)
{
  const SSL_CIPHER *c;
#ifdef HAVE_STRUCT_SSL_METHOD_ST_GET_CIPHER_BY_CHAR
  if (m && m->get_cipher_by_char) {
    unsigned char cipherid[3];
    set_uint16(cipherid, htons(cipher));
    cipherid[2] = 0; /* If ssl23_get_cipher_by_char finds no cipher starting
                      * with a two-byte 'cipherid', it may look for a v2
                      * cipher with the appropriate 3 bytes. */
    c = m->get_cipher_by_char(cipherid);
    if (c)
      tor_assert((c->id & 0xffff) == cipher);
    return c != NULL;
  } else
#endif
  if (m && m->get_cipher && m->num_ciphers) {
    /* It would seem that some of the "let's-clean-up-openssl" forks have
     * removed the get_cipher_by_char function.  Okay, so now you get a
     * quadratic search.
     */
    int i;
    for (i = 0; i < m->num_ciphers(); ++i) {
      c = m->get_cipher(i);
      if (c && (c->id & 0xffff) == cipher) {
        return 1;
      }
    }
    return 0;
  } else {
    return 1; /* No way to search */
  }
}

/** Remove from v2_cipher_list every cipher that we don't support, so that
 * comparing v2_cipher_list to a client's cipher list will give a sensible
 * result. */
static void
prune_v2_cipher_list(void)
{
  uint16_t *inp, *outp;
  const SSL_METHOD *m = SSLv23_method();

  inp = outp = v2_cipher_list;
  while (*inp) {
    if (find_cipher_by_id(m, *inp)) {
      *outp++ = *inp++;
    } else {
      inp++;
    }
  }
  *outp = 0;

  v2_cipher_list_pruned = 1;
}

/** Examine the client cipher list in <b>ssl</b>, and determine what kind of
 * client it is.  Return one of CIPHERS_ERR, CIPHERS_V1, CIPHERS_V2,
 * CIPHERS_UNRESTRICTED.
 **/
static int
tor_tls_classify_client_ciphers(const SSL *ssl,
                                STACK_OF(SSL_CIPHER) *peer_ciphers)
{
  int i, res;
  tor_tls_t *tor_tls;
  if (PREDICT_UNLIKELY(!v2_cipher_list_pruned))
    prune_v2_cipher_list();

  tor_tls = tor_tls_get_by_ssl(ssl);
  if (tor_tls && tor_tls->client_cipher_list_type)
    return tor_tls->client_cipher_list_type;

  /* If we reached this point, we just got a client hello.  See if there is
   * a cipher list. */
  if (!peer_ciphers) {
    log_info(LD_NET, "No ciphers on session");
    res = CIPHERS_ERR;
    goto done;
  }
  /* Now we need to see if there are any ciphers whose presence means we're
   * dealing with an updated Tor. */
  for (i = 0; i < sk_SSL_CIPHER_num(peer_ciphers); ++i) {
    SSL_CIPHER *cipher = sk_SSL_CIPHER_value(peer_ciphers, i);
    const char *ciphername = SSL_CIPHER_get_name(cipher);
    if (strcmp(ciphername, TLS1_TXT_DHE_RSA_WITH_AES_128_SHA) &&
        strcmp(ciphername, TLS1_TXT_DHE_RSA_WITH_AES_256_SHA) &&
        strcmp(ciphername, SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA) &&
        strcmp(ciphername, "(NONE)")) {
      log_debug(LD_NET, "Got a non-version-1 cipher called '%s'", ciphername);
      // return 1;
      goto v2_or_higher;
    }
  }
  res = CIPHERS_V1;
  goto done;
 v2_or_higher:
  {
    const uint16_t *v2_cipher = v2_cipher_list;
    for (i = 0; i < sk_SSL_CIPHER_num(peer_ciphers); ++i) {
      SSL_CIPHER *cipher = sk_SSL_CIPHER_value(peer_ciphers, i);
      uint16_t id = cipher->id & 0xffff;
      if (id == 0x00ff) /* extended renegotiation indicator. */
        continue;
      if (!id || id != *v2_cipher) {
        res = CIPHERS_UNRESTRICTED;
        goto dump_ciphers;
      }
      ++v2_cipher;
    }
    if (*v2_cipher != 0) {
      res = CIPHERS_UNRESTRICTED;
      goto dump_ciphers;
    }
    res = CIPHERS_V2;
  }

 dump_ciphers:
  {
    smartlist_t *elts = smartlist_new();
    char *s;
    for (i = 0; i < sk_SSL_CIPHER_num(peer_ciphers); ++i) {
      SSL_CIPHER *cipher = sk_SSL_CIPHER_value(peer_ciphers, i);
      const char *ciphername = SSL_CIPHER_get_name(cipher);
      smartlist_add(elts, (char*)ciphername);
    }
    s = smartlist_join_strings(elts, ":", 0, NULL);
    log_debug(LD_NET, "Got a %s V2/V3 cipher list from %s.  It is: '%s'",
              (res == CIPHERS_V2) ? "fictitious" : "real", ADDR(tor_tls), s);
    tor_free(s);
    smartlist_free(elts);
  }
 done:
  if (tor_tls)
    return tor_tls->client_cipher_list_type = res;

  return res;
}

/** Return true iff the cipher list suggested by the client for <b>ssl</b> is
 * a list that indicates that the client knows how to do the v2 TLS connection
 * handshake. */
static int
tor_tls_client_is_using_v2_ciphers(const SSL *ssl)
{
  SSL_SESSION *session;
  if (!(session = SSL_get_session((SSL *)ssl))) {
    log_info(LD_NET, "No session on TLS?");
    return CIPHERS_ERR;
  }

  return tor_tls_classify_client_ciphers(ssl, session->ciphers) >= CIPHERS_V2;
}

/** Invoked when we're accepting a connection on <b>ssl</b>, and the connection
 * changes state. We use this:
 * <ul><li>To alter the state of the handshake partway through, so we
 *         do not send or request extra certificates in v2 handshakes.</li>
 * <li>To detect renegotiation</li></ul>
 */
static void
tor_tls_server_info_callback(const SSL *ssl, int type, int val)
{
  tor_tls_t *tls;
  (void) val;

  tor_tls_debug_state_callback(ssl, type, val);

  if (type != SSL_CB_ACCEPT_LOOP)
    return;
  if ((ssl->state != SSL3_ST_SW_SRVR_HELLO_A) &&
      (ssl->state != SSL3_ST_SW_SRVR_HELLO_B))
    return;

  tls = tor_tls_get_by_ssl(ssl);
  if (tls) {
    /* Check whether we're watching for renegotiates.  If so, this is one! */
    if (tls->negotiated_callback)
      tls->got_renegotiate = 1;
    if (tls->server_handshake_count < 127) /*avoid any overflow possibility*/
      ++tls->server_handshake_count;
  } else {
    log_warn(LD_BUG, "Couldn't look up the tls for an SSL*. How odd!");
    return;
  }

  /* Now check the cipher list. */
  if (tor_tls_client_is_using_v2_ciphers(ssl)) {
    if (tls->wasV2Handshake)
      return; /* We already turned this stuff off for the first handshake;
               * This is a renegotiation. */

    /* Yes, we're casting away the const from ssl.  This is very naughty of us.
     * Let's hope openssl doesn't notice! */

    /* Set SSL_MODE_NO_AUTO_CHAIN to keep from sending back any extra certs. */
    SSL_set_mode((SSL*) ssl, SSL_MODE_NO_AUTO_CHAIN);
    /* Don't send a hello request. */
    SSL_set_verify((SSL*) ssl, SSL_VERIFY_NONE, NULL);

    if (tls) {
      tls->wasV2Handshake = 1;
#ifdef USE_BUFFEREVENTS
      if (use_unsafe_renegotiation_flag)
        tls->ssl->s3->flags |= SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION;
#endif
    } else {
      log_warn(LD_BUG, "Couldn't look up the tls for an SSL*. How odd!");
    }
  }
}
#endif

#if OPENSSL_VERSION_NUMBER >= OPENSSL_V_SERIES(1,0,0)
/** Callback to get invoked on a server after we've read the list of ciphers
 * the client supports, but before we pick our own ciphersuite.
 *
 * We can't abuse an info_cb for this, since by the time one of the
 * client_hello info_cbs is called, we've already picked which ciphersuite to
 * use.
 *
 * Technically, this function is an abuse of this callback, since the point of
 * a session_secret_cb is to try to set up and/or verify a shared-secret for
 * authentication on the fly.  But as long as we return 0, we won't actually be
 * setting up a shared secret, and all will be fine.
 */
static int
tor_tls_session_secret_cb(SSL *ssl, void *secret, int *secret_len,
                          STACK_OF(SSL_CIPHER) *peer_ciphers,
                          SSL_CIPHER **cipher, void *arg)
{
  (void) secret;
  (void) secret_len;
  (void) peer_ciphers;
  (void) cipher;
  (void) arg;

  if (tor_tls_classify_client_ciphers(ssl, peer_ciphers) ==
       CIPHERS_UNRESTRICTED) {
    SSL_set_cipher_list(ssl, UNRESTRICTED_SERVER_CIPHER_LIST);
  }

  SSL_set_session_secret_cb(ssl, NULL, NULL);

  return 0;
}
static void
tor_tls_setup_session_secret_cb(tor_tls_t *tls)
{
  SSL_set_session_secret_cb(tls->ssl, tor_tls_session_secret_cb, NULL);
}
#else
#define tor_tls_setup_session_secret_cb(tls) STMT_NIL
#endif

/** Explain which ciphers we're missing. */
static void
log_unsupported_ciphers(smartlist_t *unsupported)
{
  char *joined;

  log_notice(LD_NET, "We weren't able to find support for all of the "
             "TLS ciphersuites that we wanted to advertise. This won't "
             "hurt security, but it might make your Tor (if run as a client) "
             "more easy for censors to block.");

  if (SSLeay() < 0x10000000L) {
    log_notice(LD_NET, "To correct this, use a more recent OpenSSL, "
               "built without disabling any secure ciphers or features.");
  } else {
    log_notice(LD_NET, "To correct this, use a version of OpenSSL "
               "built with none of its ciphers disabled.");
  }

  joined = smartlist_join_strings(unsupported, ":", 0, NULL);
  log_info(LD_NET, "The unsupported ciphers were: %s", joined);
  tor_free(joined);
}

/** Replace *<b>ciphers</b> with a new list of SSL ciphersuites: specifically,
 * a list designed to mimic a common web browser.  We might not be able to do
 * that if OpenSSL doesn't support all the ciphers we want.  Some of the
 * ciphers in the list won't actually be implemented by OpenSSL: that's okay
 * so long as the server doesn't select them.
 *
 * [If the server <b>does</b> select a bogus cipher, we won't crash or
 * anything; we'll just fail later when we try to look up the cipher in
 * ssl->cipher_list_by_id.]
 */
static void
rectify_client_ciphers(STACK_OF(SSL_CIPHER) **ciphers)
{
#ifdef V2_HANDSHAKE_CLIENT
  if (PREDICT_UNLIKELY(!CLIENT_CIPHER_STACK)) {
    /* We need to set CLIENT_CIPHER_STACK to an array of the ciphers
     * we want to use/advertise. */
    int i = 0, j = 0;
    smartlist_t *unsupported = smartlist_new();

    /* First, create a dummy SSL_CIPHER for every cipher. */
    CLIENT_CIPHER_DUMMIES =
      tor_malloc_zero(sizeof(SSL_CIPHER)*N_CLIENT_CIPHERS);
    for (i=0; i < N_CLIENT_CIPHERS; ++i) {
      CLIENT_CIPHER_DUMMIES[i].valid = 1;
      /* The "3<<24" here signifies that the cipher is supposed to work with
       * SSL3 and TLS1. */
      CLIENT_CIPHER_DUMMIES[i].id = CLIENT_CIPHER_INFO_LIST[i].id | (3<<24);
      CLIENT_CIPHER_DUMMIES[i].name = CLIENT_CIPHER_INFO_LIST[i].name;
    }

    CLIENT_CIPHER_STACK = sk_SSL_CIPHER_new_null();
    tor_assert(CLIENT_CIPHER_STACK);

    log_debug(LD_NET, "List was: %s", CLIENT_CIPHER_LIST);
    for (j = 0; j < sk_SSL_CIPHER_num(*ciphers); ++j) {
      SSL_CIPHER *cipher = sk_SSL_CIPHER_value(*ciphers, j);
      log_debug(LD_NET, "Cipher %d: %lx %s", j, cipher->id, cipher->name);
    }

    /* Then copy as many ciphers as we can from the good list, inserting
     * dummies as needed. Let j be an index into list of ciphers we have
     * (*ciphers) and let i be an index into the ciphers we want
     * (CLIENT_INFO_CIPHER_LIST).  We are building a list of ciphers in
     * CLIENT_CIPHER_STACK.
     */
    for (i = j = 0; i < N_CLIENT_CIPHERS; ) {
      SSL_CIPHER *cipher = NULL;
      if (j < sk_SSL_CIPHER_num(*ciphers))
        cipher = sk_SSL_CIPHER_value(*ciphers, j);
      if (cipher && ((cipher->id >> 24) & 0xff) != 3) {
        /* Skip over non-v3 ciphers entirely.  (This should no longer be
         * needed, thanks to saying !SSLv2 above.) */
        log_debug(LD_NET, "Skipping v%d cipher %s",
                  (int)((cipher->id>>24) & 0xff),
                  cipher->name);
        ++j;
      } else if (cipher &&
                 (cipher->id & 0xffff) == CLIENT_CIPHER_INFO_LIST[i].id) {
        /* "cipher" is the cipher we expect. Put it on the list. */
        log_debug(LD_NET, "Found cipher %s", cipher->name);
        sk_SSL_CIPHER_push(CLIENT_CIPHER_STACK, cipher);
        ++j;
        ++i;
      } else if (!strcmp(CLIENT_CIPHER_DUMMIES[i].name,
                         "SSL_RSA_FIPS_WITH_3DES_EDE_CBC_SHA")) {
        /* We found bogus cipher 0xfeff, which OpenSSL doesn't support and
         * never has.  For this one, we need a dummy. */
        log_debug(LD_NET, "Inserting fake %s", CLIENT_CIPHER_DUMMIES[i].name);
        sk_SSL_CIPHER_push(CLIENT_CIPHER_STACK, &CLIENT_CIPHER_DUMMIES[i]);
        ++i;
      } else {
        /* OpenSSL doesn't have this one. */
        log_debug(LD_NET, "Completely omitting unsupported cipher %s",
                  CLIENT_CIPHER_INFO_LIST[i].name);
        smartlist_add(unsupported, (char*) CLIENT_CIPHER_INFO_LIST[i].name);
        ++i;
      }
    }

    if (smartlist_len(unsupported))
      log_unsupported_ciphers(unsupported);

    smartlist_free(unsupported);
  }

  sk_SSL_CIPHER_free(*ciphers);
  *ciphers = sk_SSL_CIPHER_dup(CLIENT_CIPHER_STACK);
  tor_assert(*ciphers);

#else
    (void)ciphers;
#endif
}

/** Create a new TLS object from a file descriptor, and a flag to
 * determine whether it is functioning as a server.
 */
tor_tls_t *
tor_tls_new(int sock, int isServer)
{
  BIO *bio = NULL;
  tor_tls_t *result = tor_malloc_zero(sizeof(tor_tls_t));
  tor_tls_context_t *context = isServer ? server_tls_context :
    client_tls_context;
  result->magic = TOR_TLS_MAGIC;

  tor_assert(context); /* make sure somebody made it first */
  if (!(result->ssl = SSL_new(context->ctx))) {
    tls_log_errors(NULL, LOG_WARN, LD_NET, "creating SSL object");
    tor_free(result);
    return NULL;
  }

#ifdef SSL_set_tlsext_host_name
  /* Browsers use the TLS hostname extension, so we should too. */
  if (!isServer) {
    char *fake_hostname = crypto_random_hostname(4,25, "www.",".com");
    SSL_set_tlsext_host_name(result->ssl, fake_hostname);
    tor_free(fake_hostname);
  }
#endif

  if (!SSL_set_cipher_list(result->ssl,
                     isServer ? SERVER_CIPHER_LIST : CLIENT_CIPHER_LIST)) {
    tls_log_errors(NULL, LOG_WARN, LD_NET, "setting ciphers");
#ifdef SSL_set_tlsext_host_name
    SSL_set_tlsext_host_name(result->ssl, NULL);
#endif
    SSL_free(result->ssl);
    tor_free(result);
    return NULL;
  }
  if (!isServer)
    rectify_client_ciphers(&result->ssl->cipher_list);
  result->socket = sock;
  bio = BIO_new_socket(sock, BIO_NOCLOSE);
  if (! bio) {
    tls_log_errors(NULL, LOG_WARN, LD_NET, "opening BIO");
#ifdef SSL_set_tlsext_host_name
    SSL_set_tlsext_host_name(result->ssl, NULL);
#endif
    SSL_free(result->ssl);
    tor_free(result);
    return NULL;
  }
  {
    int set_worked =
      SSL_set_ex_data(result->ssl, tor_tls_object_ex_data_index, result);
    if (!set_worked) {
      log_warn(LD_BUG,
               "Couldn't set the tls for an SSL*; connection will fail");
    }
  }
  SSL_set_bio(result->ssl, bio, bio);
  tor_tls_context_incref(context);
  result->context = context;
  result->state = TOR_TLS_ST_HANDSHAKE;
  result->isServer = isServer;
  result->wantwrite_n = 0;
  result->last_write_count = BIO_number_written(bio);
  result->last_read_count = BIO_number_read(bio);
  if (result->last_write_count || result->last_read_count) {
    log_warn(LD_NET, "Newly created BIO has read count %lu, write count %lu",
             result->last_read_count, result->last_write_count);
  }
#ifdef V2_HANDSHAKE_SERVER
  if (isServer) {
    SSL_set_info_callback(result->ssl, tor_tls_server_info_callback);
  } else
#endif
  {
    SSL_set_info_callback(result->ssl, tor_tls_debug_state_callback);
  }

  if (isServer)
    tor_tls_setup_session_secret_cb(result);

  /* Not expected to get called. */
  tls_log_errors(NULL, LOG_WARN, LD_NET, "creating tor_tls_t object");
  return result;
}

/** Make future log messages about <b>tls</b> display the address
 * <b>address</b>.
 */
void
tor_tls_set_logged_address(tor_tls_t *tls, const char *address)
{
  tor_assert(tls);
  tor_free(tls->address);
  tls->address = tor_strdup(address);
}

/** Set <b>cb</b> to be called with argument <b>arg</b> whenever <b>tls</b>
 * next gets a client-side renegotiate in the middle of a read.  Do not
 * invoke this function until <em>after</em> initial handshaking is done!
 */
void
tor_tls_set_renegotiate_callback(tor_tls_t *tls,
                                 void (*cb)(tor_tls_t *, void *arg),
                                 void *arg)
{
  tls->negotiated_callback = cb;
  tls->callback_arg = arg;
  tls->got_renegotiate = 0;
#ifdef V2_HANDSHAKE_SERVER
  if (cb) {
    SSL_set_info_callback(tls->ssl, tor_tls_server_info_callback);
  } else {
    SSL_set_info_callback(tls->ssl, tor_tls_debug_state_callback);
  }
#endif
}

/** If this version of openssl requires it, turn on renegotiation on
 * <b>tls</b>.
 */
void
tor_tls_unblock_renegotiation(tor_tls_t *tls)
{
  /* Yes, we know what we are doing here.  No, we do not treat a renegotiation
   * as authenticating any earlier-received data. */
  if (use_unsafe_renegotiation_flag) {
    tls->ssl->s3->flags |= SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION;
  }
  if (use_unsafe_renegotiation_op) {
    SSL_set_options(tls->ssl,
                    SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION);
  }
}

/** If this version of openssl supports it, turn off renegotiation on
 * <b>tls</b>.  (Our protocol never requires this for security, but it's nice
 * to use belt-and-suspenders here.)
 */
void
tor_tls_block_renegotiation(tor_tls_t *tls)
{
  tls->ssl->s3->flags &= ~SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION;
}

/** Assert that the flags that allow legacy renegotiation are still set */
void
tor_tls_assert_renegotiation_unblocked(tor_tls_t *tls)
{
  if (use_unsafe_renegotiation_flag) {
    tor_assert(0 != (tls->ssl->s3->flags &
                     SSL3_FLAGS_ALLOW_UNSAFE_LEGACY_RENEGOTIATION));
  }
  if (use_unsafe_renegotiation_op) {
    long options = SSL_get_options(tls->ssl);
    tor_assert(0 != (options & SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION));
  }
}

/** Return whether this tls initiated the connect (client) or
 * received it (server). */
int
tor_tls_is_server(tor_tls_t *tls)
{
  tor_assert(tls);
  return tls->isServer;
}

/** Release resources associated with a TLS object.  Does not close the
 * underlying file descriptor.
 */
void
tor_tls_free(tor_tls_t *tls)
{
  if (!tls)
    return;
  tor_assert(tls->ssl);
  {
    size_t r,w;
    tor_tls_get_n_raw_bytes(tls,&r,&w); /* ensure written_by_tls is updated */
  }
#ifdef SSL_set_tlsext_host_name
  SSL_set_tlsext_host_name(tls->ssl, NULL);
#endif
  SSL_free(tls->ssl);
  tls->ssl = NULL;
  tls->negotiated_callback = NULL;
  if (tls->context)
    tor_tls_context_decref(tls->context);
  tor_free(tls->address);
  tls->magic = 0x99999999;
  tor_free(tls);
}

/** Underlying function for TLS reading.  Reads up to <b>len</b>
 * characters from <b>tls</b> into <b>cp</b>.  On success, returns the
 * number of characters read.  On failure, returns TOR_TLS_ERROR,
 * TOR_TLS_CLOSE, TOR_TLS_WANTREAD, or TOR_TLS_WANTWRITE.
 */
int
tor_tls_read(tor_tls_t *tls, char *cp, size_t len)
{
  int r, err;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->state == TOR_TLS_ST_OPEN);
  tor_assert(len<INT_MAX);
  r = SSL_read(tls->ssl, cp, (int)len);
  if (r > 0) {
#ifdef V2_HANDSHAKE_SERVER
    if (tls->got_renegotiate) {
      /* Renegotiation happened! */
      log_info(LD_NET, "Got a TLS renegotiation from %s", ADDR(tls));
      if (tls->negotiated_callback)
        tls->negotiated_callback(tls, tls->callback_arg);
      tls->got_renegotiate = 0;
    }
#endif
    return r;
  }
  err = tor_tls_get_error(tls, r, CATCH_ZERO, "reading", LOG_DEBUG, LD_NET);
  if (err == TOR_TLS_ZERORETURN_ || err == TOR_TLS_CLOSE) {
    log_debug(LD_NET,"read returned r=%d; TLS is closed",r);
    tls->state = TOR_TLS_ST_CLOSED;
    return TOR_TLS_CLOSE;
  } else {
    tor_assert(err != TOR_TLS_DONE);
    log_debug(LD_NET,"read returned r=%d, err=%d",r,err);
    return err;
  }
}

/** Total number of bytes that we've used TLS to send.  Used to track TLS
 * overhead. */
static uint64_t total_bytes_written_over_tls = 0;
/** Total number of bytes that TLS has put on the network for us. Used to
 * track TLS overhead. */
static uint64_t total_bytes_written_by_tls = 0;

/** Underlying function for TLS writing.  Write up to <b>n</b>
 * characters from <b>cp</b> onto <b>tls</b>.  On success, returns the
 * number of characters written.  On failure, returns TOR_TLS_ERROR,
 * TOR_TLS_WANTREAD, or TOR_TLS_WANTWRITE.
 */
int
tor_tls_write(tor_tls_t *tls, const char *cp, size_t n)
{
  int r, err;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->state == TOR_TLS_ST_OPEN);
  tor_assert(n < INT_MAX);
  if (n == 0)
    return 0;
  if (tls->wantwrite_n) {
    /* if WANTWRITE last time, we must use the _same_ n as before */
    tor_assert(n >= tls->wantwrite_n);
    log_debug(LD_NET,"resuming pending-write, (%d to flush, reusing %d)",
              (int)n, (int)tls->wantwrite_n);
    n = tls->wantwrite_n;
    tls->wantwrite_n = 0;
  }
  r = SSL_write(tls->ssl, cp, (int)n);
  err = tor_tls_get_error(tls, r, 0, "writing", LOG_INFO, LD_NET);
  if (err == TOR_TLS_DONE) {
    total_bytes_written_over_tls += r;
    return r;
  }
  if (err == TOR_TLS_WANTWRITE || err == TOR_TLS_WANTREAD) {
    tls->wantwrite_n = n;
  }
  return err;
}

/** Perform initial handshake on <b>tls</b>.  When finished, returns
 * TOR_TLS_DONE.  On failure, returns TOR_TLS_ERROR, TOR_TLS_WANTREAD,
 * or TOR_TLS_WANTWRITE.
 */
int
tor_tls_handshake(tor_tls_t *tls)
{
  int r;
  int oldstate;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->state == TOR_TLS_ST_HANDSHAKE);
  check_no_tls_errors();
  oldstate = tls->ssl->state;
  if (tls->isServer) {
    log_debug(LD_HANDSHAKE, "About to call SSL_accept on %p (%s)", tls,
              SSL_state_string_long(tls->ssl));
    r = SSL_accept(tls->ssl);
  } else {
    log_debug(LD_HANDSHAKE, "About to call SSL_connect on %p (%s)", tls,
              SSL_state_string_long(tls->ssl));
    r = SSL_connect(tls->ssl);
  }
  if (oldstate != tls->ssl->state)
    log_debug(LD_HANDSHAKE, "After call, %p was in state %s",
              tls, SSL_state_string_long(tls->ssl));
  /* We need to call this here and not earlier, since OpenSSL has a penchant
   * for clearing its flags when you say accept or connect. */
  tor_tls_unblock_renegotiation(tls);
  r = tor_tls_get_error(tls,r,0, "handshaking", LOG_INFO, LD_HANDSHAKE);
  if (ERR_peek_error() != 0) {
    tls_log_errors(tls, tls->isServer ? LOG_INFO : LOG_WARN, LD_HANDSHAKE,
                   "handshaking");
    return TOR_TLS_ERROR_MISC;
  }
  if (r == TOR_TLS_DONE) {
    tls->state = TOR_TLS_ST_OPEN;
    return tor_tls_finish_handshake(tls);
  }
  return r;
}

/** Perform the final part of the intial TLS handshake on <b>tls</b>.  This
 * should be called for the first handshake only: it determines whether the v1
 * or the v2 handshake was used, and adjusts things for the renegotiation
 * handshake as appropriate.
 *
 * tor_tls_handshake() calls this on its own; you only need to call this if
 * bufferevent is doing the handshake for you.
 */
int
tor_tls_finish_handshake(tor_tls_t *tls)
{
  int r = TOR_TLS_DONE;
  if (tls->isServer) {
    SSL_set_info_callback(tls->ssl, NULL);
    SSL_set_verify(tls->ssl, SSL_VERIFY_PEER, always_accept_verify_cb);
    /* There doesn't seem to be a clear OpenSSL API to clear mode flags. */
    tls->ssl->mode &= ~SSL_MODE_NO_AUTO_CHAIN;
#ifdef V2_HANDSHAKE_SERVER
    if (tor_tls_client_is_using_v2_ciphers(tls->ssl)) {
      /* This check is redundant, but back when we did it in the callback,
       * we might have not been able to look up the tor_tls_t if the code
       * was buggy.  Fixing that. */
      if (!tls->wasV2Handshake) {
        log_warn(LD_BUG, "For some reason, wasV2Handshake didn't"
                 " get set. Fixing that.");
      }
      tls->wasV2Handshake = 1;
      log_debug(LD_HANDSHAKE, "Completed V2 TLS handshake with client; waiting"
                " for renegotiation.");
    } else {
      tls->wasV2Handshake = 0;
    }
#endif
  } else {
#ifdef V2_HANDSHAKE_CLIENT
    /* If we got no ID cert, we're a v2 handshake. */
    X509 *cert = SSL_get_peer_certificate(tls->ssl);
    STACK_OF(X509) *chain = SSL_get_peer_cert_chain(tls->ssl);
    int n_certs = sk_X509_num(chain);
    if (n_certs > 1 || (n_certs == 1 && cert != sk_X509_value(chain, 0))) {
      log_debug(LD_HANDSHAKE, "Server sent back multiple certificates; it "
                "looks like a v1 handshake on %p", tls);
      tls->wasV2Handshake = 0;
    } else {
      log_debug(LD_HANDSHAKE,
                "Server sent back a single certificate; looks like "
                "a v2 handshake on %p.", tls);
      tls->wasV2Handshake = 1;
    }
    if (cert)
      X509_free(cert);
#endif
    if (SSL_set_cipher_list(tls->ssl, SERVER_CIPHER_LIST) == 0) {
      tls_log_errors(NULL, LOG_WARN, LD_HANDSHAKE, "re-setting ciphers");
      r = TOR_TLS_ERROR_MISC;
    }
  }
  return r;
}

#ifdef USE_BUFFEREVENTS
/** Put <b>tls</b>, which must be a client connection, into renegotiation
 * mode. */
int
tor_tls_start_renegotiating(tor_tls_t *tls)
{
  int r = SSL_renegotiate(tls->ssl);
  if (r <= 0) {
    return tor_tls_get_error(tls, r, 0, "renegotiating", LOG_WARN,
                             LD_HANDSHAKE);
  }
  return 0;
}
#endif

/** Client only: Renegotiate a TLS session.  When finished, returns
 * TOR_TLS_DONE.  On failure, returns TOR_TLS_ERROR, TOR_TLS_WANTREAD, or
 * TOR_TLS_WANTWRITE.
 */
int
tor_tls_renegotiate(tor_tls_t *tls)
{
  int r;
  tor_assert(tls);
  /* We could do server-initiated renegotiation too, but that would be tricky.
   * Instead of "SSL_renegotiate, then SSL_do_handshake until done" */
  tor_assert(!tls->isServer);
  if (tls->state != TOR_TLS_ST_RENEGOTIATE) {
    int r = SSL_renegotiate(tls->ssl);
    if (r <= 0) {
      return tor_tls_get_error(tls, r, 0, "renegotiating", LOG_WARN,
                               LD_HANDSHAKE);
    }
    tls->state = TOR_TLS_ST_RENEGOTIATE;
  }
  r = SSL_do_handshake(tls->ssl);
  if (r == 1) {
    tls->state = TOR_TLS_ST_OPEN;
    return TOR_TLS_DONE;
  } else
    return tor_tls_get_error(tls, r, 0, "renegotiating handshake", LOG_INFO,
                             LD_HANDSHAKE);
}

/** Shut down an open tls connection <b>tls</b>.  When finished, returns
 * TOR_TLS_DONE.  On failure, returns TOR_TLS_ERROR, TOR_TLS_WANTREAD,
 * or TOR_TLS_WANTWRITE.
 */
int
tor_tls_shutdown(tor_tls_t *tls)
{
  int r, err;
  char buf[128];
  tor_assert(tls);
  tor_assert(tls->ssl);

  while (1) {
    if (tls->state == TOR_TLS_ST_SENTCLOSE) {
      /* If we've already called shutdown once to send a close message,
       * we read until the other side has closed too.
       */
      do {
        r = SSL_read(tls->ssl, buf, 128);
      } while (r>0);
      err = tor_tls_get_error(tls, r, CATCH_ZERO, "reading to shut down",
                              LOG_INFO, LD_NET);
      if (err == TOR_TLS_ZERORETURN_) {
        tls->state = TOR_TLS_ST_GOTCLOSE;
        /* fall through... */
      } else {
        return err;
      }
    }

    r = SSL_shutdown(tls->ssl);
    if (r == 1) {
      /* If shutdown returns 1, the connection is entirely closed. */
      tls->state = TOR_TLS_ST_CLOSED;
      return TOR_TLS_DONE;
    }
    err = tor_tls_get_error(tls, r, CATCH_SYSCALL|CATCH_ZERO, "shutting down",
                            LOG_INFO, LD_NET);
    if (err == TOR_TLS_SYSCALL_) {
      /* The underlying TCP connection closed while we were shutting down. */
      tls->state = TOR_TLS_ST_CLOSED;
      return TOR_TLS_DONE;
    } else if (err == TOR_TLS_ZERORETURN_) {
      /* The TLS connection says that it sent a shutdown record, but
       * isn't done shutting down yet.  Make sure that this hasn't
       * happened before, then go back to the start of the function
       * and try to read.
       */
      if (tls->state == TOR_TLS_ST_GOTCLOSE ||
         tls->state == TOR_TLS_ST_SENTCLOSE) {
        log_warn(LD_NET,
            "TLS returned \"half-closed\" value while already half-closed");
        return TOR_TLS_ERROR_MISC;
      }
      tls->state = TOR_TLS_ST_SENTCLOSE;
      /* fall through ... */
    } else {
      return err;
    }
  } /* end loop */
}

/** Return true iff this TLS connection is authenticated.
 */
int
tor_tls_peer_has_cert(tor_tls_t *tls)
{
  X509 *cert;
  cert = SSL_get_peer_certificate(tls->ssl);
  tls_log_errors(tls, LOG_WARN, LD_HANDSHAKE, "getting peer certificate");
  if (!cert)
    return 0;
  X509_free(cert);
  return 1;
}

/** Return the peer certificate, or NULL if there isn't one. */
tor_cert_t *
tor_tls_get_peer_cert(tor_tls_t *tls)
{
  X509 *cert;
  cert = SSL_get_peer_certificate(tls->ssl);
  tls_log_errors(tls, LOG_WARN, LD_HANDSHAKE, "getting peer certificate");
  if (!cert)
    return NULL;
  return tor_cert_new(cert);
}

/** Warn that a certificate lifetime extends through a certain range. */
static void
log_cert_lifetime(int severity, const X509 *cert, const char *problem)
{
  BIO *bio = NULL;
  BUF_MEM *buf;
  char *s1=NULL, *s2=NULL;
  char mytime[33];
  time_t now = time(NULL);
  struct tm tm;
  size_t n;

  if (problem)
    tor_log(severity, LD_GENERAL,
        "Certificate %s. Either their clock is set wrong, or your clock "
        "is wrong.",
           problem);

  if (!(bio = BIO_new(BIO_s_mem()))) {
    log_warn(LD_GENERAL, "Couldn't allocate BIO!"); goto end;
  }
  if (!(ASN1_TIME_print(bio, X509_get_notBefore(cert)))) {
    tls_log_errors(NULL, LOG_WARN, LD_NET, "printing certificate lifetime");
    goto end;
  }
  BIO_get_mem_ptr(bio, &buf);
  s1 = tor_strndup(buf->data, buf->length);

  (void)BIO_reset(bio);
  if (!(ASN1_TIME_print(bio, X509_get_notAfter(cert)))) {
    tls_log_errors(NULL, LOG_WARN, LD_NET, "printing certificate lifetime");
    goto end;
  }
  BIO_get_mem_ptr(bio, &buf);
  s2 = tor_strndup(buf->data, buf->length);

  n = strftime(mytime, 32, "%b %d %H:%M:%S %Y UTC", tor_gmtime_r(&now, &tm));
  if (n > 0) {
    tor_log(severity, LD_GENERAL,
        "(certificate lifetime runs from %s through %s. Your time is %s.)",
        s1,s2,mytime);
  } else {
    tor_log(severity, LD_GENERAL,
        "(certificate lifetime runs from %s through %s. "
        "Couldn't get your time.)",
        s1, s2);
  }

 end:
  /* Not expected to get invoked */
  tls_log_errors(NULL, LOG_WARN, LD_NET, "getting certificate lifetime");
  if (bio)
    BIO_free(bio);
  tor_free(s1);
  tor_free(s2);
}

/** Helper function: try to extract a link certificate and an identity
 * certificate from <b>tls</b>, and store them in *<b>cert_out</b> and
 * *<b>id_cert_out</b> respectively.  Log all messages at level
 * <b>severity</b>.
 *
 * Note that a reference is added to cert_out, so it needs to be
 * freed. id_cert_out doesn't. */
static void
try_to_extract_certs_from_tls(int severity, tor_tls_t *tls,
                              X509 **cert_out, X509 **id_cert_out)
{
  X509 *cert = NULL, *id_cert = NULL;
  STACK_OF(X509) *chain = NULL;
  int num_in_chain, i;
  *cert_out = *id_cert_out = NULL;

  if (!(cert = SSL_get_peer_certificate(tls->ssl)))
    return;
  *cert_out = cert;
  if (!(chain = SSL_get_peer_cert_chain(tls->ssl)))
    return;
  num_in_chain = sk_X509_num(chain);
  /* 1 means we're receiving (server-side), and it's just the id_cert.
   * 2 means we're connecting (client-side), and it's both the link
   * cert and the id_cert.
   */
  if (num_in_chain < 1) {
    log_fn(severity,LD_PROTOCOL,
           "Unexpected number of certificates in chain (%d)",
           num_in_chain);
    return;
  }
  for (i=0; i<num_in_chain; ++i) {
    id_cert = sk_X509_value(chain, i);
    if (X509_cmp(id_cert, cert) != 0)
      break;
  }
  *id_cert_out = id_cert;
}

/** If the provided tls connection is authenticated and has a
 * certificate chain that is currently valid and signed, then set
 * *<b>identity_key</b> to the identity certificate's key and return
 * 0.  Else, return -1 and log complaints with log-level <b>severity</b>.
 */
int
tor_tls_verify(int severity, tor_tls_t *tls, crypto_pk_t **identity_key)
{
  X509 *cert = NULL, *id_cert = NULL;
  EVP_PKEY *id_pkey = NULL;
  RSA *rsa;
  int r = -1;

  *identity_key = NULL;

  try_to_extract_certs_from_tls(severity, tls, &cert, &id_cert);
  if (!cert)
    goto done;
  if (!id_cert) {
    log_fn(severity,LD_PROTOCOL,"No distinct identity certificate found");
    goto done;
  }
  tls_log_errors(tls, severity, LD_HANDSHAKE, "before verifying certificate");

  if (!(id_pkey = X509_get_pubkey(id_cert)) ||
      X509_verify(cert, id_pkey) <= 0) {
    log_fn(severity,LD_PROTOCOL,"X509_verify on cert and pkey returned <= 0");
    tls_log_errors(tls, severity, LD_HANDSHAKE, "verifying certificate");
    goto done;
  }

  rsa = EVP_PKEY_get1_RSA(id_pkey);
  if (!rsa)
    goto done;
  *identity_key = crypto_new_pk_from_rsa_(rsa);

  r = 0;

 done:
  if (cert)
    X509_free(cert);
  if (id_pkey)
    EVP_PKEY_free(id_pkey);

  /* This should never get invoked, but let's make sure in case OpenSSL
   * acts unexpectedly. */
  tls_log_errors(tls, LOG_WARN, LD_HANDSHAKE, "finishing tor_tls_verify");

  return r;
}

/** Check whether the certificate set on the connection <b>tls</b> is expired
 * give or take <b>past_tolerance</b> seconds, or not-yet-valid give or take
 * <b>future_tolerance</b> seconds. Return 0 for valid, -1 for failure.
 *
 * NOTE: you should call tor_tls_verify before tor_tls_check_lifetime.
 */
int
tor_tls_check_lifetime(int severity, tor_tls_t *tls,
                       int past_tolerance, int future_tolerance)
{
  X509 *cert;
  int r = -1;

  if (!(cert = SSL_get_peer_certificate(tls->ssl)))
    goto done;

  if (check_cert_lifetime_internal(severity, cert,
                                   past_tolerance, future_tolerance) < 0)
    goto done;

  r = 0;
 done:
  if (cert)
    X509_free(cert);
  /* Not expected to get invoked */
  tls_log_errors(tls, LOG_WARN, LD_NET, "checking certificate lifetime");

  return r;
}

/** Helper: check whether <b>cert</b> is expired give or take
 * <b>past_tolerance</b> seconds, or not-yet-valid give or take
 * <b>future_tolerance</b> seconds.  If it is live, return 0.  If it is not
 * live, log a message and return -1. */
static int
check_cert_lifetime_internal(int severity, const X509 *cert,
                             int past_tolerance, int future_tolerance)
{
  time_t now, t;

  now = time(NULL);

  t = now + future_tolerance;
  if (X509_cmp_time(X509_get_notBefore(cert), &t) > 0) {
    log_cert_lifetime(severity, cert, "not yet valid");
    return -1;
  }
  t = now - past_tolerance;
  if (X509_cmp_time(X509_get_notAfter(cert), &t) < 0) {
    log_cert_lifetime(severity, cert, "already expired");
    return -1;
  }

  return 0;
}

/** Return the number of bytes available for reading from <b>tls</b>.
 */
int
tor_tls_get_pending_bytes(tor_tls_t *tls)
{
  tor_assert(tls);
  return SSL_pending(tls->ssl);
}

/** If <b>tls</b> requires that the next write be of a particular size,
 * return that size.  Otherwise, return 0. */
size_t
tor_tls_get_forced_write_size(tor_tls_t *tls)
{
  return tls->wantwrite_n;
}

/** Sets n_read and n_written to the number of bytes read and written,
 * respectively, on the raw socket used by <b>tls</b> since the last time this
 * function was called on <b>tls</b>. */
void
tor_tls_get_n_raw_bytes(tor_tls_t *tls, size_t *n_read, size_t *n_written)
{
  BIO *wbio, *tmpbio;
  unsigned long r, w;
  r = BIO_number_read(SSL_get_rbio(tls->ssl));
  /* We want the number of bytes actually for real written.  Unfortunately,
   * sometimes OpenSSL replaces the wbio on tls->ssl with a buffering bio,
   * which makes the answer turn out wrong.  Let's cope with that.  Note
   * that this approach will fail if we ever replace tls->ssl's BIOs with
   * buffering bios for reasons of our own.  As an alternative, we could
   * save the original BIO for  tls->ssl in the tor_tls_t structure, but
   * that would be tempting fate. */
  wbio = SSL_get_wbio(tls->ssl);
  if (wbio->method == BIO_f_buffer() && (tmpbio = BIO_next(wbio)) != NULL)
    wbio = tmpbio;
  w = BIO_number_written(wbio);

  /* We are ok with letting these unsigned ints go "negative" here:
   * If we wrapped around, this should still give us the right answer, unless
   * we wrapped around by more than ULONG_MAX since the last time we called
   * this function.
   */
  *n_read = (size_t)(r - tls->last_read_count);
  *n_written = (size_t)(w - tls->last_write_count);
  if (*n_read > INT_MAX || *n_written > INT_MAX) {
    log_warn(LD_BUG, "Preposterously large value in tor_tls_get_n_raw_bytes. "
             "r=%lu, last_read=%lu, w=%lu, last_written=%lu",
             r, tls->last_read_count, w, tls->last_write_count);
  }
  total_bytes_written_by_tls += *n_written;
  tls->last_read_count = r;
  tls->last_write_count = w;
}

/** Return a ratio of the bytes that TLS has sent to the bytes that we've told
 * it to send. Used to track whether our TLS records are getting too tiny. */
MOCK_IMPL(double,
tls_get_write_overhead_ratio,(void))
{
  if (total_bytes_written_over_tls == 0)
    return 1.0;

  return U64_TO_DBL(total_bytes_written_by_tls) /
    U64_TO_DBL(total_bytes_written_over_tls);
}

/** Implement check_no_tls_errors: If there are any pending OpenSSL
 * errors, log an error message. */
void
check_no_tls_errors_(const char *fname, int line)
{
  if (ERR_peek_error() == 0)
    return;
  log_warn(LD_CRYPTO, "Unhandled OpenSSL errors found at %s:%d: ",
      tor_fix_source_file(fname), line);
  tls_log_errors(NULL, LOG_WARN, LD_NET, NULL);
}

/** Return true iff the initial TLS connection at <b>tls</b> did not use a v2
 * TLS handshake. Output is undefined if the handshake isn't finished. */
int
tor_tls_used_v1_handshake(tor_tls_t *tls)
{
  if (tls->isServer) {
#ifdef V2_HANDSHAKE_SERVER
    return ! tls->wasV2Handshake;
#endif
  } else {
#ifdef V2_HANDSHAKE_CLIENT
    return ! tls->wasV2Handshake;
#endif
  }
  return 1;
}

/** Return true iff <b>name</b> is a DN of a kind that could only
 * occur in a v3-handshake-indicating certificate */
static int
dn_indicates_v3_cert(X509_NAME *name)
{
#ifdef DISABLE_V3_LINKPROTO_CLIENTSIDE
  (void)name;
  return 0;
#else
  X509_NAME_ENTRY *entry;
  int n_entries;
  ASN1_OBJECT *obj;
  ASN1_STRING *str;
  unsigned char *s;
  int len, r;

  n_entries = X509_NAME_entry_count(name);
  if (n_entries != 1)
    return 1; /* More than one entry in the DN. */
  entry = X509_NAME_get_entry(name, 0);

  obj = X509_NAME_ENTRY_get_object(entry);
  if (OBJ_obj2nid(obj) != OBJ_txt2nid("commonName"))
    return 1; /* The entry isn't a commonName. */

  str = X509_NAME_ENTRY_get_data(entry);
  len = ASN1_STRING_to_UTF8(&s, str);
  if (len < 0)
    return 0;
  r = fast_memneq(s + len - 4, ".net", 4);
  OPENSSL_free(s);
  return r;
#endif
}

/** Return true iff the peer certificate we're received on <b>tls</b>
 * indicates that this connection should use the v3 (in-protocol)
 * authentication handshake.
 *
 * Only the connection initiator should use this, and only once the initial
 * handshake is done; the responder detects a v1 handshake by cipher types,
 * and a v3/v2 handshake by Versions cell vs renegotiation.
 */
int
tor_tls_received_v3_certificate(tor_tls_t *tls)
{
  X509 *cert = SSL_get_peer_certificate(tls->ssl);
  EVP_PKEY *key = NULL;
  X509_NAME *issuer_name, *subject_name;
  int is_v3 = 0;

  if (!cert) {
    log_warn(LD_BUG, "Called on a connection with no peer certificate");
    goto done;
  }

  subject_name = X509_get_subject_name(cert);
  issuer_name = X509_get_issuer_name(cert);

  if (X509_name_cmp(subject_name, issuer_name) == 0) {
    is_v3 = 1; /* purportedly self signed */
    goto done;
  }

  if (dn_indicates_v3_cert(subject_name) ||
      dn_indicates_v3_cert(issuer_name)) {
    is_v3 = 1; /* DN is fancy */
    goto done;
  }

  key = X509_get_pubkey(cert);
  if (EVP_PKEY_bits(key) != 1024 ||
      EVP_PKEY_type(key->type) != EVP_PKEY_RSA) {
    is_v3 = 1; /* Key is fancy */
    goto done;
  }

 done:
  if (key)
    EVP_PKEY_free(key);
  if (cert)
    X509_free(cert);

  return is_v3;
}

/** Return the number of server handshakes that we've noticed doing on
 * <b>tls</b>. */
int
tor_tls_get_num_server_handshakes(tor_tls_t *tls)
{
  return tls->server_handshake_count;
}

/** Return true iff the server TLS connection <b>tls</b> got the renegotiation
 * request it was waiting for. */
int
tor_tls_server_got_renegotiate(tor_tls_t *tls)
{
  return tls->got_renegotiate;
}

/** Set the DIGEST256_LEN buffer at <b>secrets_out</b> to the value used in
 * the v3 handshake to prove that the client knows the TLS secrets for the
 * connection <b>tls</b>.  Return 0 on success, -1 on failure.
 */
int
tor_tls_get_tlssecrets(tor_tls_t *tls, uint8_t *secrets_out)
{
#define TLSSECRET_MAGIC "Tor V3 handshake TLS cross-certification"
  char buf[128];
  size_t len;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->ssl->s3);
  tor_assert(tls->ssl->session);
  /*
    The value is an HMAC, using the TLS master key as the HMAC key, of
    client_random | server_random | TLSSECRET_MAGIC
  */
  memcpy(buf +  0, tls->ssl->s3->client_random, 32);
  memcpy(buf + 32, tls->ssl->s3->server_random, 32);
  memcpy(buf + 64, TLSSECRET_MAGIC, strlen(TLSSECRET_MAGIC) + 1);
  len = 64 + strlen(TLSSECRET_MAGIC) + 1;
  crypto_hmac_sha256((char*)secrets_out,
                     (char*)tls->ssl->session->master_key,
                     tls->ssl->session->master_key_length,
                     buf, len);
  memwipe(buf, 0, sizeof(buf));
  return 0;
}

/** Examine the amount of memory used and available for buffers in <b>tls</b>.
 * Set *<b>rbuf_capacity</b> to the amount of storage allocated for the read
 * buffer and *<b>rbuf_bytes</b> to the amount actually used.
 * Set *<b>wbuf_capacity</b> to the amount of storage allocated for the write
 * buffer and *<b>wbuf_bytes</b> to the amount actually used. */
void
tor_tls_get_buffer_sizes(tor_tls_t *tls,
                         size_t *rbuf_capacity, size_t *rbuf_bytes,
                         size_t *wbuf_capacity, size_t *wbuf_bytes)
{
  if (tls->ssl->s3->rbuf.buf)
    *rbuf_capacity = tls->ssl->s3->rbuf.len;
  else
    *rbuf_capacity = 0;
  if (tls->ssl->s3->wbuf.buf)
    *wbuf_capacity = tls->ssl->s3->wbuf.len;
  else
    *wbuf_capacity = 0;
  *rbuf_bytes = tls->ssl->s3->rbuf.left;
  *wbuf_bytes = tls->ssl->s3->wbuf.left;
}

#ifdef USE_BUFFEREVENTS
/** Construct and return an TLS-encrypting bufferevent to send data over
 * <b>socket</b>, which must match the socket of the underlying bufferevent
 * <b>bufev_in</b>.  The TLS object <b>tls</b> is used for encryption.
 *
 * This function will either create a filtering bufferevent that wraps around
 * <b>bufev_in</b>, or it will free bufev_in and return a new bufferevent that
 * uses the <b>tls</b> to talk to the network directly.  Do not use
 * <b>bufev_in</b> after calling this function.
 *
 * The connection will start out doing a server handshake if <b>receiving</b>
 * is strue, and a client handshake otherwise.
 *
 * Returns NULL on failure.
 */
struct bufferevent *
tor_tls_init_bufferevent(tor_tls_t *tls, struct bufferevent *bufev_in,
                         evutil_socket_t socket, int receiving,
                         int filter)
{
  struct bufferevent *out;
  const enum bufferevent_ssl_state state = receiving ?
    BUFFEREVENT_SSL_ACCEPTING : BUFFEREVENT_SSL_CONNECTING;

  if (filter || tor_libevent_using_iocp_bufferevents()) {
    /* Grab an extra reference to the SSL, since BEV_OPT_CLOSE_ON_FREE
       means that the SSL will get freed too.

       This increment makes our SSL usage not-threadsafe, BTW.  We should
       see if we're allowed to use CRYPTO_add from outside openssl. */
    tls->ssl->references += 1;
    out = bufferevent_openssl_filter_new(tor_libevent_get_base(),
                                         bufev_in,
                                         tls->ssl,
                                         state,
                                         BEV_OPT_DEFER_CALLBACKS|
                                         BEV_OPT_CLOSE_ON_FREE);
    /* Tell the underlying bufferevent when to accept more data from the SSL
       filter (only when it's got less than 32K to write), and when to notify
       the SSL filter that it could write more (when it drops under 24K). */
    bufferevent_setwatermark(bufev_in, EV_WRITE, 24*1024, 32*1024);
  } else {
    if (bufev_in) {
      evutil_socket_t s = bufferevent_getfd(bufev_in);
      tor_assert(s == -1 || s == socket);
      tor_assert(evbuffer_get_length(bufferevent_get_input(bufev_in)) == 0);
      tor_assert(evbuffer_get_length(bufferevent_get_output(bufev_in)) == 0);
      tor_assert(BIO_number_read(SSL_get_rbio(tls->ssl)) == 0);
      tor_assert(BIO_number_written(SSL_get_rbio(tls->ssl)) == 0);
      bufferevent_free(bufev_in);
    }

    /* Current versions (as of 2.0.x) of Libevent need to defer
     * bufferevent_openssl callbacks, or else our callback functions will
     * get called reentrantly, which is bad for us.
     */
    out = bufferevent_openssl_socket_new(tor_libevent_get_base(),
                                         socket,
                                         tls->ssl,
                                         state,
                                         BEV_OPT_DEFER_CALLBACKS);
  }
  tls->state = TOR_TLS_ST_BUFFEREVENT;

  /* Unblock _after_ creating the bufferevent, since accept/connect tend to
   * clear flags. */
  tor_tls_unblock_renegotiation(tls);

  return out;
}
#endif

