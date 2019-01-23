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

/** Represents a single hidden service running at this OP. */
typedef struct rend_service_t {
  /* Fields specified in config file */
  char *directory; /**< where in the filesystem it stores it. Will be NULL if
                    * this service is ephemeral. */
  int dir_group_readable; /**< if 1, allow group read
                             permissions on directory */
  smartlist_t *ports; /**< List of rend_service_port_config_t */
  rend_auth_type_t auth_type; /**< Client authorization type or 0 if no client
                               * authorization is performed. */
  smartlist_t *clients; /**< List of rend_authorized_client_t's of
                         * clients that may access our service. Can be NULL
                         * if no client authorization is performed. */
  /* Other fields */
  crypto_pk_t *private_key; /**< Permanent hidden-service key. */
  char service_id[REND_SERVICE_ID_LEN_BASE32+1]; /**< Onion address without
                                                  * '.onion' */
  char pk_digest[DIGEST_LEN]; /**< Hash of permanent hidden-service key. */
  smartlist_t *intro_nodes; /**< List of rend_intro_point_t's we have,
                             * or are trying to establish. */
  /** List of rend_intro_point_t that are expiring. They are removed once
   * the new descriptor is successfully uploaded. A node in this list CAN
   * NOT appear in the intro_nodes list. */
  smartlist_t *expiring_nodes;
  time_t intro_period_started; /**< Start of the current period to build
                                * introduction points. */
  int n_intro_circuits_launched; /**< Count of intro circuits we have
                                  * established in this period. */
  unsigned int n_intro_points_wanted; /**< Number of intro points this
                                       * service wants to have open. */
  rend_service_descriptor_t *desc; /**< Current hidden service descriptor. */
  time_t desc_is_dirty; /**< Time at which changes to the hidden service
                         * descriptor content occurred, or 0 if it's
                         * up-to-date. */
  time_t next_upload_time; /**< Scheduled next hidden service descriptor
                            * upload time. */
  /** Replay cache for Diffie-Hellman values of INTRODUCE2 cells, to
   * detect repeats.  Clients may send INTRODUCE1 cells for the same
   * rendezvous point through two or more different introduction points;
   * when they do, this keeps us from launching multiple simultaneous attempts
   * to connect to the same rend point. */
  replaycache_t *accepted_intro_dh_parts;
  /** If true, we don't close circuits for making requests to unsupported
   * ports. */
  int allow_unknown_ports;
  /** The maximum number of simultanious streams-per-circuit that are allowed
   * to be established, or 0 if no limit is set.
   */
  int max_streams_per_circuit;
  /** If true, we close circuits that exceed the max_streams_per_circuit
   * limit.  */
  int max_streams_close_circuit;
} rend_service_t;

STATIC void rend_service_free(rend_service_t *service);
STATIC char *rend_service_sos_poison_path(const rend_service_t *service);
STATIC int rend_service_check_dir_and_add(smartlist_t *service_list,
                                          const or_options_t *options,
                                          rend_service_t *service,
                                          int validate_only);
STATIC int rend_service_verify_single_onion_poison(
                                                  const rend_service_t *s,
                                                  const or_options_t *options);
STATIC int rend_service_poison_new_single_onion_dir(
                                                  const rend_service_t *s,
                                                  const or_options_t* options);
#endif

int num_rend_services(void);
int rend_config_services(const or_options_t *options, int validate_only);
int rend_service_load_all_keys(const smartlist_t *service_list);
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

void rend_authorized_client_free(rend_authorized_client_t *client);

/** Return value from rend_service_add_ephemeral. */
typedef enum {
  RSAE_BADAUTH = -5, /**< Invalid auth_type/auth_clients */
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
                               rend_auth_type_t auth_type,
                               smartlist_t *auth_clients,
                               char **service_id_out);
int rend_service_del_ephemeral(const char *service_id);

void directory_post_to_hs_dir(rend_service_descriptor_t *renddesc,
                              smartlist_t *descs, smartlist_t *hs_dirs,
                              const char *service_id, int seconds_valid);
void rend_service_desc_has_uploaded(const rend_data_t *rend_data);

int rend_service_allow_non_anonymous_connection(const or_options_t *options);
int rend_service_reveal_startup_time(const or_options_t *options);
int rend_service_non_anonymous_mode_enabled(const or_options_t *options);

#endif

