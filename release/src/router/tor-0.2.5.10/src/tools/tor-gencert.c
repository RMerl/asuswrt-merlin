/* Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/objects.h>
#include <openssl/obj_mac.h>
#include <openssl/err.h>

#include <errno.h>
#if 0
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#endif

#include "compat.h"
#include "../common/util.h"
#include "../common/torlog.h"
#include "crypto.h"
#include "address.h"

#define IDENTITY_KEY_BITS 3072
#define SIGNING_KEY_BITS 2048
#define DEFAULT_LIFETIME 12

/* These globals are set via command line options. */
char *identity_key_file = NULL;
char *signing_key_file = NULL;
char *certificate_file = NULL;
int reuse_signing_key = 0;
int verbose = 0;
int make_new_id = 0;
int months_lifetime = DEFAULT_LIFETIME;
int passphrase_fd = -1;
char *address = NULL;

char *passphrase = NULL;
size_t passphrase_len = 0;

EVP_PKEY *identity_key = NULL;
EVP_PKEY *signing_key = NULL;

/** Write a usage message for tor-gencert to stderr. */
static void
show_help(void)
{
  fprintf(stderr, "Syntax:\n"
          "tor-gencert [-h|--help] [-v] [-r|--reuse] [--create-identity-key]\n"
          "        [-i identity_key_file] [-s signing_key_file] "
          "[-c certificate_file]\n"
          "        [-m lifetime_in_months] [-a address:port] "
          "[--passphrase-fd <fd>]\n");
}

/* XXXX copied from crypto.c */
static void
crypto_log_errors(int severity, const char *doing)
{
  unsigned long err;
  const char *msg, *lib, *func;
  while ((err = ERR_get_error()) != 0) {
    msg = (const char*)ERR_reason_error_string(err);
    lib = (const char*)ERR_lib_error_string(err);
    func = (const char*)ERR_func_error_string(err);
    if (!msg) msg = "(null)";
    if (!lib) lib = "(null)";
    if (!func) func = "(null)";
    if (doing) {
      tor_log(severity, LD_CRYPTO, "crypto error while %s: %s (in %s:%s)",
              doing, msg, lib, func);
    } else {
      tor_log(severity, LD_CRYPTO, "crypto error: %s (in %s:%s)",
              msg, lib, func);
    }
  }
}

/** Read the passphrase from the passphrase fd. */
static int
load_passphrase(void)
{
  char *cp;
  char buf[1024]; /* "Ought to be enough for anybody." */
  ssize_t n = read_all(passphrase_fd, buf, sizeof(buf), 0);
  if (n < 0) {
    log_err(LD_GENERAL, "Couldn't read from passphrase fd: %s",
            strerror(errno));
    return -1;
  }
  cp = memchr(buf, '\n', n);
  passphrase_len = cp-buf;
  passphrase = tor_strndup(buf, passphrase_len);
  memwipe(buf, 0, sizeof(buf));
  return 0;
}

static void
clear_passphrase(void)
{
  if (passphrase) {
    memwipe(passphrase, 0, passphrase_len);
    tor_free(passphrase);
  }
}

/** Read the command line options from <b>argc</b> and <b>argv</b>,
 * setting global option vars as needed.
 */
