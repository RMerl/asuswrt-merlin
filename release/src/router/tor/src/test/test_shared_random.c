#define SHARED_RANDOM_PRIVATE
#define SHARED_RANDOM_STATE_PRIVATE
#define CONFIG_PRIVATE
#define DIRVOTE_PRIVATE

#include "or.h"
#include "test.h"
#include "config.h"
#include "dirvote.h"
#include "shared_random.h"
#include "shared_random_state.h"
#include "routerkeys.h"
#include "routerlist.h"
#include "router.h"
#include "routerparse.h"
#include "networkstatus.h"
#include "log_test_helpers.h"

static authority_cert_t *mock_cert;

static authority_cert_t *
get_my_v3_authority_cert_m(void)
{
  tor_assert(mock_cert);
  return mock_cert;
}

static dir_server_t ds;

static dir_server_t *
trusteddirserver_get_by_v3_auth_digest_m(const char *digest)
{
  (void) digest;
  /* The shared random code only need to know if a valid pointer to a dir
   * server object has been found so this is safe because it won't use the
   * pointer at all never. */
  return &ds;
}

/* Setup a minimal dirauth environment by initializing the SR state and
 * making sure the options are set to be an authority directory. */
static void
init_authority_state(void)
{
  MOCK(get_my_v3_authority_cert, get_my_v3_authority_cert_m);

  or_options_t *options = get_options_mutable();
  mock_cert = authority_cert_parse_from_string(AUTHORITY_CERT_1, NULL);
  tt_assert(mock_cert);
  options->AuthoritativeDir = 1;
  tt_int_op(0, ==, load_ed_keys(options, time(NULL)));
  sr_state_init(0, 0);
  /* It's possible a commit has been generated in our state depending on
   * the phase we are currently in which uses "now" as the starting
   * timestamp. Delete it before we do any testing below. */
  sr_state_delete_commits();

 done:
  UNMOCK(get_my_v3_authority_cert);
}

static void
test_get_sr_protocol_phase(void *arg)
{
  time_t the_time;
  sr_phase_t phase;
  int retval;

  (void) arg;

  /* Initialize SR state */
  init_authority_state();

  {
    retval = parse_rfc1123_time("Wed, 20 Apr 2015 23:59:00 UTC", &the_time);
    tt_int_op(retval, ==, 0);

    phase = get_sr_protocol_phase(the_time);
    tt_int_op(phase, ==, SR_PHASE_REVEAL);
  }

  {
    retval = parse_rfc1123_time("Wed, 20 Apr 2015 00:00:00 UTC", &the_time);
    tt_int_op(retval, ==, 0);

    phase = get_sr_protocol_phase(the_time);
    tt_int_op(phase, ==, SR_PHASE_COMMIT);
  }

  {
    retval = parse_rfc1123_time("Wed, 20 Apr 2015 00:00:01 UTC", &the_time);
    tt_int_op(retval, ==, 0);

    phase = get_sr_protocol_phase(the_time);
    tt_int_op(phase, ==, SR_PHASE_COMMIT);
  }

  {
    retval = parse_rfc1123_time("Wed, 20 Apr 2015 11:59:00 UTC", &the_time);
    tt_int_op(retval, ==, 0);

    phase = get_sr_protocol_phase(the_time);
    tt_int_op(phase, ==, SR_PHASE_COMMIT);
  }

  {
    retval = parse_rfc1123_time("Wed, 20 Apr 2015 12:00:00 UTC", &the_time);
    tt_int_op(retval, ==, 0);

    phase = get_sr_protocol_phase(the_time);
    tt_int_op(phase, ==, SR_PHASE_REVEAL);
  }

  {
    retval = parse_rfc1123_time("Wed, 20 Apr 2015 12:00:01 UTC", &the_time);
    tt_int_op(retval, ==, 0);

    phase = get_sr_protocol_phase(the_time);
    tt_int_op(phase, ==, SR_PHASE_REVEAL);
  }

  {
    retval = parse_rfc1123_time("Wed, 20 Apr 2015 13:00:00 UTC", &the_time);
    tt_int_op(retval, ==, 0);

    phase = get_sr_protocol_phase(the_time);
    tt_int_op(phase, ==, SR_PHASE_REVEAL);
  }

 done:
  ;
}

static networkstatus_t *mock_consensus = NULL;

static void
test_get_state_valid_until_time(void *arg)
{
  time_t current_time;
  time_t valid_until_time;
  char tbuf[ISO_TIME_LEN + 1];
  int retval;

  (void) arg;

  {
    /* Get the valid until time if called at 00:00:01 */
    retval = parse_rfc1123_time("Mon, 20 Apr 2015 00:00:01 UTC",
                                &current_time);
    tt_int_op(retval, ==, 0);
    valid_until_time = get_state_valid_until_time(current_time);

    /* Compare it with the correct result */
    format_iso_time(tbuf, valid_until_time);
    tt_str_op("2015-04-21 00:00:00", OP_EQ, tbuf);
  }

  {
    retval = parse_rfc1123_time("Mon, 20 Apr 2015 19:22:00 UTC",
                                &current_time);
    tt_int_op(retval, ==, 0);
    valid_until_time = get_state_valid_until_time(current_time);

    format_iso_time(tbuf, valid_until_time);
    tt_str_op("2015-04-21 00:00:00", OP_EQ, tbuf);
  }

  {
    retval = parse_rfc1123_time("Mon, 20 Apr 2015 23:59:00 UTC",
                                &current_time);
    tt_int_op(retval, ==, 0);
    valid_until_time = get_state_valid_until_time(current_time);

    format_iso_time(tbuf, valid_until_time);
    tt_str_op("2015-04-21 00:00:00", OP_EQ, tbuf);
  }

  {
    retval = parse_rfc1123_time("Mon, 20 Apr 2015 00:00:00 UTC",
                                &current_time);
    tt_int_op(retval, ==, 0);
    valid_until_time = get_state_valid_until_time(current_time);

    format_iso_time(tbuf, valid_until_time);
    tt_str_op("2015-04-21 00:00:00", OP_EQ, tbuf);
  }

 done:
  ;
}

/* Mock function to immediately return our local 'mock_consensus'. */
static networkstatus_t *
mock_networkstatus_get_live_consensus(time_t now)
{
  (void) now;
  return mock_consensus;
}

