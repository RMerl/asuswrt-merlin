/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#include <math.h>

#define CONFIG_PRIVATE
#define DIRSERV_PRIVATE
#define DIRVOTE_PRIVATE
#define ROUTER_PRIVATE
#define ROUTERLIST_PRIVATE
#define HIBERNATE_PRIVATE
#define NETWORKSTATUS_PRIVATE
#define RELAY_PRIVATE

#include "or.h"
#include "confparse.h"
#include "config.h"
#include "crypto_ed25519.h"
#include "directory.h"
#include "dirserv.h"
#include "dirvote.h"
#include "hibernate.h"
#include "memarea.h"
#include "networkstatus.h"
#include "router.h"
#include "routerkeys.h"
#include "routerlist.h"
#include "routerparse.h"
#include "routerset.h"
#include "test.h"
#include "test_dir_common.h"
#include "torcert.h"
#include "relay.h"

#define NS_MODULE dir

static void
test_dir_nicknames(void *arg)
{
  (void)arg;
  tt_assert( is_legal_nickname("a"));
  tt_assert(!is_legal_nickname(""));
  tt_assert(!is_legal_nickname("abcdefghijklmnopqrst")); /* 20 chars */
  tt_assert(!is_legal_nickname("hyphen-")); /* bad char */
  tt_assert( is_legal_nickname("abcdefghijklmnopqrs")); /* 19 chars */
  tt_assert(!is_legal_nickname("$AAAAAAAA01234AAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  /* valid */
  tt_assert( is_legal_nickname_or_hexdigest(
                                 "$AAAAAAAA01234AAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  tt_assert( is_legal_nickname_or_hexdigest(
                         "$AAAAAAAA01234AAAAAAAAAAAAAAAAAAAAAAAAAAA=fred"));
  tt_assert( is_legal_nickname_or_hexdigest(
                         "$AAAAAAAA01234AAAAAAAAAAAAAAAAAAAAAAAAAAA~fred"));
  /* too short */
  tt_assert(!is_legal_nickname_or_hexdigest(
                                 "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  /* illegal char */
  tt_assert(!is_legal_nickname_or_hexdigest(
                                 "$AAAAAAzAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  /* hex part too long */
  tt_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  tt_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=fred"));
  /* Bad nickname */
  tt_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="));
  tt_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA~"));
  tt_assert(!is_legal_nickname_or_hexdigest(
                       "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA~hyphen-"));
  tt_assert(!is_legal_nickname_or_hexdigest(
                       "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA~"
                       "abcdefghijklmnoppqrst"));
  /* Bad extra char. */
  tt_assert(!is_legal_nickname_or_hexdigest(
                         "$AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA!"));
  tt_assert(is_legal_nickname_or_hexdigest("xyzzy"));
  tt_assert(is_legal_nickname_or_hexdigest("abcdefghijklmnopqrs"));
  tt_assert(!is_legal_nickname_or_hexdigest("abcdefghijklmnopqrst"));
 done:
  ;
}

static smartlist_t *mocked_configured_ports = NULL;

/** Returns mocked_configured_ports */
static const smartlist_t *
mock_get_configured_ports(void)
{
  return mocked_configured_ports;
}

/** Run unit tests for router descriptor generation logic. */
static void
test_dir_formats(void *arg)
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
  uint8_t *rsa_cc = NULL;
  or_options_t *options = get_options_mutable();
  const addr_policy_t *p;
  time_t now = time(NULL);
  port_cfg_t orport, dirport;

  (void)arg;
  pk1 = pk_generate(0);
  pk2 = pk_generate(1);

  tt_assert(pk1 && pk2);

  hibernate_set_state_for_testing_(HIBERNATE_STATE_LIVE);

  get_platform_str(platform, sizeof(platform));
  r1 = tor_malloc_zero(sizeof(routerinfo_t));
  r1->addr = 0xc0a80001u; /* 192.168.0.1 */
  r1->cache_info.published_on = 0;
  r1->or_port = 9000;
  r1->dir_port = 9003;
  r1->supports_tunnelled_dir_requests = 1;
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
  ed25519_keypair_t kp1, kp2;
  ed25519_secret_key_from_seed(&kp1.seckey,
                          (const uint8_t*)"YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY");
  ed25519_public_key_generate(&kp1.pubkey, &kp1.seckey);
  ed25519_secret_key_from_seed(&kp2.seckey,
                          (const uint8_t*)"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
  ed25519_public_key_generate(&kp2.pubkey, &kp2.seckey);
  r2->cache_info.signing_key_cert = tor_cert_create(&kp1,
                                         CERT_TYPE_ID_SIGNING,
                                         &kp2.pubkey,
                                         now, 86400,
                                         CERT_FLAG_INCLUDE_SIGNING_KEY);
  char cert_buf[256];
  base64_encode(cert_buf, sizeof(cert_buf),
                (const char*)r2->cache_info.signing_key_cert->encoded,
                r2->cache_info.signing_key_cert->encoded_len,
                BASE64_ENCODE_MULTILINE);
  r2->platform = tor_strdup(platform);
  r2->cache_info.published_on = 5;
  r2->or_port = 9005;
  r2->dir_port = 0;
  r2->supports_tunnelled_dir_requests = 1;
  r2->onion_pkey = crypto_pk_dup_key(pk2);
  curve25519_keypair_t r2_onion_keypair;
  curve25519_keypair_generate(&r2_onion_keypair, 0);
  r2->onion_curve25519_pkey = tor_memdup(&r2_onion_keypair.pubkey,
                                         sizeof(curve25519_public_key_t));
  r2->identity_pkey = crypto_pk_dup_key(pk1);
  r2->bandwidthrate = r2->bandwidthburst = r2->bandwidthcapacity = 3000;
  r2->exit_policy = smartlist_new();
  smartlist_add(r2->exit_policy, ex1);
  smartlist_add(r2->exit_policy, ex2);
  r2->nickname = tor_strdup("Fred");

  tt_assert(!crypto_pk_write_public_key_to_string(pk1, &pk1_str,
                                                    &pk1_str_len));
  tt_assert(!crypto_pk_write_public_key_to_string(pk2 , &pk2_str,
                                                    &pk2_str_len));

  /* XXXX025 router_dump_to_string should really take this from ri.*/
  options->ContactInfo = tor_strdup("Magri White "
                                    "<magri@elsewhere.example.com>");
  /* Skip reachability checks for DirPort and tunnelled-dir-server */
  options->AssumeReachable = 1;

  /* Fake just enough of an ORPort and DirPort to get by */
  MOCK(get_configured_ports, mock_get_configured_ports);
  mocked_configured_ports = smartlist_new();

  memset(&orport, 0, sizeof(orport));
  orport.type = CONN_TYPE_OR_LISTENER;
  orport.addr.family = AF_INET;
  orport.port = 9000;
  smartlist_add(mocked_configured_ports, &orport);

  memset(&dirport, 0, sizeof(dirport));
  dirport.type = CONN_TYPE_DIR_LISTENER;
  dirport.addr.family = AF_INET;
  dirport.port = 9003;
  smartlist_add(mocked_configured_ports, &dirport);

  buf = router_dump_router_to_string(r1, pk2, NULL, NULL, NULL);

  UNMOCK(get_configured_ports);
  smartlist_free(mocked_configured_ports);
  mocked_configured_ports = NULL;

  tor_free(options->ContactInfo);
  tt_assert(buf);

  strlcpy(buf2, "router Magri 192.168.0.1 9000 0 9003\n"
          "or-address [1:2:3:4::]:9999\n"
          "platform Tor "VERSION" on ", sizeof(buf2));
  strlcat(buf2, get_uname(), sizeof(buf2));
  strlcat(buf2, "\n"
          "protocols Link 1 2 Circuit 1\n"
          "published 1970-01-01 00:00:00\n"
          "fingerprint ", sizeof(buf2));
  tt_assert(!crypto_pk_get_fingerprint(pk2, fingerprint, 1));
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
  strlcat(buf2, "reject *:*\n", sizeof(buf2));
  strlcat(buf2, "tunnelled-dir-server\nrouter-signature\n", sizeof(buf2));
  buf[strlen(buf2)] = '\0'; /* Don't compare the sig; it's never the same
                             * twice */

  tt_str_op(buf,OP_EQ, buf2);
  tor_free(buf);

  buf = router_dump_router_to_string(r1, pk2, NULL, NULL, NULL);
  tt_assert(buf);
  cp = buf;
  rp1 = router_parse_entry_from_string((const char*)cp,NULL,1,0,NULL,NULL);
  tt_assert(rp1);
  tt_int_op(rp1->addr,OP_EQ, r1->addr);
  tt_int_op(rp1->or_port,OP_EQ, r1->or_port);
  tt_int_op(rp1->dir_port,OP_EQ, r1->dir_port);
  tt_int_op(rp1->bandwidthrate,OP_EQ, r1->bandwidthrate);
  tt_int_op(rp1->bandwidthburst,OP_EQ, r1->bandwidthburst);
  tt_int_op(rp1->bandwidthcapacity,OP_EQ, r1->bandwidthcapacity);
  tt_assert(crypto_pk_cmp_keys(rp1->onion_pkey, pk1) == 0);
  tt_assert(crypto_pk_cmp_keys(rp1->identity_pkey, pk2) == 0);
  tt_assert(rp1->supports_tunnelled_dir_requests);
  //tt_assert(rp1->exit_policy == NULL);
  tor_free(buf);

  strlcpy(buf2,
          "router Fred 10.3.2.1 9005 0 0\n"
          "identity-ed25519\n"
          "-----BEGIN ED25519 CERT-----\n", sizeof(buf2));
  strlcat(buf2, cert_buf, sizeof(buf2));
  strlcat(buf2, "-----END ED25519 CERT-----\n", sizeof(buf2));
  strlcat(buf2, "master-key-ed25519 ", sizeof(buf2));
  {
    char k[ED25519_BASE64_LEN+1];
    tt_assert(ed25519_public_to_base64(k,
                                &r2->cache_info.signing_key_cert->signing_key)
              >= 0);
    strlcat(buf2, k, sizeof(buf2));
    strlcat(buf2, "\n", sizeof(buf2));
  }
  strlcat(buf2, "platform Tor "VERSION" on ", sizeof(buf2));
  strlcat(buf2, get_uname(), sizeof(buf2));
  strlcat(buf2, "\n"
          "protocols Link 1 2 Circuit 1\n"
          "published 1970-01-01 00:00:05\n"
          "fingerprint ", sizeof(buf2));
  tt_assert(!crypto_pk_get_fingerprint(pk1, fingerprint, 1));
  strlcat(buf2, fingerprint, sizeof(buf2));
  strlcat(buf2, "\nuptime 0\n"
          "bandwidth 3000 3000 3000\n", sizeof(buf2));
  strlcat(buf2, "onion-key\n", sizeof(buf2));
  strlcat(buf2, pk2_str, sizeof(buf2));
  strlcat(buf2, "signing-key\n", sizeof(buf2));
  strlcat(buf2, pk1_str, sizeof(buf2));
  int rsa_cc_len;
  rsa_cc = make_tap_onion_key_crosscert(pk2,
                                        &kp1.pubkey,
                                        pk1,
                                        &rsa_cc_len);
  tt_assert(rsa_cc);
  base64_encode(cert_buf, sizeof(cert_buf), (char*)rsa_cc, rsa_cc_len,
                BASE64_ENCODE_MULTILINE);
  strlcat(buf2, "onion-key-crosscert\n"
          "-----BEGIN CROSSCERT-----\n", sizeof(buf2));
  strlcat(buf2, cert_buf, sizeof(buf2));
  strlcat(buf2, "-----END CROSSCERT-----\n", sizeof(buf2));
  int ntor_cc_sign;
  {
    tor_cert_t *ntor_cc = NULL;
    ntor_cc = make_ntor_onion_key_crosscert(&r2_onion_keypair,
                                          &kp1.pubkey,
                                          r2->cache_info.published_on,
                                          MIN_ONION_KEY_LIFETIME,
                                          &ntor_cc_sign);
    tt_assert(ntor_cc);
    base64_encode(cert_buf, sizeof(cert_buf),
                (char*)ntor_cc->encoded, ntor_cc->encoded_len,
                BASE64_ENCODE_MULTILINE);
    tor_cert_free(ntor_cc);
  }
  tor_snprintf(buf2+strlen(buf2), sizeof(buf2)-strlen(buf2),
               "ntor-onion-key-crosscert %d\n"
               "-----BEGIN ED25519 CERT-----\n"
               "%s"
               "-----END ED25519 CERT-----\n", ntor_cc_sign, cert_buf);

  strlcat(buf2, "hidden-service-dir\n", sizeof(buf2));
  strlcat(buf2, "ntor-onion-key ", sizeof(buf2));
  base64_encode(cert_buf, sizeof(cert_buf),
                (const char*)r2_onion_keypair.pubkey.public_key, 32,
                BASE64_ENCODE_MULTILINE);
  strlcat(buf2, cert_buf, sizeof(buf2));
  strlcat(buf2, "accept *:80\nreject 18.0.0.0/8:24\n", sizeof(buf2));
  strlcat(buf2, "tunnelled-dir-server\n", sizeof(buf2));
  strlcat(buf2, "router-sig-ed25519 ", sizeof(buf2));

  /* Fake just enough of an ORPort to get by */
  MOCK(get_configured_ports, mock_get_configured_ports);
  mocked_configured_ports = smartlist_new();

  memset(&orport, 0, sizeof(orport));
  orport.type = CONN_TYPE_OR_LISTENER;
  orport.addr.family = AF_INET;
  orport.port = 9005;
  smartlist_add(mocked_configured_ports, &orport);

  buf = router_dump_router_to_string(r2, pk1, pk2, &r2_onion_keypair, &kp2);
  tt_assert(buf);
  buf[strlen(buf2)] = '\0'; /* Don't compare the sig; it's never the same
                             * twice */

  tt_str_op(buf, OP_EQ, buf2);
  tor_free(buf);

  buf = router_dump_router_to_string(r2, pk1, NULL, NULL, NULL);

  UNMOCK(get_configured_ports);
  smartlist_free(mocked_configured_ports);
  mocked_configured_ports = NULL;

  /* Reset for later */
  cp = buf;
  rp2 = router_parse_entry_from_string((const char*)cp,NULL,1,0,NULL,NULL);
  tt_assert(rp2);
  tt_int_op(rp2->addr,OP_EQ, r2->addr);
  tt_int_op(rp2->or_port,OP_EQ, r2->or_port);
  tt_int_op(rp2->dir_port,OP_EQ, r2->dir_port);
  tt_int_op(rp2->bandwidthrate,OP_EQ, r2->bandwidthrate);
  tt_int_op(rp2->bandwidthburst,OP_EQ, r2->bandwidthburst);
  tt_int_op(rp2->bandwidthcapacity,OP_EQ, r2->bandwidthcapacity);
  tt_mem_op(rp2->onion_curve25519_pkey->public_key,OP_EQ,
             r2->onion_curve25519_pkey->public_key,
             CURVE25519_PUBKEY_LEN);
  tt_assert(crypto_pk_cmp_keys(rp2->onion_pkey, pk2) == 0);
  tt_assert(crypto_pk_cmp_keys(rp2->identity_pkey, pk1) == 0);
  tt_assert(rp2->supports_tunnelled_dir_requests);

  tt_int_op(smartlist_len(rp2->exit_policy),OP_EQ, 2);

  p = smartlist_get(rp2->exit_policy, 0);
  tt_int_op(p->policy_type,OP_EQ, ADDR_POLICY_ACCEPT);
  tt_assert(tor_addr_is_null(&p->addr));
  tt_int_op(p->maskbits,OP_EQ, 0);
  tt_int_op(p->prt_min,OP_EQ, 80);
  tt_int_op(p->prt_max,OP_EQ, 80);

  p = smartlist_get(rp2->exit_policy, 1);
  tt_int_op(p->policy_type,OP_EQ, ADDR_POLICY_REJECT);
  tt_assert(tor_addr_eq(&p->addr, &ex2->addr));
  tt_int_op(p->maskbits,OP_EQ, 8);
  tt_int_op(p->prt_min,OP_EQ, 24);
  tt_int_op(p->prt_max,OP_EQ, 24);

#if 0
  /* Okay, now for the directories. */
  {
    fingerprint_list = smartlist_new();
    crypto_pk_get_fingerprint(pk2, buf, 1);
    add_fingerprint_to_dir(buf, fingerprint_list, 0);
    crypto_pk_get_fingerprint(pk1, buf, 1);
    add_fingerprint_to_dir(buf, fingerprint_list, 0);
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

  tor_free(rsa_cc);
  tor_free(buf);
  tor_free(pk1_str);
  tor_free(pk2_str);
  if (pk1) crypto_pk_free(pk1);
  if (pk2) crypto_pk_free(pk2);
  if (rp1) routerinfo_free(rp1);
  tor_free(dir1); /* XXXX And more !*/
  tor_free(dir2); /* And more !*/
}

#include "failing_routerdescs.inc"

static void
test_dir_routerinfo_parsing(void *arg)
{
  (void) arg;

  int again;
  routerinfo_t *ri = NULL;

#define CHECK_OK(s)                                                     \
  do {                                                                  \
    routerinfo_free(ri);                                                \
    ri = router_parse_entry_from_string((s), NULL, 0, 0, NULL, NULL);   \
    tt_assert(ri);                                                      \
  } while (0)
#define CHECK_FAIL(s, againval)                                         \
  do {                                                                  \
    routerinfo_free(ri);                                                \
    again = 999;                                                        \
    ri = router_parse_entry_from_string((s), NULL, 0, 0, NULL, &again); \
    tt_assert(ri == NULL);                                              \
    tt_int_op(again, OP_EQ, (againval));                                   \
  } while (0)

  CHECK_OK(EX_RI_MINIMAL);
  CHECK_OK(EX_RI_MAXIMAL);

  CHECK_OK(EX_RI_MINIMAL_ED);

  /* good annotations prepended */
  routerinfo_free(ri);
  ri = router_parse_entry_from_string(EX_RI_MINIMAL, NULL, 0, 0,
                                      "@purpose bridge\n", NULL);
  tt_assert(ri != NULL);
  tt_assert(ri->purpose == ROUTER_PURPOSE_BRIDGE);
  routerinfo_free(ri);

  /* bad annotations prepended. */
  ri = router_parse_entry_from_string(EX_RI_MINIMAL,
                                      NULL, 0, 0, "@purpose\n", NULL);
  tt_assert(ri == NULL);

  /* bad annotations on router. */
  ri = router_parse_entry_from_string("@purpose\nrouter x\n", NULL, 0, 1,
                                      NULL, NULL);
  tt_assert(ri == NULL);

  /* unwanted annotations on router. */
  ri = router_parse_entry_from_string("@purpose foo\nrouter x\n", NULL, 0, 0,
                                      NULL, NULL);
  tt_assert(ri == NULL);

  /* No signature. */
  ri = router_parse_entry_from_string("router x\n", NULL, 0, 0,
                                      NULL, NULL);
  tt_assert(ri == NULL);

  /* Not a router */
  routerinfo_free(ri);
  ri = router_parse_entry_from_string("hello\n", NULL, 0, 0, NULL, NULL);
  tt_assert(ri == NULL);

  CHECK_FAIL(EX_RI_BAD_SIG1, 1);
  CHECK_FAIL(EX_RI_BAD_SIG2, 1);
  CHECK_FAIL(EX_RI_BAD_TOKENS, 0);
  CHECK_FAIL(EX_RI_BAD_PUBLISHED, 0);
  CHECK_FAIL(EX_RI_NEG_BANDWIDTH, 0);
  CHECK_FAIL(EX_RI_BAD_BANDWIDTH, 0);
  CHECK_FAIL(EX_RI_BAD_BANDWIDTH2, 0);
  CHECK_FAIL(EX_RI_BAD_ONIONKEY1, 0);
  CHECK_FAIL(EX_RI_BAD_ONIONKEY2, 0);
  CHECK_FAIL(EX_RI_BAD_PORTS, 0);
  CHECK_FAIL(EX_RI_BAD_IP, 0);
  CHECK_FAIL(EX_RI_BAD_DIRPORT, 0);
  CHECK_FAIL(EX_RI_BAD_NAME2, 0);
  CHECK_FAIL(EX_RI_BAD_UPTIME, 0);

  CHECK_FAIL(EX_RI_BAD_BANDWIDTH3, 0);
  CHECK_FAIL(EX_RI_BAD_NTOR_KEY, 0);
  CHECK_FAIL(EX_RI_BAD_FINGERPRINT, 0);
  CHECK_FAIL(EX_RI_MISMATCHED_FINGERPRINT, 0);
  CHECK_FAIL(EX_RI_BAD_HAS_ACCEPT6, 0);
  CHECK_FAIL(EX_RI_BAD_NO_EXIT_POLICY, 0);
  CHECK_FAIL(EX_RI_BAD_IPV6_EXIT_POLICY, 0);
  CHECK_FAIL(EX_RI_BAD_FAMILY, 0);
  CHECK_FAIL(EX_RI_ZERO_ORPORT, 0);

  CHECK_FAIL(EX_RI_ED_MISSING_CROSSCERT, 0);
  CHECK_FAIL(EX_RI_ED_MISSING_CROSSCERT2, 0);
  CHECK_FAIL(EX_RI_ED_MISSING_CROSSCERT_SIGN, 0);
  CHECK_FAIL(EX_RI_ED_BAD_SIG1, 0);
  CHECK_FAIL(EX_RI_ED_BAD_SIG2, 0);
  CHECK_FAIL(EX_RI_ED_BAD_SIG3, 0);
  CHECK_FAIL(EX_RI_ED_BAD_SIG4, 0);
  CHECK_FAIL(EX_RI_ED_BAD_CROSSCERT1, 0);
  CHECK_FAIL(EX_RI_ED_BAD_CROSSCERT3, 0);
  CHECK_FAIL(EX_RI_ED_BAD_CROSSCERT4, 0);
  CHECK_FAIL(EX_RI_ED_BAD_CROSSCERT5, 0);
  CHECK_FAIL(EX_RI_ED_BAD_CROSSCERT6, 0);
  CHECK_FAIL(EX_RI_ED_BAD_CROSSCERT7, 0);
  CHECK_FAIL(EX_RI_ED_MISPLACED1, 0);
  CHECK_FAIL(EX_RI_ED_MISPLACED2, 0);
  CHECK_FAIL(EX_RI_ED_BAD_CERT1, 0);
  CHECK_FAIL(EX_RI_ED_BAD_CERT2, 0);
  CHECK_FAIL(EX_RI_ED_BAD_CERT3, 0);

  /* This is allowed; we just ignore it. */
  CHECK_OK(EX_RI_BAD_EI_DIGEST);
  CHECK_OK(EX_RI_BAD_EI_DIGEST2);

#undef CHECK_FAIL
#undef CHECK_OK
 done:
  routerinfo_free(ri);
}

#include "example_extrainfo.inc"

static void
routerinfo_free_wrapper_(void *arg)
{
  routerinfo_free(arg);
}

static void
test_dir_extrainfo_parsing(void *arg)
{
  (void) arg;

#define CHECK_OK(s)                                                     \
  do {                                                                  \
    extrainfo_free(ei);                                                 \
    ei = extrainfo_parse_entry_from_string((s), NULL, 0, map, NULL);    \
    tt_assert(ei);                                                      \
  } while (0)
#define CHECK_FAIL(s, againval)                                         \
  do {                                                                  \
    extrainfo_free(ei);                                                 \
    again = 999;                                                        \
    ei = extrainfo_parse_entry_from_string((s), NULL, 0, map, &again);  \
    tt_assert(ei == NULL);                                              \
    tt_int_op(again, OP_EQ, (againval));                                   \
  } while (0)
#define ADD(name)                                                       \
  do {                                                                  \
    ri = tor_malloc_zero(sizeof(routerinfo_t));                         \
    crypto_pk_t *pk = ri->identity_pkey = crypto_pk_new();              \
    tt_assert(! crypto_pk_read_public_key_from_string(pk,               \
                                      name##_KEY, strlen(name##_KEY))); \
    tt_int_op(0,OP_EQ,base16_decode(d, 20, name##_FP, strlen(name##_FP))); \
    digestmap_set((digestmap_t*)map, d, ri);                            \
    ri = NULL;                                                          \
  } while (0)

  routerinfo_t *ri = NULL;
  char d[20];
  struct digest_ri_map_t *map = NULL;
  extrainfo_t *ei = NULL;
  int again;

  CHECK_OK(EX_EI_MINIMAL);
  tt_assert(ei->pending_sig);
  CHECK_OK(EX_EI_MAXIMAL);
  tt_assert(ei->pending_sig);
  CHECK_OK(EX_EI_GOOD_ED_EI);
  tt_assert(ei->pending_sig);

  map = (struct digest_ri_map_t *)digestmap_new();
  ADD(EX_EI_MINIMAL);
  ADD(EX_EI_MAXIMAL);
  ADD(EX_EI_GOOD_ED_EI);
  ADD(EX_EI_BAD_FP);
  ADD(EX_EI_BAD_NICKNAME);
  ADD(EX_EI_BAD_TOKENS);
  ADD(EX_EI_BAD_START);
  ADD(EX_EI_BAD_PUBLISHED);

  ADD(EX_EI_ED_MISSING_SIG);
  ADD(EX_EI_ED_MISSING_CERT);
  ADD(EX_EI_ED_BAD_CERT1);
  ADD(EX_EI_ED_BAD_CERT2);
  ADD(EX_EI_ED_BAD_SIG1);
  ADD(EX_EI_ED_BAD_SIG2);
  ADD(EX_EI_ED_MISPLACED_CERT);
  ADD(EX_EI_ED_MISPLACED_SIG);

  CHECK_OK(EX_EI_MINIMAL);
  tt_assert(!ei->pending_sig);
  CHECK_OK(EX_EI_MAXIMAL);
  tt_assert(!ei->pending_sig);
  CHECK_OK(EX_EI_GOOD_ED_EI);
  tt_assert(!ei->pending_sig);

  CHECK_FAIL(EX_EI_BAD_SIG1,1);
  CHECK_FAIL(EX_EI_BAD_SIG2,1);
  CHECK_FAIL(EX_EI_BAD_SIG3,1);
  CHECK_FAIL(EX_EI_BAD_FP,0);
  CHECK_FAIL(EX_EI_BAD_NICKNAME,0);
  CHECK_FAIL(EX_EI_BAD_TOKENS,0);
  CHECK_FAIL(EX_EI_BAD_START,0);
  CHECK_FAIL(EX_EI_BAD_PUBLISHED,0);

  CHECK_FAIL(EX_EI_ED_MISSING_SIG,0);
  CHECK_FAIL(EX_EI_ED_MISSING_CERT,0);
  CHECK_FAIL(EX_EI_ED_BAD_CERT1,0);
  CHECK_FAIL(EX_EI_ED_BAD_CERT2,0);
  CHECK_FAIL(EX_EI_ED_BAD_SIG1,0);
  CHECK_FAIL(EX_EI_ED_BAD_SIG2,0);
  CHECK_FAIL(EX_EI_ED_MISPLACED_CERT,0);
  CHECK_FAIL(EX_EI_ED_MISPLACED_SIG,0);

#undef CHECK_OK
#undef CHECK_FAIL

 done:
  escaped(NULL);
  extrainfo_free(ei);
  routerinfo_free(ri);
  digestmap_free((digestmap_t*)map, routerinfo_free_wrapper_);
}

static void
test_dir_parse_router_list(void *arg)
{
  (void) arg;
  smartlist_t *invalid = smartlist_new();
  smartlist_t *dest = smartlist_new();
  smartlist_t *chunks = smartlist_new();
  int dest_has_ri = 1;
  char *list = NULL;
  const char *cp;
  digestmap_t *map = NULL;
  char *mem_op_hex_tmp = NULL;
  routerinfo_t *ri = NULL;
  char d[DIGEST_LEN];

  smartlist_add(chunks, tor_strdup(EX_RI_MINIMAL));     // ri 0
  smartlist_add(chunks, tor_strdup(EX_RI_BAD_PORTS));   // bad ri 0
  smartlist_add(chunks, tor_strdup(EX_EI_MAXIMAL));     // ei 0
  smartlist_add(chunks, tor_strdup(EX_EI_BAD_SIG2));    // bad ei --
  smartlist_add(chunks, tor_strdup(EX_EI_BAD_NICKNAME));// bad ei 0
  smartlist_add(chunks, tor_strdup(EX_RI_BAD_SIG1));    // bad ri --
  smartlist_add(chunks, tor_strdup(EX_EI_BAD_PUBLISHED));  // bad ei 1
  smartlist_add(chunks, tor_strdup(EX_RI_MAXIMAL));     // ri 1
  smartlist_add(chunks, tor_strdup(EX_RI_BAD_FAMILY));  // bad ri 1
  smartlist_add(chunks, tor_strdup(EX_EI_MINIMAL));     // ei 1

  list = smartlist_join_strings(chunks, "", 0, NULL);

  /* First, parse the routers. */
  cp = list;
  tt_int_op(0,OP_EQ,
            router_parse_list_from_string(&cp, NULL, dest, SAVED_NOWHERE,
                                          0, 0, NULL, invalid));
  tt_int_op(2, OP_EQ, smartlist_len(dest));
  tt_ptr_op(cp, OP_EQ, list + strlen(list));

  routerinfo_t *r = smartlist_get(dest, 0);
  tt_mem_op(r->cache_info.signed_descriptor_body, OP_EQ,
            EX_RI_MINIMAL, strlen(EX_RI_MINIMAL));
  r = smartlist_get(dest, 1);
  tt_mem_op(r->cache_info.signed_descriptor_body, OP_EQ,
            EX_RI_MAXIMAL, strlen(EX_RI_MAXIMAL));

  tt_int_op(2, OP_EQ, smartlist_len(invalid));
  test_memeq_hex(smartlist_get(invalid, 0),
                 "ab9eeaa95e7d45740185b4e519c76ead756277a9");
  test_memeq_hex(smartlist_get(invalid, 1),
                 "9a651ee03b64325959e8f1b46f2b689b30750b4c");

  /* Now tidy up */
  SMARTLIST_FOREACH(dest, routerinfo_t *, ri, routerinfo_free(ri));
  SMARTLIST_FOREACH(invalid, uint8_t *, d, tor_free(d));
  smartlist_clear(dest);
  smartlist_clear(invalid);

  /* And check extrainfos. */
  dest_has_ri = 0;
  map = (digestmap_t*)router_get_routerlist()->identity_map;
  ADD(EX_EI_MINIMAL);
  ADD(EX_EI_MAXIMAL);
  ADD(EX_EI_BAD_NICKNAME);
  ADD(EX_EI_BAD_PUBLISHED);
  cp = list;
  tt_int_op(0,OP_EQ,
            router_parse_list_from_string(&cp, NULL, dest, SAVED_NOWHERE,
                                          1, 0, NULL, invalid));
  tt_int_op(2, OP_EQ, smartlist_len(dest));
  extrainfo_t *e = smartlist_get(dest, 0);
  tt_mem_op(e->cache_info.signed_descriptor_body, OP_EQ,
            EX_EI_MAXIMAL, strlen(EX_EI_MAXIMAL));
  e = smartlist_get(dest, 1);
  tt_mem_op(e->cache_info.signed_descriptor_body, OP_EQ,
            EX_EI_MINIMAL, strlen(EX_EI_MINIMAL));

  tt_int_op(2, OP_EQ, smartlist_len(invalid));
  test_memeq_hex(smartlist_get(invalid, 0),
                 "d5df4aa62ee9ffc9543d41150c9864908e0390af");
  test_memeq_hex(smartlist_get(invalid, 1),
                 "f61efd2a7f4531f3687a9043e0de90a862ec64ba");

 done:
  tor_free(list);
  if (dest_has_ri)
    SMARTLIST_FOREACH(dest, routerinfo_t *, rt, routerinfo_free(rt));
  else
    SMARTLIST_FOREACH(dest, extrainfo_t *, ei, extrainfo_free(ei));
  smartlist_free(dest);
  SMARTLIST_FOREACH(invalid, uint8_t *, d, tor_free(d));
  smartlist_free(invalid);
  SMARTLIST_FOREACH(chunks, char *, cp, tor_free(cp));
  smartlist_free(chunks);
  routerinfo_free(ri);
  if (map) {
    digestmap_free((digestmap_t*)map, routerinfo_free_wrapper_);
    router_get_routerlist()->identity_map =
      (struct digest_ri_map_t*)digestmap_new();
  }
  tor_free(mem_op_hex_tmp);

#undef ADD
}

static download_status_t dls_minimal;
static download_status_t dls_maximal;
static download_status_t dls_bad_fingerprint;
static download_status_t dls_bad_sig2;
static download_status_t dls_bad_ports;
static download_status_t dls_bad_tokens;

static int mock_router_get_dl_status_unrecognized = 0;
static int mock_router_get_dl_status_calls = 0;

static download_status_t *
mock_router_get_dl_status(const char *d)
{
  ++mock_router_get_dl_status_calls;
  char hex[HEX_DIGEST_LEN+1];
  base16_encode(hex, sizeof(hex), d, DIGEST_LEN);
  if (!strcmp(hex, "3E31D19A69EB719C00B02EC60D13356E3F7A3452")) {
    return &dls_minimal;
  } else if (!strcmp(hex, "581D8A368A0FA854ECDBFAB841D88B3F1B004038")) {
    return &dls_maximal;
  } else if (!strcmp(hex, "2578AE227C6116CDE29B3F0E95709B9872DEE5F1")) {
    return &dls_bad_fingerprint;
  } else if (!strcmp(hex, "16D387D3A58F7DB3CF46638F8D0B90C45C7D769C")) {
    return &dls_bad_sig2;
  } else if (!strcmp(hex, "AB9EEAA95E7D45740185B4E519C76EAD756277A9")) {
    return &dls_bad_ports;
  } else if (!strcmp(hex, "A0CC2CEFAD59DBF19F468BFEE60E0868C804B422")) {
    return &dls_bad_tokens;
  } else {
    ++mock_router_get_dl_status_unrecognized;
    return NULL;
  }
}

static void
test_dir_load_routers(void *arg)
{
  (void) arg;
  smartlist_t *chunks = smartlist_new();
  smartlist_t *wanted = smartlist_new();
  char buf[DIGEST_LEN];
  char *mem_op_hex_tmp = NULL;
  char *list = NULL;

#define ADD(str)                                                        \
  do {                                                                  \
    tt_int_op(0,OP_EQ,router_get_router_hash(str, strlen(str), buf));      \
    smartlist_add(wanted, tor_strdup(hex_str(buf, DIGEST_LEN)));        \
  } while (0)

  MOCK(router_get_dl_status_by_descriptor_digest, mock_router_get_dl_status);

  update_approx_time(1412510400);

  smartlist_add(chunks, tor_strdup(EX_RI_MINIMAL));
  smartlist_add(chunks, tor_strdup(EX_RI_BAD_FINGERPRINT));
  smartlist_add(chunks, tor_strdup(EX_RI_BAD_SIG2));
  smartlist_add(chunks, tor_strdup(EX_RI_MAXIMAL));
  smartlist_add(chunks, tor_strdup(EX_RI_BAD_PORTS));
  smartlist_add(chunks, tor_strdup(EX_RI_BAD_TOKENS));

  /* not ADDing MINIMIAL */
  ADD(EX_RI_MAXIMAL);
  ADD(EX_RI_BAD_FINGERPRINT);
  ADD(EX_RI_BAD_SIG2);
  /* Not ADDing BAD_PORTS */
  ADD(EX_RI_BAD_TOKENS);

  list = smartlist_join_strings(chunks, "", 0, NULL);
  tt_int_op(1, OP_EQ,
            router_load_routers_from_string(list, NULL, SAVED_IN_JOURNAL,
                                            wanted, 1, NULL));

  /* The "maximal" router was added. */
  /* "minimal" was not. */
  tt_int_op(smartlist_len(router_get_routerlist()->routers),OP_EQ,1);
  routerinfo_t *r = smartlist_get(router_get_routerlist()->routers, 0);
  test_memeq_hex(r->cache_info.signed_descriptor_digest,
                 "581D8A368A0FA854ECDBFAB841D88B3F1B004038");
  tt_int_op(dls_minimal.n_download_failures, OP_EQ, 0);
  tt_int_op(dls_maximal.n_download_failures, OP_EQ, 0);

  /* "Bad fingerprint" and "Bad tokens" should have gotten marked
   * non-retriable. */
  tt_want_int_op(mock_router_get_dl_status_calls, OP_EQ, 2);
  tt_want_int_op(mock_router_get_dl_status_unrecognized, OP_EQ, 0);
  tt_int_op(dls_bad_fingerprint.n_download_failures, OP_EQ, 255);
  tt_int_op(dls_bad_tokens.n_download_failures, OP_EQ, 255);

  /* bad_sig2 and bad ports" are retriable -- one since only the signature
   * was bad, and one because we didn't ask for it. */
  tt_int_op(dls_bad_sig2.n_download_failures, OP_EQ, 0);
  tt_int_op(dls_bad_ports.n_download_failures, OP_EQ, 0);

  /* Wanted still contains "BAD_SIG2" */
  tt_int_op(smartlist_len(wanted), OP_EQ, 1);
  tt_str_op(smartlist_get(wanted, 0), OP_EQ,
            "E0A3753CEFD54128EAB239F294954121DB23D2EF");

#undef ADD

 done:
  tor_free(mem_op_hex_tmp);
  UNMOCK(router_get_dl_status_by_descriptor_digest);
  SMARTLIST_FOREACH(chunks, char *, cp, tor_free(cp));
  smartlist_free(chunks);
  SMARTLIST_FOREACH(wanted, char *, cp, tor_free(cp));
  smartlist_free(wanted);
  tor_free(list);
}

static int mock_get_by_ei_dd_calls = 0;
static int mock_get_by_ei_dd_unrecognized = 0;

static signed_descriptor_t sd_ei_minimal;
static signed_descriptor_t sd_ei_bad_nickname;
static signed_descriptor_t sd_ei_maximal;
static signed_descriptor_t sd_ei_bad_tokens;
static signed_descriptor_t sd_ei_bad_sig2;

static signed_descriptor_t *
mock_get_by_ei_desc_digest(const char *d)
{

  ++mock_get_by_ei_dd_calls;
  char hex[HEX_DIGEST_LEN+1];
  base16_encode(hex, sizeof(hex), d, DIGEST_LEN);

  if (!strcmp(hex, "11E0EDF526950739F7769810FCACAB8C882FAEEE")) {
    return &sd_ei_minimal;
  } else if (!strcmp(hex, "47803B02A0E70E9E8BDA226CB1D74DE354D67DFF")) {
    return &sd_ei_maximal;
  } else if (!strcmp(hex, "D5DF4AA62EE9FFC9543D41150C9864908E0390AF")) {
    return &sd_ei_bad_nickname;
  } else if (!strcmp(hex, "16D387D3A58F7DB3CF46638F8D0B90C45C7D769C")) {
    return &sd_ei_bad_sig2;
  } else if (!strcmp(hex, "9D90F8C42955BBC57D54FB05E54A3F083AF42E8B")) {
    return &sd_ei_bad_tokens;
  } else {
    ++mock_get_by_ei_dd_unrecognized;
    return NULL;
  }
}

static smartlist_t *mock_ei_insert_list = NULL;
static was_router_added_t
mock_ei_insert(routerlist_t *rl, extrainfo_t *ei, int warn_if_incompatible)
{
  (void) rl;
  (void) warn_if_incompatible;
  smartlist_add(mock_ei_insert_list, ei);
  return ROUTER_ADDED_SUCCESSFULLY;
}

static void
test_dir_load_extrainfo(void *arg)
{
  (void) arg;
  smartlist_t *chunks = smartlist_new();
  smartlist_t *wanted = smartlist_new();
  char buf[DIGEST_LEN];
  char *mem_op_hex_tmp = NULL;
  char *list = NULL;

#define ADD(str)                                                        \
  do {                                                                  \
    tt_int_op(0,OP_EQ,router_get_extrainfo_hash(str, strlen(str), buf));   \
    smartlist_add(wanted, tor_strdup(hex_str(buf, DIGEST_LEN)));        \
  } while (0)

  mock_ei_insert_list = smartlist_new();
  MOCK(router_get_by_extrainfo_digest, mock_get_by_ei_desc_digest);
  MOCK(extrainfo_insert, mock_ei_insert);

  smartlist_add(chunks, tor_strdup(EX_EI_MINIMAL));
  smartlist_add(chunks, tor_strdup(EX_EI_BAD_NICKNAME));
  smartlist_add(chunks, tor_strdup(EX_EI_MAXIMAL));
  smartlist_add(chunks, tor_strdup(EX_EI_BAD_PUBLISHED));
  smartlist_add(chunks, tor_strdup(EX_EI_BAD_TOKENS));

  /* not ADDing MINIMIAL */
  ADD(EX_EI_MAXIMAL);
  ADD(EX_EI_BAD_NICKNAME);
  /* Not ADDing BAD_PUBLISHED */
  ADD(EX_EI_BAD_TOKENS);
  ADD(EX_EI_BAD_SIG2);

  list = smartlist_join_strings(chunks, "", 0, NULL);
  router_load_extrainfo_from_string(list, NULL, SAVED_IN_JOURNAL, wanted, 1);

  /* The "maximal" router was added. */
  /* "minimal" was also added, even though we didn't ask for it, since
   * that's what we do with extrainfos. */
  tt_int_op(smartlist_len(mock_ei_insert_list),OP_EQ,2);

  extrainfo_t *e = smartlist_get(mock_ei_insert_list, 0);
  test_memeq_hex(e->cache_info.signed_descriptor_digest,
                 "11E0EDF526950739F7769810FCACAB8C882FAEEE");

  e = smartlist_get(mock_ei_insert_list, 1);
  test_memeq_hex(e->cache_info.signed_descriptor_digest,
                 "47803B02A0E70E9E8BDA226CB1D74DE354D67DFF");
  tt_int_op(dls_minimal.n_download_failures, OP_EQ, 0);
  tt_int_op(dls_maximal.n_download_failures, OP_EQ, 0);

  /* "Bad nickname" and "Bad tokens" should have gotten marked
   * non-retriable. */
  tt_want_int_op(mock_get_by_ei_dd_calls, OP_EQ, 2);
  tt_want_int_op(mock_get_by_ei_dd_unrecognized, OP_EQ, 0);
  tt_int_op(sd_ei_bad_nickname.ei_dl_status.n_download_failures, OP_EQ, 255);
  tt_int_op(sd_ei_bad_tokens.ei_dl_status.n_download_failures, OP_EQ, 255);

  /* bad_ports is retriable -- because we didn't ask for it. */
  tt_int_op(dls_bad_ports.n_download_failures, OP_EQ, 0);

  /* Wanted still contains "BAD_SIG2" */
  tt_int_op(smartlist_len(wanted), OP_EQ, 1);
  tt_str_op(smartlist_get(wanted, 0), OP_EQ,
            "16D387D3A58F7DB3CF46638F8D0B90C45C7D769C");

#undef ADD

 done:
  tor_free(mem_op_hex_tmp);
  UNMOCK(router_get_by_extrainfo_digest);
  SMARTLIST_FOREACH(chunks, char *, cp, tor_free(cp));
  smartlist_free(chunks);
  SMARTLIST_FOREACH(wanted, char *, cp, tor_free(cp));
  smartlist_free(wanted);
  tor_free(list);
}

static void
test_dir_versions(void *arg)
{
  tor_version_t ver1;

  /* Try out version parsing functionality */
  (void)arg;
  tt_int_op(0,OP_EQ, tor_version_parse("0.3.4pre2-cvs", &ver1));
  tt_int_op(0,OP_EQ, ver1.major);
  tt_int_op(3,OP_EQ, ver1.minor);
  tt_int_op(4,OP_EQ, ver1.micro);
  tt_int_op(VER_PRE,OP_EQ, ver1.status);
  tt_int_op(2,OP_EQ, ver1.patchlevel);
  tt_int_op(0,OP_EQ, tor_version_parse("0.3.4rc1", &ver1));
  tt_int_op(0,OP_EQ, ver1.major);
  tt_int_op(3,OP_EQ, ver1.minor);
  tt_int_op(4,OP_EQ, ver1.micro);
  tt_int_op(VER_RC,OP_EQ, ver1.status);
  tt_int_op(1,OP_EQ, ver1.patchlevel);
  tt_int_op(0,OP_EQ, tor_version_parse("1.3.4", &ver1));
  tt_int_op(1,OP_EQ, ver1.major);
  tt_int_op(3,OP_EQ, ver1.minor);
  tt_int_op(4,OP_EQ, ver1.micro);
  tt_int_op(VER_RELEASE,OP_EQ, ver1.status);
  tt_int_op(0,OP_EQ, ver1.patchlevel);
  tt_int_op(0,OP_EQ, tor_version_parse("1.3.4.999", &ver1));
  tt_int_op(1,OP_EQ, ver1.major);
  tt_int_op(3,OP_EQ, ver1.minor);
  tt_int_op(4,OP_EQ, ver1.micro);
  tt_int_op(VER_RELEASE,OP_EQ, ver1.status);
  tt_int_op(999,OP_EQ, ver1.patchlevel);
  tt_int_op(0,OP_EQ, tor_version_parse("0.1.2.4-alpha", &ver1));
  tt_int_op(0,OP_EQ, ver1.major);
  tt_int_op(1,OP_EQ, ver1.minor);
  tt_int_op(2,OP_EQ, ver1.micro);
  tt_int_op(4,OP_EQ, ver1.patchlevel);
  tt_int_op(VER_RELEASE,OP_EQ, ver1.status);
  tt_str_op("alpha",OP_EQ, ver1.status_tag);
  tt_int_op(0,OP_EQ, tor_version_parse("0.1.2.4", &ver1));
  tt_int_op(0,OP_EQ, ver1.major);
  tt_int_op(1,OP_EQ, ver1.minor);
  tt_int_op(2,OP_EQ, ver1.micro);
  tt_int_op(4,OP_EQ, ver1.patchlevel);
  tt_int_op(VER_RELEASE,OP_EQ, ver1.status);
  tt_str_op("",OP_EQ, ver1.status_tag);

  tt_int_op(0, OP_EQ, tor_version_parse("10.1", &ver1));
  tt_int_op(10, OP_EQ, ver1.major);
  tt_int_op(1, OP_EQ, ver1.minor);
  tt_int_op(0, OP_EQ, ver1.micro);
  tt_int_op(0, OP_EQ, ver1.patchlevel);
  tt_int_op(VER_RELEASE, OP_EQ, ver1.status);
  tt_str_op("", OP_EQ, ver1.status_tag);
  tt_int_op(0, OP_EQ, tor_version_parse("5.99.999", &ver1));
  tt_int_op(5, OP_EQ, ver1.major);
  tt_int_op(99, OP_EQ, ver1.minor);
  tt_int_op(999, OP_EQ, ver1.micro);
  tt_int_op(0, OP_EQ, ver1.patchlevel);
  tt_int_op(VER_RELEASE, OP_EQ, ver1.status);
  tt_str_op("", OP_EQ, ver1.status_tag);
  tt_int_op(0, OP_EQ, tor_version_parse("10.1-alpha", &ver1));
  tt_int_op(10, OP_EQ, ver1.major);
  tt_int_op(1, OP_EQ, ver1.minor);
  tt_int_op(0, OP_EQ, ver1.micro);
  tt_int_op(0, OP_EQ, ver1.patchlevel);
  tt_int_op(VER_RELEASE, OP_EQ, ver1.status);
  tt_str_op("alpha", OP_EQ, ver1.status_tag);
  tt_int_op(0, OP_EQ, tor_version_parse("2.1.700-alpha", &ver1));
  tt_int_op(2, OP_EQ, ver1.major);
  tt_int_op(1, OP_EQ, ver1.minor);
  tt_int_op(700, OP_EQ, ver1.micro);
  tt_int_op(0, OP_EQ, ver1.patchlevel);
  tt_int_op(VER_RELEASE, OP_EQ, ver1.status);
  tt_str_op("alpha", OP_EQ, ver1.status_tag);
  tt_int_op(0, OP_EQ, tor_version_parse("1.6.8-alpha-dev", &ver1));
  tt_int_op(1, OP_EQ, ver1.major);
  tt_int_op(6, OP_EQ, ver1.minor);
  tt_int_op(8, OP_EQ, ver1.micro);
  tt_int_op(0, OP_EQ, ver1.patchlevel);
  tt_int_op(VER_RELEASE, OP_EQ, ver1.status);
  tt_str_op("alpha-dev", OP_EQ, ver1.status_tag);

#define tt_versionstatus_op(vs1, op, vs2)                               \
  tt_assert_test_type(vs1,vs2,#vs1" "#op" "#vs2,version_status_t,       \
                      (val1_ op val2_),"%d",TT_EXIT_TEST_FUNCTION)
#define test_v_i_o(val, ver, lst)                                       \
  tt_versionstatus_op(val, OP_EQ, tor_version_is_obsolete(ver, lst))

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
  tt_int_op(0,OP_EQ, tor_version_as_new_as("Tor 0.0.5", "0.0.9pre1-cvs"));
  tt_int_op(1,OP_EQ, tor_version_as_new_as(
          "Tor 0.0.8 on Darwin 64-121-192-100.c3-0."
          "sfpo-ubr1.sfrn-sfpo.ca.cable.rcn.com Power Macintosh",
          "0.0.8rc2"));
  tt_int_op(0,OP_EQ, tor_version_as_new_as(
          "Tor 0.0.8 on Darwin 64-121-192-100.c3-0."
          "sfpo-ubr1.sfrn-sfpo.ca.cable.rcn.com Power Macintosh", "0.0.8.2"));

  /* Now try svn revisions. */
  tt_int_op(1,OP_EQ, tor_version_as_new_as("Tor 0.2.1.0-dev (r100)",
                                   "Tor 0.2.1.0-dev (r99)"));
  tt_int_op(1,OP_EQ, tor_version_as_new_as(
                                   "Tor 0.2.1.0-dev (r100) on Banana Jr",
                                   "Tor 0.2.1.0-dev (r99) on Hal 9000"));
  tt_int_op(1,OP_EQ, tor_version_as_new_as("Tor 0.2.1.0-dev (r100)",
                                   "Tor 0.2.1.0-dev on Colossus"));
  tt_int_op(0,OP_EQ, tor_version_as_new_as("Tor 0.2.1.0-dev (r99)",
                                   "Tor 0.2.1.0-dev (r100)"));
  tt_int_op(0,OP_EQ, tor_version_as_new_as("Tor 0.2.1.0-dev (r99) on MCP",
                                   "Tor 0.2.1.0-dev (r100) on AM"));
  tt_int_op(0,OP_EQ, tor_version_as_new_as("Tor 0.2.1.0-dev",
                                   "Tor 0.2.1.0-dev (r99)"));
  tt_int_op(1,OP_EQ, tor_version_as_new_as("Tor 0.2.1.1",
                                   "Tor 0.2.1.0-dev (r99)"));

  /* Now try git revisions */
  tt_int_op(0,OP_EQ, tor_version_parse("0.5.6.7 (git-ff00ff)", &ver1));
  tt_int_op(0,OP_EQ, ver1.major);
  tt_int_op(5,OP_EQ, ver1.minor);
  tt_int_op(6,OP_EQ, ver1.micro);
  tt_int_op(7,OP_EQ, ver1.patchlevel);
  tt_int_op(3,OP_EQ, ver1.git_tag_len);
  tt_mem_op(ver1.git_tag,OP_EQ, "\xff\x00\xff", 3);
  tt_int_op(-1,OP_EQ, tor_version_parse("0.5.6.7 (git-ff00xx)", &ver1));
  tt_int_op(-1,OP_EQ, tor_version_parse("0.5.6.7 (git-ff00fff)", &ver1));
  tt_int_op(0,OP_EQ, tor_version_parse("0.5.6.7 (git ff00fff)", &ver1));
 done:
  ;
}

/** Run unit tests for directory fp_pair functions. */
static void
test_dir_fp_pairs(void *arg)
{
  smartlist_t *sl = smartlist_new();
  fp_pair_t *pair;

  (void)arg;
  dir_split_resource_into_fingerprint_pairs(
       /* Two pairs, out of order, with one duplicate. */
       "73656372657420646174612E0000000000FFFFFF-"
       "557365204145532d32353620696e73746561642e+"
       "73656372657420646174612E0000000000FFFFFF-"
       "557365204145532d32353620696e73746561642e+"
       "48657861646563696d616c2069736e277420736f-"
       "676f6f6420666f7220686964696e6720796f7572.z", sl);

  tt_int_op(smartlist_len(sl),OP_EQ, 2);
  pair = smartlist_get(sl, 0);
  tt_mem_op(pair->first,OP_EQ,  "Hexadecimal isn't so", DIGEST_LEN);
  tt_mem_op(pair->second,OP_EQ, "good for hiding your", DIGEST_LEN);
  pair = smartlist_get(sl, 1);
  tt_mem_op(pair->first,OP_EQ,  "secret data.\0\0\0\0\0\xff\xff\xff",
            DIGEST_LEN);
  tt_mem_op(pair->second,OP_EQ, "Use AES-256 instead.", DIGEST_LEN);

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
  tt_int_op(smartlist_len(sl), OP_EQ, 3);
  tt_str_op(smartlist_get(sl, 0), OP_EQ, "A");
  tt_str_op(smartlist_get(sl, 1), OP_EQ, "C");
  tt_str_op(smartlist_get(sl, 2), OP_EQ, "B");
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* uniq strings. */
  dir_split_resource_into_fingerprints("A+C+B+A+B+B", sl, NULL, DSR_SORT_UNIQ);
  tt_int_op(smartlist_len(sl), OP_EQ, 3);
  tt_str_op(smartlist_get(sl, 0), OP_EQ, "A");
  tt_str_op(smartlist_get(sl, 1), OP_EQ, "B");
  tt_str_op(smartlist_get(sl, 2), OP_EQ, "C");
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode hex. */
  dir_split_resource_into_fingerprints(HEX1"+"HEX2, sl, NULL, DSR_HEX);
  tt_int_op(smartlist_len(sl), OP_EQ, 2);
  test_mem_op_hex(smartlist_get(sl, 0), OP_EQ, HEX1);
  test_mem_op_hex(smartlist_get(sl, 1), OP_EQ, HEX2);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* decode hex and drop weirdness. */
  dir_split_resource_into_fingerprints(HEX1"+bogus+"HEX2"+"HEX256_1,
                                       sl, NULL, DSR_HEX);
  tt_int_op(smartlist_len(sl), OP_EQ, 2);
  test_mem_op_hex(smartlist_get(sl, 0), OP_EQ, HEX1);
  test_mem_op_hex(smartlist_get(sl, 1), OP_EQ, HEX2);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode long hex */
  dir_split_resource_into_fingerprints(HEX256_1"+"HEX256_2"+"HEX2"+"HEX256_3,
                                       sl, NULL, DSR_HEX|DSR_DIGEST256);
  tt_int_op(smartlist_len(sl), OP_EQ, 3);
  test_mem_op_hex(smartlist_get(sl, 0), OP_EQ, HEX256_1);
  test_mem_op_hex(smartlist_get(sl, 1), OP_EQ, HEX256_2);
  test_mem_op_hex(smartlist_get(sl, 2), OP_EQ, HEX256_3);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode hex and sort. */
  dir_split_resource_into_fingerprints(HEX1"+"HEX2"+"HEX3"+"HEX2,
                                       sl, NULL, DSR_HEX|DSR_SORT_UNIQ);
  tt_int_op(smartlist_len(sl), OP_EQ, 3);
  test_mem_op_hex(smartlist_get(sl, 0), OP_EQ, HEX3);
  test_mem_op_hex(smartlist_get(sl, 1), OP_EQ, HEX2);
  test_mem_op_hex(smartlist_get(sl, 2), OP_EQ, HEX1);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode long hex and sort */
  dir_split_resource_into_fingerprints(HEX256_1"+"HEX256_2"+"HEX256_3
                                       "+"HEX256_1,
                                       sl, NULL,
                                       DSR_HEX|DSR_DIGEST256|DSR_SORT_UNIQ);
  tt_int_op(smartlist_len(sl), OP_EQ, 3);
  test_mem_op_hex(smartlist_get(sl, 0), OP_EQ, HEX256_3);
  test_mem_op_hex(smartlist_get(sl, 1), OP_EQ, HEX256_2);
  test_mem_op_hex(smartlist_get(sl, 2), OP_EQ, HEX256_1);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode base64 */
  dir_split_resource_into_fingerprints(B64_1"-"B64_2, sl, NULL, DSR_BASE64);
  tt_int_op(smartlist_len(sl), OP_EQ, 2);
  test_mem_op_hex(smartlist_get(sl, 0), OP_EQ, HEX1);
  test_mem_op_hex(smartlist_get(sl, 1), OP_EQ, HEX2);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  /* Decode long base64 */
  dir_split_resource_into_fingerprints(B64_256_1"-"B64_256_2,
                                       sl, NULL, DSR_BASE64|DSR_DIGEST256);
  tt_int_op(smartlist_len(sl), OP_EQ, 2);
  test_mem_op_hex(smartlist_get(sl, 0), OP_EQ, HEX256_1);
  test_mem_op_hex(smartlist_get(sl, 1), OP_EQ, HEX256_2);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

  dir_split_resource_into_fingerprints(B64_256_1,
                                       sl, NULL, DSR_BASE64|DSR_DIGEST256);
  tt_int_op(smartlist_len(sl), OP_EQ, 1);
  test_mem_op_hex(smartlist_get(sl, 0), OP_EQ, HEX256_1);
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_clear(sl);

 done:
  SMARTLIST_FOREACH(sl, char *, cp, tor_free(cp));
  smartlist_free(sl);
  tor_free(mem_op_hex_tmp);
}

static void
test_dir_measured_bw_kb(void *arg)
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

  (void)arg;
  for (i = 0; strcmp(lines_fail[i], "end"); i++) {
    //fprintf(stderr, "Testing: %s\n", lines_fail[i]);
    tt_assert(measured_bw_line_parse(&mbwl, lines_fail[i]) == -1);
  }

  for (i = 0; strcmp(lines_pass[i], "end"); i++) {
    //fprintf(stderr, "Testing: %s %d\n", lines_pass[i], TOR_ISSPACE('\n'));
    tt_assert(measured_bw_line_parse(&mbwl, lines_pass[i]) == 0);
    tt_assert(mbwl.bw_kb == 1024);
    tt_assert(strcmp(mbwl.node_hex,
                "557365204145532d32353620696e73746561642e") == 0);
  }

 done:
  return;
}

#define MBWC_INIT_TIME 1000

/** Do the measured bandwidth cache unit test */
static void
test_dir_measured_bw_kb_cache(void *arg)
{
  /* Initial fake time_t for testing */
  time_t curr = MBWC_INIT_TIME;
  /* Some measured_bw_line_ts */
  measured_bw_line_t mbwl[3];
  /* For receiving output on cache queries */
  long bw;
  time_t as_of;

  /* First, clear the cache and assert that it's empty */
  (void)arg;
  dirserv_clear_measured_bw_cache();
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 0);
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
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 1);
  /* Okay, let's see if we can retrieve it */
  tt_assert(dirserv_query_measured_bw_cache_kb(mbwl[0].node_id,&bw, &as_of));
  tt_int_op(bw,OP_EQ, 20);
  tt_int_op(as_of,OP_EQ, MBWC_INIT_TIME);
  /* Try retrieving it without some outputs */
  tt_assert(dirserv_query_measured_bw_cache_kb(mbwl[0].node_id,NULL, NULL));
  tt_assert(dirserv_query_measured_bw_cache_kb(mbwl[0].node_id,&bw, NULL));
  tt_int_op(bw,OP_EQ, 20);
  tt_assert(dirserv_query_measured_bw_cache_kb(mbwl[0].node_id,NULL,&as_of));
  tt_int_op(as_of,OP_EQ, MBWC_INIT_TIME);
  /* Now expire it */
  curr += MAX_MEASUREMENT_AGE + 1;
  dirserv_expire_measured_bw_cache(curr);
  /* Check that the cache is empty */
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 0);
  /* Check that we can't retrieve it */
  tt_assert(!dirserv_query_measured_bw_cache_kb(mbwl[0].node_id, NULL,NULL));
  /* Try caching a few things now */
  dirserv_cache_measured_bw(&(mbwl[0]), curr);
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 1);
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_cache_measured_bw(&(mbwl[1]), curr);
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 2);
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_cache_measured_bw(&(mbwl[2]), curr);
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 3);
  curr += MAX_MEASUREMENT_AGE / 4 + 1;
  /* Do an expire that's too soon to get any of them */
  dirserv_expire_measured_bw_cache(curr);
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 3);
  /* Push the oldest one off the cliff */
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_expire_measured_bw_cache(curr);
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 2);
  /* And another... */
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_expire_measured_bw_cache(curr);
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 1);
  /* This should empty it out again */
  curr += MAX_MEASUREMENT_AGE / 4;
  dirserv_expire_measured_bw_cache(curr);
  tt_int_op(dirserv_get_measured_bw_cache_size(),OP_EQ, 0);

 done:
  return;
}

static void
test_dir_param_voting(void *arg)
{
  networkstatus_t vote1, vote2, vote3, vote4;
  smartlist_t *votes = smartlist_new();
  char *res = NULL;

  /* dirvote_compute_params only looks at the net_params field of the votes,
     so that's all we need to set.
   */
  (void)arg;
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
  tt_int_op(100,OP_EQ, networkstatus_get_param(&vote4, "x-yz", 50, 0, 300));
  tt_int_op(222,OP_EQ, networkstatus_get_param(&vote4, "foobar", 222, 0, 300));
  tt_int_op(80,OP_EQ, networkstatus_get_param(&vote4, "ab", 12, 0, 80));
  tt_int_op(-8,OP_EQ, networkstatus_get_param(&vote4, "ab", -12, -100, -8));
  tt_int_op(0,OP_EQ, networkstatus_get_param(&vote4, "foobar", 0, -100, 8));

  smartlist_add(votes, &vote1);

  /* Do the first tests without adding all the other votes, for
   * networks without many dirauths. */

  res = dirvote_compute_params(votes, 12, 2);
  tt_str_op(res,OP_EQ, "");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 1);
  tt_str_op(res,OP_EQ, "ab=90 abcd=20 cw=50 x-yz=-99");
  tor_free(res);

  smartlist_add(votes, &vote2);

  res = dirvote_compute_params(votes, 12, 2);
  tt_str_op(res,OP_EQ, "ab=27 cw=5 x-yz=-99");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 3);
  tt_str_op(res,OP_EQ, "ab=27 cw=5 x-yz=-99");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 6);
  tt_str_op(res,OP_EQ, "");
  tor_free(res);

  smartlist_add(votes, &vote3);

  res = dirvote_compute_params(votes, 12, 3);
  tt_str_op(res,OP_EQ, "ab=27 abcd=20 cw=50 x-yz=-9");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 5);
  tt_str_op(res,OP_EQ, "cw=50 x-yz=-9");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 9);
  tt_str_op(res,OP_EQ, "cw=50 x-yz=-9");
  tor_free(res);

  smartlist_add(votes, &vote4);

  res = dirvote_compute_params(votes, 12, 4);
  tt_str_op(res,OP_EQ, "ab=90 abcd=20 cw=50 x-yz=-9");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 5);
  tt_str_op(res,OP_EQ, "ab=90 abcd=20 cw=50 x-yz=-9");
  tor_free(res);

  /* Test that the special-cased "at least three dirauths voted for
   * this param" logic works as expected. */
  res = dirvote_compute_params(votes, 12, 6);
  tt_str_op(res,OP_EQ, "ab=90 abcd=20 cw=50 x-yz=-9");
  tor_free(res);

  res = dirvote_compute_params(votes, 12, 10);
  tt_str_op(res,OP_EQ, "ab=90 abcd=20 cw=50 x-yz=-9");
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