static int
parse_commandline(int argc, char **argv)
{
  int i;
  log_severity_list_t s;
  for (i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
      show_help();
      return 1;
    } else if (!strcmp(argv[i], "-i")) {
      if (i+1>=argc) {
        fprintf(stderr, "No argument to -i\n");
        return 1;
      }
      identity_key_file = tor_strdup(argv[++i]);
    } else if (!strcmp(argv[i], "-s")) {
      if (i+1>=argc) {
        fprintf(stderr, "No argument to -s\n");
        return 1;
      }
      signing_key_file = tor_strdup(argv[++i]);
    } else if (!strcmp(argv[i], "-c")) {
      if (i+1>=argc) {
        fprintf(stderr, "No argument to -c\n");
        return 1;
      }
      certificate_file = tor_strdup(argv[++i]);
    } else if (!strcmp(argv[i], "-m")) {
      if (i+1>=argc) {
        fprintf(stderr, "No argument to -m\n");
        return 1;
      }
      months_lifetime = atoi(argv[++i]);
      if (months_lifetime > 24 || months_lifetime < 0) {
        fprintf(stderr, "Lifetime (in months) was out of range.\n");
        return 1;
      }
    } else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--reuse")) {
      reuse_signing_key = 1;
    } else if (!strcmp(argv[i], "-v")) {
      verbose = 1;
    } else if (!strcmp(argv[i], "-a")) {
      uint32_t addr;
      uint16_t port;
      char b[INET_NTOA_BUF_LEN];
      struct in_addr in;
      if (i+1>=argc) {
        fprintf(stderr, "No argument to -a\n");
        return 1;
      }
      if (addr_port_lookup(LOG_ERR, argv[++i], NULL, &addr, &port)<0)
        return 1;
      in.s_addr = htonl(addr);
      tor_inet_ntoa(&in, b, sizeof(b));
      address = tor_malloc(INET_NTOA_BUF_LEN+32);
      tor_snprintf(address, INET_NTOA_BUF_LEN+32, "%s:%d", b, (int)port);
    } else if (!strcmp(argv[i], "--create-identity-key")) {
      make_new_id = 1;
    } else if (!strcmp(argv[i], "--passphrase-fd")) {
      if (i+1>=argc) {
        fprintf(stderr, "No argument to --passphrase-fd\n");
        return 1;
      }
      passphrase_fd = atoi(argv[++i]);
    } else {
      fprintf(stderr, "Unrecognized option %s\n", argv[i]);
      return 1;
    }
  }

  memwipe(&s, 0, sizeof(s));
  if (verbose)
    set_log_severity_config(LOG_DEBUG, LOG_ERR, &s);
  else
    set_log_severity_config(LOG_WARN, LOG_ERR, &s);
  add_stream_log(&s, "<stderr>", fileno(stderr));

  if (!identity_key_file) {
    identity_key_file = tor_strdup("./authority_identity_key");
    log_info(LD_GENERAL, "No identity key file given; defaulting to %s",
             identity_key_file);
  }
  if (!signing_key_file) {
    signing_key_file = tor_strdup("./authority_signing_key");
    log_info(LD_GENERAL, "No signing key file given; defaulting to %s",
             signing_key_file);
  }
  if (!certificate_file) {
    certificate_file = tor_strdup("./authority_certificate");
    log_info(LD_GENERAL, "No signing key file given; defaulting to %s",
             certificate_file);
  }
  if (passphrase_fd >= 0) {
    if (load_passphrase()<0)
      return 1;
  }
  return 0;
}

static RSA *
generate_key(int bits)
{
  RSA *rsa = NULL;
  crypto_pk_t *env = crypto_pk_new();
  if (crypto_pk_generate_key_with_bits(env,bits)<0)
    goto done;
  rsa = crypto_pk_get_rsa_(env);
  rsa = RSAPrivateKey_dup(rsa);
 done:
  crypto_pk_free(env);
  return rsa;
}

/** Try to read the identity key from <b>identity_key_file</b>.  If no such
 * file exists and create_identity_key is set, make a new identity key and
 * store it.  Return 0 on success, nonzero on failure.
 */
static int
load_identity_key(void)
{
  file_status_t status = file_status(identity_key_file);
  FILE *f;

  if (make_new_id) {
    open_file_t *open_file = NULL;
    RSA *key;
    if (status != FN_NOENT) {
      log_err(LD_GENERAL, "--create-identity-key was specified, but %s "
              "already exists.", identity_key_file);
      return 1;
    }
    log_notice(LD_GENERAL, "Generating %d-bit RSA identity key.",
               IDENTITY_KEY_BITS);
    if (!(key = generate_key(IDENTITY_KEY_BITS))) {
      log_err(LD_GENERAL, "Couldn't generate identity key.");
      crypto_log_errors(LOG_ERR, "Generating identity key");
      return 1;
    }
    identity_key = EVP_PKEY_new();
    if (!(EVP_PKEY_assign_RSA(identity_key, key))) {
      log_err(LD_GENERAL, "Couldn't assign identity key.");
      return 1;
    }

    if (!(f = start_writing_to_stdio_file(identity_key_file,
                                          OPEN_FLAGS_REPLACE | O_TEXT, 0400,
                                          &open_file)))
      return 1;

    /* Write the key to the file.  If passphrase is not set, takes it from
     * the terminal. */
    if (!PEM_write_PKCS8PrivateKey_nid(f, identity_key,
                                       NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
                                       passphrase, (int)passphrase_len,
                                       NULL, NULL)) {
      log_err(LD_GENERAL, "Couldn't write identity key to %s",
              identity_key_file);
      crypto_log_errors(LOG_ERR, "Writing identity key");
      abort_writing_to_file(open_file);
      return 1;
    }
    finish_writing_to_file(open_file);
  } else {
    if (status != FN_FILE) {
      log_err(LD_GENERAL,
              "No identity key found in %s.  To specify a location "
              "for an identity key, use -i.  To generate a new identity key, "
              "use --create-identity-key.", identity_key_file);
      return 1;
    }

    if (!(f = fopen(identity_key_file, "r"))) {
      log_err(LD_GENERAL, "Couldn't open %s for reading: %s",
              identity_key_file, strerror(errno));
      return 1;
    }

    /* Read the key.  If passphrase is not set, takes it from the terminal. */
    identity_key = PEM_read_PrivateKey(f, NULL, NULL, passphrase);
    if (!identity_key) {
      log_err(LD_GENERAL, "Couldn't read identity key from %s",
              identity_key_file);
      fclose(f);
      return 1;
    }
    fclose(f);
  }
  return 0;
}

