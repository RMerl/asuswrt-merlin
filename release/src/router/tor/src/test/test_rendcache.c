/* Copyright (c) 2010-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include "or.h"

#include "test.h"
#define RENDCACHE_PRIVATE
#include "rendcache.h"
#include "router.h"
#include "routerlist.h"
#include "config.h"
#include <openssl/rsa.h>
#include "rend_test_helpers.h"
#include "log_test_helpers.h"

#define NS_MODULE rend_cache

static const int RECENT_TIME = -10;
static const int TIME_IN_THE_PAST = -(REND_CACHE_MAX_AGE + \
                                      REND_CACHE_MAX_SKEW + 60);
static const int TIME_IN_THE_FUTURE = REND_CACHE_MAX_SKEW + 60;

static rend_data_t *
mock_rend_data(const char *onion_address)
{
  rend_data_t *rend_query = tor_malloc_zero(sizeof(rend_data_t));

  strlcpy(rend_query->onion_address, onion_address,
          sizeof(rend_query->onion_address));
  rend_query->auth_type = REND_NO_AUTH;
  rend_query->hsdirs_fp = smartlist_new();
  smartlist_add(rend_query->hsdirs_fp, tor_memdup("aaaaaaaaaaaaaaaaaaaaaaaa",
                                                 DIGEST_LEN));

  return rend_query;
}

static void
test_rend_cache_lookup_entry(void *data)
{
  int ret;
  rend_data_t *mock_rend_query = NULL;
  char desc_id_base32[REND_DESC_ID_V2_LEN_BASE32 + 1];
  rend_cache_entry_t *entry = NULL;
  rend_encoded_v2_service_descriptor_t *desc_holder = NULL;
  char *service_id = NULL;
  (void)data;

  rend_cache_init();

  generate_desc(RECENT_TIME, &desc_holder, &service_id, 3);

  ret = rend_cache_lookup_entry("abababababababab", 0, NULL);
  tt_int_op(ret, OP_EQ, -ENOENT);

  ret = rend_cache_lookup_entry("invalid query", 2, NULL);
  tt_int_op(ret, OP_EQ, -EINVAL);

  ret = rend_cache_lookup_entry("abababababababab", 2, NULL);
  tt_int_op(ret, OP_EQ, -ENOENT);

  ret = rend_cache_lookup_entry("abababababababab", 4224, NULL);
  tt_int_op(ret, OP_EQ, -ENOENT);

  mock_rend_query = mock_rend_data(service_id);
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);
  rend_cache_store_v2_desc_as_client(desc_holder->desc_str, desc_id_base32,
                                     mock_rend_query, NULL);

  ret = rend_cache_lookup_entry(service_id, 2, NULL);
  tt_int_op(ret, OP_EQ, 0);

  ret = rend_cache_lookup_entry(service_id, 2, &entry);
  tt_assert(entry);
  tt_int_op(entry->len, OP_EQ, strlen(desc_holder->desc_str));
  tt_str_op(entry->desc, OP_EQ, desc_holder->desc_str);

 done:
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_cache_free_all();
  rend_data_free(mock_rend_query);
}

static void
test_rend_cache_store_v2_desc_as_client(void *data)
{
  int ret;
  rend_data_t *mock_rend_query;
  char desc_id_base32[REND_DESC_ID_V2_LEN_BASE32 + 1];
  rend_cache_entry_t *entry = NULL;
  rend_encoded_v2_service_descriptor_t *desc_holder = NULL;
  char *service_id = NULL;
  char client_cookie[REND_DESC_COOKIE_LEN];
  (void)data;

  rend_cache_init();

  generate_desc(RECENT_TIME, &desc_holder, &service_id, 3);

  // Test success
  mock_rend_query = mock_rend_data(service_id);
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);
  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           &entry);

  tt_int_op(ret, OP_EQ, 0);
  tt_assert(entry);
  tt_int_op(entry->len, OP_EQ, strlen(desc_holder->desc_str));
  tt_str_op(entry->desc, OP_EQ, desc_holder->desc_str);

  // Test various failure modes

  // TODO: a too long desc_id_base32 argument crashes the function
   /* ret = rend_cache_store_v2_desc_as_client( */
   /*                   desc_holder->desc_str, */
   /*                   "3TOOLONG3TOOLONG3TOOLONG3TOOLONG3TOOLONG3TOOLONG", */
   /*                   &mock_rend_query, NULL); */
  /* tt_int_op(ret, OP_EQ, -1); */

  // Test bad base32 failure
  // This causes an assertion failure if we're running with assertions.
  // But when building without asserts, we can test it.
#ifdef DISABLE_ASSERTS_IN_UNIT_TESTS
  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                   "!xqunszqnaolrrfmtzgaki7mxelgvkj", mock_rend_query, NULL);
  tt_int_op(ret, OP_EQ, -1);
