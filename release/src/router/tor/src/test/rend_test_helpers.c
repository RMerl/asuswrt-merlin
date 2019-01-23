/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "or.h"
#include "test.h"
#include "rendcommon.h"
#include "rend_test_helpers.h"

void
generate_desc(int time_diff, rend_encoded_v2_service_descriptor_t **desc,
              char **service_id, int intro_points)
{
  rend_service_descriptor_t *generated = NULL;
  smartlist_t *descs = smartlist_new();
  time_t now;

  now = time(NULL) + time_diff;
  create_descriptor(&generated, service_id, intro_points);
  generated->timestamp = now;

  rend_encode_v2_descriptors(descs, generated, now, 0, REND_NO_AUTH, NULL,
                             NULL);
  tor_assert(smartlist_len(descs) > 1);
  *desc = smartlist_get(descs, 0);
  smartlist_set(descs, 0, NULL);

  SMARTLIST_FOREACH(descs, rend_encoded_v2_service_descriptor_t *, d,
                    rend_encoded_v2_service_descriptor_free(d));
  smartlist_free(descs);
  rend_service_descriptor_free(generated);
}

void
create_descriptor(rend_service_descriptor_t **generated, char **service_id,
                  int intro_points)
{
  crypto_pk_t *pk1 = NULL;
  crypto_pk_t *pk2 = NULL;
  int i;

  *service_id = tor_malloc(REND_SERVICE_ID_LEN_BASE32+1);
  pk1 = pk_generate(0);
  pk2 = pk_generate(1);

  *generated = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  (*generated)->pk = crypto_pk_dup_key(pk1);
  rend_get_service_id((*generated)->pk, *service_id);

  (*generated)->version = 2;
  (*generated)->protocols = 42;
  (*generated)->intro_nodes = smartlist_new();

  for (i = 0; i < intro_points; i++) {
    rend_intro_point_t *intro = tor_malloc_zero(sizeof(rend_intro_point_t));
    crypto_pk_t *okey = pk_generate(2 + i);
    intro->extend_info = tor_malloc_zero(sizeof(extend_info_t));
    intro->extend_info->onion_key = okey;
    crypto_pk_get_digest(intro->extend_info->onion_key,
                         intro->extend_info->identity_digest);
    intro->extend_info->nickname[0] = '$';
    base16_encode(intro->extend_info->nickname + 1,
                  sizeof(intro->extend_info->nickname) - 1,
                  intro->extend_info->identity_digest, DIGEST_LEN);
    tor_addr_from_ipv4h(&intro->extend_info->addr, crypto_rand_int(65536));
    intro->extend_info->port = 1 + crypto_rand_int(65535);
    intro->intro_key = crypto_pk_dup_key(pk2);
    smartlist_add((*generated)->intro_nodes, intro);
  }

  crypto_pk_free(pk1);
  crypto_pk_free(pk2);
}