/** Test the get_next_valid_after_time() function. */
static void
test_get_next_valid_after_time(void *arg)
{
  time_t current_time;
  time_t valid_after_time;
  char tbuf[ISO_TIME_LEN + 1];
  int retval;

  (void) arg;

  {
    /* Setup a fake consensus just to get the times out of it, since
       get_next_valid_after_time() needs them. */
    mock_consensus = tor_malloc_zero(sizeof(networkstatus_t));

    retval = parse_rfc1123_time("Mon, 13 Jan 2016 16:00:00 UTC",
                                &mock_consensus->fresh_until);
    tt_int_op(retval, ==, 0);

    retval = parse_rfc1123_time("Mon, 13 Jan 2016 15:00:00 UTC",
                                &mock_consensus->valid_after);
    tt_int_op(retval, ==, 0);

    MOCK(networkstatus_get_live_consensus,
         mock_networkstatus_get_live_consensus);
  }

  {
    /* Get the valid after time if called at 00:00:00 */
    retval = parse_rfc1123_time("Mon, 20 Apr 2015 00:00:00 UTC",
                                &current_time);
    tt_int_op(retval, ==, 0);
    valid_after_time = get_next_valid_after_time(current_time);

    /* Compare it with the correct result */
    format_iso_time(tbuf, valid_after_time);
    tt_str_op("2015-04-20 01:00:00", OP_EQ, tbuf);
  }

  {
    /* Get the valid until time if called at 00:00:01 */
    retval = parse_rfc1123_time("Mon, 20 Apr 2015 00:00:01 UTC",
                                &current_time);
    tt_int_op(retval, ==, 0);
    valid_after_time = get_next_valid_after_time(current_time);

    /* Compare it with the correct result */
    format_iso_time(tbuf, valid_after_time);
    tt_str_op("2015-04-20 01:00:00", OP_EQ, tbuf);
 }

  {
    retval = parse_rfc1123_time("Mon, 20 Apr 2015 23:30:01 UTC",
                                &current_time);
    tt_int_op(retval, ==, 0);
    valid_after_time = get_next_valid_after_time(current_time);

    /* Compare it with the correct result */
    format_iso_time(tbuf, valid_after_time);
    tt_str_op("2015-04-21 00:00:00", OP_EQ, tbuf);
 }

 done:
  networkstatus_vote_free(mock_consensus);
}

/* In this test we are going to generate a sr_commit_t object and validate
 * it. We first generate our values, and then we parse them as if they were
 * received from the network. After we parse both the commit and the reveal,
 * we verify that they indeed match. */
static void
test_sr_commit(void *arg)
{
  authority_cert_t *auth_cert = NULL;
  time_t now = time(NULL);
  sr_commit_t *our_commit = NULL;
  smartlist_t *args = smartlist_new();
  sr_commit_t *parsed_commit = NULL;

  (void) arg;

  {  /* Setup a minimal dirauth environment for this test  */
    or_options_t *options = get_options_mutable();

    auth_cert = authority_cert_parse_from_string(AUTHORITY_CERT_1, NULL);
    tt_assert(auth_cert);

    options->AuthoritativeDir = 1;
    tt_int_op(0, ==, load_ed_keys(options, now));
  }

  /* Generate our commit object and validate it has the appropriate field
   * that we can then use to build a representation that we'll find in a
   * vote coming from the network. */
  {
    sr_commit_t test_commit;
    our_commit = sr_generate_our_commit(now, auth_cert);
    tt_assert(our_commit);
    /* Default and only supported algorithm for now. */
    tt_assert(our_commit->alg == DIGEST_SHA3_256);
    /* We should have a reveal value. */
    tt_assert(commit_has_reveal_value(our_commit));
    /* We should have a random value. */
    tt_assert(!tor_mem_is_zero((char *) our_commit->random_number,
                               sizeof(our_commit->random_number)));
    /* Commit and reveal timestamp should be the same. */
    tt_u64_op(our_commit->commit_ts, ==, our_commit->reveal_ts);
    /* We should have a hashed reveal. */
    tt_assert(!tor_mem_is_zero(our_commit->hashed_reveal,
                               sizeof(our_commit->hashed_reveal)));
    /* Do we have a valid encoded commit and reveal. Note the following only
     * tests if the generated values are correct. Their could be a bug in
     * the decode function but we test them seperately. */
    tt_int_op(0, ==, reveal_decode(our_commit->encoded_reveal,
                                   &test_commit));
    tt_int_op(0, ==, commit_decode(our_commit->encoded_commit,
                                   &test_commit));
    tt_int_op(0, ==, verify_commit_and_reveal(our_commit));
  }

  /* Let's make sure our verify commit and reveal function works. We'll
   * make it fail a bit with known failure case. */
  {
    /* Copy our commit so we don't alter it for the rest of testing. */
    sr_commit_t test_commit;
    memcpy(&test_commit, our_commit, sizeof(test_commit));

    /* Timestamp MUST match. */
    test_commit.commit_ts = test_commit.reveal_ts - 42;
    setup_full_capture_of_logs(LOG_WARN);
    tt_int_op(-1, ==, verify_commit_and_reveal(&test_commit));
    expect_log_msg_containing("doesn't match reveal timestamp");
    teardown_capture_of_logs();
    memcpy(&test_commit, our_commit, sizeof(test_commit));
    tt_int_op(0, ==, verify_commit_and_reveal(&test_commit));

    /* Hashed reveal must match the H(encoded_reveal). */
    memset(test_commit.hashed_reveal, 'X',
           sizeof(test_commit.hashed_reveal));
    setup_full_capture_of_logs(LOG_WARN);
    tt_int_op(-1, ==, verify_commit_and_reveal(&test_commit));
    expect_single_log_msg_containing("doesn't match the commit value");
    teardown_capture_of_logs();
    memcpy(&test_commit, our_commit, sizeof(test_commit));
    tt_int_op(0, ==, verify_commit_and_reveal(&test_commit));
  }

  /* We'll build a list of values from our commit that our parsing function
   * takes from a vote line and see if we can parse it correctly. */
  {
    smartlist_add(args, tor_strdup("1"));
    smartlist_add(args,
               tor_strdup(crypto_digest_algorithm_get_name(our_commit->alg)));
    smartlist_add(args, tor_strdup(sr_commit_get_rsa_fpr(our_commit)));
    smartlist_add(args, tor_strdup(our_commit->encoded_commit));
    smartlist_add(args, tor_strdup(our_commit->encoded_reveal));
    parsed_commit = sr_parse_commit(args);
    tt_assert(parsed_commit);
    /* That parsed commit should be _EXACTLY_ like our original commit (we
     * have to explicitly set the valid flag though). */
    parsed_commit->valid = 1;
    tt_mem_op(parsed_commit, OP_EQ, our_commit, sizeof(*parsed_commit));
    /* Cleanup */
  }

 done:
  teardown_capture_of_logs();
  SMARTLIST_FOREACH(args, char *, cp, tor_free(cp));
  smartlist_free(args);
  sr_commit_free(our_commit);
  sr_commit_free(parsed_commit);
  authority_cert_free(auth_cert);
}

