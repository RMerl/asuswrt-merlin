/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file crypto_format.c
 *
 * \brief Formatting and parsing code for crypto-related data structures.
 */

#include "orconfig.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "container.h"
#include "crypto.h"
#include "crypto_curve25519.h"
#include "crypto_ed25519.h"
#include "crypto_format.h"
#include "util.h"
#include "util_format.h"
#include "torlog.h"

/** Write the <b>datalen</b> bytes from <b>data</b> to the file named
 * <b>fname</b> in the tagged-data format.  This format contains a
 * 32-byte header, followed by the data itself.  The header is the
 * NUL-padded string "== <b>typestring</b>: <b>tag</b> ==".  The length
 * of <b>typestring</b> and <b>tag</b> must therefore be no more than
 * 24.
 **/
int
crypto_write_tagged_contents_to_file(const char *fname,
                                     const char *typestring,
                                     const char *tag,
                                     const uint8_t *data,
                                     size_t datalen)
{
  char header[32];
  smartlist_t *chunks = smartlist_new();
  sized_chunk_t ch0, ch1;
  int r = -1;

  memset(header, 0, sizeof(header));
  if (tor_snprintf(header, sizeof(header),
                   "== %s: %s ==", typestring, tag) < 0)
    goto end;
  ch0.bytes = header;
  ch0.len = 32;
  ch1.bytes = (const char*) data;
  ch1.len = datalen;
  smartlist_add(chunks, &ch0);
  smartlist_add(chunks, &ch1);

  r = write_chunks_to_file(fname, chunks, 1, 0);

 end:
  smartlist_free(chunks);
  return r;
}

/** Read a tagged-data file from <b>fname</b> into the
 * <b>data_out_len</b>-byte buffer in <b>data_out</b>. Check that the
 * typestring matches <b>typestring</b>; store the tag into a newly allocated
 * string in <b>tag_out</b>. Return -1 on failure, and the number of bytes of
 * data on success.  Preserves the errno from reading the file. */
ssize_t
crypto_read_tagged_contents_from_file(const char *fname,
                                      const char *typestring,
                                      char **tag_out,
                                      uint8_t *data_out,
                                      ssize_t data_out_len)
{
  char prefix[33];
  char *content = NULL;
  struct stat st;
  ssize_t r = -1;
  size_t st_size = 0;
  int saved_errno = 0;

  *tag_out = NULL;
  st.st_size = 0;
  content = read_file_to_str(fname, RFTS_BIN|RFTS_IGNORE_MISSING, &st);
  if (! content) {
    saved_errno = errno;
    goto end;
  }
  if (st.st_size < 32 || st.st_size > 32 + data_out_len) {
    saved_errno = EINVAL;
    goto end;
  }
  st_size = (size_t)st.st_size;

  memcpy(prefix, content, 32);
  prefix[32] = 0;
  /* Check type, extract tag. */
  if (strcmpstart(prefix, "== ") || strcmpend(prefix, " ==") ||
      ! tor_mem_is_zero(prefix+strlen(prefix), 32-strlen(prefix))) {
    saved_errno = EINVAL;
    goto end;
  }

  if (strcmpstart(prefix+3, typestring) ||
      3+strlen(typestring) >= 32 ||
      strcmpstart(prefix+3+strlen(typestring), ": ")) {
    saved_errno = EINVAL;
    goto end;
  }

  *tag_out = tor_strndup(prefix+5+strlen(typestring),
                         strlen(prefix)-8-strlen(typestring));

  memcpy(data_out, content+32, st_size-32);
  r = st_size - 32;

 end:
  if (content)
    memwipe(content, 0, st_size);
  tor_free(content);
  if (saved_errno)
    errno = saved_errno;
  return r;
}

/** Encode <b>pkey</b> as a base64-encoded string, without trailing "="
 * characters, in the buffer <b>output</b>, which must have at least
 * CURVE25519_BASE64_PADDED_LEN+1 bytes available.  Return 0 on success, -1 on
 * failure. */
int
curve25519_public_to_base64(char *output,
                            const curve25519_public_key_t *pkey)
{
  char buf[128];
  base64_encode(buf, sizeof(buf),
                (const char*)pkey->public_key, CURVE25519_PUBKEY_LEN, 0);
  buf[CURVE25519_BASE64_PADDED_LEN] = '\0';
  memcpy(output, buf, CURVE25519_BASE64_PADDED_LEN+1);
  return 0;
}

/** Try to decode a base64-encoded curve25519 public key from <b>input</b>
 * into the object at <b>pkey</b>. Return 0 on success, -1 on failure.
 * Accepts keys with or without a trailing "=". */
