/* SSL support via OpenSSL library.
   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
   2009, 2010, 2011, 2012, 2015 Free Software Foundation, Inc.
   Originally contributed by Christian Fraenkel.

This file is part of GNU Wget.

GNU Wget is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

GNU Wget is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Wget.  If not, see <http://www.gnu.org/licenses/>.

Additional permission under GNU GPL version 3 section 7

If you modify this program, or any covered work, by linking or
combining it with the OpenSSL project's OpenSSL library (or a
modified version of that library), containing parts covered by the
terms of the OpenSSL or SSLeay licenses, the Free Software Foundation
grants you additional permission to convey the resulting work.
Corresponding Source for a non-source form of such a combination
shall include the source code for the parts of OpenSSL used as well
as that of the covered work.  */

#include "wget.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <xalloc.h>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#if OPENSSL_VERSION_NUMBER >= 0x00907000
#include <openssl/conf.h>
#include <openssl/engine.h>
#endif

#include "utils.h"
#include "connect.h"
#include "url.h"
#include "ssl.h"

#ifdef WINDOWS
# include <w32sock.h>
#endif

/* Application-wide SSL context.  This is common to all SSL
   connections.  */
static SSL_CTX *ssl_ctx;

/* Initialize the SSL's PRNG using various methods. */

static void
init_prng (void)
{
  char namebuf[256];
  const char *random_file;

  if (RAND_status ())
    /* The PRNG has been seeded; no further action is necessary. */
    return;

  /* Seed from a file specified by the user.  This will be the file
     specified with --random-file, $RANDFILE, if set, or ~/.rnd, if it
     exists.  */
  if (opt.random_file)
    random_file = opt.random_file;
  else
    {
      /* Get the random file name using RAND_file_name. */
      namebuf[0] = '\0';
      random_file = RAND_file_name (namebuf, sizeof (namebuf));
    }

  if (random_file && *random_file)
    /* Seed at most 16k (apparently arbitrary value borrowed from
       curl) from random file. */
    RAND_load_file (random_file, 16384);

  if (RAND_status ())
    return;

#ifdef HAVE_RAND_EGD
  /* Get random data from EGD if opt.egd_file was used.  */
  if (opt.egd_file && *opt.egd_file)
    RAND_egd (opt.egd_file);
#endif

  if (RAND_status ())
    return;

#ifdef WINDOWS
  /* Under Windows, we can try to seed the PRNG using screen content.
     This may or may not work, depending on whether we'll calling Wget
     interactively.  */

  RAND_screen ();
  if (RAND_status ())
    return;
#endif

#if 0 /* don't do this by default */
  {
    int maxrand = 500;

    /* Still not random enough, presumably because neither /dev/random
       nor EGD were available.  Try to seed OpenSSL's PRNG with libc
       PRNG.  This is cryptographically weak and defeats the purpose
       of using OpenSSL, which is why it is highly discouraged.  */

    logprintf (LOG_NOTQUIET, _("WARNING: using a weak random seed.\n"));

    while (RAND_status () == 0 && maxrand-- > 0)
      {
        unsigned char rnd = random_number (256);
        RAND_seed (&rnd, sizeof (rnd));
      }
  }
#endif
}

/* Print errors in the OpenSSL error stack. */

static void
print_errors (void)
{
  unsigned long err;
  while ((err = ERR_get_error ()) != 0)
    logprintf (LOG_NOTQUIET, "OpenSSL: %s\n", ERR_error_string (err, NULL));
}

/* Convert keyfile type as used by options.h to a type as accepted by
   SSL_CTX_use_certificate_file and SSL_CTX_use_PrivateKey_file.

   (options.h intentionally doesn't use values from openssl/ssl.h so
   it doesn't depend specifically on OpenSSL for SSL functionality.)  */

static int
key_type_to_ssl_type (enum keyfile_type type)
{
  switch (type)
    {
    case keyfile_pem:
      return SSL_FILETYPE_PEM;
    case keyfile_asn1:
      return SSL_FILETYPE_ASN1;
    default:
      abort ();
    }
}

