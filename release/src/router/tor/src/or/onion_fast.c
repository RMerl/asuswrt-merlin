/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file onion_fast.c
 * \brief Functions implement the CREATE_FAST circuit handshake.
 *
 * The "CREATE_FAST" handshake is an unauthenticated, non-forward-secure
 * key derivation mechanism based on SHA1.  We used to use it for the
 * first hop of each circuit, since the TAP handshake provided no
 * additional security beyond the security already provided by the TLS
 * handshake [*].
 *
 * When we switched to ntor, we deprecated CREATE_FAST, since ntor is
 * stronger than our TLS handshake was, and fast enough to not be worrisome.
 *
 * This handshake, like the other circuit-extension handshakes, is
 * invoked from onion.c.
 *
 * [*]Actually, it's possible that TAP _was_ a little better than TLS with
 * RSA1024 certificates and EDH1024 for forward secrecy, if you
 * hypothesize an adversary who can compute discrete logarithms on a
 * small number of targetted DH1024 fields, but who can't break all that
 * many RSA1024 keys.
 **/

#include "or.h"
#include "onion_fast.h"

/** Release all state held in <b>victim</b>. */
void
fast_handshake_state_free(fast_handshake_state_t *victim)
{
  if (! victim)
    return;
  memwipe(victim, 0, sizeof(fast_handshake_state_t));
  tor_free(victim);
}

/** Create the state needed to perform a CREATE_FAST handshake. Return 0
 * on success, -1 on failure. */
int
fast_onionskin_create(fast_handshake_state_t **handshake_state_out,
                      uint8_t *handshake_out)
{
  fast_handshake_state_t *s;
  *handshake_state_out = s = tor_malloc(sizeof(fast_handshake_state_t));
  crypto_rand((char*)s->state, sizeof(s->state));
  memcpy(handshake_out, s->state, DIGEST_LEN);
  return 0;
}

/** Implement the server side of the CREATE_FAST abbreviated handshake.  The
 * client has provided DIGEST_LEN key bytes in <b>key_in</b> ("x").  We
 * generate a reply of DIGEST_LEN*2 bytes in <b>key_out</b>, consisting of a
 * new random "y", followed by H(x|y) to check for correctness.  We set
 * <b>key_out_len</b> bytes of key material in <b>key_out</b>.
 * Return 0 on success, &lt;0 on failure.
 **/
int
fast_server_handshake(const uint8_t *key_in, /* DIGEST_LEN bytes */
                      uint8_t *handshake_reply_out, /* DIGEST_LEN*2 bytes */
                      uint8_t *key_out,
                      size_t key_out_len)
{
  uint8_t tmp[DIGEST_LEN+DIGEST_LEN];
  uint8_t *out = NULL;
  size_t out_len;
  int r = -1;

  crypto_rand((char*)handshake_reply_out, DIGEST_LEN);

  memcpy(tmp, key_in, DIGEST_LEN);
  memcpy(tmp+DIGEST_LEN, handshake_reply_out, DIGEST_LEN);
  out_len = key_out_len+DIGEST_LEN;
  out = tor_malloc(out_len);
  if (BUG(crypto_expand_key_material_TAP(tmp, sizeof(tmp), out, out_len))) {
    goto done; // LCOV_EXCL_LINE
  }
  memcpy(handshake_reply_out+DIGEST_LEN, out, DIGEST_LEN);
  memcpy(key_out, out+DIGEST_LEN, key_out_len);
  r = 0;
 done:
  memwipe(tmp, 0, sizeof(tmp));
  memwipe(out, 0, out_len);
  tor_free(out);
  return r;
}

/** Implement the second half of the client side of the CREATE_FAST handshake.
 * We sent the server <b>handshake_state</b> ("x") already, and the server
 * told us <b>handshake_reply_out</b> (y|H(x|y)).  Make sure that the hash is
 * correct, and generate key material in <b>key_out</b>.  Return 0 on success,
 * true on failure.
 *
 * NOTE: The "CREATE_FAST" handshake path is distinguishable from regular
 * "onionskin" handshakes, and is not secure if an adversary can see or modify
 * the messages.  Therefore, it should only be used by clients, and only as
 * the first hop of a circuit (since the first hop is already authenticated
 * and protected by TLS).
 */
int
fast_client_handshake(const fast_handshake_state_t *handshake_state,
                      const uint8_t *handshake_reply_out,/*DIGEST_LEN*2 bytes*/
                      uint8_t *key_out,
                      size_t key_out_len,
                      const char **msg_out)
{
  uint8_t tmp[DIGEST_LEN+DIGEST_LEN];
  uint8_t *out;
  size_t out_len;
  int r = -1;

  memcpy(tmp, handshake_state->state, DIGEST_LEN);
  memcpy(tmp+DIGEST_LEN, handshake_reply_out, DIGEST_LEN);
  out_len = key_out_len+DIGEST_LEN;
  out = tor_malloc(out_len);
  if (BUG(crypto_expand_key_material_TAP(tmp, sizeof(tmp), out, out_len))) {
    /* LCOV_EXCL_START */
    if (msg_out)
      *msg_out = "Failed to expand key material";
    goto done;
    /* LCOV_EXCL_STOP */
  }
  if (tor_memneq(out, handshake_reply_out+DIGEST_LEN, DIGEST_LEN)) {
    /* H(K) does *not* match. Something fishy. */
    if (msg_out)
      *msg_out = "Digest DOES NOT MATCH on fast handshake. Bug or attack.";
    goto done;
  }
  memcpy(key_out, out+DIGEST_LEN, key_out_len);
  r = 0;
 done:
  memwipe(tmp, 0, sizeof(tmp));
  memwipe(out, 0, out_len);
  tor_free(out);
  return r;
}

