/* Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rendcommon.c
 * \brief Rendezvous implementation: shared code between
 * introducers, services, clients, and rendezvous points.
 **/

#include "or.h"
#include "circuitbuild.h"
#include "config.h"
#include "control.h"
#include "rendclient.h"
#include "rendcommon.h"
#include "rendmid.h"
#include "rendservice.h"
#include "rephist.h"
#include "router.h"
#include "routerlist.h"
#include "routerparse.h"
#include "networkstatus.h"

/** Return 0 if one and two are the same service ids, else -1 or 1 */
int
rend_cmp_service_ids(const char *one, const char *two)
{
  return strcasecmp(one,two);
}

/** Free the storage held by the service descriptor <b>desc</b>.
 */
void
rend_service_descriptor_free(rend_service_descriptor_t *desc)
{
  if (!desc)
    return;
  if (desc->pk)
    crypto_pk_free(desc->pk);
  if (desc->intro_nodes) {
    SMARTLIST_FOREACH(desc->intro_nodes, rend_intro_point_t *, intro,
      rend_intro_point_free(intro););
    smartlist_free(desc->intro_nodes);
  }
  if (desc->successful_uploads) {
    SMARTLIST_FOREACH(desc->successful_uploads, char *, c, tor_free(c););
    smartlist_free(desc->successful_uploads);
  }
  tor_free(desc);
}

/** Length of the descriptor cookie that is used for versioned hidden
 * service descriptors. */
#define REND_DESC_COOKIE_LEN 16

/** Length of the replica number that is used to determine the secret ID
 * part of versioned hidden service descriptors. */
#define REND_REPLICA_LEN 1

/** Compute the descriptor ID for <b>service_id</b> of length
 * <b>REND_SERVICE_ID_LEN</b> and <b>secret_id_part</b> of length
 * <b>DIGEST_LEN</b>, and write it to <b>descriptor_id_out</b> of length
 * <b>DIGEST_LEN</b>. */
void
rend_get_descriptor_id_bytes(char *descriptor_id_out,
                             const char *service_id,
                             const char *secret_id_part)
{
  crypto_digest_t *digest = crypto_digest_new();
  crypto_digest_add_bytes(digest, service_id, REND_SERVICE_ID_LEN);
  crypto_digest_add_bytes(digest, secret_id_part, DIGEST_LEN);
  crypto_digest_get_digest(digest, descriptor_id_out, DIGEST_LEN);
  crypto_digest_free(digest);
}

/** Compute the secret ID part for time_period,
 * a <b>descriptor_cookie</b> of length
 * <b>REND_DESC_COOKIE_LEN</b> which may also be <b>NULL</b> if no
 * descriptor_cookie shall be used, and <b>replica</b>, and write it to
 * <b>secret_id_part</b> of length DIGEST_LEN. */
static void
get_secret_id_part_bytes(char *secret_id_part, uint32_t time_period,
                         const char *descriptor_cookie, uint8_t replica)
{
  crypto_digest_t *digest = crypto_digest_new();
  time_period = htonl(time_period);
  crypto_digest_add_bytes(digest, (char*)&time_period, sizeof(uint32_t));
  if (descriptor_cookie) {
    crypto_digest_add_bytes(digest, descriptor_cookie,
                            REND_DESC_COOKIE_LEN);
  }
  crypto_digest_add_bytes(digest, (const char *)&replica, REND_REPLICA_LEN);
  crypto_digest_get_digest(digest, secret_id_part, DIGEST_LEN);
  crypto_digest_free(digest);
}

/** Return the time period for time <b>now</b> plus a potentially
 * intended <b>deviation</b> of one or more periods, based on the first byte
 * of <b>service_id</b>. */
static uint32_t
get_time_period(time_t now, uint8_t deviation, const char *service_id)
{
  /* The time period is the number of REND_TIME_PERIOD_V2_DESC_VALIDITY
   * intervals that have passed since the epoch, offset slightly so that
   * each service's time periods start and end at a fraction of that
   * period based on their first byte. */
  return (uint32_t)
    (now + ((uint8_t) *service_id) * REND_TIME_PERIOD_V2_DESC_VALIDITY / 256)
    / REND_TIME_PERIOD_V2_DESC_VALIDITY + deviation;
}

/** Compute the time in seconds that a descriptor that is generated
 * <b>now</b> for <b>service_id</b> will be valid. */
static uint32_t
get_seconds_valid(time_t now, const char *service_id)
{
  uint32_t result = REND_TIME_PERIOD_V2_DESC_VALIDITY -
    ((uint32_t)
     (now + ((uint8_t) *service_id) * REND_TIME_PERIOD_V2_DESC_VALIDITY / 256)
     % REND_TIME_PERIOD_V2_DESC_VALIDITY);
  return result;
}

