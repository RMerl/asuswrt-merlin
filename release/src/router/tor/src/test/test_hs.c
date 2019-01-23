/* Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_hs.c
 * \brief Unit tests for hidden service.
 **/

#define CONTROL_PRIVATE
#define CIRCUITBUILD_PRIVATE
#define RENDSERVICE_PRIVATE

#include "or.h"
#include "test.h"
#include "control.h"
#include "config.h"
#include "rendcommon.h"
#include "rendservice.h"
#include "routerset.h"
#include "circuitbuild.h"
#include "test_helpers.h"

/* mock ID digest and longname for node that's in nodelist */
#define HSDIR_EXIST_ID "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA" \
                       "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
#define STR_HSDIR_EXIST_LONGNAME \
                       "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=TestDir"
/* mock ID digest and longname for node that's not in nodelist */
#define HSDIR_NONE_EXIST_ID "\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB" \
                            "\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB"
#define STR_HSDIR_NONE_EXIST_LONGNAME \
                       "$BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"

/* DuckDuckGo descriptor as an example. */
static const char *hs_desc_content = "\
rendezvous-service-descriptor g5ojobzupf275beh5ra72uyhb3dkpxwg\r\n\
version 2\r\n\
permanent-key\r\n\
-----BEGIN RSA PUBLIC KEY-----\r\n\
MIGJAoGBAJ/SzzgrXPxTlFrKVhXh3buCWv2QfcNgncUpDpKouLn3AtPH5Ocys0jE\r\n\
aZSKdvaiQ62md2gOwj4x61cFNdi05tdQjS+2thHKEm/KsB9BGLSLBNJYY356bupg\r\n\
I5gQozM65ENelfxYlysBjJ52xSDBd8C4f/p9umdzaaaCmzXG/nhzAgMBAAE=\r\n\
-----END RSA PUBLIC KEY-----\r\n\
secret-id-part anmjoxxwiupreyajjt5yasimfmwcnxlf\r\n\
publication-time 2015-03-11 19:00:00\r\n\
protocol-versions 2,3\r\n\
introduction-points\r\n\
-----BEGIN MESSAGE-----\r\n\
aW50cm9kdWN0aW9uLXBvaW50IDd1bnd4cmg2dG5kNGh6eWt1Z3EzaGZzdHduc2ll\r\n\
cmhyCmlwLWFkZHJlc3MgMTg4LjEzOC4xMjEuMTE4Cm9uaW9uLXBvcnQgOTAwMQpv\r\n\
bmlvbi1rZXkKLS0tLS1CRUdJTiBSU0EgUFVCTElDIEtFWS0tLS0tCk1JR0pBb0dC\r\n\
QUxGRVVyeVpDbk9ROEhURmV5cDVjMTRObWVqL1BhekFLTTBxRENTNElKUWh0Y3g1\r\n\
NXpRSFdOVWIKQ2hHZ0JqR1RjV3ZGRnA0N3FkdGF6WUZhVXE2c0lQKzVqeWZ5b0Q4\r\n\
UmJ1bzBwQmFWclJjMmNhYUptWWM0RDh6Vgpuby9sZnhzOVVaQnZ1cWY4eHIrMDB2\r\n\
S0JJNmFSMlA2OE1WeDhrMExqcUpUU2RKOE9idm9yQWdNQkFBRT0KLS0tLS1FTkQg\r\n\
UlNBIFBVQkxJQyBLRVktLS0tLQpzZXJ2aWNlLWtleQotLS0tLUJFR0lOIFJTQSBQ\r\n\
VUJMSUMgS0VZLS0tLS0KTUlHSkFvR0JBTnJHb0ozeTlHNXQzN2F2ekI1cTlwN1hG\r\n\
VUplRUVYMUNOaExnWmJXWGJhVk5OcXpoZFhyL0xTUQppM1Z6dW5OaUs3cndUVnE2\r\n\
K2QyZ1lRckhMMmIvMXBBY3ZKWjJiNSs0bTRRc0NibFpjRENXTktRbHJnRWN5WXRJ\r\n\
CkdscXJTbFFEaXA0ZnNrUFMvNDVkWTI0QmJsQ3NGU1k3RzVLVkxJck4zZFpGbmJr\r\n\
NEZIS1hBZ01CQUFFPQotLS0tLUVORCBSU0EgUFVCTElDIEtFWS0tLS0tCmludHJv\r\n\
ZHVjdGlvbi1wb2ludCBiNGM3enlxNXNheGZzN2prNXFibG1wN3I1b3pwdHRvagpp\r\n\
cC1hZGRyZXNzIDEwOS4xNjkuNDUuMjI2Cm9uaW9uLXBvcnQgOTAwMQpvbmlvbi1r\r\n\
ZXkKLS0tLS1CRUdJTiBSU0EgUFVCTElDIEtFWS0tLS0tCk1JR0pBb0dCQU8xSXpw\r\n\
WFFUTUY3RXZUb1NEUXpzVnZiRVFRQUQrcGZ6NzczMVRXZzVaUEJZY1EyUkRaeVp4\r\n\
OEQKNUVQSU1FeUE1RE83cGd0ak5LaXJvYXJGMC8yempjMkRXTUlSaXZyU29YUWVZ\r\n\
ZXlMM1pzKzFIajJhMDlCdkYxZAp6MEswblRFdVhoNVR5V3lyMHdsbGI1SFBnTlI0\r\n\
MS9oYkprZzkwZitPVCtIeGhKL1duUml2QWdNQkFBRT0KLS0tLS1FTkQgUlNBIFBV\r\n\
QkxJQyBLRVktLS0tLQpzZXJ2aWNlLWtleQotLS0tLUJFR0lOIFJTQSBQVUJMSUMg\r\n\
S0VZLS0tLS0KTUlHSkFvR0JBSzNWZEJ2ajFtQllLL3JrcHNwcm9Ub0llNUtHVmth\r\n\
QkxvMW1tK1I2YUVJek1VZFE1SjkwNGtyRwpCd3k5NC8rV0lGNFpGYXh5Z2phejl1\r\n\
N2pKY1k3ZGJhd1pFeG1hYXFCRlRwL2h2ZG9rcHQ4a1ByRVk4OTJPRHJ1CmJORUox\r\n\
N1FPSmVMTVZZZk5Kcjl4TWZCQ3JQai8zOGh2RUdrbWVRNmRVWElvbVFNaUJGOVRB\r\n\
Z01CQUFFPQotLS0tLUVORCBSU0EgUFVCTElDIEtFWS0tLS0tCmludHJvZHVjdGlv\r\n\
bi1wb2ludCBhdjVtcWl0Y2Q3cjJkandsYmN0c2Jlc2R3eGt0ZWtvegppcC1hZGRy\r\n\
ZXNzIDE0NC43Ni44LjczCm9uaW9uLXBvcnQgNDQzCm9uaW9uLWtleQotLS0tLUJF\r\n\
R0lOIFJTQSBQVUJMSUMgS0VZLS0tLS0KTUlHSkFvR0JBTzVweVZzQmpZQmNmMXBE\r\n\
dklHUlpmWXUzQ05nNldka0ZLMGlvdTBXTGZtejZRVDN0NWhzd3cyVwpjejlHMXhx\r\n\
MmN0Nkd6VWkrNnVkTDlITTRVOUdHTi9BbW8wRG9GV1hKWHpBQkFXd2YyMVdsd1lW\r\n\
eFJQMHRydi9WCkN6UDkzcHc5OG5vSmdGUGRUZ05iMjdKYmVUZENLVFBrTEtscXFt\r\n\
b3NveUN2RitRa25vUS9BZ01CQUFFPQotLS0tLUVORCBSU0EgUFVCTElDIEtFWS0t\r\n\
LS0tCnNlcnZpY2Uta2V5Ci0tLS0tQkVHSU4gUlNBIFBVQkxJQyBLRVktLS0tLQpN\r\n\
SUdKQW9HQkFMVjNKSmtWN3lTNU9jc1lHMHNFYzFQOTVRclFRR3ZzbGJ6Wi9zRGxl\r\n\
RlpKYXFSOUYvYjRUVERNClNGcFMxcU1GbldkZDgxVmRGMEdYRmN2WVpLamRJdHU2\r\n\
SndBaTRJeEhxeXZtdTRKdUxrcXNaTEFLaXRLVkx4eGsKeERlMjlDNzRWMmJrOTRJ\r\n\
MEgybTNKS2tzTHVwc3VxWWRVUmhOVXN0SElKZmgyZmNIalF0bEFnTUJBQUU9Ci0t\r\n\
LS0tRU5EIFJTQSBQVUJMSUMgS0VZLS0tLS0KCg==\r\n\
-----END MESSAGE-----\r\n\
signature\r\n\
-----BEGIN SIGNATURE-----\r\n\
d4OuCE5OLAOnRB6cQN6WyMEmg/BHem144Vec+eYgeWoKwx3MxXFplUjFxgnMlmwN\r\n\
PcftsZf2ztN0sbNCtPgDL3d0PqvxY3iHTQAI8EbaGq/IAJUZ8U4y963dD5+Bn6JQ\r\n\
myE3ctmh0vy5+QxSiRjmQBkuEpCyks7LvWvHYrhnmcg=\r\n\
-----END SIGNATURE-----";

