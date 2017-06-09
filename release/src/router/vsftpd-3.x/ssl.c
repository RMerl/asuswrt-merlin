/*
 * Part of Very Secure FTPd
 * Licence: GPL v2. Note that this code interfaces with with the OpenSSL
 * libraries, so please read LICENSE where I give explicit permission to link
 * against the OpenSSL libraries.
 * Author: Chris Evans
 * ssl.c
 *
 * Routines to handle a SSL/TLS-based implementation of RFC 2228, i.e.
 * encryption.
 */

#include "ssl.h"
#include "session.h"
#include "ftpcodes.h"
#include "ftpcmdio.h"
#include "defs.h"
#include "str.h"
#include "sysutil.h"
#include "tunables.h"
#include "utility.h"
#include "builddefs.h"
#include "logging.h"

#ifdef VSF_BUILD_SSL

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <errno.h>
#include <limits.h>

static char* get_ssl_error();
static SSL* get_ssl(struct vsf_session* p_sess, int fd);
static int ssl_session_init(struct vsf_session* p_sess);
static void setup_bio_callbacks();
static long bio_callback(
  BIO* p_bio, int oper, const char* p_arg, int argi, long argl, long retval);
static int ssl_verify_callback(int verify_ok, X509_STORE_CTX* p_ctx);
static int ssl_cert_digest(
  SSL* p_ssl, struct vsf_session* p_sess, struct mystr* p_str);
static void maybe_log_shutdown_state(struct vsf_session* p_sess);
static void maybe_log_ssl_error_state(struct vsf_session* p_sess, int ret);
static int ssl_read_common(struct vsf_session* p_sess,
                           SSL* p_ssl,
                           char* p_buf,
                           unsigned int len,
                           int (*p_ssl_func)(SSL*, void*, int));

static int ssl_inited;
static struct mystr debug_str;

void
ssl_init(struct vsf_session* p_sess)
{
  if (!ssl_inited)
  {
    SSL_CTX* p_ctx;
    long options;
    int verify_option = 0;
    SSL_library_init();
    p_ctx = SSL_CTX_new(SSLv23_server_method());
    if (p_ctx == NULL)
    {
      die("SSL: could not allocate SSL context");
    }
    options = SSL_OP_ALL;
    if (!tunable_sslv2)
    {
      options |= SSL_OP_NO_SSLv2;
    }
    if (!tunable_sslv3)
    {
      options |= SSL_OP_NO_SSLv3;
    }
    if (!tunable_tlsv1)
    {
      options |= SSL_OP_NO_TLSv1;
    }
    SSL_CTX_set_options(p_ctx, options);
    if (tunable_rsa_cert_file)
    {
      const char* p_key = tunable_rsa_private_key_file;
      if (!p_key)
      {
        p_key = tunable_rsa_cert_file;
      }
      if (SSL_CTX_use_certificate_chain_file(p_ctx, tunable_rsa_cert_file) != 1)
      {
        die("SSL: cannot load RSA certificate");
      }
      if (SSL_CTX_use_PrivateKey_file(p_ctx, p_key, X509_FILETYPE_PEM) != 1)
      {
        die("SSL: cannot load RSA private key");
      }
    }
    if (tunable_dsa_cert_file)
    {
      const char* p_key = tunable_dsa_private_key_file;
      if (!p_key)
      {
        p_key = tunable_dsa_cert_file;
      }
      if (SSL_CTX_use_certificate_chain_file(p_ctx, tunable_dsa_cert_file) != 1)
      {
        die("SSL: cannot load DSA certificate");
      }
      if (SSL_CTX_use_PrivateKey_file(p_ctx, p_key, X509_FILETYPE_PEM) != 1)
      {
        die("SSL: cannot load DSA private key");
      }
    }
    if (tunable_ssl_ciphers &&
        SSL_CTX_set_cipher_list(p_ctx, tunable_ssl_ciphers) != 1)
    {
      die("SSL: could not set cipher list");
    }
    if (RAND_status() != 1)
    {
      die("SSL: RNG is not seeded");
    }
    {
      EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
      if (key == NULL)
      {
        die("SSL: failed to get curve p256");
      }
      SSL_CTX_set_tmp_ecdh(p_ctx, key);
      EC_KEY_free(key);
    }
    if (tunable_ssl_request_cert)
    {
      verify_option |= SSL_VERIFY_PEER;
    }
    if (tunable_require_cert)
    {
      verify_option |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }
    if (verify_option)
    {
      SSL_CTX_set_verify(p_ctx, verify_option, ssl_verify_callback);
      if (tunable_ca_certs_file)
      {
        STACK_OF(X509_NAME)* p_names;
        if (!SSL_CTX_load_verify_locations(p_ctx, tunable_ca_certs_file, NULL))
        {
          die("SSL: could not load verify file");
        }
        p_names = SSL_load_client_CA_file(tunable_ca_certs_file);
        if (!p_names)
        {
          die("SSL: could not load client certs file");
        }
        SSL_CTX_set_client_CA_list(p_ctx, p_names);
      }
    }
    {
      static const char* p_ctx_id = "vsftpd";
      SSL_CTX_set_session_id_context(p_ctx, (void*) p_ctx_id,
                                     vsf_sysutil_strlen(p_ctx_id));
    }
    if (tunable_require_ssl_reuse)
    {
      /* Ensure cached session doesn't expire */
      SSL_CTX_set_timeout(p_ctx, INT_MAX);
    }
    p_sess->p_ssl_ctx = p_ctx;
    ssl_inited = 1;
  }
}

