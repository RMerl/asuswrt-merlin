/* Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rendclient.c
 * \brief Client code to access location-hidden services.
 **/

#include "or.h"
#include "circpathbias.h"
#include "circuitbuild.h"
#include "circuitlist.h"
#include "circuituse.h"
#include "config.h"
#include "connection.h"
#include "connection_edge.h"
#include "directory.h"
#include "main.h"
#include "networkstatus.h"
#include "nodelist.h"
#include "relay.h"
#include "rendclient.h"
#include "rendcommon.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"
#include "routerset.h"
#include "control.h"

static extend_info_t *rend_client_get_random_intro_impl(
                          const rend_cache_entry_t *rend_query,
                          const int strict, const int warnings);

/** Purge all potentially remotely-detectable state held in the hidden
 * service client code.  Called on SIGNAL NEWNYM. */
void
rend_client_purge_state(void)
{
  rend_cache_purge();
  rend_client_cancel_descriptor_fetches();
  rend_client_purge_last_hid_serv_requests();
}

/** Called when we've established a circuit to an introduction point:
 * send the introduction request. */
void
rend_client_introcirc_has_opened(origin_circuit_t *circ)
{
  tor_assert(circ->base_.purpose == CIRCUIT_PURPOSE_C_INTRODUCING);
  tor_assert(circ->cpath);

  log_info(LD_REND,"introcirc is open");
  connection_ap_attach_pending();
}

/** Send the establish-rendezvous cell along a rendezvous circuit. if
 * it fails, mark the circ for close and return -1. else return 0.
 */
static int
rend_client_send_establish_rendezvous(origin_circuit_t *circ)
{
  tor_assert(circ->base_.purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND);
  tor_assert(circ->rend_data);
  log_info(LD_REND, "Sending an ESTABLISH_RENDEZVOUS cell");

  if (crypto_rand(circ->rend_data->rend_cookie, REND_COOKIE_LEN) < 0) {
    log_warn(LD_BUG, "Internal error: Couldn't produce random cookie.");
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_INTERNAL);
    return -1;
  }

  /* Set timestamp_dirty, because circuit_expire_building expects it,
   * and the rend cookie also means we've used the circ. */
  circ->base_.timestamp_dirty = time(NULL);

  /* We've attempted to use this circuit. Probe it if we fail */
  pathbias_count_use_attempt(circ);

  if (relay_send_command_from_edge(0, TO_CIRCUIT(circ),
                                   RELAY_COMMAND_ESTABLISH_RENDEZVOUS,
                                   circ->rend_data->rend_cookie,
                                   REND_COOKIE_LEN,
                                   circ->cpath->prev)<0) {
    /* circ is already marked for close */
    log_warn(LD_GENERAL, "Couldn't send ESTABLISH_RENDEZVOUS cell");
    return -1;
  }

  return 0;
}

/** Extend the introduction circuit <b>circ</b> to another valid
 * introduction point for the hidden service it is trying to connect
 * to, or mark it and launch a new circuit if we can't extend it.
 * Return 0 on success or possible success.  Return -1 and mark the
 * introduction circuit for close on permanent failure.
 *
 * On failure, the caller is responsible for marking the associated
 * rendezvous circuit for close. */
static int
rend_client_reextend_intro_circuit(origin_circuit_t *circ)
{
  extend_info_t *extend_info;
  int result;
  extend_info = rend_client_get_random_intro(circ->rend_data);
  if (!extend_info) {
    log_warn(LD_REND,
             "No usable introduction points left for %s. Closing.",
             safe_str_client(circ->rend_data->onion_address));
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_INTERNAL);
    return -1;
  }
  // XXX: should we not re-extend if hs_circ_has_timed_out?
  if (circ->remaining_relay_early_cells) {
    log_info(LD_REND,
             "Re-extending circ %u, this time to %s.",
             (unsigned)circ->base_.n_circ_id,
             safe_str_client(extend_info_describe(extend_info)));
    result = circuit_extend_to_new_exit(circ, extend_info);
  } else {
    log_info(LD_REND,
             "Closing intro circ %u (out of RELAY_EARLY cells).",
             (unsigned)circ->base_.n_circ_id);
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_FINISHED);
    /* connection_ap_handshake_attach_circuit will launch a new intro circ. */
    result = 0;
  }
  extend_info_free(extend_info);
  return result;
}

/** Return true iff we should send timestamps in our INTRODUCE1 cells */
static int
rend_client_should_send_timestamp(void)
{
  if (get_options()->Support022HiddenServices >= 0)
    return get_options()->Support022HiddenServices;

  return networkstatus_get_param(NULL, "Support022HiddenServices", 1, 0, 1);
}

/** Called when we're trying to connect an ap conn; sends an INTRODUCE1 cell
 * down introcirc if possible.
 */
