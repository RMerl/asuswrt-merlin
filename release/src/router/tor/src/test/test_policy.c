/* Copyright (c) 2013-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "or.h"
#define CONFIG_PRIVATE
#include "config.h"
#include "router.h"
#include "routerparse.h"
#define POLICIES_PRIVATE
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
  tt_str_op(out, OP_EQ, expected);

 done:
  tor_free(out);
  short_policy_free(short_policy);
}

/** Helper: Parse the exit policy string in <b>policy_str</b> with
 * <b>options</b>, and make sure that policies_summarize() produces the string
 * <b>expected_summary</b> from it when called with family. */
static void
test_policy_summary_helper_family_flags(const char *policy_str,
                                        const char *expected_summary,
                                        sa_family_t family,
                                        exit_policy_parser_cfg_t options)
{
  config_line_t line;
  smartlist_t *policy = smartlist_new();
  char *summary = NULL;
  char *summary_after = NULL;
  int r;
  short_policy_t *short_policy = NULL;
  int success = 0;

  line.key = (char*)"foo";
  line.value = (char *)policy_str;
  line.next = NULL;

  r = policies_parse_exit_policy(&line, &policy,
                                 options, NULL);
  tt_int_op(r,OP_EQ, 0);

  summary = policy_summarize(policy, family);

  tt_assert(summary != NULL);
  tt_str_op(summary,OP_EQ, expected_summary);

  short_policy = parse_short_policy(summary);
  tt_assert(short_policy);
  summary_after = write_short_policy(short_policy);
  tt_str_op(summary,OP_EQ, summary_after);

  success = 1;
 done:
  /* If we don't print the flags on failure, it's very hard to diagnose bugs */
  if (!success)
    TT_DECLARE("CTXT", ("\n     IPv%d\n     Options: %x\n     Policy: %s",
                        family == AF_INET ? 4 : 6, options, policy_str));
  tor_free(summary_after);
  tor_free(summary);
  if (policy)
    addr_policy_list_free(policy);
  short_policy_free(short_policy);
}

/** Like test_policy_summary_helper_family_flags, but tries all the different
 * flag combinations */
static void
test_policy_summary_helper_family(const char *policy_str,
                                  const char *expected_summary,
                                  sa_family_t family)
{
  for (exit_policy_parser_cfg_t opt = 0;
       opt <= EXIT_POLICY_OPTION_ALL;
       opt++) {
    if (family == AF_INET6 && !(opt & EXIT_POLICY_IPV6_ENABLED))
      /* Skip the test: IPv6 addresses need IPv6 enabled */
      continue;

    if (opt & EXIT_POLICY_REJECT_LOCAL_INTERFACES)
      /* Skip the test: local interfaces are machine-specific */
      continue;

    test_policy_summary_helper_family_flags(policy_str, expected_summary,
                                            family, opt);
  }
}

/** Like test_policy_summary_helper_family, but uses expected_summary for
 * both IPv4 and IPv6. */
static void
test_policy_summary_helper(const char *policy_str,
                           const char *expected_summary)
{
  test_policy_summary_helper_family(policy_str, expected_summary, AF_INET);
  test_policy_summary_helper_family(policy_str, expected_summary, AF_INET6);
}

/** Like test_policy_summary_helper_family, but uses expected_summary4 for
 * IPv4 and expected_summary6 for IPv6. */
static void
test_policy_summary_helper6(const char *policy_str,
                            const char *expected_summary4,
                            const char *expected_summary6)
{
  test_policy_summary_helper_family(policy_str, expected_summary4, AF_INET);
  test_policy_summary_helper_family(policy_str, expected_summary6, AF_INET6);
}

