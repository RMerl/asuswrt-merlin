/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/* Ordinarily defined in tor_main.c; this bit is just here to provide one
 * since we're not linking to tor_main.c */
const char tor_git_revision[] = "";

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
#define STATEFILE_PRIVATE

/*
 * Linux doesn't provide lround in math.h by default, but mac os does...
 * It's best just to leave math.h out of the picture entirely.
 */
//#include <math.h>
long int lround(double x);
double fabs(double x);

#include "or.h"
#include "buffers.h"
#include "circuitlist.h"
#include "circuitstats.h"
#include "config.h"
#include "connection_edge.h"
#include "geoip.h"
#include "rendcommon.h"
#include "test.h"
#include "torgzip.h"
#ifdef ENABLE_MEMPOOLS
#include "mempool.h"
#endif
#include "memarea.h"
#include "onion.h"
#include "onion_ntor.h"
#include "onion_tap.h"
#include "policies.h"
#include "rephist.h"
#include "routerparse.h"
#include "statefile.h"
#ifdef CURVE25519_ENABLED
#include "crypto_curve25519.h"
#include "onion_ntor.h"
#endif

#ifdef USE_DMALLOC
#include <dmalloc.h>
#include <openssl/crypto.h>
#include "main.h"
#endif

/** Set to true if any unit test has failed.  Mostly, this is set by the macros
 * in test.h */
int have_failed = 0;

/** Temporary directory (set up by setup_directory) under which we store all
 * our files during testing. */
static char temp_dir[256];
#ifdef _WIN32
#define pid_t int
#endif
static pid_t temp_dir_setup_in_pid = 0;

/** Select and create the temporary directory we'll use to run our unit tests.
 * Store it in <b>temp_dir</b>.  Exit immediately if we can't create it.
 * idempotent. */
static void
setup_directory(void)
{
  static int is_setup = 0;
  int r;
  char rnd[256], rnd32[256];
  if (is_setup) return;

/* Due to base32 limitation needs to be a multiple of 5. */
#define RAND_PATH_BYTES 5
  crypto_rand(rnd, RAND_PATH_BYTES);
  base32_encode(rnd32, sizeof(rnd32), rnd, RAND_PATH_BYTES);

#ifdef _WIN32
  {
    char buf[MAX_PATH];
    const char *tmp = buf;
    /* If this fails, we're probably screwed anyway */
    if (!GetTempPathA(sizeof(buf),buf))
      tmp = "c:\\windows\\temp";
    tor_snprintf(temp_dir, sizeof(temp_dir),
                 "%s\\tor_test_%d_%s", tmp, (int)getpid(), rnd32);
    r = mkdir(temp_dir);
  }
#else
  tor_snprintf(temp_dir, sizeof(temp_dir), "/tmp/tor_test_%d_%s",
               (int) getpid(), rnd32);
  r = mkdir(temp_dir, 0700);
#endif
  if (r) {
    fprintf(stderr, "Can't create directory %s:", temp_dir);
    perror("");
    exit(1);
  }
  is_setup = 1;
  temp_dir_setup_in_pid = getpid();
}

/** Return a filename relative to our testing temporary directory */
const char *
get_fname(const char *name)
{
  static char buf[1024];
  setup_directory();
  if (!name)
    return temp_dir;
  tor_snprintf(buf,sizeof(buf),"%s/%s",temp_dir,name);
  return buf;
}

/* Remove a directory and all of its subdirectories */
static void
rm_rf(const char *dir)
{
  struct stat st;
  smartlist_t *elements;

  elements = tor_listdir(dir);
  if (elements) {
    SMARTLIST_FOREACH_BEGIN(elements, const char *, cp) {
         char *tmp = NULL;
         tor_asprintf(&tmp, "%s"PATH_SEPARATOR"%s", dir, cp);
         if (0 == stat(tmp,&st) && (st.st_mode & S_IFDIR)) {
           rm_rf(tmp);
         } else {
           if (unlink(tmp)) {
             fprintf(stderr, "Error removing %s: %s\n", tmp, strerror(errno));
           }
         }
         tor_free(tmp);
    } SMARTLIST_FOREACH_END(cp);
    SMARTLIST_FOREACH(elements, char *, cp, tor_free(cp));
    smartlist_free(elements);
  }
  if (rmdir(dir))
    fprintf(stderr, "Error removing directory %s: %s\n", dir, strerror(errno));
}

/** Remove all files stored under the temporary directory, and the directory
 * itself.  Called by atexit(). */
static void
remove_directory(void)
{
  if (getpid() != temp_dir_setup_in_pid) {
    /* Only clean out the tempdir when the main process is exiting. */
    return;
  }

  rm_rf(temp_dir);
}

/** Define this if unit tests spend too much time generating public keys*/
#undef CACHE_GENERATED_KEYS

