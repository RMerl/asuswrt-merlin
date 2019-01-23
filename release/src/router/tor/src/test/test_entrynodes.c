/* Copyright (c) 2014-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"

#define STATEFILE_PRIVATE
#define ENTRYNODES_PRIVATE
#define ROUTERLIST_PRIVATE

#include "or.h"
#include "test.h"

#include "config.h"
#include "entrynodes.h"
#include "nodelist.h"
#include "policies.h"
#include "routerlist.h"
#include "routerparse.h"
#include "routerset.h"
#include "statefile.h"
#include "util.h"

#include "test_helpers.h"

/* TODO:
 * choose_random_entry() test with state set.
 *
 * parse_state() tests with more than one guards.
 *
 * More tests for set_from_config(): Multiple nodes, use fingerprints,
 *                                   use country codes.
 */

/** Dummy Tor state used in unittests. */
static or_state_t *dummy_state = NULL;
static or_state_t *
get_or_state_replacement(void)
{
  return dummy_state;
}

/* Unittest cleanup function: Cleanup the fake network. */
static int
fake_network_cleanup(const struct testcase_t *testcase, void *ptr)
{
  (void) testcase;
  (void) ptr;

  routerlist_free_all();
  nodelist_free_all();
  entry_guards_free_all();
  or_state_free(dummy_state);

  return 1; /* NOP */
}

/* Unittest setup function: Setup a fake network. */
static void *
fake_network_setup(const struct testcase_t *testcase)
{
  (void) testcase;

  /* Setup fake state */
  dummy_state = tor_malloc_zero(sizeof(or_state_t));
  MOCK(get_or_state,
       get_or_state_replacement);

  /* Setup fake routerlist. */
  helper_setup_fake_routerlist();

  /* Return anything but NULL (it's interpreted as test fail) */
  return dummy_state;
}

static or_options_t mocked_options;

static const or_options_t *
mock_get_options(void)
{
  return &mocked_options;
}

/** Test choose_random_entry() with none of our routers being guard nodes. */
static void
test_choose_random_entry_no_guards(void *arg)
{
  const node_t *chosen_entry = NULL;

  (void) arg;

  MOCK(get_options, mock_get_options);

  /* Check that we get a guard if it passes preferred
   * address settings */
  memset(&mocked_options, 0, sizeof(mocked_options));
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientPreferIPv6ORPort = 0;

  /* Try to pick an entry even though none of our routers are guards. */
  chosen_entry = choose_random_entry(NULL);

  /* Unintuitively, we actually pick a random node as our entry,
     because router_choose_random_node() relaxes its constraints if it
     can't find a proper entry guard. */
  tt_assert(chosen_entry);

  /* And with the other IP version active */
  mocked_options.ClientUseIPv6 = 1;
  chosen_entry = choose_random_entry(NULL);
  tt_assert(chosen_entry);

  /* And with the preference on auto */
  mocked_options.ClientPreferIPv6ORPort = -1;
  chosen_entry = choose_random_entry(NULL);
  tt_assert(chosen_entry);

  /* Check that we don't get a guard if it doesn't pass mandatory address
   * settings */
  memset(&mocked_options, 0, sizeof(mocked_options));
  mocked_options.ClientUseIPv4 = 0;
  mocked_options.ClientPreferIPv6ORPort = 0;

  chosen_entry = choose_random_entry(NULL);

  /* If we don't allow IPv4 at all, we don't get a guard*/
  tt_assert(!chosen_entry);

  /* Check that we get a guard if it passes allowed but not preferred address
   * settings */
  memset(&mocked_options, 0, sizeof(mocked_options));
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientUseIPv6 = 1;
  mocked_options.ClientPreferIPv6ORPort = 1;

  chosen_entry = choose_random_entry(NULL);
  tt_assert(chosen_entry);

  /* Check that we get a guard if it passes preferred address settings when
   * they're auto */
  memset(&mocked_options, 0, sizeof(mocked_options));
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientPreferIPv6ORPort = -1;

  chosen_entry = choose_random_entry(NULL);
  tt_assert(chosen_entry);

  /* And with IPv6 active */
  mocked_options.ClientUseIPv6 = 1;

  chosen_entry = choose_random_entry(NULL);
  tt_assert(chosen_entry);

 done:
  memset(&mocked_options, 0, sizeof(mocked_options));
  UNMOCK(get_options);
}

