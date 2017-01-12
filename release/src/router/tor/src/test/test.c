/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test.c
 * \brief Unit tests for many pieces of the lower level Tor modules.
 **/

#include "orconfig.h"

#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef _WIN32
/* For mkdir() */
#include <direct.h>
#else
#include <dirent.h>
#endif

/* These macros pull in declarations for some functions and structures that
 * are typically file-private. */
#define GEOIP_PRIVATE
#define ROUTER_PRIVATE
#define CIRCUITSTATS_PRIVATE
#define CIRCUITLIST_PRIVATE
#define MAIN_PRIVATE
#define STATEFILE_PRIVATE

/*
 * Linux doesn't provide lround in math.h by default, but mac os does...
 * It's best just to leave math.h out of the picture entirely.
 */
//#include <math.h>
long int lround(double x);
double fabs(double x);

#include "or.h"
#include "backtrace.h"
#include "buffers.h"
#include "circuitlist.h"
#include "circuitstats.h"
#include "config.h"
#include "connection_edge.h"
#include "geoip.h"
#include "rendcommon.h"
#include "rendcache.h"
#include "test.h"
#include "torgzip.h"
#include "main.h"
#include "memarea.h"
#include "onion.h"
#include "onion_ntor.h"
#include "onion_fast.h"
#include "onion_tap.h"
#include "policies.h"
#include "rephist.h"
#include "routerparse.h"
#include "statefile.h"
#include "crypto_curve25519.h"
#include "onion_ntor.h"

/** Run unit tests for the onion handshake code. */
static void
test_onion_handshake(void *arg)
{
  /* client-side */
  crypto_dh_t *c_dh = NULL;
  char c_buf[TAP_ONIONSKIN_CHALLENGE_LEN];
  char c_keys[40];
  /* server-side */
  char s_buf[TAP_ONIONSKIN_REPLY_LEN];
  char s_keys[40];
  int i;
  /* shared */
  crypto_pk_t *pk = NULL, *pk2 = NULL;

  (void)arg;
  pk = pk_generate(0);
  pk2 = pk_generate(1);

  /* client handshake 1. */
  memset(c_buf, 0, TAP_ONIONSKIN_CHALLENGE_LEN);
  tt_assert(! onion_skin_TAP_create(pk, &c_dh, c_buf));

  for (i = 1; i <= 3; ++i) {
    crypto_pk_t *k1, *k2;
    if (i==1) {
      /* server handshake: only one key known. */
      k1 = pk;  k2 = NULL;
    } else if (i==2) {
      /* server handshake: try the right key first. */
      k1 = pk;  k2 = pk2;
    } else {
      /* server handshake: try the right key second. */
      k1 = pk2; k2 = pk;
    }

    memset(s_buf, 0, TAP_ONIONSKIN_REPLY_LEN);
    memset(s_keys, 0, 40);
    tt_assert(! onion_skin_TAP_server_handshake(c_buf, k1, k2,
                                                  s_buf, s_keys, 40));

    /* client handshake 2 */
    memset(c_keys, 0, 40);
    tt_assert(! onion_skin_TAP_client_handshake(c_dh, s_buf, c_keys,
                                                40, NULL));

    tt_mem_op(c_keys,OP_EQ, s_keys, 40);
    memset(s_buf, 0, 40);
    tt_mem_op(c_keys,OP_NE, s_buf, 40);
  }
 done:
  crypto_dh_free(c_dh);
  crypto_pk_free(pk);
  crypto_pk_free(pk2);
}

static void
test_bad_onion_handshake(void *arg)
{
  char junk_buf[TAP_ONIONSKIN_CHALLENGE_LEN];
  char junk_buf2[TAP_ONIONSKIN_CHALLENGE_LEN];
  /* client-side */
  crypto_dh_t *c_dh = NULL;
  char c_buf[TAP_ONIONSKIN_CHALLENGE_LEN];
  char c_keys[40];
  /* server-side */
  char s_buf[TAP_ONIONSKIN_REPLY_LEN];
  char s_keys[40];
  /* shared */
  crypto_pk_t *pk = NULL, *pk2 = NULL;

  (void)arg;

  pk = pk_generate(0);
  pk2 = pk_generate(1);

  /* Server: Case 1: the encrypted data is degenerate. */
  memset(junk_buf, 0, sizeof(junk_buf));
  crypto_pk_public_hybrid_encrypt(pk, junk_buf2, TAP_ONIONSKIN_CHALLENGE_LEN,
                               junk_buf, DH_KEY_LEN, PK_PKCS1_OAEP_PADDING, 1);
  tt_int_op(-1, OP_EQ,
            onion_skin_TAP_server_handshake(junk_buf2, pk, NULL,
                                            s_buf, s_keys, 40));

  /* Server: Case 2: the encrypted data is not long enough. */
  memset(junk_buf, 0, sizeof(junk_buf));
  memset(junk_buf2, 0, sizeof(junk_buf2));
  crypto_pk_public_encrypt(pk, junk_buf2, sizeof(junk_buf2),
                               junk_buf, 48, PK_PKCS1_OAEP_PADDING);
  tt_int_op(-1, OP_EQ,
            onion_skin_TAP_server_handshake(junk_buf2, pk, NULL,
                                            s_buf, s_keys, 40));

  /* client handshake 1: do it straight. */
  memset(c_buf, 0, TAP_ONIONSKIN_CHALLENGE_LEN);
  tt_assert(! onion_skin_TAP_create(pk, &c_dh, c_buf));

  /* Server: Case 3: we just don't have the right key. */
  tt_int_op(-1, OP_EQ,
            onion_skin_TAP_server_handshake(c_buf, pk2, NULL,
                                            s_buf, s_keys, 40));

  /* Server: Case 4: The RSA-encrypted portion is corrupt. */
  c_buf[64] ^= 33;
  tt_int_op(-1, OP_EQ,
            onion_skin_TAP_server_handshake(c_buf, pk, NULL,
                                            s_buf, s_keys, 40));
  c_buf[64] ^= 33;

  /* (Let the server procede) */
  tt_int_op(0, OP_EQ,
            onion_skin_TAP_server_handshake(c_buf, pk, NULL,
                                            s_buf, s_keys, 40));

  /* Client: Case 1: The server sent back junk. */
  const char *msg = NULL;
  s_buf[64] ^= 33;
  tt_int_op(-1, OP_EQ,
            onion_skin_TAP_client_handshake(c_dh, s_buf, c_keys, 40, &msg));
  s_buf[64] ^= 33;
  tt_str_op(msg, OP_EQ, "Digest DOES NOT MATCH on onion handshake. "
            "Bug or attack.");

  /* Let the client finish; make sure it can. */
  msg = NULL;
  tt_int_op(0, OP_EQ,
            onion_skin_TAP_client_handshake(c_dh, s_buf, c_keys, 40, &msg));
  tt_mem_op(s_keys,OP_EQ, c_keys, 40);
  tt_ptr_op(msg, OP_EQ, NULL);

  /* Client: Case 2: The server sent back a degenerate DH. */
  memset(s_buf, 0, sizeof(s_buf));
  tt_int_op(-1, OP_EQ,
            onion_skin_TAP_client_handshake(c_dh, s_buf, c_keys, 40, &msg));
  tt_str_op(msg, OP_EQ, "DH computation failed.");

 done:
  crypto_dh_free(c_dh);
  crypto_pk_free(pk);
  crypto_pk_free(pk2);
}