/** Compute the binary <b>desc_id_out</b> (DIGEST_LEN bytes long) for a given
 * base32-encoded <b>service_id</b> and optional unencoded
 * <b>descriptor_cookie</b> of length REND_DESC_COOKIE_LEN,
 * at time <b>now</b> for replica number
 * <b>replica</b>. <b>desc_id</b> needs to have <b>DIGEST_LEN</b> bytes
 * free. Return 0 for success, -1 otherwise. */
int
rend_compute_v2_desc_id(char *desc_id_out, const char *service_id,
                        const char *descriptor_cookie, time_t now,
                        uint8_t replica)
{
  char service_id_binary[REND_SERVICE_ID_LEN];
  char secret_id_part[DIGEST_LEN];
  uint32_t time_period;
  if (!service_id ||
      strlen(service_id) != REND_SERVICE_ID_LEN_BASE32) {
    log_warn(LD_REND, "Could not compute v2 descriptor ID: "
                      "Illegal service ID: %s",
             safe_str(service_id));
    return -1;
  }
  if (replica >= REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS) {
    log_warn(LD_REND, "Could not compute v2 descriptor ID: "
                      "Replica number out of range: %d", replica);
    return -1;
  }
  /* Convert service ID to binary. */
  if (base32_decode(service_id_binary, REND_SERVICE_ID_LEN,
                    service_id, REND_SERVICE_ID_LEN_BASE32) < 0) {
    log_warn(LD_REND, "Could not compute v2 descriptor ID: "
                      "Illegal characters in service ID: %s",
             safe_str_client(service_id));
    return -1;
  }
  /* Calculate current time-period. */
  time_period = get_time_period(now, 0, service_id_binary);
  /* Calculate secret-id-part = h(time-period | desc-cookie | replica). */
  get_secret_id_part_bytes(secret_id_part, time_period, descriptor_cookie,
                           replica);
  /* Calculate descriptor ID: H(permanent-id | secret-id-part) */
  rend_get_descriptor_id_bytes(desc_id_out, service_id_binary, secret_id_part);
  return 0;
}

/** Encode the introduction points in <b>desc</b> and write the result to a
 * newly allocated string pointed to by <b>encoded</b>. Return 0 for
 * success, -1 otherwise. */
static int
rend_encode_v2_intro_points(char **encoded, rend_service_descriptor_t *desc)
{
  size_t unenc_len;
  char *unenc = NULL;
  size_t unenc_written = 0;
  int i;
  int r = -1;
  /* Assemble unencrypted list of introduction points. */
  unenc_len = smartlist_len(desc->intro_nodes) * 1000; /* too long, but ok. */
  unenc = tor_malloc_zero(unenc_len);
  for (i = 0; i < smartlist_len(desc->intro_nodes); i++) {
    char id_base32[REND_INTRO_POINT_ID_LEN_BASE32 + 1];
    char *onion_key = NULL;
    size_t onion_key_len;
    crypto_pk_t *intro_key;
    char *service_key = NULL;
    char *address = NULL;
    size_t service_key_len;
    int res;
    rend_intro_point_t *intro = smartlist_get(desc->intro_nodes, i);
    /* Obtain extend info with introduction point details. */
    extend_info_t *info = intro->extend_info;
    /* Encode introduction point ID. */
    base32_encode(id_base32, sizeof(id_base32),
                  info->identity_digest, DIGEST_LEN);
    /* Encode onion key. */
    if (crypto_pk_write_public_key_to_string(info->onion_key, &onion_key,
                                             &onion_key_len) < 0) {
      log_warn(LD_REND, "Could not write onion key.");
      goto done;
    }
    /* Encode intro key. */
    intro_key = intro->intro_key;
    if (!intro_key ||
      crypto_pk_write_public_key_to_string(intro_key, &service_key,
                                           &service_key_len) < 0) {
      log_warn(LD_REND, "Could not write intro key.");
      tor_free(onion_key);
      goto done;
    }
    /* Assemble everything for this introduction point. */
    address = tor_dup_addr(&info->addr);
    res = tor_snprintf(unenc + unenc_written, unenc_len - unenc_written,
                         "introduction-point %s\n"
                         "ip-address %s\n"
                         "onion-port %d\n"
                         "onion-key\n%s"
                         "service-key\n%s",
                       id_base32,
                       address,
                       info->port,
                       onion_key,
                       service_key);
    tor_free(address);
    tor_free(onion_key);
    tor_free(service_key);
    if (res < 0) {
      log_warn(LD_REND, "Not enough space for writing introduction point "
                        "string.");
      goto done;
    }
    /* Update total number of written bytes for unencrypted intro points. */
    unenc_written += res;
  }
  /* Finalize unencrypted introduction points. */
  if (unenc_len < unenc_written + 2) {
    log_warn(LD_REND, "Not enough space for finalizing introduction point "
                      "string.");
    goto done;
  }
  unenc[unenc_written++] = '\n';
  unenc[unenc_written++] = 0;
  *encoded = unenc;
  r = 0;
 done:
  if (r<0)
    tor_free(unenc);
  return r;
}