void
ssl_control_handshake(struct vsf_session* p_sess)
{
  if (!ssl_session_init(p_sess))
  {
    struct mystr err_str = INIT_MYSTR;
    str_alloc_text(&err_str, "Negotiation failed: ");
    /* Technically, we shouldn't leak such detailed error messages. */
    str_append_text(&err_str, get_ssl_error());
    vsf_cmdio_write_str(p_sess, FTP_TLS_FAIL, &err_str);
    vsf_sysutil_exit(1);
  }
  p_sess->control_use_ssl = 1;
}

void
handle_auth(struct vsf_session* p_sess)
{
  str_upper(&p_sess->ftp_arg_str);
  if (str_equal_text(&p_sess->ftp_arg_str, "TLS") ||
      str_equal_text(&p_sess->ftp_arg_str, "TLS-C") ||
      str_equal_text(&p_sess->ftp_arg_str, "SSL") ||
      str_equal_text(&p_sess->ftp_arg_str, "TLS-P"))
  {
    vsf_cmdio_write(p_sess, FTP_AUTHOK, "Proceed with negotiation.");
    ssl_control_handshake(p_sess);
    if (str_equal_text(&p_sess->ftp_arg_str, "SSL") ||
        str_equal_text(&p_sess->ftp_arg_str, "TLS-P"))
    {
      p_sess->data_use_ssl = 1;
    }
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_BADAUTH, "Unknown AUTH type.");
  }
}

void
handle_pbsz(struct vsf_session* p_sess)
{
  if (!p_sess->control_use_ssl)
  {
    vsf_cmdio_write(p_sess, FTP_BADPBSZ, "PBSZ needs a secure connection.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_PBSZOK, "PBSZ set to 0.");
  }
}

void
handle_prot(struct vsf_session* p_sess)
{
  str_upper(&p_sess->ftp_arg_str);
  if (!p_sess->control_use_ssl)
  {
    vsf_cmdio_write(p_sess, FTP_BADPROT, "PROT needs a secure connection.");
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "C"))
  {
    p_sess->data_use_ssl = 0;
    vsf_cmdio_write(p_sess, FTP_PROTOK, "PROT now Clear.");
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "P"))
  {
    p_sess->data_use_ssl = 1;
    vsf_cmdio_write(p_sess, FTP_PROTOK, "PROT now Private.");
  }
  else if (str_equal_text(&p_sess->ftp_arg_str, "S") ||
           str_equal_text(&p_sess->ftp_arg_str, "E"))
  {
    vsf_cmdio_write(p_sess, FTP_NOHANDLEPROT, "PROT not supported.");
  }
  else
  {
    vsf_cmdio_write(p_sess, FTP_NOSUCHPROT, "PROT not recognized.");
  }
}

