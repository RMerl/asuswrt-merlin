/* Copyright (c) 2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#define DIRSERV_PRIVATE
#define ROUTERPARSE_PRIVATE
#define NETWORKSTATUS_PRIVATE

#include "orconfig.h"
#include "or.h"
#include "config.h"
#include "dirserv.h"
#include "container.h"
#include "entrynodes.h"
#include "util.h"
#include "routerparse.h"
#include "networkstatus.h"

#include "test.h"
#include "test_helpers.h"

/** Generate a vote_routerstatus_t for a router with identity digest
 * <b>digest_in_hex</b>. */
static vote_routerstatus_t *
gen_vote_routerstatus_for_tests(const char *digest_in_hex, int is_guard)
{
  int retval;
  vote_routerstatus_t *vrs = NULL;
  routerstatus_t *rs;

  vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
  rs = &vrs->status;

  { /* Useful information for tests */
    char digest_tmp[DIGEST_LEN];

    /* Guard or not? */
    rs->is_possible_guard = is_guard;

    /* Fill in the fpr */
    tt_int_op(strlen(digest_in_hex), ==, HEX_DIGEST_LEN);
    retval = base16_decode(digest_tmp, sizeof(digest_tmp),
                           digest_in_hex, HEX_DIGEST_LEN);
    tt_int_op(retval, ==, 0);
    memcpy(rs->identity_digest, digest_tmp, DIGEST_LEN);
  }

  { /* Misc info (maybe not used in tests) */
    vrs->version = tor_strdup("0.1.2.14");
    strlcpy(rs->nickname, "router2", sizeof(rs->nickname));
    memset(rs->descriptor_digest, 78, DIGEST_LEN);
    rs->addr = 0x99008801;
    rs->or_port = 443;
    rs->dir_port = 8000;
    /* all flags but running cleared */
    rs->is_flagged_running = 1;
    vrs->has_measured_bw = 1;
    rs->has_bandwidth = 1;
  }

  return vrs;

 done:
  vote_routerstatus_free(vrs);

  return NULL;
}

