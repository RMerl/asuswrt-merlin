/* Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rendmid.c
 * \brief Implement introductions points and rendezvous points.
 **/

#include "or.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "config.h"
#include "relay.h"
#include "rendmid.h"
#include "rephist.h"

/** Respond to an ESTABLISH_INTRO cell by checking the signed data and
 * setting the circuit's purpose and service pk digest.
 */
int
rend_mid_establish_intro(or_circuit_t *circ, const uint8_t *request,
                         size_t request_len)
{
  crypto_pk_t *pk = NULL;
  char buf[DIGEST_LEN+9];
  char expected_digest[DIGEST_LEN];
  char pk_digest[DIGEST_LEN];
  size_t asn1len;
  or_circuit_t *c;
  char serviceid[REND_SERVICE_ID_LEN_BASE32+1];
  int reason = END_CIRC_REASON_INTERNAL;

  log_info(LD_REND,
           "Received an ESTABLISH_INTRO request on circuit %u",
           (unsigned) circ->p_circ_id);

  if (circ->base_.purpose != CIRCUIT_PURPOSE_OR || circ->base_.n_chan) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
         "Rejecting ESTABLISH_INTRO on non-OR or non-edge circuit.");
    reason = END_CIRC_REASON_TORPROTOCOL;
    goto err;
  }
  if (request_len < 2+DIGEST_LEN)
    goto truncated;
  /* First 2 bytes: length of asn1-encoded key. */
  asn1len = ntohs(get_uint16(request));

  /* Next asn1len bytes: asn1-encoded key. */
  if (request_len < 2+DIGEST_LEN+asn1len)
    goto truncated;
  pk = crypto_pk_asn1_decode((char*)(request+2), asn1len);
  if (!pk) {
    reason = END_CIRC_REASON_TORPROTOCOL;
    log_warn(LD_PROTOCOL, "Couldn't decode public key.");
    goto err;
  }

  /* Next 20 bytes: Hash of rend_circ_nonce | "INTRODUCE" */
  memcpy(buf, circ->rend_circ_nonce, DIGEST_LEN);
  memcpy(buf+DIGEST_LEN, "INTRODUCE", 9);
  if (crypto_digest(expected_digest, buf, DIGEST_LEN+9) < 0) {
    log_warn(LD_BUG, "Internal error computing digest.");
    goto err;
  }
  if (tor_memneq(expected_digest, request+2+asn1len, DIGEST_LEN)) {
    log_warn(LD_PROTOCOL, "Hash of session info was not as expected.");
    reason = END_CIRC_REASON_TORPROTOCOL;
    goto err;
  }
  /* Rest of body: signature of previous data */
  note_crypto_pk_op(REND_MID);
  if (crypto_pk_public_checksig_digest(pk,
                                       (char*)request, 2+asn1len+DIGEST_LEN,
                                       (char*)(request+2+DIGEST_LEN+asn1len),
                                       request_len-(2+DIGEST_LEN+asn1len))<0) {
    log_warn(LD_PROTOCOL,
             "Incorrect signature on ESTABLISH_INTRO cell; rejecting.");
    reason = END_CIRC_REASON_TORPROTOCOL;
    goto err;
  }

  /* The request is valid.  First, compute the hash of Bob's PK.*/
  if (crypto_pk_get_digest(pk, pk_digest)<0) {
    log_warn(LD_BUG, "Internal error: couldn't hash public key.");
    goto err;
  }

  crypto_pk_free(pk); /* don't need it anymore */
  pk = NULL; /* so we don't free it again if err */

  base32_encode(serviceid, REND_SERVICE_ID_LEN_BASE32+1,
                pk_digest, REND_SERVICE_ID_LEN);

  /* Close any other intro circuits with the same pk. */
  c = NULL;
  while ((c = circuit_get_intro_point((const uint8_t *)pk_digest))) {
    log_info(LD_REND, "Replacing old circuit for service %s",
             safe_str(serviceid));
    circuit_mark_for_close(TO_CIRCUIT(c), END_CIRC_REASON_FINISHED);
    /* Now it's marked, and it won't be returned next time. */
  }

  /* Acknowledge the request. */
  if (relay_send_command_from_edge(0, TO_CIRCUIT(circ),
                                   RELAY_COMMAND_INTRO_ESTABLISHED,
                                   "", 0, NULL)<0) {
    log_info(LD_GENERAL, "Couldn't send INTRO_ESTABLISHED cell.");
    goto err;
  }

  /* Now, set up this circuit. */
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_INTRO_POINT);
  circuit_set_intro_point_digest(circ, (uint8_t *)pk_digest);

  log_info(LD_REND,
           "Established introduction point on circuit %u for service %s",
           (unsigned) circ->p_circ_id, safe_str(serviceid));

  return 0;
 truncated:
  log_warn(LD_PROTOCOL, "Rejecting truncated ESTABLISH_INTRO cell.");
  reason = END_CIRC_REASON_TORPROTOCOL;
 err:
  if (pk) crypto_pk_free(pk);
  circuit_mark_for_close(TO_CIRCUIT(circ), reason);
  return -1;
}