/* Test the encoding and decoding function for commit and reveal values. */
static void
test_encoding(void *arg)
{
  (void) arg;
  int ret;
  /* Random number is 32 bytes. */
  char raw_rand[32];
  time_t ts = 1454333590;
  char hashed_rand[DIGEST256_LEN], hashed_reveal[DIGEST256_LEN];
  sr_commit_t parsed_commit;

  /* Those values were generated by sr_commit_calc_ref.py where the random
   * value is 32 'A' and timestamp is the one in ts. */
  static const char *encoded_reveal =
    "AAAAAFavXpZJxbwTupvaJCTeIUCQmOPxAMblc7ChL5H2nZKuGchdaA==";
  static const char *encoded_commit =
    "AAAAAFavXpbkBMzMQG7aNoaGLFNpm2Wkk1ozXhuWWqL//GynltxVAg==";

  /* Set up our raw random bytes array. */
  memset(raw_rand, 'A', sizeof(raw_rand));
  /* Hash random number because we don't expose bytes of the RNG. */
  ret = crypto_digest256(hashed_rand, raw_rand,
                         sizeof(raw_rand), SR_DIGEST_ALG);
  tt_int_op(0, ==, ret);
  /* Hash reveal value. */
  tt_int_op(SR_REVEAL_BASE64_LEN, ==, strlen(encoded_reveal));
  ret = crypto_digest256(hashed_reveal, encoded_reveal,
                         strlen(encoded_reveal), SR_DIGEST_ALG);
  tt_int_op(0, ==, ret);
  tt_int_op(SR_COMMIT_BASE64_LEN, ==, strlen(encoded_commit));

  /* Test our commit/reveal decode functions. */
  {
    /* Test the reveal encoded value. */
    tt_int_op(0, ==, reveal_decode(encoded_reveal, &parsed_commit));
    tt_u64_op(ts, ==, parsed_commit.reveal_ts);
    tt_mem_op(hashed_rand, OP_EQ, parsed_commit.random_number,
              sizeof(hashed_rand));

    /* Test the commit encoded value. */
    memset(&parsed_commit, 0, sizeof(parsed_commit));
    tt_int_op(0, ==, commit_decode(encoded_commit, &parsed_commit));
    tt_u64_op(ts, ==, parsed_commit.commit_ts);
    tt_mem_op(encoded_commit, OP_EQ, parsed_commit.encoded_commit,
              sizeof(parsed_commit.encoded_commit));
    tt_mem_op(hashed_reveal, OP_EQ, parsed_commit.hashed_reveal,
              sizeof(hashed_reveal));
  }

  /* Test our commit/reveal encode functions. */
  {
    /* Test the reveal encode. */
    char encoded[SR_REVEAL_BASE64_LEN + 1];
    parsed_commit.reveal_ts = ts;
    memcpy(parsed_commit.random_number, hashed_rand,
           sizeof(parsed_commit.random_number));
    ret = reveal_encode(&parsed_commit, encoded, sizeof(encoded));
    tt_int_op(SR_REVEAL_BASE64_LEN, ==, ret);
    tt_mem_op(encoded_reveal, OP_EQ, encoded, strlen(encoded_reveal));
  }

  {
    /* Test the commit encode. */
    char encoded[SR_COMMIT_BASE64_LEN + 1];
    parsed_commit.commit_ts = ts;
    memcpy(parsed_commit.hashed_reveal, hashed_reveal,
           sizeof(parsed_commit.hashed_reveal));
    ret = commit_encode(&parsed_commit, encoded, sizeof(encoded));
    tt_int_op(SR_COMMIT_BASE64_LEN, ==, ret);
    tt_mem_op(encoded_commit, OP_EQ, encoded, strlen(encoded_commit));
  }

 done:
  ;
}

/** Setup some SRVs in our SR state. If <b>also_current</b> is set, then set
 *  both current and previous SRVs.
 *  Helper of test_vote() and test_sr_compute_srv(). */
static void
test_sr_setup_srv(int also_current)
{
  sr_srv_t *srv = tor_malloc_zero(sizeof(sr_srv_t));
  srv->num_reveals = 42;
  memcpy(srv->value,
         "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ",
         sizeof(srv->value));

 sr_state_set_previous_srv(srv);

 if (also_current) {
   srv = tor_malloc_zero(sizeof(sr_srv_t));
   srv->num_reveals = 128;
   memcpy(srv->value,
          "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN",
          sizeof(srv->value));

   sr_state_set_current_srv(srv);
 }
}

