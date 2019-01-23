/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

extern const char tor_git_revision[];
/* Ordinarily defined in tor_main.c; this bit is just here to provide one
 * since we're not linking to tor_main.c */
const char tor_git_revision[] = "";

/**
 * \file bench.c
 * \brief Benchmarks for lower level Tor modules.
 **/

#include "orconfig.h"

#include "or.h"
#include "onion_tap.h"
#include "relay.h"
#include <openssl/opensslv.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/obj_mac.h>

#include "config.h"
#include "crypto_curve25519.h"
#include "onion_ntor.h"
#include "crypto_ed25519.h"

#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_PROCESS_CPUTIME_ID)
static uint64_t nanostart;
static inline uint64_t
timespec_to_nsec(const struct timespec *ts)
{
  return ((uint64_t)ts->tv_sec)*1000000000 + ts->tv_nsec;
}

static void
reset_perftime(void)
{
  struct timespec ts;
  int r;
  r = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  tor_assert(r == 0);
  nanostart = timespec_to_nsec(&ts);
}

static uint64_t
perftime(void)
{
  struct timespec ts;
  int r;
  r = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  tor_assert(r == 0);
  return timespec_to_nsec(&ts) - nanostart;
}

#else
static struct timeval tv_start = { 0, 0 };
static void
reset_perftime(void)
{
  tor_gettimeofday(&tv_start);
}
static uint64_t
perftime(void)
{
  struct timeval now, out;
  tor_gettimeofday(&now);
  timersub(&now, &tv_start, &out);
  return ((uint64_t)out.tv_sec)*1000000000 + out.tv_usec*1000;
}
#endif

#define NANOCOUNT(start,end,iters) \
  ( ((double)((end)-(start))) / (iters) )

#define MICROCOUNT(start,end,iters) \
  ( NANOCOUNT((start), (end), (iters)) / 1000.0 )

/** Run AES performance benchmarks. */
static void
bench_aes(void)
{
  int len, i;
  char *b1, *b2;
  crypto_cipher_t *c;
  uint64_t start, end;
  const int bytes_per_iter = (1<<24);
  reset_perftime();
  char key[CIPHER_KEY_LEN];
  crypto_rand(key, sizeof(key));
  c = crypto_cipher_new(key);

  for (len = 1; len <= 8192; len *= 2) {
    int iters = bytes_per_iter / len;
    b1 = tor_malloc_zero(len);
    b2 = tor_malloc_zero(len);
    start = perftime();
    for (i = 0; i < iters; ++i) {
      crypto_cipher_encrypt(c, b1, b2, len);
    }
    end = perftime();
    tor_free(b1);
    tor_free(b2);
    printf("%d bytes: %.2f nsec per byte\n", len,
           NANOCOUNT(start, end, iters*len));
  }
  crypto_cipher_free(c);
}

