/* Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

/**
 * \file policies.c
 * \brief Code to parse and use address policies and exit policies.
 **/

#include "or.h"
#include "config.h"
#include "dirserv.h"
#include "nodelist.h"
#include "policies.h"
#include "router.h"
#include "routerparse.h"
#include "geoip.h"
#include "ht.h"

/** Policy that addresses for incoming SOCKS connections must match. */
static smartlist_t *socks_policy = NULL;
/** Policy that addresses for incoming directory connections must match. */
static smartlist_t *dir_policy = NULL;
/** Policy that addresses for incoming router descriptors must match in order
 * to be published by us. */
static smartlist_t *authdir_reject_policy = NULL;
/** Policy that addresses for incoming router descriptors must match in order
 * to be marked as valid in our networkstatus. */
static smartlist_t *authdir_invalid_policy = NULL;
/** Policy that addresses for incoming router descriptors must <b>not</b>
 * match in order to not be marked as BadDirectory. */
static smartlist_t *authdir_baddir_policy = NULL;
/** Policy that addresses for incoming router descriptors must <b>not</b>
 * match in order to not be marked as BadExit. */
static smartlist_t *authdir_badexit_policy = NULL;

/** Parsed addr_policy_t describing which addresses we believe we can start
 * circuits at. */
static smartlist_t *reachable_or_addr_policy = NULL;
/** Parsed addr_policy_t describing which addresses we believe we can connect
 * to directories at. */
static smartlist_t *reachable_dir_addr_policy = NULL;

/** Element of an exit policy summary */
typedef struct policy_summary_item_t {
    uint16_t prt_min; /**< Lowest port number to accept/reject. */
    uint16_t prt_max; /**< Highest port number to accept/reject. */
    uint64_t reject_count; /**< Number of IP-Addresses that are rejected to
                                this port range. */
    unsigned int accepted:1; /** Has this port already been accepted */
} policy_summary_item_t;

/** Private networks.  This list is used in two places, once to expand the
 *  "private" keyword when parsing our own exit policy, secondly to ignore
 *  just such networks when building exit policy summaries.  It is important
 *  that all authorities agree on that list when creating summaries, so don't
 *  just change this without a proper migration plan and a proposal and stuff.
 */
static const char *private_nets[] = {
  "0.0.0.0/8", "169.254.0.0/16",
  "127.0.0.0/8", "192.168.0.0/16", "10.0.0.0/8", "172.16.0.0/12",
  "[::]/8",
  "[fc00::]/7", "[fe80::]/10", "[fec0::]/10", "[ff00::]/8", "[::]/127",
  NULL
};

/** Replace all "private" entries in *<b>policy</b> with their expanded
 * equivalents. */
void
policy_expand_private(smartlist_t **policy)
{
  uint16_t port_min, port_max;

  int i;
  smartlist_t *tmp;

  if (!*policy) /*XXXX disallow NULL policies? */
    return;

  tmp = smartlist_new();

  SMARTLIST_FOREACH_BEGIN(*policy, addr_policy_t *, p) {
     if (! p->is_private) {
       smartlist_add(tmp, p);
       continue;
     }
     for (i = 0; private_nets[i]; ++i) {
       addr_policy_t newpolicy;
       memcpy(&newpolicy, p, sizeof(addr_policy_t));
       newpolicy.is_private = 0;
       newpolicy.is_canonical = 0;
       if (tor_addr_parse_mask_ports(private_nets[i], 0,
                               &newpolicy.addr,
                               &newpolicy.maskbits, &port_min, &port_max)<0) {
         tor_assert(0);
       }
       smartlist_add(tmp, addr_policy_get_canonical_entry(&newpolicy));
     }
     addr_policy_free(p);
  } SMARTLIST_FOREACH_END(p);

  smartlist_free(*policy);
  *policy = tmp;
}

/** Expand each of the AF_UNSPEC elements in *<b>policy</b> (which indicate
 * protocol-neutral wildcards) into a pair of wildcard elements: one IPv4-
 * specific and one IPv6-specific. */
void
policy_expand_unspec(smartlist_t **policy)
{
  smartlist_t *tmp;
  if (!*policy)
    return;

  tmp = smartlist_new();
  SMARTLIST_FOREACH_BEGIN(*policy, addr_policy_t *, p) {
    sa_family_t family = tor_addr_family(&p->addr);
    if (family == AF_INET6 || family == AF_INET || p->is_private) {
      smartlist_add(tmp, p);
    } else if (family == AF_UNSPEC) {
      addr_policy_t newpolicy_ipv4;
      addr_policy_t newpolicy_ipv6;
      memcpy(&newpolicy_ipv4, p, sizeof(addr_policy_t));
      memcpy(&newpolicy_ipv6, p, sizeof(addr_policy_t));
      newpolicy_ipv4.is_canonical = 0;
      newpolicy_ipv6.is_canonical = 0;
      if (p->maskbits != 0) {
        log_warn(LD_BUG, "AF_UNSPEC policy with maskbits==%d", p->maskbits);
        newpolicy_ipv4.maskbits = 0;
        newpolicy_ipv6.maskbits = 0;
      }
      tor_addr_from_ipv4h(&newpolicy_ipv4.addr, 0);
      tor_addr_from_ipv6_bytes(&newpolicy_ipv6.addr,
                               "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0");
      smartlist_add(tmp, addr_policy_get_canonical_entry(&newpolicy_ipv4));
      smartlist_add(tmp, addr_policy_get_canonical_entry(&newpolicy_ipv6));
      addr_policy_free(p);
    } else {
      log_warn(LD_BUG, "Funny-looking address policy with family %d", family);
      smartlist_add(tmp, p);
    }
  } SMARTLIST_FOREACH_END(p);

  smartlist_free(*policy);
  *policy = tmp;
}

/**
 * Given a linked list of config lines containing "allow" and "deny"
 * tokens, parse them and append the result to <b>dest</b>. Return -1
 * if any tokens are malformed (and don't append any), else return 0.
 *
 * If <b>assume_action</b> is nonnegative, then insert its action
 * (ADDR_POLICY_ACCEPT or ADDR_POLICY_REJECT) for items that specify no
 * action.
 */
static int
parse_addr_policy(config_line_t *cfg, smartlist_t **dest,
                  int assume_action)
{
  smartlist_t *result;
  smartlist_t *entries;
  addr_policy_t *item;
  int r = 0;

  if (!cfg)
    return 0;

  result = smartlist_new();
  entries = smartlist_new();
  for (; cfg; cfg = cfg->next) {
    smartlist_split_string(entries, cfg->value, ",",
                           SPLIT_SKIP_SPACE|SPLIT_IGNORE_BLANK, 0);
    SMARTLIST_FOREACH_BEGIN(entries, const char *, ent) {
      log_debug(LD_CONFIG,"Adding new entry '%s'",ent);
      item = router_parse_addr_policy_item_from_string(ent, assume_action);
      if (item) {
        smartlist_add(result, item);
      } else {
        log_warn(LD_CONFIG,"Malformed policy '%s'.", ent);
        r = -1;
      }
    } SMARTLIST_FOREACH_END(ent);
    SMARTLIST_FOREACH(entries, char *, ent, tor_free(ent));
    smartlist_clear(entries);
  }
  smartlist_free(entries);
  if (r == -1) {
    addr_policy_list_free(result);
  } else {
    policy_expand_private(&result);
    policy_expand_unspec(&result);

    if (*dest) {
      smartlist_add_all(*dest, result);
      smartlist_free(result);
    } else {
      *dest = result;
    }
  }

  return r;
}

/** Helper: parse the Reachable(Dir|OR)?Addresses fields into
 * reachable_(or|dir)_addr_policy.  The options should already have
 * been validated by validate_addr_policies.
 */