int
rend_client_send_introduction(origin_circuit_t *introcirc,
                              origin_circuit_t *rendcirc)
{
  size_t payload_len;
  int r, v3_shift = 0;
  char payload[RELAY_PAYLOAD_SIZE];
  char tmp[RELAY_PAYLOAD_SIZE];
  rend_cache_entry_t *entry;
  crypt_path_t *cpath;
  off_t dh_offset;
  crypto_pk_t *intro_key = NULL;
  int status = 0;

  tor_assert(introcirc->base_.purpose == CIRCUIT_PURPOSE_C_INTRODUCING);
  tor_assert(rendcirc->base_.purpose == CIRCUIT_PURPOSE_C_REND_READY);
  tor_assert(introcirc->rend_data);
  tor_assert(rendcirc->rend_data);
  tor_assert(!rend_cmp_service_ids(introcirc->rend_data->onion_address,
                                   rendcirc->rend_data->onion_address));
#ifndef NON_ANONYMOUS_MODE_ENABLED
  tor_assert(!(introcirc->build_state->onehop_tunnel));
  tor_assert(!(rendcirc->build_state->onehop_tunnel));
#endif

  if (rend_cache_lookup_entry(introcirc->rend_data->onion_address, -1,
                              &entry) < 1) {
    log_info(LD_REND,
             "query %s didn't have valid rend desc in cache. "
             "Refetching descriptor.",
             safe_str_client(introcirc->rend_data->onion_address));
    rend_client_refetch_v2_renddesc(introcirc->rend_data);
    {
      connection_t *conn;

      while ((conn = connection_get_by_type_state_rendquery(CONN_TYPE_AP,
                       AP_CONN_STATE_CIRCUIT_WAIT,
                       introcirc->rend_data->onion_address))) {
        conn->state = AP_CONN_STATE_RENDDESC_WAIT;
      }
    }

    status = -1;
    goto cleanup;
  }

  /* first 20 bytes of payload are the hash of Bob's pk */
  intro_key = NULL;
  SMARTLIST_FOREACH(entry->parsed->intro_nodes, rend_intro_point_t *,
                    intro, {
    if (tor_memeq(introcirc->build_state->chosen_exit->identity_digest,
                intro->extend_info->identity_digest, DIGEST_LEN)) {
      intro_key = intro->intro_key;
      break;
    }
  });
  if (!intro_key) {
    log_info(LD_REND, "Could not find intro key for %s at %s; we "
             "have a v2 rend desc with %d intro points. "
             "Trying a different intro point...",
             safe_str_client(introcirc->rend_data->onion_address),
             safe_str_client(extend_info_describe(
                                   introcirc->build_state->chosen_exit)),
             smartlist_len(entry->parsed->intro_nodes));

    if (rend_client_reextend_intro_circuit(introcirc)) {
      status = -2;
      goto perm_err;
    } else {
      status = -1;
      goto cleanup;
    }
  }
  if (crypto_pk_get_digest(intro_key, payload)<0) {
    log_warn(LD_BUG, "Internal error: couldn't hash public key.");
    status = -2;
    goto perm_err;
  }

  /* Initialize the pending_final_cpath and start the DH handshake. */
  cpath = rendcirc->build_state->pending_final_cpath;
  if (!cpath) {
    cpath = rendcirc->build_state->pending_final_cpath =
      tor_malloc_zero(sizeof(crypt_path_t));
    cpath->magic = CRYPT_PATH_MAGIC;
    if (!(cpath->rend_dh_handshake_state = crypto_dh_new(DH_TYPE_REND))) {
      log_warn(LD_BUG, "Internal error: couldn't allocate DH.");
      status = -2;
      goto perm_err;
    }
    if (crypto_dh_generate_public(cpath->rend_dh_handshake_state)<0) {
      log_warn(LD_BUG, "Internal error: couldn't generate g^x.");
      status = -2;
      goto perm_err;
    }
  }

  /* If version is 3, write (optional) auth data and timestamp. */
  if (entry->parsed->protocols & (1<<3)) {
    tmp[0] = 3; /* version 3 of the cell format */
    tmp[1] = (uint8_t)introcirc->rend_data->auth_type; /* auth type, if any */
    v3_shift = 1;
    if (introcirc->rend_data->auth_type != REND_NO_AUTH) {
      set_uint16(tmp+2, htons(REND_DESC_COOKIE_LEN));
      memcpy(tmp+4, introcirc->rend_data->descriptor_cookie,
             REND_DESC_COOKIE_LEN);
      v3_shift += 2+REND_DESC_COOKIE_LEN;
    }
    if (rend_client_should_send_timestamp()) {
      uint32_t now = (uint32_t)time(NULL);
      now += 300;
      now -= now % 600;
      set_uint32(tmp+v3_shift+1, htonl(now));
    } else {
      set_uint32(tmp+v3_shift+1, 0);
    }
    v3_shift += 4;
  } /* if version 2 only write version number */
  else if (entry->parsed->protocols & (1<<2)) {
    tmp[0] = 2; /* version 2 of the cell format */
  }

  /* write the remaining items into tmp */
  if (entry->parsed->protocols & (1<<3) || entry->parsed->protocols & (1<<2)) {
    /* version 2 format */
    extend_info_t *extend_info = rendcirc->build_state->chosen_exit;
    int klen;
    /* nul pads */
    set_uint32(tmp+v3_shift+1, tor_addr_to_ipv4n(&extend_info->addr));
    set_uint16(tmp+v3_shift+5, htons(extend_info->port));
    memcpy(tmp+v3_shift+7, extend_info->identity_digest, DIGEST_LEN);
    klen = crypto_pk_asn1_encode(extend_info->onion_key,
                                 tmp+v3_shift+7+DIGEST_LEN+2,
                                 sizeof(tmp)-(v3_shift+7+DIGEST_LEN+2));
    set_uint16(tmp+v3_shift+7+DIGEST_LEN, htons(klen));
    memcpy(tmp+v3_shift+7+DIGEST_LEN+2+klen, rendcirc->rend_data->rend_cookie,
           REND_COOKIE_LEN);
    dh_offset = v3_shift+7+DIGEST_LEN+2+klen+REND_COOKIE_LEN;
  } else {
    /* Version 0. */
    strncpy(tmp, rendcirc->build_state->chosen_exit->nickname,
            (MAX_NICKNAME_LEN+1)); /* nul pads */
    memcpy(tmp+MAX_NICKNAME_LEN+1, rendcirc->rend_data->rend_cookie,
           REND_COOKIE_LEN);
    dh_offset = MAX_NICKNAME_LEN+1+REND_COOKIE_LEN;
  }

  if (crypto_dh_get_public(cpath->rend_dh_handshake_state, tmp+dh_offset,
                           DH_KEY_LEN)<0) {
    log_warn(LD_BUG, "Internal error: couldn't extract g^x.");
    status = -2;
    goto perm_err;
  }

  note_crypto_pk_op(REND_CLIENT);
  /*XXX maybe give crypto_pk_public_hybrid_encrypt a max_len arg,
   * to avoid buffer overflows? */
  r = crypto_pk_public_hybrid_encrypt(intro_key, payload+DIGEST_LEN,
                                      sizeof(payload)-DIGEST_LEN,
                                      tmp,
                                      (int)(dh_offset+DH_KEY_LEN),
                                      PK_PKCS1_OAEP_PADDING, 0);
  if (r<0) {
    log_warn(LD_BUG,"Internal error: hybrid pk encrypt failed.");
    status = -2;
    goto perm_err;
  }

  payload_len = DIGEST_LEN + r;
  tor_assert(payload_len <= RELAY_PAYLOAD_SIZE); /* we overran something */

  /* Copy the rendezvous cookie from rendcirc to introcirc, so that
   * when introcirc gets an ack, we can change the state of the right
   * rendezvous circuit. */
  memcpy(introcirc->rend_data->rend_cookie, rendcirc->rend_data->rend_cookie,
         REND_COOKIE_LEN);

  log_info(LD_REND, "Sending an INTRODUCE1 cell");
  if (relay_send_command_from_edge(0, TO_CIRCUIT(introcirc),
                                   RELAY_COMMAND_INTRODUCE1,
                                   payload, payload_len,
                                   introcirc->cpath->prev)<0) {
    /* introcirc is already marked for close. leave rendcirc alone. */
    log_warn(LD_BUG, "Couldn't send INTRODUCE1 cell");
    status = -2;
    goto cleanup;
  }

  /* Now, we wait for an ACK or NAK on this circuit. */
  circuit_change_purpose(TO_CIRCUIT(introcirc),
                         CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT);
  /* Set timestamp_dirty, because circuit_expire_building expects it
   * to specify when a circuit entered the _C_INTRODUCE_ACK_WAIT
   * state. */
  introcirc->base_.timestamp_dirty = time(NULL);

  pathbias_count_use_attempt(introcirc);

  goto cleanup;

 perm_err:
  if (!introcirc->base_.marked_for_close)
    circuit_mark_for_close(TO_CIRCUIT(introcirc), END_CIRC_REASON_INTERNAL);
  circuit_mark_for_close(TO_CIRCUIT(rendcirc), END_CIRC_REASON_INTERNAL);
 cleanup:
  memwipe(payload, 0, sizeof(payload));
  memwipe(tmp, 0, sizeof(tmp));

  return status;
}