/** Helper: Test that two networkstatus_voter_info_t do in fact represent the
 * same voting authority, and that they do in fact have all the same
 * information. */
static void
test_same_voter(networkstatus_voter_info_t *v1,
                networkstatus_voter_info_t *v2)
{
  tt_str_op(v1->nickname,OP_EQ, v2->nickname);
  tt_mem_op(v1->identity_digest,OP_EQ, v2->identity_digest, DIGEST_LEN);
  tt_str_op(v1->address,OP_EQ, v2->address);
  tt_int_op(v1->addr,OP_EQ, v2->addr);
  tt_int_op(v1->dir_port,OP_EQ, v2->dir_port);
  tt_int_op(v1->or_port,OP_EQ, v2->or_port);
  tt_str_op(v1->contact,OP_EQ, v2->contact);
  tt_mem_op(v1->vote_digest,OP_EQ, v2->vote_digest, DIGEST_LEN);
 done:
  ;
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

/** Apply tweaks to the vote list for each voter */
static int
vote_tweaks_for_v3ns(networkstatus_t *v, int voter, time_t now)
{
  vote_routerstatus_t *vrs;
  const char *msg = NULL;

  tt_assert(v);
  (void)now;

  if (voter == 1) {
    measured_bw_line_t mbw;
    memset(mbw.node_id, 33, sizeof(mbw.node_id));
    mbw.bw_kb = 1024;
    tt_assert(measured_bw_line_apply(&mbw,
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
      tt_assert(router_add_to_routerlist(
                  dir_common_generate_ri_from_rs(vrs), &msg,0,0) >= 0);
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

  tt_assert(vrs);
  rs = &(vrs->status);
  tt_assert(rs);

  /* Split out by digests to test */
  if (tor_memeq(rs->identity_digest,
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
                "\x3\x3\x3\x3",
                DIGEST_LEN) &&
                (voter == 1)) {
    /* Check the first routerstatus. */
    tt_str_op(vrs->version,OP_EQ, "0.1.2.14");
    tt_int_op(rs->published_on,OP_EQ, now-1500);
    tt_str_op(rs->nickname,OP_EQ, "router2");
    tt_mem_op(rs->identity_digest,OP_EQ,
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
               "\x3\x3\x3\x3",
               DIGEST_LEN);
    tt_mem_op(rs->descriptor_digest,OP_EQ, "NNNNNNNNNNNNNNNNNNNN", DIGEST_LEN);
    tt_int_op(rs->addr,OP_EQ, 0x99008801);
    tt_int_op(rs->or_port,OP_EQ, 443);
    tt_int_op(rs->dir_port,OP_EQ, 8000);
    /* no flags except "running" (16) and "v2dir" (64) */
    tt_u64_op(vrs->flags, OP_EQ, U64_LITERAL(80));
  } else if (tor_memeq(rs->identity_digest,
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
                       "\x5\x5\x5\x5",
                       DIGEST_LEN) &&
                       (voter == 1 || voter == 2)) {
    tt_mem_op(rs->identity_digest,OP_EQ,
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
               "\x5\x5\x5\x5",
               DIGEST_LEN);

    if (voter == 1) {
      /* Check the second routerstatus. */
      tt_str_op(vrs->version,OP_EQ, "0.2.0.5");
      tt_int_op(rs->published_on,OP_EQ, now-1000);
      tt_str_op(rs->nickname,OP_EQ, "router1");
    }
    tt_mem_op(rs->descriptor_digest,OP_EQ, "MMMMMMMMMMMMMMMMMMMM", DIGEST_LEN);
    tt_int_op(rs->addr,OP_EQ, 0x99009901);
    tt_int_op(rs->or_port,OP_EQ, 443);
    tt_int_op(rs->dir_port,OP_EQ, 0);
    tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
    tt_assert(tor_addr_eq(&rs->ipv6_addr, &addr_ipv6));
    tt_int_op(rs->ipv6_orport,OP_EQ, 4711);
    if (voter == 1) {
      /* all except "authority" (1) */
      tt_u64_op(vrs->flags, OP_EQ, U64_LITERAL(254));
    } else {
      /* 1023 - authority(1) - madeofcheese(16) - madeoftin(32) */
      tt_u64_op(vrs->flags, OP_EQ, U64_LITERAL(974));
    }
  } else if (tor_memeq(rs->identity_digest,
                       "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33"
                       "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33",
                       DIGEST_LEN) &&
                       (voter == 1 || voter == 2)) {
    /* Check the measured bandwidth bits */
    tt_assert(vrs->has_measured_bw &&
                vrs->measured_bw_kb == 1024);
  } else {
    /*
     * Didn't expect this, but the old unit test only checked some of them,
     * so don't assert.
     */
    /* tt_assert(0); */
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

  tt_assert(con);
  tt_assert(!con->cert);
  tt_int_op(2,OP_EQ, smartlist_len(con->routerstatus_list));
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

  tt_assert(rs);

  /* There should be two listed routers: one with identity 3, one with
   * identity 5. */
  /* This one showed up in 2 digests. */
  if (tor_memeq(rs->identity_digest,
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
                "\x3\x3",
                DIGEST_LEN)) {
    tt_mem_op(rs->identity_digest,OP_EQ,
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
               DIGEST_LEN);
    tt_mem_op(rs->descriptor_digest,OP_EQ, "NNNNNNNNNNNNNNNNNNNN", DIGEST_LEN);
    tt_assert(!rs->is_authority);
    tt_assert(!rs->is_exit);
    tt_assert(!rs->is_fast);
    tt_assert(!rs->is_possible_guard);
    tt_assert(!rs->is_stable);
    /* (If it wasn't running it wouldn't be here) */
    tt_assert(rs->is_flagged_running);
    tt_assert(!rs->is_valid);
    tt_assert(!rs->is_named);
    tt_assert(rs->is_v2_dir);
    /* XXXX check version */
  } else if (tor_memeq(rs->identity_digest,
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
                       "\x5\x5\x5\x5",
                       DIGEST_LEN)) {
    /* This one showed up in 3 digests. Twice with ID 'M', once with 'Z'.  */
    tt_mem_op(rs->identity_digest,OP_EQ,
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
               DIGEST_LEN);
    tt_str_op(rs->nickname,OP_EQ, "router1");
    tt_mem_op(rs->descriptor_digest,OP_EQ, "MMMMMMMMMMMMMMMMMMMM", DIGEST_LEN);
    tt_int_op(rs->published_on,OP_EQ, now-1000);
    tt_int_op(rs->addr,OP_EQ, 0x99009901);
    tt_int_op(rs->or_port,OP_EQ, 443);
    tt_int_op(rs->dir_port,OP_EQ, 0);
    tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
    tt_assert(tor_addr_eq(&rs->ipv6_addr, &addr_ipv6));
    tt_int_op(rs->ipv6_orport,OP_EQ, 4711);
    tt_assert(!rs->is_authority);
    tt_assert(rs->is_exit);
    tt_assert(rs->is_fast);
    tt_assert(rs->is_possible_guard);
    tt_assert(rs->is_stable);
    tt_assert(rs->is_flagged_running);
    tt_assert(rs->is_valid);
    tt_assert(rs->is_v2_dir);
    tt_assert(!rs->is_named);
    /* XXXX check version */
  } else {
    /* Weren't expecting this... */
    tt_assert(0);
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
  char *consensus_text=NULL, *cp=NULL;
  smartlist_t *votes = smartlist_new();

  /* For generating the two other consensuses. */
  char *detached_text1=NULL, *detached_text2=NULL;
  char *consensus_text2=NULL, *consensus_text3=NULL;
  char *consensus_text_md2=NULL, *consensus_text_md3=NULL;
  char *consensus_text_md=NULL;
  networkstatus_t *con2=NULL, *con_md2=NULL, *con3=NULL, *con_md3=NULL;
  ns_detached_signatures_t *dsig1=NULL, *dsig2=NULL;

  tt_assert(vrs_gen);
  tt_assert(rs_test);
  tt_assert(vrs_test);

  tt_assert(!dir_common_authority_pk_init(&cert1, &cert2, &cert3,
                                          &sign_skey_1, &sign_skey_2,
                                          &sign_skey_3));
  sign_skey_leg1 = pk_generate(4);

  tt_assert(!dir_common_construct_vote_1(&vote, cert1, sign_skey_1, vrs_gen,
                                         &v1, &n_vrs, now, 1));
  tt_assert(v1);

  /* Make sure the parsed thing was right. */
  tt_int_op(v1->type,OP_EQ, NS_TYPE_VOTE);
  tt_int_op(v1->published,OP_EQ, vote->published);
  tt_int_op(v1->valid_after,OP_EQ, vote->valid_after);
  tt_int_op(v1->fresh_until,OP_EQ, vote->fresh_until);
  tt_int_op(v1->valid_until,OP_EQ, vote->valid_until);
  tt_int_op(v1->vote_seconds,OP_EQ, vote->vote_seconds);
  tt_int_op(v1->dist_seconds,OP_EQ, vote->dist_seconds);
  tt_str_op(v1->client_versions,OP_EQ, vote->client_versions);
  tt_str_op(v1->server_versions,OP_EQ, vote->server_versions);
  tt_assert(v1->voters && smartlist_len(v1->voters));
  voter = smartlist_get(v1->voters, 0);
  tt_str_op(voter->nickname,OP_EQ, "Voter1");
  tt_str_op(voter->address,OP_EQ, "1.2.3.4");
  tt_int_op(voter->addr,OP_EQ, 0x01020304);
  tt_int_op(voter->dir_port,OP_EQ, 80);
  tt_int_op(voter->or_port,OP_EQ, 9000);
  tt_str_op(voter->contact,OP_EQ, "voter@example.com");
  tt_assert(v1->cert);
  tt_assert(!crypto_pk_cmp_keys(sign_skey_1, v1->cert->signing_key));
  cp = smartlist_join_strings(v1->known_flags, ":", 0, NULL);
  tt_str_op(cp,OP_EQ, "Authority:Exit:Fast:Guard:Running:Stable:V2Dir:Valid");
  tor_free(cp);
  tt_int_op(smartlist_len(v1->routerstatus_list),OP_EQ, n_vrs);
  networkstatus_vote_free(vote);
  vote = NULL;

  if (vote_tweaks) params_tweaked += vote_tweaks(v1, 1, now);

  /* Check the routerstatuses. */
  for (idx = 0; idx < n_vrs; ++idx) {
    vrs = smartlist_get(v1->routerstatus_list, idx);
    tt_assert(vrs);
    vrs_test(vrs, 1, now);
  }

  /* Generate second vote. It disagrees on some of the times,
   * and doesn't list versions, and knows some crazy flags.
   * Generate and parse v2. */
  tt_assert(!dir_common_construct_vote_2(&vote, cert2, sign_skey_2, vrs_gen,
                                         &v2, &n_vrs, now, 1));
  tt_assert(v2);

  if (vote_tweaks) params_tweaked += vote_tweaks(v2, 2, now);

  /* Check that flags come out right.*/
  cp = smartlist_join_strings(v2->known_flags, ":", 0, NULL);
  tt_str_op(cp,OP_EQ, "Authority:Exit:Fast:Guard:MadeOfCheese:MadeOfTin:"
             "Running:Stable:V2Dir:Valid");
  tor_free(cp);

  /* Check the routerstatuses. */
  n_vrs = smartlist_len(v2->routerstatus_list);
  for (idx = 0; idx < n_vrs; ++idx) {
    vrs = smartlist_get(v2->routerstatus_list, idx);
    tt_assert(vrs);
    vrs_test(vrs, 2, now);
  }
  networkstatus_vote_free(vote);
  vote = NULL;

  /* Generate the third vote with a legacy id. */
  tt_assert(!dir_common_construct_vote_3(&vote, cert3, sign_skey_3, vrs_gen,
                                         &v3, &n_vrs, now, 1));
  tt_assert(v3);

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
  tt_assert(consensus_text);
  con = networkstatus_parse_vote_from_string(consensus_text, NULL,
                                             NS_TYPE_CONSENSUS);
  tt_assert(con);
  //log_notice(LD_GENERAL, "<<%s>>\n<<%s>>\n<<%s>>\n",
  //           v1_text, v2_text, v3_text);
  consensus_text_md = networkstatus_compute_consensus(votes, 3,
                                                   cert3->identity_key,
                                                   sign_skey_3,
                                                   "AAAAAAAAAAAAAAAAAAAA",
                                                   sign_skey_leg1,
                                                   FLAV_MICRODESC);
  tt_assert(consensus_text_md);
  con_md = networkstatus_parse_vote_from_string(consensus_text_md, NULL,
                                                NS_TYPE_CONSENSUS);
  tt_assert(con_md);
  tt_int_op(con_md->flavor,OP_EQ, FLAV_MICRODESC);

  /* Check consensus contents. */
  tt_assert(con->type == NS_TYPE_CONSENSUS);
  tt_int_op(con->published,OP_EQ, 0); /* this field only appears in votes. */
  tt_int_op(con->valid_after,OP_EQ, now+1000);
  tt_int_op(con->fresh_until,OP_EQ, now+2003); /* median */
  tt_int_op(con->valid_until,OP_EQ, now+3000);
  tt_int_op(con->vote_seconds,OP_EQ, 100);
  tt_int_op(con->dist_seconds,OP_EQ, 250); /* median */
  tt_str_op(con->client_versions,OP_EQ, "0.1.2.14");
  tt_str_op(con->server_versions,OP_EQ, "0.1.2.15,0.1.2.16");
  cp = smartlist_join_strings(v2->known_flags, ":", 0, NULL);
  tt_str_op(cp,OP_EQ, "Authority:Exit:Fast:Guard:MadeOfCheese:MadeOfTin:"
             "Running:Stable:V2Dir:Valid");
  tor_free(cp);
  if (!params_tweaked) {
    /* Skip this one if vote_tweaks() messed with the param lists */
    cp = smartlist_join_strings(con->net_params, ":", 0, NULL);
    tt_str_op(cp,OP_EQ, "circuitwindow=80:foo=660");
    tor_free(cp);
  }

  tt_int_op(4,OP_EQ, smartlist_len(con->voters)); /*3 voters, 1 legacy key.*/
  /* The voter id digests should be in this order. */
  tt_assert(memcmp(cert2->cache_info.identity_digest,
                     cert1->cache_info.identity_digest,DIGEST_LEN)<0);
  tt_assert(memcmp(cert1->cache_info.identity_digest,
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
  tt_assert(n_rs);
  for (idx = 0; idx < n_rs; ++idx) {
    rs = smartlist_get(con->routerstatus_list, idx);
    tt_assert(rs);
    rs_test(rs, now);
  }

  n_rs = smartlist_len(con_md->routerstatus_list);
  tt_assert(n_rs);
  for (idx = 0; idx < n_rs; ++idx) {
    rs = smartlist_get(con_md->routerstatus_list, idx);
    tt_assert(rs);
  }

  /* Check signatures.  the first voter is a pseudo-entry with a legacy key.
   * The second one hasn't signed.  The fourth one has signed: validate it. */
  voter = smartlist_get(con->voters, 1);
  tt_int_op(smartlist_len(voter->sigs),OP_EQ, 0);

  voter = smartlist_get(con->voters, 3);
  tt_int_op(smartlist_len(voter->sigs),OP_EQ, 1);
  sig = smartlist_get(voter->sigs, 0);
  tt_assert(sig->signature);
  tt_assert(!sig->good_signature);
  tt_assert(!sig->bad_signature);

  tt_assert(!networkstatus_check_document_signature(con, sig, cert3));
  tt_assert(sig->signature);
  tt_assert(sig->good_signature);
  tt_assert(!sig->bad_signature);

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
    tt_assert(consensus_text2);
    tt_assert(consensus_text3);
    tt_assert(consensus_text_md2);
    tt_assert(consensus_text_md3);
    con2 = networkstatus_parse_vote_from_string(consensus_text2, NULL,
                                                NS_TYPE_CONSENSUS);
    con3 = networkstatus_parse_vote_from_string(consensus_text3, NULL,
                                                NS_TYPE_CONSENSUS);
    con_md2 = networkstatus_parse_vote_from_string(consensus_text_md2, NULL,
                                                NS_TYPE_CONSENSUS);
    con_md3 = networkstatus_parse_vote_from_string(consensus_text_md3, NULL,
                                                NS_TYPE_CONSENSUS);
    tt_assert(con2);
    tt_assert(con3);
    tt_assert(con_md2);
    tt_assert(con_md3);

    /* All three should have the same digest. */
    tt_mem_op(&con->digests,OP_EQ, &con2->digests, sizeof(common_digests_t));
    tt_mem_op(&con->digests,OP_EQ, &con3->digests, sizeof(common_digests_t));

    tt_mem_op(&con_md->digests,OP_EQ, &con_md2->digests,
              sizeof(common_digests_t));
    tt_mem_op(&con_md->digests,OP_EQ, &con_md3->digests,
              sizeof(common_digests_t));

    /* Extract a detached signature from con3. */
    detached_text1 = get_detached_sigs(con3, con_md3);
    tt_assert(detached_text1);
    /* Try to parse it. */
    dsig1 = networkstatus_parse_detached_signatures(detached_text1, NULL);
    tt_assert(dsig1);

    /* Are parsed values as expected? */
    tt_int_op(dsig1->valid_after,OP_EQ, con3->valid_after);
    tt_int_op(dsig1->fresh_until,OP_EQ, con3->fresh_until);
    tt_int_op(dsig1->valid_until,OP_EQ, con3->valid_until);
    {
      common_digests_t *dsig_digests = strmap_get(dsig1->digests, "ns");
      tt_assert(dsig_digests);
      tt_mem_op(dsig_digests->d[DIGEST_SHA1], OP_EQ,
                con3->digests.d[DIGEST_SHA1], DIGEST_LEN);
      dsig_digests = strmap_get(dsig1->digests, "microdesc");
      tt_assert(dsig_digests);
      tt_mem_op(dsig_digests->d[DIGEST_SHA256],OP_EQ,
                 con_md3->digests.d[DIGEST_SHA256],
                 DIGEST256_LEN);
    }
    {
      smartlist_t *dsig_signatures = strmap_get(dsig1->signatures, "ns");
      tt_assert(dsig_signatures);
      tt_int_op(1,OP_EQ, smartlist_len(dsig_signatures));
      sig = smartlist_get(dsig_signatures, 0);
      tt_mem_op(sig->identity_digest,OP_EQ, cert1->cache_info.identity_digest,
                 DIGEST_LEN);
      tt_int_op(sig->alg,OP_EQ, DIGEST_SHA1);

      dsig_signatures = strmap_get(dsig1->signatures, "microdesc");
      tt_assert(dsig_signatures);
      tt_int_op(1,OP_EQ, smartlist_len(dsig_signatures));
      sig = smartlist_get(dsig_signatures, 0);
      tt_mem_op(sig->identity_digest,OP_EQ, cert1->cache_info.identity_digest,
                 DIGEST_LEN);
      tt_int_op(sig->alg,OP_EQ, DIGEST_SHA256);
    }

    /* Try adding it to con2. */
    detached_text2 = get_detached_sigs(con2,con_md2);
    tt_int_op(1,OP_EQ, networkstatus_add_detached_signatures(con2, dsig1,
                                                   "test", LOG_INFO, &msg));
    tor_free(detached_text2);
    tt_int_op(1,OP_EQ,
              networkstatus_add_detached_signatures(con_md2, dsig1, "test",
                                                     LOG_INFO, &msg));
    tor_free(detached_text2);
    detached_text2 = get_detached_sigs(con2,con_md2);
    //printf("\n<%s>\n", detached_text2);
    dsig2 = networkstatus_parse_detached_signatures(detached_text2, NULL);
    tt_assert(dsig2);
    /*
    printf("\n");
    SMARTLIST_FOREACH(dsig2->signatures, networkstatus_voter_info_t *, vi, {
        char hd[64];
        base16_encode(hd, sizeof(hd), vi->identity_digest, DIGEST_LEN);
        printf("%s\n", hd);
      });
    */
    tt_int_op(2,OP_EQ,
            smartlist_len((smartlist_t*)strmap_get(dsig2->signatures, "ns")));
    tt_int_op(2,OP_EQ,
            smartlist_len((smartlist_t*)strmap_get(dsig2->signatures,
                                                   "microdesc")));

    /* Try adding to con2 twice; verify that nothing changes. */
    tt_int_op(0,OP_EQ, networkstatus_add_detached_signatures(con2, dsig1,
                                               "test", LOG_INFO, &msg));

    /* Add to con. */
    tt_int_op(2,OP_EQ, networkstatus_add_detached_signatures(con, dsig2,
                                               "test", LOG_INFO, &msg));
    /* Check signatures */
    voter = smartlist_get(con->voters, 1);
    sig = smartlist_get(voter->sigs, 0);
    tt_assert(sig);
    tt_assert(!networkstatus_check_document_signature(con, sig, cert2));
    voter = smartlist_get(con->voters, 2);
    sig = smartlist_get(voter->sigs, 0);
    tt_assert(sig);
    tt_assert(!networkstatus_check_document_signature(con, sig, cert1));
  }

 done:
  tor_free(cp);
  smartlist_free(votes);
  tor_free(consensus_text);
  tor_free(consensus_text_md);

  networkstatus_vote_free(vote);
  networkstatus_vote_free(v1);
  networkstatus_vote_free(v2);
  networkstatus_vote_free(v3);
  networkstatus_vote_free(con);
  networkstatus_vote_free(con_md);
  crypto_pk_free(sign_skey_1);
  crypto_pk_free(sign_skey_2);
  crypto_pk_free(sign_skey_3);
  crypto_pk_free(sign_skey_leg1);
  authority_cert_free(cert1);
  authority_cert_free(cert2);
  authority_cert_free(cert3);

  tor_free(consensus_text2);
  tor_free(consensus_text3);
  tor_free(consensus_text_md2);
  tor_free(consensus_text_md3);
  tor_free(detached_text1);
  tor_free(detached_text2);

  networkstatus_vote_free(con2);
  networkstatus_vote_free(con3);
  networkstatus_vote_free(con_md2);
  networkstatus_vote_free(con_md3);
  ns_detached_signatures_free(dsig1);
  ns_detached_signatures_free(dsig2);
}

/** Run unit tests for generating and parsing V3 consensus networkstatus
 * documents. */
static void
test_dir_v3_networkstatus(void *arg)
{
  (void)arg;
  test_a_networkstatus(dir_common_gen_routerstatus_for_v3ns,
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

  tt_int_op((int)total, OP_EQ, 48);
  total = 0;
  for (i=0; i<8; ++i) {
    total += vals[i].u64;
  }
  tt_assert(total >= (U64_LITERAL(1)<<60));
  tt_assert(total <= (U64_LITERAL(1)<<62));

  for (i=0; i<8; ++i) {
    /* vals[2].u64 is the scaled value of 1.0 */
    double ratio = ((double)vals[i].u64) / vals[2].u64;
    tt_double_op(fabs(ratio - v[i]), OP_LT, .00001);
  }

  /* test handling of no entries */
  total = 1;
  scale_array_elements_to_u64(vals, 0, &total);
  tt_assert(total == 0);

  /* make sure we don't read the array when we have no entries
   * may require compiler flags to catch NULL dereferences */
  total = 1;
  scale_array_elements_to_u64(NULL, 0, &total);
  tt_assert(total == 0);

  scale_array_elements_to_u64(NULL, 0, NULL);

  /* test handling of zero totals */
  total = 1;
  vals[0].dbl = 0.0;
  scale_array_elements_to_u64(vals, 1, &total);
  tt_assert(total == 0);
  tt_assert(vals[0].u64 == 0);

  vals[0].dbl = 0.0;
  vals[1].dbl = 0.0;
  scale_array_elements_to_u64(vals, 2, NULL);
  tt_assert(vals[0].u64 == 0);
  tt_assert(vals[1].u64 == 0);

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
  tt_u64_op(total, OP_EQ, 45);
  for (i=0; i<n; ++i) {
    choice = choose_array_element_by_weight(inp, 10);
    tt_int_op(choice, OP_GE, 0);
    tt_int_op(choice, OP_LT, 10);
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
      tt_int_op(histogram[i], OP_EQ, 0);

    sq = frac_diff * frac_diff;
    if (sq > max_sq_error)
      max_sq_error = sq;
  }
  /* It should almost always be much much less than this.  If you want to
   * figure out the odds, please feel free. */
  tt_double_op(max_sq_error, OP_LT, .05);

  /* Now try a singleton; do we choose it? */
  for (i = 0; i < 100; ++i) {
    choice = choose_array_element_by_weight(inp, 1);
    tt_int_op(choice, OP_EQ, 0);
  }

  /* Now try an array of zeros.  We should choose randomly. */
  memset(histogram,0,sizeof(histogram));
  for (i = 0; i < 5; ++i)
    inp[i].u64 = 0;
  for (i = 0; i < n; ++i) {
    choice = choose_array_element_by_weight(inp, 5);
    tt_int_op(choice, OP_GE, 0);
    tt_int_op(choice, OP_LT, 5);
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
  tt_double_op(max_sq_error, OP_LT, .05);
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
      tt_assert(0);
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

  tt_assert(v);
  (void)voter;
  (void)now;

  tt_assert(v->supported_methods);
  SMARTLIST_FOREACH(v->supported_methods, char *, c, tor_free(c));
  smartlist_clear(v->supported_methods);
  /* Method 17 is MIN_METHOD_TO_CLIP_UNMEASURED_BW_KB */
  smartlist_split_string(v->supported_methods,
                         "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17",
                         NULL, 0, -1);
  /* If we're using a non-default clip bandwidth, add it to net_params */
  if (alternate_clip_bw > 0) {
    tor_asprintf(&maxbw_param, "maxunmeasuredbw=%u", alternate_clip_bw);
    tt_assert(maxbw_param);
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
  tt_assert(vrs);
  rs = &(vrs->status);
  tt_assert(rs);

  /* Split out by digests to test */
  if (tor_memeq(rs->identity_digest,
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
                DIGEST_LEN)) {
    /*
     * Check the first routerstatus - measured bandwidth below the clip
     * cutoff.
     */
    tt_str_op(vrs->version,OP_EQ, "0.1.2.14");
    tt_int_op(rs->published_on,OP_EQ, now-1500);
    tt_str_op(rs->nickname,OP_EQ, "router2");
    tt_mem_op(rs->identity_digest,OP_EQ,
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
               DIGEST_LEN);
    tt_mem_op(rs->descriptor_digest,OP_EQ, "NNNNNNNNNNNNNNNNNNNN", DIGEST_LEN);
    tt_int_op(rs->addr,OP_EQ, 0x99008801);
    tt_int_op(rs->or_port,OP_EQ, 443);
    tt_int_op(rs->dir_port,OP_EQ, 8000);
    tt_assert(rs->has_bandwidth);
    tt_assert(vrs->has_measured_bw);
    tt_int_op(rs->bandwidth_kb,OP_EQ, max_unmeasured_bw_kb / 2);
    tt_int_op(vrs->measured_bw_kb,OP_EQ, max_unmeasured_bw_kb / 2);
  } else if (tor_memeq(rs->identity_digest,
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
                       DIGEST_LEN)) {

    /*
     * Check the second routerstatus - measured bandwidth above the clip
     * cutoff.
     */
    tt_str_op(vrs->version,OP_EQ, "0.2.0.5");
    tt_int_op(rs->published_on,OP_EQ, now-1000);
    tt_str_op(rs->nickname,OP_EQ, "router1");
    tt_mem_op(rs->identity_digest,OP_EQ,
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
               DIGEST_LEN);
    tt_mem_op(rs->descriptor_digest,OP_EQ, "MMMMMMMMMMMMMMMMMMMM", DIGEST_LEN);
    tt_int_op(rs->addr,OP_EQ, 0x99009901);
    tt_int_op(rs->or_port,OP_EQ, 443);
    tt_int_op(rs->dir_port,OP_EQ, 0);
    tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
    tt_assert(tor_addr_eq(&rs->ipv6_addr, &addr_ipv6));
    tt_int_op(rs->ipv6_orport,OP_EQ, 4711);
    tt_assert(rs->has_bandwidth);
    tt_assert(vrs->has_measured_bw);
    tt_int_op(rs->bandwidth_kb,OP_EQ, max_unmeasured_bw_kb * 2);
    tt_int_op(vrs->measured_bw_kb,OP_EQ, max_unmeasured_bw_kb * 2);
  } else if (tor_memeq(rs->identity_digest,
                       "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33"
                       "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33",
                       DIGEST_LEN)) {
    /*
     * Check the third routerstatus - unmeasured bandwidth above the clip
     * cutoff; this one should be clipped later on in the consensus, but
     * appears unclipped in the vote.
     */
    tt_assert(rs->has_bandwidth);
    tt_assert(!(vrs->has_measured_bw));
    tt_int_op(rs->bandwidth_kb,OP_EQ, max_unmeasured_bw_kb * 2);
    tt_int_op(vrs->measured_bw_kb,OP_EQ, 0);
  } else if (tor_memeq(rs->identity_digest,
                       "\x34\x34\x34\x34\x34\x34\x34\x34\x34\x34"
                       "\x34\x34\x34\x34\x34\x34\x34\x34\x34\x34",
                       DIGEST_LEN)) {
    /*
     * Check the fourth routerstatus - unmeasured bandwidth below the clip
     * cutoff; this one should not be clipped.
     */
    tt_assert(rs->has_bandwidth);
    tt_assert(!(vrs->has_measured_bw));
    tt_int_op(rs->bandwidth_kb,OP_EQ, max_unmeasured_bw_kb / 2);
    tt_int_op(vrs->measured_bw_kb,OP_EQ, 0);
  } else {
    tt_assert(0);
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

  tt_assert(con);
  tt_assert(!con->cert);
  // tt_assert(con->consensus_method >= MIN_METHOD_TO_CLIP_UNMEASURED_BW_KB);
  tt_assert(con->consensus_method >= 16);
  tt_int_op(4,OP_EQ, smartlist_len(con->routerstatus_list));
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

  tt_assert(rs);

  /* There should be four listed routers, as constructed above */
  if (tor_memeq(rs->identity_digest,
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
                "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
                DIGEST_LEN)) {
    tt_mem_op(rs->identity_digest,OP_EQ,
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3"
               "\x3\x3\x3\x3\x3\x3\x3\x3\x3\x3",
               DIGEST_LEN);
    tt_mem_op(rs->descriptor_digest,OP_EQ, "NNNNNNNNNNNNNNNNNNNN", DIGEST_LEN);
    tt_assert(!rs->is_authority);
    tt_assert(!rs->is_exit);
    tt_assert(!rs->is_fast);
    tt_assert(!rs->is_possible_guard);
    tt_assert(!rs->is_stable);
    /* (If it wasn't running it wouldn't be here) */
    tt_assert(rs->is_flagged_running);
    tt_assert(!rs->is_valid);
    tt_assert(!rs->is_named);
    /* This one should have measured bandwidth below the clip cutoff */
    tt_assert(rs->has_bandwidth);
    tt_int_op(rs->bandwidth_kb,OP_EQ, max_unmeasured_bw_kb / 2);
    tt_assert(!(rs->bw_is_unmeasured));
  } else if (tor_memeq(rs->identity_digest,
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
                       "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
                       DIGEST_LEN)) {
    /* This one showed up in 3 digests. Twice with ID 'M', once with 'Z'.  */
    tt_mem_op(rs->identity_digest,OP_EQ,
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5"
               "\x5\x5\x5\x5\x5\x5\x5\x5\x5\x5",
               DIGEST_LEN);
    tt_str_op(rs->nickname,OP_EQ, "router1");
    tt_mem_op(rs->descriptor_digest,OP_EQ, "MMMMMMMMMMMMMMMMMMMM", DIGEST_LEN);
    tt_int_op(rs->published_on,OP_EQ, now-1000);
    tt_int_op(rs->addr,OP_EQ, 0x99009901);
    tt_int_op(rs->or_port,OP_EQ, 443);
    tt_int_op(rs->dir_port,OP_EQ, 0);
    tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
    tt_assert(tor_addr_eq(&rs->ipv6_addr, &addr_ipv6));
    tt_int_op(rs->ipv6_orport,OP_EQ, 4711);
    tt_assert(!rs->is_authority);
    tt_assert(rs->is_exit);
    tt_assert(rs->is_fast);
    tt_assert(rs->is_possible_guard);
    tt_assert(rs->is_stable);
    tt_assert(rs->is_flagged_running);
    tt_assert(rs->is_valid);
    tt_assert(!rs->is_named);
    /* This one should have measured bandwidth above the clip cutoff */
    tt_assert(rs->has_bandwidth);
    tt_int_op(rs->bandwidth_kb,OP_EQ, max_unmeasured_bw_kb * 2);
    tt_assert(!(rs->bw_is_unmeasured));
  } else if (tor_memeq(rs->identity_digest,
                "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33"
                "\x33\x33\x33\x33\x33\x33\x33\x33\x33\x33",
                DIGEST_LEN)) {
    /*
     * This one should have unmeasured bandwidth above the clip cutoff,
     * and so should be clipped
     */
    tt_assert(rs->has_bandwidth);
    tt_int_op(rs->bandwidth_kb,OP_EQ, max_unmeasured_bw_kb);
    tt_assert(rs->bw_is_unmeasured);
  } else if (tor_memeq(rs->identity_digest,
                "\x34\x34\x34\x34\x34\x34\x34\x34\x34\x34"
                "\x34\x34\x34\x34\x34\x34\x34\x34\x34\x34",
                DIGEST_LEN)) {
    /*
     * This one should have unmeasured bandwidth below the clip cutoff,
     * and so should not be clipped
     */
    tt_assert(rs->has_bandwidth);
    tt_int_op(rs->bandwidth_kb,OP_EQ, max_unmeasured_bw_kb / 2);
    tt_assert(rs->bw_is_unmeasured);
  } else {
    /* Weren't expecting this... */
    tt_assert(0);
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
test_dir_clip_unmeasured_bw_kb(void *arg)
{
  /* Run the test with the default clip bandwidth */
  (void)arg;
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
test_dir_clip_unmeasured_bw_kb_alt(void *arg)
{
  /*
   * Try a different one; this value is chosen so that the below-the-cutoff
   * unmeasured nodes the test uses, at alternate_clip_bw / 2, will be above
   * DEFAULT_MAX_UNMEASURED_BW_KB and if the consensus incorrectly uses that
   * cutoff it will fail the test.
   */
  (void)arg;
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
  rs.is_v2_dir = 1;
  rs.bandwidth_kb = 1000;

  s = networkstatus_getinfo_helper_single(&rs);
  tt_assert(s);
  tt_str_op(s, OP_EQ,
            "r TetsuoMilk U3RhdGVseSwgcGx1bXAgQnVjayA "
               "TXVsbGlnYW4gY2FtZSB1cCBmcm8 2013-04-02 17:53:18 "
               "32.48.64.80 9001 9002\n"
            "s Exit Fast Running V2Dir\n"
            "w Bandwidth=1000\n");

 done:
  tor_free(s);
}

static int mock_get_options_calls = 0;
static or_options_t *mock_options = NULL;

static void
reset_options(or_options_t *options, int *get_options_calls)
{
  memset(options, 0, sizeof(or_options_t));
  options->TestingTorNetwork = 1;

  *get_options_calls = 0;
}

static const or_options_t *
mock_get_options(void)
{
  ++mock_get_options_calls;
  tor_assert(mock_options);
  return mock_options;
}

static void
reset_routerstatus(routerstatus_t *rs,
                   const char *hex_identity_digest,
                   int32_t ipv4_addr)
{
  memset(rs, 0, sizeof(routerstatus_t));
  base16_decode(rs->identity_digest, sizeof(rs->identity_digest),
                hex_identity_digest, HEX_DIGEST_LEN);
  /* A zero address matches everything, so the address needs to be set.
   * But the specific value is irrelevant. */
  rs->addr = ipv4_addr;
}

#define ROUTER_A_ID_STR    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define ROUTER_A_IPV4      0xAA008801
#define ROUTER_B_ID_STR    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
#define ROUTER_B_IPV4      0xBB008801

#define ROUTERSET_ALL_STR  "*"
#define ROUTERSET_A_STR    ROUTER_A_ID_STR
#define ROUTERSET_NONE_STR ""

/*
 * Test that dirserv_set_routerstatus_testing sets router flags correctly
 * Using "*"  sets flags on A and B
 * Using "A"  sets flags on A
 * Using ""   sets flags on Neither
 * If the router is not included:
 *   - if *Strict is set, the flag is set to 0,
 *   - otherwise, the flag is not modified. */
static void
test_dir_dirserv_set_routerstatus_testing(void *arg)
{
  (void)arg;

  /* Init options */
  mock_options = malloc(sizeof(or_options_t));
  reset_options(mock_options, &mock_get_options_calls);

  MOCK(get_options, mock_get_options);

  /* Init routersets */
  routerset_t *routerset_all  = routerset_new();
  routerset_parse(routerset_all,  ROUTERSET_ALL_STR,  "All routers");

  routerset_t *routerset_a    = routerset_new();
  routerset_parse(routerset_a,    ROUTERSET_A_STR,    "Router A only");

  routerset_t *routerset_none = routerset_new();
  /* Routersets are empty when provided by routerset_new(),
   * so this is not strictly necessary */
  routerset_parse(routerset_none, ROUTERSET_NONE_STR, "No routers");

  /* Init routerstatuses */
  routerstatus_t *rs_a = malloc(sizeof(routerstatus_t));
  reset_routerstatus(rs_a, ROUTER_A_ID_STR, ROUTER_A_IPV4);

  routerstatus_t *rs_b = malloc(sizeof(routerstatus_t));
  reset_routerstatus(rs_b, ROUTER_B_ID_STR, ROUTER_B_IPV4);

  /* Sanity check that routersets correspond to routerstatuses.
   * Return values are {2, 3, 4} */

  /* We want 3 ("*" means match all addresses) */
  tt_assert(routerset_contains_routerstatus(routerset_all,  rs_a, 0) == 3);
  tt_assert(routerset_contains_routerstatus(routerset_all,  rs_b, 0) == 3);

  /* We want 4 (match id_digest [or nickname]) */
  tt_assert(routerset_contains_routerstatus(routerset_a,    rs_a, 0) == 4);
  tt_assert(routerset_contains_routerstatus(routerset_a,    rs_b, 0) == 0);

  tt_assert(routerset_contains_routerstatus(routerset_none, rs_a, 0) == 0);
  tt_assert(routerset_contains_routerstatus(routerset_none, rs_b, 0) == 0);

  /* Check that "*" sets flags on all routers: Exit
   * Check the flags aren't being confused with each other */
  reset_options(mock_options, &mock_get_options_calls);
  reset_routerstatus(rs_a, ROUTER_A_ID_STR, ROUTER_A_IPV4);
  reset_routerstatus(rs_b, ROUTER_B_ID_STR, ROUTER_B_IPV4);

  mock_options->TestingDirAuthVoteExit = routerset_all;
  mock_options->TestingDirAuthVoteExitIsStrict = 0;

  dirserv_set_routerstatus_testing(rs_a);
  tt_assert(mock_get_options_calls == 1);
  dirserv_set_routerstatus_testing(rs_b);
  tt_assert(mock_get_options_calls == 2);

  tt_assert(rs_a->is_exit == 1);
  tt_assert(rs_b->is_exit == 1);
  /* Be paranoid - check no other flags are set */
  tt_assert(rs_a->is_possible_guard == 0);
  tt_assert(rs_b->is_possible_guard == 0);
  tt_assert(rs_a->is_hs_dir == 0);
  tt_assert(rs_b->is_hs_dir == 0);

  /* Check that "*" sets flags on all routers: Guard & HSDir
   * Cover the remaining flags in one test */
  reset_options(mock_options, &mock_get_options_calls);
  reset_routerstatus(rs_a, ROUTER_A_ID_STR, ROUTER_A_IPV4);
  reset_routerstatus(rs_b, ROUTER_B_ID_STR, ROUTER_B_IPV4);

  mock_options->TestingDirAuthVoteGuard = routerset_all;
  mock_options->TestingDirAuthVoteGuardIsStrict = 0;
  mock_options->TestingDirAuthVoteHSDir = routerset_all;
  mock_options->TestingDirAuthVoteHSDirIsStrict = 0;

  dirserv_set_routerstatus_testing(rs_a);
  tt_assert(mock_get_options_calls == 1);
  dirserv_set_routerstatus_testing(rs_b);
  tt_assert(mock_get_options_calls == 2);

  tt_assert(rs_a->is_possible_guard == 1);
  tt_assert(rs_b->is_possible_guard == 1);
  tt_assert(rs_a->is_hs_dir == 1);
  tt_assert(rs_b->is_hs_dir == 1);
  /* Be paranoid - check exit isn't set */
  tt_assert(rs_a->is_exit == 0);
  tt_assert(rs_b->is_exit == 0);

  /* Check routerset A sets all flags on router A,
   * but leaves router B unmodified */
  reset_options(mock_options, &mock_get_options_calls);
  reset_routerstatus(rs_a, ROUTER_A_ID_STR, ROUTER_A_IPV4);
  reset_routerstatus(rs_b, ROUTER_B_ID_STR, ROUTER_B_IPV4);

  mock_options->TestingDirAuthVoteExit = routerset_a;
  mock_options->TestingDirAuthVoteExitIsStrict = 0;
  mock_options->TestingDirAuthVoteGuard = routerset_a;
  mock_options->TestingDirAuthVoteGuardIsStrict = 0;
  mock_options->TestingDirAuthVoteHSDir = routerset_a;
  mock_options->TestingDirAuthVoteHSDirIsStrict = 0;

  dirserv_set_routerstatus_testing(rs_a);
  tt_assert(mock_get_options_calls == 1);
  dirserv_set_routerstatus_testing(rs_b);
  tt_assert(mock_get_options_calls == 2);

  tt_assert(rs_a->is_exit == 1);
  tt_assert(rs_b->is_exit == 0);
  tt_assert(rs_a->is_possible_guard == 1);
  tt_assert(rs_b->is_possible_guard == 0);
  tt_assert(rs_a->is_hs_dir == 1);
  tt_assert(rs_b->is_hs_dir == 0);

  /* Check routerset A unsets all flags on router B when Strict is set */
  reset_options(mock_options, &mock_get_options_calls);
  reset_routerstatus(rs_b, ROUTER_B_ID_STR, ROUTER_B_IPV4);

  mock_options->TestingDirAuthVoteExit = routerset_a;
  mock_options->TestingDirAuthVoteExitIsStrict = 1;
  mock_options->TestingDirAuthVoteGuard = routerset_a;
  mock_options->TestingDirAuthVoteGuardIsStrict = 1;
  mock_options->TestingDirAuthVoteHSDir = routerset_a;
  mock_options->TestingDirAuthVoteHSDirIsStrict = 1;

  rs_b->is_exit = 1;
  rs_b->is_possible_guard = 1;
  rs_b->is_hs_dir = 1;

  dirserv_set_routerstatus_testing(rs_b);
  tt_assert(mock_get_options_calls == 1);

  tt_assert(rs_b->is_exit == 0);
  tt_assert(rs_b->is_possible_guard == 0);
  tt_assert(rs_b->is_hs_dir == 0);

  /* Check routerset A doesn't modify flags on router B without Strict set */
  reset_options(mock_options, &mock_get_options_calls);
  reset_routerstatus(rs_b, ROUTER_B_ID_STR, ROUTER_B_IPV4);

  mock_options->TestingDirAuthVoteExit = routerset_a;
  mock_options->TestingDirAuthVoteExitIsStrict = 0;
  mock_options->TestingDirAuthVoteGuard = routerset_a;
  mock_options->TestingDirAuthVoteGuardIsStrict = 0;
  mock_options->TestingDirAuthVoteHSDir = routerset_a;
  mock_options->TestingDirAuthVoteHSDirIsStrict = 0;

  rs_b->is_exit = 1;
  rs_b->is_possible_guard = 1;
  rs_b->is_hs_dir = 1;

  dirserv_set_routerstatus_testing(rs_b);
  tt_assert(mock_get_options_calls == 1);

  tt_assert(rs_b->is_exit == 1);
  tt_assert(rs_b->is_possible_guard == 1);
  tt_assert(rs_b->is_hs_dir == 1);

  /* Check the empty routerset zeroes all flags
   * on routers A & B with Strict set */
  reset_options(mock_options, &mock_get_options_calls);
  reset_routerstatus(rs_b, ROUTER_B_ID_STR, ROUTER_B_IPV4);

  mock_options->TestingDirAuthVoteExit = routerset_none;
  mock_options->TestingDirAuthVoteExitIsStrict = 1;
  mock_options->TestingDirAuthVoteGuard = routerset_none;
  mock_options->TestingDirAuthVoteGuardIsStrict = 1;
  mock_options->TestingDirAuthVoteHSDir = routerset_none;
  mock_options->TestingDirAuthVoteHSDirIsStrict = 1;

  rs_b->is_exit = 1;
  rs_b->is_possible_guard = 1;
  rs_b->is_hs_dir = 1;

  dirserv_set_routerstatus_testing(rs_b);
  tt_assert(mock_get_options_calls == 1);

  tt_assert(rs_b->is_exit == 0);
  tt_assert(rs_b->is_possible_guard == 0);
  tt_assert(rs_b->is_hs_dir == 0);

  /* Check the empty routerset doesn't modify any flags
   * on A or B without Strict set */
  reset_options(mock_options, &mock_get_options_calls);
  reset_routerstatus(rs_a, ROUTER_A_ID_STR, ROUTER_A_IPV4);
  reset_routerstatus(rs_b, ROUTER_B_ID_STR, ROUTER_B_IPV4);

  mock_options->TestingDirAuthVoteExit = routerset_none;
  mock_options->TestingDirAuthVoteExitIsStrict = 0;
  mock_options->TestingDirAuthVoteGuard = routerset_none;
  mock_options->TestingDirAuthVoteGuardIsStrict = 0;
  mock_options->TestingDirAuthVoteHSDir = routerset_none;
  mock_options->TestingDirAuthVoteHSDirIsStrict = 0;

  rs_b->is_exit = 1;
  rs_b->is_possible_guard = 1;
  rs_b->is_hs_dir = 1;

  dirserv_set_routerstatus_testing(rs_a);
  tt_assert(mock_get_options_calls == 1);
  dirserv_set_routerstatus_testing(rs_b);
  tt_assert(mock_get_options_calls == 2);

  tt_assert(rs_a->is_exit == 0);
  tt_assert(rs_a->is_possible_guard == 0);
  tt_assert(rs_a->is_hs_dir == 0);
  tt_assert(rs_b->is_exit == 1);
  tt_assert(rs_b->is_possible_guard == 1);
  tt_assert(rs_b->is_hs_dir == 1);

 done:
  free(mock_options);
  mock_options = NULL;

  UNMOCK(get_options);

  routerset_free(routerset_all);
  routerset_free(routerset_a);
  routerset_free(routerset_none);

  free(rs_a);
  free(rs_b);
}

static void
test_dir_http_handling(void *args)
{
  char *url = NULL;
  (void)args;

  /* Parse http url tests: */
  /* Good headers */
  tt_int_op(parse_http_url("GET /tor/a/b/c.txt HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: Mozilla/5.0 (Windows;"
                           " U; Windows NT 6.1; en-US; rv:1.9.1.5)\r\n",
                           &url),OP_EQ, 0);
  tt_str_op(url,OP_EQ, "/tor/a/b/c.txt");
  tor_free(url);

  tt_int_op(parse_http_url("GET /tor/a/b/c.txt HTTP/1.0\r\n", &url),OP_EQ, 0);
  tt_str_op(url,OP_EQ, "/tor/a/b/c.txt");
  tor_free(url);

  tt_int_op(parse_http_url("GET /tor/a/b/c.txt HTTP/1.600\r\n", &url),
            OP_EQ, 0);
  tt_str_op(url,OP_EQ, "/tor/a/b/c.txt");
  tor_free(url);

  /* Should prepend '/tor/' to url if required */
  tt_int_op(parse_http_url("GET /a/b/c.txt HTTP/1.1\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: Mozilla/5.0 (Windows;"
                           " U; Windows NT 6.1; en-US; rv:1.9.1.5)\r\n",
                           &url),OP_EQ, 0);
  tt_str_op(url,OP_EQ, "/tor/a/b/c.txt");
  tor_free(url);

  /* Bad headers -- no HTTP/1.x*/
  tt_int_op(parse_http_url("GET /a/b/c.txt\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: Mozilla/5.0 (Windows;"
                           " U; Windows NT 6.1; en-US; rv:1.9.1.5)\r\n",
                           &url),OP_EQ, -1);
  tt_assert(!url);

  /* Bad headers */
  tt_int_op(parse_http_url("GET /a/b/c.txt\r\n"
                           "Host: example.com\r\n"
                           "User-Agent: Mozilla/5.0 (Windows;"
                           " U; Windows NT 6.1; en-US; rv:1.9.1.5)\r\n",
                           &url),OP_EQ, -1);
  tt_assert(!url);

  tt_int_op(parse_http_url("GET /tor/a/b/c.txt", &url),OP_EQ, -1);
  tt_assert(!url);

  tt_int_op(parse_http_url("GET /tor/a/b/c.txt HTTP/1.1", &url),OP_EQ, -1);
  tt_assert(!url);

  tt_int_op(parse_http_url("GET /tor/a/b/c.txt HTTP/1.1x\r\n", &url),
            OP_EQ, -1);
  tt_assert(!url);

  tt_int_op(parse_http_url("GET /tor/a/b/c.txt HTTP/1.", &url),OP_EQ, -1);
  tt_assert(!url);

  tt_int_op(parse_http_url("GET /tor/a/b/c.txt HTTP/1.\r", &url),OP_EQ, -1);
  tt_assert(!url);

 done:
  tor_free(url);
}

static void
test_dir_purpose_needs_anonymity(void *arg)
{
  (void)arg;
  tt_int_op(1, ==, purpose_needs_anonymity(0, ROUTER_PURPOSE_BRIDGE));
  tt_int_op(1, ==, purpose_needs_anonymity(0, ROUTER_PURPOSE_GENERAL));
  tt_int_op(0, ==, purpose_needs_anonymity(DIR_PURPOSE_FETCH_MICRODESC,
                                            ROUTER_PURPOSE_GENERAL));
 done: ;
}

static void
test_dir_fetch_type(void *arg)
{
  (void)arg;
  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_EXTRAINFO, ROUTER_PURPOSE_BRIDGE,
                           NULL), OP_EQ, EXTRAINFO_DIRINFO | BRIDGE_DIRINFO);
  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_EXTRAINFO, ROUTER_PURPOSE_GENERAL,
                           NULL), OP_EQ, EXTRAINFO_DIRINFO | V3_DIRINFO);

  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_SERVERDESC, ROUTER_PURPOSE_BRIDGE,
                           NULL), OP_EQ, BRIDGE_DIRINFO);
  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_SERVERDESC,
                           ROUTER_PURPOSE_GENERAL, NULL), OP_EQ, V3_DIRINFO);

  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_STATUS_VOTE,
                           ROUTER_PURPOSE_GENERAL, NULL), OP_EQ, V3_DIRINFO);
  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_DETACHED_SIGNATURES,
                           ROUTER_PURPOSE_GENERAL, NULL), OP_EQ, V3_DIRINFO);
  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_CERTIFICATE,
                           ROUTER_PURPOSE_GENERAL, NULL), OP_EQ, V3_DIRINFO);

  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_CONSENSUS, ROUTER_PURPOSE_GENERAL,
                           "microdesc"), OP_EQ, V3_DIRINFO|MICRODESC_DIRINFO);
  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_CONSENSUS, ROUTER_PURPOSE_GENERAL,
                           NULL), OP_EQ, V3_DIRINFO);

  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_MICRODESC, ROUTER_PURPOSE_GENERAL,
                           NULL), OP_EQ, MICRODESC_DIRINFO);

  tt_int_op(dir_fetch_type(DIR_PURPOSE_FETCH_RENDDESC_V2,
                           ROUTER_PURPOSE_GENERAL, NULL), OP_EQ, NO_DIRINFO);
 done: ;
}