static int
parse_reachable_addresses(void)
{
  const or_options_t *options = get_options();
  int ret = 0;

  if (options->ReachableDirAddresses &&
      options->ReachableORAddresses &&
      options->ReachableAddresses) {
    log_warn(LD_CONFIG,
             "Both ReachableDirAddresses and ReachableORAddresses are set. "
             "ReachableAddresses setting will be ignored.");
  }
  addr_policy_list_free(reachable_or_addr_policy);
  reachable_or_addr_policy = NULL;
  if (!options->ReachableORAddresses && options->ReachableAddresses)
    log_info(LD_CONFIG,
             "Using ReachableAddresses as ReachableORAddresses.");
  if (parse_addr_policy(options->ReachableORAddresses ?
                          options->ReachableORAddresses :
                          options->ReachableAddresses,
                        &reachable_or_addr_policy, ADDR_POLICY_ACCEPT)) {
    log_warn(LD_CONFIG,
             "Error parsing Reachable%sAddresses entry; ignoring.",
             options->ReachableORAddresses ? "OR" : "");
    ret = -1;
  }

  addr_policy_list_free(reachable_dir_addr_policy);
  reachable_dir_addr_policy = NULL;
  if (!options->ReachableDirAddresses && options->ReachableAddresses)
    log_info(LD_CONFIG,
             "Using ReachableAddresses as ReachableDirAddresses");
  if (parse_addr_policy(options->ReachableDirAddresses ?
                          options->ReachableDirAddresses :
                          options->ReachableAddresses,
                        &reachable_dir_addr_policy, ADDR_POLICY_ACCEPT)) {
    if (options->ReachableDirAddresses)
      log_warn(LD_CONFIG,
               "Error parsing ReachableDirAddresses entry; ignoring.");
    ret = -1;
  }
  return ret;
}

/** Return true iff the firewall options might block any address:port
 * combination.
 */
int
firewall_is_fascist_or(void)
{
  return reachable_or_addr_policy != NULL;
}

/** Return true iff <b>policy</b> (possibly NULL) will allow a
 * connection to <b>addr</b>:<b>port</b>.
 */
static int
addr_policy_permits_tor_addr(const tor_addr_t *addr, uint16_t port,
                            smartlist_t *policy)
{
  addr_policy_result_t p;
  p = compare_tor_addr_to_addr_policy(addr, port, policy);
  switch (p) {
    case ADDR_POLICY_PROBABLY_ACCEPTED:
    case ADDR_POLICY_ACCEPTED:
      return 1;
    case ADDR_POLICY_PROBABLY_REJECTED:
    case ADDR_POLICY_REJECTED:
      return 0;
    default:
      log_warn(LD_BUG, "Unexpected result: %d", (int)p);
      return 0;
  }
}

/** Return true iff <b> policy</b> (possibly NULL) will allow a connection to
 * <b>addr</b>:<b>port</b>.  <b>addr</b> is an IPv4 address given in host
 * order. */
/* XXXX deprecate when possible. */
static int
addr_policy_permits_address(uint32_t addr, uint16_t port,
                            smartlist_t *policy)
{
  tor_addr_t a;
  tor_addr_from_ipv4h(&a, addr);
  return addr_policy_permits_tor_addr(&a, port, policy);
}

/** Return true iff we think our firewall will let us make an OR connection to
 * addr:port. */
int
fascist_firewall_allows_address_or(const tor_addr_t *addr, uint16_t port)
{
  return addr_policy_permits_tor_addr(addr, port,
                                     reachable_or_addr_policy);
}

/** Return true iff we think our firewall will let us make an OR connection to
 * <b>ri</b>. */
int
fascist_firewall_allows_or(const routerinfo_t *ri)
{
  /* XXXX proposal 118 */
  tor_addr_t addr;
  tor_addr_from_ipv4h(&addr, ri->addr);
  return fascist_firewall_allows_address_or(&addr, ri->or_port);
}

/** Return true iff we think our firewall will let us make an OR connection to
 * <b>node</b>. */
int
fascist_firewall_allows_node(const node_t *node)
{
  if (node->ri) {
    return fascist_firewall_allows_or(node->ri);
  } else if (node->rs) {
    tor_addr_t addr;
    tor_addr_from_ipv4h(&addr, node->rs->addr);
    return fascist_firewall_allows_address_or(&addr, node->rs->or_port);
  } else {
    return 1;
  }
}

/** Return true iff we think our firewall will let us make a directory
 * connection to addr:port. */
int
fascist_firewall_allows_address_dir(const tor_addr_t *addr, uint16_t port)
{
  return addr_policy_permits_tor_addr(addr, port,
                                      reachable_dir_addr_policy);
}

/** Return 1 if <b>addr</b> is permitted to connect to our dir port,
 * based on <b>dir_policy</b>. Else return 0.
 */
int
dir_policy_permits_address(const tor_addr_t *addr)
{
  return addr_policy_permits_tor_addr(addr, 1, dir_policy);
}

/** Return 1 if <b>addr</b> is permitted to connect to our socks port,
 * based on <b>socks_policy</b>. Else return 0.
 */
int
socks_policy_permits_address(const tor_addr_t *addr)
{
  return addr_policy_permits_tor_addr(addr, 1, socks_policy);
}

/** Return true iff the address <b>addr</b> is in a country listed in the
 * case-insensitive list of country codes <b>cc_list</b>. */
static int
addr_is_in_cc_list(uint32_t addr, const smartlist_t *cc_list)
{
  country_t country;
  const char *name;
  tor_addr_t tar;

  if (!cc_list)
    return 0;
  /* XXXXipv6 */
  tor_addr_from_ipv4h(&tar, addr);
  country = geoip_get_country_by_addr(&tar);
  name = geoip_get_country_name(country);
  return smartlist_contains_string_case(cc_list, name);
}

/** Return 1 if <b>addr</b>:<b>port</b> is permitted to publish to our
 * directory, based on <b>authdir_reject_policy</b>. Else return 0.
 */
int
authdir_policy_permits_address(uint32_t addr, uint16_t port)
{
  if (! addr_policy_permits_address(addr, port, authdir_reject_policy))
    return 0;
  return !addr_is_in_cc_list(addr, get_options()->AuthDirRejectCCs);
}

/** Return 1 if <b>addr</b>:<b>port</b> is considered valid in our
 * directory, based on <b>authdir_invalid_policy</b>. Else return 0.
 */
int
authdir_policy_valid_address(uint32_t addr, uint16_t port)
{
  if (! addr_policy_permits_address(addr, port, authdir_invalid_policy))
    return 0;
  return !addr_is_in_cc_list(addr, get_options()->AuthDirInvalidCCs);
}

/** Return 1 if <b>addr</b>:<b>port</b> should be marked as a bad dir,
 * based on <b>authdir_baddir_policy</b>. Else return 0.
 */
int
authdir_policy_baddir_address(uint32_t addr, uint16_t port)
{
  if (! addr_policy_permits_address(addr, port, authdir_baddir_policy))
    return 1;
  return addr_is_in_cc_list(addr, get_options()->AuthDirBadDirCCs);
}

/** Return 1 if <b>addr</b>:<b>port</b> should be marked as a bad exit,
 * based on <b>authdir_badexit_policy</b>. Else return 0.
 */
int
authdir_policy_badexit_address(uint32_t addr, uint16_t port)
{
  if (! addr_policy_permits_address(addr, port, authdir_badexit_policy))
    return 1;
  return addr_is_in_cc_list(addr, get_options()->AuthDirBadExitCCs);
}

#define REJECT(arg) \
  STMT_BEGIN *msg = tor_strdup(arg); goto err; STMT_END

/** Config helper: If there's any problem with the policy configuration
 * options in <b>options</b>, return -1 and set <b>msg</b> to a newly
 * allocated description of the error. Else return 0. */