/** Test choose_random_entry() with only one of our routers being a
    guard node. */
static void
test_choose_random_entry_one_possible_guard(void *arg)
{
  const node_t *chosen_entry = NULL;
  node_t *the_guard = NULL;
  smartlist_t *our_nodelist = NULL;

  (void) arg;

  MOCK(get_options, mock_get_options);

  /* Set one of the nodes to be a guard. */
  our_nodelist = nodelist_get_list();
  the_guard = smartlist_get(our_nodelist, 4); /* chosen by fair dice roll */
  the_guard->is_possible_guard = 1;

  /* Check that we get the guard if it passes preferred
   * address settings */
  memset(&mocked_options, 0, sizeof(mocked_options));
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientPreferIPv6ORPort = 0;

  /* Pick an entry. Make sure we pick the node we marked as guard. */
  chosen_entry = choose_random_entry(NULL);
  tt_ptr_op(chosen_entry, OP_EQ, the_guard);

  /* And with the other IP version active */
  mocked_options.ClientUseIPv6 = 1;
  chosen_entry = choose_random_entry(NULL);
  tt_ptr_op(chosen_entry, OP_EQ, the_guard);

  /* And with the preference on auto */
  mocked_options.ClientPreferIPv6ORPort = -1;
  chosen_entry = choose_random_entry(NULL);
  tt_ptr_op(chosen_entry, OP_EQ, the_guard);

  /* Check that we don't get a guard if it doesn't pass mandatory address
   * settings */
  memset(&mocked_options, 0, sizeof(mocked_options));
  mocked_options.ClientUseIPv4 = 0;
  mocked_options.ClientPreferIPv6ORPort = 0;

  chosen_entry = choose_random_entry(NULL);

  /* If we don't allow IPv4 at all, we don't get a guard*/
  tt_assert(!chosen_entry);

  /* Check that we get a node if it passes allowed but not preferred
   * address settings */
  memset(&mocked_options, 0, sizeof(mocked_options));
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientUseIPv6 = 1;
  mocked_options.ClientPreferIPv6ORPort = 1;

  chosen_entry = choose_random_entry(NULL);

  /* We disable the guard check and the preferred address check at the same
   * time, so we can't be sure we get the guard */
  tt_assert(chosen_entry);

  /* Check that we get a node if it is allowed but not preferred when settings
   * are auto */
  memset(&mocked_options, 0, sizeof(mocked_options));
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientPreferIPv6ORPort = -1;

  chosen_entry = choose_random_entry(NULL);

  /* We disable the guard check and the preferred address check at the same
   * time, so we can't be sure we get the guard */
  tt_assert(chosen_entry);

  /* and with IPv6 active */
  mocked_options.ClientUseIPv6 = 1;

  chosen_entry = choose_random_entry(NULL);
  tt_assert(chosen_entry);

 done:
  memset(&mocked_options, 0, sizeof(mocked_options));
  UNMOCK(get_options);
}