static crypto_pk_t *pregen_keys[5] = {NULL, NULL, NULL, NULL, NULL};
#define N_PREGEN_KEYS ((int)(sizeof(pregen_keys)/sizeof(pregen_keys[0])))

/** Generate and return a new keypair for use in unit tests.  If we're using
 * the key cache optimization, we might reuse keys: we only guarantee that
 * keys made with distinct values for <b>idx</b> are different.  The value of
 * <b>idx</b> must be at least 0, and less than N_PREGEN_KEYS. */
crypto_pk_t *
pk_generate(int idx)
{
#ifdef CACHE_GENERATED_KEYS
  tor_assert(idx < N_PREGEN_KEYS);
  if (! pregen_keys[idx]) {
    pregen_keys[idx] = crypto_pk_new();
    tor_assert(!crypto_pk_generate_key(pregen_keys[idx]));
  }
  return crypto_pk_dup_key(pregen_keys[idx]);
#else
  crypto_pk_t *result;
  (void) idx;
  result = crypto_pk_new();
  tor_assert(!crypto_pk_generate_key(result));
  return result;
#endif
}

/** Free all storage used for the cached key optimization. */
static void
free_pregenerated_keys(void)
{
  unsigned idx;
  for (idx = 0; idx < N_PREGEN_KEYS; ++idx) {
    if (pregen_keys[idx]) {
      crypto_pk_free(pregen_keys[idx]);
      pregen_keys[idx] = NULL;
    }
  }
}

/** Run unit tests for the onion handshake code. */
static void
test_onion_handshake(void)
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

  pk = pk_generate(0);
  pk2 = pk_generate(1);

  /* client handshake 1. */
  memset(c_buf, 0, TAP_ONIONSKIN_CHALLENGE_LEN);
  test_assert(! onion_skin_TAP_create(pk, &c_dh, c_buf));

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
    test_assert(! onion_skin_TAP_server_handshake(c_buf, k1, k2,
                                                  s_buf, s_keys, 40));

    /* client handshake 2 */
    memset(c_keys, 0, 40);
    test_assert(! onion_skin_TAP_client_handshake(c_dh, s_buf, c_keys, 40));

    test_memeq(c_keys, s_keys, 40);
    memset(s_buf, 0, 40);
    test_memneq(c_keys, s_buf, 40);
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
  tt_int_op(-1, ==,
            onion_skin_TAP_server_handshake(junk_buf2, pk, NULL,
                                            s_buf, s_keys, 40));

  /* Server: Case 2: the encrypted data is not long enough. */
  memset(junk_buf, 0, sizeof(junk_buf));
  memset(junk_buf2, 0, sizeof(junk_buf2));
  crypto_pk_public_encrypt(pk, junk_buf2, sizeof(junk_buf2),
                               junk_buf, 48, PK_PKCS1_OAEP_PADDING);
  tt_int_op(-1, ==,
            onion_skin_TAP_server_handshake(junk_buf2, pk, NULL,
                                            s_buf, s_keys, 40));

  /* client handshake 1: do it straight. */
  memset(c_buf, 0, TAP_ONIONSKIN_CHALLENGE_LEN);
  test_assert(! onion_skin_TAP_create(pk, &c_dh, c_buf));

  /* Server: Case 3: we just don't have the right key. */
  tt_int_op(-1, ==,
            onion_skin_TAP_server_handshake(c_buf, pk2, NULL,
                                            s_buf, s_keys, 40));

  /* Server: Case 4: The RSA-encrypted portion is corrupt. */
  c_buf[64] ^= 33;
  tt_int_op(-1, ==,
            onion_skin_TAP_server_handshake(c_buf, pk, NULL,
                                            s_buf, s_keys, 40));
  c_buf[64] ^= 33;

  /* (Let the server procede) */
  tt_int_op(0, ==,
            onion_skin_TAP_server_handshake(c_buf, pk, NULL,
                                            s_buf, s_keys, 40));

  /* Client: Case 1: The server sent back junk. */
  s_buf[64] ^= 33;
  tt_int_op(-1, ==,
            onion_skin_TAP_client_handshake(c_dh, s_buf, c_keys, 40));
  s_buf[64] ^= 33;

  /* Let the client finish; make sure it can. */
  tt_int_op(0, ==,
            onion_skin_TAP_client_handshake(c_dh, s_buf, c_keys, 40));
  test_memeq(s_keys, c_keys, 40);

  /* Client: Case 2: The server sent back a degenerate DH. */
  memset(s_buf, 0, sizeof(s_buf));
  tt_int_op(-1, ==,
            onion_skin_TAP_client_handshake(c_dh, s_buf, c_keys, 40));

 done:
  crypto_dh_free(c_dh);
  crypto_pk_free(pk);
  crypto_pk_free(pk2);
}

