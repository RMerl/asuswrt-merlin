/* Copyright (c) 2001, Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#ifndef TOR_CRYPTO_FORMAT_H
#define TOR_CRYPTO_FORMAT_H

#include "testsupport.h"
#include "torint.h"
#include "crypto_ed25519.h"

int crypto_write_tagged_contents_to_file(const char *fname,
                                         const char *typestring,
                                         const char *tag,
                                         const uint8_t *data,
                                         size_t datalen);

ssize_t crypto_read_tagged_contents_from_file(const char *fname,
                                              const char *typestring,
                                              char **tag_out,
                                              uint8_t *data_out,
                                              ssize_t data_out_len);

#define ED25519_BASE64_LEN 43
int ed25519_public_from_base64(ed25519_public_key_t *pkey,
                               const char *input);
int ed25519_public_to_base64(char *output,
                             const ed25519_public_key_t *pkey);

/* XXXX move these to crypto_format.h */
#define ED25519_SIG_BASE64_LEN 86

int ed25519_signature_from_base64(ed25519_signature_t *sig,
                                  const char *input);
int ed25519_signature_to_base64(char *output,
                                const ed25519_signature_t *sig);

int digest_to_base64(char *d64, const char *digest);
int digest_from_base64(char *digest, const char *d64);
int digest256_to_base64(char *d64, const char *digest);
int digest256_from_base64(char *digest, const char *d64);

#endif