static void
test_ntor_handshake(void *arg)
{
  /* client-side */
  ntor_handshake_state_t *c_state = NULL;
  uint8_t c_buf[NTOR_ONIONSKIN_LEN];
  uint8_t c_keys[400];

  /* server-side */
  di_digest256_map_t *s_keymap=NULL;
  curve25519_keypair_t s_keypair;
  uint8_t s_buf[NTOR_REPLY_LEN];
  uint8_t s_keys[400];

  /* shared */
  const curve25519_public_key_t *server_pubkey;
  uint8_t node_id[20] = "abcdefghijklmnopqrst";

  (void) arg;

  /* Make the server some keys */
  curve25519_secret_key_generate(&s_keypair.seckey, 0);
  curve25519_public_key_generate(&s_keypair.pubkey, &s_keypair.seckey);
  dimap_add_entry(&s_keymap, s_keypair.pubkey.public_key, &s_keypair);
  server_pubkey = &s_keypair.pubkey;

  /* client handshake 1. */
  memset(c_buf, 0, NTOR_ONIONSKIN_LEN);
  tt_int_op(0, OP_EQ, onion_skin_ntor_create(node_id, server_pubkey,
                                          &c_state, c_buf));

  /* server handshake */
  memset(s_buf, 0, NTOR_REPLY_LEN);
  memset(s_keys, 0, 40);
  tt_int_op(0, OP_EQ, onion_skin_ntor_server_handshake(c_buf, s_keymap, NULL,
                                                    node_id,
                                                    s_buf, s_keys, 400));

  /* client handshake 2 */
  memset(c_keys, 0, 40);
  tt_int_op(0, OP_EQ, onion_skin_ntor_client_handshake(c_state, s_buf,
                                                       c_keys, 400, NULL));

  tt_mem_op(c_keys,OP_EQ, s_keys, 400);
  memset(s_buf, 0, 40);
  tt_mem_op(c_keys,OP_NE, s_buf, 40);

  /* Now try with a bogus server response. Zero input should trigger
   * All The Problems. */
  memset(c_keys, 0, 400);
  memset(s_buf, 0, NTOR_REPLY_LEN);
  const char *msg = NULL;
  tt_int_op(-1, OP_EQ, onion_skin_ntor_client_handshake(c_state, s_buf,
                                                        c_keys, 400, &msg));
  tt_str_op(msg, OP_EQ, "Zero output from curve25519 handshake");

 done:
  ntor_handshake_state_free(c_state);
  dimap_free(s_keymap, NULL);
}

static void
test_fast_handshake(void *arg)
{
  /* tests for the obsolete "CREATE_FAST" handshake. */
  (void) arg;
  fast_handshake_state_t *state = NULL;
  uint8_t client_handshake[CREATE_FAST_LEN];
  uint8_t server_handshake[CREATED_FAST_LEN];
  uint8_t s_keys[100], c_keys[100];

  /* First, test an entire handshake. */
  memset(client_handshake, 0, sizeof(client_handshake));
  tt_int_op(0, OP_EQ, fast_onionskin_create(&state, client_handshake));
  tt_assert(! tor_mem_is_zero((char*)client_handshake,
                              sizeof(client_handshake)));

  tt_int_op(0, OP_EQ,
            fast_server_handshake(client_handshake, server_handshake,
                                  s_keys, 100));
  const char *msg = NULL;
  tt_int_op(0, OP_EQ,
            fast_client_handshake(state, server_handshake, c_keys, 100, &msg));
  tt_ptr_op(msg, OP_EQ, NULL);
  tt_mem_op(s_keys, OP_EQ, c_keys, 100);

  /* Now test a failing handshake. */
  server_handshake[0] ^= 3;
  tt_int_op(-1, OP_EQ,
            fast_client_handshake(state, server_handshake, c_keys, 100, &msg));
  tt_str_op(msg, OP_EQ, "Digest DOES NOT MATCH on fast handshake. "
            "Bug or attack.");

 done:
  fast_handshake_state_free(state);
}

/** Run unit tests for the onion queues. */
static void
test_onion_queues(void *arg)
{
  uint8_t buf1[TAP_ONIONSKIN_CHALLENGE_LEN] = {0};
  uint8_t buf2[NTOR_ONIONSKIN_LEN] = {0};

  or_circuit_t *circ1 = or_circuit_new(0, NULL);
  or_circuit_t *circ2 = or_circuit_new(0, NULL);

  create_cell_t *onionskin = NULL, *create2_ptr;
  create_cell_t *create1 = tor_malloc_zero(sizeof(create_cell_t));
  create_cell_t *create2 = tor_malloc_zero(sizeof(create_cell_t));
  (void)arg;
  create2_ptr = create2; /* remember, but do not free */

  create_cell_init(create1, CELL_CREATE, ONION_HANDSHAKE_TYPE_TAP,
                   TAP_ONIONSKIN_CHALLENGE_LEN, buf1);
  create_cell_init(create2, CELL_CREATE, ONION_HANDSHAKE_TYPE_NTOR,
                   NTOR_ONIONSKIN_LEN, buf2);

  tt_int_op(0,OP_EQ, onion_num_pending(ONION_HANDSHAKE_TYPE_TAP));
  tt_int_op(0,OP_EQ, onion_pending_add(circ1, create1));
  create1 = NULL;
  tt_int_op(1,OP_EQ, onion_num_pending(ONION_HANDSHAKE_TYPE_TAP));

  tt_int_op(0,OP_EQ, onion_num_pending(ONION_HANDSHAKE_TYPE_NTOR));
  tt_int_op(0,OP_EQ, onion_pending_add(circ2, create2));
  create2 = NULL;
  tt_int_op(1,OP_EQ, onion_num_pending(ONION_HANDSHAKE_TYPE_NTOR));

  tt_ptr_op(circ2,OP_EQ, onion_next_task(&onionskin));
  tt_int_op(1,OP_EQ, onion_num_pending(ONION_HANDSHAKE_TYPE_TAP));
  tt_int_op(0,OP_EQ, onion_num_pending(ONION_HANDSHAKE_TYPE_NTOR));
  tt_ptr_op(onionskin, OP_EQ, create2_ptr);

  clear_pending_onions();
  tt_int_op(0,OP_EQ, onion_num_pending(ONION_HANDSHAKE_TYPE_TAP));
  tt_int_op(0,OP_EQ, onion_num_pending(ONION_HANDSHAKE_TYPE_NTOR));

 done:
  circuit_free(TO_CIRCUIT(circ1));
  circuit_free(TO_CIRCUIT(circ2));
  tor_free(create1);
  tor_free(create2);
  tor_free(onionskin);
}