#ifdef CURVE25519_ENABLED
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
  tt_int_op(0, ==, onion_skin_ntor_create(node_id, server_pubkey,
                                          &c_state, c_buf));

  /* server handshake */
  memset(s_buf, 0, NTOR_REPLY_LEN);
  memset(s_keys, 0, 40);
  tt_int_op(0, ==, onion_skin_ntor_server_handshake(c_buf, s_keymap, NULL,
                                                    node_id,
                                                    s_buf, s_keys, 400));

  /* client handshake 2 */
  memset(c_keys, 0, 40);
  tt_int_op(0, ==, onion_skin_ntor_client_handshake(c_state, s_buf,
                                                    c_keys, 400));

  test_memeq(c_keys, s_keys, 400);
  memset(s_buf, 0, 40);
  test_memneq(c_keys, s_buf, 40);

 done:
  ntor_handshake_state_free(c_state);
  dimap_free(s_keymap, NULL);
}
#endif

/** Run unit tests for the onion queues. */
static void
test_onion_queues(void)
{
  uint8_t buf1[TAP_ONIONSKIN_CHALLENGE_LEN] = {0};
  uint8_t buf2[NTOR_ONIONSKIN_LEN] = {0};

  or_circuit_t *circ1 = or_circuit_new(0, NULL);
  or_circuit_t *circ2 = or_circuit_new(0, NULL);

  create_cell_t *onionskin = NULL, *create2_ptr;
  create_cell_t *create1 = tor_malloc_zero(sizeof(create_cell_t));
  create_cell_t *create2 = tor_malloc_zero(sizeof(create_cell_t));
  create2_ptr = create2; /* remember, but do not free */

  create_cell_init(create1, CELL_CREATE, ONION_HANDSHAKE_TYPE_TAP,
                   TAP_ONIONSKIN_CHALLENGE_LEN, buf1);
  create_cell_init(create2, CELL_CREATE, ONION_HANDSHAKE_TYPE_NTOR,
                   NTOR_ONIONSKIN_LEN, buf2);

  test_eq(0, onion_num_pending(ONION_HANDSHAKE_TYPE_TAP));
  test_eq(0, onion_pending_add(circ1, create1));
  create1 = NULL;
  test_eq(1, onion_num_pending(ONION_HANDSHAKE_TYPE_TAP));

  test_eq(0, onion_num_pending(ONION_HANDSHAKE_TYPE_NTOR));
  test_eq(0, onion_pending_add(circ2, create2));
  create2 = NULL;
  test_eq(1, onion_num_pending(ONION_HANDSHAKE_TYPE_NTOR));

  test_eq_ptr(circ2, onion_next_task(&onionskin));
  test_eq(1, onion_num_pending(ONION_HANDSHAKE_TYPE_TAP));
  test_eq(0, onion_num_pending(ONION_HANDSHAKE_TYPE_NTOR));
  tt_ptr_op(onionskin, ==, create2_ptr);

  clear_pending_onions();
  test_eq(0, onion_num_pending(ONION_HANDSHAKE_TYPE_TAP));
  test_eq(0, onion_num_pending(ONION_HANDSHAKE_TYPE_NTOR));

 done:
  circuit_free(TO_CIRCUIT(circ1));
  circuit_free(TO_CIRCUIT(circ2));
  tor_free(create1);
  tor_free(create2);
  tor_free(onionskin);
}

static void
test_circuit_timeout(void)
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

  test_assert(estimate.total_build_times <= CBT_NCIRCUITS_TO_OBSERVE);

  circuit_build_times_update_state(&estimate, state);
  circuit_build_times_free_timeouts(&final);
  test_assert(circuit_build_times_parse_state(&final, state) == 0);

  circuit_build_times_update_alpha(&final);
  timeout2 = circuit_build_times_calculate_timeout(&final,
                                 CBT_DEFAULT_QUANTILE_CUTOFF/100.0);

  circuit_build_times_set_timeout(&final);
  log_notice(LD_CIRC, "Timeout2 is %f, Xm is %d", timeout2, final.Xm);

  /* 5% here because some accuracy is lost due to histogram conversion */
  test_assert(fabs(circuit_build_times_cdf(&initial, timeout0) -
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

    test_assert(!circuit_build_times_network_check_changed(&estimate));
    test_assert(!circuit_build_times_network_check_changed(&final));

    /* Reset liveness to be non-live */
    final.liveness.network_last_live = 0;
    estimate.liveness.network_last_live = 0;

    build_times_idx = estimate.build_times_idx;
    total_build_times = estimate.total_build_times;

    test_assert(circuit_build_times_network_check_live(&estimate));
    test_assert(circuit_build_times_network_check_live(&final));

    circuit_build_times_count_close(&estimate, 0,
            (time_t)(approx_time()-estimate.close_ms/1000.0-1));
    circuit_build_times_count_close(&final, 0,
            (time_t)(approx_time()-final.close_ms/1000.0-1));

    test_assert(!circuit_build_times_network_check_live(&estimate));
    test_assert(!circuit_build_times_network_check_live(&final));

    log_info(LD_CIRC, "idx: %d %d, tot: %d %d",
             build_times_idx, estimate.build_times_idx,
             total_build_times, estimate.total_build_times);

    /* Check rollback index. Should match top of loop. */
    test_assert(build_times_idx == estimate.build_times_idx);
    // This can fail if estimate.total_build_times == 1000, because
    // in that case, rewind actually causes us to lose timeouts
    if (total_build_times != CBT_NCIRCUITS_TO_OBSERVE)
      test_assert(total_build_times == estimate.total_build_times);

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

    test_assert(estimate.liveness.after_firsthop_idx == 0);
    test_assert(final.liveness.after_firsthop_idx ==
                CBT_DEFAULT_MAX_RECENT_TIMEOUT_COUNT-1);

    test_assert(circuit_build_times_network_check_live(&estimate));
    test_assert(circuit_build_times_network_check_live(&final));

    circuit_build_times_count_timeout(&final, 1);
  }

 done:
  circuit_build_times_free_timeouts(&initial);
  circuit_build_times_free_timeouts(&estimate);
  circuit_build_times_free_timeouts(&final);
  or_state_free(state);
}