static void
test_dir_packages(void *arg)
{
  smartlist_t *votes = smartlist_new();
  char *res = NULL;
  (void)arg;

#define BAD(s) \
  tt_int_op(0, ==, validate_recommended_package_line(s));
#define GOOD(s) \
  tt_int_op(1, ==, validate_recommended_package_line(s));
  GOOD("tor 0.2.6.3-alpha "
       "http://torproject.example.com/dist/tor-0.2.6.3-alpha.tar.gz "
       "sha256=sssdlkfjdsklfjdskfljasdklfj");
  GOOD("tor 0.2.6.3-alpha "
       "http://torproject.example.com/dist/tor-0.2.6.3-alpha.tar.gz "
       "sha256=sssdlkfjdsklfjdskfljasdklfj blake2b=fred");
  BAD("tor 0.2.6.3-alpha "
       "http://torproject.example.com/dist/tor-0.2.6.3-alpha.tar.gz "
       "sha256=sssdlkfjdsklfjdskfljasdklfj=");
  BAD("tor 0.2.6.3-alpha "
       "http://torproject.example.com/dist/tor-0.2.6.3-alpha.tar.gz "
       "sha256=sssdlkfjdsklfjdskfljasdklfj blake2b");
  BAD("tor 0.2.6.3-alpha "
       "http://torproject.example.com/dist/tor-0.2.6.3-alpha.tar.gz ");
  BAD("tor 0.2.6.3-alpha "
       "http://torproject.example.com/dist/tor-0.2.6.3-alpha.tar.gz");
  BAD("tor 0.2.6.3-alpha ");
  BAD("tor 0.2.6.3-alpha");
  BAD("tor ");
  BAD("tor");
  BAD("");
  BAD("=foobar sha256="
      "3c179f46ca77069a6a0bac70212a9b3b838b2f66129cb52d568837fc79d8fcc7");
  BAD("= = sha256="
      "3c179f46ca77069a6a0bac70212a9b3b838b2f66129cb52d568837fc79d8fcc7");

  BAD("sha512= sha256="
      "3c179f46ca77069a6a0bac70212a9b3b838b2f66129cb52d568837fc79d8fcc7");

  smartlist_add(votes, tor_malloc_zero(sizeof(networkstatus_t)));
  smartlist_add(votes, tor_malloc_zero(sizeof(networkstatus_t)));
  smartlist_add(votes, tor_malloc_zero(sizeof(networkstatus_t)));
  smartlist_add(votes, tor_malloc_zero(sizeof(networkstatus_t)));
  smartlist_add(votes, tor_malloc_zero(sizeof(networkstatus_t)));
  smartlist_add(votes, tor_malloc_zero(sizeof(networkstatus_t)));
  SMARTLIST_FOREACH(votes, networkstatus_t *, ns,
                    ns->package_lines = smartlist_new());

#define ADD(i, s)                                                       \
  smartlist_add(((networkstatus_t*)smartlist_get(votes, (i)))->package_lines, \
                (void*)(s));

  /* Only one vote for this one. */
  ADD(4, "cisco 99z http://foobar.example.com/ sha256=blahblah");

  /* Only two matching entries for this one, but 3 voters */
  ADD(1, "mystic 99y http://barfoo.example.com/ sha256=blahblah");
  ADD(3, "mystic 99y http://foobar.example.com/ sha256=blahblah");
  ADD(4, "mystic 99y http://foobar.example.com/ sha256=blahblah");

  /* Only two matching entries for this one, but at least 4 voters */
  ADD(1, "mystic 99p http://barfoo.example.com/ sha256=ggggggg");
  ADD(3, "mystic 99p http://foobar.example.com/ sha256=blahblah");
  ADD(4, "mystic 99p http://foobar.example.com/ sha256=blahblah");
  ADD(5, "mystic 99p http://foobar.example.com/ sha256=ggggggg");

  /* This one has only invalid votes. */
  ADD(0, "haffenreffer 1.2 http://foobar.example.com/ sha256");
  ADD(1, "haffenreffer 1.2 http://foobar.example.com/ ");
  ADD(2, "haffenreffer 1.2 ");
  ADD(3, "haffenreffer ");
  ADD(4, "haffenreffer");

  /* Three matching votes for this; it should actually go in! */
  ADD(2, "element 0.66.1 http://quux.example.com/ sha256=abcdef");
  ADD(3, "element 0.66.1 http://quux.example.com/ sha256=abcdef");
  ADD(4, "element 0.66.1 http://quux.example.com/ sha256=abcdef");
  ADD(1, "element 0.66.1 http://quum.example.com/ sha256=abcdef");
  ADD(0, "element 0.66.1 http://quux.example.com/ sha256=abcde");

  /* Three votes for A, three votes for B */
  ADD(0, "clownshoes 22alpha1 http://quumble.example.com/ blake2=foob");
  ADD(1, "clownshoes 22alpha1 http://quumble.example.com/ blake2=foob");
  ADD(2, "clownshoes 22alpha1 http://quumble.example.com/ blake2=foob");
  ADD(3, "clownshoes 22alpha1 http://quumble.example.com/ blake2=fooz");
  ADD(4, "clownshoes 22alpha1 http://quumble.example.com/ blake2=fooz");
  ADD(5, "clownshoes 22alpha1 http://quumble.example.com/ blake2=fooz");

  /* Three votes for A, two votes for B */
  ADD(1, "clownshoes 22alpha3 http://quumble.example.com/ blake2=foob");
  ADD(2, "clownshoes 22alpha3 http://quumble.example.com/ blake2=foob");
  ADD(3, "clownshoes 22alpha3 http://quumble.example.com/ blake2=fooz");
  ADD(4, "clownshoes 22alpha3 http://quumble.example.com/ blake2=fooz");
  ADD(5, "clownshoes 22alpha3 http://quumble.example.com/ blake2=fooz");

  /* Four votes for A, two for B. */
  ADD(0, "clownshoes 22alpha4 http://quumble.example.com/ blake2=foob");
  ADD(1, "clownshoes 22alpha4 http://quumble.example.com/ blake2=foob");
  ADD(2, "clownshoes 22alpha4 http://quumble.example.cam/ blake2=fooa");
  ADD(3, "clownshoes 22alpha4 http://quumble.example.cam/ blake2=fooa");
  ADD(4, "clownshoes 22alpha4 http://quumble.example.cam/ blake2=fooa");
  ADD(5, "clownshoes 22alpha4 http://quumble.example.cam/ blake2=fooa");

  /* Five votes for A ... all from the same authority.  Three for B. */
  ADD(0, "cbc 99.1.11.1.1 http://example.com/cbc/ cubehash=ahooy sha512=m");
  ADD(1, "cbc 99.1.11.1.1 http://example.com/cbc/ cubehash=ahooy sha512=m");
  ADD(3, "cbc 99.1.11.1.1 http://example.com/cbc/ cubehash=ahooy sha512=m");
  ADD(2, "cbc 99.1.11.1.1 http://example.com/ cubehash=ahooy");
  ADD(2, "cbc 99.1.11.1.1 http://example.com/ cubehash=ahooy");
  ADD(2, "cbc 99.1.11.1.1 http://example.com/ cubehash=ahooy");
  ADD(2, "cbc 99.1.11.1.1 http://example.com/ cubehash=ahooy");
  ADD(2, "cbc 99.1.11.1.1 http://example.com/ cubehash=ahooy");

  /* As above but new replaces old: no two match. */
  ADD(0, "cbc 99.1.11.1.2 http://example.com/cbc/ cubehash=ahooy sha512=m");
  ADD(1, "cbc 99.1.11.1.2 http://example.com/cbc/ cubehash=ahooy sha512=m");
  ADD(1, "cbc 99.1.11.1.2 http://example.com/cbc/x cubehash=ahooy sha512=m");
  ADD(2, "cbc 99.1.11.1.2 http://example.com/cbc/ cubehash=ahooy sha512=m");
  ADD(2, "cbc 99.1.11.1.2 http://example.com/ cubehash=ahooy");
  ADD(2, "cbc 99.1.11.1.2 http://example.com/ cubehash=ahooy");
  ADD(2, "cbc 99.1.11.1.2 http://example.com/ cubehash=ahooy");
  ADD(2, "cbc 99.1.11.1.2 http://example.com/ cubehash=ahooy");
  ADD(2, "cbc 99.1.11.1.2 http://example.com/ cubehash=ahooy");

  res = compute_consensus_package_lines(votes);
  tt_assert(res);
  tt_str_op(res, ==,
    "package cbc 99.1.11.1.1 http://example.com/cbc/ cubehash=ahooy sha512=m\n"
    "package clownshoes 22alpha3 http://quumble.example.com/ blake2=fooz\n"
    "package clownshoes 22alpha4 http://quumble.example.cam/ blake2=fooa\n"
    "package element 0.66.1 http://quux.example.com/ sha256=abcdef\n"
    "package mystic 99y http://foobar.example.com/ sha256=blahblah\n"
            );

#undef ADD
#undef BAD
#undef GOOD
 done:
  SMARTLIST_FOREACH(votes, networkstatus_t *, ns,
                    { smartlist_free(ns->package_lines); tor_free(ns); });
  smartlist_free(votes);
  tor_free(res);
}

