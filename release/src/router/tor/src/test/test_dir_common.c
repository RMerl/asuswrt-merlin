/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2016, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "orconfig.h"
#define DIRVOTE_PRIVATE
#include "crypto.h"
#include "test.h"
#include "container.h"
#include "or.h"
#include "dirvote.h"
#include "nodelist.h"
#include "routerlist.h"
#include "test_dir_common.h"

void dir_common_setup_vote(networkstatus_t **vote, time_t now);
networkstatus_t * dir_common_add_rs_and_parse(networkstatus_t *vote,
                            networkstatus_t **vote_out,
               vote_routerstatus_t * (*vrs_gen)(int idx, time_t now),
                            crypto_pk_t *sign_skey, int *n_vrs,
                            time_t now, int clear_rl);

/** Initialize and set auth certs and keys
 * Returns 0 on success, -1 on failure. Clean up handled by caller.
 */
int
dir_common_authority_pk_init(authority_cert_t **cert1,
                             authority_cert_t **cert2,
                             authority_cert_t **cert3,
                             crypto_pk_t **sign_skey_1,
                             crypto_pk_t **sign_skey_2,
                             crypto_pk_t **sign_skey_3)
{
  /* Parse certificates and keys. */
  authority_cert_t *cert;
  cert = authority_cert_parse_from_string(AUTHORITY_CERT_1, NULL);
  tt_assert(cert);
  tt_assert(cert->identity_key);
  *cert1 = cert;
  tt_assert(*cert1);
  *cert2 = authority_cert_parse_from_string(AUTHORITY_CERT_2, NULL);
  tt_assert(*cert2);
  *cert3 = authority_cert_parse_from_string(AUTHORITY_CERT_3, NULL);
  tt_assert(*cert3);
  *sign_skey_1 = crypto_pk_new();
  *sign_skey_2 = crypto_pk_new();
  *sign_skey_3 = crypto_pk_new();

  tt_assert(!crypto_pk_read_private_key_from_string(*sign_skey_1,
                                                   AUTHORITY_SIGNKEY_1, -1));
  tt_assert(!crypto_pk_read_private_key_from_string(*sign_skey_2,
                                                   AUTHORITY_SIGNKEY_2, -1));
  tt_assert(!crypto_pk_read_private_key_from_string(*sign_skey_3,
                                                   AUTHORITY_SIGNKEY_3, -1));

  tt_assert(!crypto_pk_cmp_keys(*sign_skey_1, (*cert1)->signing_key));
  tt_assert(!crypto_pk_cmp_keys(*sign_skey_2, (*cert2)->signing_key));

  return 0;
 done:
  return -1;
}

/**
 * Generate a routerstatus for v3_networkstatus test.
 */