/** Run unit tests for generating summary lines of exit policies */
static void
test_policies_general(void *arg)
{
  int i;
  smartlist_t *policy = NULL, *policy2 = NULL, *policy3 = NULL,
              *policy4 = NULL, *policy5 = NULL, *policy6 = NULL,
              *policy7 = NULL, *policy8 = NULL, *policy9 = NULL,
              *policy10 = NULL, *policy11 = NULL, *policy12 = NULL;
  addr_policy_t *p;
  tor_addr_t tar, tar2;
  smartlist_t *addr_list = NULL;
  config_line_t line;
  smartlist_t *sm = NULL;
  char *policy_str = NULL;
  short_policy_t *short_parsed = NULL;
  int malformed_list = -1;
  (void)arg;

  policy = smartlist_new();

  p = router_parse_addr_policy_item_from_string("reject 192.168.0.0/16:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  tt_int_op(ADDR_POLICY_REJECT,OP_EQ, p->policy_type);
  tor_addr_from_ipv4h(&tar, 0xc0a80000u);
  tt_int_op(0,OP_EQ, tor_addr_compare(&p->addr, &tar, CMP_EXACT));
  tt_int_op(16,OP_EQ, p->maskbits);
  tt_int_op(1,OP_EQ, p->prt_min);
  tt_int_op(65535,OP_EQ, p->prt_max);

  smartlist_add(policy, p);

  tor_addr_from_ipv4h(&tar, 0x01020304u);
  tt_assert(ADDR_POLICY_ACCEPTED ==
          compare_tor_addr_to_addr_policy(&tar, 2, policy));
  tor_addr_make_unspec(&tar);
  tt_assert(ADDR_POLICY_PROBABLY_ACCEPTED ==
          compare_tor_addr_to_addr_policy(&tar, 2, policy));
  tor_addr_from_ipv4h(&tar, 0xc0a80102);
  tt_assert(ADDR_POLICY_REJECTED ==
          compare_tor_addr_to_addr_policy(&tar, 2, policy));

  tt_int_op(0, OP_EQ, policies_parse_exit_policy(NULL, &policy2,
                                              EXIT_POLICY_IPV6_ENABLED |
                                              EXIT_POLICY_REJECT_PRIVATE |
                                              EXIT_POLICY_ADD_DEFAULT, NULL));

  tt_assert(policy2);

  tor_addr_from_ipv4h(&tar, 0x0306090cu);
  tor_addr_parse(&tar2, "[2000::1234]");
  addr_list = smartlist_new();
  smartlist_add(addr_list, &tar);
  smartlist_add(addr_list, &tar2);
  tt_int_op(0, OP_EQ, policies_parse_exit_policy(NULL, &policy12,
                                                 EXIT_POLICY_IPV6_ENABLED |
                                                 EXIT_POLICY_REJECT_PRIVATE |
                                                 EXIT_POLICY_ADD_DEFAULT,
                                                 addr_list));
  smartlist_free(addr_list);
  addr_list = NULL;

  tt_assert(policy12);

  policy3 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("reject *:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy3, p);
  p = router_parse_addr_policy_item_from_string("accept *:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy3, p);

  policy4 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("accept *:443", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy4, p);
  p = router_parse_addr_policy_item_from_string("accept *:443", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy4, p);

  policy5 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("reject 0.0.0.0/8:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 169.254.0.0/16:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 127.0.0.0/8:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 192.168.0.0/16:*",
                                                -1, &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 10.0.0.0/8:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 172.16.0.0/12:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject 80.190.250.90:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject *:1-65534", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("reject *:65535", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);
  p = router_parse_addr_policy_item_from_string("accept *:1-65535", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy5, p);

  policy6 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("accept 43.3.0.0/9:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy6, p);

  policy7 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("accept 0.0.0.0/8:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy7, p);

  tt_int_op(0, OP_EQ, policies_parse_exit_policy(NULL, &policy8,
                                                 EXIT_POLICY_IPV6_ENABLED |
                                                 EXIT_POLICY_REJECT_PRIVATE |
                                                 EXIT_POLICY_ADD_DEFAULT,
                                                 NULL));

  tt_assert(policy8);

  tt_int_op(0, OP_EQ, policies_parse_exit_policy(NULL, &policy9,
                                                 EXIT_POLICY_REJECT_PRIVATE |
                                                 EXIT_POLICY_ADD_DEFAULT,
                                                 NULL));

  tt_assert(policy9);

  /* accept6 * and reject6 * produce IPv6 wildcards only */
  policy10 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("accept6 *:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy10, p);

  policy11 = smartlist_new();
  p = router_parse_addr_policy_item_from_string("reject6 *:*", -1,
                                                &malformed_list);
  tt_assert(p != NULL);
  smartlist_add(policy11, p);

  tt_assert(!exit_policy_is_general_exit(policy));
  tt_assert(exit_policy_is_general_exit(policy2));
  tt_assert(!exit_policy_is_general_exit(NULL));
  tt_assert(!exit_policy_is_general_exit(policy3));
  tt_assert(!exit_policy_is_general_exit(policy4));
  tt_assert(!exit_policy_is_general_exit(policy5));
  tt_assert(!exit_policy_is_general_exit(policy6));
  tt_assert(!exit_policy_is_general_exit(policy7));
  tt_assert(exit_policy_is_general_exit(policy8));
  tt_assert(exit_policy_is_general_exit(policy9));
  tt_assert(!exit_policy_is_general_exit(policy10));
  tt_assert(!exit_policy_is_general_exit(policy11));

  tt_assert(!addr_policies_eq(policy, policy2));
  tt_assert(!addr_policies_eq(policy, NULL));
  tt_assert(addr_policies_eq(policy2, policy2));
  tt_assert(addr_policies_eq(NULL, NULL));

  tt_assert(!policy_is_reject_star(policy2, AF_INET, 1));
  tt_assert(policy_is_reject_star(policy, AF_INET, 1));
  tt_assert(policy_is_reject_star(policy10, AF_INET, 1));
  tt_assert(!policy_is_reject_star(policy10, AF_INET6, 1));
  tt_assert(policy_is_reject_star(policy11, AF_INET, 1));
  tt_assert(policy_is_reject_star(policy11, AF_INET6, 1));
  tt_assert(policy_is_reject_star(NULL, AF_INET, 1));
  tt_assert(policy_is_reject_star(NULL, AF_INET6, 1));
  tt_assert(!policy_is_reject_star(NULL, AF_INET, 0));
  tt_assert(!policy_is_reject_star(NULL, AF_INET6, 0));

  addr_policy_list_free(policy);
  policy = NULL;

  /* make sure assume_action works */
  malformed_list = 0;
  p = router_parse_addr_policy_item_from_string("127.0.0.1",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(p);
  addr_policy_free(p);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("127.0.0.1:*",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(p);
  addr_policy_free(p);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("[::]",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(p);
  addr_policy_free(p);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("[::]:*",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(p);
  addr_policy_free(p);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("[face::b]",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(p);
  addr_policy_free(p);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("[b::aaaa]",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(p);
  addr_policy_free(p);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("*",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(p);
  addr_policy_free(p);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("*4",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(p);
  addr_policy_free(p);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("*6",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(p);
  addr_policy_free(p);
  tt_assert(!malformed_list);

  /* These are all ambiguous IPv6 addresses, it's good that we reject them */
  p = router_parse_addr_policy_item_from_string("acce::abcd",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(!p);
  tt_assert(malformed_list);
  malformed_list = 0;

  p = router_parse_addr_policy_item_from_string("7:1234",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(!p);
  tt_assert(malformed_list);
  malformed_list = 0;

  p = router_parse_addr_policy_item_from_string("::",
                                                ADDR_POLICY_ACCEPT,
                                                &malformed_list);
  tt_assert(!p);
  tt_assert(malformed_list);
  malformed_list = 0;

  /* make sure compacting logic works. */
  policy = NULL;
  line.key = (char*)"foo";
  line.value = (char*)"accept *:80,reject private:*,reject *:*";
  line.next = NULL;
  tt_int_op(0, OP_EQ, policies_parse_exit_policy(&line,&policy,
                                              EXIT_POLICY_IPV6_ENABLED |
                                              EXIT_POLICY_ADD_DEFAULT, NULL));
  tt_assert(policy);

  //test_streq(policy->string, "accept *:80");
  //test_streq(policy->next->string, "reject *:*");
  tt_int_op(smartlist_len(policy),OP_EQ, 4);

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
  test_policy_summary_helper6("reject 11.0.0.0/9:80,"
                              "reject 12.0.0.0/9:80,"
                              "reject 13.0.0.0/9:80,"
                              "reject 14.0.0.0/9:80,"
                              "reject 15.0.0.0:80,"
                              "accept *:*",
                              "reject 80",
                              "accept 1-65535");
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
  /* long policies */
  /* standard long policy on many exits */
  test_policy_summary_helper("accept *:20-23,"
                             "accept *:43,"
                             "accept *:53,"
                             "accept *:79-81,"
                             "accept *:88,"
                             "accept *:110,"
                             "accept *:143,"
                             "accept *:194,"
                             "accept *:220,"
                             "accept *:389,"
                             "accept *:443,"
                             "accept *:464,"
                             "accept *:531,"
                             "accept *:543-544,"
                             "accept *:554,"
                             "accept *:563,"
                             "accept *:636,"
                             "accept *:706,"
                             "accept *:749,"
                             "accept *:873,"
                             "accept *:902-904,"
                             "accept *:981,"
                             "accept *:989-995,"
                             "accept *:1194,"
                             "accept *:1220,"
                             "accept *:1293,"
                             "accept *:1500,"
                             "accept *:1533,"
                             "accept *:1677,"
                             "accept *:1723,"
                             "accept *:1755,"
                             "accept *:1863,"
                             "accept *:2082,"
                             "accept *:2083,"
                             "accept *:2086-2087,"
                             "accept *:2095-2096,"
                             "accept *:2102-2104,"
                             "accept *:3128,"
                             "accept *:3389,"
                             "accept *:3690,"
                             "accept *:4321,"
                             "accept *:4643,"
                             "accept *:5050,"
                             "accept *:5190,"
                             "accept *:5222-5223,"
                             "accept *:5228,"
                             "accept *:5900,"
                             "accept *:6660-6669,"
                             "accept *:6679,"
                             "accept *:6697,"
                             "accept *:8000,"
                             "accept *:8008,"
                             "accept *:8074,"
                             "accept *:8080,"
                             "accept *:8087-8088,"
                             "accept *:8332-8333,"
                             "accept *:8443,"
                             "accept *:8888,"
                             "accept *:9418,"
                             "accept *:9999,"
                             "accept *:10000,"
                             "accept *:11371,"
                             "accept *:12350,"
                             "accept *:19294,"
                             "accept *:19638,"
                             "accept *:23456,"
                             "accept *:33033,"
                             "accept *:64738,"
                             "reject *:*",
                             "accept 20-23,43,53,79-81,88,110,143,194,220,389,"
                             "443,464,531,543-544,554,563,636,706,749,873,"
                             "902-904,981,989-995,1194,1220,1293,1500,1533,"
                             "1677,1723,1755,1863,2082-2083,2086-2087,"
                             "2095-2096,2102-2104,3128,3389,3690,4321,4643,"
                             "5050,5190,5222-5223,5228,5900,6660-6669,6679,"
                             "6697,8000,8008,8074,8080,8087-8088,8332-8333,"
                             "8443,8888,9418,9999-10000,11371,12350,19294,"
                             "19638,23456,33033,64738");
  /* short policy with configured addresses */
  test_policy_summary_helper("reject 149.56.1.1:*,"
                             "reject [2607:5300:1:1::1:0]:*,"
                             "accept *:80,"
                             "accept *:443,"
                             "reject *:*",
                             "accept 80,443");
  /* short policy with configured and local interface addresses */
  test_policy_summary_helper("reject 149.56.1.0:*,"
                             "reject 149.56.1.1:*,"
                             "reject 149.56.1.2:*,"
                             "reject 149.56.1.3:*,"
                             "reject 149.56.1.4:*,"
                             "reject 149.56.1.5:*,"
                             "reject 149.56.1.6:*,"
                             "reject 149.56.1.7:*,"
                             "reject [2607:5300:1:1::1:0]:*,"
                             "reject [2607:5300:1:1::1:1]:*,"
                             "reject [2607:5300:1:1::1:2]:*,"
                             "reject [2607:5300:1:1::1:3]:*,"
                             "reject [2607:5300:1:1::2:0]:*,"
                             "reject [2607:5300:1:1::2:1]:*,"
                             "reject [2607:5300:1:1::2:2]:*,"
                             "reject [2607:5300:1:1::2:3]:*,"
                             "accept *:80,"
                             "accept *:443,"
                             "reject *:*",
                             "accept 80,443");
  /* short policy with configured netblocks */
  test_policy_summary_helper("reject 149.56.0.0/16,"
                             "reject6 2607:5300::/32,"
                             "reject6 2608:5300::/64,"
                             "reject6 2609:5300::/96,"
                             "accept *:80,"
                             "accept *:443,"
                             "reject *:*",
                             "accept 80,443");
  /* short policy with large netblocks that do not count as a rejection */
  test_policy_summary_helper("reject 148.0.0.0/7,"
                             "reject6 2600::/16,"
                             "accept *:80,"
                             "accept *:443,"
                             "reject *:*",
                             "accept 80,443");
  /* short policy with large netblocks that count as a rejection */
  test_policy_summary_helper("reject 148.0.0.0/6,"
                             "reject6 2600::/15,"
                             "accept *:80,"
                             "accept *:443,"
                             "reject *:*",
                             "reject 1-65535");
  /* short policy with huge netblocks that count as a rejection */
  test_policy_summary_helper("reject 128.0.0.0/1,"
                             "reject6 8000::/1,"
                             "accept *:80,"
                             "accept *:443,"
                             "reject *:*",
                             "reject 1-65535");
  /* short policy which blocks everything using netblocks */
  test_policy_summary_helper("reject 0.0.0.0/0,"
                             "reject6 ::/0,"
                             "accept *:80,"
                             "accept *:443,"
                             "reject *:*",
                             "reject 1-65535");
  /* short policy which has repeated redundant netblocks */
  test_policy_summary_helper("reject 0.0.0.0/0,"
                             "reject 0.0.0.0/0,"
                             "reject 0.0.0.0/0,"
                             "reject 0.0.0.0/0,"
                             "reject 0.0.0.0/0,"
                             "reject6 ::/0,"
                             "reject6 ::/0,"
                             "reject6 ::/0,"
                             "reject6 ::/0,"
                             "reject6 ::/0,"
                             "accept *:80,"
                             "accept *:443,"
                             "reject *:*",
                             "reject 1-65535");

  /* longest possible policy
   * (1-2,4-5,... is longer, but gets reduced to 3,6,... )
   * Going all the way to 65535 is incredibly slow, so we just go slightly
   * more than the expected length */
  test_policy_summary_helper("accept *:1,"
                             "accept *:3,"
                             "accept *:5,"
                             "accept *:7,"
                             "accept *:9,"
                             "accept *:11,"
                             "accept *:13,"
                             "accept *:15,"
                             "accept *:17,"
                             "accept *:19,"
                             "accept *:21,"
                             "accept *:23,"
                             "accept *:25,"
                             "accept *:27,"
                             "accept *:29,"
                             "accept *:31,"
                             "accept *:33,"
                             "accept *:35,"
                             "accept *:37,"
                             "accept *:39,"
                             "accept *:41,"
                             "accept *:43,"
                             "accept *:45,"
                             "accept *:47,"
                             "accept *:49,"
                             "accept *:51,"
                             "accept *:53,"
                             "accept *:55,"
                             "accept *:57,"
                             "accept *:59,"
                             "accept *:61,"
                             "accept *:63,"
                             "accept *:65,"
                             "accept *:67,"
                             "accept *:69,"
                             "accept *:71,"
                             "accept *:73,"
                             "accept *:75,"
                             "accept *:77,"
                             "accept *:79,"
                             "accept *:81,"
                             "accept *:83,"
                             "accept *:85,"
                             "accept *:87,"
                             "accept *:89,"
                             "accept *:91,"
                             "accept *:93,"
                             "accept *:95,"
                             "accept *:97,"
                             "accept *:99,"
                             "accept *:101,"
                             "accept *:103,"
                             "accept *:105,"
                             "accept *:107,"
                             "accept *:109,"
                             "accept *:111,"
                             "accept *:113,"
                             "accept *:115,"
                             "accept *:117,"
                             "accept *:119,"
                             "accept *:121,"
                             "accept *:123,"
                             "accept *:125,"
                             "accept *:127,"
                             "accept *:129,"
                             "accept *:131,"
                             "accept *:133,"
                             "accept *:135,"
                             "accept *:137,"
                             "accept *:139,"
                             "accept *:141,"
                             "accept *:143,"
                             "accept *:145,"
                             "accept *:147,"
                             "accept *:149,"
                             "accept *:151,"
                             "accept *:153,"
                             "accept *:155,"
                             "accept *:157,"
                             "accept *:159,"
                             "accept *:161,"
                             "accept *:163,"
                             "accept *:165,"
                             "accept *:167,"
                             "accept *:169,"
                             "accept *:171,"
                             "accept *:173,"
                             "accept *:175,"
                             "accept *:177,"
                             "accept *:179,"
                             "accept *:181,"
                             "accept *:183,"
                             "accept *:185,"
                             "accept *:187,"
                             "accept *:189,"
                             "accept *:191,"
                             "accept *:193,"
                             "accept *:195,"
                             "accept *:197,"
                             "accept *:199,"
                             "accept *:201,"
                             "accept *:203,"
                             "accept *:205,"
                             "accept *:207,"
                             "accept *:209,"
                             "accept *:211,"
                             "accept *:213,"
                             "accept *:215,"
                             "accept *:217,"
                             "accept *:219,"
                             "accept *:221,"
                             "accept *:223,"
                             "accept *:225,"
                             "accept *:227,"
                             "accept *:229,"
                             "accept *:231,"
                             "accept *:233,"
                             "accept *:235,"
                             "accept *:237,"
                             "accept *:239,"
                             "accept *:241,"
                             "accept *:243,"
                             "accept *:245,"
                             "accept *:247,"
                             "accept *:249,"
                             "accept *:251,"
                             "accept *:253,"
                             "accept *:255,"
                             "accept *:257,"
                             "accept *:259,"
                             "accept *:261,"
                             "accept *:263,"
                             "accept *:265,"
                             "accept *:267,"
                             "accept *:269,"
                             "accept *:271,"
                             "accept *:273,"
                             "accept *:275,"
                             "accept *:277,"
                             "accept *:279,"
                             "accept *:281,"
                             "accept *:283,"
                             "accept *:285,"
                             "accept *:287,"
                             "accept *:289,"
                             "accept *:291,"
                             "accept *:293,"
                             "accept *:295,"
                             "accept *:297,"
                             "accept *:299,"
                             "accept *:301,"
                             "accept *:303,"
                             "accept *:305,"
                             "accept *:307,"
                             "accept *:309,"
                             "accept *:311,"
                             "accept *:313,"
                             "accept *:315,"
                             "accept *:317,"
                             "accept *:319,"
                             "accept *:321,"
                             "accept *:323,"
                             "accept *:325,"
                             "accept *:327,"
                             "accept *:329,"
                             "accept *:331,"
                             "accept *:333,"
                             "accept *:335,"
                             "accept *:337,"
                             "accept *:339,"
                             "accept *:341,"
                             "accept *:343,"
                             "accept *:345,"
                             "accept *:347,"
                             "accept *:349,"
                             "accept *:351,"
                             "accept *:353,"
                             "accept *:355,"
                             "accept *:357,"
                             "accept *:359,"
                             "accept *:361,"
                             "accept *:363,"
                             "accept *:365,"
                             "accept *:367,"
                             "accept *:369,"
                             "accept *:371,"
                             "accept *:373,"
                             "accept *:375,"
                             "accept *:377,"
                             "accept *:379,"
                             "accept *:381,"
                             "accept *:383,"
                             "accept *:385,"
                             "accept *:387,"
                             "accept *:389,"
                             "accept *:391,"
                             "accept *:393,"
                             "accept *:395,"
                             "accept *:397,"
                             "accept *:399,"
                             "accept *:401,"
                             "accept *:403,"
                             "accept *:405,"
                             "accept *:407,"
                             "accept *:409,"
                             "accept *:411,"
                             "accept *:413,"
                             "accept *:415,"
                             "accept *:417,"
                             "accept *:419,"
                             "accept *:421,"
                             "accept *:423,"
                             "accept *:425,"
                             "accept *:427,"
                             "accept *:429,"
                             "accept *:431,"
                             "accept *:433,"
                             "accept *:435,"
                             "accept *:437,"
                             "accept *:439,"
                             "accept *:441,"
                             "accept *:443,"
                             "accept *:445,"
                             "accept *:447,"
                             "accept *:449,"
                             "accept *:451,"
                             "accept *:453,"
                             "accept *:455,"
                             "accept *:457,"
                             "accept *:459,"
                             "accept *:461,"
                             "accept *:463,"
                             "accept *:465,"
                             "accept *:467,"
                             "accept *:469,"
                             "accept *:471,"
                             "accept *:473,"
                             "accept *:475,"
                             "accept *:477,"
                             "accept *:479,"
                             "accept *:481,"
                             "accept *:483,"
                             "accept *:485,"
                             "accept *:487,"
                             "accept *:489,"
                             "accept *:491,"
                             "accept *:493,"
                             "accept *:495,"
                             "accept *:497,"
                             "accept *:499,"
                             "accept *:501,"
                             "accept *:503,"
                             "accept *:505,"
                             "accept *:507,"
                             "accept *:509,"
                             "accept *:511,"
                             "accept *:513,"
                             "accept *:515,"
                             "accept *:517,"
                             "accept *:519,"
                             "accept *:521,"
                             "accept *:523,"
                             "accept *:525,"
                             "accept *:527,"
                             "accept *:529,"
                             "reject *:*",
                             "accept 1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,"
                             "31,33,35,37,39,41,43,45,47,49,51,53,55,57,59,61,"
                             "63,65,67,69,71,73,75,77,79,81,83,85,87,89,91,93,"
                             "95,97,99,101,103,105,107,109,111,113,115,117,"
                             "119,121,123,125,127,129,131,133,135,137,139,141,"
                             "143,145,147,149,151,153,155,157,159,161,163,165,"
                             "167,169,171,173,175,177,179,181,183,185,187,189,"
                             "191,193,195,197,199,201,203,205,207,209,211,213,"
                             "215,217,219,221,223,225,227,229,231,233,235,237,"
                             "239,241,243,245,247,249,251,253,255,257,259,261,"
                             "263,265,267,269,271,273,275,277,279,281,283,285,"
                             "287,289,291,293,295,297,299,301,303,305,307,309,"
                             "311,313,315,317,319,321,323,325,327,329,331,333,"
                             "335,337,339,341,343,345,347,349,351,353,355,357,"
                             "359,361,363,365,367,369,371,373,375,377,379,381,"
                             "383,385,387,389,391,393,395,397,399,401,403,405,"
                             "407,409,411,413,415,417,419,421,423,425,427,429,"
                             "431,433,435,437,439,441,443,445,447,449,451,453,"
                             "455,457,459,461,463,465,467,469,471,473,475,477,"
                             "479,481,483,485,487,489,491,493,495,497,499,501,"
                             "503,505,507,509,511,513,515,517,519,521,523");

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
    tt_ptr_op(NULL, OP_EQ, (short_parsed = parse_short_policy((s))));      \
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

  /* Make sure that IPv4 addresses are ignored in accept6/reject6 lines. */
  p = router_parse_addr_policy_item_from_string("accept6 1.2.3.4:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("reject6 2.4.6.0/24:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(!malformed_list);

  p = router_parse_addr_policy_item_from_string("accept6 *4:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(!malformed_list);

  /* Make sure malformed policies are detected as such. */
  p = router_parse_addr_policy_item_from_string("bad_token *4:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(malformed_list);

  p = router_parse_addr_policy_item_from_string("accept6 **:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(malformed_list);

  p = router_parse_addr_policy_item_from_string("accept */15:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(malformed_list);

  p = router_parse_addr_policy_item_from_string("reject6 */:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(malformed_list);

  p = router_parse_addr_policy_item_from_string("accept 127.0.0.1/33:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(malformed_list);

  p = router_parse_addr_policy_item_from_string("accept6 [::1]/129:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(malformed_list);

  p = router_parse_addr_policy_item_from_string("reject 8.8.8.8/-1:*", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(malformed_list);

  p = router_parse_addr_policy_item_from_string("reject 8.8.4.4:10-5", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(malformed_list);

  p = router_parse_addr_policy_item_from_string("reject 1.2.3.4:-1", -1,
                                                &malformed_list);
  tt_assert(p == NULL);
  tt_assert(malformed_list);

  /* Test a too-long policy. */
  {
    char *policy_strng = NULL;
    smartlist_t *chunks = smartlist_new();
    smartlist_add(chunks, tor_strdup("accept "));
    for (i=1; i<10000; ++i)
      smartlist_add_asprintf(chunks, "%d,", i);
    smartlist_add(chunks, tor_strdup("20000"));
    policy_strng = smartlist_join_strings(chunks, "", 0, NULL);
    SMARTLIST_FOREACH(chunks, char *, ch, tor_free(ch));
    smartlist_free(chunks);
    short_parsed = parse_short_policy(policy_strng);/* shouldn't be accepted */
    tor_free(policy_strng);
    tt_ptr_op(NULL, OP_EQ, short_parsed);
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
  addr_policy_list_free(policy8);
  addr_policy_list_free(policy9);
  addr_policy_list_free(policy10);
  addr_policy_list_free(policy11);
  addr_policy_list_free(policy12);
  tor_free(policy_str);
  if (sm) {
    SMARTLIST_FOREACH(sm, char *, s, tor_free(s));
    smartlist_free(sm);
  }
  short_policy_free(short_parsed);
}

/** Helper: Check that policy_list contains address */
static int
test_policy_has_address_helper(const smartlist_t *policy_list,
                               const tor_addr_t *addr)
{
  int found = 0;

  tt_assert(policy_list);
  tt_assert(addr);

  SMARTLIST_FOREACH_BEGIN(policy_list, addr_policy_t*, p) {
    if (tor_addr_eq(&p->addr, addr)) {
      found = 1;
    }
  } SMARTLIST_FOREACH_END(p);

  return found;

 done:
  return 0;
}

#define TEST_IPV4_ADDR (0x01020304)
#define TEST_IPV6_ADDR ("2002::abcd")

/** Run unit tests for rejecting the configured addresses on this exit relay
 * using policies_parse_exit_policy_reject_private */
static void
test_policies_reject_exit_address(void *arg)
{
  smartlist_t *policy = NULL;
  tor_addr_t ipv4_addr, ipv6_addr;
  smartlist_t *ipv4_list, *ipv6_list, *both_list, *dupl_list;
  (void)arg;

  tor_addr_from_ipv4h(&ipv4_addr, TEST_IPV4_ADDR);
  tor_addr_parse(&ipv6_addr, TEST_IPV6_ADDR);

  ipv4_list = smartlist_new();
  ipv6_list = smartlist_new();
  both_list = smartlist_new();
  dupl_list = smartlist_new();

  smartlist_add(ipv4_list, &ipv4_addr);
  smartlist_add(both_list, &ipv4_addr);
  smartlist_add(dupl_list, &ipv4_addr);
  smartlist_add(dupl_list, &ipv4_addr);
  smartlist_add(dupl_list, &ipv4_addr);

  smartlist_add(ipv6_list, &ipv6_addr);
  smartlist_add(both_list, &ipv6_addr);
  smartlist_add(dupl_list, &ipv6_addr);
  smartlist_add(dupl_list, &ipv6_addr);

  /* IPv4-Only Exits */

  /* test that IPv4 addresses are rejected on an IPv4-only exit */
  policies_parse_exit_policy_reject_private(&policy, 0, ipv4_list, 0, 0);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == 1);
  tt_assert(test_policy_has_address_helper(policy, &ipv4_addr));
  addr_policy_list_free(policy);
  policy = NULL;

  /* test that IPv6 addresses are NOT rejected on an IPv4-only exit
   * (all IPv6 addresses are rejected by policies_parse_exit_policy_internal
   * on IPv4-only exits, so policies_parse_exit_policy_reject_private doesn't
   * need to do anything) */
  policies_parse_exit_policy_reject_private(&policy, 0, ipv6_list, 0, 0);
  tt_assert(policy == NULL);

  /* test that only IPv4 addresses are rejected on an IPv4-only exit */
  policies_parse_exit_policy_reject_private(&policy, 0, both_list, 0, 0);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == 1);
  tt_assert(test_policy_has_address_helper(policy, &ipv4_addr));
  addr_policy_list_free(policy);
  policy = NULL;

  /* Test that lists with duplicate entries produce the same results */
  policies_parse_exit_policy_reject_private(&policy, 0, dupl_list, 0, 0);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == 1);
  tt_assert(test_policy_has_address_helper(policy, &ipv4_addr));
  addr_policy_list_free(policy);
  policy = NULL;

  /* IPv4/IPv6 Exits */

  /* test that IPv4 addresses are rejected on an IPv4/IPv6 exit */
  policies_parse_exit_policy_reject_private(&policy, 1, ipv4_list, 0, 0);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == 1);
  tt_assert(test_policy_has_address_helper(policy, &ipv4_addr));
  addr_policy_list_free(policy);
  policy = NULL;

  /* test that IPv6 addresses are rejected on an IPv4/IPv6 exit */
  policies_parse_exit_policy_reject_private(&policy, 1, ipv6_list,  0, 0);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == 1);
  tt_assert(test_policy_has_address_helper(policy, &ipv6_addr));
  addr_policy_list_free(policy);
  policy = NULL;

  /* test that IPv4 and IPv6 addresses are rejected on an IPv4/IPv6 exit */
  policies_parse_exit_policy_reject_private(&policy, 1, both_list,  0, 0);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == 2);
  tt_assert(test_policy_has_address_helper(policy, &ipv4_addr));
  tt_assert(test_policy_has_address_helper(policy, &ipv6_addr));
  addr_policy_list_free(policy);
  policy = NULL;

  /* Test that lists with duplicate entries produce the same results */
  policies_parse_exit_policy_reject_private(&policy, 1, dupl_list,  0, 0);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == 2);
  tt_assert(test_policy_has_address_helper(policy, &ipv4_addr));
  tt_assert(test_policy_has_address_helper(policy, &ipv6_addr));
  addr_policy_list_free(policy);
  policy = NULL;

 done:
  addr_policy_list_free(policy);
  smartlist_free(ipv4_list);
  smartlist_free(ipv6_list);
  smartlist_free(both_list);
  smartlist_free(dupl_list);
}

static smartlist_t *test_configured_ports = NULL;

/** Returns test_configured_ports */
static const smartlist_t *
mock_get_configured_ports(void)
{
  return test_configured_ports;
}

/** Run unit tests for rejecting publicly routable configured port addresses
 * on this exit relay using policies_parse_exit_policy_reject_private */
static void
test_policies_reject_port_address(void *arg)
{
  smartlist_t *policy = NULL;
  port_cfg_t *ipv4_port = NULL;
  port_cfg_t *ipv6_port = NULL;
  (void)arg;

  test_configured_ports = smartlist_new();

  ipv4_port = port_cfg_new(0);
  tor_addr_from_ipv4h(&ipv4_port->addr, TEST_IPV4_ADDR);
  smartlist_add(test_configured_ports, ipv4_port);

  ipv6_port = port_cfg_new(0);
  tor_addr_parse(&ipv6_port->addr, TEST_IPV6_ADDR);
  smartlist_add(test_configured_ports, ipv6_port);

  MOCK(get_configured_ports, mock_get_configured_ports);

  /* test that an IPv4 port is rejected on an IPv4-only exit, but an IPv6 port
   * is NOT rejected (all IPv6 addresses are rejected by
   * policies_parse_exit_policy_internal on IPv4-only exits, so
   * policies_parse_exit_policy_reject_private doesn't need to do anything
   * with IPv6 addresses on IPv4-only exits) */
  policies_parse_exit_policy_reject_private(&policy, 0, NULL, 0, 1);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == 1);
  tt_assert(test_policy_has_address_helper(policy, &ipv4_port->addr));
  addr_policy_list_free(policy);
  policy = NULL;

  /* test that IPv4 and IPv6 ports are rejected on an IPv4/IPv6 exit */
  policies_parse_exit_policy_reject_private(&policy, 1, NULL, 0, 1);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == 2);
  tt_assert(test_policy_has_address_helper(policy, &ipv4_port->addr));
  tt_assert(test_policy_has_address_helper(policy, &ipv6_port->addr));
  addr_policy_list_free(policy);
  policy = NULL;

 done:
  addr_policy_list_free(policy);
  if (test_configured_ports) {
    SMARTLIST_FOREACH(test_configured_ports,
                      port_cfg_t *, p, port_cfg_free(p));
    smartlist_free(test_configured_ports);
    test_configured_ports = NULL;
  }
  UNMOCK(get_configured_ports);
}

static smartlist_t *mock_ipv4_addrs = NULL;
static smartlist_t *mock_ipv6_addrs = NULL;

/* mock get_interface_address6_list, returning a deep copy of the template
 * address list ipv4_interface_address_list or ipv6_interface_address_list */
static smartlist_t *
mock_get_interface_address6_list(int severity,
                            sa_family_t family,
                            int include_internal)
{
  (void)severity;
  (void)include_internal;
  smartlist_t *clone_list = smartlist_new();
  smartlist_t *template_list = NULL;

  if (family == AF_INET) {
    template_list = mock_ipv4_addrs;
  } else if (family == AF_INET6) {
    template_list = mock_ipv6_addrs;
  } else {
    return NULL;
  }

  tt_assert(template_list);

  SMARTLIST_FOREACH_BEGIN(template_list, tor_addr_t *, src_addr) {
    tor_addr_t *dest_addr = tor_malloc(sizeof(tor_addr_t));
    memset(dest_addr, 0, sizeof(*dest_addr));
    tor_addr_copy_tight(dest_addr, src_addr);
    smartlist_add(clone_list, dest_addr);
  } SMARTLIST_FOREACH_END(src_addr);

  return clone_list;

 done:
  free_interface_address6_list(clone_list);
  return NULL;
}

/** Run unit tests for rejecting publicly routable interface addresses on this
 * exit relay using policies_parse_exit_policy_reject_private */
static void
test_policies_reject_interface_address(void *arg)
{
  smartlist_t *policy = NULL;
  smartlist_t *public_ipv4_addrs =
    get_interface_address6_list(LOG_INFO, AF_INET, 0);
  smartlist_t *public_ipv6_addrs =
    get_interface_address6_list(LOG_INFO, AF_INET6, 0);
  tor_addr_t ipv4_addr, ipv6_addr;
  (void)arg;

  /* test that no addresses are rejected when none are supplied/requested */
  policies_parse_exit_policy_reject_private(&policy, 0, NULL, 0, 0);
  tt_assert(policy == NULL);

  /* test that only IPv4 interface addresses are rejected on an IPv4-only exit
   * (and allow for duplicates)
   */
  policies_parse_exit_policy_reject_private(&policy, 0, NULL, 1, 0);
  if (policy) {
    tt_assert(smartlist_len(policy) <= smartlist_len(public_ipv4_addrs));
    addr_policy_list_free(policy);
    policy = NULL;
  }

  /* test that IPv4 and IPv6 interface addresses are rejected on an IPv4/IPv6
   * exit (and allow for duplicates) */
  policies_parse_exit_policy_reject_private(&policy, 1, NULL, 1, 0);
  if (policy) {
    tt_assert(smartlist_len(policy) <= (smartlist_len(public_ipv4_addrs)
                                        + smartlist_len(public_ipv6_addrs)));
    addr_policy_list_free(policy);
    policy = NULL;
  }

  /* Now do it all again, but mocked */
  tor_addr_from_ipv4h(&ipv4_addr, TEST_IPV4_ADDR);
  mock_ipv4_addrs = smartlist_new();
  smartlist_add(mock_ipv4_addrs, (void *)&ipv4_addr);

  tor_addr_parse(&ipv6_addr, TEST_IPV6_ADDR);
  mock_ipv6_addrs = smartlist_new();
  smartlist_add(mock_ipv6_addrs, (void *)&ipv6_addr);

  MOCK(get_interface_address6_list, mock_get_interface_address6_list);

  /* test that no addresses are rejected when none are supplied/requested */
  policies_parse_exit_policy_reject_private(&policy, 0, NULL, 0, 0);
  tt_assert(policy == NULL);

  /* test that only IPv4 interface addresses are rejected on an IPv4-only exit
   */
  policies_parse_exit_policy_reject_private(&policy, 0, NULL, 1, 0);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == smartlist_len(mock_ipv4_addrs));
  addr_policy_list_free(policy);
  policy = NULL;

  /* test that IPv4 and IPv6 interface addresses are rejected on an IPv4/IPv6
   * exit */
  policies_parse_exit_policy_reject_private(&policy, 1, NULL, 1, 0);
  tt_assert(policy);
  tt_assert(smartlist_len(policy) == (smartlist_len(mock_ipv4_addrs)
                                      + smartlist_len(mock_ipv6_addrs)));
  addr_policy_list_free(policy);
  policy = NULL;

 done:
  addr_policy_list_free(policy);
  free_interface_address6_list(public_ipv4_addrs);
  free_interface_address6_list(public_ipv6_addrs);

  UNMOCK(get_interface_address6_list);
  /* we don't use free_interface_address6_list on these lists because their
   * address pointers are stack-based */
  smartlist_free(mock_ipv4_addrs);
  smartlist_free(mock_ipv6_addrs);
}

#undef TEST_IPV4_ADDR
#undef TEST_IPV6_ADDR

static void
test_dump_exit_policy_to_string(void *arg)
{
 char *ep;
 addr_policy_t *policy_entry;
 int malformed_list = -1;

 routerinfo_t *ri = tor_malloc_zero(sizeof(routerinfo_t));

 (void)arg;

 ri->policy_is_reject_star = 1;
 ri->exit_policy = NULL; // expecting "reject *:*"
 ep = router_dump_exit_policy_to_string(ri,1,1);

 tt_str_op("reject *:*",OP_EQ, ep);

 tor_free(ep);

 ri->exit_policy = smartlist_new();
 ri->policy_is_reject_star = 0;

 policy_entry = router_parse_addr_policy_item_from_string("accept *:*", -1,
                                                          &malformed_list);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 tt_str_op("accept *:*",OP_EQ, ep);

 tor_free(ep);

 policy_entry = router_parse_addr_policy_item_from_string("reject *:25", -1,
                                                          &malformed_list);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 tt_str_op("accept *:*\nreject *:25",OP_EQ, ep);

 tor_free(ep);

 policy_entry =
 router_parse_addr_policy_item_from_string("reject 8.8.8.8:*", -1,
                                           &malformed_list);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 tt_str_op("accept *:*\nreject *:25\nreject 8.8.8.8:*",OP_EQ, ep);
 tor_free(ep);

 policy_entry =
 router_parse_addr_policy_item_from_string("reject6 [FC00::]/7:*", -1,
                                           &malformed_list);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 tt_str_op("accept *:*\nreject *:25\nreject 8.8.8.8:*\n"
            "reject6 [fc00::]/7:*",OP_EQ, ep);
 tor_free(ep);

 policy_entry =
 router_parse_addr_policy_item_from_string("accept6 [c000::]/3:*", -1,
                                           &malformed_list);

 smartlist_add(ri->exit_policy,policy_entry);

 ep = router_dump_exit_policy_to_string(ri,1,1);

 tt_str_op("accept *:*\nreject *:25\nreject 8.8.8.8:*\n"
            "reject6 [fc00::]/7:*\naccept6 [c000::]/3:*",OP_EQ, ep);

 done:

 if (ri->exit_policy) {
   SMARTLIST_FOREACH(ri->exit_policy, addr_policy_t *,
                     entry, addr_policy_free(entry));
   smartlist_free(ri->exit_policy);
 }
 tor_free(ri);
 tor_free(ep);
}

static routerinfo_t *mock_desc_routerinfo = NULL;
static const routerinfo_t *
mock_router_get_my_routerinfo(void)
{
  return mock_desc_routerinfo;
}

#define DEFAULT_POLICY_STRING "reject *:*"
#define TEST_IPV4_ADDR (0x02040608)
#define TEST_IPV6_ADDR ("2003::ef01")

static or_options_t mock_options;

static const or_options_t *
mock_get_options(void)
{
  return &mock_options;
}

/** Run unit tests for generating summary lines of exit policies */
static void
test_policies_getinfo_helper_policies(void *arg)
{
  (void)arg;
  int rv = 0;
  size_t ipv4_len = 0, ipv6_len = 0;
  char *answer = NULL;
  const char *errmsg = NULL;
  routerinfo_t mock_my_routerinfo;

  memset(&mock_my_routerinfo, 0, sizeof(mock_my_routerinfo));

  rv = getinfo_helper_policies(NULL, "exit-policy/default", &answer, &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  tt_assert(strlen(answer) > 0);
  tor_free(answer);

  rv = getinfo_helper_policies(NULL, "exit-policy/reject-private/default",
                               &answer, &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  tt_assert(strlen(answer) > 0);
  tor_free(answer);

  memset(&mock_my_routerinfo, 0, sizeof(routerinfo_t));
  MOCK(router_get_my_routerinfo, mock_router_get_my_routerinfo);
  mock_my_routerinfo.exit_policy = smartlist_new();
  mock_desc_routerinfo = &mock_my_routerinfo;

  memset(&mock_options, 0, sizeof(or_options_t));
  MOCK(get_options, mock_get_options);

  rv = getinfo_helper_policies(NULL, "exit-policy/reject-private/relay",
                               &answer, &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  tt_assert(strlen(answer) == 0);
  tor_free(answer);

  rv = getinfo_helper_policies(NULL, "exit-policy/ipv4", &answer,
                               &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  ipv4_len = strlen(answer);
  tt_assert(ipv4_len == 0 || ipv4_len == strlen(DEFAULT_POLICY_STRING));
  tt_assert(ipv4_len == 0 || !strcasecmp(answer, DEFAULT_POLICY_STRING));
  tor_free(answer);

  rv = getinfo_helper_policies(NULL, "exit-policy/ipv6", &answer,
                               &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  ipv6_len = strlen(answer);
  tt_assert(ipv6_len == 0 || ipv6_len == strlen(DEFAULT_POLICY_STRING));
  tt_assert(ipv6_len == 0 || !strcasecmp(answer, DEFAULT_POLICY_STRING));
  tor_free(answer);

  rv = getinfo_helper_policies(NULL, "exit-policy/full", &answer,
                               &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  /* It's either empty or it's the default */
  tt_assert(strlen(answer) == 0 || !strcasecmp(answer, DEFAULT_POLICY_STRING));
  tor_free(answer);

  mock_my_routerinfo.addr = TEST_IPV4_ADDR;
  tor_addr_parse(&mock_my_routerinfo.ipv6_addr, TEST_IPV6_ADDR);
  append_exit_policy_string(&mock_my_routerinfo.exit_policy, "accept *4:*");
  append_exit_policy_string(&mock_my_routerinfo.exit_policy, "reject *6:*");

  mock_options.IPv6Exit = 1;
  tor_addr_from_ipv4h(&mock_options.OutboundBindAddressIPv4_, TEST_IPV4_ADDR);
  tor_addr_parse(&mock_options.OutboundBindAddressIPv6_, TEST_IPV6_ADDR);

  mock_options.ExitPolicyRejectPrivate = 1;
  mock_options.ExitPolicyRejectLocalInterfaces = 1;

  rv = getinfo_helper_policies(NULL, "exit-policy/reject-private/relay",
                               &answer, &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  tt_assert(strlen(answer) > 0);
  tor_free(answer);

  mock_options.ExitPolicyRejectPrivate = 1;
  mock_options.ExitPolicyRejectLocalInterfaces = 0;

  rv = getinfo_helper_policies(NULL, "exit-policy/reject-private/relay",
                               &answer, &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  tt_assert(strlen(answer) > 0);
  tor_free(answer);

  mock_options.ExitPolicyRejectPrivate = 0;
  mock_options.ExitPolicyRejectLocalInterfaces = 1;

  rv = getinfo_helper_policies(NULL, "exit-policy/reject-private/relay",
                               &answer, &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  tt_assert(strlen(answer) > 0);
  tor_free(answer);

  mock_options.ExitPolicyRejectPrivate = 0;
  mock_options.ExitPolicyRejectLocalInterfaces = 0;

  rv = getinfo_helper_policies(NULL, "exit-policy/reject-private/relay",
                               &answer, &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  tt_assert(strlen(answer) == 0);
  tor_free(answer);

  rv = getinfo_helper_policies(NULL, "exit-policy/ipv4", &answer,
                               &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  ipv4_len = strlen(answer);
  tt_assert(ipv4_len > 0);
  tor_free(answer);

  rv = getinfo_helper_policies(NULL, "exit-policy/ipv6", &answer,
                               &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  ipv6_len = strlen(answer);
  tt_assert(ipv6_len > 0);
  tor_free(answer);

  rv = getinfo_helper_policies(NULL, "exit-policy/full", &answer,
                               &errmsg);
  tt_assert(rv == 0);
  tt_assert(answer != NULL);
  tt_assert(strlen(answer) > 0);
  tt_assert(strlen(answer) == ipv4_len + ipv6_len + 1);
  tor_free(answer);

 done:
  tor_free(answer);
  UNMOCK(get_options);
  UNMOCK(router_get_my_routerinfo);
  addr_policy_list_free(mock_my_routerinfo.exit_policy);
}

#undef DEFAULT_POLICY_STRING
#undef TEST_IPV4_ADDR
#undef TEST_IPV6_ADDR

#define TEST_IPV4_ADDR_STR "1.2.3.4"
#define TEST_IPV6_ADDR_STR "[1002::4567]"
#define REJECT_IPv4_FINAL_STR "reject 0.0.0.0/0:*"
#define REJECT_IPv6_FINAL_STR "reject [::]/0:*"

#define OTHER_IPV4_ADDR_STR "6.7.8.9"
#define OTHER_IPV6_ADDR_STR "[afff::]"

/** Run unit tests for fascist_firewall_allows_address */
static void
test_policies_fascist_firewall_allows_address(void *arg)
{
  (void)arg;
  tor_addr_t ipv4_addr, ipv6_addr, r_ipv4_addr, r_ipv6_addr;
  tor_addr_t n_ipv4_addr, n_ipv6_addr;
  const uint16_t port = 1234;
  smartlist_t *policy = NULL;
  smartlist_t *e_policy = NULL;
  addr_policy_t *item = NULL;
  int malformed_list = 0;

  /* Setup the options and the items in the policies */
  memset(&mock_options, 0, sizeof(or_options_t));
  MOCK(get_options, mock_get_options);

  policy = smartlist_new();
  item = router_parse_addr_policy_item_from_string("accept "
                                                   TEST_IPV4_ADDR_STR ":*",
                                                   ADDR_POLICY_ACCEPT,
                                                   &malformed_list);
  tt_assert(item);
  tt_assert(!malformed_list);
  smartlist_add(policy, item);
  item = router_parse_addr_policy_item_from_string("accept "
                                                   TEST_IPV6_ADDR_STR,
                                                   ADDR_POLICY_ACCEPT,
                                                   &malformed_list);
  tt_assert(item);
  tt_assert(!malformed_list);
  smartlist_add(policy, item);
  /* Normally, policy_expand_unspec would do this for us */
  item = router_parse_addr_policy_item_from_string(REJECT_IPv4_FINAL_STR,
                                                   ADDR_POLICY_ACCEPT,
                                                   &malformed_list);
  tt_assert(item);
  tt_assert(!malformed_list);
  smartlist_add(policy, item);
  item = router_parse_addr_policy_item_from_string(REJECT_IPv6_FINAL_STR,
                                                   ADDR_POLICY_ACCEPT,
                                                   &malformed_list);
  tt_assert(item);
  tt_assert(!malformed_list);
  smartlist_add(policy, item);
  item = NULL;

  e_policy = smartlist_new();

  /*
  char *polstr = policy_dump_to_string(policy, 1, 1);
  printf("%s\n", polstr);
  tor_free(polstr);
   */

  /* Parse the addresses */
  tor_addr_parse(&ipv4_addr, TEST_IPV4_ADDR_STR);
  tor_addr_parse(&ipv6_addr, TEST_IPV6_ADDR_STR);
  tor_addr_parse(&r_ipv4_addr, OTHER_IPV4_ADDR_STR);
  tor_addr_parse(&r_ipv6_addr, OTHER_IPV6_ADDR_STR);
  tor_addr_make_null(&n_ipv4_addr, AF_INET);
  tor_addr_make_null(&n_ipv6_addr, AF_INET6);

  /* Test the function's address matching with IPv4 and IPv6 on */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 1;
  mock_options.ClientUseIPv6 = 1;
  mock_options.UseBridges = 0;

  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 0, 0)
            == 0);

  /* Preferring IPv4 */
  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 1, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 1, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 1, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 1, 0)
            == 0);

  /* Preferring IPv6 */
  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 1, 1)
            == 0);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 1, 1)
            == 1);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 1, 1)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 1, 1)
            == 0);

  /* Test the function's address matching with UseBridges on */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 1;
  mock_options.ClientUseIPv6 = 1;
  mock_options.UseBridges = 1;

  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 0, 0)
            == 0);

  /* Preferring IPv4 */
  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 1, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 1, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 1, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 1, 0)
            == 0);

  /* Preferring IPv6 */
  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 1, 1)
            == 0);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 1, 1)
            == 1);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 1, 1)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 1, 1)
            == 0);

  /* bridge clients always use IPv6, regardless of ClientUseIPv6 */
  mock_options.ClientUseIPv4 = 1;
  mock_options.ClientUseIPv6 = 0;
  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 0, 0)
            == 0);

  /* Test the function's address matching with IPv4 on */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 1;
  mock_options.ClientUseIPv6 = 0;
  mock_options.UseBridges = 0;

  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 0, 0)
            == 0);

  /* Test the function's address matching with IPv6 on */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 0;
  mock_options.ClientUseIPv6 = 1;
  mock_options.UseBridges = 0;

  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 0, 0)
            == 0);

  /* Test the function's address matching with ClientUseIPv4 0.
   * This means "use IPv6" regardless of the other settings. */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 0;
  mock_options.ClientUseIPv6 = 0;
  mock_options.UseBridges = 0;

  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&r_ipv4_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&r_ipv6_addr, port, policy, 0, 0)
            == 0);

  /* Test the function's address matching for unusual inputs */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 1;
  mock_options.ClientUseIPv6 = 1;
  mock_options.UseBridges = 1;

  /* NULL and tor_addr_is_null addresses are rejected */
  tt_assert(fascist_firewall_allows_address(NULL, port, policy, 0, 0) == 0);
  tt_assert(fascist_firewall_allows_address(&n_ipv4_addr, port, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&n_ipv6_addr, port, policy, 0, 0)
            == 0);

  /* zero ports are rejected */
  tt_assert(fascist_firewall_allows_address(&ipv4_addr, 0, policy, 0, 0)
            == 0);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, 0, policy, 0, 0)
            == 0);

  /* NULL and empty policies accept everything */
  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, NULL, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, NULL, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&ipv4_addr, port, e_policy, 0, 0)
            == 1);
  tt_assert(fascist_firewall_allows_address(&ipv6_addr, port, e_policy, 0, 0)
            == 1);

 done:
  addr_policy_free(item);
  addr_policy_list_free(policy);
  addr_policy_list_free(e_policy);
  UNMOCK(get_options);
}

