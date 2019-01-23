/* SSL support via GnuTLS library.
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2015
   Free Software Foundation, Inc.

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

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <xalloc.h>

#include <gnutls/abstract.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <sys/ioctl.h>

#include "utils.h"
#include "connect.h"
#include "url.h"
#include "ptimer.h"
#include "hash.h"
#include "ssl.h"

#include <fcntl.h>

#ifdef WIN32
# include "w32sock.h"
#endif

#include "host.h"

static int
_do_handshake (gnutls_session_t session, int fd, double timeout);

static int
key_type_to_gnutls_type (enum keyfile_type type)
{
  switch (type)
    {
    case keyfile_pem:
      return GNUTLS_X509_FMT_PEM;
    case keyfile_asn1:
      return GNUTLS_X509_FMT_DER;
    default:
      abort ();
    }
}

/* Note: some of the functions private to this file have names that
   begin with "wgnutls_" (e.g. wgnutls_read) so that they wouldn't be
   confused with actual gnutls functions -- such as the gnutls_read
   preprocessor macro.  */

static gnutls_certificate_credentials_t credentials;
bool
ssl_init (void)
{
  /* Becomes true if GnuTLS is initialized. */
  static bool ssl_initialized = false;
  const char *ca_directory;
  DIR *dir;
  int ncerts = -1;

  /* GnuTLS should be initialized only once. */
  if (ssl_initialized)
    return true;

  gnutls_global_init ();
  gnutls_certificate_allocate_credentials (&credentials);
  gnutls_certificate_set_verify_flags (credentials,
                                       GNUTLS_VERIFY_ALLOW_X509_V1_CA_CRT);

#if GNUTLS_VERSION_MAJOR >= 3
  if (!opt.ca_directory)
    ncerts = gnutls_certificate_set_x509_system_trust (credentials);
#endif

  /* If GnuTLS version is too old or CA loading failed, fallback to old behaviour.
   * Also use old behaviour if the CA directory is user-provided.  */
  if (ncerts <= 0)
    {
      ca_directory = opt.ca_directory ? opt.ca_directory : "/etc/ssl/certs";
      if ((dir = opendir (ca_directory)) == NULL)
        {
          if (opt.ca_directory && *opt.ca_directory)
            logprintf (LOG_NOTQUIET, _("ERROR: Cannot open directory %s.\n"),
                       opt.ca_directory);
        }
      else
        {
          struct hash_table *inode_map = hash_table_new (196, NULL, NULL);
          struct dirent *dent;
          size_t dirlen = strlen(ca_directory);
          int rc;

          ncerts = 0;

          while ((dent = readdir (dir)) != NULL)
            {
              struct stat st;
              size_t ca_file_length = dirlen + strlen(dent->d_name) + 2;
              char *ca_file = alloca(ca_file_length);

              snprintf (ca_file, ca_file_length, "%s/%s", ca_directory, dent->d_name);
              if (stat (ca_file, &st) != 0)
                continue;

              if (! S_ISREG (st.st_mode))
                continue;

              /* avoid loading the same file twice by checking the inode.  */
              if (hash_table_contains (inode_map, (void *)(intptr_t) st.st_ino))
                continue;

              hash_table_put (inode_map, (void *)(intptr_t) st.st_ino, NULL);
              if ((rc = gnutls_certificate_set_x509_trust_file (credentials, ca_file,
                                                                GNUTLS_X509_FMT_PEM)) <= 0)
                DEBUGP (("WARNING: Failed to open cert %s: (%d).\n", ca_file, rc));
              else
                ncerts += rc;
            }

          hash_table_destroy (inode_map);
          closedir (dir);
        }
    }

  if (opt.ca_cert)
    {
      int rc;

      ncerts = 0;

      if ((rc = gnutls_certificate_set_x509_trust_file (credentials, opt.ca_cert,
                                                        GNUTLS_X509_FMT_PEM)) <= 0)
        logprintf (LOG_NOTQUIET, _ ("ERROR: Failed to open cert %s: (%d).\n"),
                   opt.ca_cert, rc);
      else
        {
          ncerts += rc;
          logprintf (LOG_NOTQUIET, _ ("Loaded CA certificate '%s'\n"), opt.ca_cert);
        }
    }

  if (opt.crl_file)
    {
      int rc;

      if ((rc = gnutls_certificate_set_x509_crl_file (credentials, opt.crl_file, GNUTLS_X509_FMT_PEM)) <= 0)
        {
          logprintf (LOG_NOTQUIET, _("ERROR: Failed to load CRL file '%s': (%d)\n"), opt.crl_file, rc);
          return false;
        }

      logprintf (LOG_NOTQUIET, _ ("Loaded CRL file '%s'\n"), opt.crl_file);
    }

  DEBUGP (("Certificates loaded: %d\n", ncerts));

  /* Use the private key from the cert file unless otherwise specified. */
  if (opt.cert_file && !opt.private_key)
    {
      opt.private_key = xstrdup (opt.cert_file);
      opt.private_key_type = opt.cert_type;
    }
  /* Use the cert from the private key file unless otherwise specified. */
  if (!opt.cert_file && opt.private_key)
    {
      opt.cert_file = xstrdup (opt.private_key);
      opt.cert_type = opt.private_key_type;
    }

  if (opt.cert_file && opt.private_key)
    {
      int type;
      if (opt.private_key_type != opt.cert_type)
        {
          /* GnuTLS can't handle this */
          logprintf (LOG_NOTQUIET, _("ERROR: GnuTLS requires the key and the \
cert to be of the same type.\n"));
        }

      type = key_type_to_gnutls_type (opt.private_key_type);

      gnutls_certificate_set_x509_key_file (credentials, opt.cert_file,
                                            opt.private_key,
                                            type);
    }

  ssl_initialized = true;

  return true;
}

