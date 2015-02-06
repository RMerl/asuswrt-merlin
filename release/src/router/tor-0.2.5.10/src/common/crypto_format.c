/* Copyright (c) 2012-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/* Formatting and parsing code for crypto-related data structures. */

#include "orconfig.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "crypto.h"
#include "crypto_curve25519.h"
#include "util.h"
#include "torlog.h"

int
curve25519_public_to_base64(char *output,
                            const curve25519_public_key_t *pkey)
{
  char buf[128];
  base64_encode(buf, sizeof(buf),
                (const char*)pkey->public_key, CURVE25519_PUBKEY_LEN);
  buf[CURVE25519_BASE64_PADDED_LEN] = '\0';
  memcpy(output, buf, CURVE25519_BASE64_PADDED_LEN+1);
  return 0;
}

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