/* Helper global variable for hidden service descriptor event test.
 * It's used as a pointer to dynamically created message buffer in
 * send_control_event_string_replacement function, which mocks
 * send_control_event_string function.
 *
 * Always free it after use! */
static char *received_msg = NULL;

/** Mock function for send_control_event_string
 */
static void
queue_control_event_string_replacement(uint16_t event, char *msg)
{
  (void) event;
  tor_free(received_msg);
  received_msg = msg;
}

/** Mock function for node_describe_longname_by_id, it returns either
 * STR_HSDIR_EXIST_LONGNAME or STR_HSDIR_NONE_EXIST_LONGNAME
 */
static const char *
node_describe_longname_by_id_replacement(const char *id_digest)
{
  if (!strcmp(id_digest, HSDIR_EXIST_ID)) {
    return STR_HSDIR_EXIST_LONGNAME;
  } else {
    return STR_HSDIR_NONE_EXIST_LONGNAME;
  }
}

/** Make sure each hidden service descriptor async event generation
 *
 * function generates the message in expected format.
 */
static void
test_hs_desc_event(void *arg)
{
  #define STR_HS_ADDR "ajhb7kljbiru65qo"
  #define STR_HS_CONTENT_DESC_ID "g5ojobzupf275beh5ra72uyhb3dkpxwg"
  #define STR_DESC_ID_BASE32 "hba3gmcgpfivzfhx5rtfqkfdhv65yrj3"

  int ret;
  rend_data_t rend_query;
  const char *expected_msg;
  char desc_id_base32[REND_DESC_ID_V2_LEN_BASE32 + 1];

  (void) arg;
  MOCK(queue_control_event_string,
       queue_control_event_string_replacement);
  MOCK(node_describe_longname_by_id,
       node_describe_longname_by_id_replacement);

  /* setup rend_query struct */
  memset(&rend_query, 0, sizeof(rend_query));
  strncpy(rend_query.onion_address, STR_HS_ADDR,
          REND_SERVICE_ID_LEN_BASE32+1);
  rend_query.auth_type = REND_NO_AUTH;
  rend_query.hsdirs_fp = smartlist_new();
  smartlist_add(rend_query.hsdirs_fp, tor_memdup(HSDIR_EXIST_ID,
                                                 DIGEST_LEN));

  /* Compute descriptor ID for replica 0, should be STR_DESC_ID_BASE32. */
  ret = rend_compute_v2_desc_id(rend_query.descriptor_id[0],
                                rend_query.onion_address,
                                NULL, 0, 0);
  tt_int_op(ret, ==, 0);
  base32_encode(desc_id_base32, sizeof(desc_id_base32),
                rend_query.descriptor_id[0], DIGEST_LEN);
  /* Make sure rend_compute_v2_desc_id works properly. */
  tt_mem_op(desc_id_base32, OP_EQ, STR_DESC_ID_BASE32,
            sizeof(desc_id_base32));

  /* test request event */
  control_event_hs_descriptor_requested(&rend_query, HSDIR_EXIST_ID,
                                        STR_DESC_ID_BASE32);
  expected_msg = "650 HS_DESC REQUESTED "STR_HS_ADDR" NO_AUTH "\
                  STR_HSDIR_EXIST_LONGNAME " " STR_DESC_ID_BASE32 "\r\n";
  tt_assert(received_msg);
  tt_str_op(received_msg,OP_EQ, expected_msg);
  tor_free(received_msg);

  /* test received event */
  rend_query.auth_type = REND_BASIC_AUTH;
  control_event_hs_descriptor_received(rend_query.onion_address,
                                       &rend_query, HSDIR_EXIST_ID);
  expected_msg = "650 HS_DESC RECEIVED "STR_HS_ADDR" BASIC_AUTH "\
                  STR_HSDIR_EXIST_LONGNAME " " STR_DESC_ID_BASE32"\r\n";
  tt_assert(received_msg);
  tt_str_op(received_msg,OP_EQ, expected_msg);
  tor_free(received_msg);

  /* test failed event */
  rend_query.auth_type = REND_STEALTH_AUTH;
  control_event_hs_descriptor_failed(&rend_query,
                                     HSDIR_NONE_EXIST_ID,
                                     "QUERY_REJECTED");
  expected_msg = "650 HS_DESC FAILED "STR_HS_ADDR" STEALTH_AUTH "\
                  STR_HSDIR_NONE_EXIST_LONGNAME" REASON=QUERY_REJECTED\r\n";
  tt_assert(received_msg);
  tt_str_op(received_msg,OP_EQ, expected_msg);
  tor_free(received_msg);

  /* test invalid auth type */
  rend_query.auth_type = 999;
  control_event_hs_descriptor_failed(&rend_query,
                                     HSDIR_EXIST_ID,
                                     "QUERY_REJECTED");
  expected_msg = "650 HS_DESC FAILED "STR_HS_ADDR" UNKNOWN "\
                  STR_HSDIR_EXIST_LONGNAME " " STR_DESC_ID_BASE32\
                  " REASON=QUERY_REJECTED\r\n";
  tt_assert(received_msg);
  tt_str_op(received_msg,OP_EQ, expected_msg);
  tor_free(received_msg);

  /* test valid content. */
  char *exp_msg;
  control_event_hs_descriptor_content(rend_query.onion_address,
                                      STR_HS_CONTENT_DESC_ID, HSDIR_EXIST_ID,
                                      hs_desc_content);
  tor_asprintf(&exp_msg, "650+HS_DESC_CONTENT " STR_HS_ADDR " "\
               STR_HS_CONTENT_DESC_ID " " STR_HSDIR_EXIST_LONGNAME\
               "\r\n%s\r\n.\r\n650 OK\r\n", hs_desc_content);

  tt_assert(received_msg);
  tt_str_op(received_msg, OP_EQ, exp_msg);
  tor_free(received_msg);
  tor_free(exp_msg);
  SMARTLIST_FOREACH(rend_query.hsdirs_fp, char *, d, tor_free(d));
  smartlist_free(rend_query.hsdirs_fp);

 done:
  UNMOCK(queue_control_event_string);
  UNMOCK(node_describe_longname_by_id);
  tor_free(received_msg);
}

