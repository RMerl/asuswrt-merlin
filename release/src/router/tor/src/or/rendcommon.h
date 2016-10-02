/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rendcommon.h
 * \brief Header file for rendcommon.c.
 **/

#ifndef TOR_RENDCOMMON_H
#define TOR_RENDCOMMON_H

typedef enum rend_intro_point_failure_t {
  INTRO_POINT_FAILURE_GENERIC     = 0,
  INTRO_POINT_FAILURE_TIMEOUT     = 1,
  INTRO_POINT_FAILURE_UNREACHABLE = 2,
} rend_intro_point_failure_t;

/** Free all storage associated with <b>data</b> */
static inline void
rend_data_free(rend_data_t *data)
{
  if (!data) {
    return;
  }
  /* Cleanup the HSDir identity digest. */
  SMARTLIST_FOREACH(data->hsdirs_fp, char *, d, tor_free(d));
  smartlist_free(data->hsdirs_fp);
  tor_free(data);
}

int rend_cmp_service_ids(const char *one, const char *two);

void rend_process_relay_cell(circuit_t *circ, const crypt_path_t *layer_hint,
                             int command, size_t length,
                             const uint8_t *payload);

void rend_service_descriptor_free(rend_service_descriptor_t *desc);
int rend_get_service_id(crypto_pk_t *pk, char *out);
void rend_encoded_v2_service_descriptor_free(
                               rend_encoded_v2_service_descriptor_t *desc);
void rend_intro_point_free(rend_intro_point_t *intro);

int rend_valid_service_id(const char *query);
int rend_valid_descriptor_id(const char *query);
int rend_encode_v2_descriptors(smartlist_t *descs_out,
                               rend_service_descriptor_t *desc, time_t now,
                               uint8_t period, rend_auth_type_t auth_type,
                               crypto_pk_t *client_key,
                               smartlist_t *client_cookies);
int rend_compute_v2_desc_id(char *desc_id_out, const char *service_id,
                            const char *descriptor_cookie,
                            time_t now, uint8_t replica);
void rend_get_descriptor_id_bytes(char *descriptor_id_out,
                                  const char *service_id,
                                  const char *secret_id_part);
int hid_serv_get_responsible_directories(smartlist_t *responsible_dirs,
                                         const char *id);

rend_data_t *rend_data_dup(const rend_data_t *data);
rend_data_t *rend_data_client_create(const char *onion_address,
                                     const char *desc_id,
                                     const char *cookie,
                                     rend_auth_type_t auth_type);
rend_data_t *rend_data_service_create(const char *onion_address,
                                      const char *pk_digest,
                                      const uint8_t *cookie,
                                      rend_auth_type_t auth_type);
#endif

