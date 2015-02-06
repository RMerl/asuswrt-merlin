/* Copyright (c) 2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "or.h"
#include "router.h"
#include "routerparse.h"
#include "policies.h"
#include "test.h"

/* Helper: assert that short_policy parses and writes back out as itself,
   or as <b>expected</b> if that's provided. */
static void
test_short_policy_parse(const char *input,
                        const char *expected)
{
  short_policy_t *short_policy = NULL;
  char *out = NULL;

  if (expected == NULL)
    expected = input;

  short_policy = parse_short_policy(input);
  tt_assert(short_policy);
  out = write_short_policy(short_policy);
  tt_str_op(out, ==, expected);

 done:
  tor_free(out);
  short_policy_free(short_policy);
}

/** Helper: Parse the exit policy string in <b>policy_str</b>, and make sure
 * that policies_summarize() produces the string <b>expected_summary</b> from
 * it. */
static void
test_policy_summary_helper(const char *policy_str,
                           const char *expected_summary)
{
  config_line_t line;
  smartlist_t *policy = smartlist_new();
  char *summary = NULL;
  char *summary_after = NULL;
  int r;
  short_policy_t *short_policy = NULL;

  line.key = (char*)"foo";
  line.value = (char *)policy_str;
  line.next = NULL;

  r = policies_parse_exit_policy(&line, &policy, 1, 0, 0, 1);
  test_eq(r, 0);
  summary = policy_summarize(policy, AF_INET);

  test_assert(summary != NULL);
  test_streq(summary, expected_summary);

  short_policy = parse_short_policy(summary);
  tt_assert(short_policy);
  summary_after = write_short_policy(short_policy);
  test_streq(summary, summary_after);

 done:
  tor_free(summary_after);
  tor_free(summary);
  if (policy)
    addr_policy_list_free(policy);
  short_policy_free(short_policy);
}