#endif

  // Test invalid descriptor
  ret = rend_cache_store_v2_desc_as_client("invalid descriptor",
             "3xqunszqnaolrrfmtzgaki7mxelgvkje", mock_rend_query, NULL);
  tt_int_op(ret, OP_EQ, -1);

  // TODO: it doesn't seem to be possible to test invalid service ID condition.
  // that means it is likely not possible to have that condition without
  // earlier conditions failing first (such as signature checking of the desc)

  rend_cache_free_all();

  // Test mismatch between service ID and onion address
  rend_cache_init();
  strncpy(mock_rend_query->onion_address, "abc", REND_SERVICE_ID_LEN_BASE32+1);
  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32,
                                           mock_rend_query, NULL);
  tt_int_op(ret, OP_EQ, -1);
  rend_cache_free_all();
  rend_data_free(mock_rend_query);

  // Test incorrect descriptor ID
  rend_cache_init();
  mock_rend_query = mock_rend_data(service_id);
  desc_id_base32[0]++;
  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           NULL);
  tt_int_op(ret, OP_EQ, -1);
  desc_id_base32[0]--;
  rend_cache_free_all();

  // Test too old descriptor
  rend_cache_init();
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_data_free(mock_rend_query);

  generate_desc(TIME_IN_THE_PAST, &desc_holder, &service_id, 3);
  mock_rend_query = mock_rend_data(service_id);
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);

  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32,
                                           mock_rend_query, NULL);
  tt_int_op(ret, OP_EQ, -1);
  rend_cache_free_all();

  // Test too new descriptor (in the future)
  rend_cache_init();
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_data_free(mock_rend_query);

  generate_desc(TIME_IN_THE_FUTURE, &desc_holder, &service_id, 3);
  mock_rend_query = mock_rend_data(service_id);
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);

  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           NULL);
  tt_int_op(ret, OP_EQ, -1);
  rend_cache_free_all();

  // Test when a descriptor is already in the cache
  rend_cache_init();
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_data_free(mock_rend_query);

  generate_desc(RECENT_TIME, &desc_holder, &service_id, 3);
  mock_rend_query = mock_rend_data(service_id);
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);

  rend_cache_store_v2_desc_as_client(desc_holder->desc_str, desc_id_base32,
                                     mock_rend_query, NULL);
  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           NULL);
  tt_int_op(ret, OP_EQ, 0);

  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           &entry);
  tt_int_op(ret, OP_EQ, 0);
  tt_assert(entry);
  rend_cache_free_all();

  // Test unsuccessful decrypting of introduction points
  rend_cache_init();
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_data_free(mock_rend_query);

  generate_desc(RECENT_TIME, &desc_holder, &service_id, 3);
  mock_rend_query = mock_rend_data(service_id);
  mock_rend_query->auth_type = REND_BASIC_AUTH;
  client_cookie[0] = 'A';
  memcpy(mock_rend_query->descriptor_cookie, client_cookie,
         REND_DESC_COOKIE_LEN);
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);
  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           NULL);
  tt_int_op(ret, OP_EQ, 0);
  rend_cache_free_all();

  // Test successful run when we have REND_BASIC_AUTH but not cookie
  rend_cache_init();
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_data_free(mock_rend_query);

  generate_desc(RECENT_TIME, &desc_holder, &service_id, 3);
  mock_rend_query = mock_rend_data(service_id);
  mock_rend_query->auth_type = REND_BASIC_AUTH;
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);
  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           NULL);
  tt_int_op(ret, OP_EQ, 0);

  rend_cache_free_all();

  // Test when we have no introduction points
  rend_cache_init();
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_data_free(mock_rend_query);

  generate_desc(RECENT_TIME, &desc_holder, &service_id, 0);
  mock_rend_query = mock_rend_data(service_id);
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);
  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           NULL);
  tt_int_op(ret, OP_EQ, -1);
  rend_cache_free_all();

  // Test when we have too many intro points
  rend_cache_init();
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_data_free(mock_rend_query);

  generate_desc(RECENT_TIME, &desc_holder, &service_id, MAX_INTRO_POINTS+1);
  mock_rend_query = mock_rend_data(service_id);
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);
  ret = rend_cache_store_v2_desc_as_client(desc_holder->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           NULL);
  tt_int_op(ret, OP_EQ, -1);

 done:
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_cache_free_all();
  rend_data_free(mock_rend_query);
}

static void
test_rend_cache_store_v2_desc_as_client_with_different_time(void *data)
{
  int ret;
  rend_data_t *mock_rend_query;
  char desc_id_base32[REND_DESC_ID_V2_LEN_BASE32 + 1];
  rend_service_descriptor_t *generated = NULL;
  smartlist_t *descs = smartlist_new();
  time_t t;
  char *service_id = NULL;
  rend_encoded_v2_service_descriptor_t *desc_holder_newer;
  rend_encoded_v2_service_descriptor_t *desc_holder_older;

  t = time(NULL);
  rend_cache_init();

  create_descriptor(&generated, &service_id, 3);

  generated->timestamp = t + RECENT_TIME;
  rend_encode_v2_descriptors(descs, generated, t + RECENT_TIME, 0,
                             REND_NO_AUTH, NULL, NULL);
  desc_holder_newer = ((rend_encoded_v2_service_descriptor_t *)
                       smartlist_get(descs, 0));
  smartlist_set(descs, 0, NULL);

  SMARTLIST_FOREACH(descs, rend_encoded_v2_service_descriptor_t *, d,
                    rend_encoded_v2_service_descriptor_free(d));
  smartlist_free(descs);
  descs = smartlist_new();

  generated->timestamp = (t + RECENT_TIME) - 20;
  rend_encode_v2_descriptors(descs, generated, t + RECENT_TIME, 0,
                             REND_NO_AUTH, NULL, NULL);
  desc_holder_older = ((rend_encoded_v2_service_descriptor_t *)
                       smartlist_get(descs, 0));
  smartlist_set(descs, 0, NULL);
  (void)data;

  // Test when a descriptor is already in the cache and it is newer than the
  // one we submit
  mock_rend_query = mock_rend_data(service_id);
  base32_encode(desc_id_base32, sizeof(desc_id_base32),
                desc_holder_newer->desc_id, DIGEST_LEN);
  rend_cache_store_v2_desc_as_client(desc_holder_newer->desc_str,
                                     desc_id_base32, mock_rend_query, NULL);
  ret = rend_cache_store_v2_desc_as_client(desc_holder_older->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           NULL);
  tt_int_op(ret, OP_EQ, 0);

  rend_cache_free_all();

  // Test when an old descriptor is in the cache and we submit a newer one
  rend_cache_init();
  rend_cache_store_v2_desc_as_client(desc_holder_older->desc_str,
                                     desc_id_base32, mock_rend_query, NULL);
  ret = rend_cache_store_v2_desc_as_client(desc_holder_newer->desc_str,
                                           desc_id_base32, mock_rend_query,
                                           NULL);
  tt_int_op(ret, OP_EQ, 0);

 done:
  rend_encoded_v2_service_descriptor_free(desc_holder_newer);
  rend_encoded_v2_service_descriptor_free(desc_holder_older);
  SMARTLIST_FOREACH(descs, rend_encoded_v2_service_descriptor_t *, d,
                    rend_encoded_v2_service_descriptor_free(d));
  smartlist_free(descs);
  rend_service_descriptor_free(generated);
  tor_free(service_id);
  rend_cache_free_all();
  rend_data_free(mock_rend_query);
}

