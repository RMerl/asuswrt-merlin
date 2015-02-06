/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file crypto.c
 * \brief Wrapper functions to present a consistent interface to
 * public-key and symmetric cryptography operations from OpenSSL.
 **/

#include "orconfig.h"

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
/* Windows defines this; so does OpenSSL 0.9.8h and later. We don't actually
 * use either definition. */
#undef OCSP_RESPONSE
#endif

#include <openssl/err.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/engine.h>
#include <openssl/rand.h>
#include <openssl/opensslv.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/conf.h>
#include <openssl/hmac.h>

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_FCNTL_H
#include <sys/fcntl.h>
#endif

#define CRYPTO_PRIVATE
#include "crypto.h"
#include "../common/torlog.h"
#include "aes.h"
#include "../common/util.h"
#include "container.h"
#include "compat.h"
#include "sandbox.h"

#if OPENSSL_VERSION_NUMBER < OPENSSL_V_SERIES(0,9,8)
#error "We require OpenSSL >= 0.9.8"
#endif

#ifdef ANDROID
/* Android's OpenSSL seems to have removed all of its Engine support. */
#define DISABLE_ENGINES
#endif

/** Longest recognized */
#define MAX_DNS_LABEL_SIZE 63

/** Macro: is k a valid RSA public or private key? */
#define PUBLIC_KEY_OK(k) ((k) && (k)->key && (k)->key->n)
/** Macro: is k a valid RSA private key? */
#define PRIVATE_KEY_OK(k) ((k) && (k)->key && (k)->key->p)

#ifdef TOR_IS_MULTITHREADED
/** A number of preallocated mutexes for use by OpenSSL. */
static tor_mutex_t **openssl_mutexes_ = NULL;
/** How many mutexes have we allocated for use by OpenSSL? */
static int n_openssl_mutexes_ = 0;
#endif

/** A public key, or a public/private key-pair. */
struct crypto_pk_t
{
  int refs; /**< reference count, so we don't have to copy keys */
  RSA *key; /**< The key itself */
};

/** Key and stream information for a stream cipher. */
struct crypto_cipher_t
{
  char key[CIPHER_KEY_LEN]; /**< The raw key. */
  char iv[CIPHER_IV_LEN]; /**< The initial IV. */
  aes_cnt_cipher_t *cipher; /**< The key in format usable for counter-mode AES
                             * encryption */
};

/** A structure to hold the first half (x, g^x) of a Diffie-Hellman handshake
 * while we're waiting for the second.*/
struct crypto_dh_t {
  DH *dh; /**< The openssl DH object */
};

static int setup_openssl_threading(void);
static int tor_check_dh_key(int severity, BIGNUM *bn);

/** Return the number of bytes added by padding method <b>padding</b>.
 */
static INLINE int
crypto_get_rsa_padding_overhead(int padding)
{
  switch (padding)
    {
    case RSA_PKCS1_OAEP_PADDING: return PKCS1_OAEP_PADDING_OVERHEAD;
    default: tor_assert(0); return -1;
    }
}

/** Given a padding method <b>padding</b>, return the correct OpenSSL constant.
 */
static INLINE int
crypto_get_rsa_padding(int padding)
{
  switch (padding)
    {
    case PK_PKCS1_OAEP_PADDING: return RSA_PKCS1_OAEP_PADDING;
    default: tor_assert(0); return -1;
    }
}

/** Boolean: has OpenSSL's crypto been initialized? */
static int crypto_early_initialized_ = 0;

/** Boolean: has OpenSSL's crypto been initialized? */
static int crypto_global_initialized_ = 0;

/** Log all pending crypto errors at level <b>severity</b>.  Use
 * <b>doing</b> to describe our current activities.
 */
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

#ifndef DISABLE_ENGINES
/** Log any OpenSSL engines we're using at NOTICE. */
static void
log_engine(const char *fn, ENGINE *e)
{
  if (e) {
    const char *name, *id;
    name = ENGINE_get_name(e);
    id = ENGINE_get_id(e);
    log_notice(LD_CRYPTO, "Default OpenSSL engine for %s is %s [%s]",
               fn, name?name:"?", id?id:"?");
  } else {
    log_info(LD_CRYPTO, "Using default implementation for %s", fn);
  }
}
#endif

#ifndef DISABLE_ENGINES
/** Try to load an engine in a shared library via fully qualified path.
 */
static ENGINE *
try_load_engine(const char *path, const char *engine)
{
  ENGINE *e = ENGINE_by_id("dynamic");
  if (e) {
    if (!ENGINE_ctrl_cmd_string(e, "ID", engine, 0) ||
        !ENGINE_ctrl_cmd_string(e, "DIR_LOAD", "2", 0) ||
        !ENGINE_ctrl_cmd_string(e, "DIR_ADD", path, 0) ||
        !ENGINE_ctrl_cmd_string(e, "LOAD", NULL, 0)) {
      ENGINE_free(e);
      e = NULL;
    }
  }
  return e;
}
#endif

/* Returns a trimmed and human-readable version of an openssl version string
* <b>raw_version</b>. They are usually in the form of 'OpenSSL 1.0.0b 10
* May 2012' and this will parse them into a form similar to '1.0.0b' */
static char *
parse_openssl_version_str(const char *raw_version)
{
  const char *end_of_version = NULL;
  /* The output should be something like "OpenSSL 1.0.0b 10 May 2012. Let's
     trim that down. */
  if (!strcmpstart(raw_version, "OpenSSL ")) {
    raw_version += strlen("OpenSSL ");
    end_of_version = strchr(raw_version, ' ');
  }

  if (end_of_version)
    return tor_strndup(raw_version,
                      end_of_version-raw_version);
  else
    return tor_strdup(raw_version);
}

static char *crypto_openssl_version_str = NULL;
/* Return a human-readable version of the run-time openssl version number. */
const char *
crypto_openssl_get_version_str(void)
{
  if (crypto_openssl_version_str == NULL) {
    const char *raw_version = SSLeay_version(SSLEAY_VERSION);
    crypto_openssl_version_str = parse_openssl_version_str(raw_version);
  }
  return crypto_openssl_version_str;
}

static char *crypto_openssl_header_version_str = NULL;
/* Return a human-readable version of the compile-time openssl version
* number. */
const char *
crypto_openssl_get_header_version_str(void)
{
  if (crypto_openssl_header_version_str == NULL) {
    crypto_openssl_header_version_str =
                        parse_openssl_version_str(OPENSSL_VERSION_TEXT);
  }
  return crypto_openssl_header_version_str;
}

/** Make sure that openssl is using its default PRNG. Return 1 if we had to
 * adjust it; 0 otherwise. */
static int
crypto_force_rand_ssleay(void)
{
  if (RAND_get_rand_method() != RAND_SSLeay()) {
    log_notice(LD_CRYPTO, "It appears that one of our engines has provided "
               "a replacement the OpenSSL RNG. Resetting it to the default "
               "implementation.");
    RAND_set_rand_method(RAND_SSLeay());
    return 1;
  }
  return 0;
}

/** Set up the siphash key if we haven't already done so. */
int
crypto_init_siphash_key(void)
{
  static int have_seeded_siphash = 0;
  struct sipkey key;
  if (have_seeded_siphash)
    return 0;

  if (crypto_rand((char*) &key, sizeof(key)) < 0)
    return -1;
  siphash_set_global_key(&key);
  have_seeded_siphash = 1;
  return 0;
}

/** Initialize the crypto library.  Return 0 on success, -1 on failure.
 */
int
crypto_early_init(void)
{
  if (!crypto_early_initialized_) {

    crypto_early_initialized_ = 1;

    ERR_load_crypto_strings();
    OpenSSL_add_all_algorithms();

    setup_openssl_threading();

    if (SSLeay() == OPENSSL_VERSION_NUMBER &&
        !strcmp(SSLeay_version(SSLEAY_VERSION), OPENSSL_VERSION_TEXT)) {
      log_info(LD_CRYPTO, "OpenSSL version matches version from headers "
                 "(%lx: %s).", SSLeay(), SSLeay_version(SSLEAY_VERSION));
    } else {
      log_warn(LD_CRYPTO, "OpenSSL version from headers does not match the "
               "version we're running with. If you get weird crashes, that "
               "might be why. (Compiled with %lx: %s; running with %lx: %s).",
               (unsigned long)OPENSSL_VERSION_NUMBER, OPENSSL_VERSION_TEXT,
               SSLeay(), SSLeay_version(SSLEAY_VERSION));
    }

    if (SSLeay() < OPENSSL_V_SERIES(1,0,0)) {
      log_notice(LD_CRYPTO,
                 "Your OpenSSL version seems to be %s. We recommend 1.0.0 "
                 "or later.",
                 crypto_openssl_get_version_str());
    }

    crypto_force_rand_ssleay();

    if (crypto_seed_rng(1) < 0)
      return -1;
    if (crypto_init_siphash_key() < 0)
      return -1;
  }
  return 0;
}

/** Initialize the crypto library.  Return 0 on success, -1 on failure.
 */
int
crypto_global_init(int useAccel, const char *accelName, const char *accelDir)
{
  if (!crypto_global_initialized_) {
    crypto_early_init();

    crypto_global_initialized_ = 1;

    if (useAccel > 0) {
#ifdef DISABLE_ENGINES
      (void)accelName;
      (void)accelDir;
      log_warn(LD_CRYPTO, "No OpenSSL hardware acceleration support enabled.");
#else
      ENGINE *e = NULL;

      log_info(LD_CRYPTO, "Initializing OpenSSL engine support.");
      ENGINE_load_builtin_engines();
      ENGINE_register_all_complete();

      if (accelName) {
        if (accelDir) {
          log_info(LD_CRYPTO, "Trying to load dynamic OpenSSL engine \"%s\""
                   " via path \"%s\".", accelName, accelDir);
          e = try_load_engine(accelName, accelDir);
        } else {
          log_info(LD_CRYPTO, "Initializing dynamic OpenSSL engine \"%s\""
                   " acceleration support.", accelName);
          e = ENGINE_by_id(accelName);
        }
        if (!e) {
          log_warn(LD_CRYPTO, "Unable to load dynamic OpenSSL engine \"%s\".",
                   accelName);
        } else {
          log_info(LD_CRYPTO, "Loaded dynamic OpenSSL engine \"%s\".",
                   accelName);
        }
      }
      if (e) {
        log_info(LD_CRYPTO, "Loaded OpenSSL hardware acceleration engine,"
                 " setting default ciphers.");
        ENGINE_set_default(e, ENGINE_METHOD_ALL);
      }
      /* Log, if available, the intersection of the set of algorithms
         used by Tor and the set of algorithms available in the engine */
      log_engine("RSA", ENGINE_get_default_RSA());
      log_engine("DH", ENGINE_get_default_DH());
      log_engine("ECDH", ENGINE_get_default_ECDH());
      log_engine("ECDSA", ENGINE_get_default_ECDSA());
      log_engine("RAND", ENGINE_get_default_RAND());
      log_engine("RAND (which we will not use)", ENGINE_get_default_RAND());
      log_engine("SHA1", ENGINE_get_digest_engine(NID_sha1));
      log_engine("3DES-CBC", ENGINE_get_cipher_engine(NID_des_ede3_cbc));
      log_engine("AES-128-ECB", ENGINE_get_cipher_engine(NID_aes_128_ecb));
      log_engine("AES-128-CBC", ENGINE_get_cipher_engine(NID_aes_128_cbc));
#ifdef NID_aes_128_ctr
      log_engine("AES-128-CTR", ENGINE_get_cipher_engine(NID_aes_128_ctr));
#endif
#ifdef NID_aes_128_gcm
      log_engine("AES-128-GCM", ENGINE_get_cipher_engine(NID_aes_128_gcm));
#endif
      log_engine("AES-256-CBC", ENGINE_get_cipher_engine(NID_aes_256_cbc));
#ifdef NID_aes_256_gcm
      log_engine("AES-256-GCM", ENGINE_get_cipher_engine(NID_aes_256_gcm));
#endif

#endif
    } else {
      log_info(LD_CRYPTO, "NOT using OpenSSL engine support.");
    }

    if (crypto_force_rand_ssleay()) {
      if (crypto_seed_rng(1) < 0)
        return -1;
    }

    evaluate_evp_for_aes(-1);
    evaluate_ctr_for_aes();
  }
  return 0;
}

/** Free crypto resources held by this thread. */
void
crypto_thread_cleanup(void)
{
  ERR_remove_state(0);
}

/** used by tortls.c: wrap an RSA* in a crypto_pk_t. */
crypto_pk_t *
crypto_new_pk_from_rsa_(RSA *rsa)
{
  crypto_pk_t *env;
  tor_assert(rsa);
  env = tor_malloc(sizeof(crypto_pk_t));
  env->refs = 1;
  env->key = rsa;
  return env;
}

/** Helper, used by tor-checkkey.c and tor-gencert.c.  Return the RSA from a
 * crypto_pk_t. */
RSA *
crypto_pk_get_rsa_(crypto_pk_t *env)
{
  return env->key;
}

/** used by tortls.c: get an equivalent EVP_PKEY* for a crypto_pk_t.  Iff
 * private is set, include the private-key portion of the key. */
