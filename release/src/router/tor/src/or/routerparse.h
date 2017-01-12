/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file routerparse.h
 * \brief Header file for routerparse.c.
 **/

#ifndef TOR_ROUTERPARSE_H
#define TOR_ROUTERPARSE_H

int router_get_router_hash(const char *s, size_t s_len, char *digest);
int router_get_dir_hash(const char *s, char *digest);
int router_get_networkstatus_v3_hashes(const char *s,
                                       common_digests_t *digests);
int router_get_extrainfo_hash(const char *s, size_t s_len, char *digest);
#define DIROBJ_MAX_SIG_LEN 256
char *router_get_dirobj_signature(const char *digest,
                                  size_t digest_len,
                                  const crypto_pk_t *private_key);
int router_append_dirobj_signature(char *buf, size_t buf_len,
                                   const char *digest,
                                   size_t digest_len,
                                   crypto_pk_t *private_key);
int router_parse_list_from_string(const char **s, const char *eos,
                                  smartlist_t *dest,
                                  saved_location_t saved_location,
                                  int is_extrainfo,
                                  int allow_annotations,
                                  const char *prepend_annotations,
                                  smartlist_t *invalid_digests_out);

routerinfo_t *router_parse_entry_from_string(const char *s, const char *end,
                                             int cache_copy,
                                             int allow_annotations,
                                             const char *prepend_annotations,
                                             int *can_dl_again_out);
extrainfo_t *extrainfo_parse_entry_from_string(const char *s, const char *end,
                             int cache_copy, struct digest_ri_map_t *routermap,
                             int *can_dl_again_out);
MOCK_DECL(addr_policy_t *, router_parse_addr_policy_item_from_string,
         (const char *s, int assume_action, int *malformed_list));
version_status_t tor_version_is_obsolete(const char *myversion,
                                         const char *versionlist);
int tor_version_as_new_as(const char *platform, const char *cutoff);
int tor_version_parse(const char *s, tor_version_t *out);
int tor_version_compare(tor_version_t *a, tor_version_t *b);
int tor_version_same_series(tor_version_t *a, tor_version_t *b);
void sort_version_list(smartlist_t *lst, int remove_duplicates);
void assert_addr_policy_ok(smartlist_t *t);
void dump_distinct_digest_count(int severity);

int compare_vote_routerstatus_entries(const void **_a, const void **_b);
int networkstatus_verify_bw_weights(networkstatus_t *ns, int);
networkstatus_t *networkstatus_parse_vote_from_string(const char *s,
                                                 const char **eos_out,
                                                 networkstatus_type_t ns_type);
ns_detached_signatures_t *networkstatus_parse_detached_signatures(
                                          const char *s, const char *eos);

smartlist_t *microdescs_parse_from_string(const char *s, const char *eos,
                                          int allow_annotations,
                                          saved_location_t where,
                                          smartlist_t *invalid_digests_out);

authority_cert_t *authority_cert_parse_from_string(const char *s,
                                                   const char **end_of_string);
int rend_parse_v2_service_descriptor(rend_service_descriptor_t **parsed_out,
                                     char *desc_id_out,
                                     char **intro_points_encrypted_out,
                                     size_t *intro_points_encrypted_size_out,
                                     size_t *encoded_size_out,
                                     const char **next_out, const char *desc,
                                     int as_hsdir);
int rend_decrypt_introduction_points(char **ipos_decrypted,
                                     size_t *ipos_decrypted_size,
                                     const char *descriptor_cookie,
                                     const char *ipos_encrypted,
                                     size_t ipos_encrypted_size);
int rend_parse_introduction_points(rend_service_descriptor_t *parsed,
                                   const char *intro_points_encoded,
                                   size_t intro_points_encoded_size);
int rend_parse_client_keys(strmap_t *parsed_clients, const char *str);

void routerparse_init(void);
void routerparse_free_all(void);

#ifdef ROUTERPARSE_PRIVATE
/*
 * One entry in the list of dumped descriptors; filename dumped to, length,
 * SHA-256 and timestamp.
 */

typedef struct {
  char *filename;
  size_t len;
  uint8_t digest_sha256[DIGEST256_LEN];
  time_t when;
} dumped_desc_t;

EXTERN(uint64_t, len_descs_dumped)
EXTERN(smartlist_t *, descs_dumped)
STATIC int routerstatus_parse_guardfraction(const char *guardfraction_str,
                                            networkstatus_t *vote,
                                            vote_routerstatus_t *vote_rs,
                                            routerstatus_t *rs);
MOCK_DECL(STATIC dumped_desc_t *, dump_desc_populate_one_file,
    (const char *dirname, const char *f));
STATIC void dump_desc_populate_fifo_from_directory(const char *dirname);
STATIC void dump_desc(const char *desc, const char *type);
STATIC void dump_desc_fifo_cleanup(void);
struct memarea_t;
STATIC routerstatus_t *routerstatus_parse_entry_from_string(
                                     struct memarea_t *area,
                                     const char **s, smartlist_t *tokens,
                                     networkstatus_t *vote,
                                     vote_routerstatus_t *vote_rs,
                                     int consensus_method,
                                     consensus_flavor_t flav);
#endif

#define ED_DESC_SIGNATURE_PREFIX "Tor router descriptor signature v1"

#endif