/** Encrypt the encoded introduction points in <b>encoded</b> using
 * authorization type  'basic' with <b>client_cookies</b> and write the
 * result to a newly allocated string pointed to by <b>encrypted_out</b> of
 * length <b>encrypted_len_out</b>. Return 0 for success, -1 otherwise. */
static int
rend_encrypt_v2_intro_points_basic(char **encrypted_out,
                                   size_t *encrypted_len_out,
                                   const char *encoded,
                                   smartlist_t *client_cookies)
{
  int r = -1, i, pos, enclen, client_blocks;
  size_t len, client_entries_len;
  char *enc = NULL, iv[CIPHER_IV_LEN], *client_part = NULL,
       session_key[CIPHER_KEY_LEN];
  smartlist_t *encrypted_session_keys = NULL;
  crypto_digest_t *digest;
  crypto_cipher_t *cipher;
  tor_assert(encoded);
  tor_assert(client_cookies && smartlist_len(client_cookies) > 0);

  /* Generate session key. */
  crypto_rand(session_key, CIPHER_KEY_LEN);

  /* Determine length of encrypted introduction points including session
   * keys. */
  client_blocks = 1 + ((smartlist_len(client_cookies) - 1) /
                       REND_BASIC_AUTH_CLIENT_MULTIPLE);
  client_entries_len = client_blocks * REND_BASIC_AUTH_CLIENT_MULTIPLE *
                       REND_BASIC_AUTH_CLIENT_ENTRY_LEN;
  len = 2 + client_entries_len + CIPHER_IV_LEN + strlen(encoded);
  if (client_blocks >= 256) {
    log_warn(LD_REND, "Too many clients in introduction point string.");
    goto done;
  }
  enc = tor_malloc_zero(len);
  enc[0] = 0x01; /* type of authorization. */
  enc[1] = (uint8_t)client_blocks;

  /* Encrypt with random session key. */
  enclen = crypto_cipher_encrypt_with_iv(session_key,
      enc + 2 + client_entries_len,
      CIPHER_IV_LEN + strlen(encoded), encoded, strlen(encoded));

  if (enclen < 0) {
    log_warn(LD_REND, "Could not encrypt introduction point string.");
    goto done;
  }
  memcpy(iv, enc + 2 + client_entries_len, CIPHER_IV_LEN);

  /* Encrypt session key for cookies, determine client IDs, and put both
   * in a smartlist. */
  encrypted_session_keys = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(client_cookies, const char *, cookie) {
    client_part = tor_malloc_zero(REND_BASIC_AUTH_CLIENT_ENTRY_LEN);
    /* Encrypt session key. */
    cipher = crypto_cipher_new(cookie);
    if (crypto_cipher_encrypt(cipher, client_part +
                                  REND_BASIC_AUTH_CLIENT_ID_LEN,
                              session_key, CIPHER_KEY_LEN) < 0) {
      log_warn(LD_REND, "Could not encrypt session key for client.");
      crypto_cipher_free(cipher);
      tor_free(client_part);
      goto done;
    }
    crypto_cipher_free(cipher);

    /* Determine client ID. */
    digest = crypto_digest_new();
    crypto_digest_add_bytes(digest, cookie, REND_DESC_COOKIE_LEN);
    crypto_digest_add_bytes(digest, iv, CIPHER_IV_LEN);
    crypto_digest_get_digest(digest, client_part,
                             REND_BASIC_AUTH_CLIENT_ID_LEN);
    crypto_digest_free(digest);

    /* Put both together. */
    smartlist_add(encrypted_session_keys, client_part);
  } SMARTLIST_FOREACH_END(cookie);

  /* Add some fake client IDs and encrypted session keys. */
  for (i = (smartlist_len(client_cookies) - 1) %
           REND_BASIC_AUTH_CLIENT_MULTIPLE;
       i < REND_BASIC_AUTH_CLIENT_MULTIPLE - 1; i++) {
    client_part = tor_malloc_zero(REND_BASIC_AUTH_CLIENT_ENTRY_LEN);
    crypto_rand(client_part, REND_BASIC_AUTH_CLIENT_ENTRY_LEN);
    smartlist_add(encrypted_session_keys, client_part);
  }
  /* Sort smartlist and put elements in result in order. */
  smartlist_sort_digests(encrypted_session_keys);
  pos = 2;
  SMARTLIST_FOREACH(encrypted_session_keys, const char *, entry, {
    memcpy(enc + pos, entry, REND_BASIC_AUTH_CLIENT_ENTRY_LEN);
    pos += REND_BASIC_AUTH_CLIENT_ENTRY_LEN;
  });
  *encrypted_out = enc;
  *encrypted_len_out = len;
  enc = NULL; /* prevent free. */
  r = 0;
 done:
  tor_free(enc);
  if (encrypted_session_keys) {
    SMARTLIST_FOREACH(encrypted_session_keys, char *, d, tor_free(d););
    smartlist_free(encrypted_session_keys);
  }
  return r;
}