#undef REJECT_IPv4_FINAL_STR
#undef REJECT_IPv6_FINAL_STR
#undef OTHER_IPV4_ADDR_STR
#undef OTHER_IPV6_ADDR_STR

#define TEST_IPV4_OR_PORT  1234
#define TEST_IPV4_DIR_PORT 2345
#define TEST_IPV6_OR_PORT  61234
#define TEST_IPV6_DIR_PORT 62345

/* Check that fascist_firewall_choose_address_rs() returns the expected
 * results. */
#define CHECK_CHOSEN_ADDR_RS(fake_rs, fw_connection, pref_only, expect_rv, \
                             expect_ap) \
  STMT_BEGIN \
    tor_addr_port_t chosen_rs_ap; \
    tor_addr_make_null(&chosen_rs_ap.addr, AF_INET); \
    chosen_rs_ap.port = 0; \
    tt_int_op(fascist_firewall_choose_address_rs(&(fake_rs), \
                                                 (fw_connection), \
                                                 (pref_only), \
                                                 &chosen_rs_ap), \
              OP_EQ, (expect_rv)); \
    tt_assert(tor_addr_eq(&(expect_ap).addr, &chosen_rs_ap.addr)); \
    tt_int_op((expect_ap).port, OP_EQ, chosen_rs_ap.port); \
  STMT_END

