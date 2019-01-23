/* Copyright (c) 2015-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define CONTROL_PRIVATE
#include "or.h"
#include "control.h"
#include "entrynodes.h"
#include "networkstatus.h"
#include "rendservice.h"
#include "routerlist.h"
#include "test.h"

static void
test_add_onion_helper_keyarg(void *arg)
{
  crypto_pk_t *pk = NULL;
  crypto_pk_t *pk2 = NULL;
  const char *key_new_alg = NULL;
  char *key_new_blob = NULL;
  char *err_msg = NULL;
  char *encoded = NULL;
  char *arg_str = NULL;

  (void) arg;

  /* Test explicit RSA1024 key generation. */
  pk = add_onion_helper_keyarg("NEW:RSA1024", 0, &key_new_alg, &key_new_blob,
                               &err_msg);
  tt_assert(pk);
  tt_str_op(key_new_alg, OP_EQ, "RSA1024");
  tt_assert(key_new_blob);
  tt_assert(!err_msg);

  /* Test "BEST" key generation (Assumes BEST = RSA1024). */
  crypto_pk_free(pk);
  tor_free(key_new_blob);
  pk = add_onion_helper_keyarg("NEW:BEST", 0, &key_new_alg, &key_new_blob,
                               &err_msg);
  tt_assert(pk);
  tt_str_op(key_new_alg, OP_EQ, "RSA1024");
  tt_assert(key_new_blob);
  tt_assert(!err_msg);

  /* Test discarding the private key. */
  crypto_pk_free(pk);
  tor_free(key_new_blob);
  pk = add_onion_helper_keyarg("NEW:BEST", 1, &key_new_alg, &key_new_blob,
                               &err_msg);
  tt_assert(pk);
  tt_assert(!key_new_alg);
  tt_assert(!key_new_blob);
  tt_assert(!err_msg);

  /* Test generating a invalid key type. */
  crypto_pk_free(pk);
  pk = add_onion_helper_keyarg("NEW:RSA512", 0, &key_new_alg, &key_new_blob,
                               &err_msg);
  tt_assert(!pk);
  tt_assert(!key_new_alg);
  tt_assert(!key_new_blob);
  tt_assert(err_msg);

  /* Test loading a RSA1024 key. */
  tor_free(err_msg);
  pk = pk_generate(0);
  tt_int_op(0, OP_EQ, crypto_pk_base64_encode(pk, &encoded));
  tor_asprintf(&arg_str, "RSA1024:%s", encoded);
  pk2 = add_onion_helper_keyarg(arg_str, 0, &key_new_alg, &key_new_blob,
                                &err_msg);
  tt_assert(pk2);
  tt_assert(!key_new_alg);
  tt_assert(!key_new_blob);
  tt_assert(!err_msg);
  tt_assert(crypto_pk_cmp_keys(pk, pk2) == 0);

  /* Test loading a invalid key type. */
  tor_free(arg_str);
  crypto_pk_free(pk); pk = NULL;
  tor_asprintf(&arg_str, "RSA512:%s", encoded);
  pk = add_onion_helper_keyarg(arg_str, 0, &key_new_alg, &key_new_blob,
                               &err_msg);
  tt_assert(!pk);
  tt_assert(!key_new_alg);
  tt_assert(!key_new_blob);
  tt_assert(err_msg);

  /* Test loading a invalid key. */
  tor_free(arg_str);
  crypto_pk_free(pk); pk = NULL;
  tor_free(err_msg);
  encoded[strlen(encoded)/2] = '\0';
  tor_asprintf(&arg_str, "RSA1024:%s", encoded);
  pk = add_onion_helper_keyarg(arg_str, 0, &key_new_alg, &key_new_blob,
                               &err_msg);
  tt_assert(!pk);
  tt_assert(!key_new_alg);
  tt_assert(!key_new_blob);
  tt_assert(err_msg);

 done:
  crypto_pk_free(pk);
  crypto_pk_free(pk2);
  tor_free(key_new_blob);
  tor_free(err_msg);
  tor_free(encoded);
  tor_free(arg_str);
}

