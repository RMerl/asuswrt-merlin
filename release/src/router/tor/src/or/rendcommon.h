/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rendcommon.h
 * \brief Header file for rendcommon.c.
 **/

#ifndef TOR_RENDCOMMON_H
#define TOR_RENDCOMMON_H

/** Free all storage associated with <b>data</b> */
static INLINE void
rend_data_free(rend_data_t *data)
{
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

void rend_cache_init(void);
void rend_cache_clean(time_t now);
void rend_cache_clean_v2_descs_as_dir(time_t now);
void rend_cache_purge(void);
void rend_cache_free_all(void);
int rend_valid_service_id(const char *query);
int rend_cache_lookup_entry(const char *query, int version,
                            rend_cache_entry_t **entry_out);
int rend_cache_lookup_v2_desc_as_dir(const char *query, const char **desc);
/** Return value from rend_cache_store_v2_desc_as_{dir,client}. */
typedef enum {
  RCS_NOTDIR = -2, /**< We're not a directory */
  RCS_BADDESC = -1, /**< This descriptor is no good. */
  RCS_OKAY = 0 /**< All worked as expected */
} rend_cache_store_status_t;

rend_cache_store_status_t rend_cache_store_v2_desc_as_dir(const char *desc);
rend_cache_store_status_t rend_cache_store_v2_desc_as_client(const char *desc,
                                       const rend_data_t *rend_query);

int rend_encode_v2_descriptors(smartlist_t *descs_out,
                               rend_service_descriptor_t *desc, time_t now,
                               uint8_t period, rend_auth_type_t auth_type,
                               crypto_pk_t *client_key,
                               smartlist_t *client_cookies);
int rend_compute_v2_desc_id(char *desc_id_out, const char *service_id,
                            const char *descriptor_cookie,
                            time_t now, uint8_t replica);
int rend_id_is_in_interval(const char *a, const char *b, const char *c);
void rend_get_descriptor_id_bytes(char *descriptor_id_out,
                                  const char *service_id,
                                  const char *secret_id_part);

#endif