/* Check that fascist_firewall_choose_address_node() returns the expected
 * results. */
#define CHECK_CHOSEN_ADDR_NODE(fake_node, fw_connection, pref_only, \
                               expect_rv, expect_ap) \
  STMT_BEGIN \
    tor_addr_port_t chosen_node_ap; \
    tor_addr_make_null(&chosen_node_ap.addr, AF_INET); \
    chosen_node_ap.port = 0; \
    tt_int_op(fascist_firewall_choose_address_node(&(fake_node), \
                                                   (fw_connection), \
                                                   (pref_only), \
                                                   &chosen_node_ap), \
              OP_EQ, (expect_rv)); \
    tt_assert(tor_addr_eq(&(expect_ap).addr, &chosen_node_ap.addr)); \
    tt_int_op((expect_ap).port, OP_EQ, chosen_node_ap.port); \
  STMT_END

/* Check that fascist_firewall_choose_address_rs and
 * fascist_firewall_choose_address_node() both return the expected results. */
#define CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, fw_connection, pref_only, \
                             expect_rv, expect_ap) \
  STMT_BEGIN \
    CHECK_CHOSEN_ADDR_RS(fake_rs, fw_connection, pref_only, expect_rv, \
                         expect_ap); \
    CHECK_CHOSEN_ADDR_NODE(fake_node, fw_connection, pref_only, expect_rv, \
                           expect_ap); \
  STMT_END