int
ssl_read(struct vsf_session* p_sess, void* p_ssl, char* p_buf, unsigned int len)
{
  return ssl_read_common(p_sess, (SSL*) p_ssl, p_buf, len, SSL_read);
}

int
ssl_peek(struct vsf_session* p_sess, void* p_ssl, char* p_buf, unsigned int len)
{
  return ssl_read_common(p_sess, (SSL*) p_ssl, p_buf, len, SSL_peek);
}

static int
ssl_read_common(struct vsf_session* p_sess,
                SSL* p_void_ssl,
                char* p_buf,
                unsigned int len,
                int (*p_ssl_func)(SSL*, void*, int))
{
  int retval;
  int err;
  SSL* p_ssl = (SSL*) p_void_ssl;
  do
  {
    retval = (*p_ssl_func)(p_ssl, p_buf, len);
    err = SSL_get_error(p_ssl, retval);
  }
  while (retval < 0 && (err == SSL_ERROR_WANT_READ ||
                        err == SSL_ERROR_WANT_WRITE));
  /* If we hit an EOF, make sure it was from the peer, not injected by the
   * attacker.
   */
  if (retval == 0 && SSL_get_shutdown(p_ssl) != SSL_RECEIVED_SHUTDOWN)
  {
    if (p_ssl == p_sess->p_control_ssl)
    {
      str_alloc_text(&debug_str, "Control");
    }
    else
    {
      str_alloc_text(&debug_str, "DATA");
    }
    str_append_text(&debug_str, " connection terminated without SSL shutdown.");
    if (p_ssl != p_sess->p_control_ssl)
    {
      str_append_text(&debug_str,
                      " Buggy client! Integrity of upload cannot be asserted.");
    }
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
    if (tunable_strict_ssl_read_eof)
    {
      return -1;
    }
  }
  return retval;
}

int
ssl_write(void* p_ssl, const char* p_buf, unsigned int len)
{
  int retval;
  int err;
  do
  {
    retval = SSL_write((SSL*) p_ssl, p_buf, len);
    err = SSL_get_error((SSL*) p_ssl, retval);
  }
  while (retval < 0 && (err == SSL_ERROR_WANT_READ ||
                        err == SSL_ERROR_WANT_WRITE));
  return retval;
}

int
ssl_write_str(void* p_ssl, const struct mystr* p_str)
{
  unsigned int len = str_getlen(p_str);
  int ret = SSL_write((SSL*) p_ssl, str_getbuf(p_str), len);
  if ((unsigned int) ret != len)
  {
    return -1;
  }
  return 0;
}

int
ssl_read_into_str(struct vsf_session* p_sess, void* p_ssl, struct mystr* p_str)
{
  unsigned int len = str_getlen(p_str);
  int ret = ssl_read(p_sess, p_ssl, (char*) str_getbuf(p_str), len);
  if (ret >= 0)
  {
    str_trunc(p_str, (unsigned int) ret);
  }
  else
  {
    str_empty(p_str);
  }
  return ret;
}

static void
maybe_log_shutdown_state(struct vsf_session* p_sess)
{
  if (tunable_debug_ssl)
  {
    int ret = SSL_get_shutdown(p_sess->p_data_ssl);
    str_alloc_text(&debug_str, "SSL shutdown state is: ");
    if (ret == 0)
    {
      str_append_text(&debug_str, "NONE");
    }
    else if (ret == SSL_SENT_SHUTDOWN)
    {
      str_append_text(&debug_str, "SSL_SENT_SHUTDOWN");
    }
    else if (ret == SSL_RECEIVED_SHUTDOWN)
    {
      str_append_text(&debug_str, "SSL_RECEIVED_SHUTDOWN");
    }
    else
    {
      str_append_ulong(&debug_str, ret);
    }
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
  }
}

static void
maybe_log_ssl_error_state(struct vsf_session* p_sess, int ret)
{
  if (tunable_debug_ssl)
  {
    str_alloc_text(&debug_str, "SSL ret: ");
    str_append_ulong(&debug_str, ret);
    str_append_text(&debug_str, ", SSL error: ");
    str_append_text(&debug_str, get_ssl_error());
    str_append_text(&debug_str, ", errno: ");
    str_append_ulong(&debug_str, errno);
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
  }
}