/** Called when a rendezvous circuit is open; sends a establish
 * rendezvous circuit as appropriate. */
void
rend_client_rendcirc_has_opened(origin_circuit_t *circ)
{
  tor_assert(circ->base_.purpose == CIRCUIT_PURPOSE_C_ESTABLISH_REND);

  log_info(LD_REND,"rendcirc is open");

  /* generate a rendezvous cookie, store it in circ */
  if (rend_client_send_establish_rendezvous(circ) < 0) {
    return;
  }
}

/**
 * Called to close other intro circuits we launched in parallel
 * due to timeout.
 */
static void
rend_client_close_other_intros(const char *onion_address)
{
  circuit_t *c;
  /* abort parallel intro circs, if any */
  TOR_LIST_FOREACH(c, circuit_get_global_list(), head) {
    if ((c->purpose == CIRCUIT_PURPOSE_C_INTRODUCING ||
        c->purpose == CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT) &&
        !c->marked_for_close && CIRCUIT_IS_ORIGIN(c)) {
      origin_circuit_t *oc = TO_ORIGIN_CIRCUIT(c);
      if (oc->rend_data &&
          !rend_cmp_service_ids(onion_address,
                                oc->rend_data->onion_address)) {
        log_info(LD_REND|LD_CIRC, "Closing introduction circuit %d that we "
                 "built in parallel (Purpose %d).", oc->global_identifier,
                 c->purpose);
        circuit_mark_for_close(c, END_CIRC_REASON_TIMEOUT);
      }
    }
  }
}

/** Called when get an ACK or a NAK for a REND_INTRODUCE1 cell.
 */
int
rend_client_introduction_acked(origin_circuit_t *circ,
                               const uint8_t *request, size_t request_len)
{
  origin_circuit_t *rendcirc;
  (void) request; // XXXX Use this.

  if (circ->base_.purpose != CIRCUIT_PURPOSE_C_INTRODUCE_ACK_WAIT) {
    log_warn(LD_PROTOCOL,
             "Received REND_INTRODUCE_ACK on unexpected circuit %u.",
             (unsigned)circ->base_.n_circ_id);
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
    return -1;
  }

  tor_assert(circ->build_state->chosen_exit);
#ifndef NON_ANONYMOUS_MODE_ENABLED
  tor_assert(!(circ->build_state->onehop_tunnel));
#endif
  tor_assert(circ->rend_data);

  /* For path bias: This circuit was used successfully. Valid
   * nacks and acks count. */
  pathbias_mark_use_success(circ);

  if (request_len == 0) {
    /* It's an ACK; the introduction point relayed our introduction request. */
    /* Locate the rend circ which is waiting to hear about this ack,
     * and tell it.
     */
    log_info(LD_REND,"Received ack. Telling rend circ...");
    rendcirc = circuit_get_ready_rend_circ_by_rend_data(circ->rend_data);
    if (rendcirc) { /* remember the ack */
#ifndef NON_ANONYMOUS_MODE_ENABLED
      tor_assert(!(rendcirc->build_state->onehop_tunnel));
#endif
      circuit_change_purpose(TO_CIRCUIT(rendcirc),
                             CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED);
      /* Set timestamp_dirty, because circuit_expire_building expects
       * it to specify when a circuit entered the
       * _C_REND_READY_INTRO_ACKED state. */
      rendcirc->base_.timestamp_dirty = time(NULL);
    } else {
      log_info(LD_REND,"...Found no rend circ. Dropping on the floor.");
    }
    /* close the circuit: we won't need it anymore. */
    circuit_change_purpose(TO_CIRCUIT(circ),
                           CIRCUIT_PURPOSE_C_INTRODUCE_ACKED);
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_FINISHED);

    /* close any other intros launched in parallel */
    rend_client_close_other_intros(circ->rend_data->onion_address);
  } else {
    /* It's a NAK; the introduction point didn't relay our request. */
    circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_C_INTRODUCING);
    /* Remove this intro point from the set of viable introduction
     * points. If any remain, extend to a new one and try again.
     * If none remain, refetch the service descriptor.
     */
    log_info(LD_REND, "Got nack for %s from %s...",
        safe_str_client(circ->rend_data->onion_address),
        safe_str_client(extend_info_describe(circ->build_state->chosen_exit)));
    if (rend_client_report_intro_point_failure(circ->build_state->chosen_exit,
                                             circ->rend_data,
                                             INTRO_POINT_FAILURE_GENERIC)>0) {
      /* There are introduction points left. Re-extend the circuit to
       * another intro point and try again. */
      int result = rend_client_reextend_intro_circuit(circ);
      /* XXXX If that call failed, should we close the rend circuit,
       * too? */
      return result;
    }
  }
  return 0;
}

/** The period for which a hidden service directory cannot be queried for
 * the same descriptor ID again. */
#define REND_HID_SERV_DIR_REQUERY_PERIOD (15 * 60)

/** Contains the last request times to hidden service directories for
 * certain queries; each key is a string consisting of the
 * concatenation of a base32-encoded HS directory identity digest, a
 * base32-encoded HS descriptor ID, and a hidden service address
 * (without the ".onion" part); each value is a pointer to a time_t
 * holding the time of the last request for that descriptor ID to that
 * HS directory. */
static strmap_t *last_hid_serv_requests_ = NULL;

/** Returns last_hid_serv_requests_, initializing it to a new strmap if
 * necessary. */
static strmap_t *
get_last_hid_serv_requests(void)
{
  if (!last_hid_serv_requests_)
    last_hid_serv_requests_ = strmap_new();
  return last_hid_serv_requests_;
}

#define LAST_HID_SERV_REQUEST_KEY_LEN (REND_DESC_ID_V2_LEN_BASE32 + \
                                       REND_DESC_ID_V2_LEN_BASE32 + \
                                       REND_SERVICE_ID_LEN_BASE32)

/** Look up the last request time to hidden service directory <b>hs_dir</b>
 * for descriptor ID <b>desc_id_base32</b> for the service specified in
 * <b>rend_query</b>. If <b>set</b> is non-zero,
 * assign the current time <b>now</b> and return that. Otherwise, return
 * the most recent request time, or 0 if no such request has been sent
 * before. */