int
validate_addr_policies(const or_options_t *options, char **msg)
{
  /* XXXX Maybe merge this into parse_policies_from_options, to make sure
   * that the two can't go out of sync. */

  smartlist_t *addr_policy=NULL;
  *msg = NULL;

  if (policies_parse_exit_policy(options->ExitPolicy, &addr_policy,
                                 options->IPv6Exit,
                                 options->ExitPolicyRejectPrivate, 0,
                                 !options->BridgeRelay))
    REJECT("Error in ExitPolicy entry.");

  /* The rest of these calls *append* to addr_policy. So don't actually
   * use the results for anything other than checking if they parse! */
  if (parse_addr_policy(options->DirPolicy, &addr_policy, -1))
    REJECT("Error in DirPolicy entry.");
  if (parse_addr_policy(options->SocksPolicy, &addr_policy, -1))
    REJECT("Error in SocksPolicy entry.");
  if (parse_addr_policy(options->AuthDirReject, &addr_policy,
                        ADDR_POLICY_REJECT))
    REJECT("Error in AuthDirReject entry.");
  if (parse_addr_policy(options->AuthDirInvalid, &addr_policy,
                        ADDR_POLICY_REJECT))
    REJECT("Error in AuthDirInvalid entry.");
  if (parse_addr_policy(options->AuthDirBadDir, &addr_policy,
                        ADDR_POLICY_REJECT))
    REJECT("Error in AuthDirBadDir entry.");
  if (parse_addr_policy(options->AuthDirBadExit, &addr_policy,
                        ADDR_POLICY_REJECT))
    REJECT("Error in AuthDirBadExit entry.");

  if (parse_addr_policy(options->ReachableAddresses, &addr_policy,
                        ADDR_POLICY_ACCEPT))
    REJECT("Error in ReachableAddresses entry.");
  if (parse_addr_policy(options->ReachableORAddresses, &addr_policy,
                        ADDR_POLICY_ACCEPT))
    REJECT("Error in ReachableORAddresses entry.");
  if (parse_addr_policy(options->ReachableDirAddresses, &addr_policy,
                        ADDR_POLICY_ACCEPT))
    REJECT("Error in ReachableDirAddresses entry.");

 err:
  addr_policy_list_free(addr_policy);
  return *msg ? -1 : 0;
#undef REJECT
}

/** Parse <b>string</b> in the same way that the exit policy
 * is parsed, and put the processed version in *<b>policy</b>.
 * Ignore port specifiers.
 */
static int
load_policy_from_option(config_line_t *config, const char *option_name,
                        smartlist_t **policy,
                        int assume_action)
{
  int r;
  int killed_any_ports = 0;
  addr_policy_list_free(*policy);
  *policy = NULL;
  r = parse_addr_policy(config, policy, assume_action);
  if (r < 0) {
    return -1;
  }
  if (*policy) {
    SMARTLIST_FOREACH_BEGIN(*policy, addr_policy_t *, n) {
      /* ports aren't used in these. */
      if (n->prt_min > 1 || n->prt_max != 65535) {
        addr_policy_t newp, *c;
        memcpy(&newp, n, sizeof(newp));
        newp.prt_min = 1;
        newp.prt_max = 65535;
        newp.is_canonical = 0;
        c = addr_policy_get_canonical_entry(&newp);
        SMARTLIST_REPLACE_CURRENT(*policy, n, c);
        addr_policy_free(n);
        killed_any_ports = 1;
      }
    } SMARTLIST_FOREACH_END(n);
  }
  if (killed_any_ports) {
    log_warn(LD_CONFIG, "Ignoring ports in %s option.", option_name);
  }
  return 0;
}

/** Set all policies based on <b>options</b>, which should have been validated
 * first by validate_addr_policies. */
int
policies_parse_from_options(const or_options_t *options)
{
  int ret = 0;
  if (load_policy_from_option(options->SocksPolicy, "SocksPolicy",
                              &socks_policy, -1) < 0)
    ret = -1;
  if (load_policy_from_option(options->DirPolicy, "DirPolicy",
                              &dir_policy, -1) < 0)
    ret = -1;
  if (load_policy_from_option(options->AuthDirReject, "AuthDirReject",
                              &authdir_reject_policy, ADDR_POLICY_REJECT) < 0)
    ret = -1;
  if (load_policy_from_option(options->AuthDirInvalid, "AuthDirInvalid",
                              &authdir_invalid_policy, ADDR_POLICY_REJECT) < 0)
    ret = -1;
  if (load_policy_from_option(options->AuthDirBadDir, "AuthDirBadDir",
                              &authdir_baddir_policy, ADDR_POLICY_REJECT) < 0)
    ret = -1;
  if (load_policy_from_option(options->AuthDirBadExit, "AuthDirBadExit",
                              &authdir_badexit_policy, ADDR_POLICY_REJECT) < 0)
    ret = -1;
  if (parse_reachable_addresses() < 0)
    ret = -1;
  return ret;
}

/** Compare two provided address policy items, and return -1, 0, or 1
 * if the first is less than, equal to, or greater than the second. */
static int
cmp_single_addr_policy(addr_policy_t *a, addr_policy_t *b)
{
  int r;
  if ((r=((int)a->policy_type - (int)b->policy_type)))
    return r;
  if ((r=((int)a->is_private - (int)b->is_private)))
    return r;
  if ((r=tor_addr_compare(&a->addr, &b->addr, CMP_EXACT)))
    return r;
  if ((r=((int)a->maskbits - (int)b->maskbits)))
    return r;
  if ((r=((int)a->prt_min - (int)b->prt_min)))
    return r;
  if ((r=((int)a->prt_max - (int)b->prt_max)))
    return r;
  return 0;
}

/** Like cmp_single_addr_policy() above, but looks at the
 * whole set of policies in each case. */
int
cmp_addr_policies(smartlist_t *a, smartlist_t *b)
{
  int r, i;
  int len_a = a ? smartlist_len(a) : 0;
  int len_b = b ? smartlist_len(b) : 0;

  for (i = 0; i < len_a && i < len_b; ++i) {
    if ((r = cmp_single_addr_policy(smartlist_get(a, i), smartlist_get(b, i))))
      return r;
  }
  if (i == len_a && i == len_b)
    return 0;
  if (i < len_a)
    return -1;
  else
    return 1;
}

/** Node in hashtable used to store address policy entries. */
typedef struct policy_map_ent_t {
  HT_ENTRY(policy_map_ent_t) node;
  addr_policy_t *policy;
} policy_map_ent_t;

/* DOCDOC policy_root */
static HT_HEAD(policy_map, policy_map_ent_t) policy_root = HT_INITIALIZER();

/** Return true iff a and b are equal. */
static INLINE int
policy_eq(policy_map_ent_t *a, policy_map_ent_t *b)
{
  return cmp_single_addr_policy(a->policy, b->policy) == 0;
}

/** Return a hashcode for <b>ent</b> */
static unsigned int
policy_hash(const policy_map_ent_t *ent)
{
  const addr_policy_t *a = ent->policy;
  addr_policy_t aa;
  memset(&aa, 0, sizeof(aa));

  aa.prt_min = a->prt_min;
  aa.prt_max = a->prt_max;
  aa.maskbits = a->maskbits;
  aa.policy_type = a->policy_type;
  aa.is_private = a->is_private;

  if (a->is_private) {
    aa.is_private = 1;
  } else {
    tor_addr_copy_tight(&aa.addr, &a->addr);
  }

  return (unsigned) siphash24g(&aa, sizeof(aa));
}

HT_PROTOTYPE(policy_map, policy_map_ent_t, node, policy_hash,
             policy_eq)
HT_GENERATE(policy_map, policy_map_ent_t, node, policy_hash,
            policy_eq, 0.6, malloc, realloc, free)

/** Given a pointer to an addr_policy_t, return a copy of the pointer to the
 * "canonical" copy of that addr_policy_t; the canonical copy is a single
 * reference-counted object. */
addr_policy_t *
addr_policy_get_canonical_entry(addr_policy_t *e)
{
  policy_map_ent_t search, *found;
  if (e->is_canonical)
    return e;

  search.policy = e;
  found = HT_FIND(policy_map, &policy_root, &search);
  if (!found) {
    found = tor_malloc_zero(sizeof(policy_map_ent_t));
    found->policy = tor_memdup(e, sizeof(addr_policy_t));
    found->policy->is_canonical = 1;
    found->policy->refcnt = 0;
    HT_INSERT(policy_map, &policy_root, found);
  }

  tor_assert(!cmp_single_addr_policy(found->policy, e));
  ++found->policy->refcnt;
  return found->policy;
}

/** Helper for compare_tor_addr_to_addr_policy.  Implements the case where
 * addr and port are both known. */