/** Process an INTRODUCE1 cell by finding the corresponding introduction
 * circuit, and relaying the body of the INTRODUCE1 cell inside an
 * INTRODUCE2 cell.
 */
int
rend_mid_introduce(or_circuit_t *circ, const uint8_t *request,
                   size_t request_len)
{
  or_circuit_t *intro_circ;
  char serviceid[REND_SERVICE_ID_LEN_BASE32+1];
  char nak_body[1];

  log_info(LD_REND, "Received an INTRODUCE1 request on circuit %u",
           (unsigned)circ->p_circ_id);

  if (circ->base_.purpose != CIRCUIT_PURPOSE_OR || circ->base_.n_chan) {
    log_warn(LD_PROTOCOL,
             "Rejecting INTRODUCE1 on non-OR or non-edge circuit %u.",
             (unsigned)circ->p_circ_id);
    goto err;
  }

  /* We could change this to MAX_HEX_NICKNAME_LEN now that 0.0.9.x is
   * obsolete; however, there isn't much reason to do so, and we're going
   * to revise this protocol anyway.
   */
  if (request_len < (DIGEST_LEN+(MAX_NICKNAME_LEN+1)+REND_COOKIE_LEN+
                     DH_KEY_LEN+CIPHER_KEY_LEN+PKCS1_OAEP_PADDING_OVERHEAD)) {
    log_warn(LD_PROTOCOL, "Impossibly short INTRODUCE1 cell on circuit %u; "
             "responding with nack.",
             (unsigned)circ->p_circ_id);
    goto err;
  }

  base32_encode(serviceid, REND_SERVICE_ID_LEN_BASE32+1,
                (char*)request, REND_SERVICE_ID_LEN);

  /* The first 20 bytes are all we look at: they have a hash of Bob's PK. */
  intro_circ = circuit_get_intro_point((const uint8_t*)request);
  if (!intro_circ) {
    log_info(LD_REND,
             "No intro circ found for INTRODUCE1 cell (%s) from circuit %u; "
             "responding with nack.",
             safe_str(serviceid), (unsigned)circ->p_circ_id);
    goto err;
  }

  log_info(LD_REND,
           "Sending introduction request for service %s "
           "from circ %u to circ %u",
           safe_str(serviceid), (unsigned)circ->p_circ_id,
           (unsigned)intro_circ->p_circ_id);

  /* Great.  Now we just relay the cell down the circuit. */
  if (relay_send_command_from_edge(0, TO_CIRCUIT(intro_circ),
                                   RELAY_COMMAND_INTRODUCE2,
                                   (char*)request, request_len, NULL)) {
    log_warn(LD_GENERAL,
             "Unable to send INTRODUCE2 cell to Tor client.");
    goto err;
  }
  /* And sent an ack down Alice's circuit.  Empty body means succeeded. */
  if (relay_send_command_from_edge(0,TO_CIRCUIT(circ),
                                   RELAY_COMMAND_INTRODUCE_ACK,
                                   NULL,0,NULL)) {
    log_warn(LD_GENERAL, "Unable to send INTRODUCE_ACK cell to Tor client.");
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_INTERNAL);
    return -1;
  }

  return 0;
 err:
  /* Send the client an NACK */
  nak_body[0] = 1;
  if (relay_send_command_from_edge(0,TO_CIRCUIT(circ),
                                   RELAY_COMMAND_INTRODUCE_ACK,
                                   nak_body, 1, NULL)) {
    log_warn(LD_GENERAL, "Unable to send NAK to Tor client.");
    /* Is this right? */
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_INTERNAL);
  }
  return -1;
}

/** Process an ESTABLISH_RENDEZVOUS cell by setting the circuit's purpose and
 * rendezvous cookie.
 */