static time_t
lookup_last_hid_serv_request(routerstatus_t *hs_dir,
                             const char *desc_id_base32,
                             const rend_data_t *rend_query,
                             time_t now, int set)
{
  char hsdir_id_base32[REND_DESC_ID_V2_LEN_BASE32 + 1];
  char hsdir_desc_comb_id[LAST_HID_SERV_REQUEST_KEY_LEN + 1];
  time_t *last_request_ptr;
  strmap_t *last_hid_serv_requests = get_last_hid_serv_requests();
  base32_encode(hsdir_id_base32, sizeof(hsdir_id_base32),
                hs_dir->identity_digest, DIGEST_LEN);
  tor_snprintf(hsdir_desc_comb_id, sizeof(hsdir_desc_comb_id), "%s%s%s",
               hsdir_id_base32,
               desc_id_base32,
               rend_query->onion_address);
  /* XXX023 tor_assert(strlen(hsdir_desc_comb_id) ==
                       LAST_HID_SERV_REQUEST_KEY_LEN); */
  if (set) {
    time_t *oldptr;
    last_request_ptr = tor_malloc_zero(sizeof(time_t));
    *last_request_ptr = now;
    oldptr = strmap_set(last_hid_serv_requests, hsdir_desc_comb_id,
                        last_request_ptr);
    tor_free(oldptr);
  } else
    last_request_ptr = strmap_get_lc(last_hid_serv_requests,
                                     hsdir_desc_comb_id);
  return (last_request_ptr) ? *last_request_ptr : 0;
}

/** Clean the history of request times to hidden service directories, so that
 * it does not contain requests older than REND_HID_SERV_DIR_REQUERY_PERIOD
 * seconds any more. */
static void
directory_clean_last_hid_serv_requests(time_t now)
{
  strmap_iter_t *iter;
  time_t cutoff = now - REND_HID_SERV_DIR_REQUERY_PERIOD;
  strmap_t *last_hid_serv_requests = get_last_hid_serv_requests();
  for (iter = strmap_iter_init(last_hid_serv_requests);
       !strmap_iter_done(iter); ) {
    const char *key;
    void *val;
    time_t *ent;
    strmap_iter_get(iter, &key, &val);
    ent = (time_t *) val;
    if (*ent < cutoff) {
      iter = strmap_iter_next_rmv(last_hid_serv_requests, iter);
      tor_free(ent);
    } else {
      iter = strmap_iter_next(last_hid_serv_requests, iter);
    }
  }
}

/** Remove all requests related to the hidden service named
 * <b>onion_address</b> from the history of times of requests to
 * hidden service directories. */
static void
purge_hid_serv_from_last_hid_serv_requests(const char *onion_address)
{
  strmap_iter_t *iter;
  strmap_t *last_hid_serv_requests = get_last_hid_serv_requests();
  /* XXX023 tor_assert(strlen(onion_address) == REND_SERVICE_ID_LEN_BASE32); */
  for (iter = strmap_iter_init(last_hid_serv_requests);
       !strmap_iter_done(iter); ) {
    const char *key;
    void *val;
    strmap_iter_get(iter, &key, &val);
    /* XXX023 tor_assert(strlen(key) == LAST_HID_SERV_REQUEST_KEY_LEN); */
    if (tor_memeq(key + LAST_HID_SERV_REQUEST_KEY_LEN -
                  REND_SERVICE_ID_LEN_BASE32,
                  onion_address,
                  REND_SERVICE_ID_LEN_BASE32)) {
      iter = strmap_iter_next_rmv(last_hid_serv_requests, iter);
      tor_free(val);
    } else {
      iter = strmap_iter_next(last_hid_serv_requests, iter);
    }
  }
}

/** Purge the history of request times to hidden service directories,
 * so that future lookups of an HS descriptor will not fail because we
 * accessed all of the HSDir relays responsible for the descriptor
 * recently. */
void
rend_client_purge_last_hid_serv_requests(void)
{
  /* Don't create the table if it doesn't exist yet (and it may very
   * well not exist if the user hasn't accessed any HSes)... */
  strmap_t *old_last_hid_serv_requests = last_hid_serv_requests_;
  /* ... and let get_last_hid_serv_requests re-create it for us if
   * necessary. */
  last_hid_serv_requests_ = NULL;

  if (old_last_hid_serv_requests != NULL) {
    log_info(LD_REND, "Purging client last-HS-desc-request-time table");
    strmap_free(old_last_hid_serv_requests, tor_free_);
  }
}

/** Determine the responsible hidden service directories for <b>desc_id</b>
 * and fetch the descriptor with that ID from one of them. Only
 * send a request to a hidden service directory that we have not yet tried
 * during this attempt to connect to this hidden service; on success, return 1,
 * in the case that no hidden service directory is left to ask for the
 * descriptor, return 0, and in case of a failure -1.  */
static int
directory_get_from_hs_dir(const char *desc_id, const rend_data_t *rend_query)
{
  smartlist_t *responsible_dirs = smartlist_new();
  smartlist_t *usable_responsible_dirs = smartlist_new();
  const or_options_t *options = get_options();
  routerstatus_t *hs_dir;
  char desc_id_base32[REND_DESC_ID_V2_LEN_BASE32 + 1];
  time_t now = time(NULL);
  char descriptor_cookie_base64[3*REND_DESC_COOKIE_LEN_BASE64];
  const int tor2web_mode = options->Tor2webMode;
  int excluded_some;
  tor_assert(desc_id);
  tor_assert(rend_query);
  /* Determine responsible dirs. Even if we can't get all we want,
   * work with the ones we have. If it's empty, we'll notice below. */
  hid_serv_get_responsible_directories(responsible_dirs, desc_id);

  base32_encode(desc_id_base32, sizeof(desc_id_base32),
                desc_id, DIGEST_LEN);

  /* Only select those hidden service directories to which we did not send
   * a request recently and for which we have a router descriptor here. */

  /* Clean request history first. */
  directory_clean_last_hid_serv_requests(now);

  SMARTLIST_FOREACH(responsible_dirs, routerstatus_t *, dir, {
      time_t last = lookup_last_hid_serv_request(
                            dir, desc_id_base32, rend_query, 0, 0);
      const node_t *node = node_get_by_id(dir->identity_digest);
      if (last + REND_HID_SERV_DIR_REQUERY_PERIOD >= now ||
          !node || !node_has_descriptor(node)) {
        SMARTLIST_DEL_CURRENT(responsible_dirs, dir);
        continue;
      }
      if (! routerset_contains_node(options->ExcludeNodes, node)) {
        smartlist_add(usable_responsible_dirs, dir);
      }
  });

  excluded_some =
    smartlist_len(usable_responsible_dirs) < smartlist_len(responsible_dirs);

  hs_dir = smartlist_choose(usable_responsible_dirs);
  if (! hs_dir && ! options->StrictNodes)
    hs_dir = smartlist_choose(responsible_dirs);

  smartlist_free(responsible_dirs);
  smartlist_free(usable_responsible_dirs);
  if (!hs_dir) {
    log_info(LD_REND, "Could not pick one of the responsible hidden "
                      "service directories, because we requested them all "
                      "recently without success.");
    if (options->StrictNodes && excluded_some) {
      log_warn(LD_REND, "Could not pick a hidden service directory for the "
               "requested hidden service: they are all either down or "
               "excluded, and StrictNodes is set.");
    }
    return 0;
  }

  /* Remember that we are requesting a descriptor from this hidden service
   * directory now. */
  lookup_last_hid_serv_request(hs_dir, desc_id_base32, rend_query, now, 1);

  /* Encode descriptor cookie for logging purposes. */
  if (rend_query->auth_type != REND_NO_AUTH) {
    if (base64_encode(descriptor_cookie_base64,
                      sizeof(descriptor_cookie_base64),
                      rend_query->descriptor_cookie, REND_DESC_COOKIE_LEN)<0) {
      log_warn(LD_BUG, "Could not base64-encode descriptor cookie.");
      return 0;
    }
    /* Remove == signs and newline. */
    descriptor_cookie_base64[strlen(descriptor_cookie_base64)-3] = '\0';
  } else {
    strlcpy(descriptor_cookie_base64, "(none)",
            sizeof(descriptor_cookie_base64));
  }

  /* Send fetch request. (Pass query and possibly descriptor cookie so that
   * they can be written to the directory connection and be referred to when
   * the response arrives. */
  directory_initiate_command_routerstatus_rend(hs_dir,
                                          DIR_PURPOSE_FETCH_RENDDESC_V2,
                                          ROUTER_PURPOSE_GENERAL,
                                   tor2web_mode?DIRIND_ONEHOP:DIRIND_ANONYMOUS,
                                          desc_id_base32,
                                          NULL, 0, 0,
                                          rend_query);
  log_info(LD_REND, "Sending fetch request for v2 descriptor for "
                    "service '%s' with descriptor ID '%s', auth type %d, "
                    "and descriptor cookie '%s' to hidden service "
                    "directory %s",
           rend_query->onion_address, desc_id_base32,
           rend_query->auth_type,
           (rend_query->auth_type == REND_NO_AUTH ? "[none]" :
            escaped_safe_str_client(descriptor_cookie_base64)),
           routerstatus_describe(hs_dir));
  control_event_hs_descriptor_requested(rend_query,
                                        hs_dir->identity_digest,
                                        desc_id_base32);
  return 1;
}