int
ssl_data_close(struct vsf_session* p_sess)
{
  int success = 1;
  SSL* p_ssl = p_sess->p_data_ssl;
  if (p_ssl)
  {
    int ret;
    maybe_log_shutdown_state(p_sess);

    /* Disable Nagle algorithm. We want the shutdown packet to be sent
     * immediately, there's nothing coming after.
     */
    vsf_sysutil_set_nodelay(SSL_get_fd(p_ssl));

    /* This is a mess. Ideally, when we're the sender, we'd like to get to the
     * SSL_RECEIVED_SHUTDOWN state to get a cryptographic guarantee that the
     * peer received all the data and shut the connection down cleanly. It
     * doesn't matter hugely apart from logging, but it's a nagging detail.
     * Unfortunately, no FTP client I found was able to get sends into that
     * state, so the best we can do is issue SSL_shutdown but not check the
     * errors / returns. At least this enables the receiver to be sure of the
     * integrity of the send in terms of unwanted truncation.
     */
    ret = SSL_shutdown(p_ssl);
    maybe_log_shutdown_state(p_sess);
    if (ret == 0)
    {
      ret = SSL_shutdown(p_ssl);
      maybe_log_shutdown_state(p_sess);
      if (ret != 1)
      {
        if (tunable_strict_ssl_write_shutdown)
        {
          success = 0;
        }
        maybe_log_shutdown_state(p_sess);
        maybe_log_ssl_error_state(p_sess, ret);
      }
    }
    else if (ret < 0)
    {
      if (tunable_strict_ssl_write_shutdown)
      {
        success = 0;
      }
      maybe_log_ssl_error_state(p_sess, ret);
    }
    SSL_free(p_ssl);
    p_sess->p_data_ssl = NULL;
  }
  return success;
}

int
ssl_accept(struct vsf_session* p_sess, int fd)
{
  /* SECURITY: data SSL connections don't have any auth on them as part of the
   * protocol. If a client sends an unfortunately optional client cert then
   * we can check for a match between the control and data connections.
   */
  SSL* p_ssl;
  int reused;
  if (p_sess->p_data_ssl != NULL)
  {
    die("p_data_ssl should be NULL.");
  }
  p_ssl = get_ssl(p_sess, fd);
  if (p_ssl == NULL)
  {
    return 0;
  }
  p_sess->p_data_ssl = p_ssl;
  setup_bio_callbacks(p_ssl);
  reused = SSL_session_reused(p_ssl);
  if (tunable_require_ssl_reuse && !reused)
  {
    str_alloc_text(&debug_str, "No SSL session reuse on data channel.");
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
    ssl_data_close(p_sess);
    return 0;
  }
  if (str_getlen(&p_sess->control_cert_digest) > 0)
  {
    static struct mystr data_cert_digest;
    if (!ssl_cert_digest(p_ssl, p_sess, &data_cert_digest))
    {
      str_alloc_text(&debug_str, "Missing cert on data channel.");
      vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
      ssl_data_close(p_sess);
      return 0;
    }
    if (str_strcmp(&p_sess->control_cert_digest, &data_cert_digest))
    {
      str_alloc_text(&debug_str, "DIFFERENT cert on data channel.");
      vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
      ssl_data_close(p_sess);
      return 0;
    }
    if (tunable_debug_ssl)
    {
      str_alloc_text(&debug_str, "Matching cert on data channel.");
      vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
    }
  }
  return 1;
}

void
ssl_comm_channel_init(struct vsf_session* p_sess)
{
  const struct vsf_sysutil_socketpair_retval retval =
    vsf_sysutil_unix_stream_socketpair();
  if (p_sess->ssl_consumer_fd != -1)
  {
    bug("ssl_consumer_fd active");
  }
  if (p_sess->ssl_slave_fd != -1)
  {
    bug("ssl_slave_fd active");
  }
  p_sess->ssl_consumer_fd = retval.socket_one;
  p_sess->ssl_slave_fd = retval.socket_two;
}