/** Make sure our parsers reject corrupted guardfraction files. */
static void
test_parse_guardfraction_file_bad(void *arg)
{
  int retval;
  char *guardfraction_bad = NULL;
  const char *yesterday_date_str = get_yesterday_date_str();

  (void) arg;

  /* Start parsing all those corrupted guardfraction files! */

  /* Guardfraction file version is not a number! */
  tor_asprintf(&guardfraction_bad,
               "guardfraction-file-version nan\n"
               "written-at %s\n"
               "n-inputs 420 3\n"
               "guard-seen D0EDB47BEAD32D26D0A837F7D5357EC3AD3B8777 100 420\n"
               "guard-seen 07B5547026DF3E229806E135CFA8552D56AFBABC 5 420\n",
               yesterday_date_str);

  retval = dirserv_read_guardfraction_file_from_str(guardfraction_bad, NULL);
  tt_int_op(retval, ==, -1);
  tor_free(guardfraction_bad);

  /* This one does not have a date! Parsing should fail. */
  tor_asprintf(&guardfraction_bad,
               "guardfraction-file-version 1\n"
               "written-at not_date\n"
               "n-inputs 420 3\n"
               "guard-seen D0EDB47BEAD32D26D0A837F7D5357EC3AD3B8777 100 420\n"
               "guard-seen 07B5547026DF3E229806E135CFA8552D56AFBABC 5 420\n");

  retval = dirserv_read_guardfraction_file_from_str(guardfraction_bad, NULL);
  tt_int_op(retval, ==, -1);
  tor_free(guardfraction_bad);

  /* This one has an incomplete n-inputs line, but parsing should
     still continue. */
  tor_asprintf(&guardfraction_bad,
               "guardfraction-file-version 1\n"
               "written-at %s\n"
               "n-inputs biggie\n"
               "guard-seen D0EDB47BEAD32D26D0A837F7D5357EC3AD3B8777 100 420\n"
               "guard-seen 07B5547026DF3E229806E135CFA8552D56AFBABC 5 420\n",
               yesterday_date_str);

  retval = dirserv_read_guardfraction_file_from_str(guardfraction_bad, NULL);
  tt_int_op(retval, ==, 2);
  tor_free(guardfraction_bad);

  /* This one does not have a fingerprint in the guard line! */
  tor_asprintf(&guardfraction_bad,
               "guardfraction-file-version 1\n"
               "written-at %s\n"
               "n-inputs 420 3\n"
               "guard-seen not_a_fingerprint 100 420\n",
               yesterday_date_str);

  retval = dirserv_read_guardfraction_file_from_str(guardfraction_bad, NULL);
  tt_int_op(retval, ==, 0);
  tor_free(guardfraction_bad);

  /* This one does not even have an integer guardfraction value. */
  tor_asprintf(&guardfraction_bad,
               "guardfraction-file-version 1\n"
               "written-at %s\n"
               "n-inputs 420 3\n"
               "guard-seen D0EDB47BEAD32D26D0A837F7D5357EC3AD3B8777 NaN 420\n"
               "guard-seen 07B5547026DF3E229806E135CFA8552D56AFBABC 5 420\n",
               yesterday_date_str);

  retval = dirserv_read_guardfraction_file_from_str(guardfraction_bad, NULL);
  tt_int_op(retval, ==, 1);
  tor_free(guardfraction_bad);

  /* This one is not a percentage (not in [0, 100]) */
  tor_asprintf(&guardfraction_bad,
               "guardfraction-file-version 1\n"
               "written-at %s\n"
               "n-inputs 420 3\n"
               "guard-seen D0EDB47BEAD32D26D0A837F7D5357EC3AD3B8777 666 420\n"
               "guard-seen 07B5547026DF3E229806E135CFA8552D56AFBABC 5 420\n",
               yesterday_date_str);

  retval = dirserv_read_guardfraction_file_from_str(guardfraction_bad, NULL);
  tt_int_op(retval, ==, 1);
  tor_free(guardfraction_bad);

  /* This one is not a percentage either (not in [0, 100]) */
  tor_asprintf(&guardfraction_bad,
               "guardfraction-file-version 1\n"
               "written-at %s\n"
               "n-inputs 420 3\n"
               "guard-seen D0EDB47BEAD32D26D0A837F7D5357EC3AD3B8777 -3 420\n",
               yesterday_date_str);

  retval = dirserv_read_guardfraction_file_from_str(guardfraction_bad, NULL);
  tt_int_op(retval, ==, 0);

 done:
  tor_free(guardfraction_bad);
}

/** Make sure that our test guardfraction file gets parsed properly, and
 * its information are applied properly to our routerstatuses. */