static void
bench_onion_TAP(void)
{
  const int iters = 1<<9;
  int i;
  crypto_pk_t *key, *key2;
  uint64_t start, end;
  char os[TAP_ONIONSKIN_CHALLENGE_LEN];
  char or[TAP_ONIONSKIN_REPLY_LEN];
  crypto_dh_t *dh_out;

  key = crypto_pk_new();
  key2 = crypto_pk_new();
  if (crypto_pk_generate_key_with_bits(key, 1024) < 0)
    goto done;
  if (crypto_pk_generate_key_with_bits(key2, 1024) < 0)
    goto done;

  reset_perftime();
  start = perftime();
  for (i = 0; i < iters; ++i) {
    onion_skin_TAP_create(key, &dh_out, os);
    crypto_dh_free(dh_out);
  }
  end = perftime();
  printf("Client-side, part 1: %f usec.\n", NANOCOUNT(start, end, iters)/1e3);

  onion_skin_TAP_create(key, &dh_out, os);
  start = perftime();
  for (i = 0; i < iters; ++i) {
    char key_out[CPATH_KEY_MATERIAL_LEN];
    onion_skin_TAP_server_handshake(os, key, NULL, or,
                                    key_out, sizeof(key_out));
  }
  end = perftime();
  printf("Server-side, key guessed right: %f usec\n",
         NANOCOUNT(start, end, iters)/1e3);

  start = perftime();
  for (i = 0; i < iters; ++i) {
    char key_out[CPATH_KEY_MATERIAL_LEN];
    onion_skin_TAP_server_handshake(os, key2, key, or,
                                    key_out, sizeof(key_out));
  }
  end = perftime();
  printf("Server-side, key guessed wrong: %f usec.\n",
         NANOCOUNT(start, end, iters)/1e3);

  start = perftime();
  for (i = 0; i < iters; ++i) {
    crypto_dh_t *dh;
    char key_out[CPATH_KEY_MATERIAL_LEN];
    int s;
    dh = crypto_dh_dup(dh_out);
    s = onion_skin_TAP_client_handshake(dh, or, key_out, sizeof(key_out),
                                        NULL);
    crypto_dh_free(dh);
    tor_assert(s == 0);
  }
  end = perftime();
  printf("Client-side, part 2: %f usec.\n",
         NANOCOUNT(start, end, iters)/1e3);

 done:
  crypto_pk_free(key);
  crypto_pk_free(key2);
}

static void
bench_onion_ntor_impl(void)
{
  const int iters = 1<<10;
  int i;
  curve25519_keypair_t keypair1, keypair2;
  uint64_t start, end;
  uint8_t os[NTOR_ONIONSKIN_LEN];
  uint8_t or[NTOR_REPLY_LEN];
  ntor_handshake_state_t *state = NULL;
  uint8_t nodeid[DIGEST_LEN];
  di_digest256_map_t *keymap = NULL;

  curve25519_secret_key_generate(&keypair1.seckey, 0);
  curve25519_public_key_generate(&keypair1.pubkey, &keypair1.seckey);
  curve25519_secret_key_generate(&keypair2.seckey, 0);
  curve25519_public_key_generate(&keypair2.pubkey, &keypair2.seckey);
  dimap_add_entry(&keymap, keypair1.pubkey.public_key, &keypair1);
  dimap_add_entry(&keymap, keypair2.pubkey.public_key, &keypair2);

  reset_perftime();
  start = perftime();
  for (i = 0; i < iters; ++i) {
    onion_skin_ntor_create(nodeid, &keypair1.pubkey, &state, os);
    ntor_handshake_state_free(state);
    state = NULL;
  }
  end = perftime();
  printf("Client-side, part 1: %f usec.\n", NANOCOUNT(start, end, iters)/1e3);

  state = NULL;
  onion_skin_ntor_create(nodeid, &keypair1.pubkey, &state, os);
  start = perftime();
  for (i = 0; i < iters; ++i) {
    uint8_t key_out[CPATH_KEY_MATERIAL_LEN];
    onion_skin_ntor_server_handshake(os, keymap, NULL, nodeid, or,
                                key_out, sizeof(key_out));
  }
  end = perftime();
  printf("Server-side: %f usec\n",
         NANOCOUNT(start, end, iters)/1e3);

  start = perftime();
  for (i = 0; i < iters; ++i) {
    uint8_t key_out[CPATH_KEY_MATERIAL_LEN];
    int s;
    s = onion_skin_ntor_client_handshake(state, or, key_out, sizeof(key_out),
                                         NULL);
    tor_assert(s == 0);
  }
  end = perftime();
  printf("Client-side, part 2: %f usec.\n",
         NANOCOUNT(start, end, iters)/1e3);

  ntor_handshake_state_free(state);
  dimap_free(keymap, NULL);
}

static void
bench_onion_ntor(void)
{
  int ed;

  for (ed = 0; ed <= 1; ++ed) {
    printf("Ed25519-based basepoint multiply = %s.\n",
           (ed == 0) ? "disabled" : "enabled");
    curve25519_set_impl_params(ed);
    bench_onion_ntor_impl();
  }
}