EVP_PKEY *
crypto_pk_get_evp_pkey_(crypto_pk_t *env, int private)
{
  RSA *key = NULL;
  EVP_PKEY *pkey = NULL;
  tor_assert(env->key);
  if (private) {
    if (!(key = RSAPrivateKey_dup(env->key)))
      goto error;
  } else {
    if (!(key = RSAPublicKey_dup(env->key)))
      goto error;
  }
  if (!(pkey = EVP_PKEY_new()))
    goto error;
  if (!(EVP_PKEY_assign_RSA(pkey, key)))
    goto error;
  return pkey;
 error:
  if (pkey)
    EVP_PKEY_free(pkey);
  if (key)
    RSA_free(key);
  return NULL;
}

/** Used by tortls.c: Get the DH* from a crypto_dh_t.
 */
DH *
crypto_dh_get_dh_(crypto_dh_t *dh)
{
  return dh->dh;
}

/** Allocate and return storage for a public key.  The key itself will not yet
 * be set.
 */
crypto_pk_t *
crypto_pk_new(void)
{
  RSA *rsa;

  rsa = RSA_new();
  tor_assert(rsa);
  return crypto_new_pk_from_rsa_(rsa);
}

/** Release a reference to an asymmetric key; when all the references
 * are released, free the key.
 */
void
crypto_pk_free(crypto_pk_t *env)
{
  if (!env)
    return;

  if (--env->refs > 0)
    return;
  tor_assert(env->refs == 0);

  if (env->key)
    RSA_free(env->key);

  tor_free(env);
}

/** Allocate and return a new symmetric cipher using the provided key and iv.
 * The key is CIPHER_KEY_LEN bytes; the IV is CIPHER_IV_LEN bytes.  If you
 * provide NULL in place of either one, it is generated at random.
 */
crypto_cipher_t *
crypto_cipher_new_with_iv(const char *key, const char *iv)
{
  crypto_cipher_t *env;

  env = tor_malloc_zero(sizeof(crypto_cipher_t));

  if (key == NULL)
    crypto_rand(env->key, CIPHER_KEY_LEN);
  else
    memcpy(env->key, key, CIPHER_KEY_LEN);
  if (iv == NULL)
    crypto_rand(env->iv, CIPHER_IV_LEN);
  else
    memcpy(env->iv, iv, CIPHER_IV_LEN);

  env->cipher = aes_new_cipher(env->key, env->iv);

  return env;
}

/** Return a new crypto_cipher_t with the provided <b>key</b> and an IV of all
 * zero bytes.  */
crypto_cipher_t *
crypto_cipher_new(const char *key)
{
  char zeroiv[CIPHER_IV_LEN];
  memset(zeroiv, 0, sizeof(zeroiv));
  return crypto_cipher_new_with_iv(key, zeroiv);
}

/** Free a symmetric cipher.
 */
void
crypto_cipher_free(crypto_cipher_t *env)
{
  if (!env)
    return;

  tor_assert(env->cipher);
  aes_cipher_free(env->cipher);
  memwipe(env, 0, sizeof(crypto_cipher_t));
  tor_free(env);
}

/* public key crypto */

/** Generate a <b>bits</b>-bit new public/private keypair in <b>env</b>.
 * Return 0 on success, -1 on failure.
 */
int
crypto_pk_generate_key_with_bits(crypto_pk_t *env, int bits)
{
  tor_assert(env);

  if (env->key)
    RSA_free(env->key);

  {
    BIGNUM *e = BN_new();
    RSA *r = NULL;
    if (!e)
      goto done;
    if (! BN_set_word(e, 65537))
      goto done;
    r = RSA_new();
    if (!r)
      goto done;
    if (RSA_generate_key_ex(r, bits, e, NULL) == -1)
      goto done;

    env->key = r;
    r = NULL;
  done:
    if (e)
      BN_clear_free(e);
    if (r)
      RSA_free(r);
  }

  if (!env->key) {
    crypto_log_errors(LOG_WARN, "generating RSA key");
    return -1;
  }

  return 0;
}

/** Read a PEM-encoded private key from the <b>len</b>-byte string <b>s</b>
 * into <b>env</b>.  Return 0 on success, -1 on failure.  If len is -1,
 * the string is nul-terminated.
 */
/* Used here, and used for testing. */
int
crypto_pk_read_private_key_from_string(crypto_pk_t *env,
                                       const char *s, ssize_t len)
{
  BIO *b;

  tor_assert(env);
  tor_assert(s);
  tor_assert(len < INT_MAX && len < SSIZE_T_CEILING);

  /* Create a read-only memory BIO, backed by the string 's' */
  b = BIO_new_mem_buf((char*)s, (int)len);
  if (!b)
    return -1;

  if (env->key)
    RSA_free(env->key);

  env->key = PEM_read_bio_RSAPrivateKey(b,NULL,NULL,NULL);

  BIO_free(b);

  if (!env->key) {
    crypto_log_errors(LOG_WARN, "Error parsing private key");
    return -1;
  }
  return 0;
}

/** Read a PEM-encoded private key from the file named by
 * <b>keyfile</b> into <b>env</b>.  Return 0 on success, -1 on failure.
 */
int
crypto_pk_read_private_key_from_filename(crypto_pk_t *env,
                                         const char *keyfile)
{
  char *contents;
  int r;

  /* Read the file into a string. */
  contents = read_file_to_str(keyfile, 0, NULL);
  if (!contents) {
    log_warn(LD_CRYPTO, "Error reading private key from \"%s\"", keyfile);
    return -1;
  }

  /* Try to parse it. */
  r = crypto_pk_read_private_key_from_string(env, contents, -1);
  memwipe(contents, 0, strlen(contents));
  tor_free(contents);
  if (r)
    return -1; /* read_private_key_from_string already warned, so we don't.*/

  /* Make sure it's valid. */
  if (crypto_pk_check_key(env) <= 0)
    return -1;

  return 0;
}

/** Helper function to implement crypto_pk_write_*_key_to_string. */
static int
crypto_pk_write_key_to_string_impl(crypto_pk_t *env, char **dest,
                                   size_t *len, int is_public)
{
  BUF_MEM *buf;
  BIO *b;
  int r;

  tor_assert(env);
  tor_assert(env->key);
  tor_assert(dest);

  b = BIO_new(BIO_s_mem()); /* Create a memory BIO */
  if (!b)
    return -1;

  /* Now you can treat b as if it were a file.  Just use the
   * PEM_*_bio_* functions instead of the non-bio variants.
   */
  if (is_public)
    r = PEM_write_bio_RSAPublicKey(b, env->key);
  else
    r = PEM_write_bio_RSAPrivateKey(b, env->key, NULL,NULL,0,NULL,NULL);

  if (!r) {
    crypto_log_errors(LOG_WARN, "writing RSA key to string");
    BIO_free(b);
    return -1;
  }

  BIO_get_mem_ptr(b, &buf);
  (void)BIO_set_close(b, BIO_NOCLOSE); /* so BIO_free doesn't free buf */
  BIO_free(b);

  *dest = tor_malloc(buf->length+1);
  memcpy(*dest, buf->data, buf->length);
  (*dest)[buf->length] = 0; /* nul terminate it */
  *len = buf->length;
  BUF_MEM_free(buf);

  return 0;
}

/** PEM-encode the public key portion of <b>env</b> and write it to a
 * newly allocated string.  On success, set *<b>dest</b> to the new
 * string, *<b>len</b> to the string's length, and return 0.  On
 * failure, return -1.
 */
int
crypto_pk_write_public_key_to_string(crypto_pk_t *env, char **dest,
                                     size_t *len)
{
  return crypto_pk_write_key_to_string_impl(env, dest, len, 1);
}

/** PEM-encode the private key portion of <b>env</b> and write it to a
 * newly allocated string.  On success, set *<b>dest</b> to the new
 * string, *<b>len</b> to the string's length, and return 0.  On
 * failure, return -1.
 */
int
crypto_pk_write_private_key_to_string(crypto_pk_t *env, char **dest,
                                     size_t *len)
{
  return crypto_pk_write_key_to_string_impl(env, dest, len, 0);
}

/** Read a PEM-encoded public key from the first <b>len</b> characters of
 * <b>src</b>, and store the result in <b>env</b>.  Return 0 on success, -1 on
 * failure.
 */
int
crypto_pk_read_public_key_from_string(crypto_pk_t *env, const char *src,
                                      size_t len)
{
  BIO *b;

  tor_assert(env);
  tor_assert(src);
  tor_assert(len<INT_MAX);

  b = BIO_new(BIO_s_mem()); /* Create a memory BIO */
  if (!b)
    return -1;

  BIO_write(b, src, (int)len);

  if (env->key)
    RSA_free(env->key);
  env->key = PEM_read_bio_RSAPublicKey(b, NULL, NULL, NULL);
  BIO_free(b);
  if (!env->key) {
    crypto_log_errors(LOG_WARN, "reading public key from string");
    return -1;
  }

  return 0;
}

/** Write the private key from <b>env</b> into the file named by <b>fname</b>,
 * PEM-encoded.  Return 0 on success, -1 on failure.
 */
int
crypto_pk_write_private_key_to_filename(crypto_pk_t *env,
                                        const char *fname)
{
  BIO *bio;
  char *cp;
  long len;
  char *s;
  int r;

  tor_assert(PRIVATE_KEY_OK(env));

  if (!(bio = BIO_new(BIO_s_mem())))
    return -1;
  if (PEM_write_bio_RSAPrivateKey(bio, env->key, NULL,NULL,0,NULL,NULL)
      == 0) {
    crypto_log_errors(LOG_WARN, "writing private key");
    BIO_free(bio);
    return -1;
  }
  len = BIO_get_mem_data(bio, &cp);
  tor_assert(len >= 0);
  s = tor_malloc(len+1);
  memcpy(s, cp, len);
  s[len]='\0';
  r = write_str_to_file(fname, s, 0);
  BIO_free(bio);
  memwipe(s, 0, strlen(s));
  tor_free(s);
  return r;
}

/** Return true iff <b>env</b> has a valid key.
 */
int
crypto_pk_check_key(crypto_pk_t *env)
{
  int r;
  tor_assert(env);

  r = RSA_check_key(env->key);
  if (r <= 0)
    crypto_log_errors(LOG_WARN,"checking RSA key");
  return r;
}

/** Return true iff <b>key</b> contains the private-key portion of the RSA
 * key. */
int
crypto_pk_key_is_private(const crypto_pk_t *key)
{
  tor_assert(key);
  return PRIVATE_KEY_OK(key);
}

/** Return true iff <b>env</b> contains a public key whose public exponent
 * equals 65537.
 */
int
crypto_pk_public_exponent_ok(crypto_pk_t *env)
{
  tor_assert(env);
  tor_assert(env->key);

  return BN_is_word(env->key->e, 65537);
}

/** Compare the public-key components of a and b.  Return less than 0
 * if a\<b, 0 if a==b, and greater than 0 if a\>b.  A NULL key is
 * considered to be less than all non-NULL keys, and equal to itself.
 *
 * Note that this may leak information about the keys through timing.
 */
int
crypto_pk_cmp_keys(crypto_pk_t *a, crypto_pk_t *b)
{
  int result;
  char a_is_non_null = (a != NULL) && (a->key != NULL);
  char b_is_non_null = (b != NULL) && (b->key != NULL);
  char an_argument_is_null = !a_is_non_null | !b_is_non_null;

  result = tor_memcmp(&a_is_non_null, &b_is_non_null, sizeof(a_is_non_null));
  if (an_argument_is_null)
    return result;

  tor_assert(PUBLIC_KEY_OK(a));
  tor_assert(PUBLIC_KEY_OK(b));
  result = BN_cmp((a->key)->n, (b->key)->n);
  if (result)
    return result;
  return BN_cmp((a->key)->e, (b->key)->e);
}

/** Compare the public-key components of a and b.  Return non-zero iff
 * a==b.  A NULL key is considered to be distinct from all non-NULL
 * keys, and equal to itself.
 *
 *  Note that this may leak information about the keys through timing.
 */
int
crypto_pk_eq_keys(crypto_pk_t *a, crypto_pk_t *b)
{
  return (crypto_pk_cmp_keys(a, b) == 0);
}

/** Return the size of the public key modulus in <b>env</b>, in bytes. */
size_t
crypto_pk_keysize(crypto_pk_t *env)
{
  tor_assert(env);
  tor_assert(env->key);

  return (size_t) RSA_size(env->key);
}

/** Return the size of the public key modulus of <b>env</b>, in bits. */
int
crypto_pk_num_bits(crypto_pk_t *env)
{
  tor_assert(env);
  tor_assert(env->key);
  tor_assert(env->key->n);

  return BN_num_bits(env->key->n);
}

/** Increase the reference count of <b>env</b>, and return it.
 */
crypto_pk_t *
crypto_pk_dup_key(crypto_pk_t *env)
{
  tor_assert(env);
  tor_assert(env->key);

  env->refs++;
  return env;
}

/** Make a real honest-to-goodness copy of <b>env</b>, and return it. */
crypto_pk_t *
crypto_pk_copy_full(crypto_pk_t *env)
{
  RSA *new_key;
  int privatekey = 0;
  tor_assert(env);
  tor_assert(env->key);

  if (PRIVATE_KEY_OK(env)) {
    new_key = RSAPrivateKey_dup(env->key);
    privatekey = 1;
  } else {
    new_key = RSAPublicKey_dup(env->key);
  }
  if (!new_key) {
    log_err(LD_CRYPTO, "Unable to duplicate a %s key: openssl failed.",
            privatekey?"private":"public");
    crypto_log_errors(LOG_ERR,
                      privatekey ? "Duplicating a private key" :
                      "Duplicating a public key");
    tor_fragile_assert();
    return NULL;
  }

  return crypto_new_pk_from_rsa_(new_key);
}