/** Helper to conduct tests for populate_live_entry_guards().

   This test adds some entry guards to our list, and then tests
   populate_live_entry_guards() to mke sure it filters them correctly.

   <b>num_needed</b> is the number of guard nodes we support. It's
   configurable to make sure we function properly with 1 or 3 guard
   nodes configured.
*/
static void
populate_live_entry_guards_test_helper(int num_needed)
{
  smartlist_t *our_nodelist = NULL;
  smartlist_t *live_entry_guards = smartlist_new();
  const smartlist_t *all_entry_guards = get_entry_guards();
  or_options_t *options = get_options_mutable();
  int retval;

  /* Set NumEntryGuards to the provided number. */
  options->NumEntryGuards = num_needed;
  tt_int_op(num_needed, OP_EQ, decide_num_guards(options, 0));

  /* The global entry guards smartlist should be empty now. */
  tt_int_op(smartlist_len(all_entry_guards), OP_EQ, 0);

  /* Walk the nodelist and add all nodes as entry guards. */
  our_nodelist = nodelist_get_list();
  tt_int_op(smartlist_len(our_nodelist), OP_EQ, HELPER_NUMBER_OF_DESCRIPTORS);

  SMARTLIST_FOREACH_BEGIN(our_nodelist, const node_t *, node) {
    const node_t *node_tmp;
    node_tmp = add_an_entry_guard(node, 0, 1, 0, 0);
    tt_assert(node_tmp);
  } SMARTLIST_FOREACH_END(node);

  /* Make sure the nodes were added as entry guards. */
  tt_int_op(smartlist_len(all_entry_guards), OP_EQ,
            HELPER_NUMBER_OF_DESCRIPTORS);

  /* Ensure that all the possible entry guards are enough to satisfy us. */
  tt_int_op(smartlist_len(all_entry_guards), OP_GE, num_needed);

  /* Walk the entry guard list for some sanity checking */
  SMARTLIST_FOREACH_BEGIN(all_entry_guards, const entry_guard_t *, entry) {
    /* Since we called add_an_entry_guard() with 'for_discovery' being
       False, all guards should have made_contact enabled. */
    tt_int_op(entry->made_contact, OP_EQ, 1);

  } SMARTLIST_FOREACH_END(entry);

  /* First, try to get some fast guards. This should fail. */
  retval = populate_live_entry_guards(live_entry_guards,
                                      all_entry_guards,
                                      NULL,
                                      NO_DIRINFO, /* Don't care about DIRINFO*/
                                      0, 0,
                                      1); /* We want fast guard! */
  tt_int_op(retval, OP_EQ, 0);
  tt_int_op(smartlist_len(live_entry_guards), OP_EQ, 0);

  /* Now try to get some stable guards. This should fail too. */
  retval = populate_live_entry_guards(live_entry_guards,
                                      all_entry_guards,
                                      NULL,
                                      NO_DIRINFO,
                                      0,
                                      1, /* We want stable guard! */
                                      0);
  tt_int_op(retval, OP_EQ, 0);
  tt_int_op(smartlist_len(live_entry_guards), OP_EQ, 0);

  /* Now try to get any guard we can find. This should succeed. */
  retval = populate_live_entry_guards(live_entry_guards,
                                      all_entry_guards,
                                      NULL,
                                      NO_DIRINFO,
                                      0, 0, 0); /* No restrictions! */

  /* Since we had more than enough guards in 'all_entry_guards', we
     should have added 'num_needed' of them to live_entry_guards.
     'retval' should be 1 since we now have enough live entry guards
     to pick one.  */
  tt_int_op(retval, OP_EQ, 1);
  tt_int_op(smartlist_len(live_entry_guards), OP_EQ, num_needed);

 done:
  smartlist_free(live_entry_guards);
}

/* Test populate_live_entry_guards() for 1 guard node. */
static void
test_populate_live_entry_guards_1guard(void *arg)
{
  (void) arg;

  populate_live_entry_guards_test_helper(1);
}

/* Test populate_live_entry_guards() for 3 guard nodes. */
static void
test_populate_live_entry_guards_3guards(void *arg)
{
  (void) arg;

  populate_live_entry_guards_test_helper(3);
}

/** Append some EntryGuard lines to the Tor state at <b>state</b>.

   <b>entry_guard_lines</b> is a smartlist containing 2-tuple
   smartlists that carry the key and values of the statefile.
   As an example:
   entry_guard_lines =
     (("EntryGuard", "name 67E72FF33D7D41BF11C569646A0A7B4B188340DF DirCache"),
      ("EntryGuardDownSince", "2014-06-07 16:02:46 2014-06-07 16:02:46"))
*/
static void
state_insert_entry_guard_helper(or_state_t *state,
                                smartlist_t *entry_guard_lines)
{
  config_line_t **next, *line;

  next = &state->EntryGuards;
  *next = NULL;

  /* Loop over all the state lines in the smartlist */
  SMARTLIST_FOREACH_BEGIN(entry_guard_lines, const smartlist_t *,state_lines) {
    /* Get key and value for each line */
    const char *state_key = smartlist_get(state_lines, 0);
    const char *state_value = smartlist_get(state_lines, 1);

    *next = line = tor_malloc_zero(sizeof(config_line_t));
    line->key = tor_strdup(state_key);
    tor_asprintf(&line->value, "%s", state_value);
    next = &(line->next);
  } SMARTLIST_FOREACH_END(state_lines);
}

/** Free memory occupied by <b>entry_guard_lines</b>. */
static void
state_lines_free(smartlist_t *entry_guard_lines)
{
  SMARTLIST_FOREACH_BEGIN(entry_guard_lines, smartlist_t *, state_lines) {
    char *state_key = smartlist_get(state_lines, 0);
    char *state_value = smartlist_get(state_lines, 1);

    tor_free(state_key);
    tor_free(state_value);
    smartlist_free(state_lines);
  } SMARTLIST_FOREACH_END(state_lines);

  smartlist_free(entry_guard_lines);
}