/* Test anything that has to do with SR protocol and vote. */
static void
test_vote(void *arg)
{
  int ret;
  time_t now = time(NULL);
  sr_commit_t *our_commit = NULL;

  (void) arg;

  MOCK(trusteddirserver_get_by_v3_auth_digest,
       trusteddirserver_get_by_v3_auth_digest_m);

  {  /* Setup a minimal dirauth environment for this test  */
    init_authority_state();
    /* Set ourself in reveal phase so we can parse the reveal value in the
     * vote as well. */
    set_sr_phase(SR_PHASE_REVEAL);
  }

  /* Generate our commit object and validate it has the appropriate field
   * that we can then use to build a representation that we'll find in a
   * vote coming from the network. */
  {
    sr_commit_t *saved_commit;
    our_commit = sr_generate_our_commit(now, mock_cert);
    tt_assert(our_commit);
    sr_state_add_commit(our_commit);
    /* Make sure it's there. */
    saved_commit = sr_state_get_commit(our_commit->rsa_identity);
    tt_assert(saved_commit);
  }

  /* Also setup the SRVs */
  test_sr_setup_srv(1);

  { /* Now test the vote generation */
    smartlist_t *chunks = smartlist_new();
    smartlist_t *tokens = smartlist_new();
    /* Get our vote line and validate it. */
    char *lines = sr_get_string_for_vote();
    tt_assert(lines);
    /* Split the lines. We expect 2 here. */
    ret = smartlist_split_string(chunks, lines, "\n", SPLIT_IGNORE_BLANK, 0);
    tt_int_op(ret, ==, 4);
    tt_str_op(smartlist_get(chunks, 0), OP_EQ, "shared-rand-participate");
    /* Get our commitment line and will validate it agains our commit. The
     * format is as follow:
     * "shared-rand-commitment" SP version SP algname SP identity
     *                          SP COMMIT [SP REVEAL] NL
     */
    char *commit_line = smartlist_get(chunks, 1);
    tt_assert(commit_line);
    ret = smartlist_split_string(tokens, commit_line, " ", 0, 0);
    tt_int_op(ret, ==, 6);
    tt_str_op(smartlist_get(tokens, 0), OP_EQ, "shared-rand-commit");
    tt_str_op(smartlist_get(tokens, 1), OP_EQ, "1");
    tt_str_op(smartlist_get(tokens, 2), OP_EQ,
              crypto_digest_algorithm_get_name(DIGEST_SHA3_256));
    char digest[DIGEST_LEN];
    base16_decode(digest, sizeof(digest), smartlist_get(tokens, 3),
                  HEX_DIGEST_LEN);
    tt_mem_op(digest, ==, our_commit->rsa_identity, sizeof(digest));
    tt_str_op(smartlist_get(tokens, 4), OP_EQ, our_commit->encoded_commit);
    tt_str_op(smartlist_get(tokens, 5), OP_EQ, our_commit->encoded_reveal)
;
    /* Finally, does this vote line creates a valid commit object? */
    smartlist_t *args = smartlist_new();
    smartlist_add(args, smartlist_get(tokens, 1));
    smartlist_add(args, smartlist_get(tokens, 2));
    smartlist_add(args, smartlist_get(tokens, 3));
    smartlist_add(args, smartlist_get(tokens, 4));
    smartlist_add(args, smartlist_get(tokens, 5));
    sr_commit_t *parsed_commit = sr_parse_commit(args);
    tt_assert(parsed_commit);
    /* Set valid flag explicitly here to compare since it's not set by
     * simply parsing the commit. */
    parsed_commit->valid = 1;
    tt_mem_op(parsed_commit, ==, our_commit, sizeof(*our_commit));

    /* minor cleanup */
    SMARTLIST_FOREACH(tokens, char *, s, tor_free(s));
    smartlist_clear(tokens);

    /* Now test the previous SRV */
    char *prev_srv_line = smartlist_get(chunks, 2);
    tt_assert(prev_srv_line);
    ret = smartlist_split_string(tokens, prev_srv_line, " ", 0, 0);
    tt_int_op(ret, ==, 3);
    tt_str_op(smartlist_get(tokens, 0), OP_EQ, "shared-rand-previous-value");
    tt_str_op(smartlist_get(tokens, 1), OP_EQ, "42");
    tt_str_op(smartlist_get(tokens, 2), OP_EQ,
           "WlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlpaWlo=");

    /* minor cleanup */
    SMARTLIST_FOREACH(tokens, char *, s, tor_free(s));
    smartlist_clear(tokens);

    /* Now test the current SRV */
    char *current_srv_line = smartlist_get(chunks, 3);
    tt_assert(current_srv_line);
    ret = smartlist_split_string(tokens, current_srv_line, " ", 0, 0);
    tt_int_op(ret, ==, 3);
    tt_str_op(smartlist_get(tokens, 0), OP_EQ, "shared-rand-current-value");
    tt_str_op(smartlist_get(tokens, 1), OP_EQ, "128");
    tt_str_op(smartlist_get(tokens, 2), OP_EQ,
           "Tk5OTk5OTk5OTk5OTk5OTk5OTk5OTk5OTk5OTk5OTk4=");

    /* Clean up */
    sr_commit_free(parsed_commit);
    SMARTLIST_FOREACH(chunks, char *, s, tor_free(s));
    smartlist_free(chunks);
    SMARTLIST_FOREACH(tokens, char *, s, tor_free(s));
    smartlist_free(tokens);
    smartlist_clear(args);
    smartlist_free(args);
    tor_free(lines);
  }

 done:
  sr_commit_free(our_commit);
  UNMOCK(trusteddirserver_get_by_v3_auth_digest);
}

static const char *sr_state_str = "Version 1\n"
  "TorVersion 0.2.9.0-alpha-dev\n"
  "ValidAfter 2037-04-19 07:16:00\n"
  "ValidUntil 2037-04-20 07:16:00\n"
  "Commit 1 sha3-256 FA3CEC2C99DC68D3166B9B6E4FA21A4026C2AB1C "
      "7M8GdubCAAdh7WUG0DiwRyxTYRKji7HATa7LLJEZ/UAAAAAAVmfUSg== "
      "AAAAAFZn1EojfIheIw42bjK3VqkpYyjsQFSbv/dxNna3Q8hUEPKpOw==\n"
  "Commit 1 sha3-256 41E89EDFBFBA44983E21F18F2230A4ECB5BFB543 "
     "17aUsYuMeRjd2N1r8yNyg7aHqRa6gf4z7QPoxxAZbp0AAAAAVmfUSg==\n"
  "Commit 1 sha3-256 36637026573A04110CF3E6B1D201FB9A98B88734 "
     "DDDYtripvdOU+XPEUm5xpU64d9IURSds1xSwQsgeB8oAAAAAVmfUSg==\n"
  "SharedRandPreviousValue 4 qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqo=\n"
  "SharedRandCurrentValue 3 8dWeW12KEzTGEiLGgO1UVJ7Z91CekoRcxt6Q9KhnOFI=\n";

/** Create an SR disk state, parse it and validate that the parsing went
 *  well. Yes! */
static void
test_state_load_from_disk(void *arg)
{
  int ret;
  char *dir = tor_strdup(get_fname("test_sr_state"));
  char *sr_state_path = tor_strdup(get_fname("test_sr_state/sr_state"));
  sr_state_t *the_sr_state = NULL;

  (void) arg;

  MOCK(trusteddirserver_get_by_v3_auth_digest,
       trusteddirserver_get_by_v3_auth_digest_m);

  /* First try with a nonexistent path. */
  ret = disk_state_load_from_disk_impl("NONEXISTENTNONEXISTENT");
  tt_assert(ret == -ENOENT);

  /* Now create a mock state directory and state file */
#ifdef _WIN32
  ret = mkdir(dir);
#else
  ret = mkdir(dir, 0700);
#endif
  tt_assert(ret == 0);
  ret = write_str_to_file(sr_state_path, sr_state_str, 0);
  tt_assert(ret == 0);

  /* Try to load the directory itself. Should fail. */
  ret = disk_state_load_from_disk_impl(dir);
  tt_int_op(ret, OP_LT, 0);

  /* State should be non-existent at this point. */
  the_sr_state = get_sr_state();
  tt_assert(!the_sr_state);

  /* Now try to load the correct file! */
  ret = disk_state_load_from_disk_impl(sr_state_path);
  tt_assert(ret == 0);

  /* Check the content of the state */
  /* XXX check more deeply!!! */
  the_sr_state = get_sr_state();
  tt_assert(the_sr_state);
  tt_assert(the_sr_state->version == 1);
  tt_assert(digestmap_size(the_sr_state->commits) == 3);
  tt_assert(the_sr_state->current_srv);
  tt_assert(the_sr_state->current_srv->num_reveals == 3);
  tt_assert(the_sr_state->previous_srv);

  /* XXX Now also try loading corrupted state files and make sure parsing
     fails */

 done:
  tor_free(dir);
  tor_free(sr_state_path);
  UNMOCK(trusteddirserver_get_by_v3_auth_digest);
}

