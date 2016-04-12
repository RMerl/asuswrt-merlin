/* Copyright (c) 2012-2015, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"

#define ONION_NTOR_PRIVATE
#include "crypto.h"
#include "onion_ntor.h"
#include "torlog.h"
#include "util.h"

/** Free storage held in an ntor handshake state. */
void
ntor_handshake_state_free(ntor_handshake_state_t *state)
{
  if (!state)
    return;
  memwipe(state, 0, sizeof(*state));
  tor_free(state);
}

/** Convenience function to represent HMAC_SHA256 as our instantiation of
 * ntor's "tweaked hash'.  Hash the <b>inp_len</b> bytes at <b>inp</b> into
 * a DIGEST256_LEN-byte digest at <b>out</b>, with the hash changing
 * depending on the value of <b>tweak</b>. */
static void
h_tweak(uint8_t *out,
        const uint8_t *inp, size_t inp_len,
        const char *tweak)
{
  size_t tweak_len = strlen(tweak);
  crypto_hmac_sha256((char*)out, tweak, tweak_len, (const char*)inp, inp_len);
}

/** Wrapper around a set of tweak-values for use with the ntor handshake. */
typedef struct tweakset_t {
  const char *t_mac;
  const char *t_key;
  const char *t_verify;
  const char *m_expand;
} tweakset_t;

/** The tweaks to be used with our handshake. */
const tweakset_t proto1_tweaks = {
#define PROTOID "ntor-curve25519-sha256-1"
#define PROTOID_LEN 24
  PROTOID ":mac",
  PROTOID ":key_extract",
  PROTOID ":verify",
  PROTOID ":key_expand"
};

/** Convenience macro: copy <b>len</b> bytes from <b>inp</b> to <b>ptr</b>,
 * and advance <b>ptr</b> by the number of bytes copied. */
#define APPEND(ptr, inp, len)                   \
  STMT_BEGIN {                                  \
    memcpy(ptr, (inp), (len));                  \
    ptr += len;                                 \
  } STMT_END

/**
 * Compute the first client-side step of the ntor handshake for communicating
 * with a server whose DIGEST_LEN-byte server identity is <b>router_id</b>,
 * and whose onion key is <b>router_key</b>. Store the NTOR_ONIONSKIN_LEN-byte
 * message in <b>onion_skin_out</b>, and store the handshake state in
 * *<b>handshake_state_out</b>.  Return 0 on success, -1 on failure.
 */
int
onion_skin_ntor_create(const uint8_t *router_id,
                       const curve25519_public_key_t *router_key,
                       ntor_handshake_state_t **handshake_state_out,
                       uint8_t *onion_skin_out)
{
  ntor_handshake_state_t *state;
  uint8_t *op;

  state = tor_malloc_zero(sizeof(ntor_handshake_state_t));

  memcpy(state->router_id, router_id, DIGEST_LEN);
  memcpy(&state->pubkey_B, router_key, sizeof(curve25519_public_key_t));
  if (curve25519_secret_key_generate(&state->seckey_x, 0) < 0) {
    tor_free(state);
    return -1;
  }
  curve25519_public_key_generate(&state->pubkey_X, &state->seckey_x);

  op = onion_skin_out;
  APPEND(op, router_id, DIGEST_LEN);
  APPEND(op, router_key->public_key, CURVE25519_PUBKEY_LEN);
  APPEND(op, state->pubkey_X.public_key, CURVE25519_PUBKEY_LEN);
  tor_assert(op == onion_skin_out + NTOR_ONIONSKIN_LEN);

  *handshake_state_out = state;

  return 0;
}

#define SERVER_STR "Server"
#define SERVER_STR_LEN 6

#define SECRET_INPUT_LEN (CURVE25519_PUBKEY_LEN * 3 +   \
                          CURVE25519_OUTPUT_LEN * 2 +   \
                          DIGEST_LEN + PROTOID_LEN)
#define AUTH_INPUT_LEN (DIGEST256_LEN + DIGEST_LEN +    \
                        CURVE25519_PUBKEY_LEN*3 +       \
                        PROTOID_LEN + SERVER_STR_LEN)

/**
 * Perform the server side of an ntor handshake. Given an
 * NTOR_ONIONSKIN_LEN-byte message in <b>onion_skin</b>, our own identity
 * fingerprint as <b>my_node_id</b>, and an associative array mapping public
 * onion keys to curve25519_keypair_t in <b>private_keys</b>, attempt to
 * perform the handshake.  Use <b>junk_keys</b> if present if the handshake
 * indicates an unrecognized public key.  Write an NTOR_REPLY_LEN-byte
 * message to send back to the client into <b>handshake_reply_out</b>, and
 * generate <b>key_out_len</b> bytes of key material in <b>key_out</b>. Return
 * 0 on success, -1 on failure.
 */
