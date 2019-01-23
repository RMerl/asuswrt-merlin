/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

extern const char tor_git_revision[];

/* Ordinarily defined in tor_main.c; this bit is just here to provide one
 * since we're not linking to tor_main.c */
const char tor_git_revision[] = "";

/**
 * \file test_common.c
 * \brief Common pieces to implement unit tests.
 **/

#include "orconfig.h"
#include "or.h"
#include "control.h"
#include "config.h"
#include "rephist.h"
#include "backtrace.h"
#include "test.h"

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

#include "or.h"

#ifdef USE_DMALLOC
#include <dmalloc.h>
#include <openssl/crypto.h>
#include "main.h"
#endif

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
    const char *extra_backslash = "";
    /* If this fails, we're probably screwed anyway */
    if (!GetTempPathA(sizeof(buf),buf))
      tmp = "c:\\windows\\temp\\";
    if (strcmpend(tmp, "\\")) {
      /* According to MSDN, it should be impossible for GetTempPath to give us
       * an answer that doesn't end with \.  But let's make sure. */
      extra_backslash = "\\";
    }
    tor_snprintf(temp_dir, sizeof(temp_dir),
                 "%s%stor_test_%d_%s", tmp, extra_backslash,
                 (int)getpid(), rnd32);
    r = mkdir(temp_dir);
  }
#else
  tor_snprintf(temp_dir, sizeof(temp_dir), "/tmp/tor_test_%d_%s",
               (int) getpid(), rnd32);
  r = mkdir(temp_dir, 0700);
  if (!r) {
    /* undo sticky bit so tests don't get confused. */
    r = chown(temp_dir, getuid(), getgid());
  }
#endif
  if (r) {
    fprintf(stderr, "Can't create directory %s:", temp_dir);
    perror("");
    exit(1);
  }
  is_setup = 1;
  temp_dir_setup_in_pid = getpid();
}

/** Return a filename relative to our testing temporary directory, based on
 * name and suffix. If name is NULL, return the name of the testing temporary
 * directory. */
static const char *
get_fname_suffix(const char *name, const char *suffix)
{
  static char buf[1024];
  setup_directory();
  if (!name)
    return temp_dir;
  tor_snprintf(buf,sizeof(buf),"%s/%s%s%s",temp_dir,name,suffix ? "_" : "",
               suffix ? suffix : "");
  return buf;
}

/** Return a filename relative to our testing temporary directory. If name is
 * NULL, return the name of the testing temporary directory. */
const char *
get_fname(const char *name)
{
  return get_fname_suffix(name, NULL);
}

/** Return a filename with a random suffix, relative to our testing temporary
 * directory. If name is NULL, return the name of the testing temporary
 * directory, without any suffix. */