/** Generate three specially crafted commits (based on the test
 *  vector at sr_srv_calc_ref.py). Helper of test_sr_compute_srv(). */
static void
test_sr_setup_commits(void)
{
  time_t now = time(NULL);
  sr_commit_t *commit_a, *commit_b, *commit_c, *commit_d;
  sr_commit_t *place_holder = tor_malloc_zero(sizeof(*place_holder));
  authority_cert_t *auth_cert = NULL;

  {  /* Setup a minimal dirauth environment for this test  */
    or_options_t *options = get_options_mutable();

    auth_cert = authority_cert_parse_from_string(AUTHORITY_CERT_1, NULL);
    tt_assert(auth_cert);

    options->AuthoritativeDir = 1;
    tt_int_op(0, ==, load_ed_keys(options, now));
  }

  /* Generate three dummy commits according to sr_srv_calc_ref.py .  Then
     register them to the SR state. Also register a fourth commit 'd' with no
     reveal info, to make sure that it will get ignored during SRV
     calculation. */

  { /* Commit from auth 'a' */
    commit_a = sr_generate_our_commit(now, auth_cert);
    tt_assert(commit_a);

    /* Do some surgery on the commit */
    memset(commit_a->rsa_identity, 'A', sizeof(commit_a->rsa_identity));
    base16_encode(commit_a->rsa_identity_hex,
                  sizeof(commit_a->rsa_identity_hex), commit_a->rsa_identity,
                  sizeof(commit_a->rsa_identity));
    strlcpy(commit_a->encoded_reveal,
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
            sizeof(commit_a->encoded_reveal));
    memcpy(commit_a->hashed_reveal,
           "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
           sizeof(commit_a->hashed_reveal));
  }

  { /* Commit from auth 'b' */
    commit_b = sr_generate_our_commit(now, auth_cert);
    tt_assert(commit_b);

    /* Do some surgery on the commit */
    memset(commit_b->rsa_identity, 'B', sizeof(commit_b->rsa_identity));
    base16_encode(commit_b->rsa_identity_hex,
                  sizeof(commit_b->rsa_identity_hex), commit_b->rsa_identity,
                  sizeof(commit_b->rsa_identity));
    strlcpy(commit_b->encoded_reveal,
            "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
          sizeof(commit_b->encoded_reveal));
    memcpy(commit_b->hashed_reveal,
           "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB",
           sizeof(commit_b->hashed_reveal));
  }

  { /* Commit from auth 'c' */
    commit_c = sr_generate_our_commit(now, auth_cert);
    tt_assert(commit_c);

    /* Do some surgery on the commit */
    memset(commit_c->rsa_identity, 'C', sizeof(commit_c->rsa_identity));
    base16_encode(commit_c->rsa_identity_hex,
                  sizeof(commit_c->rsa_identity_hex), commit_c->rsa_identity,
                  sizeof(commit_c->rsa_identity));
    strlcpy(commit_c->encoded_reveal,
            "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
            sizeof(commit_c->encoded_reveal));
    memcpy(commit_c->hashed_reveal,
           "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
           sizeof(commit_c->hashed_reveal));
  }

  { /* Commit from auth 'd' */
    commit_d = sr_generate_our_commit(now, auth_cert);
    tt_assert(commit_d);

    /* Do some surgery on the commit */
    memset(commit_d->rsa_identity, 'D', sizeof(commit_d->rsa_identity));
    base16_encode(commit_d->rsa_identity_hex,
                  sizeof(commit_d->rsa_identity_hex), commit_d->rsa_identity,
                  sizeof(commit_d->rsa_identity));
    strlcpy(commit_d->encoded_reveal,
            "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
            sizeof(commit_d->encoded_reveal));
    memcpy(commit_d->hashed_reveal,
           "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
           sizeof(commit_d->hashed_reveal));
    /* Clean up its reveal info */
    memcpy(place_holder, commit_d, sizeof(*place_holder));
    memset(commit_d->encoded_reveal, 0, sizeof(commit_d->encoded_reveal));
    tt_assert(!commit_has_reveal_value(commit_d));
  }

  /* Register commits to state (during commit phase) */
  set_sr_phase(SR_PHASE_COMMIT);
  save_commit_to_state(commit_a);
  save_commit_to_state(commit_b);
  save_commit_to_state(commit_c);
  save_commit_to_state(commit_d);
  tt_int_op(digestmap_size(get_sr_state()->commits), ==, 4);

  /* Now during REVEAL phase save commit D by restoring its reveal. */
  set_sr_phase(SR_PHASE_REVEAL);
  save_commit_to_state(place_holder);
  tt_str_op(commit_d->encoded_reveal, OP_EQ,
            "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD");
  /* Go back to an empty encoded reveal value. */
  memset(commit_d->encoded_reveal, 0, sizeof(commit_d->encoded_reveal));
  memset(commit_d->random_number, 0, sizeof(commit_d->random_number));
  tt_assert(!commit_has_reveal_value(commit_d));

 done:
  authority_cert_free(auth_cert);
}

/** Verify that the SRV generation procedure is proper by testing it against
 *  the test vector from ./sr_srv_calc_ref.py. */
static void
test_sr_compute_srv(void *arg)
{
  (void) arg;
  const sr_srv_t *current_srv = NULL;

#define SRV_TEST_VECTOR \
  "2A9B1D6237DAB312A40F575DA85C147663E7ED3F80E9555395F15B515C74253D"

  MOCK(trusteddirserver_get_by_v3_auth_digest,
       trusteddirserver_get_by_v3_auth_digest_m);

  init_authority_state();

  /* Setup the commits for this unittest */
  test_sr_setup_commits();
  test_sr_setup_srv(0);

  /* Now switch to reveal phase */
  set_sr_phase(SR_PHASE_REVEAL);

  /* Compute the SRV */
  sr_compute_srv();

  /* Check the result against the test vector */
  current_srv = sr_state_get_current_srv();
  tt_assert(current_srv);
  tt_u64_op(current_srv->num_reveals, ==, 3);
  tt_str_op(hex_str((char*)current_srv->value, 32),
            ==,
            SRV_TEST_VECTOR);

 done:
  UNMOCK(trusteddirserver_get_by_v3_auth_digest);
}