static void
test_rend_service_parse_port_config(void *arg)
{
  const char *sep = ",";
  rend_service_port_config_t *cfg = NULL;
  char *err_msg = NULL;

  (void)arg;

  /* Test "VIRTPORT" only. */
  cfg = rend_service_parse_port_config("80", sep, &err_msg);
  tt_assert(cfg);
  tt_assert(!err_msg);

  /* Test "VIRTPORT,TARGET" (Target is port). */
  rend_service_port_config_free(cfg);
  cfg = rend_service_parse_port_config("80,8080", sep, &err_msg);
  tt_assert(cfg);
  tt_assert(!err_msg);

  /* Test "VIRTPORT,TARGET" (Target is IPv4:port). */
  rend_service_port_config_free(cfg);
  cfg = rend_service_parse_port_config("80,192.0.2.1:8080", sep, &err_msg);
  tt_assert(cfg);
  tt_assert(!err_msg);

  /* Test "VIRTPORT,TARGET" (Target is IPv6:port). */
  rend_service_port_config_free(cfg);
  cfg = rend_service_parse_port_config("80,[2001:db8::1]:8080", sep, &err_msg);
  tt_assert(cfg);
  tt_assert(!err_msg);
  rend_service_port_config_free(cfg);
  cfg = NULL;

  /* XXX: Someone should add tests for AF_UNIX targets if supported. */

  /* Test empty config. */
  rend_service_port_config_free(cfg);
  cfg = rend_service_parse_port_config("", sep, &err_msg);
  tt_assert(!cfg);
  tt_assert(err_msg);

  /* Test invalid port. */
  tor_free(err_msg);
  cfg = rend_service_parse_port_config("90001", sep, &err_msg);
  tt_assert(!cfg);
  tt_assert(err_msg);
  tor_free(err_msg);

  /* unix port */
  cfg = NULL;

  /* quoted unix port */
  tor_free(err_msg);
  cfg = rend_service_parse_port_config("100 unix:\"/tmp/foo bar\"",
                                       " ", &err_msg);
  tt_assert(cfg);
  tt_assert(!err_msg);
  rend_service_port_config_free(cfg);
  cfg = NULL;

  /* quoted unix port */
  tor_free(err_msg);
  cfg = rend_service_parse_port_config("100 unix:\"/tmp/foo bar\"",
                                       " ", &err_msg);
  tt_assert(cfg);
  tt_assert(!err_msg);
  rend_service_port_config_free(cfg);
  cfg = NULL;

  /* quoted unix port, missing end quote */
  cfg = rend_service_parse_port_config("100 unix:\"/tmp/foo bar",
                                       " ", &err_msg);
  tt_assert(!cfg);
  tt_str_op(err_msg, OP_EQ, "Couldn't process address <unix:\"/tmp/foo bar> "
            "from hidden service configuration");
  tor_free(err_msg);

  /* bogus IP address */
  cfg = rend_service_parse_port_config("100 1.2.3.4.5:9000",
                                       " ", &err_msg);
  tt_assert(!cfg);
  tt_str_op(err_msg, OP_EQ, "Unparseable address in hidden service port "
            "configuration.");
  tor_free(err_msg);

  /* bogus port port */
  cfg = rend_service_parse_port_config("100 99999",
                                       " ", &err_msg);
  tt_assert(!cfg);
  tt_str_op(err_msg, OP_EQ, "Unparseable or out-of-range port \"99999\" "
            "in hidden service port configuration.");
  tor_free(err_msg);

 done:
  rend_service_port_config_free(cfg);
  tor_free(err_msg);
}

static void
test_add_onion_helper_clientauth(void *arg)
{
  rend_authorized_client_t *client = NULL;
  char *err_msg = NULL;
  int created = 0;

  (void)arg;

  /* Test "ClientName" only. */
  client = add_onion_helper_clientauth("alice", &created, &err_msg);
  tt_assert(client);
  tt_assert(created);
  tt_assert(!err_msg);
  rend_authorized_client_free(client);

  /* Test "ClientName:Blob" */
  client = add_onion_helper_clientauth("alice:475hGBHPlq7Mc0cRZitK/B",
                                       &created, &err_msg);
  tt_assert(client);
  tt_assert(!created);
  tt_assert(!err_msg);
  rend_authorized_client_free(client);

  /* Test invalid client names */
  client = add_onion_helper_clientauth("no*asterisks*allowed", &created,
                                       &err_msg);
  tt_assert(!client);
  tt_assert(err_msg);
  tor_free(err_msg);

  /* Test invalid auth cookie */
  client = add_onion_helper_clientauth("alice:12345", &created, &err_msg);
  tt_assert(!client);
  tt_assert(err_msg);
  tor_free(err_msg);

  /* Test invalid syntax */
  client = add_onion_helper_clientauth(":475hGBHPlq7Mc0cRZitK/B", &created,
                                       &err_msg);
  tt_assert(!client);
  tt_assert(err_msg);
  tor_free(err_msg);

 done:
  rend_authorized_client_free(client);
  tor_free(err_msg);
}

/* Mocks and data/variables used for GETINFO download status tests */

static const download_status_t dl_status_default =
  { 0, 0, 0, DL_SCHED_CONSENSUS, DL_WANT_ANY_DIRSERVER,
    DL_SCHED_INCREMENT_FAILURE, DL_SCHED_RANDOM_EXPONENTIAL, 0, 0 };
static download_status_t ns_dl_status[N_CONSENSUS_FLAVORS];
static download_status_t ns_dl_status_bootstrap[N_CONSENSUS_FLAVORS];
static download_status_t ns_dl_status_running[N_CONSENSUS_FLAVORS];

/*
 * These should explore all the possible cases of download_status_to_string()
 * in control.c
 */
static const download_status_t dls_sample_1 =
  { 1467163900, 0, 0, DL_SCHED_GENERIC, DL_WANT_ANY_DIRSERVER,
    DL_SCHED_INCREMENT_FAILURE, DL_SCHED_DETERMINISTIC, 0, 0 };
static const char * dls_sample_1_str =
    "next-attempt-at 2016-06-29 01:31:40\n"
    "n-download-failures 0\n"
    "n-download-attempts 0\n"
    "schedule DL_SCHED_GENERIC\n"
    "want-authority DL_WANT_ANY_DIRSERVER\n"
    "increment-on DL_SCHED_INCREMENT_FAILURE\n"
    "backoff DL_SCHED_DETERMINISTIC\n";
static const download_status_t dls_sample_2 =
  { 1467164400, 1, 2, DL_SCHED_CONSENSUS, DL_WANT_AUTHORITY,
    DL_SCHED_INCREMENT_FAILURE, DL_SCHED_DETERMINISTIC, 0, 0 };
static const char * dls_sample_2_str =
    "next-attempt-at 2016-06-29 01:40:00\n"
    "n-download-failures 1\n"
    "n-download-attempts 2\n"
    "schedule DL_SCHED_CONSENSUS\n"
    "want-authority DL_WANT_AUTHORITY\n"
    "increment-on DL_SCHED_INCREMENT_FAILURE\n"
    "backoff DL_SCHED_DETERMINISTIC\n";