/** Encrypt <b>fromlen</b> bytes from <b>from</b> with the public key
 * in <b>env</b>, using the padding method <b>padding</b>.  On success,
 * write the result to <b>to</b>, and return the number of bytes
 * written.  On failure, return -1.
 *
 * <b>tolen</b> is the number of writable bytes in <b>to</b>, and must be
 * at least the length of the modulus of <b>env</b>.
 */
int
crypto_pk_public_encrypt(crypto_pk_t *env, char *to, size_t tolen,
                         const char *from, size_t fromlen, int padding)
{
  int r;
  tor_assert(env);
  tor_assert(from);
  tor_assert(to);
  tor_assert(fromlen<INT_MAX);
  tor_assert(tolen >= crypto_pk_keysize(env));

  r = RSA_public_encrypt((int)fromlen,
                         (unsigned char*)from, (unsigned char*)to,
                         env->key, crypto_get_rsa_padding(padding));
  if (r<0) {
    crypto_log_errors(LOG_WARN, "performing RSA encryption");
    return -1;
  }
  return r;
}

/** Decrypt <b>fromlen</b> bytes from <b>from</b> with the private key
 * in <b>env</b>, using the padding method <b>padding</b>.  On success,
 * write the result to <b>to</b>, and return the number of bytes
 * written.  On failure, return -1.
 *
 * <b>tolen</b> is the number of writable bytes in <b>to</b>, and must be
 * at least the length of the modulus of <b>env</b>.
 */
int
crypto_pk_private_decrypt(crypto_pk_t *env, char *to,
                          size_t tolen,
                          const char *from, size_t fromlen,
                          int padding, int warnOnFailure)
{
  int r;
  tor_assert(env);
  tor_assert(from);
  tor_assert(to);
  tor_assert(env->key);
  tor_assert(fromlen<INT_MAX);
  tor_assert(tolen >= crypto_pk_keysize(env));
  if (!env->key->p)
    /* Not a private key */
    return -1;

  r = RSA_private_decrypt((int)fromlen,
                          (unsigned char*)from, (unsigned char*)to,
                          env->key, crypto_get_rsa_padding(padding));

  if (r<0) {
    crypto_log_errors(warnOnFailure?LOG_WARN:LOG_DEBUG,
                      "performing RSA decryption");
    return -1;
  }
  return r;
}

/** Check the signature in <b>from</b> (<b>fromlen</b> bytes long) with the
 * public key in <b>env</b>, using PKCS1 padding.  On success, write the
 * signed data to <b>to</b>, and return the number of bytes written.
 * On failure, return -1.
 *
 * <b>tolen</b> is the number of writable bytes in <b>to</b>, and must be
 * at least the length of the modulus of <b>env</b>.
 */
int
crypto_pk_public_checksig(crypto_pk_t *env, char *to,
                          size_t tolen,
                          const char *from, size_t fromlen)
{
  int r;
  tor_assert(env);
  tor_assert(from);
  tor_assert(to);
  tor_assert(fromlen < INT_MAX);
  tor_assert(tolen >= crypto_pk_keysize(env));
  r = RSA_public_decrypt((int)fromlen,
                         (unsigned char*)from, (unsigned char*)to,
                         env->key, RSA_PKCS1_PADDING);

  if (r<0) {
    crypto_log_errors(LOG_WARN, "checking RSA signature");
    return -1;
  }
  return r;
}

/** Check a siglen-byte long signature at <b>sig</b> against
 * <b>datalen</b> bytes of data at <b>data</b>, using the public key
 * in <b>env</b>. Return 0 if <b>sig</b> is a correct signature for
 * SHA1(data).  Else return -1.
 */
int
crypto_pk_public_checksig_digest(crypto_pk_t *env, const char *data,
                               size_t datalen, const char *sig, size_t siglen)
{
  char digest[DIGEST_LEN];
  char *buf;
  size_t buflen;
  int r;

  tor_assert(env);
  tor_assert(data);
  tor_assert(sig);
  tor_assert(datalen < SIZE_T_CEILING);
  tor_assert(siglen < SIZE_T_CEILING);

  if (crypto_digest(digest,data,datalen)<0) {
    log_warn(LD_BUG, "couldn't compute digest");
    return -1;
  }
  buflen = crypto_pk_keysize(env);
  buf = tor_malloc(buflen);
  r = crypto_pk_public_checksig(env,buf,buflen,sig,siglen);
  if (r != DIGEST_LEN) {
    log_warn(LD_CRYPTO, "Invalid signature");
    tor_free(buf);
    return -1;
  }
  if (tor_memneq(buf, digest, DIGEST_LEN)) {
    log_warn(LD_CRYPTO, "Signature mismatched with digest.");
    tor_free(buf);
    return -1;
  }
  tor_free(buf);

  return 0;
}

/** Sign <b>fromlen</b> bytes of data from <b>from</b> with the private key in
 * <b>env</b>, using PKCS1 padding.  On success, write the signature to
 * <b>to</b>, and return the number of bytes written.  On failure, return
 * -1.
 *
 * <b>tolen</b> is the number of writable bytes in <b>to</b>, and must be
 * at least the length of the modulus of <b>env</b>.
 */
int
crypto_pk_private_sign(crypto_pk_t *env, char *to, size_t tolen,
                       const char *from, size_t fromlen)
{
  int r;
  tor_assert(env);
  tor_assert(from);
  tor_assert(to);
  tor_assert(fromlen < INT_MAX);
  tor_assert(tolen >= crypto_pk_keysize(env));
  if (!env->key->p)
    /* Not a private key */
    return -1;

  r = RSA_private_encrypt((int)fromlen,
                          (unsigned char*)from, (unsigned char*)to,
                          env->key, RSA_PKCS1_PADDING);
  if (r<0) {
    crypto_log_errors(LOG_WARN, "generating RSA signature");
    return -1;
  }
  return r;
}

/** Compute a SHA1 digest of <b>fromlen</b> bytes of data stored at
 * <b>from</b>; sign the data with the private key in <b>env</b>, and
 * store it in <b>to</b>.  Return the number of bytes written on
 * success, and -1 on failure.
 *
 * <b>tolen</b> is the number of writable bytes in <b>to</b>, and must be
 * at least the length of the modulus of <b>env</b>.
 */
int
crypto_pk_private_sign_digest(crypto_pk_t *env, char *to, size_t tolen,
                              const char *from, size_t fromlen)
{
  int r;
  char digest[DIGEST_LEN];
  if (crypto_digest(digest,from,fromlen)<0)
    return -1;
  r = crypto_pk_private_sign(env,to,tolen,digest,DIGEST_LEN);
  memwipe(digest, 0, sizeof(digest));
  return r;
}

/** Perform a hybrid (public/secret) encryption on <b>fromlen</b>
 * bytes of data from <b>from</b>, with padding type 'padding',
 * storing the results on <b>to</b>.
 *
 * Returns the number of bytes written on success, -1 on failure.
 *
 * The encrypted data consists of:
 *   - The source data, padded and encrypted with the public key, if the
 *     padded source data is no longer than the public key, and <b>force</b>
 *     is false, OR
 *   - The beginning of the source data prefixed with a 16-byte symmetric key,
 *     padded and encrypted with the public key; followed by the rest of
 *     the source data encrypted in AES-CTR mode with the symmetric key.
 */
int
crypto_pk_public_hybrid_encrypt(crypto_pk_t *env,
                                char *to, size_t tolen,
                                const char *from,
                                size_t fromlen,
                                int padding, int force)
{
  int overhead, outlen, r;
  size_t pkeylen, symlen;
  crypto_cipher_t *cipher = NULL;
  char *buf = NULL;

  tor_assert(env);
  tor_assert(from);
  tor_assert(to);
  tor_assert(fromlen < SIZE_T_CEILING);

  overhead = crypto_get_rsa_padding_overhead(crypto_get_rsa_padding(padding));
  pkeylen = crypto_pk_keysize(env);

  if (!force && fromlen+overhead <= pkeylen) {
    /* It all fits in a single encrypt. */
    return crypto_pk_public_encrypt(env,to,
                                    tolen,
                                    from,fromlen,padding);
  }
  tor_assert(tolen >= fromlen + overhead + CIPHER_KEY_LEN);
  tor_assert(tolen >= pkeylen);

  cipher = crypto_cipher_new(NULL); /* generate a new key. */

  buf = tor_malloc(pkeylen+1);
  memcpy(buf, cipher->key, CIPHER_KEY_LEN);
  memcpy(buf+CIPHER_KEY_LEN, from, pkeylen-overhead-CIPHER_KEY_LEN);

  /* Length of symmetrically encrypted data. */
  symlen = fromlen-(pkeylen-overhead-CIPHER_KEY_LEN);

  outlen = crypto_pk_public_encrypt(env,to,tolen,buf,pkeylen-overhead,padding);
  if (outlen!=(int)pkeylen) {
    goto err;
  }
  r = crypto_cipher_encrypt(cipher, to+outlen,
                            from+pkeylen-overhead-CIPHER_KEY_LEN, symlen);

  if (r<0) goto err;
  memwipe(buf, 0, pkeylen);
  tor_free(buf);
  crypto_cipher_free(cipher);
  tor_assert(outlen+symlen < INT_MAX);
  return (int)(outlen + symlen);
 err:

  memwipe(buf, 0, pkeylen);
  tor_free(buf);
  crypto_cipher_free(cipher);
  return -1;
}

/** Invert crypto_pk_public_hybrid_encrypt. */
int
crypto_pk_private_hybrid_decrypt(crypto_pk_t *env,
                                 char *to,
                                 size_t tolen,
                                 const char *from,
                                 size_t fromlen,
                                 int padding, int warnOnFailure)
{
  int outlen, r;
  size_t pkeylen;
  crypto_cipher_t *cipher = NULL;
  char *buf = NULL;

  tor_assert(fromlen < SIZE_T_CEILING);
  pkeylen = crypto_pk_keysize(env);

  if (fromlen <= pkeylen) {
    return crypto_pk_private_decrypt(env,to,tolen,from,fromlen,padding,
                                     warnOnFailure);
  }

  buf = tor_malloc(pkeylen);
  outlen = crypto_pk_private_decrypt(env,buf,pkeylen,from,pkeylen,padding,
                                     warnOnFailure);
  if (outlen<0) {
    log_fn(warnOnFailure?LOG_WARN:LOG_DEBUG, LD_CRYPTO,
           "Error decrypting public-key data");
    goto err;
  }
  if (outlen < CIPHER_KEY_LEN) {
    log_fn(warnOnFailure?LOG_WARN:LOG_INFO, LD_CRYPTO,
           "No room for a symmetric key");
    goto err;
  }
  cipher = crypto_cipher_new(buf);
  if (!cipher) {
    goto err;
  }
  memcpy(to,buf+CIPHER_KEY_LEN,outlen-CIPHER_KEY_LEN);
  outlen -= CIPHER_KEY_LEN;
  tor_assert(tolen - outlen >= fromlen - pkeylen);
  r = crypto_cipher_decrypt(cipher, to+outlen, from+pkeylen, fromlen-pkeylen);
  if (r<0)
    goto err;
  memwipe(buf,0,pkeylen);
  tor_free(buf);
  crypto_cipher_free(cipher);
  tor_assert(outlen + fromlen < INT_MAX);
  return (int)(outlen + (fromlen-pkeylen));
 err:
  memwipe(buf,0,pkeylen);
  tor_free(buf);
  crypto_cipher_free(cipher);
  return -1;
}

/** ASN.1-encode the public portion of <b>pk</b> into <b>dest</b>.
 * Return -1 on error, or the number of characters used on success.
 */
int
crypto_pk_asn1_encode(crypto_pk_t *pk, char *dest, size_t dest_len)
{
  int len;
  unsigned char *buf = NULL;

  len = i2d_RSAPublicKey(pk->key, &buf);
  if (len < 0 || buf == NULL)
    return -1;

  if ((size_t)len > dest_len || dest_len > SIZE_T_CEILING) {
    OPENSSL_free(buf);
    return -1;
  }
  /* We don't encode directly into 'dest', because that would be illegal
   * type-punning.  (C99 is smarter than me, C99 is smarter than me...)
   */
  memcpy(dest,buf,len);
  OPENSSL_free(buf);
  return len;
}

/** Decode an ASN.1-encoded public key from <b>str</b>; return the result on
 * success and NULL on failure.
 */
crypto_pk_t *
crypto_pk_asn1_decode(const char *str, size_t len)
{
  RSA *rsa;
  unsigned char *buf;
  const unsigned char *cp;
  cp = buf = tor_malloc(len);
  memcpy(buf,str,len);
  rsa = d2i_RSAPublicKey(NULL, &cp, len);
  tor_free(buf);
  if (!rsa) {
    crypto_log_errors(LOG_WARN,"decoding public key");
    return NULL;
  }
  return crypto_new_pk_from_rsa_(rsa);
}

/** Given a private or public key <b>pk</b>, put a SHA1 hash of the
 * public key into <b>digest_out</b> (must have DIGEST_LEN bytes of space).
 * Return 0 on success, -1 on failure.
 */
int
crypto_pk_get_digest(crypto_pk_t *pk, char *digest_out)
{
  unsigned char *buf = NULL;
  int len;

  len = i2d_RSAPublicKey(pk->key, &buf);
  if (len < 0 || buf == NULL)
    return -1;
  if (crypto_digest(digest_out, (char*)buf, len) < 0) {
    OPENSSL_free(buf);
    return -1;
  }
  OPENSSL_free(buf);
  return 0;
}