/* Make sure we always pick the right RP, given a well formatted
 * Tor2webRendezvousPoints value. */
static void
test_pick_tor2web_rendezvous_node(void *arg)
{
  or_options_t *options = get_options_mutable();
  const node_t *chosen_rp = NULL;
  router_crn_flags_t flags = CRN_NEED_DESC;
  int retval, i;
  const char *tor2web_rendezvous_str = "test003r";

  (void) arg;

  /* Setup fake routerlist. */
  helper_setup_fake_routerlist();

  /* Parse Tor2webRendezvousPoints as a routerset. */
  options->Tor2webRendezvousPoints = routerset_new();
  retval = routerset_parse(options->Tor2webRendezvousPoints,
                           tor2web_rendezvous_str,
                           "test_tor2web_rp");
  tt_int_op(retval, >=, 0);

  /* Pick rendezvous point. Make sure the correct one is
     picked. Repeat many times to make sure it works properly. */
  for (i = 0; i < 50 ; i++) {
    chosen_rp = pick_tor2web_rendezvous_node(flags, options);
    tt_assert(chosen_rp);
    tt_str_op(chosen_rp->ri->nickname, ==, tor2web_rendezvous_str);
  }

 done:
  routerset_free(options->Tor2webRendezvousPoints);
}

/* Make sure we never pick an RP if Tor2webRendezvousPoints doesn't
 * correspond to an actual node. */