#define NS_SUBMODULE lookup_v2_desc_as_dir
NS_DECL(const routerinfo_t *, router_get_my_routerinfo, (void));

static routerinfo_t *mock_routerinfo;

static const routerinfo_t *
NS(router_get_my_routerinfo)(void)
{
  if (!mock_routerinfo) {
    mock_routerinfo = tor_malloc(sizeof(routerinfo_t));
  }

  return mock_routerinfo;
}

static void
test_rend_cache_lookup_v2_desc_as_dir(void *data)
{
  int ret;
  char desc_id_base32[REND_DESC_ID_V2_LEN_BASE32 + 1];
  rend_encoded_v2_service_descriptor_t *desc_holder = NULL;
  char *service_id = NULL;
  const char *ret_desc = NULL;

  (void)data;

  NS_MOCK(router_get_my_routerinfo);

  rend_cache_init();

  // Test invalid base32
  ret = rend_cache_lookup_v2_desc_as_dir("!bababababababab", NULL);
  tt_int_op(ret, OP_EQ, -1);

  // Test non-existent descriptor but well formed
  ret = rend_cache_lookup_v2_desc_as_dir("3xqunszqnaolrrfmtzgaki7mxelgvkje",
                                         NULL);
  tt_int_op(ret, OP_EQ, 0);

  // Test existing descriptor
  generate_desc(RECENT_TIME, &desc_holder, &service_id, 3);
  rend_cache_store_v2_desc_as_dir(desc_holder->desc_str);
  base32_encode(desc_id_base32, sizeof(desc_id_base32), desc_holder->desc_id,
                DIGEST_LEN);
  ret = rend_cache_lookup_v2_desc_as_dir(desc_id_base32, &ret_desc);
  tt_int_op(ret, OP_EQ, 1);
  tt_assert(ret_desc);

 done:
  NS_UNMOCK(router_get_my_routerinfo);
  tor_free(mock_routerinfo);
  rend_cache_free_all();
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
}

#undef NS_SUBMODULE

#define NS_SUBMODULE store_v2_desc_as_dir
NS_DECL(const routerinfo_t *, router_get_my_routerinfo, (void));

static const routerinfo_t *
NS(router_get_my_routerinfo)(void)
{
  return mock_routerinfo;
}

static void
test_rend_cache_store_v2_desc_as_dir(void *data)
{
  (void)data;
  int ret;
  rend_encoded_v2_service_descriptor_t *desc_holder = NULL;
  char *service_id = NULL;

  NS_MOCK(router_get_my_routerinfo);

  rend_cache_init();

  // Test when we can't parse the descriptor
  mock_routerinfo = tor_malloc(sizeof(routerinfo_t));
  ret = rend_cache_store_v2_desc_as_dir("unparseable");
  tt_int_op(ret, OP_EQ, -1);

  // Test when we have an old descriptor
  generate_desc(TIME_IN_THE_PAST, &desc_holder, &service_id, 3);
  ret = rend_cache_store_v2_desc_as_dir(desc_holder->desc_str);
  tt_int_op(ret, OP_EQ, 0);

  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);

  // Test when we have a descriptor in the future
  generate_desc(TIME_IN_THE_FUTURE, &desc_holder, &service_id, 3);
  ret = rend_cache_store_v2_desc_as_dir(desc_holder->desc_str);
  tt_int_op(ret, OP_EQ, 0);

  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);

  // Test when two descriptors
  generate_desc(TIME_IN_THE_FUTURE, &desc_holder, &service_id, 3);
  ret = rend_cache_store_v2_desc_as_dir(desc_holder->desc_str);
  tt_int_op(ret, OP_EQ, 0);

  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);

  // Test when asking for hidden service statistics  HiddenServiceStatistics
  rend_cache_purge();
  generate_desc(RECENT_TIME, &desc_holder, &service_id, 3);
  get_options_mutable()->HiddenServiceStatistics = 1;
  ret = rend_cache_store_v2_desc_as_dir(desc_holder->desc_str);
  tt_int_op(ret, OP_EQ, 0);

 done:
  NS_UNMOCK(router_get_my_routerinfo);
  rend_encoded_v2_service_descriptor_free(desc_holder);
  tor_free(service_id);
  rend_cache_free_all();
  tor_free(mock_routerinfo);
}

