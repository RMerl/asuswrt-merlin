/* $QuaggaId: Format:%an, %ai, %h$ $
 *
 * BGP Multipath Unit Test
 * Copyright (C) 2010 Google Inc.
 *
 * This file is part of Quagga
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <zebra.h>

#include "vty.h"
#include "stream.h"
#include "privs.h"
#include "linklist.h"
#include "memory.h"
#include "zclient.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_table.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_attr.h"
#include "bgpd/bgp_mpath.h"

#define VT100_RESET "\x1b[0m"
#define VT100_RED "\x1b[31m"
#define VT100_GREEN "\x1b[32m"
#define VT100_YELLOW "\x1b[33m"
#define OK VT100_GREEN "OK" VT100_RESET
#define FAILED VT100_RED "failed" VT100_RESET

#define TEST_PASSED 0
#define TEST_FAILED -1

#define EXPECT_TRUE(expr, res)                                          \
  if (!(expr))                                                          \
    {                                                                   \
      printf ("Test failure in %s line %u: %s\n",                       \
              __FUNCTION__, __LINE__, #expr);                           \
      (res) = TEST_FAILED;                                              \
    }

typedef struct testcase_t__ testcase_t;

typedef int (*test_setup_func)(testcase_t *);
typedef int (*test_run_func)(testcase_t *);
typedef int (*test_cleanup_func)(testcase_t *);

struct testcase_t__ {
  const char *desc;
  void *test_data;
  void *verify_data;
  void *tmp_data;
  test_setup_func setup;
  test_run_func run;
  test_cleanup_func cleanup;
};

/* need these to link in libbgp */
struct thread_master *master = NULL;
struct zclient *zclient;
struct zebra_privs_t bgpd_privs =
{
  .user = NULL,
  .group = NULL,
  .vty_group = NULL,
};

static int tty = 0;

/* Create fake bgp instance */
static struct bgp *
bgp_create_fake (as_t *as, const char *name)
{
  struct bgp *bgp;
  afi_t afi;
  safi_t safi;

  if ( (bgp = XCALLOC (MTYPE_BGP, sizeof (struct bgp))) == NULL)
    return NULL;

  bgp_lock (bgp);
  //bgp->peer_self = peer_new (bgp);
  //bgp->peer_self->host = XSTRDUP (MTYPE_BGP_PEER_HOST, "Static announcement");

  bgp->peer = list_new ();
  //bgp->peer->cmp = (int (*)(void *, void *)) peer_cmp;

  bgp->group = list_new ();
  //bgp->group->cmp = (int (*)(void *, void *)) peer_group_cmp;

  bgp->rsclient = list_new ();
  //bgp->rsclient->cmp = (int (*)(void*, void*)) peer_cmp;

  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
        bgp->route[afi][safi] = bgp_table_init (afi, safi);
        bgp->aggregate[afi][safi] = bgp_table_init (afi, safi);
        bgp->rib[afi][safi] = bgp_table_init (afi, safi);
        bgp->maxpaths[afi][safi].maxpaths_ebgp = BGP_DEFAULT_MAXPATHS;
        bgp->maxpaths[afi][safi].maxpaths_ibgp = BGP_DEFAULT_MAXPATHS;
      }

  bgp->default_local_pref = BGP_DEFAULT_LOCAL_PREF;
  bgp->default_holdtime = BGP_DEFAULT_HOLDTIME;
  bgp->default_keepalive = BGP_DEFAULT_KEEPALIVE;
  bgp->restart_time = BGP_DEFAULT_RESTART_TIME;
  bgp->stalepath_time = BGP_DEFAULT_STALEPATH_TIME;

  bgp->as = *as;

  if (name)
    bgp->name = strdup (name);

  return bgp;
}

/*=========================================================
 * Testcase for maximum-paths configuration
 */
static int
setup_bgp_cfg_maximum_paths (testcase_t *t)
{
  as_t asn = 1;
  t->tmp_data = bgp_create_fake (&asn, NULL);
  if (!t->tmp_data)
    return -1;
  return 0;
}