static const download_status_t dls_sample_3 =
  { 1467154400, 12, 25, DL_SCHED_BRIDGE, DL_WANT_ANY_DIRSERVER,
    DL_SCHED_INCREMENT_ATTEMPT, DL_SCHED_DETERMINISTIC, 0, 0 };
static const char * dls_sample_3_str =
    "next-attempt-at 2016-06-28 22:53:20\n"
    "n-download-failures 12\n"
    "n-download-attempts 25\n"
    "schedule DL_SCHED_BRIDGE\n"
    "want-authority DL_WANT_ANY_DIRSERVER\n"
    "increment-on DL_SCHED_INCREMENT_ATTEMPT\n"
    "backoff DL_SCHED_DETERMINISTIC\n";
static const download_status_t dls_sample_4 =
  { 1467166600, 3, 0, DL_SCHED_GENERIC, DL_WANT_ANY_DIRSERVER,
    DL_SCHED_INCREMENT_FAILURE, DL_SCHED_RANDOM_EXPONENTIAL, 0, 0 };
static const char * dls_sample_4_str =
    "next-attempt-at 2016-06-29 02:16:40\n"
    "n-download-failures 3\n"
    "n-download-attempts 0\n"
    "schedule DL_SCHED_GENERIC\n"
    "want-authority DL_WANT_ANY_DIRSERVER\n"
    "increment-on DL_SCHED_INCREMENT_FAILURE\n"
    "backoff DL_SCHED_RANDOM_EXPONENTIAL\n"
    "last-backoff-position 0\n"
    "last-delay-used 0\n";
static const download_status_t dls_sample_5 =
  { 1467164600, 3, 7, DL_SCHED_CONSENSUS, DL_WANT_ANY_DIRSERVER,
    DL_SCHED_INCREMENT_FAILURE, DL_SCHED_RANDOM_EXPONENTIAL, 1, 2112, };
static const char * dls_sample_5_str =
    "next-attempt-at 2016-06-29 01:43:20\n"
    "n-download-failures 3\n"
    "n-download-attempts 7\n"
    "schedule DL_SCHED_CONSENSUS\n"
    "want-authority DL_WANT_ANY_DIRSERVER\n"
    "increment-on DL_SCHED_INCREMENT_FAILURE\n"
    "backoff DL_SCHED_RANDOM_EXPONENTIAL\n"
    "last-backoff-position 1\n"
    "last-delay-used 2112\n";
static const download_status_t dls_sample_6 =
  { 1467164200, 4, 9, DL_SCHED_CONSENSUS, DL_WANT_AUTHORITY,
    DL_SCHED_INCREMENT_ATTEMPT, DL_SCHED_RANDOM_EXPONENTIAL, 3, 432 };
static const char * dls_sample_6_str =
    "next-attempt-at 2016-06-29 01:36:40\n"
    "n-download-failures 4\n"
    "n-download-attempts 9\n"
    "schedule DL_SCHED_CONSENSUS\n"
    "want-authority DL_WANT_AUTHORITY\n"
    "increment-on DL_SCHED_INCREMENT_ATTEMPT\n"
    "backoff DL_SCHED_RANDOM_EXPONENTIAL\n"
    "last-backoff-position 3\n"
    "last-delay-used 432\n";

/* Simulated auth certs */
static const char *auth_id_digest_1_str =
    "63CDD326DFEF0CA020BDD3FEB45A3286FE13A061";
static download_status_t auth_def_cert_download_status_1;
static const char *auth_id_digest_2_str =
    "2C209FCDD8D48DC049777B8DC2C0F94A0408BE99";
static download_status_t auth_def_cert_download_status_2;
/* Expected form of digest list returned for GETINFO downloads/cert/fps */
static const char *auth_id_digest_expected_list =
    "63CDD326DFEF0CA020BDD3FEB45A3286FE13A061\n"
    "2C209FCDD8D48DC049777B8DC2C0F94A0408BE99\n";

/* Signing keys for simulated auth 1 */
static const char *auth_1_sk_1_str =
    "AA69566029B1F023BA09451B8F1B10952384EB58";
static download_status_t auth_1_sk_1_dls;
static const char *auth_1_sk_2_str =
    "710865C7F06B73C5292695A8C34F1C94F769FF72";
static download_status_t auth_1_sk_2_dls;
/*
 * Expected form of sk digest list for
 * GETINFO downloads/cert/<auth_id_digest_1_str>/sks
 */
static const char *auth_1_sk_digest_expected_list =
    "AA69566029B1F023BA09451B8F1B10952384EB58\n"
    "710865C7F06B73C5292695A8C34F1C94F769FF72\n";

/* Signing keys for simulated auth 2 */
static const char *auth_2_sk_1_str =
    "4299047E00D070AD6703FE00BE7AA756DB061E62";
static download_status_t auth_2_sk_1_dls;
static const char *auth_2_sk_2_str =
    "9451B8F1B10952384EB58B5F230C0BB701626C9B";
static download_status_t auth_2_sk_2_dls;
/*
 * Expected form of sk digest list for
 * GETINFO downloads/cert/<auth_id_digest_2_str>/sks
 */
static const char *auth_2_sk_digest_expected_list =
    "4299047E00D070AD6703FE00BE7AA756DB061E62\n"
    "9451B8F1B10952384EB58B5F230C0BB701626C9B\n";

