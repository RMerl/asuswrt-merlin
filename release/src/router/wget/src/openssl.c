/* SSL support via OpenSSL library.
   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008,
   2009, 2010, 2011, 2012 Free Software Foundation, Inc.
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

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#if OPENSSL_VERSION_NUMBER >= 0x00907000
#include <openssl/conf.h>
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

  /* Get random data from EGD if opt.egd_file was used.  */
  if (opt.egd_file && *opt.egd_file)
    RAND_egd (opt.egd_file);

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
#if OPENSSL_VERSION_NUMBER >= 0x00907000
  if (ssl_true_initialized == 0)
    {
      OPENSSL_config (NULL);
      ssl_true_initialized = 1;
    }
#endif

  SSL_METHOD const *meth;

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
  SSLeay_add_all_algorithms ();
  SSLeay_add_ssl_algorithms ();

  switch (opt.secure_protocol)
    {
#ifndef OPENSSL_NO_SSL2
    case secure_protocol_sslv2:
      meth = SSLv2_client_method ();
      break;
#endif
    case secure_protocol_sslv3:
      meth = SSLv3_client_method ();
      break;
    case secure_protocol_auto:
    case secure_protocol_pfs:
    case secure_protocol_tlsv1:
      meth = TLSv1_client_method ();
      break;
#if OPENSSL_VERSION_NUMBER < 0x01001000
    case secure_protocol_tlsv1_1:
      meth = TLSv1_1_client_method ();
      break;
    case secure_protocol_tlsv1_2:
      meth = TLSv1_2_client_method ();
      break;
#endif
    default:
      abort ();
    }

  /* The type cast below accommodates older OpenSSL versions (0.9.8)
     where SSL_CTX_new() is declared without a "const" argument. */
  ssl_ctx = SSL_CTX_new ((SSL_METHOD *)meth);
  if (!ssl_ctx)
    goto error;

  /* OpenSSL ciphers: https://www.openssl.org/docs/apps/ciphers.html
   * Since we want a good protection, we also use HIGH (that excludes MD4 ciphers and some more)
   */
  if (opt.secure_protocol == secure_protocol_pfs)
    SSL_CTX_set_cipher_list (ssl_ctx, "HIGH:MEDIUM:!RC4:!SRP:!PSK:!RSA:!aNULL@STRENGTH");

  SSL_CTX_set_default_verify_paths (ssl_ctx);
  SSL_CTX_load_verify_locations (ssl_ctx, opt.ca_cert, opt.ca_directory);

  /* SSL_VERIFY_NONE instructs OpenSSL not to abort SSL_connect if the
     certificate is invalid.  We verify the certificate separately in
     ssl_check_certificate, which provides much better diagnostics
     than examining the error stack after a failed SSL_connect.  */
  SSL_CTX_set_verify (ssl_ctx, SSL_VERIFY_NONE, NULL);

  /* Use the private key from the cert file unless otherwise specified. */
  if (opt.cert_file && !opt.private_key)
    {
      opt.private_key = opt.cert_file;
      opt.private_key_type = opt.cert_type;
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
  xfree_null (ctx->last_error);

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
  xfree_null (ctx->last_error);
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

/* Perform the SSL handshake on file descriptor FD, which is assumed
   to be connected to an SSL server.  The SSL handle provided by
   OpenSSL is registered with the file descriptor FD using
   fd_register_transport, so that subsequent calls to fd_read,
   fd_write, etc., will use the corresponding SSL functions.

   Returns true on success, false on failure.  */

bool
ssl_connect_wget (int fd, const char *hostname)
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
  /* If the SSL library was build with support for ServerNameIndication
     then use it whenever we have a hostname.  If not, don't, ever. */
  if (! is_valid_ip_address (hostname))
    {
      if (! SSL_set_tlsext_host_name (conn, hostname))
        {
          DEBUGP (("Failed to set TLS server-name indication."));
          goto error;
        }
    }
#endif

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
  if (scwt_ctx.result <= 0 || conn->state != SSL_ST_OK)
    goto error;

  ctx = xnew0 (struct openssl_transport_context);
  ctx->conn = conn;

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

  /* If the user has specified --no-check-cert, we still want to warn
     him about problems with the server's certificate.  */
  const char *severity = opt.check_cert ? _("ERROR") : _("WARNING");

  struct openssl_transport_context *ctx = fd_transport_context (fd);
  SSL *conn = ctx->conn;
  assert (conn != NULL);

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
      char *subject = X509_NAME_oneline (X509_get_subject_name (cert), 0, 0);
      char *issuer = X509_NAME_oneline (X509_get_issuer_name (cert), 0, 0);
      DEBUGP (("certificate:\n  subject: %s\n  issuer:  %s\n",
               quotearg_n_style (0, escape_quoting_style, subject),
               quotearg_n_style (1, escape_quoting_style, issuer)));
      OPENSSL_free (subject);
      OPENSSL_free (issuer);
    }

  vresult = SSL_get_verify_result (conn);
  if (vresult != X509_V_OK)
    {
      char *issuer = X509_NAME_oneline (X509_get_issuer_name (cert), 0, 0);
      logprintf (LOG_NOTQUIET,
                 _("%s: cannot verify %s's certificate, issued by %s:\n"),
                 severity, quotearg_n_style (0, escape_quoting_style, host),
                 quote_n (1, issuer));
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

      /* Do we want to check for dNSNAmes or ipAddresses (see RFC 2818)?
       * Signal it by host_in_octet_string. */
      ASN1_OCTET_STRING *host_in_octet_string = a2i_IPADDRESS (host);

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
                      if (pattern_match ((char *)name_in_utf8, host) &&
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
      sk_GENERAL_NAME_free (subjectAltNames);
      if (host_in_octet_string)
        ASN1_OCTET_STRING_free(host_in_octet_string);

      if (alt_name_checked == true && i >= numaltnames)
        {
          logprintf (LOG_NOTQUIET,
              _("%s: no certificate subject alternative name matches\n"
                "\trequested host name %s.\n"),
                     severity, quote_n (1, host));
          success = false;
        }
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


  if (success)
    DEBUGP (("X509 certificate successfully verified and matches host %s\n",
             quotearg_style (escape_quoting_style, host)));
  X509_free (cert);

 no_cert:
  if (opt.check_cert && !success)
    logprintf (LOG_NOTQUIET, _("\
To connect to %s insecurely, use `--no-check-certificate'.\n"),
               quotearg_style (escape_quoting_style, host));

  /* Allow --no-check-cert to disable certificate checking. */
  return opt.check_cert ? success : true;
}

/*
 * vim: tabstop=2 shiftwidth=2 softtabstop=2
 */