static void
test_dir_download_status_schedule(void *arg)
{
  (void)arg;
  download_status_t dls_failure = { 0, 0, 0, DL_SCHED_GENERIC,
                                             DL_WANT_AUTHORITY,
                                             DL_SCHED_INCREMENT_FAILURE };
  download_status_t dls_attempt = { 0, 0, 0, DL_SCHED_CONSENSUS,
                                             DL_WANT_ANY_DIRSERVER,
                                             DL_SCHED_INCREMENT_ATTEMPT};
  download_status_t dls_bridge  = { 0, 0, 0, DL_SCHED_BRIDGE,
                                             DL_WANT_AUTHORITY,
                                             DL_SCHED_INCREMENT_FAILURE};
  int increment = -1;
  int expected_increment = -1;
  time_t current_time = time(NULL);
  int delay1 = -1;
  int delay2 = -1;
  smartlist_t *schedule = smartlist_new();

  /* Make a dummy schedule */
  smartlist_add(schedule, (void *)&delay1);
  smartlist_add(schedule, (void *)&delay2);

  /* check a range of values */
  delay1 = 1000;
  increment = download_status_schedule_get_delay(&dls_failure,
                                                 schedule,
                                                 TIME_MIN);
  expected_increment = delay1;
  tt_assert(increment == expected_increment);
  tt_assert(dls_failure.next_attempt_at == TIME_MIN + expected_increment);

  delay1 = INT_MAX;
  increment =  download_status_schedule_get_delay(&dls_failure,
                                                  schedule,
                                                  -1);
  expected_increment = delay1;
  tt_assert(increment == expected_increment);
  tt_assert(dls_failure.next_attempt_at == TIME_MAX);

  delay1 = 0;
  increment = download_status_schedule_get_delay(&dls_attempt,
                                                 schedule,
                                                 0);
  expected_increment = delay1;
  tt_assert(increment == expected_increment);
  tt_assert(dls_attempt.next_attempt_at == 0 + expected_increment);

  delay1 = 1000;
  increment = download_status_schedule_get_delay(&dls_attempt,
                                                 schedule,
                                                 1);
  expected_increment = delay1;
  tt_assert(increment == expected_increment);
  tt_assert(dls_attempt.next_attempt_at == 1 + expected_increment);

  delay1 = INT_MAX;
  increment = download_status_schedule_get_delay(&dls_bridge,
                                                 schedule,
                                                 current_time);
  expected_increment = delay1;
  tt_assert(increment == expected_increment);
  tt_assert(dls_bridge.next_attempt_at == TIME_MAX);

  delay1 = 1;
  increment = download_status_schedule_get_delay(&dls_bridge,
                                                 schedule,
                                                 TIME_MAX);
  expected_increment = delay1;
  tt_assert(increment == expected_increment);
  tt_assert(dls_bridge.next_attempt_at == TIME_MAX);

  /* see what happens when we reach the end */
  dls_attempt.n_download_attempts++;
  dls_bridge.n_download_failures++;

  delay2 = 100;
  increment = download_status_schedule_get_delay(&dls_attempt,
                                                 schedule,
                                                 current_time);
  expected_increment = delay2;
  tt_assert(increment == expected_increment);
  tt_assert(dls_attempt.next_attempt_at == current_time + delay2);

  delay2 = 1;
  increment = download_status_schedule_get_delay(&dls_bridge,
                                                 schedule,
                                                 current_time);
  expected_increment = delay2;
  tt_assert(increment == expected_increment);
  tt_assert(dls_bridge.next_attempt_at == current_time + delay2);

  /* see what happens when we try to go off the end */
  dls_attempt.n_download_attempts++;
  dls_bridge.n_download_failures++;

  delay2 = 5;
  increment = download_status_schedule_get_delay(&dls_attempt,
                                                 schedule,
                                                 current_time);
  expected_increment = delay2;
  tt_assert(increment == expected_increment);
  tt_assert(dls_attempt.next_attempt_at == current_time + delay2);

  delay2 = 17;
  increment = download_status_schedule_get_delay(&dls_bridge,
                                                 schedule,
                                                 current_time);
  expected_increment = delay2;
  tt_assert(increment == expected_increment);
  tt_assert(dls_bridge.next_attempt_at == current_time + delay2);

  /* see what happens when we reach IMPOSSIBLE_TO_DOWNLOAD */
  dls_attempt.n_download_attempts = IMPOSSIBLE_TO_DOWNLOAD;
  dls_bridge.n_download_failures = IMPOSSIBLE_TO_DOWNLOAD;

  delay2 = 35;
  increment = download_status_schedule_get_delay(&dls_attempt,
                                                 schedule,
                                                 current_time);
  expected_increment = INT_MAX;
  tt_assert(increment == expected_increment);
  tt_assert(dls_attempt.next_attempt_at == TIME_MAX);

  delay2 = 99;
  increment = download_status_schedule_get_delay(&dls_bridge,
                                                 schedule,
                                                 current_time);
  expected_increment = INT_MAX;
  tt_assert(increment == expected_increment);
  tt_assert(dls_bridge.next_attempt_at == TIME_MAX);

 done:
  /* the pointers in schedule are allocated on the stack */
  smartlist_free(schedule);
}