/** Return a minimal vote document with a current SRV value set to
 *  <b>srv</b>. */
static networkstatus_t *
get_test_vote_with_curr_srv(const char *srv)
{
  networkstatus_t *vote = tor_malloc_zero(sizeof(networkstatus_t));

  vote->type = NS_TYPE_VOTE;
  vote->sr_info.participate = 1;
  vote->sr_info.current_srv = tor_malloc_zero(sizeof(sr_srv_t));
  vote->sr_info.current_srv->num_reveals = 42;
  memcpy(vote->sr_info.current_srv->value,
         srv,
         sizeof(vote->sr_info.current_srv->value));

  return vote;
}

/* Test the function that picks the right SRV given a bunch of votes. Make sure
 * that the function returns an SRV iff the majority/agreement requirements are
 * met. */
static void
test_sr_get_majority_srv_from_votes(void *arg)
{
  sr_srv_t *chosen_srv;
  smartlist_t *votes = smartlist_new();

#define SRV_1 "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define SRV_2 "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"

  (void) arg;

  init_authority_state();
  /* Make sure our SRV is fresh so we can consider the super majority with
   * the consensus params of number of agreements needed. */
  sr_state_set_fresh_srv();

  /* The test relies on the dirauth list being initialized. */
  clear_dir_servers();
  add_default_trusted_dir_authorities(V3_DIRINFO);

  { /* Prepare voting environment with just a single vote. */
    networkstatus_t *vote = get_test_vote_with_curr_srv(SRV_1);
    smartlist_add(votes, vote);
  }

  /* Since it's only one vote with an SRV, it should not achieve majority and
     hence no SRV will be returned. */
  chosen_srv = get_majority_srv_from_votes(votes, 1);
  tt_assert(!chosen_srv);

  { /* Now put in 8 more votes. Let SRV_1 have majority. */
    int i;
    /* Now 7 votes believe in SRV_1 */
    for (i = 0; i < 3; i++) {
      networkstatus_t *vote = get_test_vote_with_curr_srv(SRV_1);
      smartlist_add(votes, vote);
    }
    /* and 2 votes believe in SRV_2 */
    for (i = 0; i < 2; i++) {
      networkstatus_t *vote = get_test_vote_with_curr_srv(SRV_2);
      smartlist_add(votes, vote);
    }
    for (i = 0; i < 3; i++) {
      networkstatus_t *vote = get_test_vote_with_curr_srv(SRV_1);
      smartlist_add(votes, vote);
    }

    tt_int_op(smartlist_len(votes), ==, 9);
  }

  /* Now we achieve majority for SRV_1, but not the AuthDirNumSRVAgreements
     requirement. So still not picking an SRV. */
  set_num_srv_agreements(8);
  chosen_srv = get_majority_srv_from_votes(votes, 1);
  tt_assert(!chosen_srv);

  /* We will now lower the AuthDirNumSRVAgreements requirement by tweaking the
   * consensus parameter and we will try again. This time it should work. */
  set_num_srv_agreements(7);
  chosen_srv = get_majority_srv_from_votes(votes, 1);
  tt_assert(chosen_srv);
  tt_u64_op(chosen_srv->num_reveals, ==, 42);
  tt_mem_op(chosen_srv->value, OP_EQ, SRV_1, sizeof(chosen_srv->value));

 done:
  SMARTLIST_FOREACH(votes, networkstatus_t *, vote,
                    networkstatus_vote_free(vote));
  smartlist_free(votes);
}

static void
test_utils(void *arg)
{
  (void) arg;

  /* Testing srv_dup(). */
  {
    sr_srv_t *srv = NULL, *dup_srv = NULL;
    const char *srv_value =
      "1BDB7C3E973936E4D13A49F37C859B3DC69C429334CF9412E3FEF6399C52D47A";
    srv = tor_malloc_zero(sizeof(*srv));
    srv->num_reveals = 42;
    memcpy(srv->value, srv_value, sizeof(srv->value));
    dup_srv = srv_dup(srv);
    tt_assert(dup_srv);
    tt_u64_op(dup_srv->num_reveals, ==, srv->num_reveals);
    tt_mem_op(dup_srv->value, OP_EQ, srv->value, sizeof(srv->value));
    tor_free(srv);
    tor_free(dup_srv);
  }

  /* Testing commitments_are_the_same(). Currently, the check is to test the
   * value of the encoded commit so let's make sure that actually works. */
  {
    /* Payload of 57 bytes that is the length of sr_commit_t->encoded_commit.
     * 56 bytes of payload and a NUL terminated byte at the end ('\x00')
     * which comes down to SR_COMMIT_BASE64_LEN + 1. */
    const char *payload =
      "\x5d\xb9\x60\xb6\xcc\x51\x68\x52\x31\xd9\x88\x88\x71\x71\xe0\x30"
      "\x59\x55\x7f\xcd\x61\xc0\x4b\x05\xb8\xcd\xc1\x48\xe9\xcd\x16\x1f"
      "\x70\x15\x0c\xfc\xd3\x1a\x75\xd0\x93\x6c\xc4\xe0\x5c\xbe\xe2\x18"
      "\xc7\xaf\x72\xb6\x7c\x9b\x52\x00";
    sr_commit_t commit1, commit2;
    memcpy(commit1.encoded_commit, payload, sizeof(commit1.encoded_commit));
    memcpy(commit2.encoded_commit, payload, sizeof(commit2.encoded_commit));
    tt_int_op(commitments_are_the_same(&commit1, &commit2), ==, 1);
    /* Let's corrupt one of them. */
    memset(commit1.encoded_commit, 'A', sizeof(commit1.encoded_commit));
    tt_int_op(commitments_are_the_same(&commit1, &commit2), ==, 0);
  }

  /* Testing commit_is_authoritative(). */
  {
    crypto_pk_t *k = crypto_pk_new();
    char digest[DIGEST_LEN];
    sr_commit_t commit;

    tt_assert(!crypto_pk_generate_key(k));

    tt_int_op(0, ==, crypto_pk_get_digest(k, digest));
    memcpy(commit.rsa_identity, digest, sizeof(commit.rsa_identity));
    tt_int_op(commit_is_authoritative(&commit, digest), ==, 1);
    /* Change the pubkey. */
    memset(commit.rsa_identity, 0, sizeof(commit.rsa_identity));
    tt_int_op(commit_is_authoritative(&commit, digest), ==, 0);
    crypto_pk_free(k);
  }

  /* Testing get_phase_str(). */
  {
    tt_str_op(get_phase_str(SR_PHASE_REVEAL), ==, "reveal");
    tt_str_op(get_phase_str(SR_PHASE_COMMIT), ==, "commit");
  }

  /* Testing phase transition */
  {
    init_authority_state();
    set_sr_phase(SR_PHASE_COMMIT);
    tt_int_op(is_phase_transition(SR_PHASE_REVEAL), ==, 1);
    tt_int_op(is_phase_transition(SR_PHASE_COMMIT), ==, 0);
    set_sr_phase(SR_PHASE_REVEAL);
    tt_int_op(is_phase_transition(SR_PHASE_REVEAL), ==, 0);
    tt_int_op(is_phase_transition(SR_PHASE_COMMIT), ==, 1);
    /* Junk. */
    tt_int_op(is_phase_transition(42), ==, 1);
  }

 done:
  return;
}