/** Test encoding and parsing of rendezvous service descriptors. */
static void
test_rend_fns(void)
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

  test_assert(BAD_HOSTNAME == parse_extended_hostname(address1));
  test_assert(ONION_HOSTNAME == parse_extended_hostname(address2));
  test_streq(address2, "aaaaaaaaaaaaaaaa");
  test_assert(EXIT_HOSTNAME == parse_extended_hostname(address3));
  test_assert(NORMAL_HOSTNAME == parse_extended_hostname(address4));
  test_assert(ONION_HOSTNAME == parse_extended_hostname(address5));
  test_streq(address5, "abcdefghijklmnop");
  test_assert(ONION_HOSTNAME == parse_extended_hostname(address6));
  test_streq(address6, "abcdefghijklmnop");
  test_assert(BAD_HOSTNAME == parse_extended_hostname(address7));

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
  test_assert(rend_encode_v2_descriptors(descs, generated, now, 0,
                                         REND_NO_AUTH, NULL, NULL) > 0);
  test_assert(rend_compute_v2_desc_id(computed_desc_id, service_id_base32,
                                      NULL, now, 0) == 0);
  test_memeq(((rend_encoded_v2_service_descriptor_t *)
             smartlist_get(descs, 0))->desc_id, computed_desc_id, DIGEST_LEN);
  test_assert(rend_parse_v2_service_descriptor(&parsed, parsed_desc_id,
                                               &intro_points_encrypted,
                                               &intro_points_size,
                                               &encoded_size,
                                               &next_desc,
                                     ((rend_encoded_v2_service_descriptor_t *)
                                     smartlist_get(descs, 0))->desc_str) == 0);
  test_assert(parsed);
  test_memeq(((rend_encoded_v2_service_descriptor_t *)
             smartlist_get(descs, 0))->desc_id, parsed_desc_id, DIGEST_LEN);
  test_eq(rend_parse_introduction_points(parsed, intro_points_encrypted,
                                         intro_points_size), 3);
  test_assert(!crypto_pk_cmp_keys(generated->pk, parsed->pk));
  test_eq(parsed->timestamp, now);
  test_eq(parsed->version, 2);
  test_eq(parsed->protocols, 42);
  test_eq(smartlist_len(parsed->intro_nodes), 3);
  for (i = 0; i < smartlist_len(parsed->intro_nodes); i++) {
    rend_intro_point_t *par_intro = smartlist_get(parsed->intro_nodes, i),
      *gen_intro = smartlist_get(generated->intro_nodes, i);
    extend_info_t *par_info = par_intro->extend_info;
    extend_info_t *gen_info = gen_intro->extend_info;
    test_assert(!crypto_pk_cmp_keys(gen_info->onion_key, par_info->onion_key));
    test_memeq(gen_info->identity_digest, par_info->identity_digest,
               DIGEST_LEN);
    test_streq(gen_info->nickname, par_info->nickname);
    test_assert(tor_addr_eq(&gen_info->addr, &par_info->addr));
    test_eq(gen_info->port, par_info->port);
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
    test_streq(country,                                                 \
               geoip_get_country_name(geoip_get_country_by_ipv4(val))); \
    /* test ipv6 country lookup */                                      \
    SET_TEST_IPV6(val);                                                 \
    test_streq(country,                                                 \
               geoip_get_country_name(geoip_get_country_by_ipv6(&in6))); \
  } while (0)