static addr_policy_result_t
compare_known_tor_addr_to_addr_policy(const tor_addr_t *addr, uint16_t port,
                                      const smartlist_t *policy)
{
  /* We know the address and port, and we know the policy, so we can just
   * compute an exact match. */
  SMARTLIST_FOREACH_BEGIN(policy, addr_policy_t *, tmpe) {
    /* Address is known */
    if (!tor_addr_compare_masked(addr, &tmpe->addr, tmpe->maskbits,
                                 CMP_EXACT)) {
      if (port >= tmpe->prt_min && port <= tmpe->prt_max) {
        /* Exact match for the policy */
        return tmpe->policy_type == ADDR_POLICY_ACCEPT ?
          ADDR_POLICY_ACCEPTED : ADDR_POLICY_REJECTED;
      }
    }
  } SMARTLIST_FOREACH_END(tmpe);

  /* accept all by default. */
  return ADDR_POLICY_ACCEPTED;
}

/** Helper for compare_tor_addr_to_addr_policy.  Implements the case where
 * addr is known but port is not. */
static addr_policy_result_t
compare_known_tor_addr_to_addr_policy_noport(const tor_addr_t *addr,
                                             const smartlist_t *policy)
{
  /* We look to see if there's a definite match.  If so, we return that
     match's value, unless there's an intervening possible match that says
     something different. */
  int maybe_accept = 0, maybe_reject = 0;

  SMARTLIST_FOREACH_BEGIN(policy, addr_policy_t *, tmpe) {
    if (!tor_addr_compare_masked(addr, &tmpe->addr, tmpe->maskbits,
                                 CMP_EXACT)) {
      if (tmpe->prt_min <= 1 && tmpe->prt_max >= 65535) {
        /* Definitely matches, since it covers all ports. */
        if (tmpe->policy_type == ADDR_POLICY_ACCEPT) {
          /* If we already hit a clause that might trigger a 'reject', than we
           * can't be sure of this certain 'accept'.*/
          return maybe_reject ? ADDR_POLICY_PROBABLY_ACCEPTED :
            ADDR_POLICY_ACCEPTED;
        } else {
          return maybe_accept ? ADDR_POLICY_PROBABLY_REJECTED :
            ADDR_POLICY_REJECTED;
        }
      } else {
        /* Might match. */
        if (tmpe->policy_type == ADDR_POLICY_REJECT)
          maybe_reject = 1;
        else
          maybe_accept = 1;
      }
    }
  } SMARTLIST_FOREACH_END(tmpe);

  /* accept all by default. */
  return maybe_reject ? ADDR_POLICY_PROBABLY_ACCEPTED : ADDR_POLICY_ACCEPTED;
}

/** Helper for compare_tor_addr_to_addr_policy.  Implements the case where
 * port is known but address is not. */
static addr_policy_result_t
compare_unknown_tor_addr_to_addr_policy(uint16_t port,
                                        const smartlist_t *policy)
{
  /* We look to see if there's a definite match.  If so, we return that
     match's value, unless there's an intervening possible match that says
     something different. */
  int maybe_accept = 0, maybe_reject = 0;

  SMARTLIST_FOREACH_BEGIN(policy, addr_policy_t *, tmpe) {
    if (tmpe->prt_min <= port && port <= tmpe->prt_max) {
      if (tmpe->maskbits == 0) {
        /* Definitely matches, since it covers all addresses. */
        if (tmpe->policy_type == ADDR_POLICY_ACCEPT) {
          /* If we already hit a clause that might trigger a 'reject', than we
           * can't be sure of this certain 'accept'.*/
          return maybe_reject ? ADDR_POLICY_PROBABLY_ACCEPTED :
            ADDR_POLICY_ACCEPTED;
        } else {
          return maybe_accept ? ADDR_POLICY_PROBABLY_REJECTED :
            ADDR_POLICY_REJECTED;
        }
      } else {
        /* Might match. */
        if (tmpe->policy_type == ADDR_POLICY_REJECT)
          maybe_reject = 1;
        else
          maybe_accept = 1;
      }
    }
  } SMARTLIST_FOREACH_END(tmpe);

  /* accept all by default. */
  return maybe_reject ? ADDR_POLICY_PROBABLY_ACCEPTED : ADDR_POLICY_ACCEPTED;
}

/** Decide whether a given addr:port is definitely accepted,
 * definitely rejected, probably accepted, or probably rejected by a
 * given policy.  If <b>addr</b> is 0, we don't know the IP of the
 * target address.  If <b>port</b> is 0, we don't know the port of the
 * target address.  (At least one of <b>addr</b> and <b>port</b> must be
 * provided.  If you want to know whether a policy would definitely reject
 * an unknown address:port, use policy_is_reject_star().)
 *
 * We could do better by assuming that some ranges never match typical
 * addresses (127.0.0.1, and so on).  But we'll try this for now.
 */
addr_policy_result_t
compare_tor_addr_to_addr_policy(const tor_addr_t *addr, uint16_t port,
                                const smartlist_t *policy)
{
  if (!policy) {
    /* no policy? accept all. */
    return ADDR_POLICY_ACCEPTED;
  } else if (addr == NULL || tor_addr_is_null(addr)) {
    if (port == 0) {
      log_info(LD_BUG, "Rejecting null address with 0 port (family %d)",
               addr ? tor_addr_family(addr) : -1);
      return ADDR_POLICY_REJECTED;
    }
    return compare_unknown_tor_addr_to_addr_policy(port, policy);
  } else if (port == 0) {
    return compare_known_tor_addr_to_addr_policy_noport(addr, policy);
  } else {
    return compare_known_tor_addr_to_addr_policy(addr, port, policy);
  }
}

/** Return true iff the address policy <b>a</b> covers every case that
 * would be covered by <b>b</b>, so that a,b is redundant. */
static int
addr_policy_covers(addr_policy_t *a, addr_policy_t *b)
{
  if (tor_addr_family(&a->addr) != tor_addr_family(&b->addr)) {
    /* You can't cover a different family. */
    return 0;
  }
  /* We can ignore accept/reject, since "accept *:80, reject *:80" reduces
   * to "accept *:80". */
  if (a->maskbits > b->maskbits) {
    /* a has more fixed bits than b; it can't possibly cover b. */
    return 0;
  }
  if (tor_addr_compare_masked(&a->addr, &b->addr, a->maskbits, CMP_EXACT)) {
    /* There's a fixed bit in a that's set differently in b. */
    return 0;
  }
  return (a->prt_min <= b->prt_min && a->prt_max >= b->prt_max);
}

/** Return true iff the address policies <b>a</b> and <b>b</b> intersect,
 * that is, there exists an address/port that is covered by <b>a</b> that
 * is also covered by <b>b</b>.
 */
static int
addr_policy_intersects(addr_policy_t *a, addr_policy_t *b)
{
  maskbits_t minbits;
  /* All the bits we care about are those that are set in both
   * netmasks.  If they are equal in a and b's networkaddresses
   * then the networks intersect.  If there is a difference,
   * then they do not. */
  if (a->maskbits < b->maskbits)
    minbits = a->maskbits;
  else
    minbits = b->maskbits;
  if (tor_addr_compare_masked(&a->addr, &b->addr, minbits, CMP_EXACT))
    return 0;
  if (a->prt_max < b->prt_min || b->prt_max < a->prt_min)
    return 0;
  return 1;
}

/** Add the exit policy described by <b>more</b> to <b>policy</b>.
 */
static void
append_exit_policy_string(smartlist_t **policy, const char *more)
{
  config_line_t tmp;

  tmp.key = NULL;
  tmp.value = (char*) more;
  tmp.next = NULL;
  if (parse_addr_policy(&tmp, policy, -1)<0) {
    log_warn(LD_BUG, "Unable to parse internally generated policy %s",more);
  }
}

/** Add "reject <b>addr</b>:*" to <b>dest</b>, creating the list as needed. */
void
addr_policy_append_reject_addr(smartlist_t **dest, const tor_addr_t *addr)
{
  addr_policy_t p, *add;
  memset(&p, 0, sizeof(p));
  p.policy_type = ADDR_POLICY_REJECT;
  p.maskbits = tor_addr_family(addr) == AF_INET6 ? 128 : 32;
  tor_addr_copy(&p.addr, addr);
  p.prt_min = 1;
  p.prt_max = 65535;

  add = addr_policy_get_canonical_entry(&p);
  if (!*dest)
    *dest = smartlist_new();
  smartlist_add(*dest, add);
}