int
curve25519_public_from_base64(curve25519_public_key_t *pkey,
                              const char *input)
{
  size_t len = strlen(input);
  if (len == CURVE25519_BASE64_PADDED_LEN - 1) {
    /* not padded */
    return digest256_from_base64((char*)pkey->public_key, input);
  } else if (len == CURVE25519_BASE64_PADDED_LEN) {
    char buf[128];
    if (base64_decode(buf, sizeof(buf), input, len) != CURVE25519_PUBKEY_LEN)
      return -1;
    memcpy(pkey->public_key, buf, CURVE25519_PUBKEY_LEN);
    return 0;
  } else {
    return -1;
  }
}

/** Try to decode the string <b>input</b> into an ed25519 public key. On
 * success, store the value in <b>pkey</b> and return 0. Otherwise return
 * -1. */
int
ed25519_public_from_base64(ed25519_public_key_t *pkey,
                           const char *input)
{
  return digest256_from_base64((char*)pkey->pubkey, input);
}

/** Encode the public key <b>pkey</b> into the buffer at <b>output</b>,
 * which must have space for ED25519_BASE64_LEN bytes of encoded key,
 * plus one byte for a terminating NUL.  Return 0 on success, -1 on failure.
 */
int
ed25519_public_to_base64(char *output,
                         const ed25519_public_key_t *pkey)
{
  return digest256_to_base64(output, (const char *)pkey->pubkey);
}

/** Encode the signature <b>sig</b> into the buffer at <b>output</b>,
 * which must have space for ED25519_SIG_BASE64_LEN bytes of encoded signature,
 * plus one byte for a terminating NUL.  Return 0 on success, -1 on failure.
 */
int
ed25519_signature_to_base64(char *output,
                            const ed25519_signature_t *sig)
{
  char buf[256];
  int n = base64_encode_nopad(buf, sizeof(buf), sig->sig, ED25519_SIG_LEN);
  tor_assert(n == ED25519_SIG_BASE64_LEN);
  memcpy(output, buf, ED25519_SIG_BASE64_LEN+1);
  return 0;
}

/** Try to decode the string <b>input</b> into an ed25519 signature. On
 * success, store the value in <b>sig</b> and return 0. Otherwise return
 * -1. */
int
ed25519_signature_from_base64(ed25519_signature_t *sig,
                              const char *input)
{

  if (strlen(input) != ED25519_SIG_BASE64_LEN)
    return -1;
  char buf[ED25519_SIG_BASE64_LEN+3];
  memcpy(buf, input, ED25519_SIG_BASE64_LEN);
  buf[ED25519_SIG_BASE64_LEN+0] = '=';
  buf[ED25519_SIG_BASE64_LEN+1] = '=';
  buf[ED25519_SIG_BASE64_LEN+2] = 0;
  char decoded[128];
  int n = base64_decode(decoded, sizeof(decoded), buf, strlen(buf));
  if (n < 0 || n != ED25519_SIG_LEN)
    return -1;
  memcpy(sig->sig, decoded, ED25519_SIG_LEN);

  return 0;
}

/** Base64 encode DIGEST_LINE bytes from <b>digest</b>, remove the trailing =
 * characters, and store the nul-terminated result in the first
 * BASE64_DIGEST_LEN+1 bytes of <b>d64</b>.  */
/* XXXX unify with crypto_format.c code */
int
digest_to_base64(char *d64, const char *digest)
{
  char buf[256];
  base64_encode(buf, sizeof(buf), digest, DIGEST_LEN, 0);
  buf[BASE64_DIGEST_LEN] = '\0';
  memcpy(d64, buf, BASE64_DIGEST_LEN+1);
  return 0;
}

/** Given a base64 encoded, nul-terminated digest in <b>d64</b> (without
 * trailing newline or = characters), decode it and store the result in the
 * first DIGEST_LEN bytes at <b>digest</b>. */
/* XXXX unify with crypto_format.c code */
int
digest_from_base64(char *digest, const char *d64)
{
  if (base64_decode(digest, DIGEST_LEN, d64, strlen(d64)) == DIGEST_LEN)
    return 0;
  else
    return -1;
}

/** Base64 encode DIGEST256_LINE bytes from <b>digest</b>, remove the
 * trailing = characters, and store the nul-terminated result in the first
 * BASE64_DIGEST256_LEN+1 bytes of <b>d64</b>. */
 /* XXXX unify with crypto_format.c code */
int
digest256_to_base64(char *d64, const char *digest)
{
  char buf[256];
  base64_encode(buf, sizeof(buf), digest, DIGEST256_LEN, 0);
  buf[BASE64_DIGEST256_LEN] = '\0';
  memcpy(d64, buf, BASE64_DIGEST256_LEN+1);
  return 0;
}

/** Given a base64 encoded, nul-terminated digest in <b>d64</b> (without
 * trailing newline or = characters), decode it and store the result in the
 * first DIGEST256_LEN bytes at <b>digest</b>. */
/* XXXX unify with crypto_format.c code */
int
digest256_from_base64(char *digest, const char *d64)
{
  if (base64_decode(digest, DIGEST256_LEN, d64, strlen(d64)) == DIGEST256_LEN)
    return 0;
  else
    return -1;
}