vote_routerstatus_t *
dir_common_gen_routerstatus_for_v3ns(int idx, time_t now)
{
  vote_routerstatus_t *vrs=NULL;
  routerstatus_t *rs = NULL;
  tor_addr_t addr_ipv6;
  char *method_list = NULL;

  switch (idx) {
    case 0:
      /* Generate the first routerstatus. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.1.2.14");
      rs->published_on = now-1500;
      strlcpy(rs->nickname, "router2", sizeof(rs->nickname));
      memset(rs->identity_digest, TEST_DIR_ROUTER_ID_1, DIGEST_LEN);
      memset(rs->descriptor_digest, TEST_DIR_ROUTER_DD_1, DIGEST_LEN);
      rs->addr = 0x99008801;
      rs->or_port = 443;
      rs->dir_port = 8000;
      /* all flags but running and v2dir cleared */
      rs->is_flagged_running = 1;
      rs->is_v2_dir = 1;
      rs->is_valid = 1; /* xxxxx */
      break;
    case 1:
      /* Generate the second routerstatus. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.2.0.5");
      rs->published_on = now-1000;
      strlcpy(rs->nickname, "router1", sizeof(rs->nickname));
      memset(rs->identity_digest, TEST_DIR_ROUTER_ID_2, DIGEST_LEN);
      memset(rs->descriptor_digest, TEST_DIR_ROUTER_DD_2, DIGEST_LEN);
      rs->addr = 0x99009901;
      rs->or_port = 443;
      rs->dir_port = 0;
      tor_addr_parse(&addr_ipv6, "[1:2:3::4]");
      tor_addr_copy(&rs->ipv6_addr, &addr_ipv6);
      rs->ipv6_orport = 4711;
      rs->is_exit = rs->is_stable = rs->is_fast = rs->is_flagged_running =
        rs->is_valid = rs->is_possible_guard = rs->is_v2_dir = 1;
      break;
    case 2:
      /* Generate the third routerstatus. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.1.0.3");
      rs->published_on = now-1000;
      strlcpy(rs->nickname, "router3", sizeof(rs->nickname));
      memset(rs->identity_digest, TEST_DIR_ROUTER_ID_3, DIGEST_LEN);
      memset(rs->descriptor_digest, TEST_DIR_ROUTER_DD_3, DIGEST_LEN);
      rs->addr = 0xAA009901;
      rs->or_port = 400;
      rs->dir_port = 9999;
      rs->is_authority = rs->is_exit = rs->is_stable = rs->is_fast =
        rs->is_flagged_running = rs->is_valid = rs->is_v2_dir =
        rs->is_possible_guard = 1;
      break;
    case 3:
      /* Generate a fourth routerstatus that is not running. */
      vrs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      rs = &vrs->status;
      vrs->version = tor_strdup("0.1.6.3");
      rs->published_on = now-1000;
      strlcpy(rs->nickname, "router4", sizeof(rs->nickname));
      memset(rs->identity_digest, TEST_DIR_ROUTER_ID_4, DIGEST_LEN);
      memset(rs->descriptor_digest, TEST_DIR_ROUTER_DD_4, DIGEST_LEN);
      rs->addr = 0xC0000203;
      rs->or_port = 500;
      rs->dir_port = 1999;
      rs->is_v2_dir = 1;
      /* Running flag (and others) cleared */
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
    method_list = make_consensus_method_list(MIN_SUPPORTED_CONSENSUS_METHOD,
                                             MAX_SUPPORTED_CONSENSUS_METHOD,
                                             ",");
    tor_asprintf(&vrs->microdesc->microdesc_hash_line,
                 "m %s "
                 "sha256=xyzajkldsdsajdadlsdjaslsdksdjlsdjsdaskdaaa%d\n",
                 method_list, idx);
  }

 done:
  tor_free(method_list);
  return vrs;
}

/** Initialize networkstatus vote object attributes. */
void
dir_common_setup_vote(networkstatus_t **vote, time_t now)
{
  *vote = tor_malloc_zero(sizeof(networkstatus_t));
  (*vote)->type = NS_TYPE_VOTE;
  (*vote)->published = now;
  (*vote)->supported_methods = smartlist_new();
  (*vote)->known_flags = smartlist_new();
  (*vote)->net_params = smartlist_new();
  (*vote)->routerstatus_list = smartlist_new();
  (*vote)->voters = smartlist_new();
}

/** Helper: Make a new routerinfo containing the right information for a
 * given vote_routerstatus_t. */