/** Detect and excise "dead code" from the policy *<b>dest</b>. */
static void
exit_policy_remove_redundancies(smartlist_t *dest)
{
  addr_policy_t *ap, *tmp;
  int i, j;

  /* Step one: kill every ipv4 thing after *4:*, every IPv6 thing after *6:*
   */
  {
    int kill_v4=0, kill_v6=0;
    for (i = 0; i < smartlist_len(dest); ++i) {
      sa_family_t family;
      ap = smartlist_get(dest, i);
      family = tor_addr_family(&ap->addr);
      if ((family == AF_INET && kill_v4) ||
          (family == AF_INET6 && kill_v6)) {
        smartlist_del_keeporder(dest, i--);
        addr_policy_free(ap);
        continue;
      }

      if (ap->maskbits == 0 && ap->prt_min <= 1 && ap->prt_max >= 65535) {
        /* This is a catch-all line -- later lines are unreachable. */
        if (family == AF_INET) {
          kill_v4 = 1;
        } else if (family == AF_INET6) {
          kill_v6 = 1;
        }
      }
    }
  }

  /* Step two: for every entry, see if there's a redundant entry
   * later on, and remove it. */
  for (i = 0; i < smartlist_len(dest)-1; ++i) {
    ap = smartlist_get(dest, i);
    for (j = i+1; j < smartlist_len(dest); ++j) {
      tmp = smartlist_get(dest, j);
      tor_assert(j > i);
      if (addr_policy_covers(ap, tmp)) {
        char p1[POLICY_BUF_LEN], p2[POLICY_BUF_LEN];
        policy_write_item(p1, sizeof(p1), tmp, 0);
        policy_write_item(p2, sizeof(p2), ap, 0);
        log_debug(LD_CONFIG, "Removing exit policy %s (%d).  It is made "
            "redundant by %s (%d).", p1, j, p2, i);
        smartlist_del_keeporder(dest, j--);
        addr_policy_free(tmp);
      }
    }
  }

  /* Step three: for every entry A, see if there's an entry B making this one
   * redundant later on.  This is the case if A and B are of the same type
   * (accept/reject), A is a subset of B, and there is no other entry of
   * different type in between those two that intersects with A.
   *
   * Anybody want to double-check the logic here? XXX
   */
  for (i = 0; i < smartlist_len(dest)-1; ++i) {
    ap = smartlist_get(dest, i);
    for (j = i+1; j < smartlist_len(dest); ++j) {
      // tor_assert(j > i); // j starts out at i+1; j only increases; i only
      //                    // decreases.
      tmp = smartlist_get(dest, j);
      if (ap->policy_type != tmp->policy_type) {
        if (addr_policy_intersects(ap, tmp))
          break;
      } else { /* policy_types are equal. */
        if (addr_policy_covers(tmp, ap)) {
          char p1[POLICY_BUF_LEN], p2[POLICY_BUF_LEN];
          policy_write_item(p1, sizeof(p1), ap, 0);
          policy_write_item(p2, sizeof(p2), tmp, 0);
          log_debug(LD_CONFIG, "Removing exit policy %s.  It is already "
              "covered by %s.", p1, p2);
          smartlist_del_keeporder(dest, i--);
          addr_policy_free(ap);
          break;
        }
      }
    }
  }
}

#define DEFAULT_EXIT_POLICY                                         \
  "reject *:25,reject *:119,reject *:135-139,reject *:445,"         \
  "reject *:563,reject *:1214,reject *:4661-4666,"                  \
  "reject *:6346-6429,reject *:6699,reject *:6881-6999,accept *:*"

/** Parse the exit policy <b>cfg</b> into the linked list *<b>dest</b>. If
 * cfg doesn't end in an absolute accept or reject and if
 * <b>add_default_policy</b> is true, add the default exit
 * policy afterwards. If <b>rejectprivate</b> is true, prepend
 * "reject private:*" to the policy. Return -1 if we can't parse cfg,
 * else return 0.
 *
 * This function is used to parse the exit policy from our torrc. For
 * the functions used to parse the exit policy from a router descriptor,
 * see router_add_exit_policy.
 */
int
policies_parse_exit_policy(config_line_t *cfg, smartlist_t **dest,
                           int ipv6_exit,
                           int rejectprivate, uint32_t local_address,
                           int add_default_policy)
{
  if (!ipv6_exit) {
    append_exit_policy_string(dest, "reject *6:*");
  }
  if (rejectprivate) {
    append_exit_policy_string(dest, "reject private:*");
    if (local_address) {
      char buf[POLICY_BUF_LEN];
      tor_snprintf(buf, sizeof(buf), "reject %s:*", fmt_addr32(local_address));
      append_exit_policy_string(dest, buf);
    }
  }
  if (parse_addr_policy(cfg, dest, -1))
    return -1;
  if (add_default_policy) {
    append_exit_policy_string(dest, DEFAULT_EXIT_POLICY);
  } else {
    append_exit_policy_string(dest, "reject *4:*");
    append_exit_policy_string(dest, "reject *6:*");
  }
  exit_policy_remove_redundancies(*dest);

  return 0;
}

/** Add "reject *:*" to the end of the policy in *<b>dest</b>, allocating
 * *<b>dest</b> as needed. */
void
policies_exit_policy_append_reject_star(smartlist_t **dest)
{
  append_exit_policy_string(dest, "reject *4:*");
  append_exit_policy_string(dest, "reject *6:*");
}

/** Replace the exit policy of <b>node</b> with reject *:* */
void
policies_set_node_exitpolicy_to_reject_all(node_t *node)
{
  node->rejects_all = 1;
}

/** Return 1 if there is at least one /8 subnet in <b>policy</b> that
 * allows exiting to <b>port</b>.  Otherwise, return 0. */
static int
exit_policy_is_general_exit_helper(smartlist_t *policy, int port)
{
  uint32_t mask, ip, i;
  /* Is this /8 rejected (1), or undecided (0)? */
  char subnet_status[256];

  memset(subnet_status, 0, sizeof(subnet_status));
  SMARTLIST_FOREACH_BEGIN(policy, addr_policy_t *, p) {
    if (tor_addr_family(&p->addr) != AF_INET)
      continue; /* IPv4 only for now */
    if (p->prt_min > port || p->prt_max < port)
      continue; /* Doesn't cover our port. */
    mask = 0;
    tor_assert(p->maskbits <= 32);

    if (p->maskbits)
      mask = UINT32_MAX<<(32-p->maskbits);
    ip = tor_addr_to_ipv4h(&p->addr);

    /* Calculate the first and last subnet that this exit policy touches
     * and set it as loop boundaries. */
    for (i = ((mask & ip)>>24); i <= (~((mask & ip) ^ mask)>>24); ++i) {
      tor_addr_t addr;
      if (subnet_status[i] != 0)
        continue; /* We already reject some part of this /8 */
      tor_addr_from_ipv4h(&addr, i<<24);
      if (tor_addr_is_internal(&addr, 0))
        continue; /* Local or non-routable addresses */
      if (p->policy_type == ADDR_POLICY_ACCEPT) {
        if (p->maskbits > 8)
          continue; /* Narrower than a /8. */
        /* We found an allowed subnet of at least size /8. Done
         * for this port! */
        return 1;
      } else if (p->policy_type == ADDR_POLICY_REJECT) {
        subnet_status[i] = 1;
      }
    }
  } SMARTLIST_FOREACH_END(p);
  return 0;
}

/** Return true iff <b>ri</b> is "useful as an exit node", meaning
 * it allows exit to at least one /8 address space for at least
 * two of ports 80, 443, and 6667. */
int
exit_policy_is_general_exit(smartlist_t *policy)
{
  static const int ports[] = { 80, 443, 6667 };
  int n_allowed = 0;
  int i;
  if (!policy) /*XXXX disallow NULL policies? */
    return 0;

  for (i = 0; i < 3; ++i) {
    n_allowed += exit_policy_is_general_exit_helper(policy, ports[i]);
  }
  return n_allowed >= 2;
}

