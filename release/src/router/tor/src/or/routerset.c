/* Copyright (c) 2001 Matej Pfajfar.
 * Copyright (c) 2001-2004, Roger Dingledine.
 * Copyright (c) 2004-2006, Roger Dingledine, Nick Mathewson.
 * Copyright (c) 2007-2013, The Tor Project, Inc. */
/* See LICENSE for licensing information */

#include "or.h"
#include "geoip.h"
#include "nodelist.h"
#include "policies.h"
#include "router.h"
#include "routerparse.h"
#include "routerset.h"

/** A routerset specifies constraints on a set of possible routerinfos, based
 * on their names, identities, or addresses.  It is optimized for determining
 * whether a router is a member or not, in O(1+P) time, where P is the number
 * of address policy constraints. */
struct routerset_t {
  /** A list of strings for the elements of the policy.  Each string is either
   * a nickname, a hexadecimal identity fingerprint, or an address policy.  A
   * router belongs to the set if its nickname OR its identity OR its address
   * matches an entry here. */
  smartlist_t *list;
  /** A map from lowercase nicknames of routers in the set to (void*)1 */
  strmap_t *names;
  /** A map from identity digests routers in the set to (void*)1 */
  digestmap_t *digests;
  /** An address policy for routers in the set.  For implementation reasons,
   * a router belongs to the set if it is _rejected_ by this policy. */
  smartlist_t *policies;

  /** A human-readable description of what this routerset is for.  Used in
   * log messages. */
  char *description;

  /** A list of the country codes in this set. */
  smartlist_t *country_names;
  /** Total number of countries we knew about when we built <b>countries</b>.*/
  int n_countries;
  /** Bit array mapping the return value of geoip_get_country() to 1 iff the
   * country is a member of this routerset.  Note that we MUST call
   * routerset_refresh_countries() whenever the geoip country list is
   * reloaded. */
  bitarray_t *countries;
};

/** Return a new empty routerset. */
routerset_t *
routerset_new(void)
{
  routerset_t *result = tor_malloc_zero(sizeof(routerset_t));
  result->list = smartlist_new();
  result->names = strmap_new();
  result->digests = digestmap_new();
  result->policies = smartlist_new();
  result->country_names = smartlist_new();
  return result;
}

/** If <b>c</b> is a country code in the form {cc}, return a newly allocated
 * string holding the "cc" part.  Else, return NULL. */
static char *
routerset_get_countryname(const char *c)
{
  char *country;

  if (strlen(c) < 4 || c[0] !='{' || c[3] !='}')
    return NULL;

  country = tor_strndup(c+1, 2);
  tor_strlower(country);
  return country;
}

/** Update the routerset's <b>countries</b> bitarray_t. Called whenever
 * the GeoIP IPv4 database is reloaded.
 */
void
routerset_refresh_countries(routerset_t *target)
{
  int cc;
  bitarray_free(target->countries);

  if (!geoip_is_loaded(AF_INET)) {
    target->countries = NULL;
    target->n_countries = 0;
    return;
  }
  target->n_countries = geoip_get_n_countries();
  target->countries = bitarray_init_zero(target->n_countries);
  SMARTLIST_FOREACH_BEGIN(target->country_names, const char *, country) {
    cc = geoip_get_country(country);
    if (cc >= 0) {
      tor_assert(cc < target->n_countries);
      bitarray_set(target->countries, cc);
    } else {
      log_warn(LD_CONFIG, "Country code '%s' is not recognized.",
          country);
    }
  } SMARTLIST_FOREACH_END(country);
}

/** Parse the string <b>s</b> to create a set of routerset entries, and add
 * them to <b>target</b>.  In log messages, refer to the string as
 * <b>description</b>.  Return 0 on success, -1 on failure.
 *
 * Three kinds of elements are allowed in routersets: nicknames, IP address
 * patterns, and fingerprints.  They may be surrounded by optional space, and
 * must be separated by commas.
 */