static void
test_circuit_timeout(void *arg)
{
  /* Plan:
   *  1. Generate 1000 samples
   *  2. Estimate parameters
   *  3. If difference, repeat
   *  4. Save state
   *  5. load state
   *  6. Estimate parameters
   *  7. compare differences
   */
  circuit_build_times_t initial;
  circuit_build_times_t estimate;
  circuit_build_times_t final;
  double timeout1, timeout2;
  or_state_t *state=NULL;
  int i, runs;
  double close_ms;
  (void)arg;

  initialize_periodic_events();

  circuit_build_times_init(&initial);
  circuit_build_times_init(&estimate);
  circuit_build_times_init(&final);

  state = or_state_new();

  circuitbuild_running_unit_tests();
#define timeout0 (build_time_t)(30*1000.0)
  initial.Xm = 3000;
  circuit_build_times_initial_alpha(&initial,
                                    CBT_DEFAULT_QUANTILE_CUTOFF/100.0,
                                    timeout0);
  close_ms = MAX(circuit_build_times_calculate_timeout(&initial,
                             CBT_DEFAULT_CLOSE_QUANTILE/100.0),
                 CBT_DEFAULT_TIMEOUT_INITIAL_VALUE);
  do {
    for (i=0; i < CBT_DEFAULT_MIN_CIRCUITS_TO_OBSERVE; i++) {
      build_time_t sample = circuit_build_times_generate_sample(&initial,0,1);

      if (sample > close_ms) {
        circuit_build_times_add_time(&estimate, CBT_BUILD_ABANDONED);
      } else {
        circuit_build_times_add_time(&estimate, sample);
      }
    }
    circuit_build_times_update_alpha(&estimate);
    timeout1 = circuit_build_times_calculate_timeout(&estimate,
                                  CBT_DEFAULT_QUANTILE_CUTOFF/100.0);
    circuit_build_times_set_timeout(&estimate);
    log_notice(LD_CIRC, "Timeout1 is %f, Xm is %d", timeout1, estimate.Xm);
           /* 2% error */
  } while (fabs(circuit_build_times_cdf(&initial, timeout0) -
                circuit_build_times_cdf(&initial, timeout1)) > 0.02);

  tt_assert(estimate.total_build_times <= CBT_NCIRCUITS_TO_OBSERVE);

  circuit_build_times_update_state(&estimate, state);
  circuit_build_times_free_timeouts(&final);
  tt_assert(circuit_build_times_parse_state(&final, state) == 0);

  circuit_build_times_update_alpha(&final);
  timeout2 = circuit_build_times_calculate_timeout(&final,
                                 CBT_DEFAULT_QUANTILE_CUTOFF/100.0);

  circuit_build_times_set_timeout(&final);
  log_notice(LD_CIRC, "Timeout2 is %f, Xm is %d", timeout2, final.Xm);

  /* 5% here because some accuracy is lost due to histogram conversion */
  tt_assert(fabs(circuit_build_times_cdf(&initial, timeout0) -
                   circuit_build_times_cdf(&initial, timeout2)) < 0.05);

  for (runs = 0; runs < 50; runs++) {
    int build_times_idx = 0;
    int total_build_times = 0;

    final.close_ms = final.timeout_ms = CBT_DEFAULT_TIMEOUT_INITIAL_VALUE;
    estimate.close_ms = estimate.timeout_ms
                      = CBT_DEFAULT_TIMEOUT_INITIAL_VALUE;

    for (i = 0; i < CBT_DEFAULT_RECENT_CIRCUITS*2; i++) {
      circuit_build_times_network_circ_success(&estimate);
      circuit_build_times_add_time(&estimate,
            circuit_build_times_generate_sample(&estimate, 0,
                CBT_DEFAULT_QUANTILE_CUTOFF/100.0));

      circuit_build_times_network_circ_success(&estimate);
      circuit_build_times_add_time(&final,
            circuit_build_times_generate_sample(&final, 0,
                CBT_DEFAULT_QUANTILE_CUTOFF/100.0));
    }

    tt_assert(!circuit_build_times_network_check_changed(&estimate));
    tt_assert(!circuit_build_times_network_check_changed(&final));

    /* Reset liveness to be non-live */
    final.liveness.network_last_live = 0;
    estimate.liveness.network_last_live = 0;

    build_times_idx = estimate.build_times_idx;
    total_build_times = estimate.total_build_times;

    tt_assert(circuit_build_times_network_check_live(&estimate));
    tt_assert(circuit_build_times_network_check_live(&final));

    circuit_build_times_count_close(&estimate, 0,
            (time_t)(approx_time()-estimate.close_ms/1000.0-1));
    circuit_build_times_count_close(&final, 0,
            (time_t)(approx_time()-final.close_ms/1000.0-1));

    tt_assert(!circuit_build_times_network_check_live(&estimate));
    tt_assert(!circuit_build_times_network_check_live(&final));

    log_info(LD_CIRC, "idx: %d %d, tot: %d %d",
             build_times_idx, estimate.build_times_idx,
             total_build_times, estimate.total_build_times);

    /* Check rollback index. Should match top of loop. */
    tt_assert(build_times_idx == estimate.build_times_idx);
    // This can fail if estimate.total_build_times == 1000, because
    // in that case, rewind actually causes us to lose timeouts
    if (total_build_times != CBT_NCIRCUITS_TO_OBSERVE)
      tt_assert(total_build_times == estimate.total_build_times);

    /* Now simulate that the network has become live and we need
     * a change */
    circuit_build_times_network_is_live(&estimate);
    circuit_build_times_network_is_live(&final);

    for (i = 0; i < CBT_DEFAULT_MAX_RECENT_TIMEOUT_COUNT; i++) {
      circuit_build_times_count_timeout(&estimate, 1);

      if (i < CBT_DEFAULT_MAX_RECENT_TIMEOUT_COUNT-1) {
        circuit_build_times_count_timeout(&final, 1);
      }
    }

    tt_assert(estimate.liveness.after_firsthop_idx == 0);
    tt_assert(final.liveness.after_firsthop_idx ==
                CBT_DEFAULT_MAX_RECENT_TIMEOUT_COUNT-1);

    tt_assert(circuit_build_times_network_check_live(&estimate));
    tt_assert(circuit_build_times_network_check_live(&final));

    circuit_build_times_count_timeout(&final, 1);

    /* Ensure return value for degenerate cases are clamped correctly */
    initial.alpha = INT32_MAX;
    tt_assert(circuit_build_times_calculate_timeout(&initial, .99999999) <=
              INT32_MAX);
    initial.alpha = 0;
    tt_assert(circuit_build_times_calculate_timeout(&initial, .5) <=
              INT32_MAX);
  }

 done:
  circuit_build_times_free_timeouts(&initial);
  circuit_build_times_free_timeouts(&estimate);
  circuit_build_times_free_timeouts(&final);
  or_state_free(state);
  teardown_periodic_events();
}