static int
run_bgp_cfg_maximum_paths (testcase_t *t)
{
  afi_t afi;
  safi_t safi;
  struct bgp *bgp;
  int api_result;
  int test_result = TEST_PASSED;

  bgp = t->tmp_data;
  for (afi = AFI_IP; afi < AFI_MAX; afi++)
    for (safi = SAFI_UNICAST; safi < SAFI_MAX; safi++)
      {
        /* test bgp_maximum_paths_set */
        api_result = bgp_maximum_paths_set (bgp, afi, safi, BGP_PEER_EBGP, 10);
        EXPECT_TRUE (api_result == 0, test_result);
        api_result = bgp_maximum_paths_set (bgp, afi, safi, BGP_PEER_IBGP, 10);
        EXPECT_TRUE (api_result == 0, test_result);
        EXPECT_TRUE (bgp->maxpaths[afi][safi].maxpaths_ebgp == 10, test_result);
        EXPECT_TRUE (bgp->maxpaths[afi][safi].maxpaths_ibgp == 10, test_result);

        /* test bgp_maximum_paths_unset */
        api_result = bgp_maximum_paths_unset (bgp, afi, safi, BGP_PEER_EBGP);
        EXPECT_TRUE (api_result == 0, test_result);
        api_result = bgp_maximum_paths_unset (bgp, afi, safi, BGP_PEER_IBGP);
        EXPECT_TRUE (api_result == 0, test_result);
        EXPECT_TRUE ((bgp->maxpaths[afi][safi].maxpaths_ebgp ==
                      BGP_DEFAULT_MAXPATHS), test_result);
        EXPECT_TRUE ((bgp->maxpaths[afi][safi].maxpaths_ibgp ==
                      BGP_DEFAULT_MAXPATHS), test_result);
      }

  return test_result;
}

static int
cleanup_bgp_cfg_maximum_paths (testcase_t *t)
{
  return bgp_delete ((struct bgp *)t->tmp_data);
}

testcase_t test_bgp_cfg_maximum_paths = {
  .desc = "Test bgp maximum-paths config",
  .setup = setup_bgp_cfg_maximum_paths,
  .run = run_bgp_cfg_maximum_paths,
  .cleanup = cleanup_bgp_cfg_maximum_paths,
};

/*=========================================================
 * Testcase for bgp_mp_list
 */
struct peer test_mp_list_peer[] = {
  { .local_as = 1, .as = 2 },
  { .local_as = 1, .as = 2 },
  { .local_as = 1, .as = 2 },
  { .local_as = 1, .as = 2 },
  { .local_as = 1, .as = 2 },
};
int test_mp_list_peer_count = sizeof (test_mp_list_peer)/ sizeof (struct peer);
struct attr test_mp_list_attr[4];
struct bgp_info test_mp_list_info[] = {
  { .peer = &test_mp_list_peer[0], .attr = &test_mp_list_attr[0] },
  { .peer = &test_mp_list_peer[1], .attr = &test_mp_list_attr[1] },
  { .peer = &test_mp_list_peer[2], .attr = &test_mp_list_attr[1] },
  { .peer = &test_mp_list_peer[3], .attr = &test_mp_list_attr[2] },
  { .peer = &test_mp_list_peer[4], .attr = &test_mp_list_attr[3] },
};
int test_mp_list_info_count =
  sizeof (test_mp_list_info)/sizeof (struct bgp_info);

static int
setup_bgp_mp_list (testcase_t *t)
{
  test_mp_list_attr[0].nexthop.s_addr = 0x01010101;
  test_mp_list_attr[1].nexthop.s_addr = 0x02020202;
  test_mp_list_attr[2].nexthop.s_addr = 0x03030303;
  test_mp_list_attr[3].nexthop.s_addr = 0x04040404;

  if ((test_mp_list_peer[0].su_remote = sockunion_str2su ("1.1.1.1")) == NULL)
    return -1;
  if ((test_mp_list_peer[1].su_remote = sockunion_str2su ("2.2.2.2")) == NULL)
    return -1;
  if ((test_mp_list_peer[2].su_remote = sockunion_str2su ("3.3.3.3")) == NULL)
    return -1;
  if ((test_mp_list_peer[3].su_remote = sockunion_str2su ("4.4.4.4")) == NULL)
    return -1;
  if ((test_mp_list_peer[4].su_remote = sockunion_str2su ("5.5.5.5")) == NULL)
    return -1;

  return 0;
}