/* Simulated router descriptor digests or bridge identity digests */
static const char *descbr_digest_1_str =
    "616408544C7345822696074A1A3DFA16AB381CBD";
static download_status_t descbr_digest_1_dl;
static const char *descbr_digest_2_str =
    "06E8067246967265DBCB6641631B530EFEC12DC3";
static download_status_t descbr_digest_2_dl;
/* Expected form of digest list returned for GETINFO downloads/desc/descs */
static const char *descbr_expected_list =
    "616408544C7345822696074A1A3DFA16AB381CBD\n"
    "06E8067246967265DBCB6641631B530EFEC12DC3\n";
/*
 * Flag to make all descbr queries fail, to simulate not being
 * configured such that such queries make sense.
 */
static int disable_descbr = 0;

static void
reset_mocked_dl_statuses(void)
{
  int i;

  for (i = 0; i < N_CONSENSUS_FLAVORS; ++i) {
    memcpy(&(ns_dl_status[i]), &dl_status_default,
           sizeof(download_status_t));
    memcpy(&(ns_dl_status_bootstrap[i]), &dl_status_default,
           sizeof(download_status_t));
    memcpy(&(ns_dl_status_running[i]), &dl_status_default,
           sizeof(download_status_t));
  }

  memcpy(&auth_def_cert_download_status_1, &dl_status_default,
         sizeof(download_status_t));
  memcpy(&auth_def_cert_download_status_2, &dl_status_default,
         sizeof(download_status_t));
  memcpy(&auth_1_sk_1_dls, &dl_status_default,
         sizeof(download_status_t));
  memcpy(&auth_1_sk_2_dls, &dl_status_default,
         sizeof(download_status_t));
  memcpy(&auth_2_sk_1_dls, &dl_status_default,
         sizeof(download_status_t));
  memcpy(&auth_2_sk_2_dls, &dl_status_default,
         sizeof(download_status_t));

  memcpy(&descbr_digest_1_dl, &dl_status_default,
         sizeof(download_status_t));
  memcpy(&descbr_digest_2_dl, &dl_status_default,
         sizeof(download_status_t));
}

static download_status_t *
ns_dl_status_mock(consensus_flavor_t flavor)
{
  return &(ns_dl_status[flavor]);
}

static download_status_t *
ns_dl_status_bootstrap_mock(consensus_flavor_t flavor)
{
  return &(ns_dl_status_bootstrap[flavor]);
}

static download_status_t *
ns_dl_status_running_mock(consensus_flavor_t flavor)
{
  return &(ns_dl_status_running[flavor]);
}

static void
setup_ns_mocks(void)
{
  MOCK(networkstatus_get_dl_status_by_flavor, ns_dl_status_mock);
  MOCK(networkstatus_get_dl_status_by_flavor_bootstrap,
       ns_dl_status_bootstrap_mock);
  MOCK(networkstatus_get_dl_status_by_flavor_running,
       ns_dl_status_running_mock);
  reset_mocked_dl_statuses();
}

static void
clear_ns_mocks(void)
{
  UNMOCK(networkstatus_get_dl_status_by_flavor);
  UNMOCK(networkstatus_get_dl_status_by_flavor_bootstrap);
  UNMOCK(networkstatus_get_dl_status_by_flavor_running);
}

static smartlist_t *
cert_dl_status_auth_ids_mock(void)
{
  char digest[DIGEST_LEN], *tmp;
  int len;
  smartlist_t *list = NULL;

  /* Just pretend we have only the two hard-coded digests listed above */
  list = smartlist_new();
  len = base16_decode(digest, DIGEST_LEN,
                      auth_id_digest_1_str, strlen(auth_id_digest_1_str));
  tt_int_op(len, OP_EQ, DIGEST_LEN);
  tmp = tor_malloc(DIGEST_LEN);
  memcpy(tmp, digest, DIGEST_LEN);
  smartlist_add(list, tmp);
  len = base16_decode(digest, DIGEST_LEN,
                      auth_id_digest_2_str, strlen(auth_id_digest_2_str));
  tt_int_op(len, OP_EQ, DIGEST_LEN);
  tmp = tor_malloc(DIGEST_LEN);
  memcpy(tmp, digest, DIGEST_LEN);
  smartlist_add(list, tmp);

 done:
  return list;
}

static download_status_t *
cert_dl_status_def_for_auth_mock(const char *digest)
{
  download_status_t *dl = NULL;
  char digest_str[HEX_DIGEST_LEN+1];

  tt_assert(digest != NULL);
  base16_encode(digest_str, HEX_DIGEST_LEN + 1,
                digest, DIGEST_LEN);
  digest_str[HEX_DIGEST_LEN] = '\0';

  if (strcmp(digest_str, auth_id_digest_1_str) == 0) {
    dl = &auth_def_cert_download_status_1;
  } else if (strcmp(digest_str, auth_id_digest_2_str) == 0) {
    dl = &auth_def_cert_download_status_2;
  }

 done:
  return dl;
}