/** Test encoding and parsing of rendezvous service descriptors. */
static void
test_rend_fns(void *arg)
{
  rend_service_descriptor_t *generated = NULL, *parsed = NULL;
  char service_id[DIGEST_LEN];
  char service_id_base32[REND_SERVICE_ID_LEN_BASE32+1];
  const char *next_desc;
  smartlist_t *descs = smartlist_new();
  char computed_desc_id[DIGEST_LEN];
  char parsed_desc_id[DIGEST_LEN];
  crypto_pk_t *pk1 = NULL, *pk2 = NULL;
  time_t now;
  char *intro_points_encrypted = NULL;
  size_t intro_points_size;
  size_t encoded_size;
  int i;
  char address1[] = "fooaddress.onion";
  char address2[] = "aaaaaaaaaaaaaaaa.onion";
  char address3[] = "fooaddress.exit";
  char address4[] = "www.torproject.org";
  char address5[] = "foo.abcdefghijklmnop.onion";
  char address6[] = "foo.bar.abcdefghijklmnop.onion";
  char address7[] = ".abcdefghijklmnop.onion";

  (void)arg;
  tt_assert(BAD_HOSTNAME == parse_extended_hostname(address1));
  tt_assert(ONION_HOSTNAME == parse_extended_hostname(address2));
  tt_str_op(address2,OP_EQ, "aaaaaaaaaaaaaaaa");
  tt_assert(EXIT_HOSTNAME == parse_extended_hostname(address3));
  tt_assert(NORMAL_HOSTNAME == parse_extended_hostname(address4));
  tt_assert(ONION_HOSTNAME == parse_extended_hostname(address5));
  tt_str_op(address5,OP_EQ, "abcdefghijklmnop");
  tt_assert(ONION_HOSTNAME == parse_extended_hostname(address6));
  tt_str_op(address6,OP_EQ, "abcdefghijklmnop");
  tt_assert(BAD_HOSTNAME == parse_extended_hostname(address7));

  /* Initialize the service cache. */
  rend_cache_init();

  pk1 = pk_generate(0);
  pk2 = pk_generate(1);
  generated = tor_malloc_zero(sizeof(rend_service_descriptor_t));
  generated->pk = crypto_pk_dup_key(pk1);
  crypto_pk_get_digest(generated->pk, service_id);
  base32_encode(service_id_base32, REND_SERVICE_ID_LEN_BASE32+1,
                service_id, REND_SERVICE_ID_LEN);
  now = time(NULL);
  generated->timestamp = now;
  generated->version = 2;
  generated->protocols = 42;
  generated->intro_nodes = smartlist_new();

  for (i = 0; i < 3; i++) {
    rend_intro_point_t *intro = tor_malloc_zero(sizeof(rend_intro_point_t));
    crypto_pk_t *okey = pk_generate(2 + i);
    intro->extend_info = tor_malloc_zero(sizeof(extend_info_t));
    intro->extend_info->onion_key = okey;
    crypto_pk_get_digest(intro->extend_info->onion_key,
                         intro->extend_info->identity_digest);
    //crypto_rand(info->identity_digest, DIGEST_LEN); /* Would this work? */
    intro->extend_info->nickname[0] = '$';
    base16_encode(intro->extend_info->nickname + 1,
                  sizeof(intro->extend_info->nickname) - 1,
                  intro->extend_info->identity_digest, DIGEST_LEN);
    /* Does not cover all IP addresses. */
    tor_addr_from_ipv4h(&intro->extend_info->addr, crypto_rand_int(65536));
    intro->extend_info->port = 1 + crypto_rand_int(65535);
    intro->intro_key = crypto_pk_dup_key(pk2);
    smartlist_add(generated->intro_nodes, intro);
  }
  tt_assert(rend_encode_v2_descriptors(descs, generated, now, 0,
                                         REND_NO_AUTH, NULL, NULL) > 0);
  tt_assert(rend_compute_v2_desc_id(computed_desc_id, service_id_base32,
                                      NULL, now, 0) == 0);
  tt_mem_op(((rend_encoded_v2_service_descriptor_t *)
             smartlist_get(descs, 0))->desc_id, OP_EQ,
            computed_desc_id, DIGEST_LEN);
  tt_assert(rend_parse_v2_service_descriptor(&parsed, parsed_desc_id,
                                             &intro_points_encrypted,
                                             &intro_points_size,
                                             &encoded_size,
                                              &next_desc,
                             ((rend_encoded_v2_service_descriptor_t *)
                                 smartlist_get(descs, 0))->desc_str, 1) == 0);
  tt_assert(parsed);
  tt_mem_op(((rend_encoded_v2_service_descriptor_t *)
         smartlist_get(descs, 0))->desc_id,OP_EQ, parsed_desc_id, DIGEST_LEN);
  tt_int_op(rend_parse_introduction_points(parsed, intro_points_encrypted,
                                         intro_points_size),OP_EQ, 3);
  tt_assert(!crypto_pk_cmp_keys(generated->pk, parsed->pk));
  tt_int_op(parsed->timestamp,OP_EQ, now);
  tt_int_op(parsed->version,OP_EQ, 2);
  tt_int_op(parsed->protocols,OP_EQ, 42);
  tt_int_op(smartlist_len(parsed->intro_nodes),OP_EQ, 3);
  for (i = 0; i < smartlist_len(parsed->intro_nodes); i++) {
    rend_intro_point_t *par_intro = smartlist_get(parsed->intro_nodes, i),
      *gen_intro = smartlist_get(generated->intro_nodes, i);
    extend_info_t *par_info = par_intro->extend_info;
    extend_info_t *gen_info = gen_intro->extend_info;
    tt_assert(!crypto_pk_cmp_keys(gen_info->onion_key, par_info->onion_key));
    tt_mem_op(gen_info->identity_digest,OP_EQ, par_info->identity_digest,
               DIGEST_LEN);
    tt_str_op(gen_info->nickname,OP_EQ, par_info->nickname);
    tt_assert(tor_addr_eq(&gen_info->addr, &par_info->addr));
    tt_int_op(gen_info->port,OP_EQ, par_info->port);
  }

  rend_service_descriptor_free(parsed);
  rend_service_descriptor_free(generated);
  parsed = generated = NULL;

 done:
  if (descs) {
    for (i = 0; i < smartlist_len(descs); i++)
      rend_encoded_v2_service_descriptor_free(smartlist_get(descs, i));
    smartlist_free(descs);
  }
  if (parsed)
    rend_service_descriptor_free(parsed);
  if (generated)
    rend_service_descriptor_free(generated);
  if (pk1)
    crypto_pk_free(pk1);
  if (pk2)
    crypto_pk_free(pk2);
  tor_free(intro_points_encrypted);
}

  /* Record odd numbered fake-IPs using ipv6, even numbered fake-IPs
   * using ipv4.  Since our fake geoip database is the same between
   * ipv4 and ipv6, we should get the same result no matter which
   * address family we pick for each IP. */