/** Run unit tests for GeoIP code. */
static void
test_geoip(void)
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
  test_eq(0, geoip_parse_entry("10,50,AB", AF_INET));
  test_eq(0, geoip_parse_entry("52,90,XY", AF_INET));
  test_eq(0, geoip_parse_entry("95,100,AB", AF_INET));
  test_eq(0, geoip_parse_entry("\"105\",\"140\",\"ZZ\"", AF_INET));
  test_eq(0, geoip_parse_entry("\"150\",\"190\",\"XY\"", AF_INET));
  test_eq(0, geoip_parse_entry("\"200\",\"250\",\"AB\"", AF_INET));

  /* Populate the IPv6 DB equivalently with fake IPs in the same range */
  test_eq(0, geoip_parse_entry("::a,::32,AB", AF_INET6));
  test_eq(0, geoip_parse_entry("::34,::5a,XY", AF_INET6));
  test_eq(0, geoip_parse_entry("::5f,::64,AB", AF_INET6));
  test_eq(0, geoip_parse_entry("::69,::8c,ZZ", AF_INET6));
  test_eq(0, geoip_parse_entry("::96,::be,XY", AF_INET6));
  test_eq(0, geoip_parse_entry("::c8,::fa,AB", AF_INET6));

  /* We should have 4 countries: ??, ab, xy, zz. */
  test_eq(4, geoip_get_n_countries());
  memset(&in6, 0, sizeof(in6));

  CHECK_COUNTRY("??", 3);
  CHECK_COUNTRY("ab", 32);
  CHECK_COUNTRY("??", 5);
  CHECK_COUNTRY("??", 51);
  CHECK_COUNTRY("xy", 150);
  CHECK_COUNTRY("xy", 190);
  CHECK_COUNTRY("??", 2000);

  test_eq(0, geoip_get_country_by_ipv4(3));
  SET_TEST_IPV6(3);
  test_eq(0, geoip_get_country_by_ipv6(&in6));

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
  test_assert(s);
  test_assert(v);
  test_streq("zz=24,ab=16,xy=8", s);
  test_streq("v4=16,v6=16", v);
  tor_free(s);
  tor_free(v);

  /* Now clear out all the AB observations. */
  geoip_remove_old_clients(now-6000);
  geoip_get_client_history(GEOIP_CLIENT_CONNECT, &s, &v);
  test_assert(s);
  test_assert(v);
  test_streq("zz=24,xy=8", s);
  test_streq("v4=16,v6=16", v);
  tor_free(s);
  tor_free(v);

  /* Start testing bridge statistics by making sure that we don't output
   * bridge stats without initializing them. */
  s = geoip_format_bridge_stats(now + 86400);
  test_assert(!s);

  /* Initialize stats and generate the bridge-stats history string out of
   * the connecting clients added above. */
  geoip_bridge_stats_init(now);
  s = geoip_format_bridge_stats(now + 86400);
  test_assert(s);
  test_streq(bridge_stats_1, s);
  tor_free(s);

  /* Stop collecting bridge stats and make sure we don't write a history
   * string anymore. */
  geoip_bridge_stats_term();
  s = geoip_format_bridge_stats(now + 86400);
  test_assert(!s);

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
  test_assert(!s);

  /* Initialize stats, note one connecting client, and generate the
   * dirreq-stats history string. */
  geoip_dirreq_stats_init(now);
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_NETWORKSTATUS, &addr, NULL, now);
  s = geoip_format_dirreq_stats(now + 86400);
  test_streq(dirreq_stats_1, s);
  tor_free(s);

  /* Stop collecting stats, add another connecting client, and ensure we
   * don't generate a history string. */
  geoip_dirreq_stats_term();
  SET_TEST_ADDRESS(101);
  geoip_note_client_seen(GEOIP_CLIENT_NETWORKSTATUS, &addr, NULL, now);
  s = geoip_format_dirreq_stats(now + 86400);
  test_assert(!s);

  /* Re-start stats, add a connecting client, reset stats, and make sure
   * that we get an all empty history string. */
  geoip_dirreq_stats_init(now);
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_NETWORKSTATUS, &addr, NULL, now);
  geoip_reset_dirreq_stats(now);
  s = geoip_format_dirreq_stats(now + 86400);
  test_streq(dirreq_stats_2, s);
  tor_free(s);

  /* Note a successful network status response and make sure that it
   * appears in the history string. */
  geoip_note_ns_response(GEOIP_SUCCESS);
  s = geoip_format_dirreq_stats(now + 86400);
  test_streq(dirreq_stats_3, s);
  tor_free(s);

  /* Start a tunneled directory request. */
  geoip_start_dirreq((uint64_t) 1, 1024, DIRREQ_TUNNELED);
  s = geoip_format_dirreq_stats(now + 86400);
  test_streq(dirreq_stats_4, s);
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
  test_assert(!s);

  /* Initialize stats, note one connecting client, and generate the
   * entry-stats history string. */
  geoip_entry_stats_init(now);
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now);
  s = geoip_format_entry_stats(now + 86400);
  test_streq(entry_stats_1, s);
  tor_free(s);

  /* Stop collecting stats, add another connecting client, and ensure we
   * don't generate a history string. */
  geoip_entry_stats_term();
  SET_TEST_ADDRESS(101);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now);
  s = geoip_format_entry_stats(now + 86400);
  test_assert(!s);

  /* Re-start stats, add a connecting client, reset stats, and make sure
   * that we get an all empty history string. */
  geoip_entry_stats_init(now);
  SET_TEST_ADDRESS(100);
  geoip_note_client_seen(GEOIP_CLIENT_CONNECT, &addr, NULL, now);
  geoip_reset_entry_stats(now);
  s = geoip_format_entry_stats(now + 86400);
  test_streq(entry_stats_2, s);
  tor_free(s);

  /* Stop collecting entry statistics. */
  geoip_entry_stats_term();
  get_options_mutable()->EntryStatistics = 0;

 done:
  tor_free(s);
  tor_free(v);
}