/** Encrypt the encoded introduction points in <b>encoded</b> using
 * authorization type 'stealth' with <b>descriptor_cookie</b> of length
 * REND_DESC_COOKIE_LEN and write the result to a newly allocated string
 * pointed to by <b>encrypted_out</b> of length <b>encrypted_len_out</b>.
 * Return 0 for success, -1 otherwise. */
static int
rend_encrypt_v2_intro_points_stealth(char **encrypted_out,
                                     size_t *encrypted_len_out,
                                     const char *encoded,
                                     const char *descriptor_cookie)
{
  int r = -1, enclen;
  char *enc;
  tor_assert(encoded);
  tor_assert(descriptor_cookie);

  enc = tor_malloc_zero(1 + CIPHER_IV_LEN + strlen(encoded));
  enc[0] = 0x02; /* Auth type */
  enclen = crypto_cipher_encrypt_with_iv(descriptor_cookie,
                                         enc + 1,
                                         CIPHER_IV_LEN+strlen(encoded),
                                         encoded, strlen(encoded));
  if (enclen < 0) {
    log_warn(LD_REND, "Could not encrypt introduction point string.");
    goto done;
  }
  *encrypted_out = enc;
  *encrypted_len_out = enclen;
  enc = NULL; /* prevent free */
  r = 0;
 done:
  tor_free(enc);
  return r;
}

/** Attempt to parse the given <b>desc_str</b> and return true if this
 * succeeds, false otherwise. */
static int
rend_desc_v2_is_parsable(rend_encoded_v2_service_descriptor_t *desc)
{
  rend_service_descriptor_t *test_parsed = NULL;
  char test_desc_id[DIGEST_LEN];
  char *test_intro_content = NULL;
  size_t test_intro_size;
  size_t test_encoded_size;
  const char *test_next;
  int res = rend_parse_v2_service_descriptor(&test_parsed, test_desc_id,
                                         &test_intro_content,
                                         &test_intro_size,
                                         &test_encoded_size,
                                         &test_next, desc->desc_str, 1);
  rend_service_descriptor_free(test_parsed);
  tor_free(test_intro_content);
  return (res >= 0);
}

/** Free the storage held by an encoded v2 service descriptor. */
void
rend_encoded_v2_service_descriptor_free(
  rend_encoded_v2_service_descriptor_t *desc)
{
  if (!desc)
    return;
  tor_free(desc->desc_str);
  tor_free(desc);
}

/** Free the storage held by an introduction point info. */
void
rend_intro_point_free(rend_intro_point_t *intro)
{
  if (!intro)
    return;

  extend_info_free(intro->extend_info);
  crypto_pk_free(intro->intro_key);

  if (intro->accepted_intro_rsa_parts != NULL) {
    replaycache_free(intro->accepted_intro_rsa_parts);
  }

  tor_free(intro);
}

/** Encode a set of rend_encoded_v2_service_descriptor_t's for <b>desc</b>
 * at time <b>now</b> using <b>service_key</b>, depending on
 * <b>auth_type</b> a <b>descriptor_cookie</b> and a list of
 * <b>client_cookies</b> (which are both <b>NULL</b> if no client
 * authorization is performed), and <b>period</b> (e.g. 0 for the current
 * period, 1 for the next period, etc.) and add them to the existing list
 * <b>descs_out</b>; return the number of seconds that the descriptors will
 * be found by clients, or -1 if the encoding was not successful. */