/** Compute all digests of the DER encoding of <b>pk</b>, and store them
 * in <b>digests_out</b>.  Return 0 on success, -1 on failure. */
int
crypto_pk_get_all_digests(crypto_pk_t *pk, digests_t *digests_out)
{
  unsigned char *buf = NULL;
  int len;

  len = i2d_RSAPublicKey(pk->key, &buf);
  if (len < 0 || buf == NULL)
    return -1;
  if (crypto_digest_all(digests_out, (char*)buf, len) < 0) {
    OPENSSL_free(buf);
    return -1;
  }
  OPENSSL_free(buf);
  return 0;
}

/** Copy <b>in</b> to the <b>outlen</b>-byte buffer <b>out</b>, adding spaces
 * every four spaces. */
void
crypto_add_spaces_to_fp(char *out, size_t outlen, const char *in)
{
  int n = 0;
  char *end = out+outlen;
  tor_assert(outlen < SIZE_T_CEILING);

  while (*in && out<end) {
    *out++ = *in++;
    if (++n == 4 && *in && out<end) {
      n = 0;
      *out++ = ' ';
    }
  }
  tor_assert(out<end);
  *out = '\0';
}

/** Given a private or public key <b>pk</b>, put a fingerprint of the
 * public key into <b>fp_out</b> (must have at least FINGERPRINT_LEN+1 bytes of
 * space).  Return 0 on success, -1 on failure.
 *
 * Fingerprints are computed as the SHA1 digest of the ASN.1 encoding
 * of the public key, converted to hexadecimal, in upper case, with a
 * space after every four digits.
 *
 * If <b>add_space</b> is false, omit the spaces.
 */
int
crypto_pk_get_fingerprint(crypto_pk_t *pk, char *fp_out, int add_space)
{
  char digest[DIGEST_LEN];
  char hexdigest[HEX_DIGEST_LEN+1];
  if (crypto_pk_get_digest(pk, digest)) {
    return -1;
  }
  base16_encode(hexdigest,sizeof(hexdigest),digest,DIGEST_LEN);
  if (add_space) {
    crypto_add_spaces_to_fp(fp_out, FINGERPRINT_LEN+1, hexdigest);
  } else {
    strncpy(fp_out, hexdigest, HEX_DIGEST_LEN+1);
  }
  return 0;
}

/** Given a private or public key <b>pk</b>, put a hashed fingerprint of
 * the public key into <b>fp_out</b> (must have at least FINGERPRINT_LEN+1
 * bytes of space).  Return 0 on success, -1 on failure.
 *
 * Hashed fingerprints are computed as the SHA1 digest of the SHA1 digest
 * of the ASN.1 encoding of the public key, converted to hexadecimal, in
 * upper case.
 */
int
crypto_pk_get_hashed_fingerprint(crypto_pk_t *pk, char *fp_out)
{
  char digest[DIGEST_LEN], hashed_digest[DIGEST_LEN];
  if (crypto_pk_get_digest(pk, digest)) {
    return -1;
  }
  if (crypto_digest(hashed_digest, digest, DIGEST_LEN)) {
    return -1;
  }
  base16_encode(fp_out, FINGERPRINT_LEN + 1, hashed_digest, DIGEST_LEN);
  return 0;
}

/* symmetric crypto */

/** Return a pointer to the key set for the cipher in <b>env</b>.
 */
const char *
crypto_cipher_get_key(crypto_cipher_t *env)
{
  return env->key;
}

/** Encrypt <b>fromlen</b> bytes from <b>from</b> using the cipher
 * <b>env</b>; on success, store the result to <b>to</b> and return 0.
 * On failure, return -1.
 */
int
crypto_cipher_encrypt(crypto_cipher_t *env, char *to,
                      const char *from, size_t fromlen)
{
  tor_assert(env);
  tor_assert(env->cipher);
  tor_assert(from);
  tor_assert(fromlen);
  tor_assert(to);
  tor_assert(fromlen < SIZE_T_CEILING);

  aes_crypt(env->cipher, from, fromlen, to);
  return 0;
}

/** Decrypt <b>fromlen</b> bytes from <b>from</b> using the cipher
 * <b>env</b>; on success, store the result to <b>to</b> and return 0.
 * On failure, return -1.
 */
int
crypto_cipher_decrypt(crypto_cipher_t *env, char *to,
                      const char *from, size_t fromlen)
{
  tor_assert(env);
  tor_assert(from);
  tor_assert(to);
  tor_assert(fromlen < SIZE_T_CEILING);

  aes_crypt(env->cipher, from, fromlen, to);
  return 0;
}

/** Encrypt <b>len</b> bytes on <b>from</b> using the cipher in <b>env</b>;
 * on success, return 0.  On failure, return -1.
 */
int
crypto_cipher_crypt_inplace(crypto_cipher_t *env, char *buf, size_t len)
{
  tor_assert(len < SIZE_T_CEILING);
  aes_crypt_inplace(env->cipher, buf, len);
  return 0;
}

/** Encrypt <b>fromlen</b> bytes (at least 1) from <b>from</b> with the key in
 * <b>key</b> to the buffer in <b>to</b> of length
 * <b>tolen</b>. <b>tolen</b> must be at least <b>fromlen</b> plus
 * CIPHER_IV_LEN bytes for the initialization vector. On success, return the
 * number of bytes written, on failure, return -1.
 */
int
crypto_cipher_encrypt_with_iv(const char *key,
                              char *to, size_t tolen,
                              const char *from, size_t fromlen)
{
  crypto_cipher_t *cipher;
  tor_assert(from);
  tor_assert(to);
  tor_assert(fromlen < INT_MAX);

  if (fromlen < 1)
    return -1;
  if (tolen < fromlen + CIPHER_IV_LEN)
    return -1;

  cipher = crypto_cipher_new_with_iv(key, NULL);

  memcpy(to, cipher->iv, CIPHER_IV_LEN);
  crypto_cipher_encrypt(cipher, to+CIPHER_IV_LEN, from, fromlen);
  crypto_cipher_free(cipher);
  return (int)(fromlen + CIPHER_IV_LEN);
}

/** Decrypt <b>fromlen</b> bytes (at least 1+CIPHER_IV_LEN) from <b>from</b>
 * with the key in <b>key</b> to the buffer in <b>to</b> of length
 * <b>tolen</b>. <b>tolen</b> must be at least <b>fromlen</b> minus
 * CIPHER_IV_LEN bytes for the initialization vector. On success, return the
 * number of bytes written, on failure, return -1.
 */
int
crypto_cipher_decrypt_with_iv(const char *key,
                              char *to, size_t tolen,
                              const char *from, size_t fromlen)
{
  crypto_cipher_t *cipher;
  tor_assert(key);
  tor_assert(from);
  tor_assert(to);
  tor_assert(fromlen < INT_MAX);

  if (fromlen <= CIPHER_IV_LEN)
    return -1;
  if (tolen < fromlen - CIPHER_IV_LEN)
    return -1;

  cipher = crypto_cipher_new_with_iv(key, from);

  crypto_cipher_encrypt(cipher, to, from+CIPHER_IV_LEN, fromlen-CIPHER_IV_LEN);
  crypto_cipher_free(cipher);
  return (int)(fromlen - CIPHER_IV_LEN);
}

/* SHA-1 */

/** Compute the SHA1 digest of the <b>len</b> bytes on data stored in
 * <b>m</b>.  Write the DIGEST_LEN byte result into <b>digest</b>.
 * Return 0 on success, -1 on failure.
 */
int
crypto_digest(char *digest, const char *m, size_t len)
{
  tor_assert(m);
  tor_assert(digest);
  return (SHA1((const unsigned char*)m,len,(unsigned char*)digest) == NULL);
}

/** Compute a 256-bit digest of <b>len</b> bytes in data stored in <b>m</b>,
 * using the algorithm <b>algorithm</b>.  Write the DIGEST_LEN256-byte result
 * into <b>digest</b>.  Return 0 on success, -1 on failure. */
int
crypto_digest256(char *digest, const char *m, size_t len,
                 digest_algorithm_t algorithm)
{
  tor_assert(m);
  tor_assert(digest);
  tor_assert(algorithm == DIGEST_SHA256);
  return (SHA256((const unsigned char*)m,len,(unsigned char*)digest) == NULL);
}

/** Set the digests_t in <b>ds_out</b> to contain every digest on the
 * <b>len</b> bytes in <b>m</b> that we know how to compute.  Return 0 on
 * success, -1 on failure. */
int
crypto_digest_all(digests_t *ds_out, const char *m, size_t len)
{
  int i;
  tor_assert(ds_out);
  memset(ds_out, 0, sizeof(*ds_out));
  if (crypto_digest(ds_out->d[DIGEST_SHA1], m, len) < 0)
    return -1;
  for (i = DIGEST_SHA256; i < N_DIGEST_ALGORITHMS; ++i) {
    if (crypto_digest256(ds_out->d[i], m, len, i) < 0)
      return -1;
  }
  return 0;
}

/** Return the name of an algorithm, as used in directory documents. */
const char *
crypto_digest_algorithm_get_name(digest_algorithm_t alg)
{
  switch (alg) {
    case DIGEST_SHA1:
      return "sha1";
    case DIGEST_SHA256:
      return "sha256";
    default:
      tor_fragile_assert();
      return "??unknown_digest??";
  }
}

/** Given the name of a digest algorithm, return its integer value, or -1 if
 * the name is not recognized. */
int
crypto_digest_algorithm_parse_name(const char *name)
{
  if (!strcmp(name, "sha1"))
    return DIGEST_SHA1;
  else if (!strcmp(name, "sha256"))
    return DIGEST_SHA256;
  else
    return -1;
}

/** Intermediate information about the digest of a stream of data. */
struct crypto_digest_t {
  union {
    SHA_CTX sha1; /**< state for SHA1 */
    SHA256_CTX sha2; /**< state for SHA256 */
  } d; /**< State for the digest we're using.  Only one member of the
        * union is usable, depending on the value of <b>algorithm</b>. */
  digest_algorithm_bitfield_t algorithm : 8; /**< Which algorithm is in use? */
};

/** Allocate and return a new digest object to compute SHA1 digests.
 */
crypto_digest_t *
crypto_digest_new(void)
{
  crypto_digest_t *r;
  r = tor_malloc(sizeof(crypto_digest_t));
  SHA1_Init(&r->d.sha1);
  r->algorithm = DIGEST_SHA1;
  return r;
}

/** Allocate and return a new digest object to compute 256-bit digests
 * using <b>algorithm</b>. */
crypto_digest_t *
crypto_digest256_new(digest_algorithm_t algorithm)
{
  crypto_digest_t *r;
  tor_assert(algorithm == DIGEST_SHA256);
  r = tor_malloc(sizeof(crypto_digest_t));
  SHA256_Init(&r->d.sha2);
  r->algorithm = algorithm;
  return r;
}

/** Deallocate a digest object.
 */
void
crypto_digest_free(crypto_digest_t *digest)
{
  if (!digest)
    return;
  memwipe(digest, 0, sizeof(crypto_digest_t));
  tor_free(digest);
}

/** Add <b>len</b> bytes from <b>data</b> to the digest object.
 */
void
crypto_digest_add_bytes(crypto_digest_t *digest, const char *data,
                        size_t len)
{
  tor_assert(digest);
  tor_assert(data);
  /* Using the SHA*_*() calls directly means we don't support doing
   * SHA in hardware. But so far the delay of getting the question
   * to the hardware, and hearing the answer, is likely higher than
   * just doing it ourselves. Hashes are fast.
   */
  switch (digest->algorithm) {
    case DIGEST_SHA1:
      SHA1_Update(&digest->d.sha1, (void*)data, len);
      break;
    case DIGEST_SHA256:
      SHA256_Update(&digest->d.sha2, (void*)data, len);
      break;
    default:
      tor_fragile_assert();
      break;
  }
}

/** Compute the hash of the data that has been passed to the digest
 * object; write the first out_len bytes of the result to <b>out</b>.
 * <b>out_len</b> must be \<= DIGEST256_LEN.
 */
void
crypto_digest_get_digest(crypto_digest_t *digest,
                         char *out, size_t out_len)
{
  unsigned char r[DIGEST256_LEN];
  crypto_digest_t tmpenv;
  tor_assert(digest);
  tor_assert(out);
  /* memcpy into a temporary ctx, since SHA*_Final clears the context */
  memcpy(&tmpenv, digest, sizeof(crypto_digest_t));
  switch (digest->algorithm) {
    case DIGEST_SHA1:
      tor_assert(out_len <= DIGEST_LEN);
      SHA1_Final(r, &tmpenv.d.sha1);
      break;
    case DIGEST_SHA256:
      tor_assert(out_len <= DIGEST256_LEN);
      SHA256_Final(r, &tmpenv.d.sha2);
      break;
    default:
      log_warn(LD_BUG, "Called with unknown algorithm %d", digest->algorithm);
      /* If fragile_assert is not enabled, then we should at least not
       * leak anything. */
      memset(r, 0xff, sizeof(r));
      tor_fragile_assert();
      break;
  }
  memcpy(out, r, out_len);
  memwipe(r, 0, sizeof(r));
}

/** Allocate and return a new digest object with the same state as
 * <b>digest</b>
 */