/* Tests entry_guards_parse_state(). It creates a fake Tor state with
   a saved entry guard and makes sure that Tor can parse it and
   creates the right entry node out of it.
*/
static void
test_entry_guards_parse_state_simple(void *arg)
{
  or_state_t *state = or_state_new();
  const smartlist_t *all_entry_guards = get_entry_guards();
  smartlist_t *entry_state_lines = smartlist_new();
  char *msg = NULL;
  int retval;

  /* Details of our fake guard node */
  const char *nickname = "hagbard";
  const char *fpr = "B29D536DD1752D542E1FBB3C9CE4449D51298212";
  const char *tor_version = "0.2.5.3-alpha-dev";
  const char *added_at = get_yesterday_date_str();
  const char *unlisted_since = "2014-06-08 16:16:50";

  (void) arg;

  /* The global entry guards smartlist should be empty now. */
  tt_int_op(smartlist_len(all_entry_guards), OP_EQ, 0);

  { /* Prepare the state entry */

    /* Prepare the smartlist to hold the key/value of each line */
    smartlist_t *state_line = smartlist_new();
    smartlist_add_asprintf(state_line, "EntryGuard");
    smartlist_add_asprintf(state_line, "%s %s %s", nickname, fpr, "DirCache");
    smartlist_add(entry_state_lines, state_line);

    state_line = smartlist_new();
    smartlist_add_asprintf(state_line, "EntryGuardAddedBy");
    smartlist_add_asprintf(state_line, "%s %s %s", fpr, tor_version, added_at);
    smartlist_add(entry_state_lines, state_line);

    state_line = smartlist_new();
    smartlist_add_asprintf(state_line, "EntryGuardUnlistedSince");
    smartlist_add_asprintf(state_line, "%s", unlisted_since);
    smartlist_add(entry_state_lines, state_line);
  }

  /* Inject our lines in the state */
  state_insert_entry_guard_helper(state, entry_state_lines);

  /* Parse state */
  retval = entry_guards_parse_state(state, 1, &msg);
  tt_int_op(retval, OP_GE, 0);

  /* Test that the guard was registered.
     We need to re-get the entry guard list since its pointer was
     overwritten in entry_guards_parse_state(). */
  all_entry_guards = get_entry_guards();
  tt_int_op(smartlist_len(all_entry_guards), OP_EQ, 1);

  { /* Test the entry guard structure */
    char hex_digest[1024];
    char str_time[1024];

    const entry_guard_t *e = smartlist_get(all_entry_guards, 0);
    tt_str_op(e->nickname, OP_EQ, nickname); /* Verify nickname */

    base16_encode(hex_digest, sizeof(hex_digest),
                  e->identity, DIGEST_LEN);
    tt_str_op(hex_digest, OP_EQ, fpr); /* Verify fingerprint */

    tt_assert(e->is_dir_cache); /* Verify dirness */

    tt_str_op(e->chosen_by_version, OP_EQ, tor_version); /* Verify version */

    tt_assert(e->made_contact); /* All saved guards have been contacted */

    tt_assert(e->bad_since); /* Verify bad_since timestamp */
    format_iso_time(str_time, e->bad_since);
    tt_str_op(str_time, OP_EQ, unlisted_since);

    /* The rest should be unset */
    tt_assert(!e->unreachable_since);
    tt_assert(!e->can_retry);
    tt_assert(!e->path_bias_noticed);
    tt_assert(!e->path_bias_warned);
    tt_assert(!e->path_bias_extreme);
    tt_assert(!e->path_bias_disabled);
    tt_assert(!e->path_bias_use_noticed);
    tt_assert(!e->path_bias_use_extreme);
    tt_assert(!e->last_attempted);
  }

 done:
  state_lines_free(entry_state_lines);
  or_state_free(state);
  tor_free(msg);
}

/** Similar to test_entry_guards_parse_state_simple() but aims to test
    the PathBias-related details of the entry guard. */