/** Unless we already have a descriptor for <b>rend_query</b> with at least
 * one (possibly) working introduction point in it, start a connection to a
 * hidden service directory to fetch a v2 rendezvous service descriptor. */
void
rend_client_refetch_v2_renddesc(const rend_data_t *rend_query)
{
  char descriptor_id[DIGEST_LEN];
  int replicas_left_to_try[REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS];
  int i, tries_left;
  rend_cache_entry_t *e = NULL;
  tor_assert(rend_query);
  /* Are we configured to fetch descriptors? */
  if (!get_options()->FetchHidServDescriptors) {
    log_warn(LD_REND, "We received an onion address for a v2 rendezvous "
        "service descriptor, but are not fetching service descriptors.");
    return;
  }
  /* Before fetching, check if we already have a usable descriptor here. */
  if (rend_cache_lookup_entry(rend_query->onion_address, -1, &e) > 0 &&
      rend_client_any_intro_points_usable(e)) {
    log_info(LD_REND, "We would fetch a v2 rendezvous descriptor, but we "
                      "already have a usable descriptor here. Not fetching.");
    return;
  }
  log_debug(LD_REND, "Fetching v2 rendezvous descriptor for service %s",
            safe_str_client(rend_query->onion_address));
  /* Randomly iterate over the replicas until a descriptor can be fetched
   * from one of the consecutive nodes, or no options are left. */
  tries_left = REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS;
  for (i = 0; i < REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS; i++)
    replicas_left_to_try[i] = i;
  while (tries_left > 0) {
    int rand = crypto_rand_int(tries_left);
    int chosen_replica = replicas_left_to_try[rand];
    replicas_left_to_try[rand] = replicas_left_to_try[--tries_left];

    if (rend_compute_v2_desc_id(descriptor_id, rend_query->onion_address,
                                rend_query->auth_type == REND_STEALTH_AUTH ?
                                    rend_query->descriptor_cookie : NULL,
                                time(NULL), chosen_replica) < 0) {
      log_warn(LD_REND, "Internal error: Computing v2 rendezvous "
                        "descriptor ID did not succeed.");
      /*
       * Hmm, can this write anything to descriptor_id and still fail?
       * Let's clear it just to be safe.
       *
       * From here on, any returns should goto done which clears
       * descriptor_id so we don't leave key-derived material on the stack.
       */
      goto done;
    }
    if (directory_get_from_hs_dir(descriptor_id, rend_query) != 0)
      goto done; /* either success or failure, but we're done */
  }
  /* If we come here, there are no hidden service directories left. */
  log_info(LD_REND, "Could not pick one of the responsible hidden "
                    "service directories to fetch descriptors, because "
                    "we already tried them all unsuccessfully.");
  /* Close pending connections. */
  rend_client_desc_trynow(rend_query->onion_address);

 done:
  memwipe(descriptor_id, 0, sizeof(descriptor_id));

  return;
}

/** Cancel all rendezvous descriptor fetches currently in progress.
 */
void
rend_client_cancel_descriptor_fetches(void)
{
  smartlist_t *connection_array = get_connection_array();

  SMARTLIST_FOREACH_BEGIN(connection_array, connection_t *, conn) {
    if (conn->type == CONN_TYPE_DIR &&
        conn->purpose == DIR_PURPOSE_FETCH_RENDDESC_V2) {
      /* It's a rendezvous descriptor fetch in progress -- cancel it
       * by marking the connection for close.
       *
       * Even if this connection has already reached EOF, this is
       * enough to make sure that if the descriptor hasn't been
       * processed yet, it won't be.  See the end of
       * connection_handle_read; connection_reached_eof (indirectly)
       * processes whatever response the connection received. */

      const rend_data_t *rd = (TO_DIR_CONN(conn))->rend_data;
      if (!rd) {
        log_warn(LD_BUG | LD_REND,
                 "Marking for close dir conn fetching rendezvous "
                 "descriptor for unknown service!");
      } else {
        log_debug(LD_REND, "Marking for close dir conn fetching "
                  "rendezvous descriptor for service %s",
                  safe_str(rd->onion_address));
      }
      connection_mark_for_close(conn);
    }
  } SMARTLIST_FOREACH_END(conn);
}

/** Mark <b>failed_intro</b> as a failed introduction point for the
 * hidden service specified by <b>rend_query</b>. If the HS now has no
 * usable intro points, or we do not have an HS descriptor for it,
 * then launch a new renddesc fetch.
 *
 * If <b>failure_type</b> is INTRO_POINT_FAILURE_GENERIC, remove the
 * intro point from (our parsed copy of) the HS descriptor.
 *
 * If <b>failure_type</b> is INTRO_POINT_FAILURE_TIMEOUT, mark the
 * intro point as 'timed out'; it will not be retried until the
 * current hidden service connection attempt has ended or it has
 * appeared in a newly fetched rendezvous descriptor.
 *
 * If <b>failure_type</b> is INTRO_POINT_FAILURE_UNREACHABLE,
 * increment the intro point's reachability-failure count; if it has
 * now failed MAX_INTRO_POINT_REACHABILITY_FAILURES or more times,
 * remove the intro point from (our parsed copy of) the HS descriptor.
 *
 * Return -1 if error, 0 if no usable intro points remain or service
 * unrecognized, 1 if recognized and some intro points remain.
 */