crypto_digest_t *
crypto_digest_dup(const crypto_digest_t *digest)
{
  crypto_digest_t *r;
  tor_assert(digest);
  r = tor_malloc(sizeof(crypto_digest_t));
  memcpy(r,digest,sizeof(crypto_digest_t));
  return r;
}

/** Replace the state of the digest object <b>into</b> with the state
 * of the digest object <b>from</b>.
 */
void
crypto_digest_assign(crypto_digest_t *into,
                     const crypto_digest_t *from)
{
  tor_assert(into);
  tor_assert(from);
  memcpy(into,from,sizeof(crypto_digest_t));
}

/** Given a list of strings in <b>lst</b>, set the <b>len_out</b>-byte digest
 * at <b>digest_out</b> to the hash of the concatenation of those strings,
 * plus the optional string <b>append</b>, computed with the algorithm
 * <b>alg</b>.
 * <b>out_len</b> must be \<= DIGEST256_LEN. */
void
crypto_digest_smartlist(char *digest_out, size_t len_out,
                        const smartlist_t *lst, const char *append,
                        digest_algorithm_t alg)
{
  crypto_digest_t *d;
  if (alg == DIGEST_SHA1)
    d = crypto_digest_new();
  else
    d = crypto_digest256_new(alg);
  SMARTLIST_FOREACH(lst, const char *, cp,
                    crypto_digest_add_bytes(d, cp, strlen(cp)));
  if (append)
    crypto_digest_add_bytes(d, append, strlen(append));
  crypto_digest_get_digest(d, digest_out, len_out);
  crypto_digest_free(d);
}

/** Compute the HMAC-SHA-256 of the <b>msg_len</b> bytes in <b>msg</b>, using
 * the <b>key</b> of length <b>key_len</b>.  Store the DIGEST256_LEN-byte
 * result in <b>hmac_out</b>.
 */
void
crypto_hmac_sha256(char *hmac_out,
                   const char *key, size_t key_len,
                   const char *msg, size_t msg_len)
{
  /* If we've got OpenSSL >=0.9.8 we can use its hmac implementation. */
  tor_assert(key_len < INT_MAX);
  tor_assert(msg_len < INT_MAX);
  HMAC(EVP_sha256(), key, (int)key_len, (unsigned char*)msg, (int)msg_len,
       (unsigned char*)hmac_out, NULL);
}

/* DH */

/** Our DH 'g' parameter */
#define DH_GENERATOR 2

/** Shared P parameter for our circuit-crypto DH key exchanges. */
static BIGNUM *dh_param_p = NULL;
/** Shared P parameter for our TLS DH key exchanges. */
static BIGNUM *dh_param_p_tls = NULL;
/** Shared G parameter for our DH key exchanges. */
static BIGNUM *dh_param_g = NULL;

/** Generate and return a reasonable and safe DH parameter p. */
static BIGNUM *
crypto_generate_dynamic_dh_modulus(void)
{
  BIGNUM *dynamic_dh_modulus;
  DH *dh_parameters;
  int r, dh_codes;
  char *s;

  dynamic_dh_modulus = BN_new();
  tor_assert(dynamic_dh_modulus);

  dh_parameters = DH_generate_parameters(DH_BYTES*8, DH_GENERATOR, NULL, NULL);
  tor_assert(dh_parameters);

  r = DH_check(dh_parameters, &dh_codes);
  tor_assert(r && !dh_codes);

  BN_copy(dynamic_dh_modulus, dh_parameters->p);
  tor_assert(dynamic_dh_modulus);

  DH_free(dh_parameters);

  { /* log the dynamic DH modulus: */
    s = BN_bn2hex(dynamic_dh_modulus);
    tor_assert(s);
    log_info(LD_OR, "Dynamic DH modulus generated: [%s]", s);
    OPENSSL_free(s);
  }

  return dynamic_dh_modulus;
}

/** Store our dynamic DH modulus (and its group parameters) to
    <b>fname</b> for future use. */
static int
crypto_store_dynamic_dh_modulus(const char *fname)
{
  int len, new_len;
  DH *dh = NULL;
  unsigned char *dh_string_repr = NULL;
  char *base64_encoded_dh = NULL;
  char *file_string = NULL;
  int retval = -1;
  static const char file_header[] = "# This file contains stored Diffie-"
    "Hellman parameters for future use.\n# You *do not* need to edit this "
    "file.\n\n";

  tor_assert(fname);

  if (!dh_param_p_tls) {
    log_info(LD_CRYPTO, "Tried to store a DH modulus that does not exist.");
    goto done;
  }

  if (!(dh = DH_new()))
    goto done;
  if (!(dh->p = BN_dup(dh_param_p_tls)))
    goto done;
  if (!(dh->g = BN_new()))
    goto done;
  if (!BN_set_word(dh->g, DH_GENERATOR))
    goto done;

  len = i2d_DHparams(dh, &dh_string_repr);
  if ((len < 0) || (dh_string_repr == NULL)) {
    log_warn(LD_CRYPTO, "Error occured while DER encoding DH modulus (2).");
    goto done;
  }

  base64_encoded_dh = tor_malloc_zero(len * 2); /* should be enough */
  new_len = base64_encode(base64_encoded_dh, len * 2,
                          (char *)dh_string_repr, len);
  if (new_len < 0) {
    log_warn(LD_CRYPTO, "Error occured while base64-encoding DH modulus.");
    goto done;
  }

  /* concatenate file header and the dh parameters blob */
  new_len = tor_asprintf(&file_string, "%s%s", file_header, base64_encoded_dh);

  /* write to file */
  if (write_bytes_to_new_file(fname, file_string, new_len, 0) < 0) {
    log_info(LD_CRYPTO, "'%s' was already occupied.", fname);
    goto done;
  }

  retval = 0;

 done:
  if (dh)
    DH_free(dh);
  if (dh_string_repr)
    OPENSSL_free(dh_string_repr);
  tor_free(base64_encoded_dh);
  tor_free(file_string);

  return retval;
}

/** Return the dynamic DH modulus stored in <b>fname</b>. If there is no
    dynamic DH modulus stored in <b>fname</b>, return NULL. */
static BIGNUM *
crypto_get_stored_dynamic_dh_modulus(const char *fname)
{
  int retval;
  char *contents = NULL;
  const char *contents_tmp = NULL;
  int dh_codes;
  DH *stored_dh = NULL;
  BIGNUM *dynamic_dh_modulus = NULL;
  int length = 0;
  unsigned char *base64_decoded_dh = NULL;
  const unsigned char *cp = NULL;

  tor_assert(fname);

  contents = read_file_to_str(fname, RFTS_IGNORE_MISSING, NULL);
  if (!contents) {
    log_info(LD_CRYPTO, "Could not open file '%s'", fname);
    goto done; /*usually means that ENOENT. don't try to move file to broken.*/
  }

  /* skip the file header */
  contents_tmp = eat_whitespace(contents);
  if (!*contents_tmp) {
    log_warn(LD_CRYPTO, "Stored dynamic DH modulus file "
             "seems corrupted (eat_whitespace).");
    goto err;
  }

  /* 'fname' contains the DH parameters stored in base64-ed DER
   *  format. We are only interested in the DH modulus.
   *  NOTE: We allocate more storage here than we need. Since we're already
   *  doing that, we can also add 1 byte extra to appease Coverity's
   *  scanner. */

  cp = base64_decoded_dh = tor_malloc_zero(strlen(contents_tmp) + 1);
  length = base64_decode((char *)base64_decoded_dh, strlen(contents_tmp),
                         contents_tmp, strlen(contents_tmp));
  if (length < 0) {
    log_warn(LD_CRYPTO, "Stored dynamic DH modulus seems corrupted (base64).");
    goto err;
  }

  stored_dh = d2i_DHparams(NULL, &cp, length);
  if ((!stored_dh) || (cp - base64_decoded_dh != length)) {
    log_warn(LD_CRYPTO, "Stored dynamic DH modulus seems corrupted (d2i).");
    goto err;
  }

  { /* check the cryptographic qualities of the stored dynamic DH modulus: */
    retval = DH_check(stored_dh, &dh_codes);
    if (!retval || dh_codes) {
      log_warn(LD_CRYPTO, "Stored dynamic DH modulus is not a safe prime.");
      goto err;
    }

    retval = DH_size(stored_dh);
    if (retval < DH_BYTES) {
      log_warn(LD_CRYPTO, "Stored dynamic DH modulus is smaller "
               "than '%d' bits.", DH_BYTES*8);
      goto err;
    }

    if (!BN_is_word(stored_dh->g, 2)) {
      log_warn(LD_CRYPTO, "Stored dynamic DH parameters do not use '2' "
               "as the group generator.");
      goto err;
    }
  }

  { /* log the dynamic DH modulus: */
    char *s = BN_bn2hex(stored_dh->p);
    tor_assert(s);
    log_info(LD_OR, "Found stored dynamic DH modulus: [%s]", s);
    OPENSSL_free(s);
  }

  goto done;

 err:

  {
    /* move broken prime to $filename.broken */
    char *fname_new=NULL;
    tor_asprintf(&fname_new, "%s.broken", fname);

    log_warn(LD_CRYPTO, "Moving broken dynamic DH prime to '%s'.", fname_new);

    if (replace_file(fname, fname_new))
      log_notice(LD_CRYPTO, "Error while moving '%s' to '%s'.",
                 fname, fname_new);

    tor_free(fname_new);
  }

  if (stored_dh) {
    DH_free(stored_dh);
    stored_dh = NULL;
  }

 done:
  tor_free(contents);
  tor_free(base64_decoded_dh);

  if (stored_dh) {
    dynamic_dh_modulus = BN_dup(stored_dh->p);
    DH_free(stored_dh);
  }

  return dynamic_dh_modulus;
}

/** Set the global TLS Diffie-Hellman modulus.
 * If <b>dynamic_dh_modulus_fname</b> is set, try to read a dynamic DH modulus
 * off it and use it as the DH modulus. If that's not possible,
 * generate a new dynamic DH modulus.
 * If <b>dynamic_dh_modulus_fname</b> is NULL, use the Apache mod_ssl DH
 * modulus. */
void
crypto_set_tls_dh_prime(const char *dynamic_dh_modulus_fname)
{
  BIGNUM *tls_prime = NULL;
  int store_dh_prime_afterwards = 0;
  int r;

  /* If the space is occupied, free the previous TLS DH prime */
  if (dh_param_p_tls) {
    BN_clear_free(dh_param_p_tls);
    dh_param_p_tls = NULL;
  }

  if (dynamic_dh_modulus_fname) { /* use dynamic DH modulus: */
    log_info(LD_OR, "Using stored dynamic DH modulus.");
    tls_prime = crypto_get_stored_dynamic_dh_modulus(dynamic_dh_modulus_fname);

    if (!tls_prime) {
      log_notice(LD_OR, "Generating fresh dynamic DH modulus. "
                 "This might take a while...");
      tls_prime = crypto_generate_dynamic_dh_modulus();

      store_dh_prime_afterwards++;
    }
  } else { /* use the static DH prime modulus used by Apache in mod_ssl: */
    tls_prime = BN_new();
    tor_assert(tls_prime);

    /* This is the 1024-bit safe prime that Apache uses for its DH stuff; see
     * modules/ssl/ssl_engine_dh.c; Apache also uses a generator of 2 with this
     * prime.
     */
    r =BN_hex2bn(&tls_prime,
                 "D67DE440CBBBDC1936D693D34AFD0AD50C84D239A45F520BB88174CB98"
                 "BCE951849F912E639C72FB13B4B4D7177E16D55AC179BA420B2A29FE324A"
                 "467A635E81FF5901377BEDDCFD33168A461AAD3B72DAE8860078045B07A7"
                 "DBCA7874087D1510EA9FCC9DDD330507DD62DB88AEAA747DE0F4D6E2BD68"
                 "B0E7393E0F24218EB3");
    tor_assert(r);
  }

  tor_assert(tls_prime);

  dh_param_p_tls = tls_prime;

  if (store_dh_prime_afterwards)
    /* save the new dynamic DH modulus to disk. */
    if (crypto_store_dynamic_dh_modulus(dynamic_dh_modulus_fname)) {
      log_notice(LD_CRYPTO, "Failed while storing dynamic DH modulus. "
                 "Make sure your data directory is sane.");
    }
}

/** Initialize dh_param_p and dh_param_g if they are not already
 * set. */
static void
init_dh_param(void)
{
  BIGNUM *circuit_dh_prime, *generator;
  int r;
  if (dh_param_p && dh_param_g)
    return;

  circuit_dh_prime = BN_new();
  generator = BN_new();
  tor_assert(circuit_dh_prime && generator);

  /* Set our generator for all DH parameters */
  r = BN_set_word(generator, DH_GENERATOR);
  tor_assert(r);

  /* This is from rfc2409, section 6.2.  It's a safe prime, and
     supposedly it equals:
        2^1024 - 2^960 - 1 + 2^64 * { [2^894 pi] + 129093 }.
  */
  r = BN_hex2bn(&circuit_dh_prime,
                "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E08"
                "8A67CC74020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B"
                "302B0A6DF25F14374FE1356D6D51C245E485B576625E7EC6F44C42E9"
                "A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE6"
                "49286651ECE65381FFFFFFFFFFFFFFFF");
  tor_assert(r);

  /* Set the new values as the global DH parameters. */
  dh_param_p = circuit_dh_prime;
  dh_param_g = generator;

  /* Ensure that we have TLS DH parameters set up, too, even if we're
     going to change them soon. */
  if (!dh_param_p_tls) {
    crypto_set_tls_dh_prime(NULL);
  }
}