int
rend_mid_establish_rendezvous(or_circuit_t *circ, const uint8_t *request,
                              size_t request_len)
{
  char hexid[9];
  int reason = END_CIRC_REASON_TORPROTOCOL;

  log_info(LD_REND, "Received an ESTABLISH_RENDEZVOUS request on circuit %u",
           (unsigned)circ->p_circ_id);

  if (circ->base_.purpose != CIRCUIT_PURPOSE_OR) {
    log_warn(LD_PROTOCOL,
             "Tried to establish rendezvous on non-OR circuit with purpose %s",
             circuit_purpose_to_string(circ->base_.purpose));
    goto err;
  }

  if (circ->base_.n_chan) {
    log_warn(LD_PROTOCOL,
             "Tried to establish rendezvous on non-edge circuit");
    goto err;
  }

  if (request_len != REND_COOKIE_LEN) {
    log_fn(LOG_PROTOCOL_WARN,
           LD_PROTOCOL, "Invalid length on ESTABLISH_RENDEZVOUS.");
    goto err;
  }

  if (circuit_get_rendezvous(request)) {
    log_warn(LD_PROTOCOL,
             "Duplicate rendezvous cookie in ESTABLISH_RENDEZVOUS.");
    goto err;
  }

  /* Acknowledge the request. */
  if (relay_send_command_from_edge(0,TO_CIRCUIT(circ),
                                   RELAY_COMMAND_RENDEZVOUS_ESTABLISHED,
                                   "", 0, NULL)<0) {
    log_warn(LD_PROTOCOL, "Couldn't send RENDEZVOUS_ESTABLISHED cell.");
    reason = END_CIRC_REASON_INTERNAL;
    goto err;
  }

  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_REND_POINT_WAITING);
  circuit_set_rendezvous_cookie(circ, request);

  base16_encode(hexid,9,(char*)request,4);

  log_info(LD_REND,
           "Established rendezvous point on circuit %u for cookie %s",
           (unsigned)circ->p_circ_id, hexid);

  return 0;
 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), reason);
  return -1;
}

/** Process a RENDEZVOUS1 cell by looking up the correct rendezvous
 * circuit by its relaying the cell's body in a RENDEZVOUS2 cell, and
 * connecting the two circuits.
 */
int
rend_mid_rendezvous(or_circuit_t *circ, const uint8_t *request,
                    size_t request_len)
{
  or_circuit_t *rend_circ;
  char hexid[9];
  int reason = END_CIRC_REASON_INTERNAL;

  if (circ->base_.purpose != CIRCUIT_PURPOSE_OR || circ->base_.n_chan) {
    log_info(LD_REND,
             "Tried to complete rendezvous on non-OR or non-edge circuit %u.",
             (unsigned)circ->p_circ_id);
    reason = END_CIRC_REASON_TORPROTOCOL;
    goto err;
  }

  if (request_len != REND_COOKIE_LEN+DH_KEY_LEN+DIGEST_LEN) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
         "Rejecting RENDEZVOUS1 cell with bad length (%d) on circuit %u.",
         (int)request_len, (unsigned)circ->p_circ_id);
    reason = END_CIRC_REASON_TORPROTOCOL;
    goto err;
  }

  base16_encode(hexid, sizeof(hexid), (const char*)request, 4);

  log_info(LD_REND,
           "Got request for rendezvous from circuit %u to cookie %s.",
           (unsigned)circ->p_circ_id, hexid);

  rend_circ = circuit_get_rendezvous(request);
  if (!rend_circ) {
    log_fn(LOG_PROTOCOL_WARN, LD_PROTOCOL,
         "Rejecting RENDEZVOUS1 cell with unrecognized rendezvous cookie %s.",
         hexid);
    reason = END_CIRC_REASON_TORPROTOCOL;
    goto err;
  }

  /* Send the RENDEZVOUS2 cell to Alice. */
  if (relay_send_command_from_edge(0, TO_CIRCUIT(rend_circ),
                                   RELAY_COMMAND_RENDEZVOUS2,
                                   (char*)(request+REND_COOKIE_LEN),
                                   request_len-REND_COOKIE_LEN, NULL)) {
    log_warn(LD_GENERAL,
             "Unable to send RENDEZVOUS2 cell to client on circuit %u.",
             (unsigned)rend_circ->p_circ_id);
    goto err;
  }

  /* Join the circuits. */
  log_info(LD_REND,
           "Completing rendezvous: circuit %u joins circuit %u (cookie %s)",
           (unsigned)circ->p_circ_id, (unsigned)rend_circ->p_circ_id, hexid);

  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_REND_ESTABLISHED);
  circuit_change_purpose(TO_CIRCUIT(rend_circ),
                         CIRCUIT_PURPOSE_REND_ESTABLISHED);
  circuit_set_rendezvous_cookie(circ, NULL);

  rend_circ->rend_splice = circ;
  circ->rend_splice = rend_circ;

  return 0;
 err:
  circuit_mark_for_close(TO_CIRCUIT(circ), reason);
  return -1;
}