int
rend_encode_v2_descriptors(smartlist_t *descs_out,
                           rend_service_descriptor_t *desc, time_t now,
                           uint8_t period, rend_auth_type_t auth_type,
                           crypto_pk_t *client_key,
                           smartlist_t *client_cookies)
{
  char service_id[DIGEST_LEN];
  char service_id_base32[REND_SERVICE_ID_LEN_BASE32+1];
  uint32_t time_period;
  char *ipos_base64 = NULL, *ipos = NULL, *ipos_encrypted = NULL,
       *descriptor_cookie = NULL;
  size_t ipos_len = 0, ipos_encrypted_len = 0;
  int k;
  uint32_t seconds_valid;
  crypto_pk_t *service_key;
  if (!desc) {
    log_warn(LD_BUG, "Could not encode v2 descriptor: No desc given.");
    return -1;
  }
  service_key = (auth_type == REND_STEALTH_AUTH) ? client_key : desc->pk;
  tor_assert(service_key);
  if (auth_type == REND_STEALTH_AUTH) {
    descriptor_cookie = smartlist_get(client_cookies, 0);
    tor_assert(descriptor_cookie);
  }
  /* Obtain service_id from public key. */
  crypto_pk_get_digest(service_key, service_id);
  /* Calculate current time-period. */
  time_period = get_time_period(now, period, service_id);
  /* Determine how many seconds the descriptor will be valid. */
  seconds_valid = period * REND_TIME_PERIOD_V2_DESC_VALIDITY +
                  get_seconds_valid(now, service_id);
  /* Assemble, possibly encrypt, and encode introduction points. */
  if (smartlist_len(desc->intro_nodes) > 0) {
    if (rend_encode_v2_intro_points(&ipos, desc) < 0) {
      log_warn(LD_REND, "Encoding of introduction points did not succeed.");
      return -1;
    }
    switch (auth_type) {
      case REND_NO_AUTH:
        ipos_len = strlen(ipos);
        break;
      case REND_BASIC_AUTH:
        if (rend_encrypt_v2_intro_points_basic(&ipos_encrypted,
                                               &ipos_encrypted_len, ipos,
                                               client_cookies) < 0) {
          log_warn(LD_REND, "Encrypting of introduction points did not "
                            "succeed.");
          tor_free(ipos);
          return -1;
        }
        tor_free(ipos);
        ipos = ipos_encrypted;
        ipos_len = ipos_encrypted_len;
        break;
      case REND_STEALTH_AUTH:
        if (rend_encrypt_v2_intro_points_stealth(&ipos_encrypted,
                                                 &ipos_encrypted_len, ipos,
                                                 descriptor_cookie) < 0) {
          log_warn(LD_REND, "Encrypting of introduction points did not "
                            "succeed.");
          tor_free(ipos);
          return -1;
        }
        tor_free(ipos);
        ipos = ipos_encrypted;
        ipos_len = ipos_encrypted_len;
        break;
      default:
        log_warn(LD_REND|LD_BUG, "Unrecognized authorization type %d",
                 (int)auth_type);
        tor_free(ipos);
        return -1;
    }
    /* Base64-encode introduction points. */
    ipos_base64 = tor_calloc(ipos_len, 2);
    if (base64_encode(ipos_base64, ipos_len * 2, ipos, ipos_len,
                      BASE64_ENCODE_MULTILINE)<0) {
      log_warn(LD_REND, "Could not encode introduction point string to "
               "base64. length=%d", (int)ipos_len);
      tor_free(ipos_base64);
      tor_free(ipos);
      return -1;
    }
    tor_free(ipos);
  }
  /* Encode REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS descriptors. */
  for (k = 0; k < REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS; k++) {
    char secret_id_part[DIGEST_LEN];
    char secret_id_part_base32[REND_SECRET_ID_PART_LEN_BASE32 + 1];
    char desc_id_base32[REND_DESC_ID_V2_LEN_BASE32 + 1];
    char *permanent_key = NULL;
    size_t permanent_key_len;
    char published[ISO_TIME_LEN+1];
    int i;
    char protocol_versions_string[16]; /* max len: "0,1,2,3,4,5,6,7\0" */
    size_t protocol_versions_written;
    size_t desc_len;
    char *desc_str = NULL;
    int result = 0;
    size_t written = 0;
    char desc_digest[DIGEST_LEN];
    rend_encoded_v2_service_descriptor_t *enc =
      tor_malloc_zero(sizeof(rend_encoded_v2_service_descriptor_t));
    /* Calculate secret-id-part = h(time-period | cookie | replica). */
    get_secret_id_part_bytes(secret_id_part, time_period, descriptor_cookie,
                             k);
    base32_encode(secret_id_part_base32, sizeof(secret_id_part_base32),
                  secret_id_part, DIGEST_LEN);
    /* Calculate descriptor ID. */
    rend_get_descriptor_id_bytes(enc->desc_id, service_id, secret_id_part);
    base32_encode(desc_id_base32, sizeof(desc_id_base32),
                  enc->desc_id, DIGEST_LEN);
    /* PEM-encode the public key */
    if (crypto_pk_write_public_key_to_string(service_key, &permanent_key,
                                             &permanent_key_len) < 0) {
      log_warn(LD_BUG, "Could not write public key to string.");
      rend_encoded_v2_service_descriptor_free(enc);
      goto err;
    }
    /* Encode timestamp. */
    format_iso_time(published, desc->timestamp);
    /* Write protocol-versions bitmask to comma-separated value string. */
    protocol_versions_written = 0;
    for (i = 0; i < 8; i++) {
      if (desc->protocols & 1 << i) {
        tor_snprintf(protocol_versions_string + protocol_versions_written,
                     16 - protocol_versions_written, "%d,", i);
        protocol_versions_written += 2;
      }
    }
    if (protocol_versions_written)
      protocol_versions_string[protocol_versions_written - 1] = '\0';
    else
      protocol_versions_string[0]= '\0';
    /* Assemble complete descriptor. */
    desc_len = 2000 + smartlist_len(desc->intro_nodes) * 1000; /* far too long,
                                                                  but okay.*/
    enc->desc_str = desc_str = tor_malloc_zero(desc_len);
    result = tor_snprintf(desc_str, desc_len,
             "rendezvous-service-descriptor %s\n"
             "version 2\n"
             "permanent-key\n%s"
             "secret-id-part %s\n"
             "publication-time %s\n"
             "protocol-versions %s\n",
        desc_id_base32,
        permanent_key,
        secret_id_part_base32,
        published,
        protocol_versions_string);
    tor_free(permanent_key);
    if (result < 0) {
      log_warn(LD_BUG, "Descriptor ran out of room.");
      rend_encoded_v2_service_descriptor_free(enc);
      goto err;
    }
    written = result;
    /* Add introduction points. */
    if (ipos_base64) {
      result = tor_snprintf(desc_str + written, desc_len - written,
                            "introduction-points\n"
                            "-----BEGIN MESSAGE-----\n%s"
                            "-----END MESSAGE-----\n",
                            ipos_base64);
      if (result < 0) {
        log_warn(LD_BUG, "could not write introduction points.");
        rend_encoded_v2_service_descriptor_free(enc);
        goto err;
      }
      written += result;
    }
    /* Add signature. */
    strlcpy(desc_str + written, "signature\n", desc_len - written);
    written += strlen(desc_str + written);
    if (crypto_digest(desc_digest, desc_str, written) < 0) {
      log_warn(LD_BUG, "could not create digest.");
      rend_encoded_v2_service_descriptor_free(enc);
      goto err;
    }
    if (router_append_dirobj_signature(desc_str + written,
                                       desc_len - written,
                                       desc_digest, DIGEST_LEN,
                                       service_key) < 0) {
      log_warn(LD_BUG, "Couldn't sign desc.");
      rend_encoded_v2_service_descriptor_free(enc);
      goto err;
    }
    written += strlen(desc_str+written);
    if (written+2 > desc_len) {
        log_warn(LD_BUG, "Could not finish desc.");
        rend_encoded_v2_service_descriptor_free(enc);
        goto err;
    }
    desc_str[written++] = 0;
    /* Check if we can parse our own descriptor. */
    if (!rend_desc_v2_is_parsable(enc)) {
      log_warn(LD_BUG, "Could not parse my own descriptor: %s", desc_str);
      rend_encoded_v2_service_descriptor_free(enc);
      goto err;
    }
    smartlist_add(descs_out, enc);
    /* Add the uploaded descriptor to the local service's descriptor cache */
    rend_cache_store_v2_desc_as_service(enc->desc_str);
    base32_encode(service_id_base32, sizeof(service_id_base32),
          service_id, REND_SERVICE_ID_LEN);
    control_event_hs_descriptor_created(service_id_base32, desc_id_base32, k);
  }

  log_info(LD_REND, "Successfully encoded a v2 descriptor and "
                    "confirmed that it is parsable.");
  goto done;

 err:
  SMARTLIST_FOREACH(descs_out, rend_encoded_v2_service_descriptor_t *, d,
                    rend_encoded_v2_service_descriptor_free(d););
  smartlist_clear(descs_out);
  seconds_valid = -1;

 done:
  tor_free(ipos_base64);
  return seconds_valid;
}