/** Run unit tests for fascist_firewall_choose_address */
static void
test_policies_fascist_firewall_choose_address(void *arg)
{
  (void)arg;
  tor_addr_port_t ipv4_or_ap, ipv4_dir_ap, ipv6_or_ap, ipv6_dir_ap;
  tor_addr_port_t n_ipv4_ap, n_ipv6_ap;

  /* Setup the options */
  memset(&mock_options, 0, sizeof(or_options_t));
  MOCK(get_options, mock_get_options);

  /* Parse the addresses */
  tor_addr_parse(&ipv4_or_ap.addr, TEST_IPV4_ADDR_STR);
  ipv4_or_ap.port = TEST_IPV4_OR_PORT;
  tor_addr_parse(&ipv4_dir_ap.addr, TEST_IPV4_ADDR_STR);
  ipv4_dir_ap.port = TEST_IPV4_DIR_PORT;

  tor_addr_parse(&ipv6_or_ap.addr, TEST_IPV6_ADDR_STR);
  ipv6_or_ap.port = TEST_IPV6_OR_PORT;
  tor_addr_parse(&ipv6_dir_ap.addr, TEST_IPV6_ADDR_STR);
  ipv6_dir_ap.port = TEST_IPV6_DIR_PORT;

  tor_addr_make_null(&n_ipv4_ap.addr, AF_INET);
  n_ipv4_ap.port = 0;
  tor_addr_make_null(&n_ipv6_ap.addr, AF_INET6);
  n_ipv6_ap.port = 0;

  /* Sanity check fascist_firewall_choose_address with IPv4 and IPv6 on */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 1;
  mock_options.ClientUseIPv6 = 1;
  mock_options.UseBridges = 0;

  /* Prefer IPv4 */
  tt_assert(fascist_firewall_choose_address(&ipv4_or_ap, &ipv6_or_ap, 1,
                                            FIREWALL_OR_CONNECTION, 0, 0)
            == &ipv4_or_ap);
  tt_assert(fascist_firewall_choose_address(&ipv4_or_ap, &ipv6_or_ap, 1,
                                            FIREWALL_OR_CONNECTION, 1, 0)
            == &ipv4_or_ap);
  tt_assert(fascist_firewall_choose_address(&ipv4_dir_ap, &ipv6_dir_ap, 1,
                                            FIREWALL_DIR_CONNECTION, 0, 0)
            == &ipv4_dir_ap);
  tt_assert(fascist_firewall_choose_address(&ipv4_dir_ap, &ipv6_dir_ap, 1,
                                            FIREWALL_DIR_CONNECTION, 1, 0)
            == &ipv4_dir_ap);

  /* Prefer IPv6 */
  tt_assert(fascist_firewall_choose_address(&ipv4_or_ap, &ipv6_or_ap, 0,
                                            FIREWALL_OR_CONNECTION, 0, 1)
            == &ipv6_or_ap);
  tt_assert(fascist_firewall_choose_address(&ipv4_or_ap, &ipv6_or_ap, 0,
                                            FIREWALL_OR_CONNECTION, 1, 1)
            == &ipv6_or_ap);
  tt_assert(fascist_firewall_choose_address(&ipv4_dir_ap, &ipv6_dir_ap, 0,
                                            FIREWALL_DIR_CONNECTION, 0, 1)
            == &ipv6_dir_ap);
  tt_assert(fascist_firewall_choose_address(&ipv4_dir_ap, &ipv6_dir_ap, 0,
                                            FIREWALL_DIR_CONNECTION, 1, 1)
            == &ipv6_dir_ap);

  /* Unusual inputs */

  /* null preferred OR addresses */
  tt_assert(fascist_firewall_choose_address(&ipv4_or_ap, &n_ipv6_ap, 0,
                                            FIREWALL_OR_CONNECTION, 0, 1)
            == &ipv4_or_ap);
  tt_assert(fascist_firewall_choose_address(&n_ipv4_ap, &ipv6_or_ap, 1,
                                            FIREWALL_OR_CONNECTION, 0, 0)
            == &ipv6_or_ap);

  /* null both OR addresses */
  tt_assert(fascist_firewall_choose_address(&n_ipv4_ap, &n_ipv6_ap, 0,
                                            FIREWALL_OR_CONNECTION, 0, 1)
            == NULL);
  tt_assert(fascist_firewall_choose_address(&n_ipv4_ap, &n_ipv6_ap, 1,
                                            FIREWALL_OR_CONNECTION, 0, 0)
            == NULL);

  /* null preferred Dir addresses */
  tt_assert(fascist_firewall_choose_address(&ipv4_dir_ap, &n_ipv6_ap, 0,
                                            FIREWALL_DIR_CONNECTION, 0, 1)
            == &ipv4_dir_ap);
  tt_assert(fascist_firewall_choose_address(&n_ipv4_ap, &ipv6_dir_ap, 1,
                                            FIREWALL_DIR_CONNECTION, 0, 0)
            == &ipv6_dir_ap);

  /* null both Dir addresses */
  tt_assert(fascist_firewall_choose_address(&n_ipv4_ap, &n_ipv6_ap, 0,
                                            FIREWALL_DIR_CONNECTION, 0, 1)
            == NULL);
  tt_assert(fascist_firewall_choose_address(&n_ipv4_ap, &n_ipv6_ap, 1,
                                            FIREWALL_DIR_CONNECTION, 0, 0)
            == NULL);

  /* Prefer IPv4 but want IPv6 (contradictory) */
  tt_assert(fascist_firewall_choose_address(&ipv4_or_ap, &ipv6_or_ap, 0,
                                            FIREWALL_OR_CONNECTION, 0, 0)
            == &ipv4_or_ap);
  tt_assert(fascist_firewall_choose_address(&ipv4_or_ap, &ipv6_or_ap, 0,
                                            FIREWALL_OR_CONNECTION, 1, 0)
            == &ipv4_or_ap);

  /* Prefer IPv6 but want IPv4 (contradictory) */
  tt_assert(fascist_firewall_choose_address(&ipv4_or_ap, &ipv6_or_ap, 1,
                                            FIREWALL_OR_CONNECTION, 0, 1)
            == &ipv6_or_ap);
  tt_assert(fascist_firewall_choose_address(&ipv4_or_ap, &ipv6_or_ap, 1,
                                            FIREWALL_OR_CONNECTION, 1, 1)
            == &ipv6_or_ap);

  /* Make a fake rs. There will be no corresponding node.
   * This is what happens when there's no consensus and we're bootstrapping
   * from authorities / fallbacks. */
  routerstatus_t fake_rs;
  memset(&fake_rs, 0, sizeof(routerstatus_t));
  /* In a routerstatus, the OR and Dir addresses are the same */
  fake_rs.addr = tor_addr_to_ipv4h(&ipv4_or_ap.addr);
  fake_rs.or_port = ipv4_or_ap.port;
  fake_rs.dir_port = ipv4_dir_ap.port;

  tor_addr_copy(&fake_rs.ipv6_addr, &ipv6_or_ap.addr);
  fake_rs.ipv6_orport = ipv6_or_ap.port;
  /* In a routerstatus, the IPv4 and IPv6 DirPorts are the same.*/
  ipv6_dir_ap.port = TEST_IPV4_DIR_PORT;

  /* Make a fake node. Even though it contains the fake_rs, a lookup won't
  * find the node from the rs, because they're not in the hash table. */
  node_t fake_node;
  memset(&fake_node, 0, sizeof(node_t));
  fake_node.rs = &fake_rs;

  /* Choose an address with IPv4 and IPv6 on */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 1;
  mock_options.ClientUseIPv6 = 1;
  mock_options.UseBridges = 0;

  /* Preferring IPv4 */
  mock_options.ClientPreferIPv6ORPort = 0;
  mock_options.ClientPreferIPv6DirPort = 0;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv4_dir_ap);

  /* Auto (Preferring IPv4) */
  mock_options.ClientPreferIPv6ORPort = -1;
  mock_options.ClientPreferIPv6DirPort = -1;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv4_dir_ap);

  /* Preferring IPv6 */
  mock_options.ClientPreferIPv6ORPort = 1;
  mock_options.ClientPreferIPv6DirPort = 1;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv6_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv6_dir_ap);

  /* Preferring IPv4 OR / IPv6 Dir */
  mock_options.ClientPreferIPv6ORPort = 0;
  mock_options.ClientPreferIPv6DirPort = 1;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv6_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv6_dir_ap);

  /* Preferring IPv6 OR / IPv4 Dir */
  mock_options.ClientPreferIPv6ORPort = 1;
  mock_options.ClientPreferIPv6DirPort = 0;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv4_dir_ap);

  /* Choose an address with UseBridges on */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.UseBridges = 1;
  mock_options.ClientUseIPv4 = 1;
  mock_options.ClientUseIPv6 = 1;

  /* Preferring IPv4 */
  mock_options.ClientPreferIPv6ORPort = 0;
  mock_options.ClientPreferIPv6DirPort = 0;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv4_dir_ap);

  /* Auto:
   * - bridge clients prefer the configured bridge OR address from the node,
   *   (the configured address family sets node.ipv6_preferred)
   * - other clients prefer IPv4 OR by default (see above),
   * - all clients, including bridge clients, prefer IPv4 Dir by default.
   */
  mock_options.ClientPreferIPv6ORPort = -1;
  mock_options.ClientPreferIPv6DirPort = -1;

  /* Simulate the initialisation of fake_node.ipv6_preferred with a bridge
   * configured with an IPv4 address */
  fake_node.ipv6_preferred = 0;
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_OR_CONNECTION, 0, 1, ipv4_or_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_OR_CONNECTION, 1, 1, ipv4_or_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                         ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                         ipv4_dir_ap);

  /* Simulate the initialisation of fake_node.ipv6_preferred with a bridge
   * configured with an IPv6 address */
  fake_node.ipv6_preferred = 1;
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_OR_CONNECTION, 0, 1, ipv6_or_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_OR_CONNECTION, 1, 1, ipv6_or_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                         ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                         ipv4_dir_ap);

  /* When a rs has no node, it defaults to IPv4 under auto. */
  CHECK_CHOSEN_ADDR_RS(fake_rs, FIREWALL_OR_CONNECTION, 0, 1, ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RS(fake_rs, FIREWALL_OR_CONNECTION, 1, 1, ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RS(fake_rs, FIREWALL_DIR_CONNECTION, 0, 1, ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_RS(fake_rs, FIREWALL_DIR_CONNECTION, 1, 1, ipv4_dir_ap);

  /* Preferring IPv6 */
  mock_options.ClientPreferIPv6ORPort = 1;
  mock_options.ClientPreferIPv6DirPort = 1;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv6_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv6_dir_ap);

  /* In the default configuration (Auto / IPv6 off), bridge clients should
   * use both IPv4 and IPv6, but only prefer IPv6 for bridges configured with
   * an IPv6 address, regardless of ClientUseIPv6. (See above.) */
  mock_options.ClientUseIPv6 = 0;
  mock_options.ClientPreferIPv6ORPort = -1;
  mock_options.ClientPreferIPv6DirPort = -1;
  /* Simulate the initialisation of fake_node.ipv6_preferred with a bridge
   * configured with an IPv4 address */
  fake_node.ipv6_preferred = 0;
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_OR_CONNECTION, 0, 1, ipv4_or_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_OR_CONNECTION, 1, 1, ipv4_or_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                         ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                         ipv4_dir_ap);

  /* Simulate the initialisation of fake_node.ipv6_preferred with a bridge
   * configured with an IPv6 address */
  fake_node.ipv6_preferred = 1;
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_OR_CONNECTION, 0, 1, ipv6_or_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_OR_CONNECTION, 1, 1, ipv6_or_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                         ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_NODE(fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                         ipv4_dir_ap);

  /* When a rs has no node, it defaults to IPv4 under auto. */
  CHECK_CHOSEN_ADDR_RS(fake_rs, FIREWALL_OR_CONNECTION, 0, 1, ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RS(fake_rs, FIREWALL_OR_CONNECTION, 1, 1, ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RS(fake_rs, FIREWALL_DIR_CONNECTION, 0, 1, ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_RS(fake_rs, FIREWALL_DIR_CONNECTION, 1, 1, ipv4_dir_ap);

  /* Choose an address with IPv4 on */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 1;
  mock_options.ClientUseIPv6 = 0;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv4_dir_ap);

  /* Choose an address with IPv6 on */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 0;
  mock_options.ClientUseIPv6 = 1;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv6_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv6_dir_ap);

  /* Choose an address with ClientUseIPv4 0.
   * This means "use IPv6" regardless of the other settings. */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ClientUseIPv4 = 0;
  mock_options.ClientUseIPv6 = 0;
  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv6_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv6_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv6_dir_ap);

  /* Choose an address with ORPort_set 1 (server mode).
   * This means "use IPv4" regardless of the other settings. */
  memset(&mock_options, 0, sizeof(or_options_t));
  mock_options.ORPort_set = 1;
  mock_options.ClientUseIPv4 = 0;
  mock_options.ClientUseIPv6 = 1;
  mock_options.ClientPreferIPv6ORPort = 1;
  mock_options.ClientPreferIPv6DirPort = 1;

  /* Simulate the initialisation of fake_node.ipv6_preferred */
  fake_node.ipv6_preferred = fascist_firewall_prefer_ipv6_orport(
                                                                &mock_options);

  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 0, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_OR_CONNECTION, 1, 1,
                       ipv4_or_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 0, 1,
                       ipv4_dir_ap);
  CHECK_CHOSEN_ADDR_RN(fake_rs, fake_node, FIREWALL_DIR_CONNECTION, 1, 1,
                       ipv4_dir_ap);

 done:
  UNMOCK(get_options);
}