/** Load a saved signing key from disk.  Return 0 on success, nonzero on
 * failure. */
static int
load_signing_key(void)
{
  FILE *f;
  if (!(f = fopen(signing_key_file, "r"))) {
    log_err(LD_GENERAL, "Couldn't open %s for reading: %s",
            signing_key_file, strerror(errno));
    return 1;
  }
  if (!(signing_key = PEM_read_PrivateKey(f, NULL, NULL, NULL))) {
    log_err(LD_GENERAL, "Couldn't read siging key from %s", signing_key_file);
    fclose(f);
    return 1;
  }
  fclose(f);
  return 0;
}

/** Generate a new signing key and write it to disk.  Return 0 on success,
 * nonzero on failure. */
static int
generate_signing_key(void)
{
  open_file_t *open_file;
  FILE *f;
  RSA *key;
  log_notice(LD_GENERAL, "Generating %d-bit RSA signing key.",
             SIGNING_KEY_BITS);
  if (!(key = generate_key(SIGNING_KEY_BITS))) {
    log_err(LD_GENERAL, "Couldn't generate signing key.");
    crypto_log_errors(LOG_ERR, "Generating signing key");
    return 1;
  }
  signing_key = EVP_PKEY_new();
  if (!(EVP_PKEY_assign_RSA(signing_key, key))) {
    log_err(LD_GENERAL, "Couldn't assign signing key.");
    return 1;
  }

  if (!(f = start_writing_to_stdio_file(signing_key_file,
                                        OPEN_FLAGS_REPLACE | O_TEXT, 0600,
                                        &open_file)))
    return 1;

  /* Write signing key with no encryption. */
  if (!PEM_write_RSAPrivateKey(f, key, NULL, NULL, 0, NULL, NULL)) {
    crypto_log_errors(LOG_WARN, "writing signing key");
    abort_writing_to_file(open_file);
    return 1;
  }

  finish_writing_to_file(open_file);

  return 0;
}

/** Encode <b>key</b> in the format used in directory documents; return
 * a newly allocated string holding the result or NULL on failure. */
static char *
key_to_string(EVP_PKEY *key)
{
  BUF_MEM *buf;
  BIO *b;
  RSA *rsa = EVP_PKEY_get1_RSA(key);
  char *result;
  if (!rsa)
    return NULL;

  b = BIO_new(BIO_s_mem());
  if (!PEM_write_bio_RSAPublicKey(b, rsa)) {
    crypto_log_errors(LOG_WARN, "writing public key to string");
    return NULL;
  }

  BIO_get_mem_ptr(b, &buf);
  (void) BIO_set_close(b, BIO_NOCLOSE);
  BIO_free(b);
  result = tor_malloc(buf->length + 1);
  memcpy(result, buf->data, buf->length);
  result[buf->length] = 0;
  BUF_MEM_free(buf);

  return result;
}

/** Set <b>out</b> to the hex-encoded fingerprint of <b>pkey</b>. */
static int
get_fingerprint(EVP_PKEY *pkey, char *out)
{
  int r = 1;
  crypto_pk_t *pk = crypto_new_pk_from_rsa_(EVP_PKEY_get1_RSA(pkey));
  if (pk) {
    r = crypto_pk_get_fingerprint(pk, out, 0);
    crypto_pk_free(pk);
  }
  return r;
}

/** Set <b>out</b> to the hex-encoded fingerprint of <b>pkey</b>. */
static int
get_digest(EVP_PKEY *pkey, char *out)
{
  int r = 1;
  crypto_pk_t *pk = crypto_new_pk_from_rsa_(EVP_PKEY_get1_RSA(pkey));
  if (pk) {
    r = crypto_pk_get_digest(pk, out);
    crypto_pk_free(pk);
  }
  return r;
}