static void
bench_ed25519_impl(void)
{
  uint64_t start, end;
  const int iters = 1<<12;
  int i;
  const uint8_t msg[] = "but leaving, could not tell what they had heard";
  ed25519_signature_t sig;
  ed25519_keypair_t kp;
  curve25519_keypair_t curve_kp;
  ed25519_public_key_t pubkey_tmp;

  ed25519_secret_key_generate(&kp.seckey, 0);
  start = perftime();
  for (i = 0; i < iters; ++i) {
    ed25519_public_key_generate(&kp.pubkey, &kp.seckey);
  }
  end = perftime();
  printf("Generate public key: %.2f usec\n",
         MICROCOUNT(start, end, iters));

  start = perftime();
  for (i = 0; i < iters; ++i) {
    ed25519_sign(&sig, msg, sizeof(msg), &kp);
  }
  end = perftime();
  printf("Sign a short message: %.2f usec\n",
         MICROCOUNT(start, end, iters));

  start = perftime();
  for (i = 0; i < iters; ++i) {
    ed25519_checksig(&sig, msg, sizeof(msg), &kp.pubkey);
  }
  end = perftime();
  printf("Verify signature: %.2f usec\n",
         MICROCOUNT(start, end, iters));

  curve25519_keypair_generate(&curve_kp, 0);
  start = perftime();
  for (i = 0; i < iters; ++i) {
    ed25519_public_key_from_curve25519_public_key(&pubkey_tmp,
                                                  &curve_kp.pubkey, 1);
  }
  end = perftime();
  printf("Convert public point from curve25519: %.2f usec\n",
         MICROCOUNT(start, end, iters));

  curve25519_keypair_generate(&curve_kp, 0);
  start = perftime();
  for (i = 0; i < iters; ++i) {
    ed25519_public_blind(&pubkey_tmp, &kp.pubkey, msg);
  }
  end = perftime();
  printf("Blind a public key: %.2f usec\n",
         MICROCOUNT(start, end, iters));
}

static void
bench_ed25519(void)
{
  int donna;

  for (donna = 0; donna <= 1; ++donna) {
    printf("Ed25519-donna = %s.\n",
           (donna == 0) ? "disabled" : "enabled");
    ed25519_set_impl_params(donna);
    bench_ed25519_impl();
  }
}

static void
bench_cell_aes(void)
{
  uint64_t start, end;
  const int len = 509;
  const int iters = (1<<16);
  const int max_misalign = 15;
  char *b = tor_malloc(len+max_misalign);
  crypto_cipher_t *c;
  int i, misalign;
  char key[CIPHER_KEY_LEN];
  crypto_rand(key, sizeof(key));
  c = crypto_cipher_new(key);

  reset_perftime();
  for (misalign = 0; misalign <= max_misalign; ++misalign) {
    start = perftime();
    for (i = 0; i < iters; ++i) {
      crypto_cipher_crypt_inplace(c, b+misalign, len);
    }
    end = perftime();
    printf("%d bytes, misaligned by %d: %.2f nsec per byte\n", len, misalign,
           NANOCOUNT(start, end, iters*len));
  }

  crypto_cipher_free(c);
  tor_free(b);
}

