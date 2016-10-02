/* Copyright (c) 2015-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define CONTROL_PRIVATE
#include "or.h"
#include "control.h"
#include "rendservice.h"
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

 done:
  rend_service_port_config_free(cfg);
  tor_free(err_msg);
}

struct testcase_t controller_tests[] = {
  { "add_onion_helper_keyarg", test_add_onion_helper_keyarg, 0, NULL, NULL },
  { "rend_service_parse_port_config", test_rend_service_parse_port_config, 0,
    NULL, NULL },
  END_OF_TESTCASES
};