#define SET_TEST_ADDRESS(i) do {                \
    if ((i) & 1) {                              \
      SET_TEST_IPV6(i);                         \
      tor_addr_from_in6(&addr, &in6);           \
    } else {                                    \
      tor_addr_from_ipv4h(&addr, (uint32_t) i); \
    }                                           \
  } while (0)

  /* Make sure that country ID actually works. */
#define SET_TEST_IPV6(i) \
  do {                                                          \
    set_uint32(in6.s6_addr + 12, htonl((uint32_t) (i)));        \
  } while (0)
#define CHECK_COUNTRY(country, val) do {                                \
    /* test ipv4 country lookup */                                      \
    tt_str_op(country, OP_EQ,                                              \
               geoip_get_country_name(geoip_get_country_by_ipv4(val))); \
    /* test ipv6 country lookup */                                      \
    SET_TEST_IPV6(val);                                                 \
    tt_str_op(country, OP_EQ,                                              \
               geoip_get_country_name(geoip_get_country_by_ipv6(&in6))); \
  } while (0)

/** Run unit tests for GeoIP code. */
static void
test_geoip(void *arg)
{
  int i, j;
  time_t now = 1281533250; /* 2010-08-11 13:27:30 UTC */
  char *s = NULL, *v = NULL;
  const char *bridge_stats_1 =
      "bridge-stats-end 2010-08-12 13:27:30 (86400 s)\n"
      "bridge-ips zz=24,xy=8\n"
      "bridge-ip-versions v4=16,v6=16\n"
      "bridge-ip-transports <OR>=24\n",
  *dirreq_stats_1 =
      "dirreq-stats-end 2010-08-12 13:27:30 (86400 s)\n"
      "dirreq-v3-ips ab=8\n"
      "dirreq-v3-reqs ab=8\n"
      "dirreq-v3-resp ok=0,not-enough-sigs=0,unavailable=0,not-found=0,"
          "not-modified=0,busy=0\n"
      "dirreq-v3-direct-dl complete=0,timeout=0,running=0\n"
      "dirreq-v3-tunneled-dl complete=0,timeout=0,running=0\n",
  *dirreq_stats_2 =
      "dirreq-stats-end 2010-08-12 13:27:30 (86400 s)\n"
      "dirreq-v3-ips \n"
      "dirreq-v3-reqs \n"
      "dirreq-v3-resp ok=0,not-enough-sigs=0,unavailable=0,not-found=0,"
          "not-modified=0,busy=0\n"
      "dirreq-v3-direct-dl complete=0,timeout=0,running=0\n"
      "dirreq-v3-tunneled-dl complete=0,timeout=0,running=0\n",
  *dirreq_stats_3 =
      "dirreq-stats-end 2010-08-12 13:27:30 (86400 s)\n"
      "dirreq-v3-ips \n"
      "dirreq-v3-reqs \n"
      "dirreq-v3-resp ok=8,not-enough-sigs=0,unavailable=0,not-found=0,"
          "not-modified=0,busy=0\n"
      "dirreq-v3-direct-dl complete=0,timeout=0,running=0\n"
      "dirreq-v3-tunneled-dl complete=0,timeout=0,running=0\n",
  *dirreq_stats_4 =
      "dirreq-stats-end 2010-08-12 13:27:30 (86400 s)\n"
      "dirreq-v3-ips \n"
      "dirreq-v3-reqs \n"
      "dirreq-v3-resp ok=8,not-enough-sigs=0,unavailable=0,not-found=0,"
          "not-modified=0,busy=0\n"
      "dirreq-v3-direct-dl complete=0,timeout=0,running=0\n"
      "dirreq-v3-tunneled-dl complete=0,timeout=0,running=4\n",
  *entry_stats_1 =
      "entry-stats-end 2010-08-12 13:27:30 (86400 s)\n"
      "entry-ips ab=8\n",
  *entry_stats_2 =
      "entry-stats-end 2010-08-12 13:27:30 (86400 s)\n"
      "entry-ips \n";
  tor_addr_t addr;
  struct in6_addr in6;

  /* Populate the DB a bit.  Add these in order, since we can't do the final
   * 'sort' step.  These aren't very good IP addresses, but they're perfectly
   * fine uint32_t values. */
  (void)arg;
  tt_int_op(0,OP_EQ, geoip_parse_entry("10,50,AB", AF_INET));
  tt_int_op(0,OP_EQ, geoip_parse_entry("52,90,XY", AF_INET));
  tt_int_op(0,OP_EQ, geoip_parse_entry("95,100,AB", AF_INET));
  tt_int_op(0,OP_EQ, geoip_parse_entry("\"105\",\"140\",\"ZZ\"", AF_INET));
  tt_int_op(0,OP_EQ, geoip_parse_entry("\"150\",\"190\",\"XY\"", AF_INET));
  tt_int_op(0,OP_EQ, geoip_parse_entry("\"200\",\"250\",\"AB\"", AF_INET));

  /* Populate the IPv6 DB equivalently with fake IPs in the same range */
  tt_int_op(0,OP_EQ, geoip_parse_entry("::a,::32,AB", AF_INET6));
  tt_int_op(0,OP_EQ, geoip_parse_entry("::34,::5a,XY", AF_INET6));
  tt_int_op(0,OP_EQ, geoip_parse_entry("::5f,::64,AB", AF_INET6));
  tt_int_op(0,OP_EQ, geoip_parse_entry("::69,::8c,ZZ", AF_INET6));
  tt_int_op(0,OP_EQ, geoip_parse_entry("::96,::be,XY", AF_INET6));
  tt_int_op(0,OP_EQ, geoip_parse_entry("::c8,::fa,AB", AF_INET6));

  /* We should have 4 countries: ??, ab, xy, zz. */
  tt_int_op(4,OP_EQ, geoip_get_n_countries());
  memset(&in6, 0, sizeof(in6));

  CHECK_COUNTRY("??", 3);
  CHECK_COUNTRY("ab", 32);
  CHECK_COUNTRY("??", 5);
  CHECK_COUNTRY("??", 51);
  CHECK_COUNTRY("xy", 150);
  CHECK_COUNTRY("xy", 190);
  CHECK_COUNTRY("??", 2000);

  tt_int_op(0,OP_EQ, geoip_get_country_by_ipv4(3));
  SET_TEST_IPV6(3);
  tt_int_op(0,OP_EQ, geoip_get_country_by_ipv6(&in6));

  get_options_mutable()->BridgeRelay = 1;
  get_options_mutable()->BridgeRecordUsageByCountry = 1;
  /* Put 9 observations in AB... */
  for (i=32; i < 40; ++i) {
    SET_TEST_ADDRESS(i);
    geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now-7200);
  }
  SET_TEST_ADDRESS(225);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now-7200);
  /* and 3 observations in XY, several times. */
  for (j=0; j < 10; ++j)
    for (i=52; i < 55; ++i) {
      SET_TEST_ADDRESS(i);
      geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now-3600);
    }
  /* and 17 observations in ZZ... */
  for (i=110; i < 127; ++i) {
    SET_TEST_ADDRESS(i);
    geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now);
  }
  geoip_get_client_history(GEOIP_CLIENT_CONNECT, &s, &v);
  tt_assert(s);
  tt_assert(v);
  tt_str_op("zz=24,ab=16,xy=8",OP_EQ, s);
  tt_str_op("v4=16,v6=16",OP_EQ, v);
  tor_free(s);
  tor_free(v);

  /* Now clear out all the AB observations. */
  geoip_remove_old_clients(now-6000);
  geoip_get_client_history(GEOIP_CLIENT_CONNECT, &s, &v);
  tt_assert(s);
  tt_assert(v);
  tt_str_op("zz=24,xy=8",OP_EQ, s);
  tt_str_op("v4=16,v6=16",OP_EQ, v);
  tor_free(s);
  tor_free(v);

  /* Start testing bridge statistics by making sure that we don't output
   * bridge stats without initializing them. */
  s = geoip_format_bridge_stats(now + 86400);
  tt_assert(!s);

  /* Initialize stats and generate the bridge-stats history string out of
   * the connecting clients added above. */
  geoip_bridge_stats_init(now);
  s = geoip_format_bridge_stats(now + 86400);
  tt_assert(s);
  tt_str_op(bridge_stats_1,OP_EQ, s);
  tor_free(s);

  /* Stop collecting bridge stats and make sure we don't write a history
   * string anymore. */
  geoip_bridge_stats_term();
  s = geoip_format_bridge_stats(now + 86400);
  tt_assert(!s);

  /* Stop being a bridge and start being a directory mirror that gathers
   * directory request statistics. */
  geoip_bridge_stats_term();
  get_options_mutable()->BridgeRelay = 0;
  get_options_mutable()->BridgeRecordUsageByCountry = 0;
  get_options_mutable()->DirReqStatistics = 1;

  /* Start testing dirreq statistics by making sure that we don't collect
   * dirreq stats without initializing them. */
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_NETWORKSTATUS, &addr, NULL, now);
  s = geoip_format_dirreq_stats(now + 86400);
  tt_assert(!s);

  /* Initialize stats, note one connecting client, and generate the
   * dirreq-stats history string. */
  geoip_dirreq_stats_init(now);
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_NETWORKSTATUS, &addr, NULL, now);
  s = geoip_format_dirreq_stats(now + 86400);
  tt_str_op(dirreq_stats_1,OP_EQ, s);
  tor_free(s);

  /* Stop collecting stats, add another connecting client, and ensure we
   * don't generate a history string. */
  geoip_dirreq_stats_term();
  SET_TEST_ADDRESS(101);
  geoip_note_client_seen(GEOIP_CLIENT_NETWORKSTATUS, &addr, NULL, now);
  s = geoip_format_dirreq_stats(now + 86400);
  tt_assert(!s);

  /* Re-start stats, add a connecting client, reset stats, and make sure
   * that we get an all empty history string. */
  geoip_dirreq_stats_init(now);
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_NETWORKSTATUS, &addr, NULL, now);
  geoip_reset_dirreq_stats(now);
  s = geoip_format_dirreq_stats(now + 86400);
  tt_str_op(dirreq_stats_2,OP_EQ, s);
  tor_free(s);

  /* Note a successful network status response and make sure that it
   * appears in the history string. */
  geoip_note_ns_response(GEOIP_SUCCESS);
  s = geoip_format_dirreq_stats(now + 86400);
  tt_str_op(dirreq_stats_3,OP_EQ, s);
  tor_free(s);

  /* Start a tunneled directory request. */
  geoip_start_dirreq((uint64_t) 1, 1024, DIRREQ_TUNNELED);
  s = geoip_format_dirreq_stats(now + 86400);
  tt_str_op(dirreq_stats_4,OP_EQ, s);
  tor_free(s);

  /* Stop collecting directory request statistics and start gathering
   * entry stats. */
  geoip_dirreq_stats_term();
  get_options_mutable()->DirReqStatistics = 0;
  get_options_mutable()->EntryStatistics = 1;

  /* Start testing entry statistics by making sure that we don't collect
   * anything without initializing entry stats. */
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now);
  s = geoip_format_entry_stats(now + 86400);
  tt_assert(!s);

  /* Initialize stats, note one connecting client, and generate the
   * entry-stats history string. */
  geoip_entry_stats_init(now);
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now);
  s = geoip_format_entry_stats(now + 86400);
  tt_str_op(entry_stats_1,OP_EQ, s);
  tor_free(s);

  /* Stop collecting stats, add another connecting client, and ensure we
   * don't generate a history string. */
  geoip_entry_stats_term();
  SET_TEST_ADDRESS(101);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now);
  s = geoip_format_entry_stats(now + 86400);
  tt_assert(!s);

  /* Re-start stats, add a connecting client, reset stats, and make sure
   * that we get an all empty history string. */
  geoip_entry_stats_init(now);
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now);
  geoip_reset_entry_stats(now);
  s = geoip_format_entry_stats(now + 86400);
  tt_str_op(entry_stats_2,OP_EQ, s);
  tor_free(s);

  /* Stop collecting entry statistics. */
  geoip_entry_stats_term();
  get_options_mutable()->EntryStatistics = 0;

 done:
  tor_free(s);
  tor_free(v);
}