/** Run digestmap_t performance benchmarks. */
static void
bench_dmap(void)
{
  smartlist_t *sl = smartlist_new();
  smartlist_t *sl2 = smartlist_new();
  uint64_t start, end, pt2, pt3, pt4;
  int iters = 8192;
  const int elts = 4000;
  const int fpostests = 100000;
  char d[20];
  int i,n=0, fp = 0;
  digestmap_t *dm = digestmap_new();
  digestset_t *ds = digestset_new(elts);

  for (i = 0; i < elts; ++i) {
    crypto_rand(d, 20);
    smartlist_add(sl, tor_memdup(d, 20));
  }
  for (i = 0; i < elts; ++i) {
    crypto_rand(d, 20);
    smartlist_add(sl2, tor_memdup(d, 20));
  }
  printf("nbits=%d\n", ds->mask+1);

  reset_perftime();

  start = perftime();
  for (i = 0; i < iters; ++i) {
    SMARTLIST_FOREACH(sl, const char *, cp, digestmap_set(dm, cp, (void*)1));
  }
  pt2 = perftime();
  printf("digestmap_set: %.2f ns per element\n",
         NANOCOUNT(start, pt2, iters*elts));

  for (i = 0; i < iters; ++i) {
    SMARTLIST_FOREACH(sl, const char *, cp, digestmap_get(dm, cp));
    SMARTLIST_FOREACH(sl2, const char *, cp, digestmap_get(dm, cp));
  }
  pt3 = perftime();
  printf("digestmap_get: %.2f ns per element\n",
         NANOCOUNT(pt2, pt3, iters*elts*2));

  for (i = 0; i < iters; ++i) {
    SMARTLIST_FOREACH(sl, const char *, cp, digestset_add(ds, cp));
  }
  pt4 = perftime();
  printf("digestset_add: %.2f ns per element\n",
         NANOCOUNT(pt3, pt4, iters*elts));

  for (i = 0; i < iters; ++i) {
    SMARTLIST_FOREACH(sl, const char *, cp, n += digestset_contains(ds, cp));
    SMARTLIST_FOREACH(sl2, const char *, cp, n += digestset_contains(ds, cp));
  }
  end = perftime();
  printf("digestset_contains: %.2f ns per element.\n",
         NANOCOUNT(pt4, end, iters*elts*2));
  /* We need to use this, or else the whole loop gets optimized out. */
  printf("Hits == %d\n", n);

  for (i = 0; i < fpostests; ++i) {
    crypto_rand(d, 20);
    if (digestset_contains(ds, d)) ++fp;
  }
  printf("False positive rate on digestset: %.2f%%\n",
         (fp/(double)fpostests)*100);

  digestmap_free(dm, NULL);
  digestset_free(ds);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(sl2, char *, cp, tor_free(cp));
  smartlist_free(sl);
  smartlist_free(sl2);
}

static void
bench_siphash(void)
{
  char buf[128];
  int lens[] = { 7, 8, 15, 16, 20, 32, 111, 128, -1 };
  int i, j;
  uint64_t start, end;
  const int N = 300000;
  crypto_rand(buf, sizeof(buf));

  for (i = 0; lens[i] > 0; ++i) {
    reset_perftime();
    start = perftime();
    for (j = 0; j < N; ++j) {
      siphash24g(buf, lens[i]);
    }
    end = perftime();
    printf("siphash24g(%d): %.2f ns per call\n",
           lens[i], NANOCOUNT(start,end,N));
  }
}

static void
bench_digest(void)
{
  char buf[8192];
  char out[DIGEST512_LEN];
  const int lens[] = { 1, 16, 32, 64, 128, 512, 1024, 2048, -1 };
  const int N = 300000;
  uint64_t start, end;
  crypto_rand(buf, sizeof(buf));

  for (int alg = 0; alg < N_DIGEST_ALGORITHMS; alg++) {
    for (int i = 0; lens[i] > 0; ++i) {
      reset_perftime();
      start = perftime();
      for (int j = 0; j < N; ++j) {
        switch (alg) {
          case DIGEST_SHA1:
            crypto_digest(out, buf, lens[i]);
            break;
          case DIGEST_SHA256:
          case DIGEST_SHA3_256:
            crypto_digest256(out, buf, lens[i], alg);
            break;
          case DIGEST_SHA512:
          case DIGEST_SHA3_512:
            crypto_digest512(out, buf, lens[i], alg);
            break;
          default:
            tor_assert(0);
        }
      }
      end = perftime();
      printf("%s(%d): %.2f ns per call\n",
             crypto_digest_algorithm_get_name(alg),
             lens[i], NANOCOUNT(start,end,N));
    }
  }
}