static void
test_pick_bad_tor2web_rendezvous_node(void *arg)
{
  or_options_t *options = get_options_mutable();
  const node_t *chosen_rp = NULL;
  router_crn_flags_t flags = CRN_NEED_DESC;
  int retval, i;
  const char *tor2web_rendezvous_str = "dummy";

  (void) arg;

  /* Setup fake routerlist. */
  helper_setup_fake_routerlist();

  /* Parse Tor2webRendezvousPoints as a routerset. */
  options->Tor2webRendezvousPoints = routerset_new();
  retval = routerset_parse(options->Tor2webRendezvousPoints,
                           tor2web_rendezvous_str,
                           "test_tor2web_rp");
  tt_int_op(retval, >=, 0);

  /* Pick rendezvous point. Since Tor2webRendezvousPoints was set to a
     dummy value, we shouldn't find any eligible RPs. */
  for (i = 0; i < 50 ; i++) {
    chosen_rp = pick_tor2web_rendezvous_node(flags, options);
    tt_assert(!chosen_rp);
  }

 done:
  routerset_free(options->Tor2webRendezvousPoints);
}

/* Make sure rend_data_t is valid at creation, destruction and when
 * duplicated. */
static void
test_hs_rend_data(void *arg)
{
  int rep;
  rend_data_t *client = NULL, *client_dup = NULL;
  /* Binary format of a descriptor ID. */
  char desc_id[DIGEST_LEN];
  char client_cookie[REND_DESC_COOKIE_LEN];
  time_t now = time(NULL);
  rend_data_t *service_dup = NULL;
  rend_data_t *service = NULL;

  (void)arg;

  base32_decode(desc_id, sizeof(desc_id), STR_DESC_ID_BASE32,
                REND_DESC_ID_V2_LEN_BASE32);
  memset(client_cookie, 'e', sizeof(client_cookie));

  client = rend_data_client_create(STR_HS_ADDR, desc_id, client_cookie,
                                   REND_NO_AUTH);
  tt_assert(client);
  tt_int_op(client->auth_type, ==, REND_NO_AUTH);
  tt_str_op(client->onion_address, OP_EQ, STR_HS_ADDR);
  tt_mem_op(client->desc_id_fetch, OP_EQ, desc_id, sizeof(desc_id));
  tt_mem_op(client->descriptor_cookie, OP_EQ, client_cookie,
            sizeof(client_cookie));
  tt_assert(client->hsdirs_fp);
  tt_int_op(smartlist_len(client->hsdirs_fp), ==, 0);
  for (rep = 0; rep < REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS; rep++) {
    int ret = rend_compute_v2_desc_id(desc_id, client->onion_address,
                                      client->descriptor_cookie, now, rep);
    /* That shouldn't never fail. */
    tt_int_op(ret, ==, 0);
    tt_mem_op(client->descriptor_id[rep], OP_EQ, desc_id, sizeof(desc_id));
  }
  /* The rest should be zeroed because this is a client request. */
  tt_int_op(tor_digest_is_zero(client->rend_pk_digest), ==, 1);
  tt_int_op(tor_digest_is_zero(client->rend_cookie), ==, 1);

  /* Test dup(). */
  client_dup = rend_data_dup(client);
  tt_assert(client_dup);
  tt_int_op(client_dup->auth_type, ==, client->auth_type);
  tt_str_op(client_dup->onion_address, OP_EQ, client->onion_address);
  tt_mem_op(client_dup->desc_id_fetch, OP_EQ, client->desc_id_fetch,
            sizeof(client_dup->desc_id_fetch));
  tt_mem_op(client_dup->descriptor_cookie, OP_EQ, client->descriptor_cookie,
            sizeof(client_dup->descriptor_cookie));

  tt_assert(client_dup->hsdirs_fp);
  tt_int_op(smartlist_len(client_dup->hsdirs_fp), ==, 0);
  for (rep = 0; rep < REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS; rep++) {
    tt_mem_op(client_dup->descriptor_id[rep], OP_EQ,
              client->descriptor_id[rep], DIGEST_LEN);
  }
  /* The rest should be zeroed because this is a client request. */
  tt_int_op(tor_digest_is_zero(client_dup->rend_pk_digest), ==, 1);
  tt_int_op(tor_digest_is_zero(client_dup->rend_cookie), ==, 1);
  rend_data_free(client);
  client = NULL;
  rend_data_free(client_dup);
  client_dup = NULL;

  /* Reset state. */
  base32_decode(desc_id, sizeof(desc_id), STR_DESC_ID_BASE32,
                REND_DESC_ID_V2_LEN_BASE32);
  memset(client_cookie, 'e', sizeof(client_cookie));

  /* Try with different parameters here for which some content should be
   * zeroed out. */
  client = rend_data_client_create(NULL, desc_id, NULL, REND_BASIC_AUTH);
  tt_assert(client);
  tt_int_op(client->auth_type, ==, REND_BASIC_AUTH);
  tt_int_op(strlen(client->onion_address), ==, 0);
  tt_mem_op(client->desc_id_fetch, OP_EQ, desc_id, sizeof(desc_id));
  tt_int_op(tor_mem_is_zero(client->descriptor_cookie,
                            sizeof(client->descriptor_cookie)), ==, 1);
  tt_assert(client->hsdirs_fp);
  tt_int_op(smartlist_len(client->hsdirs_fp), ==, 0);
  for (rep = 0; rep < REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS; rep++) {
    tt_int_op(tor_digest_is_zero(client->descriptor_id[rep]), ==, 1);
  }
  /* The rest should be zeroed because this is a client request. */
  tt_int_op(tor_digest_is_zero(client->rend_pk_digest), ==, 1);
  tt_int_op(tor_digest_is_zero(client->rend_cookie), ==, 1);
  rend_data_free(client);
  client = NULL;

  /* Let's test the service object now. */
  char rend_pk_digest[DIGEST_LEN];
  uint8_t rend_cookie[DIGEST_LEN];
  memset(rend_pk_digest, 'f', sizeof(rend_pk_digest));
  memset(rend_cookie, 'g', sizeof(rend_cookie));

  service = rend_data_service_create(STR_HS_ADDR, rend_pk_digest,
                                     rend_cookie, REND_NO_AUTH);
  tt_assert(service);
  tt_int_op(service->auth_type, ==, REND_NO_AUTH);
  tt_str_op(service->onion_address, OP_EQ, STR_HS_ADDR);
  tt_mem_op(service->rend_pk_digest, OP_EQ, rend_pk_digest,
            sizeof(rend_pk_digest));
  tt_mem_op(service->rend_cookie, OP_EQ, rend_cookie, sizeof(rend_cookie));
  tt_assert(service->hsdirs_fp);
  tt_int_op(smartlist_len(service->hsdirs_fp), ==, 0);
  for (rep = 0; rep < REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS; rep++) {
    tt_int_op(tor_digest_is_zero(service->descriptor_id[rep]), ==, 1);
  }
  /* The rest should be zeroed because this is a service request. */
  tt_int_op(tor_digest_is_zero(service->descriptor_cookie), ==, 1);
  tt_int_op(tor_digest_is_zero(service->desc_id_fetch), ==, 1);

  /* Test dup(). */
  service_dup = rend_data_dup(service);
  tt_assert(service_dup);
  tt_int_op(service_dup->auth_type, ==, service->auth_type);
  tt_str_op(service_dup->onion_address, OP_EQ, service->onion_address);
  tt_mem_op(service_dup->rend_pk_digest, OP_EQ, service->rend_pk_digest,
            sizeof(service_dup->rend_pk_digest));
  tt_mem_op(service_dup->rend_cookie, OP_EQ, service->rend_cookie,
            sizeof(service_dup->rend_cookie));
  tt_assert(service_dup->hsdirs_fp);
  tt_int_op(smartlist_len(service_dup->hsdirs_fp), ==, 0);
  for (rep = 0; rep < REND_NUMBER_OF_NON_CONSECUTIVE_REPLICAS; rep++) {
    tt_int_op(tor_digest_is_zero(service_dup->descriptor_id[rep]), ==, 1);
  }
  /* The rest should be zeroed because this is a service request. */
  tt_int_op(tor_digest_is_zero(service_dup->descriptor_cookie), ==, 1);
  tt_int_op(tor_digest_is_zero(service_dup->desc_id_fetch), ==, 1);

 done:
  rend_data_free(service);
  rend_data_free(service_dup);
  rend_data_free(client);
  rend_data_free(client_dup);
}