static void
test_rend_cache_store_v2_desc_as_dir_with_different_time(void *data)
{
  (void)data;

  int ret;
  rend_service_descriptor_t *generated = NULL;
  smartlist_t *descs = smartlist_new();
  time_t t;
  char *service_id = NULL;
  rend_encoded_v2_service_descriptor_t *desc_holder_newer;
  rend_encoded_v2_service_descriptor_t *desc_holder_older;

  NS_MOCK(router_get_my_routerinfo);

  rend_cache_init();

  t = time(NULL);

  create_descriptor(&generated, &service_id, 3);
  generated->timestamp = t + RECENT_TIME;
  rend_encode_v2_descriptors(descs, generated, t + RECENT_TIME, 0,
                             REND_NO_AUTH, NULL, NULL);
  desc_holder_newer = ((rend_encoded_v2_service_descriptor_t *)
                       smartlist_get(descs, 0));
  smartlist_set(descs, 0, NULL);
  SMARTLIST_FOREACH(descs, rend_encoded_v2_service_descriptor_t *, d,
                    rend_encoded_v2_service_descriptor_free(d));
  smartlist_free(descs);
  descs = smartlist_new();

  generated->timestamp = (t + RECENT_TIME) - 20;
  rend_encode_v2_descriptors(descs, generated, t + RECENT_TIME, 0,
                             REND_NO_AUTH, NULL, NULL);
  desc_holder_older = ((rend_encoded_v2_service_descriptor_t *)
                       smartlist_get(descs, 0));
  smartlist_set(descs, 0, NULL);

  // Test when we have a newer descriptor stored
  mock_routerinfo = tor_malloc(sizeof(routerinfo_t));
  rend_cache_store_v2_desc_as_dir(desc_holder_newer->desc_str);
  ret = rend_cache_store_v2_desc_as_dir(desc_holder_older->desc_str);
  tt_int_op(ret, OP_EQ, 0);

  // Test when we have an old descriptor stored
  rend_cache_purge();
  rend_cache_store_v2_desc_as_dir(desc_holder_older->desc_str);
  ret = rend_cache_store_v2_desc_as_dir(desc_holder_newer->desc_str);
  tt_int_op(ret, OP_EQ, 0);

 done:
  NS_UNMOCK(router_get_my_routerinfo);
  rend_cache_free_all();
  rend_service_descriptor_free(generated);
  tor_free(service_id);
  SMARTLIST_FOREACH(descs, rend_encoded_v2_service_descriptor_t *, d,
                    rend_encoded_v2_service_descriptor_free(d));
  smartlist_free(descs);
  rend_encoded_v2_service_descriptor_free(desc_holder_newer);
  rend_encoded_v2_service_descriptor_free(desc_holder_older);
  tor_free(mock_routerinfo);
}

static void
test_rend_cache_store_v2_desc_as_dir_with_different_content(void *data)
{
  (void)data;

  int ret;
  rend_service_descriptor_t *generated = NULL;
  smartlist_t *descs = smartlist_new();
  time_t t;
  char *service_id = NULL;
  rend_encoded_v2_service_descriptor_t *desc_holder_one = NULL;
  rend_encoded_v2_service_descriptor_t *desc_holder_two = NULL;

  NS_MOCK(router_get_my_routerinfo);

  rend_cache_init();

  t = time(NULL);

  create_descriptor(&generated, &service_id, 3);
  generated->timestamp = t + RECENT_TIME;
  rend_encode_v2_descriptors(descs, generated, t + RECENT_TIME, 0,
                             REND_NO_AUTH, NULL, NULL);
  desc_holder_one = ((rend_encoded_v2_service_descriptor_t *)
                     smartlist_get(descs, 0));
  smartlist_set(descs, 0, NULL);

  SMARTLIST_FOREACH(descs, rend_encoded_v2_service_descriptor_t *, d,
                    rend_encoded_v2_service_descriptor_free(d));
  smartlist_free(descs);
  descs = smartlist_new();

  generated->timestamp = t + RECENT_TIME;
  generated->protocols = 41;
  rend_encode_v2_descriptors(descs, generated, t + RECENT_TIME, 0,
                             REND_NO_AUTH, NULL, NULL);
  desc_holder_two = ((rend_encoded_v2_service_descriptor_t *)
                     smartlist_get(descs, 0));
  smartlist_set(descs, 0, NULL);

  // Test when we have another descriptor stored, with a different descriptor
  mock_routerinfo = tor_malloc(sizeof(routerinfo_t));
  rend_cache_store_v2_desc_as_dir(desc_holder_one->desc_str);
  ret = rend_cache_store_v2_desc_as_dir(desc_holder_two->desc_str);
  tt_int_op(ret, OP_EQ, 0);

 done:
  NS_UNMOCK(router_get_my_routerinfo);
  rend_cache_free_all();
  rend_service_descriptor_free(generated);
  tor_free(service_id);
  SMARTLIST_FOREACH(descs, rend_encoded_v2_service_descriptor_t *, d,
                    rend_encoded_v2_service_descriptor_free(d));
  smartlist_free(descs);
  rend_encoded_v2_service_descriptor_free(desc_holder_one);
  rend_encoded_v2_service_descriptor_free(desc_holder_two);
}

#undef NS_SUBMODULE

static void
test_rend_cache_init(void *data)
{
  (void)data;

  tt_assert_msg(!rend_cache, "rend_cache should be NULL when starting");
  tt_assert_msg(!rend_cache_v2_dir, "rend_cache_v2_dir should be NULL "
                "when starting");
  tt_assert_msg(!rend_cache_failure, "rend_cache_failure should be NULL when "
                "starting");

  rend_cache_init();

  tt_assert_msg(rend_cache, "rend_cache should not be NULL after initing");
  tt_assert_msg(rend_cache_v2_dir, "rend_cache_v2_dir should not be NULL "
                "after initing");
  tt_assert_msg(rend_cache_failure, "rend_cache_failure should not be NULL "
                "after initing");

  tt_int_op(strmap_size(rend_cache), OP_EQ, 0);
  tt_int_op(digestmap_size(rend_cache_v2_dir), OP_EQ, 0);
  tt_int_op(strmap_size(rend_cache_failure), OP_EQ, 0);

 done:
  rend_cache_free_all();
}