static void
bench_cell_ops(void)
{
  const int iters = 1<<16;
  int i;

  /* benchmarks for cell ops at relay. */
  or_circuit_t *or_circ = tor_malloc_zero(sizeof(or_circuit_t));
  cell_t *cell = tor_malloc(sizeof(cell_t));
  int outbound;
  uint64_t start, end;

  crypto_rand((char*)cell->payload, sizeof(cell->payload));

  /* Mock-up or_circuit_t */
  or_circ->base_.magic = OR_CIRCUIT_MAGIC;
  or_circ->base_.purpose = CIRCUIT_PURPOSE_OR;

  /* Initialize crypto */
  char key1[CIPHER_KEY_LEN], key2[CIPHER_KEY_LEN];
  crypto_rand(key1, sizeof(key1));
  crypto_rand(key2, sizeof(key2));
  or_circ->p_crypto = crypto_cipher_new(key1);
  or_circ->n_crypto = crypto_cipher_new(key2);
  or_circ->p_digest = crypto_digest_new();
  or_circ->n_digest = crypto_digest_new();

  reset_perftime();

  for (outbound = 0; outbound <= 1; ++outbound) {
    cell_direction_t d = outbound ? CELL_DIRECTION_OUT : CELL_DIRECTION_IN;
    start = perftime();
    for (i = 0; i < iters; ++i) {
      char recognized = 0;
      crypt_path_t *layer_hint = NULL;
      relay_crypt(TO_CIRCUIT(or_circ), cell, d, &layer_hint, &recognized);
    }
    end = perftime();
    printf("%sbound cells: %.2f ns per cell. (%.2f ns per byte of payload)\n",
           outbound?"Out":" In",
           NANOCOUNT(start,end,iters),
           NANOCOUNT(start,end,iters*CELL_PAYLOAD_SIZE));
  }

  crypto_digest_free(or_circ->p_digest);
  crypto_digest_free(or_circ->n_digest);
  crypto_cipher_free(or_circ->p_crypto);
  crypto_cipher_free(or_circ->n_crypto);
  tor_free(or_circ);
  tor_free(cell);
}

static void
bench_dh(void)
{
  const int iters = 1<<10;
  int i;
  uint64_t start, end;

  reset_perftime();
  start = perftime();
  for (i = 0; i < iters; ++i) {
    char dh_pubkey_a[DH_BYTES], dh_pubkey_b[DH_BYTES];
    char secret_a[DH_BYTES], secret_b[DH_BYTES];
    ssize_t slen_a, slen_b;
    crypto_dh_t *dh_a = crypto_dh_new(DH_TYPE_TLS);
    crypto_dh_t *dh_b = crypto_dh_new(DH_TYPE_TLS);
    crypto_dh_generate_public(dh_a);
    crypto_dh_generate_public(dh_b);
    crypto_dh_get_public(dh_a, dh_pubkey_a, sizeof(dh_pubkey_a));
    crypto_dh_get_public(dh_b, dh_pubkey_b, sizeof(dh_pubkey_b));
    slen_a = crypto_dh_compute_secret(LOG_NOTICE,
                                      dh_a, dh_pubkey_b, sizeof(dh_pubkey_b),
                                      secret_a, sizeof(secret_a));
    slen_b = crypto_dh_compute_secret(LOG_NOTICE,
                                      dh_b, dh_pubkey_a, sizeof(dh_pubkey_a),
                                      secret_b, sizeof(secret_b));
    tor_assert(slen_a == slen_b);
    tor_assert(fast_memeq(secret_a, secret_b, slen_a));
    crypto_dh_free(dh_a);
    crypto_dh_free(dh_b);
  }
  end = perftime();
  printf("Complete DH handshakes (1024 bit, public and private ops):\n"
         "      %f millisec each.\n", NANOCOUNT(start, end, iters)/1e6);
}