static void
test_state_transition(void *arg)
{
  sr_state_t *state = NULL;
  time_t now = time(NULL);

  (void) arg;

  {  /* Setup a minimal dirauth environment for this test  */
    init_authority_state();
    state = get_sr_state();
    tt_assert(state);
  }

  /* Test our state reset for a new protocol run. */
  {
    /* Add a commit to the state so we can test if the reset cleans the
     * commits. Also, change all params that we expect to be updated. */
    sr_commit_t *commit = sr_generate_our_commit(now, mock_cert);
    tt_assert(commit);
    sr_state_add_commit(commit);
    tt_int_op(digestmap_size(state->commits), ==, 1);
    /* Let's test our delete feature. */
    sr_state_delete_commits();
    tt_int_op(digestmap_size(state->commits), ==, 0);
    /* Add it back so we can continue the rest of the test because after
     * deletiong our commit will be freed so generate a new one. */
    commit = sr_generate_our_commit(now, mock_cert);
    tt_assert(commit);
    sr_state_add_commit(commit);
    tt_int_op(digestmap_size(state->commits), ==, 1);
    state->n_reveal_rounds = 42;
    state->n_commit_rounds = 43;
    state->n_protocol_runs = 44;
    reset_state_for_new_protocol_run(now);
    tt_int_op(state->n_reveal_rounds, ==, 0);
    tt_int_op(state->n_commit_rounds, ==, 0);
    tt_u64_op(state->n_protocol_runs, ==, 45);
    tt_int_op(digestmap_size(state->commits), ==, 0);
  }

  /* Test SRV rotation in our state. */
  {
    const sr_srv_t *cur, *prev;
    test_sr_setup_srv(1);
    cur = sr_state_get_current_srv();
    tt_assert(cur);
    /* After, current srv should be the previous and then set to NULL. */
    state_rotate_srv();
    prev = sr_state_get_previous_srv();
    tt_assert(prev == cur);
    tt_assert(!sr_state_get_current_srv());
    sr_state_clean_srvs();
  }

  /* New protocol run. */
  {
    const sr_srv_t *cur;
    /* Setup some new SRVs so we can confirm that a new protocol run
     * actually makes them rotate and compute new ones. */
    test_sr_setup_srv(1);
    cur = sr_state_get_current_srv();
    tt_assert(cur);
    set_sr_phase(SR_PHASE_REVEAL);
    MOCK(get_my_v3_authority_cert, get_my_v3_authority_cert_m);
    new_protocol_run(now);
    UNMOCK(get_my_v3_authority_cert);
    /* Rotation happened. */
    tt_assert(sr_state_get_previous_srv() == cur);
    /* We are going into COMMIT phase so we had to rotate our SRVs. Usually
     * our current SRV would be NULL but a new protocol run should make us
     * compute a new SRV. */
    tt_assert(sr_state_get_current_srv());
    /* Also, make sure we did change the current. */
    tt_assert(sr_state_get_current_srv() != cur);
    /* We should have our commitment alone. */
    tt_int_op(digestmap_size(state->commits), ==, 1);
    tt_int_op(state->n_reveal_rounds, ==, 0);
    tt_int_op(state->n_commit_rounds, ==, 0);
    /* 46 here since we were at 45 just before. */
    tt_u64_op(state->n_protocol_runs, ==, 46);
  }

  /* Cleanup of SRVs. */
  {
    sr_state_clean_srvs();
    tt_assert(!sr_state_get_current_srv());
    tt_assert(!sr_state_get_previous_srv());
  }

 done:
  return;
}