static int
run_bgp_mp_list (testcase_t *t)
{
  struct list mp_list;
  struct listnode *mp_node;
  struct bgp_info *info;
  int i;
  int test_result = TEST_PASSED;
  bgp_mp_list_init (&mp_list);
  EXPECT_TRUE (listcount(&mp_list) == 0, test_result);

  bgp_mp_list_add (&mp_list, &test_mp_list_info[1]);
  bgp_mp_list_add (&mp_list, &test_mp_list_info[4]);
  bgp_mp_list_add (&mp_list, &test_mp_list_info[2]);
  bgp_mp_list_add (&mp_list, &test_mp_list_info[3]);
  bgp_mp_list_add (&mp_list, &test_mp_list_info[0]);

  for (i = 0, mp_node = listhead(&mp_list); i < test_mp_list_info_count;
       i++, mp_node = listnextnode(mp_node))
    {
      info = listgetdata(mp_node);
      EXPECT_TRUE (info == &test_mp_list_info[i], test_result);
    }

  bgp_mp_list_clear (&mp_list);
  EXPECT_TRUE (listcount(&mp_list) == 0, test_result);

  return test_result;
}

static int
cleanup_bgp_mp_list (testcase_t *t)
{
  int i;

  for (i = 0; i < test_mp_list_peer_count; i++)
    sockunion_free (test_mp_list_peer[i].su_remote);

  return 0;
}

testcase_t test_bgp_mp_list = {
  .desc = "Test bgp_mp_list",
  .setup = setup_bgp_mp_list,
  .run = run_bgp_mp_list,
  .cleanup = cleanup_bgp_mp_list,
};

/*=========================================================
 * Testcase for bgp_info_mpath_update
 */

struct bgp_node test_rn;

static int
setup_bgp_info_mpath_update (testcase_t *t)
{
  int i;
  str2prefix ("42.1.1.0/24", &test_rn.p);
  setup_bgp_mp_list (t);
  for (i = 0; i < test_mp_list_info_count; i++)
    bgp_info_add (&test_rn, &test_mp_list_info[i]);
  return 0;
}

static int
run_bgp_info_mpath_update (testcase_t *t)
{
  struct bgp_info *new_best, *old_best, *mpath;
  struct list mp_list;
  struct bgp_maxpaths_cfg mp_cfg = { 3, 3 };
  int test_result = TEST_PASSED;
  bgp_mp_list_init (&mp_list);
  bgp_mp_list_add (&mp_list, &test_mp_list_info[4]);
  bgp_mp_list_add (&mp_list, &test_mp_list_info[3]);
  bgp_mp_list_add (&mp_list, &test_mp_list_info[0]);
  bgp_mp_list_add (&mp_list, &test_mp_list_info[1]);
  new_best = &test_mp_list_info[3];
  old_best = NULL;
  bgp_info_mpath_update (&test_rn, new_best, old_best, &mp_list, &mp_cfg);
  bgp_mp_list_clear (&mp_list);
  EXPECT_TRUE (bgp_info_mpath_count (new_best) == 2, test_result);
  mpath = bgp_info_mpath_first (new_best);
  EXPECT_TRUE (mpath == &test_mp_list_info[0], test_result);
  EXPECT_TRUE (CHECK_FLAG (mpath->flags, BGP_INFO_MULTIPATH), test_result);
  mpath = bgp_info_mpath_next (mpath);
  EXPECT_TRUE (mpath == &test_mp_list_info[1], test_result);
  EXPECT_TRUE (CHECK_FLAG (mpath->flags, BGP_INFO_MULTIPATH), test_result);

  bgp_mp_list_add (&mp_list, &test_mp_list_info[0]);
  bgp_mp_list_add (&mp_list, &test_mp_list_info[1]);
  new_best = &test_mp_list_info[0];
  old_best = &test_mp_list_info[3];
  bgp_info_mpath_update (&test_rn, new_best, old_best, &mp_list, &mp_cfg);
  bgp_mp_list_clear (&mp_list);
  EXPECT_TRUE (bgp_info_mpath_count (new_best) == 1, test_result);
  mpath = bgp_info_mpath_first (new_best);
  EXPECT_TRUE (mpath == &test_mp_list_info[1], test_result);
  EXPECT_TRUE (CHECK_FLAG (mpath->flags, BGP_INFO_MULTIPATH), test_result);
  EXPECT_TRUE (!CHECK_FLAG (test_mp_list_info[0].flags, BGP_INFO_MULTIPATH),
               test_result);

  return test_result;
}