/** Return false if <b>policy</b> might permit access to some addr:port;
 * otherwise if we are certain it rejects everything, return true. */
int
policy_is_reject_star(const smartlist_t *policy, sa_family_t family)
{
  if (!policy) /*XXXX disallow NULL policies? */
    return 1;
  SMARTLIST_FOREACH_BEGIN(policy, addr_policy_t *, p) {
    if (p->policy_type == ADDR_POLICY_ACCEPT &&
        (tor_addr_family(&p->addr) == family ||
         tor_addr_family(&p->addr) == AF_UNSPEC)) {
      return 0;
    } else if (p->policy_type == ADDR_POLICY_REJECT &&
               p->prt_min <= 1 && p->prt_max == 65535 &&
               p->maskbits == 0 &&
               (tor_addr_family(&p->addr) == family ||
                tor_addr_family(&p->addr) == AF_UNSPEC)) {
      return 1;
    }
  } SMARTLIST_FOREACH_END(p);
  return 1;
}

/** Write a single address policy to the buf_len byte buffer at buf.  Return
 * the number of characters written, or -1 on failure. */
int
policy_write_item(char *buf, size_t buflen, addr_policy_t *policy,
                  int format_for_desc)
{
  size_t written = 0;
  char addrbuf[TOR_ADDR_BUF_LEN];
  const char *addrpart;
  int result;
  const int is_accept = policy->policy_type == ADDR_POLICY_ACCEPT;
  const sa_family_t family = tor_addr_family(&policy->addr);
  const int is_ip6 = (family == AF_INET6);

  tor_addr_to_str(addrbuf, &policy->addr, sizeof(addrbuf), 1);

  /* write accept/reject 1.2.3.4 */
  if (policy->is_private) {
    addrpart = "private";
  } else if (policy->maskbits == 0) {
    if (format_for_desc)
      addrpart = "*";
    else if (family == AF_INET6)
      addrpart = "*6";
    else if (family == AF_INET)
      addrpart = "*4";
    else
      addrpart = "*";
  } else {
    addrpart = addrbuf;
  }

  result = tor_snprintf(buf, buflen, "%s%s %s",
                        is_accept ? "accept" : "reject",
                        (is_ip6&&format_for_desc)?"6":"",
                        addrpart);
  if (result < 0)
    return -1;
  written += strlen(buf);
  /* If the maskbits is 32 we don't need to give it.  If the mask is 0,
   * we already wrote "*". */
  if (policy->maskbits < 32 && policy->maskbits > 0) {
    if (tor_snprintf(buf+written, buflen-written, "/%d", policy->maskbits)<0)
      return -1;
    written += strlen(buf+written);
  }
  if (policy->prt_min <= 1 && policy->prt_max == 65535) {
    /* There is no port set; write ":*" */
    if (written+4 > buflen)
      return -1;
    strlcat(buf+written, ":*", buflen-written);
    written += 2;
  } else if (policy->prt_min == policy->prt_max) {
    /* There is only one port; write ":80". */
    result = tor_snprintf(buf+written, buflen-written, ":%d", policy->prt_min);
    if (result<0)
      return -1;
    written += result;
  } else {
    /* There is a range of ports; write ":79-80". */
    result = tor_snprintf(buf+written, buflen-written, ":%d-%d",
                          policy->prt_min, policy->prt_max);
    if (result<0)
      return -1;
    written += result;
  }
  if (written < buflen)
    buf[written] = '\0';
  else
    return -1;

  return (int)written;
}

/** Create a new exit policy summary, initially only with a single
 *  port 1-64k item */
/* XXXX This entire thing will do most stuff in O(N^2), or worse.  Use an
 *      RB-tree if that turns out to matter. */
static smartlist_t *
policy_summary_create(void)
{
  smartlist_t *summary;
  policy_summary_item_t* item;

  item = tor_malloc_zero(sizeof(policy_summary_item_t));
  item->prt_min = 1;
  item->prt_max = 65535;
  item->reject_count = 0;
  item->accepted = 0;

  summary = smartlist_new();
  smartlist_add(summary, item);

  return summary;
}

/** Split the summary item in <b>item</b> at the port <b>new_starts</b>.
 * The current item is changed to end at new-starts - 1, the new item
 * copies reject_count and accepted from the old item,
 * starts at new_starts and ends at the port where the original item
 * previously ended.
 */
static policy_summary_item_t*
policy_summary_item_split(policy_summary_item_t* old, uint16_t new_starts)
{
  policy_summary_item_t* new;

  new = tor_malloc_zero(sizeof(policy_summary_item_t));
  new->prt_min = new_starts;
  new->prt_max = old->prt_max;
  new->reject_count = old->reject_count;
  new->accepted = old->accepted;

  old->prt_max = new_starts-1;

  tor_assert(old->prt_min <= old->prt_max);
  tor_assert(new->prt_min <= new->prt_max);
  return new;
}

/* XXXX Nick says I'm going to hell for this.  If he feels charitably towards
 * my immortal soul, he can clean it up himself. */
#define AT(x) ((policy_summary_item_t*)smartlist_get(summary, x))

#define REJECT_CUTOFF_COUNT (1<<25)
/** Split an exit policy summary so that prt_min and prt_max
 * fall at exactly the start and end of an item respectively.
 */
static int
policy_summary_split(smartlist_t *summary,
                     uint16_t prt_min, uint16_t prt_max)
{
  int start_at_index;

  int i = 0;

  while (AT(i)->prt_max < prt_min)
    i++;
  if (AT(i)->prt_min != prt_min) {
    policy_summary_item_t* new_item;
    new_item = policy_summary_item_split(AT(i), prt_min);
    smartlist_insert(summary, i+1, new_item);
    i++;
  }
  start_at_index = i;

  while (AT(i)->prt_max < prt_max)
    i++;
  if (AT(i)->prt_max != prt_max) {
    policy_summary_item_t* new_item;
    new_item = policy_summary_item_split(AT(i), prt_max+1);
    smartlist_insert(summary, i+1, new_item);
  }

  return start_at_index;
}

/** Mark port ranges as accepted if they are below the reject_count */
static void
policy_summary_accept(smartlist_t *summary,
                      uint16_t prt_min, uint16_t prt_max)
{
  int i = policy_summary_split(summary, prt_min, prt_max);
  while (i < smartlist_len(summary) &&
         AT(i)->prt_max <= prt_max) {
    if (!AT(i)->accepted &&
        AT(i)->reject_count <= REJECT_CUTOFF_COUNT)
      AT(i)->accepted = 1;
    i++;
  }
  tor_assert(i < smartlist_len(summary) || prt_max==65535);
}

/** Count the number of addresses in a network with prefixlen maskbits
 * against the given portrange. */
static void
policy_summary_reject(smartlist_t *summary,
                      maskbits_t maskbits,
                      uint16_t prt_min, uint16_t prt_max)
{
  int i = policy_summary_split(summary, prt_min, prt_max);
  /* XXX: ipv4 specific */
  uint64_t count = (U64_LITERAL(1) << (32-maskbits));
  while (i < smartlist_len(summary) &&
         AT(i)->prt_max <= prt_max) {
    AT(i)->reject_count += count;
    i++;
  }
  tor_assert(i < smartlist_len(summary) || prt_max==65535);
}

/** Add a single exit policy item to our summary:
 *  If it is an accept ignore it unless it is for all IP addresses
 *  ("*"), i.e. it's prefixlen/maskbits is 0, else call
 *  policy_summary_accept().
 *  If it's a reject ignore it if it is about one of the private
 *  networks, else call policy_summary_reject().
 */