static void
test_rend_cache_decrement_allocation(void *data)
{
  (void)data;

  // Test when the cache has enough allocations
  rend_cache_total_allocation = 10;
  rend_cache_decrement_allocation(3);
  tt_int_op(rend_cache_total_allocation, OP_EQ, 7);

  // Test when there are not enough allocations
  rend_cache_total_allocation = 1;
  setup_full_capture_of_logs(LOG_WARN);
  rend_cache_decrement_allocation(2);
  tt_int_op(rend_cache_total_allocation, OP_EQ, 0);
  expect_single_log_msg_containing(
                    "Underflow in rend_cache_decrement_allocation");
  teardown_capture_of_logs();

  // And again
  rend_cache_decrement_allocation(2);
  tt_int_op(rend_cache_total_allocation, OP_EQ, 0);

 done:
  teardown_capture_of_logs();
}

static void
test_rend_cache_increment_allocation(void *data)
{
  (void)data;

  // Test when the cache is not overflowing
  rend_cache_total_allocation = 5;
  rend_cache_increment_allocation(3);
  tt_int_op(rend_cache_total_allocation, OP_EQ, 8);

  // Test when there are too many allocations
  rend_cache_total_allocation = SIZE_MAX-1;
  setup_full_capture_of_logs(LOG_WARN);
  rend_cache_increment_allocation(2);
  tt_u64_op(rend_cache_total_allocation, OP_EQ, SIZE_MAX);
  expect_single_log_msg_containing(
                    "Overflow in rend_cache_increment_allocation");
  teardown_capture_of_logs();

  // And again
  rend_cache_increment_allocation(2);
  tt_u64_op(rend_cache_total_allocation, OP_EQ, SIZE_MAX);

 done:
  teardown_capture_of_logs();
}

static void
test_rend_cache_failure_intro_entry_new(void *data)
{
  time_t now;
  rend_cache_failure_intro_t *entry;
  rend_intro_point_failure_t failure;

  (void)data;

  failure = INTRO_POINT_FAILURE_TIMEOUT;
  now = time(NULL);
  entry = rend_cache_failure_intro_entry_new(failure);

  tt_int_op(entry->failure_type, OP_EQ, INTRO_POINT_FAILURE_TIMEOUT);
  tt_int_op(entry->created_ts, OP_GE, now-5);
  tt_int_op(entry->created_ts, OP_LE, now+5);

 done:
  tor_free(entry);
}

static void
test_rend_cache_failure_intro_lookup(void *data)
{
  (void)data;
  int ret;
  rend_cache_failure_t *failure;
  rend_cache_failure_intro_t *ip;
  rend_cache_failure_intro_t *entry;
  const char key_ip_one[DIGEST_LEN] = "ip1";
  const char key_ip_two[DIGEST_LEN] = "ip2";
  const char key_foo[DIGEST_LEN] = "foo1";

  rend_cache_init();

  failure = rend_cache_failure_entry_new();
  ip = rend_cache_failure_intro_entry_new(INTRO_POINT_FAILURE_TIMEOUT);
  digestmap_set(failure->intro_failures, key_ip_one, ip);
  strmap_set_lc(rend_cache_failure, "foo1", failure);

  // Test not found
  ret = cache_failure_intro_lookup((const uint8_t *) key_foo, "foo2", NULL);
  tt_int_op(ret, OP_EQ, 0);

  // Test found with no intro failures in it
  ret = cache_failure_intro_lookup((const uint8_t *) key_ip_two, "foo1", NULL);
  tt_int_op(ret, OP_EQ, 0);

  // Test found
  ret = cache_failure_intro_lookup((const uint8_t *) key_ip_one, "foo1", NULL);
  tt_int_op(ret, OP_EQ, 1);

  // Test found and asking for entry
  cache_failure_intro_lookup((const uint8_t *) key_ip_one, "foo1", &entry);
  tt_assert(entry);
  tt_assert(entry == ip);

 done:
  rend_cache_free_all();
}

static void
test_rend_cache_clean(void *data)
{
  rend_cache_entry_t *one, *two;
  rend_service_descriptor_t *desc_one, *desc_two;
  strmap_iter_t *iter = NULL;
  const char *key;
  void *val;

  (void)data;

  rend_cache_init();

  // Test with empty rendcache
  rend_cache_clean(time(NULL), REND_CACHE_TYPE_CLIENT);
  tt_int_op(strmap_size(rend_cache), OP_EQ, 0);

  // Test with two old entries
  one = tor_malloc_zero(sizeof(rend_cache_entry_t));
  two = tor_malloc_zero(sizeof(rend_cache_entry_t));
  desc_one = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  desc_two = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  one->parsed = desc_one;
  two->parsed = desc_two;

  desc_one->timestamp = time(NULL) + TIME_IN_THE_PAST;
  desc_two->timestamp = (time(NULL) + TIME_IN_THE_PAST) - 10;
  desc_one->pk = pk_generate(0);
  desc_two->pk = pk_generate(1);

  strmap_set_lc(rend_cache, "foo1", one);
  strmap_set_lc(rend_cache, "foo2", two);

  rend_cache_clean(time(NULL), REND_CACHE_TYPE_CLIENT);
  tt_int_op(strmap_size(rend_cache), OP_EQ, 0);

  // Test with one old entry and one newer entry
  one = tor_malloc_zero(sizeof(rend_cache_entry_t));
  two = tor_malloc_zero(sizeof(rend_cache_entry_t));
  desc_one = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  desc_two = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  one->parsed = desc_one;
  two->parsed = desc_two;

  desc_one->timestamp = (time(NULL) + TIME_IN_THE_PAST) - 10;
  desc_two->timestamp = time(NULL) - 100;
  desc_one->pk = pk_generate(0);
  desc_two->pk = pk_generate(1);

  strmap_set_lc(rend_cache, "foo1", one);
  strmap_set_lc(rend_cache, "foo2", two);

  rend_cache_clean(time(NULL), REND_CACHE_TYPE_CLIENT);
  tt_int_op(strmap_size(rend_cache), OP_EQ, 1);

  iter = strmap_iter_init(rend_cache);
  strmap_iter_get(iter, &key, &val);
  tt_str_op(key, OP_EQ, "foo2");

 done:
  rend_cache_free_all();
}