static void
test_geoip_with_pt(void *arg)
{
  time_t now = 1281533250; /* 2010-08-11 13:27:30 UTC */
  char *s = NULL;
  int i;
  tor_addr_t addr;
  struct in6_addr in6;

  (void)arg;
  get_options_mutable()->BridgeRelay = 1;
  get_options_mutable()->BridgeRecordUsageByCountry = 1;

  memset(&in6, 0, sizeof(in6));

  /* No clients seen yet. */
  s = geoip_get_transport_history();
  tor_assert(!s);

  /* 4 connections without a pluggable transport */
  for (i=0; i < 4; ++i) {
    SET_TEST_ADDRESS(i);
    geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now-7200);
  }

  /* 9 connections with "alpha" */
  for (i=4; i < 13; ++i) {
    SET_TEST_ADDRESS(i);
    geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, "alpha", now-7200);
  }

  /* one connection with "beta" */
  SET_TEST_ADDRESS(13);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, "beta", now-7200);

  /* 14 connections with "charlie" */
  for (i=14; i < 28; ++i) {
    SET_TEST_ADDRESS(i);
    geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, "charlie", now-7200);
  }

  /* 131 connections with "ddr" */
  for (i=28; i < 159; ++i) {
    SET_TEST_ADDRESS(i);
    geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, "ddr", now-7200);
  }

  /* 8 connections with "entropy" */
  for (i=159; i < 167; ++i) {
    SET_TEST_ADDRESS(i);
    geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, "entropy", now-7200);
  }

  /* 2 connections from the same IP with two different transports. */
  SET_TEST_ADDRESS(++i);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, "fire", now-7200);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, "google", now-7200);

  /* Test the transport history string. */
  s = geoip_get_transport_history();
  tor_assert(s);
  tt_str_op(s,OP_EQ, "<OR>=8,alpha=16,beta=8,charlie=16,ddr=136,"
             "entropy=8,fire=8,google=8");

  /* Stop collecting entry statistics. */
  geoip_entry_stats_term();
  get_options_mutable()->EntryStatistics = 0;

 done:
  tor_free(s);
}