int
routerset_parse(routerset_t *target, const char *s, const char *description)
{
  int r = 0;
  int added_countries = 0;
  char *countryname;
  smartlist_t *list = smartlist_new();
  smartlist_split_string(list, s, ",",
                         SPLIT_SKIP_SPACE | SPLIT_IGNORE_BLANK, 0);
  SMARTLIST_FOREACH_BEGIN(list, char *, nick) {
      addr_policy_t *p;
      if (is_legal_hexdigest(nick)) {
        char d[DIGEST_LEN];
        if (*nick == '$')
          ++nick;
        log_debug(LD_CONFIG, "Adding identity %s to %s", nick, description);
        base16_decode(d, sizeof(d), nick, HEX_DIGEST_LEN);
        digestmap_set(target->digests, d, (void*)1);
      } else if (is_legal_nickname(nick)) {
        log_debug(LD_CONFIG, "Adding nickname %s to %s", nick, description);
        strmap_set_lc(target->names, nick, (void*)1);
      } else if ((countryname = routerset_get_countryname(nick)) != NULL) {
        log_debug(LD_CONFIG, "Adding country %s to %s", nick,
                  description);
        smartlist_add(target->country_names, countryname);
        added_countries = 1;
      } else if ((strchr(nick,'.') || strchr(nick, '*')) &&
                 (p = router_parse_addr_policy_item_from_string(
                                     nick, ADDR_POLICY_REJECT))) {
        log_debug(LD_CONFIG, "Adding address %s to %s", nick, description);
        smartlist_add(target->policies, p);
      } else {
        log_warn(LD_CONFIG, "Entry '%s' in %s is malformed.", nick,
                 description);
        r = -1;
        tor_free(nick);
        SMARTLIST_DEL_CURRENT(list, nick);
      }
  } SMARTLIST_FOREACH_END(nick);
  policy_expand_unspec(&target->policies);
  smartlist_add_all(target->list, list);
  smartlist_free(list);
  if (added_countries)
    routerset_refresh_countries(target);
  return r;
}

/** Add all members of the set <b>source</b> to <b>target</b>. */
void
routerset_union(routerset_t *target, const routerset_t *source)
{
  char *s;
  tor_assert(target);
  if (!source || !source->list)
    return;
  s = routerset_to_string(source);
  routerset_parse(target, s, "other routerset");
  tor_free(s);
}

/** Return true iff <b>set</b> lists only nicknames and digests, and includes
 * no IP ranges or countries. */
int
routerset_is_list(const routerset_t *set)
{
  return smartlist_len(set->country_names) == 0 &&
    smartlist_len(set->policies) == 0;
}

/** Return true iff we need a GeoIP IP-to-country database to make sense of
 * <b>set</b>. */
int
routerset_needs_geoip(const routerset_t *set)
{
  return set && smartlist_len(set->country_names);
}

/** Return true iff there are no entries in <b>set</b>. */
int
routerset_is_empty(const routerset_t *set)
{
  return !set || smartlist_len(set->list) == 0;
}

/** Helper.  Return true iff <b>set</b> contains a router based on the other
 * provided fields.  Return higher values for more specific subentries: a
 * single router is more specific than an address range of routers, which is
 * more specific in turn than a country code.
 *
 * (If country is -1, then we take the country
 * from addr.) */
static int
routerset_contains(const routerset_t *set, const tor_addr_t *addr,
                   uint16_t orport,
                   const char *nickname, const char *id_digest,
                   country_t country)
{
  if (!set || !set->list)
    return 0;
  if (nickname && strmap_get_lc(set->names, nickname))
    return 4;
  if (id_digest && digestmap_get(set->digests, id_digest))
    return 4;
  if (addr && compare_tor_addr_to_addr_policy(addr, orport, set->policies)
      == ADDR_POLICY_REJECTED)
    return 3;
  if (set->countries) {
    if (country < 0 && addr)
      country = geoip_get_country_by_addr(addr);

    if (country >= 0 && country < set->n_countries &&
        bitarray_is_set(set->countries, country))
      return 2;
  }
  return 0;
}

/** If *<b>setp</b> includes at least one country code, or if
 * <b>only_some_cc_set</b> is 0, add the ?? and A1 country codes to
 * *<b>setp</b>, creating it as needed.  Return true iff *<b>setp</b> changed.
 */
int
routerset_add_unknown_ccs(routerset_t **setp, int only_if_some_cc_set)
{
  routerset_t *set;
  int add_unknown, add_a1;
  if (only_if_some_cc_set) {
    if (!*setp || smartlist_len((*setp)->country_names) == 0)
      return 0;
  }
  if (!*setp)
    *setp = routerset_new();

  set = *setp;

  add_unknown = ! smartlist_contains_string_case(set->country_names, "??") &&
    geoip_get_country("??") >= 0;
  add_a1 = ! smartlist_contains_string_case(set->country_names, "a1") &&
    geoip_get_country("A1") >= 0;

  if (add_unknown) {
    smartlist_add(set->country_names, tor_strdup("??"));
    smartlist_add(set->list, tor_strdup("{??}"));
  }
  if (add_a1) {
    smartlist_add(set->country_names, tor_strdup("a1"));
    smartlist_add(set->list, tor_strdup("{a1}"));
  }

  if (add_unknown || add_a1) {
    routerset_refresh_countries(set);
    return 1;
  }
  return 0;
}

/** Return true iff we can tell that <b>ei</b> is a member of <b>set</b>. */
int
routerset_contains_extendinfo(const routerset_t *set, const extend_info_t *ei)
{
  return routerset_contains(set,
                            &ei->addr,
                            ei->port,
                            ei->nickname,
                            ei->identity_digest,
                            -1 /*country*/);
}

/** Return true iff <b>ri</b> is in <b>set</b>.  If country is <b>-1</b>, we
 * look up the country. */