/** Sets <b>out</b> to the first 10 bytes of the digest of <b>pk</b>,
 * base32 encoded.  NUL-terminates out.  (We use this string to
 * identify services in directory requests and .onion URLs.)
 */
int
rend_get_service_id(crypto_pk_t *pk, char *out)
{
  char buf[DIGEST_LEN];
  tor_assert(pk);
  if (crypto_pk_get_digest(pk, buf) < 0)
    return -1;
  base32_encode(out, REND_SERVICE_ID_LEN_BASE32+1, buf, REND_SERVICE_ID_LEN);
  return 0;
}

/** Return true iff <b>query</b> is a syntactically valid service ID (as
 * generated by rend_get_service_id).  */
int
rend_valid_service_id(const char *query)
{
  if (strlen(query) != REND_SERVICE_ID_LEN_BASE32)
    return 0;

  if (strspn(query, BASE32_CHARS) != REND_SERVICE_ID_LEN_BASE32)
    return 0;

  return 1;
}

/** Return true iff <b>query</b> is a syntactically valid descriptor ID.
 * (as generated by rend_get_descriptor_id_bytes). */
int
rend_valid_descriptor_id(const char *query)
{
  if (strlen(query) != REND_DESC_ID_V2_LEN_BASE32) {
    goto invalid;
  }
  if (strspn(query, BASE32_CHARS) != REND_DESC_ID_V2_LEN_BASE32) {
    goto invalid;
  }

  return 1;

 invalid:
  return 0;
}