static void
test_parse_guardfraction_file_good(void *arg)
{
  int retval;
  vote_routerstatus_t *vrs_guard = NULL;
  vote_routerstatus_t *vrs_dummy = NULL;
  char *guardfraction_good = NULL;
  const char *yesterday_date_str = get_yesterday_date_str();
  smartlist_t *routerstatuses = smartlist_new();

  /* Some test values that we need to validate later */
  const char fpr_guard[] = "D0EDB47BEAD32D26D0A837F7D5357EC3AD3B8777";
  const char fpr_unlisted[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
  const int guardfraction_value = 42;

  (void) arg;

  {
    /* Populate the smartlist with some fake routerstatuses, so that
       after parsing the guardfraction file we can check that their
       elements got filled properly. */

    /* This one is a guard */
    vrs_guard = gen_vote_routerstatus_for_tests(fpr_guard, 1);
    tt_assert(vrs_guard);
    smartlist_add(routerstatuses, vrs_guard);

    /* This one is a guard but it's not in the guardfraction file */
    vrs_dummy = gen_vote_routerstatus_for_tests(fpr_unlisted, 1);
    tt_assert(vrs_dummy);
    smartlist_add(routerstatuses, vrs_dummy);
  }

  tor_asprintf(&guardfraction_good,
               "guardfraction-file-version 1\n"
               "written-at %s\n"
               "n-inputs 420 3\n"
               "guard-seen %s %d 420\n",
               yesterday_date_str,
               fpr_guard, guardfraction_value);

  /* Read the guardfraction file */
  retval = dirserv_read_guardfraction_file_from_str(guardfraction_good,
                                                    routerstatuses);
  tt_int_op(retval, ==, 1);

  { /* Test that routerstatus fields got filled properly */

    /* The guardfraction fields of the guard should be filled. */
    tt_assert(vrs_guard->status.has_guardfraction);
    tt_int_op(vrs_guard->status.guardfraction_percentage,
              ==,
              guardfraction_value);

    /* The guard that was not in the guardfraction file should not have
       been touched either. */
    tt_assert(!vrs_dummy->status.has_guardfraction);
  }

 done:
  vote_routerstatus_free(vrs_guard);
  vote_routerstatus_free(vrs_dummy);
  smartlist_free(routerstatuses);
  tor_free(guardfraction_good);
}

/** Make sure that the guardfraction bandwidths get calculated properly. */
static void
test_get_guardfraction_bandwidth(void *arg)
{
  guardfraction_bandwidth_t gf_bw;
  const int orig_bw = 1000;

  (void) arg;

  /* A guard with bandwidth 1000 and GuardFraction 0.25, should have
     bandwidth 250 as a guard and bandwidth 750 as a non-guard.  */
  guard_get_guardfraction_bandwidth(&gf_bw,
                                    orig_bw, 25);

  tt_int_op(gf_bw.guard_bw, ==, 250);
  tt_int_op(gf_bw.non_guard_bw, ==, 750);

  /* Also check the 'guard_bw + non_guard_bw == original_bw'
   * invariant. */
  tt_int_op(gf_bw.non_guard_bw + gf_bw.guard_bw, ==, orig_bw);

 done:
  ;
}

/** Parse the GuardFraction element of the consensus, and make sure it
 * gets parsed correctly. */
static void
test_parse_guardfraction_consensus(void *arg)
{
  int retval;
  or_options_t *options = get_options_mutable();

  const char *guardfraction_str_good = "GuardFraction=66";
  routerstatus_t rs_good;
  routerstatus_t rs_no_guard;

  const char *guardfraction_str_bad1 = "GuardFraction="; /* no value */
  routerstatus_t rs_bad1;

  const char *guardfraction_str_bad2 = "GuardFraction=166"; /* no percentage */
  routerstatus_t rs_bad2;

  (void) arg;

  /* GuardFraction use is currently disabled by default. So we need to
     manually enable it. */
  options->UseGuardFraction = 1;

  { /* Properly formatted GuardFraction. Check that it gets applied
       correctly. */
    memset(&rs_good, 0, sizeof(routerstatus_t));
    rs_good.is_possible_guard = 1;

    retval = routerstatus_parse_guardfraction(guardfraction_str_good,
                                              NULL, NULL,
                                              &rs_good);
    tt_int_op(retval, ==, 0);
    tt_assert(rs_good.has_guardfraction);
    tt_int_op(rs_good.guardfraction_percentage, ==, 66);
  }

  { /* Properly formatted GuardFraction but router is not a
       guard. GuardFraction should not get applied. */
    memset(&rs_no_guard, 0, sizeof(routerstatus_t));
    tt_assert(!rs_no_guard.is_possible_guard);

    retval = routerstatus_parse_guardfraction(guardfraction_str_good,
                                              NULL, NULL,
                                              &rs_no_guard);
    tt_int_op(retval, ==, 0);
    tt_assert(!rs_no_guard.has_guardfraction);
  }

  { /* Bad GuardFraction. Function should fail and not apply. */
    memset(&rs_bad1, 0, sizeof(routerstatus_t));
    rs_bad1.is_possible_guard = 1;

    retval = routerstatus_parse_guardfraction(guardfraction_str_bad1,
                                              NULL, NULL,
                                              &rs_bad1);
    tt_int_op(retval, ==, -1);
    tt_assert(!rs_bad1.has_guardfraction);
  }

  { /* Bad GuardFraction. Function should fail and not apply. */
    memset(&rs_bad2, 0, sizeof(routerstatus_t));
    rs_bad2.is_possible_guard = 1;

    retval = routerstatus_parse_guardfraction(guardfraction_str_bad2,
                                              NULL, NULL,
                                              &rs_bad2);
    tt_int_op(retval, ==, -1);
    tt_assert(!rs_bad2.has_guardfraction);
  }

 done:
  ;
}

/** Make sure that we use GuardFraction information when we should,
 * according to the torrc option and consensus parameter. */
static void
test_should_apply_guardfraction(void *arg)
{
  networkstatus_t vote_enabled, vote_disabled, vote_missing;
  or_options_t *options = get_options_mutable();

  (void) arg;

  { /* Fill the votes for later */
    /* This one suggests enabled GuardFraction. */
    memset(&vote_enabled, 0, sizeof(vote_enabled));
    vote_enabled.net_params = smartlist_new();
    smartlist_split_string(vote_enabled.net_params,
                           "UseGuardFraction=1", NULL, 0, 0);

    /* This one suggests disabled GuardFraction. */
    memset(&vote_disabled, 0, sizeof(vote_disabled));
    vote_disabled.net_params = smartlist_new();
    smartlist_split_string(vote_disabled.net_params,
                           "UseGuardFraction=0", NULL, 0, 0);

    /* This one doesn't have GuardFraction at all. */
    memset(&vote_missing, 0, sizeof(vote_missing));
    vote_missing.net_params = smartlist_new();
    smartlist_split_string(vote_missing.net_params,
                           "leon=trout", NULL, 0, 0);
  }

  /* If torrc option is set to yes, we should always use
   * guardfraction.*/
  options->UseGuardFraction = 1;
  tt_int_op(should_apply_guardfraction(&vote_disabled), ==, 1);

  /* If torrc option is set to no, we should never use
   * guardfraction.*/
  options->UseGuardFraction = 0;
  tt_int_op(should_apply_guardfraction(&vote_enabled), ==, 0);

  /* Now let's test torrc option set to auto. */
  options->UseGuardFraction = -1;

  /* If torrc option is set to auto, and consensus parameter is set to
   * yes, we should use guardfraction. */
  tt_int_op(should_apply_guardfraction(&vote_enabled), ==, 1);

  /* If torrc option is set to auto, and consensus parameter is set to
   * no, we should use guardfraction. */
  tt_int_op(should_apply_guardfraction(&vote_disabled), ==, 0);

  /* If torrc option is set to auto, and consensus parameter is not
   * set, we should fallback to "no". */
  tt_int_op(should_apply_guardfraction(&vote_missing), ==, 0);

 done:
  SMARTLIST_FOREACH(vote_enabled.net_params, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(vote_disabled.net_params, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(vote_missing.net_params, char *, cp, tor_free(cp));
  smartlist_free(vote_enabled.net_params);
  smartlist_free(vote_disabled.net_params);
  smartlist_free(vote_missing.net_params);
}

struct testcase_t guardfraction_tests[] = {
  { "parse_guardfraction_file_bad", test_parse_guardfraction_file_bad,
    TT_FORK, NULL, NULL },
  { "parse_guardfraction_file_good", test_parse_guardfraction_file_good,
    TT_FORK, NULL, NULL },
  { "parse_guardfraction_consensus", test_parse_guardfraction_consensus,
    TT_FORK, NULL, NULL },
  { "get_guardfraction_bandwidth", test_get_guardfraction_bandwidth,
    TT_FORK, NULL, NULL },
  { "should_apply_guardfraction", test_should_apply_guardfraction,
    TT_FORK, NULL, NULL },

  END_OF_TESTCASES
};