static int
cleanup_bgp_info_mpath_update (testcase_t *t)
{
  int i;

  for (i = 0; i < test_mp_list_peer_count; i++)
    sockunion_free (test_mp_list_peer[i].su_remote);

  return 0;
}

testcase_t test_bgp_info_mpath_update = {
  .desc = "Test bgp_info_mpath_update",
  .setup = setup_bgp_info_mpath_update,
  .run = run_bgp_info_mpath_update,
  .cleanup = cleanup_bgp_info_mpath_update,
};

/*=========================================================
 * Set up testcase vector
 */
testcase_t *all_tests[] = {
  &test_bgp_cfg_maximum_paths,
  &test_bgp_mp_list,
  &test_bgp_info_mpath_update,
};

int all_tests_count = (sizeof(all_tests)/sizeof(testcase_t *));

/*=========================================================
 * Test Driver Functions
 */
static int
global_test_init (void)
{
  master = thread_master_create ();
  zclient = zclient_new ();
  bgp_master_init ();
  bgp_option_set (BGP_OPT_NO_LISTEN);
  
  if (fileno (stdout) >= 0)
    tty = isatty (fileno (stdout));
  return 0;
}

static int
global_test_cleanup (void)
{
  zclient_free (zclient);
  thread_master_free (master);
  return 0;
}

static void
display_result (testcase_t *test, int result)
{
  if (tty)
    printf ("%s: %s\n", test->desc, result == TEST_PASSED ? OK : FAILED);
  else
    printf ("%s: %s\n", test->desc, result == TEST_PASSED ? "OK" : "FAILED");
}

static int
setup_test (testcase_t *t)
{
  int res = 0;
  if (t->setup)
    res = t->setup (t);
  return res;
}

static int
cleanup_test (testcase_t *t)
{
  int res = 0;
  if (t->cleanup)
    res = t->cleanup (t);
  return res;
}

static void
run_tests (testcase_t *tests[], int num_tests, int *pass_count, int *fail_count)
{
  int test_index, result;
  testcase_t *cur_test;

  *pass_count = *fail_count = 0;

  for (test_index = 0; test_index < num_tests; test_index++)
    {
      cur_test = tests[test_index];
      if (!cur_test->desc)
        {
          printf ("error: test %d has no description!\n", test_index);
          continue;
        }
      if (!cur_test->run)
        {
          printf ("error: test %s has no run function!\n", cur_test->desc);
          continue;
        }
      if (setup_test (cur_test) != 0)
        {
          printf ("error: setup failed for test %s\n", cur_test->desc);
          continue;
        }
      result = cur_test->run (cur_test);
      if (result == TEST_PASSED)
        *pass_count += 1;
      else
        *fail_count += 1;
      display_result (cur_test, result);
      if (cleanup_test (cur_test) != 0)
        {
          printf ("error: cleanup failed for test %s\n", cur_test->desc);
          continue;
        }
    }
}

int
main (void)
{
  int pass_count, fail_count;
  time_t cur_time;

  time (&cur_time);
  printf("BGP Multipath Tests Run at %s", ctime(&cur_time));
  if (global_test_init () != 0)
    {
      printf("Global init failed. Terminating.\n");
      exit(1);
    }
  run_tests (all_tests, all_tests_count, &pass_count, &fail_count);
  global_test_cleanup ();
  printf("Total pass/fail: %d/%d\n", pass_count, fail_count);
  return fail_count;
}