void
ssl_comm_channel_set_consumer_context(struct vsf_session* p_sess)
{
  if (p_sess->ssl_slave_fd == -1)
  {
    bug("ssl_slave_fd already closed");
  }
  vsf_sysutil_close(p_sess->ssl_slave_fd);
  p_sess->ssl_slave_fd = -1;
}

void
ssl_comm_channel_set_producer_context(struct vsf_session* p_sess)
{
  if (p_sess->ssl_consumer_fd == -1)
  {
    bug("ssl_consumer_fd already closed");
  }
  vsf_sysutil_close(p_sess->ssl_consumer_fd);
  p_sess->ssl_consumer_fd = -1;
}

static SSL*
get_ssl(struct vsf_session* p_sess, int fd)
{
  SSL* p_ssl = SSL_new(p_sess->p_ssl_ctx);
  if (p_ssl == NULL)
  {
    if (tunable_debug_ssl)
    {
      str_alloc_text(&debug_str, "SSL_new failed");
      vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
    }
    return NULL;
  }
  if (!SSL_set_fd(p_ssl, fd))
  {
    if (tunable_debug_ssl)
    {
      str_alloc_text(&debug_str, "SSL_set_fd failed");
      vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
    }
    SSL_free(p_ssl);
    return NULL;
  }
  if (SSL_accept(p_ssl) != 1)
  {
    const char* p_err = get_ssl_error();
    if (tunable_debug_ssl)
    {
      str_alloc_text(&debug_str, "SSL_accept failed: ");
      str_append_text(&debug_str, p_err);
      vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
    }
    /* The RFC is quite clear that we can just close the control channel
     * here.
     */
    die(p_err);
  }
  if (tunable_debug_ssl)
  {
    const char* p_ssl_version = SSL_get_cipher_version(p_ssl);
    const SSL_CIPHER* p_ssl_cipher = SSL_get_current_cipher(p_ssl);
    const char* p_cipher_name = SSL_CIPHER_get_name(p_ssl_cipher);
    X509* p_ssl_cert = SSL_get_peer_certificate(p_ssl);
    int reused = SSL_session_reused(p_ssl);
    str_alloc_text(&debug_str, "SSL version: ");
    str_append_text(&debug_str, p_ssl_version);
    str_append_text(&debug_str, ", SSL cipher: ");
    str_append_text(&debug_str, p_cipher_name);
    if (reused)
    {
      str_append_text(&debug_str, ", reused");
    }
    else
    {
      str_append_text(&debug_str, ", not reused");
    }
    if (p_ssl_cert != NULL)
    {
      str_append_text(&debug_str, ", CERT PRESENTED");
      X509_free(p_ssl_cert);
    }
    else
    {
      str_append_text(&debug_str, ", no cert");
    }
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
  }
  return p_ssl;
}

static int
ssl_session_init(struct vsf_session* p_sess)
{
  SSL* p_ssl = get_ssl(p_sess, VSFTP_COMMAND_FD);
  if (p_ssl == NULL)
  {
    return 0;
  }
  p_sess->p_control_ssl = p_ssl;
  (void) ssl_cert_digest(p_ssl, p_sess, &p_sess->control_cert_digest);
  setup_bio_callbacks(p_ssl);
  return 1;
}

static int
ssl_cert_digest(SSL* p_ssl, struct vsf_session* p_sess, struct mystr* p_str)
{
  X509* p_cert = SSL_get_peer_certificate(p_ssl);
  unsigned int num_bytes = 0;
  if (p_cert == NULL)
  {
    return 0;
  }
  str_reserve(p_str, EVP_MAX_MD_SIZE);
  str_empty(p_str);
  str_rpad(p_str, EVP_MAX_MD_SIZE);
  if (!X509_digest(p_cert, EVP_sha256(), (unsigned char*) str_getbuf(p_str),
                   &num_bytes))
  {
    die("X509_digest failed");
  }
  X509_free(p_cert);
  if (tunable_debug_ssl)
  {
    unsigned int i;
    str_alloc_text(&debug_str, "Cert digest:");
    for (i = 0; i < num_bytes; ++i)
    { 
      str_append_char(&debug_str, ' ');
      str_append_ulong(
        &debug_str, (unsigned long) (unsigned char) str_get_char_at(p_str, i));
    }
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
  }
  str_trunc(p_str, num_bytes);
  return 1;
}