const char *
get_fname_rnd(const char *name)
{
  char rnd[256], rnd32[256];
  crypto_rand(rnd, RAND_PATH_BYTES);
  base32_encode(rnd32, sizeof(rnd32), rnd, RAND_PATH_BYTES);
  return get_fname_suffix(name, rnd32);
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
#define CACHE_GENERATED_KEYS

#define N_PREGEN_KEYS 11
static crypto_pk_t *pregen_keys[N_PREGEN_KEYS];
static int next_key_idx;

/** Generate and return a new keypair for use in unit tests.  If we're using
 * the key cache optimization, we might reuse keys. "idx" is ignored.
 * Our only guarantee is that we won't reuse a key till this function has been
 * called several times. The order in which keys are returned is slightly
 * randomized, so that tests that depend on a particular order will not be
 * reliable. */
crypto_pk_t *
pk_generate(int idx)
{
  (void) idx;
#ifdef CACHE_GENERATED_KEYS
  /* Either skip 1 or 2 keys. */
  next_key_idx += crypto_rand_int_range(1,3);
  next_key_idx %= N_PREGEN_KEYS;
  return crypto_pk_dup_key(pregen_keys[next_key_idx]);
#else
  crypto_pk_t *result;
  int res;
  result = crypto_pk_new();
  res = crypto_pk_generate_key__real(result);
  tor_assert(!res);
  return result;
#endif
}

#ifdef CACHE_GENERATED_KEYS
static int
crypto_pk_generate_key_with_bits__get_cached(crypto_pk_t *env, int bits)
{
  if (bits != 1024)
    return crypto_pk_generate_key_with_bits__real(env, bits);

  crypto_pk_t *newkey = pk_generate(0);
  crypto_pk_assign_(env, newkey);
  crypto_pk_free(newkey);
  return 0;
}
#endif

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

static void *
passthrough_test_setup(const struct testcase_t *testcase)
{
  /* Make sure the passthrough doesn't unintentionally fail or skip tests */
  tor_assert(testcase->setup_data);
  tor_assert(testcase->setup_data != (void*)TT_SKIP);
  return testcase->setup_data;
}
static int
passthrough_test_cleanup(const struct testcase_t *testcase, void *ptr)
{
  (void)testcase;
  (void)ptr;
  return 1;
}

static void *
ed25519_testcase_setup(const struct testcase_t *testcase)
{
  crypto_ed25519_testing_force_impl(testcase->setup_data);
  return testcase->setup_data;
}
static int
ed25519_testcase_cleanup(const struct testcase_t *testcase, void *ptr)
{
  (void)testcase;
  (void)ptr;
  crypto_ed25519_testing_restore_impl();
  return 1;
}
const struct testcase_setup_t ed25519_test_setup = {
  ed25519_testcase_setup, ed25519_testcase_cleanup
};

const struct testcase_setup_t passthrough_setup = {
  passthrough_test_setup, passthrough_test_cleanup
};

static void
an_assertion_failed(void)
{
  tinytest_set_test_failed_();
}

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

  /* We must initialise logs before we call tor_assert() */
  init_logging(1);

#ifdef USE_DMALLOC
  {
    int r = CRYPTO_set_mem_ex_functions(tor_malloc_, tor_realloc_, tor_free_);
    tor_assert(r);
  }
#endif

  update_approx_time(time(NULL));
  options = options_new();
  tor_threads_init();

  network_init();

  monotime_init();

  struct tor_libevent_cfg cfg;
  memset(&cfg, 0, sizeof(cfg));
  tor_libevent_initialize(&cfg);

  control_initialize_event_queue();
  configure_backtrace_handler(get_version());

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
    /* ALWAYS log bug warnings. */
    s.masks[LOG_WARN-LOG_ERR] |= LD_BUG;
    add_stream_log(&s, "", fileno(stdout));
  }

  options->command = CMD_RUN_UNITTESTS;
  if (crypto_global_init(accel_crypto, NULL, NULL)) {
    printf("Can't initialize crypto subsystem; exiting.\n");
    return 1;
  }
  crypto_set_tls_dh_prime();
  if (crypto_seed_rng() < 0) {
    printf("Couldn't seed RNG; exiting.\n");
    return 1;
  }
  rep_hist_init();
  setup_directory();
  options_init(options);
  options->DataDirectory = tor_strdup(temp_dir);
  options->EntryStatistics = 1;
  if (set_options(options, &errmsg) < 0) {
    printf("Failed to set initial options: %s\n", errmsg);
    tor_free(errmsg);
    return 1;
  }
  tor_set_failed_assertion_callback(an_assertion_failed);

#ifdef CACHE_GENERATED_KEYS
  for (i = 0; i < N_PREGEN_KEYS; ++i) {
    pregen_keys[i] = crypto_pk_new();
    int r = crypto_pk_generate_key(pregen_keys[i]);
    tor_assert(r == 0);
  }
  MOCK(crypto_pk_generate_key_with_bits,
       crypto_pk_generate_key_with_bits__get_cached);
#endif

  atexit(remove_directory);

  int have_failed = (tinytest_main(c, v, testgroups) != 0);

  free_pregenerated_keys();
#ifdef USE_DMALLOC
  tor_free_all(0);
  dmalloc_log_unfreed();
#endif
  crypto_global_cleanup();

  if (have_failed)
    return 1;
  else
    return 0;
}