struct wgnutls_transport_context
{
  gnutls_session_t session;       /* GnuTLS session handle */
  gnutls_datum_t *session_data;
  int last_error;               /* last error returned by read/write/... */

  /* Since GnuTLS doesn't support the equivalent to recv(...,
     MSG_PEEK) or SSL_peek(), we have to do it ourselves.  Peeked data
     is stored to PEEKBUF, and wgnutls_read checks that buffer before
     actually reading.  */
  char peekbuf[512];
  int peeklen;
};

static int
wgnutls_read_timeout (int fd, char *buf, int bufsize, void *arg, double timeout)
{
#ifdef F_GETFL
  int flags = 0;
#endif
  int ret = 0;
  struct ptimer *timer = NULL;
  struct wgnutls_transport_context *ctx = arg;
  int timed_out = 0;

  if (timeout)
    {
#ifdef F_GETFL
      flags = fcntl (fd, F_GETFL, 0);
      if (flags < 0)
        return flags;
      if (fcntl (fd, F_SETFL, flags | O_NONBLOCK))
        return -1;
#else
      /* XXX: Assume it was blocking before.  */
      const int one = 1;
      if (ioctl (fd, FIONBIO, &one) < 0)
        return -1;
#endif

      timer = ptimer_new ();
      if (timer == NULL)
        return -1;
    }

  do
    {
      double next_timeout = 0;
      if (timeout)
        {
          next_timeout = timeout - ptimer_measure (timer);
          if (next_timeout < 0)
            break;
        }

      ret = GNUTLS_E_AGAIN;
      if (timeout == 0 || gnutls_record_check_pending (ctx->session)
          || select_fd (fd, next_timeout, WAIT_FOR_READ))
        {
          ret = gnutls_record_recv (ctx->session, buf, bufsize);
          timed_out = timeout && ptimer_measure (timer) >= timeout;
          if (!timed_out && ret == GNUTLS_E_REHANDSHAKE)
            {
              DEBUGP (("GnuTLS: *** REHANDSHAKE while reading\n"));
              if ((ret = _do_handshake (ctx->session, fd, timeout)) == 0)
                ret = GNUTLS_E_AGAIN; /* restart reading */
            }
        }
    }
  while (ret == GNUTLS_E_INTERRUPTED || (ret == GNUTLS_E_AGAIN && !timed_out));

  if (timeout)
    {
      ptimer_destroy (timer);

#ifdef F_GETFL
      if (fcntl (fd, F_SETFL, flags) < 0)
        return -1;
#else
      const int zero = 0;
      if (ioctl (fd, FIONBIO, &zero) < 0)
        return -1;
#endif

      if (timed_out && ret == GNUTLS_E_AGAIN)
        errno = ETIMEDOUT;
    }

  return ret;
}