/** Run unit tests for generating summary lines of exit policies */
static void
test_policies_general(void *arg)
{
  int i;
  smartlist_t *policy = NULL, *policy2 = NULL, *policy3 = NULL,
              *policy4 = NULL, *policy5 = NULL, *policy6 = NULL,
              *policy7 = NULL;
  addr_policy_t *p;
  tor_addr_t tar;
  config_line_t line;
  smartlist_t *sm = NULL;
  char *policy_str = NULL;
  short_policy_t *short_parsed = NULL;
  (void)arg;

  policy = smartlist_new();

  p = router_parse_addr_policy_item_from_string("reject 192.168.0.0/16:*",-1);
  test_assert(p != NULL);
  test_eq(ADDR_POLICY_REJECT, p->policy_type);
  tor_addr_from_ipv4h(&tar, 0xc0a80000u);
  test_eq(0, tor_addr_compare(&p->addr, &tar, CMP_EXACT));
  test_eq(16, p->maskbits);
  test_eq(1, p->prt_min);
  test_eq(65535, p->prt_max);

  smartlist_add(policy, p);

  tor_addr_from_ipv4h(&tar, 0x01020304u);
  test_assert(ADDR_POLICY_ACCEPTED ==
          compare_tor_addr_to_addr_policy(&tar, 2, policy));
  tor_addr_make_unspec(&tar);
  test_assert(ADDR_POLICY_PROBABLY_ACCEPTED ==
          compare_tor_addr_to_addr_policy(&tar, 2, policy));
  tor_addr_from_ipv4h(&tar, 0xc0a80102);
  test_assert(ADDR_POLICY_REJECTED ==
          compare_tor_addr_to_addr_policy(&tar, 2, policy));

  test_assert(0 == policies_parse_exit_policy(NULL, &policy2, 1, 1, 0, 1));
  test_assert(policy2);

  policy3 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("reject *:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy3, p);
  p = router_parse_addr_policy_item_from_string("accept *:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy3, p);

  policy4 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("accept *:443",-1);
  test_assert(p != NULL);
  smartlist_add(policy4, p);
  p = router_parse_addr_policy_item_from_string("accept *:443",-1);
  test_assert(p != NULL);
  smartlist_add(policy4, p);

  policy5 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("reject 0.0.0.0/8:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 169.254.0.0/16:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 127.0.0.0/8:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 192.168.0.0/16:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 10.0.0.0/8:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 172.16.0.0/12:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 80.190.250.90:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject *:1-65534",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject *:65535",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("accept *:1-65535",-1);
  test_assert(p != NULL);
  smartlist_add(policy5, p);

  policy6 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("accept 43.3.0.0/9:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy6, p);

  policy7 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("accept 0.0.0.0/8:*",-1);
  test_assert(p != NULL);
  smartlist_add(policy7, p);

  test_assert(!exit_policy_is_general_exit(policy));
  test_assert(exit_policy_is_general_exit(policy2));
  test_assert(!exit_policy_is_general_exit(NULL));
  test_assert(!exit_policy_is_general_exit(policy3));
  test_assert(!exit_policy_is_general_exit(policy4));
  test_assert(!exit_policy_is_general_exit(policy5));
  test_assert(!exit_policy_is_general_exit(policy6));
  test_assert(!exit_policy_is_general_exit(policy7));

  test_assert(cmp_addr_policies(policy, policy2));
  test_assert(cmp_addr_policies(policy, NULL));
  test_assert(!cmp_addr_policies(policy2, policy2));
  test_assert(!cmp_addr_policies(NULL, NULL));

  test_assert(!policy_is_reject_star(policy2, AF_INET));
  test_assert(policy_is_reject_star(policy, AF_INET));
  test_assert(policy_is_reject_star(NULL, AF_INET));

  addr_policy_list_free(policy);
  policy = NULL;

  /* make sure compacting logic works. */
  policy = NULL;
  line.key = (char*)"foo";
  line.value = (char*)"accept *:80,reject private:*,reject *:*";
  line.next = NULL;
  test_assert(0 == policies_parse_exit_policy(&line, &policy, 1, 0, 0, 1));
  test_assert(policy);
  //test_streq(policy->string, "accept *:80");
  //test_streq(policy->next->string, "reject *:*");
  test_eq(smartlist_len(policy), 4);

  /* test policy summaries */
  /* check if we properly ignore private IP addresses */
  test_policy_summary_helper("reject 192.168.0.0/16:*,"
                             "reject 0.0.0.0/8:*,"
                             "reject 10.0.0.0/8:*,"
                             "accept *:10-30,"
                             "accept *:90,"
                             "reject *:*",
                             "accept 10-30,90");
  /* check all accept policies, and proper counting of rejects */
  test_policy_summary_helper("reject 11.0.0.0/9:80,"
                             "reject 12.0.0.0/9:80,"
                             "reject 13.0.0.0/9:80,"
                             "reject 14.0.0.0/9:80,"
                             "accept *:*", "accept 1-65535");
  test_policy_summary_helper("reject 11.0.0.0/9:80,"
                             "reject 12.0.0.0/9:80,"
                             "reject 13.0.0.0/9:80,"
                             "reject 14.0.0.0/9:80,"
                             "reject 15.0.0.0:81,"
                             "accept *:*", "accept 1-65535");
  test_policy_summary_helper("reject 11.0.0.0/9:80,"
                             "reject 12.0.0.0/9:80,"
                             "reject 13.0.0.0/9:80,"
                             "reject 14.0.0.0/9:80,"
                             "reject 15.0.0.0:80,"
                             "accept *:*",
                             "reject 80");
  /* no exits */
  test_policy_summary_helper("accept 11.0.0.0/9:80,"
                             "reject *:*",
                             "reject 1-65535");
  /* port merging */
  test_policy_summary_helper("accept *:80,"
                             "accept *:81,"
                             "accept *:100-110,"
                             "accept *:111,"
                             "reject *:*",
                             "accept 80-81,100-111");
  /* border ports */
  test_policy_summary_helper("accept *:1,"
                             "accept *:3,"
                             "accept *:65535,"
                             "reject *:*",
                             "accept 1,3,65535");
  /* holes */
  test_policy_summary_helper("accept *:1,"
                             "accept *:3,"
                             "accept *:5,"
                             "accept *:7,"
                             "reject *:*",
                             "accept 1,3,5,7");
  test_policy_summary_helper("reject *:1,"
                             "reject *:3,"
                             "reject *:5,"
                             "reject *:7,"
                             "accept *:*",
                             "reject 1,3,5,7");

  /* Short policies with unrecognized formats should get accepted. */
  test_short_policy_parse("accept fred,2,3-5", "accept 2,3-5");
  test_short_policy_parse("accept 2,fred,3", "accept 2,3");
  test_short_policy_parse("accept 2,fred,3,bob", "accept 2,3");
  test_short_policy_parse("accept 2,-3,500-600", "accept 2,500-600");
  /* Short policies with nil entries are accepted too. */
  test_short_policy_parse("accept 1,,3", "accept 1,3");
  test_short_policy_parse("accept 100-200,,", "accept 100-200");
  test_short_policy_parse("reject ,1-10,,,,30-40", "reject 1-10,30-40");

  /* Try parsing various broken short policies */
#define TT_BAD_SHORT_POLICY(s)                                          \
  do {                                                                  \
    tt_ptr_op(NULL, ==, (short_parsed = parse_short_policy((s))));      \
  } while (0)
  TT_BAD_SHORT_POLICY("accept 200-199");
  TT_BAD_SHORT_POLICY("");
  TT_BAD_SHORT_POLICY("rejekt 1,2,3");
  TT_BAD_SHORT_POLICY("reject ");
  TT_BAD_SHORT_POLICY("reject");
  TT_BAD_SHORT_POLICY("rej");
  TT_BAD_SHORT_POLICY("accept 2,3,100000");
  TT_BAD_SHORT_POLICY("accept 2,3x,4");
  TT_BAD_SHORT_POLICY("accept 2,3x,4");
  TT_BAD_SHORT_POLICY("accept 2-");
  TT_BAD_SHORT_POLICY("accept 2-x");
  TT_BAD_SHORT_POLICY("accept 1-,3");
  TT_BAD_SHORT_POLICY("accept 1-,3");

  /* Test a too-long policy. */
  {
    int i;
    char *policy = NULL;
    smartlist_t *chunks = smartlist_new();
    smartlist_add(chunks, tor_strdup("accept "));
    for (i=1; i<10000; ++i)
      smartlist_add_asprintf(chunks, "%d,", i);
    smartlist_add(chunks, tor_strdup("20000"));
    policy = smartlist_join_strings(chunks, "", 0, NULL);
    SMARTLIST_FOREACH(chunks, char *, ch, tor_free(ch));
    smartlist_free(chunks);
    short_parsed = parse_short_policy(policy);/* shouldn't be accepted */
    tor_free(policy);
    tt_ptr_op(NULL, ==, short_parsed);
  }

  /* truncation ports */
  sm = smartlist_new();
  for (i=1; i<2000; i+=2) {
    char buf[POLICY_BUF_LEN];
    tor_snprintf(buf, sizeof(buf), "reject *:%d", i);
    smartlist_add(sm, tor_strdup(buf));
  }
  smartlist_add(sm, tor_strdup("accept *:*"));
  policy_str = smartlist_join_strings(sm, ",", 0, NULL);
  test_policy_summary_helper( policy_str,
    "accept 2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,"
    "46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80,82,84,86,88,90,"
    "92,94,96,98,100,102,104,106,108,110,112,114,116,118,120,122,124,126,128,"
    "130,132,134,136,138,140,142,144,146,148,150,152,154,156,158,160,162,164,"
    "166,168,170,172,174,176,178,180,182,184,186,188,190,192,194,196,198,200,"
    "202,204,206,208,210,212,214,216,218,220,222,224,226,228,230,232,234,236,"
    "238,240,242,244,246,248,250,252,254,256,258,260,262,264,266,268,270,272,"
    "274,276,278,280,282,284,286,288,290,292,294,296,298,300,302,304,306,308,"
    "310,312,314,316,318,320,322,324,326,328,330,332,334,336,338,340,342,344,"
    "346,348,350,352,354,356,358,360,362,364,366,368,370,372,374,376,378,380,"
    "382,384,386,388,390,392,394,396,398,400,402,404,406,408,410,412,414,416,"
    "418,420,422,424,426,428,430,432,434,436,438,440,442,444,446,448,450,452,"
    "454,456,458,460,462,464,466,468,470,472,474,476,478,480,482,484,486,488,"
    "490,492,494,496,498,500,502,504,506,508,510,512,514,516,518,520,522");

 done:
  addr_policy_list_free(policy);
  addr_policy_list_free(policy2);
  addr_policy_list_free(policy3);
  addr_policy_list_free(policy4);
  addr_policy_list_free(policy5);
  addr_policy_list_free(policy6);
  addr_policy_list_free(policy7);
  tor_free(policy_str);
  if (sm) {
    SMARTLIST_FOREACH(sm, char *, s, tor_free(s));
    smartlist_free(sm);
  }
  short_policy_free(short_parsed);
}

static void
test_dump_exit_policy_to_string(void *arg)
{
 char *ep;
 addr_policy_t *policy_entry;

 routerinfo_t *ri = tor_malloc_zero(sizeof(routerinfo_t));

 (void)arg;

 ri->policy_is_reject_star = 1;
 ri->exit_policy = NULL; // expecting "reject *:*"
 ep = router_dump_exit_policy_to_string(ri,1,1);

 test_streq("reject *:*",ep);

 tor_free(ep);

 ri->exit_policy = smartlist_new();
 ri->policy_is_reject_star = 0;

 policy_entry = router_parse_addr_policy_item_from_string("accept *:*",-1);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 test_streq("accept *:*",ep);

 tor_free(ep);

 policy_entry = router_parse_addr_policy_item_from_string("reject *:25",-1);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 test_streq("accept *:*\nreject *:25",ep);

 tor_free(ep);

 policy_entry =
 router_parse_addr_policy_item_from_string("reject 8.8.8.8:*",-1);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 test_streq("accept *:*\nreject *:25\nreject 8.8.8.8:*",ep);
 tor_free(ep);

 policy_entry =
 router_parse_addr_policy_item_from_string("reject6 [FC00::]/7:*",-1);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 test_streq("accept *:*\nreject *:25\nreject 8.8.8.8:*\n"
            "reject6 [fc00::]/7:*",ep);
 tor_free(ep);

 policy_entry =
 router_parse_addr_policy_item_from_string("accept6 [c000::]/3:*",-1);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 test_streq("accept *:*\nreject *:25\nreject 8.8.8.8:*\n"
            "reject6 [fc00::]/7:*\naccept6 [c000::]/3:*",ep);

 done:

 if (ri->exit_policy) {
   SMARTLIST_FOREACH(ri->exit_policy, addr_policy_t *,
                     entry, addr_policy_free(entry));
   smartlist_free(ri->exit_policy);
 }
 tor_free(ri);
 tor_free(ep);
}

struct testcase_t policy_tests[] = {
  { "router_dump_exit_policy_to_string", test_dump_exit_policy_to_string, 0,
    NULL, NULL },
  { "general", test_policies_general, 0, NULL, NULL },
  END_OF_TESTCASES
};