#undef TEST_IPV4_ADDR_STR
#undef TEST_IPV6_ADDR_STR
#undef TEST_IPV4_OR_PORT
#undef TEST_IPV4_DIR_PORT
#undef TEST_IPV6_OR_PORT
#undef TEST_IPV6_DIR_PORT

#undef CHECK_CHOSEN_ADDR_RS
#undef CHECK_CHOSEN_ADDR_NODE
#undef CHECK_CHOSEN_ADDR_RN

struct testcase_t policy_tests[] = {
  { "router_dump_exit_policy_to_string", test_dump_exit_policy_to_string, 0,
    NULL, NULL },
  { "general", test_policies_general, 0, NULL, NULL },
  { "getinfo_helper_policies", test_policies_getinfo_helper_policies, 0, NULL,
    NULL },
  { "reject_exit_address", test_policies_reject_exit_address, 0, NULL, NULL },
  { "reject_interface_address", test_policies_reject_interface_address, 0,
    NULL, NULL },
  { "reject_port_address", test_policies_reject_port_address, 0, NULL, NULL },
  { "fascist_firewall_allows_address",
    test_policies_fascist_firewall_allows_address, 0, NULL, NULL },
  { "fascist_firewall_choose_address",
    test_policies_fascist_firewall_choose_address, 0, NULL, NULL },
  END_OF_TESTCASES
};

