/* Copyright (c) 2014, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file test_helpers.c
 * \brief Some helper functions to avoid code duplication in unit tests.
 */

#define ROUTERLIST_PRIVATE
#include "orconfig.h"
#include "or.h"

#include "routerlist.h"
#include "nodelist.h"

#include "test.h"
#include "test_helpers.h"

#include "test_descriptors.inc"

/* Return a statically allocated string representing yesterday's date
 * in ISO format. We use it so that state file items are not found to
 * be outdated. */
const char *
get_yesterday_date_str(void)
{
  static char buf[ISO_TIME_LEN+1];

  time_t yesterday = time(NULL) - 24*60*60;
  format_iso_time(buf, yesterday);
  return buf;
}

/* NOP replacement for router_descriptor_is_older_than() */
static int
router_descriptor_is_older_than_replacement(const routerinfo_t *router,
                                            int seconds)
{
  (void) router;
  (void) seconds;
  return 0;
}

/** Parse a file containing router descriptors and load them to our
    routerlist. This function is used to setup an artificial network
    so that we can conduct tests on it. */
void
helper_setup_fake_routerlist(void)
{
  int retval;
  routerlist_t *our_routerlist = NULL;
  smartlist_t *our_nodelist = NULL;

  /* Read the file that contains our test descriptors. */

  /* We need to mock this function otherwise the descriptors will not
     accepted as they are too old. */
  MOCK(router_descriptor_is_older_than,
       router_descriptor_is_older_than_replacement);

  /* Load all the test descriptors to the routerlist. */
  retval = router_load_routers_from_string(TEST_DESCRIPTORS,
                                           NULL, SAVED_IN_JOURNAL,
                                           NULL, 0, NULL);
  tt_int_op(retval, ==, HELPER_NUMBER_OF_DESCRIPTORS);

  /* Sanity checking of routerlist and nodelist. */
  our_routerlist = router_get_routerlist();
  tt_int_op(smartlist_len(our_routerlist->routers), ==,
              HELPER_NUMBER_OF_DESCRIPTORS);
  routerlist_assert_ok(our_routerlist);

  our_nodelist = nodelist_get_list();
  tt_int_op(smartlist_len(our_nodelist), ==, HELPER_NUMBER_OF_DESCRIPTORS);

  /* Mark all routers as non-guards but up and running! */
  SMARTLIST_FOREACH_BEGIN(our_nodelist, node_t *, node) {
    node->is_running = 1;
    node->is_valid = 1;
    node->is_possible_guard = 0;
  } SMARTLIST_FOREACH_END(node);

 done:
  UNMOCK(router_descriptor_is_older_than);
}