/** Number of bits to use when choosing the x or y value in a Diffie-Hellman
 * handshake.  Since we exponentiate by this value, choosing a smaller one
 * lets our handhake go faster.
 */
#define DH_PRIVATE_KEY_BITS 320

/** Allocate and return a new DH object for a key exchange.
 */
crypto_dh_t *
crypto_dh_new(int dh_type)
{
  crypto_dh_t *res = tor_malloc_zero(sizeof(crypto_dh_t));

  tor_assert(dh_type == DH_TYPE_CIRCUIT || dh_type == DH_TYPE_TLS ||
             dh_type == DH_TYPE_REND);

  if (!dh_param_p)
    init_dh_param();

  if (!(res->dh = DH_new()))
    goto err;

  if (dh_type == DH_TYPE_TLS) {
    if (!(res->dh->p = BN_dup(dh_param_p_tls)))
      goto err;
  } else {
    if (!(res->dh->p = BN_dup(dh_param_p)))
      goto err;
  }

  if (!(res->dh->g = BN_dup(dh_param_g)))
    goto err;

  res->dh->length = DH_PRIVATE_KEY_BITS;

  return res;
 err:
  crypto_log_errors(LOG_WARN, "creating DH object");
  if (res->dh) DH_free(res->dh); /* frees p and g too */
  tor_free(res);
  return NULL;
}

/** Return a copy of <b>dh</b>, sharing its internal state. */
crypto_dh_t *
crypto_dh_dup(const crypto_dh_t *dh)
{
  crypto_dh_t *dh_new = tor_malloc_zero(sizeof(crypto_dh_t));
  dh_new->dh = dh->dh;
  DH_up_ref(dh->dh);
  return dh_new;
}

/** Return the length of the DH key in <b>dh</b>, in bytes.
 */
int
crypto_dh_get_bytes(crypto_dh_t *dh)
{
  tor_assert(dh);
  return DH_size(dh->dh);
}

/** Generate \<x,g^x\> for our part of the key exchange.  Return 0 on
 * success, -1 on failure.
 */
int
crypto_dh_generate_public(crypto_dh_t *dh)
{
 again:
  if (!DH_generate_key(dh->dh)) {
    crypto_log_errors(LOG_WARN, "generating DH key");
    return -1;
  }
  if (tor_check_dh_key(LOG_WARN, dh->dh->pub_key)<0) {
    log_warn(LD_CRYPTO, "Weird! Our own DH key was invalid.  I guess once-in-"
             "the-universe chances really do happen.  Trying again.");
    /* Free and clear the keys, so OpenSSL will actually try again. */
    BN_clear_free(dh->dh->pub_key);
    BN_clear_free(dh->dh->priv_key);
    dh->dh->pub_key = dh->dh->priv_key = NULL;
    goto again;
  }
  return 0;
}

/** Generate g^x as necessary, and write the g^x for the key exchange
 * as a <b>pubkey_len</b>-byte value into <b>pubkey</b>. Return 0 on
 * success, -1 on failure.  <b>pubkey_len</b> must be \>= DH_BYTES.
 */
int
crypto_dh_get_public(crypto_dh_t *dh, char *pubkey, size_t pubkey_len)
{
  int bytes;
  tor_assert(dh);
  if (!dh->dh->pub_key) {
    if (crypto_dh_generate_public(dh)<0)
      return -1;
  }

  tor_assert(dh->dh->pub_key);
  bytes = BN_num_bytes(dh->dh->pub_key);
  tor_assert(bytes >= 0);
  if (pubkey_len < (size_t)bytes) {
    log_warn(LD_CRYPTO,
             "Weird! pubkey_len (%d) was smaller than DH_BYTES (%d)",
             (int) pubkey_len, bytes);
    return -1;
  }

  memset(pubkey, 0, pubkey_len);
  BN_bn2bin(dh->dh->pub_key, (unsigned char*)(pubkey+(pubkey_len-bytes)));

  return 0;
}

/** Check for bad Diffie-Hellman public keys (g^x).  Return 0 if the key is
 * okay (in the subgroup [2,p-2]), or -1 if it's bad.
 * See http://www.cl.cam.ac.uk/ftp/users/rja14/psandqs.ps.gz for some tips.
 */
static int
tor_check_dh_key(int severity, BIGNUM *bn)
{
  BIGNUM *x;
  char *s;
  tor_assert(bn);
  x = BN_new();
  tor_assert(x);
  if (!dh_param_p)
    init_dh_param();
  BN_set_word(x, 1);
  if (BN_cmp(bn,x)<=0) {
    log_fn(severity, LD_CRYPTO, "DH key must be at least 2.");
    goto err;
  }
  BN_copy(x,dh_param_p);
  BN_sub_word(x, 1);
  if (BN_cmp(bn,x)>=0) {
    log_fn(severity, LD_CRYPTO, "DH key must be at most p-2.");
    goto err;
  }
  BN_clear_free(x);
  return 0;
 err:
  BN_clear_free(x);
  s = BN_bn2hex(bn);
  log_fn(severity, LD_CRYPTO, "Rejecting insecure DH key [%s]", s);
  OPENSSL_free(s);
  return -1;
}

#undef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
/** Given a DH key exchange object, and our peer's value of g^y (as a
 * <b>pubkey_len</b>-byte value in <b>pubkey</b>) generate
 * <b>secret_bytes_out</b> bytes of shared key material and write them
 * to <b>secret_out</b>.  Return the number of bytes generated on success,
 * or -1 on failure.
 *
 * (We generate key material by computing
 *         SHA1( g^xy || "\x00" ) || SHA1( g^xy || "\x01" ) || ...
 * where || is concatenation.)
 */
ssize_t
crypto_dh_compute_secret(int severity, crypto_dh_t *dh,
                         const char *pubkey, size_t pubkey_len,
                         char *secret_out, size_t secret_bytes_out)
{
  char *secret_tmp = NULL;
  BIGNUM *pubkey_bn = NULL;
  size_t secret_len=0, secret_tmp_len=0;
  int result=0;
  tor_assert(dh);
  tor_assert(secret_bytes_out/DIGEST_LEN <= 255);
  tor_assert(pubkey_len < INT_MAX);

  if (!(pubkey_bn = BN_bin2bn((const unsigned char*)pubkey,
                              (int)pubkey_len, NULL)))
    goto error;
  if (tor_check_dh_key(severity, pubkey_bn)<0) {
    /* Check for invalid public keys. */
    log_fn(severity, LD_CRYPTO,"Rejected invalid g^x");
    goto error;
  }
  secret_tmp_len = crypto_dh_get_bytes(dh);
  secret_tmp = tor_malloc(secret_tmp_len);
  result = DH_compute_key((unsigned char*)secret_tmp, pubkey_bn, dh->dh);
  if (result < 0) {
    log_warn(LD_CRYPTO,"DH_compute_key() failed.");
    goto error;
  }
  secret_len = result;
  if (crypto_expand_key_material_TAP((uint8_t*)secret_tmp, secret_len,
                                     (uint8_t*)secret_out, secret_bytes_out)<0)
    goto error;
  secret_len = secret_bytes_out;

  goto done;
 error:
  result = -1;
 done:
  crypto_log_errors(LOG_WARN, "completing DH handshake");
  if (pubkey_bn)
    BN_clear_free(pubkey_bn);
  if (secret_tmp) {
    memwipe(secret_tmp, 0, secret_tmp_len);
    tor_free(secret_tmp);
  }
  if (result < 0)
    return result;
  else
    return secret_len;
}

/** Given <b>key_in_len</b> bytes of negotiated randomness in <b>key_in</b>
 * ("K"), expand it into <b>key_out_len</b> bytes of negotiated key material in
 * <b>key_out</b> by taking the first <b>key_out_len</b> bytes of
 *    H(K | [00]) | H(K | [01]) | ....
 *
 * This is the key expansion algorithm used in the "TAP" circuit extension
 * mechanism; it shouldn't be used for new protocols.
 *
 * Return 0 on success, -1 on failure.
 */
int
crypto_expand_key_material_TAP(const uint8_t *key_in, size_t key_in_len,
                               uint8_t *key_out, size_t key_out_len)
{
  int i;
  uint8_t *cp, *tmp = tor_malloc(key_in_len+1);
  uint8_t digest[DIGEST_LEN];

  /* If we try to get more than this amount of key data, we'll repeat blocks.*/
  tor_assert(key_out_len <= DIGEST_LEN*256);

  memcpy(tmp, key_in, key_in_len);
  for (cp = key_out, i=0; cp < key_out+key_out_len;
       ++i, cp += DIGEST_LEN) {
    tmp[key_in_len] = i;
    if (crypto_digest((char*)digest, (const char *)tmp, key_in_len+1))
      goto err;
    memcpy(cp, digest, MIN(DIGEST_LEN, key_out_len-(cp-key_out)));
  }
  memwipe(tmp, 0, key_in_len+1);
  tor_free(tmp);
  memwipe(digest, 0, sizeof(digest));
  return 0;

 err:
  memwipe(tmp, 0, key_in_len+1);
  tor_free(tmp);
  memwipe(digest, 0, sizeof(digest));
  return -1;
}

/** Expand some secret key material according to RFC5869, using SHA256 as the
 * underlying hash.  The <b>key_in_len</b> bytes at <b>key_in</b> are the
 * secret key material; the <b>salt_in_len</b> bytes at <b>salt_in</b> and the
 * <b>info_in_len</b> bytes in <b>info_in_len</b> are the algorithm's "salt"
 * and "info" parameters respectively.  On success, write <b>key_out_len</b>
 * bytes to <b>key_out</b> and return 0.  On failure, return -1.
 */
int
crypto_expand_key_material_rfc5869_sha256(
                                    const uint8_t *key_in, size_t key_in_len,
                                    const uint8_t *salt_in, size_t salt_in_len,
                                    const uint8_t *info_in, size_t info_in_len,
                                    uint8_t *key_out, size_t key_out_len)
{
  uint8_t prk[DIGEST256_LEN];
  uint8_t tmp[DIGEST256_LEN + 128 + 1];
  uint8_t mac[DIGEST256_LEN];
  int i;
  uint8_t *outp;
  size_t tmp_len;

  crypto_hmac_sha256((char*)prk,
                     (const char*)salt_in, salt_in_len,
                     (const char*)key_in, key_in_len);

  /* If we try to get more than this amount of key data, we'll repeat blocks.*/
  tor_assert(key_out_len <= DIGEST256_LEN * 256);
  tor_assert(info_in_len <= 128);
  memset(tmp, 0, sizeof(tmp));
  outp = key_out;
  i = 1;

  while (key_out_len) {
    size_t n;
    if (i > 1) {
      memcpy(tmp, mac, DIGEST256_LEN);
      memcpy(tmp+DIGEST256_LEN, info_in, info_in_len);
      tmp[DIGEST256_LEN+info_in_len] = i;
      tmp_len = DIGEST256_LEN + info_in_len + 1;
    } else {
      memcpy(tmp, info_in, info_in_len);
      tmp[info_in_len] = i;
      tmp_len = info_in_len + 1;
    }
    crypto_hmac_sha256((char*)mac,
                       (const char*)prk, DIGEST256_LEN,
                       (const char*)tmp, tmp_len);
    n = key_out_len < DIGEST256_LEN ? key_out_len : DIGEST256_LEN;
    memcpy(outp, mac, n);
    key_out_len -= n;
    outp += n;
    ++i;
  }

  memwipe(tmp, 0, sizeof(tmp));
  memwipe(mac, 0, sizeof(mac));
  return 0;
}

/** Free a DH key exchange object.
 */
void
crypto_dh_free(crypto_dh_t *dh)
{
  if (!dh)
    return;
  tor_assert(dh->dh);
  DH_free(dh->dh);
  tor_free(dh);
}

/* random numbers */

/** How many bytes of entropy we add at once.
 *
 * This is how much entropy OpenSSL likes to add right now, so maybe it will
 * work for us too. */
#define ADD_ENTROPY 32

/** True iff it's safe to use RAND_poll after setup.
 *
 * Versions of OpenSSL prior to 0.9.7k and 0.9.8c had a bug where RAND_poll
 * would allocate an fd_set on the stack, open a new file, and try to FD_SET
 * that fd without checking whether it fit in the fd_set.  Thus, if the
 * system has not just been started up, it is unsafe to call */
#define RAND_POLL_IS_SAFE                       \
  (OPENSSL_VERSION_NUMBER >= OPENSSL_V(0,9,8,'c'))

/** Set the seed of the weak RNG to a random value. */
void
crypto_seed_weak_rng(tor_weak_rng_t *rng)
{
  unsigned seed;
  crypto_rand((void*)&seed, sizeof(seed));
  tor_init_weak_random(rng, seed);
}

/** Try to get <b>out_len</b> bytes of the strongest entropy we can generate,
 * storing it into <b>out</b>.
 */
int
crypto_strongest_rand(uint8_t *out, size_t out_len)
{
#ifdef _WIN32
  static int provider_set = 0;
  static HCRYPTPROV provider;
#else
  static const char *filenames[] = {
    "/dev/srandom", "/dev/urandom", "/dev/random", NULL
  };
  int fd, i;
  size_t n;
#endif

#ifdef _WIN32
  if (!provider_set) {
    if (!CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL,
                             CRYPT_VERIFYCONTEXT)) {
      if ((unsigned long)GetLastError() != (unsigned long)NTE_BAD_KEYSET) {
        log_warn(LD_CRYPTO, "Can't get CryptoAPI provider [1]");
        return -1;
      }
    }
    provider_set = 1;
  }
  if (!CryptGenRandom(provider, out_len, out)) {
    log_warn(LD_CRYPTO, "Can't get entropy from CryptoAPI.");
    return -1;
  }

  return 0;