static void
test_geoip_with_pt(void)
{
  time_t now = 1281533250; /* 2010-08-11 13:27:30 UTC */
  char *s = NULL;
  int i;
  tor_addr_t addr;
  struct in6_addr in6;

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
  test_streq(s, "<OR>=8,alpha=16,beta=8,charlie=16,ddr=136,"
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
test_stats(void)
{
  time_t now = 1281533250; /* 2010-08-11 13:27:30 UTC */
  char *s = NULL;
  int i;

  /* Start with testing exit port statistics; we shouldn't collect exit
   * stats without initializing them. */
  rep_hist_note_exit_stream_opened(80);
  rep_hist_note_exit_bytes(80, 100, 10000);
  s = rep_hist_format_exit_stats(now + 86400);
  test_assert(!s);

  /* Initialize stats, note some streams and bytes, and generate history
   * string. */
  rep_hist_exit_stats_init(now);
  rep_hist_note_exit_stream_opened(80);
  rep_hist_note_exit_bytes(80, 100, 10000);
  rep_hist_note_exit_stream_opened(443);
  rep_hist_note_exit_bytes(443, 100, 10000);
  rep_hist_note_exit_bytes(443, 100, 10000);
  s = rep_hist_format_exit_stats(now + 86400);
  test_streq("exit-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "exit-kibibytes-written 80=1,443=1,other=0\n"
             "exit-kibibytes-read 80=10,443=20,other=0\n"
             "exit-streams-opened 80=4,443=4,other=0\n", s);
  tor_free(s);

  /* Add a few bytes on 10 more ports and ensure that only the top 10
   * ports are contained in the history string. */
  for (i = 50; i < 60; i++) {
    rep_hist_note_exit_bytes(i, i, i);
    rep_hist_note_exit_stream_opened(i);
  }
  s = rep_hist_format_exit_stats(now + 86400);
  test_streq("exit-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "exit-kibibytes-written 52=1,53=1,54=1,55=1,56=1,57=1,58=1,"
             "59=1,80=1,443=1,other=1\n"
             "exit-kibibytes-read 52=1,53=1,54=1,55=1,56=1,57=1,58=1,"
             "59=1,80=10,443=20,other=1\n"
             "exit-streams-opened 52=4,53=4,54=4,55=4,56=4,57=4,58=4,"
             "59=4,80=4,443=4,other=4\n", s);
  tor_free(s);

  /* Stop collecting stats, add some bytes, and ensure we don't generate
   * a history string. */
  rep_hist_exit_stats_term();
  rep_hist_note_exit_bytes(80, 100, 10000);
  s = rep_hist_format_exit_stats(now + 86400);
  test_assert(!s);

  /* Re-start stats, add some bytes, reset stats, and see what history we
   * get when observing no streams or bytes at all. */
  rep_hist_exit_stats_init(now);
  rep_hist_note_exit_stream_opened(80);
  rep_hist_note_exit_bytes(80, 100, 10000);
  rep_hist_reset_exit_stats(now);
  s = rep_hist_format_exit_stats(now + 86400);
  test_streq("exit-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "exit-kibibytes-written other=0\n"
             "exit-kibibytes-read other=0\n"
             "exit-streams-opened other=0\n", s);
  tor_free(s);

  /* Continue with testing connection statistics; we shouldn't collect
   * conn stats without initializing them. */
  rep_hist_note_or_conn_bytes(1, 20, 400, now);
  s = rep_hist_format_conn_stats(now + 86400);
  test_assert(!s);

  /* Initialize stats, note bytes, and generate history string. */
  rep_hist_conn_stats_init(now);
  rep_hist_note_or_conn_bytes(1, 30000, 400000, now);
  rep_hist_note_or_conn_bytes(1, 30000, 400000, now + 5);
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 10);
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 15);
  s = rep_hist_format_conn_stats(now + 86400);
  test_streq("conn-bi-direct 2010-08-12 13:27:30 (86400 s) 0,0,1,0\n", s);
  tor_free(s);

  /* Stop collecting stats, add some bytes, and ensure we don't generate
   * a history string. */
  rep_hist_conn_stats_term();
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 15);
  s = rep_hist_format_conn_stats(now + 86400);
  test_assert(!s);

  /* Re-start stats, add some bytes, reset stats, and see what history we
   * get when observing no bytes at all. */
  rep_hist_conn_stats_init(now);
  rep_hist_note_or_conn_bytes(1, 30000, 400000, now);
  rep_hist_note_or_conn_bytes(1, 30000, 400000, now + 5);
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 10);
  rep_hist_note_or_conn_bytes(2, 400000, 30000, now + 15);
  rep_hist_reset_conn_stats(now);
  s = rep_hist_format_conn_stats(now + 86400);
  test_streq("conn-bi-direct 2010-08-12 13:27:30 (86400 s) 0,0,0,0\n", s);
  tor_free(s);

  /* Continue with testing buffer statistics; we shouldn't collect buffer
   * stats without initializing them. */
  rep_hist_add_buffer_stats(2.0, 2.0, 20);
  s = rep_hist_format_buffer_stats(now + 86400);
  test_assert(!s);

  /* Initialize stats, add statistics for a single circuit, and generate
   * the history string. */
  rep_hist_buffer_stats_init(now);
  rep_hist_add_buffer_stats(2.0, 2.0, 20);
  s = rep_hist_format_buffer_stats(now + 86400);
  test_streq("cell-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "cell-processed-cells 20,0,0,0,0,0,0,0,0,0\n"
             "cell-queued-cells 2.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,"
                               "0.00,0.00\n"
             "cell-time-in-queue 2,0,0,0,0,0,0,0,0,0\n"
             "cell-circuits-per-decile 1\n", s);
  tor_free(s);

  /* Add nineteen more circuit statistics to the one that's already in the
   * history to see that the math works correctly. */
  for (i = 21; i < 30; i++)
    rep_hist_add_buffer_stats(2.0, 2.0, i);
  for (i = 20; i < 30; i++)
    rep_hist_add_buffer_stats(3.5, 3.5, i);
  s = rep_hist_format_buffer_stats(now + 86400);
  test_streq("cell-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "cell-processed-cells 29,28,27,26,25,24,23,22,21,20\n"
             "cell-queued-cells 2.75,2.75,2.75,2.75,2.75,2.75,2.75,2.75,"
                               "2.75,2.75\n"
             "cell-time-in-queue 3,3,3,3,3,3,3,3,3,3\n"
             "cell-circuits-per-decile 2\n", s);
  tor_free(s);

  /* Stop collecting stats, add statistics for one circuit, and ensure we
   * don't generate a history string. */
  rep_hist_buffer_stats_term();
  rep_hist_add_buffer_stats(2.0, 2.0, 20);
  s = rep_hist_format_buffer_stats(now + 86400);
  test_assert(!s);

  /* Re-start stats, add statistics for one circuit, reset stats, and make
   * sure that the history has all zeros. */
  rep_hist_buffer_stats_init(now);
  rep_hist_add_buffer_stats(2.0, 2.0, 20);
  rep_hist_reset_buffer_stats(now);
  s = rep_hist_format_buffer_stats(now + 86400);
  test_streq("cell-stats-end 2010-08-12 13:27:30 (86400 s)\n"
             "cell-processed-cells 0,0,0,0,0,0,0,0,0,0\n"
             "cell-queued-cells 0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00,"
                               "0.00,0.00\n"
             "cell-time-in-queue 0,0,0,0,0,0,0,0,0,0\n"
             "cell-circuits-per-decile 0\n", s);

 done:
  tor_free(s);
}