static smartlist_t *
cert_dl_status_sks_for_auth_id_mock(const char *digest)
{
  smartlist_t *list = NULL;
  char sk[DIGEST_LEN];
  char digest_str[HEX_DIGEST_LEN+1];
  char *tmp;
  int len;

  tt_assert(digest != NULL);
  base16_encode(digest_str, HEX_DIGEST_LEN + 1,
                digest, DIGEST_LEN);
  digest_str[HEX_DIGEST_LEN] = '\0';

  /*
   * Build a list of two hard-coded digests, depending on what we
   * were just passed.
   */
  if (strcmp(digest_str, auth_id_digest_1_str) == 0) {
    list = smartlist_new();
    len = base16_decode(sk, DIGEST_LEN,
                        auth_1_sk_1_str, strlen(auth_1_sk_1_str));
    tt_int_op(len, OP_EQ, DIGEST_LEN);
    tmp = tor_malloc(DIGEST_LEN);
    memcpy(tmp, sk, DIGEST_LEN);
    smartlist_add(list, tmp);
    len = base16_decode(sk, DIGEST_LEN,
                        auth_1_sk_2_str, strlen(auth_1_sk_2_str));
    tt_int_op(len, OP_EQ, DIGEST_LEN);
    tmp = tor_malloc(DIGEST_LEN);
    memcpy(tmp, sk, DIGEST_LEN);
    smartlist_add(list, tmp);
  } else if (strcmp(digest_str, auth_id_digest_2_str) == 0) {
    list = smartlist_new();
    len = base16_decode(sk, DIGEST_LEN,
                        auth_2_sk_1_str, strlen(auth_2_sk_1_str));
    tt_int_op(len, OP_EQ, DIGEST_LEN);
    tmp = tor_malloc(DIGEST_LEN);
    memcpy(tmp, sk, DIGEST_LEN);
    smartlist_add(list, tmp);
    len = base16_decode(sk, DIGEST_LEN,
                        auth_2_sk_2_str, strlen(auth_2_sk_2_str));
    tt_int_op(len, OP_EQ, DIGEST_LEN);
    tmp = tor_malloc(DIGEST_LEN);
    memcpy(tmp, sk, DIGEST_LEN);
    smartlist_add(list, tmp);
  }

 done:
  return list;
}

static download_status_t *
cert_dl_status_fp_sk_mock(const char *fp_digest, const char *sk_digest)
{
  download_status_t *dl = NULL;
  char fp_digest_str[HEX_DIGEST_LEN+1], sk_digest_str[HEX_DIGEST_LEN+1];

  /*
   * Unpack the digests so we can compare them and figure out which
   * dl status we want.
   */

  tt_assert(fp_digest != NULL);
  base16_encode(fp_digest_str, HEX_DIGEST_LEN + 1,
                fp_digest, DIGEST_LEN);
  fp_digest_str[HEX_DIGEST_LEN] = '\0';
  tt_assert(sk_digest != NULL);
  base16_encode(sk_digest_str, HEX_DIGEST_LEN + 1,
                sk_digest, DIGEST_LEN);
  sk_digest_str[HEX_DIGEST_LEN] = '\0';

  if (strcmp(fp_digest_str, auth_id_digest_1_str) == 0) {
    if (strcmp(sk_digest_str, auth_1_sk_1_str) == 0) {
      dl = &auth_1_sk_1_dls;
    } else if (strcmp(sk_digest_str, auth_1_sk_2_str) == 0) {
      dl = &auth_1_sk_2_dls;
    }
  } else if (strcmp(fp_digest_str, auth_id_digest_2_str) == 0) {
    if (strcmp(sk_digest_str, auth_2_sk_1_str) == 0) {
      dl = &auth_2_sk_1_dls;
    } else if (strcmp(sk_digest_str, auth_2_sk_2_str) == 0) {
      dl = &auth_2_sk_2_dls;
    }
  }

 done:
  return dl;
}

static void
setup_cert_mocks(void)
{
  MOCK(list_authority_ids_with_downloads, cert_dl_status_auth_ids_mock);
  MOCK(id_only_download_status_for_authority_id,
       cert_dl_status_def_for_auth_mock);
  MOCK(list_sk_digests_for_authority_id,
       cert_dl_status_sks_for_auth_id_mock);
  MOCK(download_status_for_authority_id_and_sk,
       cert_dl_status_fp_sk_mock);
  reset_mocked_dl_statuses();
}

static void
clear_cert_mocks(void)
{
  UNMOCK(list_authority_ids_with_downloads);
  UNMOCK(id_only_download_status_for_authority_id);
  UNMOCK(list_sk_digests_for_authority_id);
  UNMOCK(download_status_for_authority_id_and_sk);
}

static smartlist_t *
descbr_get_digests_mock(void)
{
  char digest[DIGEST_LEN], *tmp;
  int len;
  smartlist_t *list = NULL;

  if (!disable_descbr) {
    /* Just pretend we have only the two hard-coded digests listed above */
    list = smartlist_new();
    len = base16_decode(digest, DIGEST_LEN,
                        descbr_digest_1_str, strlen(descbr_digest_1_str));
    tt_int_op(len, OP_EQ, DIGEST_LEN);
    tmp = tor_malloc(DIGEST_LEN);
    memcpy(tmp, digest, DIGEST_LEN);
    smartlist_add(list, tmp);
    len = base16_decode(digest, DIGEST_LEN,
                        descbr_digest_2_str, strlen(descbr_digest_2_str));
    tt_int_op(len, OP_EQ, DIGEST_LEN);
    tmp = tor_malloc(DIGEST_LEN);
    memcpy(tmp, digest, DIGEST_LEN);
    smartlist_add(list, tmp);
  }

 done:
  return list;
}