/** Called when we get a rendezvous-related relay cell on circuit
 * <b>circ</b>.  Dispatch on rendezvous relay command. */
void
rend_process_relay_cell(circuit_t *circ, const crypt_path_t *layer_hint,
                        int command, size_t length,
                        const uint8_t *payload)
{
  or_circuit_t *or_circ = NULL;
  origin_circuit_t *origin_circ = NULL;
  int r = -2;
  if (CIRCUIT_IS_ORIGIN(circ)) {
    origin_circ = TO_ORIGIN_CIRCUIT(circ);
    if (!layer_hint || layer_hint != origin_circ->cpath->prev) {
      log_fn(LOG_PROTOCOL_WARN, LD_APP,
             "Relay cell (rend purpose %d) from wrong hop on origin circ",
             command);
      origin_circ = NULL;
    }
  } else {
    or_circ = TO_OR_CIRCUIT(circ);
  }

  switch (command) {
    case RELAY_COMMAND_ESTABLISH_INTRO:
      if (or_circ)
        r = rend_mid_establish_intro(or_circ,payload,length);
      break;
    case RELAY_COMMAND_ESTABLISH_RENDEZVOUS:
      if (or_circ)
        r = rend_mid_establish_rendezvous(or_circ,payload,length);
      break;
    case RELAY_COMMAND_INTRODUCE1:
      if (or_circ)
        r = rend_mid_introduce(or_circ,payload,length);
      break;
    case RELAY_COMMAND_INTRODUCE2:
      if (origin_circ)
        r = rend_service_receive_introduction(origin_circ,payload,length);
      break;
    case RELAY_COMMAND_INTRODUCE_ACK:
      if (origin_circ)
        r = rend_client_introduction_acked(origin_circ,payload,length);
      break;
    case RELAY_COMMAND_RENDEZVOUS1:
      if (or_circ)
        r = rend_mid_rendezvous(or_circ,payload,length);
      break;
    case RELAY_COMMAND_RENDEZVOUS2:
      if (origin_circ)
        r = rend_client_receive_rendezvous(origin_circ,payload,length);
      break;
    case RELAY_COMMAND_INTRO_ESTABLISHED:
      if (origin_circ)
        r = rend_service_intro_established(origin_circ,payload,length);
      break;
    case RELAY_COMMAND_RENDEZVOUS_ESTABLISHED:
      if (origin_circ)
        r = rend_client_rendezvous_acked(origin_circ,payload,length);
      break;
    default:
      tor_fragile_assert();
  }

  if (r == -2)
    log_info(LD_PROTOCOL, "Dropping cell (type %d) for wrong circuit type.",
             command);
}

/** Allocate and return a new rend_data_t with the same
 * contents as <b>query</b>. */
rend_data_t *
rend_data_dup(const rend_data_t *data)
{
  rend_data_t *data_dup;
  tor_assert(data);
  data_dup = tor_memdup(data, sizeof(rend_data_t));
  data_dup->hsdirs_fp = smartlist_new();
  SMARTLIST_FOREACH(data->hsdirs_fp, char *, fp,
                    smartlist_add(data_dup->hsdirs_fp,
                                  tor_memdup(fp, DIGEST_LEN)));
  return data_dup;
}