static void
bench_ecdh_impl(int nid, const char *name)
{
  const int iters = 1<<10;
  int i;
  uint64_t start, end;

  reset_perftime();
  start = perftime();
  for (i = 0; i < iters; ++i) {
    char secret_a[DH_BYTES], secret_b[DH_BYTES];
    ssize_t slen_a, slen_b;
    EC_KEY *dh_a = EC_KEY_new_by_curve_name(nid);
    EC_KEY *dh_b = EC_KEY_new_by_curve_name(nid);
    if (!dh_a || !dh_b) {
      puts("Skipping.  (No implementation?)");
      return;
    }

    EC_KEY_generate_key(dh_a);
    EC_KEY_generate_key(dh_b);
    slen_a = ECDH_compute_key(secret_a, DH_BYTES,
                              EC_KEY_get0_public_key(dh_b), dh_a,
                              NULL);
    slen_b = ECDH_compute_key(secret_b, DH_BYTES,
                              EC_KEY_get0_public_key(dh_a), dh_b,
                              NULL);

    tor_assert(slen_a == slen_b);
    tor_assert(fast_memeq(secret_a, secret_b, slen_a));
    EC_KEY_free(dh_a);
    EC_KEY_free(dh_b);
  }
  end = perftime();
  printf("Complete ECDH %s handshakes (2 public and 2 private ops):\n"
         "      %f millisec each.\n", name, NANOCOUNT(start, end, iters)/1e6);
}

static void
bench_ecdh_p256(void)
{
  bench_ecdh_impl(NID_X9_62_prime256v1, "P-256");
}

static void
bench_ecdh_p224(void)
{
  bench_ecdh_impl(NID_secp224r1, "P-224");
}

typedef void (*bench_fn)(void);

typedef struct benchmark_t {
  const char *name;
  bench_fn fn;
  int enabled;
} benchmark_t;

#define ENT(s) { #s , bench_##s, 0 }

static struct benchmark_t benchmarks[] = {
  ENT(dmap),
  ENT(siphash),
  ENT(digest),
  ENT(aes),
  ENT(onion_TAP),
  ENT(onion_ntor),
  ENT(ed25519),

  ENT(cell_aes),
  ENT(cell_ops),
  ENT(dh),
  ENT(ecdh_p256),
  ENT(ecdh_p224),
  {NULL,NULL,0}
};

static benchmark_t *
find_benchmark(const char *name)
{
  benchmark_t *b;
  for (b = benchmarks; b->name; ++b) {
    if (!strcmp(name, b->name)) {
      return b;
    }
  }
  return NULL;
}

/** Main entry point for benchmark code: parse the command line, and run
 * some benchmarks. */
int
main(int argc, const char **argv)
{
  int i;
  int list=0, n_enabled=0;
  char *errmsg;
  or_options_t *options;

  tor_threads_init();

  for (i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--list")) {
      list = 1;
    } else {
      benchmark_t *benchmark = find_benchmark(argv[i]);
      ++n_enabled;
      if (benchmark) {
        benchmark->enabled = 1;
      } else {
        printf("No such benchmark as %s\n", argv[i]);
      }
    }
  }

  reset_perftime();

  if (crypto_seed_rng() < 0) {
    printf("Couldn't seed RNG; exiting.\n");
    return 1;
  }
  crypto_init_siphash_key();
  options = options_new();
  init_logging(1);
  options->command = CMD_RUN_UNITTESTS;
  options->DataDirectory = tor_strdup("");
  options_init(options);
  if (set_options(options, &errmsg) < 0) {
    printf("Failed to set initial options: %s\n", errmsg);
    tor_free(errmsg);
    return 1;
  }

  for (benchmark_t *b = benchmarks; b->name; ++b) {
    if (b->enabled || n_enabled == 0) {
      printf("===== %s =====\n", b->name);
      if (!list)
        b->fn();
    }
  }

  return 0;
}