static void
test_dir_download_status_increment(void *arg)
{
  (void)arg;
  download_status_t dls_failure = { 0, 0, 0, DL_SCHED_GENERIC,
    DL_WANT_AUTHORITY,
    DL_SCHED_INCREMENT_FAILURE };
  download_status_t dls_attempt = { 0, 0, 0, DL_SCHED_BRIDGE,
    DL_WANT_ANY_DIRSERVER,
    DL_SCHED_INCREMENT_ATTEMPT};
  int delay0 = -1;
  int delay1 = -1;
  int delay2 = -1;
  smartlist_t *schedule = smartlist_new();
  or_options_t test_options;
  time_t next_at = TIME_MAX;
  time_t current_time = time(NULL);

  /* Provide some values for the schedule */
  delay0 = 10;
  delay1 = 99;
  delay2 = 20;

  /* Make the schedule */
  smartlist_add(schedule, (void *)&delay0);
  smartlist_add(schedule, (void *)&delay1);
  smartlist_add(schedule, (void *)&delay2);

  /* Put it in the options */
  mock_options = &test_options;
  reset_options(mock_options, &mock_get_options_calls);
  mock_options->TestingClientDownloadSchedule = schedule;
  mock_options->TestingBridgeDownloadSchedule = schedule;

  MOCK(get_options, mock_get_options);

  /* Check that a failure reset works */
  mock_get_options_calls = 0;
  download_status_reset(&dls_failure);
  /* we really want to test that it's equal to time(NULL) + delay0, but that's
   * an unrealiable test, because time(NULL) might change. */
  tt_assert(download_status_get_next_attempt_at(&dls_failure)
            >= current_time + delay0);
  tt_assert(download_status_get_next_attempt_at(&dls_failure)
            != TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_failure) == 0);
  tt_assert(download_status_get_n_attempts(&dls_failure) == 0);
  tt_assert(mock_get_options_calls >= 1);

  /* avoid timing inconsistencies */
  dls_failure.next_attempt_at = current_time + delay0;

  /* check that a reset schedule becomes ready at the right time */
  tt_assert(download_status_is_ready(&dls_failure,
                                     current_time + delay0 - 1,
                                     1) == 0);
  tt_assert(download_status_is_ready(&dls_failure,
                                     current_time + delay0,
                                     1) == 1);
  tt_assert(download_status_is_ready(&dls_failure,
                                     current_time + delay0 + 1,
                                     1) == 1);

  /* Check that a failure increment works */
  mock_get_options_calls = 0;
  next_at = download_status_increment_failure(&dls_failure, 404, "test", 0,
                                              current_time);
  tt_assert(next_at == current_time + delay1);
  tt_assert(download_status_get_n_failures(&dls_failure) == 1);
  tt_assert(download_status_get_n_attempts(&dls_failure) == 1);
  tt_assert(mock_get_options_calls >= 1);

  /* check that an incremented schedule becomes ready at the right time */
  tt_assert(download_status_is_ready(&dls_failure,
                                     current_time + delay1 - 1,
                                     1) == 0);
  tt_assert(download_status_is_ready(&dls_failure,
                                     current_time + delay1,
                                     1) == 1);
  tt_assert(download_status_is_ready(&dls_failure,
                                     current_time + delay1 + 1,
                                     1) == 1);

  /* check that a schedule isn't ready if it's had too many failures */
  tt_assert(download_status_is_ready(&dls_failure,
                                     current_time + delay1 + 10,
                                     0) == 0);

  /* Check that failure increments don't happen on 503 for clients, but that
   * attempt increments do. */
  mock_get_options_calls = 0;
  next_at = download_status_increment_failure(&dls_failure, 503, "test", 0,
                                              current_time);
  tt_assert(next_at == current_time + delay1);
  tt_assert(download_status_get_n_failures(&dls_failure) == 1);
  tt_assert(download_status_get_n_attempts(&dls_failure) == 2);
  tt_assert(mock_get_options_calls >= 1);

  /* Check that failure increments do happen on 503 for servers */
  mock_get_options_calls = 0;
  next_at = download_status_increment_failure(&dls_failure, 503, "test", 1,
                                              current_time);
  tt_assert(next_at == current_time + delay2);
  tt_assert(download_status_get_n_failures(&dls_failure) == 2);
  tt_assert(download_status_get_n_attempts(&dls_failure) == 3);
  tt_assert(mock_get_options_calls >= 1);

  /* Check what happens when we run off the end of the schedule */
  mock_get_options_calls = 0;
  next_at = download_status_increment_failure(&dls_failure, 404, "test", 0,
                                              current_time);
  tt_assert(next_at == current_time + delay2);
  tt_assert(download_status_get_n_failures(&dls_failure) == 3);
  tt_assert(download_status_get_n_attempts(&dls_failure) == 4);
  tt_assert(mock_get_options_calls >= 1);

  /* Check what happens when we hit the failure limit */
  mock_get_options_calls = 0;
  download_status_mark_impossible(&dls_failure);
  next_at = download_status_increment_failure(&dls_failure, 404, "test", 0,
                                              current_time);
  tt_assert(next_at == TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_failure)
            == IMPOSSIBLE_TO_DOWNLOAD);
  tt_assert(download_status_get_n_attempts(&dls_failure)
            == IMPOSSIBLE_TO_DOWNLOAD);
  tt_assert(mock_get_options_calls >= 1);

  /* Check that a failure reset doesn't reset at the limit */
  mock_get_options_calls = 0;
  download_status_reset(&dls_failure);
  tt_assert(download_status_get_next_attempt_at(&dls_failure)
            == TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_failure)
            == IMPOSSIBLE_TO_DOWNLOAD);
  tt_assert(download_status_get_n_attempts(&dls_failure)
            == IMPOSSIBLE_TO_DOWNLOAD);
  tt_assert(mock_get_options_calls == 0);

  /* Check that a failure reset resets just before the limit */
  mock_get_options_calls = 0;
  dls_failure.n_download_failures = IMPOSSIBLE_TO_DOWNLOAD - 1;
  dls_failure.n_download_attempts = IMPOSSIBLE_TO_DOWNLOAD - 1;
  download_status_reset(&dls_failure);
  /* we really want to test that it's equal to time(NULL) + delay0, but that's
   * an unrealiable test, because time(NULL) might change. */
  tt_assert(download_status_get_next_attempt_at(&dls_failure)
            >= current_time + delay0);
  tt_assert(download_status_get_next_attempt_at(&dls_failure)
            != TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_failure) == 0);
  tt_assert(download_status_get_n_attempts(&dls_failure) == 0);
  tt_assert(mock_get_options_calls >= 1);

  /* Check that failure increments do happen on attempt-based schedules,
   * but that the retry is set at the end of time */
  mock_get_options_calls = 0;
  next_at = download_status_increment_failure(&dls_attempt, 404, "test", 0,
                                              current_time);
  tt_assert(next_at == TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_attempt) == 1);
  tt_assert(download_status_get_n_attempts(&dls_attempt) == 0);
  tt_assert(mock_get_options_calls == 0);

  /* Check that an attempt reset works */
  mock_get_options_calls = 0;
  download_status_reset(&dls_attempt);
  /* we really want to test that it's equal to time(NULL) + delay0, but that's
   * an unrealiable test, because time(NULL) might change. */
  tt_assert(download_status_get_next_attempt_at(&dls_attempt)
            >= current_time + delay0);
  tt_assert(download_status_get_next_attempt_at(&dls_attempt)
            != TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_attempt) == 0);
  tt_assert(download_status_get_n_attempts(&dls_attempt) == 0);
  tt_assert(mock_get_options_calls >= 1);

  /* avoid timing inconsistencies */
  dls_attempt.next_attempt_at = current_time + delay0;

  /* check that a reset schedule becomes ready at the right time */
  tt_assert(download_status_is_ready(&dls_attempt,
                                     current_time + delay0 - 1,
                                     1) == 0);
  tt_assert(download_status_is_ready(&dls_attempt,
                                     current_time + delay0,
                                     1) == 1);
  tt_assert(download_status_is_ready(&dls_attempt,
                                     current_time + delay0 + 1,
                                     1) == 1);

  /* Check that an attempt increment works */
  mock_get_options_calls = 0;
  next_at = download_status_increment_attempt(&dls_attempt, "test",
                                              current_time);
  tt_assert(next_at == current_time + delay1);
  tt_assert(download_status_get_n_failures(&dls_attempt) == 0);
  tt_assert(download_status_get_n_attempts(&dls_attempt) == 1);
  tt_assert(mock_get_options_calls >= 1);

  /* check that an incremented schedule becomes ready at the right time */
  tt_assert(download_status_is_ready(&dls_attempt,
                                     current_time + delay1 - 1,
                                     1) == 0);
  tt_assert(download_status_is_ready(&dls_attempt,
                                     current_time + delay1,
                                     1) == 1);
  tt_assert(download_status_is_ready(&dls_attempt,
                                     current_time + delay1 + 1,
                                     1) == 1);

  /* check that a schedule isn't ready if it's had too many attempts */
  tt_assert(download_status_is_ready(&dls_attempt,
                                     current_time + delay1 + 10,
                                     0) == 0);

  /* Check what happens when we reach then run off the end of the schedule */
  mock_get_options_calls = 0;
  next_at = download_status_increment_attempt(&dls_attempt, "test",
                                              current_time);
  tt_assert(next_at == current_time + delay2);
  tt_assert(download_status_get_n_failures(&dls_attempt) == 0);
  tt_assert(download_status_get_n_attempts(&dls_attempt) == 2);
  tt_assert(mock_get_options_calls >= 1);

  mock_get_options_calls = 0;
  next_at = download_status_increment_attempt(&dls_attempt, "test",
                                              current_time);
  tt_assert(next_at == current_time + delay2);
  tt_assert(download_status_get_n_failures(&dls_attempt) == 0);
  tt_assert(download_status_get_n_attempts(&dls_attempt) == 3);
  tt_assert(mock_get_options_calls >= 1);

  /* Check what happens when we hit the attempt limit */
  mock_get_options_calls = 0;
  download_status_mark_impossible(&dls_attempt);
  next_at = download_status_increment_attempt(&dls_attempt, "test",
                                              current_time);
  tt_assert(next_at == TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_attempt)
            == IMPOSSIBLE_TO_DOWNLOAD);
  tt_assert(download_status_get_n_attempts(&dls_attempt)
            == IMPOSSIBLE_TO_DOWNLOAD);
  tt_assert(mock_get_options_calls >= 1);

  /* Check that an attempt reset doesn't reset at the limit */
  mock_get_options_calls = 0;
  download_status_reset(&dls_attempt);
  tt_assert(download_status_get_next_attempt_at(&dls_attempt)
            == TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_attempt)
            == IMPOSSIBLE_TO_DOWNLOAD);
  tt_assert(download_status_get_n_attempts(&dls_attempt)
            == IMPOSSIBLE_TO_DOWNLOAD);
  tt_assert(mock_get_options_calls == 0);

  /* Check that an attempt reset resets just before the limit */
  mock_get_options_calls = 0;
  dls_attempt.n_download_failures = IMPOSSIBLE_TO_DOWNLOAD - 1;
  dls_attempt.n_download_attempts = IMPOSSIBLE_TO_DOWNLOAD - 1;
  download_status_reset(&dls_attempt);
  /* we really want to test that it's equal to time(NULL) + delay0, but that's
   * an unrealiable test, because time(NULL) might change. */
  tt_assert(download_status_get_next_attempt_at(&dls_attempt)
            >= current_time + delay0);
  tt_assert(download_status_get_next_attempt_at(&dls_attempt)
            != TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_attempt) == 0);
  tt_assert(download_status_get_n_attempts(&dls_attempt) == 0);
  tt_assert(mock_get_options_calls >= 1);

  /* Check that attempt increments don't happen on failure-based schedules,
   * and that the attempt is set at the end of time */
  mock_get_options_calls = 0;
  next_at = download_status_increment_attempt(&dls_failure, "test",
                                              current_time);
  tt_assert(next_at == TIME_MAX);
  tt_assert(download_status_get_n_failures(&dls_failure) == 0);
  tt_assert(download_status_get_n_attempts(&dls_failure) == 0);
  tt_assert(mock_get_options_calls == 0);

 done:
  /* the pointers in schedule are allocated on the stack */
  smartlist_free(schedule);
  UNMOCK(get_options);
  mock_options = NULL;
  mock_get_options_calls = 0;
}