int
rend_client_report_intro_point_failure(extend_info_t *failed_intro,
                                       const rend_data_t *rend_query,
                                       unsigned int failure_type)
{
  int i, r;
  rend_cache_entry_t *ent;
  connection_t *conn;

  r = rend_cache_lookup_entry(rend_query->onion_address, -1, &ent);
  if (r<0) {
    log_warn(LD_BUG, "Malformed service ID %s.",
             escaped_safe_str_client(rend_query->onion_address));
    return -1;
  }
  if (r==0) {
    log_info(LD_REND, "Unknown service %s. Re-fetching descriptor.",
             escaped_safe_str_client(rend_query->onion_address));
    rend_client_refetch_v2_renddesc(rend_query);
    return 0;
  }

  for (i = 0; i < smartlist_len(ent->parsed->intro_nodes); i++) {
    rend_intro_point_t *intro = smartlist_get(ent->parsed->intro_nodes, i);
    if (tor_memeq(failed_intro->identity_digest,
                intro->extend_info->identity_digest, DIGEST_LEN)) {
      switch (failure_type) {
      default:
        log_warn(LD_BUG, "Unknown failure type %u. Removing intro point.",
                 failure_type);
        tor_fragile_assert();
        /* fall through */
      case INTRO_POINT_FAILURE_GENERIC:
        rend_intro_point_free(intro);
        smartlist_del(ent->parsed->intro_nodes, i);
        break;
      case INTRO_POINT_FAILURE_TIMEOUT:
        intro->timed_out = 1;
        break;
      case INTRO_POINT_FAILURE_UNREACHABLE:
        ++(intro->unreachable_count);
        {
          int zap_intro_point =
            intro->unreachable_count >= MAX_INTRO_POINT_REACHABILITY_FAILURES;
          log_info(LD_REND, "Failed to reach this intro point %u times.%s",
                   intro->unreachable_count,
                   zap_intro_point ? " Removing from descriptor.": "");
          if (zap_intro_point) {
            rend_intro_point_free(intro);
            smartlist_del(ent->parsed->intro_nodes, i);
          }
        }
        break;
      }
      break;
    }
  }

  if (! rend_client_any_intro_points_usable(ent)) {
    log_info(LD_REND,
             "No more intro points remain for %s. Re-fetching descriptor.",
             escaped_safe_str_client(rend_query->onion_address));
    rend_client_refetch_v2_renddesc(rend_query);

    /* move all pending streams back to renddesc_wait */
    while ((conn = connection_get_by_type_state_rendquery(CONN_TYPE_AP,
                                   AP_CONN_STATE_CIRCUIT_WAIT,
                                   rend_query->onion_address))) {
      conn->state = AP_CONN_STATE_RENDDESC_WAIT;
    }

    return 0;
  }
  log_info(LD_REND,"%d options left for %s.",
           smartlist_len(ent->parsed->intro_nodes),
           escaped_safe_str_client(rend_query->onion_address));
  return 1;
}

/** Called when we receive a RENDEZVOUS_ESTABLISHED cell; changes the state of
 * the circuit to C_REND_READY.
 */
int
rend_client_rendezvous_acked(origin_circuit_t *circ, const uint8_t *request,
                             size_t request_len)
{
  (void) request;
  (void) request_len;
  /* we just got an ack for our establish-rendezvous. switch purposes. */
  if (circ->base_.purpose != CIRCUIT_PURPOSE_C_ESTABLISH_REND) {
    log_warn(LD_PROTOCOL,"Got a rendezvous ack when we weren't expecting one. "
             "Closing circ.");
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
    return -1;
  }
  log_info(LD_REND,"Got rendezvous ack. This circuit is now ready for "
           "rendezvous.");
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_C_REND_READY);
  /* Set timestamp_dirty, because circuit_expire_building expects it
   * to specify when a circuit entered the _C_REND_READY state. */
  circ->base_.timestamp_dirty = time(NULL);

  /* From a path bias point of view, this circuit is now successfully used.
   * Waiting any longer opens us up to attacks from Bob. He could induce
   * Alice to attempt to connect to his hidden service and never reply
   * to her rend requests */
  pathbias_mark_use_success(circ);

  /* XXXX This is a pretty brute-force approach. It'd be better to
   * attach only the connections that are waiting on this circuit, rather
   * than trying to attach them all. See comments bug 743. */
  /* If we already have the introduction circuit built, make sure we send
   * the INTRODUCE cell _now_ */
  connection_ap_attach_pending();
  return 0;
}

/** Bob sent us a rendezvous cell; join the circuits. */
int
rend_client_receive_rendezvous(origin_circuit_t *circ, const uint8_t *request,
                               size_t request_len)
{
  crypt_path_t *hop;
  char keys[DIGEST_LEN+CPATH_KEY_MATERIAL_LEN];

  if ((circ->base_.purpose != CIRCUIT_PURPOSE_C_REND_READY &&
       circ->base_.purpose != CIRCUIT_PURPOSE_C_REND_READY_INTRO_ACKED)
      || !circ->build_state->pending_final_cpath) {
    log_warn(LD_PROTOCOL,"Got rendezvous2 cell from hidden service, but not "
             "expecting it. Closing.");
    circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
    return -1;
  }

  if (request_len != DH_KEY_LEN+DIGEST_LEN) {
    log_warn(LD_PROTOCOL,"Incorrect length (%d) on RENDEZVOUS2 cell.",
             (int)request_len);
    goto err;
  }

  log_info(LD_REND,"Got RENDEZVOUS2 cell from hidden service.");

  /* first DH_KEY_LEN bytes are g^y from bob. Finish the dh handshake...*/
  tor_assert(circ->build_state);
  tor_assert(circ->build_state->pending_final_cpath);
  hop = circ->build_state->pending_final_cpath;
  tor_assert(hop->rend_dh_handshake_state);
  if (crypto_dh_compute_secret(LOG_PROTOCOL_WARN,
                               hop->rend_dh_handshake_state, (char*)request,
                               DH_KEY_LEN,
                               keys, DIGEST_LEN+CPATH_KEY_MATERIAL_LEN)<0) {
    log_warn(LD_GENERAL, "Couldn't complete DH handshake.");
    goto err;
  }
  /* ... and set up cpath. */
  if (circuit_init_cpath_crypto(hop, keys+DIGEST_LEN, 0)<0)
    goto err;

  /* Check whether the digest is right... */
  if (tor_memneq(keys, request+DH_KEY_LEN, DIGEST_LEN)) {
    log_warn(LD_PROTOCOL, "Incorrect digest of key material.");
    goto err;
  }

  crypto_dh_free(hop->rend_dh_handshake_state);
  hop->rend_dh_handshake_state = NULL;

  /* All is well. Extend the circuit. */
  circuit_change_purpose(TO_CIRCUIT(circ), CIRCUIT_PURPOSE_C_REND_JOINED);
  hop->state = CPATH_STATE_OPEN;
  /* set the windows to default. these are the windows
   * that alice thinks bob has.
   */
  hop->package_window = circuit_initial_package_window();
  hop->deliver_window = CIRCWINDOW_START;

  /* Now that this circuit has finished connecting to its destination,
   * make sure circuit_get_open_circ_or_launch is willing to return it
   * so we can actually use it. */
  circ->hs_circ_has_timed_out = 0;

  onion_append_to_cpath(&circ->cpath, hop);
  circ->build_state->pending_final_cpath = NULL; /* prevent double-free */

  circuit_try_attaching_streams(circ);

  memwipe(keys, 0, sizeof(keys));
  return 0;
 err:
  memwipe(keys, 0, sizeof(keys));
  circuit_mark_for_close(TO_CIRCUIT(circ), END_CIRC_REASON_TORPROTOCOL);
  return -1;
}