#else
  for (i = 0; filenames[i]; ++i) {
    log_debug(LD_FS, "Opening %s for entropy", filenames[i]);
    fd = open(sandbox_intern_string(filenames[i]), O_RDONLY, 0);
    if (fd<0) continue;
    log_info(LD_CRYPTO, "Reading entropy from \"%s\"", filenames[i]);
    n = read_all(fd, (char*)out, out_len, 0);
    close(fd);
    if (n != out_len) {
      log_warn(LD_CRYPTO,
               "Error reading from entropy source (read only %lu bytes).",
               (unsigned long)n);
      return -1;
    }

    return 0;
  }

  log_warn(LD_CRYPTO, "Cannot get strong entropy: no entropy source found.");
  return -1;
#endif
}

/** Seed OpenSSL's random number generator with bytes from the operating
 * system.  <b>startup</b> should be true iff we have just started Tor and
 * have not yet allocated a bunch of fds.  Return 0 on success, -1 on failure.
 */
int
crypto_seed_rng(int startup)
{
  int rand_poll_ok = 0, load_entropy_ok = 0;
  uint8_t buf[ADD_ENTROPY];

  /* OpenSSL has a RAND_poll function that knows about more kinds of
   * entropy than we do.  We'll try calling that, *and* calling our own entropy
   * functions.  If one succeeds, we'll accept the RNG as seeded. */
  if (startup || RAND_POLL_IS_SAFE) {
    rand_poll_ok = RAND_poll();
    if (rand_poll_ok == 0)
      log_warn(LD_CRYPTO, "RAND_poll() failed.");
  }

  load_entropy_ok = !crypto_strongest_rand(buf, sizeof(buf));
  if (load_entropy_ok) {
    RAND_seed(buf, sizeof(buf));
  }

  memwipe(buf, 0, sizeof(buf));

  if (rand_poll_ok || load_entropy_ok)
    return 0;
  else
    return -1;
}

/** Write <b>n</b> bytes of strong random data to <b>to</b>. Return 0 on
 * success, -1 on failure.
 */
MOCK_IMPL(int,
crypto_rand, (char *to, size_t n))
{
  int r;
  tor_assert(n < INT_MAX);
  tor_assert(to);
  r = RAND_bytes((unsigned char*)to, (int)n);
  if (r == 0)
    crypto_log_errors(LOG_WARN, "generating random data");
  return (r == 1) ? 0 : -1;
}

/** Return a pseudorandom integer, chosen uniformly from the values
 * between 0 and <b>max</b>-1 inclusive.  <b>max</b> must be between 1 and
 * INT_MAX+1, inclusive. */
int
crypto_rand_int(unsigned int max)
{
  unsigned int val;
  unsigned int cutoff;
  tor_assert(max <= ((unsigned int)INT_MAX)+1);
  tor_assert(max > 0); /* don't div by 0 */

  /* We ignore any values that are >= 'cutoff,' to avoid biasing the
   * distribution with clipping at the upper end of unsigned int's
   * range.
   */
  cutoff = UINT_MAX - (UINT_MAX%max);
  while (1) {
    crypto_rand((char*)&val, sizeof(val));
    if (val < cutoff)
      return val % max;
  }
}

/** Return a pseudorandom 64-bit integer, chosen uniformly from the values
 * between 0 and <b>max</b>-1. */
uint64_t
crypto_rand_uint64(uint64_t max)
{
  uint64_t val;
  uint64_t cutoff;
  tor_assert(max < UINT64_MAX);
  tor_assert(max > 0); /* don't div by 0 */

  /* We ignore any values that are >= 'cutoff,' to avoid biasing the
   * distribution with clipping at the upper end of unsigned int's
   * range.
   */
  cutoff = UINT64_MAX - (UINT64_MAX%max);
  while (1) {
    crypto_rand((char*)&val, sizeof(val));
    if (val < cutoff)
      return val % max;
  }
}

/** Return a pseudorandom double d, chosen uniformly from the range
 * 0.0 <= d < 1.0.
 */
double
crypto_rand_double(void)
{
  /* We just use an unsigned int here; we don't really care about getting
   * more than 32 bits of resolution */
  unsigned int uint;
  crypto_rand((char*)&uint, sizeof(uint));
#if SIZEOF_INT == 4
#define UINT_MAX_AS_DOUBLE 4294967296.0
#elif SIZEOF_INT == 8
#define UINT_MAX_AS_DOUBLE 1.8446744073709552e+19
#else
#error SIZEOF_INT is neither 4 nor 8
#endif
  return ((double)uint) / UINT_MAX_AS_DOUBLE;
}

/** Generate and return a new random hostname starting with <b>prefix</b>,
 * ending with <b>suffix</b>, and containing no fewer than
 * <b>min_rand_len</b> and no more than <b>max_rand_len</b> random base32
 * characters between.
 *
 * Clip <b>max_rand_len</b> to MAX_DNS_LABEL_SIZE.
 **/
char *
crypto_random_hostname(int min_rand_len, int max_rand_len, const char *prefix,
                       const char *suffix)
{
  char *result, *rand_bytes;
  int randlen, rand_bytes_len;
  size_t resultlen, prefixlen;

  if (max_rand_len > MAX_DNS_LABEL_SIZE)
    max_rand_len = MAX_DNS_LABEL_SIZE;
  if (min_rand_len > max_rand_len)
    min_rand_len = max_rand_len;

  randlen = min_rand_len + crypto_rand_int(max_rand_len - min_rand_len + 1);

  prefixlen = strlen(prefix);
  resultlen = prefixlen + strlen(suffix) + randlen + 16;

  rand_bytes_len = ((randlen*5)+7)/8;
  if (rand_bytes_len % 5)
    rand_bytes_len += 5 - (rand_bytes_len%5);
  rand_bytes = tor_malloc(rand_bytes_len);
  crypto_rand(rand_bytes, rand_bytes_len);

  result = tor_malloc(resultlen);
  memcpy(result, prefix, prefixlen);
  base32_encode(result+prefixlen, resultlen-prefixlen,
                rand_bytes, rand_bytes_len);
  tor_free(rand_bytes);
  strlcpy(result+prefixlen+randlen, suffix, resultlen-(prefixlen+randlen));

  return result;
}

/** Return a randomly chosen element of <b>sl</b>; or NULL if <b>sl</b>
 * is empty. */
void *
smartlist_choose(const smartlist_t *sl)
{
  int len = smartlist_len(sl);
  if (len)
    return smartlist_get(sl,crypto_rand_int(len));
  return NULL; /* no elements to choose from */
}

/** Scramble the elements of <b>sl</b> into a random order. */
void
smartlist_shuffle(smartlist_t *sl)
{
  int i;
  /* From the end of the list to the front, choose at random from the
     positions we haven't looked at yet, and swap that position into the
     current position.  Remember to give "no swap" the same probability as
     any other swap. */
  for (i = smartlist_len(sl)-1; i > 0; --i) {
    int j = crypto_rand_int(i+1);
    smartlist_swap(sl, i, j);
  }
}

/** Base64 encode <b>srclen</b> bytes of data from <b>src</b>.  Write
 * the result into <b>dest</b>, if it will fit within <b>destlen</b>
 * bytes.  Return the number of bytes written on success; -1 if
 * destlen is too short, or other failure.
 */
int
base64_encode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  /* FFFF we might want to rewrite this along the lines of base64_decode, if
   * it ever shows up in the profile. */
  EVP_ENCODE_CTX ctx;
  int len, ret;
  tor_assert(srclen < INT_MAX);

  /* 48 bytes of input -> 64 bytes of output plus newline.
     Plus one more byte, in case I'm wrong.
  */
  if (destlen < ((srclen/48)+1)*66)
    return -1;
  if (destlen > SIZE_T_CEILING)
    return -1;

  EVP_EncodeInit(&ctx);
  EVP_EncodeUpdate(&ctx, (unsigned char*)dest, &len,
                   (unsigned char*)src, (int)srclen);
  EVP_EncodeFinal(&ctx, (unsigned char*)(dest+len), &ret);
  ret += len;
  return ret;
}

/** @{ */
/** Special values used for the base64_decode_table */
#define X 255
#define SP 64
#define PAD 65
/** @} */
/** Internal table mapping byte values to what they represent in base64.
 * Numbers 0..63 are 6-bit integers.  SPs are spaces, and should be
 * skipped.  Xs are invalid and must not appear in base64. PAD indicates
 * end-of-string. */
static const uint8_t base64_decode_table[256] = {
  X, X, X, X, X, X, X, X, X, SP, SP, SP, X, SP, X, X, /* */
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  SP, X, X, X, X, X, X, X, X, X, X, 62, X, X, X, 63,
  52, 53, 54, 55, 56, 57, 58, 59, 60, 61, X, X, X, PAD, X, X,
  X, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, X, X, X, X, X,
  X, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
  X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
};

/** Base64 decode <b>srclen</b> bytes of data from <b>src</b>.  Write
 * the result into <b>dest</b>, if it will fit within <b>destlen</b>
 * bytes.  Return the number of bytes written on success; -1 if
 * destlen is too short, or other failure.
 *
 * NOTE 1: destlen is checked conservatively, as though srclen contained no
 * spaces or padding.
 *
 * NOTE 2: This implementation does not check for the correct number of
 * padding "=" characters at the end of the string, and does not check
 * for internal padding characters.
 */
int
base64_decode(char *dest, size_t destlen, const char *src, size_t srclen)
{
#ifdef USE_OPENSSL_BASE64
  EVP_ENCODE_CTX ctx;
  int len, ret;
  /* 64 bytes of input -> *up to* 48 bytes of output.
     Plus one more byte, in case I'm wrong.
  */
  if (destlen < ((srclen/64)+1)*49)
    return -1;
  if (destlen > SIZE_T_CEILING)
    return -1;

  EVP_DecodeInit(&ctx);
  EVP_DecodeUpdate(&ctx, (unsigned char*)dest, &len,
                   (unsigned char*)src, srclen);
  EVP_DecodeFinal(&ctx, (unsigned char*)dest, &ret);
  ret += len;
  return ret;
#else
  const char *eos = src+srclen;
  uint32_t n=0;
  int n_idx=0;
  char *dest_orig = dest;

  /* Max number of bits == srclen*6.
   * Number of bytes required to hold all bits == (srclen*6)/8.
   * Yes, we want to round down: anything that hangs over the end of a
   * byte is padding. */
  if (destlen < (srclen*3)/4)
    return -1;
  if (destlen > SIZE_T_CEILING)
    return -1;

  /* Iterate over all the bytes in src.  Each one will add 0 or 6 bits to the
   * value we're decoding.  Accumulate bits in <b>n</b>, and whenever we have
   * 24 bits, batch them into 3 bytes and flush those bytes to dest.
   */
  for ( ; src < eos; ++src) {
    unsigned char c = (unsigned char) *src;
    uint8_t v = base64_decode_table[c];
    switch (v) {
      case X:
        /* This character isn't allowed in base64. */
        return -1;
      case SP:
        /* This character is whitespace, and has no effect. */
        continue;
      case PAD:
        /* We've hit an = character: the data is over. */
        goto end_of_loop;
      default:
        /* We have an actual 6-bit value.  Append it to the bits in n. */
        n = (n<<6) | v;
        if ((++n_idx) == 4) {
          /* We've accumulated 24 bits in n. Flush them. */
          *dest++ = (n>>16);
          *dest++ = (n>>8) & 0xff;
          *dest++ = (n) & 0xff;
          n_idx = 0;
          n = 0;
        }
    }
  }
 end_of_loop:
  /* If we have leftover bits, we need to cope. */
  switch (n_idx) {
    case 0:
    default:
      /* No leftover bits.  We win. */
      break;
    case 1:
      /* 6 leftover bits. That's invalid; we can't form a byte out of that. */
      return -1;
    case 2:
      /* 12 leftover bits: The last 4 are padding and the first 8 are data. */
      *dest++ = n >> 4;
      break;
    case 3:
      /* 18 leftover bits: The last 2 are padding and the first 16 are data. */
      *dest++ = n >> 10;
      *dest++ = n >> 2;
  }

  tor_assert((dest-dest_orig) <= (ssize_t)destlen);
  tor_assert((dest-dest_orig) <= INT_MAX);

  return (int)(dest-dest_orig);
#endif
}
#undef X
#undef SP
#undef PAD

/** Base64 encode DIGEST_LINE bytes from <b>digest</b>, remove the trailing =
 * and newline characters, and store the nul-terminated result in the first
 * BASE64_DIGEST_LEN+1 bytes of <b>d64</b>.  */
int
digest_to_base64(char *d64, const char *digest)
{
  char buf[256];
  base64_encode(buf, sizeof(buf), digest, DIGEST_LEN);
  buf[BASE64_DIGEST_LEN] = '\0';
  memcpy(d64, buf, BASE64_DIGEST_LEN+1);
  return 0;
}

/** Given a base64 encoded, nul-terminated digest in <b>d64</b> (without
 * trailing newline or = characters), decode it and store the result in the
 * first DIGEST_LEN bytes at <b>digest</b>. */