static char*
get_ssl_error()
{
  SSL_load_error_strings();
  return ERR_error_string(ERR_get_error(), NULL);
}

static void setup_bio_callbacks(SSL* p_ssl)
{
  BIO* p_bio = SSL_get_rbio(p_ssl);
  BIO_set_callback(p_bio, bio_callback);
  p_bio = SSL_get_wbio(p_ssl);
  BIO_set_callback(p_bio, bio_callback);
}

static long
bio_callback(
  BIO* p_bio, int oper, const char* p_arg, int argi, long argl, long ret)
{
  int retval = 0;
  int fd = 0;
  (void) p_arg;
  (void) argi;
  (void) argl;
  if (oper == (BIO_CB_READ | BIO_CB_RETURN) ||
      oper == (BIO_CB_WRITE | BIO_CB_RETURN))
  {
    retval = (int) ret;
    fd = BIO_get_fd(p_bio, NULL);
  }
  vsf_sysutil_check_pending_actions(kVSFSysUtilIO, retval, fd);
  return ret;
}

static int
ssl_verify_callback(int verify_ok, X509_STORE_CTX* p_ctx)
{
  (void) p_ctx;
  if (tunable_validate_cert)
  {
    return verify_ok;
  }
  return 1;
}

void
ssl_add_entropy(struct vsf_session* p_sess)
{
  /* Although each child does seem to have its different pool of entropy, I
   * don't trust the interaction of OpenSSL's opaque RAND API and fork(). So
   * throw a bit more in (only works on systems with /dev/urandom for now).
   */
  int ret = RAND_load_file("/dev/urandom", 16);
  if (ret != 16)
  {
    str_alloc_text(&debug_str, "Couldn't add extra OpenSSL entropy: ");
    str_append_ulong(&debug_str, (unsigned long) ret);
    vsf_log_line(p_sess, kVSFLogEntryDebug, &debug_str);
  }
}

#else /* VSF_BUILD_SSL */

void
ssl_init(struct vsf_session* p_sess)
{
  (void) p_sess;
  die("SSL: ssl_enable is set but SSL support not compiled in");
}

void
ssl_control_handshake(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
handle_auth(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
handle_pbsz(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
handle_prot(struct vsf_session* p_sess)
{
  (void) p_sess;
}

int
ssl_read(struct vsf_session* p_sess, void* p_ssl, char* p_buf, unsigned int len)
{
  (void) p_sess;
  (void) p_ssl;
  (void) p_buf;
  (void) len;
  return -1;
}

int
ssl_peek(struct vsf_session* p_sess, void* p_ssl, char* p_buf, unsigned int len)
{
  (void) p_sess;
  (void) p_ssl;
  (void) p_buf;
  (void) len;
  return -1;
}

int
ssl_write(void* p_ssl, const char* p_buf, unsigned int len)
{
  (void) p_ssl;
  (void) p_buf;
  (void) len;
  return -1;
}

int
ssl_write_str(void* p_ssl, const struct mystr* p_str)
{
  (void) p_ssl;
  (void) p_str;
  return -1;
}

int
ssl_accept(struct vsf_session* p_sess, int fd)
{
  (void) p_sess;
  (void) fd;
  return -1;
}

int
ssl_data_close(struct vsf_session* p_sess)
{
  (void) p_sess;
  return 1;
}

void
ssl_comm_channel_init(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
ssl_comm_channel_set_consumer_context(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
ssl_comm_channel_set_producer_context(struct vsf_session* p_sess)
{
  (void) p_sess;
}

void
ssl_add_entropy(struct vsf_session* p_sess)
{
  (void) p_sess;
}

int
ssl_read_into_str(struct vsf_session* p_sess, void* p_ssl, struct mystr* p_str)
{
  (void) p_sess;
  (void) p_ssl;
  (void) p_str;
  return -1;
}

#endif /* VSF_BUILD_SSL */