#undef SET_TEST_ADDRESS
#undef SET_TEST_IPV6
#undef CHECK_COUNTRY

/** Run unit tests for stats code. */
static void
test_stats(void *arg)
{
  time_t now = 1281533250; /* 2010-08-11 13:27:30 UTC */
  char *s = NULL;
  int i;

  /* Start with testing exit port statistics; we shouldn't collect exit
   * stats without initializing them. */
  (void)arg;
  rep_hist_note_exit_stream_opened(80);
  rep_hist_note_exit_bytes(80, 100, 10000);
  s = rep_hist_format_exit_stats(now + 86400);
  tt_assert(!s);

  /* Initialize stats, note some streams and bytes, and generate history
   * string. */
  rep_hist_exit_stats_init(now);
  rep_hist_note_exit_stream_opened(80);
  rep_hist_note_exit_bytes(80, 100, 10000);
  rep_hist_note_exit_stream_opened(443);
  rep_hist_note_exit_bytes(443, 100, 10000);
  rep_hist_note_exit_bytes(443, 100, 10000);
  s = rep_hist_format_exit_stats(now + 86400);
  tt_str_op("exit-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "exit-kibibytes-written 80=1,443=1,other=0\n"
             "exit-kibibytes-read 80=10,443=20,other=0\n"
             "exit-streams-opened 80=4,443=4,other=0\n",OP_EQ, s);
  tor_free(s);

  /* Add a few bytes on 10 more ports and ensure that only the top 10
   * ports are contained in the history string. */
  for (i = 50; i < 60; i++) {
    rep_hist_note_exit_bytes(i, i, i);
    rep_hist_note_exit_stream_opened(i);
  }
  s = rep_hist_format_exit_stats(now + 86400);
  tt_str_op("exit-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "exit-kibibytes-written 52=1,53=1,54=1,55=1,56=1,57=1,58=1,"
             "59=1,80=1,443=1,other=1\n"
             "exit-kibibytes-read 52=1,53=1,54=1,55=1,56=1,57=1,58=1,"
             "59=1,80=10,443=20,other=1\n"
             "exit-streams-opened 52=4,53=4,54=4,55=4,56=4,57=4,58=4,"
             "59=4,80=4,443=4,other=4\n",OP_EQ, s);
  tor_free(s);

  /* Stop collecting stats, add some bytes, and ensure we don't generate
   * a history string. */
  rep_hist_exit_stats_term();
  rep_hist_note_exit_bytes(80, 100, 10000);
  s = rep_hist_format_exit_stats(now + 86400);
  tt_assert(!s);

  /* Re-start stats, add some bytes, reset stats, and see what history we
   * get when observing no streams or bytes at all. */
  rep_hist_exit_stats_init(now);
  rep_hist_note_exit_stream_opened(80);
  rep_hist_note_exit_bytes(80, 100, 10000);
  rep_hist_reset_exit_stats(now);
  s = rep_hist_format_exit_stats(now + 86400);
  tt_str_op("exit-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "exit-kibibytes-written other=0\n"
             "exit-kibibytes-read other=0\n"
             "exit-streams-opened other=0\n",OP_EQ, s);
  tor_free(s);

  /* Continue with testing connection statistics; we shouldn't collect
   * conn stats without initializing them. */
  rep_hist_note_or_conn_bytes(1, 20, 400, now);
  s = rep_hist_format_conn_stats(now + 86400);
  tt_assert(!s);

  /* Initialize stats, note bytes, and generate history string. */
  rep_hist_conn_stats_init(now);
  rep_hist_note_or_conn_bytes(1, 30000, 400000, now);
  rep_hist_note_or_conn_bytes(1, 30000, 400000, now + 5);
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 10);
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 15);
  s = rep_hist_format_conn_stats(now + 86400);
  tt_str_op("conn-bi-direct 2010-08-12 13:27:30 (86400 s) 0,0,1,0\n",OP_EQ, s);
  tor_free(s);

  /* Stop collecting stats, add some bytes, and ensure we don't generate
   * a history string. */
  rep_hist_conn_stats_term();
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 15);
  s = rep_hist_format_conn_stats(now + 86400);
  tt_assert(!s);

  /* Re-start stats, add some bytes, reset stats, and see what history we
   * get when observing no bytes at all. */
  rep_hist_conn_stats_init(now);
  rep_hist_note_or_conn_bytes(1, 30000, 400000, now);
  rep_hist_note_or_conn_bytes(1, 30000, 400000, now + 5);
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 10);
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 15);
  rep_hist_reset_conn_stats(now);
  s = rep_hist_format_conn_stats(now + 86400);
  tt_str_op("conn-bi-direct 2010-08-12 13:27:30 (86400 s) 0,0,0,0\n",OP_EQ, s);
  tor_free(s);

  /* Continue with testing buffer statistics; we shouldn't collect buffer
   * stats without initializing them. */
  rep_hist_add_buffer_stats(2.0, 2.0, 20);
  s = rep_hist_format_buffer_stats(now + 86400);
  tt_assert(!s);

  /* Initialize stats, add statistics for a single circuit, and generate
   * the history string. */
  rep_hist_buffer_stats_init(now);
  rep_hist_add_buffer_stats(2.0, 2.0, 20);
  s = rep_hist_format_buffer_stats(now + 86400);
  tt_str_op("cell-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "cell-processed-cells 20,0,0,0,0,0,0,0,0,0\n"
             "cell-queued-cells 2.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,"
                               "0.00,0.00\n"
             "cell-time-in-queue 2,0,0,0,0,0,0,0,0,0\n"
             "cell-circuits-per-decile 1\n",OP_EQ, s);
  tor_free(s);

  /* Add nineteen more circuit statistics to the one that's already in the
   * history to see that the math works correctly. */
  for (i = 21; i < 30; i++)
    rep_hist_add_buffer_stats(2.0, 2.0, i);
  for (i = 20; i < 30; i++)
    rep_hist_add_buffer_stats(3.5, 3.5, i);
  s = rep_hist_format_buffer_stats(now + 86400);
  tt_str_op("cell-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "cell-processed-cells 29,28,27,26,25,24,23,22,21,20\n"
             "cell-queued-cells 2.75,2.75,2.75,2.75,2.75,2.75,2.75,2.75,"
                               "2.75,2.75\n"
             "cell-time-in-queue 3,3,3,3,3,3,3,3,3,3\n"
             "cell-circuits-per-decile 2\n",OP_EQ, s);
  tor_free(s);

  /* Stop collecting stats, add statistics for one circuit, and ensure we
   * don't generate a history string. */
  rep_hist_buffer_stats_term();
  rep_hist_add_buffer_stats(2.0, 2.0, 20);
  s = rep_hist_format_buffer_stats(now + 86400);
  tt_assert(!s);

  /* Re-start stats, add statistics for one circuit, reset stats, and make
   * sure that the history has all zeros. */
  rep_hist_buffer_stats_init(now);
  rep_hist_add_buffer_stats(2.0, 2.0, 20);
  rep_hist_reset_buffer_stats(now);
  s = rep_hist_format_buffer_stats(now + 86400);
  tt_str_op("cell-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "cell-processed-cells 0,0,0,0,0,0,0,0,0,0\n"
             "cell-queued-cells 0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,"
                               "0.00,0.00\n"
             "cell-time-in-queue 0,0,0,0,0,0,0,0,0,0\n"
             "cell-circuits-per-decile 0\n",OP_EQ, s);

 done:
  tor_free(s);
}