static int
wgnutls_read (int fd, char *buf, int bufsize, void *arg)
{
  int ret = 0;
  struct wgnutls_transport_context *ctx = arg;

  if (ctx->peeklen)
    {
      /* If we have any peek data, simply return that. */
      int copysize = MIN (bufsize, ctx->peeklen);
      memcpy (buf, ctx->peekbuf, copysize);
      ctx->peeklen -= copysize;
      if (ctx->peeklen != 0)
        memmove (ctx->peekbuf, ctx->peekbuf + copysize, ctx->peeklen);

      return copysize;
    }

  ret = wgnutls_read_timeout (fd, buf, bufsize, arg, opt.read_timeout);
  if (ret < 0)
    ctx->last_error = ret;

  return ret;
}

static int
wgnutls_write (int fd _GL_UNUSED, char *buf, int bufsize, void *arg)
{
  int ret;
  struct wgnutls_transport_context *ctx = arg;
  do
    ret = gnutls_record_send (ctx->session, buf, bufsize);
  while (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN);
  if (ret < 0)
    ctx->last_error = ret;
  return ret;
}

static int
wgnutls_poll (int fd, double timeout, int wait_for, void *arg)
{
  struct wgnutls_transport_context *ctx = arg;

  if (timeout)
    return ctx->peeklen || gnutls_record_check_pending (ctx->session)
      || select_fd (fd, timeout, wait_for);
  else
    return ctx->peeklen || gnutls_record_check_pending (ctx->session);
}

static int
wgnutls_peek (int fd, char *buf, int bufsize, void *arg)
{
  int read = 0;
  struct wgnutls_transport_context *ctx = arg;
  int offset = MIN (bufsize, ctx->peeklen);

  if (ctx->peeklen)
    {
      memcpy (buf, ctx->peekbuf, offset);
      return offset;
    }

  if (bufsize > (int) sizeof ctx->peekbuf)
    bufsize = sizeof ctx->peekbuf;

  if (bufsize > offset)
    {
      if (opt.read_timeout && gnutls_record_check_pending (ctx->session) == 0
          && select_fd (fd, 0.0, WAIT_FOR_READ) <= 0)
        read = 0;
      else
        read = wgnutls_read_timeout (fd, buf + offset, bufsize - offset,
                                     ctx, opt.read_timeout);
      if (read < 0)
        {
          if (offset)
            read = 0;
          else
            return read;
        }

      if (read > 0)
        {
          memcpy (ctx->peekbuf + offset, buf + offset,
                  read);
          ctx->peeklen += read;
        }
    }

  return offset + read;
}

static const char *
wgnutls_errstr (int fd _GL_UNUSED, void *arg)
{
  struct wgnutls_transport_context *ctx = arg;
  return gnutls_strerror (ctx->last_error);
}

static void
wgnutls_close (int fd, void *arg)
{
  struct wgnutls_transport_context *ctx = arg;
  /*gnutls_bye (ctx->session, GNUTLS_SHUT_RDWR);*/
  if (ctx->session_data)
    {
      gnutls_free (ctx->session_data->data);
      gnutls_free (ctx->session_data);
    }
  gnutls_deinit (ctx->session);
  xfree (ctx);
  close (fd);
}

/* gnutls_transport is the singleton that describes the SSL transport
   methods provided by this file.  */

static struct transport_implementation wgnutls_transport =
{
  wgnutls_read, wgnutls_write, wgnutls_poll,
  wgnutls_peek, wgnutls_errstr, wgnutls_close
};