static void
test_keep_commit(void *arg)
{
  char fp[FINGERPRINT_LEN + 1];
  sr_commit_t *commit = NULL, *dup_commit = NULL;
  sr_state_t *state;
  time_t now = time(NULL);
  crypto_pk_t *k = NULL;

  (void) arg;

  MOCK(trusteddirserver_get_by_v3_auth_digest,
       trusteddirserver_get_by_v3_auth_digest_m);

  {
    k = pk_generate(1);
    /* Setup a minimal dirauth environment for this test  */
    /* Have a key that is not the one from our commit. */
    init_authority_state();
    state = get_sr_state();
  }

  /* Test this very important function that tells us if we should keep a
   * commit or not in our state. Most of it depends on the phase and what's
   * in the commit so we'll change the commit as we go. */
  commit = sr_generate_our_commit(now, mock_cert);
  tt_assert(commit);
  /* Set us in COMMIT phase for starter. */
  set_sr_phase(SR_PHASE_COMMIT);
  /* We should never keep a commit from a non authoritative authority. */
  tt_int_op(should_keep_commit(commit, fp, SR_PHASE_COMMIT), ==, 0);
  /* This should NOT be kept because it has a reveal value in it. */
  tt_assert(commit_has_reveal_value(commit));
  tt_int_op(should_keep_commit(commit, commit->rsa_identity,
                               SR_PHASE_COMMIT), ==, 0);
  /* Add it to the state which should return to not keep it. */
  sr_state_add_commit(commit);
  tt_int_op(should_keep_commit(commit, commit->rsa_identity,
                               SR_PHASE_COMMIT), ==, 0);
  /* Remove it from state so we can continue our testing. */
  digestmap_remove(state->commits, commit->rsa_identity);
  /* Let's remove our reveal value which should make it OK to keep it. */
  memset(commit->encoded_reveal, 0, sizeof(commit->encoded_reveal));
  tt_int_op(should_keep_commit(commit, commit->rsa_identity,
                               SR_PHASE_COMMIT), ==, 1);

  /* Let's reset our commit and go into REVEAL phase. */
  sr_commit_free(commit);
  commit = sr_generate_our_commit(now, mock_cert);
  tt_assert(commit);
  /* Dup the commit so we have one with and one without a reveal value. */
  dup_commit = tor_malloc_zero(sizeof(*dup_commit));
  memcpy(dup_commit, commit, sizeof(*dup_commit));
  memset(dup_commit->encoded_reveal, 0, sizeof(dup_commit->encoded_reveal));
  set_sr_phase(SR_PHASE_REVEAL);
  /* We should never keep a commit from a non authoritative authority. */
  tt_int_op(should_keep_commit(commit, fp, SR_PHASE_REVEAL), ==, 0);
  /* We shouldn't accept a commit that is not in our state. */
  tt_int_op(should_keep_commit(commit, commit->rsa_identity,
                               SR_PHASE_REVEAL), ==, 0);
  /* Important to add the commit _without_ the reveal here. */
  sr_state_add_commit(dup_commit);
  tt_int_op(digestmap_size(state->commits), ==, 1);
  /* Our commit should be valid that is authoritative, contains a reveal, be
   * in the state and commitment and reveal values match. */
  tt_int_op(should_keep_commit(commit, commit->rsa_identity,
                               SR_PHASE_REVEAL), ==, 1);
  /* The commit shouldn't be kept if it's not verified that is no matchin
   * hashed reveal. */
  {
    /* Let's save the hash reveal so we can restore it. */
    sr_commit_t place_holder;
    memcpy(place_holder.hashed_reveal, commit->hashed_reveal,
           sizeof(place_holder.hashed_reveal));
    memset(commit->hashed_reveal, 0, sizeof(commit->hashed_reveal));
    setup_full_capture_of_logs(LOG_WARN);
    tt_int_op(should_keep_commit(commit, commit->rsa_identity,
                                 SR_PHASE_REVEAL), ==, 0);
    expect_log_msg_containing("doesn't match the commit value.");
    expect_log_msg_containing("has an invalid reveal value.");
    assert_log_predicate(mock_saved_log_n_entries() == 2,
                         "expected 2 log entries");
    teardown_capture_of_logs();
    memcpy(commit->hashed_reveal, place_holder.hashed_reveal,
           sizeof(commit->hashed_reveal));
  }
  /* We shouldn't keep a commit that has no reveal. */
  tt_int_op(should_keep_commit(dup_commit, dup_commit->rsa_identity,
                               SR_PHASE_REVEAL), ==, 0);
  /* We must not keep a commit that is not the same from the commit phase. */
  memset(commit->encoded_commit, 0, sizeof(commit->encoded_commit));
  tt_int_op(should_keep_commit(commit, commit->rsa_identity,
                               SR_PHASE_REVEAL), ==, 0);

 done:
  teardown_capture_of_logs();
  sr_commit_free(commit);
  sr_commit_free(dup_commit);
  crypto_pk_free(k);
  UNMOCK(trusteddirserver_get_by_v3_auth_digest);
}

static void
test_state_update(void *arg)
{
  time_t commit_phase_time = 1452076000;
  time_t reveal_phase_time = 1452086800;
  sr_state_t *state;

  (void) arg;

  {
    init_authority_state();
    state = get_sr_state();
    set_sr_phase(SR_PHASE_COMMIT);
    /* We'll cheat a bit here and reset the creation time of the state which
     * will avoid us to compute a valid_after time that fits the commit
     * phase. */
    state->valid_after = 0;
    state->n_reveal_rounds = 0;
    state->n_commit_rounds = 0;
    state->n_protocol_runs = 0;
  }

  /* We need to mock for the state update function call. */
  MOCK(get_my_v3_authority_cert, get_my_v3_authority_cert_m);

  /* We are in COMMIT phase here and we'll trigger a state update but no
   * transition. */
  sr_state_update(commit_phase_time);
  tt_int_op(state->valid_after, ==, commit_phase_time);
  tt_int_op(state->n_commit_rounds, ==, 1);
  tt_int_op(state->phase, ==, SR_PHASE_COMMIT);
  tt_int_op(digestmap_size(state->commits), ==, 1);

  /* We are still in the COMMIT phase here but we'll trigger a state
   * transition to the REVEAL phase. */
  sr_state_update(reveal_phase_time);
  tt_int_op(state->phase, ==, SR_PHASE_REVEAL);
  tt_int_op(state->valid_after, ==, reveal_phase_time);
  /* Only our commit should be in there. */
  tt_int_op(digestmap_size(state->commits), ==, 1);
  tt_int_op(state->n_reveal_rounds, ==, 1);

  /* We can't update a state with a valid after _lower_ than the creation
   * time so here it is. */
  sr_state_update(commit_phase_time);
  tt_int_op(state->valid_after, ==, reveal_phase_time);

  /* Finally, let's go back in COMMIT phase so we can test the state update
   * of a new protocol run. */
  state->valid_after = 0;
  sr_state_update(commit_phase_time);
  tt_int_op(state->valid_after, ==, commit_phase_time);
  tt_int_op(state->n_commit_rounds, ==, 1);
  tt_int_op(state->n_reveal_rounds, ==, 0);
  tt_u64_op(state->n_protocol_runs, ==, 1);
  tt_int_op(state->phase, ==, SR_PHASE_COMMIT);
  tt_int_op(digestmap_size(state->commits), ==, 1);
  tt_assert(state->current_srv);

 done:
  sr_state_free();
  UNMOCK(get_my_v3_authority_cert);
}

struct testcase_t sr_tests[] = {
  { "get_sr_protocol_phase", test_get_sr_protocol_phase, TT_FORK,
    NULL, NULL },
  { "sr_commit", test_sr_commit, TT_FORK,
    NULL, NULL },
  { "keep_commit", test_keep_commit, TT_FORK,
    NULL, NULL },
  { "encoding", test_encoding, TT_FORK,
    NULL, NULL },
  { "get_next_valid_after_time", test_get_next_valid_after_time, TT_FORK,
    NULL, NULL },
  { "get_state_valid_until_time", test_get_state_valid_until_time, TT_FORK,
    NULL, NULL },
  { "vote", test_vote, TT_FORK,
    NULL, NULL },
  { "state_load_from_disk", test_state_load_from_disk, TT_FORK,
    NULL, NULL },
  { "sr_compute_srv", test_sr_compute_srv, TT_FORK, NULL, NULL },
  { "sr_get_majority_srv_from_votes", test_sr_get_majority_srv_from_votes,
    TT_FORK, NULL, NULL },
  { "utils", test_utils, TT_FORK, NULL, NULL },
  { "state_transition", test_state_transition, TT_FORK, NULL, NULL },
  { "state_update", test_state_update, TT_FORK,
    NULL, NULL },
  END_OF_TESTCASES
};