#define ENT(name)                                                       \
  { #name, test_ ## name , 0, NULL, NULL }
#define FORK(name)                                                      \
  { #name, test_ ## name , TT_FORK, NULL, NULL }

static struct testcase_t test_array[] = {
  ENT(onion_handshake),
  { "bad_onion_handshake", test_bad_onion_handshake, 0, NULL, NULL },
  ENT(onion_queues),
  { "ntor_handshake", test_ntor_handshake, 0, NULL, NULL },
  { "fast_handshake", test_fast_handshake, 0, NULL, NULL },
  FORK(circuit_timeout),
  FORK(rend_fns),
  ENT(geoip),
  FORK(geoip_with_pt),
  FORK(stats),

  END_OF_TESTCASES
};

struct testgroup_t testgroups[] = {
  { "", test_array },
  { "accounting/", accounting_tests },
  { "addr/", addr_tests },
  { "address/", address_tests },
  { "buffer/", buffer_tests },
  { "cellfmt/", cell_format_tests },
  { "cellqueue/", cell_queue_tests },
  { "channel/", channel_tests },
  { "channeltls/", channeltls_tests },
  { "checkdir/", checkdir_tests },
  { "circuitlist/", circuitlist_tests },
  { "circuitmux/", circuitmux_tests },
  { "compat/libevent/", compat_libevent_tests },
  { "config/", config_tests },
  { "connection/", connection_tests },
  { "container/", container_tests },
  { "control/", controller_tests },
  { "control/event/", controller_event_tests },
  { "crypto/", crypto_tests },
  { "dir/", dir_tests },
  { "dir_handle_get/", dir_handle_get_tests },
  { "dir/md/", microdesc_tests },
  { "entryconn/", entryconn_tests },
  { "entrynodes/", entrynodes_tests },
  { "guardfraction/", guardfraction_tests },
  { "extorport/", extorport_tests },
  { "hs/", hs_tests },
  { "introduce/", introduce_tests },
  { "keypin/", keypin_tests },
  { "link-handshake/", link_handshake_tests },
  { "nodelist/", nodelist_tests },
  { "oom/", oom_tests },
  { "oos/", oos_tests },
  { "options/", options_tests },
  { "policy/" , policy_tests },
  { "procmon/", procmon_tests },
  { "protover/", protover_tests },
  { "pt/", pt_tests },
  { "relay/" , relay_tests },
  { "relaycell/", relaycell_tests },
  { "rend_cache/", rend_cache_tests },
  { "replaycache/", replaycache_tests },
  { "routerkeys/", routerkeys_tests },
  { "routerlist/", routerlist_tests },
  { "routerset/" , routerset_tests },
  { "scheduler/", scheduler_tests },
  { "socks/", socks_tests },
  { "shared-random/", sr_tests },
  { "status/" , status_tests },
  { "tortls/", tortls_tests },
  { "util/", util_tests },
  { "util/format/", util_format_tests },
  { "util/logging/", logging_tests },
  { "util/process/", util_process_tests },
  { "util/pubsub/", pubsub_tests },
  { "util/thread/", thread_tests },
  { "util/handle/", handle_tests },
  { "dns/", dns_tests },
  END_OF_GROUPS
};