static void
test_dir_authdir_type_to_string(void *data)
{
  (void)data;
  char *res;

  tt_str_op(res = authdir_type_to_string(NO_DIRINFO), OP_EQ,
            "[Not an authority]");
  tor_free(res);

  tt_str_op(res = authdir_type_to_string(EXTRAINFO_DIRINFO), OP_EQ,
            "[Not an authority]");
  tor_free(res);

  tt_str_op(res = authdir_type_to_string(MICRODESC_DIRINFO), OP_EQ,
            "[Not an authority]");
  tor_free(res);

  tt_str_op(res = authdir_type_to_string(V3_DIRINFO), OP_EQ, "V3");
  tor_free(res);

  tt_str_op(res = authdir_type_to_string(BRIDGE_DIRINFO), OP_EQ, "Bridge");
  tor_free(res);

  tt_str_op(res = authdir_type_to_string(
            V3_DIRINFO | BRIDGE_DIRINFO | EXTRAINFO_DIRINFO), OP_EQ,
            "V3, Bridge");
  done:
  tor_free(res);
}

static void
test_dir_conn_purpose_to_string(void *data)
{
  (void)data;

#define EXPECT_CONN_PURPOSE(purpose, expected) \
  tt_str_op(dir_conn_purpose_to_string(purpose), OP_EQ, expected);

  EXPECT_CONN_PURPOSE(DIR_PURPOSE_UPLOAD_DIR, "server descriptor upload");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_UPLOAD_VOTE, "server vote upload");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_UPLOAD_SIGNATURES,
                      "consensus signature upload");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_FETCH_SERVERDESC, "server descriptor fetch");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_FETCH_EXTRAINFO, "extra-info fetch");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_FETCH_CONSENSUS,
                      "consensus network-status fetch");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_FETCH_CERTIFICATE, "authority cert fetch");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_FETCH_STATUS_VOTE, "status vote fetch");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_FETCH_DETACHED_SIGNATURES,
                      "consensus signature fetch");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_FETCH_RENDDESC_V2,
                      "hidden-service v2 descriptor fetch");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_UPLOAD_RENDDESC_V2,
                      "hidden-service v2 descriptor upload");
  EXPECT_CONN_PURPOSE(DIR_PURPOSE_FETCH_MICRODESC, "microdescriptor fetch");
  EXPECT_CONN_PURPOSE(1024, "(unknown)");

  done: ;
}