static void
test_rend_cache_failure_entry_new(void *data)
{
  rend_cache_failure_t *failure;

  (void)data;

  failure = rend_cache_failure_entry_new();
  tt_assert(failure);
  tt_int_op(digestmap_size(failure->intro_failures), OP_EQ, 0);

 done:
  rend_cache_failure_entry_free(failure);
}

static void
test_rend_cache_failure_entry_free(void *data)
{
  (void)data;

  // Test that it can deal with a NULL argument
  rend_cache_failure_entry_free(NULL);

 /* done: */
 /*  (void)0; */
}

static void
test_rend_cache_failure_clean(void *data)
{
  rend_cache_failure_t *failure;
  rend_cache_failure_intro_t *ip_one, *ip_two;

  const char key_one[DIGEST_LEN] = "ip1";
  const char key_two[DIGEST_LEN] = "ip2";

  (void)data;

  rend_cache_init();

  // Test with empty failure cache
  rend_cache_failure_clean(time(NULL));
  tt_int_op(strmap_size(rend_cache_failure), OP_EQ, 0);

  // Test with one empty failure entry
  failure = rend_cache_failure_entry_new();
  strmap_set_lc(rend_cache_failure, "foo1", failure);
  rend_cache_failure_clean(time(NULL));
  tt_int_op(strmap_size(rend_cache_failure), OP_EQ, 0);

  // Test with one new intro point
  failure = rend_cache_failure_entry_new();
  ip_one = rend_cache_failure_intro_entry_new(INTRO_POINT_FAILURE_TIMEOUT);
  digestmap_set(failure->intro_failures, key_one, ip_one);
  strmap_set_lc(rend_cache_failure, "foo1", failure);
  rend_cache_failure_clean(time(NULL));
  tt_int_op(strmap_size(rend_cache_failure), OP_EQ, 1);

  // Test with one old intro point
  rend_cache_failure_purge();
  failure = rend_cache_failure_entry_new();
  ip_one = rend_cache_failure_intro_entry_new(INTRO_POINT_FAILURE_TIMEOUT);
  ip_one->created_ts = time(NULL) - 7*60;
  digestmap_set(failure->intro_failures, key_one, ip_one);
  strmap_set_lc(rend_cache_failure, "foo1", failure);
  rend_cache_failure_clean(time(NULL));
  tt_int_op(strmap_size(rend_cache_failure), OP_EQ, 0);

  // Test with one old intro point and one new one
  rend_cache_failure_purge();
  failure = rend_cache_failure_entry_new();
  ip_one = rend_cache_failure_intro_entry_new(INTRO_POINT_FAILURE_TIMEOUT);
  ip_one->created_ts = time(NULL) - 7*60;
  digestmap_set(failure->intro_failures, key_one, ip_one);
  ip_two = rend_cache_failure_intro_entry_new(INTRO_POINT_FAILURE_TIMEOUT);
  ip_two->created_ts = time(NULL) - 2*60;
  digestmap_set(failure->intro_failures, key_two, ip_two);
  strmap_set_lc(rend_cache_failure, "foo1", failure);
  rend_cache_failure_clean(time(NULL));
  tt_int_op(strmap_size(rend_cache_failure), OP_EQ, 1);
  tt_int_op(digestmap_size(failure->intro_failures), OP_EQ, 1);

 done:
  rend_cache_free_all();
}

static void
test_rend_cache_failure_remove(void *data)
{
  rend_service_descriptor_t *desc;
  (void)data;

  rend_cache_init();

  // Test that it deals well with a NULL desc
  rend_cache_failure_remove(NULL);

  // Test a descriptor that isn't in the cache
  desc = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  desc->pk = pk_generate(0);
  rend_cache_failure_remove(desc);

  // There seems to not exist any way of getting rend_cache_failure_remove()
  // to fail because of a problem with rend_get_service_id from here
  rend_cache_free_all();

  rend_service_descriptor_free(desc);
 /* done: */
 /*  (void)0; */
}

static void
test_rend_cache_free_all(void *data)
{
  rend_cache_failure_t *failure;
  rend_cache_entry_t *one;
  rend_service_descriptor_t *desc_one;

  (void)data;

  rend_cache_init();

  failure = rend_cache_failure_entry_new();
  strmap_set_lc(rend_cache_failure, "foo1", failure);

  one = tor_malloc_zero(sizeof(rend_cache_entry_t));
  desc_one = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  one->parsed = desc_one;
  desc_one->timestamp = time(NULL) + TIME_IN_THE_PAST;
  desc_one->pk = pk_generate(0);
  strmap_set_lc(rend_cache, "foo1", one);

  rend_cache_free_all();

  tt_assert(!rend_cache);
  tt_assert(!rend_cache_v2_dir);
  tt_assert(!rend_cache_failure);
  tt_assert(!rend_cache_total_allocation);

 done:
  rend_cache_free_all();
}