static download_status_t *
descbr_get_dl_by_digest_mock(const char *digest)
{
  download_status_t *dl = NULL;
  char digest_str[HEX_DIGEST_LEN+1];

  if (!disable_descbr) {
    tt_assert(digest != NULL);
    base16_encode(digest_str, HEX_DIGEST_LEN + 1,
                  digest, DIGEST_LEN);
    digest_str[HEX_DIGEST_LEN] = '\0';

    if (strcmp(digest_str, descbr_digest_1_str) == 0) {
      dl = &descbr_digest_1_dl;
    } else if (strcmp(digest_str, descbr_digest_2_str) == 0) {
      dl = &descbr_digest_2_dl;
    }
  }

 done:
  return dl;
}

static void
setup_desc_mocks(void)
{
  MOCK(router_get_descriptor_digests,
       descbr_get_digests_mock);
  MOCK(router_get_dl_status_by_descriptor_digest,
       descbr_get_dl_by_digest_mock);
  reset_mocked_dl_statuses();
}

static void
clear_desc_mocks(void)
{
  UNMOCK(router_get_descriptor_digests);
  UNMOCK(router_get_dl_status_by_descriptor_digest);
}

static void
setup_bridge_mocks(void)
{
  disable_descbr = 0;

  MOCK(list_bridge_identities,
       descbr_get_digests_mock);
  MOCK(get_bridge_dl_status_by_id,
       descbr_get_dl_by_digest_mock);
  reset_mocked_dl_statuses();
}

static void
clear_bridge_mocks(void)
{
  UNMOCK(list_bridge_identities);
  UNMOCK(get_bridge_dl_status_by_id);

  disable_descbr = 0;
}