NS_DECL(int,
public_server_mode, (const or_options_t *options));

static int
NS(public_server_mode)(const or_options_t *options)
{
  (void)options;

  if (CALLED(public_server_mode)++ == 0) {
    return 1;
  }

  return 0;
}

static void
test_dir_should_use_directory_guards(void *data)
{
  or_options_t *options;
  char *errmsg = NULL;
  (void)data;

  NS_MOCK(public_server_mode);

  options = options_new();
  options_init(options);

  tt_int_op(should_use_directory_guards(options), OP_EQ, 0);
  tt_int_op(CALLED(public_server_mode), OP_EQ, 1);

  options->UseEntryGuardsAsDirGuards = 1;
  options->UseEntryGuards = 1;
  options->DownloadExtraInfo = 0;
  options->FetchDirInfoEarly = 0;
  options->FetchDirInfoExtraEarly = 0;
  options->FetchUselessDescriptors = 0;
  tt_int_op(should_use_directory_guards(options), OP_EQ, 1);
  tt_int_op(CALLED(public_server_mode), OP_EQ, 2);

  options->UseEntryGuards = 0;
  tt_int_op(should_use_directory_guards(options), OP_EQ, 0);
  tt_int_op(CALLED(public_server_mode), OP_EQ, 3);
  options->UseEntryGuards = 1;

  options->UseEntryGuardsAsDirGuards = 0;
  tt_int_op(should_use_directory_guards(options), OP_EQ, 0);
  tt_int_op(CALLED(public_server_mode), OP_EQ, 4);
  options->UseEntryGuardsAsDirGuards = 1;

  options->DownloadExtraInfo = 1;
  tt_int_op(should_use_directory_guards(options), OP_EQ, 0);
  tt_int_op(CALLED(public_server_mode), OP_EQ, 5);
  options->DownloadExtraInfo = 0;

  options->FetchDirInfoEarly = 1;
  tt_int_op(should_use_directory_guards(options), OP_EQ, 0);
  tt_int_op(CALLED(public_server_mode), OP_EQ, 6);
  options->FetchDirInfoEarly = 0;

  options->FetchDirInfoExtraEarly = 1;
  tt_int_op(should_use_directory_guards(options), OP_EQ, 0);
  tt_int_op(CALLED(public_server_mode), OP_EQ, 7);
  options->FetchDirInfoExtraEarly = 0;

  options->FetchUselessDescriptors = 1;
  tt_int_op(should_use_directory_guards(options), OP_EQ, 0);
  tt_int_op(CALLED(public_server_mode), OP_EQ, 8);
  options->FetchUselessDescriptors = 0;

  done:
    NS_UNMOCK(public_server_mode);
    or_options_free(options);
    tor_free(errmsg);
}