static void
test_entry_guards_parse_state_pathbias(void *arg)
{
  or_state_t *state = or_state_new();
  const smartlist_t *all_entry_guards = get_entry_guards();
  char *msg = NULL;
  int retval;
  smartlist_t *entry_state_lines = smartlist_new();

  /* Path bias details of the fake guard */
  const double circ_attempts = 9;
  const double circ_successes = 8;
  const double successful_closed = 4;
  const double collapsed = 2;
  const double unusable = 0;
  const double timeouts = 1;

  (void) arg;

  /* The global entry guards smartlist should be empty now. */
  tt_int_op(smartlist_len(all_entry_guards), OP_EQ, 0);

  { /* Prepare the state entry */

    /* Prepare the smartlist to hold the key/value of each line */
    smartlist_t *state_line = smartlist_new();
    smartlist_add_asprintf(state_line, "EntryGuard");
    smartlist_add_asprintf(state_line,
             "givethanks B29D536DD1752D542E1FBB3C9CE4449D51298212 NoDirCache");
    smartlist_add(entry_state_lines, state_line);

    state_line = smartlist_new();
    smartlist_add_asprintf(state_line, "EntryGuardAddedBy");
    smartlist_add_asprintf(state_line,
      "B29D536DD1752D542E1FBB3C9CE4449D51298212 0.2.5.3-alpha-dev "
                           "%s", get_yesterday_date_str());
    smartlist_add(entry_state_lines, state_line);

    state_line = smartlist_new();
    smartlist_add_asprintf(state_line, "EntryGuardUnlistedSince");
    smartlist_add_asprintf(state_line, "2014-06-08 16:16:50");
    smartlist_add(entry_state_lines, state_line);

    state_line = smartlist_new();
    smartlist_add_asprintf(state_line, "EntryGuardPathBias");
    smartlist_add_asprintf(state_line, "%f %f %f %f %f %f",
                           circ_attempts, circ_successes, successful_closed,
                           collapsed, unusable, timeouts);
    smartlist_add(entry_state_lines, state_line);
  }

  /* Inject our lines in the state */
  state_insert_entry_guard_helper(state, entry_state_lines);

  /* Parse state */
  retval = entry_guards_parse_state(state, 1, &msg);
  tt_int_op(retval, OP_GE, 0);

  /* Test that the guard was registered */
  all_entry_guards = get_entry_guards();
  tt_int_op(smartlist_len(all_entry_guards), OP_EQ, 1);

  { /* Test the path bias of this guard */
    const entry_guard_t *e = smartlist_get(all_entry_guards, 0);

    tt_assert(!e->is_dir_cache);
    tt_assert(!e->can_retry);

    /* XXX tt_double_op doesn't support equality. Cast to int for now. */
    tt_int_op((int)e->circ_attempts, OP_EQ, (int)circ_attempts);
    tt_int_op((int)e->circ_successes, OP_EQ, (int)circ_successes);
    tt_int_op((int)e->successful_circuits_closed, OP_EQ,
              (int)successful_closed);
    tt_int_op((int)e->timeouts, OP_EQ, (int)timeouts);
    tt_int_op((int)e->collapsed_circuits, OP_EQ, (int)collapsed);
    tt_int_op((int)e->unusable_circuits, OP_EQ, (int)unusable);
  }

 done:
  or_state_free(state);
  state_lines_free(entry_state_lines);
  tor_free(msg);
}

/* Simple test of entry_guards_set_from_config() by specifying a
   particular EntryNode and making sure it gets picked. */
static void
test_entry_guards_set_from_config(void *arg)
{
  or_options_t *options = get_options_mutable();
  const smartlist_t *all_entry_guards = get_entry_guards();
  const char *entrynodes_str = "test003r";
  const node_t *chosen_entry = NULL;
  int retval;

  (void) arg;

  /* Prase EntryNodes as a routerset. */
  options->EntryNodes = routerset_new();
  retval = routerset_parse(options->EntryNodes,
                           entrynodes_str,
                           "test_entrynodes");
  tt_int_op(retval, OP_GE, 0);

  /* Read nodes from EntryNodes */
  entry_guards_set_from_config(options);

  /* Test that only one guard was added. */
  tt_int_op(smartlist_len(all_entry_guards), OP_EQ, 1);

  /* Make sure it was the guard we specified. */
  chosen_entry = choose_random_entry(NULL);
  tt_str_op(chosen_entry->ri->nickname, OP_EQ, entrynodes_str);

 done:
  routerset_free(options->EntryNodes);
}