int
routerset_contains_router(const routerset_t *set, const routerinfo_t *ri,
                          country_t country)
{
  tor_addr_t addr;
  tor_addr_from_ipv4h(&addr, ri->addr);
  return routerset_contains(set,
                            &addr,
                            ri->or_port,
                            ri->nickname,
                            ri->cache_info.identity_digest,
                            country);
}

/** Return true iff <b>rs</b> is in <b>set</b>.  If country is <b>-1</b>, we
 * look up the country. */
int
routerset_contains_routerstatus(const routerset_t *set,
                                const routerstatus_t *rs,
                                country_t country)
{
  tor_addr_t addr;
  tor_addr_from_ipv4h(&addr, rs->addr);
  return routerset_contains(set,
                            &addr,
                            rs->or_port,
                            rs->nickname,
                            rs->identity_digest,
                            country);
}

/** Return true iff <b>node</b> is in <b>set</b>. */
int
routerset_contains_node(const routerset_t *set, const node_t *node)
{
  if (node->rs)
    return routerset_contains_routerstatus(set, node->rs, node->country);
  else if (node->ri)
    return routerset_contains_router(set, node->ri, node->country);
  else
    return 0;
}

/** Add every known node_t that is a member of <b>routerset</b> to
 * <b>out</b>, but never add any that are part of <b>excludeset</b>.
 * If <b>running_only</b>, only add the running ones. */
void
routerset_get_all_nodes(smartlist_t *out, const routerset_t *routerset,
                        const routerset_t *excludeset, int running_only)
{
  tor_assert(out);
  if (!routerset || !routerset->list)
    return;

  if (routerset_is_list(routerset)) {
    /* No routers are specified by type; all are given by name or digest.
     * we can do a lookup in O(len(routerset)). */
    SMARTLIST_FOREACH(routerset->list, const char *, name, {
        const node_t *node = node_get_by_nickname(name, 1);
        if (node) {
          if (!running_only || node->is_running)
            if (!routerset_contains_node(excludeset, node))
              smartlist_add(out, (void*)node);
        }
    });
  } else {
    /* We need to iterate over the routerlist to get all the ones of the
     * right kind. */
    smartlist_t *nodes = nodelist_get_list();
    SMARTLIST_FOREACH(nodes, const node_t *, node, {
        if (running_only && !node->is_running)
          continue;
        if (routerset_contains_node(routerset, node) &&
            !routerset_contains_node(excludeset, node))
          smartlist_add(out, (void*)node);
    });
  }
}

/** Remove every node_t from <b>lst</b> that is in <b>routerset</b>. */
void
routerset_subtract_nodes(smartlist_t *lst, const routerset_t *routerset)
{
  tor_assert(lst);
  if (!routerset)
    return;
  SMARTLIST_FOREACH(lst, const node_t *, node, {
      if (routerset_contains_node(routerset, node)) {
        //log_debug(LD_DIR, "Subtracting %s",r->nickname);
        SMARTLIST_DEL_CURRENT(lst, node);
      }
    });
}

/** Return a new string that when parsed by routerset_parse_string() will
 * yield <b>set</b>. */
char *
routerset_to_string(const routerset_t *set)
{
  if (!set || !set->list)
    return tor_strdup("");
  return smartlist_join_strings(set->list, ",", 0, NULL);
}

/** Helper: return true iff old and new are both NULL, or both non-NULL
 * equal routersets. */
int
routerset_equal(const routerset_t *old, const routerset_t *new)
{
  if (routerset_is_empty(old) && routerset_is_empty(new)) {
    /* Two empty sets are equal */
    return 1;
  } else if (routerset_is_empty(old) || routerset_is_empty(new)) {
    /* An empty set is equal to nothing else. */
    return 0;
  }
  tor_assert(old != NULL);
  tor_assert(new != NULL);

  if (smartlist_len(old->list) != smartlist_len(new->list))
    return 0;

  SMARTLIST_FOREACH(old->list, const char *, cp1, {
    const char *cp2 = smartlist_get(new->list, cp1_sl_idx);
    if (strcmp(cp1, cp2))
      return 0;
  });

  return 1;
}

/** Free all storage held in <b>routerset</b>. */
void
routerset_free(routerset_t *routerset)
{
  if (!routerset)
    return;

  SMARTLIST_FOREACH(routerset->list, char *, cp, tor_free(cp));
  smartlist_free(routerset->list);
  SMARTLIST_FOREACH(routerset->policies, addr_policy_t *, p,
                    addr_policy_free(p));
  smartlist_free(routerset->policies);
  SMARTLIST_FOREACH(routerset->country_names, char *, cp, tor_free(cp));
  smartlist_free(routerset->country_names);

  strmap_free(routerset->names, NULL);
  digestmap_free(routerset->digests, NULL);
  bitarray_free(routerset->countries);
  tor_free(routerset);
}

