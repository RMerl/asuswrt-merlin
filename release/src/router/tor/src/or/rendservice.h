/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file rendservice.h
 * \brief Header file for rendservice.c.
 **/

#ifndef TOR_RENDSERVICE_H
#define TOR_RENDSERVICE_H

#include "or.h"

typedef struct rend_intro_cell_s rend_intro_cell_t;
typedef struct rend_service_port_config_s rend_service_port_config_t;

#ifdef RENDSERVICE_PRIVATE

/* This can be used for both INTRODUCE1 and INTRODUCE2 */

struct rend_intro_cell_s {
  /* Is this an INTRODUCE1 or INTRODUCE2? (set to 1 or 2) */
  uint8_t type;
  /* Public key digest */
  uint8_t pk[DIGEST_LEN];
  /* Optionally, store ciphertext here */
  uint8_t *ciphertext;
  ssize_t ciphertext_len;
  /* Optionally, store plaintext */
  uint8_t *plaintext;
  ssize_t plaintext_len;
  /* Have we parsed the plaintext? */
  uint8_t parsed;
  /* intro protocol version (0, 1, 2 or 3) */
  uint8_t version;
  /* Version-specific parts */
  union {
    struct {
      /* Rendezvous point nickname or hex-encoded key digest */
      uint8_t rp[42];
    } v0_v1;
    struct {
      /* The extend_info_t struct has everything v2 uses */
      extend_info_t *extend_info;
    } v2;
    struct {
      /* Auth type used */
      uint8_t auth_type;
      /* Length of auth data */
      uint16_t auth_len;
      /* Auth data */
      uint8_t *auth_data;
      /* Rendezvous point's IP address/port, identity digest and onion key */
      extend_info_t *extend_info;
    } v3;
  } u;
  /* Rendezvous cookie */
  uint8_t rc[REND_COOKIE_LEN];
  /* Diffie-Hellman data */
  uint8_t dh[DH_KEY_LEN];
};

#endif

int num_rend_services(void);
int rend_config_services(const or_options_t *options, int validate_only);
int rend_service_load_all_keys(void);
void rend_services_add_filenames_to_lists(smartlist_t *open_lst,
                                          smartlist_t *stat_lst);
void rend_consider_services_intro_points(void);
void rend_consider_services_upload(time_t now);
void rend_hsdir_routers_changed(void);
void rend_consider_descriptor_republication(void);

void rend_service_intro_has_opened(origin_circuit_t *circuit);
int rend_service_intro_established(origin_circuit_t *circuit,
                                   const uint8_t *request,
                                   size_t request_len);
void rend_service_rendezvous_has_opened(origin_circuit_t *circuit);
int rend_service_receive_introduction(origin_circuit_t *circuit,
                                      const uint8_t *request,
                                      size_t request_len);
int rend_service_decrypt_intro(rend_intro_cell_t *request,
                               crypto_pk_t *key,
                               char **err_msg_out);
void rend_service_free_intro(rend_intro_cell_t *request);
rend_intro_cell_t * rend_service_begin_parse_intro(const uint8_t *request,
                                                   size_t request_len,
                                                   uint8_t type,
                                                   char **err_msg_out);
int rend_service_parse_intro_plaintext(rend_intro_cell_t *intro,
                                       char **err_msg_out);
int rend_service_validate_intro_late(const rend_intro_cell_t *intro,
                                     char **err_msg_out);
void rend_service_relaunch_rendezvous(origin_circuit_t *oldcirc);
int rend_service_set_connection_addr_port(edge_connection_t *conn,
                                          origin_circuit_t *circ);
void rend_service_dump_stats(int severity);
void rend_service_free_all(void);

rend_service_port_config_t *rend_service_parse_port_config(const char *string,
                                                           const char *sep,
                                                           char **err_msg_out);
void rend_service_port_config_free(rend_service_port_config_t *p);

/** Return value from rend_service_add_ephemeral. */
typedef enum {
  RSAE_BADVIRTPORT = -4, /**< Invalid VIRTPORT/TARGET(s) */
  RSAE_ADDREXISTS = -3, /**< Onion address collision */
  RSAE_BADPRIVKEY = -2, /**< Invalid public key */
  RSAE_INTERNAL = -1, /**< Internal error */
  RSAE_OKAY = 0 /**< Service added as expected */
} rend_service_add_ephemeral_status_t;
rend_service_add_ephemeral_status_t rend_service_add_ephemeral(crypto_pk_t *pk,
                               smartlist_t *ports,
                               int max_streams_per_circuit,
                               int max_streams_close_circuit,
                               char **service_id_out);
int rend_service_del_ephemeral(const char *service_id);

void directory_post_to_hs_dir(rend_service_descriptor_t *renddesc,
                              smartlist_t *descs, smartlist_t *hs_dirs,
                              const char *service_id, int seconds_valid);
void rend_service_desc_has_uploaded(const rend_data_t *rend_data);

#endif

