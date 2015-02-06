/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#define ROUTER_PRIVATE
#include "or.h"
#include "config.h"
#include "router.h"
#include "util.h"
#include "crypto.h"

#include "test.h"

static void
test_routerkeys_write_fingerprint(void *arg)
{
  crypto_pk_t *key = pk_generate(2);
  or_options_t *options = get_options_mutable();
  const char *ddir = get_fname("write_fingerprint");
  char *cp = NULL, *cp2 = NULL;
  char fp[FINGERPRINT_LEN+1];

  (void)arg;

  tt_assert(key);

  options->ORPort_set = 1; /* So that we can get the server ID key */
  tor_free(options->DataDirectory);
  options->DataDirectory = tor_strdup(ddir);
  options->Nickname = tor_strdup("haflinger");
  set_server_identity_key(key);
  set_client_identity_key(crypto_pk_dup_key(key));

  tt_int_op(0, ==, check_private_dir(ddir, CPD_CREATE, NULL));
  tt_int_op(crypto_pk_cmp_keys(get_server_identity_key(),key),==,0);

  /* Write fingerprint file */
  tt_int_op(0, ==, router_write_fingerprint(0));
  cp = read_file_to_str(get_fname("write_fingerprint/fingerprint"),
                        0, NULL);
  crypto_pk_get_fingerprint(key, fp, 0);
  tor_asprintf(&cp2, "haflinger %s\n", fp);
  tt_str_op(cp, ==, cp2);
  tor_free(cp);
  tor_free(cp2);

  /* Write hashed-fingerprint file */
  tt_int_op(0, ==, router_write_fingerprint(1));
  cp = read_file_to_str(get_fname("write_fingerprint/hashed-fingerprint"),
                        0, NULL);
  crypto_pk_get_hashed_fingerprint(key, fp);
  tor_asprintf(&cp2, "haflinger %s\n", fp);
  tt_str_op(cp, ==, cp2);
  tor_free(cp);
  tor_free(cp2);

  /* Replace outdated file */
  write_str_to_file(get_fname("write_fingerprint/hashed-fingerprint"),
                    "junk goes here", 0);
  tt_int_op(0, ==, router_write_fingerprint(1));
  cp = read_file_to_str(get_fname("write_fingerprint/hashed-fingerprint"),
                        0, NULL);
  crypto_pk_get_hashed_fingerprint(key, fp);
  tor_asprintf(&cp2, "haflinger %s\n", fp);
  tt_str_op(cp, ==, cp2);
  tor_free(cp);
  tor_free(cp2);

 done:
  crypto_pk_free(key);
  set_client_identity_key(NULL);
  tor_free(cp);
  tor_free(cp2);
}

#define TEST(name, flags)                                       \
  { #name , test_routerkeys_ ## name, (flags), NULL, NULL }

struct testcase_t routerkeys_tests[] = {
  TEST(write_fingerprint, TT_FORK),
  END_OF_TESTCASES
};