static void
test_rend_cache_entry_free(void *data)
{
  (void)data;
  rend_cache_entry_t *e;

  // Handles NULL correctly
  rend_cache_entry_free(NULL);

  // Handles NULL descriptor correctly
  e = tor_malloc_zero(sizeof(rend_cache_entry_t));
  rend_cache_entry_free(e);

  // Handles non-NULL descriptor correctly
  e = tor_malloc_zero(sizeof(rend_cache_entry_t));
  e->desc = tor_malloc(10);
  rend_cache_entry_free(e);

 /* done: */
 /*  (void)0; */
}

static void
test_rend_cache_purge(void *data)
{
  (void)data;

  // Deals with a NULL rend_cache
  rend_cache_purge();
  tt_assert(rend_cache);
  tt_assert(strmap_size(rend_cache) == 0);

  // Deals with existing rend_cache
  rend_cache_free_all();
  rend_cache_init();
  tt_assert(rend_cache);
  tt_assert(strmap_size(rend_cache) == 0);

  rend_cache_purge();
  tt_assert(rend_cache);
  tt_assert(strmap_size(rend_cache) == 0);

 done:
  rend_cache_free_all();
}

static void
test_rend_cache_failure_intro_add(void *data)
{
  (void)data;
  rend_cache_failure_t *fail_entry;
  rend_cache_failure_intro_t *entry;
  const char identity[DIGEST_LEN] = "foo1";

  rend_cache_init();

  // Adds non-existing entry
  cache_failure_intro_add((const uint8_t *) identity, "foo2",
                          INTRO_POINT_FAILURE_TIMEOUT);
  fail_entry = strmap_get_lc(rend_cache_failure, "foo2");
  tt_assert(fail_entry);
  tt_int_op(digestmap_size(fail_entry->intro_failures), OP_EQ, 1);
  entry = digestmap_get(fail_entry->intro_failures, identity);
  tt_assert(entry);

  // Adds existing entry
  cache_failure_intro_add((const uint8_t *) identity, "foo2",
                          INTRO_POINT_FAILURE_TIMEOUT);
  fail_entry = strmap_get_lc(rend_cache_failure, "foo2");
  tt_assert(fail_entry);
  tt_int_op(digestmap_size(fail_entry->intro_failures), OP_EQ, 1);
  entry = digestmap_get(fail_entry->intro_failures, identity);
  tt_assert(entry);

 done:
  rend_cache_free_all();
}

static void
test_rend_cache_intro_failure_note(void *data)
{
  (void)data;
  rend_cache_failure_t *fail_entry;
  rend_cache_failure_intro_t *entry;
  const char key[DIGEST_LEN] = "foo1";

  rend_cache_init();

  // Test not found
  rend_cache_intro_failure_note(INTRO_POINT_FAILURE_TIMEOUT,
                                (const uint8_t *) key, "foo2");
  fail_entry = strmap_get_lc(rend_cache_failure, "foo2");
  tt_assert(fail_entry);
  tt_int_op(digestmap_size(fail_entry->intro_failures), OP_EQ, 1);
  entry = digestmap_get(fail_entry->intro_failures, key);
  tt_assert(entry);
  tt_int_op(entry->failure_type, OP_EQ, INTRO_POINT_FAILURE_TIMEOUT);

  // Test found
  rend_cache_intro_failure_note(INTRO_POINT_FAILURE_UNREACHABLE,
                                (const uint8_t *) key, "foo2");
  tt_int_op(entry->failure_type, OP_EQ, INTRO_POINT_FAILURE_UNREACHABLE);

 done:
  rend_cache_free_all();
}

#define NS_SUBMODULE clean_v2_descs_as_dir

static void
test_rend_cache_clean_v2_descs_as_dir(void *data)
{
  rend_cache_entry_t *e;
  time_t now;
  rend_service_descriptor_t *desc;
  now = time(NULL);
  const char key[DIGEST_LEN] = "abcde";

  (void)data;

  rend_cache_init();

  // Test running with an empty cache
  rend_cache_clean_v2_descs_as_dir(now, 0);
  tt_int_op(digestmap_size(rend_cache_v2_dir), OP_EQ, 0);

  // Test with only one new entry
  e = tor_malloc_zero(sizeof(rend_cache_entry_t));
  e->last_served = now;
  desc = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  desc->timestamp = now;
  desc->pk = pk_generate(0);
  e->parsed = desc;
  digestmap_set(rend_cache_v2_dir, key, e);

  rend_cache_clean_v2_descs_as_dir(now, 0);
  tt_int_op(digestmap_size(rend_cache_v2_dir), OP_EQ, 1);

  // Test with one old entry
  desc->timestamp = now - (REND_CACHE_MAX_AGE + REND_CACHE_MAX_SKEW + 1000);
  rend_cache_clean_v2_descs_as_dir(now, 0);
  tt_int_op(digestmap_size(rend_cache_v2_dir), OP_EQ, 0);

  // Test with one entry that has an old last served
  e = tor_malloc_zero(sizeof(rend_cache_entry_t));
  e->last_served = now - (REND_CACHE_MAX_AGE + REND_CACHE_MAX_SKEW + 1000);
  desc = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  desc->timestamp = now;
  desc->pk = pk_generate(0);
  e->parsed = desc;
  digestmap_set(rend_cache_v2_dir, key, e);

  rend_cache_clean_v2_descs_as_dir(now, 0);
  tt_int_op(digestmap_size(rend_cache_v2_dir), OP_EQ, 0);

  // Test a run through asking for a large force_remove
  e = tor_malloc_zero(sizeof(rend_cache_entry_t));
  e->last_served = now;
  desc = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  desc->timestamp = now;
  desc->pk = pk_generate(0);
  e->parsed = desc;
  digestmap_set(rend_cache_v2_dir, key, e);

  rend_cache_clean_v2_descs_as_dir(now, 20000);
  tt_int_op(digestmap_size(rend_cache_v2_dir), OP_EQ, 1);

 done:
  rend_cache_free_all();
}