/* Test encoding and decoding service authorization cookies */
static void
test_hs_auth_cookies(void *arg)
{
#define TEST_COOKIE_RAW ((const uint8_t *) "abcdefghijklmnop")
#define TEST_COOKIE_ENCODED "YWJjZGVmZ2hpamtsbW5vcA"
#define TEST_COOKIE_ENCODED_STEALTH "YWJjZGVmZ2hpamtsbW5vcB"
#define TEST_COOKIE_ENCODED_INVALID "YWJjZGVmZ2hpamtsbW5vcD"

  char *encoded_cookie;
  uint8_t raw_cookie[REND_DESC_COOKIE_LEN];
  rend_auth_type_t auth_type;
  char *err_msg;
  int re;

  (void)arg;

  /* Test that encoding gives the expected result */
  encoded_cookie = rend_auth_encode_cookie(TEST_COOKIE_RAW, REND_BASIC_AUTH);
  tt_str_op(encoded_cookie, OP_EQ, TEST_COOKIE_ENCODED);
  tor_free(encoded_cookie);

  encoded_cookie = rend_auth_encode_cookie(TEST_COOKIE_RAW, REND_STEALTH_AUTH);
  tt_str_op(encoded_cookie, OP_EQ, TEST_COOKIE_ENCODED_STEALTH);
  tor_free(encoded_cookie);

  /* Decoding should give the original value */
  re = rend_auth_decode_cookie(TEST_COOKIE_ENCODED, raw_cookie, &auth_type,
                               &err_msg);
  tt_assert(!re);
  tt_assert(!err_msg);
  tt_mem_op(raw_cookie, OP_EQ, TEST_COOKIE_RAW, REND_DESC_COOKIE_LEN);
  tt_int_op(auth_type, OP_EQ, REND_BASIC_AUTH);
  memset(raw_cookie, 0, sizeof(raw_cookie));

  re = rend_auth_decode_cookie(TEST_COOKIE_ENCODED_STEALTH, raw_cookie,
                               &auth_type, &err_msg);
  tt_assert(!re);
  tt_assert(!err_msg);
  tt_mem_op(raw_cookie, OP_EQ, TEST_COOKIE_RAW, REND_DESC_COOKIE_LEN);
  tt_int_op(auth_type, OP_EQ, REND_STEALTH_AUTH);
  memset(raw_cookie, 0, sizeof(raw_cookie));

  /* Decoding with padding characters should also work */
  re = rend_auth_decode_cookie(TEST_COOKIE_ENCODED "==", raw_cookie, NULL,
                               &err_msg);
  tt_assert(!re);
  tt_assert(!err_msg);
  tt_mem_op(raw_cookie, OP_EQ, TEST_COOKIE_RAW, REND_DESC_COOKIE_LEN);

  /* Decoding with an unknown type should fail */
  re = rend_auth_decode_cookie(TEST_COOKIE_ENCODED_INVALID, raw_cookie,
                               &auth_type, &err_msg);
  tt_int_op(re, OP_LT, 0);
  tt_assert(err_msg);
  tor_free(err_msg);

 done:
  return;
}