NS_DECL(void,
directory_initiate_command_routerstatus, (const routerstatus_t *status,
                                          uint8_t dir_purpose,
                                          uint8_t router_purpose,
                                          dir_indirection_t indirection,
                                          const char *resource,
                                          const char *payload,
                                          size_t payload_len,
                                          time_t if_modified_since));

static void
test_dir_should_not_init_request_to_ourselves(void *data)
{
  char digest[DIGEST_LEN];
  dir_server_t *ourself = NULL;
  crypto_pk_t *key = pk_generate(2);
  (void) data;

  NS_MOCK(directory_initiate_command_routerstatus);

  clear_dir_servers();
  routerlist_free_all();

  set_server_identity_key(key);
  crypto_pk_get_digest(key, (char*) &digest);
  ourself = trusted_dir_server_new("ourself", "127.0.0.1", 9059, 9060,
                                   NULL, digest,
                                   NULL, V3_DIRINFO, 1.0);

  tt_assert(ourself);
  dir_server_add(ourself);

  directory_get_from_all_authorities(DIR_PURPOSE_FETCH_STATUS_VOTE, 0, NULL);
  tt_int_op(CALLED(directory_initiate_command_routerstatus), OP_EQ, 0);

  directory_get_from_all_authorities(DIR_PURPOSE_FETCH_DETACHED_SIGNATURES, 0,
                                     NULL);

  tt_int_op(CALLED(directory_initiate_command_routerstatus), OP_EQ, 0);

  done:
    NS_UNMOCK(directory_initiate_command_routerstatus);
    clear_dir_servers();
    routerlist_free_all();
    crypto_pk_free(key);
}

static void
test_dir_should_not_init_request_to_dir_auths_without_v3_info(void *data)
{
  dir_server_t *ds = NULL;
  dirinfo_type_t dirinfo_type = BRIDGE_DIRINFO | EXTRAINFO_DIRINFO \
                                | MICRODESC_DIRINFO;
  (void) data;

  NS_MOCK(directory_initiate_command_routerstatus);

  clear_dir_servers();
  routerlist_free_all();

  ds = trusted_dir_server_new("ds", "10.0.0.1", 9059, 9060, NULL,
                              "12345678901234567890", NULL, dirinfo_type, 1.0);
  tt_assert(ds);
  dir_server_add(ds);

  directory_get_from_all_authorities(DIR_PURPOSE_FETCH_STATUS_VOTE, 0, NULL);
  tt_int_op(CALLED(directory_initiate_command_routerstatus), OP_EQ, 0);

  directory_get_from_all_authorities(DIR_PURPOSE_FETCH_DETACHED_SIGNATURES, 0,
                                     NULL);
  tt_int_op(CALLED(directory_initiate_command_routerstatus), OP_EQ, 0);

  done:
    NS_UNMOCK(directory_initiate_command_routerstatus);
    clear_dir_servers();
    routerlist_free_all();
}

static void
test_dir_should_init_request_to_dir_auths(void *data)
{
  dir_server_t *ds = NULL;
  (void) data;

  NS_MOCK(directory_initiate_command_routerstatus);

  clear_dir_servers();
  routerlist_free_all();

  ds = trusted_dir_server_new("ds", "10.0.0.1", 9059, 9060, NULL,
                              "12345678901234567890", NULL, V3_DIRINFO, 1.0);
  tt_assert(ds);
  dir_server_add(ds);

  directory_get_from_all_authorities(DIR_PURPOSE_FETCH_STATUS_VOTE, 0, NULL);
  tt_int_op(CALLED(directory_initiate_command_routerstatus), OP_EQ, 1);

  directory_get_from_all_authorities(DIR_PURPOSE_FETCH_DETACHED_SIGNATURES, 0,
                                     NULL);
  tt_int_op(CALLED(directory_initiate_command_routerstatus), OP_EQ, 2);

  done:
    NS_UNMOCK(directory_initiate_command_routerstatus);
    clear_dir_servers();
    routerlist_free_all();
}

void
NS(directory_initiate_command_routerstatus)(const routerstatus_t *status,
                                            uint8_t dir_purpose,
                                            uint8_t router_purpose,
                                            dir_indirection_t indirection,
                                            const char *resource,
                                            const char *payload,
                                            size_t payload_len,
                                            time_t if_modified_since)
{
  (void)status;
  (void)dir_purpose;
  (void)router_purpose;
  (void)indirection;
  (void)resource;
  (void)payload;
  (void)payload_len;
  (void)if_modified_since;
  CALLED(directory_initiate_command_routerstatus)++;
}

static void
test_dir_choose_compression_level(void* data)
{
  (void)data;

  /* It starts under_memory_pressure */
  tt_int_op(have_been_under_memory_pressure(), OP_EQ, 1);

  tt_assert(HIGH_COMPRESSION == choose_compression_level(-1));
  tt_assert(LOW_COMPRESSION == choose_compression_level(1024-1));
  tt_assert(MEDIUM_COMPRESSION == choose_compression_level(2048-1));
  tt_assert(HIGH_COMPRESSION == choose_compression_level(2048));

  /* Reset under_memory_pressure timer */
  cell_queues_check_size();
  tt_int_op(have_been_under_memory_pressure(), OP_EQ, 0);

  tt_assert(HIGH_COMPRESSION == choose_compression_level(-1));
  tt_assert(HIGH_COMPRESSION == choose_compression_level(1024-1));
  tt_assert(HIGH_COMPRESSION == choose_compression_level(2048-1));
  tt_assert(HIGH_COMPRESSION == choose_compression_level(2048));

  done: ;
}

static int mock_networkstatus_consensus_is_bootstrapping_value = 0;
static int
mock_networkstatus_consensus_is_bootstrapping(time_t now)
{
  (void)now;
  return mock_networkstatus_consensus_is_bootstrapping_value;
}

static int mock_networkstatus_consensus_can_use_extra_fallbacks_value = 0;
static int
mock_networkstatus_consensus_can_use_extra_fallbacks(
                                                  const or_options_t *options)
{
  (void)options;
  return mock_networkstatus_consensus_can_use_extra_fallbacks_value;
}

/* data is a 2 character nul-terminated string.
 * If data[0] is 'b', set bootstrapping, anything else means not bootstrapping
 * If data[1] is 'f', set extra fallbacks, anything else means no extra
 * fallbacks.
 */
static void
test_dir_find_dl_schedule(void* data)
{
  const char *str = (const char *)data;

  tt_assert(strlen(data) == 2);

  if (str[0] == 'b') {
    mock_networkstatus_consensus_is_bootstrapping_value = 1;
  } else {
    mock_networkstatus_consensus_is_bootstrapping_value = 0;
  }

  if (str[1] == 'f') {
    mock_networkstatus_consensus_can_use_extra_fallbacks_value = 1;
  } else {
    mock_networkstatus_consensus_can_use_extra_fallbacks_value = 0;
  }

  MOCK(networkstatus_consensus_is_bootstrapping,
       mock_networkstatus_consensus_is_bootstrapping);
  MOCK(networkstatus_consensus_can_use_extra_fallbacks,
       mock_networkstatus_consensus_can_use_extra_fallbacks);

  download_status_t dls;
  smartlist_t server, client, server_cons, client_cons;
  smartlist_t client_boot_auth_only_cons, client_boot_auth_cons;
  smartlist_t client_boot_fallback_cons, bridge;

  mock_options = malloc(sizeof(or_options_t));
  reset_options(mock_options, &mock_get_options_calls);
  MOCK(get_options, mock_get_options);

  mock_options->TestingServerDownloadSchedule = &server;
  mock_options->TestingClientDownloadSchedule = &client;
  mock_options->TestingServerConsensusDownloadSchedule = &server_cons;
  mock_options->TestingClientConsensusDownloadSchedule = &client_cons;
  mock_options->ClientBootstrapConsensusAuthorityOnlyDownloadSchedule =
    &client_boot_auth_only_cons;
  mock_options->ClientBootstrapConsensusAuthorityDownloadSchedule =
    &client_boot_auth_cons;
  mock_options->ClientBootstrapConsensusFallbackDownloadSchedule =
    &client_boot_fallback_cons;
  mock_options->TestingBridgeDownloadSchedule = &bridge;

  dls.schedule = DL_SCHED_GENERIC;
  /* client */
  mock_options->ClientOnly = 1;
  tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ, &client);
  mock_options->ClientOnly = 0;

  /* dir mode */
  mock_options->DirPort_set = 1;
  mock_options->DirCache = 1;
  tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ, &server);
  mock_options->DirPort_set = 0;
  mock_options->DirCache = 0;

  dls.schedule = DL_SCHED_CONSENSUS;
  /* public server mode */
  mock_options->ORPort_set = 1;
  tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ, &server_cons);
  mock_options->ORPort_set = 0;

  /* client and bridge modes */
  if (networkstatus_consensus_is_bootstrapping(time(NULL))) {
    if (networkstatus_consensus_can_use_extra_fallbacks(mock_options)) {
      dls.want_authority = 1;
      /* client */
      mock_options->ClientOnly = 1;
      tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ,
                &client_boot_auth_cons);
      mock_options->ClientOnly = 0;

      /* bridge relay */
      mock_options->ORPort_set = 1;
      mock_options->BridgeRelay = 1;
      tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ,
                &client_boot_auth_cons);
      mock_options->ORPort_set = 0;
      mock_options->BridgeRelay = 0;

      dls.want_authority = 0;
      /* client */
      mock_options->ClientOnly = 1;
      tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ,
                &client_boot_fallback_cons);
      mock_options->ClientOnly = 0;

      /* bridge relay */
      mock_options->ORPort_set = 1;
      mock_options->BridgeRelay = 1;
      tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ,
                &client_boot_fallback_cons);
      mock_options->ORPort_set = 0;
      mock_options->BridgeRelay = 0;

    } else {
      /* dls.want_authority is ignored */
      /* client */
      mock_options->ClientOnly = 1;
      tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ,
                &client_boot_auth_only_cons);
      mock_options->ClientOnly = 0;

      /* bridge relay */
      mock_options->ORPort_set = 1;
      mock_options->BridgeRelay = 1;
      tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ,
                &client_boot_auth_only_cons);
      mock_options->ORPort_set = 0;
      mock_options->BridgeRelay = 0;
    }
  } else {
    /* client */
    mock_options->ClientOnly = 1;
    tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ,
              &client_cons);
    mock_options->ClientOnly = 0;

    /* bridge relay */
    mock_options->ORPort_set = 1;
    mock_options->BridgeRelay = 1;
    tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ,
              &client_cons);
    mock_options->ORPort_set = 0;
    mock_options->BridgeRelay = 0;
  }

  dls.schedule = DL_SCHED_BRIDGE;
  /* client */
  mock_options->ClientOnly = 1;
  tt_ptr_op(find_dl_schedule(&dls, mock_options), OP_EQ, &bridge);

 done:
  UNMOCK(networkstatus_consensus_is_bootstrapping);
  UNMOCK(networkstatus_consensus_can_use_extra_fallbacks);
  UNMOCK(get_options);
  free(mock_options);
  mock_options = NULL;
}

#define DIR_LEGACY(name)                             \
  { #name, test_dir_ ## name , TT_FORK, NULL, NULL }

#define DIR(name,flags)                              \
  { #name, test_dir_##name, (flags), NULL, NULL }

/* where arg is a string constant */
#define DIR_ARG(name,flags,arg)                      \
  { #name "_" arg, test_dir_##name, (flags), &passthrough_setup, (void*) arg }

struct testcase_t dir_tests[] = {
  DIR_LEGACY(nicknames),
  DIR_LEGACY(formats),
  DIR(routerinfo_parsing, 0),
  DIR(extrainfo_parsing, 0),
  DIR(parse_router_list, TT_FORK),
  DIR(load_routers, TT_FORK),
  DIR(load_extrainfo, TT_FORK),
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
  DIR(dirserv_set_routerstatus_testing, 0),
  DIR(http_handling, 0),
  DIR(purpose_needs_anonymity, 0),
  DIR(fetch_type, 0),
  DIR(packages, 0),
  DIR(download_status_schedule, 0),
  DIR(download_status_increment, 0),
  DIR(authdir_type_to_string, 0),
  DIR(conn_purpose_to_string, 0),
  DIR(should_use_directory_guards, 0),
  DIR(should_not_init_request_to_ourselves, TT_FORK),
  DIR(should_not_init_request_to_dir_auths_without_v3_info, 0),
  DIR(should_init_request_to_dir_auths, 0),
  DIR(choose_compression_level, 0),
  DIR_ARG(find_dl_schedule, TT_FORK, "bf"),
  DIR_ARG(find_dl_schedule, TT_FORK, "ba"),
  DIR_ARG(find_dl_schedule, TT_FORK, "cf"),
  DIR_ARG(find_dl_schedule, TT_FORK, "ca"),
  END_OF_TESTCASES
};