/** Generate a new certificate for our loaded or generated keys, and write it
 * to disk.  Return 0 on success, nonzero on failure. */
static int
generate_certificate(void)
{
  char buf[8192];
  time_t now = time(NULL);
  struct tm tm;
  char published[ISO_TIME_LEN+1];
  char expires[ISO_TIME_LEN+1];
  char id_digest[DIGEST_LEN];
  char fingerprint[FINGERPRINT_LEN+1];
  char *ident = key_to_string(identity_key);
  char *signing = key_to_string(signing_key);
  FILE *f;
  size_t signed_len;
  char digest[DIGEST_LEN];
  char signature[1024]; /* handles up to 8192-bit keys. */
  int r;

  get_fingerprint(identity_key, fingerprint);
  get_digest(identity_key, id_digest);

  tor_localtime_r(&now, &tm);
  tm.tm_mon += months_lifetime;

  format_iso_time(published, now);
  format_iso_time(expires, mktime(&tm));

  tor_snprintf(buf, sizeof(buf),
               "dir-key-certificate-version 3"
               "%s%s"
               "\nfingerprint %s\n"
               "dir-key-published %s\n"
               "dir-key-expires %s\n"
               "dir-identity-key\n%s"
               "dir-signing-key\n%s"
               "dir-key-crosscert\n"
               "-----BEGIN ID SIGNATURE-----\n",
               address?"\ndir-address ":"", address?address:"",
               fingerprint, published, expires, ident, signing
               );
  tor_free(ident);
  tor_free(signing);

  /* Append a cross-certification */
  r = RSA_private_encrypt(DIGEST_LEN, (unsigned char*)id_digest,
                          (unsigned char*)signature,
                          EVP_PKEY_get1_RSA(signing_key),
                          RSA_PKCS1_PADDING);
  signed_len = strlen(buf);
  base64_encode(buf+signed_len, sizeof(buf)-signed_len, signature, r);

  strlcat(buf,
          "-----END ID SIGNATURE-----\n"
          "dir-key-certification\n", sizeof(buf));

  signed_len = strlen(buf);
  SHA1((const unsigned char*)buf,signed_len,(unsigned char*)digest);

  r = RSA_private_encrypt(DIGEST_LEN, (unsigned char*)digest,
                          (unsigned char*)signature,
                          EVP_PKEY_get1_RSA(identity_key),
                          RSA_PKCS1_PADDING);
  strlcat(buf, "-----BEGIN SIGNATURE-----\n", sizeof(buf));
  signed_len = strlen(buf);
  base64_encode(buf+signed_len, sizeof(buf)-signed_len, signature, r);
  strlcat(buf, "-----END SIGNATURE-----\n", sizeof(buf));

  if (!(f = fopen(certificate_file, "w"))) {
    log_err(LD_GENERAL, "Couldn't open %s for writing: %s",
            certificate_file, strerror(errno));
    return 1;
  }

  if (fputs(buf, f) < 0) {
    log_err(LD_GENERAL, "Couldn't write to %s: %s",
            certificate_file, strerror(errno));
    fclose(f);
    return 1;
  }
  fclose(f);
  return 0;
}

/** Entry point to tor-gencert */
int
main(int argc, char **argv)
{
  int r = 1;
  init_logging();

  /* Don't bother using acceleration. */
  if (crypto_global_init(0, NULL, NULL)) {
    fprintf(stderr, "Couldn't initialize crypto library.\n");
    return 1;
  }
  if (crypto_seed_rng(1)) {
    fprintf(stderr, "Couldn't seed RNG.\n");
    goto done;
  }
  /* Make sure that files are made private. */
  umask(0077);

  if (parse_commandline(argc, argv))
    goto done;
  if (load_identity_key())
    goto done;
  if (reuse_signing_key) {
    if (load_signing_key())
      goto done;
  } else {
    if (generate_signing_key())
      goto done;
  }
  if (generate_certificate())
    goto done;

  r = 0;
 done:
  clear_passphrase();
  if (identity_key)
    EVP_PKEY_free(identity_key);
  if (signing_key)
    EVP_PKEY_free(signing_key);
  tor_free(address);
  tor_free(identity_key_file);
  tor_free(signing_key_file);
  tor_free(certificate_file);

  crypto_global_cleanup();
  return r;
}