#undef NS_SUBMODULE

static void
test_rend_cache_entry_allocation(void *data)
{
  (void)data;

  size_t ret;
  rend_cache_entry_t *e = NULL;

  // Handles a null argument
  ret = rend_cache_entry_allocation(NULL);
  tt_int_op(ret, OP_EQ, 0);

  // Handles a non-null argument
  e = tor_malloc_zero(sizeof(rend_cache_entry_t));
  ret = rend_cache_entry_allocation(e);
  tt_int_op(ret, OP_GT, sizeof(rend_cache_entry_t));

 done:
  tor_free(e);
}

static void
test_rend_cache_failure_intro_entry_free(void *data)
{
  (void)data;
  rend_cache_failure_intro_t *entry;

  // Handles a null argument
  rend_cache_failure_intro_entry_free(NULL);

  // Handles a non-null argument
  entry = rend_cache_failure_intro_entry_new(INTRO_POINT_FAILURE_TIMEOUT);
  rend_cache_failure_intro_entry_free(entry);
}

static void
test_rend_cache_failure_purge(void *data)
{
  (void)data;

  // Handles a null failure cache
  strmap_free(rend_cache_failure, rend_cache_failure_entry_free_);
  rend_cache_failure = NULL;

  rend_cache_failure_purge();

  tt_ptr_op(rend_cache_failure, OP_NE, NULL);
  tt_int_op(strmap_size(rend_cache_failure), OP_EQ, 0);

 done:
  rend_cache_free_all();
}

static void
test_rend_cache_validate_intro_point_failure(void *data)
{
  (void)data;
  rend_service_descriptor_t *desc = NULL;
  char *service_id = NULL;
  rend_intro_point_t *intro = NULL;
  const char *identity = NULL;
  rend_cache_failure_t *failure;
  rend_cache_failure_intro_t *ip;

  rend_cache_init();

  create_descriptor(&desc, &service_id, 3);
  desc->timestamp = time(NULL) + RECENT_TIME;

  intro = (rend_intro_point_t *)smartlist_get(desc->intro_nodes, 0);
  identity = intro->extend_info->identity_digest;

  failure = rend_cache_failure_entry_new();
  ip = rend_cache_failure_intro_entry_new(INTRO_POINT_FAILURE_TIMEOUT);
  digestmap_set(failure->intro_failures, identity, ip);
  strmap_set_lc(rend_cache_failure, service_id, failure);

  // Test when we have an intro point in our cache
  validate_intro_point_failure(desc, service_id);
  tt_int_op(smartlist_len(desc->intro_nodes), OP_EQ, 2);

 done:
  rend_cache_free_all();
  rend_service_descriptor_free(desc);
  tor_free(service_id);
}

struct testcase_t rend_cache_tests[] = {
  { "init", test_rend_cache_init, 0, NULL, NULL },
  { "decrement_allocation", test_rend_cache_decrement_allocation, 0,
    NULL, NULL },
  { "increment_allocation", test_rend_cache_increment_allocation, 0,
    NULL, NULL },
  { "clean", test_rend_cache_clean, TT_FORK, NULL, NULL },
  { "clean_v2_descs_as_dir", test_rend_cache_clean_v2_descs_as_dir, 0,
    NULL, NULL },
  { "entry_allocation", test_rend_cache_entry_allocation, 0, NULL, NULL },
  { "entry_free", test_rend_cache_entry_free, 0, NULL, NULL },
  { "failure_intro_entry_free", test_rend_cache_failure_intro_entry_free, 0,
    NULL, NULL },
  { "free_all", test_rend_cache_free_all, 0, NULL, NULL },
  { "purge", test_rend_cache_purge, 0, NULL, NULL },
  { "failure_clean", test_rend_cache_failure_clean, 0, NULL, NULL },
  { "failure_entry_new", test_rend_cache_failure_entry_new, 0, NULL, NULL },
  { "failure_entry_free", test_rend_cache_failure_entry_free, 0, NULL, NULL },
  { "failure_intro_add", test_rend_cache_failure_intro_add, 0, NULL, NULL },
  { "failure_intro_entry_new", test_rend_cache_failure_intro_entry_new, 0,
    NULL, NULL },
  { "failure_intro_lookup", test_rend_cache_failure_intro_lookup, 0,
    NULL, NULL },
  { "failure_purge", test_rend_cache_failure_purge, 0, NULL, NULL },
  { "failure_remove", test_rend_cache_failure_remove, 0, NULL, NULL },
  { "intro_failure_note", test_rend_cache_intro_failure_note, 0, NULL, NULL },
  { "lookup", test_rend_cache_lookup_entry, 0, NULL, NULL },
  { "lookup_v2_desc_as_dir", test_rend_cache_lookup_v2_desc_as_dir, 0,
    NULL, NULL },
  { "store_v2_desc_as_client", test_rend_cache_store_v2_desc_as_client, 0,
    NULL, NULL },
  { "store_v2_desc_as_client_with_different_time",
    test_rend_cache_store_v2_desc_as_client_with_different_time, 0,
    NULL, NULL },
  { "store_v2_desc_as_dir", test_rend_cache_store_v2_desc_as_dir, 0,
    NULL, NULL },
  { "store_v2_desc_as_dir_with_different_time",
    test_rend_cache_store_v2_desc_as_dir_with_different_time, 0, NULL, NULL },
  { "store_v2_desc_as_dir_with_different_content",
    test_rend_cache_store_v2_desc_as_dir_with_different_content, 0,
    NULL, NULL },
  { "validate_intro_point_failure",
    test_rend_cache_validate_intro_point_failure, 0, NULL, NULL },
  END_OF_TESTCASES
};