/** Find all the apconns in state AP_CONN_STATE_RENDDESC_WAIT that are
 * waiting on <b>query</b>. If there's a working cache entry here with at
 * least one intro point, move them to the next state. */
void
rend_client_desc_trynow(const char *query)
{
  entry_connection_t *conn;
  rend_cache_entry_t *entry;
  const rend_data_t *rend_data;
  time_t now = time(NULL);

  smartlist_t *conns = get_connection_array();
  SMARTLIST_FOREACH_BEGIN(conns, connection_t *, base_conn) {
    if (base_conn->type != CONN_TYPE_AP ||
        base_conn->state != AP_CONN_STATE_RENDDESC_WAIT ||
        base_conn->marked_for_close)
      continue;
    conn = TO_ENTRY_CONN(base_conn);
    rend_data = ENTRY_TO_EDGE_CONN(conn)->rend_data;
    if (!rend_data)
      continue;
    if (rend_cmp_service_ids(query, rend_data->onion_address))
      continue;
    assert_connection_ok(base_conn, now);
    if (rend_cache_lookup_entry(rend_data->onion_address, -1,
                                &entry) == 1 &&
        rend_client_any_intro_points_usable(entry)) {
      /* either this fetch worked, or it failed but there was a
       * valid entry from before which we should reuse */
      log_info(LD_REND,"Rend desc is usable. Launching circuits.");
      base_conn->state = AP_CONN_STATE_CIRCUIT_WAIT;

      /* restart their timeout values, so they get a fair shake at
       * connecting to the hidden service. */
      base_conn->timestamp_created = now;
      base_conn->timestamp_lastread = now;
      base_conn->timestamp_lastwritten = now;

      if (connection_ap_handshake_attach_circuit(conn) < 0) {
        /* it will never work */
        log_warn(LD_REND,"Rendezvous attempt failed. Closing.");
        if (!base_conn->marked_for_close)
          connection_mark_unattached_ap(conn, END_STREAM_REASON_CANT_ATTACH);
      }
    } else { /* 404, or fetch didn't get that far */
      log_notice(LD_REND,"Closing stream for '%s.onion': hidden service is "
                 "unavailable (try again later).",
                 safe_str_client(query));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_RESOLVEFAILED);
      rend_client_note_connection_attempt_ended(query);
    }
  } SMARTLIST_FOREACH_END(base_conn);
}

/** Clear temporary state used only during an attempt to connect to
 * the hidden service named <b>onion_address</b>.  Called when a
 * connection attempt has ended; may be called occasionally at other
 * times, and should be reasonably harmless. */
void
rend_client_note_connection_attempt_ended(const char *onion_address)
{
  rend_cache_entry_t *cache_entry = NULL;
  rend_cache_lookup_entry(onion_address, -1, &cache_entry);

  log_info(LD_REND, "Connection attempt for %s has ended; "
           "cleaning up temporary state.",
           safe_str_client(onion_address));

  /* Clear the timed_out flag on all remaining intro points for this HS. */
  if (cache_entry != NULL) {
    SMARTLIST_FOREACH(cache_entry->parsed->intro_nodes,
                      rend_intro_point_t *, ip,
                      ip->timed_out = 0; );
  }

  /* Remove the HS's entries in last_hid_serv_requests. */
  purge_hid_serv_from_last_hid_serv_requests(onion_address);
}

/** Return a newly allocated extend_info_t* for a randomly chosen introduction
 * point for the named hidden service.  Return NULL if all introduction points
 * have been tried and failed.
 */
extend_info_t *
rend_client_get_random_intro(const rend_data_t *rend_query)
{
  extend_info_t *result;
  rend_cache_entry_t *entry;

  if (rend_cache_lookup_entry(rend_query->onion_address, -1, &entry) < 1) {
      log_warn(LD_REND,
               "Query '%s' didn't have valid rend desc in cache. Failing.",
               safe_str_client(rend_query->onion_address));
    return NULL;
  }

  /* See if we can get a node that complies with ExcludeNodes */
  if ((result = rend_client_get_random_intro_impl(entry, 1, 1)))
    return result;
  /* If not, and StrictNodes is not set, see if we can return any old node
   */
  if (!get_options()->StrictNodes)
    return rend_client_get_random_intro_impl(entry, 0, 1);
  return NULL;
}

/** As rend_client_get_random_intro, except assume that StrictNodes is set
 * iff <b>strict</b> is true. If <b>warnings</b> is false, don't complain
 * to the user when we're out of nodes, even if StrictNodes is true.
 */
static extend_info_t *
rend_client_get_random_intro_impl(const rend_cache_entry_t *entry,
                                  const int strict,
                                  const int warnings)
{
  int i;

  rend_intro_point_t *intro;
  const or_options_t *options = get_options();
  smartlist_t *usable_nodes;
  int n_excluded = 0;

  /* We'll keep a separate list of the usable nodes.  If this becomes empty,
   * no nodes are usable.  */
  usable_nodes = smartlist_new();
  smartlist_add_all(usable_nodes, entry->parsed->intro_nodes);

  /* Remove the intro points that have timed out during this HS
   * connection attempt from our list of usable nodes. */
  SMARTLIST_FOREACH(usable_nodes, rend_intro_point_t *, ip,
                    if (ip->timed_out) {
                      SMARTLIST_DEL_CURRENT(usable_nodes, ip);
                    });

 again:
  if (smartlist_len(usable_nodes) == 0) {
    if (n_excluded && get_options()->StrictNodes && warnings) {
      /* We only want to warn if StrictNodes is really set. Otherwise
       * we're just about to retry anyways.
       */
      log_warn(LD_REND, "All introduction points for hidden service are "
               "at excluded relays, and StrictNodes is set. Skipping.");
    }
    smartlist_free(usable_nodes);
    return NULL;
  }

  i = crypto_rand_int(smartlist_len(usable_nodes));
  intro = smartlist_get(usable_nodes, i);
  /* Do we need to look up the router or is the extend info complete? */
  if (!intro->extend_info->onion_key) {
    const node_t *node;
    extend_info_t *new_extend_info;
    if (tor_digest_is_zero(intro->extend_info->identity_digest))
      node = node_get_by_hex_id(intro->extend_info->nickname);
    else
      node = node_get_by_id(intro->extend_info->identity_digest);
    if (!node) {
      log_info(LD_REND, "Unknown router with nickname '%s'; trying another.",
               intro->extend_info->nickname);
      smartlist_del(usable_nodes, i);
      goto again;
    }
    new_extend_info = extend_info_from_node(node, 0);
    if (!new_extend_info) {
      log_info(LD_REND, "We don't have a descriptor for the intro-point relay "
               "'%s'; trying another.",
               extend_info_describe(intro->extend_info));
      smartlist_del(usable_nodes, i);
      goto again;
    } else {
      extend_info_free(intro->extend_info);
      intro->extend_info = new_extend_info;
    }
    tor_assert(intro->extend_info != NULL);
  }
  /* Check if we should refuse to talk to this router. */
  if (strict &&
      routerset_contains_extendinfo(options->ExcludeNodes,
                                    intro->extend_info)) {
    n_excluded++;
    smartlist_del(usable_nodes, i);
    goto again;
  }

  smartlist_free(usable_nodes);
  return extend_info_dup(intro->extend_info);
}