int
digest_from_base64(char *digest, const char *d64)
{
#ifdef USE_OPENSSL_BASE64
  char buf_in[BASE64_DIGEST_LEN+3];
  char buf[256];
  if (strlen(d64) != BASE64_DIGEST_LEN)
    return -1;
  memcpy(buf_in, d64, BASE64_DIGEST_LEN);
  memcpy(buf_in+BASE64_DIGEST_LEN, "=\n\0", 3);
  if (base64_decode(buf, sizeof(buf), buf_in, strlen(buf_in)) != DIGEST_LEN)
    return -1;
  memcpy(digest, buf, DIGEST_LEN);
  return 0;
#else
  if (base64_decode(digest, DIGEST_LEN, d64, strlen(d64)) == DIGEST_LEN)
    return 0;
  else
    return -1;
#endif
}

/** Base64 encode DIGEST256_LINE bytes from <b>digest</b>, remove the
 * trailing = and newline characters, and store the nul-terminated result in
 * the first BASE64_DIGEST256_LEN+1 bytes of <b>d64</b>.  */
int
digest256_to_base64(char *d64, const char *digest)
{
  char buf[256];
  base64_encode(buf, sizeof(buf), digest, DIGEST256_LEN);
  buf[BASE64_DIGEST256_LEN] = '\0';
  memcpy(d64, buf, BASE64_DIGEST256_LEN+1);
  return 0;
}

/** Given a base64 encoded, nul-terminated digest in <b>d64</b> (without
 * trailing newline or = characters), decode it and store the result in the
 * first DIGEST256_LEN bytes at <b>digest</b>. */
int
digest256_from_base64(char *digest, const char *d64)
{
#ifdef USE_OPENSSL_BASE64
  char buf_in[BASE64_DIGEST256_LEN+3];
  char buf[256];
  if (strlen(d64) != BASE64_DIGEST256_LEN)
    return -1;
  memcpy(buf_in, d64, BASE64_DIGEST256_LEN);
  memcpy(buf_in+BASE64_DIGEST256_LEN, "=\n\0", 3);
  if (base64_decode(buf, sizeof(buf), buf_in, strlen(buf_in)) != DIGEST256_LEN)
    return -1;
  memcpy(digest, buf, DIGEST256_LEN);
  return 0;
#else
  if (base64_decode(digest, DIGEST256_LEN, d64, strlen(d64)) == DIGEST256_LEN)
    return 0;
  else
    return -1;
#endif
}

/** Implements base32 encoding as in RFC 4648.  Limitation: Requires
 * that srclen*8 is a multiple of 5.
 */
void
base32_encode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  unsigned int i, v, u;
  size_t nbits = srclen * 8, bit;

  tor_assert(srclen < SIZE_T_CEILING/8);
  tor_assert((nbits%5) == 0); /* We need an even multiple of 5 bits. */
  tor_assert((nbits/5)+1 <= destlen); /* We need enough space. */
  tor_assert(destlen < SIZE_T_CEILING);

  for (i=0,bit=0; bit < nbits; ++i, bit+=5) {
    /* set v to the 16-bit value starting at src[bits/8], 0-padded. */
    v = ((uint8_t)src[bit/8]) << 8;
    if (bit+5<nbits) v += (uint8_t)src[(bit/8)+1];
    /* set u to the 5-bit value at the bit'th bit of src. */
    u = (v >> (11-(bit%8))) & 0x1F;
    dest[i] = BASE32_CHARS[u];
  }
  dest[i] = '\0';
}

/** Implements base32 decoding as in RFC 4648.  Limitation: Requires
 * that srclen*5 is a multiple of 8. Returns 0 if successful, -1 otherwise.
 */
int
base32_decode(char *dest, size_t destlen, const char *src, size_t srclen)
{
  /* XXXX we might want to rewrite this along the lines of base64_decode, if
   * it ever shows up in the profile. */
  unsigned int i;
  size_t nbits, j, bit;
  char *tmp;
  nbits = srclen * 5;

  tor_assert(srclen < SIZE_T_CEILING / 5);
  tor_assert((nbits%8) == 0); /* We need an even multiple of 8 bits. */
  tor_assert((nbits/8) <= destlen); /* We need enough space. */
  tor_assert(destlen < SIZE_T_CEILING);

  /* Convert base32 encoded chars to the 5-bit values that they represent. */
  tmp = tor_malloc_zero(srclen);
  for (j = 0; j < srclen; ++j) {
    if (src[j] > 0x60 && src[j] < 0x7B) tmp[j] = src[j] - 0x61;
    else if (src[j] > 0x31 && src[j] < 0x38) tmp[j] = src[j] - 0x18;
    else if (src[j] > 0x40 && src[j] < 0x5B) tmp[j] = src[j] - 0x41;
    else {
      log_warn(LD_BUG, "illegal character in base32 encoded string");
      tor_free(tmp);
      return -1;
    }
  }

  /* Assemble result byte-wise by applying five possible cases. */
  for (i = 0, bit = 0; bit < nbits; ++i, bit += 8) {
    switch (bit % 40) {
    case 0:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 3) +
                (((uint8_t)tmp[(bit/5)+1]) >> 2);
      break;
    case 8:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 6) +
                (((uint8_t)tmp[(bit/5)+1]) << 1) +
                (((uint8_t)tmp[(bit/5)+2]) >> 4);
      break;
    case 16:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 4) +
                (((uint8_t)tmp[(bit/5)+1]) >> 1);
      break;
    case 24:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 7) +
                (((uint8_t)tmp[(bit/5)+1]) << 2) +
                (((uint8_t)tmp[(bit/5)+2]) >> 3);
      break;
    case 32:
      dest[i] = (((uint8_t)tmp[(bit/5)]) << 5) +
                ((uint8_t)tmp[(bit/5)+1]);
      break;
    }
  }

  memwipe(tmp, 0, srclen);
  tor_free(tmp);
  tmp = NULL;
  return 0;
}

/** Implement RFC2440-style iterated-salted S2K conversion: convert the
 * <b>secret_len</b>-byte <b>secret</b> into a <b>key_out_len</b> byte
 * <b>key_out</b>.  As in RFC2440, the first 8 bytes of s2k_specifier
 * are a salt; the 9th byte describes how much iteration to do.
 * Does not support <b>key_out_len</b> &gt; DIGEST_LEN.
 */
void
secret_to_key(char *key_out, size_t key_out_len, const char *secret,
              size_t secret_len, const char *s2k_specifier)
{
  crypto_digest_t *d;
  uint8_t c;
  size_t count, tmplen;
  char *tmp;
  tor_assert(key_out_len < SIZE_T_CEILING);

#define EXPBIAS 6
  c = s2k_specifier[8];
  count = ((uint32_t)16 + (c & 15)) << ((c >> 4) + EXPBIAS);
#undef EXPBIAS

  tor_assert(key_out_len <= DIGEST_LEN);

  d = crypto_digest_new();
  tmplen = 8+secret_len;
  tmp = tor_malloc(tmplen);
  memcpy(tmp,s2k_specifier,8);
  memcpy(tmp+8,secret,secret_len);
  secret_len += 8;
  while (count) {
    if (count >= secret_len) {
      crypto_digest_add_bytes(d, tmp, secret_len);
      count -= secret_len;
    } else {
      crypto_digest_add_bytes(d, tmp, count);
      count = 0;
    }
  }
  crypto_digest_get_digest(d, key_out, key_out_len);
  memwipe(tmp, 0, tmplen);
  tor_free(tmp);
  crypto_digest_free(d);
}

/**
 * Destroy the <b>sz</b> bytes of data stored at <b>mem</b>, setting them to
 * the value <b>byte</b>.
 *
 * This function is preferable to memset, since many compilers will happily
 * optimize out memset() when they can convince themselves that the data being
 * cleared will never be read.
 *
 * Right now, our convention is to use this function when we are wiping data
 * that's about to become inaccessible, such as stack buffers that are about
 * to go out of scope or structures that are about to get freed.  (In
 * practice, it appears that the compilers we're currently using will optimize
 * out the memset()s for stack-allocated buffers, but not those for
 * about-to-be-freed structures. That could change, though, so we're being
 * wary.)  If there are live reads for the data, then you can just use
 * memset().
 */
void
memwipe(void *mem, uint8_t byte, size_t sz)
{
  /* Because whole-program-optimization exists, we may not be able to just
   * have this function call "memset".  A smart compiler could inline it, then
   * eliminate dead memsets, and declare itself to be clever. */

  /* This is a slow and ugly function from OpenSSL that fills 'mem' with junk
   * based on the pointer value, then uses that junk to update a global
   * variable.  It's an elaborate ruse to trick the compiler into not
   * optimizing out the "wipe this memory" code.  Read it if you like zany
   * programming tricks! In later versions of Tor, we should look for better
   * not-optimized-out memory wiping stuff. */
  OPENSSL_cleanse(mem, sz);
  /* Just in case some caller of memwipe() is relying on getting a buffer
   * filled with a particular value, fill the buffer.
   *
   * If this function gets inlined, this memset might get eliminated, but
   * that's okay: We only care about this particular memset in the case where
   * the caller should have been using memset(), and the memset() wouldn't get
   * eliminated.  In other words, this is here so that we won't break anything
   * if somebody accidentally calls memwipe() instead of memset().
   **/
  memset(mem, byte, sz);
}

#ifdef TOR_IS_MULTITHREADED

#ifndef OPENSSL_THREADS
#error OpenSSL has been built without thread support. Tor requires an \
 OpenSSL library with thread support enabled.
#endif

/** Helper: OpenSSL uses this callback to manipulate mutexes. */
static void
openssl_locking_cb_(int mode, int n, const char *file, int line)
{
  (void)file;
  (void)line;
  if (!openssl_mutexes_)
    /* This is not a really good fix for the
     * "release-freed-lock-from-separate-thread-on-shutdown" problem, but
     * it can't hurt. */
    return;
  if (mode & CRYPTO_LOCK)
    tor_mutex_acquire(openssl_mutexes_[n]);
  else
    tor_mutex_release(openssl_mutexes_[n]);
}

/** OpenSSL helper type: wraps a Tor mutex so that OpenSSL can use it
 * as a lock. */
struct CRYPTO_dynlock_value {
  tor_mutex_t *lock;
};

/** OpenSSL callback function to allocate a lock: see CRYPTO_set_dynlock_*
 * documentation in OpenSSL's docs for more info. */
static struct CRYPTO_dynlock_value *
openssl_dynlock_create_cb_(const char *file, int line)
{
  struct CRYPTO_dynlock_value *v;
  (void)file;
  (void)line;
  v = tor_malloc(sizeof(struct CRYPTO_dynlock_value));
  v->lock = tor_mutex_new();
  return v;
}

/** OpenSSL callback function to acquire or release a lock: see
 * CRYPTO_set_dynlock_* documentation in OpenSSL's docs for more info. */
static void
openssl_dynlock_lock_cb_(int mode, struct CRYPTO_dynlock_value *v,
                         const char *file, int line)
{
  (void)file;
  (void)line;
  if (mode & CRYPTO_LOCK)
    tor_mutex_acquire(v->lock);
  else
    tor_mutex_release(v->lock);
}

/** OpenSSL callback function to free a lock: see CRYPTO_set_dynlock_*
 * documentation in OpenSSL's docs for more info. */
static void
openssl_dynlock_destroy_cb_(struct CRYPTO_dynlock_value *v,
                            const char *file, int line)
{
  (void)file;
  (void)line;
  tor_mutex_free(v->lock);
  tor_free(v);
}

/** @{ */
/** Helper: Construct mutexes, and set callbacks to help OpenSSL handle being
 * multithreaded. */
static int
setup_openssl_threading(void)
{
  int i;
  int n = CRYPTO_num_locks();
  n_openssl_mutexes_ = n;
  openssl_mutexes_ = tor_malloc(n*sizeof(tor_mutex_t *));
  for (i=0; i < n; ++i)
    openssl_mutexes_[i] = tor_mutex_new();
  CRYPTO_set_locking_callback(openssl_locking_cb_);
  CRYPTO_set_id_callback(tor_get_thread_id);
  CRYPTO_set_dynlock_create_callback(openssl_dynlock_create_cb_);
  CRYPTO_set_dynlock_lock_callback(openssl_dynlock_lock_cb_);
  CRYPTO_set_dynlock_destroy_callback(openssl_dynlock_destroy_cb_);
  return 0;
}
#else
static int
setup_openssl_threading(void)
{
  return 0;
}
#endif

/** Uninitialize the crypto library. Return 0 on success, -1 on failure.
 */
int
crypto_global_cleanup(void)
{
  EVP_cleanup();
  ERR_remove_state(0);
  ERR_free_strings();

  if (dh_param_p)
    BN_clear_free(dh_param_p);
  if (dh_param_p_tls)
    BN_clear_free(dh_param_p_tls);
  if (dh_param_g)
    BN_clear_free(dh_param_g);

#ifndef DISABLE_ENGINES
  ENGINE_cleanup();
#endif

  CONF_modules_unload(1);
  CRYPTO_cleanup_all_ex_data();
#ifdef TOR_IS_MULTITHREADED
  if (n_openssl_mutexes_) {
    int n = n_openssl_mutexes_;
    tor_mutex_t **ms = openssl_mutexes_;
    int i;
    openssl_mutexes_ = NULL;
    n_openssl_mutexes_ = 0;
    for (i=0;i<n;++i) {
      tor_mutex_free(ms[i]);
    }
    tor_free(ms);
  }
#endif
  tor_free(crypto_openssl_version_str);
  tor_free(crypto_openssl_header_version_str);
  return 0;
}

/** @} */