static int mock_get_options_calls = 0;
static or_options_t *mock_options = NULL;

static void
reset_options(or_options_t *options, int *get_options_calls)
{
  memset(options, 0, sizeof(or_options_t));
  options->TestingTorNetwork = 1;

  *get_options_calls = 0;
}

static const or_options_t *
mock_get_options(void)
{
  ++mock_get_options_calls;
  tor_assert(mock_options);
  return mock_options;
}

/* arg can't be 0 (the test fails) or 2 (the test is skipped) */
#define CREATE_HS_DIR_NONE ((intptr_t)0x04)
#define CREATE_HS_DIR1     ((intptr_t)0x08)
#define CREATE_HS_DIR2     ((intptr_t)0x10)

/* Test that single onion poisoning works. */
static void
test_single_onion_poisoning(void *arg)
{
  or_options_t opt;
  mock_options = &opt;
  reset_options(mock_options, &mock_get_options_calls);
  MOCK(get_options, mock_get_options);

  int ret = -1;
  intptr_t create_dir_mask = (intptr_t)arg;
  /* Get directories with a random suffix so we can repeat the tests */
  mock_options->DataDirectory = tor_strdup(get_fname_rnd("test_data_dir"));
  rend_service_t *service_1 = tor_malloc_zero(sizeof(rend_service_t));
  char *dir1 = tor_strdup(get_fname_rnd("test_hs_dir1"));
  rend_service_t *service_2 = tor_malloc_zero(sizeof(rend_service_t));
  char *dir2 = tor_strdup(get_fname_rnd("test_hs_dir2"));
  smartlist_t *services = smartlist_new();
  char *poison_path = NULL;

  /* No services, no service to verify, no problem! */
  mock_options->HiddenServiceSingleHopMode = 0;
  mock_options->HiddenServiceNonAnonymousMode = 0;
  ret = rend_config_services(mock_options, 1);
  tt_assert(ret == 0);

  /* Either way, no problem. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_config_services(mock_options, 1);
  tt_assert(ret == 0);

  /* Create the data directory, and, if the correct bit in arg is set,
   * create a directory for that service.
   * The data directory is required for the lockfile, which is used when
   * loading keys. */
  ret = check_private_dir(mock_options->DataDirectory, CPD_CREATE, NULL);
  tt_assert(ret == 0);
  if (create_dir_mask & CREATE_HS_DIR1) {
    ret = check_private_dir(dir1, CPD_CREATE, NULL);
    tt_assert(ret == 0);
  }
  if (create_dir_mask & CREATE_HS_DIR2) {
    ret = check_private_dir(dir2, CPD_CREATE, NULL);
    tt_assert(ret == 0);
  }

  service_1->directory = dir1;
  service_2->directory = dir2;
  /* The services own the directory pointers now */
  dir1 = dir2 = NULL;
  /* Add port to service 1 */
  service_1->ports = smartlist_new();
  service_2->ports = smartlist_new();
  char *err_msg = NULL;
  rend_service_port_config_t *port1 = rend_service_parse_port_config("80", " ",
                                                                     &err_msg);
  tt_assert(port1);
  tt_assert(!err_msg);
  smartlist_add(service_1->ports, port1);

  rend_service_port_config_t *port2 = rend_service_parse_port_config("90", " ",
                                                                     &err_msg);
  /* Add port to service 2 */
  tt_assert(port2);
  tt_assert(!err_msg);
  smartlist_add(service_2->ports, port2);

  /* No services, a service to verify, no problem! */
  mock_options->HiddenServiceSingleHopMode = 0;
  mock_options->HiddenServiceNonAnonymousMode = 0;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Either way, no problem. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Add the first service */
  ret = rend_service_check_dir_and_add(services, mock_options, service_1, 0);
  tt_assert(ret == 0);
  /* But don't add the second service yet. */

  /* Service directories, but no previous keys, no problem! */
  mock_options->HiddenServiceSingleHopMode = 0;
  mock_options->HiddenServiceNonAnonymousMode = 0;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Either way, no problem. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Poison! Poison! Poison!
   * This can only be done in HiddenServiceSingleHopMode. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_poison_new_single_onion_dir(service_1, mock_options);
  tt_assert(ret == 0);
  /* Poisoning twice is a no-op. */
  ret = rend_service_poison_new_single_onion_dir(service_1, mock_options);
  tt_assert(ret == 0);

  /* Poisoned service directories, but no previous keys, no problem! */
  mock_options->HiddenServiceSingleHopMode = 0;
  mock_options->HiddenServiceNonAnonymousMode = 0;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Either way, no problem. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Now add some keys, and we'll have a problem. */
  ret = rend_service_load_all_keys(services);
  tt_assert(ret == 0);

  /* Poisoned service directories with previous keys are not allowed. */
  mock_options->HiddenServiceSingleHopMode = 0;
  mock_options->HiddenServiceNonAnonymousMode = 0;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret < 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* But they are allowed if we're in non-anonymous mode. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Re-poisoning directories with existing keys is a no-op, because
   * directories with existing keys are ignored. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_poison_new_single_onion_dir(service_1, mock_options);
  tt_assert(ret == 0);
  /* And it keeps the poison. */
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Now add the second service: it has no key and no poison file */
  ret = rend_service_check_dir_and_add(services, mock_options, service_2, 0);
  tt_assert(ret == 0);

  /* A new service, and an existing poisoned service. Not ok. */
  mock_options->HiddenServiceSingleHopMode = 0;
  mock_options->HiddenServiceNonAnonymousMode = 0;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret < 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* But ok to add in non-anonymous mode. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Now remove the poisoning from the first service, and we have the opposite
   * problem. */
  poison_path = rend_service_sos_poison_path(service_1);
  tt_assert(poison_path);
  ret = unlink(poison_path);
  tt_assert(ret == 0);

  /* Unpoisoned service directories with previous keys are ok, as are empty
   * directories. */
  mock_options->HiddenServiceSingleHopMode = 0;
  mock_options->HiddenServiceNonAnonymousMode = 0;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* But the existing unpoisoned key is not ok in non-anonymous mode, even if
   * there is an empty service. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret < 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Poisoning directories with existing keys is a no-op, because directories
   * with existing keys are ignored. But the new directory should poison. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_poison_new_single_onion_dir(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_poison_new_single_onion_dir(service_2, mock_options);
  tt_assert(ret == 0);
  /* And the old directory remains unpoisoned. */
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret < 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* And the new directory should be ignored, because it has no key. */
  mock_options->HiddenServiceSingleHopMode = 0;
  mock_options->HiddenServiceNonAnonymousMode = 0;
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

  /* Re-poisoning directories without existing keys is a no-op. */
  mock_options->HiddenServiceSingleHopMode = 1;
  mock_options->HiddenServiceNonAnonymousMode = 1;
  ret = rend_service_poison_new_single_onion_dir(service_1, mock_options);
  tt_assert(ret == 0);
  ret = rend_service_poison_new_single_onion_dir(service_2, mock_options);
  tt_assert(ret == 0);
  /* And the old directory remains unpoisoned. */
  ret = rend_service_verify_single_onion_poison(service_1, mock_options);
  tt_assert(ret < 0);
  ret = rend_service_verify_single_onion_poison(service_2, mock_options);
  tt_assert(ret == 0);

 done:
  /* The test harness deletes the directories at exit */
  tor_free(poison_path);
  tor_free(dir1);
  tor_free(dir2);
  smartlist_free(services);
  rend_service_free(service_1);
  rend_service_free(service_2);
  UNMOCK(get_options);
  tor_free(mock_options->DataDirectory);
}

struct testcase_t hs_tests[] = {
  { "hs_rend_data", test_hs_rend_data, TT_FORK,
    NULL, NULL },
  { "hs_desc_event", test_hs_desc_event, TT_FORK,
    NULL, NULL },
  { "pick_tor2web_rendezvous_node", test_pick_tor2web_rendezvous_node, TT_FORK,
    NULL, NULL },
  { "pick_bad_tor2web_rendezvous_node",
    test_pick_bad_tor2web_rendezvous_node, TT_FORK,
    NULL, NULL },
  { "hs_auth_cookies", test_hs_auth_cookies, TT_FORK,
    NULL, NULL },
  { "single_onion_poisoning_create_dir_none", test_single_onion_poisoning,
    TT_FORK, &passthrough_setup, (void*)(CREATE_HS_DIR_NONE) },
  { "single_onion_poisoning_create_dir1", test_single_onion_poisoning,
    TT_FORK, &passthrough_setup, (void*)(CREATE_HS_DIR1) },
  { "single_onion_poisoning_create_dir2", test_single_onion_poisoning,
    TT_FORK, &passthrough_setup, (void*)(CREATE_HS_DIR2) },
  { "single_onion_poisoning_create_dir_both", test_single_onion_poisoning,
    TT_FORK, &passthrough_setup, (void*)(CREATE_HS_DIR1 | CREATE_HS_DIR2) },
  END_OF_TESTCASES
};

