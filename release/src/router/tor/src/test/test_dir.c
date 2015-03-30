/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include <math.h>

#define DIRSERV_PRIVATE
#define DIRVOTE_PRIVATE
#define ROUTER_PRIVATE
#define ROUTERLIST_PRIVATE
#define HIBERNATE_PRIVATE
#define NETWORKSTATUS_PRIVATE
#include "or.h"
#include "config.h"
#include "directory.h"
#include "dirserv.h"
#include "dirvote.h"
#include "hibernate.h"
#include "networkstatus.h"
#include "router.h"
#include "routerlist.h"
#include "routerparse.h"
#include "test.h"

static void
test_dir_nicknames(void)
{
  test_assert( is_legal_nickname("a"));
  test_assert(!is_legal_nickname(""));
  test_assert(!is_legal_nickname("abcdefghijklmnopqrst")); /* 20 chars */
  test_assert(!is_legal_nickname("hyphen-")); /* bad char */
  test_assert( is_legal_nickname("abcdefghijklmnopqrs")); /* 19 chars */
  test_assert(!is_legal_nickname("$AAAAAAAA01234AAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  /* valid */
  test_assert( is_legal_nickname_or_hexdigest(
                                 "$AAAAAAAA01234AAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  test_assert( is_legal_nickname_or_hexdigest(
                         "$AAAAAAAA01234AAAAAAAAAAAAAAAAAAAAAAAAAAA=fred"));
  test_assert( is_legal_nickname_or_hexdigest(
                         "$AAAAAAAA01234AAAAAAAAAAAAAAAAAAAAAAAAAAA~fred"));
  /* too short */
  test_assert(!is_legal_nickname_or_hexdigest(
                                 "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  /* illegal char */
  test_assert(!is_legal_nickname_or_hexdigest(
                                 "$AAAAAAzAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  /* hex part too long */
  test_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  test_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=fred"));
  /* Bad nickname */
  test_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="));
  test_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA~"));
  test_assert(!is_legal_nickname_or_hexdigest(
                       "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA~hyphen-"));
  test_assert(!is_legal_nickname_or_hexdigest(
                       "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA~"
                       "abcdefghijklmnoppqrst"));
  /* Bad extra char. */
  test_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA!"));
  test_assert(is_legal_nickname_or_hexdigest("xyzzy"));
  test_assert(is_legal_nickname_or_hexdigest("abcdefghijklmnopqrs"));
  test_assert(!is_legal_nickname_or_hexdigest("abcdefghijklmnopqrst"));
 done:
  ;
}

/** Run unit tests for router descriptor generation logic. */
static void
test_dir_formats(void)
{
  char *buf = NULL;
  char buf2[8192];
  char platform[256];
  char fingerprint[FINGERPRINT_LEN+1];
  char *pk1_str = NULL, *pk2_str = NULL, *cp;
  size_t pk1_str_len, pk2_str_len;
  routerinfo_t *r1=NULL, *r2=NULL;
  crypto_pk_t *pk1 = NULL, *pk2 = NULL;
  routerinfo_t *rp1 = NULL, *rp2 = NULL;
  addr_policy_t *ex1, *ex2;
  routerlist_t *dir1 = NULL, *dir2 = NULL;
  or_options_t *options = get_options_mutable();
  const addr_policy_t *p;

  pk1 = pk_generate(0);
  pk2 = pk_generate(1);

  test_assert(pk1 && pk2);

  hibernate_set_state_for_testing_(HIBERNATE_STATE_LIVE);

  get_platform_str(platform, sizeof(platform));
  r1 = tor_malloc_zero(sizeof(routerinfo_t));
  r1->addr = 0xc0a80001u; /* 192.168.0.1 */
  r1->cache_info.published_on = 0;
  r1->or_port = 9000;
  r1->dir_port = 9003;
  tor_addr_parse(&r1->ipv6_addr, "1:2:3:4::");
  r1->ipv6_orport = 9999;
  r1->onion_pkey = crypto_pk_dup_key(pk1);
  r1->identity_pkey = crypto_pk_dup_key(pk2);
  r1->bandwidthrate = 1000;
  r1->bandwidthburst = 5000;
  r1->bandwidthcapacity = 10000;
  r1->exit_policy = NULL;
  r1->nickname = tor_strdup("Magri");
  r1->platform = tor_strdup(platform);

  ex1 = tor_malloc_zero(sizeof(addr_policy_t));
  ex2 = tor_malloc_zero(sizeof(addr_policy_t));
  ex1->policy_type = ADDR_POLICY_ACCEPT;
  tor_addr_from_ipv4h(&ex1->addr, 0);
  ex1->maskbits = 0;
  ex1->prt_min = ex1->prt_max = 80;
  ex2->policy_type = ADDR_POLICY_REJECT;
  tor_addr_from_ipv4h(&ex2->addr, 18<<24);
  ex2->maskbits = 8;
  ex2->prt_min = ex2->prt_max = 24;
  r2 = tor_malloc_zero(sizeof(routerinfo_t));
  r2->addr = 0x0a030201u; /* 10.3.2.1 */
  r2->platform = tor_strdup(platform);
  r2->cache_info.published_on = 5;
  r2->or_port = 9005;
  r2->dir_port = 0;
  r2->onion_pkey = crypto_pk_dup_key(pk2);
  r2->onion_curve25519_pkey = tor_malloc_zero(sizeof(curve25519_public_key_t));
  curve25519_public_from_base64(r2->onion_curve25519_pkey,
                                "skyinAnvardNostarsNomoonNowindormistsorsnow");
  r2->identity_pkey = crypto_pk_dup_key(pk1);
  r2->bandwidthrate = r2->bandwidthburst = r2->bandwidthcapacity = 3000;
  r2->exit_policy = smartlist_new();
  smartlist_add(r2->exit_policy, ex1);
  smartlist_add(r2->exit_policy, ex2);
  r2->nickname = tor_strdup("Fred");

  test_assert(!crypto_pk_write_public_key_to_string(pk1, &pk1_str,
                                                    &pk1_str_len));
  test_assert(!crypto_pk_write_public_key_to_string(pk2 , &pk2_str,
                                                    &pk2_str_len));

  /* XXXX025 router_dump_to_string should really take this from ri.*/
  options->ContactInfo = tor_strdup("Magri White "
                                    "<magri@elsewhere.example.com>");
  buf = router_dump_router_to_string(r1, pk2);
  tor_free(options->ContactInfo);
  test_assert(buf);

  strlcpy(buf2, "router Magri 192.168.0.1 9000 0 9003\n"
          "or-address [1:2:3:4::]:9999\n"
          "platform Tor "VERSION" on ", sizeof(buf2));
  strlcat(buf2, get_uname(), sizeof(buf2));
  strlcat(buf2, "\n"
          "protocols Link 1 2 Circuit 1\n"
          "published 1970-01-01 00:00:00\n"
          "fingerprint ", sizeof(buf2));
  test_assert(!crypto_pk_get_fingerprint(pk2, fingerprint, 1));
  strlcat(buf2, fingerprint, sizeof(buf2));
  strlcat(buf2, "\nuptime 0\n"
  /* XXX the "0" above is hard-coded, but even if we made it reflect
   * uptime, that still wouldn't make it right, because the two
   * descriptors might be made on different seconds... hm. */
         "bandwidth 1000 5000 10000\n"
          "onion-key\n", sizeof(buf2));
  strlcat(buf2, pk1_str, sizeof(buf2));
  strlcat(buf2, "signing-key\n", sizeof(buf2));
  strlcat(buf2, pk2_str, sizeof(buf2));
  strlcat(buf2, "hidden-service-dir\n", sizeof(buf2));
  strlcat(buf2, "contact Magri White <magri@elsewhere.example.com>\n",
          sizeof(buf2));
  strlcat(buf2, "reject *:*\nrouter-signature\n", sizeof(buf2));
  buf[strlen(buf2)] = '\0'; /* Don't compare the sig; it's never the same
                             * twice */

  test_streq(buf, buf2);
  tor_free(buf);

  buf = router_dump_router_to_string(r1, pk2);
  test_assert(buf);
  cp = buf;
  rp1 = router_parse_entry_from_string((const char*)cp,NULL,1,0,NULL);
  test_assert(rp1);
  test_eq(rp1->addr, r1->addr);
  test_eq(rp1->or_port, r1->or_port);
  //test_eq(rp1->dir_port, r1->dir_port);
  test_eq(rp1->bandwidthrate, r1->bandwidthrate);
  test_eq(rp1->bandwidthburst, r1->bandwidthburst);
  test_eq(rp1->bandwidthcapacity, r1->bandwidthcapacity);
  test_assert(crypto_pk_cmp_keys(rp1->onion_pkey, pk1) == 0);
  test_assert(crypto_pk_cmp_keys(rp1->identity_pkey, pk2) == 0);
  //test_assert(rp1->exit_policy == NULL);
  tor_free(buf);

  strlcpy(buf2,
          "router Fred 10.3.2.1 9005 0 0\n"
          "platform Tor "VERSION" on ", sizeof(buf2));
  strlcat(buf2, get_uname(), sizeof(buf2));
  strlcat(buf2, "\n"
          "protocols Link 1 2 Circuit 1\n"
          "published 1970-01-01 00:00:05\n"
          "fingerprint ", sizeof(buf2));
  test_assert(!crypto_pk_get_fingerprint(pk1, fingerprint, 1));
  strlcat(buf2, fingerprint, sizeof(buf2));
  strlcat(buf2, "\nuptime 0\n"
          "bandwidth 3000 3000 3000\n", sizeof(buf2));
  strlcat(buf2, "onion-key\n", sizeof(buf2));
  strlcat(buf2, pk2_str, sizeof(buf2));
  strlcat(buf2, "signing-key\n", sizeof(buf2));
  strlcat(buf2, pk1_str, sizeof(buf2));
  strlcat(buf2, "hidden-service-dir\n", sizeof(buf2));
#ifdef CURVE25519_ENABLED
  strlcat(buf2, "ntor-onion-key "
          "skyinAnvardNostarsNomoonNowindormistsorsnow=\n", sizeof(buf2));
#endif
  strlcat(buf2, "accept *:80\nreject 18.0.0.0/8:24\n", sizeof(buf2));
  strlcat(buf2, "router-signature\n", sizeof(buf2));

  buf = router_dump_router_to_string(r2, pk1);
  buf[strlen(buf2)] = '\0'; /* Don't compare the sig; it's never the same
                             * twice */
  test_streq(buf, buf2);
  tor_free(buf);

  buf = router_dump_router_to_string(r2, pk1);
  cp = buf;
  rp2 = router_parse_entry_from_string((const char*)cp,NULL,1,0,NULL);
  test_assert(rp2);
  test_eq(rp2->addr, r2->addr);
  test_eq(rp2->or_port, r2->or_port);
  test_eq(rp2->dir_port, r2->dir_port);
  test_eq(rp2->bandwidthrate, r2->bandwidthrate);
  test_eq(rp2->bandwidthburst, r2->bandwidthburst);
  test_eq(rp2->bandwidthcapacity, r2->bandwidthcapacity);
#ifdef CURVE25519_ENABLED
  test_memeq(rp2->onion_curve25519_pkey->public_key,
             r2->onion_curve25519_pkey->public_key,
             CURVE25519_PUBKEY_LEN);
#endif
  test_assert(crypto_pk_cmp_keys(rp2->onion_pkey, pk2) == 0);
  test_assert(crypto_pk_cmp_keys(rp2->identity_pkey, pk1) == 0);

  test_eq(smartlist_len(rp2->exit_policy), 2);

  p = smartlist_get(rp2->exit_policy, 0);
  test_eq(p->policy_type, ADDR_POLICY_ACCEPT);
  test_assert(tor_addr_is_null(&p->addr));
  test_eq(p->maskbits, 0);
  test_eq(p->prt_min, 80);
  test_eq(p->prt_max, 80);

  p = smartlist_get(rp2->exit_policy, 1);
  test_eq(p->policy_type, ADDR_POLICY_REJECT);
  test_assert(tor_addr_eq(&p->addr, &ex2->addr));
  test_eq(p->maskbits, 8);
  test_eq(p->prt_min, 24);
  test_eq(p->prt_max, 24);

#if 0
  /* Okay, now for the directories. */
  {
    fingerprint_list = smartlist_new();
    crypto_pk_get_fingerprint(pk2, buf, 1);
    add_fingerprint_to_dir("Magri", buf, fingerprint_list);
    crypto_pk_get_fingerprint(pk1, buf, 1);
    add_fingerprint_to_dir("Fred", buf, fingerprint_list);
  }

#endif
  dirserv_free_fingerprint_list();

 done:
  if (r1)
    routerinfo_free(r1);
  if (r2)
    routerinfo_free(r2);
  if (rp2)
    routerinfo_free(rp2);

  tor_free(buf);
  tor_free(pk1_str);
  tor_free(pk2_str);
  if (pk1) crypto_pk_free(pk1);
  if (pk2) crypto_pk_free(pk2);
  if (rp1) routerinfo_free(rp1);
  tor_free(dir1); /* XXXX And more !*/
  tor_free(dir2); /* And more !*/
}

static void
test_dir_versions(void)
{
  tor_version_t ver1;

  /* Try out version parsing functionality */
  test_eq(0, tor_version_parse("0.3.4pre2-cvs", &ver1));
  test_eq(0, ver1.major);
  test_eq(3, ver1.minor);
  test_eq(4, ver1.micro);
  test_eq(VER_PRE, ver1.status);
  test_eq(2, ver1.patchlevel);
  test_eq(0, tor_version_parse("0.3.4rc1", &ver1));
  test_eq(0, ver1.major);
  test_eq(3, ver1.minor);
  test_eq(4, ver1.micro);
  test_eq(VER_RC, ver1.status);
  test_eq(1, ver1.patchlevel);
  test_eq(0, tor_version_parse("1.3.4", &ver1));
  test_eq(1, ver1.major);
  test_eq(3, ver1.minor);
  test_eq(4, ver1.micro);
  test_eq(VER_RELEASE, ver1.status);
  test_eq(0, ver1.patchlevel);
  test_eq(0, tor_version_parse("1.3.4.999", &ver1));
  test_eq(1, ver1.major);
  test_eq(3, ver1.minor);
  test_eq(4, ver1.micro);
  test_eq(VER_RELEASE, ver1.status);
  test_eq(999, ver1.patchlevel);
  test_eq(0, tor_version_parse("0.1.2.4-alpha", &ver1));
  test_eq(0, ver1.major);
  test_eq(1, ver1.minor);
  test_eq(2, ver1.micro);
  test_eq(4, ver1.patchlevel);
  test_eq(VER_RELEASE, ver1.status);
  test_streq("alpha", ver1.status_tag);
  test_eq(0, tor_version_parse("0.1.2.4", &ver1));
  test_eq(0, ver1.major);
  test_eq(1, ver1.minor);
  test_eq(2, ver1.micro);
  test_eq(4, ver1.patchlevel);
  test_eq(VER_RELEASE, ver1.status);
  test_streq("", ver1.status_tag);

#define tt_versionstatus_op(vs1, op, vs2)                               \
  tt_assert_test_type(vs1,vs2,#vs1" "#op" "#vs2,version_status_t,       \
                      (val1_ op val2_),"%d",TT_EXIT_TEST_FUNCTION)
#define test_v_i_o(val, ver, lst)                                       \
  tt_versionstatus_op(val, ==, tor_version_is_obsolete(ver, lst))

  /* make sure tor_version_is_obsolete() works */
  test_v_i_o(VS_OLD, "0.0.1", "Tor 0.0.2");
  test_v_i_o(VS_OLD, "0.0.1", "0.0.2, Tor 0.0.3");
  test_v_i_o(VS_OLD, "0.0.1", "0.0.2,Tor 0.0.3");
  test_v_i_o(VS_OLD, "0.0.1","0.0.3,BetterTor 0.0.1");
  test_v_i_o(VS_RECOMMENDED, "0.0.2", "Tor 0.0.2,Tor 0.0.3");
  test_v_i_o(VS_NEW_IN_SERIES, "0.0.2", "Tor 0.0.2pre1,Tor 0.0.3");
  test_v_i_o(VS_OLD, "0.0.2", "Tor 0.0.2.1,Tor 0.0.3");
  test_v_i_o(VS_NEW, "0.1.0", "Tor 0.0.2,Tor 0.0.3");
  test_v_i_o(VS_RECOMMENDED, "0.0.7rc2", "0.0.7,Tor 0.0.7rc2,Tor 0.0.8");
  test_v_i_o(VS_OLD, "0.0.5.0", "0.0.5.1-cvs");
  test_v_i_o(VS_NEW_IN_SERIES, "0.0.5.1-cvs", "0.0.5, 0.0.6");
  /* Not on list, but newer than any in same series. */
  test_v_i_o(VS_NEW_IN_SERIES, "0.1.0.3",
             "Tor 0.1.0.2,Tor 0.0.9.5,Tor 0.1.1.0");
  /* Series newer than any on list. */
  test_v_i_o(VS_NEW, "0.1.2.3", "Tor 0.1.0.2,Tor 0.0.9.5,Tor 0.1.1.0");
  /* Series older than any on list. */
  test_v_i_o(VS_OLD, "0.0.1.3", "Tor 0.1.0.2,Tor 0.0.9.5,Tor 0.1.1.0");
  /* Not on list, not newer than any on same series. */
  test_v_i_o(VS_UNRECOMMENDED, "0.1.0.1",
             "Tor 0.1.0.2,Tor 0.0.9.5,Tor 0.1.1.0");
  /* On list, not newer than any on same series. */
  test_v_i_o(VS_UNRECOMMENDED,
             "0.1.0.1", "Tor 0.1.0.2,Tor 0.0.9.5,Tor 0.1.1.0");
  test_eq(0, tor_version_as_new_as("Tor 0.0.5", "0.0.9pre1-cvs"));
  test_eq(1, tor_version_as_new_as(
          "Tor 0.0.8 on Darwin 64-121-192-100.c3-0."
          "sfpo-ubr1.sfrn-sfpo.ca.cable.rcn.com Power Macintosh",
          "0.0.8rc2"));
  test_eq(0, tor_version_as_new_as(
          "Tor 0.0.8 on Darwin 64-121-192-100.c3-0."
          "sfpo-ubr1.sfrn-sfpo.ca.cable.rcn.com Power Macintosh", "0.0.8.2"));

  /* Now try svn revisions. */
  test_eq(1, tor_version_as_new_as("Tor 0.2.1.0-dev (r100)",
                                   "Tor 0.2.1.0-dev (r99)"));
  test_eq(1, tor_version_as_new_as("Tor 0.2.1.0-dev (r100) on Banana Jr",
                                   "Tor 0.2.1.0-dev (r99) on Hal 9000"));
  test_eq(1, tor_version_as_new_as("Tor 0.2.1.0-dev (r100)",
                                   "Tor 0.2.1.0-dev on Colossus"));
  test_eq(0, tor_version_as_new_as("Tor 0.2.1.0-dev (r99)",
                                   "Tor 0.2.1.0-dev (r100)"));
  test_eq(0, tor_version_as_new_as("Tor 0.2.1.0-dev (r99) on MCP",
                                   "Tor 0.2.1.0-dev (r100) on AM"));
  test_eq(0, tor_version_as_new_as("Tor 0.2.1.0-dev",
                                   "Tor 0.2.1.0-dev (r99)"));
  test_eq(1, tor_version_as_new_as("Tor 0.2.1.1",
                                   "Tor 0.2.1.0-dev (r99)"));

  /* Now try git revisions */
  test_eq(0, tor_version_parse("0.5.6.7 (git-ff00ff)", &ver1));
  test_eq(0, ver1.major);
  test_eq(5, ver1.minor);
  test_eq(6, ver1.micro);
  test_eq(7, ver1.patchlevel);
  test_eq(3, ver1.git_tag_len);
  test_memeq(ver1.git_tag, "\xff\x00\xff", 3);
  test_eq(-1, tor_version_parse("0.5.6.7 (git-ff00xx)", &ver1));
  test_eq(-1, tor_version_parse("0.5.6.7 (git-ff00fff)", &ver1));
  test_eq(0, tor_version_parse("0.5.6.7 (git ff00fff)", &ver1));
 done:
  ;
}

/** Run unit tests for directory fp_pair functions. */
static void
test_dir_fp_pairs(void)
{
  smartlist_t *sl = smartlist_new();
  fp_pair_t *pair;

  dir_split_resource_into_fingerprint_pairs(
       /* Two pairs, out of order, with one duplicate. */
       "73656372657420646174612E0000000000FFFFFF-"
       "557365204145532d32353620696e73746561642e+"
       "73656372657420646174612E0000000000FFFFFF-"
       "557365204145532d32353620696e73746561642e+"
       "48657861646563696d616c2069736e277420736f-"
       "676f6f6420666f7220686964696e6720796f7572.z", sl);

  test_eq(smartlist_len(sl), 2);
  pair = smartlist_get(sl, 0);
  test_memeq(pair->first,  "Hexadecimal isn't so", DIGEST_LEN);
  test_memeq(pair->second, "good for hiding your", DIGEST_LEN);
  pair = smartlist_get(sl, 1);
  test_memeq(pair->first,  "secret data.\0\0\0\0\0\xff\xff\xff", DIGEST_LEN);
  test_memeq(pair->second, "Use AES-256 instead.", DIGEST_LEN);

 done:
  SMARTLIST_FOREACH(sl, fp_pair_t *, pair, tor_free(pair));
  smartlist_free(sl);
}

static void
test_dir_split_fps(void *testdata)
{
  smartlist_t *sl = smartlist_new();
  char *mem_op_hex_tmp = NULL;
  (void)testdata;

  /* Some example hex fingerprints and their base64 equivalents */
#define HEX1 "Fe0daff89127389bc67558691231234551193EEE"
#define HEX2 "Deadbeef99999991111119999911111111f00ba4"
#define HEX3 "b33ff00db33ff00db33ff00db33ff00db33ff00d"
#define HEX256_1 \
    "f3f3f3f3fbbbbf3f3f3f3fbbbf3f3f3f3fbbbbf3f3f3f3fbbbf3f3f3f3fbbbbf"
#define HEX256_2 \
    "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccCCc"
#define HEX256_3 \
    "0123456789ABCdef0123456789ABCdef0123456789ABCdef0123456789ABCdef"
#define B64_1 "/g2v+JEnOJvGdVhpEjEjRVEZPu4"
#define B64_2 "3q2+75mZmZERERmZmRERERHwC6Q"
#define B64_256_1 "8/Pz8/u7vz8/Pz+7vz8/Pz+7u/Pz8/P7u/Pz8/P7u78"
#define B64_256_2 "zMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMzMw"

  /* no flags set */
  dir_split_resource_into_fingerprints("A+C+B", sl, NULL, 0);
  tt_int_op(smartlist_len(sl), ==, 3);
  tt_str_op(smartlist_get(sl, 0), ==, "A");
  tt_str_op(smartlist_get(sl, 1), ==, "C");
  tt_str_op(smartlist_get(sl, 2), ==, "B");
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* uniq strings. */
  dir_split_resource_into_fingerprints("A+C+B+A+B+B", sl, NULL, DSR_SORT_UNIQ);
  tt_int_op(smartlist_len(sl), ==, 3);
  tt_str_op(smartlist_get(sl, 0), ==, "A");
  tt_str_op(smartlist_get(sl, 1), ==, "B");
  tt_str_op(smartlist_get(sl, 2), ==, "C");
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode hex. */
  dir_split_resource_into_fingerprints(HEX1"+"HEX2, sl, NULL, DSR_HEX);
  tt_int_op(smartlist_len(sl), ==, 2);
  test_mem_op_hex(smartlist_get(sl, 0), ==, HEX1);
  test_mem_op_hex(smartlist_get(sl, 1), ==, HEX2);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* decode hex and drop weirdness. */
  dir_split_resource_into_fingerprints(HEX1"+bogus+"HEX2"+"HEX256_1,
                                       sl, NULL, DSR_HEX);
  tt_int_op(smartlist_len(sl), ==, 2);
  test_mem_op_hex(smartlist_get(sl, 0), ==, HEX1);
  test_mem_op_hex(smartlist_get(sl, 1), ==, HEX2);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode long hex */
  dir_split_resource_into_fingerprints(HEX256_1"+"HEX256_2"+"HEX2"+"HEX256_3,
                                       sl, NULL, DSR_HEX|DSR_DIGEST256);
  tt_int_op(smartlist_len(sl), ==, 3);
  test_mem_op_hex(smartlist_get(sl, 0), ==, HEX256_1);
  test_mem_op_hex(smartlist_get(sl, 1), ==, HEX256_2);
  test_mem_op_hex(smartlist_get(sl, 2), ==, HEX256_3);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode hex and sort. */
  dir_split_resource_into_fingerprints(HEX1"+"HEX2"+"HEX3"+"HEX2,
                                       sl, NULL, DSR_HEX|DSR_SORT_UNIQ);
  tt_int_op(smartlist_len(sl), ==, 3);
  test_mem_op_hex(smartlist_get(sl, 0), ==, HEX3);
  test_mem_op_hex(smartlist_get(sl, 1), ==, HEX2);
  test_mem_op_hex(smartlist_get(sl, 2), ==, HEX1);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode long hex and sort */
  dir_split_resource_into_fingerprints(HEX256_1"+"HEX256_2"+"HEX256_3
                                       "+"HEX256_1,
                                       sl, NULL,
                                       DSR_HEX|DSR_DIGEST256|DSR_SORT_UNIQ);
  tt_int_op(smartlist_len(sl), ==, 3);
  test_mem_op_hex(smartlist_get(sl, 0), ==, HEX256_3);
  test_mem_op_hex(smartlist_get(sl, 1), ==, HEX256_2);
  test_mem_op_hex(smartlist_get(sl, 2), ==, HEX256_1);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode base64 */
  dir_split_resource_into_fingerprints(B64_1"-"B64_2, sl, NULL, DSR_BASE64);
  tt_int_op(smartlist_len(sl), ==, 2);
  test_mem_op_hex(smartlist_get(sl, 0), ==, HEX1);
  test_mem_op_hex(smartlist_get(sl, 1), ==, HEX2);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode long base64 */
  dir_split_resource_into_fingerprints(B64_256_1"-"B64_256_2,
                                       sl, NULL, DSR_BASE64|DSR_DIGEST256);
  tt_int_op(smartlist_len(sl), ==, 2);
  test_mem_op_hex(smartlist_get(sl, 0), ==, HEX256_1);
  test_mem_op_hex(smartlist_get(sl, 1), ==, HEX256_2);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  dir_split_resource_into_fingerprints(B64_256_1,
                                       sl, NULL, DSR_BASE64|DSR_DIGEST256);
  tt_int_op(smartlist_len(sl), ==, 1);
  test_mem_op_hex(smartlist_get(sl, 0), ==, HEX256_1);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

 done:
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_free(sl);
  tor_free(mem_op_hex_tmp);
}

static void
test_dir_measured_bw_kb(void)
{
  measured_bw_line_t mbwl;
  int i;
  const char *lines_pass[] = {
    "node_id=$557365204145532d32353620696e73746561642e bw=1024\n",
    "node_id=$557365204145532d32353620696e73746561642e\t  bw=1024 \n",
    " node_id=$557365204145532d32353620696e73746561642e  bw=1024\n",
    "\tnoise\tnode_id=$557365204145532d32353620696e73746561642e  "
                "bw=1024 junk=007\n",
    "misc=junk node_id=$557365204145532d32353620696e73746561642e  "
                "bw=1024 junk=007\n",
    "end"
  };
  const char *lines_fail[] = {
    /* Test possible python stupidity on input */
    "node_id=None bw=1024\n",
    "node_id=$None bw=1024\n",
    "node_id=$557365204145532d32353620696e73746561642e bw=None\n",
    "node_id=$557365204145532d32353620696e73746561642e bw=1024.0\n",
    "node_id=$557365204145532d32353620696e73746561642e bw=.1024\n",
    "node_id=$557365204145532d32353620696e73746561642e bw=1.024\n",
    "node_id=$557365204145532d32353620696e73746561642e bw=1024 bw=0\n",
    "node_id=$557365204145532d32353620696e73746561642e bw=1024 bw=None\n",
    "node_id=$557365204145532d32353620696e73746561642e bw=-1024\n",
    /* Test incomplete writes due to race conditions, partial copies, etc */
    "node_i",
    "node_i\n",
    "node_id=",
    "node_id=\n",
    "node_id=$557365204145532d32353620696e73746561642e bw=",
    "node_id=$557365204145532d32353620696e73746561642e bw=1024",
    "node_id=$557365204145532d32353620696e73746561642e bw=\n",
    "node_id=$557365204145532d32353620696e7374",
    "node_id=$557365204145532d32353620696e7374\n",
    "",
    "\n",
    " \n ",
    " \n\n",
    /* Test assorted noise */
    " node_id= ",
    "node_id==$557365204145532d32353620696e73746561642e bw==1024\n",
    "node_id=$55736520414552d32353620696e73746561642e bw=1024\n",
    "node_id=557365204145532d32353620696e73746561642e bw=1024\n",
    "node_id= $557365204145532d32353620696e73746561642e bw=0.23\n",
    "end"
  };

  for (i = 0; strcmp(lines_fail[i], "end"); i++) {
    //fprintf(stderr, "Testing: %s\n", lines_fail[i]);
    test_assert(measured_bw_line_parse(&mbwl, lines_fail[i]) == -1);
  }

  for (i = 0; strcmp(lines_pass[i], "end"); i++) {
    //fprintf(stderr, "Testing: %s %d\n", lines_pass[i], TOR_ISSPACE('\n'));
    test_assert(measured_bw_line_parse(&mbwl, lines_pass[i]) == 0);
    test_assert(mbwl.bw_kb == 1024);
    test_assert(strcmp(mbwl.node_hex,
                "557365204145532d32353620696e73746561642e") == 0);
  }

 done:
  return;
}

#define MBWC_INIT_TIME 1000

/** Do the measured bandwidth cache unit test */
static void
test_dir_measured_bw_kb_cache(void)
{
  /* Initial fake time_t for testing */
  time_t curr = MBWC_INIT_TIME;
  /* Some measured_bw_line_ts */
  measured_bw_line_t mbwl[3];
  /* For receiving output on cache queries */
  long bw;
  time_t as_of;

  /* First, clear the cache and assert that it's empty */
  dirserv_clear_measured_bw_cache();
  test_eq(dirserv_get_measured_bw_cache_size(), 0);
  /*
   * Set up test mbwls; none of the dirserv_cache_*() functions care about
   * the node_hex field.
   */
  memset(mbwl[0].node_id, 0x01, DIGEST_LEN);
  mbwl[0].bw_kb = 20;
  memset(mbwl[1].node_id, 0x02, DIGEST_LEN);
  mbwl[1].bw_kb = 40;
  memset(mbwl[2].node_id, 0x03, DIGEST_LEN);
  mbwl[2].bw_kb = 80;
  /* Try caching something */
  dirserv_cache_measured_bw(&(mbwl[0]), curr);
  test_eq(dirserv_get_measured_bw_cache_size(), 1);
  /* Okay, let's see if we can retrieve it */
  test_assert(dirserv_query_measured_bw_cache_kb(mbwl[0].node_id,&bw, &as_of));
  test_eq(bw, 20);
  test_eq(as_of, MBWC_INIT_TIME);
  /* Try retrieving it without some outputs */
  test_assert(dirserv_query_measured_bw_cache_kb(mbwl[0].node_id,NULL, NULL));
  test_assert(dirserv_query_measured_bw_cache_kb(mbwl[0].node_id,&bw, NULL));
  test_eq(bw, 20);
  test_assert(dirserv_query_measured_bw_cache_kb(mbwl[0].node_id,NULL,&as_of));
  test_eq(as_of, MBWC_INIT_TIME);
  /* Now expire it */
  curr += MAX_MEASUREMENT_AGE + 1;
  dirserv_expire_measured_bw_cache(curr);
  /* Check that the cache is empty */
  test_eq(dirserv_get_measured_bw_cache_size(), 0);
  /* Check that we can't retrieve it */
  test_assert(!dirserv_query_measured_bw_cache_kb(mbwl[0].node_id, NULL,NULL));
  /* Try caching a few things now */
  dirserv_cache_measured_bw(&(mbwl[0]), curr);
  test_eq(dirserv_get_measured_bw_cache_size(), 1);
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_cache_measured_bw(&(mbwl[1]), curr);
  test_eq(dirserv_get_measured_bw_cache_size(), 2);
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_cache_measured_bw(&(mbwl[2]), curr);
  test_eq(dirserv_get_measured_bw_cache_size(), 3);
  curr += MAX_MEASUREMENT_AGE / 4 + 1;
  /* Do an expire that's too soon to get any of them */
  dirserv_expire_measured_bw_cache(curr);
  test_eq(dirserv_get_measured_bw_cache_size(), 3);
  /* Push the oldest one off the cliff */
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_expire_measured_bw_cache(curr);
  test_eq(dirserv_get_measured_bw_cache_size(), 2);
  /* And another... */
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_expire_measured_bw_cache(curr);
  test_eq(dirserv_get_measured_bw_cache_size(), 1);
  /* This should empty it out again */
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_expire_measured_bw_cache(curr);
  test_eq(dirserv_get_measured_bw_cache_size(), 0);

 done:
  return;
}

static void
test_dir_param_voting(void)
{
  networkstatus_t vote1, vote2, vote3, vote4;
  smartlist_t *votes = smartlist_new();
  char *res = NULL;

  /* dirvote_compute_params only looks at the net_params field of the votes,
     so that's all we need to set.
   */
  memset(&vote1, 0, sizeof(vote1));
  memset(&vote2, 0, sizeof(vote2));
  memset(&vote3, 0, sizeof(vote3));
  memset(&vote4, 0, sizeof(vote4));
  vote1.net_params = smartlist_new();
  vote2.net_params = smartlist_new();
  vote3.net_params = smartlist_new();
  vote4.net_params = smartlist_new();
  smartlist_split_string(vote1.net_params,
                         "ab=90 abcd=20 cw=50 x-yz=-99", NULL, 0, 0);
  smartlist_split_string(vote2.net_params,
                         "ab=27 cw=5 x-yz=88", NULL, 0, 0);
  smartlist_split_string(vote3.net_params,
                         "abcd=20 c=60 cw=500 x-yz=-9 zzzzz=101", NULL, 0, 0);
  smartlist_split_string(vote4.net_params,
                         "ab=900 abcd=200 c=1 cw=51 x-yz=100", NULL, 0, 0);
  test_eq(100, networkstatus_get_param(&vote4, "x-yz", 50, 0, 300));
  test_eq(222, networkstatus_get_param(&vote4, "foobar", 222, 0, 300));
  test_eq(80, networkstatus_get_param(&vote4, "ab", 12, 0, 80));
  test_eq(-8, networkstatus_get_param(&vote4, "ab", -12, -100, -8));
  test_eq(0, networkstatus_get_param(&vote4, "foobar", 0, -100, 8));

  smartlist_add(votes, &vote1);

  /* Do the first tests without adding all the other votes, for
   * networks without many dirauths. */

  res = dirvote_compute_params(votes, 11, 6);
  test_streq(res, "ab=90 abcd=20 cw=50 x-yz=-99");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 2);
  test_streq(res, "");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 1);
  test_streq(res, "ab=90 abcd=20 cw=50 x-yz=-99");
  tor_free(res);

  smartlist_add(votes, &vote2);

  res = dirvote_compute_params(votes, 11, 2);
  test_streq(res, "ab=27 abcd=20 cw=5 x-yz=-99");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 2);
  test_streq(res, "ab=27 cw=5 x-yz=-99");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 3);
  test_streq(res, "ab=27 cw=5 x-yz=-99");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 6);
  test_streq(res, "");
  tor_free(res);

  smartlist_add(votes, &vote3);

  res = dirvote_compute_params(votes, 11, 3);
  test_streq(res, "ab=27 abcd=20 c=60 cw=50 x-yz=-9 zzzzz=101");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 3);
  test_streq(res, "ab=27 abcd=20 cw=50 x-yz=-9");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 5);
  test_streq(res, "cw=50 x-yz=-9");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 9);
  test_streq(res, "cw=50 x-yz=-9");
  tor_free(res);

  smartlist_add(votes, &vote4);

  res = dirvote_compute_params(votes, 11, 4);
  test_streq(res, "ab=90 abcd=20 c=1 cw=50 x-yz=-9 zzzzz=101");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 4);
  test_streq(res, "ab=90 abcd=20 cw=50 x-yz=-9");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 5);
  test_streq(res, "ab=90 abcd=20 cw=50 x-yz=-9");
  tor_free(res);

  /* Test that the special-cased "at least three dirauths voted for
   * this param" logic works as expected. */
  res = dirvote_compute_params(votes, 12, 6);
  test_streq(res, "ab=90 abcd=20 cw=50 x-yz=-9");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 10);
  test_streq(res, "ab=90 abcd=20 cw=50 x-yz=-9");
  tor_free(res);

 done:
  tor_free(res);
  SMARTLIST_FOREACH(vote1.net_params, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(vote2.net_params, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(vote3.net_params, char *, cp, tor_free(cp));
  SMARTLIST_FOREACH(vote4.net_params, char *, cp, tor_free(cp));
  smartlist_free(vote1.net_params);
  smartlist_free(vote2.net_params);
  smartlist_free(vote3.net_params);
  smartlist_free(vote4.net_params);
  smartlist_free(votes);

  return;
}

extern const char AUTHORITY_CERT_1[];
extern const char AUTHORITY_SIGNKEY_1[];
extern const char AUTHORITY_CERT_2[];
extern const char AUTHORITY_SIGNKEY_2[];
extern const char AUTHORITY_CERT_3[];
extern const char AUTHORITY_SIGNKEY_3[];

/** Helper: Test that two networkstatus_voter_info_t do in fact represent the
 * same voting authority, and that they do in fact have all the same
 * information. */
static void
test_same_voter(networkstatus_voter_info_t *v1,
                networkstatus_voter_info_t *v2)
{
  test_streq(v1->nickname, v2->nickname);
  test_memeq(v1->identity_digest, v2->identity_digest, DIGEST_LEN);
  test_streq(v1->address, v2->address);
  test_eq(v1->addr, v2->addr);
  test_eq(v1->dir_port, v2->dir_port);
  test_eq(v1->or_port, v2->or_port);
  test_streq(v1->contact, v2->contact);
  test_memeq(v1->vote_digest, v2->vote_digest, DIGEST_LEN);
 done:
  ;
}

/** Helper: Make a new routerinfo containing the right information for a
 * given vote_routerstatus_t. */
static routerinfo_t *
generate_ri_from_rs(const vote_routerstatus_t *vrs)
{
  routerinfo_t *r;
  const routerstatus_t *rs = &vrs->status;
  static time_t published = 0;

  r = tor_malloc_zero(sizeof(routerinfo_t));
  memcpy(r->cache_info.identity_digest, rs->identity_digest, DIGEST_LEN);
  memcpy(r->cache_info.signed_descriptor_digest, rs->descriptor_digest,
         DIGEST_LEN);
  r->cache_info.do_not_cache = 1;
  r->cache_info.routerlist_index = -1;
  r->cache_info.signed_descriptor_body =
    tor_strdup("123456789012345678901234567890123");
  r->cache_info.signed_descriptor_len =
    strlen(r->cache_info.signed_descriptor_body);
  r->exit_policy = smartlist_new();
  r->cache_info.published_on = ++published + time(NULL);
  if (rs->has_bandwidth) {
    /*
     * Multiply by 1000 because the routerinfo_t and the routerstatus_t
     * seem to use different units (*sigh*) and because we seem stuck on
     * icky and perverse decimal kilobytes (*double sigh*) - see
     * router_get_advertised_bandwidth_capped() of routerlist.c and
     * routerstatus_format_entry() of dirserv.c.
     */
    r->bandwidthrate = rs->bandwidth_kb * 1000;
    r->bandwidthcapacity = rs->bandwidth_kb * 1000;
  }
  return r;
}

/** Helper: get a detached signatures document for one or two
 * consensuses. */
static char *
get_detached_sigs(networkstatus_t *ns, networkstatus_t *ns2)
{
  char *r;
  smartlist_t *sl;
  tor_assert(ns && ns->flavor == FLAV_NS);
  sl = smartlist_new();
  smartlist_add(sl,ns);
  if (ns2)
    smartlist_add(sl,ns2);
  r = networkstatus_get_detached_signatures(sl);
  smartlist_free(sl);
  return r;
}

/**
 * Generate a routerstatus for v3_networkstatus test
 */
static vote_routerstatus_t *
gen_routerstatus_for_v3ns(int idx, time_t now)
{
  vote_routerstatus_t *vrs=NULL;
  routerstatus_t *rs;
  tor_addr_t addr_ipv6;

  switch (idx) {
    case 0:
      /* Generate the first routerstatus. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.1.2.14");
      rs->published_on = now-1500;
      strlcpy(rs->nickname, "router2", sizeof(rs->nickname));
      memset(rs->identity_digest, 3, DIGEST_LEN);
      memset(rs->descriptor_digest, 78, DIGEST_LEN);
      rs->addr = 0x99008801;
      rs->or_port = 443;
      rs->dir_port = 8000;
      /* all flags but running cleared */
      rs->is_flagged_running = 1;
      break;
    case 1:
      /* Generate the second routerstatus. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.2.0.5");
      rs->published_on = now-1000;
      strlcpy(rs->nickname, "router1", sizeof(rs->nickname));
      memset(rs->identity_digest, 5, DIGEST_LEN);
      memset(rs->descriptor_digest, 77, DIGEST_LEN);
      rs->addr = 0x99009901;
      rs->or_port = 443;
      rs->dir_port = 0;
      tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
      tor_addr_copy(&rs->ipv6_addr, &addr_ipv6);
      rs->ipv6_orport = 4711;
      rs->is_exit = rs->is_stable = rs->is_fast = rs->is_flagged_running =
        rs->is_valid = rs->is_possible_guard = 1;
      break;
    case 2:
      /* Generate the third routerstatus. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.1.0.3");
      rs->published_on = now-1000;
      strlcpy(rs->nickname, "router3", sizeof(rs->nickname));
      memset(rs->identity_digest, 33, DIGEST_LEN);
      memset(rs->descriptor_digest, 79, DIGEST_LEN);
      rs->addr = 0xAA009901;
      rs->or_port = 400;
      rs->dir_port = 9999;
      rs->is_authority = rs->is_exit = rs->is_stable = rs->is_fast =
        rs->is_flagged_running = rs->is_valid =
        rs->is_possible_guard = 1;
      break;
    case 3:
      /* Generate a fourth routerstatus that is not running. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.1.6.3");
      rs->published_on = now-1000;
      strlcpy(rs->nickname, "router4", sizeof(rs->nickname));
      memset(rs->identity_digest, 34, DIGEST_LEN);
      memset(rs->descriptor_digest, 47, DIGEST_LEN);
      rs->addr = 0xC0000203;
      rs->or_port = 500;
      rs->dir_port = 1999;
      /* Running flag (and others) cleared */
      break;
    case 4:
      /* No more for this test; return NULL */
      vrs = NULL;
      break;
    default:
      /* Shouldn't happen */
      test_assert(0);
  }
  if (vrs) {
    vrs->microdesc = tor_malloc_zero(sizeof(vote_microdesc_hash_t));
    tor_asprintf(&vrs->microdesc->microdesc_hash_line,
                 "m 9,10,11,12,13,14,15,16,17 "
                 "sha256=xyzajkldsdsajdadlsdjaslsdksdjlsdjsdaskdaaa%d\n",
                 idx);
  }

 done:
  return vrs;
}

/** Apply tweaks to the vote list for each voter */
static int
vote_tweaks_for_v3ns(networkstatus_t *v, int voter, time_t now)
{
  vote_routerstatus_t *vrs;
  const char *msg = NULL;

  test_assert(v);
  (void)now;

  if (voter == 1) {
    measured_bw_line_t mbw;
    memset(mbw.node_id, 33, sizeof(mbw.node_id));
    mbw.bw_kb = 1024;
    test_assert(measured_bw_line_apply(&mbw,
                v->routerstatus_list) == 1);
  } else if (voter == 2 || voter == 3) {
    /* Monkey around with the list a bit */
    vrs = smartlist_get(v->routerstatus_list, 2);
    smartlist_del_keeporder(v->routerstatus_list, 2);
    vote_routerstatus_free(vrs);
    vrs = smartlist_get(v->routerstatus_list, 0);
    vrs->status.is_fast = 1;

    if (voter == 3) {
      vrs = smartlist_get(v->routerstatus_list, 0);
      smartlist_del_keeporder(v->routerstatus_list, 0);
      vote_routerstatus_free(vrs);
      vrs = smartlist_get(v->routerstatus_list, 0);
      memset(vrs->status.descriptor_digest, (int)'Z', DIGEST_LEN);
      test_assert(router_add_to_routerlist(
                  generate_ri_from_rs(vrs), &msg,0,0) >= 0);
    }
  }

 done:
  return 0;
}

/**
 * Test a parsed vote_routerstatus_t for v3_networkstatus test
 */
static void
test_vrs_for_v3ns(vote_routerstatus_t *vrs, int voter, time_t now)
{
  routerstatus_t *rs;
  tor_addr_t addr_ipv6;

  test_assert(vrs);
  rs = &(vrs->status);
  test_assert(rs);

  /* Split out by digests to test */
  if (tor_memeq(rs->identity_digest,
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
                "\x3\x3\x3\x3",
                DIGEST_LEN) &&
                (voter == 1)) {
    /* Check the first routerstatus. */
    test_streq(vrs->version, "0.1.2.14");
    test_eq(rs->published_on, now-1500);
    test_streq(rs->nickname, "router2");
    test_memeq(rs->identity_digest,
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
               "\x3\x3\x3\x3",
               DIGEST_LEN);
    test_memeq(rs->descriptor_digest, "NNNNNNNNNNNNNNNNNNNN", DIGEST_LEN);
    test_eq(rs->addr, 0x99008801);
    test_eq(rs->or_port, 443);
    test_eq(rs->dir_port, 8000);
    /* no flags except "running" (16) and "v2dir" (64) */
    tt_u64_op(vrs->flags, ==, U64_LITERAL(80));
  } else if (tor_memeq(rs->identity_digest,
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
                       "\x5\x5\x5\x5",
                       DIGEST_LEN) &&
                       (voter == 1 || voter == 2)) {
    test_memeq(rs->identity_digest,
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
               "\x5\x5\x5\x5",
               DIGEST_LEN);

    if (voter == 1) {
      /* Check the second routerstatus. */
      test_streq(vrs->version, "0.2.0.5");
      test_eq(rs->published_on, now-1000);
      test_streq(rs->nickname, "router1");
    }
    test_memeq(rs->descriptor_digest, "MMMMMMMMMMMMMMMMMMMM", DIGEST_LEN);
    test_eq(rs->addr, 0x99009901);
    test_eq(rs->or_port, 443);
    test_eq(rs->dir_port, 0);
    tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
    test_assert(tor_addr_eq(&rs->ipv6_addr, &addr_ipv6));
    test_eq(rs->ipv6_orport, 4711);
    if (voter == 1) {
      /* all except "authority" (1) and "v2dir" (64) */
      tt_u64_op(vrs->flags, ==, U64_LITERAL(190));
    } else {
      /* 1023 - authority(1) - madeofcheese(16) - madeoftin(32) - v2dir(256) */
      tt_u64_op(vrs->flags, ==, U64_LITERAL(718));
    }
  } else if (tor_memeq(rs->identity_digest,
                       "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33"
                       "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33",
                       DIGEST_LEN) &&
                       (voter == 1 || voter == 2)) {
    /* Check the measured bandwidth bits */
    test_assert(vrs->has_measured_bw &&
                vrs->measured_bw_kb == 1024);
  } else {
    /*
     * Didn't expect this, but the old unit test only checked some of them,
     * so don't assert.
     */
    /* test_assert(0); */
  }

 done:
  return;
}

/**
 * Test a consensus for v3_networkstatus_test
 */
static void
test_consensus_for_v3ns(networkstatus_t *con, time_t now)
{
  (void)now;

  test_assert(con);
  test_assert(!con->cert);
  test_eq(2, smartlist_len(con->routerstatus_list));
  /* There should be two listed routers: one with identity 3, one with
   * identity 5. */

 done:
  return;
}

/**
 * Test a router list entry for v3_networkstatus test
 */
static void
test_routerstatus_for_v3ns(routerstatus_t *rs, time_t now)
{
  tor_addr_t addr_ipv6;

  test_assert(rs);

  /* There should be two listed routers: one with identity 3, one with
   * identity 5. */
  /* This one showed up in 2 digests. */
  if (tor_memeq(rs->identity_digest,
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
                "\x3\x3",
                DIGEST_LEN)) {
    test_memeq(rs->identity_digest,
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
               DIGEST_LEN);
    test_memeq(rs->descriptor_digest, "NNNNNNNNNNNNNNNNNNNN", DIGEST_LEN);
    test_assert(!rs->is_authority);
    test_assert(!rs->is_exit);
    test_assert(!rs->is_fast);
    test_assert(!rs->is_possible_guard);
    test_assert(!rs->is_stable);
    /* (If it wasn't running it wouldn't be here) */
    test_assert(rs->is_flagged_running);
    test_assert(!rs->is_valid);
    test_assert(!rs->is_named);
    /* XXXX check version */
  } else if (tor_memeq(rs->identity_digest,
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
                       "\x5\x5\x5\x5",
                       DIGEST_LEN)) {
    /* This one showed up in 3 digests. Twice with ID 'M', once with 'Z'.  */
    test_memeq(rs->identity_digest,
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
               DIGEST_LEN);
    test_streq(rs->nickname, "router1");
    test_memeq(rs->descriptor_digest, "MMMMMMMMMMMMMMMMMMMM", DIGEST_LEN);
    test_eq(rs->published_on, now-1000);
    test_eq(rs->addr, 0x99009901);
    test_eq(rs->or_port, 443);
    test_eq(rs->dir_port, 0);
    tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
    test_assert(tor_addr_eq(&rs->ipv6_addr, &addr_ipv6));
    test_eq(rs->ipv6_orport, 4711);
    test_assert(!rs->is_authority);
    test_assert(rs->is_exit);
    test_assert(rs->is_fast);
    test_assert(rs->is_possible_guard);
    test_assert(rs->is_stable);
    test_assert(rs->is_flagged_running);
    test_assert(rs->is_valid);
    test_assert(!rs->is_named);
    /* XXXX check version */
  } else {
    /* Weren't expecting this... */
    test_assert(0);
  }

 done:
  return;
}

/** Run a unit tests for generating and parsing networkstatuses, with
 * the supply test fns. */
static void
test_a_networkstatus(
    vote_routerstatus_t * (*vrs_gen)(int idx, time_t now),
    int (*vote_tweaks)(networkstatus_t *v, int voter, time_t now),
    void (*vrs_test)(vote_routerstatus_t *vrs, int voter, time_t now),
    void (*consensus_test)(networkstatus_t *con, time_t now),
    void (*rs_test)(routerstatus_t *rs, time_t now))
{
  authority_cert_t *cert1=NULL, *cert2=NULL, *cert3=NULL;
  crypto_pk_t *sign_skey_1=NULL, *sign_skey_2=NULL, *sign_skey_3=NULL;
  crypto_pk_t *sign_skey_leg1=NULL;
  const char *msg=NULL;
  /*
   * Sum the non-zero returns from vote_tweaks() we've seen; if vote_tweaks()
   * returns non-zero, it changed net_params and we should skip the tests for
   * that later as they will fail.
   */
  int params_tweaked = 0;

  time_t now = time(NULL);
  networkstatus_voter_info_t *voter;
  document_signature_t *sig;
  networkstatus_t *vote=NULL, *v1=NULL, *v2=NULL, *v3=NULL, *con=NULL,
    *con_md=NULL;
  vote_routerstatus_t *vrs;
  routerstatus_t *rs;
  int idx, n_rs, n_vrs;
  char *v1_text=NULL, *v2_text=NULL, *v3_text=NULL, *consensus_text=NULL,
    *cp=NULL;
  smartlist_t *votes = smartlist_new();

  /* For generating the two other consensuses. */
  char *detached_text1=NULL, *detached_text2=NULL;
  char *consensus_text2=NULL, *consensus_text3=NULL;
  char *consensus_text_md2=NULL, *consensus_text_md3=NULL;
  char *consensus_text_md=NULL;
  networkstatus_t *con2=NULL, *con_md2=NULL, *con3=NULL, *con_md3=NULL;
  ns_detached_signatures_t *dsig1=NULL, *dsig2=NULL;

  test_assert(vrs_gen);
  test_assert(rs_test);
  test_assert(vrs_test);

  /* Parse certificates and keys. */
  cert1 = authority_cert_parse_from_string(AUTHORITY_CERT_1, NULL);
  test_assert(cert1);
  cert2 = authority_cert_parse_from_string(AUTHORITY_CERT_2, NULL);
  test_assert(cert2);
  cert3 = authority_cert_parse_from_string(AUTHORITY_CERT_3, NULL);
  test_assert(cert3);
  sign_skey_1 = crypto_pk_new();
  sign_skey_2 = crypto_pk_new();
  sign_skey_3 = crypto_pk_new();
  sign_skey_leg1 = pk_generate(4);

  test_assert(!crypto_pk_read_private_key_from_string(sign_skey_1,
                                                   AUTHORITY_SIGNKEY_1, -1));
  test_assert(!crypto_pk_read_private_key_from_string(sign_skey_2,
                                                   AUTHORITY_SIGNKEY_2, -1));
  test_assert(!crypto_pk_read_private_key_from_string(sign_skey_3,
                                                   AUTHORITY_SIGNKEY_3, -1));

  test_assert(!crypto_pk_cmp_keys(sign_skey_1, cert1->signing_key));
  test_assert(!crypto_pk_cmp_keys(sign_skey_2, cert2->signing_key));

  /*
   * Set up a vote; generate it; try to parse it.
   */
  vote = tor_malloc_zero(sizeof(networkstatus_t));
  vote->type = NS_TYPE_VOTE;
  vote->published = now;
  vote->valid_after = now+1000;
  vote->fresh_until = now+2000;
  vote->valid_until = now+3000;
  vote->vote_seconds = 100;
  vote->dist_seconds = 200;
  vote->supported_methods = smartlist_new();
  smartlist_split_string(vote->supported_methods, "1 2 3", NULL, 0, -1);
  vote->client_versions = tor_strdup("0.1.2.14,0.1.2.15");
  vote->server_versions = tor_strdup("0.1.2.14,0.1.2.15,0.1.2.16");
  vote->known_flags = smartlist_new();
  smartlist_split_string(vote->known_flags,
                     "Authority Exit Fast Guard Running Stable V2Dir Valid",
                     0, SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  vote->voters = smartlist_new();
  voter = tor_malloc_zero(sizeof(networkstatus_voter_info_t));
  voter->nickname = tor_strdup("Voter1");
  voter->address = tor_strdup("1.2.3.4");
  voter->addr = 0x01020304;
  voter->dir_port = 80;
  voter->or_port = 9000;
  voter->contact = tor_strdup("voter@example.com");
  crypto_pk_get_digest(cert1->identity_key, voter->identity_digest);
  smartlist_add(vote->voters, voter);
  vote->cert = authority_cert_dup(cert1);
  vote->net_params = smartlist_new();
  smartlist_split_string(vote->net_params, "circuitwindow=101 foo=990",
                         NULL, 0, 0);
  vote->routerstatus_list = smartlist_new();
  /* add routerstatuses */
  idx = 0;
  do {
    vrs = vrs_gen(idx, now);
    if (vrs) {
      smartlist_add(vote->routerstatus_list, vrs);
      test_assert(router_add_to_routerlist(generate_ri_from_rs(vrs),
                                           &msg,0,0)>=0);
      ++idx;
    }
  } while (vrs);
  n_vrs = idx;

  /* dump the vote and try to parse it. */
  v1_text = format_networkstatus_vote(sign_skey_1, vote);
  test_assert(v1_text);
  v1 = networkstatus_parse_vote_from_string(v1_text, NULL, NS_TYPE_VOTE);
  test_assert(v1);

  /* Make sure the parsed thing was right. */
  test_eq(v1->type, NS_TYPE_VOTE);
  test_eq(v1->published, vote->published);
  test_eq(v1->valid_after, vote->valid_after);
  test_eq(v1->fresh_until, vote->fresh_until);
  test_eq(v1->valid_until, vote->valid_until);
  test_eq(v1->vote_seconds, vote->vote_seconds);
  test_eq(v1->dist_seconds, vote->dist_seconds);
  test_streq(v1->client_versions, vote->client_versions);
  test_streq(v1->server_versions, vote->server_versions);
  test_assert(v1->voters && smartlist_len(v1->voters));
  voter = smartlist_get(v1->voters, 0);
  test_streq(voter->nickname, "Voter1");
  test_streq(voter->address, "1.2.3.4");
  test_eq(voter->addr, 0x01020304);
  test_eq(voter->dir_port, 80);
  test_eq(voter->or_port, 9000);
  test_streq(voter->contact, "voter@example.com");
  test_assert(v1->cert);
  test_assert(!crypto_pk_cmp_keys(sign_skey_1, v1->cert->signing_key));
  cp = smartlist_join_strings(v1->known_flags, ":", 0, NULL);
  test_streq(cp, "Authority:Exit:Fast:Guard:Running:Stable:V2Dir:Valid");
  tor_free(cp);
  test_eq(smartlist_len(v1->routerstatus_list), n_vrs);

  if (vote_tweaks) params_tweaked += vote_tweaks(v1, 1, now);

  /* Check the routerstatuses. */
  for (idx = 0; idx < n_vrs; ++idx) {
    vrs = smartlist_get(v1->routerstatus_list, idx);
    test_assert(vrs);
    vrs_test(vrs, 1, now);
  }

  /* Generate second vote. It disagrees on some of the times,
   * and doesn't list versions, and knows some crazy flags */
  vote->published = now+1;
  vote->fresh_until = now+3005;
  vote->dist_seconds = 300;
  authority_cert_free(vote->cert);
  vote->cert = authority_cert_dup(cert2);
  SMARTLIST_FOREACH(vote->net_params, char *, c, tor_free(c));
  smartlist_clear(vote->net_params);
  smartlist_split_string(vote->net_params, "bar=2000000000 circuitwindow=20",
                         NULL, 0, 0);
  tor_free(vote->client_versions);
  tor_free(vote->server_versions);
  voter = smartlist_get(vote->voters, 0);
  tor_free(voter->nickname);
  tor_free(voter->address);
  voter->nickname = tor_strdup("Voter2");
  voter->address = tor_strdup("2.3.4.5");
  voter->addr = 0x02030405;
  crypto_pk_get_digest(cert2->identity_key, voter->identity_digest);
  smartlist_add(vote->known_flags, tor_strdup("MadeOfCheese"));
  smartlist_add(vote->known_flags, tor_strdup("MadeOfTin"));
  smartlist_sort_strings(vote->known_flags);

  /* generate and parse v2. */
  v2_text = format_networkstatus_vote(sign_skey_2, vote);
  test_assert(v2_text);
  v2 = networkstatus_parse_vote_from_string(v2_text, NULL, NS_TYPE_VOTE);
  test_assert(v2);

  if (vote_tweaks) params_tweaked += vote_tweaks(v2, 2, now);

  /* Check that flags come out right.*/
  cp = smartlist_join_strings(v2->known_flags, ":", 0, NULL);
  test_streq(cp, "Authority:Exit:Fast:Guard:MadeOfCheese:MadeOfTin:"
             "Running:Stable:V2Dir:Valid");
  tor_free(cp);

  /* Check the routerstatuses. */
  n_vrs = smartlist_len(v2->routerstatus_list);
  for (idx = 0; idx < n_vrs; ++idx) {
    vrs = smartlist_get(v2->routerstatus_list, idx);
    test_assert(vrs);
    vrs_test(vrs, 2, now);
  }

  /* Generate the third vote. */
  vote->published = now;
  vote->fresh_until = now+2003;
  vote->dist_seconds = 250;
  authority_cert_free(vote->cert);
  vote->cert = authority_cert_dup(cert3);
  SMARTLIST_FOREACH(vote->net_params, char *, c, tor_free(c));
  smartlist_clear(vote->net_params);
  smartlist_split_string(vote->net_params, "circuitwindow=80 foo=660",
                         NULL, 0, 0);
  smartlist_add(vote->supported_methods, tor_strdup("4"));
  vote->client_versions = tor_strdup("0.1.2.14,0.1.2.17");
  vote->server_versions = tor_strdup("0.1.2.10,0.1.2.15,0.1.2.16");
  voter = smartlist_get(vote->voters, 0);
  tor_free(voter->nickname);
  tor_free(voter->address);
  voter->nickname = tor_strdup("Voter3");
  voter->address = tor_strdup("3.4.5.6");
  voter->addr = 0x03040506;
  crypto_pk_get_digest(cert3->identity_key, voter->identity_digest);
  /* This one has a legacy id. */
  memset(voter->legacy_id_digest, (int)'A', DIGEST_LEN);

  v3_text = format_networkstatus_vote(sign_skey_3, vote);
  test_assert(v3_text);

  v3 = networkstatus_parse_vote_from_string(v3_text, NULL, NS_TYPE_VOTE);
  test_assert(v3);

  if (vote_tweaks) params_tweaked += vote_tweaks(v3, 3, now);

  /* Compute a consensus as voter 3. */
  smartlist_add(votes, v3);
  smartlist_add(votes, v1);
  smartlist_add(votes, v2);
  consensus_text = networkstatus_compute_consensus(votes, 3,
                                                   cert3->identity_key,
                                                   sign_skey_3,
                                                   "AAAAAAAAAAAAAAAAAAAA",
                                                   sign_skey_leg1,
                                                   FLAV_NS);
  test_assert(consensus_text);
  con = networkstatus_parse_vote_from_string(consensus_text, NULL,
                                             NS_TYPE_CONSENSUS);
  test_assert(con);
  //log_notice(LD_GENERAL, "<<%s>>\n<<%s>>\n<<%s>>\n",
  //           v1_text, v2_text, v3_text);
  consensus_text_md = networkstatus_compute_consensus(votes, 3,
                                                   cert3->identity_key,
                                                   sign_skey_3,
                                                   "AAAAAAAAAAAAAAAAAAAA",
                                                   sign_skey_leg1,
                                                   FLAV_MICRODESC);
  test_assert(consensus_text_md);
  con_md = networkstatus_parse_vote_from_string(consensus_text_md, NULL,
                                                NS_TYPE_CONSENSUS);
  test_assert(con_md);
  test_eq(con_md->flavor, FLAV_MICRODESC);

  /* Check consensus contents. */
  test_assert(con->type == NS_TYPE_CONSENSUS);
  test_eq(con->published, 0); /* this field only appears in votes. */
  test_eq(con->valid_after, now+1000);
  test_eq(con->fresh_until, now+2003); /* median */
  test_eq(con->valid_until, now+3000);
  test_eq(con->vote_seconds, 100);
  test_eq(con->dist_seconds, 250); /* median */
  test_streq(con->client_versions, "0.1.2.14");
  test_streq(con->server_versions, "0.1.2.15,0.1.2.16");
  cp = smartlist_join_strings(v2->known_flags, ":", 0, NULL);
  test_streq(cp, "Authority:Exit:Fast:Guard:MadeOfCheese:MadeOfTin:"
             "Running:Stable:V2Dir:Valid");
  tor_free(cp);
  if (!params_tweaked) {
    /* Skip this one if vote_tweaks() messed with the param lists */
    cp = smartlist_join_strings(con->net_params, ":", 0, NULL);
    test_streq(cp, "circuitwindow=80:foo=660");
    tor_free(cp);
  }

  test_eq(4, smartlist_len(con->voters)); /*3 voters, 1 legacy key.*/
  /* The voter id digests should be in this order. */
  test_assert(memcmp(cert2->cache_info.identity_digest,
                     cert1->cache_info.identity_digest,DIGEST_LEN)<0);
  test_assert(memcmp(cert1->cache_info.identity_digest,
                     cert3->cache_info.identity_digest,DIGEST_LEN)<0);
  test_same_voter(smartlist_get(con->voters, 1),
                  smartlist_get(v2->voters, 0));
  test_same_voter(smartlist_get(con->voters, 2),
                  smartlist_get(v1->voters, 0));
  test_same_voter(smartlist_get(con->voters, 3),
                  smartlist_get(v3->voters, 0));

  consensus_test(con, now);

  /* Check the routerstatuses. */
  n_rs = smartlist_len(con->routerstatus_list);
  for (idx = 0; idx < n_rs; ++idx) {
    rs = smartlist_get(con->routerstatus_list, idx);
    test_assert(rs);
    rs_test(rs, now);
  }

  /* Check signatures.  the first voter is a pseudo-entry with a legacy key.
   * The second one hasn't signed.  The fourth one has signed: validate it. */
  voter = smartlist_get(con->voters, 1);
  test_eq(smartlist_len(voter->sigs), 0);

  voter = smartlist_get(con->voters, 3);
  test_eq(smartlist_len(voter->sigs), 1);
  sig = smartlist_get(voter->sigs, 0);
  test_assert(sig->signature);
  test_assert(!sig->good_signature);
  test_assert(!sig->bad_signature);

  test_assert(!networkstatus_check_document_signature(con, sig, cert3));
  test_assert(sig->signature);
  test_assert(sig->good_signature);
  test_assert(!sig->bad_signature);

  {
    const char *msg=NULL;
    /* Compute the other two signed consensuses. */
    smartlist_shuffle(votes);
    consensus_text2 = networkstatus_compute_consensus(votes, 3,
                                                      cert2->identity_key,
                                                      sign_skey_2, NULL,NULL,
                                                      FLAV_NS);
    consensus_text_md2 = networkstatus_compute_consensus(votes, 3,
                                                      cert2->identity_key,
                                                      sign_skey_2, NULL,NULL,
                                                      FLAV_MICRODESC);
    smartlist_shuffle(votes);
    consensus_text3 = networkstatus_compute_consensus(votes, 3,
                                                      cert1->identity_key,
                                                      sign_skey_1, NULL,NULL,
                                                      FLAV_NS);
    consensus_text_md3 = networkstatus_compute_consensus(votes, 3,
                                                      cert1->identity_key,
                                                      sign_skey_1, NULL,NULL,
                                                      FLAV_MICRODESC);
    test_assert(consensus_text2);
    test_assert(consensus_text3);
    test_assert(consensus_text_md2);
    test_assert(consensus_text_md3);
    con2 = networkstatus_parse_vote_from_string(consensus_text2, NULL,
                                                NS_TYPE_CONSENSUS);
    con3 = networkstatus_parse_vote_from_string(consensus_text3, NULL,
                                                NS_TYPE_CONSENSUS);
    con_md2 = networkstatus_parse_vote_from_string(consensus_text_md2, NULL,
                                                NS_TYPE_CONSENSUS);
    con_md3 = networkstatus_parse_vote_from_string(consensus_text_md3, NULL,
                                                NS_TYPE_CONSENSUS);
    test_assert(con2);
    test_assert(con3);
    test_assert(con_md2);
    test_assert(con_md3);

    /* All three should have the same digest. */
    test_memeq(&con->digests, &con2->digests, sizeof(digests_t));
    test_memeq(&con->digests, &con3->digests, sizeof(digests_t));

    test_memeq(&con_md->digests, &con_md2->digests, sizeof(digests_t));
    test_memeq(&con_md->digests, &con_md3->digests, sizeof(digests_t));

    /* Extract a detached signature from con3. */
    detached_text1 = get_detached_sigs(con3, con_md3);
    tt_assert(detached_text1);
    /* Try to parse it. */
    dsig1 = networkstatus_parse_detached_signatures(detached_text1, NULL);
    tt_assert(dsig1);

    /* Are parsed values as expected? */
    test_eq(dsig1->valid_after, con3->valid_after);
    test_eq(dsig1->fresh_until, con3->fresh_until);
    test_eq(dsig1->valid_until, con3->valid_until);
    {
      digests_t *dsig_digests = strmap_get(dsig1->digests, "ns");
      test_assert(dsig_digests);
      test_memeq(dsig_digests->d[DIGEST_SHA1], con3->digests.d[DIGEST_SHA1],
                 DIGEST_LEN);
      dsig_digests = strmap_get(dsig1->digests, "microdesc");
      test_assert(dsig_digests);
      test_memeq(dsig_digests->d[DIGEST_SHA256],
                 con_md3->digests.d[DIGEST_SHA256],
                 DIGEST256_LEN);
    }
    {
      smartlist_t *dsig_signatures = strmap_get(dsig1->signatures, "ns");
      test_assert(dsig_signatures);
      test_eq(1, smartlist_len(dsig_signatures));
      sig = smartlist_get(dsig_signatures, 0);
      test_memeq(sig->identity_digest, cert1->cache_info.identity_digest,
                 DIGEST_LEN);
      test_eq(sig->alg, DIGEST_SHA1);

      dsig_signatures = strmap_get(dsig1->signatures, "microdesc");
      test_assert(dsig_signatures);
      test_eq(1, smartlist_len(dsig_signatures));
      sig = smartlist_get(dsig_signatures, 0);
      test_memeq(sig->identity_digest, cert1->cache_info.identity_digest,
                 DIGEST_LEN);
      test_eq(sig->alg, DIGEST_SHA256);
    }

    /* Try adding it to con2. */
    detached_text2 = get_detached_sigs(con2,con_md2);
    test_eq(1, networkstatus_add_detached_signatures(con2, dsig1, "test",
                                                     LOG_INFO, &msg));
    tor_free(detached_text2);
    test_eq(1, networkstatus_add_detached_signatures(con_md2, dsig1, "test",
                                                     LOG_INFO, &msg));
    tor_free(detached_text2);
    detached_text2 = get_detached_sigs(con2,con_md2);
    //printf("\n<%s>\n", detached_text2);
    dsig2 = networkstatus_parse_detached_signatures(detached_text2, NULL);
    test_assert(dsig2);
    /*
    printf("\n");
    SMARTLIST_FOREACH(dsig2->signatures, networkstatus_voter_info_t *, vi, {
        char hd[64];
        base16_encode(hd, sizeof(hd), vi->identity_digest, DIGEST_LEN);
        printf("%s\n", hd);
      });
    */
    test_eq(2,
            smartlist_len((smartlist_t*)strmap_get(dsig2->signatures, "ns")));
    test_eq(2,
            smartlist_len((smartlist_t*)strmap_get(dsig2->signatures,
                                                   "microdesc")));

    /* Try adding to con2 twice; verify that nothing changes. */
    test_eq(0, networkstatus_add_detached_signatures(con2, dsig1, "test",
                                                     LOG_INFO, &msg));

    /* Add to con. */
    test_eq(2, networkstatus_add_detached_signatures(con, dsig2, "test",
                                                     LOG_INFO, &msg));
    /* Check signatures */
    voter = smartlist_get(con->voters, 1);
    sig = smartlist_get(voter->sigs, 0);
    test_assert(sig);
    test_assert(!networkstatus_check_document_signature(con, sig, cert2));
    voter = smartlist_get(con->voters, 2);
    sig = smartlist_get(voter->sigs, 0);
    test_assert(sig);
    test_assert(!networkstatus_check_document_signature(con, sig, cert1));
  }

 done:
  tor_free(cp);
  smartlist_free(votes);
  tor_free(v1_text);
  tor_free(v2_text);
  tor_free(v3_text);
  tor_free(consensus_text);
  tor_free(consensus_text_md);

  if (vote)
    networkstatus_vote_free(vote);
  if (v1)
    networkstatus_vote_free(v1);
  if (v2)
    networkstatus_vote_free(v2);
  if (v3)
    networkstatus_vote_free(v3);
  if (con)
    networkstatus_vote_free(con);
  if (con_md)
    networkstatus_vote_free(con_md);
  if (sign_skey_1)
    crypto_pk_free(sign_skey_1);
  if (sign_skey_2)
    crypto_pk_free(sign_skey_2);
  if (sign_skey_3)
    crypto_pk_free(sign_skey_3);
  if (sign_skey_leg1)
    crypto_pk_free(sign_skey_leg1);
  if (cert1)
    authority_cert_free(cert1);
  if (cert2)
    authority_cert_free(cert2);
  if (cert3)
    authority_cert_free(cert3);

  tor_free(consensus_text2);
  tor_free(consensus_text3);
  tor_free(consensus_text_md2);
  tor_free(consensus_text_md3);
  tor_free(detached_text1);
  tor_free(detached_text2);
  if (con2)
    networkstatus_vote_free(con2);
  if (con3)
    networkstatus_vote_free(con3);
  if (con_md2)
    networkstatus_vote_free(con_md2);
  if (con_md3)
    networkstatus_vote_free(con_md3);
  if (dsig1)
    ns_detached_signatures_free(dsig1);
  if (dsig2)
    ns_detached_signatures_free(dsig2);
}

/** Run unit tests for generating and parsing V3 consensus networkstatus
 * documents. */
static void
test_dir_v3_networkstatus(void)
{
  test_a_networkstatus(gen_routerstatus_for_v3ns,
                       vote_tweaks_for_v3ns,
                       test_vrs_for_v3ns,
                       test_consensus_for_v3ns,
                       test_routerstatus_for_v3ns);
}

static void
test_dir_scale_bw(void *testdata)
{
  double v[8] = { 2.0/3,
                  7.0,
                  1.0,
                  3.0,
                  1.0/5,
                  1.0/7,
                  12.0,
                  24.0 };
  u64_dbl_t vals[8];
  uint64_t total;
  int i;

  (void) testdata;

  for (i=0; i<8; ++i)
    vals[i].dbl = v[i];

  scale_array_elements_to_u64(vals, 8, &total);

  tt_int_op((int)total, ==, 48);
  total = 0;
  for (i=0; i<8; ++i) {
    total += vals[i].u64;
  }
  tt_assert(total >= (U64_LITERAL(1)<<60));
  tt_assert(total <= (U64_LITERAL(1)<<62));

  for (i=0; i<8; ++i) {
    double ratio = ((double)vals[i].u64) / vals[2].u64;
    tt_double_op(fabs(ratio - v[i]), <, .00001);
  }

 done:
  ;
}

static void
test_dir_random_weighted(void *testdata)
{
  int histogram[10];
  uint64_t vals[10] = {3,1,2,4,6,0,7,5,8,9}, total=0;
  u64_dbl_t inp[10];
  int i, choice;
  const int n = 50000;
  double max_sq_error;
  (void) testdata;

  /* Try a ten-element array with values from 0 through 10. The values are
   * in a scrambled order to make sure we don't depend on order. */
  memset(histogram,0,sizeof(histogram));
  for (i=0; i<10; ++i) {
    inp[i].u64 = vals[i];
    total += vals[i];
  }
  tt_u64_op(total, ==, 45);
  for (i=0; i<n; ++i) {
    choice = choose_array_element_by_weight(inp, 10);
    tt_int_op(choice, >=, 0);
    tt_int_op(choice, <, 10);
    histogram[choice]++;
  }

  /* Now see if we chose things about frequently enough. */
  max_sq_error = 0;
  for (i=0; i<10; ++i) {
    int expected = (int)(n*vals[i]/total);
    double frac_diff = 0, sq;
    TT_BLATHER(("  %d : %5d vs %5d\n", (int)vals[i], histogram[i], expected));
    if (expected)
      frac_diff = (histogram[i] - expected) / ((double)expected);
    else
      tt_int_op(histogram[i], ==, 0);

    sq = frac_diff * frac_diff;
    if (sq > max_sq_error)
      max_sq_error = sq;
  }
  /* It should almost always be much much less than this.  If you want to
   * figure out the odds, please feel free. */
  tt_double_op(max_sq_error, <, .05);

  /* Now try a singleton; do we choose it? */
  for (i = 0; i < 100; ++i) {
    choice = choose_array_element_by_weight(inp, 1);
    tt_int_op(choice, ==, 0);
  }

  /* Now try an array of zeros.  We should choose randomly. */
  memset(histogram,0,sizeof(histogram));
  for (i = 0; i < 5; ++i)
    inp[i].u64 = 0;
  for (i = 0; i < n; ++i) {
    choice = choose_array_element_by_weight(inp, 5);
    tt_int_op(choice, >=, 0);
    tt_int_op(choice, <, 5);
    histogram[choice]++;
  }
  /* Now see if we chose things about frequently enough. */
  max_sq_error = 0;
  for (i=0; i<5; ++i) {
    int expected = n/5;
    double frac_diff = 0, sq;
    TT_BLATHER(("  %d : %5d vs %5d\n", (int)vals[i], histogram[i], expected));
    frac_diff = (histogram[i] - expected) / ((double)expected);
    sq = frac_diff * frac_diff;
    if (sq > max_sq_error)
      max_sq_error = sq;
  }
  /* It should almost always be much much less than this.  If you want to
   * figure out the odds, please feel free. */
  tt_double_op(max_sq_error, <, .05);
 done:
  ;
}

/* Function pointers for test_dir_clip_unmeasured_bw_kb() */

static uint32_t alternate_clip_bw = 0;

/**
 * Generate a routerstatus for clip_unmeasured_bw_kb test; based on the
 * v3_networkstatus ones.
 */
static vote_routerstatus_t *
gen_routerstatus_for_umbw(int idx, time_t now)
{
  vote_routerstatus_t *vrs = NULL;
  routerstatus_t *rs;
  tor_addr_t addr_ipv6;
  uint32_t max_unmeasured_bw_kb = (alternate_clip_bw > 0) ?
    alternate_clip_bw : DEFAULT_MAX_UNMEASURED_BW_KB;

  switch (idx) {
    case 0:
      /* Generate the first routerstatus. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.1.2.14");
      rs->published_on = now-1500;
      strlcpy(rs->nickname, "router2", sizeof(rs->nickname));
      memset(rs->identity_digest, 3, DIGEST_LEN);
      memset(rs->descriptor_digest, 78, DIGEST_LEN);
      rs->addr = 0x99008801;
      rs->or_port = 443;
      rs->dir_port = 8000;
      /* all flags but running cleared */
      rs->is_flagged_running = 1;
      /*
       * This one has measured bandwidth below the clip cutoff, and
       * so shouldn't be clipped; we'll have to test that it isn't
       * later.
       */
      vrs->has_measured_bw = 1;
      rs->has_bandwidth = 1;
      vrs->measured_bw_kb = rs->bandwidth_kb = max_unmeasured_bw_kb / 2;
      break;
    case 1:
      /* Generate the second routerstatus. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.2.0.5");
      rs->published_on = now-1000;
      strlcpy(rs->nickname, "router1", sizeof(rs->nickname));
      memset(rs->identity_digest, 5, DIGEST_LEN);
      memset(rs->descriptor_digest, 77, DIGEST_LEN);
      rs->addr = 0x99009901;
      rs->or_port = 443;
      rs->dir_port = 0;
      tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
      tor_addr_copy(&rs->ipv6_addr, &addr_ipv6);
      rs->ipv6_orport = 4711;
      rs->is_exit = rs->is_stable = rs->is_fast = rs->is_flagged_running =
        rs->is_valid = rs->is_possible_guard = 1;
      /*
       * This one has measured bandwidth above the clip cutoff, and
       * so shouldn't be clipped; we'll have to test that it isn't
       * later.
       */
      vrs->has_measured_bw = 1;
      rs->has_bandwidth = 1;
      vrs->measured_bw_kb = rs->bandwidth_kb = 2 * max_unmeasured_bw_kb;
      break;
    case 2:
      /* Generate the third routerstatus. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.1.0.3");
      rs->published_on = now-1000;
      strlcpy(rs->nickname, "router3", sizeof(rs->nickname));
      memset(rs->identity_digest, 0x33, DIGEST_LEN);
      memset(rs->descriptor_digest, 79, DIGEST_LEN);
      rs->addr = 0xAA009901;
      rs->or_port = 400;
      rs->dir_port = 9999;
      rs->is_authority = rs->is_exit = rs->is_stable = rs->is_fast =
        rs->is_flagged_running = rs->is_valid =
        rs->is_possible_guard = 1;
      /*
       * This one has unmeasured bandwidth above the clip cutoff, and
       * so should be clipped; we'll have to test that it isn't
       * later.
       */
      vrs->has_measured_bw = 0;
      rs->has_bandwidth = 1;
      vrs->measured_bw_kb = 0;
      rs->bandwidth_kb = 2 * max_unmeasured_bw_kb;
      break;
    case 3:
      /* Generate a fourth routerstatus that is not running. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.1.6.3");
      rs->published_on = now-1000;
      strlcpy(rs->nickname, "router4", sizeof(rs->nickname));
      memset(rs->identity_digest, 0x34, DIGEST_LEN);
      memset(rs->descriptor_digest, 47, DIGEST_LEN);
      rs->addr = 0xC0000203;
      rs->or_port = 500;
      rs->dir_port = 1999;
      /* all flags but running cleared */
      rs->is_flagged_running = 1;
      /*
       * This one has unmeasured bandwidth below the clip cutoff, and
       * so shouldn't be clipped; we'll have to test that it isn't
       * later.
       */
      vrs->has_measured_bw = 0;
      rs->has_bandwidth = 1;
      vrs->measured_bw_kb = 0;
      rs->bandwidth_kb = max_unmeasured_bw_kb / 2;
      break;
    case 4:
      /* No more for this test; return NULL */
      vrs = NULL;
      break;
    default:
      /* Shouldn't happen */
      test_assert(0);
  }
  if (vrs) {
    vrs->microdesc = tor_malloc_zero(sizeof(vote_microdesc_hash_t));
    tor_asprintf(&vrs->microdesc->microdesc_hash_line,
                 "m 9,10,11,12,13,14,15,16,17 "
                 "sha256=xyzajkldsdsajdadlsdjaslsdksdjlsdjsdaskdaaa%d\n",
                 idx);
  }

 done:
  return vrs;
}

/** Apply tweaks to the vote list for each voter; for the umbw test this is
 * just adding the right consensus methods to let clipping happen */
static int
vote_tweaks_for_umbw(networkstatus_t *v, int voter, time_t now)
{
  char *maxbw_param = NULL;
  int rv = 0;

  test_assert(v);
  (void)voter;
  (void)now;

  test_assert(v->supported_methods);
  SMARTLIST_FOREACH(v->supported_methods, char *, c, tor_free(c));
  smartlist_clear(v->supported_methods);
  /* Method 17 is MIN_METHOD_TO_CLIP_UNMEASURED_BW_KB */
  smartlist_split_string(v->supported_methods,
                         "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17",
                         NULL, 0, -1);
  /* If we're using a non-default clip bandwidth, add it to net_params */
  if (alternate_clip_bw > 0) {
    tor_asprintf(&maxbw_param, "maxunmeasuredbw=%u", alternate_clip_bw);
    test_assert(maxbw_param);
    if (maxbw_param) {
      smartlist_add(v->net_params, maxbw_param);
      rv = 1;
    }
  }

 done:
  return rv;
}

/**
 * Test a parsed vote_routerstatus_t for umbw test.
 */
static void
test_vrs_for_umbw(vote_routerstatus_t *vrs, int voter, time_t now)
{
  routerstatus_t *rs;
  tor_addr_t addr_ipv6;
  uint32_t max_unmeasured_bw_kb = (alternate_clip_bw > 0) ?
    alternate_clip_bw : DEFAULT_MAX_UNMEASURED_BW_KB;

  (void)voter;
  test_assert(vrs);
  rs = &(vrs->status);
  test_assert(rs);

  /* Split out by digests to test */
  if (tor_memeq(rs->identity_digest,
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
                DIGEST_LEN)) {
    /*
     * Check the first routerstatus - measured bandwidth below the clip
     * cutoff.
     */
    test_streq(vrs->version, "0.1.2.14");
    test_eq(rs->published_on, now-1500);
    test_streq(rs->nickname, "router2");
    test_memeq(rs->identity_digest,
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
               DIGEST_LEN);
    test_memeq(rs->descriptor_digest, "NNNNNNNNNNNNNNNNNNNN", DIGEST_LEN);
    test_eq(rs->addr, 0x99008801);
    test_eq(rs->or_port, 443);
    test_eq(rs->dir_port, 8000);
    test_assert(rs->has_bandwidth);
    test_assert(vrs->has_measured_bw);
    test_eq(rs->bandwidth_kb, max_unmeasured_bw_kb / 2);
    test_eq(vrs->measured_bw_kb, max_unmeasured_bw_kb / 2);
  } else if (tor_memeq(rs->identity_digest,
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
                       DIGEST_LEN)) {

    /*
     * Check the second routerstatus - measured bandwidth above the clip
     * cutoff.
     */
    test_streq(vrs->version, "0.2.0.5");
    test_eq(rs->published_on, now-1000);
    test_streq(rs->nickname, "router1");
    test_memeq(rs->identity_digest,
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
               DIGEST_LEN);
    test_memeq(rs->descriptor_digest, "MMMMMMMMMMMMMMMMMMMM", DIGEST_LEN);
    test_eq(rs->addr, 0x99009901);
    test_eq(rs->or_port, 443);
    test_eq(rs->dir_port, 0);
    tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
    test_assert(tor_addr_eq(&rs->ipv6_addr, &addr_ipv6));
    test_eq(rs->ipv6_orport, 4711);
    test_assert(rs->has_bandwidth);
    test_assert(vrs->has_measured_bw);
    test_eq(rs->bandwidth_kb, max_unmeasured_bw_kb * 2);
    test_eq(vrs->measured_bw_kb, max_unmeasured_bw_kb * 2);
  } else if (tor_memeq(rs->identity_digest,
                       "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33"
                       "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33",
                       DIGEST_LEN)) {
    /*
     * Check the third routerstatus - unmeasured bandwidth above the clip
     * cutoff; this one should be clipped later on in the consensus, but
     * appears unclipped in the vote.
     */
    test_assert(rs->has_bandwidth);
    test_assert(!(vrs->has_measured_bw));
    test_eq(rs->bandwidth_kb, max_unmeasured_bw_kb * 2);
    test_eq(vrs->measured_bw_kb, 0);
  } else if (tor_memeq(rs->identity_digest,
                       "\x34\x34\x34\x34\x34\x34\x34\x34\x34\x34"
                       "\x34\x34\x34\x34\x34\x34\x34\x34\x34\x34",
                       DIGEST_LEN)) {
    /*
     * Check the fourth routerstatus - unmeasured bandwidth below the clip
     * cutoff; this one should not be clipped.
     */
    test_assert(rs->has_bandwidth);
    test_assert(!(vrs->has_measured_bw));
    test_eq(rs->bandwidth_kb, max_unmeasured_bw_kb / 2);
    test_eq(vrs->measured_bw_kb, 0);
  } else {
    test_assert(0);
  }

 done:
  return;
}

/**
 * Test a consensus for v3_networkstatus_test
 */
static void
test_consensus_for_umbw(networkstatus_t *con, time_t now)
{
  (void)now;

  test_assert(con);
  test_assert(!con->cert);
  // test_assert(con->consensus_method >= MIN_METHOD_TO_CLIP_UNMEASURED_BW_KB);
  test_assert(con->consensus_method >= 16);
  test_eq(4, smartlist_len(con->routerstatus_list));
  /* There should be four listed routers; all voters saw the same in this */

 done:
  return;
}

/**
 * Test a router list entry for umbw test
 */
static void
test_routerstatus_for_umbw(routerstatus_t *rs, time_t now)
{
  tor_addr_t addr_ipv6;
  uint32_t max_unmeasured_bw_kb = (alternate_clip_bw > 0) ?
    alternate_clip_bw : DEFAULT_MAX_UNMEASURED_BW_KB;

  test_assert(rs);

  /* There should be four listed routers, as constructed above */
  if (tor_memeq(rs->identity_digest,
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
                DIGEST_LEN)) {
    test_memeq(rs->identity_digest,
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
               DIGEST_LEN);
    test_memeq(rs->descriptor_digest, "NNNNNNNNNNNNNNNNNNNN", DIGEST_LEN);
    test_assert(!rs->is_authority);
    test_assert(!rs->is_exit);
    test_assert(!rs->is_fast);
    test_assert(!rs->is_possible_guard);
    test_assert(!rs->is_stable);
    /* (If it wasn't running it wouldn't be here) */
    test_assert(rs->is_flagged_running);
    test_assert(!rs->is_valid);
    test_assert(!rs->is_named);
    /* This one should have measured bandwidth below the clip cutoff */
    test_assert(rs->has_bandwidth);
    test_eq(rs->bandwidth_kb, max_unmeasured_bw_kb / 2);
    test_assert(!(rs->bw_is_unmeasured));
  } else if (tor_memeq(rs->identity_digest,
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
                       DIGEST_LEN)) {
    /* This one showed up in 3 digests. Twice with ID 'M', once with 'Z'.  */
    test_memeq(rs->identity_digest,
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
               DIGEST_LEN);
    test_streq(rs->nickname, "router1");
    test_memeq(rs->descriptor_digest, "MMMMMMMMMMMMMMMMMMMM", DIGEST_LEN);
    test_eq(rs->published_on, now-1000);
    test_eq(rs->addr, 0x99009901);
    test_eq(rs->or_port, 443);
    test_eq(rs->dir_port, 0);
    tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
    test_assert(tor_addr_eq(&rs->ipv6_addr, &addr_ipv6));
    test_eq(rs->ipv6_orport, 4711);
    test_assert(!rs->is_authority);
    test_assert(rs->is_exit);
    test_assert(rs->is_fast);
    test_assert(rs->is_possible_guard);
    test_assert(rs->is_stable);
    test_assert(rs->is_flagged_running);
    test_assert(rs->is_valid);
    test_assert(!rs->is_named);
    /* This one should have measured bandwidth above the clip cutoff */
    test_assert(rs->has_bandwidth);
    test_eq(rs->bandwidth_kb, max_unmeasured_bw_kb * 2);
    test_assert(!(rs->bw_is_unmeasured));
  } else if (tor_memeq(rs->identity_digest,
                "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33"
                "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33",
                DIGEST_LEN)) {
    /*
     * This one should have unmeasured bandwidth above the clip cutoff,
     * and so should be clipped
     */
    test_assert(rs->has_bandwidth);
    test_eq(rs->bandwidth_kb, max_unmeasured_bw_kb);
    test_assert(rs->bw_is_unmeasured);
  } else if (tor_memeq(rs->identity_digest,
                "\x34\x34\x34\x34\x34\x34\x34\x34\x34\x34"
                "\x34\x34\x34\x34\x34\x34\x34\x34\x34\x34",
                DIGEST_LEN)) {
    /*
     * This one should have unmeasured bandwidth below the clip cutoff,
     * and so should not be clipped
     */
    test_assert(rs->has_bandwidth);
    test_eq(rs->bandwidth_kb, max_unmeasured_bw_kb / 2);
    test_assert(rs->bw_is_unmeasured);
  } else {
    /* Weren't expecting this... */
    test_assert(0);
  }

 done:
  return;
}

/**
 * Compute a consensus involving clipping unmeasured bandwidth with consensus
 * method 17; this uses the same test_a_networkstatus() function that the
 * v3_networkstatus test uses.
 */

static void
test_dir_clip_unmeasured_bw_kb(void)
{
  /* Run the test with the default clip bandwidth */
  alternate_clip_bw = 0;
  test_a_networkstatus(gen_routerstatus_for_umbw,
                       vote_tweaks_for_umbw,
                       test_vrs_for_umbw,
                       test_consensus_for_umbw,
                       test_routerstatus_for_umbw);
}

/**
 * This version of test_dir_clip_unmeasured_bw_kb() uses a non-default choice
 * of clip bandwidth.
 */

static void
test_dir_clip_unmeasured_bw_kb_alt(void)
{
  /*
   * Try a different one; this value is chosen so that the below-the-cutoff
   * unmeasured nodes the test uses, at alternate_clip_bw / 2, will be above
   * DEFAULT_MAX_UNMEASURED_BW_KB and if the consensus incorrectly uses that
   * cutoff it will fail the test.
   */
  alternate_clip_bw = 3 * DEFAULT_MAX_UNMEASURED_BW_KB;
  test_a_networkstatus(gen_routerstatus_for_umbw,
                       vote_tweaks_for_umbw,
                       test_vrs_for_umbw,
                       test_consensus_for_umbw,
                       test_routerstatus_for_umbw);
}

static void
test_dir_fmt_control_ns(void *arg)
{
  char *s = NULL;
  routerstatus_t rs;
  (void)arg;

  memset(&rs, 0, sizeof(rs));
  rs.published_on = 1364925198;
  strlcpy(rs.nickname, "TetsuoMilk", sizeof(rs.nickname));
  memcpy(rs.identity_digest, "Stately, plump Buck ", DIGEST_LEN);
  memcpy(rs.descriptor_digest, "Mulligan came up fro", DIGEST_LEN);
  rs.addr = 0x20304050;
  rs.or_port = 9001;
  rs.dir_port = 9002;
  rs.is_exit = 1;
  rs.is_fast = 1;
  rs.is_flagged_running = 1;
  rs.has_bandwidth = 1;
  rs.bandwidth_kb = 1000;

  s = networkstatus_getinfo_helper_single(&rs);
  tt_assert(s);
  tt_str_op(s, ==,
            "r TetsuoMilk U3RhdGVseSwgcGx1bXAgQnVjayA "
               "TXVsbGlnYW4gY2FtZSB1cCBmcm8 2013-04-02 17:53:18 "
               "32.48.64.80 9001 9002\n"
            "s Exit Fast Running V2Dir\n"
            "w Bandwidth=1000\n");

 done:
  tor_free(s);
}

static void
test_dir_http_handling(void *args)
{
  char *url = NULL;
  (void)args;

  /* Parse http url tests: */
  /* Good headers */
  test_eq(parse_http_url("GET /tor/a/b/c.txt HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: Mozilla/5.0 (Windows;"
                           " U; Windows NT 6.1; en-US; rv:1.9.1.5)\r\n",
                           &url), 0);
  test_streq(url, "/tor/a/b/c.txt");
  tor_free(url);

  test_eq(parse_http_url("GET /tor/a/b/c.txt HTTP/1.0\r\n", &url), 0);
  test_streq(url, "/tor/a/b/c.txt");
  tor_free(url);

  test_eq(parse_http_url("GET /tor/a/b/c.txt HTTP/1.600\r\n", &url), 0);
  test_streq(url, "/tor/a/b/c.txt");
  tor_free(url);

  /* Should prepend '/tor/' to url if required */
  test_eq(parse_http_url("GET /a/b/c.txt HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: Mozilla/5.0 (Windows;"
                           " U; Windows NT 6.1; en-US; rv:1.9.1.5)\r\n",
                           &url), 0);
  test_streq(url, "/tor/a/b/c.txt");
  tor_free(url);

  /* Bad headers -- no HTTP/1.x*/
  test_eq(parse_http_url("GET /a/b/c.txt\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: Mozilla/5.0 (Windows;"
                           " U; Windows NT 6.1; en-US; rv:1.9.1.5)\r\n",
                           &url), -1);
  tt_assert(!url);

  /* Bad headers */
  test_eq(parse_http_url("GET /a/b/c.txt\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: Mozilla/5.0 (Windows;"
                           " U; Windows NT 6.1; en-US; rv:1.9.1.5)\r\n",
                           &url), -1);
  tt_assert(!url);

  test_eq(parse_http_url("GET /tor/a/b/c.txt", &url), -1);
  tt_assert(!url);

  test_eq(parse_http_url("GET /tor/a/b/c.txt HTTP/1.1", &url), -1);
  tt_assert(!url);

  test_eq(parse_http_url("GET /tor/a/b/c.txt HTTP/1.1x\r\n", &url), -1);
  tt_assert(!url);

  test_eq(parse_http_url("GET /tor/a/b/c.txt HTTP/1.", &url), -1);
  tt_assert(!url);

  test_eq(parse_http_url("GET /tor/a/b/c.txt HTTP/1.\r", &url), -1);
  tt_assert(!url);

 done:
  tor_free(url);
}

#define DIR_LEGACY(name)                                                   \
  { #name, legacy_test_helper, TT_FORK, &legacy_setup, test_dir_ ## name }

#define DIR(name,flags)                              \
  { #name, test_dir_##name, (flags), NULL, NULL }

struct testcase_t dir_tests[] = {
  DIR_LEGACY(nicknames),
  DIR_LEGACY(formats),
  DIR_LEGACY(versions),
  DIR_LEGACY(fp_pairs),
  DIR(split_fps, 0),
  DIR_LEGACY(measured_bw_kb),
  DIR_LEGACY(measured_bw_kb_cache),
  DIR_LEGACY(param_voting),
  DIR_LEGACY(v3_networkstatus),
  DIR(random_weighted, 0),
  DIR(scale_bw, 0),
  DIR_LEGACY(clip_unmeasured_bw_kb),
  DIR_LEGACY(clip_unmeasured_bw_kb_alt),
  DIR(fmt_control_ns, 0),
  DIR(http_handling, 0),
  END_OF_TESTCASES
};