int
onion_skin_ntor_server_handshake(const uint8_t *onion_skin,
                                 const di_digest256_map_t *private_keys,
                                 const curve25519_keypair_t *junk_keys,
                                 const uint8_t *my_node_id,
                                 uint8_t *handshake_reply_out,
                                 uint8_t *key_out,
                                 size_t key_out_len)
{
  const tweakset_t *T = &proto1_tweaks;
  /* Sensitive stack-allocated material. Kept in an anonymous struct to make
   * it easy to wipe. */
  struct {
    uint8_t secret_input[SECRET_INPUT_LEN];
    uint8_t auth_input[AUTH_INPUT_LEN];
    curve25519_public_key_t pubkey_X;
    curve25519_secret_key_t seckey_y;
    curve25519_public_key_t pubkey_Y;
    uint8_t verify[DIGEST256_LEN];
  } s;
  uint8_t *si = s.secret_input, *ai = s.auth_input;
  const curve25519_keypair_t *keypair_bB;
  int bad;

  /* Decode the onion skin */
  /* XXXX Does this possible early-return business threaten our security? */
  if (tor_memneq(onion_skin, my_node_id, DIGEST_LEN))
    return -1;
  /* Note that on key-not-found, we go through with this operation anyway,
   * using "junk_keys". This will result in failed authentication, but won't
   * leak whether we recognized the key. */
  keypair_bB = dimap_search(private_keys, onion_skin + DIGEST_LEN,
                            (void*)junk_keys);
  if (!keypair_bB)
    return -1;

  memcpy(s.pubkey_X.public_key, onion_skin+DIGEST_LEN+DIGEST256_LEN,
         CURVE25519_PUBKEY_LEN);

  /* Make y, Y */
  curve25519_secret_key_generate(&s.seckey_y, 0);
  curve25519_public_key_generate(&s.pubkey_Y, &s.seckey_y);

  /* NOTE: If we ever use a group other than curve25519, or a different
   * representation for its points, we may need to perform different or
   * additional checks on X here and on Y in the client handshake, or lose our
   * security properties. What checks we need would depend on the properties
   * of the group and its representation.
   *
   * In short: if you use anything other than curve25519, this aspect of the
   * code will need to be reconsidered carefully. */

  /* build secret_input */
  curve25519_handshake(si, &s.seckey_y, &s.pubkey_X);
  bad = safe_mem_is_zero(si, CURVE25519_OUTPUT_LEN);
  si += CURVE25519_OUTPUT_LEN;
  curve25519_handshake(si, &keypair_bB->seckey, &s.pubkey_X);
  bad |= safe_mem_is_zero(si, CURVE25519_OUTPUT_LEN);
  si += CURVE25519_OUTPUT_LEN;

  APPEND(si, my_node_id, DIGEST_LEN);
  APPEND(si, keypair_bB->pubkey.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(si, s.pubkey_X.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(si, s.pubkey_Y.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(si, PROTOID, PROTOID_LEN);
  tor_assert(si == s.secret_input + sizeof(s.secret_input));

  /* Compute hashes of secret_input */
  h_tweak(s.verify, s.secret_input, sizeof(s.secret_input), T->t_verify);

  /* Compute auth_input */
  APPEND(ai, s.verify, DIGEST256_LEN);
  APPEND(ai, my_node_id, DIGEST_LEN);
  APPEND(ai, keypair_bB->pubkey.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(ai, s.pubkey_Y.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(ai, s.pubkey_X.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(ai, PROTOID, PROTOID_LEN);
  APPEND(ai, SERVER_STR, SERVER_STR_LEN);
  tor_assert(ai == s.auth_input + sizeof(s.auth_input));

  /* Build the reply */
  memcpy(handshake_reply_out, s.pubkey_Y.public_key, CURVE25519_PUBKEY_LEN);
  h_tweak(handshake_reply_out+CURVE25519_PUBKEY_LEN,
          s.auth_input, sizeof(s.auth_input),
          T->t_mac);

  /* Generate the key material */
  crypto_expand_key_material_rfc5869_sha256(
                           s.secret_input, sizeof(s.secret_input),
                           (const uint8_t*)T->t_key, strlen(T->t_key),
                           (const uint8_t*)T->m_expand, strlen(T->m_expand),
                           key_out, key_out_len);

  /* Wipe all of our local state */
  memwipe(&s, 0, sizeof(s));

  return bad ? -1 : 0;
}

/**
 * Perform the final client side of the ntor handshake, using the state in
 * <b>handshake_state</b> and the server's NTOR_REPLY_LEN-byte reply in
 * <b>handshake_reply</b>.  Generate <b>key_out_len</b> bytes of key material
 * in <b>key_out</b>. Return 0 on success, -1 on failure.
 */
int
onion_skin_ntor_client_handshake(
                             const ntor_handshake_state_t *handshake_state,
                             const uint8_t *handshake_reply,
                             uint8_t *key_out,
                             size_t key_out_len,
                             const char **msg_out)
{
  const tweakset_t *T = &proto1_tweaks;
  /* Sensitive stack-allocated material. Kept in an anonymous struct to make
   * it easy to wipe. */
  struct {
    curve25519_public_key_t pubkey_Y;
    uint8_t secret_input[SECRET_INPUT_LEN];
    uint8_t verify[DIGEST256_LEN];
    uint8_t auth_input[AUTH_INPUT_LEN];
    uint8_t auth[DIGEST256_LEN];
  } s;
  uint8_t *ai = s.auth_input, *si = s.secret_input;
  const uint8_t *auth_candidate;
  int bad;

  /* Decode input */
  memcpy(s.pubkey_Y.public_key, handshake_reply, CURVE25519_PUBKEY_LEN);
  auth_candidate = handshake_reply + CURVE25519_PUBKEY_LEN;

  /* See note in server_handshake above about checking points.  The
   * circumstances under which we'd need to check Y for membership are
   * different than those under which we'd be checking X. */

  /* Compute secret_input */
  curve25519_handshake(si, &handshake_state->seckey_x, &s.pubkey_Y);
  bad = safe_mem_is_zero(si, CURVE25519_OUTPUT_LEN);
  si += CURVE25519_OUTPUT_LEN;
  curve25519_handshake(si, &handshake_state->seckey_x,
                       &handshake_state->pubkey_B);
  bad |= (safe_mem_is_zero(si, CURVE25519_OUTPUT_LEN) << 1);
  si += CURVE25519_OUTPUT_LEN;
  APPEND(si, handshake_state->router_id, DIGEST_LEN);
  APPEND(si, handshake_state->pubkey_B.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(si, handshake_state->pubkey_X.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(si, s.pubkey_Y.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(si, PROTOID, PROTOID_LEN);
  tor_assert(si == s.secret_input + sizeof(s.secret_input));

  /* Compute verify from secret_input */
  h_tweak(s.verify, s.secret_input, sizeof(s.secret_input), T->t_verify);

  /* Compute auth_input */
  APPEND(ai, s.verify, DIGEST256_LEN);
  APPEND(ai, handshake_state->router_id, DIGEST_LEN);
  APPEND(ai, handshake_state->pubkey_B.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(ai, s.pubkey_Y.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(ai, handshake_state->pubkey_X.public_key, CURVE25519_PUBKEY_LEN);
  APPEND(ai, PROTOID, PROTOID_LEN);
  APPEND(ai, SERVER_STR, SERVER_STR_LEN);
  tor_assert(ai == s.auth_input + sizeof(s.auth_input));

  /* Compute auth */
  h_tweak(s.auth, s.auth_input, sizeof(s.auth_input), T->t_mac);

  bad |= (tor_memneq(s.auth, auth_candidate, DIGEST256_LEN) << 2);

  crypto_expand_key_material_rfc5869_sha256(
                           s.secret_input, sizeof(s.secret_input),
                           (const uint8_t*)T->t_key, strlen(T->t_key),
                           (const uint8_t*)T->m_expand, strlen(T->m_expand),
                           key_out, key_out_len);

  memwipe(&s, 0, sizeof(s));

  if (bad) {
    if (bad & 4) {
      if (msg_out)
        *msg_out = NULL; /* Don't report this one; we probably just had the
                          * wrong onion key.*/
      log_fn(LOG_INFO, LD_PROTOCOL,
             "Invalid result from curve25519 handshake: %d", bad);
    }
    if (bad & 3) {
      if (msg_out)
        *msg_out = "Zero output from curve25519 handshake";
      log_fn(LOG_WARN, LD_PROTOCOL,
             "Invalid result from curve25519 handshake: %d", bad);
    }
  }

  return bad ? -1 : 0;
}