static void
policy_summary_add_item(smartlist_t *summary, addr_policy_t *p)
{
  if (p->policy_type == ADDR_POLICY_ACCEPT) {
    if (p->maskbits == 0) {
      policy_summary_accept(summary, p->prt_min, p->prt_max);
    }
  } else if (p->policy_type == ADDR_POLICY_REJECT) {

     int is_private = 0;
     int i;
     for (i = 0; private_nets[i]; ++i) {
       tor_addr_t addr;
       maskbits_t maskbits;
       if (tor_addr_parse_mask_ports(private_nets[i], 0, &addr,
                                     &maskbits, NULL, NULL)<0) {
         tor_assert(0);
       }
       if (tor_addr_compare(&p->addr, &addr, CMP_EXACT) == 0 &&
           p->maskbits == maskbits) {
         is_private = 1;
         break;
       }
     }

     if (!is_private) {
       policy_summary_reject(summary, p->maskbits, p->prt_min, p->prt_max);
     }
  } else
    tor_assert(0);
}

/** Create a string representing a summary for an exit policy.
 * The summary will either be an "accept" plus a comma-separated list of port
 * ranges or a "reject" plus port-ranges, depending on which is shorter.
 *
 * If no exits are allowed at all then NULL is returned, if no ports
 * are blocked instead of "reject " we return "accept 1-65535" (this
 * is an exception to the shorter-representation-wins rule).
 */
char *
policy_summarize(smartlist_t *policy, sa_family_t family)
{
  smartlist_t *summary = policy_summary_create();
  smartlist_t *accepts, *rejects;
  int i, last, start_prt;
  size_t accepts_len, rejects_len;
  char *accepts_str = NULL, *rejects_str = NULL, *shorter_str, *result;
  const char *prefix;

  tor_assert(policy);

  /* Create the summary list */
  SMARTLIST_FOREACH_BEGIN(policy, addr_policy_t *, p) {
    sa_family_t f = tor_addr_family(&p->addr);
    if (f != AF_INET && f != AF_INET6) {
      log_warn(LD_BUG, "Weird family when summarizing address policy");
    }
    if (f != family)
      continue;
    /* XXXX-ipv6 More family work is needed */
    policy_summary_add_item(summary, p);
  } SMARTLIST_FOREACH_END(p);

  /* Now create two lists of strings, one for accepted and one
   * for rejected ports.  We take care to merge ranges so that
   * we avoid getting stuff like "1-4,5-9,10", instead we want
   * "1-10"
   */
  i = 0;
  start_prt = 1;
  accepts = smartlist_new();
  rejects = smartlist_new();
  while (1) {
    last = i == smartlist_len(summary)-1;
    if (last ||
        AT(i)->accepted != AT(i+1)->accepted) {
      char buf[POLICY_BUF_LEN];

      if (start_prt == AT(i)->prt_max)
        tor_snprintf(buf, sizeof(buf), "%d", start_prt);
      else
        tor_snprintf(buf, sizeof(buf), "%d-%d", start_prt, AT(i)->prt_max);

      if (AT(i)->accepted)
        smartlist_add(accepts, tor_strdup(buf));
      else
        smartlist_add(rejects, tor_strdup(buf));

      if (last)
        break;

      start_prt = AT(i+1)->prt_min;
    };
    i++;
  };

  /* Figure out which of the two stringlists will be shorter and use
   * that to build the result
   */
  if (smartlist_len(accepts) == 0) { /* no exits at all */
    result = tor_strdup("reject 1-65535");
    goto cleanup;
  }
  if (smartlist_len(rejects) == 0) { /* no rejects at all */
    result = tor_strdup("accept 1-65535");
    goto cleanup;
  }

  accepts_str = smartlist_join_strings(accepts, ",", 0, &accepts_len);
  rejects_str = smartlist_join_strings(rejects, ",", 0, &rejects_len);

  if (rejects_len > MAX_EXITPOLICY_SUMMARY_LEN-strlen("reject")-1 &&
      accepts_len > MAX_EXITPOLICY_SUMMARY_LEN-strlen("accept")-1) {
    char *c;
    shorter_str = accepts_str;
    prefix = "accept";

    c = shorter_str + (MAX_EXITPOLICY_SUMMARY_LEN-strlen(prefix)-1);
    while (*c != ',' && c >= shorter_str)
      c--;
    tor_assert(c >= shorter_str);
    tor_assert(*c == ',');
    *c = '\0';

  } else if (rejects_len < accepts_len) {
    shorter_str = rejects_str;
    prefix = "reject";
  } else {
    shorter_str = accepts_str;
    prefix = "accept";
  }

  tor_asprintf(&result, "%s %s", prefix, shorter_str);

 cleanup:
  /* cleanup */
  SMARTLIST_FOREACH(summary, policy_summary_item_t *, s, tor_free(s));
  smartlist_free(summary);

  tor_free(accepts_str);
  SMARTLIST_FOREACH(accepts, char *, s, tor_free(s));
  smartlist_free(accepts);

  tor_free(rejects_str);
  SMARTLIST_FOREACH(rejects, char *, s, tor_free(s));
  smartlist_free(rejects);

  return result;
}

/** Convert a summarized policy string into a short_policy_t.  Return NULL
 * if the string is not well-formed. */
short_policy_t *
parse_short_policy(const char *summary)
{
  const char *orig_summary = summary;
  short_policy_t *result;
  int is_accept;
  int n_entries;
  short_policy_entry_t entries[MAX_EXITPOLICY_SUMMARY_LEN]; /* overkill */
  const char *next;

  if (!strcmpstart(summary, "accept ")) {
    is_accept = 1;
    summary += strlen("accept ");
  } else if (!strcmpstart(summary, "reject ")) {
    is_accept = 0;
    summary += strlen("reject ");
  } else {
    log_fn(LOG_PROTOCOL_WARN, LD_DIR, "Unrecognized policy summary keyword");
    return NULL;
  }

  n_entries = 0;
  for ( ; *summary; summary = next) {
    const char *comma = strchr(summary, ',');
    unsigned low, high;
    char dummy;
    char ent_buf[32];
    size_t len;

    next = comma ? comma+1 : strchr(summary, '\0');
    len = comma ? (size_t)(comma - summary) : strlen(summary);

    if (n_entries == MAX_EXITPOLICY_SUMMARY_LEN) {
      log_fn(LOG_PROTOCOL_WARN, LD_DIR, "Impossibly long policy summary %s",
             escaped(orig_summary));
      return NULL;
    }

    if (! TOR_ISDIGIT(*summary) || len > (sizeof(ent_buf)-1)) {
      /* unrecognized entry format. skip it. */
      continue;
    }
    if (len < 1) {
      /* empty; skip it. */
      /* XXX This happens to be unreachable, since if len==0, then *summary is
       * ',' or '\0', and the TOR_ISDIGIT test above would have failed. */
      continue;
    }

    memcpy(ent_buf, summary, len);
    ent_buf[len] = '\0';

    if (tor_sscanf(ent_buf, "%u-%u%c", &low, &high, &dummy) == 2) {
      if (low<1 || low>65535 || high<1 || high>65535 || low>high) {
        log_fn(LOG_PROTOCOL_WARN, LD_DIR,
               "Found bad entry in policy summary %s", escaped(orig_summary));
        return NULL;
      }
    } else if (tor_sscanf(ent_buf, "%u%c", &low, &dummy) == 1) {
      if (low<1 || low>65535) {
        log_fn(LOG_PROTOCOL_WARN, LD_DIR,
               "Found bad entry in policy summary %s", escaped(orig_summary));
        return NULL;
      }
      high = low;
    } else {
      log_fn(LOG_PROTOCOL_WARN, LD_DIR,"Found bad entry in policy summary %s",
             escaped(orig_summary));
      return NULL;
    }

    entries[n_entries].min_port = low;
    entries[n_entries].max_port = high;
    n_entries++;
  }

  if (n_entries == 0) {
    log_fn(LOG_PROTOCOL_WARN, LD_DIR,
           "Found no port-range entries in summary %s", escaped(orig_summary));
    return NULL;
  }

  {
    size_t size = STRUCT_OFFSET(short_policy_t, entries) +
      sizeof(short_policy_entry_t)*(n_entries);
    result = tor_malloc_zero(size);

    tor_assert( (char*)&result->entries[n_entries-1] < ((char*)result)+size);
  }

  result->is_accept = is_accept;
  result->n_entries = n_entries;
  memcpy(result->entries, entries, sizeof(short_policy_entry_t)*n_entries);
  return result;
}

/** Write <b>policy</b> back out into a string. Used only for unit tests
 * currently. */