static int
_do_handshake (gnutls_session_t session, int fd, double timeout)
{
#ifdef F_GETFL
  int flags = 0;
#endif
  int err;

  if (timeout)
    {
#ifdef F_GETFL
      flags = fcntl (fd, F_GETFL, 0);
      if (flags < 0)
        return flags;
      if (fcntl (fd, F_SETFL, flags | O_NONBLOCK))
        return -1;
#else
      /* XXX: Assume it was blocking before.  */
      const int one = 1;
      if (ioctl (fd, FIONBIO, &one) < 0)
        return -1;
#endif
    }

  /* We don't stop the handshake process for non-fatal errors */
  do
    {
      err = gnutls_handshake (session);

      if (timeout && err == GNUTLS_E_AGAIN)
        {
          if (gnutls_record_get_direction (session))
            {
              /* wait for writeability */
              err = select_fd (fd, timeout, WAIT_FOR_WRITE);
            }
          else
            {
              /* wait for readability */
              err = select_fd (fd, timeout, WAIT_FOR_READ);
            }

          if (err <= 0)
            {
              if (err == 0)
                {
                  errno = ETIMEDOUT;
                  err = -1;
                }
              break;
            }

           err = GNUTLS_E_AGAIN;
        }
      else if (err < 0)
        {
          logprintf (LOG_NOTQUIET, "GnuTLS: %s\n", gnutls_strerror (err));
          if (err == GNUTLS_E_WARNING_ALERT_RECEIVED ||
              err == GNUTLS_E_FATAL_ALERT_RECEIVED)
            {
              gnutls_alert_description_t alert = gnutls_alert_get (session);
              const char *str = gnutls_alert_get_name (alert);
              logprintf (LOG_NOTQUIET, "GnuTLS: received alert [%u]: %s\n",
                         alert, str ? str : "(unknown)");
            }
        }
    }
  while (err && gnutls_error_is_fatal (err) == 0);

  if (timeout)
    {
#ifdef F_GETFL
      if (fcntl (fd, F_SETFL, flags) < 0)
        return -1;
#else
      const int zero = 0;
      if (ioctl (fd, FIONBIO, &zero) < 0)
        return -1;
#endif
    }

  return err;
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

bool
ssl_connect_wget (int fd, const char *hostname, int *continue_session)
{
  struct wgnutls_transport_context *ctx;
  gnutls_session_t session;
  int err;

  gnutls_init (&session, GNUTLS_CLIENT);

  /* We set the server name but only if it's not an IP address. */
  if (! is_valid_ip_address (hostname))
    {
      /* GnuTLS 3.4.x (x<=10) disrespects the length parameter, we have to construct a new string */
      /* see https://gitlab.com/gnutls/gnutls/issues/78 */
      const char *sni_hostname = _sni_hostname(hostname);

      gnutls_server_name_set (session, GNUTLS_NAME_DNS, sni_hostname, strlen(sni_hostname));
      xfree(sni_hostname);
    }

  gnutls_credentials_set (session, GNUTLS_CRD_CERTIFICATE, credentials);
#ifndef FD_TO_SOCKET
# define FD_TO_SOCKET(X) (X)
#endif
#ifdef HAVE_INTPTR_T
  gnutls_transport_set_ptr (session, (gnutls_transport_ptr_t) (intptr_t) FD_TO_SOCKET (fd));
#else
  gnutls_transport_set_ptr (session, (gnutls_transport_ptr_t) FD_TO_SOCKET (fd));
#endif

#if HAVE_GNUTLS_PRIORITY_SET_DIRECT
  switch (opt.secure_protocol)
    {
    case secure_protocol_auto:
      err = gnutls_set_default_priority (session);
      gnutls_session_enable_compatibility_mode(session);
      break;

    case secure_protocol_sslv2:
    case secure_protocol_sslv3:
      err = gnutls_priority_set_direct (session, "NORMAL:-VERS-TLS-ALL:+VERS-SSL3.0", NULL);
      break;

    case secure_protocol_tlsv1:
      err = gnutls_priority_set_direct (session, "NORMAL:-VERS-SSL3.0", NULL);
      break;

    case secure_protocol_tlsv1_1:
      err = gnutls_priority_set_direct (session, "NORMAL:-VERS-SSL3.0:-VERS-TLS1.0", NULL);
      break;

    case secure_protocol_tlsv1_2:
      err = gnutls_priority_set_direct (session, "NORMAL:-VERS-SSL3.0:-VERS-TLS1.0:-VERS-TLS1.1", NULL);
      break;

    case secure_protocol_pfs:
      err = gnutls_priority_set_direct (session, "PFS:-VERS-SSL3.0", NULL);
      if (err != GNUTLS_E_SUCCESS)
        /* fallback if PFS is not available */
        err = gnutls_priority_set_direct (session, "NORMAL:-RSA:-VERS-SSL3.0", NULL);
      break;

    default:
      logprintf (LOG_NOTQUIET, _("GnuTLS: unimplemented 'secure-protocol' option value %u\n"),
                 (unsigned) opt.secure_protocol);
      logprintf (LOG_NOTQUIET, _("Please report this issue to bug-wget@gnu.org\n"));
      abort ();
    }
#else
  int allowed_protocols[4] = {0, 0, 0, 0};
  switch (opt.secure_protocol)
    {
    case secure_protocol_auto:
      err = gnutls_set_default_priority (session);
      break;

    case secure_protocol_sslv2:
    case secure_protocol_sslv3:
      allowed_protocols[0] = GNUTLS_SSL3;
      err = gnutls_protocol_set_priority (session, allowed_protocols);
      break;

    case secure_protocol_tlsv1:
      allowed_protocols[0] = GNUTLS_TLS1_0;
      allowed_protocols[1] = GNUTLS_TLS1_1;
      allowed_protocols[2] = GNUTLS_TLS1_2;
      err = gnutls_protocol_set_priority (session, allowed_protocols);
      break;

    case secure_protocol_tlsv1_1:
      allowed_protocols[0] = GNUTLS_TLS1_1;
      allowed_protocols[1] = GNUTLS_TLS1_2;
      err = gnutls_protocol_set_priority (session, allowed_protocols);
      break;

    case secure_protocol_tlsv1_2:
      allowed_protocols[0] = GNUTLS_TLS1_2;
      err = gnutls_protocol_set_priority (session, allowed_protocols);
      break;

    default:
      logprintf (LOG_NOTQUIET, _("GnuTLS: unimplemented 'secure-protocol' option value %d\n"), opt.secure_protocol);
      logprintf (LOG_NOTQUIET, _("Please report this issue to bug-wget@gnu.org\n"));
      abort ();
    }
#endif

  if (err < 0)
    {
      logprintf (LOG_NOTQUIET, "GnuTLS: %s\n", gnutls_strerror (err));
      gnutls_deinit (session);
      return false;
    }

  if (continue_session)
    {
      ctx = (struct wgnutls_transport_context *) fd_transport_context (*continue_session);
      if (!gnutls_session_is_resumed (session))
        {
          if (!ctx || !ctx->session_data || gnutls_session_set_data (session, ctx->session_data->data, ctx->session_data->size))
            {
              if (ctx && ctx->session_data)
                {
                  /* server does not want to continue the session */
                  if (ctx->session_data->data)
                    gnutls_free (ctx->session_data->data);
                  gnutls_free (ctx->session_data);
                }
              gnutls_deinit (session);
              return false;
            }
        }
      else
        {
          logputs (LOG_ALWAYS, "SSL session has already been resumed. Continuing.\n");
          continue_session = NULL;
        }
    }

  err = _do_handshake (session, fd, opt.connect_timeout);

  if (err < 0)
    {
      gnutls_deinit (session);
      return false;
    }

  ctx = xnew0 (struct wgnutls_transport_context);
  ctx->session_data = xnew0 (gnutls_datum_t);
  ctx->session = session;
  if (gnutls_session_get_data2 (session, ctx->session_data))
    {
      xfree (ctx->session_data);
      logprintf (LOG_NOTQUIET, "WARNING: Could not save SSL session data for socket %d\n", fd);
    }
  fd_register_transport (fd, &wgnutls_transport, ctx);
  return true;
}

static bool
pkp_pin_peer_pubkey (gnutls_x509_crt_t cert, const char *pinnedpubkey)
{
  /* Scratch */
  size_t len1 = 0, len2 = 0;
  char *buff1 = NULL;

  gnutls_pubkey_t key = NULL;

  /* Result is returned to caller */
  int ret = 0;
  bool result = false;

  /* if a path wasn't specified, don't pin */
  if (NULL == pinnedpubkey)
    return true;

  if (NULL == cert)
    return result;

  /* Begin Gyrations to get the public key     */
  gnutls_pubkey_init (&key);

  ret = gnutls_pubkey_import_x509 (key, cert, 0);
  if (ret < 0)
    goto cleanup; /* failed */

  ret = gnutls_pubkey_export (key, GNUTLS_X509_FMT_DER, NULL, &len1);
  if (ret != GNUTLS_E_SHORT_MEMORY_BUFFER || len1 == 0)
    goto cleanup; /* failed */

  buff1 = xmalloc (len1);

  len2 = len1;

  ret = gnutls_pubkey_export (key, GNUTLS_X509_FMT_DER, buff1, &len2);
  if (ret < 0 || len1 != len2)
    goto cleanup; /* failed */

  /* End Gyrations */

  /* The one good exit point */
  result = wg_pin_peer_pubkey (pinnedpubkey, buff1, len1);

 cleanup:
  if (NULL != key)
    gnutls_pubkey_deinit (key);

  xfree (buff1);

  return result;
}

#define _CHECK_CERT(flag,msg) \
  if (status & (flag))\
    {\
      logprintf (LOG_NOTQUIET, (msg),\
                 severity, quote (host));\
      success = false;\
    }

bool
ssl_check_certificate (int fd, const char *host)
{
  struct wgnutls_transport_context *ctx = fd_transport_context (fd);

  unsigned int status;
  int err;

  /* If the user has specified --no-check-cert, we still want to warn
     him about problems with the server's certificate.  */
  const char *severity = opt.check_cert ? _("ERROR") : _("WARNING");
  bool success = true;
  bool pinsuccess = opt.pinnedpubkey == NULL;

  /* The user explicitly said to not check for the certificate.  */
  if (opt.check_cert == CHECK_CERT_QUIET && pinsuccess)
    return success;

  err = gnutls_certificate_verify_peers2 (ctx->session, &status);
  if (err < 0)
    {
      logprintf (LOG_NOTQUIET, _("%s: No certificate presented by %s.\n"),
                 severity, quotearg_style (escape_quoting_style, host));
      success = false;
      goto out;
    }

  _CHECK_CERT (GNUTLS_CERT_INVALID, _("%s: The certificate of %s is not trusted.\n"));
  _CHECK_CERT (GNUTLS_CERT_SIGNER_NOT_FOUND, _("%s: The certificate of %s hasn't got a known issuer.\n"));
  _CHECK_CERT (GNUTLS_CERT_REVOKED, _("%s: The certificate of %s has been revoked.\n"));
  _CHECK_CERT (GNUTLS_CERT_SIGNER_NOT_CA, _("%s: The certificate signer of %s was not a CA.\n"));
  _CHECK_CERT (GNUTLS_CERT_INSECURE_ALGORITHM, _("%s: The certificate of %s was signed using an insecure algorithm.\n"));
  _CHECK_CERT (GNUTLS_CERT_NOT_ACTIVATED, _("%s: The certificate of %s is not yet activated.\n"));
  _CHECK_CERT (GNUTLS_CERT_EXPIRED, _("%s: The certificate of %s has expired.\n"));

  if (gnutls_certificate_type_get (ctx->session) == GNUTLS_CRT_X509)
    {
      time_t now = time (NULL);
      gnutls_x509_crt_t cert;
      const gnutls_datum_t *cert_list;
      unsigned int cert_list_size;
      const char *sni_hostname;

      if ((err = gnutls_x509_crt_init (&cert)) < 0)
        {
          logprintf (LOG_NOTQUIET, _("Error initializing X509 certificate: %s\n"),
                     gnutls_strerror (err));
          success = false;
          goto out;
        }

      cert_list = gnutls_certificate_get_peers (ctx->session, &cert_list_size);
      if (!cert_list)
        {
          logprintf (LOG_NOTQUIET, _("No certificate found\n"));
          success = false;
          goto crt_deinit;
        }
      err = gnutls_x509_crt_import (cert, cert_list, GNUTLS_X509_FMT_DER);
      if (err < 0)
        {
          logprintf (LOG_NOTQUIET, _("Error parsing certificate: %s\n"),
                     gnutls_strerror (err));
          success = false;
          goto crt_deinit;
        }
      if (now < gnutls_x509_crt_get_activation_time (cert))
        {
          logprintf (LOG_NOTQUIET, _("The certificate has not yet been activated\n"));
          success = false;
        }
      if (now >= gnutls_x509_crt_get_expiration_time (cert))
        {
          logprintf (LOG_NOTQUIET, _("The certificate has expired\n"));
          success = false;
        }
      sni_hostname = _sni_hostname(host);
      if (!gnutls_x509_crt_check_hostname (cert, sni_hostname))
        {
          logprintf (LOG_NOTQUIET,
                     _("The certificate's owner does not match hostname %s\n"),
                     quote (sni_hostname));
          success = false;
        }
      xfree(sni_hostname);

      pinsuccess = pkp_pin_peer_pubkey (cert, opt.pinnedpubkey);
      if (!pinsuccess)
        {
          logprintf (LOG_ALWAYS, _("The public key does not match pinned public key!\n"));
          success = false;
        }

 crt_deinit:
      gnutls_x509_crt_deinit (cert);
    }
  else
    {
      logprintf (LOG_NOTQUIET, _("Certificate must be X.509\n"));
      success = false;
    }

 out:
  /* never return true if pinsuccess fails */
  return !pinsuccess ? false : (opt.check_cert == CHECK_CERT_ON ? success : true);
}