static void
test_download_status_consensus(void *arg)
{
  /* We just need one of these to pass, it doesn't matter what's in it */
  control_connection_t dummy;
  /* Get results out */
  char *answer = NULL;
  const char *errmsg = NULL;

  (void)arg;

  /* Check that the unknown prefix case works; no mocks needed yet */
  getinfo_helper_downloads(&dummy, "downloads/foo", &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_str_op(errmsg, OP_EQ, "Unknown download status query");

  setup_ns_mocks();

  /*
   * Check returning serialized dlstatuses, and implicitly also test
   * download_status_to_string().
   */

  /* Case 1 default/FLAV_NS*/
  memcpy(&(ns_dl_status[FLAV_NS]), &dls_sample_1,
         sizeof(download_status_t));
  getinfo_helper_downloads(&dummy, "downloads/networkstatus/ns",
                           &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_1_str);
  tor_free(answer);
  errmsg = NULL;

  /* Case 2 default/FLAV_MICRODESC */
  memcpy(&(ns_dl_status[FLAV_MICRODESC]), &dls_sample_2,
         sizeof(download_status_t));
  getinfo_helper_downloads(&dummy, "downloads/networkstatus/microdesc",
                           &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_2_str);
  tor_free(answer);
  errmsg = NULL;

  /* Case 3 bootstrap/FLAV_NS */
  memcpy(&(ns_dl_status_bootstrap[FLAV_NS]), &dls_sample_3,
         sizeof(download_status_t));
  getinfo_helper_downloads(&dummy, "downloads/networkstatus/ns/bootstrap",
                           &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_3_str);
  tor_free(answer);
  errmsg = NULL;

  /* Case 4 bootstrap/FLAV_MICRODESC */
  memcpy(&(ns_dl_status_bootstrap[FLAV_MICRODESC]), &dls_sample_4,
         sizeof(download_status_t));
  getinfo_helper_downloads(&dummy,
                           "downloads/networkstatus/microdesc/bootstrap",
                           &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_4_str);
  tor_free(answer);
  errmsg = NULL;

  /* Case 5 running/FLAV_NS */
  memcpy(&(ns_dl_status_running[FLAV_NS]), &dls_sample_5,
         sizeof(download_status_t));
  getinfo_helper_downloads(&dummy,
                           "downloads/networkstatus/ns/running",
                           &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_5_str);
  tor_free(answer);
  errmsg = NULL;

  /* Case 6 running/FLAV_MICRODESC */
  memcpy(&(ns_dl_status_running[FLAV_MICRODESC]), &dls_sample_6,
         sizeof(download_status_t));
  getinfo_helper_downloads(&dummy,
                           "downloads/networkstatus/microdesc/running",
                           &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_6_str);
  tor_free(answer);
  errmsg = NULL;

  /* Now check the error case */
  getinfo_helper_downloads(&dummy, "downloads/networkstatus/foo",
                           &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "Unknown flavor");
  errmsg = NULL;

 done:
  clear_ns_mocks();
  tor_free(answer);

  return;
}

static void
test_download_status_cert(void *arg)
{
  /* We just need one of these to pass, it doesn't matter what's in it */
  control_connection_t dummy;
  /* Get results out */
  char *question = NULL;
  char *answer = NULL;
  const char *errmsg = NULL;

  (void)arg;

  setup_cert_mocks();

  /*
   * Check returning serialized dlstatuses and digest lists, and implicitly
   * also test download_status_to_string() and digest_list_to_string().
   */

  /* Case 1 - list of authority identity fingerprints */
  getinfo_helper_downloads(&dummy,
                           "downloads/cert/fps",
                           &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, auth_id_digest_expected_list);
  tor_free(answer);
  errmsg = NULL;

  /* Case 2 - download status for default cert for 1st auth id */
  memcpy(&auth_def_cert_download_status_1, &dls_sample_1,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/cert/fp/%s", auth_id_digest_1_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_1_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Case 3 - download status for default cert for 2nd auth id */
  memcpy(&auth_def_cert_download_status_2, &dls_sample_2,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/cert/fp/%s", auth_id_digest_2_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_2_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Case 4 - list of signing key digests for 1st auth id */
  tor_asprintf(&question, "downloads/cert/fp/%s/sks", auth_id_digest_1_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, auth_1_sk_digest_expected_list);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Case 5 - list of signing key digests for 2nd auth id */
  tor_asprintf(&question, "downloads/cert/fp/%s/sks", auth_id_digest_2_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, auth_2_sk_digest_expected_list);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Case 6 - download status for 1st auth id, 1st sk */
  memcpy(&auth_1_sk_1_dls, &dls_sample_3,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/cert/fp/%s/%s",
               auth_id_digest_1_str, auth_1_sk_1_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_3_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Case 7 - download status for 1st auth id, 2nd sk */
  memcpy(&auth_1_sk_2_dls, &dls_sample_4,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/cert/fp/%s/%s",
               auth_id_digest_1_str, auth_1_sk_2_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_4_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Case 8 - download status for 2nd auth id, 1st sk */
  memcpy(&auth_2_sk_1_dls, &dls_sample_5,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/cert/fp/%s/%s",
               auth_id_digest_2_str, auth_2_sk_1_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_5_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Case 9 - download status for 2nd auth id, 2nd sk */
  memcpy(&auth_2_sk_2_dls, &dls_sample_6,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/cert/fp/%s/%s",
               auth_id_digest_2_str, auth_2_sk_2_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_6_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Now check the error cases */

  /* Case 1 - query is garbage after downloads/cert/ part */
  getinfo_helper_downloads(&dummy, "downloads/cert/blahdeblah",
                           &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "Unknown certificate download status query");
  errmsg = NULL;

  /*
   * Case 2 - looks like downloads/cert/fp/<fp>, but <fp> isn't even
   * the right length for a digest.
   */
  getinfo_helper_downloads(&dummy, "downloads/cert/fp/2B1D36D32B2942406",
                           &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "That didn't look like a digest");
  errmsg = NULL;

  /*
   * Case 3 - looks like downloads/cert/fp/<fp>, and <fp> is digest-sized,
   * but not parseable as one.
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/82F52AF55D250115FE44D3GC81D49643241D56A1",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "That didn't look like a digest");
  errmsg = NULL;

  /*
   * Case 4 - downloads/cert/fp/<fp>, and <fp> is not a known authority
   * identity digest
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/AC4F23B5745BDD2A77997B85B1FD85D05C2E0F61",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ,
      "Failed to get download status for this authority identity digest");
  errmsg = NULL;

  /*
   * Case 5 - looks like downloads/cert/fp/<fp>/<anything>, but <fp> doesn't
   * parse as a sensible digest.
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/82F52AF55D250115FE44D3GC81D49643241D56A1/blah",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "That didn't look like an identity digest");
  errmsg = NULL;

  /*
   * Case 6 - looks like downloads/cert/fp/<fp>/<anything>, but <fp> doesn't
   * parse as a sensible digest.
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/82F52AF55D25/blah",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "That didn't look like an identity digest");
  errmsg = NULL;

  /*
   * Case 7 - downloads/cert/fp/<fp>/sks, and <fp> is not a known authority
   * digest.
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/AC4F23B5745BDD2A77997B85B1FD85D05C2E0F61/sks",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ,
      "Failed to get list of signing key digests for this authority "
      "identity digest");
  errmsg = NULL;

  /*
   * Case 8 - looks like downloads/cert/fp/<fp>/<sk>, but <sk> doesn't
   * parse as a signing key digest.
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/AC4F23B5745BDD2A77997B85B1FD85D05C2E0F61/"
      "82F52AF55D250115FE44D3GC81D49643241D56A1",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "That didn't look like a signing key digest");
  errmsg = NULL;

  /*
   * Case 9 - looks like downloads/cert/fp/<fp>/<sk>, but <sk> doesn't
   * parse as a signing key digest.
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/AC4F23B5745BDD2A77997B85B1FD85D05C2E0F61/"
      "82F52AF55D250115FE44D",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "That didn't look like a signing key digest");
  errmsg = NULL;

  /*
   * Case 10 - downloads/cert/fp/<fp>/<sk>, but <fp> isn't a known
   * authority identity digest.
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/C6B05DF332F74DB9A13498EE3BBC7AA2F69FCB45/"
      "3A214FC21AE25B012C2ECCB5F4EC8A3602D0545D",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ,
      "Failed to get download status for this identity/"
      "signing key digest pair");
  errmsg = NULL;

  /*
   * Case 11 - downloads/cert/fp/<fp>/<sk>, but <sk> isn't a known
   * signing key digest.
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/63CDD326DFEF0CA020BDD3FEB45A3286FE13A061/"
      "3A214FC21AE25B012C2ECCB5F4EC8A3602D0545D",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ,
      "Failed to get download status for this identity/"
      "signing key digest pair");
  errmsg = NULL;

  /*
   * Case 12 - downloads/cert/fp/<fp>/<sk>, but <sk> is on the list for
   * a different authority identity digest.
   */
  getinfo_helper_downloads(&dummy,
      "downloads/cert/fp/63CDD326DFEF0CA020BDD3FEB45A3286FE13A061/"
      "9451B8F1B10952384EB58B5F230C0BB701626C9B",
      &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ,
      "Failed to get download status for this identity/"
      "signing key digest pair");
  errmsg = NULL;

 done:
  clear_cert_mocks();
  tor_free(answer);

  return;
}

static void
test_download_status_desc(void *arg)
{
  /* We just need one of these to pass, it doesn't matter what's in it */
  control_connection_t dummy;
  /* Get results out */
  char *question = NULL;
  char *answer = NULL;
  const char *errmsg = NULL;

  (void)arg;

  setup_desc_mocks();

  /*
   * Check returning serialized dlstatuses and digest lists, and implicitly
   * also test download_status_to_string() and digest_list_to_string().
   */

  /* Case 1 - list of router descriptor digests */
  getinfo_helper_downloads(&dummy,
                           "downloads/desc/descs",
                           &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, descbr_expected_list);
  tor_free(answer);
  errmsg = NULL;

  /* Case 2 - get download status for router descriptor 1 */
  memcpy(&descbr_digest_1_dl, &dls_sample_1,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/desc/%s", descbr_digest_1_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_1_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Case 3 - get download status for router descriptor 1 */
  memcpy(&descbr_digest_2_dl, &dls_sample_2,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/desc/%s", descbr_digest_2_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_2_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Now check the error cases */

  /* Case 1 - non-digest-length garbage after downloads/desc */
  getinfo_helper_downloads(&dummy, "downloads/desc/blahdeblah",
                           &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "Unknown router descriptor download status query");
  errmsg = NULL;

  /* Case 2 - nonparseable digest-shaped thing */
  getinfo_helper_downloads(
    &dummy,
    "downloads/desc/774EC52FD9A5B80A6FACZE536616E8022E3470AG",
    &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "That didn't look like a digest");
  errmsg = NULL;

  /* Case 3 - digest we have no descriptor for */
  getinfo_helper_downloads(
    &dummy,
    "downloads/desc/B05B46135B0B2C04EBE1DD6A6AE4B12D7CD2226A",
    &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "No such descriptor digest found");
  errmsg = NULL;

  /* Case 4 - microdescs only */
  disable_descbr = 1;
  getinfo_helper_downloads(&dummy,
                           "downloads/desc/descs",
                           &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ,
            "We don't seem to have a networkstatus-flavored consensus");
  errmsg = NULL;
  disable_descbr = 0;

 done:
  clear_desc_mocks();
  tor_free(answer);

  return;
}

static void
test_download_status_bridge(void *arg)
{
  /* We just need one of these to pass, it doesn't matter what's in it */
  control_connection_t dummy;
  /* Get results out */
  char *question = NULL;
  char *answer = NULL;
  const char *errmsg = NULL;

  (void)arg;

  setup_bridge_mocks();

  /*
   * Check returning serialized dlstatuses and digest lists, and implicitly
   * also test download_status_to_string() and digest_list_to_string().
   */

  /* Case 1 - list of bridge identity digests */
  getinfo_helper_downloads(&dummy,
                           "downloads/bridge/bridges",
                           &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, descbr_expected_list);
  tor_free(answer);
  errmsg = NULL;

  /* Case 2 - get download status for bridge descriptor 1 */
  memcpy(&descbr_digest_1_dl, &dls_sample_3,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/bridge/%s", descbr_digest_1_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_3_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Case 3 - get download status for router descriptor 1 */
  memcpy(&descbr_digest_2_dl, &dls_sample_4,
         sizeof(download_status_t));
  tor_asprintf(&question, "downloads/bridge/%s", descbr_digest_2_str);
  tt_assert(question != NULL);
  getinfo_helper_downloads(&dummy, question, &answer, &errmsg);
  tt_assert(answer != NULL);
  tt_assert(errmsg == NULL);
  tt_str_op(answer, OP_EQ, dls_sample_4_str);
  tor_free(question);
  tor_free(answer);
  errmsg = NULL;

  /* Now check the error cases */

  /* Case 1 - non-digest-length garbage after downloads/bridge */
  getinfo_helper_downloads(&dummy, "downloads/bridge/blahdeblah",
                           &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "Unknown bridge descriptor download status query");
  errmsg = NULL;

  /* Case 2 - nonparseable digest-shaped thing */
  getinfo_helper_downloads(
    &dummy,
    "downloads/bridge/774EC52FD9A5B80A6FACZE536616E8022E3470AG",
    &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "That didn't look like a digest");
  errmsg = NULL;

  /* Case 3 - digest we have no descriptor for */
  getinfo_helper_downloads(
    &dummy,
    "downloads/bridge/B05B46135B0B2C04EBE1DD6A6AE4B12D7CD2226A",
    &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "No such bridge identity digest found");
  errmsg = NULL;

  /* Case 4 - bridges disabled */
  disable_descbr = 1;
  getinfo_helper_downloads(&dummy,
                           "downloads/bridge/bridges",
                           &answer, &errmsg);
  tt_assert(answer == NULL);
  tt_assert(errmsg != NULL);
  tt_str_op(errmsg, OP_EQ, "We don't seem to be using bridges");
  errmsg = NULL;
  disable_descbr = 0;

 done:
  clear_bridge_mocks();
  tor_free(answer);

  return;
}

struct testcase_t controller_tests[] = {
  { "add_onion_helper_keyarg", test_add_onion_helper_keyarg, 0, NULL, NULL },
  { "rend_service_parse_port_config", test_rend_service_parse_port_config, 0,
    NULL, NULL },
  { "add_onion_helper_clientauth", test_add_onion_helper_clientauth, 0, NULL,
    NULL },
  { "download_status_consensus", test_download_status_consensus, 0, NULL,
    NULL },
  { "download_status_cert", test_download_status_cert, 0, NULL,
    NULL },
  { "download_status_desc", test_download_status_desc, 0, NULL, NULL },
  { "download_status_bridge", test_download_status_bridge, 0, NULL, NULL },
  END_OF_TESTCASES
};