char *
write_short_policy(const short_policy_t *policy)
{
  int i;
  char *answer;
  smartlist_t *sl = smartlist_new();

  smartlist_add_asprintf(sl, "%s", policy->is_accept ? "accept " : "reject ");

  for (i=0; i < policy->n_entries; i++) {
    const short_policy_entry_t *e = &policy->entries[i];
    if (e->min_port == e->max_port) {
      smartlist_add_asprintf(sl, "%d", e->min_port);
    } else {
      smartlist_add_asprintf(sl, "%d-%d", e->min_port, e->max_port);
    }
    if (i < policy->n_entries-1)
      smartlist_add(sl, tor_strdup(","));
  }
  answer = smartlist_join_strings(sl, "", 0, NULL);
  SMARTLIST_FOREACH(sl, char *, a, tor_free(a));
  smartlist_free(sl);
  return answer;
}

/** Release all storage held in <b>policy</b>. */
void
short_policy_free(short_policy_t *policy)
{
  tor_free(policy);
}

/** See whether the <b>addr</b>:<b>port</b> address is likely to be accepted
 * or rejected by the summarized policy <b>policy</b>.  Return values are as
 * for compare_tor_addr_to_addr_policy.  Unlike the regular addr_policy
 * functions, requires the <b>port</b> be specified. */
addr_policy_result_t
compare_tor_addr_to_short_policy(const tor_addr_t *addr, uint16_t port,
                                 const short_policy_t *policy)
{
  int i;
  int found_match = 0;
  int accept;

  tor_assert(port != 0);

  if (addr && tor_addr_is_null(addr))
    addr = NULL; /* Unspec means 'no address at all,' in this context. */

  if (addr && get_options()->ClientRejectInternalAddresses &&
      (tor_addr_is_internal(addr, 0) || tor_addr_is_loopback(addr)))
    return ADDR_POLICY_REJECTED;

  for (i=0; i < policy->n_entries; ++i) {
    const short_policy_entry_t *e = &policy->entries[i];
    if (e->min_port <= port && port <= e->max_port) {
      found_match = 1;
      break;
    }
  }

  if (found_match)
    accept = policy->is_accept;
  else
    accept = ! policy->is_accept;

  /* ???? are these right? -NM */
  /* We should be sure not to return ADDR_POLICY_ACCEPTED in the accept
   * case here, because it would cause clients to believe that the node
   * allows exit enclaving. Trying it anyway would open up a cool attack
   * where the node refuses due to exitpolicy, the client reacts in
   * surprise by rewriting the node's exitpolicy to reject *:*, and then
   * a bad guy targets users by causing them to attempt such connections
   * to 98% of the exits.
   *
   * Once microdescriptors can handle addresses in special cases (e.g. if
   * we ever solve ticket 1774), we can provide certainty here. -RD */
  if (accept)
    return ADDR_POLICY_PROBABLY_ACCEPTED;
  else
    return ADDR_POLICY_REJECTED;
}

/** Return true iff <b>policy</b> seems reject all ports */
int
short_policy_is_reject_star(const short_policy_t *policy)
{
  /* This doesn't need to be as much on the lookout as policy_is_reject_star,
   * since policy summaries are from the consensus or from consensus
   * microdescs.
   */
  tor_assert(policy);
  /* Check for an exact match of "reject 1-65535". */
  return (policy->is_accept == 0 && policy->n_entries == 1 &&
          policy->entries[0].min_port == 1 &&
          policy->entries[0].max_port == 65535);
}

/** Decide whether addr:port is probably or definitely accepted or rejected by
 * <b>node</b>.  See compare_tor_addr_to_addr_policy for details on addr/port
 * interpretation. */
addr_policy_result_t
compare_tor_addr_to_node_policy(const tor_addr_t *addr, uint16_t port,
                                const node_t *node)
{
  if (node->rejects_all)
    return ADDR_POLICY_REJECTED;

  if (addr && tor_addr_family(addr) == AF_INET6) {
    const short_policy_t *p = NULL;
    if (node->ri)
      p = node->ri->ipv6_exit_policy;
    else if (node->md)
      p = node->md->ipv6_exit_policy;
    if (p)
      return compare_tor_addr_to_short_policy(addr, port, p);
    else
      return ADDR_POLICY_REJECTED;
  }

  if (node->ri) {
    return compare_tor_addr_to_addr_policy(addr, port, node->ri->exit_policy);
  } else if (node->md) {
    if (node->md->exit_policy == NULL)
      return ADDR_POLICY_REJECTED;
    else
      return compare_tor_addr_to_short_policy(addr, port,
                                              node->md->exit_policy);
  } else {
    return ADDR_POLICY_PROBABLY_REJECTED;
  }
}

/** Implementation for GETINFO control command: knows the answer for questions
 * about "exit-policy/..." */
int
getinfo_helper_policies(control_connection_t *conn,
                        const char *question, char **answer,
                        const char **errmsg)
{
  (void) conn;
  (void) errmsg;
  if (!strcmp(question, "exit-policy/default")) {
    *answer = tor_strdup(DEFAULT_EXIT_POLICY);
  } else if (!strcmpstart(question, "exit-policy/")) {
    const routerinfo_t *me = router_get_my_routerinfo();

    int include_ipv4 = 0;
    int include_ipv6 = 0;

    if (!strcmp(question, "exit-policy/ipv4")) {
      include_ipv4 = 1;
    } else if (!strcmp(question, "exit-policy/ipv6")) {
      include_ipv6 = 1;
    } else if (!strcmp(question, "exit-policy/full")) {
      include_ipv4 = include_ipv6 = 1;
    } else {
      return 0; /* No such key. */
    }

    if (!me) {
      *errmsg = "router_get_my_routerinfo returned NULL";
      return -1;
    }

    *answer = router_dump_exit_policy_to_string(me,include_ipv4,include_ipv6);
  }
  return 0;
}

/** Release all storage held by <b>p</b>. */
void
addr_policy_list_free(smartlist_t *lst)
{
  if (!lst)
    return;
  SMARTLIST_FOREACH(lst, addr_policy_t *, policy, addr_policy_free(policy));
  smartlist_free(lst);
}

/** Release all storage held by <b>p</b>. */
void
addr_policy_free(addr_policy_t *p)
{
  if (!p)
    return;

  if (--p->refcnt <= 0) {
    if (p->is_canonical) {
      policy_map_ent_t search, *found;
      search.policy = p;
      found = HT_REMOVE(policy_map, &policy_root, &search);
      if (found) {
        tor_assert(p == found->policy);
        tor_free(found);
      }
    }
    tor_free(p);
  }
}

/** Release all storage held by policy variables. */
void
policies_free_all(void)
{
  addr_policy_list_free(reachable_or_addr_policy);
  reachable_or_addr_policy = NULL;
  addr_policy_list_free(reachable_dir_addr_policy);
  reachable_dir_addr_policy = NULL;
  addr_policy_list_free(socks_policy);
  socks_policy = NULL;
  addr_policy_list_free(dir_policy);
  dir_policy = NULL;
  addr_policy_list_free(authdir_reject_policy);
  authdir_reject_policy = NULL;
  addr_policy_list_free(authdir_invalid_policy);
  authdir_invalid_policy = NULL;
  addr_policy_list_free(authdir_baddir_policy);
  authdir_baddir_policy = NULL;
  addr_policy_list_free(authdir_badexit_policy);
  authdir_badexit_policy = NULL;

  if (!HT_EMPTY(&policy_root)) {
    policy_map_ent_t **ent;
    int n = 0;
    char buf[POLICY_BUF_LEN];

    log_warn(LD_MM, "Still had %d address policies cached at shutdown.",
             (int)HT_SIZE(&policy_root));

    /* Note the first 10 cached policies to try to figure out where they
     * might be coming from. */
    HT_FOREACH(ent, policy_map, &policy_root) {
      if (++n > 10)
        break;
      if (policy_write_item(buf, sizeof(buf), (*ent)->policy, 0) >= 0)
        log_warn(LD_MM,"  %d [%d]: %s", n, (*ent)->policy->refcnt, buf);
    }
  }
  HT_CLEAR(policy_map, &policy_root);
}