static void
test_entry_is_time_to_retry(void *arg)
{
  entry_guard_t *test_guard;
  time_t now;
  int retval;
  (void)arg;

  now = time(NULL);

  test_guard = tor_malloc_zero(sizeof(entry_guard_t));

  test_guard->last_attempted = now - 10;
  test_guard->unreachable_since = now - 1;

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,1);

  test_guard->unreachable_since = now - (6*60*60 - 1);
  test_guard->last_attempted = now - (60*60 + 1);

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,1);

  test_guard->last_attempted = now - (60*60 - 1);

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,0);

  test_guard->unreachable_since = now - (6*60*60 + 1);
  test_guard->last_attempted = now - (4*60*60 + 1);

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,1);

  test_guard->unreachable_since = now - (3*24*60*60 - 1);
  test_guard->last_attempted = now - (4*60*60 + 1);

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,1);

  test_guard->unreachable_since = now - (3*24*60*60 + 1);
  test_guard->last_attempted = now - (18*60*60 + 1);

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,1);

  test_guard->unreachable_since = now - (7*24*60*60 - 1);
  test_guard->last_attempted = now - (18*60*60 + 1);

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,1);

  test_guard->last_attempted = now - (18*60*60 - 1);

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,0);

  test_guard->unreachable_since = now - (7*24*60*60 + 1);
  test_guard->last_attempted = now - (36*60*60 + 1);

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,1);

  test_guard->unreachable_since = now - (7*24*60*60 + 1);
  test_guard->last_attempted = now - (36*60*60 + 1);

  retval = entry_is_time_to_retry(test_guard,now);
  tt_int_op(retval,OP_EQ,1);

 done:
  tor_free(test_guard);
}

/** XXX Do some tests that entry_is_live() */
static void
test_entry_is_live(void *arg)
{
  smartlist_t *our_nodelist = NULL;
  const smartlist_t *all_entry_guards = get_entry_guards();
  const node_t *test_node = NULL;
  const entry_guard_t *test_entry = NULL;
  const char *msg;
  int which_node;

  (void) arg;

  /* The global entry guards smartlist should be empty now. */
  tt_int_op(smartlist_len(all_entry_guards), OP_EQ, 0);

  /* Walk the nodelist and add all nodes as entry guards. */
  our_nodelist = nodelist_get_list();
  tt_int_op(smartlist_len(our_nodelist), OP_EQ, HELPER_NUMBER_OF_DESCRIPTORS);

  SMARTLIST_FOREACH_BEGIN(our_nodelist, const node_t *, node) {
    const node_t *node_tmp;
    node_tmp = add_an_entry_guard(node, 0, 1, 0, 0);
    tt_assert(node_tmp);

    tt_int_op(node->is_stable, OP_EQ, 0);
    tt_int_op(node->is_fast, OP_EQ, 0);
  } SMARTLIST_FOREACH_END(node);

  /* Make sure the nodes were added as entry guards. */
  tt_int_op(smartlist_len(all_entry_guards), OP_EQ,
            HELPER_NUMBER_OF_DESCRIPTORS);

  /* Now get a random test entry that we will use for this unit test. */
  which_node = 3;  /* (chosen by fair dice roll) */
  test_entry = smartlist_get(all_entry_guards, which_node);

  /* Let's do some entry_is_live() tests! */

  /* Require the node to be stable, but it's not. Should fail.
     Also enable 'assume_reachable' because why not. */
  test_node = entry_is_live(test_entry,
                            ENTRY_NEED_UPTIME | ENTRY_ASSUME_REACHABLE,
                            &msg);
  tt_assert(!test_node);

  /* Require the node to be fast, but it's not. Should fail. */
  test_node = entry_is_live(test_entry,
                            ENTRY_NEED_CAPACITY | ENTRY_ASSUME_REACHABLE,
                            &msg);
  tt_assert(!test_node);

  /* Don't impose any restrictions on the node. Should succeed. */
  test_node = entry_is_live(test_entry, 0, &msg);
  tt_assert(test_node);
  tt_ptr_op(test_node, OP_EQ, node_get_by_id(test_entry->identity));

  /* Require descriptor for this node. It has one so it should succeed. */
  test_node = entry_is_live(test_entry, ENTRY_NEED_DESCRIPTOR, &msg);
  tt_assert(test_node);
  tt_ptr_op(test_node, OP_EQ, node_get_by_id(test_entry->identity));

 done:
  ; /* XXX */
}

#define TEST_IPV4_ADDR "123.45.67.89"
#define TEST_IPV6_ADDR "[1234:5678:90ab:cdef::]"