/** Return true iff any introduction points still listed in <b>entry</b> are
 * usable. */
int
rend_client_any_intro_points_usable(const rend_cache_entry_t *entry)
{
  extend_info_t *extend_info =
    rend_client_get_random_intro_impl(entry, get_options()->StrictNodes, 0);

  int rv = (extend_info != NULL);

  extend_info_free(extend_info);
  return rv;
}

/** Client-side authorizations for hidden services; map of onion address to
 * rend_service_authorization_t*. */
static strmap_t *auth_hid_servs = NULL;

/** Look up the client-side authorization for the hidden service with
 * <b>onion_address</b>. Return NULL if no authorization is available for
 * that address. */
rend_service_authorization_t*
rend_client_lookup_service_authorization(const char *onion_address)
{
  tor_assert(onion_address);
  if (!auth_hid_servs) return NULL;
  return strmap_get(auth_hid_servs, onion_address);
}

/** Helper: Free storage held by rend_service_authorization_t. */
static void
rend_service_authorization_free(rend_service_authorization_t *auth)
{
  tor_free(auth);
}

/** Helper for strmap_free. */
static void
rend_service_authorization_strmap_item_free(void *service_auth)
{
  rend_service_authorization_free(service_auth);
}

/** Release all the storage held in auth_hid_servs.
 */
void
rend_service_authorization_free_all(void)
{
  if (!auth_hid_servs) {
    return;
  }
  strmap_free(auth_hid_servs, rend_service_authorization_strmap_item_free);
  auth_hid_servs = NULL;
}

/** Parse <b>config_line</b> as a client-side authorization for a hidden
 * service and add it to the local map of hidden service authorizations.
 * Return 0 for success and -1 for failure. */
int
rend_parse_service_authorization(const or_options_t *options,
                                 int validate_only)
{
  config_line_t *line;
  int res = -1;
  strmap_t *parsed = strmap_new();
  smartlist_t *sl = smartlist_new();
  rend_service_authorization_t *auth = NULL;
  char descriptor_cookie_tmp[REND_DESC_COOKIE_LEN+2];
  char descriptor_cookie_base64ext[REND_DESC_COOKIE_LEN_BASE64+2+1];

  for (line = options->HidServAuth; line; line = line->next) {
    char *onion_address, *descriptor_cookie;
    int auth_type_val = 0;
    auth = NULL;
    SMARTLIST_FOREACH(sl, char *, c, tor_free(c););
    smartlist_clear(sl);
    smartlist_split_string(sl, line->value, " ",
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 3);
    if (smartlist_len(sl) < 2) {
      log_warn(LD_CONFIG, "Configuration line does not consist of "
               "\"onion-address authorization-cookie [service-name]\": "
               "'%s'", line->value);
      goto err;
    }
    auth = tor_malloc_zero(sizeof(rend_service_authorization_t));
    /* Parse onion address. */
    onion_address = smartlist_get(sl, 0);
    if (strlen(onion_address) != REND_SERVICE_ADDRESS_LEN ||
        strcmpend(onion_address, ".onion")) {
      log_warn(LD_CONFIG, "Onion address has wrong format: '%s'",
               onion_address);
      goto err;
    }
    strlcpy(auth->onion_address, onion_address, REND_SERVICE_ID_LEN_BASE32+1);
    if (!rend_valid_service_id(auth->onion_address)) {
      log_warn(LD_CONFIG, "Onion address has wrong format: '%s'",
               onion_address);
      goto err;
    }
    /* Parse descriptor cookie. */
    descriptor_cookie = smartlist_get(sl, 1);
    if (strlen(descriptor_cookie) != REND_DESC_COOKIE_LEN_BASE64) {
      log_warn(LD_CONFIG, "Authorization cookie has wrong length: '%s'",
               descriptor_cookie);
      goto err;
    }
    /* Add trailing zero bytes (AA) to make base64-decoding happy. */
    tor_snprintf(descriptor_cookie_base64ext,
                 REND_DESC_COOKIE_LEN_BASE64+2+1,
                 "%sAA", descriptor_cookie);
    if (base64_decode(descriptor_cookie_tmp, sizeof(descriptor_cookie_tmp),
                      descriptor_cookie_base64ext,
                      strlen(descriptor_cookie_base64ext)) < 0) {
      log_warn(LD_CONFIG, "Decoding authorization cookie failed: '%s'",
               descriptor_cookie);
      goto err;
    }
    auth_type_val = (((uint8_t)descriptor_cookie_tmp[16]) >> 4) + 1;
    if (auth_type_val < 1 || auth_type_val > 2) {
      log_warn(LD_CONFIG, "Authorization cookie has unknown authorization "
                          "type encoded.");
      goto err;
    }
    auth->auth_type = auth_type_val == 1 ? REND_BASIC_AUTH : REND_STEALTH_AUTH;
    memcpy(auth->descriptor_cookie, descriptor_cookie_tmp,
           REND_DESC_COOKIE_LEN);
    if (strmap_get(parsed, auth->onion_address)) {
      log_warn(LD_CONFIG, "Duplicate authorization for the same hidden "
                          "service.");
      goto err;
    }
    strmap_set(parsed, auth->onion_address, auth);
    auth = NULL;
  }
  res = 0;
  goto done;
 err:
  res = -1;
 done:
  rend_service_authorization_free(auth);
  SMARTLIST_FOREACH(sl, char *, c, tor_free(c););
  smartlist_free(sl);
  if (!validate_only && res == 0) {
    rend_service_authorization_free_all();
    auth_hid_servs = parsed;
  } else {
    strmap_free(parsed, rend_service_authorization_strmap_item_free);
  }
  memwipe(descriptor_cookie_tmp, 0, sizeof(descriptor_cookie_tmp));
  memwipe(descriptor_cookie_base64ext, 0, sizeof(descriptor_cookie_base64ext));
  return res;
}