/* SSL has been initialized */
static int ssl_true_initialized = 0;

/* Create an SSL Context and set default paths etc.  Called the first
   time an HTTP download is attempted.

   Returns true on success, false otherwise.  */

bool
ssl_init (void)
{
  SSL_METHOD const *meth;
  long ssl_options = 0;

#if OPENSSL_VERSION_NUMBER >= 0x00907000
  if (ssl_true_initialized == 0)
    {
      OPENSSL_config (NULL);
      ssl_true_initialized = 1;
    }
#endif

  if (ssl_ctx)
    /* The SSL has already been initialized. */
    return true;

  /* Init the PRNG.  If that fails, bail out.  */
  init_prng ();
  if (RAND_status () != 1)
    {
      logprintf (LOG_NOTQUIET,
                 _("Could not seed PRNG; consider using --random-file.\n"));
      goto error;
    }

#if OPENSSL_VERSION_NUMBER >= 0x00907000
  OPENSSL_load_builtin_modules();
  ENGINE_load_builtin_engines();
  CONF_modules_load_file(NULL, NULL,
      CONF_MFLAGS_DEFAULT_SECTION|CONF_MFLAGS_IGNORE_MISSING_FILE);
#endif
  SSL_library_init ();
  SSL_load_error_strings ();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  SSLeay_add_all_algorithms ();
  SSLeay_add_ssl_algorithms ();
#endif

  switch (opt.secure_protocol)
    {
#if !defined OPENSSL_NO_SSL2 && OPENSSL_VERSION_NUMBER < 0x10100000L
    case secure_protocol_sslv2:
      meth = SSLv2_client_method ();
      break;
#endif

#ifndef OPENSSL_NO_SSL3_METHOD
    case secure_protocol_sslv3:
      meth = SSLv3_client_method ();
      break;
#endif

    case secure_protocol_auto:
    case secure_protocol_pfs:
      meth = SSLv23_client_method ();
      ssl_options |= SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
      break;
    case secure_protocol_tlsv1:
      meth = TLSv1_client_method ();
      break;

#if OPENSSL_VERSION_NUMBER >= 0x10001000
    case secure_protocol_tlsv1_1:
      meth = TLSv1_1_client_method ();
      break;

    case secure_protocol_tlsv1_2:
      meth = TLSv1_2_client_method ();
      break;
#else
    case secure_protocol_tlsv1_1:
      logprintf (LOG_NOTQUIET, _("Your OpenSSL version is too old to support TLSv1.1\n"));
      goto error;

    case secure_protocol_tlsv1_2:
      logprintf (LOG_NOTQUIET, _("Your OpenSSL version is too old to support TLSv1.2\n"));
      goto error;
#endif

    default:
      logprintf (LOG_NOTQUIET, _("OpenSSL: unimplemented 'secure-protocol' option value %d\n"), opt.secure_protocol);
      logprintf (LOG_NOTQUIET, _("Please report this issue to bug-wget@gnu.org\n"));
      abort ();
    }

  /* The type cast below accommodates older OpenSSL versions (0.9.8)
     where SSL_CTX_new() is declared without a "const" argument. */
  ssl_ctx = SSL_CTX_new ((SSL_METHOD *)meth);
  if (!ssl_ctx)
    goto error;

  if (ssl_options)
    SSL_CTX_set_options (ssl_ctx, ssl_options);

  /* OpenSSL ciphers: https://www.openssl.org/docs/apps/ciphers.html
   * Since we want a good protection, we also use HIGH (that excludes MD4 ciphers and some more)
   */
  if (opt.secure_protocol == secure_protocol_pfs)
    SSL_CTX_set_cipher_list (ssl_ctx, "HIGH:MEDIUM:!RC4:!SRP:!PSK:!RSA:!aNULL@STRENGTH");

  SSL_CTX_set_default_verify_paths (ssl_ctx);
  SSL_CTX_load_verify_locations (ssl_ctx, opt.ca_cert, opt.ca_directory);

  if (opt.crl_file)
    {
      X509_STORE *store = SSL_CTX_get_cert_store (ssl_ctx);
      X509_LOOKUP *lookup;

      if (!(lookup = X509_STORE_add_lookup (store, X509_LOOKUP_file ()))
          || (!X509_load_crl_file (lookup, opt.crl_file, X509_FILETYPE_PEM)))
        goto error;

      X509_STORE_set_flags (store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
    }

  /* SSL_VERIFY_NONE instructs OpenSSL not to abort SSL_connect if the
     certificate is invalid.  We verify the certificate separately in
     ssl_check_certificate, which provides much better diagnostics
     than examining the error stack after a failed SSL_connect.  */
  SSL_CTX_set_verify (ssl_ctx, SSL_VERIFY_NONE, NULL);

  /* Use the private key from the cert file unless otherwise specified. */
  if (opt.cert_file && !opt.private_key)
    {
      opt.private_key = xstrdup (opt.cert_file);
      opt.private_key_type = opt.cert_type;
    }

  /* Use cert from private key file unless otherwise specified. */
  if (opt.private_key && !opt.cert_file)
    {
      opt.cert_file = xstrdup (opt.private_key);
      opt.cert_type = opt.private_key_type;
    }

  if (opt.cert_file)
    if (SSL_CTX_use_certificate_file (ssl_ctx, opt.cert_file,
                                      key_type_to_ssl_type (opt.cert_type))
        != 1)
      goto error;
  if (opt.private_key)
    if (SSL_CTX_use_PrivateKey_file (ssl_ctx, opt.private_key,
                                     key_type_to_ssl_type (opt.private_key_type))
        != 1)
      goto error;

  /* Since fd_write unconditionally assumes partial writes (and
     handles them correctly), allow them in OpenSSL.  */
  SSL_CTX_set_mode (ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);

  /* The OpenSSL library can handle renegotiations automatically, so
     tell it to do so.  */
  SSL_CTX_set_mode (ssl_ctx, SSL_MODE_AUTO_RETRY);

  return true;

 error:
  if (ssl_ctx)
    SSL_CTX_free (ssl_ctx);
  print_errors ();
  return false;
}

struct openssl_transport_context
{
  SSL *conn;                    /* SSL connection handle */
  SSL_SESSION *sess;            /* SSL session info */
  char *last_error;             /* last error printed with openssl_errstr */
};

struct openssl_read_args
{
  int fd;
  struct openssl_transport_context *ctx;
  char *buf;
  int bufsize;
  int retval;
};

static void openssl_read_callback(void *arg)
{
  struct openssl_read_args *args = (struct openssl_read_args *) arg;
  struct openssl_transport_context *ctx = args->ctx;
  SSL *conn = ctx->conn;
  char *buf = args->buf;
  int bufsize = args->bufsize;
  int ret;

  do
    ret = SSL_read (conn, buf, bufsize);
  while (ret == -1 && SSL_get_error (conn, ret) == SSL_ERROR_SYSCALL
         && errno == EINTR);
  args->retval = ret;
}

static int
openssl_read (int fd, char *buf, int bufsize, void *arg)
{
  struct openssl_read_args args;
  args.fd = fd;
  args.buf = buf;
  args.bufsize = bufsize;
  args.ctx = (struct openssl_transport_context*) arg;

  if (run_with_timeout(opt.read_timeout, openssl_read_callback, &args)) {
    return -1;
  }
  return args.retval;
}

static int
openssl_write (int fd _GL_UNUSED, char *buf, int bufsize, void *arg)
{
  int ret = 0;
  struct openssl_transport_context *ctx = arg;
  SSL *conn = ctx->conn;
  do
    ret = SSL_write (conn, buf, bufsize);
  while (ret == -1
         && SSL_get_error (conn, ret) == SSL_ERROR_SYSCALL
         && errno == EINTR);
  return ret;
}

static int
openssl_poll (int fd, double timeout, int wait_for, void *arg)
{
  struct openssl_transport_context *ctx = arg;
  SSL *conn = ctx->conn;
  if (SSL_pending (conn))
    return 1;
  if (timeout == 0)
    return 1;
  return select_fd (fd, timeout, wait_for);
}

static int
openssl_peek (int fd, char *buf, int bufsize, void *arg)
{
  int ret;
  struct openssl_transport_context *ctx = arg;
  SSL *conn = ctx->conn;
  if (! openssl_poll (fd, 0.0, WAIT_FOR_READ, arg))
    return 0;
  do
    ret = SSL_peek (conn, buf, bufsize);
  while (ret == -1
         && SSL_get_error (conn, ret) == SSL_ERROR_SYSCALL
         && errno == EINTR);
  return ret;
}

static const char *
openssl_errstr (int fd _GL_UNUSED, void *arg)
{
  struct openssl_transport_context *ctx = arg;
  unsigned long errcode;
  char *errmsg = NULL;
  int msglen = 0;

  /* If there are no SSL-specific errors, just return NULL. */
  if ((errcode = ERR_get_error ()) == 0)
    return NULL;

  /* Get rid of previous contents of ctx->last_error, if any.  */
  xfree (ctx->last_error);

  /* Iterate over OpenSSL's error stack and accumulate errors in the
     last_error buffer, separated by "; ".  This is better than using
     a static buffer, which *always* takes up space (and has to be
     large, to fit more than one error message), whereas these
     allocations are only performed when there is an actual error.  */

  for (;;)
    {
      const char *str = ERR_error_string (errcode, NULL);
      int len = strlen (str);

      /* Allocate space for the existing message, plus two more chars
         for the "; " separator and one for the terminating \0.  */
      errmsg = xrealloc (errmsg, msglen + len + 2 + 1);
      memcpy (errmsg + msglen, str, len);
      msglen += len;

      /* Get next error and bail out if there are no more. */
      errcode = ERR_get_error ();
      if (errcode == 0)
        break;

      errmsg[msglen++] = ';';
      errmsg[msglen++] = ' ';
    }
  errmsg[msglen] = '\0';

  /* Store the error in ctx->last_error where openssl_close will
     eventually find it and free it.  */
  ctx->last_error = errmsg;

  return errmsg;
}

static void
openssl_close (int fd, void *arg)
{
  struct openssl_transport_context *ctx = arg;
  SSL *conn = ctx->conn;

  SSL_shutdown (conn);
  SSL_free (conn);
  xfree (ctx->last_error);
  xfree (ctx);

  close (fd);

  DEBUGP (("Closed %d/SSL 0x%0*lx\n", fd, PTR_FORMAT (conn)));
}

/* openssl_transport is the singleton that describes the SSL transport
   methods provided by this file.  */

static struct transport_implementation openssl_transport = {
  openssl_read, openssl_write, openssl_poll,
  openssl_peek, openssl_errstr, openssl_close
};

struct scwt_context
{
  SSL *ssl;
  int result;
};

static void
ssl_connect_with_timeout_callback(void *arg)
{
  struct scwt_context *ctx = (struct scwt_context *)arg;
  ctx->result = SSL_connect(ctx->ssl);
}

static const char *
_sni_hostname(const char *hostname)
{
  size_t len = strlen(hostname);

  char *sni_hostname = xmemdup(hostname, len + 1);

  /* Remove trailing dot(s) to fix #47408.
   * Regarding RFC 6066 (SNI): The hostname is represented as a byte
   * string using ASCII encoding without a trailing dot. */
  while (len && sni_hostname[--len] == '.')
    sni_hostname[len] = 0;

  return sni_hostname;
}

/* Perform the SSL handshake on file descriptor FD, which is assumed
   to be connected to an SSL server.  The SSL handle provided by
   OpenSSL is registered with the file descriptor FD using
   fd_register_transport, so that subsequent calls to fd_read,
   fd_write, etc., will use the corresponding SSL functions.

   Returns true on success, false on failure.  */

bool
ssl_connect_wget (int fd, const char *hostname, int *continue_session)
{
  SSL *conn;
  struct scwt_context scwt_ctx;
  struct openssl_transport_context *ctx;

  DEBUGP (("Initiating SSL handshake.\n"));

  assert (ssl_ctx != NULL);
  conn = SSL_new (ssl_ctx);
  if (!conn)
    goto error;
#if OPENSSL_VERSION_NUMBER >= 0x0090806fL && !defined(OPENSSL_NO_TLSEXT)
  /* If the SSL library was built with support for ServerNameIndication
     then use it whenever we have a hostname.  If not, don't, ever. */
  if (! is_valid_ip_address (hostname))
    {
      const char *sni_hostname = _sni_hostname(hostname);

      long rc = SSL_set_tlsext_host_name (conn, sni_hostname);
      xfree(sni_hostname);

      if (rc == 0)
        {
          DEBUGP (("Failed to set TLS server-name indication."));
          goto error;
        }
    }
#endif

  if (continue_session)
    {
      /* attempt to resume a previous SSL session */
      ctx = (struct openssl_transport_context *) fd_transport_context (*continue_session);
      if (!ctx || !ctx->sess || !SSL_set_session (conn, ctx->sess))
        goto error;
    }

#ifndef FD_TO_SOCKET
# define FD_TO_SOCKET(X) (X)
#endif
  if (!SSL_set_fd (conn, FD_TO_SOCKET (fd)))
    goto error;
  SSL_set_connect_state (conn);

  scwt_ctx.ssl = conn;
  if (run_with_timeout(opt.read_timeout, ssl_connect_with_timeout_callback,
                       &scwt_ctx)) {
    DEBUGP (("SSL handshake timed out.\n"));
    goto timeout;
  }
  if (scwt_ctx.result <= 0 || !SSL_is_init_finished(conn))
    goto error;

  ctx = xnew0 (struct openssl_transport_context);
  ctx->conn = conn;
  ctx->sess = SSL_get0_session (conn);
  if (!ctx->sess)
    logprintf (LOG_NOTQUIET, "WARNING: Could not save SSL session data for socket %d\n", fd);

  /* Register FD with Wget's transport layer, i.e. arrange that our
     functions are used for reading, writing, and polling.  */
  fd_register_transport (fd, &openssl_transport, ctx);
  DEBUGP (("Handshake successful; connected socket %d to SSL handle 0x%0*lx\n",
           fd, PTR_FORMAT (conn)));
  return true;

 error:
  DEBUGP (("SSL handshake failed.\n"));
  print_errors ();
 timeout:
  if (conn)
    SSL_free (conn);
  return false;
}

#define ASTERISK_EXCLUDES_DOT   /* mandated by rfc2818 */

/* Return true is STRING (case-insensitively) matches PATTERN, false
   otherwise.  The recognized wildcard character is "*", which matches
   any character in STRING except ".".  Any number of the "*" wildcard
   may be present in the pattern.

   This is used to match of hosts as indicated in rfc2818: "Names may
   contain the wildcard character * which is considered to match any
   single domain name component or component fragment. E.g., *.a.com
   matches foo.a.com but not bar.foo.a.com. f*.com matches foo.com but
   not bar.com [or foo.bar.com]."

   If the pattern contain no wildcards, pattern_match(a, b) is
   equivalent to !strcasecmp(a, b).  */

static bool
pattern_match (const char *pattern, const char *string)
{
  const char *p = pattern, *n = string;
  char c;
  for (; (c = c_tolower (*p++)) != '\0'; n++)
    if (c == '*')
      {
        for (c = c_tolower (*p); c == '*'; c = c_tolower (*++p))
          ;
        for (; *n != '\0'; n++)
          if (c_tolower (*n) == c && pattern_match (p, n))
            return true;
#ifdef ASTERISK_EXCLUDES_DOT
          else if (*n == '.')
            return false;
#endif
        return c == '\0';
      }
    else
      {
        if (c != c_tolower (*n))
          return false;
      }
  return *n == '\0';
}

static char *_get_rfc2253_formatted (X509_NAME *name)
{
  int len;
  char *out = NULL;
  BIO* b;

  if ((b = BIO_new (BIO_s_mem ())))
    {
      if (X509_NAME_print_ex (b, name, 0, XN_FLAG_RFC2253) >= 0
          && (len = BIO_number_written (b)) > 0)
        {
          out = xmalloc (len + 1);
          BIO_read (b, out, len);
          out[len] = 0;
        }
      BIO_free (b);
    }

  return out ? out : xstrdup("");
}

/*
 * Heavily modified from:
 * https://www.owasp.org/index.php/Certificate_and_Public_Key_Pinning#OpenSSL
 */
static bool
pkp_pin_peer_pubkey (X509* cert, const char *pinnedpubkey)
{
  /* Scratch */
  int len1 = 0, len2 = 0;
  char *buff1 = NULL, *temp = NULL;

  /* Result is returned to caller */
  bool result = false;

  /* if a path wasn't specified, don't pin */
  if (!pinnedpubkey)
    return true;

  if (!cert)
    return result;

  /* Begin Gyrations to get the subjectPublicKeyInfo     */
  /* Thanks to Viktor Dukhovni on the OpenSSL mailing list */

  /* https://groups.google.com/group/mailing.openssl.users/browse_thread
   /thread/d61858dae102c6c7 */
  len1 = i2d_X509_PUBKEY (X509_get_X509_PUBKEY (cert), NULL);
  if (len1 < 1)
    goto cleanup; /* failed */

  /* https://www.openssl.org/docs/crypto/buffer.html */
  buff1 = temp = OPENSSL_malloc (len1);
  if (!buff1)
    goto cleanup; /* failed */

  /* https://www.openssl.org/docs/crypto/d2i_X509.html */
  len2 = i2d_X509_PUBKEY (X509_get_X509_PUBKEY (cert), (unsigned char **) &temp);

  /*
   * These checks are verifying we got back the same values as when we
   * sized the buffer. It's pretty weak since they should always be the
   * same. But it gives us something to test.
   */
  if ((len1 != len2) || !temp || ((temp - buff1) != len1))
    goto cleanup; /* failed */

  /* End Gyrations */

  /* The one good exit point */
  result = wg_pin_peer_pubkey (pinnedpubkey, buff1, len1);

 cleanup:
  /* https://www.openssl.org/docs/crypto/buffer.html */
  if (NULL != buff1)
    OPENSSL_free (buff1);

  return result;
}

/* Verify the validity of the certificate presented by the server.
   Also check that the "common name" of the server, as presented by
   its certificate, corresponds to HOST.  (HOST typically comes from
   the URL and is what the user thinks he's connecting to.)

   This assumes that ssl_connect_wget has successfully finished, i.e. that
   the SSL handshake has been performed and that FD is connected to an
   SSL handle.

   If opt.check_cert is true (the default), this returns 1 if the
   certificate is valid, 0 otherwise.  If opt.check_cert is 0, the
   function always returns 1, but should still be called because it
   warns the user about any problems with the certificate.  */

bool
ssl_check_certificate (int fd, const char *host)
{
  X509 *cert;
  GENERAL_NAMES *subjectAltNames;
  char common_name[256];
  long vresult;
  bool success = true;
  bool alt_name_checked = false;
  bool pinsuccess = opt.pinnedpubkey == NULL;

  /* If the user has specified --no-check-cert, we still want to warn
     him about problems with the server's certificate.  */
  const char *severity = opt.check_cert ? _("ERROR") : _("WARNING");

  struct openssl_transport_context *ctx = fd_transport_context (fd);
  SSL *conn = ctx->conn;
  assert (conn != NULL);

  /* The user explicitly said to not check for the certificate.  */
  if (opt.check_cert == CHECK_CERT_QUIET && pinsuccess)
    return success;

  cert = SSL_get_peer_certificate (conn);
  if (!cert)
    {
      logprintf (LOG_NOTQUIET, _("%s: No certificate presented by %s.\n"),
                 severity, quotearg_style (escape_quoting_style, host));
      success = false;
      goto no_cert;             /* must bail out since CERT is NULL */
    }

  IF_DEBUG
    {
      char *subject = _get_rfc2253_formatted (X509_get_subject_name (cert));
      char *issuer = _get_rfc2253_formatted (X509_get_issuer_name (cert));
      DEBUGP (("certificate:\n  subject: %s\n  issuer:  %s\n",
               quotearg_n_style (0, escape_quoting_style, subject),
               quotearg_n_style (1, escape_quoting_style, issuer)));
      xfree (subject);
      xfree (issuer);
    }

  vresult = SSL_get_verify_result (conn);
  if (vresult != X509_V_OK)
    {
      char *issuer = _get_rfc2253_formatted (X509_get_issuer_name (cert));
      logprintf (LOG_NOTQUIET,
                 _("%s: cannot verify %s's certificate, issued by %s:\n"),
                 severity, quotearg_n_style (0, escape_quoting_style, host),
                 quote_n (1, issuer));
      xfree(issuer);

      /* Try to print more user-friendly (and translated) messages for
         the frequent verification errors.  */
      switch (vresult)
        {
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
          logprintf (LOG_NOTQUIET,
                     _("  Unable to locally verify the issuer's authority.\n"));
          break;
        case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
          logprintf (LOG_NOTQUIET,
                     _("  Self-signed certificate encountered.\n"));
          break;
        case X509_V_ERR_CERT_NOT_YET_VALID:
          logprintf (LOG_NOTQUIET, _("  Issued certificate not yet valid.\n"));
          break;
        case X509_V_ERR_CERT_HAS_EXPIRED:
          logprintf (LOG_NOTQUIET, _("  Issued certificate has expired.\n"));
          break;
        default:
          /* For the less frequent error strings, simply provide the
             OpenSSL error message.  */
          logprintf (LOG_NOTQUIET, "  %s\n",
                     X509_verify_cert_error_string (vresult));
        }
      success = false;
      /* Fall through, so that the user is warned about *all* issues
         with the cert (important with --no-check-certificate.)  */
    }

  /* Check that HOST matches the common name in the certificate.
     #### The following remains to be done:

     - When matching against common names, it should loop over all
       common names and choose the most specific one, i.e. the last
       one, not the first one, which the current code picks.

     - Ensure that ASN1 strings from the certificate are encoded as
       UTF-8 which can be meaningfully compared to HOST.  */

  subjectAltNames = X509_get_ext_d2i (cert, NID_subject_alt_name, NULL, NULL);

  if (subjectAltNames)
    {
      /* Test subject alternative names */

      /* SNI hostname must not have a trailing dot */
      const char *sni_hostname = _sni_hostname(host);

      /* Do we want to check for dNSNAmes or ipAddresses (see RFC 2818)?
       * Signal it by host_in_octet_string. */
      ASN1_OCTET_STRING *host_in_octet_string = a2i_IPADDRESS (sni_hostname);

      int numaltnames = sk_GENERAL_NAME_num (subjectAltNames);
      int i;
      for (i=0; i < numaltnames; i++)
        {
          const GENERAL_NAME *name =
            sk_GENERAL_NAME_value (subjectAltNames, i);
          if (name)
            {
              if (host_in_octet_string)
                {
                  if (name->type == GEN_IPADD)
                    {
                      /* Check for ipAddress */
                      /* TODO: Should we convert between IPv4-mapped IPv6
                       * addresses and IPv4 addresses? */
                      alt_name_checked = true;
                      if (!ASN1_STRING_cmp (host_in_octet_string,
                            name->d.iPAddress))
                        break;
                    }
                }
              else if (name->type == GEN_DNS)
                {
                  /* dNSName should be IA5String (i.e. ASCII), however who
                   * does trust CA? Convert it into UTF-8 for sure. */
                  unsigned char *name_in_utf8 = NULL;

                  /* Check for dNSName */
                  alt_name_checked = true;

                  if (0 <= ASN1_STRING_to_UTF8 (&name_in_utf8, name->d.dNSName))
                    {
                      /* Compare and check for NULL attack in ASN1_STRING */
                      if (pattern_match ((char *)name_in_utf8, sni_hostname) &&
                            (strlen ((char *)name_in_utf8) ==
                                (size_t) ASN1_STRING_length (name->d.dNSName)))
                        {
                          OPENSSL_free (name_in_utf8);
                          break;
                        }
                      OPENSSL_free (name_in_utf8);
                    }
                }
            }
        }
      sk_GENERAL_NAME_pop_free(subjectAltNames, GENERAL_NAME_free);
      if (host_in_octet_string)
        ASN1_OCTET_STRING_free(host_in_octet_string);

      if (alt_name_checked == true && i >= numaltnames)
        {
          logprintf (LOG_NOTQUIET,
              _("%s: no certificate subject alternative name matches\n"
                "\trequested host name %s.\n"),
                     severity, quote_n (1, sni_hostname));
          success = false;
        }

      xfree(sni_hostname);
    }

  if (alt_name_checked == false)
    {
      /* Test commomName */
      X509_NAME *xname = X509_get_subject_name(cert);
      common_name[0] = '\0';
      X509_NAME_get_text_by_NID (xname, NID_commonName, common_name,
                                 sizeof (common_name));

      if (!pattern_match (common_name, host))
        {
          logprintf (LOG_NOTQUIET, _("\
    %s: certificate common name %s doesn't match requested host name %s.\n"),
                     severity, quote_n (0, common_name), quote_n (1, host));
          success = false;
        }
      else
        {
          /* We now determine the length of the ASN1 string. If it
           * differs from common_name's length, then there is a \0
           * before the string terminates.  This can be an instance of a
           * null-prefix attack.
           *
           * https://www.blackhat.com/html/bh-usa-09/bh-usa-09-archives.html#Marlinspike
           * */

          int i = -1, j;
          X509_NAME_ENTRY *xentry;
          ASN1_STRING *sdata;

          if (xname) {
            for (;;)
              {
                j = X509_NAME_get_index_by_NID (xname, NID_commonName, i);
                if (j == -1) break;
                i = j;
              }
          }

          xentry = X509_NAME_get_entry(xname,i);
          sdata = X509_NAME_ENTRY_get_data(xentry);
          if (strlen (common_name) != (size_t) ASN1_STRING_length (sdata))
            {
              logprintf (LOG_NOTQUIET, _("\
    %s: certificate common name is invalid (contains a NUL character).\n\
    This may be an indication that the host is not who it claims to be\n\
    (that is, it is not the real %s).\n"),
                         severity, quote (host));
              success = false;
            }
        }
    }

    pinsuccess = pkp_pin_peer_pubkey (cert, opt.pinnedpubkey);
    if (!pinsuccess)
      {
        logprintf (LOG_ALWAYS, _("The public key does not match pinned public key!\n"));
        success = false;
      }


  if (success)
    DEBUGP (("X509 certificate successfully verified and matches host %s\n",
             quotearg_style (escape_quoting_style, host)));
  X509_free (cert);

 no_cert:
  if (opt.check_cert == CHECK_CERT_ON && !success)
    logprintf (LOG_NOTQUIET, _("\
To connect to %s insecurely, use `--no-check-certificate'.\n"),
               quotearg_style (escape_quoting_style, host));

  /* never return true if pinsuccess fails */
  return !pinsuccess ? false : (opt.check_cert == CHECK_CERT_ON ? success : true);
}

/*
 * vim: tabstop=2 shiftwidth=2 softtabstop=2
 */