static void
test_node_preferred_orport(void *arg)
{
  (void)arg;
  tor_addr_t ipv4_addr;
  const uint16_t ipv4_port = 4444;
  tor_addr_t ipv6_addr;
  const uint16_t ipv6_port = 6666;
  routerinfo_t node_ri;
  node_t node;
  tor_addr_port_t ap;

  /* Setup options */
  memset(&mocked_options, 0, sizeof(mocked_options));
  /* We don't test ClientPreferIPv6ORPort here, because it's used in
   * nodelist_set_consensus to setup node.ipv6_preferred, which we set
   * directly. */
  MOCK(get_options, mock_get_options);

  /* Setup IP addresses */
  tor_addr_parse(&ipv4_addr, TEST_IPV4_ADDR);
  tor_addr_parse(&ipv6_addr, TEST_IPV6_ADDR);

  /* Setup node_ri */
  memset(&node_ri, 0, sizeof(node_ri));
  node_ri.addr = tor_addr_to_ipv4h(&ipv4_addr);
  node_ri.or_port = ipv4_port;
  tor_addr_copy(&node_ri.ipv6_addr, &ipv6_addr);
  node_ri.ipv6_orport = ipv6_port;

  /* Setup node */
  memset(&node, 0, sizeof(node));
  node.ri = &node_ri;

  /* Check the preferred address is IPv4 if we're only using IPv4, regardless
   * of whether we prefer it or not */
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientUseIPv6 = 0;
  node.ipv6_preferred = 0;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv4_addr));
  tt_assert(ap.port == ipv4_port);

  node.ipv6_preferred = 1;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv4_addr));
  tt_assert(ap.port == ipv4_port);

  /* Check the preferred address is IPv4 if we're using IPv4 and IPv6, but
   * don't prefer the IPv6 address */
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientUseIPv6 = 1;
  node.ipv6_preferred = 0;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv4_addr));
  tt_assert(ap.port == ipv4_port);

  /* Check the preferred address is IPv6 if we prefer it and
   * ClientUseIPv6 is 1, regardless of ClientUseIPv4 */
  mocked_options.ClientUseIPv4 = 1;
  mocked_options.ClientUseIPv6 = 1;
  node.ipv6_preferred = 1;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv6_addr));
  tt_assert(ap.port == ipv6_port);

  mocked_options.ClientUseIPv4 = 0;
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv6_addr));
  tt_assert(ap.port == ipv6_port);

  /* Check the preferred address is IPv6 if we don't prefer it, but
   * ClientUseIPv4 is 0 */
  mocked_options.ClientUseIPv4 = 0;
  mocked_options.ClientUseIPv6 = 1;
  node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(&mocked_options);
  node_get_pref_orport(&node, &ap);
  tt_assert(tor_addr_eq(&ap.addr, &ipv6_addr));
  tt_assert(ap.port == ipv6_port);

 done:
  UNMOCK(get_options);
}

static const struct testcase_setup_t fake_network = {
  fake_network_setup, fake_network_cleanup
};

struct testcase_t entrynodes_tests[] = {
  { "entry_is_time_to_retry", test_entry_is_time_to_retry,
    TT_FORK, NULL, NULL },
  { "choose_random_entry_no_guards", test_choose_random_entry_no_guards,
    TT_FORK, &fake_network, NULL },
  { "choose_random_entry_one_possibleguard",
    test_choose_random_entry_one_possible_guard,
    TT_FORK, &fake_network, NULL },
  { "populate_live_entry_guards_1guard",
    test_populate_live_entry_guards_1guard,
    TT_FORK, &fake_network, NULL },
  { "populate_live_entry_guards_3guards",
    test_populate_live_entry_guards_3guards,
    TT_FORK, &fake_network, NULL },
  { "entry_guards_parse_state_simple",
    test_entry_guards_parse_state_simple,
    TT_FORK, &fake_network, NULL },
  { "entry_guards_parse_state_pathbias",
    test_entry_guards_parse_state_pathbias,
    TT_FORK, &fake_network, NULL },
  { "entry_guards_set_from_config",
    test_entry_guards_set_from_config,
    TT_FORK, &fake_network, NULL },
  { "entry_is_live",
    test_entry_is_live,
    TT_FORK, &fake_network, NULL },
  { "node_preferred_orport",
    test_node_preferred_orport,
    0, NULL, NULL },
  END_OF_TESTCASES
};