/** Compute descriptor ID for each replicas and save them. A valid onion
 * address must be present in the <b>rend_data</b>.
 *
 * Return 0 on success else -1. */
static int
compute_desc_id(rend_data_t *rend_data)
{
  int ret = 0;
  unsigned replica;
  time_t now = time(NULL);

  tor_assert(rend_data);

  /* Compute descriptor ID for each replicas. */
  for (replica = 0; replica < ARRAY_LENGTH(rend_data->descriptor_id);
       replica++) {
    ret = rend_compute_v2_desc_id(rend_data->descriptor_id[replica],
                                  rend_data->onion_address,
                                  rend_data->descriptor_cookie,
                                  now, replica);
    if (ret < 0) {
      goto end;
    }
  }

 end:
  return ret;
}

/** Allocate and initialize a rend_data_t object for a service using the
 * given arguments. Only the <b>onion_address</b> is not optional.
 *
 * Return a valid rend_data_t pointer. */
rend_data_t *
rend_data_service_create(const char *onion_address, const char *pk_digest,
                         const uint8_t *cookie, rend_auth_type_t auth_type)
{
  rend_data_t *rend_data = tor_malloc_zero(sizeof(*rend_data));

  /* We need at least one else the call is wrong. */
  tor_assert(onion_address != NULL);

  if (pk_digest) {
    memcpy(rend_data->rend_pk_digest, pk_digest,
           sizeof(rend_data->rend_pk_digest));
  }
  if (cookie) {
    memcpy(rend_data->rend_cookie, cookie,
           sizeof(rend_data->rend_cookie));
  }

  strlcpy(rend_data->onion_address, onion_address,
          sizeof(rend_data->onion_address));
  rend_data->auth_type = auth_type;
  /* Won't be used but still need to initialize it for rend_data dup and
   * free. */
  rend_data->hsdirs_fp = smartlist_new();

  return rend_data;
}

/** Allocate and initialize a rend_data_t object for a client request using
 * the given arguments.  Either an onion address or a descriptor ID is
 * needed. Both can be given but only the onion address will be used to make
 * the descriptor fetch.
 *
 * Return a valid rend_data_t pointer or NULL on error meaning the
 * descriptor IDs couldn't be computed from the given data. */
rend_data_t *
rend_data_client_create(const char *onion_address, const char *desc_id,
                        const char *cookie, rend_auth_type_t auth_type)
{
  rend_data_t *rend_data = tor_malloc_zero(sizeof(*rend_data));

  /* We need at least one else the call is wrong. */
  tor_assert(onion_address != NULL || desc_id != NULL);

  if (cookie) {
    memcpy(rend_data->descriptor_cookie, cookie,
           sizeof(rend_data->descriptor_cookie));
  }
  if (desc_id) {
    memcpy(rend_data->desc_id_fetch, desc_id,
           sizeof(rend_data->desc_id_fetch));
  }
  if (onion_address) {
    strlcpy(rend_data->onion_address, onion_address,
            sizeof(rend_data->onion_address));
    if (compute_desc_id(rend_data) < 0) {
      goto error;
    }
  }

  rend_data->auth_type = auth_type;
  rend_data->hsdirs_fp = smartlist_new();

  return rend_data;

 error:
  rend_data_free(rend_data);
  return NULL;
}

/** Determine the routers that are responsible for <b>id</b> (binary) and
 * add pointers to those routers' routerstatus_t to <b>responsible_dirs</b>.
 * Return -1 if we're returning an empty smartlist, else return 0.
 */
int
hid_serv_get_responsible_directories(smartlist_t *responsible_dirs,
                                     const char *id)
{
  int start, found, n_added = 0, i;
  networkstatus_t *c = networkstatus_get_latest_consensus();
  if (!c || !smartlist_len(c->routerstatus_list)) {
    log_warn(LD_REND, "We don't have a consensus, so we can't perform v2 "
             "rendezvous operations.");
    return -1;
  }
  tor_assert(id);
  start = networkstatus_vote_find_entry_idx(c, id, &found);
  if (start == smartlist_len(c->routerstatus_list)) start = 0;
  i = start;
  do {
    routerstatus_t *r = smartlist_get(c->routerstatus_list, i);
    if (r->is_hs_dir) {
      smartlist_add(responsible_dirs, r);
      if (++n_added == REND_NUMBER_OF_CONSECUTIVE_REPLICAS)
        return 0;
    }
    if (++i == smartlist_len(c->routerstatus_list))
      i = 0;
  } while (i != start);

  /* Even though we don't have the desired number of hidden service
   * directories, be happy if we got any. */
  return smartlist_len(responsible_dirs) ? 0 : -1;
}