static void *
legacy_test_setup(const struct testcase_t *testcase)
{
  return testcase->setup_data;
}

void
legacy_test_helper(void *data)
{
  void (*fn)(void) = data;
  fn();
}

static int
legacy_test_cleanup(const struct testcase_t *testcase, void *ptr)
{
  (void)ptr;
  (void)testcase;
  return 1;
}

const struct testcase_setup_t legacy_setup = {
  legacy_test_setup, legacy_test_cleanup
};

#define ENT(name)                                                       \
  { #name, legacy_test_helper, 0, &legacy_setup, test_ ## name }
#define FORK(name)                                                      \
  { #name, legacy_test_helper, TT_FORK, &legacy_setup, test_ ## name }

static struct testcase_t test_array[] = {
  ENT(onion_handshake),
  { "bad_onion_handshake", test_bad_onion_handshake, 0, NULL, NULL },
  ENT(onion_queues),
#ifdef CURVE25519_ENABLED
  { "ntor_handshake", test_ntor_handshake, 0, NULL, NULL },
#endif
  ENT(circuit_timeout),
  ENT(rend_fns),
  ENT(geoip),
  FORK(geoip_with_pt),
  FORK(stats),

  END_OF_TESTCASES
};

extern struct testcase_t addr_tests[];
extern struct testcase_t buffer_tests[];
extern struct testcase_t crypto_tests[];
extern struct testcase_t container_tests[];
extern struct testcase_t util_tests[];
extern struct testcase_t dir_tests[];
extern struct testcase_t microdesc_tests[];
extern struct testcase_t pt_tests[];
extern struct testcase_t config_tests[];
extern struct testcase_t introduce_tests[];
extern struct testcase_t replaycache_tests[];
extern struct testcase_t relaycell_tests[];
extern struct testcase_t cell_format_tests[];
extern struct testcase_t circuitlist_tests[];
extern struct testcase_t circuitmux_tests[];
extern struct testcase_t cell_queue_tests[];
extern struct testcase_t options_tests[];
extern struct testcase_t socks_tests[];
extern struct testcase_t extorport_tests[];
extern struct testcase_t controller_event_tests[];
extern struct testcase_t logging_tests[];
extern struct testcase_t hs_tests[];
extern struct testcase_t nodelist_tests[];
extern struct testcase_t routerkeys_tests[];
extern struct testcase_t oom_tests[];
extern struct testcase_t policy_tests[];
extern struct testcase_t status_tests[];

static struct testgroup_t testgroups[] = {
  { "", test_array },
  { "buffer/", buffer_tests },
  { "socks/", socks_tests },
  { "addr/", addr_tests },
  { "crypto/", crypto_tests },
  { "container/", container_tests },
  { "util/", util_tests },
  { "util/logging/", logging_tests },
  { "cellfmt/", cell_format_tests },
  { "cellqueue/", cell_queue_tests },
  { "dir/", dir_tests },
  { "dir/md/", microdesc_tests },
  { "pt/", pt_tests },
  { "config/", config_tests },
  { "replaycache/", replaycache_tests },
  { "relaycell/", relaycell_tests },
  { "introduce/", introduce_tests },
  { "circuitlist/", circuitlist_tests },
  { "circuitmux/", circuitmux_tests },
  { "options/", options_tests },
  { "extorport/", extorport_tests },
  { "control/", controller_event_tests },
  { "hs/", hs_tests },
  { "nodelist/", nodelist_tests },
  { "routerkeys/", routerkeys_tests },
  { "oom/", oom_tests },
  { "policy/" , policy_tests },
  { "status/" , status_tests },
  END_OF_GROUPS
};

/** Main entry point for unit test code: parse the command line, and run
 * some unit tests. */
int
main(int c, const char **v)
{
  or_options_t *options;
  char *errmsg = NULL;
  int i, i_out;
  int loglevel = LOG_ERR;
  int accel_crypto = 0;

#ifdef USE_DMALLOC
  {
    int r = CRYPTO_set_mem_ex_functions(tor_malloc_, tor_realloc_, tor_free_);
    tor_assert(r);
  }
#endif

  update_approx_time(time(NULL));
  options = options_new();
  tor_threads_init();
  init_logging();

  for (i_out = i = 1; i < c; ++i) {
    if (!strcmp(v[i], "--warn")) {
      loglevel = LOG_WARN;
    } else if (!strcmp(v[i], "--notice")) {
      loglevel = LOG_NOTICE;
    } else if (!strcmp(v[i], "--info")) {
      loglevel = LOG_INFO;
    } else if (!strcmp(v[i], "--debug")) {
      loglevel = LOG_DEBUG;
    } else if (!strcmp(v[i], "--accel")) {
      accel_crypto = 1;
    } else {
      v[i_out++] = v[i];
    }
  }
  c = i_out;

  {
    log_severity_list_t s;
    memset(&s, 0, sizeof(s));
    set_log_severity_config(loglevel, LOG_ERR, &s);
    add_stream_log(&s, "", fileno(stdout));
  }

  options->command = CMD_RUN_UNITTESTS;
  if (crypto_global_init(accel_crypto, NULL, NULL)) {
    printf("Can't initialize crypto subsystem; exiting.\n");
    return 1;
  }
  crypto_set_tls_dh_prime(NULL);
  crypto_seed_rng(1);
  rep_hist_init();
  network_init();
  setup_directory();
  options_init(options);
  options->DataDirectory = tor_strdup(temp_dir);
  options->EntryStatistics = 1;
  if (set_options(options, &errmsg) < 0) {
    printf("Failed to set initial options: %s\n", errmsg);
    tor_free(errmsg);
    return 1;
  }

  atexit(remove_directory);

  have_failed = (tinytest_main(c, v, testgroups) != 0);

  free_pregenerated_keys();
#ifdef USE_DMALLOC
  tor_free_all(0);
  dmalloc_log_unfreed();
#endif

  if (have_failed)
    return 1;
  else
    return 0;
}