routerinfo_t *
dir_common_generate_ri_from_rs(const vote_routerstatus_t *vrs)
{
  routerinfo_t *r;
  const routerstatus_t *rs = &vrs->status;
  static time_t published = 0;

  r = tor_malloc_zero(sizeof(routerinfo_t));
  r->cert_expiration_time = TIME_MAX;
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

/** Create routerstatuses and signed vote.
 * Create routerstatuses using *vrs_gen* and add them to global routerlist.
 * Next, create signed vote using *sign_skey* and *vote*, which should have
 * predefined header fields.
 * Setting *clear_rl* clears the global routerlist before adding the new
 * routers.
 * Return the signed vote, same as *vote_out*. Save the number of routers added
 * in *n_vrs*.
 */
networkstatus_t *
dir_common_add_rs_and_parse(networkstatus_t *vote, networkstatus_t **vote_out,
                       vote_routerstatus_t * (*vrs_gen)(int idx, time_t now),
                       crypto_pk_t *sign_skey, int *n_vrs, time_t now,
                       int clear_rl)
{
  vote_routerstatus_t *vrs;
  char *v_text=NULL;
  const char *msg=NULL;
  int idx;
  was_router_added_t router_added = -1;
  *vote_out = NULL;

  if (clear_rl) {
    nodelist_free_all();
    routerlist_free_all();
  }

  idx = 0;
  do {
    vrs = vrs_gen(idx, now);
    if (vrs) {
      smartlist_add(vote->routerstatus_list, vrs);
      router_added =
        router_add_to_routerlist(dir_common_generate_ri_from_rs(vrs),
                                 &msg,0,0);
      tt_assert(router_added >= 0);
      ++idx;
    }
  } while (vrs);
  *n_vrs = idx;

  /* dump the vote and try to parse it. */
  v_text = format_networkstatus_vote(sign_skey, vote);
  tt_assert(v_text);
  *vote_out = networkstatus_parse_vote_from_string(v_text, NULL, NS_TYPE_VOTE);

 done:
  if (v_text)
    tor_free(v_text);

  return *vote_out;
}

/** Create a fake *vote* where *cert* describes the signer, *sign_skey*
 * is the signing key, and *vrs_gen* is the function we'll use to create the
 * routers on which we're voting.
 * We pass *vote_out*, *n_vrs*, and *clear_rl* directly to vrs_gen().
 * Return 0 on success, return -1 on failure.
 */
int
dir_common_construct_vote_1(networkstatus_t **vote, authority_cert_t *cert,
                        crypto_pk_t *sign_skey,
                        vote_routerstatus_t * (*vrs_gen)(int idx, time_t now),
                        networkstatus_t **vote_out, int *n_vrs,
                        time_t now, int clear_rl)
{
  networkstatus_voter_info_t *voter;

  dir_common_setup_vote(vote, now);
  (*vote)->valid_after = now+1000;
  (*vote)->fresh_until = now+2000;
  (*vote)->valid_until = now+3000;
  (*vote)->vote_seconds = 100;
  (*vote)->dist_seconds = 200;
  smartlist_split_string((*vote)->supported_methods, "1 2 3", NULL, 0, -1);
  (*vote)->client_versions = tor_strdup("0.1.2.14,0.1.2.15");
  (*vote)->server_versions = tor_strdup("0.1.2.14,0.1.2.15,0.1.2.16");
  smartlist_split_string((*vote)->known_flags,
                     "Authority Exit Fast Guard Running Stable V2Dir Valid",
                     0, SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  voter = tor_malloc_zero(sizeof(networkstatus_voter_info_t));
  voter->nickname = tor_strdup("Voter1");
  voter->address = tor_strdup("1.2.3.4");
  voter->addr = 0x01020304;
  voter->dir_port = 80;
  voter->or_port = 9000;
  voter->contact = tor_strdup("voter@example.com");
  crypto_pk_get_digest(cert->identity_key, voter->identity_digest);
  /*
   * Set up a vote; generate it; try to parse it.
   */
  smartlist_add((*vote)->voters, voter);
  (*vote)->cert = authority_cert_dup(cert);
  smartlist_split_string((*vote)->net_params, "circuitwindow=101 foo=990",
                         NULL, 0, 0);
  *n_vrs = 0;
  /* add routerstatuses */
  if (!dir_common_add_rs_and_parse(*vote, vote_out, vrs_gen, sign_skey,
                                  n_vrs, now, clear_rl))
    return -1;

  return 0;
}

/** See dir_common_construct_vote_1.
 * Produces a vote with slightly different values.
 */
int
dir_common_construct_vote_2(networkstatus_t **vote, authority_cert_t *cert,
                        crypto_pk_t *sign_skey,
                        vote_routerstatus_t * (*vrs_gen)(int idx, time_t now),
                        networkstatus_t **vote_out, int *n_vrs,
                        time_t now, int clear_rl)
{
  networkstatus_voter_info_t *voter;

  dir_common_setup_vote(vote, now);
  (*vote)->type = NS_TYPE_VOTE;
  (*vote)->published += 1;
  (*vote)->valid_after = now+1000;
  (*vote)->fresh_until = now+3005;
  (*vote)->valid_until = now+3000;
  (*vote)->vote_seconds = 100;
  (*vote)->dist_seconds = 300;
  smartlist_split_string((*vote)->supported_methods, "1 2 3", NULL, 0, -1);
  smartlist_split_string((*vote)->known_flags,
                         "Authority Exit Fast Guard MadeOfCheese MadeOfTin "
                         "Running Stable V2Dir Valid", 0,
                         SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  voter = tor_malloc_zero(sizeof(networkstatus_voter_info_t));
  voter->nickname = tor_strdup("Voter2");
  voter->address = tor_strdup("2.3.4.5");
  voter->addr = 0x02030405;
  voter->dir_port = 80;
  voter->or_port = 9000;
  voter->contact = tor_strdup("voter@example.com");
  crypto_pk_get_digest(cert->identity_key, voter->identity_digest);
  /*
   * Set up a vote; generate it; try to parse it.
   */
  smartlist_add((*vote)->voters, voter);
  (*vote)->cert = authority_cert_dup(cert);
  if (! (*vote)->net_params)
    (*vote)->net_params = smartlist_new();
  smartlist_split_string((*vote)->net_params,
                         "bar=2000000000 circuitwindow=20",
                         NULL, 0, 0);
  /* add routerstatuses */
  /* dump the vote and try to parse it. */
  dir_common_add_rs_and_parse(*vote, vote_out, vrs_gen, sign_skey,
                              n_vrs, now, clear_rl);

  return 0;
}

/** See dir_common_construct_vote_1.
 * Produces a vote with slightly different values. Adds a legacy key.
 */
int
dir_common_construct_vote_3(networkstatus_t **vote, authority_cert_t *cert,
                        crypto_pk_t *sign_skey,
                        vote_routerstatus_t * (*vrs_gen)(int idx, time_t now),
                        networkstatus_t **vote_out, int *n_vrs,
                        time_t now, int clear_rl)
{
  networkstatus_voter_info_t *voter;

  dir_common_setup_vote(vote, now);
  (*vote)->valid_after = now+1000;
  (*vote)->fresh_until = now+2003;
  (*vote)->valid_until = now+3000;
  (*vote)->vote_seconds = 100;
  (*vote)->dist_seconds = 250;
  smartlist_split_string((*vote)->supported_methods, "1 2 3 4", NULL, 0, -1);
  (*vote)->client_versions = tor_strdup("0.1.2.14,0.1.2.17");
  (*vote)->server_versions = tor_strdup("0.1.2.10,0.1.2.15,0.1.2.16");
  smartlist_split_string((*vote)->known_flags,
                     "Authority Exit Fast Guard Running Stable V2Dir Valid",
                     0, SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
  voter = tor_malloc_zero(sizeof(networkstatus_voter_info_t));
  voter->nickname = tor_strdup("Voter2");
  voter->address = tor_strdup("3.4.5.6");
  voter->addr = 0x03040506;
  voter->dir_port = 80;
  voter->or_port = 9000;
  voter->contact = tor_strdup("voter@example.com");
  crypto_pk_get_digest(cert->identity_key, voter->identity_digest);
  memset(voter->legacy_id_digest, (int)'A', DIGEST_LEN);
  /*
   * Set up a vote; generate it; try to parse it.
   */
  smartlist_add((*vote)->voters, voter);
  (*vote)->cert = authority_cert_dup(cert);
  smartlist_split_string((*vote)->net_params, "circuitwindow=80 foo=660",
                         NULL, 0, 0);
  /* add routerstatuses */
  /* dump the vote and try to parse it. */
  dir_common_add_rs_and_parse(*vote, vote_out, vrs_gen, sign_skey,
                              n_vrs, now, clear_rl);

  return 0;
}

